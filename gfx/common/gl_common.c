/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  copyright (c) 2011-2017 - Daniel De Matteis
 *  copyright (c) 2016-2019 - Brad Parker
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <glsym/glsym.h>

#include "gl_common.h"

#ifdef HAVE_GL_PLANAR
#include <string.h>
#include <formats/image_yuv_blit.h>
#endif

void gl_flush(void)
{
   glFlush();
}

void gl_clear(void)
{
   glClear(GL_COLOR_BUFFER_BIT);
}

void gl_disable(unsigned _cap)
{
   GLenum cap = (GLenum)_cap;
   glDisable(cap);
}

void gl_enable(unsigned _cap)
{
   GLenum cap = (GLenum)_cap;
   glEnable(cap);
}

void gl_finish(void)
{
   glFinish();
}

#ifdef HAVE_GL_MENU_BLOOM
/* Desktop OpenGL against a macOS SDK before 10.12 names the
 * framebuffer objects with the EXT suffix only */
#if defined(__MACH__) && !defined(HAVE_OPENGLES) \
   && defined(MAC_OS_X_VERSION_MAX_ALLOWED) \
   && (MAC_OS_X_VERSION_MAX_ALLOWED < 101200)
#define GL_MENU_BLOOM_FRAMEBUFFER              GL_FRAMEBUFFER_EXT
#define GL_MENU_BLOOM_FRAMEBUFFER_BINDING      GL_FRAMEBUFFER_BINDING_EXT
#define GL_MENU_BLOOM_FRAMEBUFFER_COMPLETE     GL_FRAMEBUFFER_COMPLETE_EXT
#define GL_MENU_BLOOM_COLOR_ATTACHMENT0        GL_COLOR_ATTACHMENT0_EXT
#define GL_MENU_BLOOM_READ_FRAMEBUFFER         GL_READ_FRAMEBUFFER_EXT
#define GL_MENU_BLOOM_READ_FRAMEBUFFER_BINDING GL_READ_FRAMEBUFFER_BINDING_EXT
#else
#define GL_MENU_BLOOM_FRAMEBUFFER              GL_FRAMEBUFFER
#define GL_MENU_BLOOM_FRAMEBUFFER_BINDING      GL_FRAMEBUFFER_BINDING
#define GL_MENU_BLOOM_FRAMEBUFFER_COMPLETE     GL_FRAMEBUFFER_COMPLETE
#define GL_MENU_BLOOM_COLOR_ATTACHMENT0        GL_COLOR_ATTACHMENT0
#define GL_MENU_BLOOM_READ_FRAMEBUFFER         GL_READ_FRAMEBUFFER
#define GL_MENU_BLOOM_READ_FRAMEBUFFER_BINDING GL_READ_FRAMEBUFFER_BINDING
#endif
#include "menu_bloom.h"
#include <gfx/gl_capabilities.h>

#ifndef GL_RGBA16F
#define GL_RGBA16F 0x881A
#endif
#ifndef GL_HALF_FLOAT
#define GL_HALF_FLOAT 0x140B
#endif
#ifndef GL_HALF_FLOAT_OES
#define GL_HALF_FLOAT_OES 0x8D61
#endif

struct gl_menu_bloom
{
   menu_bloom_pass_t plan[MENU_BLOOM_PASSES];
   GLuint tex[2];
   GLuint fbo[2];
   GLuint prog;
   GLuint vbo;
   GLint source;
   GLint step;
   GLint threshold;
   GLint gain;
   /* Inline only: the target the menu draws into, put back after */
   GLint target;
   GLint read_target;
   GLint viewport[4];
   unsigned dims;
   unsigned passes;
   bool floating;
   bool failed;
   bool core;
   bool defer;
};

static GLuint gl_menu_bloom_shader(GLenum type, const char *prefix,
      const char *body)
{
   const GLchar *src[2];
   GLint ok = 0;
   GLuint shader = glCreateShader(type);
   src[0] = prefix;
   src[1] = body;
   glShaderSource(shader, 2, src, NULL);
   glCompileShader(shader);
   glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
   if (!ok)
   {
      glDeleteShader(shader);
      return 0;
   }
   return shader;
}

static bool gl_menu_bloom_program(gl_menu_bloom_t *b, bool core, bool es)
{
   static const char vertex[] =
      "IN vec2 Position; OUT vec2 uv;"
      "void main(){uv=Position*0.5+0.5;gl_Position=vec4(Position,0.,1.);}";
   static const char fragment[] =
      "IN vec2 uv;uniform sampler2D Source;uniform vec2 Step;"
      "uniform float Threshold;uniform float Gain;"
      "vec3 light(vec2 p){return max(TEX(Source,p).rgb-Threshold,0.);}"
      "void main(){vec3 c=light(uv);if(dot(Step,Step)>0.){c*=.227027;"
      "c+=(light(uv+Step*1.384615)+light(uv-Step*1.384615))*.316216;"
      "c+=(light(uv+Step*3.230769)+light(uv-Step*3.230769))*.0702703;}"
      "COLOR=vec4(c*Gain,0.);}";
   static const GLfloat quad[8] = {-1,-1, 1,-1, -1,1, 1,1};
   const char *vp = core ? (es
      ? "#version 300 es\n#define IN in\n#define OUT out\n"
      : "#version 150\n#define IN in\n#define OUT out\n")
      : "#define IN attribute\n#define OUT varying\n";
   const char *fp = core ? (es
      ? "#version 300 es\nprecision mediump float;\n#define IN in\n#define TEX texture\n#define COLOR FragColor\nout vec4 FragColor;\n"
      : "#version 150\n#define IN in\n#define TEX texture\n#define COLOR FragColor\nout vec4 FragColor;\n")
      : (es
      ? "precision mediump float;\n#define IN varying\n#define TEX texture2D\n#define COLOR gl_FragColor\n"
      : "#define IN varying\n#define TEX texture2D\n#define COLOR gl_FragColor\n");
   GLuint vs = gl_menu_bloom_shader(GL_VERTEX_SHADER, vp, vertex);
   GLuint fs = gl_menu_bloom_shader(GL_FRAGMENT_SHADER, fp, fragment);
   GLint ok  = 0;
   if (vs && fs)
   {
      b->prog = glCreateProgram();
      glAttachShader(b->prog, vs);
      glAttachShader(b->prog, fs);
      glBindAttribLocation(b->prog, 0, "Position");
      glLinkProgram(b->prog);
      glGetProgramiv(b->prog, GL_LINK_STATUS, &ok);
   }
   if (vs)
      glDeleteShader(vs);
   if (fs)
      glDeleteShader(fs);
   if (!ok)
      return false;
   b->source    = glGetUniformLocation(b->prog, "Source");
   b->step      = glGetUniformLocation(b->prog, "Step");
   b->threshold = glGetUniformLocation(b->prog, "Threshold");
   b->gain      = glGetUniformLocation(b->prog, "Gain");
   glGenBuffers(1, &b->vbo);
   glBindBuffer(GL_ARRAY_BUFFER, b->vbo);
   glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
   return b->vbo != 0;
}

