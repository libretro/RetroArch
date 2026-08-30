#include "slang_cache.h"
#include "glslang_util.h"
#include "slang_process.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <vfs/vfs.h>
#include <compat/strl.h>
#include <lrc_hash.h>

#include "../../configuration.h"
#include "../../verbosity.h"

#define SPIRV_CACHE_VERSION 1
#define SPIRV_CACHE_SUBDIR  "spirv"

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

/**
 * Get the full path to the SPIR-V cache directory
 *
 * @param cache_dir_out Output buffer for the cache directory path (must be at least PATH_MAX_LENGTH)
 * @param cache_dir_out_len Size of the output buffer
 * @return true on success, false if cache dir is not configured
 */
static bool spirv_cache_get_dir(char *cache_dir_out, size_t cache_dir_out_len)
{
   settings_t *settings = config_get_ptr();

   if (!settings || !settings->paths.directory_cache[0])
      return false;

   /* Build the spirv subdirectory path */
   fill_pathname_join_special(cache_dir_out,
         settings->paths.directory_cache, SPIRV_CACHE_SUBDIR,
         cache_dir_out_len);

   return true;
}

/**
 * Ensure the SPIR-V cache directory exists
 *
 * @return true if directory exists or was created, false on error
 */
static bool spirv_cache_ensure_dir(void)
{
   char cache_dir[PATH_MAX_LENGTH];

   if (!spirv_cache_get_dir(cache_dir, sizeof(cache_dir)))
      return false;

   return path_mkdir(cache_dir);
}

/**
 * Get the full path to a cache file for a given hash
 *
 * @param hash Hash string (64 characters)
 * @param cache_file_out Output buffer for the full cache file path
 * @param cache_file_out_len Size of the output buffer
 * @return true on success, false on error
 */
static bool spirv_cache_get_filename(const char *hash,
      char *cache_file_out, size_t cache_file_out_len)
{
   char cache_dir[PATH_MAX_LENGTH];
   char hash_filename[128];

   if (!spirv_cache_get_dir(cache_dir, sizeof(cache_dir)))
      return false;

   snprintf(hash_filename, sizeof(hash_filename), "%s.spirv", hash);
   fill_pathname_join_special(cache_file_out, cache_dir, hash_filename,
         cache_file_out_len);

   return true;
}

/**
 * Write a string to a file with a u32 length prefix (no terminator
 * on disk).
 *
 * @param file File pointer (opened in binary mode)
 * @param str  '\0'-terminated string to write (must not be NULL)
 * @return true on success, false on error
 */
static bool spirv_cache_write_string(RFILE *file, const char *str)
{
   uint32_t _len;
   size_t s_len = strlen(str);
   if (s_len > UINT32_MAX)
      return false;
   _len = (uint32_t)s_len;

   if (filestream_write(file, &_len, sizeof(uint32_t)) != sizeof(uint32_t))
      return false;

   if (_len > 0 && filestream_write(file, str, _len) != _len)
      return false;

   return true;
}

/**
 * Read a u32-length-prefixed string from a file into a fixed buffer.
 * A stored length that does not fit the buffer (terminator included)
 * is treated as corruption and rejected, never truncated: the data
 * model guarantees writers only ever store strings that fit.
 *
 * @param file        File pointer (opened in binary mode)
 * @param str_out     Output buffer
 * @param str_out_len Size of the output buffer
 * @return true on success, false on error
 */
static bool spirv_cache_read_string(RFILE *file,
      char *str_out, size_t str_out_len)
{
   uint32_t _len;

   if (filestream_read(file, &_len, sizeof(uint32_t)) != sizeof(uint32_t))
      return false;

   if (_len >= str_out_len)
      return false;

   if (_len > 0 &&
         filestream_read(file, str_out, _len) != (int64_t)_len)
      return false;

   str_out[_len] = '\0';
   return true;
}

