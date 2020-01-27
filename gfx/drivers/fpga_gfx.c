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

#include "../font_driver.h"

#include "../../driver.h"
#include "../../configuration.h"
#include "../../verbosity.h"
#include "../common/fpga_common.h"

typedef struct RegOp
{
   int fd;
   void *ptr;
   int only_mmap;
   int only_munmap;
} RegOp;

static unsigned char *fpga_menu_frame = NULL;
static unsigned fpga_menu_width       = 0;
static unsigned fpga_menu_height      = 0;
static unsigned fpga_menu_pitch       = 0;
static unsigned fpga_video_width      = 0;
static unsigned fpga_video_height     = 0;
static unsigned fpga_video_pitch      = 0;
static unsigned fpga_video_bits       = 0;
static unsigned fpga_menu_bits        = 0;
static bool fpga_rgb32                = false;
static bool fpga_menu_rgb32           = false;
static RegOp regOp;

static unsigned int get_memory_size(void)
{
   FILE *size_fp;
   unsigned int size;

   /* this file holds the memory range needed to map the framebuffer into
    * kernel address space, it is specified in the device tree
    */
   size_fp = fopen("/sys/class/uio/uio0/maps/map0/size", "r");

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

      regOp->ptr = mmap(NULL, get_memory_size(), PROT_READ|PROT_WRITE, MAP_SHARED, regOp->fd, 0);

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

static void fpga_gfx_create(fpga_t *fpga)
{
   memset(&regOp, 0, sizeof(regOp));

   regOp.only_mmap = 1;

   do_mmap_op(&regOp);

   fpga->framebuffer = ((volatile unsigned*)regOp.ptr);
}

static void *fpga_gfx_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   unsigned full_x, full_y;
   gfx_ctx_input_t inp;
   gfx_ctx_mode_t mode;
   const gfx_ctx_driver_t *ctx_driver   = NULL;
   unsigned win_width = 0, win_height   = 0;
   unsigned temp_width = 0, temp_height = 0;
   settings_t *settings                 = config_get_ptr();
   fpga_t *fpga                         = (fpga_t*)calloc(1, sizeof(*fpga));

   *input                               = NULL;
   *input_data                          = NULL;

   fpga_video_width                      = video->width;
   fpga_video_height                     = video->height;
   fpga_rgb32                            = video->rgb32;

   fpga_video_bits                       = video->rgb32 ? 32 : 16;

   if (video->rgb32)
      fpga_video_pitch = video->width * 4;
   else
      fpga_video_pitch = video->width * 2;

   fpga_gfx_create(fpga);

   ctx_driver = video_context_driver_init_first(fpga,
         settings->arrays.video_context_driver,
         GFX_CTX_FPGA_API, 1, 0, false);
   if (!ctx_driver)
      goto error;

   video_context_driver_set((const gfx_ctx_driver_t*)ctx_driver);

   RARCH_LOG("[FPGA]: Found FPGA context: %s\n", ctx_driver->ident);

   video_context_driver_get_video_size(&mode);

   full_x      = mode.width;
   full_y      = mode.height;
   mode.width  = 0;
   mode.height = 0;

   RARCH_LOG("[FPGA]: Detecting screen resolution %ux%u.\n", full_x, full_y);

   win_width   = video->width;
   win_height  = video->height;

   if (video->fullscreen && (win_width == 0) && (win_height == 0))
   {
      win_width  = full_x;
      win_height = full_y;
   }

   mode.width      = win_width;
   mode.height     = win_height;
   mode.fullscreen = video->fullscreen;

   if (!video_context_driver_set_video_mode(&mode))
      goto error;

   mode.width     = 0;
   mode.height    = 0;

   video_context_driver_get_video_size(&mode);

   temp_width     = mode.width;
   temp_height    = mode.height;
   mode.width     = 0;
   mode.height    = 0;

   /* Get real known video size, which might have been altered by context. */

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(&temp_width, &temp_height);

   video_driver_get_size(&temp_width, &temp_height);

   RARCH_LOG("[FPGA]: Using resolution %ux%u\n", temp_width, temp_height);

   inp.input      = input;
   inp.input_data = input_data;

   video_context_driver_input_driver(&inp);

   if (settings->bools.video_font_enable)
      font_driver_init_osd(NULL, false,
            video->is_threaded,
            FONT_DRIVER_RENDER_FPGA);

   RARCH_LOG("[FPGA]: Init complete.\n");

   return fpga;

error:
   video_context_driver_destroy();
   if (fpga)
      free(fpga);
   return NULL;
}

static bool fpga_gfx_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   gfx_ctx_mode_t mode;
   const void *frame_to_copy = frame;
   unsigned width            = 0;
   unsigned height           = 0;
   unsigned bits             = fpga_video_bits;
   bool draw                 = true;
   fpga_t *fpga                = (fpga_t*)data;

   if (!frame || !frame_width || !frame_height)
      return true;

#ifdef HAVE_MENU
   menu_driver_frame(video_info);
#endif

   if (fpga_video_width != frame_width || fpga_video_height != frame_height || fpga_video_pitch != pitch)
   {
      if (frame_width > 4 && frame_height > 4)
      {
         fpga_video_width = frame_width;
         fpga_video_height = frame_height;
         fpga_video_pitch = pitch;
      }
   }

   if (fpga_menu_frame && video_info->menu_is_alive)
   {
      frame_to_copy = fpga_menu_frame;
      width         = fpga_menu_width;
      height        = fpga_menu_height;
      pitch         = fpga_menu_pitch;
      bits          = fpga_menu_bits;
   }
   else
   {
      width         = fpga_video_width;
      height        = fpga_video_height;
      pitch         = fpga_video_pitch;

      if (frame_width == 4 && frame_height == 4 && (frame_width < width && frame_height < height))
         draw = false;

      if (video_info->menu_is_alive)
         draw = false;
   }

   video_context_driver_get_video_size(&mode);

   if (draw)
   {
      if (bits == 16)
      {
         if (frame_to_copy == fpga_menu_frame)
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
      else
      {
         /* TODO/FIXME: handle 32-bit core output */
      }
   }

   if (msg)
      font_driver_render_msg(fpga, video_info, msg, NULL, NULL);

   return true;
}

