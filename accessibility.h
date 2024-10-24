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

typedef struct
{
   int ai_service_auto;
   /* Is text-to-speech accessibility turned on? */
   bool enabled;
} access_state_t;

bool is_accessibility_enabled(bool accessibility_enable, bool accessibility_enabled);

#ifdef HAVE_TRANSLATE
bool is_narrator_running(bool accessibility_enable);
#endif

/*
   This function does all the stuff needed to translate the game screen,
   using the URL given in the settings.  Once the image from the frame
   buffer is sent to the server, the callback will write the translated
   image to the screen.

   Supported client/services (thus far)
   -VGTranslate client ( www.gitlab.com/spherebeaker/vg_translate )
   -Ztranslate client/service ( www.ztranslate.net/docs/service )

   To use a client, download the relevant code/release, configure
   them, and run them on your local machine, or network.  Set the
   retroarch configuration to point to your local client (usually
   listening on localhost:4404 ) and enable translation service.

   If you don't want to run a client, you can also use a service,
   which is basically like someone running a client for you.  The
   downside here is that your retroarch device will have to have
   an internet connection, and you may have to sign up for it.

   To make your own server, it must listen for a POST request, which
   will consist of a JSON body, with the "image" field as a base64
   encoded string of a 24bit-BMP/PNG that the will be translated.
   The server must output the translated image in the form of a
   JSON body, with the "image" field also as a base64 encoded
   24bit-BMP, or as an alpha channel png.

  "paused" boolean is passed in to indicate if the current call
   was made during a paused frame.  Due to how the menu widgets work,
   if the ai service is called in "auto" mode, then this call will
   be made while the menu widgets unpause the core for a frame to update
   the on-screen widgets.  To tell the ai service what the pause
   mode is honestly, we store the runloop_paused variable from before
   the handle_translation_cb wipes the widgets, and pass that in here.
*/
bool run_translation_service(settings_t *settings, bool paused);

bool accessibility_speak_priority(
      bool accessibility_enable,
      unsigned accessibility_narrator_speech_speed,
      const char* speak_text, int priority);

access_state_t *access_state_get_ptr(void);

#endif
