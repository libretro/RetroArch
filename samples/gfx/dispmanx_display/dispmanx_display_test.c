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
#define MAX_ELEM 8

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
   return (t == VC_IMAGE_XRGB8888 || t == VC_IMAGE_ARGB8888
         || t == VC_IMAGE_RGBA32) ? 4 : 2;
}

void bcm_host_init(void) { }
void bcm_host_deinit(void) { }
int32_t graphics_get_display_size(const uint16_t n, uint32_t *w, uint32_t *h)
{
   (void)n;
   *w = DISP_W;
   *h = DISP_H;
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
{ (void)d; return 1; }
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

int main(void)
{
   unsigned i;

   test_layout();
   test_pitch();
   test_rotation();
   test_lend();
   test_nonblock();
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
