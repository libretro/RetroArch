/* gfx/drivers_shader/glslang_util.c's include cache.
 *
 * A leaf include that a shader names more than once is expanded once
 * and copied from the cache after that. The copy is checked against
 * the first expansion, the leaf is read from its source only once, and
 * every copy is to leave nothing behind, which ASan's leak check holds
 * to - with or without a caller-owned cache, and for the pragmas-only
 * read as for the full one.
 *
 * The sources are served from memory in place of video_shader_parse.c,
 * so nothing here touches the file system. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

#include <boolean.h>

#include "gfx/drivers_shader/glslang_util.h"

static unsigned failures = 0;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
         failures++; \
      } \
   } while (0)

void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }

/* ---- the sources ---- */
struct source
{
   const char *name;
   const char *text;
   unsigned    reads;
};

static struct source sources[] =
{
   { "root.slang",
     "#version 450\n"
     "#pragma name Root\n"
     "#include \"leaf.inc\"\n"
     "void a() {}\n"
     "#include \"leaf.inc\"\n"
     "void b() {}\n"
     "#include \"leaf.inc\"\n", 0 },
   { "leaf.inc",
     "#pragma parameter LEAF \"Leaf\" 1.0 0.0 2.0 0.1\n"
     "float leaf() { return LEAF; }\n", 0 }
};

static struct source *source_find(const char *name)
{
   unsigned i;
   for (i = 0; i < sizeof(sources) / sizeof(sources[0]); i++)
      if (!strcmp(sources[i].name, name))
         return &sources[i];
   return NULL;
}

bool video_shader_source_read(const char *ident, char **buf, int64_t *len)
{
   struct source *src = ident ? source_find(ident) : NULL;
   size_t         n;
   if (!src || !buf)
      return false;
   n = strlen(src->text);
   if (!(*buf = (char*)malloc(n + 1)))
      return false;
   memcpy(*buf, src->text, n + 1);
   if (len)
      *len = (int64_t)n;
   src->reads++;
   return true;
}

bool video_shader_source_resolve(const char *parent, const char *name,
      char *s, size_t len)
{
   (void)parent;
   if (!name || !*name || !s || !len || strlen(name) >= len)
      return false;
   strcpy(s, name);
   return true;
}

const char *video_shader_source_ident_name(const char *ident)
{
   return (ident && *ident) ? ident : NULL;
}

bool video_shader_source_ident_is_slang(const char *ident)
{
   size_t n = ident ? strlen(ident) : 0;
   return n > 6 && !strcmp(ident + n - 6, ".slang");
}

/* ---- the lanes ---- */

/* How many lines of @buf are exactly @line. */
static unsigned count_lines(const struct shader_line_buf *buf,
      const char *line)
{
   size_t   i;
   unsigned n = 0;
   for (i = 0; i < buf->num_lines; i++)
   {
      const char *l = shader_line_buf_get(buf, i);
      if (l && !strcmp(l, line))
         n++;
   }
   return n;
}

static void reset_reads(void)
{
   unsigned i;
   for (i = 0; i < sizeof(sources) / sizeof(sources[0]); i++)
      sources[i].reads = 0;
}

static void lane_expand(const char *tag, void *cache, unsigned reads)
{
   struct shader_line_buf out;

   reset_reads();
   CHECK(shader_line_buf_init(&out), "line buffer");
   CHECK(glslang_read_shader_file_cached("root.slang", &out, true, false,
            cache), "the root expands");
   CHECK(count_lines(&out, "float leaf() { return LEAF; }") == 3,
         "each include of the leaf expands to its lines");
   CHECK(count_lines(&out, "void b() {}") == 1, "the root's own lines");
   CHECK(source_find("leaf.inc")->reads == reads,
         "the leaf is read at most once, and copied after that");
   shader_line_buf_free(&out);
   printf("      %-40s done\n", tag);
}

static void lane_pragmas(const char *tag, void *cache, unsigned reads)
{
   struct shader_line_buf out;

   reset_reads();
   CHECK(shader_line_buf_init(&out), "line buffer");
   CHECK(glslang_read_shader_pragmas_cached("root.slang", &out, cache),
         "the root's pragmas expand");
   CHECK(count_lines(&out,
            "#pragma parameter LEAF \"Leaf\" 1.0 0.0 2.0 0.1") == 3,
         "each include of the leaf gives its pragma");
   CHECK(count_lines(&out, "float leaf() { return LEAF; }") == 0,
         "and nothing else of it");
   CHECK(source_find("leaf.inc")->reads == reads,
         "the leaf is read at most once, and copied after that");
   shader_line_buf_free(&out);
   printf("      %-40s done\n", tag);
}

int main(void)
{
   void *cache;

   lane_expand("expand, cache scoped to the call", NULL, 1);
   lane_pragmas("pragmas, cache scoped to the call", NULL, 1);

   if ((cache = glslang_include_cache_new()))
   {
      /* The caller's cache outlives a root: the second root reads
       * the leaf from it, not from its source. */
      lane_expand("expand, caller's cache", cache, 1);
      lane_pragmas("pragmas, caller's cache", cache, 0);
      lane_expand("expand again, caller's cache", cache, 0);
      glslang_include_cache_free(cache);
   }
   else
      CHECK(0, "a caller's cache");

   if (failures)
   {
      printf("[fail] slang_include_cache_test: %u check(s) failed\n", failures);
      return 1;
   }
   printf("[pass] slang_include_cache_test\n");
   return 0;
}
