/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2017 - Manuel Alfayate
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#include <compat/strl.h>
#include <encodings/utf.h>
#include <rthreads/rthreads.h>
#include <rthreads/retro_eventcount.h>
#include <retro_atomic.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../font_driver.h"
#include "../video_views.h"
#include "../video_views_cpu.h"
#include "../video_turn.h"

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

#define NUMPAGES 2

/* Lowlevel SunxiG2D functions block */
#define FBIOGET_LAYER_HDL_0 0x4700
#define FBIOGET_LAYER_HDL_1 0x4701
#define DISP_FORMAT_ARGB8888 0xa
#define DISP_MOD_INTERLEAVED 0x1
#define DISP_SEQ_ARGB 0x0
#define DISP_LAYER_WORK_MODE_NORMAL 0
#define DISP_LAYER_WORK_MODE_SCALER 4
#define DISP_CMD_VERSION 0x00

/* for tracking the ioctls API/ABI */
#define SUNXI_DISP_VERSION_MAJOR 1
#define SUNXI_DISP_VERSION_MINOR 0
#define SUNXI_DISP_VERSION ((SUNXI_DISP_VERSION_MAJOR << 16) | SUNXI_DISP_VERSION_MINOR)

typedef struct
{
   __s32 x;
   __s32 y;
   __u32 width;
   __u32 height;
} __disp_rect_t;

typedef struct
{
   __u32 width;
   __u32 height;
} __disp_rectsz_t;

typedef enum tag_DISP_CMD
{
   /* ----layer---- */
   DISP_CMD_LAYER_REQUEST        = 0x40,
   DISP_CMD_LAYER_RELEASE        = 0x41,
   DISP_CMD_LAYER_OPEN           = 0x42,
   DISP_CMD_LAYER_CLOSE          = 0x43,
   DISP_CMD_LAYER_SET_FB         = 0x44,
   DISP_CMD_LAYER_GET_FB         = 0x45,
   DISP_CMD_LAYER_SET_SRC_WINDOW = 0x46,
   DISP_CMD_LAYER_GET_SRC_WINDOW = 0x47,
   DISP_CMD_LAYER_SET_SCN_WINDOW = 0x48,
   DISP_CMD_LAYER_GET_SCN_WINDOW = 0x49,
   DISP_CMD_LAYER_SET_PARA       = 0x4a,
   DISP_CMD_LAYER_GET_PARA       = 0x4b
} __disp_cmd_t;

typedef struct
{
   uintptr_t           framebuffer_paddr; /* physical address */
   uint8_t            *framebuffer_addr;  /* mmapped address */
   int                 fd_fb;
   int                 fd_disp;
   int                 fb_id;             /* /dev/fb0 = 0, /dev/fb1 = 1 */
   int                 xres, yres;
   /* Layers support */
   int                 gfx_layer_id;
   int                 layer_id;
   int                 layer_has_scaler;
   int                 layer_scaler_is_enabled;
   uint32_t            framebuffer_size;  /* total size of the framebuffer */
   uint32_t            gfx_layer_size;    /* the size of the primary layer */
   float               refresh_rate;
} sunxi_disp_t;

typedef struct
{
   /*
    * the contents of the frame buffer address for rgb type only addr[0]
    * valid
    */
   __u32 addr[3];
   __disp_rectsz_t size; /* unit is pixel */
   unsigned int format;
   unsigned int seq;
   unsigned int mode;
   /*
    * blue red color swap flag, FALSE:RGB; TRUE:BGR,only used in rgb format
    */
   signed char br_swap;
   unsigned int cs_mode; /* color space */
   signed char b_trd_src; /* if 3d source, used for scaler mode layer */
   /* source 3d mode, used for scaler mode layer */
   unsigned int trd_mode;
   __u32 trd_right_addr[3]; /* used when in frame packing 3d mode */
} __disp_fb_t;

typedef struct
{
   unsigned int mode;         /* layer work mode */
   signed char b_from_screen;
   /*
    * layer pipe,0/1,if in scaler mode, scaler0 must be pipe0,
    * scaler1 must be pipe1
    */
   __u8 pipe;
   /*
    * layer priority,can get layer prio,but never set layer prio.
    * From bottom to top, priority from low to high
    */
   __u8 prio;
   signed char alpha_en;      /* layer global alpha enable */
   __u16 alpha_val;           /* layer global alpha value */
   signed char ck_enable;     /* layer color key enable */
   /*  framebuffer source window,only care x,y if is not scaler mode */
   __disp_rect_t src_win;
   __disp_rect_t scn_win;     /* screen window */
   __disp_fb_t fb;            /* framebuffer */
   signed char b_trd_out;     /* if output 3d mode, used for scaler mode layer */
   unsigned int out_trd_mode; /* output 3d mode, used for scaler mode layer */
} __disp_layer_info_t;

/*****************************************************************************
 * Support for scaled layers                                                 *
 *****************************************************************************/

static int sunxi_layer_change_work_mode(sunxi_disp_t *ctx, int new_mode)
{
   __disp_layer_info_t layer_info;
   unsigned long tmp[4];

   if (ctx->layer_id < 0)
      return -1;

   tmp[0] = ctx->fb_id;
   tmp[1] = ctx->layer_id;
   tmp[2] = (unsigned long)&layer_info;

   if (ioctl(ctx->fd_disp, DISP_CMD_LAYER_GET_PARA, tmp) < 0)
      return -1;

   layer_info.mode = new_mode;

   tmp[0] = ctx->fb_id;
   tmp[1] = ctx->layer_id;
   tmp[2] = (unsigned long)&layer_info;
   return ioctl(ctx->fd_disp, DISP_CMD_LAYER_SET_PARA, tmp);
}

static int sunxi_layer_reserve(sunxi_disp_t *ctx)
{
   __disp_layer_info_t layer_info;
   unsigned long tmp[4];

   /* try to allocate a layer */

   tmp[0] = ctx->fb_id;
   tmp[1] = DISP_LAYER_WORK_MODE_NORMAL;
   ctx->layer_id = ioctl(ctx->fd_disp, DISP_CMD_LAYER_REQUEST, &tmp);
   if (ctx->layer_id < 0)
      return -1;

   /* Initially set the layer configuration to something reasonable */

   tmp[0] = ctx->fb_id;
   tmp[1] = ctx->layer_id;
   tmp[2] = (unsigned long)&layer_info;
   if (ioctl(ctx->fd_disp, DISP_CMD_LAYER_GET_PARA, tmp) < 0)
      return -1;

   /* the screen and overlay layers need to be in different pipes */
   layer_info.pipe      = 1;
   layer_info.alpha_en  = 1;
   layer_info.alpha_val = 255;

   layer_info.fb.addr[0] = ctx->framebuffer_paddr;
   layer_info.fb.size.width = 1;
   layer_info.fb.size.height = 1;
   layer_info.fb.format = DISP_FORMAT_ARGB8888;
   layer_info.fb.seq = DISP_SEQ_ARGB;
   layer_info.fb.mode = DISP_MOD_INTERLEAVED;

   tmp[0] = ctx->fb_id;
   tmp[1] = ctx->layer_id;
   tmp[2] = (unsigned long)&layer_info;
   if (ioctl(ctx->fd_disp, DISP_CMD_LAYER_SET_PARA, tmp) < 0)
      return -1;

   /* Now probe the scaler mode to see if there is a free scaler available */
   if (sunxi_layer_change_work_mode(ctx, DISP_LAYER_WORK_MODE_SCALER) == 0)
      ctx->layer_has_scaler = 1;

   /* Revert back to normal mode */
   sunxi_layer_change_work_mode(ctx, DISP_LAYER_WORK_MODE_NORMAL);
   ctx->layer_scaler_is_enabled = 0;

   return ctx->layer_id;
}

