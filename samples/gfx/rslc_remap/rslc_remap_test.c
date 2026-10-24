/* rslc's frame-rectangle remap (opts.remap): a stage compiled to read
 * its frame where it lies in a larger texture.
 *
 * The shaders of samples/gfx/slang_rect/shaders are compiled with the
 * remap for each way a preset can sample its frame - nearest or
 * linear, each wrap - the rectangle in the push constants, or in the
 * uniform block where those are full:
 *  - what comes back is what is expected of that shader and that
 *    sampling (rewritten, left alone, or refused);
 *  - it is what gfx/drivers_shader/slang_rect.c says of rslc's own
 *    module of the stage, the rewrite the remap stands in for;
 *  - every rewritten stage prints as GLSL, GLSL ES, HLSL and MSL;
 *  - placed past what another stage's block holds, the rectangle's
 *    members take the offsets slang_rect.c gives them.
 *
 * With RSLC_REMAP_TEST_GL (Linux, EGL), each rewritten fragment stage
 * is then drawn from rslc's GLSL: the stage as it was, reading a w x h
 * texture, and as rewritten, reading that texture's texels at (x, y)
 * of a larger one filled with noise around them. Read in floats over
 * spans and scales that put coordinates outside the frame, on texel
 * edges and on texel centres, the two must match bit for bit - linear
 * filtering with border wrap to within one step of 8 bits, which is
 * how far its blend with the border may round otherwise. --require-gl
 * fails when no context can be had rather than skipping.
 *
 * --corpus <file>: a list of .glsl stages (one path a line, named
 * *.vert.glsl / *.frag.glsl), each held to slang_rect.c's verdict on
 * rslc's module for every sampling, as the first pass and as a later
 * one. No CI lane: it needs a corpus on disk. */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <gfx/shaders/rslc.h>

#include "../../../gfx/drivers_shader/slang_rect.h"

#ifdef RSLC_REMAP_TEST_GL
#define GL_GLEXT_PROTOTYPES 1
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#define N_ELEMS(a) (sizeof(a) / sizeof((a)[0]))
#define SHADER_DIR "../slang_rect/shaders/"

/* What a shader should come back as, sampled each way. */
enum expect
{
   X_ALL = 0,     /* rewritten every way */
   X_SAMPLES,     /* samples: rewritten but for linear, repeat or mirror */
   X_NONE,        /* never reads the frame: left alone */
   X_REFUSED      /* reads it as cannot be kept: refused */
};

struct shader
{
   const char *file;
   bool        source;
   enum expect expect;
   bool        draw;
};

static const struct shader shaders[] = {
   { "tex.frag",       true,  X_SAMPLES, true  },
   { "offset.frag",    true,  X_SAMPLES, true  },
   { "lod.frag",       true,  X_SAMPLES, true  },
   { "bias.frag",      true,  X_SAMPLES, true  },
   { "grad.frag",      true,  X_SAMPLES, true  },
   { "gather.frag",    true,  X_ALL,     true  },
   { "gather2.frag",   true,  X_ALL,     true  },
   { "gatheroff.frag", true,  X_ALL,     true  },
   { "fetch.frag",     true,  X_ALL,     true  },
   { "fetchoff.frag",  true,  X_ALL,     true  },
   { "size.frag",      true,  X_ALL,     true  },
   { "helper.frag",    true,  X_SAMPLES, true  },
   { "nested.frag",    true,  X_SAMPLES, true  },
   { "original.frag",  false, X_SAMPLES, true  },
   { "lut.frag",       true,  X_NONE,    false },
   { "levels.frag",    true,  X_REFUSED, false },
   { "proj.frag",      true,  X_REFUSED, false },
   { "full.frag",      true,  X_SAMPLES, false },
   { "nopush.frag",    true,  X_SAMPLES, false },
   { "stock.vert",     true,  X_NONE,    false },
   /* Source is the frame only in the first pass. */
   { "tex.frag",       false, X_NONE,    false }
};

struct sampling
{
   const char          *name;
   bool                 linear;
   enum slang_rect_wrap wrap;
};

static const struct sampling samplings[] = {
   { "nearest, edge",   false, SLANG_RECT_WRAP_EDGE   },
   { "nearest, border", false, SLANG_RECT_WRAP_BORDER },
   { "nearest, repeat", false, SLANG_RECT_WRAP_REPEAT },
   { "nearest, mirror", false, SLANG_RECT_WRAP_MIRROR },
   { "linear, edge",    true,  SLANG_RECT_WRAP_EDGE   },
   { "linear, border",  true,  SLANG_RECT_WRAP_BORDER },
   { "linear, repeat",  true,  SLANG_RECT_WRAP_REPEAT },
   { "linear, mirror",  true,  SLANG_RECT_WRAP_MIRROR }
};

