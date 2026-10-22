/* The check in gfx/common/egl_gbm_frames.c on a real GL (Mesa, EGL
 * surfaceless): GBM and the dma-buf import are stand-ins, the import
 * sampling the buffer's memory as it is at each draw, so the shader,
 * the target, the draw and the readback are the GL's own. A buffer the
 * GPU samples as written must pass, in both formats, on GLES 2 and on
 * desktop GL; one whose writes the GPU never sees must fail. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <glsym/glsym.h>

#include "../../../gfx/common/egl_gbm_frames.h"

void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }

struct gbm_device { int fd; };
struct gbm_bo { unsigned w, h, bpp; uint32_t stride; uint8_t *mem, *seen; };
static int blind; /* the GPU never sees what is written */
static struct gbm_bo *last_bo;

struct gbm_device *gbm_create_device(int fd)
{ struct gbm_device *d = calloc(1, sizeof(*d)); d->fd = fd; return d; }
void gbm_device_destroy(struct gbm_device *d) { free(d); }
struct gbm_bo *gbm_bo_create(struct gbm_device *d, uint32_t w, uint32_t h,
      uint32_t fmt, uint32_t flags)
{
   struct gbm_bo *b = calloc(1, sizeof(*b));
   (void)d; (void)flags;
   b->w = w; b->h = h; b->bpp = fmt == GBM_FORMAT_XRGB8888 ? 4 : 2;
   b->stride = (w * b->bpp + 63) & ~63u;
   b->mem  = calloc(1, (size_t)b->stride * h);
   b->seen = calloc(1, (size_t)b->stride * h);
   last_bo = b;
   return b;
}
void gbm_bo_destroy(struct gbm_bo *b) { free(b->mem); free(b->seen); free(b); }
uint32_t gbm_bo_get_stride(struct gbm_bo *b) { return b->stride; }
int gbm_bo_get_fd(struct gbm_bo *b) { (void)b; return open("/dev/null", O_RDONLY); }
void *gbm_bo_map(struct gbm_bo *b, uint32_t x, uint32_t y, uint32_t w,
      uint32_t h, uint32_t f, uint32_t *stride, void **md)
{ (void)x; (void)y; (void)w; (void)h; (void)f; *stride = b->stride; *md = b; return b->mem; }
void gbm_bo_unmap(struct gbm_bo *b, void *md) { (void)b; (void)md; }

/* The import: an image is its buffer; the texture it is bound to is
 * filled from the buffer before each draw, as sampling it would */
static struct gbm_bo *tex_bo[256];
static EGLImageKHR fake_create_image(EGLDisplay d, EGLContext c, EGLenum t,
      EGLClientBuffer b, const EGLint *a)
{ (void)d; (void)c; (void)t; (void)b; (void)a; return (EGLImageKHR)last_bo; }
static EGLBoolean fake_destroy_image(EGLDisplay d, EGLImageKHR i)
{ (void)d; (void)i; return EGL_TRUE; }
static void fake_image_target(GLenum target, void *image)
{
   GLint t = 0;
   (void)target;
   glGetIntegerv(GL_TEXTURE_BINDING_2D, &t);
   if (t > 0 && t < 256)
      tex_bo[t] = (struct gbm_bo*)image;
}

void (*__real_eglGetProcAddress(const char *n))(void);
void (*__wrap_eglGetProcAddress(const char *n))(void)
{
   if (!strcmp(n, "eglCreateImageKHR"))  return (void(*)(void))fake_create_image;
   if (!strcmp(n, "eglDestroyImageKHR")) return (void(*)(void))fake_destroy_image;
   if (!strcmp(n, "glEGLImageTargetTexture2DOES")) return (void(*)(void))fake_image_target;
   return __real_eglGetProcAddress(n);
}
const char *__real_eglQueryString(EGLDisplay d, EGLint n);
const char *__wrap_eglQueryString(EGLDisplay d, EGLint n)
{
   static char ext[8192];
   const char *r = __real_eglQueryString(d, n);
   if (n != EGL_EXTENSIONS)
      return r;
   snprintf(ext, sizeof(ext), "%s EGL_EXT_image_dma_buf_import", r ? r : "");
   return ext;
}