static int sunxi_layer_set_output_window(sunxi_disp_t *ctx,
      const __disp_rect_t *win)
{
   __disp_rect_t win_rect = *win;
   unsigned long tmp[4];

   if (ctx->layer_id < 0 || !win->width || !win->height)
      return -1;

   tmp[0] = ctx->fb_id;
   tmp[1] = ctx->layer_id;
   tmp[2] = (unsigned long)&win_rect;
   return ioctl(ctx->fd_disp, DISP_CMD_LAYER_SET_SCN_WINDOW, &tmp);
}

static int sunxi_layer_show(sunxi_disp_t *ctx)
{
   unsigned long tmp[4];

   if (ctx->layer_id < 0)
      return -1;

   tmp[0] = ctx->fb_id;
   tmp[1] = ctx->layer_id;
   return ioctl(ctx->fd_disp, DISP_CMD_LAYER_OPEN, &tmp);
}

static int sunxi_layer_release(sunxi_disp_t *ctx)
{
   unsigned long tmp[4];

   if (ctx->layer_id < 0)
      return -1;

   tmp[0] = ctx->fb_id;
   tmp[1] = ctx->layer_id;
   ioctl(ctx->fd_disp, DISP_CMD_LAYER_RELEASE, &tmp);

   ctx->layer_id = -1;
   ctx->layer_has_scaler = 0;
   return 0;
}

/* The layer reads XRGB8888 from @offset into the framebuffer, rows
 * @stride pixels apart and @height of them, the part @src shown. The
 * pages are always 32 bits a pixel, whatever the console's depth. */
static int sunxi_layer_set_input(sunxi_disp_t *ctx,
      uint32_t offset, unsigned stride, unsigned height,
      const __disp_rect_t *src)
{
   __disp_fb_t fb;
   __disp_rect_t rect = *src;
   unsigned long tmp[4];

   memset(&fb, 0, sizeof(fb));

   if (ctx->layer_id < 0)
      return -1;

   if (!ctx->layer_scaler_is_enabled)
   {
      if (sunxi_layer_change_work_mode(ctx, DISP_LAYER_WORK_MODE_SCALER) == 0)
         ctx->layer_scaler_is_enabled = 1;
      else
         return -1;
   }

   fb.addr[0]     = (__u32)(ctx->framebuffer_paddr + offset);
   fb.size.width  = stride;
   fb.size.height = height;
   fb.format      = DISP_FORMAT_ARGB8888;
   fb.seq         = DISP_SEQ_ARGB;
   fb.mode        = DISP_MOD_INTERLEAVED;

   tmp[0]         = ctx->fb_id;
   tmp[1]         = ctx->layer_id;
   tmp[2]         = (unsigned long)&fb;

   if (ioctl(ctx->fd_disp, DISP_CMD_LAYER_SET_FB, &tmp) < 0)
      return -1;

   tmp[0]         = ctx->fb_id;
   tmp[1]         = ctx->layer_id;
   tmp[2]         = (unsigned long)&rect;

   return ioctl(ctx->fd_disp, DISP_CMD_LAYER_SET_SRC_WINDOW, &tmp);
}

static sunxi_disp_t *sunxi_disp_init(int fb_id)
{
   int tmp, version;
   char device[16];
   struct fb_var_screeninfo fb_var;
   struct fb_fix_screeninfo fb_fix;
   sunxi_disp_t *ctx = (sunxi_disp_t*)calloc(1, sizeof(sunxi_disp_t));

   if (!ctx)
      return NULL;

   ctx->fb_id    = fb_id;
   ctx->fd_fb    = -1;
   ctx->layer_id = -1;
   snprintf(device, sizeof(device), "/dev/fb%d", fb_id);

   ctx->fd_disp = open("/dev/disp", O_RDWR);

   /* maybe it's even not a sunxi hardware */
   if (ctx->fd_disp < 0)
      goto error;

   /* version check */
   tmp     = SUNXI_DISP_VERSION;
   version = ioctl(ctx->fd_disp, DISP_CMD_VERSION, &tmp);
   if (version < 0)
      goto error;

   if ((ctx->fd_fb = open(device, O_RDWR)) < 0)
      goto error;

   if (     ioctl(ctx->fd_fb, FBIOGET_VSCREENINFO, &fb_var) < 0
         || ioctl(ctx->fd_fb, FBIOGET_FSCREENINFO, &fb_fix) < 0)
      goto error;

   ctx->xres               = fb_var.xres;
   ctx->yres               = fb_var.yres;
   ctx->framebuffer_paddr  = fb_fix.smem_start;
   ctx->framebuffer_size   = fb_fix.smem_len;
   ctx->gfx_layer_size     = fb_fix.line_length
      ? fb_fix.line_length * fb_var.yres
      : fb_var.xres * fb_var.yres * fb_var.bits_per_pixel / 8;
   /* Unknown without a pixel clock: the frontend's own rate stands */
   if (fb_var.pixclock)
      ctx->refresh_rate    = 1000000.0f / fb_var.pixclock * 1000000.0f /
         (fb_var.yres + fb_var.upper_margin + fb_var.lower_margin + fb_var.vsync_len) /
         (fb_var.xres + fb_var.left_margin  + fb_var.right_margin + fb_var.hsync_len);

   if (ctx->framebuffer_size < ctx->gfx_layer_size)
      goto error;

   /* mmap framebuffer memory */
   ctx->framebuffer_addr = (uint8_t *)mmap(0, ctx->framebuffer_size,
         PROT_READ | PROT_WRITE, MAP_SHARED, ctx->fd_fb, 0);

   if (ctx->framebuffer_addr == MAP_FAILED)
   {
      ctx->framebuffer_addr = NULL;
      goto error;
   }

   /* Get the id of the screen layer */
   if (ioctl(ctx->fd_fb,
            ctx->fb_id == 0 ? FBIOGET_LAYER_HDL_0 : FBIOGET_LAYER_HDL_1,
            &ctx->gfx_layer_id))
      goto error;

   if (sunxi_layer_reserve(ctx) < 0)
      goto error;

   /* The frame is only ever shown scaled */
   if (!ctx->layer_has_scaler)
   {
      RARCH_ERR("[Sunxi] No display scaler free for the layer.\n");
      goto error;
   }

   return ctx;

error:
   if (ctx->layer_id >= 0)
      sunxi_layer_release(ctx);
   if (ctx->framebuffer_addr)
      munmap(ctx->framebuffer_addr, ctx->framebuffer_size);
   if (ctx->fd_fb >= 0)
      close(ctx->fd_fb);
   if (ctx->fd_disp >= 0)
      close(ctx->fd_disp);
   free(ctx);
   return NULL;
}

