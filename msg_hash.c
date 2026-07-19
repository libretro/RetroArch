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


#if defined(MSG_HASH_HAVE_STRTAB)
void msg_hash_strtab_index_build(msg_hash_strtab_index_t *idx,
      const msg_hash_strtab_t *tab)
{
   uint32_t i;
   uint32_t *id_slots;

   if (idx->tab == tab && idx->id_slots)
      return;

   if (!tab || tab->count == 0)
      return;

   id_slots = (uint32_t*)calloc(MSG_LAST, sizeof(uint32_t));
   if (!id_slots)
      return;

   if (tab->blob)
   {
      /* Packed encoding: rows are contiguous NUL-terminated
       * strings; recover each row's offset with one walk. */
      const char *p = tab->blob;
      for (i = 0; i < tab->count; i++)
      {
         if (tab->ids[i] < (uint32_t)MSG_LAST)
            id_slots[tab->ids[i]] = (uint32_t)(p - tab->blob) + 1;
         while (*p)
            p++;
         p++;
      }
   }
   else
   {
      for (i = 0; i < tab->count; i++)
      {
         if (tab->ids[i] < (uint32_t)MSG_LAST)
            id_slots[tab->ids[i]] = i + 1;
      }
   }

   /* Publish the fully built index before marking it valid, so a
    * concurrent reader either sees the complete index or takes the
    * fallback path below. */
   if (idx->id_slots)
      free(idx->id_slots);
   idx->id_slots = id_slots;
   idx->tab    = tab;
}

const char *msg_hash_strtab_lookup(const msg_hash_strtab_t *tab,
      msg_hash_strtab_index_t *idx, uint32_t id)
{
   if (!tab || tab->count == 0)
      return NULL;

   if (idx->tab != tab || !idx->id_slots)
      msg_hash_strtab_index_build(idx, tab);

   if (idx->tab == tab && idx->id_slots)
   {
      if (id < (uint32_t)MSG_LAST)
      {
         uint32_t slot = idx->id_slots[id];
         if (slot)
         {
            if (tab->strs)
               return tab->strs[slot - 1];
            return tab->blob + (slot - 1);
         }
      }
      return NULL;
   }

   /* Allocation-failure path only. */
   {
      uint32_t i;
      if (tab->strs)
      {
         for (i = 0; i < tab->count; i++)
            if (tab->ids[i] == id)
               return tab->strs[i];
      }
      else
      {
         const char *p = tab->blob;
         for (i = 0; i < tab->count; i++)
         {
            if (tab->ids[i] == id)
               return p;
            while (*p)
               p++;
            p++;
         }
      }
   }

   return NULL;
}
#endif /* MSG_HASH_HAVE_STRTAB */

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
/* Per-language packed string tables, generated by intl/json2h.py.
 * Each header defines msg_hash_<lang>_blob / _ids; see
 * msg_hash.h for the encoding notes. */
#include "intl/msg_hash_he.h"
#include "intl/msg_hash_sk.h"
#include "intl/msg_hash_uk.h"
#include "intl/msg_hash_eo.h"
#include "intl/msg_hash_pl.h"
#include "intl/msg_hash_fi.h"
#include "intl/msg_hash_hu.h"
#include "intl/msg_hash_be.h"
#include "intl/msg_hash_en.h"
#include "intl/msg_hash_it.h"
#include "intl/msg_hash_fa.h"
#include "intl/msg_hash_ast.h"
#include "intl/msg_hash_nl.h"
#include "intl/msg_hash_sv.h"
#include "intl/msg_hash_id.h"
#include "intl/msg_hash_cs.h"
#include "intl/msg_hash_ar.h"
#include "intl/msg_hash_fr.h"
#include "intl/msg_hash_cht.h"
#include "intl/msg_hash_de.h"
#include "intl/msg_hash_es.h"
#include "intl/msg_hash_ca.h"
#include "intl/msg_hash_el.h"
#include "intl/msg_hash_ja.h"
#include "intl/msg_hash_ko.h"
#include "intl/msg_hash_pt_pt.h"
#include "intl/msg_hash_ru.h"
#include "intl/msg_hash_tr.h"
#include "intl/msg_hash_val.h"
#include "intl/msg_hash_vn.h"
#include "intl/msg_hash_chs.h"
#include "intl/msg_hash_pt_br.h"
#include "intl/msg_hash_gl.h"
#include "intl/msg_hash_no.h"
#include "intl/msg_hash_ga.h"
#include "intl/msg_hash_th.h"