static const char *const frame_names[3] = {
   "Original", "OriginalHistory0", "Source" };

static int failures;

static void fail(const char *fmt, ...)
{
   va_list ap;
   printf("[FAIL] ");
   va_start(ap, fmt);
   vprintf(fmt, ap);
   va_end(ap);
   failures++;
}

static char *read_file(const char *path, size_t *len)
{
   FILE *f = fopen(path, "rb");
   long  n;
   char *s;
   if (!f)
      return NULL;
   if (fseek(f, 0, SEEK_END) || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET))
   {
      fclose(f);
      return NULL;
   }
   if ((s = (char*)malloc((size_t)n + 1)))
   {
      if (fread(s, 1, (size_t)n, f) != (size_t)n)
      {
         free(s);
         s = NULL;
      }
      else
      {
         s[n] = '\0';
         *len = (size_t)n;
      }
   }
   fclose(f);
   return s;
}

static enum slang_rect_result expected(const struct shader *s,
      const struct sampling *m)
{
   switch (s->expect)
   {
      case X_ALL:
         return SLANG_RECT_REWRITTEN;
      case X_SAMPLES:
         return (    m->linear
                  && (m->wrap == SLANG_RECT_WRAP_REPEAT
                     || m->wrap == SLANG_RECT_WRAP_MIRROR))
            ? SLANG_RECT_UNSUPPORTED : SLANG_RECT_REWRITTEN;
      case X_NONE:
         return SLANG_RECT_UNCHANGED;
      case X_REFUSED:
      default:
         break;
   }
   return SLANG_RECT_UNSUPPORTED;
}

/* One compile of @src: the remap as @rm says (none for NULL), into
 * @dst. false with the message printed when rslc refuses the stage. */
static bool compile(const char *src, size_t len, bool vert, enum rslc_dst dst,
      unsigned version, const struct rslc_remap *rm, struct rslc_result *res,
      struct rslc_opts *o)
{
   static char err[256];
   memset(o, 0, sizeof(*o));
   o->dst     = dst;
   o->version = version;
   o->remap   = rm;
   o->err     = err;
   o->err_cap = sizeof(err);
   if (!rslc_compile(src, len, vert ? RSLC_STAGE_VERTEX : RSLC_STAGE_FRAGMENT,
            o, res))
   {
      printf("[info] rslc: %s\n", err);
      return false;
   }
   return true;
}

static void remap_for(struct rslc_remap *rm, const struct slang_rect_place *pl,
      bool source, const struct sampling *m)
{
   memset(rm, 0, sizeof(*rm));
   rm->samplers      = frame_names;
   rm->sampler_count = source ? 3 : 2;
   rm->rect_name     = SLANG_RECT_NAME_RECT;
   rm->clamp_name    = SLANG_RECT_NAME_CLAMP;
   rm->texels_name   = SLANG_RECT_NAME_TEXELS;
   rm->offset        = pl->offset;
   rm->set           = pl->set;
   rm->binding       = pl->binding;
   rm->where         = pl->where == SLANG_RECT_UBO ? RSLC_REMAP_UBO : RSLC_REMAP_PUSH;
   rm->linear        = m->linear;
   rm->wrap          = (enum rslc_remap_wrap)m->wrap;
}

static enum slang_rect_result verdict(enum rslc_remap_result r)
{
   switch (r)
   {
      case RSLC_REMAP_REWRITTEN: return SLANG_RECT_REWRITTEN;
      case RSLC_REMAP_UNCHANGED: return SLANG_RECT_UNCHANGED;
      default: break;
   }
   return SLANG_RECT_UNSUPPORTED;
}

/* @src, @len as the first pass (@source) or a later one, sampled as @m:
 * rslc's verdict, held to slang_rect.c's on rslc's module; and, when
 * rewritten, printed every way. -1 when the stage does not compile. */
static int remap_one(const char *name, const char *src, size_t len,
      bool vert, bool source, const struct sampling *m,
      enum slang_rect_result *out)
{
   static const struct { enum rslc_dst dst; unsigned version; } prints[] = {
      { RSLC_DST_GLSL, 450 }, { RSLC_DST_GLSL, 330 },
      { RSLC_DST_GLSL_ES, 310 }, { RSLC_DST_HLSL, 50 },
      { RSLC_DST_HLSL, 40 }, { RSLC_DST_MSL, 20000 } };
   struct rslc_opts        o;
   struct rslc_result      plain, res;
   struct rslc_remap       rm;
   struct slang_rect_place pl;
   enum slang_rect_result  want, got;
   uint32_t *out_spv = NULL;
   size_t    out_len = 0;
   unsigned  i;

