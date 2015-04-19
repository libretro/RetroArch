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
#include <file/file_path.h>
#include "record_driver.h"

#include "../driver.h"
#include "../dynamic.h"
#include "../general.h"
#include "../retroarch.h"
#include "../runloop.h"
#include "../intl/intl.h"
#include "../gfx/video_driver.h"
#include "../gfx/video_viewport.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

static const record_driver_t *record_drivers[] = {
#ifdef HAVE_FFMPEG
   &ffemu_ffmpeg,
#endif
   &ffemu_null,
   NULL,
};

/**
 * record_driver_find_ident:
 * @idx                : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of record driver at index. Can be NULL
 * if nothing found.
 **/
const char *record_driver_find_ident(int idx)
{
   const record_driver_t *drv = record_drivers[idx];
   if (!drv)
      return NULL;
   return drv->ident;
}

/**
 * record_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to record driver at index. Can be NULL
 * if nothing found.
 **/
const void *record_driver_find_handle(int idx)
{
   const void *drv = record_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * config_get_record_driver_options:
 *
 * Get an enumerated list of all record driver names, separated by '|'.
 *
 * Returns: string listing of all record driver names, separated by '|'.
 **/
const char* config_get_record_driver_options(void)
{
   union string_list_elem_attr attr;
   unsigned i;
   char *options = NULL;
   int options_len = 0;
   struct string_list *options_l = string_list_new();

   attr.i = 0;

   if (!options_l)
      return NULL;

   for (i = 0; record_driver_find_handle(i); i++)
   {
      const char *opt = record_driver_find_ident(i);
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

void find_record_driver(void)
{
   driver_t *driver     = driver_get_ptr();
   settings_t *settings = config_get_ptr();

   int i = find_driver_index("record_driver", settings->record.driver);

   if (i >= 0)
      driver->recording = (const record_driver_t*)audio_driver_find_handle(i);
   else
   {
      unsigned d;
      RARCH_ERR("Couldn't find any audio driver named \"%s\"\n",
            settings->audio.driver);
      RARCH_LOG_OUTPUT("Available audio drivers are:\n");
      for (d = 0; audio_driver_find_handle(d); d++)
         RARCH_LOG_OUTPUT("\t%s\n", record_driver_find_ident(d));
      RARCH_WARN("Going to default to first audio driver...\n");

      driver->audio = (const audio_driver_t*)audio_driver_find_handle(0);

      if (!driver->audio)
         rarch_fail(1, "find_audio_driver()");
   }
}

/**
 * ffemu_find_backend:
 * @ident                   : Identifier of driver to find.
 *
 * Finds a recording driver with the name @ident.
 *
 * Returns: recording driver handle if successful, otherwise
 * NULL.
 **/
const record_driver_t *ffemu_find_backend(const char *ident)
{
   unsigned i;

   for (i = 0; record_drivers[i]; i++)
   {
      if (!strcmp(record_drivers[i]->ident, ident))
         return record_drivers[i];
   }

   return NULL;
}


/**
 * gfx_ctx_init_first:
 * @backend                 : Recording backend handle.
 * @data                    : Recording data handle.
 * @params                  : Recording info parameters.
 *
 * Finds first suitable recording context driver and initializes.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool record_driver_init_first(const record_driver_t **backend, void **data,
      const struct ffemu_params *params)
{
   unsigned i;

   for (i = 0; record_drivers[i]; i++)
   {
      void *handle = record_drivers[i]->init(params);

      if (!handle)
         continue;

      *backend = record_drivers[i];
      *data = handle;
      return true;
   }

   return false;
}

void recording_dump_frame(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
   struct ffemu_video_data ffemu_data = {0};
   driver_t *driver = driver_get_ptr();
   global_t *global = global_get_ptr();

   if (!driver->recording_data)
      return;

   ffemu_data.pitch   = pitch;
   ffemu_data.width   = width;
   ffemu_data.height  = height;
   ffemu_data.data    = data;

   if (global->record.gpu_buffer)
   {
      struct video_viewport vp = {0};

      video_driver_viewport_info(&vp);

      if (!vp.width || !vp.height)
      {
         RARCH_WARN("Viewport size calculation failed! Will continue using raw data. This will probably not work right ...\n");
         event_command(EVENT_CMD_GPU_RECORD_DEINIT);

         recording_dump_frame(data, width, height, pitch);
         return;
      }

      /* User has resized. We kinda have a problem now. */
      if (vp.width != global->record.gpu_width ||
            vp.height != global->record.gpu_height)
      {
         static const char msg[] = "Recording terminated due to resize.";
         RARCH_WARN("%s\n", msg);

         rarch_main_msg_queue_push(msg, 1, 180, true);
         event_command(EVENT_CMD_RECORD_DEINIT);
         return;
      }

      /* Big bottleneck.
       * Since we might need to do read-backs asynchronously,
       * it might take 3-4 times before this returns true. */
      if (!video_driver_read_viewport(global->record.gpu_buffer))
            return;

      ffemu_data.pitch  = global->record.gpu_width * 3;
      ffemu_data.width  = global->record.gpu_width;
      ffemu_data.height = global->record.gpu_height;
      ffemu_data.data   = global->record.gpu_buffer +
         (ffemu_data.height - 1) * ffemu_data.pitch;

      ffemu_data.pitch  = -ffemu_data.pitch;
   }

   if (!global->record.gpu_buffer)
      ffemu_data.is_dupe = !data;

   if (driver->recording && driver->recording->push_video)
      driver->recording->push_video(driver->recording_data, &ffemu_data);
}

bool recording_deinit(void)
{
   driver_t *driver = driver_get_ptr();

   if (!driver->recording_data || !driver->recording)
      return false;

   if (driver->recording->finalize)
      driver->recording->finalize(driver->recording_data);

   if (driver->recording->free)
      driver->recording->free(driver->recording_data);

   driver->recording_data = NULL;
   driver->recording      = NULL;

   event_command(EVENT_CMD_GPU_RECORD_DEINIT);

   return true;
}

/**
 * recording_init:
 *
 * Initializes recording.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool recording_init(void)
{
   char recording_file[PATH_MAX_LENGTH];
   struct ffemu_params params = {0};
   global_t *global = global_get_ptr();
   driver_t *driver     = driver_get_ptr();
   settings_t *settings = config_get_ptr();
   const struct retro_system_av_info *info = &global->system.av_info;

   if (!global->record.enable)
      return false;

   if (global->libretro_dummy)
   {
      RARCH_WARN(RETRO_LOG_INIT_RECORDING_SKIPPED);
      return false;
   }

   if (!settings->video.gpu_record
         && global->system.hw_render_callback.context_type)
   {
      RARCH_WARN("Libretro core is hardware rendered. Must use post-shaded recording as well.\n");
      return false;
   }

   RARCH_LOG("Custom timing given: FPS: %.4f, Sample rate: %.4f\n",
         (float)global->system.av_info.timing.fps,
         (float)global->system.av_info.timing.sample_rate);

   strlcpy(recording_file, global->record.path, sizeof(recording_file));

   if (global->record.use_output_dir)
      fill_pathname_join(recording_file,
            global->record.output_dir,
            global->record.path, sizeof(recording_file));

   params.out_width  = info->geometry.base_width;
   params.out_height = info->geometry.base_height;
   params.fb_width   = info->geometry.max_width;
   params.fb_height  = info->geometry.max_height;
   params.channels   = 2;
   params.filename   = recording_file;
   params.fps        = global->system.av_info.timing.fps;
   params.samplerate = global->system.av_info.timing.sample_rate;
   params.pix_fmt    = (global->system.pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888) ?
      FFEMU_PIX_ARGB8888 : FFEMU_PIX_RGB565;
   params.config     = NULL;
   
   if (*global->record.config)
      params.config = global->record.config;

   if (settings->video.gpu_record && driver->video->read_viewport)
   {
      struct video_viewport vp = {0};

      video_driver_viewport_info(&vp);

      if (!vp.width || !vp.height)
      {
         RARCH_ERR("Failed to get viewport information from video driver. "
               "Cannot start recording ...\n");
         return false;
      }

      params.out_width  = vp.width;
      params.out_height = vp.height;
      params.fb_width   = next_pow2(vp.width);
      params.fb_height  = next_pow2(vp.height);

      if (settings->video.force_aspect &&
            (global->system.aspect_ratio > 0.0f))
         params.aspect_ratio  = global->system.aspect_ratio;
      else
         params.aspect_ratio  = (float)vp.width / vp.height;

      params.pix_fmt             = FFEMU_PIX_BGR24;
      global->record.gpu_width   = vp.width;
      global->record.gpu_height  = vp.height;

      RARCH_LOG("Detected viewport of %u x %u\n",
            vp.width, vp.height);

      global->record.gpu_buffer = (uint8_t*)malloc(vp.width * vp.height * 3);
      if (!global->record.gpu_buffer)
      {
         RARCH_ERR("Failed to allocate GPU record buffer.\n");
         return false;
      }
   }
   else
   {
      if (global->record.width || global->record.height)
      {
         params.out_width  = global->record.width;
         params.out_height = global->record.height;
      }

      if (settings->video.force_aspect &&
            (global->system.aspect_ratio > 0.0f))
         params.aspect_ratio = global->system.aspect_ratio;
      else
         params.aspect_ratio = (float)params.out_width / params.out_height;

      if (settings->video.post_filter_record && global->filter.filter)
      {
         unsigned max_width  = 0;
         unsigned max_height = 0;

         if (global->filter.out_rgb32)
            params.pix_fmt = FFEMU_PIX_ARGB8888;
         else
            params.pix_fmt =  FFEMU_PIX_RGB565;

         rarch_softfilter_get_max_output_size(global->filter.filter,
               &max_width, &max_height);
         params.fb_width  = next_pow2(max_width);
         params.fb_height = next_pow2(max_height);
      }
   }

   RARCH_LOG("Recording to %s @ %ux%u. (FB size: %ux%u pix_fmt: %u)\n",
         global->record.path,
         params.out_width, params.out_height,
         params.fb_width, params.fb_height,
         (unsigned)params.pix_fmt);

   if (!record_driver_init_first(&driver->recording, &driver->recording_data, &params))
   {
      RARCH_ERR(RETRO_LOG_INIT_RECORDING_FAILED);
      event_command(EVENT_CMD_GPU_RECORD_DEINIT);

      return false;
   }

   return true;
}
