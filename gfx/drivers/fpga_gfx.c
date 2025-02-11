/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2017 - Brad Parker
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

#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../../driver.h"
#include "../../configuration.h"
#include "../../verbosity.h"

#define NUMBER_OF_WRITE_FRAMES 1 /* XPAR_AXIVDMA_0_NUM_FSTORES */
#define STORAGE_SIZE NUMBER_OF_WRITE_FRAMES * ((1920*1080)<<2)
#define FRAME_SIZE (STORAGE_SIZE / NUMBER_OF_WRITE_FRAMES)

#define FB_WIDTH  1920
#define FB_HEIGHT 1080

typedef struct RegOp
{
   void *ptr;
   int fd;
   int only_mmap;
   int only_munmap;
} RegOp;

typedef struct fpga
{
   RegOp regOp; /* ptr alignment */
   volatile unsigned *framebuffer;
   unsigned char *menu_frame;
   unsigned menu_width;
   unsigned menu_height;
   unsigned menu_pitch;
   unsigned video_width;
   unsigned video_height;
   unsigned video_pitch;
   unsigned video_bits;
   unsigned menu_bits;
   bool rgb32;
} fpga_t;

static unsigned int get_memory_size(void)
{
   unsigned int size;

   /* this file holds the memory range needed to map the framebuffer into
    * kernel address space, it is specified in the device tree
    */
   FILE *size_fp = fopen("/sys/class/uio/uio0/maps/map0/size", "r");

   if (!size_fp)
   {
      RARCH_ERR("unable to open the uio size file\n");
      exit(1);
   }

   fscanf(size_fp, "0x%08X", &size);
   fclose(size_fp);

   return size;
}

static void do_mmap_op(RegOp *regOp)
{
   if (regOp->only_munmap == 0)
   {
      regOp->fd = open("/dev/uio0", O_RDWR);

      if (regOp->fd < 1)
         return;

      regOp->ptr = mmap(NULL, get_memory_size(),
            PROT_READ|PROT_WRITE, MAP_SHARED, regOp->fd, 0);

      if (regOp->ptr == MAP_FAILED)
      {
         RARCH_ERR("could not mmap() memory\n");
         exit(1);
      }
   }

   if (regOp->only_mmap == 0)
   {
      if (munmap(regOp->ptr, get_memory_size()) == -1)
      {
         RARCH_ERR("could not munmap() memory\n");
         exit(1);
      }

      close(regOp->fd);
   }

   return;
}

static void fpga_create(fpga_t *fpga)
{
   memset(&fpga->regOp, 0, sizeof(fpga->regOp));

   fpga->regOp.only_mmap = 1;

   do_mmap_op(&fpga->regOp);

   fpga->framebuffer = ((volatile unsigned*)fpga->regOp.ptr);
}

static void *fpga_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   fpga_t *fpga                         = (fpga_t*)calloc(1, sizeof(*fpga));

   *input                               = NULL;
   *input_data                          = NULL;

   fpga->video_width                    = video->width;
   fpga->video_height                   = video->height;
   fpga->rgb32                          = video->rgb32;

   fpga->video_bits                     = video->rgb32 ? 32 : 16;

   if (video->rgb32)
      fpga->video_pitch = video->width * 4;
   else
      fpga->video_pitch = video->width * 2;

   fpga_create(fpga);

   return fpga;

error:
   if (fpga)
      free(fpga);
   return NULL;
}

static bool fpga_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   const void *frame_to_copy = frame;
   unsigned width            = 0;
   unsigned height           = 0;
   bool draw                 = true;
   fpga_t *fpga              = (fpga_t*)data;
   unsigned bits             = fpga->video_bits;
#ifdef HAVE_MENU
   bool menu_is_alive = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif

   if (!frame || !frame_width || !frame_height)
      return true;

#ifdef HAVE_MENU
   menu_driver_frame(menu_is_alive, video_info);
#endif

   if (     (fpga->video_width  != frame_width)
         || (fpga->video_height != frame_height)
         || (fpga->video_pitch  != pitch))
   {
      if (frame_width > 4 && frame_height > 4)
      {
         fpga->video_width = frame_width;
         fpga->video_height = frame_height;
         fpga->video_pitch = pitch;
      }
   }

#ifdef HAVE_MENU
   if (fpga->menu_frame && menu_is_alive)
   {
      frame_to_copy = fpga->menu_frame;
      width         = fpga->menu_width;
      height        = fpga->menu_height;
      pitch         = fpga->menu_pitch;
      bits          = fpga->menu_bits;
   }
   else