   if (!compile(src, len, vert, RSLC_DST_SPIRV, 0, NULL, &plain, &o))
      return -1;
   if (!slang_rect_place_pass(plain.spirv, plain.spirv_len,
            plain.spirv, plain.spirv_len, &pl))
   {
      /* nothing to place: slang_rect.c refuses the stage */
      rslc_result_free(&plain, &o);
      *out = SLANG_RECT_UNSUPPORTED;
      return 0;
   }
   want = slang_rect_remap(plain.spirv, plain.spirv_len, source, m->linear,
         m->wrap, &pl, &out_spv, &out_len);
   free(out_spv);
   rslc_result_free(&plain, &o);

   remap_for(&rm, &pl, source, m);
   if (!compile(src, len, vert, RSLC_DST_SPIRV, 0, &rm, &res, &o))
   {
      fail("%s, %s: no module with the remap\n", name, m->name);
      return 0;
   }
   got = verdict(res.remap);
   rslc_result_free(&res, &o);
   if (got != want)
      fail("%s, %s%s: rslc says %d, slang_rect.c %d\n", name, m->name,
            source ? "" : ", a later pass", (int)got, (int)want);
   *out = got;
   if (got != SLANG_RECT_REWRITTEN)
      return 0;
   for (i = 0; i < N_ELEMS(prints); i++)
   {
      if (!compile(src, len, vert, prints[i].dst, prints[i].version, &rm, &res, &o))
      {
         fail("%s, %s: does not print as dst %d version %u\n", name, m->name,
               (int)prints[i].dst, prints[i].version);
         continue;
      }
      if (!res.text || !strstr(res.text, SLANG_RECT_NAME_TEXELS))
         fail("%s, %s: dst %d prints no rectangle\n", name, m->name,
               (int)prints[i].dst);
      rslc_result_free(&res, &o);
   }
   return 0;
}


/* The offset member @name of a block takes in module @w; ~0u when it
 * has none. */
static uint32_t member_offset(const uint32_t *w, size_t n, const char *name)
{
   size_t   p, len = strlen(name) + 1;
   uint32_t st = 0, mem = 0;
   bool     found = false;
   for (p = 5; p < n && !found; p += w[p] >> 16)
      if (     (w[p] & 0xffffu) == 6 /* OpMemberName */
            && (w[p] >> 16) >= 4
            && ((w[p] >> 16) - 3) * 4 >= len
            && !memcmp(&w[p + 3], name, len))
      {
         st    = w[p + 1];
         mem   = w[p + 2];
         found = true;
      }
   if (!found)
      return ~0u;
   for (p = 5; p < n; p += w[p] >> 16)
      if (     (w[p] & 0xffffu) == 72 /* OpMemberDecorate */
            && (w[p] >> 16) >= 5
            && w[p + 1] == st && w[p + 2] == mem && w[p + 3] == 35)
         return w[p + 4];
   return ~0u;
}

/* The rectangle where the caller places it past what the stage holds:
 * another stage's block may end later, and the members follow that. */
struct placement
{
   const char           *file;
   enum rslc_remap_where where;
   uint32_t              offset;
   enum rslc_remap_result want;
   uint32_t              rect;   /* the first member's offset */
};

static const struct placement placements[] = {
   { "tex.frag",    RSLC_REMAP_PUSH, 0,   RSLC_REMAP_REWRITTEN,   32  },
   { "tex.frag",    RSLC_REMAP_PUSH, 40,  RSLC_REMAP_REWRITTEN,   48  },
   { "tex.frag",    RSLC_REMAP_PUSH, 64,  RSLC_REMAP_REWRITTEN,   64  },
   { "tex.frag",    RSLC_REMAP_PUSH, 81,  RSLC_REMAP_UNSUPPORTED, 0   },
   { "nopush.frag", RSLC_REMAP_PUSH, 0,   RSLC_REMAP_REWRITTEN,   0   },
   { "nopush.frag", RSLC_REMAP_PUSH, 20,  RSLC_REMAP_REWRITTEN,   32  },
   { "full.frag",   RSLC_REMAP_UBO,  0,   RSLC_REMAP_REWRITTEN,   64  },
   { "full.frag",   RSLC_REMAP_UBO,  100, RSLC_REMAP_REWRITTEN,   112 }
};

