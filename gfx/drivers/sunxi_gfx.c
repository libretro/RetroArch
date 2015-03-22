/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015 - Manuel Alfayate
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

#include "../../general.h"
#include "../../retroarch.h"
#include "../video_viewport.h"
#include "../video_monitor.h"
#include "../font_renderer_driver.h"

/* Specific includes for the driver */
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <signal.h>
#include <fcntl.h>
#include <rthreads/rthreads.h>

#define NUMPAGES 2

/* Lowlevel SunxiG2D functions block */
#define FBIOGET_LAYER_HDL_0 0x4700
#define FBIOGET_LAYER_HDL_1 0x4701
#define DISP_FORMAT_RGB565 0x5
#define DISP_FORMAT_ARGB8888 0xa
#define DISP_MOD_INTERLEAVED 0x1
#define DISP_SEQ_ARGB 0x0
#define DISP_SEQ_P10 0xd
#define DISP_BT601 = 0,
#define DISP_LAYER_WORK_MODE_NORMAL 0
#define DISP_LAYER_WORK_MODE_SCALER 4

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

typedef struct
{
   __s32 x;
   __s32 y;
} __disp_pos_t;

typedef enum tag_DISP_CMD
{
   /* ----layer---- */
   DISP_CMD_LAYER_REQUEST = 0x40,
   DISP_CMD_LAYER_RELEASE = 0x41,
   DISP_CMD_LAYER_OPEN = 0x42,
   DISP_CMD_LAYER_CLOSE = 0x43,
   DISP_CMD_LAYER_SET_FB = 0x44,
   DISP_CMD_LAYER_GET_FB = 0x45,
   DISP_CMD_LAYER_SET_SRC_WINDOW = 0x46,
   DISP_CMD_LAYER_GET_SRC_WINDOW = 0x47,
   DISP_CMD_LAYER_SET_SCN_WINDOW = 0x48,
   DISP_CMD_LAYER_GET_SCN_WINDOW = 0x49,
   DISP_CMD_LAYER_SET_PARA = 0x4a,
   DISP_CMD_LAYER_GET_PARA = 0x4b,
} __disp_cmd_t;

typedef struct
{
   int                 fd_fb;
   int                 fd_disp;
   int                 fb_id;             /* /dev/fb0 = 0, /dev/fb1 = 1 */

   int                 xres, yres, bits_per_pixel;
   uint8_t            *framebuffer_addr;  /* mmapped address */
   uintptr_t           framebuffer_paddr; /* physical address */
   uint32_t            framebuffer_size;  /* total size of the framebuffer */
   int                 framebuffer_height;/* virtual vertical resolution */
   uint32_t            gfx_layer_size;    /* the size of the primary layer */

   /* Layers support */
   int                 gfx_layer_id;
   int                 layer_id;
   int                 layer_has_scaler;

   int                 layer_buf_x, layer_buf_y, layer_buf_w, layer_buf_h;
   int                 layer_win_x, layer_win_y;
   int                 layer_scaler_is_enabled;
   int                 layer_format;
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
   unsigned int mode; /* layer work mode */
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
   signed char alpha_en; /* layer global alpha enable */
   __u16 alpha_val; /* layer global alpha value */
   signed char ck_enable; /* layer color key enable */
   /*  framebuffer source window,only care x,y if is not scaler mode */
   __disp_rect_t src_win;
   __disp_rect_t scn_win; /* screen window */
   __disp_fb_t fb; /* framebuffer */
   signed char b_trd_out; /* if output 3d mode, used for scaler mode layer */
   /* output 3d mode, used for scaler mode layer */
   unsigned int out_trd_mode;
} __disp_layer_info_t;

/*****************************************************************************
 * Support for scaled layers                                                 *
 *****************************************************************************/

