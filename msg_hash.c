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

#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <string/stdstring.h> /* for string_replace_substring */
#include <libretro.h>
#include <retro_miscellaneous.h> /* ARRAY_SIZE */

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
   const char *temp = string_replace_substring(s, strlen(s),
         "\n",    (sizeof("\n")-1),
         "\n \n", (sizeof("\n \n")-1));

   if (temp)
   {
      strlcpy(s, temp, len);
      free((void*)temp);
   }
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
      case RETRO_LANGUAGE_GALICIAN:
          return "gl";
      case RETRO_LANGUAGE_NORWEGIAN:
          return "no";
      case RETRO_LANGUAGE_IRISH:
          return "ga";
      case RETRO_LANGUAGE_THAI:
          return "th";
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

static const char *msg_hash_to_str_gl(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_gl.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_no(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_no.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_ga(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_ga.h"
      default:
         break;
   }

   return "null";
}

static const char *msg_hash_to_str_th(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "intl/msg_hash_th.h"
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
      case RETRO_LANGUAGE_GALICIAN:
         ret = msg_hash_to_str_gl(msg);
         break;
      case RETRO_LANGUAGE_NORWEGIAN:
         ret = msg_hash_to_str_no(msg);
         break;
      case RETRO_LANGUAGE_IRISH:
         ret = msg_hash_to_str_ga(msg);
         break;
      case RETRO_LANGUAGE_THAI:
         ret = msg_hash_to_str_th(msg);
         break;
      default:
         break;
   }
#endif
   if (ret && strcmp(ret, "null") != 0)
      return ret;
   return msg_hash_to_str_us(msg);
}

/* String-table lookup for menu entry values and file extensions.
 *
 * Historically this dispatched through DJB2 hashes of the value
 * string against ~80 precomputed magic hex constants — readable
 * only via reverse-engineering, prone to silent collisions if a
 * new extension hashed to the same value as an existing one, and
 * carried 79 TOC entries on PPC64 (which contributed to the PS3
 * griffin build crossing the 64K TOC limit).
 *
 * The hash dispatch saved ~2 us/frame in menu-mode file browsing
 * vs a linear strcmp scan — not enough to justify the obscurity.
 *
 * Callers passing case-insensitively must lowercase their input
 * before calling this; only the lowercase entries below are
 * checked for those cases (jpg, png, etc. include uppercase
 * variants because the menu callers may receive either). */
