/* gfx/drivers_shader/slang_rect.c: a slang stage rewritten to read its
 * frame where it lies in a larger texture.
 *
 * Every shader in shaders/ goes through the rewrite for each way a
 * preset can sample its frame - nearest or linear, each wrap - with its
 * rectangle in the push constants, and in the uniform block where those
 * are full. What comes back must be what is expected of that shader and
 * that sampling (rewritten, left alone, or refused), and every module
 * rewritten must cross-compile to GLSL and HLSL.
 *
 * With SLANG_RECT_TEST_GL (Linux, EGL), each rewritten fragment stage is
 * then drawn: the shader as it was, reading a w x h texture, and as
 * rewritten, reading that texture's texels at (x, y) of a larger one
 * filled with noise around them. Read in floats over spans and scales
 * that put coordinates outside the frame, on texel edges and on texel
 * centres, the two must match bit for bit. --require-gl fails when no
 * context can be had rather than skipping. */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <spirv_cross_c.h>

#include "../../../gfx/drivers_shader/slang_rect.h"
#include "slang_rect_spirv.h"

#ifdef SLANG_RECT_TEST_GL
#define GL_GLEXT_PROTOTYPES 1
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#define N_WORDS(a) (sizeof(a) / sizeof((a)[0]))

/* What a shader should come back as, sampled each way. */
enum expect
{
   X_ALL = 0,     /* rewritten every way */
   X_SAMPLES,     /* samples: rewritten but for linear with other wraps */
   X_NONE,        /* never reads the frame: left alone */
   X_REFUSED      /* reads it as cannot be kept: refused */
};

struct shader
{
   const char     *name;
   const uint32_t *spv;
   size_t          words;
   bool            source;
   enum expect     expect;
   bool            draw;
};

