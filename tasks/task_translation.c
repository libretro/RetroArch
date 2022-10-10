/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2021 - Daniel De Matteis
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

#include <stdint.h>
#include <string.h>

#include <boolean.h>
#include <compat/strl.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <encodings/base64.h>
#include <formats/rbmp.h>
#include <formats/rpng.h>
#include <formats/rjson.h>
#include <gfx/scaler/pixconv.h>
#include <gfx/scaler/scaler.h>
#include <gfx/video_frame.h>
#include "../translation_defines.h"

#ifdef HAVE_GFX_WIDGETS
#include "../gfx/gfx_widgets.h"
#endif

#include "../accessibility.h"
#include "../audio/audio_driver.h"
#include "../gfx/video_driver.h"
#include "../frontend/frontend_driver.h"
#include "../input/input_driver.h"
#include "../command.h"
#include "../paths.h"
#include "../runloop.h"
#include "../verbosity.h"

#include "tasks_internal.h"

static void task_auto_translate_handler(retro_task_t *task)
{
   int               *mode_ptr = (int*)task->user_data;
   uint32_t runloop_flags      = runloop_get_flags();
   access_state_t *access_st   = access_state_get_ptr();
#ifdef HAVE_ACCESSIBILITY
   settings_t *settings        = config_get_ptr();
#endif

   if (task_get_cancelled(task))
      goto task_finished;

   switch (*mode_ptr)
   {
      case 1: /* Speech   Mode */
#ifdef HAVE_AUDIOMIXER
         if (!audio_driver_is_ai_service_speech_running())
            goto task_finished;
#endif
         break;
      case 2: /* Narrator Mode */
#ifdef HAVE_ACCESSIBILITY
         if (!is_narrator_running(
                  settings->bools.accessibility_enable))
            goto task_finished;
#endif
         break;
      default:
         break;
   }

   return;

task_finished:
   if (access_st->ai_service_auto == 1)
      access_st->ai_service_auto = 2;

   task_set_finished(task, true);

   if (*mode_ptr == 1 || *mode_ptr == 2)
   {
      bool was_paused = runloop_flags & RUNLOOP_FLAG_PAUSED;
      command_event(CMD_EVENT_AI_SERVICE_CALL, &was_paused);
   }
   if (task->user_data)
       free(task->user_data);
}

static void call_auto_translate_task(
      settings_t *settings,
      bool *was_paused)
{
   int ai_service_mode       = settings->uints.ai_service_mode;
   access_state_t *access_st = access_state_get_ptr();

   /*Image Mode*/
   if (ai_service_mode == 0)
   {
      if (access_st->ai_service_auto == 1)
         access_st->ai_service_auto = 2;

      command_event(CMD_EVENT_AI_SERVICE_CALL, was_paused);
   }
   else /* Speech or Narrator Mode */
   {
      int* mode                          = NULL;
      retro_task_t  *t                   = task_init();
      if (!t)
         return;

      mode                               = (int*)malloc(sizeof(int));
      *mode                              = ai_service_mode;

      t->handler                         = task_auto_translate_handler;
      t->user_data                       = mode;
      t->mute                            = true;
      task_queue_push(t);
   }
}