void gl_menu_bloom_free(gl_menu_bloom_t **bloom)
{
   gl_menu_bloom_t *b = *bloom;
   if (!b)
      return;
   glDeleteTextures(2, b->tex);
   glDeleteFramebuffers(2, b->fbo);
   if (b->vbo)
      glDeleteBuffers(1, &b->vbo);
   if (b->prog)
      glDeleteProgram(b->prog);
   free(b);
   *bloom = NULL;
}

/* The drivers bind their own program, textures and buffers for each
 * draw. Deferred, the frame has drawn nothing yet: only the default
 * framebuffer and the viewport carry over to it. */
static void gl_menu_bloom_restore(gl_menu_bloom_t *b)
{
   if (b->defer)
      glBindFramebuffer(GL_MENU_BLOOM_FRAMEBUFFER, 0);
   else
   {
      glBindFramebuffer(GL_MENU_BLOOM_FRAMEBUFFER, b->target);
#ifndef HAVE_OPENGLES2
      if (b->core)
         glBindFramebuffer(GL_MENU_BLOOM_READ_FRAMEBUFFER, b->read_target);
#endif
   }
   glViewport(b->viewport[0], b->viewport[1],
         b->viewport[2], b->viewport[3]);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glBindTexture(GL_TEXTURE_2D, 0);
}

static bool gl_menu_bloom_targets(gl_menu_bloom_t *b, unsigned small_dims,
      bool es)
{
   unsigned i, attempt;
   unsigned w = VIDEO_SCALE_W(small_dims);
   unsigned h = VIDEO_SCALE_H(small_dims);
   glDeleteTextures(2, b->tex);
   glDeleteFramebuffers(2, b->fbo);
   glGenTextures(2, b->tex);
   glGenFramebuffers(2, b->fbo);
   for (attempt = 0; attempt < 2; attempt++)
   {
      bool es3 = es && gl_check_capability(GL_CAPS_GLES3_SUPPORTED);
      bool floating = attempt == 0 && (es
         ? (es3 ? gl_query_extension("EXT_color_buffer_float")
               || gl_query_extension("EXT_color_buffer_half_float")
            : gl_query_extension("EXT_color_buffer_half_float")
               && gl_query_extension("OES_texture_half_float_linear"))
         : gl_check_capability(GL_CAPS_FP_FBO));
      for (i = 0; i < 2; i++)
      {
         glBindTexture(GL_TEXTURE_2D, b->tex[i]);
         glTexImage2D(GL_TEXTURE_2D, 0,
               floating && (!es || es3) ? GL_RGBA16F : GL_RGBA,
               w, h, 0, GL_RGBA, floating
                  ? (es && !es3 ? GL_HALF_FLOAT_OES : GL_HALF_FLOAT)
                  : GL_UNSIGNED_BYTE, NULL);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
         glBindFramebuffer(GL_MENU_BLOOM_FRAMEBUFFER, b->fbo[i]);
         glFramebufferTexture2D(GL_MENU_BLOOM_FRAMEBUFFER, GL_MENU_BLOOM_COLOR_ATTACHMENT0,
               GL_TEXTURE_2D, b->tex[i], 0);
         if (glCheckFramebufferStatus(GL_MENU_BLOOM_FRAMEBUFFER) != GL_MENU_BLOOM_FRAMEBUFFER_COMPLETE)
            break;
      }
      if (i == 2)
      {
         b->dims     = small_dims;
         b->floating = floating;
         b->passes   = menu_bloom_plan(b->plan, small_dims, floating);
         return true;
      }
   }
   return false;
}

unsigned gl_menu_bloom_begin(gl_menu_bloom_t **bloom,
      unsigned dims, bool core, bool es, bool defer)
{
   static const GLfloat clear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
   gl_menu_bloom_t *b = *bloom;
   unsigned small_dims;
   if (!VIDEO_SCALE_W(dims) || !VIDEO_SCALE_H(dims))
      return 0;
#ifndef HAVE_OPENGLES
   if (!glCreateShader || !glGenFramebuffers)
      return 0;
#endif
   if (!b && !(b = *bloom = (gl_menu_bloom_t*)calloc(1, sizeof(*b))))
      return 0;
   if (b->failed)
      return 0;
   small_dims    = menu_bloom_dims(dims);
   b->core  = core;
   b->defer = defer;
   glGetIntegerv(GL_VIEWPORT, b->viewport);
   if (!defer)
   {
      glGetIntegerv(GL_MENU_BLOOM_FRAMEBUFFER_BINDING, &b->target);
#ifndef HAVE_OPENGLES2
      if (core)
         glGetIntegerv(GL_MENU_BLOOM_READ_FRAMEBUFFER_BINDING, &b->read_target);
#endif
   }
   glActiveTexture(GL_TEXTURE0);
   glDisable(GL_SCISSOR_TEST);
   if (     (!b->prog && !gl_menu_bloom_program(b, core, es))
         || (small_dims != b->dims && !gl_menu_bloom_targets(b, small_dims, es)))
   {
      b->failed = true;
      gl_menu_bloom_restore(b);
      return 0;
   }
   /* Two composites cannot be split from the blurs */
   if (defer && !b->floating)
   {
      gl_menu_bloom_restore(b);
      return 0;
   }
   glBindFramebuffer(GL_MENU_BLOOM_FRAMEBUFFER, b->fbo[0]);
   glViewport(0, 0, VIDEO_SCALE_W(small_dims), VIDEO_SCALE_H(small_dims));
#ifndef HAVE_OPENGLES2
   if (core)
      glClearBufferfv(GL_COLOR, 0, clear);
   else
#endif
   {
      glClearColor(clear[0], clear[1], clear[2], clear[3]);
      glClear(GL_COLOR_BUFFER_BIT);
   }
   return small_dims;
}

/* The passes from first to last: drawn with attribute 0 from the quad
 * buffer, left disabled as the drivers leave their attributes between
 * draws. Output passes go to the saved target, or with into_bound to
 * the framebuffer bound now; deferred, they wait for the composite. */
