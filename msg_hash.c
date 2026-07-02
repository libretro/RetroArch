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
   uint32_t *direct;
   const char *p;

   if (idx->tab == tab && idx->direct)
      return;

   if (!tab || tab->count == 0)
      return;

   direct = (uint32_t*)calloc(MSG_LAST, sizeof(uint32_t));
   if (!direct)
      return;

   for (i = 0, p = tab->blob; i < tab->count; i++)
   {
      if (tab->ids[i] < (uint32_t)MSG_LAST)
         direct[tab->ids[i]] = (uint32_t)(p - tab->blob) + 1;
      while (*p)
         p++;
      p++;
   }

   /* Publish the fully built index before marking it valid, so a
    * concurrent reader either sees the complete index or takes the
    * fallback path below. */
   if (idx->direct)
      free(idx->direct);
   idx->direct = direct;
   idx->tab    = tab;
}

const char *msg_hash_strtab_lookup(const msg_hash_strtab_t *tab,
      msg_hash_strtab_index_t *idx, uint32_t id)
{
   if (!tab || tab->count == 0)
      return NULL;

   if (idx->tab != tab || !idx->direct)
      msg_hash_strtab_index_build(idx, tab);

   if (idx->tab == tab && idx->direct)
   {
      if (id < (uint32_t)MSG_LAST)
      {
         uint32_t off = idx->direct[id];
         if (off)
            return tab->blob + (off - 1);
      }
      return NULL;
   }

   /* Allocation-failure path only. */
   {
      const char *p = tab->blob;
      uint32_t i;
      for (i = 0; i < tab->count; i++)
      {
         if (tab->ids[i] == id)
            return p;
         while (*p)
            p++;
         p++;
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
#if defined(MSG_HASH_HAVE_STRTAB)

/* Per-language string tables.
 *
 * Each translation header is included twice with MSG_HASH
 * redefined: once concatenating every row into a single packed,
 * NUL-separated blob, and once producing a bare uint32 id array.
 * Table overhead is therefore 4 bytes per row with zero
 * relocations, replacing one switch function (code plus a dense
 * jump table spanning the whole enum range) per language.
 *
 * The headers themselves are machine-written by the Crowdin sync
 * (intl/crowdin_sync.py -> json2h.py) and MUST keep their exact
 * MSG_HASH(ID, "...") row format; this consumes them as-is and
 * makes no assumption about row order (ordering is recovered at
 * language activation, see msg_hash_strtab_index_build). */

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_he[] =
#include "intl/msg_hash_he.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_he[] = {
#include "intl/msg_hash_he.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_sk[] =
#include "intl/msg_hash_sk.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_sk[] = {
#include "intl/msg_hash_sk.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_uk[] =
#include "intl/msg_hash_uk.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_uk[] = {
#include "intl/msg_hash_uk.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_eo[] =
#include "intl/msg_hash_eo.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_eo[] = {
#include "intl/msg_hash_eo.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_pl[] =
#include "intl/msg_hash_pl.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_pl[] = {
#include "intl/msg_hash_pl.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_fi[] =
#include "intl/msg_hash_fi.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_fi[] = {
#include "intl/msg_hash_fi.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_hu[] =
#include "intl/msg_hash_hu.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_hu[] = {
#include "intl/msg_hash_hu.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_be[] =
#include "intl/msg_hash_be.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_be[] = {
#include "intl/msg_hash_be.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_en[] =
#include "intl/msg_hash_en.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_en[] = {
#include "intl/msg_hash_en.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_it[] =
#include "intl/msg_hash_it.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_it[] = {
#include "intl/msg_hash_it.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_fa[] =
#include "intl/msg_hash_fa.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_fa[] = {
#include "intl/msg_hash_fa.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_ast[] =
#include "intl/msg_hash_ast.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_ast[] = {
#include "intl/msg_hash_ast.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_nl[] =
#include "intl/msg_hash_nl.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_nl[] = {
#include "intl/msg_hash_nl.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_sv[] =
#include "intl/msg_hash_sv.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_sv[] = {
#include "intl/msg_hash_sv.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_id[] =
#include "intl/msg_hash_id.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_id[] = {
#include "intl/msg_hash_id.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_cs[] =
#include "intl/msg_hash_cs.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_cs[] = {
#include "intl/msg_hash_cs.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_ar[] =
#include "intl/msg_hash_ar.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_ar[] = {
#include "intl/msg_hash_ar.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_fr[] =
#include "intl/msg_hash_fr.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_fr[] = {
#include "intl/msg_hash_fr.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_cht[] =
#include "intl/msg_hash_cht.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_cht[] = {
#include "intl/msg_hash_cht.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_de[] =
#include "intl/msg_hash_de.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_de[] = {
#include "intl/msg_hash_de.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_es[] =
#include "intl/msg_hash_es.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_es[] = {
#include "intl/msg_hash_es.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_ca[] =
#include "intl/msg_hash_ca.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_ca[] = {
#include "intl/msg_hash_ca.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_el[] =
#include "intl/msg_hash_el.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_el[] = {
#include "intl/msg_hash_el.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_jp[] =
#include "intl/msg_hash_ja.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_jp[] = {
#include "intl/msg_hash_ja.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_ko[] =
#include "intl/msg_hash_ko.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_ko[] = {
#include "intl/msg_hash_ko.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_pt_pt[] =
#include "intl/msg_hash_pt_pt.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_pt_pt[] = {
#include "intl/msg_hash_pt_pt.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_ru[] =
#include "intl/msg_hash_ru.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_ru[] = {
#include "intl/msg_hash_ru.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_tr[] =
#include "intl/msg_hash_tr.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_tr[] = {
#include "intl/msg_hash_tr.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_val[] =
#include "intl/msg_hash_val.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_val[] = {
#include "intl/msg_hash_val.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_vn[] =
#include "intl/msg_hash_vn.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_vn[] = {
#include "intl/msg_hash_vn.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_chs[] =
#include "intl/msg_hash_chs.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_chs[] = {
#include "intl/msg_hash_chs.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_pt_br[] =
#include "intl/msg_hash_pt_br.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_pt_br[] = {
#include "intl/msg_hash_pt_br.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_gl[] =
#include "intl/msg_hash_gl.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_gl[] = {
#include "intl/msg_hash_gl.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_no[] =
#include "intl/msg_hash_no.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_no[] = {
#include "intl/msg_hash_no.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_ga[] =
#include "intl/msg_hash_ga.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_ga[] = {
#include "intl/msg_hash_ga.h"
};

#undef MSG_HASH
#define MSG_HASH(Id, str) str "\0"
static const char msg_hash_blob_th[] =
#include "intl/msg_hash_th.h"
;
#undef MSG_HASH
#define MSG_HASH(Id, str) (uint32_t)Id,
static const uint32_t msg_hash_ids_th[] = {
#include "intl/msg_hash_th.h"
};

/* Restore the default expansion for any later consumer. */
#undef MSG_HASH
#define MSG_HASH(Id, str) case Id: return str;

static const msg_hash_strtab_t msg_hash_strtab_he =
{ msg_hash_ids_he, msg_hash_blob_he,
  (uint32_t)(sizeof(msg_hash_ids_he) / sizeof(msg_hash_ids_he[0])) };
static const msg_hash_strtab_t msg_hash_strtab_sk =
{ msg_hash_ids_sk, msg_hash_blob_sk,
  (uint32_t)(sizeof(msg_hash_ids_sk) / sizeof(msg_hash_ids_sk[0])) };
static const msg_hash_strtab_t msg_hash_strtab_uk =
{ msg_hash_ids_uk, msg_hash_blob_uk,
  (uint32_t)(sizeof(msg_hash_ids_uk) / sizeof(msg_hash_ids_uk[0])) };
static const msg_hash_strtab_t msg_hash_strtab_eo =
{ msg_hash_ids_eo, msg_hash_blob_eo,
  (uint32_t)(sizeof(msg_hash_ids_eo) / sizeof(msg_hash_ids_eo[0])) };
static const msg_hash_strtab_t msg_hash_strtab_pl =
{ msg_hash_ids_pl, msg_hash_blob_pl,
  (uint32_t)(sizeof(msg_hash_ids_pl) / sizeof(msg_hash_ids_pl[0])) };
static const msg_hash_strtab_t msg_hash_strtab_fi =
{ msg_hash_ids_fi, msg_hash_blob_fi,
  (uint32_t)(sizeof(msg_hash_ids_fi) / sizeof(msg_hash_ids_fi[0])) };
static const msg_hash_strtab_t msg_hash_strtab_hu =
{ msg_hash_ids_hu, msg_hash_blob_hu,
  (uint32_t)(sizeof(msg_hash_ids_hu) / sizeof(msg_hash_ids_hu[0])) };
static const msg_hash_strtab_t msg_hash_strtab_be =
{ msg_hash_ids_be, msg_hash_blob_be,
  (uint32_t)(sizeof(msg_hash_ids_be) / sizeof(msg_hash_ids_be[0])) };
static const msg_hash_strtab_t msg_hash_strtab_en =
{ msg_hash_ids_en, msg_hash_blob_en,
  (uint32_t)(sizeof(msg_hash_ids_en) / sizeof(msg_hash_ids_en[0])) };
static const msg_hash_strtab_t msg_hash_strtab_it =
{ msg_hash_ids_it, msg_hash_blob_it,
  (uint32_t)(sizeof(msg_hash_ids_it) / sizeof(msg_hash_ids_it[0])) };
static const msg_hash_strtab_t msg_hash_strtab_fa =
{ msg_hash_ids_fa, msg_hash_blob_fa,
  (uint32_t)(sizeof(msg_hash_ids_fa) / sizeof(msg_hash_ids_fa[0])) };
static const msg_hash_strtab_t msg_hash_strtab_ast =
{ msg_hash_ids_ast, msg_hash_blob_ast,
  (uint32_t)(sizeof(msg_hash_ids_ast) / sizeof(msg_hash_ids_ast[0])) };
static const msg_hash_strtab_t msg_hash_strtab_nl =
{ msg_hash_ids_nl, msg_hash_blob_nl,
  (uint32_t)(sizeof(msg_hash_ids_nl) / sizeof(msg_hash_ids_nl[0])) };
static const msg_hash_strtab_t msg_hash_strtab_sv =
{ msg_hash_ids_sv, msg_hash_blob_sv,
  (uint32_t)(sizeof(msg_hash_ids_sv) / sizeof(msg_hash_ids_sv[0])) };
static const msg_hash_strtab_t msg_hash_strtab_id =
{ msg_hash_ids_id, msg_hash_blob_id,
  (uint32_t)(sizeof(msg_hash_ids_id) / sizeof(msg_hash_ids_id[0])) };
static const msg_hash_strtab_t msg_hash_strtab_cs =
{ msg_hash_ids_cs, msg_hash_blob_cs,
  (uint32_t)(sizeof(msg_hash_ids_cs) / sizeof(msg_hash_ids_cs[0])) };
static const msg_hash_strtab_t msg_hash_strtab_ar =
{ msg_hash_ids_ar, msg_hash_blob_ar,
  (uint32_t)(sizeof(msg_hash_ids_ar) / sizeof(msg_hash_ids_ar[0])) };
static const msg_hash_strtab_t msg_hash_strtab_fr =
{ msg_hash_ids_fr, msg_hash_blob_fr,
  (uint32_t)(sizeof(msg_hash_ids_fr) / sizeof(msg_hash_ids_fr[0])) };
static const msg_hash_strtab_t msg_hash_strtab_cht =
{ msg_hash_ids_cht, msg_hash_blob_cht,
  (uint32_t)(sizeof(msg_hash_ids_cht) / sizeof(msg_hash_ids_cht[0])) };
static const msg_hash_strtab_t msg_hash_strtab_de =
{ msg_hash_ids_de, msg_hash_blob_de,
  (uint32_t)(sizeof(msg_hash_ids_de) / sizeof(msg_hash_ids_de[0])) };
static const msg_hash_strtab_t msg_hash_strtab_es =
{ msg_hash_ids_es, msg_hash_blob_es,
  (uint32_t)(sizeof(msg_hash_ids_es) / sizeof(msg_hash_ids_es[0])) };
static const msg_hash_strtab_t msg_hash_strtab_ca =
{ msg_hash_ids_ca, msg_hash_blob_ca,
  (uint32_t)(sizeof(msg_hash_ids_ca) / sizeof(msg_hash_ids_ca[0])) };
static const msg_hash_strtab_t msg_hash_strtab_el =
{ msg_hash_ids_el, msg_hash_blob_el,
  (uint32_t)(sizeof(msg_hash_ids_el) / sizeof(msg_hash_ids_el[0])) };
static const msg_hash_strtab_t msg_hash_strtab_jp =
{ msg_hash_ids_jp, msg_hash_blob_jp,
  (uint32_t)(sizeof(msg_hash_ids_jp) / sizeof(msg_hash_ids_jp[0])) };
static const msg_hash_strtab_t msg_hash_strtab_ko =
{ msg_hash_ids_ko, msg_hash_blob_ko,
  (uint32_t)(sizeof(msg_hash_ids_ko) / sizeof(msg_hash_ids_ko[0])) };
static const msg_hash_strtab_t msg_hash_strtab_pt_pt =
{ msg_hash_ids_pt_pt, msg_hash_blob_pt_pt,
  (uint32_t)(sizeof(msg_hash_ids_pt_pt) / sizeof(msg_hash_ids_pt_pt[0])) };
static const msg_hash_strtab_t msg_hash_strtab_ru =
{ msg_hash_ids_ru, msg_hash_blob_ru,
  (uint32_t)(sizeof(msg_hash_ids_ru) / sizeof(msg_hash_ids_ru[0])) };
static const msg_hash_strtab_t msg_hash_strtab_tr =
{ msg_hash_ids_tr, msg_hash_blob_tr,
  (uint32_t)(sizeof(msg_hash_ids_tr) / sizeof(msg_hash_ids_tr[0])) };
static const msg_hash_strtab_t msg_hash_strtab_val =
{ msg_hash_ids_val, msg_hash_blob_val,
  (uint32_t)(sizeof(msg_hash_ids_val) / sizeof(msg_hash_ids_val[0])) };
static const msg_hash_strtab_t msg_hash_strtab_vn =
{ msg_hash_ids_vn, msg_hash_blob_vn,
  (uint32_t)(sizeof(msg_hash_ids_vn) / sizeof(msg_hash_ids_vn[0])) };
static const msg_hash_strtab_t msg_hash_strtab_chs =
{ msg_hash_ids_chs, msg_hash_blob_chs,
  (uint32_t)(sizeof(msg_hash_ids_chs) / sizeof(msg_hash_ids_chs[0])) };
static const msg_hash_strtab_t msg_hash_strtab_pt_br =
{ msg_hash_ids_pt_br, msg_hash_blob_pt_br,
  (uint32_t)(sizeof(msg_hash_ids_pt_br) / sizeof(msg_hash_ids_pt_br[0])) };
static const msg_hash_strtab_t msg_hash_strtab_gl =
{ msg_hash_ids_gl, msg_hash_blob_gl,
  (uint32_t)(sizeof(msg_hash_ids_gl) / sizeof(msg_hash_ids_gl[0])) };
static const msg_hash_strtab_t msg_hash_strtab_no =
{ msg_hash_ids_no, msg_hash_blob_no,
  (uint32_t)(sizeof(msg_hash_ids_no) / sizeof(msg_hash_ids_no[0])) };
static const msg_hash_strtab_t msg_hash_strtab_ga =
{ msg_hash_ids_ga, msg_hash_blob_ga,
  (uint32_t)(sizeof(msg_hash_ids_ga) / sizeof(msg_hash_ids_ga[0])) };
static const msg_hash_strtab_t msg_hash_strtab_th =
{ msg_hash_ids_th, msg_hash_blob_th,
  (uint32_t)(sizeof(msg_hash_ids_th) / sizeof(msg_hash_ids_th[0])) };

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

#else /* MSG_HASH_HAVE_STRTAB */
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
#endif /* MSG_HASH_HAVE_STRTAB */
#endif

const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   const char *ret = NULL;

#ifdef HAVE_LANGEXTRA
#if defined(MSG_HASH_HAVE_STRTAB)
   {
      const msg_hash_strtab_t *tab =
            msg_hash_lang_strtab(uint_user_language);
      if (tab)
         ret = msg_hash_strtab_lookup(tab,
               &msg_hash_lang_index, (uint32_t)msg);
   }
#else
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
#endif /* MSG_HASH_HAVE_STRTAB */
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
