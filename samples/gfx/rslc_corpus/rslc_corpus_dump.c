/* rslc corpus dump.
 *
 * Takes .slang files through the frontend's own path: include
 * expansion and stage split by gfx/drivers_shader/glslang_util.c, each
 * stage compiled by the system's glslang through glslang_compile_spirv.
 * Writes, per shader, the two stage sources and the two SPIR-V modules
 * under a name derived from the shader's path, plus one manifest line:
 *
 *   <status> <name> <path>
 *
 * with status ok, read-fail, vertex-fail or fragment-fail. The stage
 * source is written even when its compile fails, so a census can see
 * what glslang rejects. The frontend's three source callbacks are
 * provided here over plain stdio, the way video_shader_parse.c does
 * over filestream.
 *
 *   rslc_corpus_dump <outdir> <a.slang> [b.slang ...]
 *   rslc_corpus_dump <outdir> -          (paths on stdin, one per line)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

#include <boolean.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <compat/strl.h>

#include "gfx/drivers_shader/glslang_util.h"
#include "gfx/drivers_shader/glslang_compile.h"

/* Logging the frontend's units expect; errors go to stderr so a
 * failing shader's reason lands next to its manifest line. */
void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }

bool video_shader_source_read(const char *ident, char **buf, int64_t *len)
{
   FILE  *f;
   long   n;
   char  *data;

   if (!ident || !*ident || !buf)
      return false;
   *buf = NULL;
   if (len)
      *len = 0;
   f = fopen(ident, "rb");
   if (!f)
      return false;
   if (fseek(f, 0, SEEK_END) || (n = ftell(f)) <= 0 || fseek(f, 0, SEEK_SET))
   {
      fclose(f);
      return false;
   }
   data = (char*)malloc((size_t)n + 1);
   if (!data || fread(data, 1, (size_t)n, f) != (size_t)n)
   {
      free(data);
      fclose(f);
      return false;
   }
   fclose(f);
   data[n] = '\0';
   *buf = data;
   if (len)
      *len = n;
   return true;
}

bool video_shader_source_resolve(const char *parent, const char *name,
      char *s, size_t len)
{
   if (!name || !*name || !s || !len)
      return false;
   s[0] = '\0';
   if (!parent || !*parent)
   {
      strlcpy(s, name, len);
      return true;
   }
   fill_pathname_resolve_relative(s, parent, name, len);
   return s[0] != '\0';
}

const char *video_shader_source_ident_name(const char *ident)
{
   if (!ident || !*ident)
      return NULL;
   return path_basename_nocompression(ident);
}

bool video_shader_source_ident_is_slang(const char *ident)
{
   if (!ident || !*ident)
      return false;
   return string_is_equal(path_get_extension(ident), "slang");
}

/* "shaders/crt/crt-royale.slang" -> "shaders_crt_crt-royale" */
static void dump_name(const char *path, char *s, size_t len)
{
   size_t i = 0;
   const char *p = path;

   while (*p == '.' || *p == '/')
      p++;
   for (; *p && i + 1 < len; p++)
   {
      char ch = *p;
      if (ch == '/' || ch == '\\' || ch == ' ')
         ch = '_';
      s[i++] = ch;
   }
   s[i] = '\0';
   if (i > 6 && !strcmp(s + i - 6, ".slang"))
      s[i - 6] = '\0';
}

static bool write_file(const char *dir, const char *name, const char *ext,
      const void *data, size_t len)
{
   char  path[1024];
   FILE *f;

   snprintf(path, sizeof(path), "%s/%s.%s", dir, name, ext);
   f = fopen(path, "wb");
   if (!f)
      return false;
   if (len && fwrite(data, 1, len, f) != len)
   {
      fclose(f);
      return false;
   }
   fclose(f);
   return true;
}

static const char *dump_one(const char *outdir, const char *path)
{
   struct shader_line_buf lines;
   char      name[512];
   char     *vs      = NULL;
   char     *fs      = NULL;
   uint32_t *vs_spv  = NULL;
   uint32_t *fs_spv  = NULL;
   size_t    vs_len  = 0;
   size_t    fs_len  = 0;
   const char *status = "ok";

   dump_name(path, name, sizeof(name));

   if (!shader_line_buf_init(&lines))
      return "read-fail";
   if (!glslang_read_shader_file(path, &lines, true, false))
   {
      shader_line_buf_free(&lines);
      return "read-fail";
   }
   vs = glslang_build_stage_source(&lines, "vertex");
   fs = glslang_build_stage_source(&lines, "fragment");
   shader_line_buf_free(&lines);
   if (!vs || !fs)
   {
      free(vs);
      free(fs);
      return "read-fail";
   }

   write_file(outdir, name, "vert.glsl", vs, strlen(vs));
   write_file(outdir, name, "frag.glsl", fs, strlen(fs));

   if (!glslang_compile_spirv(vs, GLSLANG_COMPILE_STAGE_VERTEX,
            &vs_spv, &vs_len))
      status = "vertex-fail";
   else
   {
      write_file(outdir, name, "vert.spv", vs_spv, vs_len * 4);
      if (!glslang_compile_spirv(fs, GLSLANG_COMPILE_STAGE_FRAGMENT,
               &fs_spv, &fs_len))
         status = "fragment-fail";
      else
         write_file(outdir, name, "frag.spv", fs_spv, fs_len * 4);
   }

   free(vs_spv);
   free(fs_spv);
   free(vs);
   free(fs);
   return status;
}

int main(int argc, char **argv)
{
   const char *outdir;
   char        manifest_path[1024];
   FILE       *manifest;
   char        line[4096];
   int         i;
   char        name[512];

   if (argc < 3)
   {
      fprintf(stderr, "usage: %s <outdir> <a.slang>... | -\n", argv[0]);
      return 2;
   }
   outdir = argv[1];
   snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.txt", outdir);
   manifest = fopen(manifest_path, "w");
   if (!manifest)
   {
      fprintf(stderr, "cannot write %s\n", manifest_path);
      return 2;
   }

   if (argc == 3 && !strcmp(argv[2], "-"))
   {
      while (fgets(line, sizeof(line), stdin))
      {
         size_t n = strlen(line);
         const char *status;
         while (n && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = '\0';
         if (!n)
            continue;
         status = dump_one(outdir, line);
         dump_name(line, name, sizeof(name));
         fprintf(manifest, "%s %s %s\n", status, name, line);
         fflush(manifest);
      }
   }
   else
   {
      for (i = 2; i < argc; i++)
      {
         const char *status = dump_one(outdir, argv[i]);
         dump_name(argv[i], name, sizeof(name));
         fprintf(manifest, "%s %s %s\n", status, name, argv[i]);
         fflush(manifest);
      }
   }
   fclose(manifest);
   return 0;
}
