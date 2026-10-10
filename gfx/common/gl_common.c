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

#ifdef HAVE_GL_TEXTURE_LEND
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

#define GL_LEND_SLOTS 2

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

/* Persistent mapping is GL 4.4 or ARB_buffer_storage, fences GL 3.2
 * or ARB_sync */
static bool gl_texture_lend_supported(void)
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
   GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT
      | GL_MAP_COHERENT_BIT;

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
      glBufferStorage(GL_PIXEL_UNPACK_BUFFER, (GLsizeiptr)st->size,
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
#endif

#ifdef HAVE_GL_PLANAR
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
   GLint ok  = 0;
   GLuint sh = glCreateShader(stage);
   if (!sh)
      return 0;
   glShaderSource(sh, n, src, NULL);
   glCompileShader(sh);
   glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
   if (!ok)
   {
      glDeleteShader(sh);
      return 0;
   }
   return sh;
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