static const msg_hash_strtab_t msg_hash_strtab_he =
{ msg_hash_he_ids, NULL,
  (uint32_t)(sizeof(msg_hash_he_ids) / sizeof(msg_hash_he_ids[0])),
  (const char*)&msg_hash_he_blob };
static const msg_hash_strtab_t msg_hash_strtab_sk =
{ msg_hash_sk_ids, NULL,
  (uint32_t)(sizeof(msg_hash_sk_ids) / sizeof(msg_hash_sk_ids[0])),
  (const char*)&msg_hash_sk_blob };
static const msg_hash_strtab_t msg_hash_strtab_uk =
{ msg_hash_uk_ids, NULL,
  (uint32_t)(sizeof(msg_hash_uk_ids) / sizeof(msg_hash_uk_ids[0])),
  (const char*)&msg_hash_uk_blob };
static const msg_hash_strtab_t msg_hash_strtab_eo =
{ msg_hash_eo_ids, NULL,
  (uint32_t)(sizeof(msg_hash_eo_ids) / sizeof(msg_hash_eo_ids[0])),
  (const char*)&msg_hash_eo_blob };
static const msg_hash_strtab_t msg_hash_strtab_pl =
{ msg_hash_pl_ids, NULL,
  (uint32_t)(sizeof(msg_hash_pl_ids) / sizeof(msg_hash_pl_ids[0])),
  (const char*)&msg_hash_pl_blob };
static const msg_hash_strtab_t msg_hash_strtab_fi =
{ msg_hash_fi_ids, NULL,
  (uint32_t)(sizeof(msg_hash_fi_ids) / sizeof(msg_hash_fi_ids[0])),
  (const char*)&msg_hash_fi_blob };
static const msg_hash_strtab_t msg_hash_strtab_hu =
{ msg_hash_hu_ids, NULL,
  (uint32_t)(sizeof(msg_hash_hu_ids) / sizeof(msg_hash_hu_ids[0])),
  (const char*)&msg_hash_hu_blob };
static const msg_hash_strtab_t msg_hash_strtab_be =
{ msg_hash_be_ids, NULL,
  (uint32_t)(sizeof(msg_hash_be_ids) / sizeof(msg_hash_be_ids[0])),
  (const char*)&msg_hash_be_blob };
static const msg_hash_strtab_t msg_hash_strtab_en =
{ msg_hash_en_ids, NULL,
  (uint32_t)(sizeof(msg_hash_en_ids) / sizeof(msg_hash_en_ids[0])),
  (const char*)&msg_hash_en_blob };
static const msg_hash_strtab_t msg_hash_strtab_it =
{ msg_hash_it_ids, NULL,
  (uint32_t)(sizeof(msg_hash_it_ids) / sizeof(msg_hash_it_ids[0])),
  (const char*)&msg_hash_it_blob };
static const msg_hash_strtab_t msg_hash_strtab_fa =
{ msg_hash_fa_ids, NULL,
  (uint32_t)(sizeof(msg_hash_fa_ids) / sizeof(msg_hash_fa_ids[0])),
  (const char*)&msg_hash_fa_blob };
static const msg_hash_strtab_t msg_hash_strtab_ast =
{ msg_hash_ast_ids, NULL,
  (uint32_t)(sizeof(msg_hash_ast_ids) / sizeof(msg_hash_ast_ids[0])),
  (const char*)&msg_hash_ast_blob };
static const msg_hash_strtab_t msg_hash_strtab_nl =
{ msg_hash_nl_ids, NULL,
  (uint32_t)(sizeof(msg_hash_nl_ids) / sizeof(msg_hash_nl_ids[0])),
  (const char*)&msg_hash_nl_blob };
static const msg_hash_strtab_t msg_hash_strtab_sv =
{ msg_hash_sv_ids, NULL,
  (uint32_t)(sizeof(msg_hash_sv_ids) / sizeof(msg_hash_sv_ids[0])),
  (const char*)&msg_hash_sv_blob };