static void sunxi_disp_close(sunxi_disp_t *ctx)
{
   if (!ctx)
      return;
   sunxi_layer_release(ctx);
   munmap(ctx->framebuffer_addr, ctx->framebuffer_size);
   close(ctx->fd_fb);
   close(ctx->fd_disp);
   free(ctx);
}
/* END of lowlevel SunxiG2D functions block */

void pixman_composite_src_0565_8888_asm_neon(int width,
   int height,
   uint32_t *dst,
   int dst_stride_pixels,
   uint16_t *src,
   int src_stride_pixels);

void pixman_composite_src_8888_8888_asm_neon(int width,
   int height,
   uint32_t *dst,
   int dst_stride_pixels,
   uint16_t *src,
   int src_stride_pixels);

typedef void (*sunxi_blit_t)(int width, int height, uint32_t *dst,
      int dst_stride_pixels, uint16_t *src, int src_stride_pixels);

struct sunxi_page
{
   uint32_t *address;
   uint32_t offset;
};

struct sunxi_video
{
   /* Sunxi framebuffer information struct */
   sunxi_disp_t *sunxi_disp;
   /* The core's pixels to the pages' XRGB8888, NEON */
   sunxi_blit_t blit;
   const font_renderer_driver_t *font_driver;
   void *font;
   /* The console as it was, put back on exit */
   char *screen_bck;
   /* A 16-bit frame turned, ahead of its blit */
   uint8_t *turned;
   size_t turned_size;
   /* Both eyes side by side, for anaglyph and interlaced */
   uint32_t *views_canvas;
   size_t views_canvas_size;
   sthread_t *vsync_thread;
   video_views_cpu_t views_cpu;
   retro_eventcount_t vsync_ec;

   struct sunxi_page pages[NUMPAGES];
   /* The menu's frame, at the end of the framebuffer */
   struct sunxi_page menu_page;

   /* Where the frame shows, as the frontend lays it out */
   video_viewport_t vp;
   /* What the layer was last told, so only changes go to the kernel */
   __disp_rect_t scn_set;

   /* A flip issued and not yet seen by the vsync thread */
   retro_atomic_int_t pageflip_pending;
   /* Vsyncs seen, for the menu to pace on */
   retro_atomic_int_t vsync_count;
   retro_atomic_int_t keep_vsync;
   /* The framebuffer cannot wait for a vsync: nothing waits */
   retro_atomic_int_t no_vsync;
   /* For get_flags: the last frame with a map was drawn whole */
   retro_atomic_int_t views_fallback;

   size_t screensize;
   /* The frame as it comes, and as the pages hold it (turned) */
   unsigned frame_dims;
   unsigned page_dims;
   /* Pixels from one page row to the next */
   unsigned page_stride;
   /* The rotation the pages were laid out for */
   unsigned page_rot;
   /* The page to draw next, and the one last flipped to */
   unsigned next;
   unsigned shown;
   /* A core's views, composed straight into the pages the scaler
    * enlarges to the whole screen: the size they are composed at, 0
    * while the frame is drawn whole */
   unsigned views_dims;
   /* Quarter turns counter-clockwise, from set_rotation */
   unsigned rotation;
   unsigned menu_dims;
   unsigned menu_stride;
   unsigned font_px;
   unsigned bytes_per_pixel;
   char font_path[PATH_MAX_LENGTH];

   bool menu_active;
   bool menu_full_screen;
   bool keep_aspect;
   bool nonblock;
   bool font_enable;
   bool views_seen;
   bool room_logged;
};

static void sunxi_blank_console(struct sunxi_video *_dispvars)
{
   sunxi_disp_t *disp = _dispvars->sunxi_disp;

   /* Disable cursor blinking so it's not visible. */
   if (system("setterm -cursor off") != 0) { }

   /* Back up the console and blank it */
   _dispvars->screensize = disp->gfx_layer_size;
   _dispvars->screen_bck = (char*)malloc(_dispvars->screensize);
   if (_dispvars->screen_bck)
      memcpy(_dispvars->screen_bck, disp->framebuffer_addr,
            _dispvars->screensize);
   memset(disp->framebuffer_addr, 0x00, _dispvars->screensize);
}

static void sunxi_restore_console(struct sunxi_video *_dispvars)
{
   if (system("setterm -cursor on") != 0) { }

   if (_dispvars->screen_bck)
      memcpy(_dispvars->sunxi_disp->framebuffer_addr,
            _dispvars->screen_bck, _dispvars->screensize);
   free(_dispvars->screen_bck);
   _dispvars->screen_bck = NULL;
}

/* Clears the flip in flight at each vsync. A framebuffer that cannot
 * wait for one ends the thread, and from then on nothing waits. */
static void sunxi_vsync_thread_func(void *data)
{
   struct sunxi_video *_dispvars = (struct sunxi_video*)data;
   int fd                        = _dispvars->sunxi_disp->fd_fb;

   while (retro_atomic_load_acquire_int(&_dispvars->keep_vsync))
   {
      __u32 crtc = 0;
      if (ioctl(fd, FBIO_WAITFORVSYNC, &crtc) < 0)
      {
         retro_atomic_store_release_int(&_dispvars->no_vsync, 1);
         retro_atomic_store_release_int(&_dispvars->pageflip_pending, 0);
         retro_eventcount_notify(&_dispvars->vsync_ec);
         break;
      }
      retro_atomic_fetch_add_int(&_dispvars->vsync_count, 1);
      retro_atomic_store_release_int(&_dispvars->pageflip_pending, 0);
      retro_eventcount_notify(&_dispvars->vsync_ec);
   }
}

/* Until the vsync thread has seen the last flip land */
static void sunxi_wait_flip(struct sunxi_video *_dispvars)
{
   int key;

   while (retro_atomic_load_acquire_int(&_dispvars->pageflip_pending))
   {
      key = retro_eventcount_prepare_wait(&_dispvars->vsync_ec);

      if (!retro_atomic_load_acquire_int(&_dispvars->pageflip_pending))
      {
         retro_eventcount_cancel_wait(&_dispvars->vsync_ec);
         break;
      }

      retro_eventcount_commit_wait(&_dispvars->vsync_ec, key);
   }
}

