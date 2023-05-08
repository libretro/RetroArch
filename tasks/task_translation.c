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
#include <formats/rjson_helpers.h>
#include <gfx/scaler/pixconv.h>
#include <gfx/scaler/scaler.h>
#include <gfx/video_frame.h>
#include <retro_timers.h>
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
#include "../msg_hash.h"

#include "tasks_internal.h"

static const char* ACCESS_INPUT_LABELS[] = { 
   "b", "y", "select", "start", "up", "down", "left", "right", 
   "a", "x", "l", "r", "l2", "r2", "l3", "r3" 
};

static const char* ACCESS_RESPONSE_KEYS[] = { 
   "image", "sound", "text", "error", "auto", "press", "text_position"
};

typedef struct {
   uint8_t *data;
   unsigned size;
   unsigned width;
   unsigned height;
   
   unsigned content_x;
   unsigned content_y;
   unsigned content_width;
   unsigned content_height;
   unsigned viewport_width;
   unsigned viewport_height;
} access_frame_t;

typedef struct {
   int length;
   char *data;
   char format[4];
} access_base64_t;

typedef struct {
   bool paused;
   char *inputs;
} access_request_t;

typedef struct {
   char *image;
   int image_size;
#ifdef HAVE_AUDIOMIXER
   void *sound;
   int sound_size;
#endif
   char *error;
   char *text;
   int text_position;
   char *recall;
   char *input;
} access_response_t;

/* UTILITIES ---------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

/**
 * Returns true if the accessibility narrator is currently playing audio.
 */
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
   return false;
}
#endif

/**
 * Returns true if array {a} and {b}, both of the same size {size} are equal.
 * This method prevents a potential bug with memcmp on some platforms.
 */
static bool u8_array_equal(uint8_t *a, uint8_t *b, int size)
{
   int i = 0;
   for (; i < size; i++)
   {
      if (a[i] != b[i])
         return false;
   }
   return true;
}

/**
 * Helper method to simplify accessibility speech usage. This method will only
 * use TTS to read the provided text if accessibility has been enabled in the
 * frontend or by RetroArch's internal override mechanism.
 */
static void accessibility_speak(const char *text)
{
#ifdef HAVE_ACCESSIBILITY
   settings_t *settings = config_get_ptr();
   unsigned speed       = settings->uints.accessibility_narrator_speech_speed;
   bool narrator_on     = settings->bools.accessibility_enable;
   
   accessibility_speak_priority(narrator_on, speed, text, 10);
#endif
}

/**
 * Speaks the provided text using TTS. This only happens if the narrator has 
 * been enabled or the service is running in Narrator mode, in which case it
 * must been used even if the user has disabled it.
 */
static void translation_speak(const char *text)
{
#ifdef HAVE_ACCESSIBILITY
   settings_t *settings       = config_get_ptr();
   access_state_t *access_st  = access_state_get_ptr();
   
   unsigned mode     = settings->uints.ai_service_mode;
   unsigned speed    = settings->uints.accessibility_narrator_speech_speed;
   bool narrator_on  = settings->bools.accessibility_enable;

   /* Force the use of the narrator in Narrator modes (TTS) */
   if (mode == 2 || mode == 4 || mode == 5 || narrator_on || access_st->enabled)
     accessibility_speak_priority(true, speed, text, 10);
#endif
}

/**
 * Displays the given message on screen and returns true. Returns false if no
 * {message} is provided (i.e. it is NULL). The message will be displayed as
 * information or error depending on the {error} boolean. In addition, it will
 * be logged if {error} is true, or if this is a debug build. The message will
 * also be played by the accessibility narrator if the user enabled it.
 */
static bool translation_user_message(const char *message, bool error)
{
   if (message)
   {
      accessibility_speak(message);
      runloop_msg_queue_push(
            message, 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, 
            error ? MESSAGE_QUEUE_CATEGORY_ERROR : MESSAGE_QUEUE_CATEGORY_INFO);
      if (error)
         RARCH_ERR("[Translate] %s\n", message);
#ifdef DEBUG
      else
         RARCH_LOG("[Translate] %s\n", message);
#endif
      return true;
   }
   return false;
}

/**
 * Displays the given hash on screen and returns true. Returns false if no
 * {hash} is provided (i.e. it is NULL). The message will be displayed as
 * information or error depending on the {error} boolean. In addition, it will
 * be logged if {error} is true, or if this is a debug build. The message will
 * also be played by the accessibility narrator if the user enabled it.
 */
static bool translation_hash_message(enum msg_hash_enums hash, bool error)
{
   if (hash)
   {
      const char *message  = msg_hash_to_str(hash);
      const char *intl     = msg_hash_to_str_us(hash);
      
      accessibility_speak(message);
      runloop_msg_queue_push(
            message, 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, 
            error ? MESSAGE_QUEUE_CATEGORY_ERROR : MESSAGE_QUEUE_CATEGORY_INFO);
      if (error)
         RARCH_ERR("[Translate] %s\n", intl);
#ifdef DEBUG
      else
         RARCH_LOG("[Translate] %s\n", intl);
#endif
      return true;
   }
   return false;
}

/**
 * Displays the given message on screen and returns true. Returns false if no
 * {message} is provided (i.e. it is NULL). The message will be displayed as
 * an error and it will be logged. The message will also be played by the 
 * accessibility narrator if the user enabled it.
 */
static INLINE bool translation_user_error(const char *message)
{
   return translation_user_message(message, true);
}

/**
 * Displays the given message on screen and returns true. Returns false if no
 * {message} is provided (i.e. it is NULL). The message will be displayed as
 * information and will only be logged if this is a debug build. The message 
 * will also be played by the accessibility narrator if the user enabled it.
 */
static INLINE bool translation_user_info(const char *message)
{
   return translation_user_message(message, false);
}

/**
 * Displays the given hash on screen and returns true. Returns false if no
 * {hash} is provided (i.e. it is NULL). The message will be displayed as
 * an error and it will be logged. The message will also be played by the 
 * accessibility narrator if the user enabled it.
 */
static INLINE bool translation_hash_error(enum msg_hash_enums hash)
{
   return translation_hash_message(hash, true);
}

