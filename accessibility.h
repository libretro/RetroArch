/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#ifndef __RETROARCH_ACCESSIBILITY_H
#define __RETROARCH_ACCESSIBILITY_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdlib.h>

#include <boolean.h>
#include <retro_inline.h>
#include <retro_common_api.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "configuration.h"
#include "tasks/tasks_internal.h"

#ifdef HAVE_THREADS
#include "rthreads/rthreads.h"
#endif

typedef struct
{
   /* 1 if the automatic mode has been enabled, 0 otherwise */
   int ai_service_auto;
   
   /* Text-to-speech narrator override flag */
   bool enabled;
   
   /* The last request task, used to prepare and send the translation */
   retro_task_t *request_task;
   
   /* The last response task, used to parse costly translation data */
   retro_task_t *response_task;
   
   /* Timestamp of the last translation request */
   retro_time_t last_call;
   
   /* Frame captured during the last call to the translation service */
   int last_image_size;
   uint8_t *last_image;
   
#ifdef HAVE_THREADS
   /* Necessary because last_image is manipulated by task handlers */
   slock_t *image_lock;
#endif
   
} access_state_t;

bool is_accessibility_enabled(bool accessibility_enable, bool accessibility_enabled);

#ifdef HAVE_TRANSLATE
bool is_narrator_running(bool accessibility_enable);
#endif

/*
   Invoke this method to send a request to the AI service. 
   It makes the following POST request using URL params:
      – source_lang (optional): language code of the content currently running.
      – target_lang (optional): language of the content to return.
      – output: comma-separated list of formats that must be provided by the
         service. Also lists supported sub-formats.
         
   The currently supported formats are:
      – sound: raw audio to playback. (wav)
      – text: text to be read through internal text-to-speech capabilities.
         'subs' can be specified on top of that to explain that we are looking
         for short text response in the manner of subtitles.
      – image: image to display on top of the video feed. Widgets will be used
         first if possible, otherwise we'll try to draw it directly on the 
         video buffer. (bmp, png, png-a) [All in 24-bits BGR formats]
         
   In addition, the request contains a JSON payload, formatted as such:
      – image: captured frame from the currently running content (in base64).
      – format: format of the captured frame ("png", or "bmp").
      – coords: array describing the coordinates of the image within the 
         viewport space (x, y, width, height).
      – viewport: array describing the size of the viewport (width, height).
      – label: a text string describing the content (<system id>__<content id>).
      – state: a JSON object describing the state of the frontend, containing:
         – paused: 1 if the content has been paused, 0 otherwise.
         – <key>: the name of a retropad input, valued 1 if pressed.
            (a, b, x, y, l, r, l2, r2, l3, r3)
            (up, down, left, right, start, select)
            
   The translation component then expects a response from the AI service in the
   form of a JSON payload, formatted as such:
      – image: base64 representation of an image in a supported format.
      – sound: base64 representation of a sound byte in a supported format.
      – text: results from the service as a string.
      – text_position: hint for the position of the text when the service is
         running in text mode (ie subtitles). Position is a number,
         1 for Bottom or 2 for Top (defaults to bottom).
      – press: a list of retropad input to forcibly press. On top of the 
         expected keys (cf. 'state' above) values 'pause' and 'unpause' can be
         specified to control the flow of the content.
      – error: any error encountered with the request.
      – auto: either 'auto' or 'continue' to control automatic requests.
      
   All fields are optional, but at least one of them must be present.
   If 'error' is set, the error is shown to the user and everything else is
   ignored, even 'auto' settings.
   
   With 'auto' on 'auto', RetroArch will automatically send a new request
   (with a minimum delay enforced by uints.ai_service_poll_delay), with a value
   of 'continue', RetroArch will ignore the returned content and skip to the 
   next automatic request. This allows the service to specify that the returned
   content is the same as the one previously sent, so RetroArch does not need to
   update its display unless necessary. With 'continue' the service *must* 
   still send the content, as we may need to display it if the user paused the 
   AI service for instance.

   {paused} boolean is passed in to indicate if the current call was made 
   during a paused frame. Due to how the menu widgets work, if the AI service 
   is called in 'auto' mode, then this call will be made while the menu widgets 
   unpause the core for a frame to update the on-screen widgets. To tell the AI
   service what the pause mode is honestly, we store the runloop_paused 
   variable from before the service wipes the widgets, and pass that in here.
*/
bool run_translation_service(settings_t *settings, bool paused);

void translation_release(bool inform);

bool accessibility_speak_priority(
      bool accessibility_enable,
      unsigned accessibility_narrator_speech_speed,
      const char* speak_text, int priority);

access_state_t *access_state_get_ptr(void);

#endif
