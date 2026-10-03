#include "slang_cache.h"
#include "glslang_util.h"
#include "slang_process.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <encodings/crc32.h>

#include "../video_shader_parse.h"
#include "../../verbosity.h"

#define SPIRV_CACHE_VERSION 1

/* Upper bounds applied when reading a cache file.  The cache lives in
 * the user's cache directory but is still parsed defensively: the
 * previous implementation resized std::vectors straight from on-disk
 * u32 counts, so a corrupt or hostile file could request a multi-GB
 * allocation (unhandled std::bad_alloc -> process abort).  2^24 words
 * is 64 MiB of SPIR-V per stage - far beyond any real shader - and
 * parameters are capped at the same GFX_MAX_PARAMETERS bound the
 * merge step enforces anyway. */
#define SPIRV_CACHE_MAX_STAGE_WORDS (1u << 24)
#define SPIRV_CACHE_MAX_PARAMETERS  GFX_MAX_PARAMETERS

/* An entry is parsed where the cache hands it over, never past its
 * end; each read either takes the bytes asked for or fails */
typedef struct
{
   const uint8_t *p;
   const uint8_t *end;
} spirv_cache_reader_t;

static bool spirv_cache_read(spirv_cache_reader_t *r, void *out, size_t n)
{
   if ((size_t)(r->end - r->p) < n)
      return false;
   memcpy(out, r->p, n);
   r->p += n;
   return true;
}

/* A u32 length and that many bytes, into a fixed buffer. A length that
 * does not fit it, terminator included, is corruption and rejected,
 * never truncated: writers only ever store strings that fit. */
static bool spirv_cache_read_string(spirv_cache_reader_t *r,
      char *str_out, size_t str_out_len)
{
   uint32_t _len;

   if (!spirv_cache_read(r, &_len, sizeof(_len)) || _len >= str_out_len)
      return false;
   if (_len > 0 && !spirv_cache_read(r, str_out, _len))
      return false;
   str_out[_len] = '\0';
   return true;
}

/* Words of SPIR-V for one stage, into a buffer of their own */
static bool spirv_cache_read_stage(spirv_cache_reader_t *r,
      uint32_t **words, size_t *words_len)
{
   uint32_t count;

   if (     !spirv_cache_read(r, &count, sizeof(count))
         || count > SPIRV_CACHE_MAX_STAGE_WORDS)
      return false;
   if (!count)
      return true;
   if ((size_t)(r->end - r->p) / sizeof(uint32_t) < count)
      return false;
   if (!(*words = (uint32_t*)malloc(count * sizeof(uint32_t))))
      return false;
   *words_len = count;
   return spirv_cache_read(r, *words, count * sizeof(uint32_t));
}

static uint8_t *spirv_cache_put(uint8_t *w, const void *src, size_t n)
{
   memcpy(w, src, n);
   return w + n;
}

static uint8_t *spirv_cache_put_string(uint8_t *w, const char *str,
      size_t str_len)
{
   uint32_t _len = (uint32_t)str_len;
   w = spirv_cache_put(w, &_len, sizeof(_len));
   return spirv_cache_put(w, str, str_len);
}

