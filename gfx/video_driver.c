/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include <string.h>
#include <string/string_list.h>
#include "video_driver.h"
#include "video_thread_wrapper.h"
#include "video_pixel_converter.h"
#include "video_monitor.h"
#include "../general.h"
#include "../retroarch.h"
#include "../runloop.h"

static const video_driver_t *video_drivers[] = {
#ifdef HAVE_OPENGL
   &video_gl,
#endif
#ifdef XENON
   &video_xenon360,
#endif
#if defined(_XBOX) && (defined(HAVE_D3D8) || defined(HAVE_D3D9)) || defined(HAVE_WIN32_D3D9)
   &video_d3d,
#endif
#ifdef SN_TARGET_PSP2
   &video_vita,
#endif
#ifdef PSP
   &video_psp1,
#endif
#ifdef HAVE_SDL
   &video_sdl,
#endif
#ifdef HAVE_SDL2
   &video_sdl2,
#endif
#ifdef HAVE_XVIDEO
   &video_xvideo,
#endif
#ifdef GEKKO
   &video_gx,
#endif
#ifdef HAVE_VG
   &video_vg,
#endif
#ifdef HAVE_OMAP
   &video_omap,
#endif
#ifdef HAVE_EXYNOS
   &video_exynos,
#endif
#ifdef HAVE_DISPMANX
   &video_dispmanx,
#endif
#ifdef HAVE_SUNXI
   &video_sunxi,
#endif
   &video_null,
   NULL,
};

/**
 * video_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to video driver at index. Can be NULL
 * if nothing found.
 **/