static void placement_lanes(void)
{
   size_t i;
   for (i = 0; i < N_ELEMS(placements); i++)
   {
      const struct placement *pc = &placements[i];
      struct slang_rect_place pl;
      struct rslc_remap       rm;
      struct rslc_opts        o;
      struct rslc_result      res, plain;
      char   path[256];
      char  *src;
      size_t len = 0;
      uint32_t *out = NULL;
      size_t    out_len = 0;
      enum slang_rect_result want;
      strcpy(path, SHADER_DIR);
      strcat(path, pc->file);
      if (!(src = read_file(path, &len)))
         continue;
      pl.where   = pc->where == RSLC_REMAP_UBO ? SLANG_RECT_UBO : SLANG_RECT_PUSH;
      pl.set     = 0;
      pl.binding = 0;
      pl.offset  = pc->offset;
      remap_for(&rm, &pl, true, &samplings[0]);
      if (     compile(src, len, false, RSLC_DST_SPIRV, 0, NULL, &plain, &o))
      {
         want = slang_rect_remap(plain.spirv, plain.spirv_len, true, false,
               SLANG_RECT_WRAP_EDGE, &pl, &out, &out_len);
         if (want == SLANG_RECT_REWRITTEN && pc->want == RSLC_REMAP_REWRITTEN
               && member_offset(out, out_len, SLANG_RECT_NAME_RECT) != pc->rect)
            fail("%s past %u: slang_rect.c puts the rectangle at %u\n",
                  pc->file, pc->offset,
                  member_offset(out, out_len, SLANG_RECT_NAME_RECT));
         free(out);
         rslc_result_free(&plain, &o);
         if (verdict(pc->want) != want)
            fail("%s past %u: slang_rect.c says %d\n", pc->file, pc->offset, (int)want);
      }
      if (compile(src, len, false, RSLC_DST_SPIRV, 0, &rm, &res, &o))
      {
         if (res.remap != pc->want)
            fail("%s past %u: came back %d, want %d\n", pc->file, pc->offset,
                  (int)res.remap, (int)pc->want);
         else if (pc->want == RSLC_REMAP_REWRITTEN)
         {
            uint32_t r = member_offset(res.spirv, res.spirv_len, SLANG_RECT_NAME_RECT);
            uint32_t c = member_offset(res.spirv, res.spirv_len, SLANG_RECT_NAME_CLAMP);
            uint32_t t = member_offset(res.spirv, res.spirv_len, SLANG_RECT_NAME_TEXELS);
            if (r != pc->rect || c != pc->rect + 16 || t != pc->rect + 32)
               fail("%s past %u: the rectangle at %u %u %u, want %u\n",
                     pc->file, pc->offset, r, c, t, pc->rect);
         }
         rslc_result_free(&res, &o);
      }
      else
         fail("%s past %u: does not compile\n", pc->file, pc->offset);
      free(src);
   }
   printf("[%s] the rectangle placed past what both stages hold\n",
         failures ? "FAIL" : "pass");
}


/* Out of memory at every point of a remapped compile - the clone, the
 * helpers' text and their parse, the members - it fails with its
 * message and gives back every allocation; a caller's block too small
 * for it, likewise. */
static long   ex_fail_at = -1;
static long   ex_calls;
static size_t ex_live;

static void *ex_alloc(void *ud, size_t len)
{
   size_t *p;
   (void)ud;
   if (ex_fail_at >= 0 && ex_calls >= ex_fail_at)
      return NULL;
   ex_calls++;
   if (!(p = (size_t*)malloc(len + sizeof(size_t) * 2)))
      return NULL;
   p[0]     = len;
   ex_live += len;
   return p + 2;
}

static void ex_free(void *ud, void *ptr)
{
   size_t *p;
   (void)ud;
   if (!ptr)
      return;
   p        = (size_t*)ptr - 2;
   ex_live -= p[0];
   free(p);
}

