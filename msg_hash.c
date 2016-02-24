/*  RetroArch - A frontend for libretro.
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

#include <string.h>

#include <rhash.h>
#include <string/stdstring.h>

#include "msg_hash.h"

#include "configuration.h"

const char *msg_hash_to_str(uint32_t hash)
{
   const char *ret = NULL;
   settings_t *settings = config_get_ptr();

   if (!settings)
      goto end;

   switch (settings->user_language)
   {
      case RETRO_LANGUAGE_FRENCH:
         ret = msg_hash_to_str_fr(hash);
         break;
      case RETRO_LANGUAGE_GERMAN:
         ret = msg_hash_to_str_de(hash);
         break;
      case RETRO_LANGUAGE_SPANISH:
         ret = msg_hash_to_str_es(hash);
         break;
      case RETRO_LANGUAGE_ITALIAN:
         ret = msg_hash_to_str_it(hash);
         break;
      case RETRO_LANGUAGE_PORTUGUESE:
         ret = msg_hash_to_str_pt(hash);
         break;
      case RETRO_LANGUAGE_DUTCH:
         ret = msg_hash_to_str_nl(hash);
         break;
      case RETRO_LANGUAGE_ESPERANTO:
         ret = msg_hash_to_str_eo(hash);
         break;
      case RETRO_LANGUAGE_POLISH:
         ret = msg_hash_to_str_pl(hash);
         break;
      case RETRO_LANGUAGE_RUSSIAN:
#ifdef HAVE_UTF8
         ret = msg_hash_to_str_ru(hash);
#endif
         break;
      default:
         break;
   }

   if (ret && !string_is_equal(ret, "null"))
      return ret;

end:
   return msg_hash_to_str_us(hash);
}

uint32_t msg_hash_calculate(const char *s)
{
   return djb2_calculate(s);
}
