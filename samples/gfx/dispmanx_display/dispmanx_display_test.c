/* The dispmanx driver against a fake VideoCore.
 *
 * The firmware keeps each resource at its own pitch - the row bytes
 * rounded up - and a write sends rows of the pitch the caller gives,
 * which it takes as the resource's: a frame whose rows are some other
 * distance apart lands skewed, and the write reads past its end. The
 * fake models that, keeps every element's place, turn and blend, and
 * runs a flip's callback as its vsync comes.
 *
 * Checked: the frame shows where the frontend lays it out (aspect,
 * integer scale, custom viewport, off the screen) and moves there
 * without being made again; a core's rotation, a quarter by the CPU
 * and a half by the HVS; rows of any pitch land straight; the scaler
 * kernel follows video_smooth at init and at run time and is put back
 * on exit; the menu blends by its own alpha above the frame, where the
 * frame shows or over the screen, and stays up while the frame's
 * surface is made again. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

#include "../../../gfx/drivers/dispmanx_gfx.c"

/* ---- frontend pieces the driver reaches ------------------------------ */

void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }

/* A font of one glyph, 'A': a solid 3x5 block drawn from its top left
 * at the pen, lines 10 high */
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

static settings_t settings;
settings_t *config_get_ptr(void) { return &settings; }

void video_driver_display_type_set(enum rarch_display_type t) { (void)t; }
void video_driver_set_output_dims(unsigned dims) { (void)dims; }

/* The frontend's layout, as a test sets it: the whole screen when no
 * place is set */
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

/* ---- the fake VideoCore ---------------------------------------------- */

#define DISP_W   1920
#define DISP_H   1080
#define MAX_RES  32
#define MAX_ELEM 16

struct fres
{
   uint8_t *mem;
   unsigned w, h, bpp, pitch;
   int live;
};

struct felem
{
   VC_RECT_T dst, src;
   uint32_t res, transform, opacity, aflags;
   int32_t layer;
   int live;
};

static struct fres  res[MAX_RES];
static struct felem elem[MAX_ELEM];
static unsigned res_made, submits, attr_changes, writes, bad_pitch, overreads;
static char last_kernel[512];
static const void *last_write_src;
static unsigned kernels_sent;
/* The frame the driver is handed, for reads past its end */
static const uint8_t *frame_base;
static size_t frame_size;

static unsigned type_bpp(VC_IMAGE_TYPE_T t)
{
   if (t == VC_IMAGE_RGB888)
      return 3;
   return (t == VC_IMAGE_XRGB8888 || t == VC_IMAGE_ARGB8888
         || t == VC_IMAGE_RGBA32) ? 4 : 2;
}

void bcm_host_init(void) { }
void bcm_host_deinit(void) { }
/* Displays the board has: the default and HDMI0 at 1920x1080, HDMI1 at
 * 1280x720 when connected, nothing else */
static int hdmi1_connected;
static uint32_t display_opened, size_asked = 99;
int32_t graphics_get_display_size(const uint16_t n, uint32_t *w, uint32_t *h)
{
   size_asked = n;
   *w = n == DISPMANX_ID_HDMI1 ? 1280 : DISP_W;
   *h = n == DISPMANX_ID_HDMI1 ? 720 : DISP_H;
   return 0;
}
int vc_gencmd_send(const char *format, ...)
{
   va_list ap;
   va_start(ap, format);
   vsprintf(last_kernel, format, ap);
   va_end(ap);
   kernels_sent++;
   return 0;
}
int vc_dispmanx_rect_set(VC_RECT_T *r, uint32_t x, uint32_t y,
      uint32_t w, uint32_t h)
{
   r->x = (int32_t)x; r->y = (int32_t)y;
   r->width = (int32_t)w; r->height = (int32_t)h;
   return 0;
}
DISPMANX_DISPLAY_HANDLE_T vc_dispmanx_display_open(uint32_t d)
{
   if (     d != DISPMANX_ID_MAIN_LCD && d != DISPMANX_ID_HDMI0
         && !(d == DISPMANX_ID_HDMI1 && hdmi1_connected))
      return 0;
   display_opened = d;
   return 1;
}
int vc_dispmanx_display_close(DISPMANX_DISPLAY_HANDLE_T d)
{ (void)d; return 0; }

