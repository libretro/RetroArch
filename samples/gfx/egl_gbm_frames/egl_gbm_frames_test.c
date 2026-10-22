/* gfx/common/egl_gbm_frames.c against a fake GBM, EGL and GPU.
 *
 * The GPU samples a buffer either where the CPU writes it, or from a
 * copy taken when it was mapped, as a driver that maps through staging
 * memory does: the check that runs before a format is first used must
 * take the first and refuse the second, for good. A buffer a texture
 * holds, one a draw may still read, and the last frame's are never
 * lent; the GL state the check touches comes back as it was; nothing
 * outlives the free. */
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
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }

static unsigned failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("   FAIL "); \
   printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---- GBM --------------------------------------------------------------- */

struct gbm_device { int fd; };
struct gbm_bo
{
   unsigned w, h, bpp;
   uint32_t stride;
   uint8_t *mem;      /* what the GPU reads */
   uint8_t *staging;  /* what a staging map hands the CPU */
   bool mapped;
};

/* The import finds the buffer last made, as each is imported at once */
static struct gbm_bo *last_bo_made;
static int  staging_maps;   /* the driver maps through staging memory */
static int  devices, bos, maps, devices_made;

struct gbm_device *gbm_create_device(int fd)
{
   struct gbm_device *d = (struct gbm_device*)calloc(1, sizeof(*d));
   d->fd = fd;
   devices++;
   devices_made++;
   return d;
}
void gbm_device_destroy(struct gbm_device *d) { free(d); devices--; }

struct gbm_bo *gbm_bo_create(struct gbm_device *d, uint32_t w, uint32_t h,
      uint32_t fmt, uint32_t flags)
{
   struct gbm_bo *b = (struct gbm_bo*)calloc(1, sizeof(*b));
   (void)d; (void)flags;
   b->w      = w;
   b->h      = h;
   b->bpp    = (fmt == GBM_FORMAT_XRGB8888) ? 4 : 2;
   b->stride = (w * b->bpp + 63) & ~63u;
   b->mem    = (uint8_t*)calloc(1, (size_t)b->stride * h);
   last_bo_made = b;
   bos++;
   return b;
}
void gbm_bo_destroy(struct gbm_bo *b) { free(b->mem); free(b->staging); free(b); bos--; }
uint32_t gbm_bo_get_stride(struct gbm_bo *b) { return b->stride; }
int gbm_bo_get_fd(struct gbm_bo *b) { (void)b; return open("/dev/null", O_RDONLY); }
void *gbm_bo_map(struct gbm_bo *b, uint32_t x, uint32_t y, uint32_t w,
      uint32_t h, uint32_t flags, uint32_t *stride, void **map_data)
{
   (void)x; (void)y; (void)w; (void)h; (void)flags;
   *stride   = b->stride;
   *map_data = b;
   b->mapped = true;
   maps++;
   if (staging_maps)
   {
      b->staging = (uint8_t*)calloc(1, (size_t)b->stride * b->h);
      return b->staging;
   }
   return b->mem;
}
void gbm_bo_unmap(struct gbm_bo *b, void *map_data)
{
   (void)map_data;
   b->mapped = false;
   maps--;
}

/* ---- EGL --------------------------------------------------------------- */

typedef struct { struct gbm_bo *bo; } fake_image_t;
typedef struct { unsigned seq; } fake_sync_t;

static int images, syncs, image_binds;
static unsigned gpu_seq, gpu_done;
static int has_import = 1;

EGLDisplay eglGetCurrentDisplay(void) { return (EGLDisplay)1; }
const char *eglQueryString(EGLDisplay d, EGLint n)
{
   (void)d; (void)n;
   return has_import ? "EGL_KHR_image_base EGL_EXT_image_dma_buf_import "
      "EGL_KHR_fence_sync" : "EGL_KHR_image_base EGL_KHR_fence_sync";
}