static int sunxi_layer_change_work_mode(sunxi_disp_t *ctx, int new_mode)
{
   __disp_layer_info_t layer_info;
   uint32_t tmp[4];

   if (ctx->layer_id < 0)
      return -1;

   tmp[0] = ctx->fb_id;
   tmp[1] = ctx->layer_id;
   tmp[2] = (uintptr_t)&layer_info;

   if (ioctl(ctx->fd_disp, DISP_CMD_LAYER_GET_PARA, tmp) < 0)
      return -1;

   layer_info.mode = new_mode;

   tmp[0] = ctx->fb_id;
   tmp[1] = ctx->layer_id;
   tmp[2] = (uintptr_t)&layer_info;
   return ioctl(ctx->fd_disp, DISP_CMD_LAYER_SET_PARA, tmp);
}

static int sunxi_layer_reserve(sunxi_disp_t *ctx)
{
   __disp_layer_info_t layer_info;
   uint32_t tmp[4];

   /* try to allocate a layer */

   tmp[0] = ctx->fb_id;
   tmp[1] = DISP_LAYER_WORK_MODE_NORMAL;
   ctx->layer_id = ioctl(ctx->fd_disp, DISP_CMD_LAYER_REQUEST, &tmp);
   if (ctx->layer_id < 0)
      return -1;

   /* Initially set the layer configuration to something reasonable */

   tmp[0] = ctx->fb_id;
   tmp[1] = ctx->layer_id;
   tmp[2] = (uintptr_t)&layer_info;
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
   tmp[2] = (uintptr_t)&layer_info;
   if (ioctl(ctx->fd_disp, DISP_CMD_LAYER_SET_PARA, tmp) < 0)
      return -1;

   /* Now probe the scaler mode to see if there is a free scaler available */
   if (sunxi_layer_change_work_mode(ctx, DISP_LAYER_WORK_MODE_SCALER) == 0)
      ctx->layer_has_scaler = 1;

   /* Revert back to normal mode */
   sunxi_layer_change_work_mode(ctx, DISP_LAYER_WORK_MODE_NORMAL);
   ctx->layer_scaler_is_enabled = 0;
   ctx->layer_format = DISP_FORMAT_ARGB8888;

   return ctx->layer_id;
}

static int sunxi_layer_set_output_window(sunxi_disp_t *ctx, int x, int y, int w, int h)
{
   __disp_rect_t buf_rect = {
      ctx->layer_buf_x, ctx->layer_buf_y,
      ctx->layer_buf_w, ctx->layer_buf_h
   };
   __disp_rect_t win_rect = { x, y, w, h };
   uint32_t tmp[4];
   int err;

   if (ctx->layer_id < 0 || w <= 0 || h <= 0)
      return -1;

   /* Save the new non-adjusted window position */
   ctx->layer_win_x = x;
   ctx->layer_win_y = y;

   tmp[0] = ctx->fb_id;
   tmp[1] = ctx->layer_id;
   tmp[2] = (uintptr_t)&win_rect;
   return ioctl(ctx->fd_disp, DISP_CMD_LAYER_SET_SCN_WINDOW, &tmp);
}

static int sunxi_layer_show(sunxi_disp_t *ctx)
{
   uint32_t tmp[4];

   if (ctx->layer_id < 0)
      return -1;

   tmp[0] = ctx->fb_id;
   tmp[1] = ctx->layer_id;
   return ioctl(ctx->fd_disp, DISP_CMD_LAYER_OPEN, &tmp);
}

static int sunxi_layer_release(sunxi_disp_t *ctx)
{
   int result;
   uint32_t tmp[4];

   if (ctx->layer_id < 0)
      return -1;

   tmp[0] = ctx->fb_id;
   tmp[1] = ctx->layer_id;
   ioctl(ctx->fd_disp, DISP_CMD_LAYER_RELEASE, &tmp);

   ctx->layer_id = -1;
   ctx->layer_has_scaler = 0;
   return 0;
}


