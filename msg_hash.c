/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <stdio.h>
#include <string.h>

#include <rhash.h>
#include <string/stdstring.h>
#include <libretro.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "msg_hash.h"

static unsigned uint_user_language;

int menu_hash_get_help_enum(enum msg_hash_enums msg, char *s, size_t len)
{
#ifdef HAVE_MENU
   int ret = -1;

#ifdef HAVE_LANGEXTRA
   switch (uint_user_language)
   {
      case RETRO_LANGUAGE_FRENCH:
         ret = menu_hash_get_help_fr_enum(msg, s, len);
         break;
      case RETRO_LANGUAGE_GERMAN:
         ret = menu_hash_get_help_de_enum(msg, s, len);
         break;
      case RETRO_LANGUAGE_SPANISH:
         ret = menu_hash_get_help_es_enum(msg, s, len);
         break;
      case RETRO_LANGUAGE_ITALIAN:
         ret = menu_hash_get_help_it_enum(msg, s, len);
         break;
      case RETRO_LANGUAGE_PORTUGUESE_BRAZIL:
         ret = menu_hash_get_help_pt_br_enum(msg, s, len);
         break;
      case RETRO_LANGUAGE_PORTUGUESE_PORTUGAL:
         ret = menu_hash_get_help_pt_pt_enum(msg, s, len);
         break;
      case RETRO_LANGUAGE_DUTCH:
         ret = menu_hash_get_help_nl_enum(msg, s, len);
         break;
      case RETRO_LANGUAGE_ESPERANTO:
         ret = menu_hash_get_help_eo_enum(msg, s, len);
         break;
      case RETRO_LANGUAGE_POLISH:
         ret = menu_hash_get_help_pl_enum(msg, s, len);
         break;
      case RETRO_LANGUAGE_JAPANESE:
         ret = menu_hash_get_help_jp_enum(msg, s, len);
         break;
      case RETRO_LANGUAGE_KOREAN:
         ret = menu_hash_get_help_ko_enum(msg, s, len);
         break;
      case RETRO_LANGUAGE_VIETNAMESE:
         ret = menu_hash_get_help_vn_enum(msg, s, len);
         break;
      case RETRO_LANGUAGE_CHINESE_SIMPLIFIED:
         ret = menu_hash_get_help_chs_enum(msg, s, len);
         break;
      case RETRO_LANGUAGE_CHINESE_TRADITIONAL:
         ret = menu_hash_get_help_cht_enum(msg, s, len);
         break;
      case RETRO_LANGUAGE_ARABIC:
         ret = menu_hash_get_help_ar_enum(msg, s, len);
         break;
      case RETRO_LANGUAGE_GREEK:
         ret = menu_hash_get_help_el_enum(msg, s, len);
         break;
      case RETRO_LANGUAGE_TURKISH:
         ret = menu_hash_get_help_tr_enum(msg, s, len);
         break;
      default:
         break;
   }
#endif

   if (ret == 0)
      return ret;

   return menu_hash_get_help_us_enum(msg, s, len);
#else
   return 0;
#endif
}

const char *get_user_language_iso639_1(bool limit)
{
   const char *voice;
   voice = "en";
   switch (uint_user_language)
   {
      case RETRO_LANGUAGE_FRENCH:
         voice = "fr";
         break;
      case RETRO_LANGUAGE_GERMAN:
         voice = "de";
         break;
      case RETRO_LANGUAGE_SPANISH:
         voice = "es";
         break;
      case RETRO_LANGUAGE_ITALIAN:
         voice = "it";
         break;
      case RETRO_LANGUAGE_PORTUGUESE_BRAZIL:
         if (limit)
            voice = "pt";
         else
            voice = "pt_br";
         break;
      case RETRO_LANGUAGE_PORTUGUESE_PORTUGAL:
         if (limit)
            voice = "pt";
         else
            voice = "pt_pt";
         break;
      case RETRO_LANGUAGE_DUTCH:
         voice = "nl";
         break;
      case RETRO_LANGUAGE_ESPERANTO:
         voice = "eo";
         break;
      case RETRO_LANGUAGE_POLISH:
         voice = "pl";
         break;
      case RETRO_LANGUAGE_JAPANESE:
         voice = "ja";
         break;
      case RETRO_LANGUAGE_KOREAN:
         voice = "ko";
         break;
      case RETRO_LANGUAGE_VIETNAMESE:
         voice = "vi";
         break;
      case RETRO_LANGUAGE_CHINESE_SIMPLIFIED:
         voice = "zh";
         break;
      case RETRO_LANGUAGE_CHINESE_TRADITIONAL:
         voice = "zh";
         break;
      case RETRO_LANGUAGE_ARABIC:
         voice = "ar";
         break;
      case RETRO_LANGUAGE_GREEK:
         voice = "el";
         break;
      case RETRO_LANGUAGE_TURKISH:
         voice = "tr";
         break;
      case RETRO_LANGUAGE_RUSSIAN:
         voice = "ru";
         break;

   }
   return voice;
}