static void exhaustion_lane(void)
{
   struct slang_rect_place pl;
   struct rslc_remap       rm;
   struct rslc_opts        o;
   struct rslc_result      res;
   char   err[128];
   char   path[256];
   char  *src;
   size_t len = 0, at;
   long   i, calls;
   unsigned char *block;
   int    before = failures;

   strcpy(path, SHADER_DIR);
   strcat(path, "nested.frag");
   if (!(src = read_file(path, &len)))
      return;
   pl.where = SLANG_RECT_PUSH;
   pl.set = pl.binding = 0;
   pl.offset = 32;
   remap_for(&rm, &pl, true, &samplings[1]);
   memset(&o, 0, sizeof(o));
   o.dst     = RSLC_DST_GLSL;
   o.version = 450;
   o.remap   = &rm;
   o.err     = err;
   o.err_cap = sizeof(err);
   o.alloc   = ex_alloc;
   o.free_fn = ex_free;
   ex_calls  = 0;
   if (!rslc_compile(src, len, RSLC_STAGE_FRAGMENT, &o, &res) || res.remap != RSLC_REMAP_REWRITTEN)
      fail("nested.frag: no remapped compile to exhaust\n");
   else
      rslc_result_free(&res, &o);
   calls = ex_calls;
   for (i = 0; i < calls; i++)
   {
      ex_fail_at = i;
      ex_calls   = 0;
      if (rslc_compile(src, len, RSLC_STAGE_FRAGMENT, &o, &res))
      {
         fail("nested.frag: compiled with allocation %ld refused\n", i);
         rslc_result_free(&res, &o);
      }
      if (ex_live)
         fail("nested.frag: %lu bytes kept with allocation %ld refused\n",
               (unsigned long)ex_live, i);
   }
   ex_fail_at = -1;
   if ((block = (unsigned char*)malloc(4u << 20)))
   {
      for (at = 8192; at < (4u << 20); at += 2048)
      {
         memset(block, 0xa5, at);
         o.arena     = block;
         o.arena_len = at;
         if (rslc_compile(src, len, RSLC_STAGE_FRAGMENT, &o, &res))
         {
            if (res.remap != RSLC_REMAP_REWRITTEN)
               fail("nested.frag: not rewritten in a block of %lu\n", (unsigned long)at);
            rslc_result_free(&res, &o);
            break;
         }
         if (strcmp(err, "rslc: caller arena exhausted"))
            fail("nested.frag, block of %lu: %s\n", (unsigned long)at, err);
      }
      free(block);
   }
   if (ex_live)
      fail("nested.frag: %lu bytes kept\n", (unsigned long)ex_live);
   free(src);
   printf("[%s] a remapped compile out of memory at each of %ld points fails "
         "cleanly\n", failures == before ? "pass" : "FAIL", calls);
}

#ifdef RSLC_REMAP_TEST_GL

/* The frame, w x h, at (x, y) of a W x H texture; drawn ow x oh. */
enum { FW = 13, FH = 9, TW = 40, TH = 30, FX = 7, FY = 5, OW = 211, OH = 157 };

static const char *vs_src =
   "#version 450\n"
   "void main()\n"
   "{\n"
   "   vec2 p = vec2((gl_VertexID & 1) * 4 - 1, (gl_VertexID & 2) * 2 - 1);\n"
   "   gl_Position = vec4(p, 0.0, 1.0);\n"
   "}\n";

static GLuint gl_shader(GLenum type, const char *src)
{
   GLint  ok  = 0;
   GLuint sh  = glCreateShader(type);
   glShaderSource(sh, 1, &src, NULL);
   glCompileShader(sh);
   glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
   if (!ok)
   {
      char log[2048];
      glGetShaderInfoLog(sh, sizeof(log), NULL, log);
      printf("%s\n%s\n", log, src);
      glDeleteShader(sh);
      return 0;
   }
   return sh;
}

/* A program of rslc's GLSL of @src, the remap as @rm says; its samplers
 * at the units the draws bind. */
static GLuint gl_program(const char *src, size_t len, const struct rslc_remap *rm)
{
   struct rslc_opts   o;
   struct rslc_result res;
   GLint  ok = 0;
   GLuint vs, fs, p;
   if (!compile(src, len, false, RSLC_DST_GLSL, 450, rm, &res, &o))
      return 0;
   vs = gl_shader(GL_VERTEX_SHADER, vs_src);
   fs = gl_shader(GL_FRAGMENT_SHADER, res.text);
   rslc_result_free(&res, &o);
   if (!vs || !fs)
      return 0;
   p = glCreateProgram();
   glAttachShader(p, vs);
   glAttachShader(p, fs);
   glLinkProgram(p);
   glDeleteShader(vs);
   glDeleteShader(fs);
   glGetProgramiv(p, GL_LINK_STATUS, &ok);
   if (!ok)
   {
      glDeleteProgram(p);
      return 0;
   }
   glUseProgram(p);
   {
      static const char *const frames[3] = {
         "Source", "Original", "OriginalHistory0" };
      unsigned i;
      for (i = 0; i < 3; i++)
      {
         GLint loc = glGetUniformLocation(p, frames[i]);
         if (loc >= 0)
            glUniform1i(loc, 2);
      }
      if ((ok = glGetUniformLocation(p, "LUT")) >= 0)
         glUniform1i(ok, 3);
   }
   return p;
}