static int sunxi_layer_set_rgb_input_buffer(sunxi_disp_t *ctx,
   int           bpp,
   uint32_t      offset_in_framebuffer,
   int           width,
   int           height,
   int           stride)
{
   __disp_fb_t fb;
   __disp_rect_t rect = { 0, 0, width, height };
   uint32_t tmp[4];

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

   fb.addr[0] = ctx->framebuffer_paddr + offset_in_framebuffer;
   fb.size.height = height;

   if (bpp == 32)
   {
      fb.format = DISP_FORMAT_ARGB8888;
      fb.seq = DISP_SEQ_ARGB;
      fb.mode = DISP_MOD_INTERLEAVED;
      fb.size.width = stride;
   }
   else if (bpp == 16)
   {
      fb.format = DISP_FORMAT_RGB565;
      fb.seq = DISP_SEQ_P10;
      fb.mode = DISP_MOD_INTERLEAVED;
      fb.size.width = stride * 2;
   }
   else
      return -1;

   tmp[0] = ctx->fb_id;
   tmp[1] = ctx->layer_id;
   tmp[2] = (uintptr_t)&fb;

   if (ioctl(ctx->fd_disp, DISP_CMD_LAYER_SET_FB, &tmp) < 0)
      return -1;

   ctx->layer_buf_x = rect.x;
   ctx->layer_buf_y = rect.y;
   ctx->layer_buf_w = rect.width;
   ctx->layer_buf_h = rect.height;
   ctx->layer_format = fb.format;

   tmp[0] = ctx->fb_id;
   tmp[1] = ctx->layer_id;
   tmp[2] = (uintptr_t)&rect;

   return ioctl(ctx->fd_disp, DISP_CMD_LAYER_SET_SRC_WINDOW, &tmp);
}

static sunxi_disp_t *sunxi_disp_init(const char *device)
{
   int tmp, version;
   int gfx_layer_size;
   int ovl_layer_size;
   struct fb_var_screeninfo fb_var;
   struct fb_fix_screeninfo fb_fix;

   sunxi_disp_t *ctx = calloc(sizeof(sunxi_disp_t), 1);

   if (!ctx)
      return NULL;

   /* use /dev/fb0 by default */
   if (!device)
      device = "/dev/fb0";

   if (strcmp(device, "/dev/fb0") == 0)
      ctx->fb_id = 0;
   else if (strcmp(device, "/dev/fb1") == 0)
      ctx->fb_id = 1;
   else
   {
      free(ctx);
      return NULL;
   }

   ctx->fd_disp = open("/dev/disp", O_RDWR);

   /* maybe it's even not a sunxi hardware */
   if (ctx->fd_disp < 0)
   {
      free(ctx);
      return NULL;
   }

   ctx->fd_fb = open(device, O_RDWR);

   if (ctx->fd_fb < 0)
   {
      close(ctx->fd_disp);
      free(ctx);
      return NULL;
   }

   if (ioctl(ctx->fd_fb, FBIOGET_VSCREENINFO, &fb_var) < 0 ||
         ioctl(ctx->fd_fb, FBIOGET_FSCREENINFO, &fb_fix) < 0)
   {
      close(ctx->fd_fb);
      close(ctx->fd_disp);
      free(ctx);
      return NULL;
   }

   ctx->xres               = fb_var.xres;
   ctx->yres               = fb_var.yres;
   ctx->bits_per_pixel     = fb_var.bits_per_pixel;
   ctx->framebuffer_paddr  = fb_fix.smem_start;
   ctx->framebuffer_size   = fb_fix.smem_len;
   ctx->framebuffer_height = ctx->framebuffer_size /
      (ctx->xres * ctx->bits_per_pixel / 8);
   ctx->gfx_layer_size     = ctx->xres * ctx->yres * fb_var.bits_per_pixel / 8;

   if (ctx->framebuffer_size < ctx->gfx_layer_size)
   {
      close(ctx->fd_fb);
      close(ctx->fd_disp);
      free(ctx);
      return NULL;
   }

   /* mmap framebuffer memory */
   ctx->framebuffer_addr = (uint8_t *)mmap(0, ctx->framebuffer_size,
	 PROT_READ | PROT_WRITE,
	 MAP_SHARED, ctx->fd_fb, 0);

   if (ctx->framebuffer_addr == MAP_FAILED)
   {
      close(ctx->fd_fb);
      close(ctx->fd_disp);
      free(ctx);
      return NULL;
   }

   /* Get the id of the screen layer */
   if (ioctl(ctx->fd_fb,
            ctx->fb_id == 0 ? FBIOGET_LAYER_HDL_0 : FBIOGET_LAYER_HDL_1,
            &ctx->gfx_layer_id))
   {
      close(ctx->fd_fb);
      close(ctx->fd_disp);
      free(ctx);
      return NULL;
   }

   if (sunxi_layer_reserve(ctx) < 0)
   {
      close(ctx->fd_fb);
      close(ctx->fd_disp);
      free(ctx);
      return NULL;
   }

   return ctx;
}

