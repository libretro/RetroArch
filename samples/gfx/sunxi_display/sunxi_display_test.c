/* The sunxi driver against a fake display engine and framebuffer.
 *
 * /dev/disp and /dev/fb0 are faked through --wrap: the layer's
 * buffer, source and screen windows are recorded as the kernel would
 * keep them, the framebuffer is plain memory the size the kernel gives,
 * and a vsync comes when the test lets it, or at once. The NEON blits
 * are stood in for by C.
 *
 * Checked: hardware that is not sunxi, or a layer with no scaler, fails
 * init cleanly; the pages sit past the console and are XRGB8888 for a
 * 16-bit console too; a page on the screen is never drawn into, however
 * many vsyncs pass between frames; a pitch change lands straight; a
 * frame with no room is not drawn past the framebuffer; the frame shows
 * where the frontend lays it out, cut where it leaves the screen; a
 * rotation turns it; without vsync nothing waits; a framebuffer that
 * cannot wait for a vsync does not spin; the menu shows before any
 * core, past the pages; an XRGB8888 core renders into the page itself;
 * screenshots, messages, and the console put back. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <sched.h>
#include <pthread.h>
#include <errno.h>

#include "../../../gfx/drivers/sunxi_gfx.c"

/* ---- frontend pieces the driver reaches ------------------------------ */

void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
static unsigned errors_logged;
void RARCH_ERR(const char *fmt, ...) { (void)fmt; errors_logged++; }
void video_driver_set_output_dims(unsigned dims) { (void)dims; }
static unsigned rotation_snap;
unsigned video_driver_get_rotation_snapshot(void) { return rotation_snap; }
static settings_t settings;
settings_t *config_get_ptr(void) { return &settings; }

static unsigned lay_pos, lay_dims;
static int      lay_keep_aspect = -1;
void video_driver_update_viewport(struct video_viewport *vp,
      bool force_full, bool keep_aspect, bool y_down)
{
   (void)force_full; (void)y_down;
   lay_keep_aspect = keep_aspect ? 1 : 0;
   if (lay_dims)
   {
      vp->pos  = lay_pos;
      vp->dims = lay_dims;
   }
}

/* A font of one glyph, 'A': a solid 3x5 block, lines 10 high */
static uint8_t           font_cov[16 * 16];
static struct font_atlas font_atl;
static struct font_glyph font_a;
static unsigned          font_made, font_freed, font_asked;
static void *fake_font_init(uint8_t *d, size_t l, unsigned f, float sz,
      enum font_atlas_format fmt)
{ (void)d; (void)l; (void)f; (void)sz; (void)fmt; return &font_atl; }
static struct font_atlas *fake_font_atlas(void *d) { (void)d; return &font_atl; }
static const struct font_glyph *fake_font_glyph(void *d, uint32_t c)
{ (void)d; return c == 'A' ? &font_a : NULL; }
static void fake_font_free(void *d) { (void)d; font_freed++; }
static void fake_font_metrics(void *d, struct font_line_metrics **m)
{
   static struct font_line_metrics lm = { 10.0f, 8.0f, 2.0f };
   (void)d;
   *m = &lm;
}
static font_renderer_driver_t fake_font = { fake_font_init,
   fake_font_atlas, fake_font_glyph, fake_font_free, NULL, "fake",
   fake_font_metrics, false };
int font_renderer_create_default(const font_renderer_driver_t **drv,
      void **handle, const char *path, unsigned size,
      enum font_atlas_format fmt)
{
   unsigned y, x;
   (void)path; (void)fmt;
   font_made++;
   font_asked = size;
   memset(font_cov, 0, sizeof(font_cov));
   for (y = 0; y < 5; y++)
      for (x = 0; x < 3; x++)
         font_cov[y * 16 + x] = 255;
   font_atl.buffer  = font_cov;
   font_atl.width   = 16;
   font_atl.height  = 16;
   font_a.dims      = VIDEO_SCALE_PACK(3, 5);
   font_a.atlas_pos = 0;
   font_a.advance_x = 4;
   *drv             = &fake_font;
   *handle          = &font_atl;
   return 1;
}

/* ---- the NEON blits, in C -------------------------------------------- */