static void set4(GLuint p, const char *name,
      float a, float b, float c, float d)
{
   GLint loc = glGetUniformLocation(p, name);
   if (loc >= 0)
      glUniform4f(loc, a, b, c, d);
}

static GLuint gl_texture(int w, int h, const unsigned char *px,
      const struct sampling *m)
{
   GLuint t;
   GLint  filter = m->linear ? GL_LINEAR : GL_NEAREST;
   GLint  wrap   = GL_CLAMP_TO_EDGE;
   switch (m->wrap)
   {
      case SLANG_RECT_WRAP_BORDER: wrap = GL_CLAMP_TO_BORDER;   break;
      case SLANG_RECT_WRAP_REPEAT: wrap = GL_REPEAT;            break;
      case SLANG_RECT_WRAP_MIRROR: wrap = GL_MIRRORED_REPEAT;   break;
      default:                                                  break;
   }
   glGenTextures(1, &t);
   glBindTexture(GL_TEXTURE_2D, t);
   glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, w, h);
   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA,
         GL_UNSIGNED_BYTE, px);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
   return t;
}

/* Draws program p over the coordinates (s0, t0) to (s1, t1), the
 * texture bound, into out. rect: the rewritten stage's rectangle. */
static void draw(GLuint p, float s0, float t0, float s1, float t1,
      bool rect, float *out)
{
   glUseProgram(p);
   set4(p, "params.SourceSize", FW, FH, 1.0f / FW, 1.0f / FH);
   set4(p, "params.OriginalSize", s0, t0, (s1 - s0) / OW, (t1 - t0) / OH);
   if (rect)
   {
      set4(p, "params." SLANG_RECT_NAME_RECT,
            (float)FW / TW, (float)FH / TH, (float)FX / TW, (float)FY / TH);
      set4(p, "params." SLANG_RECT_NAME_CLAMP,
            (FX + 0.5f + SLANG_RECT_CLAMP_BIAS) / TW,
            (FY + 0.5f + SLANG_RECT_CLAMP_BIAS) / TH,
            (FX + FW - 0.5f + SLANG_RECT_CLAMP_BIAS) / TW,
            (FY + FH - 0.5f + SLANG_RECT_CLAMP_BIAS) / TH);
      set4(p, "params." SLANG_RECT_NAME_TEXELS, FW, FH, FX, FY);
   }
   glDrawArrays(GL_TRIANGLES, 0, 3);
   glReadPixels(0, 0, OW, OH, GL_RGBA, GL_FLOAT, out);
}

struct span
{
   float s0, s1;
   float scale;   /* output pixels to a texel, when s0 == s1 */
};

static const struct span spans[] = {
   { -0.37f,  1.41f, 0 },   /* past every edge */
   { -4.3f,   5.7f,  0 },   /* many periods */
   { -17.01f, 13.9f, 0 },   /* many more, minified */
   {  0.47f,  0.53f, 0 },   /* magnified about the middle */
   {  0, 0, 1.0f },         /* texel centres; gathers on texel edges */
   {  0, 0, 0.5f },         /* nearest on texel edges */
   {  0, 0, 1.5f },
   {  0, 0, 2.0f },
   {  0, 0, 3.0f }
};

static unsigned rnd_state = 12345;

static unsigned rnd(void)
{
   rnd_state = rnd_state * 1103515245u + 12345u;
   return rnd_state >> 8;
}

