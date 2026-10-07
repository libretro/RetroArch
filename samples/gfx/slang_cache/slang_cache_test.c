/* The SPIR-V cache, from the shipping gfx/drivers_shader/slang_cache.c
 * and the cache store in gfx/video_shader_parse.c, against a real
 * directory through the real VFS.
 *
 * slang_cache.c serialises and parses entries in memory; where they
 * live, how they are read (mapped or in one read) and how they are
 * written (whole, by rename) is video_shader_parse.c's. Lanes:
 *
 *  roundtrip   an entry saved and loaded comes back field for field,
 *              and no temporary file is left beside it.
 *  format      the bytes on disk are the layout entries have always
 *              had, so caches written before stay valid.
 *  truncated   every prefix of a valid entry is turned away, the
 *              output left empty.
 *  corrupt     a wrong version, oversized stage and parameter counts
 *              and an over-long string are turned away.
 *  keys        a key that is not letters and digits names nothing:
 *              no read, no write, nothing outside the cache.
 *
 * Built twice: mapped (HAVE_MMAP) and reading the entry in one go. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <file/file_path.h>
#include <streams/file_stream.h>

#include "configuration.h"
#include "gfx/video_shader_parse.h"
#include "gfx/drivers_shader/slang_process.h"
#include "gfx/drivers_shader/slang_cache.h"

static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); fails++; } } while (0)

/* ---- what the cache reads of the frontend ------------------------- */

static settings_t settings;
settings_t *config_get_ptr(void) { return &settings; }
void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; }
void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }
uint64_t cpu_features_get(void) { return 0; }

/* slang_process.c's, which the rest of that file would drag in */
void glslang_output_init(glslang_output *output)
{
   memset(output, 0, sizeof(*output));
}
void glslang_output_free(glslang_output *output)
{
   free(output->vertex);
   free(output->fragment);
   free(output->meta.parameters);
   glslang_output_init(output);
}

/* ---- fixtures ------------------------------------------------------ */

static char cache_dir[512];
static char entry_path[600];
#define KEY "0123456789abcdef0123456789abcdef"

static void make_output(glslang_output *o)
{
   size_t i;
   glslang_output_init(o);
   o->vertex_len   = 777;
   o->fragment_len = 1234;
   o->vertex       = (uint32_t*)malloc(o->vertex_len * 4);
   o->fragment     = (uint32_t*)malloc(o->fragment_len * 4);
   for (i = 0; i < o->vertex_len; i++)
      o->vertex[i] = 0x07230203u ^ (uint32_t)(i * 2654435761u);
   for (i = 0; i < o->fragment_len; i++)
      o->fragment[i] = (uint32_t)(i * 40503u + 17u);
   o->meta.num_parameters = o->meta.cap_parameters = 3;
   o->meta.parameters = (glslang_parameter*)calloc(3, sizeof(glslang_parameter));
   for (i = 0; i < 3; i++)
   {
      snprintf(o->meta.parameters[i].id, 64, "PARAM_%u", (unsigned)i);
      snprintf(o->meta.parameters[i].desc, 64, "Parameter number %u", (unsigned)i);
      o->meta.parameters[i].initial = 0.5f + (float)i;
      o->meta.parameters[i].minimum = -1.0f;
      o->meta.parameters[i].maximum = 10.0f;
      o->meta.parameters[i].step    = 0.25f;
   }
   strcpy(o->meta.name, "pass_name");
   o->meta.rt_format = (enum glslang_format)5;
}

static bool outputs_equal(const glslang_output *a, const glslang_output *b)
{
   size_t i;
   if (     a->vertex_len != b->vertex_len || a->fragment_len != b->fragment_len
         || memcmp(a->vertex, b->vertex, a->vertex_len * 4)
         || memcmp(a->fragment, b->fragment, a->fragment_len * 4)
         || a->meta.num_parameters != b->meta.num_parameters
         || strcmp(a->meta.name, b->meta.name)
         || a->meta.rt_format != b->meta.rt_format)
      return false;
   for (i = 0; i < a->meta.num_parameters; i++)
   {
      const glslang_parameter *p = &a->meta.parameters[i];
      const glslang_parameter *q = &b->meta.parameters[i];
      if (     strcmp(p->id, q->id) || strcmp(p->desc, q->desc)
            || p->initial != q->initial || p->minimum != q->minimum
            || p->maximum != q->maximum || p->step != q->step)
         return false;
   }
   return true;
}

