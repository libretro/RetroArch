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

int menu_hash_get_help(uint32_t hash, char *s, size_t len)
{
   int ret = -1;
   settings_t *settings = config_get_ptr();

   if (!settings)
      goto end;

#ifdef HAVE_LANGEXTRA
   switch (settings->user_language)
   {
      case RETRO_LANGUAGE_FRENCH:
         ret = menu_hash_get_help_fr(hash, s, len);
         break;
      case RETRO_LANGUAGE_GERMAN:
         ret = menu_hash_get_help_de(hash, s, len);
         break;
      case RETRO_LANGUAGE_SPANISH:
         ret = menu_hash_get_help_es(hash, s, len);
         break;
      case RETRO_LANGUAGE_ITALIAN:
         ret = menu_hash_get_help_it(hash, s, len);
         break;
      case RETRO_LANGUAGE_PORTUGUESE:
         ret = menu_hash_get_help_pt(hash, s, len);
         break;
      case RETRO_LANGUAGE_DUTCH:
         ret = menu_hash_get_help_nl(hash, s, len);
         break;
      case RETRO_LANGUAGE_ESPERANTO:
         ret = menu_hash_get_help_eo(hash, s, len);
         break;
      case RETRO_LANGUAGE_POLISH:
         ret = menu_hash_get_help_pl(hash, s, len);
         break;
      default:
         break;
   }
#endif

   if (ret == 0)
      return ret;

end:
   return menu_hash_get_help_us(hash, s, len);
}

const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   const char *ret = NULL;
   settings_t *settings = config_get_ptr();

   if (!settings)
      goto end;

#ifdef HAVE_LANGEXTRA
   switch (settings->user_language)
   {
      case RETRO_LANGUAGE_FRENCH:
         ret = msg_hash_to_str_fr(msg);
         break;
      case RETRO_LANGUAGE_GERMAN:
         ret = msg_hash_to_str_de(msg);
         break;
      case RETRO_LANGUAGE_SPANISH:
         ret = msg_hash_to_str_es(msg);
         break;
      case RETRO_LANGUAGE_ITALIAN:
         ret = msg_hash_to_str_it(msg);
         break;
      case RETRO_LANGUAGE_PORTUGUESE:
         ret = msg_hash_to_str_pt(msg);
         break;
      case RETRO_LANGUAGE_DUTCH:
         ret = msg_hash_to_str_nl(msg);
         break;
      case RETRO_LANGUAGE_ESPERANTO:
         ret = msg_hash_to_str_eo(msg);
         break;
      case RETRO_LANGUAGE_POLISH:
         ret = msg_hash_to_str_pl(msg);
         break;
      case RETRO_LANGUAGE_RUSSIAN:
#ifdef HAVE_UTF8
         ret = msg_hash_to_str_ru(msg);
#endif
         break;
      default:
         break;
   }
#endif

   if (ret && !string_is_equal(ret, "null"))
      return ret;

end:
   return msg_hash_to_str_us(msg);
}

uint32_t msg_hash_calculate(const char *s)
{
   return djb2_calculate(s);
}