/* 1 for a pass, 0 a failure, -1 no GL. */
static int gl_run(void)
{
   static const EGLint ctx_attribs[] = {
      EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 5,
      EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
      EGL_NONE };
   static const EGLint cfg_attribs[] = {
      EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_NONE };
   PFNEGLGETPLATFORMDISPLAYEXTPROC get_display;
   EGLDisplay dpy;
   EGLContext ctx;
   EGLConfig  cfg;
   EGLint     major, minor, ncfg = 0;
   GLuint     fbo, rb, vao;
   unsigned char *small, *big;
   float     *a, *b;
   size_t     si, mi, pi, i;
   int        j;
   int        drawn  = 0;
   int        before = failures;

   get_display = (PFNEGLGETPLATFORMDISPLAYEXTPROC)
      eglGetProcAddress("eglGetPlatformDisplayEXT");
   if (!get_display)
      return -1;
   dpy = get_display(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY,
         NULL);
   if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, &major, &minor))
      return -1;
   eglBindAPI(EGL_OPENGL_API);
   eglChooseConfig(dpy, cfg_attribs, &cfg, 1, &ncfg);
   ctx = eglCreateContext(dpy, ncfg ? cfg : NULL, EGL_NO_CONTEXT,
         ctx_attribs);
   if (     ctx == EGL_NO_CONTEXT
         || !eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx))
   {
      eglTerminate(dpy);
      return -1;
   }
   printf("[info] GL: %s\n", (const char*)glGetString(GL_RENDERER));

   small = (unsigned char*)malloc(FW * FH * 4);
   big   = (unsigned char*)malloc(TW * TH * 4);
   a     = (float*)malloc(OW * OH * 4 * sizeof(float));
   b     = (float*)malloc(OW * OH * 4 * sizeof(float));
   for (i = 0; i < TW * TH * 4; i++)
      big[i] = (unsigned char)rnd();
   for (j = 0; j < FH; j++)
      memcpy(small + j * FW * 4, big + ((FY + j) * TW + FX) * 4, FW * 4);

   glGenFramebuffers(1, &fbo);
   glBindFramebuffer(GL_FRAMEBUFFER, fbo);
   glGenRenderbuffers(1, &rb);
   glBindRenderbuffer(GL_RENDERBUFFER, rb);
   glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, OW, OH);
   glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
         GL_RENDERBUFFER, rb);
   glViewport(0, 0, OW, OH);
   glGenVertexArrays(1, &vao);
   glBindVertexArray(vao);

   for (si = 0; si < N_ELEMS(shaders); si++)
   {
      const struct shader *s = &shaders[si];
      char   path[256];
      char  *src;
      size_t len = 0;
      GLuint ref;
      if (!s->draw)
         continue;
      strcpy(path, SHADER_DIR);
      strcat(path, s->file);
      if (!(src = read_file(path, &len)))
         continue;
      if (!(ref = gl_program(src, len, NULL)))
      {
         fail("%s: no GL program as it was\n", s->file);
         free(src);
         continue;
      }
      for (mi = 0; mi < N_ELEMS(samplings); mi++)
      {
         const struct sampling  *m = &samplings[mi];
         struct slang_rect_place pl;
         struct rslc_remap       rm;
         struct rslc_opts        o;
         struct rslc_result      plain;
         GLuint prog, t_ref, t_big;

         if (expected(s, m) != SLANG_RECT_REWRITTEN)
            continue;
         if (!compile(src, len, false, RSLC_DST_SPIRV, 0, NULL, &plain, &o))
            continue;
         if (!slang_rect_place_pass(plain.spirv, plain.spirv_len,
            plain.spirv, plain.spirv_len, &pl))
         {
            rslc_result_free(&plain, &o);
            continue;
         }
         rslc_result_free(&plain, &o);
         remap_for(&rm, &pl, s->source, m);
         if (!(prog = gl_program(src, len, &rm)))
         {
            fail("%s, %s: no GL program rewritten\n", s->file, m->name);
            continue;
         }
         t_ref = gl_texture(FW, FH, small, m);
         t_big = gl_texture(TW, TH, big, m);

         for (pi = 0; pi < N_ELEMS(spans); pi++)
         {
            const struct span *sp = &spans[pi];
            float s0 = sp->s0, s1 = sp->s1;
            float t0 = sp->s0 * 0.9f, t1 = sp->s1 * 1.07f;
            int   bad = 0, first = -1, k;
            /* Linear with border: the edge texels' blend with the border
             * is the shader's, its weight unquantised, which a sampler
             * may round to its own steps - within one of 8 bits. */
            float tolerance = (m->linear && m->wrap == SLANG_RECT_WRAP_BORDER)
               ? 1.0f / 255.0f : 0.0f;
            if (sp->scale)
            {
               s0 = t0 = 0.0f;
               s1 = OW / (FW * sp->scale);
               t1 = OH / (FH * sp->scale);
               /* A mirrored gather on a texel's edge: which pair the
                * sampler takes there turns on its own rounding, and
                * llvmpipe's differs from the spec's arithmetic. */
               if (     m->wrap == SLANG_RECT_WRAP_MIRROR
                     && !strncmp(s->file, "gather", 6)
                     && (sp->scale == 1.0f || sp->scale == 3.0f))
                  continue;
            }
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, t_ref);
            draw(ref, s0, t0, s1, t1, false, a);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, t_big);
            draw(prog, s0, t0, s1, t1, true, b);
            for (k = 0; k < OW * OH; k++)
            {
               int c;
               for (c = 0; c < 4; c++)
               {
                  float d = a[k * 4 + c] - b[k * 4 + c];
                  if (d < 0.0f)
                     d = -d;
                  if (d > tolerance)
                     break;
               }
               if (c < 4)
               {
                  if (first < 0)
                     first = k;
                  bad++;
               }
            }
            drawn++;
            if (bad)
               fail("%s, %s, span %u: %d of %d pixels differ, the first "
                     "at %d,%d (%g %g %g %g, want %g %g %g %g)\n",
                     s->file, m->name, (unsigned)pi, bad, OW * OH,
                     first % OW, first / OW,
                     b[first * 4], b[first * 4 + 1], b[first * 4 + 2],
                     b[first * 4 + 3], a[first * 4], a[first * 4 + 1],
                     a[first * 4 + 2], a[first * 4 + 3]);
         }
         glDeleteTextures(1, &t_ref);
         glDeleteTextures(1, &t_big);
         glDeleteProgram(prog);
      }
      glDeleteProgram(ref);
      free(src);
   }
   if (glGetError() != GL_NO_ERROR)
      fail("GL error\n");
   printf("[%s] %d draws read the frame as its own texture would\n",
         failures == before ? "pass" : "FAIL", drawn);

   free(small);
   free(big);
   free(a);
   free(b);
   eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   eglDestroyContext(dpy, ctx);
   eglTerminate(dpy);
   return failures == before;
}