static unsigned blits;
void pixman_composite_src_0565_8888_asm_neon(int width, int height,
      uint32_t *dst, int dst_stride, uint16_t *src, int src_stride)
{
   int x, y;
   blits++;
   for (y = 0; y < height; y++)
      for (x = 0; x < width; x++)
      {
         uint32_t p = src[(size_t)y * src_stride + x];
         uint32_t r = (p >> 11) & 0x1f, g = (p >> 5) & 0x3f, b = p & 0x1f;
         dst[(size_t)y * dst_stride + x] = 0xff000000u
            | (((r << 3) | (r >> 2)) << 16) | (((g << 2) | (g >> 4)) << 8)
            | ((b << 3) | (b >> 2));
      }
}
void pixman_composite_src_8888_8888_asm_neon(int width, int height,
      uint32_t *dst, int dst_stride, uint16_t *src, int src_stride)
{
   int y;
   blits++;
   for (y = 0; y < height; y++)
      memcpy(dst + (size_t)y * dst_stride,
            (const uint32_t*)src + (size_t)y * src_stride,
            (size_t)width * 4);
}

/* ---- the fake kernel ------------------------------------------------- */

#define FD_DISP 900
#define FD_FB0  901
#define PADDR   0x40000000u
#define XRES    1280
#define YRES    720

static int      sunxi_present = 1, scaler_free = 1, console_bpp = 32;
static uint32_t smem_len      = XRES * YRES * 4 * 3;
static uint8_t *fbmem;
static int      fds_open, layer_open, layer_mode, mapped;
static __disp_fb_t   set_fb;
static __disp_rect_t set_src, set_scn;
static unsigned      set_fbs, set_scns;
static int           system_calls;

/* vsync: at once, held for the test to let through, or failing */
enum { VSYNC_AUTO, VSYNC_HELD, VSYNC_FAIL };
static int vsync_mode = VSYNC_AUTO;
static int vsync_tickets;
static int vsync_waits;
static pthread_mutex_t vs_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  vs_cond = PTHREAD_COND_INITIALIZER;

int __real_open(const char *path, int flags, ...);
int __wrap_open(const char *path, int flags, ...)
{
   (void)flags;
   if (!strcmp(path, "/dev/disp"))
   {
      if (!sunxi_present)
      {
         errno = ENOENT;
         return -1;
      }
      fds_open++;
      return FD_DISP;
   }
   if (!strcmp(path, "/dev/fb0"))
   {
      fds_open++;
      return FD_FB0;
   }
   errno = ENOENT;
   return -1;
}
int __real_close(int fd);
int __wrap_close(int fd)
{
   if (fd == FD_DISP || fd == FD_FB0)
   {
      fds_open--;
      return 0;
   }
   return __real_close(fd);
}
void *__wrap_mmap(void *a, size_t len, int p, int f, int fd, off_t o)
{
   (void)a; (void)p; (void)f; (void)o;
   if (fd != FD_FB0 || len != smem_len)
      return MAP_FAILED;
   mapped++;
   return fbmem;
}
int __wrap_munmap(void *a, size_t len)
{
   (void)a; (void)len;
   mapped--;
   return 0;
}
int __wrap_system(const char *cmd) { (void)cmd; system_calls++; return 0; }