static bool output_empty(const glslang_output *o)
{
   return !o->vertex && !o->fragment && !o->meta.parameters
      && !o->vertex_len && !o->fragment_len && !o->meta.num_parameters;
}

/* The layout as the previous slang_cache.c wrote it, field by field */
static size_t reference_encode(const glslang_output *o, uint8_t *buf)
{
   uint8_t *w = buf;
   uint32_t u;
   uint16_t rt;
   size_t   i;
#define PUT(src, n) do { memcpy(w, (src), (n)); w += (n); } while (0)
#define PUT_STR(s) do { u = (uint32_t)strlen(s); PUT(&u, 4); PUT((s), u); } while (0)
   *w++ = 1;
   u = (uint32_t)o->vertex_len;   PUT(&u, 4); PUT(o->vertex, o->vertex_len * 4);
   u = (uint32_t)o->fragment_len; PUT(&u, 4); PUT(o->fragment, o->fragment_len * 4);
   u = (uint32_t)o->meta.num_parameters; PUT(&u, 4);
   for (i = 0; i < o->meta.num_parameters; i++)
   {
      const glslang_parameter *p = &o->meta.parameters[i];
      PUT_STR(p->id);
      PUT_STR(p->desc);
      PUT(&p->initial, 4); PUT(&p->minimum, 4); PUT(&p->maximum, 4); PUT(&p->step, 4);
   }
   PUT_STR(o->meta.name);
   rt = (uint16_t)o->meta.rt_format;
   PUT(&rt, 2);
#undef PUT
#undef PUT_STR
   return (size_t)(w - buf);
}

static uint8_t *slurp(const char *path, size_t *len)
{
   FILE *f = fopen(path, "rb");
   long  n;
   uint8_t *b;
   if (!f)
      return NULL;
   fseek(f, 0, SEEK_END);
   n = ftell(f);
   fseek(f, 0, SEEK_SET);
   b = (uint8_t*)malloc(n > 0 ? (size_t)n : 1);
   *len = fread(b, 1, (size_t)n, f);
   fclose(f);
   return b;
}

static void put_file(const char *path, const uint8_t *b, size_t n)
{
   FILE *f = fopen(path, "wb");
   if (f)
   {
      fwrite(b, 1, n, f);
      fclose(f);
   }
}

static bool loads(const char *key)
{
   glslang_output o;
   bool ok;
   glslang_output_init(&o);
   ok = spirv_cache_load(key, &o);
   if (!ok)
      CHECK(output_empty(&o), "a refused entry leaves the output empty");
   glslang_output_free(&o);
   return ok;
}