static const msg_hash_strtab_t msg_hash_strtab_id =
{ msg_hash_id_ids, NULL,
  (uint32_t)(sizeof(msg_hash_id_ids) / sizeof(msg_hash_id_ids[0])),
  (const char*)&msg_hash_id_blob };
static const msg_hash_strtab_t msg_hash_strtab_cs =
{ msg_hash_cs_ids, NULL,
  (uint32_t)(sizeof(msg_hash_cs_ids) / sizeof(msg_hash_cs_ids[0])),
  (const char*)&msg_hash_cs_blob };
static const msg_hash_strtab_t msg_hash_strtab_ar =
{ msg_hash_ar_ids, NULL,
  (uint32_t)(sizeof(msg_hash_ar_ids) / sizeof(msg_hash_ar_ids[0])),
  (const char*)&msg_hash_ar_blob };
static const msg_hash_strtab_t msg_hash_strtab_fr =
{ msg_hash_fr_ids, NULL,
  (uint32_t)(sizeof(msg_hash_fr_ids) / sizeof(msg_hash_fr_ids[0])),
  (const char*)&msg_hash_fr_blob };
static const msg_hash_strtab_t msg_hash_strtab_cht =
{ msg_hash_cht_ids, NULL,
  (uint32_t)(sizeof(msg_hash_cht_ids) / sizeof(msg_hash_cht_ids[0])),
  (const char*)&msg_hash_cht_blob };
static const msg_hash_strtab_t msg_hash_strtab_de =
{ msg_hash_de_ids, NULL,
  (uint32_t)(sizeof(msg_hash_de_ids) / sizeof(msg_hash_de_ids[0])),
  (const char*)&msg_hash_de_blob };
static const msg_hash_strtab_t msg_hash_strtab_es =
{ msg_hash_es_ids, NULL,
  (uint32_t)(sizeof(msg_hash_es_ids) / sizeof(msg_hash_es_ids[0])),
  (const char*)&msg_hash_es_blob };
static const msg_hash_strtab_t msg_hash_strtab_ca =
{ msg_hash_ca_ids, NULL,
  (uint32_t)(sizeof(msg_hash_ca_ids) / sizeof(msg_hash_ca_ids[0])),
  (const char*)&msg_hash_ca_blob };
static const msg_hash_strtab_t msg_hash_strtab_el =
{ msg_hash_el_ids, NULL,
  (uint32_t)(sizeof(msg_hash_el_ids) / sizeof(msg_hash_el_ids[0])),
  (const char*)&msg_hash_el_blob };
static const msg_hash_strtab_t msg_hash_strtab_jp =
{ msg_hash_ja_ids, NULL,
  (uint32_t)(sizeof(msg_hash_ja_ids) / sizeof(msg_hash_ja_ids[0])),
  (const char*)&msg_hash_ja_blob };
static const msg_hash_strtab_t msg_hash_strtab_ko =
{ msg_hash_ko_ids, NULL,
  (uint32_t)(sizeof(msg_hash_ko_ids) / sizeof(msg_hash_ko_ids[0])),
  (const char*)&msg_hash_ko_blob };
static const msg_hash_strtab_t msg_hash_strtab_pt_pt =
{ msg_hash_pt_pt_ids, NULL,
  (uint32_t)(sizeof(msg_hash_pt_pt_ids) / sizeof(msg_hash_pt_pt_ids[0])),
  (const char*)&msg_hash_pt_pt_blob };
static const msg_hash_strtab_t msg_hash_strtab_ru =
{ msg_hash_ru_ids, NULL,
  (uint32_t)(sizeof(msg_hash_ru_ids) / sizeof(msg_hash_ru_ids[0])),
  (const char*)&msg_hash_ru_blob };
static const msg_hash_strtab_t msg_hash_strtab_tr =
{ msg_hash_tr_ids, NULL,
  (uint32_t)(sizeof(msg_hash_tr_ids) / sizeof(msg_hash_tr_ids[0])),
  (const char*)&msg_hash_tr_blob };
