/*  RetroArch - A frontend for libretro.
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

#if defined(HAVE_KMS) && defined(HAVE_EGL) \
      && (defined(HAVE_OPENGL) || defined(HAVE_OPENGLES))

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <libdrm/drm_fourcc.h>

#include <glsym/glsym.h>
#include <string/stdstring.h>
#include <retro_miscellaneous.h>

#include "../../verbosity.h"
#include "egl_gbm_frames.h"

typedef void (*egl_gbm_image_target_t)(GLenum target, void *image);

typedef struct
{
   struct gbm_bo *bo;
   void *map;
   void *map_data;
   EGLImageKHR image;
   /* After the last draw that may read it, while no texture holds it */
   EGLSyncKHR fence;
   /* The frame texture slots that hold it */
   uint32_t held;
   uint32_t stride;
   /* What may have been written: the rest is blank */
   unsigned used_w;
   unsigned used_h;
} egl_gbm_buf_t;

struct egl_gbm_frames
{
   egl_gbm_buf_t buf[EGL_GBM_FRAMES_MAX];
   struct gbm_device *dev;
   EGLDisplay dpy;
   PFNEGLCREATEIMAGEKHRPROC create_image;
   PFNEGLDESTROYIMAGEKHRPROC destroy_image;
   PFNEGLCREATESYNCKHRPROC create_sync;
   PFNEGLDESTROYSYNCKHRPROC destroy_sync;
   PFNEGLCLIENTWAITSYNCKHRPROC client_wait;
   egl_gbm_image_target_t image_target;
   unsigned width;
   unsigned height;
   unsigned bpp;
   unsigned count;
   unsigned next;
   /* The buffer the last frame came from, which the cached frame names */
   int last;
};

/* Per format, XRGB8888 then RGB565: 0 untried, 1 sampled as written,
 * -1 not */
static int egl_gbm_frames_verified[2];

static bool egl_gbm_has_ext(const char *list, const char *ext)
{
   size_t len = strlen(ext);
   const char *p;
   if (!list)
      return false;
   for (p = list; (p = strstr(p, ext)); p += len)
      if ((p == list || p[-1] == ' ') && (!p[len] || p[len] == ' '))
         return true;
   return false;
}

static void egl_gbm_buf_free(egl_gbm_frames_t *f, egl_gbm_buf_t *b)
{
   if (b->fence)
      f->destroy_sync(f->dpy, b->fence);
   if (b->image)
      f->destroy_image(f->dpy, b->image);
   if (b->map)
      gbm_bo_unmap(b->bo, b->map_data);
   if (b->bo)
      gbm_bo_destroy(b->bo);
   memset(b, 0, sizeof(*b));
}

static bool egl_gbm_buf_init(egl_gbm_frames_t *f, egl_gbm_buf_t *b)
{
   int fd;
   EGLint attr[13];
   uint32_t stride = 0;
   uint32_t fmt    = (f->bpp == 4) ? GBM_FORMAT_XRGB8888 : GBM_FORMAT_RGB565;

   if (!(b->bo = gbm_bo_create(f->dev, f->width, f->height, fmt,
               GBM_BO_USE_LINEAR | GBM_BO_USE_RENDERING)))
      return false;
   /* Mapped for good: where the map is the buffer itself, the GPU
    * samples what the core writes, which the check makes sure of */
   if (!(b->map = gbm_bo_map(b->bo, 0, 0, f->width, f->height,
               GBM_BO_TRANSFER_READ_WRITE, &stride, &b->map_data)))
      return false;
   b->stride = stride;
   if (     stride != gbm_bo_get_stride(b->bo)
         || (fd = gbm_bo_get_fd(b->bo)) < 0)
      return false;

   attr[0]  = EGL_WIDTH;
   attr[1]  = (EGLint)f->width;
   attr[2]  = EGL_HEIGHT;
   attr[3]  = (EGLint)f->height;
   attr[4]  = EGL_LINUX_DRM_FOURCC_EXT;
   attr[5]  = (EGLint)fmt;
   attr[6]  = EGL_DMA_BUF_PLANE0_FD_EXT;
   attr[7]  = fd;
   attr[8]  = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
   attr[9]  = 0;
   attr[10] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
   attr[11] = (EGLint)stride;
   attr[12] = EGL_NONE;
   b->image = f->create_image(f->dpy, EGL_NO_CONTEXT,
         EGL_LINUX_DMA_BUF_EXT, NULL, attr);
   /* EGL holds its own reference */
   close(fd);
   return b->image != EGL_NO_IMAGE_KHR;
}

