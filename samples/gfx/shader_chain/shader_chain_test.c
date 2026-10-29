/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (shader_chain_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Counts what a chain of passes costs to compile through the real
 * slang_process.c: a .slang two passes name compiles once, and a chain
 * built while the one it replaces is still alive - append, remove,
 * reload - compiles only the passes the live chain does not have.  The
 * count is the blob table's: every compile lists two stages, every
 * reuse lists none and hands the live chain's words out by reference.
 *
 *   shader_chain_test
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx/drivers_shader/slang_process.h"
#include "gfx/drivers_shader/slang_blob.h"
#include "gfx/video_shader_parse.h"

static unsigned failures;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
         failures++; \
      } \
   } while (0)

/* ---- what slang_process.c reads of the frontend ---- */
void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }
/* the refusal line, for the lane that holds what it says */
static char last_err[512];
void RARCH_ERR(const char *fmt, ...)
{
   char    line[512];
   va_list ap;
   va_start(ap, fmt);
   vsnprintf(line, sizeof(line), fmt, ap);
   va_end(ap);
   if (strstr(line, "rslc rejected"))
      memcpy(last_err, line, sizeof(line));
   fputs(line, stdout);
}
void runloop_msg_queue_push(const char *msg, size_t len, unsigned prio,
      unsigned duration, bool flush, char *title,
      enum message_queue_icon icon, enum message_queue_category category)
{
   (void)msg; (void)len; (void)prio; (void)duration; (void)flush;
   (void)title; (void)icon; (void)category;
}

/* The disk cache misses; the key it would file under is what the blob
 * table lists live stages by, so it tells the sources apart. */
static unsigned long key_sum(const char *s, unsigned long h)
{
   while (*s)
      h = h * 33 + (unsigned char)*s++;
   return h;
}

bool spirv_cache_compute_hash(const char *vertex_source,
      const char *fragment_source, const char *compiler, char *hash_out)
{
   sprintf(hash_out, "%08lx%08lx%08lx%08lx",
         key_sum(vertex_source, 5381) & 0xfffffffful,
         key_sum(fragment_source, 5381) & 0xfffffffful,
         key_sum(compiler, 5381) & 0xfffffffful,
         (unsigned long)(strlen(vertex_source) + strlen(fragment_source))
            & 0xfffffffful);
   return true;
}
bool spirv_cache_load(const char *h, struct glslang_output *o) { (void)h; (void)o; return false; }
bool spirv_cache_save(const char *h, const struct glslang_output *o) { (void)h; (void)o; return true; }

/* ---- the sources, served from memory ---- */
#define STAGE_VS \
   "#pragma stage vertex\n" \
   "layout(location = 0) in vec4 Position;\n" \
   "layout(location = 1) in vec2 TexCoord;\n" \
   "layout(location = 0) out vec2 vTexCoord;\n" \
   "void main() { gl_Position = global.MVP * Position; vTexCoord = TexCoord; }\n"
#define HEAD \
   "#version 450\n" \
   "layout(std140, set = 0, binding = 0) uniform UBO { mat4 MVP; } global;\n" \
   "layout(set = 0, binding = 2) uniform sampler2D Source;\n"
#define STAGE_FS(expr) \
   "#pragma stage fragment\n" \
   "layout(location = 0) in vec2 vTexCoord;\n" \
   "layout(location = 0) out vec4 FragColor;\n" \
   "void main() { FragColor = " expr "; }\n"

struct source
{
   const char *name;
   const char *text;
};

static const struct source sources[] =
{
   { "a.slang", HEAD STAGE_VS STAGE_FS("texture(Source, vTexCoord)") },
   { "b.slang", HEAD STAGE_VS STAGE_FS("texture(Source, vTexCoord) * 0.5") },
   { "c.slang", HEAD STAGE_VS STAGE_FS("vec4(vTexCoord, 0.0, 1.0)") },
   { "d.slang", HEAD STAGE_VS STAGE_FS("texture(Source, vTexCoord).bgra") },
   /* a fragment stage rslc refuses: an undeclared name */
   { "bad.slang", HEAD STAGE_VS STAGE_FS("texture(Source, nowhere)") }
};