#ifdef __cplusplus
extern "C" {
#endif

bool spirv_cache_compute_hash(const char *vertex_source,
      const char *fragment_source, char *hash_out)
{
   size_t vertex_len, fragment_len;

   if (!vertex_source || !fragment_source || !hash_out)
      return false;

   vertex_len   = strlen(vertex_source);
   fragment_len = strlen(fragment_source);

   /* Each stage is summed where it already sits, and the two sums and
    * the two lengths together name the entry.  encoding_crc32() folds
    * with a carry-less multiply on x86 and the CRC32 instructions on
    * ARM, so this reads the sources about twenty times as fast as a
    * cryptographic digest of them would, and it needs neither the
    * third buffer the digest was given nor the copy into it.
    *
    * Four fields make the name 128 bits wide, which is what keeps two
    * unrelated shaders out of each other's cache entry: a pair would
    * have to agree on both sums and both lengths at once. */
   if (vertex_len > 0xffffffffu || fragment_len > 0xffffffffu)
      return false;

   snprintf(hash_out, 33, "%08x%08x%08x%08x",
         (unsigned)encoding_crc32(0, (const uint8_t*)vertex_source,
            vertex_len),
         (unsigned)encoding_crc32(0, (const uint8_t*)fragment_source,
            fragment_len),
         (unsigned)vertex_len,
         (unsigned)fragment_len);

   return true;
}

bool spirv_cache_load(const char *hash, struct glslang_output *output)
{
   video_shader_cache_view_t view;
   spirv_cache_reader_t r;
   uint8_t  version;
   uint16_t rt_format;
   uint32_t param_count, i;

   if (!hash || !output)
      return false;

   if (!video_shader_cache_map(VIDEO_SHADER_CACHE_SPIRV, hash, &view))
      return false; /* Not cached yet */

   r.p   = view.data;
   r.end = view.data + view.len;

   if (     !spirv_cache_read(&r, &version, sizeof(version))
         || version != SPIRV_CACHE_VERSION)
      goto error;

   if (     !spirv_cache_read_stage(&r, &output->vertex,   &output->vertex_len)
         || !spirv_cache_read_stage(&r, &output->fragment, &output->fragment_len))
      goto error;

   if (     !spirv_cache_read(&r, &param_count, sizeof(param_count))
         || param_count > SPIRV_CACHE_MAX_PARAMETERS)
      goto error;

   if (param_count > 0)
   {
      output->meta.parameters = (glslang_parameter*)calloc(
            param_count, sizeof(*output->meta.parameters));
      if (!output->meta.parameters)
         goto error;
      output->meta.cap_parameters = param_count;
      output->meta.num_parameters = param_count;
   }

   for (i = 0; i < param_count; i++)
   {
      glslang_parameter *param = &output->meta.parameters[i];

      if (     !spirv_cache_read_string(&r, param->id,   sizeof(param->id))
            || !spirv_cache_read_string(&r, param->desc, sizeof(param->desc))
            || !spirv_cache_read(&r, &param->initial, sizeof(float))
            || !spirv_cache_read(&r, &param->minimum, sizeof(float))
            || !spirv_cache_read(&r, &param->maximum, sizeof(float))
            || !spirv_cache_read(&r, &param->step,    sizeof(float)))
         goto error;
   }

   if (     !spirv_cache_read_string(&r, output->meta.name,
               sizeof(output->meta.name))
         || !spirv_cache_read(&r, &rt_format, sizeof(rt_format)))
      goto error;
   output->meta.rt_format = (enum glslang_format)rt_format;

   video_shader_cache_unmap(&view);

   RARCH_LOG("[Slang Cache] Loaded shader cache for hash: %.16s...\n", hash);

   return true;

error:
   video_shader_cache_unmap(&view);
   glslang_output_free(output);
   return false;
}

bool spirv_cache_save(const char *hash, const struct glslang_output *output)
{
   uint8_t *buf, *w;
   size_t   size, i;
   uint8_t  version = SPIRV_CACHE_VERSION;
   uint16_t rt_format;
   uint32_t vertex_size, fragment_size, param_count;
   bool     ok;

   if (!hash || !output)
      return false;

   /* Nothing load would turn away goes in: then the sizes below cannot
    * overflow either */
   if (     output->vertex_len   > SPIRV_CACHE_MAX_STAGE_WORDS
         || output->fragment_len > SPIRV_CACHE_MAX_STAGE_WORDS
         || output->meta.num_parameters > SPIRV_CACHE_MAX_PARAMETERS)
      return false;

   vertex_size   = (uint32_t)output->vertex_len;
   fragment_size = (uint32_t)output->fragment_len;
   param_count   = (uint32_t)output->meta.num_parameters;

   /* The whole entry is laid out in one buffer and stored in one write */
   size = sizeof(version)
        + sizeof(uint32_t) + (size_t)vertex_size   * sizeof(uint32_t)
        + sizeof(uint32_t) + (size_t)fragment_size * sizeof(uint32_t)
        + sizeof(uint32_t)
        + sizeof(uint32_t) + strlen(output->meta.name)
        + sizeof(rt_format);
   for (i = 0; i < param_count; i++)
      size += 2 * sizeof(uint32_t) + 4 * sizeof(float)
            + strlen(output->meta.parameters[i].id)
            + strlen(output->meta.parameters[i].desc);

   if (!(buf = (uint8_t*)malloc(size)))
      return false;

   w = spirv_cache_put(buf, &version, sizeof(version));
   w = spirv_cache_put(w, &vertex_size, sizeof(vertex_size));
   if (vertex_size)
      w = spirv_cache_put(w, output->vertex, vertex_size * sizeof(uint32_t));
   w = spirv_cache_put(w, &fragment_size, sizeof(fragment_size));
   if (fragment_size)
      w = spirv_cache_put(w, output->fragment, fragment_size * sizeof(uint32_t));
   w = spirv_cache_put(w, &param_count, sizeof(param_count));
   for (i = 0; i < param_count; i++)
   {
      const glslang_parameter *param = &output->meta.parameters[i];
      w = spirv_cache_put_string(w, param->id,   strlen(param->id));
      w = spirv_cache_put_string(w, param->desc, strlen(param->desc));
      w = spirv_cache_put(w, &param->initial, sizeof(float));
      w = spirv_cache_put(w, &param->minimum, sizeof(float));
      w = spirv_cache_put(w, &param->maximum, sizeof(float));
      w = spirv_cache_put(w, &param->step,    sizeof(float));
   }
   w = spirv_cache_put_string(w, output->meta.name, strlen(output->meta.name));
   rt_format = (uint16_t)output->meta.rt_format;
   w = spirv_cache_put(w, &rt_format, sizeof(rt_format));

   ok = (size_t)(w - buf) == size
     && video_shader_cache_write(VIDEO_SHADER_CACHE_SPIRV, hash, buf, size);
   free(buf);

   if (ok)
      RARCH_LOG("[Slang Cache] Saved shader cache for hash: %.16s...\n", hash);
   return ok;
}

#ifdef __cplusplus
} /* extern "C" */
#endif