static void gl_menu_bloom_passes(gl_menu_bloom_t *b, unsigned dims,
      unsigned first, unsigned last, bool into_bound)
{
   unsigned i;
   glUseProgram(b->prog);
   glUniform1i(b->source, 0);
   glActiveTexture(GL_TEXTURE0);
#ifndef HAVE_OPENGLES2
   if (b->core
#ifndef HAVE_OPENGLES
         && glBindSampler
#endif
      )
      glBindSampler(0, 0);
#endif
   glBindBuffer(GL_ARRAY_BUFFER, b->vbo);
   glEnableVertexAttribArray(0);
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
   for (i = first; i < last; i++)
   {
      const menu_bloom_pass_t *pass = &b->plan[i];
      if (pass->dst == MENU_BLOOM_OUTPUT)
      {
         if (b->defer && !into_bound)
            continue;
         if (!into_bound)
            glBindFramebuffer(GL_MENU_BLOOM_FRAMEBUFFER, b->target);
         glViewport(0, 0, VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims));
      }
      else
      {
         glBindFramebuffer(GL_MENU_BLOOM_FRAMEBUFFER, b->fbo[pass->dst]);
         glViewport(0, 0, VIDEO_SCALE_W(b->dims), VIDEO_SCALE_H(b->dims));
      }
      glBindTexture(GL_TEXTURE_2D, b->tex[pass->src]);
      glUniform2f(b->step, pass->params[0], pass->params[1]);
      glUniform1f(b->threshold, pass->params[2]);
      glUniform1f(b->gain, pass->params[3]);
      if (pass->add)
      {
         glEnable(GL_BLEND);
         glBlendFunc(GL_ONE, GL_ONE);
      }
      else
         glDisable(GL_BLEND);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   }
   glDisableVertexAttribArray(0);
   /* Not left bound: a GLSL program wins over the Cg backend's ARB
    * programs, which gl2 binds without touching this */
   glUseProgram(0);
}

void gl_menu_bloom_end(gl_menu_bloom_t *b, unsigned dims, bool defer)
{
   (void)defer;
   gl_menu_bloom_passes(b, dims, 0, b->passes, false);
   if (b->defer)
      glDisable(GL_BLEND);
   gl_menu_bloom_restore(b);
}

/* Deferred plans are float: one output pass, the last */
void gl_menu_bloom_composite(gl_menu_bloom_t *b, unsigned dims)
{
   if (!b || !b->passes)
      return;
   gl_menu_bloom_passes(b, dims, b->passes - 1, b->passes, true);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glBindTexture(GL_TEXTURE_2D, 0);
}
#endif

#ifdef HAVE_GL_FRAME_MEM
#include <gfx/gl_capabilities.h>

#ifndef GL_MAP_PERSISTENT_BIT
#define GL_MAP_PERSISTENT_BIT 0x0040
#endif
#ifndef GL_MAP_COHERENT_BIT
#define GL_MAP_COHERENT_BIT   0x0080
#endif
#ifndef GL_RGBA16F
#define GL_RGBA16F            0x881A
#endif
/* Not in every header the drivers build against (the old Mac OS X
 * gl.h among them); the entry points are resolved at run time */
#ifndef GL_MAP_WRITE_BIT
#define GL_MAP_WRITE_BIT              0x0002
#endif
#ifndef GL_PIXEL_UNPACK_BUFFER
#define GL_PIXEL_UNPACK_BUFFER        0x88EC
#endif
#ifndef GL_TEXTURE_INTERNAL_FORMAT
#define GL_TEXTURE_INTERNAL_FORMAT    0x1003
#endif
#ifndef GL_SYNC_GPU_COMMANDS_COMPLETE
#define GL_SYNC_GPU_COMMANDS_COMPLETE 0x9117
#endif
#ifndef GL_ALREADY_SIGNALED
#define GL_ALREADY_SIGNALED           0x911A
#endif
#ifndef GL_CONDITION_SATISFIED
#define GL_CONDITION_SATISFIED        0x911C
#endif

#ifndef GL_MAP_READ_BIT
#define GL_MAP_READ_BIT               0x0001
#endif
#ifndef GL_CLIENT_STORAGE_BIT
#define GL_CLIENT_STORAGE_BIT         0x0200
#endif

#ifdef HAVE_OPENGLES
/* EXT_buffer_storage: the same storage, flags and values under the
 * extension's name */
#define gl_buffer_storage glBufferStorageEXT

/* GLES 3.0 for the mapping and the fences, a version a GLES 3 build
 * can be handed less than; EXT_buffer_storage for the persistence */
static bool gl_buffer_storage_supported(void)
{
   const char *ver = (const char*)glGetString(GL_VERSION);
   if (     !glBufferStorageEXT || !ver
         || strncmp(ver, "OpenGL ES ", 10)
         || ver[10] < '3' || ver[10] > '9')
      return false;
   return gl_query_extension("EXT_buffer_storage");
}
#else
#define gl_buffer_storage glBufferStorage

/* Persistent mapping is GL 4.4 or ARB_buffer_storage, fences GL 3.2
 * or ARB_sync */
static bool gl_buffer_storage_supported(void)
{
   const char *ver = (const char*)glGetString(GL_VERSION);
   int major       = 0, minor = 0;
   if (!glBufferStorage || !glMapBufferRange || !glFenceSync
         || !glClientWaitSync)
      return false;
   if (     ver && ver[0] >= '0' && ver[0] <= '9' && ver[1] == '.'
         && ver[2] >= '0' && ver[2] <= '9')
   {
      major = ver[0] - '0';
      minor = ver[2] - '0';
   }
   if (major > 4 || (major == 4 && minor >= 4))
      return true;
   return    gl_query_extension("ARB_buffer_storage")
          && (major > 3 || (major == 3 && minor >= 2)
             || gl_query_extension("ARB_sync"));
}
#endif

#ifdef HAVE_GL_TEXTURE_LEND
#define GL_LEND_SLOTS 2

#ifndef GL_TEXTURE_WIDTH
#define GL_TEXTURE_WIDTH 0x1000
#endif
#ifndef GL_TEXTURE_HEIGHT
#define GL_TEXTURE_HEIGHT 0x1001
#endif

static bool gl_texture_lend_supported(void)
{
#ifdef HAVE_OPENGLES
   const char *ver = (const char*)glGetString(GL_VERSION);
   if (!glGetTexLevelParameteriv || !ver
         || strncmp(ver, "OpenGL ES ", 10)
         || ver[10] < '3' || ver[10] > '9'
         || (ver[10] == '3' && (ver[11] != '.'
               || ver[12] < '1' || ver[12] > '9')))
      return false;
#endif
   return gl_buffer_storage_supported();
}

struct gl_texture_lend
{
   GLsync   fence[GL_LEND_SLOTS];
   void    *mapped[GL_LEND_SLOTS];
   struct gl_texture_lend *next;
   size_t   size;
   size_t   pitch;
   GLuint   pbo[GL_LEND_SLOTS];
   GLuint   tex;
   /* An upload from the slot has been issued and not seen to finish.
    * Kept apart from the fence, which can fail to be made: a slot with
    * no fence is not thereby free. */
   bool     busy[GL_LEND_SLOTS];
};