DISPMANX_RESOURCE_HANDLE_T vc_dispmanx_resource_create(VC_IMAGE_TYPE_T type,
      uint32_t w, uint32_t h, uint32_t *native)
{
   unsigned i;
   (void)native;
   for (i = 1; i < MAX_RES; i++)
   {
      if (res[i].live)
         continue;
      res[i].live  = 1;
      res[i].w     = w;
      res[i].h     = h;
      res[i].bpp   = type_bpp(type);
      /* The firmware's rows: 32 bytes aligned */
      res[i].pitch = (w * res[i].bpp + 31) & ~31u;
      res[i].mem   = (uint8_t*)calloc(res[i].pitch, h);
      res_made++;
      return i;
   }
   return 0;
}
/* The screen as composed: R = x, G = y, B = 0x5a, low bytes */
int vc_dispmanx_snapshot(DISPMANX_DISPLAY_HANDLE_T d,
      DISPMANX_RESOURCE_HANDLE_T r, DISPMANX_TRANSFORM_T t)
{
   unsigned x, y;
   (void)d; (void)t;
   for (y = 0; y < res[r].h; y++)
      for (x = 0; x < res[r].w; x++)
      {
         uint8_t *p = res[r].mem + (size_t)y * res[r].pitch + x * 3;
         p[0] = (uint8_t)x;
         p[1] = (uint8_t)y;
         p[2] = 0x5a;
      }
   return 0;
}
int vc_dispmanx_resource_read_data(DISPMANX_RESOURCE_HANDLE_T r,
      const VC_RECT_T *rect, void *dst, uint32_t dst_pitch)
{
   if (dst_pitch != res[r].pitch)
      bad_pitch++;
   memcpy((uint8_t*)dst + (size_t)dst_pitch * rect->y,
         res[r].mem + (size_t)res[r].pitch * rect->y,
         (size_t)dst_pitch * rect->height);
   return 0;
}
int vc_dispmanx_resource_delete(DISPMANX_RESOURCE_HANDLE_T r)
{
   free(res[r].mem);
   memset(&res[r], 0, sizeof(res[r]));
   return 0;
}
int vc_dispmanx_resource_write_data(DISPMANX_RESOURCE_HANDLE_T r,
      VC_IMAGE_TYPE_T t, int src_pitch, void *src, const VC_RECT_T *rect)
{
   size_t len;
   const uint8_t *from = (const uint8_t*)src + (size_t)src_pitch * rect->y;
   (void)t;
   writes++;
   last_write_src = src;
   if ((unsigned)src_pitch != res[r].pitch)
      bad_pitch++;
   len = (size_t)src_pitch * rect->height;
   /* A write from the frame itself stays inside it */
   if (     from >= frame_base && from < frame_base + frame_size
         && from + len > frame_base + frame_size)
   {
      overreads++;
      len = (size_t)(frame_base + frame_size - from);
   }
   if (len > (size_t)res[r].pitch * (res[r].h - rect->y))
      len = (size_t)res[r].pitch * (res[r].h - rect->y);
   memcpy(res[r].mem + (size_t)res[r].pitch * rect->y, from, len);
   return 0;
}