/* Until the next vsync, for a frame that flips nothing */
static void sunxi_wait_vsync(struct sunxi_video *_dispvars)
{
   int key;
   int start = retro_atomic_load_acquire_int(&_dispvars->vsync_count);

   while (!retro_atomic_load_acquire_int(&_dispvars->no_vsync))
   {
      key = retro_eventcount_prepare_wait(&_dispvars->vsync_ec);
      if (     retro_atomic_load_acquire_int(&_dispvars->vsync_count) != start
            || retro_atomic_load_acquire_int(&_dispvars->no_vsync))
      {
         retro_eventcount_cancel_wait(&_dispvars->vsync_ec);
         break;
      }
      retro_eventcount_commit_wait(&_dispvars->vsync_ec, key);
   }
}

/* Where the pages begin: after the console, 64 bytes aligned */
static uint32_t sunxi_pages_base(const sunxi_disp_t *disp)
{
   return (disp->gfx_layer_size + 63) & ~63u;
}

/* The pages for @dims, laid out after the console: rows the screen's
 * width apart when there is room for that, else as narrow as the frame.
 * False when the framebuffer has no room for two. */
static bool sunxi_setup_pages(struct sunxi_video *_dispvars, unsigned dims)
{
   unsigned i;
   sunxi_disp_t *disp = _dispvars->sunxi_disp;
   uint32_t base      = sunxi_pages_base(disp);
   unsigned w         = VIDEO_SCALE_W(dims);
   unsigned h         = VIDEO_SCALE_H(dims);
   unsigned stride    = (w > (unsigned)disp->xres)
      ? ((w + 15) & ~15u) : (unsigned)disp->xres;
   uint64_t page      = (uint64_t)stride * 4 * h;

   if (base + NUMPAGES * page > disp->framebuffer_size)
   {
      stride = (w + 15) & ~15u;
      page   = (uint64_t)stride * 4 * h;
   }
   if (!w || !h || base + NUMPAGES * page > disp->framebuffer_size)
   {
      if (!_dispvars->room_logged)
         RARCH_ERR("[Sunxi] No room in the framebuffer for two %ux%u pages.\n",
               w, h);
      _dispvars->room_logged = true;
      _dispvars->page_dims   = 0;
      return false;
   }

   for (i = 0; i < NUMPAGES; i++)
   {
      _dispvars->pages[i].offset  = base + (uint32_t)(i * page);
      _dispvars->pages[i].address = (uint32_t*)(disp->framebuffer_addr
            + _dispvars->pages[i].offset);
   }
   _dispvars->page_dims   = dims;
   _dispvars->page_stride = stride;
   /* The menu finds its place past these when it next draws */
   _dispvars->menu_dims   = 0;
   return true;
}

/* The part of a @src_dims source shown over @vp, cut where it leaves
 * the screen: the screen and source windows. False when none of it is
 * on the screen. */
static bool sunxi_place(const sunxi_disp_t *disp, unsigned src_dims,
      const video_viewport_t *vp, __disp_rect_t *scn, __disp_rect_t *src)
{
   int x  = VIDEO_POS_X(vp->pos);
   int y  = VIDEO_POS_Y(vp->pos);
   int w  = (int)VIDEO_SCALE_W(vp->dims);
   int h  = (int)VIDEO_SCALE_H(vp->dims);
   int x1 = x + w;
   int y1 = y + h;
   int sw = (int)VIDEO_SCALE_W(src_dims);
   int sh = (int)VIDEO_SCALE_H(src_dims);
   int cl = 0, ct = 0, cr = 0, cb = 0;

   if (w <= 0 || h <= 0 || !sw || !sh)
      return false;
   if (x < 0)
   {
      cl = -x;
      x  = 0;
   }
   if (y < 0)
   {
      ct = -y;
      y  = 0;
   }
   if (x1 > disp->xres)
   {
      cr = x1 - disp->xres;
      x1 = disp->xres;
   }
   if (y1 > disp->yres)
   {
      cb = y1 - disp->yres;
      y1 = disp->yres;
   }
   if (x1 <= x || y1 <= y)
      return false;

   scn->x      = x;
   scn->y      = y;
   scn->width  = (__u32)(x1 - x);
   scn->height = (__u32)(y1 - y);
   src->x      = (__s32)((int64_t)cl * sw / w);
   src->y      = (__s32)((int64_t)ct * sh / h);
   src->width  = (__u32)(sw - src->x - (int)((int64_t)cr * sw / w));
   src->height = (__u32)(sh - src->y - (int)((int64_t)cb * sh / h));
   if (!src->width || !src->height)
      return false;
   return true;
}

/* The whole screen */
static void sunxi_full_viewport(const struct sunxi_video *_dispvars,
      video_viewport_t *vp)
{
   vp->pos       = VIDEO_POS_PACK(0, 0);
   vp->dims      = VIDEO_SCALE_PACK(_dispvars->sunxi_disp->xres,
         _dispvars->sunxi_disp->yres);
   vp->full_dims = vp->dims;
}

/* Where the frame shows, as the frontend lays it out on the screen */
static void sunxi_update_viewport(struct sunxi_video *_dispvars)
{
   sunxi_full_viewport(_dispvars, &_dispvars->vp);
   video_driver_update_viewport(&_dispvars->vp, false,
         _dispvars->keep_aspect, true);
}

/* Flips to the @src_dims image at @offset, rows @stride pixels apart,
 * shown over @vp: the screen window goes to the kernel only when it
 * moved. The flip lands at the next vsync. */
static void sunxi_flip(struct sunxi_video *_dispvars, uint32_t offset,
      unsigned stride, unsigned src_dims, const video_viewport_t *vp)
{
   __disp_rect_t scn, src;
   sunxi_disp_t *disp = _dispvars->sunxi_disp;

   if (!sunxi_place(disp, src_dims, vp, &scn, &src))
      return;
   sunxi_layer_set_input(disp, offset, stride, VIDEO_SCALE_H(src_dims),
         &src);
   if (memcmp(&scn, &_dispvars->scn_set, sizeof(scn)))
   {
      sunxi_layer_set_output_window(disp, &scn);
      if (!_dispvars->scn_set.width)
         sunxi_layer_show(disp);
      _dispvars->scn_set = scn;
   }
   /* Without a vsync to wait for, nothing is in flight */
   if (!retro_atomic_load_acquire_int(&_dispvars->no_vsync))
      retro_atomic_store_release_int(&_dispvars->pageflip_pending, 1);
}