static int sunxi_disp_close(sunxi_disp_t *ctx)
{
   if (ctx->fd_disp >= 0)
   {
      /* release layer */
      sunxi_layer_release(ctx);
      /* close descriptors */
      munmap(ctx->framebuffer_addr, ctx->framebuffer_size);
      close(ctx->fd_fb);
      close(ctx->fd_disp);
      ctx->fd_disp = -1;
      free(ctx);
   }
   return 0;
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

/* Pointer to the blitting function. Will be asigned 
 * when we find out what bpp the core uses. */
void (*pixman_blit) (int width,
   int height,
   uint32_t *dst,
   int dst_stride_pixels,
   uint16_t *src,
   int src_stride_pixels);

extern void *memcpy_neon(void *dst, const void *src, size_t n);

sunxi_disp_t *disp;

struct sunxi_page
{
   unsigned yoffset;
};

struct sunxi_video
{
   void *font;
   const font_renderer_driver_t *font_driver;

   uint8_t font_rgb[4];

   /* current dimensions of the emulator fb */
   unsigned src_width;
   unsigned src_height;
   unsigned src_pitch;
   unsigned src_bpp;
   unsigned src_bytes_per_pixel;
   unsigned dst_pitch;
   unsigned visible_width;	
   unsigned bytes_per_pixel;	

   struct sunxi_page *pages;	
   struct sunxi_page *nextPage;	
   bool pageflip_pending;

   /* Keep the vsync while loop going. Set to false to exit. */
   bool keep_vsync;

   /* Variables to restore screen on exit */
   unsigned screensize;
   char *screen_bck;

   /* For threading */
   sthread_t *vsync_thread;
   scond_t *vsync_condition;	
   slock_t *pending_mutex;

   /* menu data */
   unsigned menu_rotation;
   bool menu_active;

   bool aspect_changed;
};

static void sunxi_blank_console(struct sunxi_video *_dispvars)
{
   if (!_dispvars)
      return;

   /* Disable cursor blinking so it's not visible. */
   system("setterm -cursor off");

   /* Figure out the size of the screen in bytes. */
   _dispvars->screensize = disp->xres * disp->yres * disp->bits_per_pixel / 8;

   /* Backup screen contents. */
   _dispvars->screen_bck = (char*)malloc(_dispvars->screensize * sizeof(char));

   if (!_dispvars->screen_bck)
      return;

   memcpy((char*)_dispvars->screen_bck, (char*)disp->framebuffer_addr, _dispvars->screensize);

   /* Blank screen. */
   memset((char*)(disp->framebuffer_addr), 0x00, _dispvars->screensize);
}

static void sunxi_unblank_console(struct sunxi_video *_dispvars)
{
   if (!_dispvars)
      return;

   system("setterm -cursor on");

#if 0
   memcpy((char*)disp->framebuffer_addr, (char*)_dispvars->screen_bck, _dispvars->screensize);
#endif

   free(_dispvars->screen_bck);
}

static void vsync_thread_func(void *data)
{
   struct sunxi_page *page;
   struct sunxi_video *_dispvars = data;

   while (_dispvars->keep_vsync)
   {
      /* Wait for next vsync */
      ioctl(disp->fd_fb, FBIO_WAITFORVSYNC, 0);

      /* Changing the page to write must be done before the signaling
       * so we have the right page in nextPage when update_main continues */
      if (_dispvars->nextPage == &_dispvars->pages[0])
         _dispvars->nextPage = &_dispvars->pages[1];
      else
         _dispvars->nextPage = &_dispvars->pages[0];

      /* These two things must be isolated "atomically" to avoid getting
       * a false positive in the pending_mutex test in update_main. */
      slock_lock(_dispvars->pending_mutex);
      _dispvars->pageflip_pending = false;
      scond_signal(_dispvars->vsync_condition);
      slock_unlock(_dispvars->pending_mutex);
   }	
}

static void *sunxi_gfx_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   int i;
   struct sunxi_video *_dispvars = (struct sunxi_video*)
   calloc(1, sizeof(struct sunxi_video));

   if (!_dispvars)
      return NULL;

   _dispvars->src_bytes_per_pixel = video->rgb32 ? 4 : 2;

   disp = sunxi_disp_init("/dev/fb0");

   /* Blank text console and disable cursor blinking. */
   sunxi_blank_console(_dispvars);
   
   _dispvars->pages = calloc(NUMPAGES, sizeof (struct sunxi_page));

   if (!_dispvars->pages)
   {
      free(_dispvars);
      return NULL;
   }
   
   _dispvars->dst_pitch = disp->xres * disp->bits_per_pixel / 8;
   _dispvars->pageflip_pending = false;
   _dispvars->nextPage = &_dispvars->pages[0];
   _dispvars->keep_vsync = true;

   _dispvars->src_bpp = video->rgb32 ? 32 : 16;	
   _dispvars->bytes_per_pixel = _dispvars->src_bpp / 8;

   switch (_dispvars->src_bpp)
   {
       case 16:
	  pixman_blit = pixman_composite_src_0565_8888_asm_neon;
	  break;
       case 32:
	  pixman_blit = pixman_composite_src_8888_8888_asm_neon;
	  break;
       default:
	  return NULL;
   }

   _dispvars->pending_mutex    = slock_new();
   _dispvars->vsync_condition  = scond_new();
   
   if (input && input_data)
	   *input = NULL;
   
   /* Launching vsync thread */
   _dispvars->vsync_thread = sthread_create(vsync_thread_func, _dispvars);

   return _dispvars;
}