const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   const char *ret = NULL;

#ifdef HAVE_LANGEXTRA
   switch (uint_user_language)
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
      case RETRO_LANGUAGE_PORTUGUESE_BRAZIL:
         ret = msg_hash_to_str_pt_br(msg);
         break;
      case RETRO_LANGUAGE_PORTUGUESE_PORTUGAL:
         ret = msg_hash_to_str_pt_pt(msg);
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
         ret = msg_hash_to_str_ru(msg);
         break;
      case RETRO_LANGUAGE_JAPANESE:
         ret = msg_hash_to_str_jp(msg);
         break;
      case RETRO_LANGUAGE_KOREAN:
         ret = msg_hash_to_str_ko(msg);
         break;
      case RETRO_LANGUAGE_VIETNAMESE:
         ret = msg_hash_to_str_vn(msg);
         break;
      case RETRO_LANGUAGE_CHINESE_SIMPLIFIED:
         ret = msg_hash_to_str_chs(msg);
         break;
      case RETRO_LANGUAGE_CHINESE_TRADITIONAL:
         ret = msg_hash_to_str_cht(msg);
         break;
      case RETRO_LANGUAGE_ARABIC:
         ret = msg_hash_to_str_ar(msg);
         break;
      case RETRO_LANGUAGE_GREEK:
         ret = msg_hash_to_str_el(msg);
         break;
      case RETRO_LANGUAGE_TURKISH:
         ret = msg_hash_to_str_tr(msg);
         break;
      default:
         break;
   }
#endif

   if (ret && !string_is_equal(ret, "null"))
      return ret;

   return msg_hash_to_str_us(msg);
}

uint32_t msg_hash_calculate(const char *s)
{
   return djb2_calculate(s);
}

#define MENU_VALUE_FILE_WEBM                                                   0x7ca00b50U
#define MENU_VALUE_FILE_F4F                                                    0x0b886be5U
#define MENU_VALUE_FILE_F4V                                                    0x0b886bf5U
#define MENU_VALUE_FILE_OGM                                                    0x0b8898c8U
#define MENU_VALUE_FILE_MKV                                                    0x0b8890d3U
#define MENU_VALUE_FILE_AVI                                                    0x0b885f25U
#define MENU_VALUE_FILE_M4A                                                    0x0b8889a7U
#define MENU_VALUE_FILE_3GP                                                    0x0b87998fU
#define MENU_VALUE_FILE_MP4                                                    0x0b889136U
#define MENU_VALUE_FILE_MP3                                                    0x0b889135U
#define MENU_VALUE_FILE_FLAC                                                   0x7c96d67bU
#define MENU_VALUE_FILE_OGG                                                    0x0b8898c2U
#define MENU_VALUE_FILE_MOD                                                    0x0b889145U
#define MENU_VALUE_FILE_S3M                                                    0x0b88a318U
#define MENU_VALUE_FILE_XM                                                     0x00597a2aU
#define MENU_VALUE_FILE_FLV                                                    0x0b88732dU
#define MENU_VALUE_FILE_WAV                                                    0x0b88ba13U
#define MENU_VALUE_FILE_MOV                                                    0x0b889157U
#define MENU_VALUE_FILE_WMV                                                    0x0b88bb9fU
#define MENU_VALUE_FILE_3G2                                                    0x0b879951U
#define MENU_VALUE_FILE_MPG                                                    0x0b889169U
#define MENU_VALUE_FILE_MPEG                                                   0x7c9abeaeU
#define MENU_VALUE_FILE_VOB                                                    0x0b88b78cU
#define MENU_VALUE_FILE_ASF                                                    0x0b885ebfU
#define MENU_VALUE_FILE_DIVX                                                   0x7c95b3c0U
#define MENU_VALUE_FILE_M2P                                                    0x0b888974U
#define MENU_VALUE_FILE_M2TS                                                   0x7c99b8ebU
#define MENU_VALUE_FILE_PS                                                     0x00597928U
#define MENU_VALUE_FILE_TS                                                     0x005979acU
#define MENU_VALUE_FILE_MXF                                                    0x0b889270U
#define MENU_VALUE_FILE_WMA                                                    0x0b88bb8aU