static void handle_translation_cb(
      retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
   size_t pitch;
   unsigned width, height;
   unsigned image_width, image_height;
   uint8_t* raw_output_data          = NULL;
   char* raw_image_file_data         = NULL;
   struct scaler_ctx* scaler         = NULL;
   http_transfer_data_t *data        = (http_transfer_data_t*)task_data;
   int new_image_size                = 0;
#ifdef HAVE_AUDIOMIXER
   int new_sound_size                = 0;
#endif
   const void* dummy_data            = NULL;
   void* raw_image_data              = NULL;
   void* raw_image_data_alpha        = NULL;
   void* raw_sound_data              = NULL;
   int retval                        = 0;
   rjson_t* json                     = NULL;
   int json_current_key              = 0;
   char* err_string                  = NULL;
   char* text_string                 = NULL;
   char* auto_string                 = NULL;
   char* key_string                  = NULL;
   settings_t* settings              = config_get_ptr();
   uint32_t runloop_flags            = runloop_get_flags();
#ifdef HAVE_ACCESSIBILITY
   input_driver_state_t *input_st    = input_state_get_ptr();
#endif
   bool was_paused                   = runloop_flags & RUNLOOP_FLAG_PAUSED;
   video_driver_state_t 
      *video_st                      = video_state_get_ptr();
   const enum retro_pixel_format
      video_driver_pix_fmt           = video_st->pix_fmt;
   access_state_t *access_st         = access_state_get_ptr();
#ifdef HAVE_GFX_WIDGETS
   bool gfx_widgets_paused           = video_st->flags &
      VIDEO_FLAG_WIDGETS_PAUSED;
   dispgfx_widget_t *p_dispwidget    = dispwidget_get_ptr();
#endif
#ifdef HAVE_ACCESSIBILITY
   bool accessibility_enable         = settings->bools.accessibility_enable;
   unsigned accessibility_narrator_speech_speed = settings->uints.accessibility_narrator_speech_speed;
#ifdef HAVE_GFX_WIDGETS
   /* When auto mode is on, we turn off the overlay
    * once we have the result for the next call.*/
   if (p_dispwidget->ai_service_overlay_state != 0
       && access_st->ai_service_auto == 2)
      gfx_widgets_ai_service_overlay_unload();
#endif
#endif

#ifdef DEBUG
   if (access_st->ai_service_auto != 2)
      RARCH_LOG("RESULT FROM AI SERVICE...\n");
#endif

   if (!data || error || !data->data)
      goto finish;

   json = rjson_open_buffer(data->data, data->len);
   if (!json)
      goto finish;

   /* Parse JSON body for the image and sound data */
   for (;;)
   {
      static const char* keys[] = { "image", "sound", "text", "error", "auto", "press" };

      const char *str           = NULL;
      size_t str_len            = 0;
      enum rjson_type json_type = rjson_next(json);

      if (json_type == RJSON_DONE || json_type == RJSON_ERROR)
         break;
      if (json_type != RJSON_STRING)
         continue;
      if (rjson_get_context_type(json) != RJSON_OBJECT)
         continue;
      str                       = rjson_get_string(json, &str_len);

      if ((rjson_get_context_count(json) & 1) == 1)
      {
         int i;
         json_current_key = -1;

         for (i = 0; i < ARRAY_SIZE(keys); i++)
         {
            if (string_is_equal(str, keys[i]))
            {
               json_current_key = i;
               break;
            }
         }
      }
      else
      {
         switch (json_current_key)
         {
            case 0: /* image */
               raw_image_file_data = (char*)unbase64(str,
                    (int)str_len, &new_image_size);
               break;
#ifdef HAVE_AUDIOMIXER
            case 1: /* sound */
               raw_sound_data = (void*)unbase64(str,
                    (int)str_len, &new_sound_size);
               break;
#endif
            case 2: /* text */
               text_string = strdup(str);
               break;
            case 3: /* error */
               err_string  = strdup(str);
               break;
            case 4: /* auto */
               auto_string = strdup(str);
               break;
            case 5: /* press */
               key_string  = strdup(str);
               break;
         }
         json_current_key = -1;
      }
   }

   if (string_is_equal(err_string, "No text found."))
   {
#ifdef DEBUG
      RARCH_LOG("No text found...\n");
#endif
      if (text_string)
      {
         free(text_string);
         text_string = NULL;
      }

      text_string = (char*)malloc(15);

      strlcpy(text_string, err_string, 15);
#ifdef HAVE_GFX_WIDGETS
      if (gfx_widgets_paused)
      {
         /* In this case we have to unpause and then repause for a frame */
         p_dispwidget->ai_service_overlay_state = 2;
         command_event(CMD_EVENT_UNPAUSE, NULL);
      }
#endif
   }

   if (     !raw_image_file_data
         && !raw_sound_data
         && !text_string
         && (access_st->ai_service_auto != 2)
         && !key_string)
   {
      error = "Invalid JSON body.";
      goto finish;
   }

   if (raw_image_file_data)
   {
      /* Get the video frame dimensions reference */
      video_driver_cached_frame_get(&dummy_data, &width, &height, &pitch);

      /* try two different modes for text display *
       * In the first mode, we use display widget overlays, but they require
       * the video poke interface to be able to load image buffers.
       *
       * The other method is to draw to the video buffer directly, which needs
       * a software core to be running. */
#ifdef HAVE_GFX_WIDGETS
      if (   video_st->poke
          && video_st->poke->load_texture
          && video_st->poke->unload_texture)
      {
         bool ai_res;
         enum image_type_enum image_type;
         /* Write to overlay */
         if (  raw_image_file_data[0] == 'B' &&
               raw_image_file_data[1] == 'M')
             image_type = IMAGE_TYPE_BMP;
         else if (raw_image_file_data[1] == 'P' &&
                  raw_image_file_data[2] == 'N' &&
                  raw_image_file_data[3] == 'G')
            image_type = IMAGE_TYPE_PNG;
         else
         {
            RARCH_LOG("Invalid image type returned from server.\n");
            goto finish;
         }

         ai_res = gfx_widgets_ai_service_overlay_load(
               raw_image_file_data, (unsigned)new_image_size,
               image_type);

         if (!ai_res)
         {
            RARCH_LOG("Video driver not supported for AI Service.");
            runloop_msg_queue_push(
               /* msg_hash_to_str(MSG_VIDEO_DRIVER_NOT_SUPPORTED), */
               "Video driver not supported.",
               1, 180, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
         else if (gfx_widgets_paused)
         {
            /* In this case we have to unpause and then repause for a frame */
            /* Unpausing state */
            p_dispwidget->ai_service_overlay_state = 2;
            command_event(CMD_EVENT_UNPAUSE, NULL);
         }
      }
      else
#endif
      /* Can't use display widget overlays, so try writing to video buffer */
      {
         /* Write to video buffer directly (software cores only) */
         if (raw_image_file_data[0] == 'B' && raw_image_file_data[1] == 'M')
         {
            /* This is a BMP file coming back. */
            /* Get image data (24 bit), and convert to the emulated pixel format */
            image_width    =
               ((uint32_t) ((uint8_t)raw_image_file_data[21]) << 24) +
               ((uint32_t) ((uint8_t)raw_image_file_data[20]) << 16) +
               ((uint32_t) ((uint8_t)raw_image_file_data[19]) << 8) +
               ((uint32_t) ((uint8_t)raw_image_file_data[18]) << 0);

            image_height   =
               ((uint32_t) ((uint8_t)raw_image_file_data[25]) << 24) +
               ((uint32_t) ((uint8_t)raw_image_file_data[24]) << 16) +
               ((uint32_t) ((uint8_t)raw_image_file_data[23]) << 8) +
               ((uint32_t) ((uint8_t)raw_image_file_data[22]) << 0);
            raw_image_data = (void*)malloc(image_width*image_height*3*sizeof(uint8_t));
            memcpy(raw_image_data,
                   raw_image_file_data+54*sizeof(uint8_t),
                   image_width*image_height*3*sizeof(uint8_t));
         }
         else if (raw_image_file_data[1] == 'P' && raw_image_file_data[2] == 'N' &&
                  raw_image_file_data[3] == 'G')
         {
            rpng_t *rpng = NULL;
            /* PNG coming back from the url */
            image_width  =
                ((uint32_t) ((uint8_t)raw_image_file_data[16]) << 24)+
                ((uint32_t) ((uint8_t)raw_image_file_data[17]) << 16)+
                ((uint32_t) ((uint8_t)raw_image_file_data[18]) << 8)+
                ((uint32_t) ((uint8_t)raw_image_file_data[19]) << 0);
            image_height =
                ((uint32_t) ((uint8_t)raw_image_file_data[20]) << 24)+
                ((uint32_t) ((uint8_t)raw_image_file_data[21]) << 16)+
                ((uint32_t) ((uint8_t)raw_image_file_data[22]) << 8)+
                ((uint32_t) ((uint8_t)raw_image_file_data[23]) << 0);
            rpng = rpng_alloc();

            if (!rpng)
            {
               error = "Can't allocate memory.";
               goto finish;
            }

            rpng_set_buf_ptr(rpng, raw_image_file_data, (size_t)new_image_size);
            rpng_start(rpng);
            while (rpng_iterate_image(rpng));

            do
            {
               retval = rpng_process_image(rpng, &raw_image_data_alpha,
                     (size_t)new_image_size, &image_width, &image_height);
            } while (retval == IMAGE_PROCESS_NEXT);

            /* Returned output from the png processor is an upside down RGBA
             * image, so we have to change that to RGB first.  This should
             * probably be replaced with a scaler call.*/
            {
               unsigned ui;
               int d,tw,th,tc;
               d=0;
               raw_image_data = (void*)malloc(image_width*image_height*3*sizeof(uint8_t));
               for (ui = 0; ui < image_width * image_height * 4; ui++)
               {
                  if (ui % 4 != 3)
                  {
                     tc = d%3;
                     th = image_height-d / (3*image_width)-1;
                     tw = (d%(image_width*3)) / 3;
                     ((uint8_t*) raw_image_data)[tw*3+th*3*image_width+tc] = ((uint8_t *)raw_image_data_alpha)[ui];
                     d+=1;
                  }
               }
            }
            rpng_free(rpng);
         }
         else
         {
            RARCH_LOG("Output from URL not a valid file type, or is not supported.\n");
            goto finish;
         }

         scaler = (struct scaler_ctx*)calloc(1, sizeof(struct scaler_ctx));
         if (!scaler)
            goto finish;

         if (dummy_data == RETRO_HW_FRAME_BUFFER_VALID)
         {
            /*
               In this case, we used the viewport to grab the image
               and translate it, and we have the translated image in
               the raw_image_data buffer.
            */
            RARCH_LOG("Hardware frame buffer core, but selected video driver isn't supported.\n");
            goto finish;
         }

         /* The assigned pitch may not be reliable.  The width of
            the video frame can change during run-time, but the
            pitch may not, so we just assign it as the width
            times the byte depth.
         */

         if (video_driver_pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888)
         {
            raw_output_data    = (uint8_t*)malloc(width * height * 4 * sizeof(uint8_t));
            scaler->out_fmt    = SCALER_FMT_ARGB8888;
            pitch              = width * 4;
            scaler->out_stride = width * 4;
         }
         else
         {
            raw_output_data    = (uint8_t*)malloc(width * height * 2 * sizeof(uint8_t));
            scaler->out_fmt    = SCALER_FMT_RGB565;
            pitch              = width * 2;
            scaler->out_stride = width * 1;
         }

         if (!raw_output_data)
            goto finish;

         scaler->in_fmt        = SCALER_FMT_BGR24;
         scaler->in_width      = image_width;
         scaler->in_height     = image_height;
         scaler->out_width     = width;
         scaler->out_height    = height;
         scaler->scaler_type   = SCALER_TYPE_POINT;
         scaler_ctx_gen_filter(scaler);
         scaler->in_stride     = -1 * width * 3;

         scaler_ctx_scale_direct(scaler, raw_output_data,
               (uint8_t*)raw_image_data + (image_height - 1) * width * 3);
         video_driver_frame(raw_output_data, image_width, image_height, pitch);
      }
   }

#ifdef HAVE_AUDIOMIXER
   if (raw_sound_data)
   {
      audio_mixer_stream_params_t params;

      params.volume               = 1.0f;
      params.slot_selection_type  = AUDIO_MIXER_SLOT_SELECTION_MANUAL; /* user->slot_selection_type; */
      params.slot_selection_idx   = 10;
      params.stream_type          = AUDIO_STREAM_TYPE_SYSTEM; /* user->stream_type; */
      params.type                 = AUDIO_MIXER_TYPE_WAV;
      params.state                = AUDIO_STREAM_STATE_PLAYING;
      params.buf                  = raw_sound_data;
      params.bufsize              = new_sound_size;
      params.cb                   = NULL;
      params.basename             = NULL;

      audio_driver_mixer_add_stream(&params);

      if (raw_sound_data)
      {
         free(raw_sound_data);
         raw_sound_data = NULL;
      }
   }
#endif

   if (key_string)
   {
      char key[8];
      size_t length = strlen(key_string);
      int i         = 0;
      int start     = 0;
      char t        = ' ';

      for (i = 1; i < (int)length; i++)
      {
         t = key_string[i];
         if (i == length-1 || t == ' ' || t == ',')
         {
            if (i == length-1 && t != ' ' && t!= ',')
               i++;

            if (i-start > 7)
            {
               start = i;
               continue;
            }

            strncpy(key, key_string+start, i-start);
            key[i-start] = '\0';

#ifdef HAVE_ACCESSIBILITY
            if (string_is_equal(key, "b"))
               input_st->ai_gamepad_state[0] = 2;
            if (string_is_equal(key, "y"))
               input_st->ai_gamepad_state[1] = 2;
            if (string_is_equal(key, "select"))
               input_st->ai_gamepad_state[2] = 2;
            if (string_is_equal(key, "start"))
               input_st->ai_gamepad_state[3] = 2;

            if (string_is_equal(key, "up"))
               input_st->ai_gamepad_state[4] = 2;
            if (string_is_equal(key, "down"))
               input_st->ai_gamepad_state[5] = 2;
            if (string_is_equal(key, "left"))
               input_st->ai_gamepad_state[6] = 2;
            if (string_is_equal(key, "right"))
               input_st->ai_gamepad_state[7] = 2;

            if (string_is_equal(key, "a"))
               input_st->ai_gamepad_state[8] = 2;
            if (string_is_equal(key, "x"))
               input_st->ai_gamepad_state[9] = 2;
            if (string_is_equal(key, "l"))
               input_st->ai_gamepad_state[10] = 2;
            if (string_is_equal(key, "r"))
               input_st->ai_gamepad_state[11] = 2;

            if (string_is_equal(key, "l2"))
               input_st->ai_gamepad_state[12] = 2;
            if (string_is_equal(key, "r2"))
               input_st->ai_gamepad_state[13] = 2;
            if (string_is_equal(key, "l3"))
               input_st->ai_gamepad_state[14] = 2;
            if (string_is_equal(key, "r3"))
               input_st->ai_gamepad_state[15] = 2;
#endif

            if (string_is_equal(key, "pause"))
               command_event(CMD_EVENT_PAUSE, NULL);
            if (string_is_equal(key, "unpause"))
               command_event(CMD_EVENT_UNPAUSE, NULL);

            start = i+1;
         }
      }
   }

#ifdef HAVE_ACCESSIBILITY
   if (     text_string 
         && is_accessibility_enabled(
            accessibility_enable,
            access_st->enabled))
      accessibility_speak_priority(
            accessibility_enable,
            accessibility_narrator_speech_speed,
            text_string, 10);
#endif

finish:
   if (error)
      RARCH_ERR("%s: %s\n", msg_hash_to_str(MSG_DOWNLOAD_FAILED), error);

   if (user_data)
      free(user_data);

   if (json)
      rjson_free(json);
   if (raw_image_file_data)
      free(raw_image_file_data);
   if (raw_image_data_alpha)
       free(raw_image_data_alpha);
   if (raw_image_data)
      free(raw_image_data);
   if (scaler)
      free(scaler);
   if (err_string)
      free(err_string);
   if (text_string)
      free(text_string);
   if (raw_output_data)
      free(raw_output_data);

   if (string_is_equal(auto_string, "auto"))
   {
      if (     (access_st->ai_service_auto != 0)
            && !settings->bools.ai_service_pause)
         call_auto_translate_task(settings, &was_paused);
   }
   if (auto_string)
      free(auto_string);
   if (key_string)
      free(key_string);
}

static const char *ai_service_get_str(enum translation_lang id)
{
   switch (id)
   {
      case TRANSLATION_LANG_EN:
         return "en";
      case TRANSLATION_LANG_ES:
         return "es";
      case TRANSLATION_LANG_FR:
         return "fr";
      case TRANSLATION_LANG_IT:
         return "it";
      case TRANSLATION_LANG_DE:
         return "de";
      case TRANSLATION_LANG_JP:
         return "ja";
      case TRANSLATION_LANG_NL:
         return "nl";
      case TRANSLATION_LANG_CS:
         return "cs";
      case TRANSLATION_LANG_DA:
         return "da";
      case TRANSLATION_LANG_SV:
         return "sv";
      case TRANSLATION_LANG_HR:
         return "hr";
      case TRANSLATION_LANG_KO:
         return "ko";
      case TRANSLATION_LANG_ZH_CN:
         return "zh-CN";
      case TRANSLATION_LANG_ZH_TW:
         return "zh-TW";
      case TRANSLATION_LANG_CA:
         return "ca";
      case TRANSLATION_LANG_BG:
         return "bg";
      case TRANSLATION_LANG_BN:
         return "bn";
      case TRANSLATION_LANG_EU:
         return "eu";
      case TRANSLATION_LANG_AZ:
         return "az";
      case TRANSLATION_LANG_AR:
         return "ar";
      case TRANSLATION_LANG_AST:
         return "ast";
      case TRANSLATION_LANG_SQ:
         return "sq";
      case TRANSLATION_LANG_AF:
         return "af";
      case TRANSLATION_LANG_EO:
         return "eo";
      case TRANSLATION_LANG_ET:
         return "et";
      case TRANSLATION_LANG_TL:
         return "tl";
      case TRANSLATION_LANG_FI:
         return "fi";
      case TRANSLATION_LANG_GL:
         return "gl";
      case TRANSLATION_LANG_KA:
         return "ka";
      case TRANSLATION_LANG_EL:
         return "el";
      case TRANSLATION_LANG_GU:
         return "gu";
      case TRANSLATION_LANG_HT:
         return "ht";
      case TRANSLATION_LANG_HE:
         return "he";
      case TRANSLATION_LANG_HI:
         return "hi";
      case TRANSLATION_LANG_HU:
         return "hu";
      case TRANSLATION_LANG_IS:
         return "is";
      case TRANSLATION_LANG_ID:
         return "id";
      case TRANSLATION_LANG_GA:
         return "ga";
      case TRANSLATION_LANG_KN:
         return "kn";
      case TRANSLATION_LANG_LA:
         return "la";
      case TRANSLATION_LANG_LV:
         return "lv";
      case TRANSLATION_LANG_LT:
         return "lt";
      case TRANSLATION_LANG_MK:
         return "mk";
      case TRANSLATION_LANG_MS:
         return "ms";
      case TRANSLATION_LANG_MT:
         return "mt";
      case TRANSLATION_LANG_NO:
         return "no";
      case TRANSLATION_LANG_FA:
         return "fa";
      case TRANSLATION_LANG_PL:
         return "pl";
      case TRANSLATION_LANG_PT:
         return "pt";
      case TRANSLATION_LANG_RO:
         return "ro";
      case TRANSLATION_LANG_RU:
         return "ru";
      case TRANSLATION_LANG_SR:
         return "sr";
      case TRANSLATION_LANG_SK:
         return "sk";
      case TRANSLATION_LANG_SL:
         return "sl";
      case TRANSLATION_LANG_SW:
         return "sw";
      case TRANSLATION_LANG_TA:
         return "ta";
      case TRANSLATION_LANG_TE:
         return "te";
      case TRANSLATION_LANG_TH:
         return "th";
      case TRANSLATION_LANG_TR:
         return "tr";
      case TRANSLATION_LANG_UK:
         return "uk";
      case TRANSLATION_LANG_UR:
         return "ur";
      case TRANSLATION_LANG_VI:
         return "vi";
      case TRANSLATION_LANG_CY:
         return "cy";
      case TRANSLATION_LANG_YI:
         return "yi";
      case TRANSLATION_LANG_DONT_CARE:
      case TRANSLATION_LANG_LAST:
         break;
   }

   return "";
}

bool run_translation_service(settings_t *settings, bool paused)
{
   struct video_viewport vp;
   uint8_t header[54];
   size_t pitch;
   unsigned width, height;
   const void *data                  = NULL;
   uint8_t *bit24_image              = NULL;
   uint8_t *bit24_image_prev         = NULL;
   struct scaler_ctx *scaler         = (struct scaler_ctx*)
      calloc(1, sizeof(struct scaler_ctx));
   bool error                        = false;

   uint8_t *bmp_buffer               = NULL;
   uint64_t buffer_bytes             = 0;
   char *bmp64_buffer                = NULL;
   rjsonwriter_t* jsonwriter         = NULL;
   const char *json_buffer           = NULL;

   int bmp64_length                  = 0;
   bool TRANSLATE_USE_BMP            = false;

   const char *label                 = NULL;
   char* system_label                = NULL;
   core_info_t *core_info            = NULL;
   video_driver_state_t *video_st    = video_state_get_ptr();
   const enum retro_pixel_format
      video_driver_pix_fmt           = video_st->pix_fmt;
   access_state_t *access_st         = access_state_get_ptr();
#ifdef HAVE_ACCESSIBILITY
   input_driver_state_t *input_st    = input_state_get_ptr();
#endif

#ifdef HAVE_GFX_WIDGETS
   dispgfx_widget_t *p_dispwidget    = dispwidget_get_ptr();
   /* For the case when ai service pause is disabled. */
   if (     (p_dispwidget->ai_service_overlay_state != 0)
         && (access_st->ai_service_auto == 1))
   {
      gfx_widgets_ai_service_overlay_unload();
      goto finish;
   }
#endif


   /* get the core info here so we can pass long the game name */
   core_info_get_current_core(&core_info);

   if (core_info)
   {
      size_t label_len;
      const char *system_id               = core_info->system_id
         ? core_info->system_id : "core";
      size_t system_id_len                = strlen(system_id);
      const struct playlist_entry *entry  = NULL;
      playlist_t *current_playlist        = playlist_get_cached();

      if (current_playlist)
      {
         playlist_get_index_by_path(
            current_playlist, path_get(RARCH_PATH_CONTENT), &entry);

         if (entry && !string_is_empty(entry->label))
            label = entry->label;
      }

      if (!label)
         label     = path_basename(path_get(RARCH_PATH_BASENAME));
      label_len    = strlen(label);
      system_label = (char*)malloc(label_len + system_id_len + 3);
      memcpy(system_label, system_id, system_id_len);
      memcpy(system_label + system_id_len, "__", 2);
      memcpy(system_label + 2 + system_id_len, label, label_len);
      system_label[system_id_len + 2 + label_len] = '\0';
   }

   if (!scaler)
      goto finish;

   video_driver_cached_frame_get(&data, &width, &height, &pitch);

   if (!data)
      goto finish;

   if (data == RETRO_HW_FRAME_BUFFER_VALID)
   {
      /*
        The direct frame capture didn't work, so try getting it
        from the viewport instead.  This isn't as good as the
        raw frame buffer, since the viewport may us bilinear
        filtering, or other shaders that will completely trash
        the OCR, but it's better than nothing.
      */
      vp.x                           = 0;
      vp.y                           = 0;
      vp.width                       = 0;
      vp.height                      = 0;
      vp.full_width                  = 0;
      vp.full_height                 = 0;

      video_driver_get_viewport_info(&vp);

      if (!vp.width || !vp.height)
         goto finish;

      bit24_image_prev = (uint8_t*)malloc(vp.width * vp.height * 3);
      bit24_image      = (uint8_t*)malloc(width * height * 3);

      if (!bit24_image_prev || !bit24_image)
         goto finish;

      if (!video_driver_read_viewport(bit24_image_prev, false))
      {
         RARCH_LOG("Could not read viewport for translation service...\n");
         goto finish;
      }

      /* TODO: Rescale down to regular resolution */
      scaler->in_fmt      = SCALER_FMT_BGR24;
      scaler->out_fmt     = SCALER_FMT_BGR24;
      scaler->scaler_type = SCALER_TYPE_POINT;
      scaler->in_width    = vp.width;
      scaler->in_height   = vp.height;
      scaler->out_width   = width;
      scaler->out_height  = height;
      scaler_ctx_gen_filter(scaler);

      scaler->in_stride   = vp.width*3;
      scaler->out_stride  = width*3;
      scaler_ctx_scale_direct(scaler, bit24_image, bit24_image_prev);
   }
   else
   {
      /* This is a software core, so just change the pixel format to 24-bit. */
      if (!(bit24_image = (uint8_t*)malloc(width * height * 3)))
          goto finish;

      if (video_driver_pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888)
         scaler->in_fmt = SCALER_FMT_ARGB8888;
      else
         scaler->in_fmt = SCALER_FMT_RGB565;
      video_frame_convert_to_bgr24(
         scaler,
         (uint8_t *)bit24_image,
         (const uint8_t*)data + ((int)height - 1)*pitch,
         width, height,
         (int)-pitch);
   }
   scaler_ctx_gen_reset(scaler);

   if (!bit24_image)
   {
      error = true;
      goto finish;
   }

   if (TRANSLATE_USE_BMP)
   {
      /*
        At this point, we should have a screenshot in the buffer,
        so allocate an array to contain the BMP image along with
        the BMP header as bytes, and then covert that to a
        b64 encoded array for transport in JSON.
      */
      form_bmp_header(header, width, height, false);
      if (!(bmp_buffer  = (uint8_t*)malloc(width * height * 3 + 54)))
         goto finish;

      memcpy(bmp_buffer, header, 54 * sizeof(uint8_t));
      memcpy(bmp_buffer + 54,
            bit24_image,
            width * height * 3 * sizeof(uint8_t));
      buffer_bytes = sizeof(uint8_t) * (width * height * 3 + 54);
   }
   else
   {
      pitch        = width * 3;
      bmp_buffer   = rpng_save_image_bgr24_string(
            bit24_image + width * (height-1) * 3,
            width, height, (signed)-pitch, &buffer_bytes);
   }

   if (!(bmp64_buffer = base64((void *)bmp_buffer,
         (int)(sizeof(uint8_t) * buffer_bytes),
         &bmp64_length)))
      goto finish;

   if (!(jsonwriter = rjsonwriter_open_memory()))
      goto finish;

   rjsonwriter_raw(jsonwriter, "{", 1);
   rjsonwriter_raw(jsonwriter, " ", 1);
   rjsonwriter_add_string(jsonwriter, "image");
   rjsonwriter_raw(jsonwriter, ":", 1);
   rjsonwriter_raw(jsonwriter, " ", 1);
   rjsonwriter_add_string_len(jsonwriter, bmp64_buffer, bmp64_length);

   /* Form request... */
   if (system_label)
   {
      rjsonwriter_raw(jsonwriter, ",", 1);
      rjsonwriter_raw(jsonwriter, " ", 1);
      rjsonwriter_add_string(jsonwriter, "label");
      rjsonwriter_raw(jsonwriter, ":", 1);
      rjsonwriter_raw(jsonwriter, " ", 1);
      rjsonwriter_add_string(jsonwriter, system_label);
   }

   rjsonwriter_raw(jsonwriter, ",", 1);
   rjsonwriter_raw(jsonwriter, " ", 1);
   rjsonwriter_add_string(jsonwriter, "state");
   rjsonwriter_raw(jsonwriter, ":", 1);
   rjsonwriter_raw(jsonwriter, " ", 1);
   rjsonwriter_raw(jsonwriter, "{", 1);
   rjsonwriter_raw(jsonwriter, " ", 1);
   rjsonwriter_add_string(jsonwriter, "paused");
   rjsonwriter_raw(jsonwriter, ":", 1);
   rjsonwriter_raw(jsonwriter, " ", 1);
   rjsonwriter_rawf(jsonwriter, "%u", (paused ? 1 : 0));
   {
      static const char* state_labels[] = { "b", "y", "select", "start", "up", "down", "left", "right", "a", "x", "l", "r", "l2", "r2", "l3", "r3" };
      int i;
      for (i = 0; i < ARRAY_SIZE(state_labels); i++)
      {
         rjsonwriter_raw(jsonwriter, ",", 1);
         rjsonwriter_raw(jsonwriter, " ", 1);
         rjsonwriter_add_string(jsonwriter, state_labels[i]);
         rjsonwriter_raw(jsonwriter, ":", 1);
         rjsonwriter_raw(jsonwriter, " ", 1);
#ifdef HAVE_ACCESSIBILITY
         rjsonwriter_rawf(jsonwriter, "%u",
               (input_st->ai_gamepad_state[i] ? 1 : 0));
#else
         rjsonwriter_rawf(jsonwriter, "%u", 0);
#endif
      }
   }
   rjsonwriter_raw(jsonwriter, " ", 1);
   rjsonwriter_raw(jsonwriter, "}", 1);
   rjsonwriter_raw(jsonwriter, " ", 1);
   rjsonwriter_raw(jsonwriter, "}", 1);

   if (!(json_buffer = rjsonwriter_get_memory_buffer(jsonwriter, NULL)))
      goto finish; /* ran out of memory */

#ifdef DEBUG
   if (access_st->ai_service_auto != 2)
      RARCH_LOG("Request size: %d\n", bmp64_length);
#endif
   {
      size_t _len;
      char new_ai_service_url[PATH_MAX_LENGTH];
      char separator                  = '?';
      unsigned ai_service_source_lang = settings->uints.ai_service_source_lang;
      unsigned ai_service_target_lang = settings->uints.ai_service_target_lang;
      const char *ai_service_url      = settings->arrays.ai_service_url;

      _len = strlcpy(new_ai_service_url,
            ai_service_url, sizeof(new_ai_service_url));

      /* if query already exists in url, then use &'s instead */
      if (strrchr(new_ai_service_url, '?'))
          separator = '&';

      /* source lang */
      if (ai_service_source_lang != TRANSLATION_LANG_DONT_CARE)
      {
         const char *lang_source = ai_service_get_str(
               (enum translation_lang)ai_service_source_lang);

         if (!string_is_empty(lang_source))
         {
            new_ai_service_url[_len  ] = separator;
            new_ai_service_url[_len+1] = '\0';
                                         strlcat(
                  new_ai_service_url, "source_lang=",
                  sizeof(new_ai_service_url));
            _len                       = strlcat(
                  new_ai_service_url, lang_source,
                  sizeof(new_ai_service_url));
            separator                  = '&';
         }
      }

      /* target lang */
      if (ai_service_target_lang != TRANSLATION_LANG_DONT_CARE)
      {
         const char *lang_target = ai_service_get_str(
               (enum translation_lang)ai_service_target_lang);

         if (!string_is_empty(lang_target))
         {
            new_ai_service_url[_len  ] = separator;
            new_ai_service_url[_len+1] = '\0';
                                         strlcat(
                  new_ai_service_url, "target_lang=",
                  sizeof(new_ai_service_url));
            _len                       = strlcat(
                  new_ai_service_url, lang_target,
                  sizeof(new_ai_service_url));
            separator                  = '&';
         }
      }

      /* mode */
      {
         unsigned ai_service_mode      = settings->uints.ai_service_mode;
         /*"image" is included for backwards compatability with
          * vgtranslate < 1.04 */

         new_ai_service_url[_len  ] = separator;
         new_ai_service_url[_len+1] = '\0';
         strlcat(
               new_ai_service_url, "output=",
               sizeof(new_ai_service_url));

         switch (ai_service_mode)
         {
            case 2:
               strlcat(new_ai_service_url, "text",
                     sizeof(new_ai_service_url));
               break;
            case 1:
            case 3:
               strlcat(new_ai_service_url, "sound,wav",
                     sizeof(new_ai_service_url));
               if (ai_service_mode == 1)
                  break;
               /* fall-through intentional for ai_service_mode == 3 */
            case 0:
               strlcat(new_ai_service_url, "image,png",
                     sizeof(new_ai_service_url));
#ifdef HAVE_GFX_WIDGETS
               if (     video_st->poke
                     && video_st->poke->load_texture
                     && video_st->poke->unload_texture)
                  strlcat(new_ai_service_url, ",png-a",
                        sizeof(new_ai_service_url));
#endif
               break;
            default:
               break;
         }

      }
#ifdef DEBUG
      if (access_st->ai_service_auto != 2)
         RARCH_LOG("SENDING... %s\n", new_ai_service_url);
#endif
      task_push_http_post_transfer(new_ai_service_url,
            json_buffer, true, NULL, handle_translation_cb, NULL);
   }

   error = false;
finish:
   if (bit24_image_prev)
      free(bit24_image_prev);
   if (bit24_image)
      free(bit24_image);

   if (scaler)
      free(scaler);

   if (bmp_buffer)
      free(bmp_buffer);

   if (bmp64_buffer)
      free(bmp64_buffer);
   if (system_label)
      free(system_label);
   if (jsonwriter)
      rjsonwriter_free(jsonwriter);
   return !error;
}

#ifdef HAVE_ACCESSIBILITY
bool is_narrator_running(bool accessibility_enable)
{
   access_state_t *access_st = access_state_get_ptr();
   if (is_accessibility_enabled(
            accessibility_enable,
            access_st->enabled))
   {
      frontend_ctx_driver_t *frontend = 
         frontend_state_get_ptr()->current_frontend_ctx;
      if (frontend && frontend->is_narrator_running)
         return frontend->is_narrator_running();
   }
   return true;
}
#endif