int __wrap_ioctl(int fd, unsigned long req, ...)
{
   va_list ap;
   void *arg;
   unsigned long *tmp;
   va_start(ap, req);
   arg = va_arg(ap, void*);
   va_end(ap);
   tmp = (unsigned long*)arg;

   if (fd == FD_FB0)
   {
      if (req == FBIOGET_VSCREENINFO)
      {
         struct fb_var_screeninfo *v = (struct fb_var_screeninfo*)arg;
         memset(v, 0, sizeof(*v));
         v->xres = XRES;
         v->yres = YRES;
         v->bits_per_pixel = (unsigned)console_bpp;
         v->pixclock = 13468;
         return 0;
      }
      if (req == FBIOGET_FSCREENINFO)
      {
         struct fb_fix_screeninfo *f = (struct fb_fix_screeninfo*)arg;
         memset(f, 0, sizeof(*f));
         f->smem_start  = PADDR;
         f->smem_len    = smem_len;
         f->line_length = XRES * (unsigned)console_bpp / 8;
         return 0;
      }
      if (req == FBIOGET_LAYER_HDL_0)
      {
         *(int*)arg = 0;
         return 0;
      }
      if (req == FBIO_WAITFORVSYNC)
      {
         int ret = 0, yield;
         if (!arg)
            return -1;
         pthread_mutex_lock(&vs_lock);
         vsync_waits++;
         if (vsync_mode == VSYNC_FAIL)
            ret = -1;
         else if (vsync_mode == VSYNC_HELD)
         {
            while (vsync_mode == VSYNC_HELD && !vsync_tickets)
               pthread_cond_wait(&vs_cond, &vs_lock);
            if (vsync_tickets)
               vsync_tickets--;
         }
         yield = vsync_mode == VSYNC_AUTO;
         pthread_mutex_unlock(&vs_lock);
         if (yield)
            sched_yield();
         return ret;
      }
      return -1;
   }
   if (fd != FD_DISP)
      return -1;
   switch (req)
   {
      case DISP_CMD_VERSION:
         return 0;
      case DISP_CMD_LAYER_REQUEST:
         return 1;
      case DISP_CMD_LAYER_RELEASE:
         layer_open = 0;
         return 0;
      case DISP_CMD_LAYER_GET_PARA:
         memset((void*)(uintptr_t)tmp[2], 0, sizeof(__disp_layer_info_t));
         ((__disp_layer_info_t*)(uintptr_t)tmp[2])->mode = (unsigned)layer_mode;
         return 0;
      case DISP_CMD_LAYER_SET_PARA:
      {
         unsigned mode = ((__disp_layer_info_t*)(uintptr_t)tmp[2])->mode;
         if (mode == DISP_LAYER_WORK_MODE_SCALER && !scaler_free)
            return -1;
         layer_mode = (int)mode;
         return 0;
      }
      case DISP_CMD_LAYER_SET_FB:
         set_fb = *(__disp_fb_t*)(uintptr_t)tmp[2];
         set_fbs++;
         return 0;
      case DISP_CMD_LAYER_SET_SRC_WINDOW:
         set_src = *(__disp_rect_t*)(uintptr_t)tmp[2];
         return 0;
      case DISP_CMD_LAYER_SET_SCN_WINDOW:
         set_scn = *(__disp_rect_t*)(uintptr_t)tmp[2];
         set_scns++;
         return 0;
      case DISP_CMD_LAYER_OPEN:
         layer_open = 1;
         return 0;
      default:
         break;
   }
   return -1;
}

/* A 32-bit userland built with 64-bit file offsets and time reaches
 * the same calls by these names */
int __wrap_open64(const char *path, int flags, ...)
{ return __wrap_open(path, flags); }
void *__wrap_mmap64(void *a, size_t len, int p, int f, int fd, off_t o)
{ return __wrap_mmap(a, len, p, f, fd, o); }
int __wrap___ioctl_time64(int fd, unsigned long req, ...)
{
   va_list ap;
   void *arg;
   va_start(ap, req);
   arg = va_arg(ap, void*);
   va_end(ap);
   return __wrap_ioctl(fd, req, arg);
}

/* Lets one vsync through, and waits until the driver has seen it */
static void vsync(struct sunxi_video *v)
{
   int before = retro_atomic_load_acquire_int(&v->vsync_count);
   pthread_mutex_lock(&vs_lock);
   vsync_tickets++;
   pthread_cond_broadcast(&vs_cond);
   pthread_mutex_unlock(&vs_lock);
   while (retro_atomic_load_acquire_int(&v->vsync_count) == before)
      sched_yield();
}

static void set_vsync_mode(int mode)
{
   pthread_mutex_lock(&vs_lock);
   vsync_mode = mode;
   pthread_cond_broadcast(&vs_cond);
   pthread_mutex_unlock(&vs_lock);
}

static void vsync_release_all(void)
{
   pthread_mutex_lock(&vs_lock);
   vsync_mode = VSYNC_AUTO;
   pthread_cond_broadcast(&vs_cond);
   pthread_mutex_unlock(&vs_lock);
}

/* ---- the tests ------------------------------------------------------- */

static int failures;
static void check(int line, int ok, const char *fmt, ...)
{
   va_list ap;
   if (ok)
      return;
   failures++;
   printf("FAIL line %d: ", line);
   va_start(ap, fmt);
   vprintf(fmt, ap);
   va_end(ap);
   printf("\n");
}

static struct sunxi_video *try_driver(bool rgb32)
{
   video_info_t info;
   memset(&info, 0, sizeof(info));
   info.vsync        = true;
   info.rgb32        = rgb32;
   info.force_aspect = true;
   info.font_enable  = true;
   return (struct sunxi_video*)sunxi_init(&info);
}