/**
 * Displays the given hash on screen and returns true. Returns false if no
 * {hash} is provided (i.e. it is NULL). The message will be displayed as
 * information and will only be logged if this is a debug build. The message 
 * will also be played by the accessibility narrator if the user enabled it.
 */
static INLINE bool translation_hash_info(enum msg_hash_enums hash)
{
   return translation_hash_message(hash, false);
}

/**
 * Releases all data held by the service and stops it as soon as possible.
 * If {inform} is true, a message will be displayed to the user if the service
 * was running in automatic mode to warn them that it is now stopping.
 */
void translation_release(bool inform)
{
#ifdef HAVE_GFX_WIDGETS
   dispgfx_widget_t *p_dispwidget = dispwidget_get_ptr();
#endif
   access_state_t *access_st      = access_state_get_ptr();
   unsigned service_auto_prev     = access_st->ai_service_auto;
   access_st->ai_service_auto     = 0;
   
#ifdef DEBUG
   RARCH_LOG("[Translate]: AI Service is now stopping.\n");
#endif

   if (access_st->request_task)
      task_set_cancelled(access_st->request_task, true);
   if (access_st->response_task)
      task_set_cancelled(access_st->response_task, true);
   
#ifdef HAVE_THREADS
   if (access_st->image_lock)
   {
      slock_lock(access_st->image_lock);
#endif
      if (access_st->last_image)
         free(access_st->last_image);
   
      access_st->last_image      = NULL;
      access_st->last_image_size = 0;
      
#ifdef HAVE_THREADS
      slock_unlock(access_st->image_lock);
   }
#endif

#ifdef HAVE_GFX_WIDGETS
   if (p_dispwidget->ai_service_overlay_state != 0)
      gfx_widgets_ai_service_overlay_unload();
#endif

   if (inform && service_auto_prev != 0)
      translation_hash_info(MSG_AI_AUTO_MODE_DISABLED);
}

/**
 * Returns the string representation of the translation language enum value.
 */
static const char* ai_service_get_str(enum translation_lang id)
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

/* AUTOMATION --------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

/**
 * Handler invoking the next automatic request. This method simply waits for
 * any previous request to terminate before re-invoking the translation service.
 * By delegating this to a task handler we can safely do so in the task thread
 * instead of hogging the main thread.
 */
static void call_auto_translate_hndl(retro_task_t *task)
{
   int *mode_ptr               = (int*)task->user_data;
   uint32_t runloop_flags      = runloop_get_flags();
   access_state_t *access_st   = access_state_get_ptr();
   settings_t *settings        = config_get_ptr();

   if (task_get_cancelled(task))
      goto finish;

   switch (*mode_ptr)
   {
      case 1: /* Speech Mode   */
#ifdef HAVE_AUDIOMIXER
         if (!audio_driver_is_ai_service_speech_running())
            goto finish;
#endif
         break;
      case 2: /* Narrator Mode    */
      case 3: /* Text Mode        */
      case 4: /* Text + Narrator  */
      case 5: /* Image + Narrator */
#ifdef HAVE_ACCESSIBILITY
         if (!is_narrator_running(settings->bools.accessibility_enable))
            goto finish;
#endif
         break;
      default:
         goto finish;
   }
   return;

finish:
   task_set_finished(task, true);

   if (task->user_data)
      free(task->user_data);

   /* Final check to see if the user did not disable the service altogether */
   if (access_st->ai_service_auto != 0)
   {
      bool was_paused = runloop_flags & RUNLOOP_FLAG_PAUSED;
      command_event(CMD_EVENT_AI_SERVICE_CALL, &was_paused);
   }
}

/**
 * Invokes the next automatic request. This method delegates the invokation to
 * a task to allow for threading. The task will only execute after the polling
 * delay configured by the user has been honored since the last request.
 */
static void call_auto_translate_task(settings_t *settings)
{
   int* mode                  = NULL;
   access_state_t *access_st  = access_state_get_ptr();
   int ai_service_mode        = settings->uints.ai_service_mode;
   unsigned delay             = settings->uints.ai_service_poll_delay;
   retro_task_t *task         = task_init();
   if (!task)
      return;

   mode  = (int*)malloc(sizeof(int));
   *mode = ai_service_mode;
   
   task->handler     = call_auto_translate_hndl;
   task->user_data   = mode;
   task->mute        = true;
   task->when        = access_st->last_call + (delay * 1000);
   task_queue_push(task);
}

/* RESPONSE ----------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

/**
 * Parses the JSON returned by the translation server and returns structured
 * data. May return NULL if the parsing cannot be completed or the JSON is
 * malformed. If unsupported keys are provided in the JSON, they will simply
 * be ignored. Only the available data will be populated in the returned object
 * and everything else will be zero-initialized.
 */