int main(void)
{
   glslang_output a, b;
   uint8_t *disk, *ref;
   size_t   disk_len = 0, ref_len, i;
   char     tmp_path[700], cmd[600];
   unsigned refused = 0;

   snprintf(cache_dir, sizeof(cache_dir), "/tmp/slang_cache_test_%ld", (long)getpid());
   strlcpy(settings.paths.directory_cache, cache_dir,
         sizeof(settings.paths.directory_cache));
   snprintf(entry_path, sizeof(entry_path), "%s/spirv/%s.spirv", cache_dir, KEY);
   snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", entry_path);

   /* roundtrip */
   make_output(&a);
   CHECK(!loads(KEY), "nothing cached yet");
   CHECK(spirv_cache_save(KEY, &a), "an entry is saved");
   CHECK(path_is_valid(entry_path), "where the cache keeps it");
   CHECK(!path_is_valid(tmp_path), "no temporary file left beside it");
   glslang_output_init(&b);
   CHECK(spirv_cache_load(KEY, &b), "and loads");
   CHECK(outputs_equal(&a, &b), "field for field");
   glslang_output_free(&b);
   {
      video_shader_cache_view_t view;
      CHECK(video_shader_cache_map(VIDEO_SHADER_CACHE_SPIRV, KEY, &view),
            "the entry maps");
#ifdef HAVE_MMAP
      CHECK(view.mapped && !view.heap, "mapped, not copied");
#else
      CHECK(view.heap && !view.mapped, "read in one go");
#endif
      video_shader_cache_unmap(&view);
   }
   printf("[ok] roundtrip\n");

   /* format */
   disk = slurp(entry_path, &disk_len);
   ref  = (uint8_t*)malloc(disk_len + 65536);
   ref_len = reference_encode(&a, ref);
   CHECK(disk && disk_len == ref_len && !memcmp(disk, ref, ref_len),
         "the bytes on disk are the layout caches have always had");
   printf("[ok] format (%u bytes)\n", (unsigned)disk_len);

   /* truncated: every prefix */
   for (i = 0; i < disk_len; i++)
   {
      put_file(entry_path, disk, i);
      if (!loads(KEY))
         refused++;
   }
   CHECK(refused == disk_len, "every truncated entry is turned away");
   printf("[ok] truncated (%u prefixes)\n", refused);

   /* corrupt */
   {
      uint8_t *c = (uint8_t*)malloc(disk_len);
      uint32_t big;
      size_t   off;

      memcpy(c, disk, disk_len); c[0] = 2;
      put_file(entry_path, c, disk_len);
      CHECK(!loads(KEY), "a different version is turned away");

      memcpy(c, disk, disk_len); big = (1u << 24) + 1;
      memcpy(c + 1, &big, 4);
      put_file(entry_path, c, disk_len);
      CHECK(!loads(KEY), "an oversized vertex count is turned away");

      memcpy(c, disk, disk_len); big = 0xFFFFFFFFu;
      memcpy(c + 1 + 4 + a.vertex_len * 4, &big, 4);
      put_file(entry_path, c, disk_len);
      CHECK(!loads(KEY), "an oversized fragment count is turned away");

      off = 1 + 4 + a.vertex_len * 4 + 4 + a.fragment_len * 4;
      memcpy(c, disk, disk_len); big = 100000;
      memcpy(c + off, &big, 4);
      put_file(entry_path, c, disk_len);
      CHECK(!loads(KEY), "an oversized parameter count is turned away");

      memcpy(c, disk, disk_len); big = 64;
      memcpy(c + off + 4, &big, 4);
      put_file(entry_path, c, disk_len);
      CHECK(!loads(KEY), "a 64-byte id is turned away");

      /* A 64-byte id with every byte after it in place: only the bound
       * on the length keeps it out of the 64-byte field */
      {
         glslang_output l;
         uint8_t *e = (uint8_t*)malloc(disk_len + 256);
         size_t   e_len, id_at;
         make_output(&l);
         memset(l.meta.parameters[0].id, 'A', 63);
         l.meta.parameters[0].id[63] = '\0';
         e_len = reference_encode(&l, e);
         id_at = 1 + 4 + l.vertex_len * 4 + 4 + l.fragment_len * 4 + 4;
         memmove(e + id_at + 4 + 64, e + id_at + 4 + 63, e_len - (id_at + 4 + 63));
         e[id_at + 4 + 63] = 'A';
         big = 64;
         memcpy(e + id_at, &big, 4);
         put_file(entry_path, e, e_len + 1);
         CHECK(!loads(KEY), "a 64-byte id is turned away, the rest intact");
         e[id_at + 4 + 63] = 'A';
         big = 63;
         memcpy(e + id_at, &big, 4);
         memmove(e + id_at + 4 + 63, e + id_at + 4 + 64, e_len - (id_at + 4 + 63));
         put_file(entry_path, e, e_len);
         CHECK(loads(KEY), "and the same entry at 63 loads");
         glslang_output_free(&l);
         free(e);
      }

      put_file(entry_path, disk, disk_len);
      CHECK(loads(KEY), "the intact entry still loads");
      free(c);
      printf("[ok] corrupt\n");
   }

   /* keys */
   {
      static const char *bad[] = { "", "../escape", "a/b", "a.b", "a b", NULL };
      char outside[700];
      int  k;
      snprintf(outside, sizeof(outside), "%s/escape.spirv", cache_dir);
      for (k = 0; bad[k]; k++)
      {
         video_shader_cache_view_t view;
         CHECK(!video_shader_cache_write(VIDEO_SHADER_CACHE_SPIRV, bad[k], "x", 1),
               "a bad key writes nothing");
         CHECK(!video_shader_cache_map(VIDEO_SHADER_CACHE_SPIRV, bad[k], &view),
               "a bad key reads nothing");
      }
      CHECK(!path_is_valid(outside), "nothing written outside the cache");
      printf("[ok] keys\n");
   }

   free(disk);
   free(ref);
   glslang_output_free(&a);
   snprintf(cmd, sizeof(cmd), "rm -rf %s", cache_dir);
   if (system(cmd) != 0) { }

   if (fails)
   {
      printf("slang_cache_test: %d failure(s)\n", fails);
      return 1;
   }
   printf("slang_cache_test: all lanes pass\n");
   return 0;
}