/* A driver the fake hardware has to give: none ends the run */
static struct sunxi_video *open_driver(bool rgb32)
{
   struct sunxi_video *v = try_driver(rgb32);
   if (!v)
   {
      printf("FAIL: no driver on the fake hardware\n");
      exit(1);
   }
   return v;
}

static void frame_info(video_frame_info_t *fi)
{
   memset(fi, 0, sizeof(*fi));
}

static void push(struct sunxi_video *v, const void *f, unsigned w,
      unsigned h, unsigned pitch)
{
   video_frame_info_t fi;
   frame_info(&fi);
   sunxi_frame(v, f, VIDEO_SCALE_PACK(w, h), 0, pitch, NULL, &fi);
}

/* A @w x @h RGB565 frame, rows @pitch bytes apart: pixel y * 64 + x */
static uint16_t *make565(unsigned w, unsigned h, unsigned pitch)
{
   unsigned x, y;
   uint16_t *f = (uint16_t*)calloc(1, (size_t)pitch * h);
   for (y = 0; y < h; y++)
      for (x = 0; x < w; x++)
         f[(size_t)y * (pitch / 2) + x] = (uint16_t)(y * 64 + x);
   return f;
}

static uint32_t expand565(uint32_t p)
{
   uint32_t r = (p >> 11) & 0x1f, g = (p >> 5) & 0x3f, b = p & 0x1f;
   return 0xff000000u | (((r << 3) | (r >> 2)) << 16)
      | (((g << 2) | (g >> 4)) << 8) | ((b << 3) | (b >> 2));
}

/* The pixel the layer shows at @x, @y of its buffer */
static uint32_t shown_px(unsigned x, unsigned y)
{
   const uint32_t *b = (const uint32_t*)(fbmem + (set_fb.addr[0] - PADDR));
   return b[(size_t)(y + (unsigned)set_src.y) * set_fb.size.width
      + x + (unsigned)set_src.x];
}

static void test_init_failures(void)
{
   struct sunxi_video *v;
   sunxi_present = 0;
   v = try_driver(false);
   check(__LINE__, v == NULL, "init on hardware not sunxi");
   if (v)
      sunxi_free(v);
   check(__LINE__, fds_open == 0 && mapped == 0, "init left %d fds", fds_open);
   sunxi_present = 1;

   scaler_free = 0;
   v = try_driver(false);
   check(__LINE__, v == NULL, "init without a scaler");
   if (v)
      sunxi_free(v);
   check(__LINE__, fds_open == 0 && mapped == 0 && errors_logged,
         "no scaler: %d fds left, error logged %u", fds_open, errors_logged);
   scaler_free = 1;
}

static void test_pages(void)
{
   struct sunxi_video *v;
   uint16_t *f = make565(256, 224, 512);
   uint32_t first;
   unsigned x, y, wrong = 0;

   /* A 16-bit console: the pages past it, the layer XRGB8888 still */
   console_bpp = 16;
   memset(fbmem, 0x5a, smem_len);
   v = open_driver(false);
   check(__LINE__, v && system_calls, "init");
   check(__LINE__, fbmem[0] == 0, "console not blanked");
   push(v, f, 256, 224, 512);
   check(__LINE__, set_fb.format == DISP_FORMAT_ARGB8888
         && set_fb.addr[0] >= PADDR + XRES * YRES * 2
         && layer_open && layer_mode == DISP_LAYER_WORK_MODE_SCALER,
         "layer %x at %x, open %d", set_fb.format, set_fb.addr[0],
         layer_open);
   for (y = 0; y < 224; y += 7)
      for (x = 0; x < 256; x += 5)
         if (shown_px(x, y) != expand565(y * 64 + x))
            wrong++;
   check(__LINE__, !wrong, "%u pixels wrong", wrong);
   check(__LINE__, set_src.width == 256 && set_src.height == 224
         && set_scn.width == XRES && set_scn.height == YRES,
         "src %ux%u scn %ux%u", set_src.width, set_src.height,
         set_scn.width, set_scn.height);
   sunxi_free(v);
   check(__LINE__, fbmem[0] == 0x5a && fds_open == 0 && mapped == 0,
         "console not put back, or fds left");
   console_bpp = 32;

   /* Many vsyncs between frames: the next frame never goes to the page
    * on the screen */
   v = open_driver(false);
   set_vsync_mode(VSYNC_HELD);
   push(v, f, 256, 224, 512);
   first = set_fb.addr[0];
   vsync(v);
   vsync(v);
   vsync(v);
   push(v, f, 256, 224, 512);
   check(__LINE__, set_fb.addr[0] != first,
         "frame drawn into the page on the screen");
   vsync(v);
   vsync(v);
   push(v, f, 256, 224, 512);
   check(__LINE__, set_fb.addr[0] == first, "pages not alternating");

   /* Without vsync: a frame meeting the flip in flight is dropped. A
    * driver that did not take it would wait for a vsync held here. */
   sunxi_set_nonblock_state(v, true, false, 0);
   if (!v->nonblock)
   {
      check(__LINE__, 0, "vsync off not taken");
      vsync(v);
      vsync_release_all();
      sunxi_free(v);
      free(f);
      return;
   }
   x = set_fbs;
   y = blits;
   push(v, f, 256, 224, 512);
   check(__LINE__, set_fbs == x && blits == y,
         "frame not dropped under a flip in flight without vsync");
   vsync(v);
   push(v, f, 256, 224, 512);
   check(__LINE__, set_fbs == x + 1, "frame after the vsync not shown");
   sunxi_set_nonblock_state(v, false, false, 1);
   vsync(v);
   vsync_release_all();
   sunxi_free(v);
   free(f);
}