#ifdef __cplusplus
extern "C" {
#endif

bool spirv_cache_compute_hash(const char *vertex_source, const char *fragment_source, char *hash_out)
{
   uint8_t *combined;
   size_t vertex_len, fragment_len, total_len;
   if (!vertex_source || !fragment_source || !hash_out)
      return false;

   /* Build combined hash input: vertex + "|" + fragment */
   vertex_len   = strlen(vertex_source);
   fragment_len = strlen(fragment_source);
   total_len    = vertex_len + 1 + fragment_len;  /* 1 for "|" separator */
   combined     = (uint8_t*)malloc(total_len);
   if (!combined)
      return false;

   memcpy(combined, vertex_source, vertex_len);
   combined[vertex_len] = '|';
   memcpy(combined + vertex_len + 1, fragment_source, fragment_len);

   /* Compute SHA256 hash using libretro-common */
   sha256_hash(hash_out, combined, total_len);

   free(combined);
   return true;
}

bool spirv_cache_load(const char *hash, struct glslang_output *output)
{
   RFILE *file;
   uint8_t version;
   char cache_file[PATH_MAX_LENGTH];
   uint32_t vertex_size, fragment_size, param_count, i;
   uint16_t rt_format;

   if (!hash || !output)
      return false;

   if (!spirv_cache_get_filename(hash, cache_file, sizeof(cache_file)))
      return false;

   file = filestream_open(cache_file, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!file)
      return false; /* Cache file doesn't exist yet */

   /* Read version */
   if (filestream_read(file, &version, sizeof(uint8_t)) != sizeof(uint8_t))
      goto error;

   if (version != SPIRV_CACHE_VERSION)
      goto error; /* Version mismatch */

   /* Read vertex SPIR-V */
   if (filestream_read(file, &vertex_size, sizeof(uint32_t)) != sizeof(uint32_t))
      goto error;

   if (vertex_size > SPIRV_CACHE_MAX_STAGE_WORDS)
      goto error;

   if (vertex_size > 0)
   {
      output->vertex = (uint32_t*)malloc(vertex_size * sizeof(uint32_t));
      if (!output->vertex)
         goto error;
      output->vertex_len = vertex_size;
      if (filestream_read(file, output->vertex, vertex_size * sizeof(uint32_t)) != (int64_t)(vertex_size * sizeof(uint32_t)))
         goto error;
   }

   /* Read fragment SPIR-V */
   if (filestream_read(file, &fragment_size, sizeof(uint32_t)) != sizeof(uint32_t))
      goto error;

   if (fragment_size > SPIRV_CACHE_MAX_STAGE_WORDS)
      goto error;

   if (fragment_size > 0)
   {
      output->fragment = (uint32_t*)malloc(fragment_size * sizeof(uint32_t));
      if (!output->fragment)
         goto error;
      output->fragment_len = fragment_size;
      if (filestream_read(file, output->fragment, fragment_size * sizeof(uint32_t)) != (int64_t)(fragment_size * sizeof(uint32_t)))
         goto error;
   }

   /* Read parameters count */
   if (filestream_read(file, &param_count, sizeof(uint32_t)) != sizeof(uint32_t))
      goto error;

   if (param_count > SPIRV_CACHE_MAX_PARAMETERS)
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

   /* Read each parameter */
   for (i = 0; i < param_count; i++)
   {
      glslang_parameter *param = &output->meta.parameters[i];

      if (!spirv_cache_read_string(file, param->id, sizeof(param->id)))
         goto error;

      if (!spirv_cache_read_string(file, param->desc, sizeof(param->desc)))
         goto error;

      if (filestream_read(file, &param->initial, sizeof(float)) != sizeof(float))
         goto error;
      if (filestream_read(file, &param->minimum, sizeof(float)) != sizeof(float))
         goto error;
      if (filestream_read(file, &param->maximum, sizeof(float)) != sizeof(float))
         goto error;
      if (filestream_read(file, &param->step, sizeof(float)) != sizeof(float))
         goto error;
   }

   /* Read shader name */
   if (!spirv_cache_read_string(file, output->meta.name,
            sizeof(output->meta.name)))
      goto error;

   /* Read render target format */
   if (filestream_read(file, &rt_format, sizeof(uint16_t)) != sizeof(uint16_t))
      goto error;
   output->meta.rt_format = (enum glslang_format)rt_format;

   filestream_close(file);

   RARCH_LOG("[Slang Cache] Loaded shader cache for hash: %.16s...\n", hash);

   return true;

error:
   filestream_close(file);
   glslang_output_free(output);
   return false;
}

bool spirv_cache_save(const char *hash, const struct glslang_output *output)
{
   RFILE *file;
   uint16_t rt_format;
   char cache_file[PATH_MAX_LENGTH];
   uint8_t version = SPIRV_CACHE_VERSION;
   uint32_t vertex_size, fragment_size, param_count, i;

   if (!hash || !output)
      return false;

   /* Ensure cache directory exists */
   if (!spirv_cache_ensure_dir())
      return false;

   if (!spirv_cache_get_filename(hash, cache_file, sizeof(cache_file)))
      return false;

   file = filestream_open(cache_file, RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!file)
      return false;

   /* Write version */
   if (filestream_write(file, &version, sizeof(uint8_t)) != sizeof(uint8_t))
      goto error;

   /* Write vertex SPIR-V */
   if (output->vertex_len > UINT32_MAX)
      goto error;
   vertex_size = (uint32_t)output->vertex_len;
   if (filestream_write(file, &vertex_size, sizeof(uint32_t)) != sizeof(uint32_t))
      goto error;
   if (vertex_size > 0)
   {
      if (filestream_write(file, output->vertex, vertex_size * sizeof(uint32_t)) != (int64_t)(vertex_size * sizeof(uint32_t)))
         goto error;
   }

   /* Write fragment SPIR-V */
   if (output->fragment_len > UINT32_MAX)
      goto error;
   fragment_size = (uint32_t)output->fragment_len;
   if (filestream_write(file, &fragment_size, sizeof(uint32_t)) != sizeof(uint32_t))
      goto error;
   if (fragment_size > 0)
   {
      if (filestream_write(file, output->fragment, fragment_size * sizeof(uint32_t)) != (int64_t)(fragment_size * sizeof(uint32_t)))
         goto error;
   }

   /* Write parameters */
   if (output->meta.num_parameters > UINT32_MAX)
      goto error;
   param_count = (uint32_t)output->meta.num_parameters;
   if (filestream_write(file, &param_count, sizeof(uint32_t)) != sizeof(uint32_t))
      goto error;

   for (i = 0; i < param_count; i++)
   {
      const glslang_parameter *param = &output->meta.parameters[i];

      if (!spirv_cache_write_string(file, param->id))
         goto error;
      if (!spirv_cache_write_string(file, param->desc))
         goto error;

      if (filestream_write(file, &param->initial, sizeof(float)) != sizeof(float))
         goto error;
      if (filestream_write(file, &param->minimum, sizeof(float)) != sizeof(float))
         goto error;
      if (filestream_write(file, &param->maximum, sizeof(float)) != sizeof(float))
         goto error;
      if (filestream_write(file, &param->step, sizeof(float)) != sizeof(float))
         goto error;
   }

   /* Write shader name */
   if (!spirv_cache_write_string(file, output->meta.name))
      goto error;

   /* Write render target format */
   rt_format = (uint16_t)output->meta.rt_format;
   if (filestream_write(file, &rt_format, sizeof(uint16_t)) != sizeof(uint16_t))
      goto error;

   filestream_close(file);

   RARCH_LOG("[Slang Cache] Saved shader cache for hash: %.16s...\n", hash);

   return true;

error:
   filestream_close(file);
   filestream_delete(cache_file); /* Clean up partial file on error */
   return false;
}

#ifdef __cplusplus
} /* extern "C" */
#endif