static gl_texture_lend_t *gl_texture_lend_find(gl_texture_lend_t *list,
      unsigned tex)
{
   while (list && list->tex != (GLuint)tex)
      list = list->next;
   return list;
}

static void gl_texture_lend_destroy(gl_texture_lend_t *st)
{
   unsigned i;
   for (i = 0; i < GL_LEND_SLOTS; i++)
      if (st->fence[i])
         glDeleteSync(st->fence[i]);
   /* Deleting a mapped buffer unmaps it */
   glDeleteBuffers(GL_LEND_SLOTS, st->pbo);
   free(st);
}

/* Both slots at once, sized from the texture as it was made */
static gl_texture_lend_t *gl_texture_lend_new(unsigned tex, size_t pitch)
{
   unsigned i;
   GLint w = 0, h = 0, ifmt = 0;
   size_t bpp;
   gl_texture_lend_t *st;
   GLbitfield flags = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT
      | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;

   if (!gl_texture_lend_supported())
      return NULL;
   glBindTexture(GL_TEXTURE_2D, (GLuint)tex);
   glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH,  &w);
   glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
   glGetTexLevelParameteriv(GL_TEXTURE_2D, 0,
         GL_TEXTURE_INTERNAL_FORMAT, &ifmt);
   glBindTexture(GL_TEXTURE_2D, 0);
   bpp = (ifmt == GL_RGBA16F) ? 8 : 4;
   if (w <= 0 || h <= 0 || pitch != (size_t)w * bpp)
      return NULL;
   if (!(st = (gl_texture_lend_t*)calloc(1, sizeof(*st))))
      return NULL;
   st->tex   = (GLuint)tex;
   st->pitch = pitch;
   st->size  = pitch * (size_t)h;
   glGenBuffers(GL_LEND_SLOTS, st->pbo);
   for (i = 0; i < GL_LEND_SLOTS; i++)
   {
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, st->pbo[i]);
      gl_buffer_storage(GL_PIXEL_UNPACK_BUFFER, (GLsizeiptr)st->size,
            NULL, flags);
      st->mapped[i] = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0,
            (GLsizeiptr)st->size, flags);
      if (!st->mapped[i])
         break;
   }
   glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
   if (i < GL_LEND_SLOTS)
   {
      gl_texture_lend_destroy(st);
      return NULL;
   }
   return st;
}

void *gl_texture_lend(gl_texture_lend_t **list, unsigned tex,
      unsigned slot, size_t pitch)
{
   gl_texture_lend_t *st;
   if (!list || !tex || slot >= GL_LEND_SLOTS)
      return NULL;
   if (!(st = gl_texture_lend_find(*list, tex)))
   {
      if (!(st = gl_texture_lend_new(tex, pitch)))
         return NULL;
      st->next = *list;
      *list    = st;
   }
   else if (pitch != st->pitch)
      return NULL;
   return st->mapped[slot];
}

static bool gl_texture_lend_slot_ready(gl_texture_lend_t *st,
      unsigned slot)
{
   GLenum r;
   if (!st->busy[slot])
      return true;
   /* The upload's own fence could not be made: one made now follows
    * it in the command stream, so its signal covers the upload too */
   if (     !st->fence[slot]
         && !(st->fence[slot] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0)))
      return false;
   r = glClientWaitSync(st->fence[slot], 0, 0);
   if (r != GL_ALREADY_SIGNALED && r != GL_CONDITION_SATISFIED)
      return false;
   glDeleteSync(st->fence[slot]);
   st->fence[slot] = 0;
   st->busy[slot]  = false;
   return true;
}

bool gl_texture_lend_ready(gl_texture_lend_t *list, unsigned tex,
      unsigned slot)
{
   gl_texture_lend_t *st = gl_texture_lend_find(list, tex);
   if (!st || slot >= GL_LEND_SLOTS)
      return true;
   return gl_texture_lend_slot_ready(st, slot);
}

int gl_texture_lend_bind(gl_texture_lend_t *list, unsigned tex,
      const void *pixels)
{
   unsigned i;
   gl_texture_lend_t *st = gl_texture_lend_find(list, tex);
   if (!st)
      return -1;
   for (i = 0; i < GL_LEND_SLOTS; i++)
      if (pixels == st->mapped[i])
      {
         if (!gl_texture_lend_slot_ready(st, i))
            return -2;
         glBindBuffer(GL_PIXEL_UNPACK_BUFFER, st->pbo[i]);
         return (int)i;
      }
   return -1;
}

void gl_texture_lend_unbind(gl_texture_lend_t *list, unsigned tex,
      int slot)
{
   gl_texture_lend_t *st = gl_texture_lend_find(list, tex);
   glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
   if (st && slot >= 0 && slot < GL_LEND_SLOTS)
   {
      st->busy[slot]  = true;
      st->fence[slot] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
   }
}

void gl_texture_lend_forget(gl_texture_lend_t **list, unsigned tex)
{
   gl_texture_lend_t **p;
   if (!list)
      return;
   for (p = list; *p; p = &(*p)->next)
      if ((*p)->tex == (GLuint)tex)
      {
         gl_texture_lend_t *st = *p;
         *p = st->next;
         gl_texture_lend_destroy(st);
         return;
      }
}

void gl_texture_lend_free(gl_texture_lend_t **list)
{
   if (!list)
      return;
   while (*list)
   {
      gl_texture_lend_t *st = *list;
      *list = st->next;
      gl_texture_lend_destroy(st);
   }
}

#endif /* HAVE_GL_TEXTURE_LEND */

#define GL_FRAME_MEM_MAX 12

struct gl_frame_mem
{
   GLsync   fence[GL_FRAME_MEM_MAX];
   void    *mapped[GL_FRAME_MEM_MAX];
   size_t   size[GL_FRAME_MEM_MAX];
   GLuint   pbo[GL_FRAME_MEM_MAX];
   /* As a lent slot's: an upload issued and not seen to finish */
   bool     busy[GL_FRAME_MEM_MAX];
};