static void test_pitch_and_room(void)
{
   struct sunxi_video *v = open_driver(false);
   uint16_t *a = make565(200, 100, 400);
   uint16_t *b = make565(200, 100, 512);
   unsigned x, y, wrong = 0;

   push(v, a, 200, 100, 400);
   push(v, b, 200, 100, 512);
   for (y = 0; y < 100; y += 3)
      for (x = 0; x < 200; x += 3)
         if (shown_px(x, y) != expand565(y * 64 + x))
            wrong++;
   check(__LINE__, !wrong, "pitch change: %u pixels skewed", wrong);
   free(a);
   free(b);

   /* No room for two pages: nothing drawn, nothing past the memory */
   {
      unsigned fbs = set_fbs;
      uint16_t *big = make565(1280, 1000, 2560);
      errors_logged = 0;
      push(v, big, 1280, 1000, 2560);
      push(v, big, 1280, 1000, 2560);
      check(__LINE__, set_fbs == fbs && errors_logged == 1,
            "frame with no room: %u flips, %u errors", set_fbs - fbs,
            errors_logged);
      free(big);
   }
   sunxi_free(v);
}

static void test_layout(void)
{
   struct sunxi_video *v = open_driver(false);
   uint16_t *f = make565(256, 224, 512);
   unsigned scns;
   struct video_viewport vp;

   /* Integer scale: bordered */
   lay_pos  = VIDEO_POS_PACK(256, 24);
   lay_dims = VIDEO_SCALE_PACK(768, 672);
   push(v, f, 256, 224, 512);
   check(__LINE__, set_scn.x == 256 && set_scn.y == 24 && set_scn.width == 768
         && set_scn.height == 672, "window %d,%d %ux%u", set_scn.x,
         set_scn.y, set_scn.width, set_scn.height);
   /* Unchanged: the window is not set again */
   scns = set_scns;
   push(v, f, 256, 224, 512);
   check(__LINE__, set_scns == scns, "unchanged window set again");

   /* Wider than the screen and off its left: cut, not wrapped */
   lay_pos  = VIDEO_POS_PACK(-256, 0);
   lay_dims = VIDEO_SCALE_PACK(1792, 720);
   push(v, f, 256, 224, 512);
   check(__LINE__, set_scn.x == 0 && set_scn.width == XRES
         && set_src.x == 36 && set_src.width == 184,
         "cut window %d %u, src %d %u", set_scn.x, set_scn.width,
         set_src.x, set_src.width);
   sunxi_viewport_info(v, &vp);
   check(__LINE__, VIDEO_POS_X(vp.pos) == -256
         && VIDEO_SCALE_W(vp.full_dims) == XRES, "viewport_info");

   sunxi_set_aspect_ratio(v, 0);
   push(v, f, 256, 224, 512);
   check(__LINE__, lay_keep_aspect == 1, "aspect kept after it was picked");
   lay_dims = 0;
   sunxi_free(v);
   free(f);
}