/* The font at @px pixels, made again when the size changes */
static bool sunxi_font(struct sunxi_video *_dispvars, unsigned px)
{
   if (px < 8)
      px = 8;
   if (_dispvars->font && _dispvars->font_px == px)
      return true;
   if (_dispvars->font)
      _dispvars->font_driver->free(_dispvars->font);
   _dispvars->font = NULL;
   if (!font_renderer_create_default(&_dispvars->font_driver,
            &_dispvars->font,
            *_dispvars->font_path ? _dispvars->font_path : NULL,
            px, FONT_ATLAS_FORMAT_A8))
   {
      _dispvars->font_enable = false;
      return false;
   }
   _dispvars->font_px = px;
   /* Glyphs are read from the atlas in memory: it may grow */
   _dispvars->font_driver->get_atlas(_dispvars->font)->max_dims
      = VIDEO_SCALE_PACK(2048, 2048);
   return true;
}

/* @msg blended over the XRGB8888 page at @out, @w x @h pixels, rows
 * @stride pixels apart, its first baseline at @base_x, @base_y */
static void sunxi_draw_text(struct sunxi_video *_dispvars, uint32_t *out,
      unsigned stride, unsigned w, unsigned h, const char *msg,
      size_t msg_len, int base_x, int base_y, uint32_t fr, uint32_t fg,
      uint32_t fb)
{
   int x, y;
   void *font_data = _dispvars->font;
   const struct font_glyph *(*get_glyph)(void*, uint32_t)
      = _dispvars->font_driver->get_glyph;
   const struct font_glyph *glyph_q = get_glyph(font_data, '?');
   const struct font_atlas *atlas;
   struct font_line_metrics *line_metrics = NULL;
   int line_h = 0, line_x = base_x, line_y = base_y;

   if (_dispvars->font_driver->get_line_metrics)
      _dispvars->font_driver->get_line_metrics(font_data, &line_metrics);
   if (line_metrics)
      line_h = (int)line_metrics->height;

#define FONT_LAYOUT_ALIGNED 0
#define FONT_LAYOUT_LINE(line, line_width, count, bytes) \
   do \
   { \
      (void)(line_width); (void)(count); (void)(bytes); \
      line_x = base_x; \
      line_y = base_y + (line) * line_h; \
   } while (0)
#define FONT_LAYOUT_GLYPH(glyph, pen_x, pen_y) \
   do \
   { \
      int gx = line_x + (pen_x) + (glyph)->draw_offset_x; \
      int gy = line_y + (pen_y) + (glyph)->draw_offset_y; \
      int gw = (int)VIDEO_SCALE_W((glyph)->dims); \
      int gh = (int)VIDEO_SCALE_H((glyph)->dims); \
      const uint8_t *gsrc; \
      atlas = _dispvars->font_driver->get_atlas(font_data); \
      gsrc  = atlas->buffer + VIDEO_SCALE_W((glyph)->atlas_pos) \
         + (size_t)VIDEO_SCALE_H((glyph)->atlas_pos) * atlas->width; \
      for (y = 0; y < gh; y++) \
      { \
         int py = gy + y; \
         if (py < 0 || py >= (int)h) \
            continue; \
         for (x = 0; x < gw; x++) \
         { \
            int px = gx + x; \
            uint32_t a = gsrc[(size_t)y * atlas->width + x]; \
            uint32_t q, *d; \
            if (!a || px < 0 || px >= (int)w) \
               continue; \
            d  = out + (size_t)py * stride + px; \
            q  = *d; \
            *d = ((((q >> 16) & 0xff) * (255 - a) + fr * a) / 255) << 16 \
               | ((((q >> 8) & 0xff) * (255 - a) + fg * a) / 255) << 8 \
               |  (((q & 0xff) * (255 - a) + fb * a) / 255); \
         } \
      } \
   } while (0)
#include "../font_layout.h"
}

/* The message and the statistics over the page about to be shown, at
 * the size they show on the screen */
static void sunxi_draw_texts(struct sunxi_video *_dispvars, uint32_t *page,
      unsigned dims, const char *msg, const video_frame_info_t *info)
{
   float cr, cg, cb;
   unsigned w     = VIDEO_SCALE_W(dims);
   unsigned h     = VIDEO_SCALE_H(dims);
   unsigned shown = _dispvars->scn_set.height
      ? _dispvars->scn_set.height : (unsigned)_dispvars->sunxi_disp->yres;
   const char *stat = (info && info->statistics_show && info->stat_text_len)
      ? info->stat_text : NULL;

   if (     !_dispvars->font_enable || !info || !w || !h
         || (!(msg && *msg) && !stat))
      return;
   if (!sunxi_font(_dispvars,
            (unsigned)(info->font_size * (float)h / (float)shown + 0.5f)))
      return;

   cr = info->font_msg_color_r < 0.0f ? 0.0f
      : info->font_msg_color_r > 1.0f ? 1.0f : info->font_msg_color_r;
   cg = info->font_msg_color_g < 0.0f ? 0.0f
      : info->font_msg_color_g > 1.0f ? 1.0f : info->font_msg_color_g;
   cb = info->font_msg_color_b < 0.0f ? 0.0f
      : info->font_msg_color_b > 1.0f ? 1.0f : info->font_msg_color_b;

   if (msg && *msg)
      sunxi_draw_text(_dispvars, page, _dispvars->page_stride, w, h, msg,
            strlen(msg), (int)(info->font_msg_pos_x * w),
            (int)((1.0f - info->font_msg_pos_y) * h),
            (uint32_t)(cr * 255.0f), (uint32_t)(cg * 255.0f),
            (uint32_t)(cb * 255.0f));
   if (stat)
      sunxi_draw_text(_dispvars, page, _dispvars->page_stride, w, h, stat,
            info->stat_text_len, (int)(info->osd_stat_params.x * w),
            (int)((1.0f - info->osd_stat_params.y) * h),
            (uint32_t)(cr * 255.0f), (uint32_t)(cg * 255.0f),
            (uint32_t)(cb * 255.0f));
}

/* The frame into page @page as the pages hold it: turned by the CPU
 * when a rotation is set, converted to XRGB8888 by the NEON blit. A
 * frame the core drew into the page itself is there already. */