static access_response_t* parse_response_json(http_transfer_data_t *data)
{
   int key                       = -1;
   rjson_t* json                 = NULL;
   char* image_data              = NULL;
   int image_size                = 0;
#ifdef HAVE_AUDIOMIXER
   void *sound_data              = NULL;
   int sound_size                = 0;
#endif
   access_response_t *response   = NULL;
   bool empty                    = true;
   enum rjson_type type;
   
   if (!data || !data->data)
      goto finish;
   if (!(json = rjson_open_buffer(data->data, data->len)))
      goto finish;
   if (!(response = (access_response_t*)calloc(1, sizeof(access_response_t))))
      goto finish;

   for (;;)
   {
      size_t length        = 0;
      const char *string   = NULL;
      type                 = rjson_next(json);

      if (type == RJSON_DONE || type == RJSON_ERROR)
         break;
      if (rjson_get_context_type(json) != RJSON_OBJECT)
         continue;
      
      if (type == RJSON_STRING && (rjson_get_context_count(json) & 1) == 1)
      {
         int i;
         string = rjson_get_string(json, &length);
         for (i = 0; i < ARRAY_SIZE(ACCESS_RESPONSE_KEYS) && key == -1; i++)
         {
            if (string_is_equal(string, ACCESS_RESPONSE_KEYS[i]))
               key = i;
         }
      }
      else
      {
         if (type != RJSON_STRING && key < 6)
            continue;
         else
            string = rjson_get_string(json, &length);
         
         switch (key)
         {
            case 0: /* image */
               response->image = (length == 0) ? NULL : (char*)unbase64(
                     string, (int)length, &response->image_size);
               break;
#ifdef HAVE_AUDIOMIXER
            case 1: /* sound */
               response->sound = (length == 0) ? NULL : (void*)unbase64(
                     string, (int)length, &response->sound_size);
               break;
#endif
            case 2: /* text */
               response->text = strdup(string);
               break;
            case 3: /* error */
               response->error = strdup(string);
               break;
            case 4: /* auto */
               response->recall = strdup(string);
               break;
            case 5: /* press */
               response->input = strdup(string);
               break;
            case 6: /* text_position */
               if (type == RJSON_NUMBER)
                  response->text_position = rjson_get_int(json);
               break;
         }
         key = -1;
      }
   }
   
   if (type == RJSON_ERROR)
   {
      RARCH_LOG("[Translate] JSON error: %s\n", rjson_get_error(json));
      translation_user_error("Service returned a malformed JSON");
      free(response);
      response = NULL;
   }
   
finish:
   if (json)
      rjson_free(json);
   else
      translation_user_error("Internal error parsing returned JSON.");
   
   return response;
}

/**
 * Parses the image data of given type and displays it using widgets. If the
 * image widget is already shown, it will be unloaded first automatically.
 * This method will disable automatic translation if the widget could not be
 * loaded to prevent further errors.
 */
#ifdef HAVE_GFX_WIDGETS
static void translation_response_image_widget(
      char *image, int image_length, enum image_type_enum *image_type)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   dispgfx_widget_t *p_dispwidget = dispwidget_get_ptr();
   access_state_t *access_st      = access_state_get_ptr();
 
   bool ai_res;
   bool gfx_widgets_paused        = video_st->flags & VIDEO_FLAG_WIDGETS_PAUSED;
   
   if (p_dispwidget->ai_service_overlay_state != 0)
      gfx_widgets_ai_service_overlay_unload();
   
   ai_res = gfx_widgets_ai_service_overlay_load(
         image, (unsigned)image_length, (*image_type));

   if (!ai_res)
   {
      translation_hash_error(MSG_AI_VIDEO_DRIVER_NOT_SUPPORTED);
      translation_release(true);
   }
   else if (gfx_widgets_paused)
   {
      /* Unpause for a frame otherwise widgets won't be displayed */
      p_dispwidget->ai_service_overlay_state = 2;
      command_event(CMD_EVENT_UNPAUSE, NULL);
   }
}
#endif

/**
 * Parses the image buffer, converting the data to the raw image format we need
 * to display the image within RetroArch. Writes the raw image data in {body} 
 * as well as its {width} and {height} as determined by the image header. 
 * Returns true if the process was successful.
 */
static bool translation_get_image_body(
      char *image, int image_size, enum image_type_enum *image_type,
      void *body, unsigned *width, unsigned *height)
{
#ifdef HAVE_RPNG
   rpng_t *rpng      = NULL;
   void *rpng_alpha  = NULL;
   int rpng_ret      = 0;
#endif
   
   if ((*image_type) == IMAGE_TYPE_BMP)
   {
      if (image_size < 55)
         return false;
      
      *width   = ((uint32_t) ((uint8_t)image[21]) << 24) 
               + ((uint32_t) ((uint8_t)image[20]) << 16) 
               + ((uint32_t) ((uint8_t)image[19]) <<  8) 
               + ((uint32_t) ((uint8_t)image[18]) <<  0);
      *height  = ((uint32_t) ((uint8_t)image[25]) << 24) 
               + ((uint32_t) ((uint8_t)image[24]) << 16) 
               + ((uint32_t) ((uint8_t)image[23]) <<  8) 
               + ((uint32_t) ((uint8_t)image[22]) <<  0);
               
      image_size = (*width) * (*height) * 3 * sizeof(uint8_t);
      body       = (void*)malloc(image_size);
      if (!body)
         return false;
      
      memcpy(body, image + 54 * sizeof(uint8_t), image_size);
      return true;
   }
   
#ifdef HAVE_RPNG
   else if ((*image_type) == IMAGE_TYPE_PNG)
   {
      if (image_size < 24)
         return false;
      if (!(rpng = rpng_alloc()))
         return false;
      
      *width   = ((uint32_t) ((uint8_t)image[16]) << 24)
               + ((uint32_t) ((uint8_t)image[17]) << 16)
               + ((uint32_t) ((uint8_t)image[18]) <<  8)
               + ((uint32_t) ((uint8_t)image[19]) <<  0);
      *height  = ((uint32_t) ((uint8_t)image[20]) << 24)
               + ((uint32_t) ((uint8_t)image[21]) << 16)
               + ((uint32_t) ((uint8_t)image[22]) <<  8)
               + ((uint32_t) ((uint8_t)image[23]) <<  0);

      rpng_set_buf_ptr(rpng, image, (size_t)image_size);
      rpng_start(rpng);
      while (rpng_iterate_image(rpng));

      do
      {
         rpng_ret = rpng_process_image(
               rpng, &rpng_alpha, (size_t)image_size, width, height);
      } while (rpng_ret == IMAGE_PROCESS_NEXT);

      /* 
       * Returned output from the png processor is an upside down RGBA
       * image, so we have to change that to RGB first. This should
       * probably be replaced with a scaler call. 
       */
      {
         int d      = 0;
         int tw, th, tc;
         unsigned ui;
         image_size = (*width) * (*height) * 3 * sizeof(uint8_t);
         body       = (void*)malloc(image_size);
         if (!body)
         {
            free(rpng_alpha);
            rpng_free(rpng);
            return false;
         }
         
         for (ui = 0; ui < (*width) * (*height) * 4; ui++)
         {
            if (ui % 4 != 3)
            {
               tc    = d % 3;
               th    = (*height) - d / (3 * (*width)) - 1;
               tw    = (d % ((*width) * 3)) / 3;
               ((uint8_t*) body)[tw * 3 + th * 3 * (*width) + tc] 
                     = ((uint8_t*)rpng_alpha)[ui];
               d++;
            }
         }
      }
      free(rpng_alpha);
      rpng_free(rpng);
      return true;
   }
#endif

   return false;
}

