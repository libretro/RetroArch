/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (slang_blob_reflect_check.c).
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


/* Holds the filter chain's reflection built from the compiler's tables
 * (slang_reflect_blobs) against the one built by SPIRV-Cross from the
 * same words (slang_reflect_spirv), over every pass of a corpus dump:
 * the slang_reflection the drivers bind by must come out the same
 * whichever way it was read.  Each pass is tried twice: rslc's own
 * modules, and glslang's modules read back through rslc_compile_spirv
 * (what a cache hit yields).  A pass that names a texture no preset
 * is here to define refuses both ways, and that is compared too.
 *
 *   slang_blob_reflect_check <dumpdir>
 */

#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gfx/shaders/rslc.h>
#include "gfx/drivers_shader/slang_process.h"
#include "gfx/drivers_shader/slang_blob.h"
#include "gfx/video_shader_parse.h"

/* ---- what slang_process.c reads of the frontend ---- */
static char last_error[512];
void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vsnprintf(last_error, sizeof(last_error), fmt, ap);
   va_end(ap);
}
bool spirv_cache_compute_hash(const char *v, const char *f, const char *c, char *h)
{ (void)v; (void)f; (void)c; strcpy(h, "0"); return true; }
bool spirv_cache_load(const char *h, struct glslang_output *o) { (void)h; (void)o; return false; }
bool spirv_cache_save(const char *h, const struct glslang_output *o) { (void)h; (void)o; return true; }
bool video_shader_source_read(const char *i, char **b, int64_t *l) { (void)i; (void)b; (void)l; return false; }
bool video_shader_source_resolve(const char *p, const char *n, char *s, size_t l)
{ (void)p; (void)n; (void)s; (void)l; return false; }
const char *video_shader_source_ident_name(const char *i) { return i; }
bool video_shader_source_ident_is_slang(const char *i) { (void)i; return true; }
void runloop_msg_queue_push(const char *msg, size_t len, unsigned prio,
      unsigned duration, bool flush, char *title,
      enum message_queue_icon icon, enum message_queue_category category)
{
   (void)msg; (void)len; (void)prio; (void)duration; (void)flush;
   (void)title; (void)icon; (void)category;
}

static unsigned diffs, both_refused, passes;

static char *read_file(const char *path, size_t *len)
{
   FILE *f = fopen(path, "rb");
   char *buf;
   long  n;
   if (!f)
      return NULL;
   fseek(f, 0, SEEK_END);
   n = ftell(f);
   fseek(f, 0, SEEK_SET);
   buf = (char*)malloc((size_t)n + 1);
   if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n)
   {
      free(buf);
      fclose(f);
      return NULL;
   }
   fclose(f);
   buf[n] = '\0';
   *len   = (size_t)n;
   return buf;
}

static void mismatch(const char *pass, const char *what)
{
   printf("MISMATCH %s: %s\n", pass, what);
   diffs++;
}

/* The parameter names a stage declares, in order, into the semantic
 * map, as the chain does from the preset's parameter list. */
static void add_parameters(slang_semantic_name_map *map, const char *src, unsigned *n)
{
   const char *p = src;
   while ((p = strstr(p, "#pragma parameter ")))
   {
      char id[64];
      unsigned i = 0;
      p += 18;
      while (*p == ' ' || *p == '\t')
         p++;
      while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' && i < sizeof(id) - 1)
         id[i++] = *p++;
      id[i] = '\0';
      if (i && slang_semantic_name_map_set_unique(map, id, NULL,
               SLANG_SEMANTIC_FLOAT_PARAMETER, *n))
         (*n)++;
   }
}

/* The samplers a stage declares under names no builtin semantic owns
 * become user textures, as a preset's LUT list would make them, so the
 * pass reflects instead of refusing on the first unknown name. */
static bool builtin_texture_name(const char *id)
{
   static const char *const prefixes[] = {
      "Source", "Original", "OriginalHistory", "PassOutput", "PassFeedback", "User"
   };
   unsigned i;
   for (i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++)
   {
      size_t n = strlen(prefixes[i]);
      if (!strncmp(id, prefixes[i], n))
      {
         const char *rest = id + n;
         bool digits = true;
         while (*rest)
            if (*rest < '0' || *rest > '9')
            {
               digits = false;
               break;
            }
            else
               rest++;
         if (digits)
            return true;
      }
   }
   return false;
}

static void add_samplers(slang_texture_semantic_name_map *tex,
      slang_texture_semantic_name_map *texu, const char *src, unsigned *n)
{
   const char *p = src;
   while ((p = strstr(p, "sampler2D ")))
   {
      char id[64];
      unsigned i = 0;
      p += 10;
      while (*p == ' ' || *p == '\t')
         p++;
      while (*p && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')
               || (*p >= '0' && *p <= '9') || *p == '_') && i < sizeof(id) - 1)
         id[i++] = *p++;
      id[i] = '\0';
      if (i && !builtin_texture_name(id)
            && slang_texture_semantic_name_map_set_unique(tex, id, NULL,
               SLANG_TEXTURE_SEMANTIC_USER, *n))
      {
         slang_texture_semantic_name_map_set_unique(texu, id, "Size",
               SLANG_TEXTURE_SEMANTIC_USER, *n);
         (*n)++;
      }
   }
}