static void sunxi_draw_frame(struct sunxi_video *_dispvars,
      const void *frame, unsigned pitch, uint32_t *page)
{
   unsigned w   = VIDEO_SCALE_W(_dispvars->frame_dims);
   unsigned h   = VIDEO_SCALE_H(_dispvars->frame_dims);
   unsigned bpp = _dispvars->bytes_per_pixel;
   unsigned rot = _dispvars->page_rot;

   if ((const void*)page == frame)
      return;
   if (!rot)
   {
      _dispvars->blit((int)w, (int)h, page, (int)_dispvars->page_stride,
            (uint16_t*)frame, (int)(pitch / bpp));
      return;
   }
   /* XRGB8888 is turned straight into the page */
   if (bpp == 4)
   {
      video_turn_copy((uint8_t*)page, (size_t)_dispvars->page_stride * 4,
            (const uint8_t*)frame, pitch, w, h, 4, rot);
      return;
   }
   /* RGB565 turned aside, then blitted */
   if ((size_t)w * h * 2 > _dispvars->turned_size)
   {
      uint8_t *grown = (uint8_t*)realloc(_dispvars->turned,
            (size_t)w * h * 2);
      if (!grown)
         return;
      _dispvars->turned      = grown;
      _dispvars->turned_size = (size_t)w * h * 2;
   }
   video_turn_copy(_dispvars->turned,
         (size_t)VIDEO_SCALE_W(_dispvars->page_dims) * 2,
         (const uint8_t*)frame, pitch, w, h, 2, rot);
   _dispvars->blit((int)VIDEO_SCALE_W(_dispvars->page_dims),
         (int)VIDEO_SCALE_H(_dispvars->page_dims), page,
         (int)_dispvars->page_stride, (uint16_t*)_dispvars->turned,
         (int)VIDEO_SCALE_W(_dispvars->page_dims));
}

static void *sunxi_init(const video_info_t *video)
{
   settings_t *settings          = config_get_ptr();
   int fb_id                     = (settings
         && settings->uints.video_monitor_index == 2) ? 1 : 0;
   struct sunxi_video *_dispvars = (struct sunxi_video*)
      calloc(1, sizeof(struct sunxi_video));

   if (!_dispvars)
      return NULL;

   if (!(_dispvars->sunxi_disp = sunxi_disp_init(fb_id)))
   {
      free(_dispvars);
      return NULL;
   }

   _dispvars->bytes_per_pixel = video->rgb32 ? 4 : 2;
   _dispvars->blit            = video->rgb32
      ? pixman_composite_src_8888_8888_asm_neon
      : pixman_composite_src_0565_8888_asm_neon;
   _dispvars->keep_aspect     = video->force_aspect;
   _dispvars->nonblock        = !video->vsync;
   _dispvars->font_enable     = video->font_enable;
   if (video->path_font)
      strlcpy(_dispvars->font_path, video->path_font,
            sizeof(_dispvars->font_path));
   sunxi_full_viewport(_dispvars, &_dispvars->vp);

   if (!retro_eventcount_init(&_dispvars->vsync_ec))
   {
      sunxi_disp_close(_dispvars->sunxi_disp);
      free(_dispvars);
      return NULL;
   }

   /* Blank text console and disable cursor blinking. */
   sunxi_blank_console(_dispvars);

   /* What views are laid out on */
   video_driver_set_output_dims(VIDEO_SCALE_PACK(
            _dispvars->sunxi_disp->xres, _dispvars->sunxi_disp->yres));

   retro_atomic_store_release_int(&_dispvars->keep_vsync, 1);
   if (!(_dispvars->vsync_thread = sthread_create(sunxi_vsync_thread_func,
               _dispvars)))
      retro_atomic_store_release_int(&_dispvars->no_vsync, 1);

   return _dispvars;
}

static void sunxi_free(void *data)
{
   struct sunxi_video *_dispvars = (struct sunxi_video*)data;

   if (!_dispvars)
      return;

   /* The vsync thread ends at the next vsync */
   retro_atomic_store_release_int(&_dispvars->keep_vsync, 0);
   if (_dispvars->vsync_thread)
      sthread_join(_dispvars->vsync_thread);

   retro_eventcount_free(&_dispvars->vsync_ec);

   video_views_cpu_free(&_dispvars->views_cpu);
   free(_dispvars->views_canvas);
   free(_dispvars->turned);
   if (_dispvars->font)
      _dispvars->font_driver->free(_dispvars->font);

   /* Restore text console contents and reactivate cursor blinking. */
   sunxi_restore_console(_dispvars);

   sunxi_disp_close(_dispvars->sunxi_disp);
   free(_dispvars);
}

/* A core's views: laid out on the screen, composed at the scale that
 * draws no view larger than it is straight into the next page, which
 * the layer's scaler enlarges to the whole screen. Each view straight
 * from the frame. False when the frame is to be drawn whole. */
static bool sunxi_views_frame(struct sunxi_video *_dispvars,
      const void *frame, unsigned dims, unsigned pitch,
      const video_frame_info_t *info)
{
   float scale;
   unsigned cdims;
   video_viewport_t full;
   const video_views_layout_t *layout;
   bool views = info && frame && info->views.num_views
      && video_views_fit_frame(&info->views, dims);

   if (views)
   {
      layout = &info->views_layout;
      scale  = video_views_cpu_scale(&info->views, layout);
      cdims  = video_views_cpu_dims(info->dims, scale);
      if (layout->offscreen)
      {
         size_t size = (size_t)2 * VIDEO_SCALE_AREA(cdims) * sizeof(uint32_t);
         if (size > _dispvars->views_canvas_size)
         {
            uint32_t *grown = (uint32_t*)realloc(_dispvars->views_canvas,
                  size);
            if (grown)
            {
               _dispvars->views_canvas      = grown;
               _dispvars->views_canvas_size = size;
            }
            else
               views = false;
         }
      }
   }
   retro_atomic_store_release_int(&_dispvars->views_fallback,
         (info && info->views.num_views && !views) ? 1 : 0);
   if (!views)
   {
      /* The pages go back to the frame's */
      if (_dispvars->views_dims)
      {
         sunxi_wait_flip(_dispvars);
         _dispvars->views_dims = 0;
         _dispvars->frame_dims = 0;
      }
      return false;
   }

   /* No page is rearranged under a flip in flight */
   sunxi_wait_flip(_dispvars);
   if (_dispvars->views_dims != cdims)
   {
      if (!sunxi_setup_pages(_dispvars, cdims))
         return true;
      _dispvars->views_dims = cdims;
      _dispvars->frame_dims = 0;
   }
   if (!_dispvars->views_seen)
   {
      _dispvars->views_seen = true;
      RARCH_LOG("[Sunxi] Views drawn straight from the frame.\n");
   }

   /* Each view turned in its own place, as the layout laid them out */
   _dispvars->views_cpu.rot = video_driver_get_rotation_snapshot();
   video_views_cpu_compose(&_dispvars->views_cpu, &info->views, layout,
         frame, pitch,
         _dispvars->bytes_per_pixel == 4
         ? SCALER_FMT_ARGB8888 : SCALER_FMT_RGB565,
         info->video_smooth ? SCALER_TYPE_BILINEAR : SCALER_TYPE_POINT,
         scale, cdims, _dispvars->pages[_dispvars->next].address,
         _dispvars->page_stride,
         layout->offscreen ? _dispvars->views_canvas : NULL);

   sunxi_full_viewport(_dispvars, &full);
   sunxi_flip(_dispvars, _dispvars->pages[_dispvars->next].offset,
         _dispvars->page_stride, cdims, &full);
   _dispvars->shown = _dispvars->next;
   _dispvars->next  = (_dispvars->next + 1) % NUMPAGES;
   return true;
}

