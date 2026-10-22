/* gfx/drivers/gl2.c's frames in scanout memory, against a scripted
 * egl_gbm_frames and GL: on KMS, a plain software frame is lent from the
 * buffers, a lent frame becomes the frame texture's storage with no
 * upload, any other frame gets the texture's own storage back before it
 * goes up, and what may not be lent never is. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../gfx/drivers/gl2.c"

/* The texture uploads, counted here rather than in the shared fakes */
#define glTexImage2D    gl11_unused_TexImage2D
#define glTexSubImage2D gl11_unused_TexSubImage2D
#include "../gl2_pbo_readback/gl11_fakes.h"
#undef glTexImage2D
#undef glTexSubImage2D

static unsigned tex_images, tex_uploads;
void glTexImage2D(GLenum t, GLint l, GLint i, GLsizei w, GLsizei h, GLint b,
      GLenum f, GLenum ty, const GLvoid *p)
{ (void)t; (void)l; (void)i; (void)w; (void)h; (void)b; (void)f; (void)ty; (void)p; tex_images++; }
void glTexSubImage2D(GLenum t, GLint l, GLint x, GLint y, GLsizei w, GLsizei h,
      GLenum f, GLenum ty, const GLvoid *p)
{ (void)t; (void)l; (void)x; (void)y; (void)w; (void)h; (void)f; (void)ty; (void)p; tex_uploads++; }
static void fake_bind_buffer(GLenum t, GLuint b) { (void)t; (void)b; }

/* ---- the frontend ------------------------------------------------------ */

int g_drm_fd = 42;
static enum rarch_display_type display = RARCH_DISPLAY_KMS;
enum rarch_display_type video_driver_display_type_get(void) { return display; }

/* ---- a scripted egl_gbm_frames ------------------------------------------ */

struct egl_gbm_frames { unsigned w, h, bpp, count; int held_slot; };
static struct egl_gbm_frames frames;
static uint8_t buf[2][256 * 256 * 4];
static unsigned news, frees, lends, attaches, detaches, lent_w, lent_h;
static int fail_new, new_fd;
static unsigned turn;

egl_gbm_frames_t *egl_gbm_frames_new(int fd, unsigned w, unsigned h,
      unsigned bpp, unsigned count)
{
   news++;
   new_fd = fd;
   if (fail_new)
      return NULL;
   frames.w = w; frames.h = h; frames.bpp = bpp; frames.count = count;
   frames.held_slot = -1;
   return &frames;
}
bool egl_gbm_frames_fits(const egl_gbm_frames_t *f, unsigned w, unsigned h,
      unsigned bpp, unsigned count)
{ return f && f->w == w && f->h == h && f->bpp == bpp && f->count == count; }
void *egl_gbm_frames_lend(egl_gbm_frames_t *f, unsigned w, unsigned h, size_t *pitch)
{ (void)f; lends++; lent_w = w; lent_h = h; *pitch = 1024; return buf[turn++ & 1]; }
bool egl_gbm_frames_attach(egl_gbm_frames_t *f, const void *frame, unsigned slot)
{
   if (frame != buf[0] && frame != buf[1])
      return false;
   attaches++;
   f->held_slot = (int)slot;
   return true;
}
bool egl_gbm_frames_detach(egl_gbm_frames_t *f, unsigned slot)
{
   bool held = f->held_slot == (int)slot;
   detaches++;
   f->held_slot = -1;
   return held;
}
void egl_gbm_frames_free(egl_gbm_frames_t **f)
{ frees++; *f = NULL; }

/* ---- the test ---------------------------------------------------------- */

static int fails;
#define CHECK(cond, what) do { \
   if (cond) printf("  [ok]   %s\n", what); \
   else { printf("  [FAIL] %s\n", what); fails++; } \
} while (0)

static bool lend(gl2_t *gl, unsigned access, struct retro_framebuffer *fb)
{
   memset(fb, 0, sizeof(*fb));
   fb->width        = 200;
   fb->height       = 150;
   fb->access_flags = access;
   return gl2_get_current_software_framebuffer(gl, fb);
}