static bool same_meta(const slang_semantic_meta *a, const slang_semantic_meta *b)
{
   return a->uniform == b->uniform && a->push_constant == b->push_constant
      && a->ubo_offset == b->ubo_offset
      && a->push_constant_offset == b->push_constant_offset
      && a->num_components == b->num_components;
}

static bool same_tex(const slang_texture_semantic_meta *a, const slang_texture_semantic_meta *b)
{
   return a->texture == b->texture && a->uniform == b->uniform
      && a->push_constant == b->push_constant && a->binding == b->binding
      && a->stage_mask == b->stage_mask && a->ubo_offset == b->ubo_offset
      && a->push_constant_offset == b->push_constant_offset;
}

static void compare(const char *pass, const char *how,
      const slang_reflection *a, const slang_reflection *b)
{
   char what[256];
   unsigned i, j;
   if (a->ubo_size != b->ubo_size || a->push_constant_size != b->push_constant_size
         || a->ubo_binding != b->ubo_binding || a->ubo_stage_mask != b->ubo_stage_mask
         || a->push_constant_stage_mask != b->push_constant_stage_mask)
   {
      snprintf(what, sizeof(what), "%s: blocks (ubo %u/%u b%u/%u m%x/%x, push %u/%u m%x/%x)", how,
            (unsigned)a->ubo_size, (unsigned)b->ubo_size, a->ubo_binding, b->ubo_binding,
            a->ubo_stage_mask, b->ubo_stage_mask, (unsigned)a->push_constant_size,
            (unsigned)b->push_constant_size, a->push_constant_stage_mask, b->push_constant_stage_mask);
      mismatch(pass, what);
   }
   for (i = 0; i < SLANG_NUM_SEMANTICS; i++)
      if (!same_meta(&a->semantics[i], &b->semantics[i]))
      {
         snprintf(what, sizeof(what), "%s: semantic %u", how, i);
         mismatch(pass, what);
      }
   if (a->num_float_parameters != b->num_float_parameters)
   {
      snprintf(what, sizeof(what), "%s: %u vs %u parameters", how,
            (unsigned)a->num_float_parameters, (unsigned)b->num_float_parameters);
      mismatch(pass, what);
   }
   else
      for (i = 0; i < a->num_float_parameters; i++)
         if (!same_meta(&a->semantic_float_parameters[i], &b->semantic_float_parameters[i]))
         {
            snprintf(what, sizeof(what), "%s: parameter %u", how, i);
            mismatch(pass, what);
         }
   for (i = 0; i < SLANG_NUM_TEXTURE_SEMANTICS; i++)
   {
      if (a->semantic_textures[i].size != b->semantic_textures[i].size)
      {
         snprintf(what, sizeof(what), "%s: texture semantic %u: %u vs %u entries", how, i,
               (unsigned)a->semantic_textures[i].size, (unsigned)b->semantic_textures[i].size);
         mismatch(pass, what);
         continue;
      }
      for (j = 0; j < a->semantic_textures[i].size; j++)
         if (!same_tex(&a->semantic_textures[i].data[j], &b->semantic_textures[i].data[j]))
         {
            snprintf(what, sizeof(what), "%s: texture semantic %u #%u", how, i, j);
            mismatch(pass, what);
         }
   }
}

/* Reflects @vs / @fs both ways and compares; the blobs must carry
 * tables. */
static void check_pair(const char *pass, const char *how,
      struct slang_blob *vs, struct slang_blob *fs,
      const slang_semantic_name_map *params,
      const slang_texture_semantic_name_map *tex,
      const slang_texture_semantic_name_map *texu)
{
   slang_reflection a, b;
   bool ra, rb;
   if (!slang_reflection_init(&a) || !slang_reflection_init(&b))
      return;
   /* late in a long chain, so PassOutputN and feedback names validate */
   a.pass_number = b.pass_number = GFX_MAX_SHADERS - 1;
   a.texture_semantic_map = b.texture_semantic_map = tex;
   a.texture_semantic_uniform_map = b.texture_semantic_uniform_map = texu;
   a.semantic_map = b.semantic_map = params;
   ra = slang_reflect_blobs(vs, fs, &a);
   rb = slang_reflect_spirv(vs->words, vs->nwords, fs->words, fs->nwords, &b);
   if (ra != rb)
   {
      char what[600];
      snprintf(what, sizeof(what), "%s: tables %s, SPIRV-Cross %s (%s)", how,
            ra ? "accept" : "refuse", rb ? "accepts" : "refuses", last_error);
      mismatch(pass, what);
   }
   else if (ra)
      compare(pass, how, &a, &b);
   else
   {
      both_refused++;
      if (getenv("SLANG_BLOB_REFLECT_VERBOSE"))
         printf("REFUSED %s: %s\n", pass, last_error);
   }
   slang_reflection_free(&a);
   slang_reflection_free(&b);
}