static void fill(GLint t)
{
   unsigned x, y;
   struct gbm_bo *b = tex_bo[t];
   uint8_t *rgba;
   if (!b)
      return;
   if (!blind)
      memcpy(b->seen, b->mem, (size_t)b->stride * b->h);
   rgba = malloc((size_t)b->w * b->h * 4);
   for (y = 0; y < b->h; y++)
      for (x = 0; x < b->w; x++)
      {
         const uint8_t *row = b->seen + (size_t)y * b->stride;
         uint8_t *o = rgba + ((size_t)y * b->w + x) * 4;
         if (b->bpp == 4)
         {
            uint32_t p = ((const uint32_t*)row)[x];
            o[0] = p >> 16; o[1] = p >> 8; o[2] = p;
         }
         else
         {
            uint16_t p = ((const uint16_t*)row)[x];
            o[0] = ((p >> 11) & 0x1f) * 255 / 31;
            o[1] = ((p >> 5) & 0x3f) * 255 / 63;
            o[2] = (p & 0x1f) * 255 / 31;
         }
         o[3] = 0xff;
      }
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, b->w, b->h, 0, GL_RGBA,
         GL_UNSIGNED_BYTE, rgba);
   free(rgba);
}

void __real_glDrawArrays(GLenum m, GLint f, GLsizei c);
void __wrap_glDrawArrays(GLenum m, GLint f, GLsizei c)
{ GLint t = 0; glGetIntegerv(GL_TEXTURE_BINDING_2D, &t); fill(t); __real_glDrawArrays(m, f, c); }

int main(int argc, char **argv)
{
   static const EGLint cfg_attr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
      EGL_RENDERABLE_TYPE,
#ifdef HAVE_OPENGLES
      EGL_OPENGL_ES2_BIT,
#else
      EGL_OPENGL_BIT,
#endif
      EGL_NONE };
   static const EGLint ctx_attr[] = {
#ifdef HAVE_OPENGLES
      EGL_CONTEXT_CLIENT_VERSION, 2,
#endif
      EGL_NONE };
   EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   EGLConfig cfg;
   EGLContext ctx;
   EGLint n = 0;
   egl_gbm_frames_t *f;
   int fails = 0;
   unsigned bpp;

   if (!eglInitialize(dpy, NULL, NULL)
         || !eglBindAPI(
#ifdef HAVE_OPENGLES
            EGL_OPENGL_ES_API
#else
            EGL_OPENGL_API
#endif
            )
         || !eglChooseConfig(dpy, cfg_attr, &cfg, 1, &n) || n < 1
         || !(ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attr))
         || !eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx))
   {
      printf("no EGL context\n");
      return 1;
   }
#ifndef HAVE_OPENGLES
   rglgen_resolve_symbols((rglgen_proc_address_t)eglGetProcAddress);
#endif
   printf("%s\n", (const char*)glGetString(GL_VERSION));

   /* Seen: both formats taken. Blind: both refused, and not tried again */
   blind = argc > 1 && !strcmp(argv[1], "blind");
   for (bpp = 2; bpp <= 4; bpp += 2)
   {
      f = egl_gbm_frames_new(3, 256, 128, bpp, 3);
      if (!!f == !!blind)
      {
         printf("FAIL %s %s\n", bpp == 4 ? "XRGB8888" : "RGB565",
               blind ? "passed though the GPU never sees the write"
                     : "sampled as written was refused");
         fails++;
      }
      else
         printf("ok   %s %s\n", bpp == 4 ? "XRGB8888" : "RGB565",
               blind ? "refused" : "taken");
      egl_gbm_frames_free(&f);
      if (blind && (f = egl_gbm_frames_new(3, 256, 128, bpp, 3)))
      {
         printf("FAIL tried again after it failed\n");
         fails++;
         egl_gbm_frames_free(&f);
      }
   }
   if (glGetError() != GL_NO_ERROR)
   {
      printf("FAIL the check left a GL error\n");
      fails++;
   }
   printf(fails ? "FAIL\n" : "PASS mesa_check\n");
   return fails ? 1 : 0;
}