/* Pixel (x, y) of pattern @n, and what the GPU should read back */
static uint32_t egl_gbm_pattern(unsigned bpp, unsigned x, unsigned y,
      unsigned n, uint8_t rgba[4])
{
   if (bpp == 4)
   {
      uint32_t r = (x * 29 + n * 101) & 0xff;
      uint32_t g = (y * 31 + n * 53)  & 0xff;
      uint32_t b = (x * 7 + y * 13 + n * 17) & 0xff;
      rgba[0] = (uint8_t)r;
      rgba[1] = (uint8_t)g;
      rgba[2] = (uint8_t)b;
      rgba[3] = 0xff;
      return (r << 16) | (g << 8) | b;
   }
   else
   {
      uint32_t r = (x * 3 + n * 11) & 0x1f;
      uint32_t g = (y * 5 + n * 23) & 0x3f;
      uint32_t b = (x + y + n * 7)  & 0x1f;
      rgba[0] = (uint8_t)((r << 3) | (r >> 2));
      rgba[1] = (uint8_t)((g << 2) | (g >> 4));
      rgba[2] = (uint8_t)((b << 3) | (b >> 2));
      rgba[3] = 0xff;
      return (r << 11) | (g << 5) | b;
   }
}

#define EGL_GBM_CHECK 8

static const char *egl_gbm_check_vs =
   "attribute vec2 pos;\n"
   "uniform vec2 scale;\n"
   "varying vec2 uv;\n"
   "void main() { uv = (pos * 0.5 + 0.5) * scale;"
   " gl_Position = vec4(pos, 0.0, 1.0); }\n";
static const char *egl_gbm_check_fs =
   "#ifdef GL_ES\nprecision mediump float;\n#endif\n"
   "uniform sampler2D tex;\n"
   "varying vec2 uv;\n"
   "void main() { gl_FragColor = texture2D(tex, uv); }\n";

static GLuint egl_gbm_shader(GLenum type, const char *src)
{
   GLint ok    = 0;
   GLuint s    = glCreateShader(type);
   glShaderSource(s, 1, &src, NULL);
   glCompileShader(s);
   glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
   if (!ok)
   {
      glDeleteShader(s);
      return 0;
   }
   return s;
}

/* Writes two patterns in turn through the one mapping, each sampled
 * through the image into a small target and read back: a staging map,
 * a sampler that cannot read linear, or a lost write all show. */