void *gl_frame_mem_alloc(gl_frame_mem_t **fm, size_t size, uintptr_t *id)
{
   unsigned i;
   gl_frame_mem_t *f;
   /* Readable too: a frame lent and pushed is read back as the cached
    * frame (a screenshot, a paused redraw elsewhere), which client
    * storage, a hint, keeps in system memory where the GL can */
   GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_READ_BIT
      | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
   GLbitfield store = flags | GL_CLIENT_STORAGE_BIT;

   if (!fm || !size || !gl_buffer_storage_supported())
      return NULL;
   if (!*fm && !(*fm = (gl_frame_mem_t*)calloc(1, sizeof(**fm))))
      return NULL;
   f = *fm;
   for (i = 0; i < GL_FRAME_MEM_MAX; i++)
      if (!f->pbo[i])
         break;
   if (i == GL_FRAME_MEM_MAX)
      return NULL;
   glGenBuffers(1, &f->pbo[i]);
   glBindBuffer(GL_PIXEL_UNPACK_BUFFER, f->pbo[i]);
   gl_buffer_storage(GL_PIXEL_UNPACK_BUFFER, (GLsizeiptr)size, NULL, store);
   f->mapped[i] = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0,
         (GLsizeiptr)size, flags);
   glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
   if (!f->mapped[i])
   {
      glDeleteBuffers(1, &f->pbo[i]);
      f->pbo[i] = 0;
      return NULL;
   }
   f->size[i] = size;
   f->busy[i] = false;
   *id        = (uintptr_t)i + 1;
   return f->mapped[i];
}

static bool gl_frame_mem_slot_ready(gl_frame_mem_t *f, unsigned i)
{
   GLenum r;
   if (!f->busy[i])
      return true;
   if (     !f->fence[i]
         && !(f->fence[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0)))
      return false;
   r = glClientWaitSync(f->fence[i], 0, 0);
   if (r != GL_ALREADY_SIGNALED && r != GL_CONDITION_SATISFIED)
      return false;
   glDeleteSync(f->fence[i]);
   f->fence[i] = 0;
   f->busy[i]  = false;
   return true;
}

bool gl_frame_mem_ready(gl_frame_mem_t *fm, uintptr_t id)
{
   if (!fm || !id || id > GL_FRAME_MEM_MAX || !fm->pbo[id - 1])
      return true;
   return gl_frame_mem_slot_ready(fm, (unsigned)id - 1);
}

void gl_frame_mem_free(gl_frame_mem_t *fm, uintptr_t id)
{
   unsigned i;
   if (!fm || !id || id > GL_FRAME_MEM_MAX || !fm->pbo[id - 1])
      return;
   i = (unsigned)id - 1;
   if (fm->fence[i])
      glDeleteSync(fm->fence[i]);
   /* Deleting a mapped buffer unmaps it */
   glDeleteBuffers(1, &fm->pbo[i]);
   fm->fence[i]  = 0;
   fm->pbo[i]    = 0;
   fm->mapped[i] = NULL;
   fm->size[i]   = 0;
   fm->busy[i]   = false;
}

int gl_frame_mem_bind(gl_frame_mem_t *fm, const void *pixels,
      size_t extent, size_t *offset)
{
   unsigned i;
   uintptr_t p = (uintptr_t)pixels;
   glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
   if (!fm || !p)
      return -1;
   for (i = 0; i < GL_FRAME_MEM_MAX; i++)
   {
      uintptr_t base = (uintptr_t)fm->mapped[i];
      if (     base && p >= base && p - base < fm->size[i]
            && extent <= fm->size[i] - (size_t)(p - base))
      {
         glBindBuffer(GL_PIXEL_UNPACK_BUFFER, fm->pbo[i]);
         *offset = (size_t)(p - base);
         return (int)i;
      }
   }
   return -1;
}