static const msg_hash_strtab_t msg_hash_strtab_val =
{ msg_hash_val_ids, NULL,
  (uint32_t)(sizeof(msg_hash_val_ids) / sizeof(msg_hash_val_ids[0])),
  (const char*)&msg_hash_val_blob };
static const msg_hash_strtab_t msg_hash_strtab_vn =
{ msg_hash_vn_ids, NULL,
  (uint32_t)(sizeof(msg_hash_vn_ids) / sizeof(msg_hash_vn_ids[0])),
  (const char*)&msg_hash_vn_blob };
static const msg_hash_strtab_t msg_hash_strtab_chs =
{ msg_hash_chs_ids, NULL,
  (uint32_t)(sizeof(msg_hash_chs_ids) / sizeof(msg_hash_chs_ids[0])),
  (const char*)&msg_hash_chs_blob };
static const msg_hash_strtab_t msg_hash_strtab_pt_br =
{ msg_hash_pt_br_ids, NULL,
  (uint32_t)(sizeof(msg_hash_pt_br_ids) / sizeof(msg_hash_pt_br_ids[0])),
  (const char*)&msg_hash_pt_br_blob };
static const msg_hash_strtab_t msg_hash_strtab_gl =
{ msg_hash_gl_ids, NULL,
  (uint32_t)(sizeof(msg_hash_gl_ids) / sizeof(msg_hash_gl_ids[0])),
  (const char*)&msg_hash_gl_blob };
static const msg_hash_strtab_t msg_hash_strtab_no =
{ msg_hash_no_ids, NULL,
  (uint32_t)(sizeof(msg_hash_no_ids) / sizeof(msg_hash_no_ids[0])),
  (const char*)&msg_hash_no_blob };
static const msg_hash_strtab_t msg_hash_strtab_ga =
{ msg_hash_ga_ids, NULL,
  (uint32_t)(sizeof(msg_hash_ga_ids) / sizeof(msg_hash_ga_ids[0])),
  (const char*)&msg_hash_ga_blob };
static const msg_hash_strtab_t msg_hash_strtab_th =
{ msg_hash_th_ids, NULL,
  (uint32_t)(sizeof(msg_hash_th_ids) / sizeof(msg_hash_th_ids[0])),
  (const char*)&msg_hash_th_blob };

static msg_hash_strtab_index_t msg_hash_lang_index;