static bool egl_gbm_frames_check(egl_gbm_frames_t *f)
{
   static const GLfloat quad[8] = { -1, -1, 1, -1, -1, 1, 1, 1 };
   uint8_t got[EGL_GBM_CHECK * EGL_GBM_CHECK * 4];
   uint8_t want[4];
   GLint prog_was = 0, fb_was = 0, tex_was = 0, ab_was = 0, unit_was = 0;
   GLint vp_was[4];
   GLint attr_was = 0;
   GLboolean blend, scissor, depth, stencil, cull;
   GLuint vs, fs, prog = 0, tex = 0, target = 0, fbo = 0;
   unsigned x, y, n;
   bool ok = false;
   egl_gbm_buf_t *b = &f->buf[0];

   glGetIntegerv(GL_CURRENT_PROGRAM, &prog_was);
   glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fb_was);
   glGetIntegerv(GL_ACTIVE_TEXTURE, &unit_was);
   glActiveTexture(GL_TEXTURE0);
   glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex_was);
   glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &ab_was);
   glGetIntegerv(GL_VIEWPORT, vp_was);
   glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &attr_was);
   blend   = glIsEnabled(GL_BLEND);
   scissor = glIsEnabled(GL_SCISSOR_TEST);
   depth   = glIsEnabled(GL_DEPTH_TEST);
   stencil = glIsEnabled(GL_STENCIL_TEST);
   cull    = glIsEnabled(GL_CULL_FACE);
   glDisable(GL_BLEND);
   glDisable(GL_SCISSOR_TEST);
   glDisable(GL_DEPTH_TEST);
   glDisable(GL_STENCIL_TEST);
   glDisable(GL_CULL_FACE);

   vs = egl_gbm_shader(GL_VERTEX_SHADER, egl_gbm_check_vs);
   fs = egl_gbm_shader(GL_FRAGMENT_SHADER, egl_gbm_check_fs);
   if (vs && fs)
   {
      GLint linked = 0;
      prog         = glCreateProgram();
      glAttachShader(prog, vs);
      glAttachShader(prog, fs);
      glBindAttribLocation(prog, 0, "pos");
      glLinkProgram(prog);
      glGetProgramiv(prog, GL_LINK_STATUS, &linked);
      if (!linked)
      {
         glDeleteProgram(prog);
         prog = 0;
      }
   }
   if (vs)
      glDeleteShader(vs);
   if (fs)
      glDeleteShader(fs);
   if (!prog)
      goto end;

   glGenTextures(1, &target);
   glBindTexture(GL_TEXTURE_2D, target);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, EGL_GBM_CHECK, EGL_GBM_CHECK, 0,
         GL_RGBA, GL_UNSIGNED_BYTE, NULL);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glGenFramebuffers(1, &fbo);
   glBindFramebuffer(GL_FRAMEBUFFER, fbo);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
         GL_TEXTURE_2D, target, 0);
   if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
      goto end;

   glGenTextures(1, &tex);
   glBindTexture(GL_TEXTURE_2D, tex);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   f->image_target(GL_TEXTURE_2D, b->image);

   glUseProgram(prog);
   glUniform1i(glGetUniformLocation(prog, "tex"), 0);
   glUniform2f(glGetUniformLocation(prog, "scale"),
         (GLfloat)EGL_GBM_CHECK / f->width,
         (GLfloat)EGL_GBM_CHECK / f->height);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, quad);
   glEnableVertexAttribArray(0);
   glViewport(0, 0, EGL_GBM_CHECK, EGL_GBM_CHECK);
   glPixelStorei(GL_PACK_ALIGNMENT, 4);

   for (n = 0; n < 2; n++)
   {
      for (y = 0; y < EGL_GBM_CHECK; y++)
         for (x = 0; x < EGL_GBM_CHECK; x++)
         {
            uint8_t *row = (uint8_t*)b->map + (size_t)y * b->stride;
            uint32_t px  = egl_gbm_pattern(f->bpp, x, y, n, want);
            if (f->bpp == 4)
               ((uint32_t*)row)[x] = px;
            else
               ((uint16_t*)row)[x] = (uint16_t)px;
         }
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      glReadPixels(0, 0, EGL_GBM_CHECK, EGL_GBM_CHECK, GL_RGBA,
            GL_UNSIGNED_BYTE, got);
      for (y = 0; y < EGL_GBM_CHECK; y++)
         for (x = 0; x < EGL_GBM_CHECK; x++)
         {
            unsigned c;
            const uint8_t *g = got + (y * EGL_GBM_CHECK + x) * 4;
            egl_gbm_pattern(f->bpp, x, y, n, want);
            /* 16-bit channels may be widened by rounding, not replication */
            for (c = 0; c < 3; c++)
               if (g[c] > want[c] + 1 || g[c] + 1 < want[c])
                  goto end;
         }
   }
   ok = true;
   b->used_w = EGL_GBM_CHECK;
   b->used_h = EGL_GBM_CHECK;