#define SHADER(n, src, x, draw) \
   { #n, spv_##n, N_WORDS(spv_##n), src, x, draw }

static const struct shader shaders[] = {
   SHADER(tex_frag,       true,  X_SAMPLES, true),
   SHADER(offset_frag,    true,  X_SAMPLES, true),
   SHADER(lod_frag,       true,  X_SAMPLES, true),
   SHADER(bias_frag,      true,  X_SAMPLES, true),
   SHADER(grad_frag,      true,  X_SAMPLES, true),
   SHADER(gather_frag,    true,  X_ALL,     true),
   SHADER(gather2_frag,   true,  X_ALL,     true),
   SHADER(gatheroff_frag, true,  X_ALL,     true),
   SHADER(fetch_frag,     true,  X_ALL,     true),
   SHADER(fetchoff_frag,  true,  X_ALL,     true),
   SHADER(size_frag,      true,  X_ALL,     true),
   SHADER(helper_frag,    true,  X_SAMPLES, true),
   SHADER(nested_frag,    true,  X_SAMPLES, true),
   SHADER(original_frag,  false, X_SAMPLES, true),
   SHADER(lut_frag,       true,  X_NONE,    false),
   SHADER(levels_frag,    true,  X_REFUSED, false),
   SHADER(proj_frag,      true,  X_REFUSED, false),
   SHADER(full_frag,      true,  X_SAMPLES, false),
   SHADER(nopush_frag,    true,  X_SAMPLES, false),
   SHADER(stock_vert,     true,  X_NONE,    false),
   /* Source is the frame only in the first pass. */
   { "tex_frag, a later pass", spv_tex_frag, N_WORDS(spv_tex_frag),
      false, X_NONE, false }
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

static enum slang_rect_result expected(const struct shader *s,
      const struct sampling *m)
{
   switch (s->expect)
   {
      case X_ALL:
         return SLANG_RECT_REWRITTEN;
      case X_SAMPLES:
         return (m->linear && m->wrap != SLANG_RECT_WRAP_EDGE)
            ? SLANG_RECT_UNSUPPORTED : SLANG_RECT_REWRITTEN;
      case X_NONE:
         return SLANG_RECT_UNCHANGED;
      case X_REFUSED:
      default:
         break;
   }
   return SLANG_RECT_UNSUPPORTED;
}

/* Where the rectangle goes, as a chain decides it for a pass of one
 * stage: the push constants, or the uniform block when they would come
 * out past SLANG_RECT_PUSH_LIMIT. false when neither will do. */
static bool place_for(const struct shader *s, struct slang_rect_place *p)
{
   uint32_t end;
   p->where   = SLANG_RECT_PUSH;
   p->set     = 0;
   p->binding = 0;
   p->offset  = 0;
   if ((end = slang_rect_block_end(s->spv, s->words, p)) == ~0u)
      return false;
   if (((end + 15) & ~15u) + 48 <= SLANG_RECT_PUSH_LIMIT)
   {
      p->offset = end;
      return true;
   }
   if (!slang_rect_uniform_block(s->spv, s->words, &p->set, &p->binding))
      return false;
   p->where = SLANG_RECT_UBO;
   if ((end = slang_rect_block_end(s->spv, s->words, p)) == ~0u)
      return false;
   p->offset = end;
   return true;
}

/* The module's GLSL (version 450) or HLSL (shader model 5.0), malloc'd;
 * NULL when SPIRV-Cross will not take it. */
static char *cross(const uint32_t *spv, size_t words, bool hlsl)
{
   spvc_context      ctx    = NULL;
   spvc_parsed_ir    ir     = NULL;
   spvc_compiler     comp   = NULL;
   spvc_compiler_options opts = NULL;
   const char       *src    = NULL;
   char             *out    = NULL;

   if (spvc_context_create(&ctx) != SPVC_SUCCESS)
      return NULL;
   if (     spvc_context_parse_spirv(ctx, spv, words, &ir) == SPVC_SUCCESS
         && spvc_context_create_compiler(ctx,
               hlsl ? SPVC_BACKEND_HLSL : SPVC_BACKEND_GLSL, ir,
               SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &comp) == SPVC_SUCCESS
         && spvc_compiler_create_compiler_options(comp, &opts)
               == SPVC_SUCCESS)
   {
      if (hlsl)
         spvc_compiler_options_set_uint(opts,
               SPVC_COMPILER_OPTION_HLSL_SHADER_MODEL, 50);
      else
      {
         spvc_compiler_options_set_uint(opts,
               SPVC_COMPILER_OPTION_GLSL_VERSION, 450);
         spvc_compiler_options_set_bool(opts,
               SPVC_COMPILER_OPTION_GLSL_ES, SPVC_FALSE);
         spvc_compiler_options_set_bool(opts,
               SPVC_COMPILER_OPTION_GLSL_VULKAN_SEMANTICS, SPVC_FALSE);
      }
      if (     spvc_compiler_install_compiler_options(comp, opts)
                  == SPVC_SUCCESS
            && spvc_compiler_compile(comp, &src) == SPVC_SUCCESS && src)
      {
         size_t len = strlen(src) + 1;
         if ((out = (char*)malloc(len)))
            memcpy(out, src, len);
      }
   }
   spvc_context_destroy(ctx);
   return out;
}

/* The instructions fill the module word for word, and the header's
 * bound is past every id the rewrite added. */
static bool well_formed(const uint32_t *w, size_t n)
{
   size_t p = 5;
   if (n < 5 || w[0] != 0x07230203u || !w[3])
      return false;
   while (p < n)
   {
      unsigned len = w[p] >> 16;
      if (!len || p + len > n)
         return false;
      p += len;
   }
   return p == n;
}

#ifdef SLANG_RECT_TEST_GL

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

static GLuint gl_program(const char *fs_src)
{
   GLint  ok = 0;
   GLuint vs = gl_shader(GL_VERTEX_SHADER, vs_src);
   GLuint fs = gl_shader(GL_FRAGMENT_SHADER, fs_src);
   GLuint p;
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

/* The frame at Source's or Original's binding, and OriginalHistory0's. */
static void bind_frame(GLuint t)
{
   glActiveTexture(GL_TEXTURE4);
   glBindTexture(GL_TEXTURE_2D, t);
   glActiveTexture(GL_TEXTURE2);
   glBindTexture(GL_TEXTURE_2D, t);
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
   int        drawn = 0;
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
   glActiveTexture(GL_TEXTURE2);

   for (si = 0; si < N_WORDS(shaders); si++)
   {
      const struct shader *s = &shaders[si];
      char *ref_src;
      GLuint ref;
      if (!s->draw)
         continue;
      ref_src = cross(s->spv, s->words, false);
      ref     = ref_src ? gl_program(ref_src) : 0;
      free(ref_src);
      if (!ref)
      {
         fail("%s: no GL program as it was\n", s->name);
         continue;
      }
      for (mi = 0; mi < N_WORDS(samplings); mi++)
      {
         const struct sampling  *m = &samplings[mi];
         struct slang_rect_place place;
         uint32_t *out     = NULL;
         size_t    out_len = 0;
         char     *src;
         GLuint    prog, t_ref, t_big;

         if (     !place_for(s, &place)
               || slang_rect_remap(s->spv, s->words, s->source, m->linear,
                     m->wrap, &place, &out, &out_len)
                  != SLANG_RECT_REWRITTEN)
            continue;
         src  = cross(out, out_len, false);
         prog = src ? gl_program(src) : 0;
         free(src);
         free(out);
         if (!prog)
         {
            fail("%s, %s: no GL program rewritten\n", s->name, m->name);
            continue;
         }
         t_ref = gl_texture(FW, FH, small, m);
         t_big = gl_texture(TW, TH, big, m);

         for (pi = 0; pi < N_WORDS(spans); pi++)
         {
            const struct span *sp = &spans[pi];
            float s0 = sp->s0, s1 = sp->s1;
            float t0 = sp->s0 * 0.9f, t1 = sp->s1 * 1.07f;
            int   bad = 0, first = -1, k;
            if (sp->scale)
            {
               s0 = t0 = 0.0f;
               s1 = OW / (FW * sp->scale);
               t1 = OH / (FH * sp->scale);
               /* A mirrored gather on a texel's edge: which pair the
                * sampler takes there turns on its own rounding, and
                * llvmpipe's differs from the spec's arithmetic. */
               if (     m->wrap == SLANG_RECT_WRAP_MIRROR
                     && !strncmp(s->name, "gather", 6)
                     && (sp->scale == 1.0f || sp->scale == 3.0f))
                  continue;
            }
            bind_frame(t_ref);
            draw(ref, s0, t0, s1, t1, false, a);
            bind_frame(t_big);
            draw(prog, s0, t0, s1, t1, true, b);
            for (k = 0; k < OW * OH; k++)
               if (memcmp(a + k * 4, b + k * 4, 4 * sizeof(float)))
               {
                  if (first < 0)
                     first = k;
                  bad++;
               }
            drawn++;
            if (bad)
               fail("%s, %s, span %u: %d of %d pixels differ, the first "
                     "at %d,%d (%g %g %g %g, want %g %g %g %g)\n",
                     s->name, m->name, (unsigned)pi, bad, OW * OH,
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

int main(int argc, char **argv)
{
   size_t si, mi;
   int    rewritten   = 0;
   bool   require_gl  = argc > 1 && !strcmp(argv[1], "--require-gl");

   for (si = 0; si < N_WORDS(shaders); si++)
   {
      const struct shader *s = &shaders[si];
      for (mi = 0; mi < N_WORDS(samplings); mi++)
      {
         const struct sampling  *m = &samplings[mi];
         struct slang_rect_place place;
         enum slang_rect_result  res;
         enum slang_rect_result  want = expected(s, m);
         uint32_t *out     = NULL;
         size_t    out_len = 0;
         char     *glsl, *hlsl;

         if (!place_for(s, &place))
         {
            fail("%s: no place for the rectangle\n", s->name);
            continue;
         }
         res = slang_rect_remap(s->spv, s->words, s->source, m->linear,
               m->wrap, &place, &out, &out_len);
         if (res != want)
         {
            fail("%s, %s: came back %d, want %d\n",
                  s->name, m->name, (int)res, (int)want);
            free(out);
            continue;
         }
         if (res != SLANG_RECT_REWRITTEN)
            continue;
         rewritten++;

         if (!well_formed(out, out_len) || out[3] < s->spv[3])
            fail("%s, %s: malformed\n", s->name, m->name);
         glsl = cross(out, out_len, false);
         hlsl = cross(out, out_len, true);
         if (!glsl || !strstr(glsl, SLANG_RECT_NAME_TEXELS))
            fail("%s, %s: no GLSL\n", s->name, m->name);
         if (!hlsl)
            fail("%s, %s: no HLSL\n", s->name, m->name);
         if (!strcmp(s->name, "full_frag") && place.where != SLANG_RECT_UBO)
            fail("%s: the rectangle not in the uniform block\n", s->name);
         free(glsl);
         free(hlsl);
         free(out);
      }
   }
   printf("[%s] %d stages rewritten, the rest left or refused as they "
         "should be\n", failures ? "FAIL" : "pass", rewritten);

#ifdef SLANG_RECT_TEST_GL
   switch (gl_run())
   {
      case -1:
         if (require_gl)
            fail("no GL context\n");
         else
            printf("[skip] no GL context: not drawn\n");
         break;
      default:
         break;
   }
#else
   if (require_gl)
      fail("built without GL\n");
#endif

   return failures ? 1 : 0;
}