static EGLImageKHR fake_create_image(EGLDisplay d, EGLContext c, EGLenum t,
      EGLClientBuffer b, const EGLint *attr)
{
   fake_image_t *img = (fake_image_t*)calloc(1, sizeof(*img));
   (void)d; (void)c; (void)t; (void)b; (void)attr;
   img->bo = last_bo_made;
   images++;
   return (EGLImageKHR)img;
}
static EGLBoolean fake_destroy_image(EGLDisplay d, EGLImageKHR i)
{ (void)d; free(i); images--; return EGL_TRUE; }
static EGLSyncKHR fake_create_sync(EGLDisplay d, EGLenum t, const EGLint *a)
{
   fake_sync_t *s = (fake_sync_t*)calloc(1, sizeof(*s));
   (void)d; (void)t; (void)a;
   s->seq = ++gpu_seq;
   syncs++;
   return (EGLSyncKHR)s;
}
static EGLBoolean fake_destroy_sync(EGLDisplay d, EGLSyncKHR s)
{ (void)d; free(s); syncs--; return EGL_TRUE; }
static EGLint fake_client_wait(EGLDisplay d, EGLSyncKHR s, EGLint f, EGLTimeKHR t)
{
   (void)d; (void)f; (void)t;
   return ((fake_sync_t*)s)->seq <= gpu_done
      ? EGL_CONDITION_SATISFIED_KHR : EGL_TIMEOUT_EXPIRED_KHR;
}

/* ---- GL: textures, one framebuffer, a sampler -------------------------- */

#define MAX_TEX 64
static fake_image_t *tex_image[MAX_TEX];
static GLuint next_tex = 1, bound_tex, bound_fb, fb_color, program, array_buf;
static GLenum active_unit = GL_TEXTURE0;
static GLint viewport[4] = { 1, 2, 3, 4 };
static int attr0_enabled;
static GLboolean enabled_blend = GL_TRUE, enabled_scissor = GL_TRUE;
static uint8_t target_px[8 * 8 * 4];
static GLfloat scale_u, scale_v;

static void fake_image_target(GLenum target, void *image)
{
   (void)target;
   tex_image[bound_tex] = (fake_image_t*)image;
   image_binds++;
}

void (*eglGetProcAddress(const char *name))(void)
{
   if (!strcmp(name, "eglCreateImageKHR"))  return (void(*)(void))fake_create_image;
   if (!strcmp(name, "eglDestroyImageKHR")) return (void(*)(void))fake_destroy_image;
   if (!strcmp(name, "eglCreateSyncKHR"))   return (void(*)(void))fake_create_sync;
   if (!strcmp(name, "eglDestroySyncKHR"))  return (void(*)(void))fake_destroy_sync;
   if (!strcmp(name, "eglClientWaitSyncKHR")) return (void(*)(void))fake_client_wait;
   if (!strcmp(name, "glEGLImageTargetTexture2DOES")) return (void(*)(void))fake_image_target;
   return NULL;
}