static bool sunxi_frame(void *data, const void *frame,
      unsigned dims, uint64_t frame_count, unsigned pitch, const char *msg,
      video_frame_info_t *video_info)
{
   unsigned pdims;
   uint32_t *page;
   struct sunxi_video *_dispvars = (struct sunxi_video*)data;
#ifdef HAVE_MENU
   bool menu_is_alive            = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif

#ifdef HAVE_MENU
   menu_driver_frame(menu_is_alive, video_info);
#endif

   /* The menu is shown in the frame's place: nothing to flip, paced on
    * the vsync */
   if (_dispvars->menu_active)
   {
      if (!_dispvars->nonblock)
         sunxi_wait_vsync(_dispvars);
      return true;
   }

   /* A dupe leaves the frame on the screen */
   if (!frame || !VIDEO_SCALE_W(dims) || !VIDEO_SCALE_H(dims))
      return true;

   /* Without vsync, a frame that meets a flip in flight is dropped */
   if (     _dispvars->nonblock
         && retro_atomic_load_acquire_int(&_dispvars->pageflip_pending))
      return true;

   if (sunxi_views_frame(_dispvars, frame, dims, pitch, video_info))
      return true;

   /* No page is written while it may still be on the screen */
   sunxi_wait_flip(_dispvars);

   if (     dims != _dispvars->frame_dims
         || _dispvars->rotation != _dispvars->page_rot)
   {
      _dispvars->frame_dims = dims;
      _dispvars->page_rot   = _dispvars->rotation;
      pdims = (_dispvars->page_rot & 1)
         ? VIDEO_SCALE_PACK(VIDEO_SCALE_H(dims), VIDEO_SCALE_W(dims)) : dims;
      sunxi_setup_pages(_dispvars, pdims);
   }
   if (!_dispvars->page_dims)
      return true;

   /* A frame the core rendered into the lent page is there already */
   page = _dispvars->pages[_dispvars->next].address;
   sunxi_draw_frame(_dispvars, frame, pitch, page);
   sunxi_draw_texts(_dispvars, page, _dispvars->page_dims, msg, video_info);

   sunxi_update_viewport(_dispvars);
   sunxi_flip(_dispvars, _dispvars->pages[_dispvars->next].offset,
         _dispvars->page_stride, _dispvars->page_dims, &_dispvars->vp);
   _dispvars->shown = _dispvars->next;
   _dispvars->next  = (_dispvars->next + 1) % NUMPAGES;
   return true;
}

static void sunxi_set_nonblock_state(void *data, bool toggle,
      bool adaptive_vsync_enabled, unsigned swap_interval)
{
   struct sunxi_video *_dispvars = (struct sunxi_video*)data;
   if (_dispvars)
      _dispvars->nonblock = toggle;
}
static bool sunxi_alive(void *data) { return true; }
/* Framebuffer device always has focus */
static bool sunxi_focus(void *data) { return true; }
static bool sunxi_suppress_screensaver(void *a, bool b) { return false; }

static void sunxi_viewport_info(void *data, struct video_viewport *vp)
{
   struct sunxi_video *_dispvars = (struct sunxi_video*)data;

   if (!vp || !_dispvars)
      return;

   /* The whole screen while views draw, else where the frame shows */
   if (_dispvars->views_dims)
      sunxi_full_viewport(_dispvars, vp);
   else
   {
      *vp           = _dispvars->vp;
      vp->full_dims = VIDEO_SCALE_PACK(_dispvars->sunxi_disp->xres,
            _dispvars->sunxi_disp->yres);
   }
}

/* What is on the screen, at the viewport's size: BGR24, bottom row
 * first, scaled from the menu's frame while it is up, else from the
 * page last flipped to */
static bool sunxi_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   unsigned x, y, vw, vh, pw, ph, stride;
   video_viewport_t vp;
   const uint32_t *page;
   struct sunxi_video *_dispvars = (struct sunxi_video*)data;
   bool menu;

   if (!_dispvars || !buffer)
      return false;
   menu = _dispvars->menu_active && _dispvars->menu_dims;
   if (!menu && !_dispvars->page_dims)
      return false;
   sunxi_viewport_info(_dispvars, &vp);
   vw     = VIDEO_SCALE_W(vp.dims);
   vh     = VIDEO_SCALE_H(vp.dims);
   pw     = VIDEO_SCALE_W(menu ? _dispvars->menu_dims : _dispvars->page_dims);
   ph     = VIDEO_SCALE_H(menu ? _dispvars->menu_dims : _dispvars->page_dims);
   page   = menu ? _dispvars->menu_page.address
      : _dispvars->pages[_dispvars->shown].address;
   stride = menu ? _dispvars->menu_stride : _dispvars->page_stride;
   if (!vw || !vh)
      return false;

   for (y = 0; y < vh; y++)
   {
      const uint32_t *row = page + (size_t)((uint64_t)(vh - 1 - y) * ph / vh)
         * stride;
      uint8_t *out        = buffer + (size_t)y * vw * 3;
      for (x = 0; x < vw; x++, out += 3)
      {
         uint32_t p = row[(uint64_t)x * pw / vw];
         out[0]     = (uint8_t)p;
         out[1]     = (uint8_t)(p >> 8);
         out[2]     = (uint8_t)(p >> 16);
      }
   }
   return true;
}

/* Taken up when the next frame comes, where the pages are laid out */
static void sunxi_set_rotation(void *data, unsigned rotation)
{
   struct sunxi_video *_dispvars = (struct sunxi_video*)data;
   if (_dispvars)
      _dispvars->rotation = rotation & 3;
}

static bool sunxi_set_shader(void *data,
      enum rarch_shader_type type, const char *path) { return false; }

static void sunxi_set_texture_enable(void *data, bool state, bool full_screen)
{
   struct sunxi_video *_dispvars = (struct sunxi_video*)data;

   if (!_dispvars)
      return;
   _dispvars->menu_active      = state;
   _dispvars->menu_full_screen = full_screen;
   /* The frame comes back over the whole of its window */
   if (!state)
      memset(&_dispvars->scn_set, 0, sizeof(_dispvars->scn_set));
}

/* The menu's frame, converted to XRGB8888 at the end of the
 * framebuffer, and shown in the frame's place */