#endif

/* Every stage the list names, each sampling, first pass and later. */
static int corpus(const char *list)
{
   FILE    *f = fopen(list, "r");
   char     line[1024];
   unsigned stages = 0, counts[3] = { 0, 0, 0 };
   if (!f)
   {
      fail("cannot open %s\n", list);
      return 1;
   }
   while (fgets(line, sizeof(line), f))
   {
      char  *src;
      size_t len = 0, mi;
      int    pass;
      line[strcspn(line, "\r\n")] = '\0';
      if (!*line || !(src = read_file(line, &len)))
         continue;
      stages++;
      for (pass = 0; pass < 2; pass++)
         for (mi = 0; mi < N_ELEMS(samplings); mi++)
         {
            enum slang_rect_result r = SLANG_RECT_UNSUPPORTED;
            if (remap_one(line, src, len, strstr(line, ".vert") != NULL,
                     pass == 0, &samplings[mi], &r) == 0)
               counts[r]++;
         }
      free(src);
   }
   fclose(f);
   printf("[%s] %u stages: %u left alone, %u rewritten, %u refused, "
         "each as slang_rect.c says of rslc's module\n",
         failures ? "FAIL" : "pass", stages,
         counts[SLANG_RECT_UNCHANGED], counts[SLANG_RECT_REWRITTEN],
         counts[SLANG_RECT_UNSUPPORTED]);
   return failures ? 1 : 0;
}

int main(int argc, char **argv)
{
   size_t si, mi;
   int    rewritten  = 0;
   bool   require_gl = argc > 1 && !strcmp(argv[1], "--require-gl");

   if (argc > 2 && !strcmp(argv[1], "--corpus"))
      return corpus(argv[2]);

   for (si = 0; si < N_ELEMS(shaders); si++)
   {
      const struct shader *s = &shaders[si];
      char   path[256];
      char  *src;
      size_t len = 0;
      strcpy(path, SHADER_DIR);
      strcat(path, s->file);
      if (!(src = read_file(path, &len)))
      {
         fail("%s: cannot read %s\n", s->file, path);
         continue;
      }
      for (mi = 0; mi < N_ELEMS(samplings); mi++)
      {
         const struct sampling *m = &samplings[mi];
         enum slang_rect_result got = SLANG_RECT_UNSUPPORTED;
         enum slang_rect_result want = expected(s, m);
         if (remap_one(s->file, src, len, strstr(s->file, ".vert") != NULL,
                  s->source, m, &got) < 0)
         {
            fail("%s: does not compile\n", s->file);
            break;
         }
         if (got != want)
            fail("%s, %s%s: came back %d, want %d\n", s->file, m->name,
                  s->source ? "" : ", a later pass", (int)got, (int)want);
         else if (got == SLANG_RECT_REWRITTEN)
            rewritten++;
      }
      free(src);
   }
   printf("[%s] %d stages rewritten, the rest left or refused as they "
         "should be\n", failures ? "FAIL" : "pass", rewritten);
   placement_lanes();
   exhaustion_lane();

#ifdef RSLC_REMAP_TEST_GL
   if (gl_run() < 0)
   {
      if (require_gl)
         fail("no GL context\n");
      else
         printf("[skip] no GL context: not drawn\n");
   }
#else
   if (require_gl)
      fail("built without GL\n");
#endif

   return failures ? 1 : 0;
}