/**
 * Displays the raw image on screen by directly writing to the frame buffer.
 * This method may fail depending on the current video driver.
 */
 /* TODO/FIXME: Does nothing with Vulkan apparently? */
static void translation_response_image_direct(
      char *image, int image_size, enum image_type_enum *image_type)
{
   size_t pitch;
   unsigned width;
   unsigned height;
   unsigned vp_width;
   unsigned vp_height;
   
   void *image_body                                   = NULL;
   uint8_t *raw_output_data                           = NULL;
   size_t raw_output_size                             = 0;
   const void *dummy_data                             = NULL;
   struct scaler_ctx *scaler                          = NULL;
   video_driver_state_t *video_st                     = video_state_get_ptr();
   const enum retro_pixel_format video_driver_pix_fmt = video_st->pix_fmt;
   
   if (!(translation_get_image_body(
         image, image_size, image_type, image_body, &width, &height)))
      goto finish;

   if (!(scaler = (struct scaler_ctx*)calloc(1, sizeof(struct scaler_ctx))))
      goto finish;
   
   video_driver_cached_frame_get(&dummy_data, &vp_width, &vp_height, &pitch);
   if (!vp_width || !vp_height)
      goto finish;
   
   if (dummy_data == RETRO_HW_FRAME_BUFFER_VALID)
   {
      /* In this case, we used the viewport to grab the image and translate it, 
       * and we have the translated image in the image_body buffer. */
      translation_user_error("Video driver unsupported for hardware frame.");
      translation_release(true);
      goto finish;
   }

   /* 
    * The assigned pitch may not be reliable. The width of the video frame can 
    * change during run-time, but the pitch may not, so we just assign it as 
    * the width times the byte depth. 
    */
   if (video_driver_pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888)
   {
      raw_output_size      = vp_width * vp_height * 4 * sizeof(uint8_t);
      raw_output_data      = (uint8_t*)malloc(raw_output_size);
      scaler->out_fmt      = SCALER_FMT_ARGB8888;
      scaler->out_stride   = vp_width * 4;
      pitch                = vp_width * 4;
   }
   else
   {
      raw_output_size      = vp_width * vp_height * 2 * sizeof(uint8_t);
      raw_output_data      = (uint8_t*)malloc(raw_output_size);
      scaler->out_fmt      = SCALER_FMT_RGB565;
      scaler->out_stride   = vp_width * 1;
      pitch                = vp_width * 2;
   }

   if (!raw_output_data)
      goto finish;

   scaler->in_fmt        = SCALER_FMT_BGR24;
   scaler->in_width      = width;
   scaler->in_height     = height;
   scaler->out_width     = vp_width;
   scaler->out_height    = vp_height;
   scaler->scaler_type   = SCALER_TYPE_POINT;
   scaler_ctx_gen_filter(scaler);
   
   scaler->in_stride     = -1 * vp_width * 3;

   scaler_ctx_scale_direct(
         scaler, raw_output_data,
         (uint8_t*)image_body + (height - 1) * width * 3);
   video_driver_frame(raw_output_data, width, height, pitch);
   
finish:
   if (image_body)
      free(image_body);
   if (scaler)
      free(scaler);
   if (raw_output_data)
      free(raw_output_data);
}

/**
 * Parses image data received by the server following a translation request.
 * This method assumes that image data is present in the response, it cannot
 * be null. If widgets are supported, this method will prefer using them to
 * overlay the picture on top of the video, otherwise it will try to write the
 * data directly into the frame buffer, which is much less reliable.
 */
static void translation_response_image_hndl(retro_task_t *task)
{
   /* 
    * TODO/FIXME: Moved processing to the callback to fix an issue with
    * texture loading off the main thread in OpenGL. I'm leaving the original
    * structure here so we can move back to the handler if it becomes possible
    * in the future.
    */
   task_set_finished(task, true);
}

/**
 * Callback invoked once the image data received from the server has been
 * processed and eventually displayed. This is necessary to ensure that the
 * next automatic request will be invoked once the task is finished.
 */
static void translation_response_image_cb(
      retro_task_t *task, void *task_data, void *user_data, const char *error)
{
   settings_t* settings          = config_get_ptr();
   access_state_t *access_st     = access_state_get_ptr();
   
   enum image_type_enum image_type;
   access_response_t *response      = (access_response_t*)task->user_data;
   video_driver_state_t *video_st   = video_state_get_ptr();
   
   if (task_get_cancelled(task) || response->image_size < 4)
      goto finish;
   
   if (     response->image[0] == 'B' 
         && response->image[1] == 'M')
      image_type = IMAGE_TYPE_BMP;
#ifdef HAVE_RPNG
   else if (response->image[1] == 'P' 
         && response->image[2] == 'N' 
         && response->image[3] == 'G')
      image_type = IMAGE_TYPE_PNG;
#endif
   else
   {
      translation_user_error("Service returned an unsupported image type.");
      translation_release(true);
      goto finish;
   }
   
#ifdef HAVE_GFX_WIDGETS
   if (     video_st->poke
         && video_st->poke->load_texture
         && video_st->poke->unload_texture)
      translation_response_image_widget(
            response->image, response->image_size, &image_type);
   else
#endif
      translation_response_image_direct(
            response->image, response->image_size, &image_type);
   
finish:
   free(response->image);
   free(response);

   if (access_st->ai_service_auto != 0)
      call_auto_translate_task(settings);
}

/**
 * Processes text data received by the server following a translation request.
 * Does nothing if the response does not contain any text data (NULL). Text
 * is either forcibly read by the narrator, even if it is disabled in the 
 * front-end (Narrator Mode) or displayed on screen (in Text Mode). In the 
 * later, it will only be read if the front-end narrator is enabled.
 */
