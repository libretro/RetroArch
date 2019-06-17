/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <compat/strl.h>
#include <string/stdstring.h>

#include "../configuration.h"
#include "../msg_hash.h"

#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

int menu_hash_get_help_eo_enum(enum msg_hash_enums msg, char *s, size_t len)
{
   switch (msg)
   {
      case MSG_UNKNOWN:
      default:
         if (string_is_empty(s))
            strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
         /* TODO/FIXME - should change this to -1 once we add more entries */
         break;
   }

   /* TODO/FIXME - should change this to 0 once we add more entries */
   return -1;
}

const char *msg_hash_to_str_eo(enum msg_hash_enums msg)
{
   switch (msg)
   {
      #include "msg_hash_eo.h"
      default:
         break;
   }

   return "null";
}