static struct slang_blob *compile(const char *src, size_t len, enum rslc_stage stage, char *err)
{
   struct rslc_opts   opts;
   struct rslc_result res;
   memset(&opts, 0, sizeof(opts));
   opts.dst     = RSLC_DST_SPIRV;
   opts.err     = err;
   opts.err_cap = 256;
   err[0]       = '\0';
   if (!rslc_compile(src, len, stage, &opts, &res))
      return NULL;
   return slang_blob_adopt_result(&res, &opts);
}

static struct slang_blob *ingest(const char *path, enum rslc_stage stage, char *err)
{
   struct rslc_opts   opts;
   struct rslc_result res;
   size_t len;
   char  *words = read_file(path, &len);
   struct slang_blob *b;
   if (!words || (len & 3))
   {
      free(words);
      return NULL;
   }
   memset(&opts, 0, sizeof(opts));
   opts.dst     = RSLC_DST_SPIRV;
   opts.err     = err;
   opts.err_cap = 256;
   err[0]       = '\0';
   b = rslc_compile_spirv((const uint32_t*)words, len / 4, stage, &opts, &res)
      ? slang_blob_adopt_result(&res, &opts) : NULL;
   free(words);
   return b;
}

static void check_pass(const char *dir, const char *name)
{
   char   vpath[1024], fpath[1024], err[256];
   char  *vsrc, *fsrc;
   size_t vlen, flen;
   unsigned nparams = 0, ntex = 0;
   slang_semantic_name_map params;
   slang_texture_semantic_name_map tex, texu;
   struct slang_blob *vs, *fs;

   snprintf(vpath, sizeof(vpath), "%s/%s.vert.glsl", dir, name);
   snprintf(fpath, sizeof(fpath), "%s/%s.frag.glsl", dir, name);
   vsrc = read_file(vpath, &vlen);
   fsrc = read_file(fpath, &flen);
   if (!vsrc || !fsrc)
   {
      free(vsrc);
      free(fsrc);
      return;
   }
   /* a stage glslang rejected has no .spv beside it and is not a gate */
   snprintf(vpath, sizeof(vpath), "%s/%s.vert.spv", dir, name);
   snprintf(fpath, sizeof(fpath), "%s/%s.frag.spv", dir, name);
   {
      FILE *a = fopen(vpath, "rb"), *b = fopen(fpath, "rb");
      bool have = a && b;
      if (a) fclose(a);
      if (b) fclose(b);
      if (!have)
      {
         free(vsrc);
         free(fsrc);
         return;
      }
   }
   passes++;
   memset(&params, 0, sizeof(params));
   memset(&tex, 0, sizeof(tex));
   memset(&texu, 0, sizeof(texu));
   add_parameters(&params, vsrc, &nparams);
   add_parameters(&params, fsrc, &nparams);
   add_samplers(&tex, &texu, vsrc, &ntex);
   add_samplers(&tex, &texu, fsrc, &ntex);

   vs = compile(vsrc, vlen, RSLC_STAGE_VERTEX, err);
   fs = vs ? compile(fsrc, flen, RSLC_STAGE_FRAGMENT, err) : NULL;
   if (!vs || !fs)
      mismatch(name, err);
   else
      check_pair(name, "rslc modules", vs, fs, &params, &tex, &texu);
   slang_blob_unref(vs);
   slang_blob_unref(fs);

   vs = ingest(vpath, RSLC_STAGE_VERTEX, err);
   fs = vs ? ingest(fpath, RSLC_STAGE_FRAGMENT, err) : NULL;
   if (!vs || !fs)
      mismatch(name, err);
   else
      check_pair(name, "glslang modules through the ingest", vs, fs, &params, &tex, &texu);
   slang_blob_unref(vs);
   slang_blob_unref(fs);

   slang_semantic_name_map_free(&params);
   slang_texture_semantic_name_map_free(&tex);
   slang_texture_semantic_name_map_free(&texu);
   free(vsrc);
   free(fsrc);
}

int main(int argc, char **argv)
{
   DIR *d;
   struct dirent *ent;
   if (argc != 2)
   {
      fprintf(stderr, "usage: slang_blob_reflect_check <dumpdir>\n");
      return 2;
   }
   d = opendir(argv[1]);
   if (!d)
   {
      fprintf(stderr, "cannot open %s\n", argv[1]);
      return 2;
   }
   while ((ent = readdir(d)))
   {
      size_t n = strlen(ent->d_name);
      if (n > 10 && !strcmp(ent->d_name + n - 10, ".frag.glsl"))
      {
         char name[512];
         if (n - 10 >= sizeof(name))
            continue;
         memcpy(name, ent->d_name, n - 10);
         name[n - 10] = '\0';
         check_pass(argv[1], name);
      }
   }
   closedir(d);
   printf("%u passes, %u mismatches, %u refused both ways\n", passes, diffs, both_refused);
   return diffs ? 1 : 0;
}