static void sunxi_gfx_free(void *data)
{
   int i;
   struct sunxi_video *_dispvars = data;

   /* Stop the vsync thread and wait for it to join. */
   _dispvars->keep_vsync = false;
   sthread_join(_dispvars->vsync_thread);

   slock_free(_dispvars->pending_mutex);		
   scond_free(_dispvars->vsync_condition);

   free(_dispvars->pages);	

   /* Restore text console contents and reactivate cursor blinking. */
   sunxi_unblank_console(_dispvars);
   sunxi_disp_close(disp);
   free(_dispvars);
}

static void sunxi_update_main(const void *frame, struct sunxi_video *_dispvars)
{
   slock_lock(_dispvars->pending_mutex);
   if (_dispvars->pageflip_pending)
   {
      scond_wait(_dispvars->vsync_condition, _dispvars->pending_mutex);
   }
   slock_unlock(_dispvars->pending_mutex);

   /* Frame blitting */
   pixman_blit(
      _dispvars->src_width,
      _dispvars->src_height,
      ((uint32_t*) disp->framebuffer_addr + (disp->yres + _dispvars->nextPage->yoffset) * _dispvars->dst_pitch/4),
      _dispvars->dst_pitch/4,
      (uint16_t*)frame,
      _dispvars->src_pitch/_dispvars->bytes_per_pixel
      );

   /* Issue pageflip. Will flip on next vsync. */
   sunxi_layer_set_rgb_input_buffer(disp, disp->bits_per_pixel, 
      (disp->yres + _dispvars->nextPage->yoffset) * disp->xres * 4, 
      _dispvars->src_width, _dispvars->src_height, disp->xres);

   slock_lock(_dispvars->pending_mutex);
   _dispvars->pageflip_pending = true;
   slock_unlock(_dispvars->pending_mutex);
}

