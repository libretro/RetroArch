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

#include <lrc_hash.h>
#include <string/stdstring.h>
#include <libretro.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "msg_hash.h"

#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

/* TODO/FIXME - static public global variable */
static unsigned uint_user_language;

int msg_hash_get_help_enum(enum msg_hash_enums msg, char *s, size_t len)
{
   int ret = msg_hash_get_help_us_enum(msg, s, len);
   /* Replace line-breaks with "empty line-breaks" for readability */
   const char *temp = string_replace_substring(s,
         "\n",    STRLEN_CONST("\n"),
         "\n \n", STRLEN_CONST("\n \n"));

   strlcpy(s, temp, len);
   return ret;
}

const char *get_user_language_iso639_1(bool limit)
{
   switch (uint_user_language)
   {
      case RETRO_LANGUAGE_FRENCH:
         return "fr";
      case RETRO_LANGUAGE_GERMAN:
         return "de";
      case RETRO_LANGUAGE_SPANISH:
         return "es";
      case RETRO_LANGUAGE_ITALIAN:
         return "it";
      case RETRO_LANGUAGE_PORTUGUESE_BRAZIL:
         if (limit)
            return "pt";
         return "pt_br";
      case RETRO_LANGUAGE_PORTUGUESE_PORTUGAL:
         if (limit)
            return "pt";
         return "pt_pt";
      case RETRO_LANGUAGE_DUTCH:
         return "nl";
      case RETRO_LANGUAGE_ESPERANTO:
         return "eo";
      case RETRO_LANGUAGE_POLISH:
         return "pl";
      case RETRO_LANGUAGE_JAPANESE:
         return "ja";
      case RETRO_LANGUAGE_KOREAN:
         return "ko";
      case RETRO_LANGUAGE_VIETNAMESE:
         return "vi";
      case RETRO_LANGUAGE_CHINESE_SIMPLIFIED:
         if (limit)
            return "zh";
         return "zh_cn";
      case RETRO_LANGUAGE_CHINESE_TRADITIONAL:
         if (limit)
            return "zh";
         return "zh_tw";
      case RETRO_LANGUAGE_ARABIC:
         return "ar";
      case RETRO_LANGUAGE_GREEK:
         return "el";
      case RETRO_LANGUAGE_TURKISH:
         return "tr";
      case RETRO_LANGUAGE_SLOVAK:
         return "sk";
      case RETRO_LANGUAGE_RUSSIAN:
         return "ru";
      case RETRO_LANGUAGE_PERSIAN:
         return "fa";
      case RETRO_LANGUAGE_HEBREW:
         return "he";
      case RETRO_LANGUAGE_ASTURIAN:
         return "ast";
      case RETRO_LANGUAGE_FINNISH:
         return "fi";
      case RETRO_LANGUAGE_INDONESIAN:
         return "id";
      case RETRO_LANGUAGE_SWEDISH:
         return "sv";
      case RETRO_LANGUAGE_UKRAINIAN:
         return "uk";
      case RETRO_LANGUAGE_CZECH:
         return "cs";
      case RETRO_LANGUAGE_CATALAN_VALENCIA:
         if (limit)
            return "ca";
         return "ca_ES@valencia";
      case RETRO_LANGUAGE_CATALAN:
         return "ca";
      case RETRO_LANGUAGE_BRITISH_ENGLISH:
         if (limit)
            return "en";
         return "en_gb";
      case RETRO_LANGUAGE_HUNGARIAN:
         return "hu";
      case RETRO_LANGUAGE_BELARUSIAN:
         return "be";
   }
   return "en";
}