bool video_shader_source_read(const char *ident, char **buf, int64_t *len)
{
   unsigned i;
   for (i = 0; i < sizeof(sources) / sizeof(sources[0]); i++)
   {
      size_t n;
      if (!ident || strcmp(sources[i].name, ident))
         continue;
      n = strlen(sources[i].text);
      if (!buf || !(*buf = (char*)malloc(n + 1)))
         return false;
      memcpy(*buf, sources[i].text, n + 1);
      if (len)
         *len = (int64_t)n;
      return true;
   }
   return false;
}
bool video_shader_source_resolve(const char *p, const char *n, char *s, size_t l)
{ (void)p; (void)n; (void)s; (void)l; return false; }
const char *video_shader_source_ident_name(const char *i) { return i; }
bool video_shader_source_ident_is_slang(const char *i) { (void)i; return true; }

/* ---- a chain: the compile output of each pass, held as a chain holds
 * its blobs ---- */
#define MAX_PASSES 8

struct chain
{
   glslang_output pass[MAX_PASSES];
   unsigned       passes;
};

static void chain_free(struct chain *c)
{
   unsigned i;
   for (i = 0; i < c->passes; i++)
      glslang_output_free(&c->pass[i]);
   c->passes = 0;
}

/* Builds @c from @names; returns the stages compiled (not reused). */
static size_t chain_build(struct chain *c, const char *const *names,
      unsigned n)
{
   size_t   live0, live1;
   unsigned reused0, reused1, i;

   slang_blob_table_stats(&live0, &reused0);
   c->passes = 0;
   for (i = 0; i < n; i++)
   {
      if (!glslang_compile_shader(names[i], &c->pass[i]))
      {
         printf("FAIL compile %s\n", names[i]);
         failures++;
         break;
      }
      c->passes++;
   }
   slang_blob_table_stats(&live1, &reused1);
   printf("  [%u passes] compiled %u stages, reused %u, live %u\n",
         n, (unsigned)(live1 - live0), reused1 - reused0, (unsigned)live1);
   return live1 - live0;
}

static bool same_stages(const glslang_output *x, const glslang_output *y)
{
   return x->vertex == y->vertex && x->fragment == y->fragment
      && x->vertex_blob == y->vertex_blob
      && x->fragment_blob == y->fragment_blob;
}