end:
   if (fbo)
      glDeleteFramebuffers(1, &fbo);
   if (target)
      glDeleteTextures(1, &target);
   if (tex)
      glDeleteTextures(1, &tex);
   if (prog)
      glDeleteProgram(prog);
   glUseProgram((GLuint)prog_was);
   glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)fb_was);
   glBindTexture(GL_TEXTURE_2D, (GLuint)tex_was);
   glActiveTexture((GLenum)unit_was);
   glBindBuffer(GL_ARRAY_BUFFER, (GLuint)ab_was);
   glViewport(vp_was[0], vp_was[1], vp_was[2], vp_was[3]);
   if (!attr_was)
      glDisableVertexAttribArray(0);
   if (blend)
      glEnable(GL_BLEND);
   if (scissor)
      glEnable(GL_SCISSOR_TEST);
   if (depth)
      glEnable(GL_DEPTH_TEST);
   if (stencil)
      glEnable(GL_STENCIL_TEST);
   if (cull)
      glEnable(GL_CULL_FACE);
   return ok;
}

egl_gbm_frames_t *egl_gbm_frames_new(int drm_fd, unsigned width,
      unsigned height, unsigned bpp, unsigned count)
{
   unsigned i;
   const char *ext;
   egl_gbm_frames_t *f;
   int *verified;

   if (     drm_fd < 0 || (bpp != 4 && bpp != 2) || !width || !height
         || count < 3 || count > EGL_GBM_FRAMES_MAX)
      return NULL;
   verified = &egl_gbm_frames_verified[bpp == 4 ? 0 : 1];
   if (*verified < 0)
      return NULL;
   if (!(f = (egl_gbm_frames_t*)calloc(1, sizeof(*f))))
      return NULL;
   f->width  = width;
   f->height = height;
   f->bpp    = bpp;
   f->count  = count;
   f->last   = -1;
   f->dpy    = eglGetCurrentDisplay();
   ext       = (f->dpy != EGL_NO_DISPLAY)
      ? eglQueryString(f->dpy, EGL_EXTENSIONS) : NULL;
   if (     !egl_gbm_has_ext(ext, "EGL_EXT_image_dma_buf_import")
         || !egl_gbm_has_ext(ext, "EGL_KHR_fence_sync"))
      goto fail;
   f->create_image  = (PFNEGLCREATEIMAGEKHRPROC)
      eglGetProcAddress("eglCreateImageKHR");
   f->destroy_image = (PFNEGLDESTROYIMAGEKHRPROC)
      eglGetProcAddress("eglDestroyImageKHR");
   f->create_sync   = (PFNEGLCREATESYNCKHRPROC)
      eglGetProcAddress("eglCreateSyncKHR");
   f->destroy_sync  = (PFNEGLDESTROYSYNCKHRPROC)
      eglGetProcAddress("eglDestroySyncKHR");
   f->client_wait   = (PFNEGLCLIENTWAITSYNCKHRPROC)
      eglGetProcAddress("eglClientWaitSyncKHR");
   f->image_target  = (egl_gbm_image_target_t)
      eglGetProcAddress("glEGLImageTargetTexture2DOES");
   if (     !f->create_image || !f->destroy_image || !f->create_sync
         || !f->destroy_sync || !f->client_wait || !f->image_target
         || !(f->dev = gbm_create_device(drm_fd)))
      goto fail;

   for (i = 0; i < count; i++)
      if (!egl_gbm_buf_init(f, &f->buf[i]))
         goto fail;

   if (!*verified)
   {
      *verified = egl_gbm_frames_check(f) ? 1 : -1;
      if (*verified < 0)
      {
         RARCH_LOG("[GL] Scanout-memory frames: the GPU does not sample "
               "what is written; frames are uploaded.\n");
         goto fail;
      }
      RARCH_LOG("[GL] Software frames are sampled where the core writes "
            "them.\n");
   }
   return f;

fail:
   egl_gbm_frames_free(&f);
   return NULL;
}

bool egl_gbm_frames_fits(const egl_gbm_frames_t *f, unsigned width,
      unsigned height, unsigned bpp, unsigned count)
{
   return f && f->width == width && f->height == height && f->bpp == bpp
      && f->count == count;
}

static bool egl_gbm_buf_idle(egl_gbm_frames_t *f, egl_gbm_buf_t *b)
{
   EGLint r;
   if (b->held)
      return false;
   if (!b->fence)
      return true;
   r = f->client_wait(f->dpy, b->fence, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, 0);
   if (r != EGL_CONDITION_SATISFIED_KHR)
      return false;
   f->destroy_sync(f->dpy, b->fence);
   b->fence = EGL_NO_SYNC_KHR;
   return true;
}