#define MENU_VALUE_FILE_JPG                                                    0x0b8884a6U
#define MENU_VALUE_FILE_JPEG                                                   0x7c99198bU
#define MENU_VALUE_FILE_JPG_CAPS                                               0x0b87f846U
#define MENU_VALUE_FILE_JPEG_CAPS                                              0x7c87010bU
#define MENU_VALUE_FILE_PNG                                                    0x0b889deaU
#define MENU_VALUE_FILE_PNG_CAPS                                               0x0b88118aU
#define MENU_VALUE_FILE_GONG                                                   0x7c977150U
#define MENU_VALUE_FILE_GONG_CAPS                                              0x7c8558d0U
#define MENU_VALUE_FILE_TGA                                                    0x0b88ae01U
#define MENU_VALUE_FILE_BMP                                                    0x0b886244U

#define MENU_VALUE_MD5                                                         0x0b888fabU
#define MENU_VALUE_SHA1                                                        0x7c9de632U
#define MENU_VALUE_CRC                                                         0x0b88671dU
#define MENU_VALUE_MORE                                                        0x0b877cafU
#define MENU_VALUE_CFILE                                                       0xac3ec4f9U
#define MENU_VALUE_ON                                                          0x005974c2U
#define MENU_VALUE_OFF                                                         0x0b880c40U
#define MENU_VALUE_COMP                                                        0x6a166ba5U
#define MENU_VALUE_MUSIC                                                       0xc4a73997U
#define MENU_VALUE_IMAGE                                                       0xbab7ebf9U
#define MENU_VALUE_MOVIE                                                       0xc43c4bf6U
#define MENU_VALUE_CORE                                                        0x6a167f7fU
#define MENU_VALUE_CURSOR                                                      0x57bba8b4U
#define MENU_VALUE_FILE                                                        0x6a496536U
#define MENU_VALUE_RDB                                                         0x0b00f54eU
#define MENU_VALUE_DIR                                                         0x0af95f55U
#define MENU_VALUE_GLSLP                                                       0x0f840c87U
#define MENU_VALUE_CGP                                                         0x0b8865bfU
#define MENU_VALUE_GLSL                                                        0x7c976537U
#define MENU_VALUE_HLSL                                                        0x7c97f198U
#define MENU_VALUE_HLSLP                                                       0x0f962508U
#define MENU_VALUE_CG                                                          0x0059776fU
#define MENU_VALUE_SLANG                                                       0x105ce63aU
#define MENU_VALUE_SLANGP                                                      0x1bf9adeaU

#define FILE_HASH_APK                                                          0x0b885e61U

#define HASH_EXTENSION_7Z                                                      0x005971d6U
#define HASH_EXTENSION_7Z_UPP                                                  0x005971b6U
#define HASH_EXTENSION_ZIP                                                     0x0b88c7d8U
#define HASH_EXTENSION_ZIP_UPP                                                 0x0b883b78U
#define HASH_EXTENSION_CUE                                                     0x0b886782U
#define HASH_EXTENSION_CUE_UPPERCASE                                           0x0b87db22U
#define HASH_EXTENSION_GDI                                                     0x00b887659
#define HASH_EXTENSION_GDI_UPPERCASE                                           0x00b87e9f9
#define HASH_EXTENSION_ISO                                                     0x0b8880d0U
#define HASH_EXTENSION_ISO_UPPERCASE                                           0x0b87f470U
#define HASH_EXTENSION_LUTRO                                                   0x0fe37b7bU
#define HASH_EXTENSION_CHD                                                     0x0b8865d4U