int main(void)
{
   static const char *const abac[] = { "a.slang", "b.slang", "a.slang", "c.slang" };
   static const char *const abacb[] = { "a.slang", "b.slang", "a.slang", "c.slang", "b.slang" };
   static const char *const ac[]   = { "a.slang", "c.slang" };
   static const char *const dac[]  = { "d.slang", "a.slang", "c.slang" };
   static const char *const dabc[] = { "d.slang", "a.slang", "b.slang", "c.slang" };
   struct chain live, next;
   size_t   n;
   unsigned reused;

   memset(&live, 0, sizeof(live));
   memset(&next, 0, sizeof(next));

   /* one preset naming a.slang twice: three compiles, one reuse */
   printf("load a b a c\n");
   CHECK(chain_build(&live, abac, 4) == 6, "a preset compiles each distinct .slang once");
   CHECK(same_stages(&live.pass[0], &live.pass[2]), "the second a.slang holds the first one's blobs");
   CHECK(live.pass[0].vertex_blob && retro_atomic_load_relaxed_int(&live.pass[0].vertex_blob->refs) == 2, "two passes, one blob, two references");
   CHECK(live.pass[0].vertex_blob->refl != NULL, "the shared blob carries its tables");

   /* append b while the chain is live: nothing compiles */
   printf("append b\n");
   CHECK(chain_build(&next, abacb, 5) == 0, "appending a pass the live chain has compiles nothing");
   CHECK(same_stages(&next.pass[1], &live.pass[1]), "the new chain holds the live chain's blobs");
   chain_free(&live);
   live = next;
   memset(&next, 0, sizeof(next));
   slang_blob_table_stats(&n, &reused);
   CHECK(n == 6, "freeing the outgoing chain keeps every blob the new one shares");

   /* remove passes: nothing compiles, the dropped passes' blobs stay
    * only while something holds them */
   printf("remove b, b, a\n");
   CHECK(chain_build(&next, ac, 2) == 0, "removing passes compiles nothing");
   chain_free(&live);
   live = next;
   memset(&next, 0, sizeof(next));
   slang_blob_table_stats(&n, &reused);
   CHECK(n == 4, "the removed pass's blobs left the table with their last reference");

   /* prepend a new pass: one pass compiles, the rest is reused */
   printf("prepend d\n");
   CHECK(chain_build(&next, dac, 3) == 2, "a new pass compiles once; the live passes are reused");
   CHECK(same_stages(&next.pass[1], &live.pass[0]), "pass order does not matter to reuse");
   chain_free(&live);
   live = next;
   memset(&next, 0, sizeof(next));

   /* reload the same preset over itself: nothing compiles */
   printf("reload d a c\n");
   CHECK(chain_build(&next, dac, 3) == 0, "reloading a live preset compiles nothing");
   chain_free(&next);
   chain_free(&live);
   slang_blob_table_stats(&n, &reused);
   CHECK(n == 0, "no chain, no blobs");

   /* with nothing live the same preset compiles again */
   printf("load d a b c cold\n");
   CHECK(chain_build(&live, dabc, 4) == 8, "a cold load compiles every pass");
   chain_free(&live);
   slang_blob_table_stats(&n, &reused);
   CHECK(n == 0, "no chain, no blobs");

   /* a pass compiled detached - on a thread that is not the chain's -
    * lists nothing until the chain's thread offers it, and the offer
    * is what the chain's own compile then takes */
   printf("detached a, offer, load a\n");
   {
      glslang_output out;
      struct slang_blob *vs, *fs;
      char key[33];
      CHECK(slang_compile_pass_detached("a.slang", &out, key), "detached compile");
      slang_blob_table_stats(&n, &reused);
      CHECK(n == 0, "a detached compile lists nothing");
      CHECK(strlen(key) == 32, "the detached compile names the pass's key");
      vs = slang_blob_ref(out.vertex_blob);
      fs = slang_blob_ref(out.fragment_blob);
      glslang_output_free(&out);
      slang_blob_offer(key, vs, fs);
      slang_blob_table_stats(&n, &reused);
      CHECK(n == 2, "the offer lists both stages");
      CHECK(chain_build(&live, abac, 1) == 0, "the chain's compile takes the offered stages");
      CHECK(live.pass[0].vertex_blob == vs && live.pass[0].fragment_blob == fs,
            "by reference");
      /* a second offer of a listed pass is dropped, references and all */
      CHECK(slang_compile_pass_detached("a.slang", &out, key), "detached compile again");
      vs = slang_blob_ref(out.vertex_blob);
      glslang_output_free(&out);
      slang_blob_offer(key, vs, NULL);
      slang_blob_table_stats(&n, &reused);
      CHECK(n == 2, "an offer of a listed pass lists nothing");
      chain_free(&live);
      slang_blob_table_stats(&n, &reused);
      CHECK(n == 2, "the offer holds the stages after the chain let go");
      slang_blob_offers_release();
      slang_blob_table_stats(&n, &reused);
      CHECK(n == 0, "released offers leave the table");
   }

   /* a stage rslc refuses: the line names the stage, the file and the
    * pass key, then rslc's reason */
   {
      glslang_output out;
      last_err[0] = '\0';
      printf("load bad (refused)\n");
      CHECK(!glslang_compile_shader("bad.slang", &out), "a refused stage fails the pass");
      CHECK(strstr(last_err, "rslc rejected the fragment stage of \"bad.slang\" (pass ") != NULL,
            "the refusal names the stage and the file");
      CHECK(strstr(last_err, "nowhere") != NULL, "the refusal carries rslc's reason");
      slang_blob_table_stats(&n, &reused);
      CHECK(n == 0, "a refused pass lists nothing");
   }

   printf("%s: %u failure(s)\n", failures ? "FAIL" : "ok", failures);
   return failures ? 1 : 0;
}