void gl_frame_mem_unbind(gl_frame_mem_t *fm, int slot)
{
   glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
   if (!fm || slot < 0 || slot >= GL_FRAME_MEM_MAX)
      return;
   if (fm->fence[slot])
      glDeleteSync(fm->fence[slot]);
   fm->busy[slot]  = true;
   fm->fence[slot] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

bool gl_sw_fb_fits(const gl_sw_fb_t *sw, unsigned width, unsigned height,
      unsigned bpp)
{
   return sw->dims == ((width << 16) | height) && sw->bpp == bpp;
}

void gl_sw_fb_free(gl_frame_mem_t *fm, gl_sw_fb_t *sw)
{
   unsigned i;
   for (i = 0; i < GL_SW_FB_SLOTS; i++)
      gl_frame_mem_free(fm, sw->id[i]);
   memset(sw, 0, sizeof(*sw));
}

void *gl_sw_fb_lend(gl_frame_mem_t **fm, gl_sw_fb_t *sw,
      unsigned width, unsigned height, unsigned bpp)
{
   unsigned i, k;
   if (!fm || !width || !height || width > 0xffff || height > 0xffff)
      return NULL;
   if (!gl_sw_fb_fits(sw, width, height, bpp))
   {
      size_t size = (size_t)width * bpp * height;
      gl_sw_fb_free(*fm, sw);
      sw->dims    = (width << 16) | height;
      sw->bpp     = bpp;
      for (i = 0; i < GL_SW_FB_SLOTS; i++)
         if (!(sw->mapped[i] = gl_frame_mem_alloc(fm, size, &sw->id[i])))
         {
            unsigned dims = sw->dims;
            gl_sw_fb_free(*fm, sw);
            sw->dims   = dims;
            sw->bpp    = bpp;
            sw->failed = true;
            return NULL;
         }
   }
   if (sw->failed)
      return NULL;
   for (k = 0; k < GL_SW_FB_SLOTS; k++)
   {
      i = (sw->next + k) % GL_SW_FB_SLOTS;
      if (     sw->id[i] != sw->last
            && gl_frame_mem_ready(*fm, sw->id[i]))
      {
         sw->next = (i + 1) % GL_SW_FB_SLOTS;
         return sw->mapped[i];
      }
   }
   return NULL;
}

void gl_sw_fb_pushed(gl_sw_fb_t *sw, int slot)
{
   sw->last = (slot >= 0) ? (uintptr_t)slot + 1 : 0;
}

void gl_frame_mem_destroy(gl_frame_mem_t **fm)
{
   unsigned i;
   if (!fm || !*fm)
      return;
   for (i = 0; i < GL_FRAME_MEM_MAX; i++)
      gl_frame_mem_free(*fm, (uintptr_t)i + 1);
   free(*fm);
   *fm = NULL;
}
#endif

#ifdef HAVE_GL_PLANAR
#ifndef GL_LUMINANCE
#define GL_LUMINANCE 0x1909
#endif
#ifndef GL_R8
#define GL_R8 0x8229
#endif
#ifndef GL_RG8
#define GL_RG8 0x822B
#endif
#ifndef GL_RED
#define GL_RED 0x1903
#endif
#ifndef GL_RG
#define GL_RG 0x8227
#endif
#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif
#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#endif
#ifndef GL_PIXEL_UNPACK_BUFFER_BINDING
#define GL_PIXEL_UNPACK_BUFFER_BINDING 0x88EF
#endif
#ifndef GL_PIXEL_UNPACK_BUFFER
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#endif
#ifndef GL_VERTEX_ARRAY_BINDING
#define GL_VERTEX_ARRAY_BINDING 0x85B5
#endif

#if !defined(HAVE_OPENGLES) || defined(HAVE_OPENGLES3)
#define GL_PLANAR_HAVE_VAO
#define GL_PLANAR_HAVE_UNPACK_BUFFER
#endif
/* Rows of any stride in one upload (GL_UNPACK_ROW_LENGTH) */
#if !defined(HAVE_OPENGLES) || defined(HAVE_OPENGLES3)
#define GL_PLANAR_ROW_LENGTH 1
#else
#define GL_PLANAR_ROW_LENGTH 0
#endif

/* The attribute the triangle comes in on: high, out of the way of the
 * driver's own */
#define GL_PLANAR_ATTR 7

struct gl_planar_tex
{
   gl_planar_tex_t *next;
   GLuint tex;   /* RGBA, the handle */
   GLuint plane[3];
   GLuint fbo;
   unsigned w;
   unsigned h;
   /* Chroma in one interleaved plane, and Cr first in it */
   bool interleaved;
   bool swap;
};

/* Pixel centres, the chroma of a 2x2 block from the one sample it
 * shares - as exact as the CPU conversion at any size. CH reads the
 * two chroma channels of the interleaved plane. */
static const char gl_planar_fs_decl[] =
   "uniform sampler2D uY;\n"
   "uniform sampler2D uC0;\n"
   "uniform sampler2D uC1;\n"
   "uniform vec4 uSize;\n"
   "uniform vec4 uCoef[2];\n";
static const char gl_planar_fs_main[] =
   "void main()\n"
   "{\n"
   "   vec2 p  = floor(gl_FragCoord.xy);\n"
   "   vec2 cc = (floor(p * 0.5) + 0.5) / uSize.zw;\n"
   "   float y = TEX(uY, (p + 0.5) / uSize.xy).r;\n"
   "   vec2 c  = uCoef[1].z > 0.5 ? TEX(uC0, cc).CH\n"
   "           : vec2(TEX(uC0, cc).r, TEX(uC1, cc).r);\n"
   "   vec3 rgb;\n"
   "   if (uCoef[1].w > 0.5)\n"
   "      c = c.yx;\n"
   "   c  -= vec2(128.0 / 255.0);\n";
static const char gl_planar_fs_end[] =
   "   rgb = vec3(uCoef[0].x * y + uCoef[0].y)\n"
   "       + vec3(uCoef[0].z * c.y, uCoef[0].w * c.x + uCoef[1].x * c.y,\n"
   "              uCoef[1].y * c.x);\n"
   "   OUT = vec4(clamp(rgb, 0.0, 1.0), 1.0);\n"
   "}\n";

#if defined(HAVE_OPENGLES)
static const char gl_planar_core_head[] =
   "#version 300 es\nprecision highp float;\n";
#else
static const char gl_planar_core_head[] = "#version 140\n";
#endif

static const char gl_planar_core_vs[] =
   "in vec2 aPos;\n"
   "void main() { gl_Position = vec4(aPos, 0.0, 1.0); }\n";
static const char gl_planar_core_fs[] =
   "#define TEX texture\n#define CH rg\n#define OUT FragColor\n"
   "out vec4 FragColor;\n";

static const char gl_planar_legacy_head[] =
   "#ifdef GL_ES\n"
   "#ifdef GL_FRAGMENT_PRECISION_HIGH\nprecision highp float;\n"
   "#else\nprecision mediump float;\n#endif\n"
   "#endif\n";
static const char gl_planar_legacy_vs[] =
   "attribute vec2 aPos;\n"
   "void main() { gl_Position = vec4(aPos, 0.0, 1.0); }\n";
/* Luminance-alpha holds the interleaved pair in .r and .a */
static const char gl_planar_legacy_fs[] =
   "#define TEX texture2D\n#define CH ra\n#define OUT gl_FragColor\n";

static GLuint gl_planar_shader(GLenum stage, const char **src,
      GLsizei n)
{
   GLuint sh = glCreateShader(stage);
   if (sh)
   {
      GLint ok  = 0;
      glShaderSource(sh, n, src, NULL);
      glCompileShader(sh);
      glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
      if (ok)
         return sh;
      glDeleteShader(sh);
   }
   return 0;
}

/* The program and the triangle, made on the first planar texture; a
 * failure is final */
static bool gl_planar_init(gl_planar_t *p, bool core)
{
   static const float tri[6] = { -1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f };
   const char *src[5];
   GLuint vs, fs;
   GLint ok = 0;
   if (p->prog)
      return true;
   if (p->flags & GL_PLANAR_FAILED)
      return false;
   p->flags |= GL_PLANAR_FAILED;
   if (core)
      p->flags |= GL_PLANAR_CORE;
   src[0] = core ? gl_planar_core_head : gl_planar_legacy_head;
   src[1] = core ? gl_planar_core_vs   : gl_planar_legacy_vs;
   vs     = gl_planar_shader(GL_VERTEX_SHADER, src, 2);
   src[1] = core ? gl_planar_core_fs   : gl_planar_legacy_fs;
   src[2] = gl_planar_fs_decl;
   src[3] = gl_planar_fs_main;
   src[4] = gl_planar_fs_end;
   fs     = gl_planar_shader(GL_FRAGMENT_SHADER, src, 5);
   if (vs && fs && (p->prog = glCreateProgram()))
   {
      glAttachShader(p->prog, vs);
      glAttachShader(p->prog, fs);
      glBindAttribLocation(p->prog, GL_PLANAR_ATTR, "aPos");
      glLinkProgram(p->prog);
      glGetProgramiv(p->prog, GL_LINK_STATUS, &ok);
   }
   if (vs)
      glDeleteShader(vs);
   if (fs)
      glDeleteShader(fs);
   if (!ok)
   {
      if (p->prog)
         glDeleteProgram(p->prog);
      p->prog = 0;
      return false;
   }
   p->loc[0] = glGetUniformLocation(p->prog, "uY");
   p->loc[1] = glGetUniformLocation(p->prog, "uC0");
   p->loc[2] = glGetUniformLocation(p->prog, "uC1");
   p->loc[3] = glGetUniformLocation(p->prog, "uSize");
   p->loc[4] = glGetUniformLocation(p->prog, "uCoef");
   glGenBuffers(1, &p->vbo);
   {
      GLint prev = 0;
      glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prev);
      glBindBuffer(GL_ARRAY_BUFFER, p->vbo);
      glBufferData(GL_ARRAY_BUFFER, sizeof(tri), tri, GL_STATIC_DRAW);
      glBindBuffer(GL_ARRAY_BUFFER, (GLuint)prev);
   }
#ifdef GL_PLANAR_HAVE_VAO
   if (core)
   {
      GLint prev = 0;
      glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prev);
      glGenVertexArrays(1, &p->vao);
      glBindVertexArray(p->vao);
      glBindBuffer(GL_ARRAY_BUFFER, p->vbo);
      glEnableVertexAttribArray(GL_PLANAR_ATTR);
      glVertexAttribPointer(GL_PLANAR_ATTR, 2, GL_FLOAT, GL_FALSE, 0, NULL);
      glBindVertexArray((GLuint)prev);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
   }