#ifdef HAVE_LANGEXTRA
static const char *msg_hash_to_str_he(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_he.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_sk(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_sk.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_uk(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_uk.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_eo(enum msg_hash_enums msg)
{
   switch (msg)
   {
      #include "intl/msg_hash_eo.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_pl(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_pl.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_fi(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_fi.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_hu(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_hu.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_be(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_be.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_en(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_en.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_it(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_it.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_fa(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_fa.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_ast(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_ast.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_nl(enum msg_hash_enums msg)
{
   switch (msg)
   {
      #include "intl/msg_hash_nl.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_sv(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_sv.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_id(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_id.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_cs(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_cs.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_ar(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_ar.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_fr(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_fr.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_cht(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_cht.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_de(enum msg_hash_enums msg)
{
   switch (msg)
   {
      #include "intl/msg_hash_de.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_es(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_es.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_ca(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_ca.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_el(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_el.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_jp(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_ja.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_ko(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_ko.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_pt_pt(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_pt_pt.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_ru(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_ru.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_tr(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_tr.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_val(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_val.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_vn(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_vn.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_chs(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_chs.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_pt_br(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_pt_br.h"
      default:
         break;
   }

   return "null";
}

#endif

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
      case RETRO_LANGUAGE_SLOVAK:
         ret = msg_hash_to_str_sk(msg);
         break;
      case RETRO_LANGUAGE_PERSIAN:
         ret = msg_hash_to_str_fa(msg);
         break;
      case RETRO_LANGUAGE_HEBREW:
         ret = msg_hash_to_str_he(msg);
         break;
      case RETRO_LANGUAGE_ASTURIAN:
         ret = msg_hash_to_str_ast(msg);
         break;
      case RETRO_LANGUAGE_FINNISH:
         ret = msg_hash_to_str_fi(msg);
         break;
      case RETRO_LANGUAGE_INDONESIAN:
         ret = msg_hash_to_str_id(msg);
         break;
      case RETRO_LANGUAGE_SWEDISH:
         ret = msg_hash_to_str_sv(msg);
         break;
      case RETRO_LANGUAGE_UKRAINIAN:
         ret = msg_hash_to_str_uk(msg);
         break;
      case RETRO_LANGUAGE_CZECH:
         ret = msg_hash_to_str_cs(msg);
         break;
      case RETRO_LANGUAGE_CATALAN_VALENCIA:
         ret = msg_hash_to_str_val(msg);
         break;
      case RETRO_LANGUAGE_CATALAN:
         ret = msg_hash_to_str_ca(msg);
         break;
      case RETRO_LANGUAGE_BRITISH_ENGLISH:
         ret = msg_hash_to_str_en(msg);
         break;
      case RETRO_LANGUAGE_HUNGARIAN:
         ret = msg_hash_to_str_hu(msg);
         break;
      case RETRO_LANGUAGE_BELARUSIAN:
         ret = msg_hash_to_str_be(msg);
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
      case MENU_VALUE_FILE_M4A:
         return FILE_TYPE_M4A;
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
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV) || defined(HAVE_AUDIOMIXER)
#if !defined(HAVE_AUDIOMIXER) || defined(HAVE_DR_MP3)
      case MENU_VALUE_FILE_MP3:
         return FILE_TYPE_MP3;
#endif
#if !defined(HAVE_AUDIOMIXER) || defined(HAVE_STB_VORBIS)
      case MENU_VALUE_FILE_OGG:
         return FILE_TYPE_OGG;
#endif
#if !defined(HAVE_AUDIOMIXER) || defined(HAVE_DR_FLAC)
      case MENU_VALUE_FILE_FLAC:
         return FILE_TYPE_FLAC;
#endif
#if !defined(HAVE_AUDIOMIXER) || defined(HAVE_RWAV)
      case MENU_VALUE_FILE_WAV:
         return FILE_TYPE_WAV;
#endif
#if !defined(HAVE_AUDIOMIXER) || defined(HAVE_IBXM)
       case MENU_VALUE_FILE_MOD:
           return FILE_TYPE_MOD;
       case MENU_VALUE_FILE_S3M:
           return FILE_TYPE_S3M;
       case MENU_VALUE_FILE_XM:
           return FILE_TYPE_XM;
#endif
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

const char *msg_hash_get_wideglyph_str(void)
{
#ifdef HAVE_LANGEXTRA
   switch (uint_user_language)
   {
      case RETRO_LANGUAGE_CHINESE_SIMPLIFIED:
         return "菜";
      case RETRO_LANGUAGE_CHINESE_TRADITIONAL:
         return "主";
      case RETRO_LANGUAGE_JAPANESE:
         return "漢";
      case RETRO_LANGUAGE_KOREAN:
         return "메";
      default:
         break;
   }
#endif
   
   return NULL;
}