static void test_rotation(void)
{
   unsigned r;
   uint16_t *f = make565(4, 2, 8);
   static uint32_t f32[4 * 2];

   for (r = 0; r < 8; r++)
      f32[r] = 0xff000000u | r;
   for (r = 1; r < 4; r++)
   {
      struct sunxi_video *v = open_driver(false);
      sunxi_set_rotation(v, r);
      push(v, f, 4, 2, 8);
      if (r & 1)
         check(__LINE__, set_src.width == 2 && set_src.height == 4
               && shown_px(0, 0) == expand565(r == 1 ? 3 : 64)
               && shown_px(0, 3) == expand565(r == 1 ? 0 : 67),
               "565 rotation %u: %08x", r, shown_px(0, 0));
      else
         check(__LINE__, shown_px(0, 0) == expand565(67),
               "565 rotation 2: %08x", shown_px(0, 0));
      sunxi_free(v);

      v = open_driver(true);
      sunxi_set_rotation(v, r);
      push(v, f32, 4, 2, 16);
      check(__LINE__, shown_px(0, 0) == (r == 1 ? f32[3] : r == 2 ? f32[7]
               : f32[4]), "xrgb rotation %u: %08x", r, shown_px(0, 0));
      sunxi_free(v);
   }
   free(f);
}

static void test_no_vsync(void)
{
   struct sunxi_video *v;
   uint16_t *f = make565(64, 64, 128);
   int waits;

   set_vsync_mode(VSYNC_FAIL);
   v = open_driver(false);
   /* Bounded: a driver that never notices would wait on its flip */
   for (waits = 0; waits < 100000
         && !retro_atomic_load_acquire_int(&v->no_vsync); waits++)
      sched_yield();
   if (!retro_atomic_load_acquire_int(&v->no_vsync))
   {
      check(__LINE__, 0, "failed vsync not noticed");
      sunxi_free(v);
      set_vsync_mode(VSYNC_AUTO);
      free(f);
      return;
   }
   push(v, f, 64, 64, 128);
   push(v, f, 64, 64, 128);
   push(v, f, 64, 64, 128);
   check(__LINE__, retro_atomic_load_acquire_int(&v->no_vsync),
         "failed vsync not noticed");
   pthread_mutex_lock(&vs_lock);
   waits = vsync_waits;
   pthread_mutex_unlock(&vs_lock);
   push(v, f, 64, 64, 128);
   pthread_mutex_lock(&vs_lock);
   check(__LINE__, vsync_waits == waits, "the vsync thread kept trying");
   pthread_mutex_unlock(&vs_lock);
   sunxi_free(v);
   set_vsync_mode(VSYNC_AUTO);
   free(f);
}

static void test_menu(void)
{
   struct sunxi_video *v = open_driver(false);
   static uint16_t m[320 * 240];
   uint16_t *f = make565(256, 224, 512);
   unsigned i;

   for (i = 0; i < 320 * 240; i++)
      m[i] = 0xf00fu; /* red, opaque */
   /* Before any core: shown, at the end of the framebuffer */
   sunxi_set_texture_enable(v, true, true);
   sunxi_set_texture_frame(v, m, false, VIDEO_SCALE_PACK(320, 240), 1.0f);
   check(__LINE__, set_fb.addr[0] - PADDR + 320 * 4 * 240 <= smem_len
         && set_fb.addr[0] - PADDR >= XRES * YRES * 4
         && shown_px(319, 239) == 0x00ff0000u && set_scn.width == XRES,
         "menu at %x: %08x", set_fb.addr[0], shown_px(319, 239));
   sunxi_set_texture_enable(v, false, false);
   push(v, f, 256, 224, 512);
   /* With a core: past its pages */
   sunxi_set_texture_enable(v, true, false);
   sunxi_set_texture_frame(v, m, false, VIDEO_SCALE_PACK(320, 240), 1.0f);
   check(__LINE__, set_fb.addr[0] - PADDR >= v->pages[1].offset
         + v->page_stride * 4 * 224, "menu over the pages");
   push(v, f, 256, 224, 512);
   check(__LINE__, set_src.width == 320, "a frame flipped under the menu");
   /* A screenshot under the menu is of the menu: red */
   {
      uint8_t *shot = (uint8_t*)malloc((size_t)XRES * YRES * 3);
      check(__LINE__, sunxi_read_viewport(v, shot, false)
            && shot[0] == 0 && shot[1] == 0 && shot[2] == 0xff,
            "screenshot under the menu %02x %02x %02x", shot[0], shot[1],
            shot[2]);
      free(shot);
   }
   sunxi_set_texture_enable(v, false, false);

   /* Pages grown into where the menu was: it finds a place past them,
    * or is not shown, never over them */
   {
      uint16_t *big = make565(1280, 700, 2560);
      uint32_t pages_end;
      push(v, big, 1280, 700, 2560);
      pages_end = v->pages[1].offset + v->page_stride * 4 * 700;
      sunxi_set_texture_enable(v, true, false);
      sunxi_set_texture_frame(v, m, false, VIDEO_SCALE_PACK(320, 240), 1.0f);
      check(__LINE__, set_fb.addr[0] - PADDR >= pages_end
            || set_fb.addr[0] - PADDR == v->pages[v->shown].offset,
            "menu drawn over the pages at %x", set_fb.addr[0] - PADDR);
      sunxi_set_texture_enable(v, false, false);
      free(big);
   }
   sunxi_free(v);
   free(f);
}