DISPMANX_UPDATE_HANDLE_T vc_dispmanx_update_start(int32_t p)
{ (void)p; return 1; }
DISPMANX_ELEMENT_HANDLE_T vc_dispmanx_element_add(
      DISPMANX_UPDATE_HANDLE_T u, DISPMANX_DISPLAY_HANDLE_T d,
      int32_t layer, const VC_RECT_T *dst, DISPMANX_RESOURCE_HANDLE_T src,
      const VC_RECT_T *src_rect, DISPMANX_PROTECTION_T prot,
      VC_DISPMANX_ALPHA_T *alpha, DISPMANX_CLAMP_T *clamp,
      DISPMANX_TRANSFORM_T transform)
{
   unsigned i;
   (void)u; (void)d; (void)prot; (void)clamp;
   for (i = 1; i < MAX_ELEM; i++)
   {
      if (elem[i].live)
         continue;
      elem[i].live      = 1;
      elem[i].layer     = layer;
      elem[i].dst       = *dst;
      elem[i].src       = *src_rect;
      elem[i].res       = src;
      elem[i].transform = (uint32_t)transform;
      elem[i].opacity   = alpha->opacity;
      elem[i].aflags    = (uint32_t)alpha->flags;
      return i;
   }
   return 0;
}
int vc_dispmanx_element_change_source(DISPMANX_UPDATE_HANDLE_T u,
      DISPMANX_ELEMENT_HANDLE_T e, DISPMANX_RESOURCE_HANDLE_T src)
{
   (void)u;
   elem[e].res = src;
   return 0;
}
int vc_dispmanx_element_change_attributes(DISPMANX_UPDATE_HANDLE_T u,
      DISPMANX_ELEMENT_HANDLE_T e, uint32_t flags, int32_t layer,
      uint8_t opacity, const VC_RECT_T *dst, const VC_RECT_T *src,
      DISPMANX_RESOURCE_HANDLE_T mask, DISPMANX_TRANSFORM_T transform)
{
   (void)u; (void)mask;
   attr_changes++;
   if (flags & (1 << 0)) elem[e].layer     = layer;
   if (flags & (1 << 1)) elem[e].opacity   = opacity;
   if (flags & (1 << 2)) elem[e].dst       = *dst;
   if (flags & (1 << 3)) elem[e].src       = *src;
   if (flags & (1 << 5)) elem[e].transform = (uint32_t)transform;
   return 0;
}
int vc_dispmanx_element_remove(DISPMANX_UPDATE_HANDLE_T u,
      DISPMANX_ELEMENT_HANDLE_T e)
{
   (void)u;
   memset(&elem[e], 0, sizeof(elem[e]));
   return 0;
}
/* The vsync comes at once, or when a test lets it */
static int vsync_held;
static DISPMANX_CALLBACK_FUNC_T held_cb;
static void *held_arg;
int vc_dispmanx_update_submit(DISPMANX_UPDATE_HANDLE_T u,
      DISPMANX_CALLBACK_FUNC_T cb, void *arg)
{
   submits++;
   if (cb && vsync_held)
   {
      held_cb  = cb;
      held_arg = arg;
   }
   else if (cb)
      cb(u, arg);
   return 0;
}
static void vsync(void)
{
   DISPMANX_CALLBACK_FUNC_T cb = held_cb;
   held_cb = NULL;
   if (cb)
      cb(1, held_arg);
}
int vc_dispmanx_update_submit_sync(DISPMANX_UPDATE_HANDLE_T u)
{
   (void)u;
   submits++;
   return 0;
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

static struct felem *elem_on(int32_t layer)
{
   unsigned i;
   for (i = 1; i < MAX_ELEM; i++)
      if (elem[i].live && elem[i].layer == layer)
         return &elem[i];
   return NULL;
}

/* Pixel @x,@y of what an element's resource holds, 16 or 32 bits */
static uint32_t res_px(const struct felem *e, unsigned x, unsigned y)
{
   const struct fres *r = &res[e->res];
   const uint8_t *p     = r->mem + (size_t)y * r->pitch + (size_t)x * r->bpp;
   return r->bpp == 4 ? *(const uint32_t*)p : *(const uint16_t*)p;
}

static void *open_driver(bool smooth, bool force_aspect)
{
   video_info_t info;
   memset(&info, 0, sizeof(info));
   info.vsync        = true;
   info.smooth       = smooth;
   info.force_aspect = force_aspect;
   return dispmanx_init(&info);
}

static void frame_info(video_frame_info_t *fi)
{
   memset(fi, 0, sizeof(*fi));
   fi->max_swapchain_images = 3;
}

/* A @w x @h RGB565 frame, rows @pitch bytes apart, pixel value
 * y * 256 + x, allocated to its exact size */
static uint16_t *make_frame(unsigned w, unsigned h, unsigned pitch)
{
   unsigned x, y;
   uint8_t *f = (uint8_t*)malloc((size_t)pitch * (h - 1) + w * 2);
   for (y = 0; y < h; y++)
      for (x = 0; x < w; x++)
         ((uint16_t*)(f + (size_t)y * pitch))[x] = (uint16_t)(y * 256 + x);
   return (uint16_t*)f;
}

static void push(void *d, const uint16_t *f, unsigned w, unsigned h,
      unsigned pitch)
{
   video_frame_info_t fi;
   frame_info(&fi);
   frame_base = (const uint8_t*)f;
   frame_size = (size_t)pitch * (h - 1) + w * 2;
   dispmanx_frame(d, f, VIDEO_SCALE_PACK(w, h), 0, pitch, NULL, &fi);
}

static void test_layout(void)
{
   void *d          = open_driver(true, false);
   uint16_t *f      = make_frame(256, 224, 512);
   struct felem *e;
   unsigned made, subs;

   /* Integer scale: four times, bordered, not stretched to the height */
   lay_pos  = VIDEO_POS_PACK(448, 92);
   lay_dims = VIDEO_SCALE_PACK(1024, 896);
   push(d, f, 256, 224, 512);
   e = elem_on(0);
   check(__LINE__, e && e->dst.x == 448 && e->dst.y == 92
         && e->dst.width == 1024 && e->dst.height == 896,
         "integer scale: element at %d,%d %dx%d", e ? e->dst.x : -1,
         e ? e->dst.y : -1, e ? e->dst.width : -1, e ? e->dst.height : -1);
   check(__LINE__, e && e->src.width == (256 << 16) && e->src.height == (224 << 16),
         "integer scale: whole frame shown");
   check(__LINE__, lay_keep_aspect == 0, "keep aspect follows video_force_aspect");

   /* A new aspect moves the element in the flip, nothing made again */
   made     = res_made;
   subs     = submits;
   lay_pos  = VIDEO_POS_PACK(240, 0);
   lay_dims = VIDEO_SCALE_PACK(1440, 1080);
   push(d, f, 256, 224, 512);
   e = elem_on(0);
   check(__LINE__, e && e->dst.x == 240 && e->dst.width == 1440,
         "aspect change: element at %d, %d wide", e ? e->dst.x : -1,
         e ? e->dst.width : -1);
   check(__LINE__, res_made == made, "aspect change made %u resources",
         res_made - made);
   check(__LINE__, submits == subs + 1, "aspect change took %u submits, not the flip's",
         submits - subs);

   /* A custom viewport hanging off the screen: the part shown is cut */
   lay_pos  = VIDEO_POS_PACK(-128, 0);
   lay_dims = VIDEO_SCALE_PACK(1024, 896);
   push(d, f, 256, 224, 512);
   e = elem_on(0);
   check(__LINE__, e && e->dst.x == 0 && e->dst.width == 896,
         "off screen: element at %d, %d wide", e ? e->dst.x : -1,
         e ? e->dst.width : -1);
   check(__LINE__, e && e->src.x == (32 << 16) && e->src.width == (224 << 16),
         "off screen: source from %d, %d wide (16.16)", e ? e->src.x : -1,
         e ? e->src.width : -1);

   /* The viewport the frontend reads back is the one laid out */
   {
      struct video_viewport vp;
      dispmanx_viewport_info(d, &vp);
      check(__LINE__, VIDEO_POS_X(vp.pos) == -128 && VIDEO_SCALE_W(vp.dims) == 1024
            && VIDEO_SCALE_W(vp.full_dims) == DISP_W,
            "viewport_info");
   }

   /* An aspect picked in the menu keeps the aspect from then on */
   dispmanx_set_aspect_ratio(d, 0);
   push(d, f, 256, 224, 512);
   check(__LINE__, lay_keep_aspect == 1, "set_aspect_ratio keeps the aspect");

   dispmanx_free(d);
   free(f);
   lay_dims = 0;
}

static void test_pitch(void)
{
   /* 360 pixels of RGB565 are 720 bytes, which no firmware pitch is */
   void *d     = open_driver(true, true);
   uint16_t *f = make_frame(360, 8, 720);
   struct felem *e;
   unsigned x, y, wrong = 0;

   bad_pitch = overreads = 0;
   push(d, f, 360, 8, 720);
   e = elem_on(0);
   check(__LINE__, bad_pitch == 0, "%u writes at a pitch not the resource's", bad_pitch);
   check(__LINE__, overreads == 0, "%u writes read past the frame", overreads);
   for (y = 0; e && y < 8; y++)
      for (x = 0; x < 360; x++)
         if (res_px(e, x, y) != y * 256 + x)
            wrong++;
   check(__LINE__, e && !wrong, "%u pixels landed skewed", wrong);
   dispmanx_free(d);
   free(f);

   /* Rows already aligned, padded past the width: sent as they are */
   d = open_driver(true, true);
   f = make_frame(256, 8, 2048);
   writes = 0;
   push(d, f, 256, 8, 2048);
   e = elem_on(0);
   check(__LINE__, e && res[e->res].pitch == 2048, "padded frame not sent in place");
   check(__LINE__, e && res_px(e, 255, 7) == 7 * 256 + 255, "padded frame content");
   dispmanx_free(d);
   free(f);
}

static void test_lend(void)
{
   struct retro_framebuffer fb;
   void *d = open_driver(true, true);
   unsigned x, y, wrong = 0;
   uint16_t *own = make_frame(360, 8, 720);
   struct felem *e;

   /* A core's own rows, laid out by the driver, then the frame it is
    * lent, which goes as it is to the same surface */
   push(d, own, 360, 8, 720);
   memset(&fb, 0, sizeof(fb));
   fb.width  = 360;
   fb.height = 8;
   fb.access_flags = RETRO_MEMORY_ACCESS_WRITE | RETRO_MEMORY_ACCESS_READ;
   check(__LINE__, dispmanx_get_current_software_framebuffer(d, &fb),
         "no frame lent");
   check(__LINE__, fb.pitch == 736 && fb.format == RETRO_PIXEL_FORMAT_RGB565,
         "lent at pitch %u, not the firmware's", (unsigned)fb.pitch);
   for (y = 0; y < 8; y++)
      for (x = 0; x < 360; x++)
         ((uint16_t*)((uint8_t*)fb.data + y * fb.pitch))[x] =
            (uint16_t)(y * 256 + x);
   bad_pitch = 0;
   frame_base = (const uint8_t*)fb.data;
   frame_size = fb.pitch * 8;
   {
      video_frame_info_t fi;
      frame_info(&fi);
      dispmanx_frame(d, fb.data, VIDEO_SCALE_PACK(360, 8), 0,
            (unsigned)fb.pitch, NULL, &fi);
   }
   e = elem_on(0);
   check(__LINE__, last_write_src == fb.data && !bad_pitch,
         "lent frame not sent where it lies");
   for (y = 0; e && y < 8; y++)
      for (x = 0; x < 360; x++)
         if (res_px(e, x, y) != y * 256 + x)
            wrong++;
   check(__LINE__, e && !wrong, "lent frame landed skewed (%u)", wrong);

   /* Not while the CPU turns the frame */
   dispmanx_set_rotation(d, 1);
   check(__LINE__, !dispmanx_get_current_software_framebuffer(d, &fb),
         "frame lent while the CPU turns it");
   dispmanx_set_rotation(d, 0);
   dispmanx_free(d);
   free(own);

   /* The threaded wrapper's frame memory: rows padded to the
    * firmware's pitch go from there as they are */
   {
      uintptr_t id      = 0;
      bool cached       = false;
      unsigned align    = 0;
      uint8_t *mem;
      d   = open_driver(true, true);
      mem = (uint8_t*)dispmanx_frame_mem_alloc(d, 736 * 8, &id, &cached,
            &align);
      check(__LINE__, mem && !((uintptr_t)mem & 63) && cached && align == 32,
            "frame memory: align %u", align);
      check(__LINE__, dispmanx_frame_mem_ready(d, id), "frame memory busy");
      if (mem)
      {
         video_frame_info_t fi;
         memset(mem, 0, 736 * 8);
         frame_info(&fi);
         frame_base = mem;
         frame_size = 736 * 8;
         dispmanx_frame(d, mem, VIDEO_SCALE_PACK(360, 8), 0, 736, NULL, &fi);
         check(__LINE__, last_write_src == mem,
               "wrapper frame not sent where it lies");
         dispmanx_frame_mem_free(d, id);
      }
      dispmanx_free(d);
   }
}

static void test_nonblock(void)
{
   void *d     = open_driver(true, true);
   uint16_t *f = make_frame(256, 8, 512);
   unsigned w, subs;

   push(d, f, 256, 8, 512);
   dispmanx_set_nonblock_state(d, true, false, 0);
   /* A driver that waits for the held vsync would never return */
   if (!((struct dispmanx_video*)d)->nonblock)
   {
      check(__LINE__, 0, "vsync off not taken");
      dispmanx_free(d);
      free(f);
      return;
   }
   vsync_held = 1;
   push(d, f, 256, 8, 512);
   /* The flip is pending: the next frame is dropped, nothing waits */
   w    = writes;
   subs = submits;
   push(d, f, 256, 8, 512);
   check(__LINE__, writes == w && submits == subs,
         "frame sent while a flip was pending without vsync");
   vsync();
   push(d, f, 256, 8, 512);
   check(__LINE__, writes == w + 1, "frame after the vsync not sent");
   vsync();
   vsync_held = 0;
   dispmanx_set_nonblock_state(d, false, false, 1);
   dispmanx_free(d);
   free(f);
}

static void test_rotation(void)
{
   unsigned r;
   uint16_t *f = make_frame(4, 2, 32);

   for (r = 0; r < 4; r++)
   {
      void *d = open_driver(true, true);
      struct felem *e;
      dispmanx_set_rotation(d, r);
      push(d, f, 4, 2, 32);
      e = elem_on(0);
      if (!e)
      {
         check(__LINE__, 0, "rotation %u: no element", r);
         dispmanx_free(d);
         continue;
      }
      /* A quarter turned by the CPU: 2 x 4, read counter-clockwise */
      if (r & 1)
         check(__LINE__, res[e->res].w >= 2 && res_px(e, 0, 0) == 3
               && res_px(e, 1, 0) == 256 + 3 && res_px(e, 0, 3) == 0
               && e->src.width == (2 << 16) && e->src.height == (4 << 16),
               "rotation %u: frame not turned a quarter", r);
      else
         check(__LINE__, res_px(e, 0, 0) == 0 && res_px(e, 3, 1) == 256 + 3,
               "rotation %u: frame not as it came", r);
      check(__LINE__, e->transform == ((r & 2) ? DISPMANX_ROTATE_180
               : DISPMANX_NO_ROTATE),
            "rotation %u: HVS transform %u", r, e->transform);
      dispmanx_free(d);
   }
   free(f);
}

static void test_filtering(void)
{
   void *d;

   kernels_sent = 0;
   d = open_driver(true, true);
   check(__LINE__, kernels_sent == 0, "smooth init loaded a kernel over config.txt's");
   dispmanx_set_filtering(d, 1, false, false);
   check(__LINE__, kernels_sent == 1 && strstr(last_kernel, "255 255"),
         "video_smooth off at run time did not load nearest");
   dispmanx_free(d);
   check(__LINE__, kernels_sent == 2 && strstr(last_kernel, "227 227"),
         "nearest left loaded on exit");

   kernels_sent = 0;
   d = open_driver(false, true);
   check(__LINE__, kernels_sent == 1 && strstr(last_kernel, "255 255"),
         "video_smooth off at init did not load nearest");
   dispmanx_set_filtering(d, 1, false, false);
   check(__LINE__, kernels_sent == 1, "unchanged filtering resent the kernel");
   dispmanx_free(d);
   check(__LINE__, kernels_sent == 2 && strstr(last_kernel, "227 227"),
         "nearest left loaded on exit");
}

static void test_menu(void)
{
   void *d       = open_driver(true, true);
   uint16_t *f   = make_frame(320, 240, 640);
   uint16_t *m   = make_frame(426, 240, 852);
   struct felem *e;
   unsigned x, y, wrong = 0;

   lay_pos  = VIDEO_POS_PACK(240, 0);
   lay_dims = VIDEO_SCALE_PACK(1440, 1080);
   push(d, f, 320, 240, 640);

   dispmanx_set_texture_enable(d, true, false);
   frame_base = (const uint8_t*)m;
   frame_size = (size_t)852 * 240;
   overreads  = 0;
   dispmanx_set_texture_frame(d, m, false, VIDEO_SCALE_PACK(426, 240), 1.0f);
   e = elem_on(1);
   check(__LINE__, e && e->dst.x == 240 && e->dst.width == 1440,
         "menu not where the frame shows");
   check(__LINE__, e && (e->aflags & DISPMANX_FLAGS_ALPHA_MIX) && e->opacity == 255,
         "menu not blended by its own alpha");
   for (y = 0; e && y < 240; y++)
      for (x = 0; x < 426; x++)
         if (res_px(e, x, y) != ((y * 256 + x) & 0xffff))
            wrong++;
   check(__LINE__, e && !wrong && !overreads, "menu frame landed skewed (%u)", wrong);

   /* The frame's surface made again under the menu leaves it up */
   push(d, f, 320, 200, 640);
   check(__LINE__, elem_on(1) != NULL, "menu gone when the frame's surface was made");

   /* Over the whole screen when asked */
   dispmanx_set_texture_enable(d, true, true);
   dispmanx_set_texture_frame(d, m, false, VIDEO_SCALE_PACK(426, 240), 0.5f);
   e = elem_on(1);
   check(__LINE__, e && e->dst.x == 0 && e->dst.width == DISP_W
         && e->dst.height == DISP_H && e->opacity == 128,
         "full-screen menu at %d, %d wide, opacity %u", e ? e->dst.x : -1,
         e ? e->dst.width : -1, e ? e->opacity : 0);

   /* A menu frame of a new size */
   dispmanx_set_texture_frame(d, m, false, VIDEO_SCALE_PACK(320, 240), 1.0f);
   e = elem_on(1);
   check(__LINE__, e && e->src.width == (320 << 16), "menu not made for its new size");

   dispmanx_set_texture_enable(d, false, false);
   check(__LINE__, elem_on(1) == NULL, "menu left up when closed");

   dispmanx_free(d);
   free(f);
   free(m);
   lay_dims = 0;
}

static unsigned elems_on_from(int32_t layer)
{
   unsigned i, n = 0;
   for (i = 1; i < MAX_ELEM; i++)
      if (elem[i].live && elem[i].layer >= layer)
         n++;
   return n;
}

static void test_overlay(void)
{
   uint32_t px_a[4 * 2], px_b[2 * 2];
   struct texture_image img[2];
   const video_overlay_interface_t *ov;
   void *d     = open_driver(true, true);
   uint16_t *f = make_frame(320, 240, 640);
   struct felem *e;
   unsigned i, made, attrs;

   for (i = 0; i < 8; i++)
      px_a[i] = 0x80000000u | (i << 16) | 0x00ff00u | 0x11u;
   for (i = 0; i < 4; i++)
      px_b[i] = 0xff000000u | 0x0000aau; /* RGBA in memory: R = 0xaa */
   memset(img, 0, sizeof(img));
   img[0].pixels = px_a; img[0].width = 4; img[0].height = 2;
   img[1].pixels = px_b; img[1].width = 2; img[1].height = 2;
   img[1].supports_rgba = true;

   lay_pos  = VIDEO_POS_PACK(240, 0);
   lay_dims = VIDEO_SCALE_PACK(1440, 1080);
   dispmanx_get_overlay_interface(d, &ov);
   check(__LINE__, ov->load(d, img, 2), "overlay load");
   ov->vertex_geom(d, 0, 0.5f, 0.5f, 0.25f, 0.25f);
   ov->vertex_geom(d, 1, 0.0f, 0.0f, 0.1f, 0.1f);
   push(d, f, 320, 240, 640);
   check(__LINE__, elems_on_from(DISPMANX_OVERLAY_LAYER) == 0,
         "overlay shown before it is enabled");

   ov->enable(d, true);
   push(d, f, 320, 240, 640);
   e = elem_on(DISPMANX_OVERLAY_LAYER);
   check(__LINE__, e && e->dst.x == 240 + 720 && e->dst.y == 540
         && e->dst.width == 360 && e->dst.height == 270,
         "overlay image at %d,%d %dx%d", e ? e->dst.x : -1,
         e ? e->dst.y : -1, e ? e->dst.width : -1, e ? e->dst.height : -1);
   check(__LINE__, e && e->src.width == (4 << 16) && e->src.height == (2 << 16)
         && (e->aflags & DISPMANX_FLAGS_ALPHA_MIX) && e->opacity == 255,
         "overlay image source or blend");
   check(__LINE__, e && res_px(e, 1, 1) == (0x80000000u | (5u << 16)
            | 0x00ff00u | 0x11u), "overlay pixels not uploaded as they are");
   e = elem_on(DISPMANX_OVERLAY_LAYER + 1);
   check(__LINE__, e && res_px(e, 0, 0) == 0xffaa0000u,
         "RGBA overlay image not turned to ARGB");

   /* Nothing changed: nothing told */
   attrs = attr_changes;
   push(d, f, 320, 240, 640);
   check(__LINE__, attr_changes == attrs, "overlay restated unchanged");

   /* Fade, crop, mirror: the element changes, nothing is uploaded */
   made = res_made;
   ov->set_alpha(d, 0, 0.5f);
   ov->tex_geom(d, 0, 0.5f, 0.0f, 0.5f, 1.0f);
   ov->vertex_geom(d, 0, 0.75f, 0.5f, -0.25f, 0.25f);
   push(d, f, 320, 240, 640);
   e = elem_on(DISPMANX_OVERLAY_LAYER);
   check(__LINE__, e && e->opacity == 128 && e->src.x == (2 << 16)
         && e->src.width == (2 << 16) && e->dst.x == 240 + 720
         && e->transform == DISPMANX_FLIP_HRIZ && res_made == made,
         "overlay fade/crop/mirror: opacity %u src %d+%d transform %u",
         e ? e->opacity : 0, e ? e->src.x : -1, e ? e->src.width : -1,
         e ? e->transform : 0);

   /* Over the whole screen, partly off it: cut */
   ov->full_screen(d, true);
   ov->vertex_geom(d, 1, -0.05f, 0.0f, 0.1f, 0.1f);
   push(d, f, 320, 240, 640);
   e = elem_on(DISPMANX_OVERLAY_LAYER + 1);
   check(__LINE__, e && e->dst.x == 0 && e->dst.width == 96
         && e->src.x == (1 << 16) && e->src.width == (1 << 16),
         "overlay off the screen at %d, %d wide, source %d+%d",
         e ? e->dst.x : -1, e ? e->dst.width : -1, e ? e->src.x : -1,
         e ? e->src.width : -1);

   /* Transparent, then off: no elements */
   ov->set_alpha(d, 1, 0.0f);
   push(d, f, 320, 240, 640);
   check(__LINE__, !elem_on(DISPMANX_OVERLAY_LAYER + 1),
         "transparent overlay image left on the screen");
   ov->enable(d, false);
   push(d, f, 320, 240, 640);
   check(__LINE__, elems_on_from(DISPMANX_OVERLAY_LAYER) == 0,
         "overlay left up when disabled");

   /* A new overlay over a shown one */
   ov->enable(d, true);
   push(d, f, 320, 240, 640);
   check(__LINE__, ov->load(d, img, 1), "second overlay load");
   push(d, f, 320, 240, 640);
   check(__LINE__, elems_on_from(DISPMANX_OVERLAY_LAYER) == 1,
         "old overlay images left on the screen");

   dispmanx_free(d);
   free(f);
   lay_dims = 0;
}

static void push_msg(void *d, const uint16_t *f, const char *msg,
      bool stats)
{
   video_frame_info_t fi;
   frame_info(&fi);
   fi.font_size        = 24.0f;
   fi.font_msg_pos_x   = 0.05f;
   fi.font_msg_pos_y   = 0.05f;
   fi.font_msg_color_r = 1.0f;
   fi.font_msg_color_g = 0.5f;
   fi.font_msg_color_b = 0.0f;
   if (stats)
   {
      fi.statistics_show   = true;
      fi.stat_text         = "AA\nA";
      fi.stat_text_len     = 4;
      fi.osd_stat_params.x = 0.0f;
      fi.osd_stat_params.y = 1.0f;
   }
   frame_base = (const uint8_t*)f;
   frame_size = (size_t)640 * 240;
   dispmanx_frame(d, f, VIDEO_SCALE_PACK(320, 240), 0, 640, msg, &fi);
}

static void test_text(void)
{
   void *d;
   video_info_t info;
   uint16_t *f = make_frame(320, 240, 640);
   struct felem *e;
   unsigned w, a, shown;
   static char long_msg[64];

   memset(&info, 0, sizeof(info));
   info.vsync       = true;
   info.smooth      = true;
   info.force_aspect = true;
   info.font_enable = true;
   d = dispmanx_init(&info);

   push_msg(d, f, "AA", false);
   e = elem_on(DISPMANX_TEXT_LAYER);
   /* Baseline at 1080 * 0.95 = 1026, two glyphs 4 apart, 3x5 each */
   check(__LINE__, e && e->dst.x == 96 && e->dst.y == 1026
         && e->dst.width == 7 && e->dst.height == 5
         && e->src.width == (7 << 16) && e->aflags
            == DISPMANX_FLAGS_ALPHA_FROM_SOURCE,
         "message at %d,%d %dx%d", e ? e->dst.x : -1, e ? e->dst.y : -1,
         e ? e->dst.width : -1, e ? e->dst.height : -1);
   check(__LINE__, font_asked == 24, "font made at %u pixels", font_asked);
   check(__LINE__, e && res_px(e, 0, 0) == 0xffff7f00u && res_px(e, 3, 0) == 0
         && res_px(e, 4, 4) == 0xffff7f00u,
         "message pixels %08x %08x", e ? res_px(e, 0, 0) : 0,
         e ? res_px(e, 3, 0) : 0);
   shown = e ? e->res : 0;

   /* The same message: nothing drawn, nothing told */
   w = writes;
   a = attr_changes;
   push_msg(d, f, "AA", false);
   check(__LINE__, writes == w + 1 && attr_changes == a,
         "unchanged message redrawn (%u writes, %u changes)",
         writes - w, attr_changes - a);

   /* A new one goes into the other resource, which the flip shows */
   push_msg(d, f, "A", false);
   e = elem_on(DISPMANX_TEXT_LAYER);
   check(__LINE__, e && e->res != shown && e->dst.width == 3,
         "changed message not flipped to the other resource");

   /* Outgrowing the resources: made again, the old ones go once the
    * flip that took them off has landed */
   memset(long_msg, 'A', 40);
   push_msg(d, f, long_msg, false);
   e = elem_on(DISPMANX_TEXT_LAYER);
   check(__LINE__, e && e->dst.width == 159 && res[e->res].w >= 159,
         "long message %d wide", e ? e->dst.width : -1);

   /* Statistics on a layer of their own, lines 10 apart */
   push_msg(d, f, NULL, true);
   check(__LINE__, !elem_on(DISPMANX_TEXT_LAYER), "message left up");
   e = elem_on(DISPMANX_TEXT_LAYER + 1);
   check(__LINE__, e && e->dst.x == 0 && e->dst.width == 7
         && e->dst.height == 15, "statistics at %d %dx%d",
         e ? e->dst.x : -1, e ? e->dst.width : -1, e ? e->dst.height : -1);
   push_msg(d, f, NULL, false);
   check(__LINE__, !elem_on(DISPMANX_TEXT_LAYER + 1), "statistics left up");

   dispmanx_free(d);
   check(__LINE__, font_freed == font_made, "font left made");

   /* Freed right after outgrowing: the retired resources go too */
   d = dispmanx_init(&info);
   push_msg(d, f, "A", false);
   push_msg(d, f, long_msg, false);
   dispmanx_free(d);
   for (w = 0; w < MAX_RES; w++)
      check(__LINE__, !res[w].live, "resource %u left after free", w);

   /* Messages off: nothing */
   info.font_enable = false;
   d = dispmanx_init(&info);
   push_msg(d, f, "AA", true);
   check(__LINE__, !elem_on(DISPMANX_TEXT_LAYER)
         && !elem_on(DISPMANX_TEXT_LAYER + 1), "text drawn with fonts off");
   dispmanx_free(d);
   free(f);
}

static void test_screen(void)
{
   void *d;
   uint8_t *buf;
   uint16_t *f = make_frame(320, 240, 640);

   /* The monitor index picks the display; one not there falls back */
   settings.uints.video_monitor_index = 2;
   hdmi1_connected = 1;
   d = open_driver(true, true);
   check(__LINE__, display_opened == DISPMANX_ID_HDMI1
         && size_asked == DISPMANX_ID_HDMI1
         && ((struct dispmanx_video*)d)->disp_dims
            == VIDEO_SCALE_PACK(1280, 720),
         "HDMI1 not opened at its size");
   dispmanx_free(d);
   hdmi1_connected = 0;
   d = open_driver(true, true);
   check(__LINE__, d && display_opened == DISPMANX_ID_MAIN_LCD
         && size_asked == DISPMANX_ID_MAIN_LCD,
         "missing display did not fall back to the default");
   dispmanx_free(d);
   settings.uints.video_monitor_index = 0;

   /* Screenshots: the viewport of the screen as composed, BGR, bottom
    * row first, black off the screen */
   d        = open_driver(true, true);
   lay_pos  = VIDEO_POS_PACK(-8, 0);
   lay_dims = VIDEO_SCALE_PACK(1440, 1080);
   push(d, f, 320, 240, 640);
   buf       = (uint8_t*)malloc((size_t)1440 * 1080 * 3);
   bad_pitch = 0;
   check(__LINE__, dispmanx_read_viewport(d, buf, false), "read_viewport");
   check(__LINE__, !bad_pitch, "snapshot read at a pitch not the resource's");
   check(__LINE__, buf[0] == 0 && buf[1] == 0 && buf[2] == 0,
         "off the screen not black");
   /* Row 0 is the screen's row 1079; x 8 of it is the screen's x 0 */
   check(__LINE__, buf[8 * 3] == 0x5a && buf[8 * 3 + 1] == (1079 & 0xff)
         && buf[8 * 3 + 2] == 0,
         "screenshot pixel %02x %02x %02x", buf[8 * 3], buf[8 * 3 + 1],
         buf[8 * 3 + 2]);
   check(__LINE__, buf[((size_t)1079 * 1440 + 100) * 3 + 2] == 92
         && buf[((size_t)1079 * 1440 + 100) * 3 + 1] == 0,
         "screenshot rows not bottom first");
   free(buf);
   dispmanx_free(d);
   free(f);
   lay_dims = 0;
}

int main(void)
{
   unsigned i;

   test_layout();
   test_pitch();
   test_rotation();
   test_lend();
   test_nonblock();
   test_overlay();
   test_screen();
   test_text();
   test_filtering();
   test_menu();

   for (i = 0; i < MAX_RES; i++)
      check(__LINE__, !res[i].live, "resource %u left alive", i);
   for (i = 0; i < MAX_ELEM; i++)
      check(__LINE__, !elem[i].live, "element %u left on the screen", i);

   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("dispmanx_display: all passed\n");
   return 0;
}