/* Blanks what an earlier, larger frame left outside @w x @h: the edge
 * filtering of a smaller frame would reach it */
static void egl_gbm_buf_trim(egl_gbm_frames_t *f, egl_gbm_buf_t *b,
      unsigned w, unsigned h)
{
   unsigned y;
   uint8_t *row = (uint8_t*)b->map;
   if (b->used_w > w)
      for (y = 0; y < MIN(b->used_h, h); y++)
         memset(row + (size_t)y * b->stride + (size_t)w * f->bpp, 0,
               (size_t)(b->used_w - w) * f->bpp);
   for (y = h; y < b->used_h; y++)
      memset(row + (size_t)y * b->stride, 0, (size_t)b->used_w * f->bpp);
   b->used_w = w;
   b->used_h = h;
}

void *egl_gbm_frames_lend(egl_gbm_frames_t *f, unsigned width,
      unsigned height, size_t *pitch)
{
   unsigned k;
   if (     !f || !pitch || !width || !height
         || width > f->width || height > f->height)
      return NULL;
   for (k = 0; k < f->count; k++)
   {
      unsigned i = (f->next + k) % f->count;
      if ((int)i != f->last && egl_gbm_buf_idle(f, &f->buf[i]))
      {
         egl_gbm_buf_t *b = &f->buf[i];
         if (b->used_w > width || b->used_h > height)
            egl_gbm_buf_trim(f, b, width, height);
         else
         {
            b->used_w = width;
            b->used_h = height;
         }
         f->next = (i + 1) % f->count;
         *pitch  = b->stride;
         return b->map;
      }
   }
   return NULL;
}

/* Frame texture @slot no longer holds buffer @b */
static void egl_gbm_buf_release(egl_gbm_frames_t *f, egl_gbm_buf_t *b,
      unsigned slot)
{
   b->held &= ~(1u << slot);
   if (!b->held && !b->fence)
      b->fence = f->create_sync(f->dpy, EGL_SYNC_FENCE_KHR, NULL);
}

bool egl_gbm_frames_attach(egl_gbm_frames_t *f, const void *frame,
      unsigned slot)
{
   unsigned i, j;
   if (!f || !frame || slot >= 32)
      return false;
   for (i = 0; i < f->count; i++)
      if (f->buf[i].map == frame)
         break;
   if (i == f->count)
      return false;
   for (j = 0; j < f->count; j++)
      if (j != i && (f->buf[j].held & (1u << slot)))
         egl_gbm_buf_release(f, &f->buf[j], slot);
   if (!(f->buf[i].held & (1u << slot)))
   {
      f->image_target(GL_TEXTURE_2D, f->buf[i].image);
      f->buf[i].held |= 1u << slot;
   }
   /* Read again from now on: a fence from an earlier release is stale */
   if (f->buf[i].fence)
   {
      f->destroy_sync(f->dpy, f->buf[i].fence);
      f->buf[i].fence = EGL_NO_SYNC_KHR;
   }
   f->last = (int)i;
   return true;
}

bool egl_gbm_frames_detach(egl_gbm_frames_t *f, unsigned slot)
{
   unsigned j;
   bool held = false;
   if (!f || slot >= 32)
      return false;
   for (j = 0; j < f->count; j++)
      if (f->buf[j].held & (1u << slot))
      {
         egl_gbm_buf_release(f, &f->buf[j], slot);
         held = true;
      }
   f->last = -1;
   return held;
}

void egl_gbm_frames_free(egl_gbm_frames_t **fp)
{
   unsigned i;
   egl_gbm_frames_t *f;
   if (!fp || !(f = *fp))
      return;
   for (i = 0; i < EGL_GBM_FRAMES_MAX; i++)
      egl_gbm_buf_free(f, &f->buf[i]);
   if (f->dev)
      gbm_device_destroy(f->dev);
   free(f);
   *fp = NULL;
}

#else
/* Not built: ISO C wants something in the unit */
typedef int egl_gbm_frames_unused;
#endif