static bool sunxi_gfx_frame(void *data, const void *frame, unsigned width,
                             unsigned height, unsigned pitch, const char *msg)
{
   struct sunxi_video *_dispvars = data;
   struct sunxi_page *page = NULL;

   if (_dispvars->src_width != width || _dispvars->src_height != height)
   {
      int i;
      float aspect;
      unsigned inc_yoffset, xpos, visible_width;
      settings_t *settings = config_get_ptr();

      /* Sanity check on new dimensions */
      if (width == 0 || height == 0)
         return true;

      RARCH_LOG("video_sunxi: internal resolution changed by core: %ux%u -> %ux%u\n",
         _dispvars->src_width, _dispvars->src_height, width, height);

      _dispvars->src_width  = width;
      _dispvars->src_height = height;

      /* Total pitch, including things the 
       * cores render between "visible" scanlines. */
      _dispvars->src_pitch  = pitch;	

      /* Incremental offset that sums up on 
       * each previous page offset.
       * Total offset of each page has to 
       * be adjusted when internal resolution changes. */
      inc_yoffset = _dispvars->src_height;

      for (i = 0; i < NUMPAGES; i++)
         _dispvars->pages[i].yoffset = i * inc_yoffset;

      switch (settings->video.aspect_ratio_idx)
      {
         case ASPECT_RATIO_4_3:
            aspect = (float)4 / (float)3;
            break;
         case ASPECT_RATIO_16_9:
            aspect = (float)16 / (float)9;
            break;
         case ASPECT_RATIO_16_10: 
            aspect = (float)16 / (float)10;
            break;
         case ASPECT_RATIO_16_15: 
            aspect = (float)16 / (float)15;
            break;
         case ASPECT_RATIO_CORE: 
            aspect = (float)_dispvars->src_width / (float)_dispvars->src_height;
            break;
         default: 
            aspect = (float)_dispvars->src_width / (float)_dispvars->src_height;
            break;
      }        

      visible_width = disp->yres * aspect;
      xpos = (disp->xres - visible_width) / 2; 

      /* setup layer window */
      sunxi_layer_set_output_window(disp, xpos, 0, visible_width, disp->yres);

      /* make the layer visible */
      sunxi_layer_show(disp);
   }

   sunxi_update_main(frame, _dispvars);

   return true;
}

static void sunxi_gfx_set_nonblock_state(void *data, bool state)
{
   struct sunxi_video *vid = data;

   (void)vid;
   (void)state;

#if 0
   vid->data->sync = !state;
#endif
}

static bool sunxi_gfx_alive(void *data)
{
   (void)data;
   return true; /* always alive */
}

static bool sunxi_gfx_focus(void *data)
{
   (void)data;
   return true; /* fb device always has focus */
}

static void sunxi_gfx_set_rotation(void *data, unsigned rotation)
{
   (void)data;
   (void)rotation;
}

static bool sunxi_gfx_has_windowed(void *data)
{
   (void)data;

   return false;
}

static bool sunxi_gfx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;

   return false;
}

static void sunxi_gfx_viewport_info(void *data, struct video_viewport *vp)
{
   struct sunxi_video *_dispvars = data;

   if (!vp || !_dispvars)
      return;

   vp->x = vp->y = 0;

   vp->width  = vp->full_width  = _dispvars->src_width;
   vp->height = vp->full_height = _dispvars->src_height;
}

static bool sunxi_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false; 
}

static const video_poke_interface_t sunxi_poke_interface = {
  NULL, /* set_video_mode */
  NULL, /* set_filtering */
  NULL, /* get_video_output_size */
  NULL, /* get_video_output_prev */
  NULL, /* get_video_output_next */
#ifdef HAVE_FBO
  NULL, /* get_current_framebuffer */
#endif
  NULL, /* get_proc_address */
  //sunxi_set_aspect_ratio,
  //sunxi_apply_state_changes,
#ifdef HAVE_MENU
  //sunxi_set_texture_frame,
  //sunxi_set_texture_enable,
#endif
  //sunxi_set_osd_msg,
  //sunxi_show_mouse
};

static void sunxi_gfx_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &sunxi_poke_interface;
}

video_driver_t video_sunxi = {
  sunxi_gfx_init,
  sunxi_gfx_frame,
  sunxi_gfx_set_nonblock_state,
  sunxi_gfx_alive,
  sunxi_gfx_focus,
  sunxi_gfx_suppress_screensaver,
  sunxi_gfx_has_windowed,
  sunxi_gfx_set_shader,
  sunxi_gfx_free,
  "sunxi",
  sunxi_gfx_set_rotation,
  sunxi_gfx_viewport_info,
  NULL, /* read_viewport */
  NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
  NULL, /* overlay_interface */
#endif
  sunxi_gfx_get_poke_interface
};