const void *video_driver_find_handle(int idx)
{
   const void *drv = video_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * video_driver_find_ident:
 * @idx                : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of video driver at index. Can be NULL
 * if nothing found.
 **/
const char *video_driver_find_ident(int idx)
{
   const video_driver_t *drv = video_drivers[idx];
   if (!drv)
      return NULL;
   return drv->ident;
}

/**
 * config_get_video_driver_options:
 *
 * Get an enumerated list of all video driver names, separated by '|'.
 *
 * Returns: string listing of all video driver names, separated by '|'.
 **/
const char* config_get_video_driver_options(void)
{
   union string_list_elem_attr attr;
   unsigned i;
   char *options = NULL;
   int options_len = 0;
   struct string_list *options_l = string_list_new();

   attr.i = 0;

   if (!options_l)
      return NULL;

   for (i = 0; video_driver_find_handle(i); i++)
   {
      const char *opt = video_driver_find_ident(i);
      options_len += strlen(opt) + 1;
      string_list_append(options_l, opt, attr);
   }

   options = (char*)calloc(options_len, sizeof(char));

   if (!options)
   {
      string_list_free(options_l);
      options_l = NULL;
      return NULL;
   }

   string_list_join_concat(options, options_len, options_l, "|");

   string_list_free(options_l);
   options_l = NULL;

   return options;
}

void find_video_driver(void)
{
   int i;
   driver_t *driver     = driver_get_ptr();
   global_t *global     = global_get_ptr();
   settings_t *settings = config_get_ptr();

   (void)global;

#if defined(HAVE_OPENGL) && defined(HAVE_FBO)
   if (global->system.hw_render_callback.context_type)
   {
      RARCH_LOG("Using HW render, OpenGL driver forced.\n");
      driver->video = &video_gl;
      return;
   }
#endif

   if (driver->frontend_ctx &&
       driver->frontend_ctx->get_video_driver)
   {
      driver->video = driver->frontend_ctx->get_video_driver();

      if (driver->video)
         return;
      RARCH_WARN("Frontend supports get_video_driver() but did not specify one.\n");
   }

   i = find_driver_index("video_driver", settings->video.driver);
   if (i >= 0)
      driver->video = (const video_driver_t*)video_driver_find_handle(i);
   else
   {
      unsigned d;
      RARCH_ERR("Couldn't find any video driver named \"%s\"\n",
            settings->video.driver);
      RARCH_LOG_OUTPUT("Available video drivers are:\n");
      for (d = 0; video_driver_find_handle(d); d++)
         RARCH_LOG_OUTPUT("\t%s\n", video_driver_find_ident(d));
      RARCH_WARN("Going to default to first video driver...\n");

      driver->video = (const video_driver_t*)video_driver_find_handle(0);

      if (!driver->video)
         rarch_fail(1, "find_video_driver()");
   }
}

/**
 * video_driver_get_ptr:
 * @drv                : real video driver will be set to this.
 *
 * Use this if you need the real video driver 
 * and driver data pointers.
 *
 * Returns: video driver's userdata.
 **/
void *video_driver_get_ptr(const video_driver_t **drv)
{
   driver_t *driver     = driver_get_ptr();
   global_t *global     = global_get_ptr();
   settings_t *settings = config_get_ptr();

#ifdef HAVE_THREADS
   if (settings->video.threaded
         && !global->system.hw_render_callback.context_type)
      return rarch_threaded_video_get_ptr(drv);
#endif
   if (drv)
      *drv = driver->video;

   return driver->video_data;
}

/**
 * video_driver_get_current_framebuffer:
 *
 * Gets pointer to current hardware renderer framebuffer object.
 * Used by RETRO_ENVIRONMENT_SET_HW_RENDER.
 *
 * Returns: pointer to hardware framebuffer object, otherwise 0.
 **/
uintptr_t video_driver_get_current_framebuffer(void)
{
#ifdef HAVE_FBO
   driver_t *driver = driver_get_ptr();
   if (driver && driver->video_poke && driver->video_poke->get_current_framebuffer)
      return driver->video_poke->get_current_framebuffer(driver->video_data);
#endif
   return 0;
}

retro_proc_address_t video_driver_get_proc_address(const char *sym)
{
   driver_t *driver = driver_get_ptr();
   if (driver && driver && driver->video_poke && driver->video_poke->get_proc_address)
      return driver->video_poke->get_proc_address(driver->video_data, sym);
   return NULL;
}

bool video_driver_is_alive(void)
{
   driver_t *driver = driver_get_ptr();
   /* Possible race issue, return true */
   if (!driver->video || !driver->video_data)
      return true;
   if (!driver->video->alive(driver->video_data))
      return false;
   return true;
}

bool video_driver_has_focus(void)
{
   driver_t *driver = driver_get_ptr();
   if (!driver->video || !driver->video_data)
      return false;
   if (!driver->video->focus(driver->video_data))
      return false;
   return true;
}

bool video_driver_set_shader(enum rarch_shader_type type,
      const char *path)
{
   driver_t *driver            = driver_get_ptr();
   if (!driver)
      return false;
   if (!driver->video_data)
      return false;
   if (!driver->video->set_shader)
      return false;

   return driver->video->set_shader(driver->video_data, type, path);
}

static void deinit_video_filter(void)
{
   global_t *global     = global_get_ptr();

   rarch_softfilter_free(global->filter.filter);
   free(global->filter.buffer);
   memset(&global->filter, 0, sizeof(global->filter));
}

static void init_video_filter(enum retro_pixel_format colfmt)
{
   unsigned width, height, pow2_x, pow2_y, maxsize;
   struct retro_game_geometry *geom = NULL;
   global_t *global                 = global_get_ptr();
   settings_t *settings             = config_get_ptr();

   deinit_video_filter();

   if (!*settings->video.softfilter_plugin)
      return;

   /* Deprecated format. Gets pre-converted. */
   if (colfmt == RETRO_PIXEL_FORMAT_0RGB1555)
      colfmt = RETRO_PIXEL_FORMAT_RGB565;

   if (global->system.hw_render_callback.context_type)
   {
      RARCH_WARN("Cannot use CPU filters when hardware rendering is used.\n");
      return;
   }

   geom    = (struct retro_game_geometry*)&global->system.av_info.geometry;
   width   = geom->max_width;
   height  = geom->max_height;

   global->filter.filter = rarch_softfilter_new(
         settings->video.softfilter_plugin,
         RARCH_SOFTFILTER_THREADS_AUTO, colfmt, width, height);

   if (!global->filter.filter)
   {
      RARCH_ERR("Failed to load filter.\n");
      return;
   }

   rarch_softfilter_get_max_output_size(global->filter.filter,
         &width, &height);

   pow2_x                = next_pow2(width);
   pow2_y                = next_pow2(height);
   maxsize               = max(pow2_x, pow2_y); 
   global->filter.scale  = maxsize / RARCH_SCALE_BASE;

   global->filter.out_rgb32 = rarch_softfilter_get_output_format(
         global->filter.filter) == RETRO_PIXEL_FORMAT_XRGB8888;

   global->filter.out_bpp = global->filter.out_rgb32 ?
      sizeof(uint32_t) : sizeof(uint16_t);

   /* TODO: Aligned output. */
   global->filter.buffer = malloc(width * height * global->filter.out_bpp);
   if (!global->filter.buffer)
      goto error;

   return;

error:
   RARCH_ERR("Softfilter initialization failed.\n");
   deinit_video_filter();
}

static void init_video_input(const input_driver_t *tmp)
{
   driver_t *driver = driver_get_ptr();

   /* Video driver didn't provide an input driver,
    * so we use configured one. */
   RARCH_LOG("Graphics driver did not initialize an input driver. Attempting to pick a suitable driver.\n");

   if (tmp)
      driver->input = tmp;
   else
      find_input_driver();

   if (!driver->input)
   {
      /* This should never really happen as tmp (driver.input) is always 
       * found before this in find_driver_input(), or we have aborted 
       * in a similar fashion anyways. */
      rarch_fail(1, "init_video_input()");
   }

   driver->input_data = input_driver_init();

   if (driver->input_data)
      return;

   RARCH_ERR("Cannot initialize input driver. Exiting ...\n");
   rarch_fail(1, "init_video_input()");
}

void uninit_video_input(void)
{
   driver_t *driver = driver_get_ptr();

   rarch_main_command(RARCH_CMD_OVERLAY_DEINIT);

   if (
         !driver->input_data_own &&
         (driver->input_data != driver->video_data)
      )
      input_driver_free();

   if (
         !driver->video_data_own &&
         driver->video_data &&
         driver->video &&
         driver->video->free)
      driver->video->free(driver->video_data);

   deinit_pixel_converter();

   deinit_video_filter();

   rarch_main_command(RARCH_CMD_SHADER_DIR_DEINIT);
   video_monitor_compute_fps_statistics();
}

void init_video(void)
{
   unsigned max_dim, scale, width, height;
   video_viewport_t *custom_vp = NULL;
   const input_driver_t *tmp = NULL;
   const struct retro_game_geometry *geom = NULL;
   video_info_t video = {0};
   static uint16_t dummy_pixels[32] = {0};
   runloop_t *runloop   = rarch_main_get_ptr();
   driver_t *driver     = driver_get_ptr();
   global_t *global     = global_get_ptr();
   settings_t *settings = config_get_ptr();

   init_video_filter(global->system.pix_fmt);
   rarch_main_command(RARCH_CMD_SHADER_DIR_INIT);

   geom      = (const struct retro_game_geometry*)&global->system.av_info.geometry;
   max_dim   = max(geom->max_width, geom->max_height);
   scale     = next_pow2(max_dim) / RARCH_SCALE_BASE;
   scale     = max(scale, 1);

   if (global->filter.filter)
      scale = global->filter.scale;

   /* Update core-dependent aspect ratio values. */
   video_viewport_set_square_pixel(geom->base_width, geom->base_height);
   video_viewport_set_core();
   video_viewport_set_config();

   /* Update CUSTOM viewport. */
   custom_vp = &global->console.screen.viewports.custom_vp;

   if (settings->video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
   {
      float default_aspect = aspectratio_lut[ASPECT_RATIO_CORE].value;
      aspectratio_lut[ASPECT_RATIO_CUSTOM].value = 
         (custom_vp->width && custom_vp->height) ?
         (float)custom_vp->width / custom_vp->height : default_aspect;
   }

   global->system.aspect_ratio = 
      aspectratio_lut[settings->video.aspect_ratio_idx].value;

   if (settings->video.fullscreen)
   {
      width  = settings->video.fullscreen_x;
      height = settings->video.fullscreen_y;
   }
   else
   {
      if (settings->video.force_aspect)
      {
         /* Do rounding here to simplify integer scale correctness. */
         unsigned base_width = roundf(geom->base_height *
               global->system.aspect_ratio);
         width  = roundf(base_width * settings->video.scale);
         height = roundf(geom->base_height * settings->video.scale);
      }
      else
      {
         width  = roundf(geom->base_width   * settings->video.scale);
         height = roundf(geom->base_height * settings->video.scale);
      }
   }

   if (width && height)
      RARCH_LOG("Video @ %ux%u\n", width, height);
   else
      RARCH_LOG("Video @ fullscreen\n");

   driver->display_type  = RARCH_DISPLAY_NONE;
   driver->video_display = 0;
   driver->video_window  = 0;

   if (!init_video_pixel_converter(RARCH_SCALE_BASE * scale))
   {
      RARCH_ERR("Failed to initialize pixel converter.\n");
      rarch_fail(1, "init_video()");
   }

   video.width        = width;
   video.height       = height;
   video.fullscreen   = settings->video.fullscreen;
   video.vsync        = settings->video.vsync && !global->system.force_nonblock;
   video.force_aspect = settings->video.force_aspect;
#ifdef GEKKO
   video.viwidth      = settings->video.viwidth;
   video.vfilter      = settings->video.vfilter;
#endif
   video.smooth       = settings->video.smooth;
   video.input_scale  = scale;
   video.rgb32        = global->filter.filter ? 
      global->filter.out_rgb32 : 
      (global->system.pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888);

   tmp = (const input_driver_t*)driver->input;
   /* Need to grab the "real" video driver interface on a reinit. */
   find_video_driver();

#ifdef HAVE_THREADS
   if (settings->video.threaded && !global->system.hw_render_callback.context_type)
   {
      /* Can't do hardware rendering with threaded driver currently. */
      RARCH_LOG("Starting threaded video driver ...\n");

      if (!rarch_threaded_video_init(&driver->video, &driver->video_data,
               &driver->input, &driver->input_data,
               driver->video, &video))
      {
         RARCH_ERR("Cannot open threaded video driver ... Exiting ...\n");
         rarch_fail(1, "init_video()");
      }
   }
   else
#endif
      driver->video_data = driver->video->init(&video, &driver->input,
            &driver->input_data);

   if (!driver->video_data)
   {
      RARCH_ERR("Cannot open video driver ... Exiting ...\n");
      rarch_fail(1, "init_video()");
   }

   driver->video_poke = NULL;
   if (driver->video->poke_interface)
      driver->video->poke_interface(driver->video_data, &driver->video_poke);

   if (driver->video->viewport_info && (!custom_vp->width ||
            !custom_vp->height))
   {
      /* Force custom viewport to have sane parameters. */
      custom_vp->width = width;
      custom_vp->height = height;
      video_driver_viewport_info(custom_vp);
   }

   video_driver_set_rotation(
            (settings->video.rotation + global->system.rotation) % 4);

   if (driver->video->suppress_screensaver)
      driver->video->suppress_screensaver(driver->video_data,
            settings->ui.suspend_screensaver_enable);

   if (!driver->input)
      init_video_input(tmp);

   rarch_main_command(RARCH_CMD_OVERLAY_DEINIT);
   rarch_main_command(RARCH_CMD_OVERLAY_INIT);

   runloop->measure_data.frame_time_samples_count = 0;

   global->frame_cache.width = 4;
   global->frame_cache.height = 4;
   global->frame_cache.pitch = 8;
   global->frame_cache.data = &dummy_pixels;

#if defined(PSP)
   video_driver_set_texture_frame(&dummy_pixels, false, 1, 1, 1.0f);
#endif
}

bool video_driver_has_windowed(void)
{
   driver_t *driver     = driver_get_ptr();

   if (!driver)
      return false;
   if (!driver->video)
      return false;

   /* If video driver/context does not support windowed
    * mode, don't perform command. */
   if (!driver->video->has_windowed(driver->video_data))
      return false;
   return true;
}

void video_driver_set_nonblock_state(bool toggle)
{
   driver_t *driver     = driver_get_ptr();

   if (!driver)
      return;
   if (!driver->video)
      return;

   if (driver->video->set_nonblock_state)
      driver->video->set_nonblock_state(driver->video_data, toggle);
}

bool video_driver_set_rotation(unsigned rotation)
{
   driver_t *driver     = driver_get_ptr();

   if (!driver)
      return false;
   if (!driver->video)
      return false;
   if (!driver->video->set_rotation)
      return false;

   driver->video->set_rotation(driver->video_data, rotation);

   return true;
}

void video_driver_set_video_mode(unsigned width,
      unsigned height, bool fullscreen)
{
   driver_t *driver = driver_get_ptr();

   if (!driver)
      return;
   if (!driver->video_data)
      return;
   if (!driver->video_poke)
      return;
   if (!driver->video_poke->set_video_mode)
      return;

   driver->video_poke->set_video_mode(driver->video_data,
         width, height, fullscreen);
}

bool video_driver_get_video_output_size(unsigned *width, unsigned *height)
{
   driver_t *driver = driver_get_ptr();

   if (!driver)
      return false;
   if (!driver->video_data)
      return false;
   if (!driver->video_poke)
      return false;
   if (!driver->video_poke->get_video_output_size)
      return false;

   driver->video_poke->get_video_output_size(driver->video_data,
         width, height);
   return true;
}

void video_driver_set_aspect_ratio(unsigned aspectratio_index)
{
   driver_t *driver     = driver_get_ptr();

   if (!driver)
      return;
   if (!driver->video_data)
      return;
   if (!driver->video_poke)
      return;
   if (!driver->video_poke->set_aspect_ratio)
      return;

   driver->video_poke->set_aspect_ratio(driver->video_data,
         aspectratio_index);
}

void video_driver_show_mouse(bool state)
{
   driver_t *driver     = driver_get_ptr();

   if (!driver)
      return;
   if (!driver->video_data)
      return;
   if (!driver->video_poke)
      return;
   if (!driver->video_poke->show_mouse)
      return;
   driver->video_poke->show_mouse(
         driver->video_data, state);
}

void video_driver_set_osd_msg(const char *msg,
      const struct font_params *params, void *font)
{
   driver_t *driver     = driver_get_ptr();

   if (!driver)
      return;
   if (!driver->video_data)
      return;
   if (!driver->video_poke)
      return;
   if (!driver->video_poke->set_osd_msg)
      return;
   driver->video_poke->set_osd_msg(driver->video_data,
         msg, params, font);
}

void video_driver_set_texture_enable(bool enable, bool fullscreen)
{
#ifdef HAVE_MENU
   driver_t *driver     = driver_get_ptr();

   if (!driver)
      return;
   if (!driver->video_data)
      return;
   if (!driver->video_poke)
      return;
   if (!driver->video_poke->set_texture_enable)
      return;
   driver->video_poke->set_texture_enable(driver->video_data,
         enable, fullscreen);
#endif
}

void video_driver_set_texture_frame(const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
#ifdef HAVE_MENU
   driver_t *driver     = driver_get_ptr();

   if (!driver)
      return;
   if (!driver->video_data)
      return;
   if (!driver->video_poke)
      return;
   if (!driver->video_poke->set_texture_frame)
      return;
   driver->video_poke->set_texture_frame(
         driver->video_data, frame, rgb32, width, height, alpha);
#endif
}

bool video_driver_viewport_info(struct video_viewport *vp)
{
   driver_t *driver     = driver_get_ptr();

   if (!driver)
      return false;
   if (!driver->video_data)
      return false;
   if (!driver->video)
      return false;
   if (!driver->video->viewport_info)
      return false;

   driver->video->viewport_info(driver->video_data, vp);

   return true;
}

bool video_driver_read_viewport(uint8_t *buffer)
{
   driver_t *driver     = driver_get_ptr();

   if (!driver)
      return false;
   if (!driver->video)
      return false;
   if (!driver->video->read_viewport)
      return false;

   return driver->video->read_viewport(driver->video_data,
         buffer);
}

bool video_driver_focus(void)
{
   driver_t *driver     = driver_get_ptr();

   if (!driver)
      return false;
   if (!driver->video)
      return false;
   if (!driver->video->focus)
      return false;
   return driver->video->focus(driver->video_data);
}

#ifdef HAVE_OVERLAY
bool video_driver_overlay_interface(const video_overlay_interface_t **iface)
{
   driver_t *driver     = driver_get_ptr();

   if (!driver)
      return false;
   if (!driver->video)
      return false;

   if (driver->video && driver->video->overlay_interface)
      driver->video->overlay_interface(driver->video_data, iface);

   return true;
}
#endif

void * video_driver_read_frame_raw(unsigned *width,
   unsigned *height, size_t *pitch)
{
   driver_t *driver     = driver_get_ptr();

   if (!driver)
      return NULL;
   if (!driver->video)
      return NULL;
   if (!driver->video->read_frame_raw)
      return NULL;

   return driver->video->read_frame_raw(driver->video_data, width,
         height, pitch);
}

void video_driver_set_filtering(unsigned index, bool smooth)
{
   driver_t *driver     = driver_get_ptr();

   if (!driver)
      return;
   if (!driver->video)
      return;
   if (!driver->video_poke)
      return;
   if (!driver->video_poke->set_filtering)
      return;
   driver->video_poke->set_filtering(driver->video_data,
         index, smooth);
}

void video_driver_apply_state_changes(void)
{
   driver_t *driver     = driver_get_ptr();

   if (!driver)
      return;
   if (!driver->video)
      return;
   if (!driver->video_poke)
      return;
   if (!driver->video_poke->apply_state_changes)
      return;
   driver->video_poke->apply_state_changes(driver->video_data);
}

void video_driver_get_video_output_next(void)
{
   driver_t *driver     = driver_get_ptr();

   if (!driver)
      return;
   if (!driver->video)
      return;
   if (!driver->video_data)
      return;
   if (!driver->video_poke)
      return;
   if (!driver->video_poke->get_video_output_next)
      return;
   driver->video_poke->get_video_output_next(driver->video_data);
}

void video_driver_get_video_output_prev(void)
{
   driver_t *driver     = driver_get_ptr();

   if (!driver)
      return;
   if (!driver->video)
      return;
   if (!driver->video_data)
      return;
   if (!driver->video_poke)
      return;
   if (!driver->video_poke->get_video_output_prev)
      return;
   driver->video_poke->get_video_output_prev(driver->video_data);
}