static void translation_response_text(access_response_t *response)
{
   settings_t *settings       = config_get_ptr();
   unsigned service_mode      = settings->uints.ai_service_mode;
   access_state_t *access_st  = access_state_get_ptr();
   
   if (     (!response->text || string_is_empty(response->text))
         && (service_mode == 2 || service_mode == 3 || service_mode == 4)
         && access_st->ai_service_auto == 0)
   {
      translation_hash_info(MSG_AI_NOTHING_TO_TRANSLATE);
      return;
   }
   
   if (response->text)
   {
      /* The text should be displayed on screen in Text or Text+Narrator mode */
      if (service_mode == 3 || service_mode == 4)
      {
#ifdef HAVE_GFX_WIDGETS
         if (settings->bools.menu_enable_widgets)
         {
            dispgfx_widget_t *p_dispwidget = dispwidget_get_ptr();
            
            if (p_dispwidget->ai_service_overlay_state == 1)
               gfx_widgets_ai_service_overlay_unload();
            
            strlcpy(p_dispwidget->ai_service_text, response->text, 255);
            
            if (response->text_position > 0)
               p_dispwidget->ai_service_text_position 
                     = (unsigned)response->text_position;
            else
               p_dispwidget->ai_service_text_position = 0;
            
            p_dispwidget->ai_service_overlay_state = 1;
         }
         else
         {
#endif
            /* 
             * TODO/FIXME: Obviously this will not be as good as using widgets, 
             * since messages run on a timer but it's an alternative at least.
             * Maybe split the message here so it fits the viewport.
             */
            runloop_msg_queue_push(
                  response->text, 2, 180, 
                  true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, 
                  MESSAGE_QUEUE_CATEGORY_INFO);
                  
#ifdef HAVE_GFX_WIDGETS
         }
#endif
      }
      translation_speak(&response->text[0]);
      free(response->text);
   }
}

/**
 * Processes audio data received by the server following a translation request.
 * Does nothing if the response does not contain any audio data (NULL). Audio
 * data is simply played as soon as possible using the audio driver.
 */
static void translation_response_sound(access_response_t *response)
{
#ifdef HAVE_AUDIOMIXER
   if (response->sound)
   {
      audio_mixer_stream_params_t params;

      params.volume               = 1.0f;
      /* user->slot_selection_type; */
      params.slot_selection_type  = AUDIO_MIXER_SLOT_SELECTION_MANUAL; 
      params.slot_selection_idx   = 10;
      /* user->stream_type; */
      params.stream_type          = AUDIO_STREAM_TYPE_SYSTEM; 
      params.type                 = AUDIO_MIXER_TYPE_WAV;
      params.state                = AUDIO_STREAM_STATE_PLAYING;
      params.buf                  = response->sound;
      params.bufsize              = response->sound_size;
      params.cb                   = NULL;
      params.basename             = NULL;

      audio_driver_mixer_add_stream(&params);
      free(response->sound);
   }
#endif
}

/**
 * Processes input data received by the server following a translation request.
 * Does nothing if the response does not contain any input data (NULL). This
 * method will try to forcibly press all the retropad keys listed in the input
 * string (comma-separated).
 */
static void translation_response_input(access_response_t *response)
{
   if (response->input)
   {
#ifdef HAVE_ACCESSIBILITY
      input_driver_state_t *input_st   = input_state_get_ptr();
#endif
      int length                       = strlen(response->input);
      char *token                      = strtok(response->input, ",");

      while (token)
      {
         if (string_is_equal(token, "pause"))
            command_event(CMD_EVENT_PAUSE, NULL);
         else if (string_is_equal(token, "unpause"))
            command_event(CMD_EVENT_UNPAUSE, NULL);
#ifdef HAVE_ACCESSIBILITY
         else
         {
            int i      = 0;
            bool found = false;
            
            for (; i < ARRAY_SIZE(ACCESS_INPUT_LABELS) && !found; i++)
               found = string_is_equal(ACCESS_INPUT_LABELS[i], response->input);
            
            if (found)
               input_st->ai_gamepad_state[i] = 2;
         }
#endif
         token = strtok(NULL, ",");
      }
      free(response->input);
   }
}

/**
 * Callback invoked when the server responds to our translation request. If the
 * service is still running by then, this method will parse the JSON payload
 * and process the data, eventually re-invoking the translation service for
 * a new request if the server allowed automatic translation.
 */
static void translation_response_cb(
      retro_task_t *task, void *task_data, void *user_data, const char *error)
{
   http_transfer_data_t *data       = (http_transfer_data_t*)task_data;
   access_state_t *access_st        = access_state_get_ptr();
   settings_t *settings             = config_get_ptr();
   access_response_t *response      = NULL;
   bool auto_mode_prev              = access_st->ai_service_auto;
   unsigned service_mode            = settings->uints.ai_service_mode;
   
   /* We asked the service to stop by calling translation_release, so bail */
   if (!access_st->last_image)
      goto finish;
   if (translation_user_error(error))
      goto abort;
   if (!(response = parse_response_json(data)))
      goto abort;
   if (translation_user_error(response->error))
      goto abort;

   access_st->ai_service_auto = (response->recall == NULL) ? 0 : 1;
   if (auto_mode_prev != access_st->ai_service_auto)
      translation_hash_info(auto_mode_prev 
            ? MSG_AI_AUTO_MODE_DISABLED : MSG_AI_AUTO_MODE_ENABLED);

   /* 
    * We want to skip the data on auto=continue, unless automatic translation
    * has just been enabled, meaning data must be displayed again to the user.
    */
   if (     !string_is_equal(response->recall, "continue") 
         || (auto_mode_prev == 0 && access_st->ai_service_auto == 1))
   {
#ifdef HAVE_GFX_WIDGETS
      dispgfx_widget_t *p_dispwidget = dispwidget_get_ptr();
      if (p_dispwidget->ai_service_overlay_state != 0)
         gfx_widgets_ai_service_overlay_unload();
#endif
      translation_response_text(response);
      translation_response_sound(response);
      translation_response_input(response);
   
      if (response->image)
      {
         retro_task_t *task = task_init();
         if (!task)
            goto finish;
         
         task->handler              = translation_response_image_hndl;
         task->callback             = translation_response_image_cb;
         task->user_data            = response;
         task->mute                 = true;
         access_st->response_task   = task;
         task_queue_push(task);

         /* Leave memory clean-up and auto callback to the task itself */
         return;
      }
      else if (access_st->ai_service_auto == 0
            && (service_mode == 0 || service_mode == 5))
         translation_hash_info(MSG_AI_NOTHING_TO_TRANSLATE);
   }
   goto finish;
   
abort:
   translation_release(true);
   if (response && response->error)
      free(response->error);

finish:
   if (response)
   {
      if (response->image)
         free(response->image);
      if (response->recall)
         free(response->recall);
      free(response);
      
      if (access_st->ai_service_auto != 0)
         call_auto_translate_task(settings);
   }
}