#endif
   p->flags &= ~GL_PLANAR_FAILED;
   return true;
}

static gl_planar_tex_t *gl_planar_find(gl_planar_t *p, unsigned tex)
{
   gl_planar_tex_t *t = p->list;
   while (t && t->tex != (GLuint)tex)
      t = t->next;
   return t;
}

/* One plane of @w x @h samples of @bpp bytes, rows @stride apart, from
 * where they lie: in one upload where rows of any stride can be, else
 * a row at a time */
static void gl_planar_plane(GLenum fmt, unsigned bpp, unsigned w,
      unsigned h, const uint8_t *src, unsigned stride)
{
   unsigned y;
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   if (GL_PLANAR_ROW_LENGTH && !(stride % bpp))
   {
      glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)(stride / bpp));
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, fmt, GL_UNSIGNED_BYTE,
            src);
      return;
   }
   if (stride == w * bpp)
   {
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, fmt, GL_UNSIGNED_BYTE,
            src);
      return;
   }
   for (y = 0; y < h; y++)
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, (GLint)y, w, 1, fmt,
            GL_UNSIGNED_BYTE, src + (size_t)y * stride);
}

/* The planes of @ti into @t's textures and the draw that converts them
 * into @t->tex, the driver's state put back as it was */
static bool gl_planar_draw(gl_planar_t *p, gl_planar_tex_t *t,
      const struct texture_image *ti)
{
   const struct texture_planar *tp = ti->planar;
   bool core   = (p->flags & GL_PLANAR_CORE) != 0;
   GLenum one  = core ? GL_RED : GL_LUMINANCE;
   GLenum two  = core ? GL_RG  : GL_LUMINANCE_ALPHA;
   unsigned cw = (t->w + 1) / 2;
   unsigned ch = (t->h + 1) / 2;
   const uint8_t *c0;
   GLint fbo = 0, prog = 0, active = 0, align = 4, vao = 0, abuf = 0;
   GLint unpack = 0, attr_on = 0, row_length = 0;
   GLint bound[3], vp[4];
   GLboolean blend, scissor, depth, cull, stencil;
   float size[4], coef[8], k[6];
   bool interleaved, swap;
   unsigned i;

   if (     !tp || !tp->planes[0] || !tp->planes[1] || !tp->planes[2]
         || ti->width != t->w || ti->height != t->h)
      return false;
   /* One interleaved plane, either order, or two */
   if (tp->chroma_step == 1)
   {
      interleaved = false;
      swap        = false;
      c0          = tp->planes[1];
   }
   else if (tp->chroma_step == 2 && tp->planes[2] == tp->planes[1] + 1)
   {
      interleaved = true;
      swap        = false;
      c0          = tp->planes[1];
   }
   else if (tp->chroma_step == 2 && tp->planes[1] == tp->planes[2] + 1)
   {
      interleaved = true;
      swap        = true;
      c0          = tp->planes[2];
   }
   else
      return false;
   if (interleaved != t->interleaved)
      return false;
   t->swap = swap;

   glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
   glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
   glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
   glGetIntegerv(GL_UNPACK_ALIGNMENT, &align);
   if (GL_PLANAR_ROW_LENGTH)
      glGetIntegerv(GL_UNPACK_ROW_LENGTH, &row_length);
   glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &abuf);
   glGetIntegerv(GL_VIEWPORT, vp);
#ifdef GL_PLANAR_HAVE_VAO
   if (core)
      glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
#endif
#ifdef GL_PLANAR_HAVE_UNPACK_BUFFER
   /* A lent texture's buffer may be bound: the planes are host memory */
   glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack);
   if (unpack)
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
#endif
   for (i = 0; i < 3; i++)
   {
      glActiveTexture(GL_TEXTURE0 + i);
      glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound[i]);
   }
   blend   = glIsEnabled(GL_BLEND);
   scissor = glIsEnabled(GL_SCISSOR_TEST);
   depth   = glIsEnabled(GL_DEPTH_TEST);
   cull    = glIsEnabled(GL_CULL_FACE);
   stencil = glIsEnabled(GL_STENCIL_TEST);

   /* The planes, from where they lie */
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, t->plane[0]);
   gl_planar_plane(one, 1, t->w, t->h, tp->planes[0], tp->strides[0]);
   glActiveTexture(GL_TEXTURE1);
   glBindTexture(GL_TEXTURE_2D, t->plane[1]);
   gl_planar_plane(interleaved ? two : one, interleaved ? 2 : 1, cw, ch,
         c0, swap ? tp->strides[2] : tp->strides[1]);
   glActiveTexture(GL_TEXTURE2);
   glBindTexture(GL_TEXTURE_2D, interleaved ? t->plane[1] : t->plane[2]);
   if (!interleaved)
      gl_planar_plane(one, 1, cw, ch, tp->planes[2], tp->strides[2]);

   /* The conversion */
   image_yuv_coefficients(tp->yuv, k);
   size[0] = (float)t->w;
   size[1] = (float)t->h;
   size[2] = (float)cw;
   size[3] = (float)ch;
   coef[0] = k[0];
   coef[1] = k[1];
   coef[2] = k[2];
   coef[3] = k[3];
   coef[4] = k[4];
   coef[5] = k[5];
   coef[6] = interleaved ? 1.0f : 0.0f;
   coef[7] = swap ? 1.0f : 0.0f;
   glBindFramebuffer(GL_FRAMEBUFFER, t->fbo);
   glViewport(0, 0, (GLsizei)t->w, (GLsizei)t->h);
   glDisable(GL_BLEND);
   glDisable(GL_SCISSOR_TEST);
   glDisable(GL_DEPTH_TEST);
   glDisable(GL_CULL_FACE);
   glDisable(GL_STENCIL_TEST);
   glUseProgram(p->prog);
   glUniform1i(p->loc[0], 0);
   glUniform1i(p->loc[1], 1);
   glUniform1i(p->loc[2], 2);
   glUniform4fv(p->loc[3], 1, size);
   glUniform4fv(p->loc[4], 2, coef);
