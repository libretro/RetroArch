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

#define FILE_HASH_ZIP         0x0b88c7d8U
#define FILE_HASH_ZIP_UPP     0x0b883b78U
#define FILE_HASH_APK         0x0b885e61U

enum menu_file_type msg_hash_to_file_type(uint32_t hash)
{
   switch (hash)
   {
      case MENU_VALUE_COMP:
      case FILE_HASH_ZIP:
      case FILE_HASH_ZIP_UPP:
      case FILE_HASH_APK:
         return FILE_TYPE_COMPRESSED;
      case MENU_VALUE_MORE:
         return FILE_TYPE_MORE;
      case MENU_VALUE_CORE:
         return FILE_TYPE_CORE;
      case MENU_VALUE_RDB:
         return FILE_TYPE_RDB;
      case MENU_VALUE_CURSOR:
         return FILE_TYPE_CURSOR;
      case MENU_VALUE_FILE:
         return FILE_TYPE_PLAIN;
      case MENU_VALUE_DIR:
         return FILE_TYPE_DIRECTORY;
      case MENU_VALUE_MUSIC:
         return FILE_TYPE_MUSIC;
      case MENU_VALUE_IMAGE:
         return FILE_TYPE_IMAGE;
      case MENU_VALUE_MOVIE:
         return FILE_TYPE_MOVIE;
      case MENU_VALUE_ON:
         return FILE_TYPE_BOOL_ON;
      case MENU_VALUE_OFF:
         return FILE_TYPE_BOOL_OFF;
      case MENU_VALUE_GLSL:
         return FILE_TYPE_SHADER_GLSL;
      case MENU_VALUE_HLSL:
         return FILE_TYPE_SHADER_HLSL;
      case MENU_VALUE_CG:
         return FILE_TYPE_SHADER_HLSL;
      case MENU_VALUE_SLANG:
         return FILE_TYPE_SHADER_SLANG;
      case MENU_VALUE_GLSLP:
         return FILE_TYPE_SHADER_PRESET_GLSLP;
      case MENU_VALUE_HLSLP:
         return FILE_TYPE_SHADER_PRESET_HLSLP;
      case MENU_VALUE_CGP:
         return FILE_TYPE_SHADER_PRESET_HLSLP;
      case MENU_VALUE_SLANGP:
         return FILE_TYPE_SHADER_PRESET_SLANGP;
      case MENU_VALUE_CRC:
         return FILE_TYPE_CRC;
      case MENU_VALUE_SHA1:
         return FILE_TYPE_SHA1;
      case MENU_VALUE_MD5:
         return FILE_TYPE_MD5;
#ifdef HAVE_FFMPEG
      case MENU_VALUE_FILE_OGM:
         return FILE_TYPE_OGM;
      case MENU_VALUE_FILE_MKV:
         return FILE_TYPE_MKV;
      case MENU_VALUE_FILE_AVI:
         return FILE_TYPE_AVI;
      case MENU_VALUE_FILE_MP4:
         return FILE_TYPE_MP4;
      case MENU_VALUE_FILE_FLV:
         return FILE_TYPE_FLV;
      case MENU_VALUE_FILE_WEBM:
         return FILE_TYPE_WEBM;
      case MENU_VALUE_FILE_3GP:
         return FILE_TYPE_3GP;
      case MENU_VALUE_FILE_F4F:
         return FILE_TYPE_F4F;
      case MENU_VALUE_FILE_F4V:
         return FILE_TYPE_F4V;
      case MENU_VALUE_FILE_MOV:
         return FILE_TYPE_MOV;
      case MENU_VALUE_FILE_WMV:
         return FILE_TYPE_WMV;
      case MENU_VALUE_FILE_MP3:
         return FILE_TYPE_MP3;
      case MENU_VALUE_FILE_M4A:
         return FILE_TYPE_M4A;
      case MENU_VALUE_FILE_OGG:
         return FILE_TYPE_OGG;
      case MENU_VALUE_FILE_FLAC:
         return FILE_TYPE_FLAC;
      case MENU_VALUE_FILE_WAV:
         return FILE_TYPE_WAV;
#endif
#ifdef HAVE_IMAGEVIEWER
      case MENU_VALUE_FILE_JPG:
      case MENU_VALUE_FILE_JPG_CAPS:
      case MENU_VALUE_FILE_JPEG:
      case MENU_VALUE_FILE_JPEG_CAPS:
         return FILE_TYPE_JPEG;
      case MENU_VALUE_FILE_PNG:
      case MENU_VALUE_FILE_PNG_CAPS:
         return FILE_TYPE_PNG;
      case MENU_VALUE_FILE_TGA:
         return FILE_TYPE_TGA;
      case MENU_VALUE_FILE_BMP:
         return FILE_TYPE_BMP;
#endif
      default:
         break;
   }

   return FILE_TYPE_NONE;
}