static void fpga_gfx_set_nonblock_state(void *data, bool toggle)
{
   (void)data;
   (void)toggle;
}

static bool fpga_gfx_alive(void *data)
{
   gfx_ctx_size_t size_data;
   unsigned temp_width  = 0;
   unsigned temp_height = 0;
   bool quit            = false;
   bool resize          = false;
 
   /* Needed because some context drivers don't track their sizes */
   video_driver_get_size(&temp_width, &temp_height);

   size_data.quit       = &quit;
   size_data.resize     = &resize;
   size_data.width      = &temp_width;
   size_data.height     = &temp_height;

   video_context_driver_check_window(&size_data);

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(&temp_width, &temp_height);

   return true;
}

static bool fpga_gfx_focus(void *data)
{
   (void)data;
   return true;
}

static bool fpga_gfx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool fpga_gfx_has_windowed(void *data)
{
   (void)data;
   return true;
}

static void fpga_gfx_free(void *data)
{
   fpga_t *fpga = (fpga_t*)data;

   if (fpga_menu_frame)
   {
      free(fpga_menu_frame);
      fpga_menu_frame = NULL;
   }

   if (!fpga)
      return;

   font_driver_free_osd();
   video_context_driver_free();

   free(fpga);

   regOp.only_mmap = 0;
   regOp.only_munmap = 1;

   do_mmap_op(&regOp);
}

static bool fpga_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

static void fpga_gfx_set_rotation(void *data,
      unsigned rotation)
{
   (void)data;
   (void)rotation;
}

static void fpga_gfx_viewport_info(void *data,
      struct video_viewport *vp)
{
   (void)data;
   (void)vp;
}

static bool fpga_gfx_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   (void)data;
   (void)buffer;

   return true;
}

static void fpga_set_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   unsigned pitch = width * 2;

   if (rgb32)
      pitch = width * 4;

   if (fpga_menu_frame)
   {
      free(fpga_menu_frame);
      fpga_menu_frame = NULL;
   }

   if (!fpga_menu_frame || fpga_menu_width != width || fpga_menu_height != height || fpga_menu_pitch != pitch)
      if (pitch && height)
         fpga_menu_frame = (unsigned char*)malloc(pitch * height);

   if (fpga_menu_frame && frame && pitch && height)
   {
      memcpy(fpga_menu_frame, frame, pitch * height);
      fpga_menu_width  = width;
      fpga_menu_height = height;
      fpga_menu_pitch  = pitch;
      fpga_menu_bits   = rgb32 ? 32 : 16;
   }
}

static void fpga_set_osd_msg(void *data, 
      video_frame_info_t *video_info,
      const char *msg,
      const void *params, void *font)
{
   font_driver_render_msg(data, video_info, msg, params, font);
}

static void fpga_get_video_output_size(void *data,
      unsigned *width, unsigned *height)
{
   gfx_ctx_size_t size_data;
   size_data.width  = width;
   size_data.height = height;
   video_context_driver_get_video_output_size(&size_data);
}

static void fpga_get_video_output_prev(void *data)
{
   video_context_driver_get_video_output_prev();
}

static void fpga_get_video_output_next(void *data)
{
   video_context_driver_get_video_output_next();
}

static void fpga_set_video_mode(void *data, unsigned width, unsigned height,
      bool fullscreen)
{
   gfx_ctx_mode_t mode;

   mode.width      = width;
   mode.height     = height;
   mode.fullscreen = fullscreen;

   video_context_driver_set_video_mode(&mode);
}

static const video_poke_interface_t fpga_poke_interface = {
   NULL,
   NULL,
   fpga_set_video_mode,
   NULL,
   fpga_get_video_output_size,
   fpga_get_video_output_prev,
   fpga_get_video_output_next,
   NULL,
   NULL,
   NULL,
   NULL,
#if defined(HAVE_MENU)
   fpga_set_texture_frame,
   NULL,
   fpga_set_osd_msg,
   NULL,
#else
   NULL,
   NULL,
   NULL,
   NULL,
#endif

   NULL,
#ifdef HAVE_MENU
   NULL,
#endif
};

static void fpga_gfx_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &fpga_poke_interface;
}

static void fpga_gfx_set_viewport(void *data, unsigned viewport_width,
      unsigned viewport_height, bool force_full, bool allow_rotate)
{
}

bool fpga_has_menu_frame(void)
{
   return (fpga_menu_frame != NULL);
}

video_driver_t video_fpga = {
   fpga_gfx_init,
   fpga_gfx_frame,
   fpga_gfx_set_nonblock_state,
   fpga_gfx_alive,
   fpga_gfx_focus,
   fpga_gfx_suppress_screensaver,
   fpga_gfx_has_windowed,
   fpga_gfx_set_shader,
   fpga_gfx_free,
   "fpga",
   fpga_gfx_set_viewport,
   fpga_gfx_set_rotation,
   fpga_gfx_viewport_info,
   fpga_gfx_read_viewport,
   NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
  NULL, /* overlay_interface */
#endif
  fpga_gfx_get_poke_interface,
};