/* REQUEST ------------------------------------------------------------------ */
/* -------------------------------------------------------------------------- */

/**
 * Grabs and returns a frame from the video driver. If the frame buffer cannot
 * be accessed, this method will try to obtain a capture of the viewport as a
 * fallback, although this frame may be altered by any filter or shader enabled
 * by the user. Returns null if both methods fail.
 */
static access_frame_t* translation_grab_frame()
{
   size_t pitch;
   struct video_viewport vp               = {0};
   const void *data                       = NULL;
   uint8_t *bit24_image_prev              = NULL;
   struct scaler_ctx *scaler              = NULL;
   access_frame_t *frame                  = NULL;
   video_driver_state_t *video_st         = video_state_get_ptr();
   const enum retro_pixel_format pix_fmt  = video_st->pix_fmt;
      
   if (!(scaler = (struct scaler_ctx*)calloc(1, sizeof(struct scaler_ctx))))
      goto finish;   
   if (!(frame = (access_frame_t*)malloc(sizeof(access_frame_t))))
      goto finish;
   
   video_driver_cached_frame_get(&data, &frame->width, &frame->height, &pitch);
   if (!data)
      goto finish;

   video_driver_get_viewport_info(&vp);
   if (!vp.width || !vp.height)
      goto finish;
   
   frame->content_x        = vp.x;
   frame->content_y        = vp.y;
   frame->content_width    = vp.width;
   frame->content_height   = vp.height;
   frame->viewport_width   = vp.full_width;
   frame->viewport_height  = vp.full_height;
   frame->size             = frame->width * frame->height * 3;
   
   if (!(frame->data = (uint8_t*)malloc(frame->size)))
      goto finish;

   if (data == RETRO_HW_FRAME_BUFFER_VALID)
   {
      /* Direct frame capture failed, fallback on viewport capture */
      if (!(bit24_image_prev = (uint8_t*)malloc(vp.width * vp.height * 3)))
         goto finish;

      if (!video_driver_read_viewport(bit24_image_prev, false))
      {
         translation_user_error("Could not read viewport.");
         translation_release(true);
         goto finish;
      }

      /* TODO: Rescale down to regular resolution */
      scaler->in_fmt      = SCALER_FMT_BGR24;
      scaler->out_fmt     = SCALER_FMT_BGR24;
      scaler->scaler_type = SCALER_TYPE_POINT;
      scaler->in_width    = vp.width;
      scaler->in_height   = vp.height;
      scaler->out_width   = frame->width;
      scaler->out_height  = frame->height;
      scaler_ctx_gen_filter(scaler);

      scaler->in_stride   = vp.width * 3;
      scaler->out_stride  = frame->width * 3;
      scaler_ctx_scale_direct(scaler, frame->data, bit24_image_prev);
   }
   else
   {
      /* This is a software core, so just change the pixel format to 24-bit */
      if (pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888)
         scaler->in_fmt = SCALER_FMT_ARGB8888;
      else
         scaler->in_fmt = SCALER_FMT_RGB565;
      
      video_frame_convert_to_bgr24(
         scaler, frame->data, (const uint8_t*)data, 
         frame->width, frame->height, (int)pitch);
   }
   scaler_ctx_gen_reset(scaler);
   
finish:
   if (bit24_image_prev)
      free(bit24_image_prev);
   if (scaler)
      free(scaler);

   if (frame)
   {
      if (frame->data)
         return frame;
      else
         free(frame);
   }
   return NULL;
}

/**
 * Returns true if the {frame} passed in parameter is a duplicate of the last
 * frame the service was invoked on. This method effectively helps to prevent 
 * the service from spamming the server with the same request over and over 
 * again when running in automatic mode. This method will also save the image
 * in the {frame} structure as the new last image for the service.
 */
static bool translation_dupe_fail(access_frame_t *frame)
{
   access_state_t *access_st  = access_state_get_ptr();
   bool size_equal            = (frame->size == access_st->last_image_size);
   bool has_failed            = false;
   
#ifdef HAVE_THREADS
   slock_lock(access_st->image_lock);
#endif
   if (access_st->last_image && access_st->ai_service_auto != 0)
   {
      if (     size_equal
            && u8_array_equal(frame->data, access_st->last_image, frame->size))
         has_failed = true;
   }
   
   /* Init last image or reset buffer size if image size changed */
   if (!has_failed && (!access_st->last_image || !size_equal))
   {
      if (access_st->last_image)
         free(access_st->last_image);

      access_st->last_image_size  = frame->size;
      if (!(access_st->last_image = (uint8_t*)malloc(frame->size)))
         has_failed = true;
   }
   
   if (!has_failed)
      memcpy(access_st->last_image, frame->data, frame->size);

#ifdef HAVE_THREADS
   slock_unlock(access_st->image_lock);
#endif
   return has_failed;
}

/**
 * Converts and returns the {frame} as a base64 encoded PNG or BMP. The 
 * selected image type will be available in the returned object, and will 
 * favor PNG if possible. Returns NULL on failure.
 */
