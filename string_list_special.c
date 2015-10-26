/*  RetroArch - A frontend for libretro.
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

#include "string_list_special.h"

#ifdef HAVE_MENU
#include "menu/menu_driver.h"
#endif

#ifdef HAVE_CAMERA
#include "camera/camera_driver.h"
#endif

#ifdef HAVE_LOCATION
#include "location/location_driver.h"
#endif

#include "gfx/video_driver.h"
#include "input/input_driver.h"
#include "input/input_joypad_driver.h"
#include "audio/audio_driver.h"
#include "record/record_driver.h"

const char *string_list_special_new(enum string_list_type type)
{
   union string_list_elem_attr attr;
   unsigned i;
   char         *options = NULL;
   int               len = 0;
   struct string_list *s = string_list_new();

   attr.i = 0;

   if (!s)
      return NULL;

   switch (type)
   {
      case STRING_LIST_MENU_DRIVERS:
#ifdef HAVE_MENU
         for (i = 0; menu_driver_find_handle(i); i++)
         {
            const char *opt  = menu_driver_find_ident(i);
            len             += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
#endif
      case STRING_LIST_CAMERA_DRIVERS:
#ifdef HAVE_CAMERA
         for (i = 0; camera_driver_find_handle(i); i++)
         {
            const char *opt  = camera_driver_find_ident(i);
            len             += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
#endif
      case STRING_LIST_LOCATION_DRIVERS:
#ifdef HAVE_LOCATION
         for (i = 0; location_driver_find_handle(i); i++)
         {
            const char *opt  = location_driver_find_ident(i);
            options_len     += strlen(opt) + 1;
            string_list_append(options_l, opt, attr);
         }
         break;
#endif
      case STRING_LIST_AUDIO_DRIVERS:
         for (i = 0; audio_driver_find_handle(i); i++)
         {
            const char *opt  = audio_driver_find_ident(i);
            len             += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_VIDEO_DRIVERS:
         for (i = 0; video_driver_find_handle(i); i++)
         {
            const char *opt  = video_driver_find_ident(i);
            len             += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_INPUT_DRIVERS:
         for (i = 0; input_driver_find_handle(i); i++)
         {
            const char *opt  = input_driver_find_ident(i);
            len             += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_INPUT_JOYPAD_DRIVERS:
         for (i = 0; joypad_driver_find_handle(i); i++)
         {
            const char *opt  = joypad_driver_find_ident(i);
            len             += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_RECORD_DRIVERS:
         for (i = 0; record_driver_find_handle(i); i++)
         {
            const char *opt  = record_driver_find_ident(i);
            len             += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_NONE:
      default:
         goto end;
   }

   options = (char*)calloc(len, sizeof(char));

   if (options)
      string_list_join_concat(options, len, s, "|");

end:
   string_list_free(s);
   s = NULL;

   return options;
}