#endif
   {
      width         = fpga->video_width;
      height        = fpga->video_height;
      pitch         = fpga->video_pitch;

      if (frame_width == 4 && frame_height == 4 && (frame_width < width && frame_height < height))
         draw = false;

#ifdef HAVE_MENU
      if (menu_is_alive)
         draw = false;
#endif
   }

   if (draw)
   {
      if (bits == 16)
      {
         if (frame_to_copy == fpga->menu_frame)
         {
            /* RGBX4444 color bits for RGUI */
            unsigned x, y;

            for (y = 0; y < FB_HEIGHT; y++)
            {
               for (x = 0; x < FB_WIDTH; x++)
               {
                  /* scale incoming frame to fit the screen */
                  unsigned scaled_x    = (width * x) / FB_WIDTH;
                  unsigned scaled_y    = (height * y) / FB_HEIGHT;
                  unsigned short pixel = ((unsigned short*)frame_to_copy)[width * scaled_y + scaled_x];

                  /* convert RGBX444 to XRGB8888 */
                  unsigned r = ((pixel & 0xF000) >> 12);
                  unsigned g = ((pixel & 0x0F00) >> 8);
                  unsigned b = ((pixel & 0x00F0) >> 4);

                  fpga->framebuffer[FB_WIDTH * y + x] = (r << 20) | (b << 12) | (g << 4);
               }
            }
         }
         else
         {
            /* RGB565 color bits for core */
            unsigned x, y;

            for (y = 0; y < FB_HEIGHT; y++)
            {
               for (x = 0; x < FB_WIDTH; x++)
               {
                  /* scale incoming frame to fit the screen */
                  unsigned scaled_x    = (width * x) / FB_WIDTH;
                  unsigned scaled_y    = (height * y) / FB_HEIGHT;
                  unsigned short pixel = ((unsigned short*)frame_to_copy)[width * scaled_y + scaled_x];

                  /* convert RGB565 to XRBG8888 */
                  unsigned r = ((pixel & 0xF800) >> 11);
                  unsigned g = ((pixel & 0x07E0) >> 5);
                  unsigned b = ((pixel & 0x001F) >> 0);

                  fpga->framebuffer[FB_WIDTH * y + x] = (r << 19) | (b << 11) | (g << 2);
               }
            }
         }
      }
#if 0
      else
      {
         /* TODO/FIXME: handle 32-bit core output */
      }
#endif
   }

   return true;
}

static void fpga_set_nonblock_state(void *a, bool b, bool c, unsigned d) { }
static bool fpga_alive(void *data) { return true; }
static bool fpga_focus(void *data) { return true; }
static bool fpga_suppress_screensaver(void *data, bool enable) { return false; }
static bool fpga_has_windowed(void *data) { return true; }

static void fpga_free(void *data)
{
   fpga_t *fpga = (fpga_t*)data;

   if (!fpga)
      return;

   if (fpga->menu_frame)
      free(fpga->menu_frame);
   fpga->menu_frame = NULL;

   free(fpga);

   fpga->regOp.only_mmap = 0;
   fpga->regOp.only_munmap = 1;

   do_mmap_op(&fpga->regOp);
}

/* TODO/FIXME - implement */
static bool fpga_set_shader(void *data,
      enum rarch_shader_type type, const char *path) { return false; }
static void fpga_set_rotation(void *data,
      unsigned rotation) { }

static void fpga_set_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   fpga_t *fpga   = (fpga_t*)data;
   unsigned pitch = width * 2;

   if (fpga->rgb32)
      pitch = width * 4;

   if (fpga->menu_frame)
      free(fpga->menu_frame);
   fpga->menu_frame = NULL;

   if (  !fpga->menu_frame           ||
         fpga->menu_width  != width  ||
         fpga->menu_height != height ||
         fpga->menu_pitch != pitch)
      if (pitch && height)
         fpga->menu_frame = (unsigned char*)malloc(pitch * height);

   if (fpga->menu_frame && frame && pitch && height)
   {
      memcpy(fpga->menu_frame, frame, pitch * height);
      fpga->menu_width  = width;
      fpga->menu_height = height;
      fpga->menu_pitch  = pitch;
      fpga->menu_bits   = fpga->rgb32 ? 32 : 16;
   }
}

/* TODO/FIXME - implement */
static void fpga_set_osd_msg(void *data, const char *msg,
      const struct font_params *params, void *font) { }
static void fpga_get_video_output_size(void *data,
      unsigned *width, unsigned *height, char *desc, size_t desc_len) { }
static void fpga_get_video_output_prev(void *data) { }
static void fpga_get_video_output_next(void *data) { }
static void fpga_set_video_mode(void *data, unsigned width, unsigned height,
      bool fullscreen) { }

static const video_poke_interface_t fpga_poke_interface = {
   NULL, /* get_flags */
   NULL, /* load_texture */
   NULL, /* unload_texture */
   fpga_set_video_mode,
   NULL, /* get_refresh_rate */
   NULL, /* set_filtering */
   fpga_get_video_output_size,
   fpga_get_video_output_prev,
   fpga_get_video_output_next,
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   NULL, /* set_aspect_ratio */
   NULL, /* apply_state_changes */
#ifdef HAVE_MENU
   fpga_set_texture_frame,
   NULL, /* set_texture_enable */
   fpga_set_osd_msg,
   NULL, /* show_mouse */
#else
   NULL, /* set_texture_frame */
   NULL, /* set_texture_enable */
   NULL, /* set_osd_msg */
   NULL, /* show_mouse */
#endif
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_max_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_contrast */
   NULL  /* set_hdr_expand_gamut */
};

static void fpga_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &fpga_poke_interface;
}

/* TODO/FIXME - implement */
static void fpga_set_viewport(void *data, unsigned vp_width,
      unsigned vp_height, bool force_full, bool allow_rotate) { }

video_driver_t video_fpga = {
   fpga_init,
   fpga_frame,
   fpga_set_nonblock_state,
   fpga_alive,
   fpga_focus,
   fpga_suppress_screensaver,
   fpga_has_windowed,
   fpga_set_shader,
   fpga_free,
   "fpga",
   fpga_set_viewport,
   fpga_set_rotation,
   NULL, /* viewport_info */
   NULL, /* read_viewport */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   NULL, /* get_overlay_interface */
#endif
   fpga_get_poke_interface,
   NULL, /* wrap_type_to_enum */
#ifdef HAVE_GFX_WIDGETS
   NULL  /* gfx_widgets_enabled */
#endif
};