static void sunxi_set_texture_frame(void *data, const void *frame, bool rgb32,
      unsigned dims, float alpha)
{
   unsigned x, y;
   uint64_t bytes;
   uint32_t floor_off;
   video_viewport_t vp;
   uint32_t *dst;
   struct sunxi_video *_dispvars = (struct sunxi_video*)data;
   sunxi_disp_t *disp;
   unsigned w = VIDEO_SCALE_W(dims);
   unsigned h = VIDEO_SCALE_H(dims);

   if (!_dispvars || !_dispvars->menu_active || !frame || !w || !h)
      return;
   disp = _dispvars->sunxi_disp;

   /* At the end of the framebuffer, past the pages */
   if (dims != _dispvars->menu_dims)
   {
      unsigned stride = (w + 15) & ~15u;
      bytes     = (uint64_t)stride * 4 * h;
      floor_off = _dispvars->page_dims
         ? _dispvars->pages[NUMPAGES - 1].offset
            + _dispvars->page_stride * 4 * VIDEO_SCALE_H(_dispvars->page_dims)
         : sunxi_pages_base(disp);
      _dispvars->menu_dims = 0;
      if (bytes > disp->framebuffer_size
            || ((disp->framebuffer_size - (uint32_t)bytes) & ~63u) < floor_off)
         return;
      _dispvars->menu_page.offset  = (disp->framebuffer_size
            - (uint32_t)bytes) & ~63u;
      _dispvars->menu_page.address = (uint32_t*)(disp->framebuffer_addr
            + _dispvars->menu_page.offset);
      _dispvars->menu_stride       = stride;
      _dispvars->menu_dims         = dims;
   }

   dst = _dispvars->menu_page.address;
   if (rgb32)
      for (y = 0; y < h; y++)
         memcpy(dst + (size_t)y * _dispvars->menu_stride,
               (const uint32_t*)frame + (size_t)y * w, (size_t)w * 4);
   else
   {
      /* RGUI's RGBA4444: each channel's nibble repeated to eight bits */
      for (y = 0; y < h; y++)
      {
         const uint16_t *src = (const uint16_t*)frame + (size_t)y * w;
         uint32_t *row       = dst + (size_t)y * _dispvars->menu_stride;
         for (x = 0; x < w; x++)
         {
            uint32_t p = src[x];
            uint32_t r = (p >> 12) & 0xf;
            uint32_t g = (p >> 8) & 0xf;
            uint32_t b = (p >> 4) & 0xf;
            row[x]     = ((r | (r << 4)) << 16) | ((g | (g << 4)) << 8)
               | (b | (b << 4));
         }
      }
   }

   /* Where the frame shows, or over the whole screen */
   if (_dispvars->menu_full_screen)
      sunxi_full_viewport(_dispvars, &vp);
   else
   {
      sunxi_update_viewport(_dispvars);
      vp = _dispvars->vp;
   }
   sunxi_flip(_dispvars, _dispvars->menu_page.offset,
         _dispvars->menu_stride, dims, &vp);
}

/* A frame for the core to render into: the page to be shown next, so
 * the frame is never copied. XRGB8888 only - a 16-bit frame is
 * converted on its way into the page - and not while the CPU turns it
 * or views are composed. The framebuffer is not read at cached speed,
 * so a core that reads its frame back is not given it. */
static bool sunxi_get_current_software_framebuffer(void *data,
      struct retro_framebuffer *fb)
{
   struct sunxi_video *_dispvars = (struct sunxi_video*)data;

   if (     !_dispvars || !fb || _dispvars->bytes_per_pixel != 4
         || _dispvars->rotation || _dispvars->page_rot
         || _dispvars->views_dims || _dispvars->menu_active
         || !_dispvars->page_dims
         || _dispvars->page_dims != VIDEO_SCALE_PACK(fb->width, fb->height)
         || (fb->access_flags & RETRO_MEMORY_ACCESS_READ))
      return false;
   /* Without vsync nothing waits: a flip in flight refuses the loan */
   if (_dispvars->nonblock)
   {
      if (retro_atomic_load_acquire_int(&_dispvars->pageflip_pending))
         return false;
   }
   else
      sunxi_wait_flip(_dispvars);

   fb->data         = _dispvars->pages[_dispvars->next].address;
   fb->pitch        = (size_t)_dispvars->page_stride * 4;
   fb->format       = RETRO_PIXEL_FORMAT_XRGB8888;
   fb->memory_flags = 0;
   return true;
}

/* The frontend has laid the new aspect out; the next frame takes it */
static void sunxi_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   struct sunxi_video *_dispvars = (struct sunxi_video*)data;
   if (_dispvars)
      _dispvars->keep_aspect = true;
}

static float sunxi_get_refresh_rate (void *data)
{
   struct sunxi_video *_dispvars = (struct sunxi_video*)data;

   return _dispvars->sunxi_disp->refresh_rate;
}

static uint32_t sunxi_get_flags(void *data)
{
   uint32_t flags                = 0;
   struct sunxi_video *_dispvars = (struct sunxi_video*)data;
   if (_dispvars)
   {
      BIT32_SET(flags, GFX_CTX_FLAGS_VIDEO_VIEWS);
      if (retro_atomic_load_acquire_int(&_dispvars->views_fallback))
         BIT32_SET(flags, GFX_CTX_FLAGS_VIDEO_VIEWS_FALLBACK);
   }
   return flags;
}

static const video_poke_interface_t sunxi_poke_interface = {
   sunxi_get_flags,
   NULL, /* load_texture */
   NULL, /* unload_texture */
   NULL, /* set_video_mode */
   sunxi_get_refresh_rate,
   NULL, /* set_filtering */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   sunxi_set_aspect_ratio,
   NULL, /* sunxi_apply_state_changes */
   sunxi_set_texture_frame,
   sunxi_set_texture_enable,
   NULL, /* set_osd_msg */
   NULL, /* show_mouse */
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   sunxi_get_current_software_framebuffer,
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_menu_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_expand_gamut */
   NULL, /* set_hdr_scanlines */
   NULL  /* set_hdr_subpixel_layout */
};

static void sunxi_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   *iface = &sunxi_poke_interface;
}

video_driver_t video_sunxi = {
   sunxi_init,
   sunxi_frame,
   sunxi_set_nonblock_state,
   sunxi_alive,
   sunxi_focus,
   sunxi_suppress_screensaver,
   NULL, /* has_windowed */
   sunxi_set_shader,
   sunxi_free,
   "sunxi",
   NULL, /* set_viewport */
   sunxi_set_rotation,
   sunxi_viewport_info,
   sunxi_read_viewport,
#ifdef HAVE_OVERLAY
   NULL, /* get_overlay_interface */
#endif
   sunxi_get_poke_interface,
   NULL, /* wrap_type_to_enum */
   NULL, /* shader_load_begin */
   NULL, /* shader_load_step */
#ifdef HAVE_GFX_WIDGETS
   NULL  /* gfx_widgets_enabled */
#endif
};