#ifdef GL_PLANAR_HAVE_VAO
   if (core)
      glBindVertexArray(p->vao);
   else
#endif
   {
      glGetVertexAttribiv(GL_PLANAR_ATTR, GL_VERTEX_ATTRIB_ARRAY_ENABLED,
            &attr_on);
      glBindBuffer(GL_ARRAY_BUFFER, p->vbo);
      glEnableVertexAttribArray(GL_PLANAR_ATTR);
      glVertexAttribPointer(GL_PLANAR_ATTR, 2, GL_FLOAT, GL_FALSE, 0, NULL);
   }
   glDrawArrays(GL_TRIANGLES, 0, 3);

   /* The driver's state as it was */
#ifdef GL_PLANAR_HAVE_VAO
   if (core)
      glBindVertexArray((GLuint)vao);
   else
#endif
   if (!attr_on)
      glDisableVertexAttribArray(GL_PLANAR_ATTR);
   glBindBuffer(GL_ARRAY_BUFFER, (GLuint)abuf);
   glUseProgram((GLuint)prog);
   glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)fbo);
   glViewport(vp[0], vp[1], vp[2], vp[3]);
   if (blend)
      glEnable(GL_BLEND);
   if (scissor)
      glEnable(GL_SCISSOR_TEST);
   if (depth)
      glEnable(GL_DEPTH_TEST);
   if (cull)
      glEnable(GL_CULL_FACE);
   if (stencil)
      glEnable(GL_STENCIL_TEST);
   for (i = 0; i < 3; i++)
   {
      glActiveTexture(GL_TEXTURE0 + i);
      glBindTexture(GL_TEXTURE_2D, (GLuint)bound[i]);
   }
   glActiveTexture((GLenum)active);
   glPixelStorei(GL_UNPACK_ALIGNMENT, align);
   if (GL_PLANAR_ROW_LENGTH)
      glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length);
#ifdef GL_PLANAR_HAVE_UNPACK_BUFFER
   if (unpack)
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, (GLuint)unpack);
#endif
   return true;
}

static GLuint gl_planar_texture(GLint internal, GLenum fmt, unsigned w,
      unsigned h, GLint filter)
{
   GLuint id = 0;
   glGenTextures(1, &id);
   glBindTexture(GL_TEXTURE_2D, id);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glTexImage2D(GL_TEXTURE_2D, 0, internal, (GLsizei)w, (GLsizei)h, 0, fmt,
         GL_UNSIGNED_BYTE, NULL);
   return id;
}

static void gl_planar_destroy(gl_planar_tex_t *t)
{
   unsigned i;
   for (i = 0; i < 3; i++)
      if (t->plane[i])
         glDeleteTextures(1, &t->plane[i]);
   if (t->fbo)
      glDeleteFramebuffers(1, &t->fbo);
   free(t);
}

unsigned gl_planar_load(gl_planar_t *p, const struct texture_image *ti,
      bool linear, bool core)
{
   const struct texture_planar *tp = ti ? ti->planar : NULL;
   gl_planar_tex_t *t;
   GLint prev_tex = 0, prev_fbo = 0, active = 0;
   GLenum one, two;
   GLint one_int, two_int, rgba_int;
   unsigned cw, ch;
   bool ok;

   if (     !tp || !ti->width || !ti->height
         || (tp->chroma_step != 1 && tp->chroma_step != 2)
         || !gl_planar_init(p, core)
         || !(t = (gl_planar_tex_t*)calloc(1, sizeof(*t))))
      return 0;
   core     = (p->flags & GL_PLANAR_CORE) != 0;
   one      = core ? GL_RED : GL_LUMINANCE;
   two      = core ? GL_RG  : GL_LUMINANCE_ALPHA;
   one_int  = core ? GL_R8  : GL_LUMINANCE;
   two_int  = core ? GL_RG8 : GL_LUMINANCE_ALPHA;
#if defined(HAVE_OPENGLES) && !defined(HAVE_OPENGLES3)
   rgba_int = GL_RGBA;
#else
   rgba_int = GL_RGBA8;
#endif
   t->w           = ti->width;
   t->h           = ti->height;
   t->interleaved = tp->chroma_step == 2;
   cw             = (t->w + 1) / 2;
   ch             = (t->h + 1) / 2;

   glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
   glActiveTexture(GL_TEXTURE0);
   glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
   glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
   t->tex      = gl_planar_texture(rgba_int, GL_RGBA, t->w, t->h,
         linear ? GL_LINEAR : GL_NEAREST);
   t->plane[0] = gl_planar_texture(one_int, one, t->w, t->h, GL_NEAREST);
   t->plane[1] = gl_planar_texture(t->interleaved ? two_int : one_int,
         t->interleaved ? two : one, cw, ch, GL_NEAREST);
   if (!t->interleaved)
      t->plane[2] = gl_planar_texture(one_int, one, cw, ch, GL_NEAREST);
   glGenFramebuffers(1, &t->fbo);
   glBindFramebuffer(GL_FRAMEBUFFER, t->fbo);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
         GL_TEXTURE_2D, t->tex, 0);
   ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
   glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
   glBindTexture(GL_TEXTURE_2D, (GLuint)prev_tex);
   glActiveTexture((GLenum)active);

   if (!ok || !t->tex || !gl_planar_draw(p, t, ti))
   {
      if (t->tex)
         glDeleteTextures(1, &t->tex);
      gl_planar_destroy(t);
      return 0;
   }
   t->next = p->list;
   p->list = t;
   return t->tex;
}

bool gl_planar_update(gl_planar_t *p, unsigned tex,
      const struct texture_image *ti)
{
   gl_planar_tex_t *t = gl_planar_find(p, tex);
   return t && ti && gl_planar_draw(p, t, ti);
}

void gl_planar_forget(gl_planar_t *p, unsigned tex)
{
   gl_planar_tex_t **cur = &p->list;
   while (*cur)
   {
      gl_planar_tex_t *t = *cur;
      if (t->tex == (GLuint)tex)
      {
         *cur = t->next;
         gl_planar_destroy(t);
         return;
      }
      cur = &t->next;
   }
}

void gl_planar_free(gl_planar_t *p)
{
   while (p->list)
   {
      gl_planar_tex_t *t = p->list;
      p->list            = t->next;
      gl_planar_destroy(t);
   }
   if (p->prog)
      glDeleteProgram(p->prog);
   if (p->vbo)
      glDeleteBuffers(1, &p->vbo);
#ifdef GL_PLANAR_HAVE_VAO
   if (p->vao)
      glDeleteVertexArrays(1, &p->vao);
#endif
   memset(p, 0, sizeof(*p));
}
#endif