void glGetIntegerv(GLenum p, GLint *v)
{
   switch (p)
   {
      case GL_CURRENT_PROGRAM:       *v = (GLint)program; break;
      case GL_FRAMEBUFFER_BINDING:   *v = (GLint)bound_fb; break;
      case GL_TEXTURE_BINDING_2D:    *v = (GLint)bound_tex; break;
      case GL_ARRAY_BUFFER_BINDING:  *v = (GLint)array_buf; break;
      case GL_ACTIVE_TEXTURE:        *v = (GLint)active_unit; break;
      case GL_VIEWPORT: memcpy(v, viewport, sizeof(viewport)); break;
      default: *v = 0;
   }
}
void glGetVertexAttribiv(GLuint i, GLenum p, GLint *v) { (void)i; (void)p; *v = attr0_enabled; }
GLboolean glIsEnabled(GLenum c)
{ return c == GL_BLEND ? enabled_blend : c == GL_SCISSOR_TEST ? enabled_scissor : GL_FALSE; }
void glEnable(GLenum c)
{ if (c == GL_BLEND) enabled_blend = GL_TRUE; if (c == GL_SCISSOR_TEST) enabled_scissor = GL_TRUE; }
void glDisable(GLenum c)
{ if (c == GL_BLEND) enabled_blend = GL_FALSE; if (c == GL_SCISSOR_TEST) enabled_scissor = GL_FALSE; }
void glActiveTexture(GLenum u) { active_unit = u; }
GLuint glCreateShader(GLenum t) { (void)t; return 7; }
void glShaderSource(GLuint s, GLsizei n, const GLchar *const *src, const GLint *l)
{ (void)s; (void)n; (void)src; (void)l; }
void glCompileShader(GLuint s) { (void)s; }
void glGetShaderiv(GLuint s, GLenum p, GLint *v) { (void)s; (void)p; *v = 1; }
void glDeleteShader(GLuint s) { (void)s; }
GLuint glCreateProgram(void) { return 9; }
void glAttachShader(GLuint p, GLuint s) { (void)p; (void)s; }
void glBindAttribLocation(GLuint p, GLuint i, const GLchar *n) { (void)p; (void)i; (void)n; }
void glLinkProgram(GLuint p) { (void)p; }
void glGetProgramiv(GLuint p, GLenum e, GLint *v) { (void)p; (void)e; *v = 1; }
void glDeleteProgram(GLuint p) { (void)p; }
void glUseProgram(GLuint p) { program = p; }
GLint glGetUniformLocation(GLuint p, const GLchar *n) { (void)p; return n[0] == 's' ? 2 : 1; }
void glUniform1i(GLint l, GLint v) { (void)l; (void)v; }
void glUniform2f(GLint l, GLfloat a, GLfloat b) { (void)l; scale_u = a; scale_v = b; }
void glGenTextures(GLsizei n, GLuint *t) { int i; for (i = 0; i < n; i++) t[i] = next_tex++; }
void glDeleteTextures(GLsizei n, const GLuint *t)
{ int i; for (i = 0; i < n; i++) if (t[i] < MAX_TEX) tex_image[t[i]] = NULL; }
void glBindTexture(GLenum t, GLuint id) { (void)t; bound_tex = id; }
void glTexImage2D(GLenum t, GLint l, GLint i, GLsizei w, GLsizei h, GLint b,
      GLenum f, GLenum ty, const void *p)
{ (void)t; (void)l; (void)i; (void)w; (void)h; (void)b; (void)f; (void)ty; (void)p; }
void glTexParameteri(GLenum t, GLenum p, GLint v) { (void)t; (void)p; (void)v; }
void glGenFramebuffers(GLsizei n, GLuint *f) { (void)n; *f = 40; }
void glDeleteFramebuffers(GLsizei n, const GLuint *f) { (void)n; (void)f; }
void glBindFramebuffer(GLenum t, GLuint f) { (void)t; bound_fb = f; }
void glFramebufferTexture2D(GLenum t, GLenum a, GLenum tt, GLuint tex, GLint l)
{ (void)t; (void)a; (void)tt; (void)l; fb_color = tex; }
GLenum glCheckFramebufferStatus(GLenum t) { (void)t; return GL_FRAMEBUFFER_COMPLETE; }
void glBindBuffer(GLenum t, GLuint b) { if (t == GL_ARRAY_BUFFER) array_buf = b; }
void glVertexAttribPointer(GLuint i, GLint s, GLenum t, GLboolean n, GLsizei st, const void *p)
{ (void)i; (void)s; (void)t; (void)n; (void)st; (void)p; }
void glEnableVertexAttribArray(GLuint i) { (void)i; attr0_enabled = 1; }
void glDisableVertexAttribArray(GLuint i) { (void)i; attr0_enabled = 0; }
void glViewport(GLint x, GLint y, GLsizei w, GLsizei h)
{ viewport[0] = x; viewport[1] = y; viewport[2] = w; viewport[3] = h; }
void glPixelStorei(GLenum p, GLint v) { (void)p; (void)v; }

/* Samples texel (x, y) of the image on the bound texture into the 8x8
 * target, widening 565 by rounding as some GPUs do */