enum msg_file_type msg_hash_to_file_type(const char *value)
{
   size_t i;
   struct ft_entry {
      const char *value;
      enum msg_file_type type;
   };
   /* Order roughly by expected hit frequency to minimize average
    * compare count for the common cases (game ROMs hitting CHD,
    * ISO, CUE, ZIP first; image viewers hitting PNG/JPG). */
   static const struct ft_entry table[] = {
      /* disc / archive containers — top of file-browser traffic */
      { "chd",       FILE_TYPE_CHD },
      { "iso",       FILE_TYPE_ISO },
      { "ISO",       FILE_TYPE_ISO },
      { "cue",       FILE_TYPE_CUE },
      { "CUE",       FILE_TYPE_CUE },
      { "gdi",       FILE_TYPE_GDI },
      { "GDI",       FILE_TYPE_GDI },
      { "zip",       FILE_TYPE_COMPRESSED },
      { "ZIP",       FILE_TYPE_COMPRESSED },
      { "7z",        FILE_TYPE_COMPRESSED },
      { "7Z",        FILE_TYPE_COMPRESSED },
      { "apk",       FILE_TYPE_COMPRESSED },
      { "(COMP)",    FILE_TYPE_COMPRESSED },
      { "lutro",     FILE_TYPE_LUTRO },
      /* menu placeholder values from displaylist enumeration */
      { "(CFILE)",   FILE_TYPE_IN_CARCHIVE },
      { "...",       FILE_TYPE_MORE },
      { "(CORE)",    FILE_TYPE_CORE },
      { "(RDB)",     FILE_TYPE_RDB },
      { "(FILE)",    FILE_TYPE_PLAIN },
      { "(DIR)",     FILE_TYPE_DIRECTORY },
      { "(MUSIC)",   FILE_TYPE_MUSIC },
      { "(IMAGE)",   FILE_TYPE_IMAGE },
      { "(MOVIE)",   FILE_TYPE_MOVIE },
      { "ON",        FILE_TYPE_BOOL_ON },
      { "OFF",       FILE_TYPE_BOOL_OFF },
      /* shader source extensions */
      { "glsl",      FILE_TYPE_SHADER_GLSL },
      { "hlsl",      FILE_TYPE_SHADER_HLSL },
      { "cg",        FILE_TYPE_SHADER_CG },
      { "slang",     FILE_TYPE_SHADER_SLANG },
      { "glslp",     FILE_TYPE_SHADER_PRESET_GLSLP },
      { "hlslp",     FILE_TYPE_SHADER_PRESET_HLSLP },
      { "cgp",       FILE_TYPE_SHADER_PRESET_CGP },
      { "slangp",    FILE_TYPE_SHADER_PRESET_SLANGP },
      /* checksum / hash labels */
      { "crc",       FILE_TYPE_CRC },
      { "sha1",      FILE_TYPE_SHA1 },
      { "md5",       FILE_TYPE_MD5 },
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
      /* video containers */
      { "ogm",       FILE_TYPE_OGM },
      { "mkv",       FILE_TYPE_MKV },
      { "avi",       FILE_TYPE_AVI },
      { "mp4",       FILE_TYPE_MP4 },
      { "flv",       FILE_TYPE_FLV },
      { "webm",      FILE_TYPE_WEBM },
      { "3gp",       FILE_TYPE_3GP },
      { "f4f",       FILE_TYPE_F4F },
      { "f4v",       FILE_TYPE_F4V },
      { "mov",       FILE_TYPE_MOV },
      { "wmv",       FILE_TYPE_WMV },
      { "m4a",       FILE_TYPE_M4A },
      { "3g2",       FILE_TYPE_3G2 },
      { "mpg",       FILE_TYPE_MPG },
      { "mpeg",      FILE_TYPE_MPEG },
      { "vob",       FILE_TYPE_VOB },
      { "asf",       FILE_TYPE_ASF },
      { "divx",      FILE_TYPE_DIVX },
      { "m2p",       FILE_TYPE_M2P },
      { "m2ts",      FILE_TYPE_M2TS },
      { "ps",        FILE_TYPE_PS },
      { "ts",        FILE_TYPE_TS },
      { "mxf",       FILE_TYPE_MXF },
      { "wma",       FILE_TYPE_WMA },
#endif
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV) || defined(HAVE_AUDIOMIXER)
#if !defined(HAVE_AUDIOMIXER) || defined(HAVE_DR_MP3)
      { "mp3",       FILE_TYPE_MP3 },
#endif
#if !defined(HAVE_AUDIOMIXER) || defined(HAVE_STB_VORBIS)
      { "ogg",       FILE_TYPE_OGG },
#endif
#if !defined(HAVE_AUDIOMIXER) || defined(HAVE_DR_FLAC)
      { "flac",      FILE_TYPE_FLAC },
#endif
#if !defined(HAVE_AUDIOMIXER) || defined(HAVE_RWAV)
      { "wav",       FILE_TYPE_WAV },
#endif
#if !defined(HAVE_AUDIOMIXER) || defined(HAVE_IBXM)
      { "mod",       FILE_TYPE_MOD },
      { "s3m",       FILE_TYPE_S3M },
      { "xm",        FILE_TYPE_XM },
#endif
#endif
#ifdef HAVE_IMAGEVIEWER
      { "jpg",       FILE_TYPE_JPEG },
      { "JPG",       FILE_TYPE_JPEG },
      { "jpeg",      FILE_TYPE_JPEG },
      { "JPEG",      FILE_TYPE_JPEG },
      { "png",       FILE_TYPE_PNG },
      { "PNG",       FILE_TYPE_PNG },
      { "tga",       FILE_TYPE_TGA },
      { "bmp",       FILE_TYPE_BMP },
      { "webp",      FILE_TYPE_WEBP },
      { "WEBP",      FILE_TYPE_WEBP },
#endif
#ifdef HAVE_EASTEREGG
      { "gong",      FILE_TYPE_GONG },
      { "GONG",      FILE_TYPE_GONG },
#endif
   };

   if (!value || !*value)
      return FILE_TYPE_NONE;

   for (i = 0; i < ARRAY_SIZE(table); i++)
   {
      /* Inline short string compare: most entries are 2-6 bytes so
       * the strcmp/string_is_equal overhead would dominate.  Bail
       * on first mismatch. */
      const char *a = value;
      const char *b = table[i].value;
      while (*a && *a == *b) { a++; b++; }
      if (!*a && !*b)
         return table[i].type;
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