static access_base64_t* translation_frame_encode(access_frame_t *frame)
{
   uint8_t header[54];
   uint8_t *buffer         = NULL;
   uint64_t bytes          = 0;
   access_base64_t *encode = NULL;
   
   if (!(encode = (access_base64_t*)malloc(sizeof(access_base64_t))))
      goto finish;
   
#ifdef HAVE_RPNG
   strcpy(encode->format, "png");
   buffer = rpng_save_image_bgr24_string(
         frame->data, frame->width, frame->height, 
         frame->width * 3, &bytes);
#else
   strcpy(encode->format, "bmp");
   form_bmp_header(header, frame->width, frame->height, false);
   if (!(buffer = (uint8_t*)malloc(frame->size + 54)))
      goto finish;

   memcpy(buffer, header, 54 * sizeof(uint8_t));
   memcpy(buffer + 54, frame->data, frame->size * sizeof(uint8_t));
   bytes = sizeof(uint8_t) * (frame->size + 54);
#endif

   encode->data = base64(
         (void*)buffer, (int)(bytes * sizeof(uint8_t)), &encode->length);

finish:
   if (buffer)
      free(buffer);
   
   if (encode->data)
      return encode;
   else
      free(encode);

   return NULL;
}

/**
 * Returns a newly allocated string describing the content and core currently
 * running. The string will contains the name of the core (or 'core') followed
 * by a double underscore (_) and the name of the content. Returns NULL on 
 * failure.
 */
static char* translation_get_content_label()
{
   const char *label                 = NULL;
   char* system_label                = NULL;
   core_info_t *core_info            = NULL;
   
   core_info_get_current_core(&core_info);
   if (core_info)
   {
      const struct playlist_entry *entry  = NULL;
      playlist_t *current_playlist        = playlist_get_cached();
      const char *system_id;
      size_t system_id_len;
      size_t label_len;
      
      system_id      = (core_info->system_id) ? core_info->system_id : "core";
      system_id_len  = strlen(system_id);

      if (current_playlist)
      {
         playlist_get_index_by_path(
               current_playlist, path_get(RARCH_PATH_CONTENT), &entry);

         if (entry && !string_is_empty(entry->label))
            label = entry->label;
      }

      if (!label)
         label = path_basename(path_get(RARCH_PATH_BASENAME));
      
      label_len          = strlen(label);
      if (!(system_label = (char*)malloc(label_len + system_id_len + 3)))
         return NULL;
      
      memcpy(system_label, system_id, system_id_len);
      memcpy(system_label + system_id_len, "__", 2);
      memcpy(system_label + 2 + system_id_len, label, label_len);
      system_label[system_id_len + 2 + label_len] = '\0';
   }
   
   return system_label;
}

/**
 * Creates and returns a JSON writer containing the payload to send alongside
 * the translation request. {label} may be NULL, in which case no label will
 * be supplied in the JSON. Returns NULL if the writer cannot be initialized.
 */
static rjsonwriter_t* build_request_json(
      access_base64_t *image, access_request_t *request, 
      access_frame_t *frame, char *label)
{
   unsigned i;
   rjsonwriter_t* writer = NULL;
   
   if (!(writer = rjsonwriter_open_memory()))
      return NULL;

   rjsonwriter_add_start_object(writer);
   {
      rjsonwriter_add_string(writer, "image");
      rjsonwriter_add_colon(writer);
      rjsonwriter_add_string_len(writer, image->data, image->length);
      
      rjsonwriter_add_comma(writer);
      rjsonwriter_add_string(writer, "format");
      rjsonwriter_add_colon(writer);
      rjsonwriter_add_string(writer, image->format);
      
      rjsonwriter_add_comma(writer);
      rjsonwriter_add_string(writer, "coords");
      rjsonwriter_add_colon(writer);
      rjsonwriter_add_start_array(writer);
      {
         rjsonwriter_add_unsigned(writer, frame->content_x);
         rjsonwriter_add_comma(writer);
         rjsonwriter_add_unsigned(writer, frame->content_y);
         rjsonwriter_add_comma(writer);
         rjsonwriter_add_unsigned(writer, frame->content_width);
         rjsonwriter_add_comma(writer);
         rjsonwriter_add_unsigned(writer, frame->content_height);
      }
      rjsonwriter_add_end_array(writer);
      
      rjsonwriter_add_comma(writer);
      rjsonwriter_add_string(writer, "viewport");
      rjsonwriter_add_colon(writer);
      rjsonwriter_add_start_array(writer);
      {
         rjsonwriter_add_unsigned(writer, frame->viewport_width);
         rjsonwriter_add_comma(writer);
         rjsonwriter_add_unsigned(writer, frame->viewport_height);
      }
      rjsonwriter_add_end_array(writer);

      if (label)
      {
         rjsonwriter_add_comma(writer);
         rjsonwriter_add_string(writer, "label");
         rjsonwriter_add_colon(writer);
         rjsonwriter_add_string(writer, label);
      }

      rjsonwriter_add_comma(writer);
      rjsonwriter_add_string(writer, "state");
      rjsonwriter_add_colon(writer);
      rjsonwriter_add_start_object(writer);
      {
         rjsonwriter_add_string(writer, "paused");
         rjsonwriter_add_colon(writer);
         rjsonwriter_add_unsigned(writer, (request->paused ? 1 : 0));
         
         for (i = 0; i < ARRAY_SIZE(ACCESS_INPUT_LABELS); i++)
         {
            rjsonwriter_add_comma(writer);
            rjsonwriter_add_string(writer, ACCESS_INPUT_LABELS[i]);
            rjsonwriter_add_colon(writer);
            rjsonwriter_add_unsigned(writer, request->inputs[i]);
         }      
         rjsonwriter_add_end_object(writer);
      }
      rjsonwriter_add_end_object(writer);
   }
 
   return writer;
}

/**
 * Writes in the provided {buffer} the URL for the translation request. The 
 * buffer is guaranteed to contain the server URL as well as an 'output' param
 * specifying the accepted data types for this service.
 */