int main(void)
{
   gl2_t gl;
   gl2_renderchain_data_t chain;
   struct retro_framebuffer fb;
   static uint32_t own[200 * 150];
   unsigned n;

   __rglgen_glBindBuffer = fake_bind_buffer;
   memset(&gl, 0, sizeof(gl));
   memset(&chain, 0, sizeof(chain));
   gl.renderchain_data = &chain;
   gl.tex_dims         = VIDEO_SCALE_PACK(256, 256);
   gl.base_size        = 4;
   gl.textures         = 1;
   gl.internal_fmt     = GL_RGBA;
   gl.texture_type     = GL_BGRA_EXT;
   gl.texture_fmt      = GL_UNSIGNED_INT_8_8_8_8_REV;

   printf("lending:\n");
   CHECK(lend(&gl, RETRO_MEMORY_ACCESS_WRITE, &fb) && fb.data == buf[0]
         && fb.pitch == 1024 && fb.format == RETRO_PIXEL_FORMAT_XRGB8888
         && lent_w == 200 && lent_h == 150,
         "a plain frame is lent from the buffers, at their stride, for its size");
   CHECK(news == 1 && new_fd == 42 && frames.w == 256 && frames.h == 256
         && frames.bpp == 4 && frames.count == 4,
         "made once on the card, as large as the frame texture, textures + 3");
   n = lends;
   CHECK(!lend(&gl, RETRO_MEMORY_ACCESS_WRITE | RETRO_MEMORY_ACCESS_READ, &fb)
         && lends == n, "never to a core that reads its frame back");
   gl.flags |= GL2_FLAG_HW_RENDER_USE;
   CHECK(!lend(&gl, RETRO_MEMORY_ACCESS_WRITE, &fb) && lends == n, "nor to a hardware core");
   gl.flags &= ~GL2_FLAG_HW_RENDER_USE;
   gl.flags |= GL2_FLAG_TEXTURE_MIPMAP;
   CHECK(!lend(&gl, RETRO_MEMORY_ACCESS_WRITE, &fb) && lends == n, "nor where mipmaps are made");
   gl.flags &= ~GL2_FLAG_TEXTURE_MIPMAP;
   gl.video_info.source_10bit = true;
   CHECK(!lend(&gl, RETRO_MEMORY_ACCESS_WRITE, &fb) && lends == n, "nor for a 10-bit source");
   gl.video_info.source_10bit = false;
   display = RARCH_DISPLAY_X11;
   CHECK(!lend(&gl, RETRO_MEMORY_ACCESS_WRITE, &fb) && lends == n, "nor off KMS");
   display = RARCH_DISPLAY_KMS;
   fb.width = 300;
   CHECK(!gl2_get_current_software_framebuffer(&gl, &fb) && lends == n,
         "nor past the frame texture's size");

   printf("upload:\n");
   CHECK(lend(&gl, RETRO_MEMORY_ACCESS_WRITE, &fb), "lent again");
   tex_images = tex_uploads = 0;
   gl2_renderchain_copy_frame(&gl, &chain, false, fb.data, 200, 150, 1024);
   CHECK(attaches == 1 && frames.held_slot == 0 && !tex_uploads && !tex_images,
         "a lent frame becomes the texture's storage, nothing uploaded");
   gl2_renderchain_copy_frame(&gl, &chain, false, own, 200, 150, 800);
   CHECK(tex_images == 1 && tex_uploads == 1 && frames.held_slot == -1,
         "another frame: own storage made again, then uploaded");
   gl2_renderchain_copy_frame(&gl, &chain, false, own, 200, 150, 800);
   CHECK(tex_images == 1 && tex_uploads == 2,
         "and again: just uploaded");

   printf("lifetime:\n");
   gl2_kms_frames_drop(&gl);
   CHECK(frees == 1 && !gl.kms_frames, "dropped with the textures");
   gl2_kms_frames_drop(&gl);
   CHECK(frees == 1, "dropping nothing frees nothing");
   gl.tex_dims = VIDEO_SCALE_PACK(512, 512);
   fail_new    = 1;
   n           = news;
   CHECK(!lend(&gl, RETRO_MEMORY_ACCESS_WRITE, &fb) && news == n + 1,
         "made anew at a new size; when that fails, nothing is lent");
   CHECK(!lend(&gl, RETRO_MEMORY_ACCESS_WRITE, &fb) && news == n + 1,
         "and it is not tried every frame");
   tex_uploads = 0;
   gl2_renderchain_copy_frame(&gl, &chain, false, own, 200, 150, 800);
   CHECK(tex_uploads == 1, "frames go up as before");

   if (fails)
   {
      printf("%d check(s) failed\n", fails);
      return 1;
   }
   printf("all checks passed\n");
   return 0;
}