enum msg_file_type msg_hash_to_file_type(uint32_t hash)
{
   switch (hash)
   {
      case MENU_VALUE_COMP:
      case HASH_EXTENSION_7Z:
      case HASH_EXTENSION_7Z_UPP:
      case HASH_EXTENSION_ZIP:
      case HASH_EXTENSION_ZIP_UPP:
      case FILE_HASH_APK:
         return FILE_TYPE_COMPRESSED;
      case MENU_VALUE_CFILE:
         return FILE_TYPE_IN_CARCHIVE;
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
         return FILE_TYPE_SHADER_CG;
      case MENU_VALUE_SLANG:
         return FILE_TYPE_SHADER_SLANG;
      case MENU_VALUE_GLSLP:
         return FILE_TYPE_SHADER_PRESET_GLSLP;
      case MENU_VALUE_HLSLP:
         return FILE_TYPE_SHADER_PRESET_HLSLP;
      case MENU_VALUE_CGP:
         return FILE_TYPE_SHADER_PRESET_CGP;
      case MENU_VALUE_SLANGP:
         return FILE_TYPE_SHADER_PRESET_SLANGP;
      case MENU_VALUE_CRC:
         return FILE_TYPE_CRC;
      case MENU_VALUE_SHA1:
         return FILE_TYPE_SHA1;
      case MENU_VALUE_MD5:
         return FILE_TYPE_MD5;
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
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
      case MENU_VALUE_FILE_3G2:
         return FILE_TYPE_3G2;
      case MENU_VALUE_FILE_MPG:
         return FILE_TYPE_MPG;
      case MENU_VALUE_FILE_MPEG:
         return FILE_TYPE_MPEG;
      case MENU_VALUE_FILE_VOB:
         return FILE_TYPE_VOB;
      case MENU_VALUE_FILE_ASF:
         return FILE_TYPE_ASF;
      case MENU_VALUE_FILE_DIVX:
         return FILE_TYPE_DIVX;
      case MENU_VALUE_FILE_M2P:
         return FILE_TYPE_M2P;
      case MENU_VALUE_FILE_M2TS:
         return FILE_TYPE_M2TS;
      case MENU_VALUE_FILE_PS:
         return FILE_TYPE_PS;
      case MENU_VALUE_FILE_TS:
         return FILE_TYPE_TS;
      case MENU_VALUE_FILE_MXF:
         return FILE_TYPE_MXF;
      case MENU_VALUE_FILE_WMA:
         return FILE_TYPE_WMA;
#endif
#ifdef HAVE_IBXM
       case MENU_VALUE_FILE_MOD:
           return FILE_TYPE_MOD;
       case MENU_VALUE_FILE_S3M:
           return FILE_TYPE_S3M;
       case MENU_VALUE_FILE_XM:
           return FILE_TYPE_XM;
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
#ifdef HAVE_EASTEREGG
      case MENU_VALUE_FILE_GONG:
      case MENU_VALUE_FILE_GONG_CAPS:
         return FILE_TYPE_GONG;
#endif
      case HASH_EXTENSION_CUE:
      case HASH_EXTENSION_CUE_UPPERCASE:
         return FILE_TYPE_CUE;
      case HASH_EXTENSION_GDI:
      case HASH_EXTENSION_GDI_UPPERCASE:
         return FILE_TYPE_GDI;
      case HASH_EXTENSION_ISO:
      case HASH_EXTENSION_ISO_UPPERCASE:
         return FILE_TYPE_ISO;
      case HASH_EXTENSION_LUTRO:
         return FILE_TYPE_LUTRO;
      case HASH_EXTENSION_CHD:
         return FILE_TYPE_CHD;
      default:
         break;
   }

   return FILE_TYPE_NONE;
}

unsigned *msg_hash_get_uint(enum msg_hash_action type)
{
   switch (type)
   {
      case MSG_HASH_USER_LANGUAGE:
         return &uint_user_language;
      case MSG_HASH_NONE:
         break;
   }

   return NULL;
}

void msg_hash_set_uint(enum msg_hash_action type, unsigned val)
{
   switch (type)
   {
      case MSG_HASH_USER_LANGUAGE:
         uint_user_language = val;
         break;
      case MSG_HASH_NONE:
         break;
   }
}