static void build_request_url(char *buffer, size_t length, settings_t *settings)
{
   char token[2];
   bool poke_supported              = false;
   unsigned service_source_lang     = settings->uints.ai_service_source_lang;
   unsigned service_target_lang     = settings->uints.ai_service_target_lang;
   const char *service_url          = settings->arrays.ai_service_url;
   unsigned ai_service_mode         = settings->uints.ai_service_mode;
#ifdef HAVE_GFX_WIDGETS
   video_driver_state_t *video_st   = video_state_get_ptr();
   poke_supported                   = video_st->poke
                                   && video_st->poke->load_texture
                                   && video_st->poke->unload_texture;
#endif
      
   strlcpy(buffer, service_url, length);
   
   token[1] = '\0';
   if (strrchr(buffer, '?'))
      token[0] = '&';
   else
      token[0] = '?';
   
   if (service_source_lang != TRANSLATION_LANG_DONT_CARE)
   {
      const char *lang_source 
            = ai_service_get_str((enum translation_lang)service_source_lang);

      if (!string_is_empty(lang_source))
      {
         strlcat(buffer, token, length);
         strlcat(buffer, "source_lang=", length);
         strlcat(buffer, lang_source, length);
         token[0] = '&';
      }
   }
   
   if (service_target_lang != TRANSLATION_LANG_DONT_CARE)
   {
      const char *lang_target 
            = ai_service_get_str((enum translation_lang)service_target_lang);

      if (!string_is_empty(lang_target))
      {
         strlcat(buffer, token, length);
         strlcat(buffer, "target_lang=", length);
         strlcat(buffer, lang_target, length);
         token[0] = '&';
      }
   }
   
   strlcat(buffer, token, length);
   strlcat(buffer, "output=", length);
   switch (ai_service_mode)
   {
      case 0: /* Image Mode */
         strlcat(buffer, "image,bmp", length);
#ifdef HAVE_RPNG
         strlcat(buffer, ",png", length);         
         if (poke_supported)
            strlcat(buffer, ",png-a", length);
#endif
         break;
         
      case 1: /* Speech Mode */
         strlcat(buffer, "sound,wav", length);
         break;
      
      case 2: /* Narrator Mode */
         strlcat(buffer, "text", length);
         break;
         
      case 3: /* Text Mode */
      case 4: /* Text + Narrator */
         strlcat(buffer, "text,subs", length);
         break;
         
      case 5: /* Image + Narrator */
         strlcat(buffer, "text,image,bmp", length);
#ifdef HAVE_RPNG
         strlcat(buffer, ",png", length);         
         if (poke_supported)
            strlcat(buffer, ",png-a", length);
#endif
         break;
   }
}

/**
 * Captures a frame from the currently running core and sends a request to the
 * translation server. Processing and encoding this data comes with a cost, so
 * it is offloaded to the task thread.
 */
static void translation_request_hndl(retro_task_t *task)
{
   access_request_t *request     = (access_request_t*)task->user_data;
   settings_t *settings          = config_get_ptr();
   access_state_t *access_st     = access_state_get_ptr();
   access_frame_t *frame         = NULL;
   access_base64_t *encode       = NULL;
   char *label                   = NULL;
   rjsonwriter_t *writer         = NULL;
   const char *json              = NULL;
   bool sent                     = false;
   char url[PATH_MAX_LENGTH];
   
   if (task_get_cancelled(task))
      goto finish;
   
   access_st->last_call = cpu_features_get_time_usec();
   
   frame = translation_grab_frame();
   if (task_get_cancelled(task) || !frame)
      goto finish;
   
   if (translation_dupe_fail(frame))
      goto finish;
      
   encode = translation_frame_encode(frame);
   if (task_get_cancelled(task) || !encode)
      goto finish;
   
   label  = translation_get_content_label();
   writer = build_request_json(encode, request, frame, label);
   if (task_get_cancelled(task) || !writer)
      goto finish;
   
   json = rjsonwriter_get_memory_buffer(writer, NULL);
   build_request_url(url, PATH_MAX_LENGTH, settings);
   if (task_get_cancelled(task) || !json)
      goto finish;
   
#ifdef DEBUG
   if (access_st->ai_service_auto == 0)
      RARCH_LOG("[Translate]: Sending request to: %s\n", url);
#endif
   sent = true;
   task_push_http_post_transfer(
         url, json, true, NULL, translation_response_cb, NULL);
   
finish:
   task_set_finished(task, true);

   if (frame && frame->data)
      free(frame->data);
   if (frame)
      free(frame);
   if (encode && encode->data)
      free(encode->data);
   if (encode)
      free(encode);
   if (label)
      free(label);
   if (writer)
      rjsonwriter_free(writer);
   if (request && request->inputs)
      free(request->inputs);
   if (request)
      free(request);
   
   /* Plan next auto-request if this one was skipped */
   if (!sent && access_st->ai_service_auto != 0)
      call_auto_translate_task(settings);
}

/**
 * Invokes the translation service. Captures a frame from the current content
 * core and sends it over HTTP to the translation server. Once the server 
 * responds, the translation data is displayed accordingly to the preferences
 * of the user. Returns true if the request could be built and sent.
 */
bool run_translation_service(settings_t *settings, bool paused)
{
   unsigned i;
   retro_task_t *task               = NULL;
   access_request_t *request        = NULL;
   access_state_t *access_st        = access_state_get_ptr();
#ifdef HAVE_ACCESSIBILITY
   input_driver_state_t *input_st   = input_state_get_ptr();
#endif

   if (!(request = (access_request_t*)malloc(sizeof(access_request_t))))
      goto failure;
   
#ifdef HAVE_THREADS
   if (!access_st->image_lock)
   {
      if (!(access_st->image_lock = slock_new()))
         goto failure;
   }
#endif
   
   task = task_init();
   if (!task)
      goto failure;

   /* Freeze frontend state while we're still running on the main thread */
   request->paused = paused;  
   request->inputs = (char*)malloc(
         sizeof(char) * ARRAY_SIZE(ACCESS_INPUT_LABELS));
         
#ifdef HAVE_ACCESSIBILITY   
   for (i = 0; i < ARRAY_SIZE(ACCESS_INPUT_LABELS); i++)
      request->inputs[i] = input_st->ai_gamepad_state[i] ? 1 : 0;
#endif
   
   task->handler           = translation_request_hndl;
   task->user_data         = request;
   task->mute              = true;
   access_st->request_task = task;
   task_queue_push(task);
   
   return true;
   
failure:
   if (request)
      free(request);

   return false;
}