static void test_lend_shot_text(void)
{
   struct sunxi_video *v = open_driver(true);
   static uint32_t own[64 * 32];
   struct retro_framebuffer fb;
   unsigned i, b;
   uint8_t *shot;
   video_frame_info_t fi;

   for (i = 0; i < 64 * 32; i++)
      own[i] = 0xff000000u | i;
   push(v, own, 64, 32, 256);
   memset(&fb, 0, sizeof(fb));
   fb.width  = 64;
   fb.height = 32;
   fb.access_flags = RETRO_MEMORY_ACCESS_WRITE;
   check(__LINE__, sunxi_get_current_software_framebuffer(v, &fb)
         && fb.data == v->pages[v->next].address
         && fb.pitch == v->page_stride * 4, "page not lent");
   for (i = 0; i < 32; i++)
      memcpy((uint8_t*)fb.data + i * fb.pitch, own + i * 64, 64 * 4);
   b = blits;
   sunxi_frame(v, fb.data, VIDEO_SCALE_PACK(64, 32), 0,
         (unsigned)fb.pitch, NULL, (frame_info(&fi), &fi));
   check(__LINE__, blits == b && set_fb.addr[0] - PADDR
         == v->pages[v->shown].offset, "lent page copied or not flipped");
   fb.access_flags = RETRO_MEMORY_ACCESS_READ;
   check(__LINE__, !sunxi_get_current_software_framebuffer(v, &fb),
         "framebuffer lent to a core that reads it");

   /* Screenshot at the viewport's size, BGR, bottom row first */
   shot = (uint8_t*)malloc((size_t)XRES * YRES * 3);
   check(__LINE__, sunxi_read_viewport(v, shot, false), "read_viewport");
   check(__LINE__, shot[0] == (uint8_t)(31 * 64) && shot[1] == 0x07,
         "screenshot %02x %02x", shot[0], shot[1]);
   free(shot);

   /* A message, drawn into the page at the size it shows */
   frame_info(&fi);
   fi.font_size        = 45.0f;
   fi.font_msg_pos_x   = 0.0f;
   fi.font_msg_pos_y   = 0.5f;
   fi.font_msg_color_r = 1.0f;
   sunxi_frame(v, own, VIDEO_SCALE_PACK(64, 32), 0, 256, "A", &fi);
   check(__LINE__, font_asked == 2 || font_asked == 8,
         "font at %u pixels", font_asked);
   check(__LINE__, (shown_px(0, 16) & 0xffffff) == 0xff0000,
         "message not drawn: %08x", shown_px(0, 16));
   sunxi_free(v);
   check(__LINE__, font_freed == font_made, "font left made");
}

int main(void)
{
   fbmem = (uint8_t*)malloc(smem_len);
   test_init_failures();
   test_pages();
   test_pitch_and_room();
   test_layout();
   test_rotation();
   test_no_vsync();
   test_menu();
   test_lend_shot_text();
   check(__LINE__, fds_open == 0 && mapped == 0, "fds %d maps %d left",
         fds_open, mapped);
   free(fbmem);
   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("sunxi_display: all passed\n");
   return 0;
}