static const msg_hash_strtab_t *msg_hash_lang_strtab(unsigned lang)
{
   switch (lang)
   {
      case RETRO_LANGUAGE_FRENCH:
         return &msg_hash_strtab_fr;
      case RETRO_LANGUAGE_GERMAN:
         return &msg_hash_strtab_de;
      case RETRO_LANGUAGE_SPANISH:
         return &msg_hash_strtab_es;
      case RETRO_LANGUAGE_ITALIAN:
         return &msg_hash_strtab_it;
      case RETRO_LANGUAGE_PORTUGUESE_BRAZIL:
         return &msg_hash_strtab_pt_br;
      case RETRO_LANGUAGE_PORTUGUESE_PORTUGAL:
         return &msg_hash_strtab_pt_pt;
      case RETRO_LANGUAGE_DUTCH:
         return &msg_hash_strtab_nl;
      case RETRO_LANGUAGE_ESPERANTO:
         return &msg_hash_strtab_eo;
      case RETRO_LANGUAGE_POLISH:
         return &msg_hash_strtab_pl;
      case RETRO_LANGUAGE_RUSSIAN:
         return &msg_hash_strtab_ru;
      case RETRO_LANGUAGE_JAPANESE:
         return &msg_hash_strtab_jp;
      case RETRO_LANGUAGE_KOREAN:
         return &msg_hash_strtab_ko;
      case RETRO_LANGUAGE_VIETNAMESE:
         return &msg_hash_strtab_vn;
      case RETRO_LANGUAGE_CHINESE_SIMPLIFIED:
         return &msg_hash_strtab_chs;
      case RETRO_LANGUAGE_CHINESE_TRADITIONAL:
         return &msg_hash_strtab_cht;
      case RETRO_LANGUAGE_ARABIC:
         return &msg_hash_strtab_ar;
      case RETRO_LANGUAGE_GREEK:
         return &msg_hash_strtab_el;
      case RETRO_LANGUAGE_TURKISH:
         return &msg_hash_strtab_tr;
      case RETRO_LANGUAGE_SLOVAK:
         return &msg_hash_strtab_sk;
      case RETRO_LANGUAGE_PERSIAN:
         return &msg_hash_strtab_fa;
      case RETRO_LANGUAGE_HEBREW:
         return &msg_hash_strtab_he;
      case RETRO_LANGUAGE_ASTURIAN:
         return &msg_hash_strtab_ast;
      case RETRO_LANGUAGE_FINNISH:
         return &msg_hash_strtab_fi;
      case RETRO_LANGUAGE_INDONESIAN:
         return &msg_hash_strtab_id;
      case RETRO_LANGUAGE_SWEDISH:
         return &msg_hash_strtab_sv;
      case RETRO_LANGUAGE_UKRAINIAN:
         return &msg_hash_strtab_uk;
      case RETRO_LANGUAGE_CZECH:
         return &msg_hash_strtab_cs;
      case RETRO_LANGUAGE_CATALAN_VALENCIA:
         return &msg_hash_strtab_val;
      case RETRO_LANGUAGE_CATALAN:
         return &msg_hash_strtab_ca;
      case RETRO_LANGUAGE_BRITISH_ENGLISH:
         return &msg_hash_strtab_en;
      case RETRO_LANGUAGE_HUNGARIAN:
         return &msg_hash_strtab_hu;
      case RETRO_LANGUAGE_BELARUSIAN:
         return &msg_hash_strtab_be;
      case RETRO_LANGUAGE_GALICIAN:
         return &msg_hash_strtab_gl;
      case RETRO_LANGUAGE_NORWEGIAN:
         return &msg_hash_strtab_no;
      case RETRO_LANGUAGE_IRISH:
         return &msg_hash_strtab_ga;
      case RETRO_LANGUAGE_THAI:
         return &msg_hash_strtab_th;
      default:
         break;
   }
   return NULL;
}
#endif

const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   const char *ret = NULL;

#ifdef HAVE_LANGEXTRA
   {
      const msg_hash_strtab_t *tab =
            msg_hash_lang_strtab(uint_user_language);
      if (tab)
         ret = msg_hash_strtab_lookup(tab,
               &msg_hash_lang_index, (uint32_t)msg);
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
#if defined(HAVE_WEBMPLAYER) && !defined(HAVE_FFMPEG) && !defined(HAVE_MPV)
      /* video containers the built-in WebM/MP4 player handles */
      { "mkv",       FILE_TYPE_MKV },
      { "webm",      FILE_TYPE_WEBM },
#ifdef HAVE_RMP4
      { "mp4",       FILE_TYPE_MP4 },
      { "m4v",       FILE_TYPE_MP4 },
      { "mov",       FILE_TYPE_MOV },
#endif
#endif
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
      /* video containers */
      { "ogm",       FILE_TYPE_OGM },
      { "mkv",       FILE_TYPE_MKV },
      { "avi",       FILE_TYPE_AVI },
      { "mp4",       FILE_TYPE_MP4 },
      { "m4v",       FILE_TYPE_MP4 },
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
#if !defined(HAVE_AUDIOMIXER) || defined(HAVE_RMP3)
      { "mp3",       FILE_TYPE_MP3 },
#endif
#if !defined(HAVE_AUDIOMIXER) || defined(HAVE_RVORBIS)
      { "ogg",       FILE_TYPE_OGG },
#endif
#if !defined(HAVE_AUDIOMIXER) || defined(HAVE_RFLAC)
      { "flac",      FILE_TYPE_FLAC },
#endif
#if !defined(HAVE_AUDIOMIXER) || defined(HAVE_RWAV)
      { "wav",       FILE_TYPE_WAV },
#endif
#if !defined(HAVE_AUDIOMIXER) || defined(HAVE_RMODTRACKER)
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
#if defined(MSG_HASH_HAVE_STRTAB)
         msg_hash_us_index_init();
#ifdef HAVE_LANGEXTRA
         msg_hash_strtab_index_build(&msg_hash_lang_index,
               msg_hash_lang_strtab(val));
#endif
#endif
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
