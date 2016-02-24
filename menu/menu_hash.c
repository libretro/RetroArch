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

#include <stdint.h>
#include <string.h>

#include <rhash.h>
#include <string/stdstring.h>

#include "menu_hash.h"

#include "../configuration.h"

const char *menu_hash_to_str(uint32_t hash)
{
   const char *ret = NULL;
   settings_t *settings = config_get_ptr();

   if (!settings)
      return "null";

   switch (settings->user_language)
   {
      case RETRO_LANGUAGE_FRENCH:
         ret = menu_hash_to_str_fr(hash);
         break;
      case RETRO_LANGUAGE_GERMAN:
         ret = menu_hash_to_str_de(hash);
         break;
      case RETRO_LANGUAGE_SPANISH:
         ret = menu_hash_to_str_es(hash);
         break;
      case RETRO_LANGUAGE_ITALIAN:
         ret = menu_hash_to_str_it(hash);
         break;
      case RETRO_LANGUAGE_PORTUGUESE:
         ret = menu_hash_to_str_pt(hash);
         break;
      case RETRO_LANGUAGE_DUTCH:
         ret = menu_hash_to_str_nl(hash);
         break;
      case RETRO_LANGUAGE_ESPERANTO:
         ret = menu_hash_to_str_eo(hash);
         break;
      case RETRO_LANGUAGE_POLISH:
         ret = menu_hash_to_str_pl(hash);
         break;
      case RETRO_LANGUAGE_RUSSIAN:
#ifdef HAVE_UTF8
         ret = menu_hash_to_str_ru(hash);
#endif
         break;
      default:
         break;
   }

   if (ret && !string_is_equal(ret, "null"))
      return ret;

   return menu_hash_to_str_us(hash);
}

int menu_hash_get_help(uint32_t hash, char *s, size_t len)
{
   int ret = -1;
   settings_t *settings = config_get_ptr();

   if (!settings)
      return -1;

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
      case RETRO_LANGUAGE_RUSSIAN:
#ifdef HAVE_UTF8
         ret = menu_hash_get_help_ru(hash, s, len);
#endif
         break;
      default:
         break;
   }

   if (ret == 0)
      return ret;

   return menu_hash_get_help_us(hash, s, len);
}

uint32_t menu_hash_calculate(const char *s)
{
   return djb2_calculate(s);
}