void glDrawArrays(GLenum m, GLint f, GLsizei c)
{
   unsigned x, y;
   fake_image_t *img = tex_image[bound_tex];
   (void)m; (void)f; (void)c;
   if (!img || bound_fb != 40)
      return;
   for (y = 0; y < 8; y++)
      for (x = 0; x < 8; x++)
      {
         const uint8_t *row = img->bo->mem + (size_t)y * img->bo->stride;
         uint8_t *o         = target_px + (y * 8 + x) * 4;
         if (img->bo->bpp == 4)
         {
            uint32_t p = ((const uint32_t*)row)[x];
            o[0] = (uint8_t)(p >> 16); o[1] = (uint8_t)(p >> 8); o[2] = (uint8_t)p;
         }
         else
         {
            uint16_t p = ((const uint16_t*)row)[x];
            o[0] = (uint8_t)(((p >> 11) & 0x1f) * 255 / 31.0 + 0.5);
            o[1] = (uint8_t)(((p >> 5) & 0x3f) * 255 / 63.0 + 0.5);
            o[2] = (uint8_t)((p & 0x1f) * 255 / 31.0 + 0.5);
         }
         o[3] = 0xff;
      }
}
void glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum t, void *p)
{ (void)x; (void)y; (void)w; (void)h; (void)f; (void)t; memcpy(p, target_px, sizeof(target_px)); }

/* ---- the test ---------------------------------------------------------- */

static int open_fds(void)
{
   int fd, n = 0;
   for (fd = 0; fd < 256; fd++)
      if (fcntl(fd, F_GETFD) != -1)
         n++;
   return n;
}

int main(void)
{
   egl_gbm_frames_t *f;
   void *p[8];
   size_t pitch = 0;
   unsigned i, before = 0;
   int fds = open_fds();

   setvbuf(stdout, NULL, _IONBF, 0);
   bound_tex = 3;
   bound_fb  = 5;
   program   = 6;

   /* A GPU that samples the buffer itself: RGB565, widened by rounding */
   f = egl_gbm_frames_new(10, 512, 256, 2, 4);
   CHECK(f != NULL, "a buffer the GPU samples as written is taken");
   CHECK(program == 6 && bound_fb == 5 && bound_tex == 3
         && active_unit == GL_TEXTURE0 && array_buf == 0
         && viewport[0] == 1 && viewport[3] == 4 && !attr0_enabled
         && enabled_blend && enabled_scissor,
         "the check left the GL state as it found it");
   CHECK(scale_u == 8.0f / 512 && scale_v == 8.0f / 256,
         "the check samples texel for texel");
   if (failures == before)
      printf("   ok   a format the GPU samples as written is taken, the GL state kept\n");
   before = failures;

   if (f)
   {
      CHECK(egl_gbm_frames_fits(f, 512, 256, 2, 4)
            && !egl_gbm_frames_fits(f, 512, 256, 2, 5)
            && !egl_gbm_frames_fits(f, 256, 256, 2, 4),
            "fits only what it was made for");

      /* Lent in turn, at the buffer's stride */
      p[0] = egl_gbm_frames_lend(f, 400, 200, &pitch);
      CHECK(p[0] && pitch == 1024, "lent at the buffer's stride (%u)", (unsigned)pitch);
      bound_tex = 21;
      CHECK(egl_gbm_frames_attach(f, p[0], 0) && tex_image[21],
            "a lent frame becomes the texture's storage");
      CHECK(!egl_gbm_frames_attach(f, target_px, 0), "any other frame is not a buffer");
      i = image_binds;
      CHECK(egl_gbm_frames_attach(f, p[0], 0) && image_binds == (int)i,
            "the same frame again binds nothing");

      /* The texture moves on: the buffer it held waits for the GPU */
      p[1] = egl_gbm_frames_lend(f, 400, 200, &pitch);
      CHECK(p[1] && p[1] != p[0], "the held buffer is not lent");
      egl_gbm_frames_attach(f, p[1], 0);
      /* A lent buffer never submitted is free again; the one a draw may
       * still read and the last frame's are not, however often asked */
      {
         bool reused = false;
         for (i = 0; i < 12; i++)
         {
            p[2] = egl_gbm_frames_lend(f, 400, 200, &pitch);
            if (!p[2] || p[2] == p[0] || p[2] == p[1])
               reused = true;
         }
         CHECK(!reused, "a buffer a draw may read, or the last frame's, was lent");
      }
      gpu_done = gpu_seq;
      for (i = 0; i < 4 && (p[4] = egl_gbm_frames_lend(f, 400, 200, &pitch)) != p[0]; i++);
      CHECK(p[4] == p[0], "the GPU done with it, it is lent again");
      CHECK(egl_gbm_frames_lend(f, 400, 200, &pitch) != p[1], "the last frame's still is not");
      p[2] = egl_gbm_frames_lend(f, 400, 200, &pitch);

      /* Frame history: two texture slots hold one buffer each */
      egl_gbm_frames_attach(f, p[2], 1);
      CHECK(egl_gbm_frames_detach(f, 0), "the slot held a buffer");
      CHECK(!egl_gbm_frames_detach(f, 0), "and holds none once let go");
      CHECK(egl_gbm_frames_detach(f, 1), "the other slot let go too");
      /* A smaller frame: what the larger one left is blanked */
      {
         unsigned x, y, stale = 0;
         uint8_t *m;
         gpu_done = gpu_seq;
         for (i = 0; i < 8; i++)
         {
            m = (uint8_t*)egl_gbm_frames_lend(f, 400, 200, &pitch);
            if (m)
               for (y = 0; y < 200; y++)
                  memset(m + y * pitch, 0x7f, 400 * 2);
         }
         for (i = 0; i < 4; i++)
         {
            m = (uint8_t*)egl_gbm_frames_lend(f, 300, 100, &pitch);
            if (!m)
               continue;
            for (y = 0; y < 220; y++)
               for (x = 0; x < 420; x++)
               {
                  uint8_t v = m[y * pitch + x * 2];
                  bool in   = x < 300 && y < 100;
                  if (!in && v)
                     stale++;
                  if (in && v != 0x7f)
                     stale++;
               }
         }
         CHECK(!stale, "%u stale or lost bytes around a smaller frame", stale);
         CHECK(!egl_gbm_frames_lend(f, 513, 10, &pitch)
               && !egl_gbm_frames_lend(f, 10, 257, &pitch)
               && !egl_gbm_frames_lend(f, 0, 10, &pitch),
               "nothing is lent for a frame larger than the buffers");
      }

      if (failures == before)
         printf("   ok   held, in-flight and last buffers are never lent; margins blanked\n");
      before = failures;

      egl_gbm_frames_free(&f);
      CHECK(!f && !bos && !maps && !images && !syncs && !devices,
            "after the free: %d buffers, %d maps, %d images, %d fences, %d devices",
            bos, maps, images, syncs, devices);
      CHECK(open_fds() == fds, "an fd outlived the import");
      if (failures == before)
         printf("   ok   the free leaves nothing\n");
      before = failures;
   }

   /* A driver that maps through staging: the GPU never sees the write */
   staging_maps = 1;
   f = egl_gbm_frames_new(10, 512, 256, 4, 4);
   CHECK(!f, "a staging map is refused");
   CHECK(!bos && !maps && !images && !syncs && !devices && open_fds() == fds,
         "the refusal leaves nothing");
   staging_maps = 0;
   i = devices_made;
   f = egl_gbm_frames_new(10, 512, 256, 4, 4);
   CHECK(!f && devices_made == (int)i, "refused once, never tried again");
   if (failures == before)
      printf("   ok   a GPU that does not sample the write is refused, for good\n");
   before = failures;

   /* No dma-buf import: nothing made, the format not condemned */
   has_import = 0;
   i = devices_made;
   CHECK(!egl_gbm_frames_new(10, 512, 256, 2, 4) && devices_made == (int)i,
         "without the import, nothing is made");
   has_import = 1;
   CHECK(!egl_gbm_frames_new(-1, 512, 256, 2, 4)
         && !egl_gbm_frames_new(10, 512, 256, 3, 4)
         && !egl_gbm_frames_new(10, 512, 256, 2, 2)
         && !egl_gbm_frames_new(10, 512, 256, 2, EGL_GBM_FRAMES_MAX + 1),
         "bad arguments are refused");
   f = egl_gbm_frames_new(10, 64, 64, 2, 3);
   CHECK(f != NULL, "a verified format is made again without a check");
   egl_gbm_frames_free(&f);
   if (failures == before)
      printf("   ok   missing extensions and bad arguments make nothing\n");

   if (failures)
   {
      printf("FAIL egl_gbm_frames_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("PASS egl_gbm_frames_test\n");
   return 0;
}
