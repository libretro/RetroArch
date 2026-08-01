/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - RetroArch
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation, either version 3 of the License, or (at your option)
 *  any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with RetroArch. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#include <compat/strl.h>
#include <formats/rjson.h>
#include <net/net_http.h>

#include "screenscraper.h"

#include "../configuration.h"

bool screenscraper_signed_in(void)
{
   settings_t *settings = config_get_ptr();
   if (!settings)
      return false;
   return    !string_is_empty(settings->arrays.screenscraper_username)
          && !string_is_empty(settings->arrays.screenscraper_password)
          && !string_is_empty(settings->arrays.screenscraper_devid)
          && !string_is_empty(settings->arrays.screenscraper_devpassword);
}

/* ---- Request allowance tracking ---- */

static size_t ss_append_param(char *s, size_t len, size_t _len,
      const char *key, const char *value);

static bool ss_quota_exhausted = false;

bool screenscraper_quota_exhausted(void)
{
   return ss_quota_exhausted;
}

void screenscraper_set_quota_exhausted(bool exhausted)
{
   ss_quota_exhausted = exhausted;
}

bool screenscraper_status_is_quota(int status,
      const char *data, size_t len)
{
   /* 429 is the documented "too many requests" status; the service
    * also answers 430/431 for the per-day and per-minute ceilings, and
    * some error paths return the explanation with a 400. */
   if (status == 429 || status == 430 || status == 431)
      return true;

   if (data && len > 0)
   {
      char buf[512];
      size_t i;
      size_t _len = (len < sizeof(buf) - 1) ? len : sizeof(buf) - 1;
      memcpy(buf, data, _len);
      buf[_len] = '\0';
      for (i = 0; i < _len; i++)
         buf[i] = (char)tolower((unsigned char)buf[i]);

      if (   strstr(buf, "quota")
          || strstr(buf, "maximum threads")
          || strstr(buf, "closed for non-registered")
          || strstr(buf, "requests per day")
          || strstr(buf, "non membre"))
         return true;
   }

   return false;
}

size_t screenscraper_build_user_info_url(char *s, size_t len,
      const screenscraper_creds_t *creds)
{
   size_t _len;

   if (   !creds
       || string_is_empty(creds->devid)
       || string_is_empty(creds->devpassword))
      return 0;

   _len = strlcpy(s, SCREENSCRAPER_API_URL "/ssuserInfos.php?output=json",
         len);
   _len = ss_append_param(s, len, _len, "devid",       creds->devid);
   _len = ss_append_param(s, len, _len, "devpassword", creds->devpassword);
   _len = ss_append_param(s, len, _len, "softname",    SCREENSCRAPER_SOFTNAME);
   _len = ss_append_param(s, len, _len, "ssid",        creds->username);
   _len = ss_append_param(s, len, _len, "sspassword",  creds->password);

   return _len;
}

/* ---- Region/language preference tables ---- */

typedef struct
{
   const char *code;
   const char *name;
} ss_pref_entry_t;

/* Keep in sync with SCREENSCRAPER_REGION_LAST in
 * settings/settings_def_screenscraper.h */
static const ss_pref_entry_t ss_regions[] = {
   { "wor", "World"          },
   { "us",  "USA"            },
   { "eu",  "Europe"         },
   { "jp",  "Japan"          },
   { "asi", "Asia"           },
   { "au",  "Australia"      },
   { "br",  "Brazil"         },
   { "ca",  "Canada"         },
   { "cn",  "China"          },
   { "de",  "Germany"        },
   { "es",  "Spain"          },
   { "fr",  "France"         },
   { "it",  "Italy"          },
   { "kr",  "Korea"          },
   { "nl",  "Netherlands"    },
   { "pt",  "Portugal"       },
   { "ru",  "Russia"         },
   { "uk",  "United Kingdom" },
};

/* Keep in sync with SCREENSCRAPER_LANGUAGE_LAST in
 * settings/settings_def_screenscraper.h */
static const ss_pref_entry_t ss_languages[] = {
   { "en", "English"    },
   { "fr", "French"     },
   { "de", "German"     },
   { "es", "Spanish"    },
   { "it", "Italian"    },
   { "pt", "Portuguese" },
   { "nl", "Dutch"      },
   { "ja", "Japanese"   },
   { "ko", "Korean"     },
   { "ru", "Russian"    },
   { "zh", "Chinese"    },
   { "pl", "Polish"     },
   { "sv", "Swedish"    },
   { "no", "Norwegian"  },
   { "da", "Danish"     },
   { "fi", "Finnish"    },
};

const char *screenscraper_region_code(unsigned idx)
{
   if (idx < ARRAY_SIZE(ss_regions))
      return ss_regions[idx].code;
   return ss_regions[0].code;
}

const char *screenscraper_region_name(unsigned idx)
{
   if (idx < ARRAY_SIZE(ss_regions))
      return ss_regions[idx].name;
   return ss_regions[0].name;
}

const char *screenscraper_language_code(unsigned idx)
{
   if (idx < ARRAY_SIZE(ss_languages))
      return ss_languages[idx].code;
   return ss_languages[0].code;
}

const char *screenscraper_language_name(unsigned idx)
{
   if (idx < ARRAY_SIZE(ss_languages))
      return ss_languages[idx].name;
   return ss_languages[0].name;
}

/* ---- libretro database name -> ScreenScraper systemeid ---- */

typedef struct
{
   const char *db_name;
   unsigned id;
} ss_system_map_t;

/* Well-known ScreenScraper platform identifiers, keyed by libretro
 * database (playlist) names. Extend freely; order is irrelevant. */
static const ss_system_map_t ss_system_map[] = {
   { "Nintendo - Nintendo Entertainment System",        3   },
   { "Nintendo - Family Computer Disk System",          106 },
   { "Nintendo - Super Nintendo Entertainment System",  4   },
   { "Nintendo - Nintendo 64",                          14  },
   { "Nintendo - GameCube",                             13  },
   { "Nintendo - Wii",                                  16  },
   { "Nintendo - Wii U",                                18  },
   { "Nintendo - Game Boy",                             9   },
   { "Nintendo - Game Boy Color",                       10  },
   { "Nintendo - Game Boy Advance",                     12  },
   { "Nintendo - Nintendo DS",                          15  },
   { "Nintendo - Nintendo 3DS",                         17  },
   { "Nintendo - Virtual Boy",                          11  },
   { "Nintendo - Pokemon Mini",                         211 },
   { "Sega - SG-1000",                                  109 },
   { "Sega - Master System - Mark III",                 2   },
   { "Sega - Mega Drive - Genesis",                     1   },
   { "Sega - 32X",                                      19  },
   { "Sega - Mega-CD - Sega CD",                        20  },
   { "Sega - Game Gear",                                21  },
   { "Sega - Saturn",                                   22  },
   { "Sega - Dreamcast",                                23  },
   { "Sony - PlayStation",                              57  },
   { "Sony - PlayStation 2",                            58  },
   { "Sony - PlayStation 3",                            59  },
   { "Sony - PlayStation Portable",                     61  },
   { "Sony - PlayStation Vita",                         62  },
   { "NEC - PC Engine - TurboGrafx 16",                 31  },
   { "NEC - PC Engine SuperGrafx",                      105 },
   { "NEC - PC Engine CD - TurboGrafx-CD",              114 },
   { "NEC - PC-FX",                                     72  },
   { "SNK - Neo Geo",                                   142 },
   { "SNK - Neo Geo CD",                                70  },
   { "SNK - Neo Geo Pocket",                            25  },
   { "SNK - Neo Geo Pocket Color",                      82  },
   { "Atari - 2600",                                    26  },
   { "Atari - 5200",                                    40  },
   { "Atari - 7800",                                    41  },
   { "Atari - Jaguar",                                  27  },
   { "Atari - Lynx",                                    28  },
   { "Atari - ST",                                      42  },
   { "Bandai - WonderSwan",                             45  },
   { "Bandai - WonderSwan Color",                       46  },
   { "Coleco - ColecoVision",                           48  },
   { "Mattel - Intellivision",                          115 },
   { "Magnavox - Odyssey2",                             104 },
   { "Microsoft - MSX",                                 113 },
   { "Microsoft - MSX2",                                116 },
   { "Commodore - 64",                                  66  },
   { "Commodore - Amiga",                               64  },
   { "Amstrad - CPC",                                   65  },
   { "Sinclair - ZX Spectrum",                          76  },
   { "GCE - Vectrex",                                   102 },
   { "The 3DO Company - 3DO",                           29  },
   { "MAME",                                            75  },
   { "FBNeo - Arcade Games",                            75  },
   { "DOS",                                             135 },
   { "ScummVM",                                         123 },
};

unsigned screenscraper_system_id(const char *db_name)
{
   char name[NAME_MAX_LENGTH];
   size_t i;
   char *ext = NULL;

   if (string_is_empty(db_name))
      return 0;

   /* Strip a trailing ".lpl", if present */
   strlcpy(name, db_name, sizeof(name));
   if ((ext = strrchr(name, '.')) && string_is_equal(ext, ".lpl"))
      *ext = '\0';

   for (i = 0; i < ARRAY_SIZE(ss_system_map); i++)
   {
      if (string_is_equal(name, ss_system_map[i].db_name))
         return ss_system_map[i].id;
   }

   return 0;
}

/* ---- Request building ---- */

static size_t ss_append_param(char *s, size_t len, size_t _len,
      const char *key, const char *value)
{
   char *encoded = NULL;

   if (string_is_empty(value))
      return _len;

   net_http_urlencode(&encoded, value);
   if (!encoded)
      return _len;

   _len += snprintf(s + _len, len - _len, "&%s=%s", key, encoded);
   free(encoded);
   return _len;
}

size_t screenscraper_build_game_info_url(char *s, size_t len,
      const screenscraper_creds_t *creds,
      unsigned system_id, const char *crc32,
      const char *rom_name, int64_t rom_size)
{
   size_t _len;

   if (   !creds
       || string_is_empty(creds->devid)
       || string_is_empty(creds->devpassword))
      return 0;

   _len  = strlcpy(s, SCREENSCRAPER_API_URL "/jeuInfos.php?output=json",
         len);
   _len  = ss_append_param(s, len, _len, "devid",       creds->devid);
   _len  = ss_append_param(s, len, _len, "devpassword", creds->devpassword);
   _len  = ss_append_param(s, len, _len, "softname",    SCREENSCRAPER_SOFTNAME);
   _len  = ss_append_param(s, len, _len, "ssid",        creds->username);
   _len  = ss_append_param(s, len, _len, "sspassword",  creds->password);

   if (system_id > 0)
      _len += snprintf(s + _len, len - _len, "&systemeid=%u", system_id);

   _len += snprintf(s + _len, len - _len, "&romtype=rom");
   _len  = ss_append_param(s, len, _len, "crc",    crc32);
   _len  = ss_append_param(s, len, _len, "romnom", rom_name);

   if (rom_size > 0)
      _len += snprintf(s + _len, len - _len, "&romtaille=%lld",
            (long long)rom_size);

   return _len;
}

/* ---- Response parsing ---- */

/* Region preference chain used when the configured region misses */
static const char *ss_region_fallback[] = {
   "wor", "us", "eu", "jp", "ss", "uk", "fr"
};

static int ss_region_score(const char *region, const char *preferred)
{
   size_t i;
   if (string_is_empty(region))
      return (int)ARRAY_SIZE(ss_region_fallback) + 2;
   if (!string_is_empty(preferred) && string_is_equal(region, preferred))
      return 0;
   for (i = 0; i < ARRAY_SIZE(ss_region_fallback); i++)
   {
      if (string_is_equal(region, ss_region_fallback[i]))
         return (int)i + 1;
   }
   return (int)ARRAY_SIZE(ss_region_fallback) + 1;
}

static int ss_language_score(const char *lang, const char *preferred)
{
   if (string_is_empty(lang))
      return 3;
   if (!string_is_empty(preferred) && string_is_equal(lang, preferred))
      return 0;
   if (string_is_equal(lang, "en"))
      return 1;
   return 2;
}

#define SS_MAX_DEPTH 12
#define SS_WORST_SCORE 0x7fffffff

typedef struct
{
   rjson_t *json;
   const screenscraper_creds_t *creds;
   screenscraper_game_t *game;

   /* Names of the containers enclosing the current position;
    * stack[0] is the outermost object/array. */
   char stack[SS_MAX_DEPTH][20];
   unsigned depth;
   /* Member name seen last at the current level; becomes the name of
    * the next container that opens. */
   char pending_key[20];

   /* Scores of the currently selected regional/language variants */
   int name_score;
   int synopsis_score;
   int date_score;

   /* Accumulator for the current array item (nom/date/media/...) */
   char item_region[8];
   char item_lang[8];
   char item_type[32];
   char item_parent[16];
   char *item_text;
   char *item_url;
   char *item_format;

   /* Genre handling: per-genre best language variant */
   int genre_lang_score;
   char *genre_best;
} ss_parse_ctx_t;

static void ss_set_str(char **dst, const char *src, size_t len)
{
   if (*dst)
      free(*dst);
   if (!(*dst = (char*)malloc(len + 1)))
      return;
   memcpy(*dst, src, len);
   (*dst)[len] = '\0';
}

static void ss_replace_str(char **dst, char **src)
{
   if (*dst)
      free(*dst);
   *dst = *src;
   *src = NULL;
}

static void ss_reset_item(ss_parse_ctx_t *ctx)
{
   ctx->item_region[0] = '\0';
   ctx->item_lang[0]   = '\0';
   ctx->item_type[0]   = '\0';
   ctx->item_parent[0] = '\0';
   if (ctx->item_text)
   {
      free(ctx->item_text);
      ctx->item_text = NULL;
   }
   if (ctx->item_url)
   {
      free(ctx->item_url);
      ctx->item_url = NULL;
   }
   if (ctx->item_format)
   {
      free(ctx->item_format);
      ctx->item_format = NULL;
   }
}

/* True when the enclosing containers, innermost last, match the
 * argument list (a NULL matches any name, e.g. array items). */
static bool ss_in(ss_parse_ctx_t *ctx, unsigned n, ...)
{
   va_list ap;
   unsigned i;
   bool match = true;

   if (ctx->depth < n)
      return false;

   va_start(ap, n);
   for (i = 0; i < n; i++)
   {
      const char *want = va_arg(ap, const char*);
      const char *have = ctx->stack[ctx->depth - n + i];
      if (want && !string_is_equal(want, have))
      {
         match = false;
         break;
      }
   }
   va_end(ap);
   return match;
}

static bool ss_handle_member(void *context, const char *s, size_t len)
{
   ss_parse_ctx_t *ctx = (ss_parse_ctx_t*)context;
   size_t _len = (len < sizeof(ctx->pending_key) - 1)
         ? len : sizeof(ctx->pending_key) - 1;
   memcpy(ctx->pending_key, s, _len);
   ctx->pending_key[_len] = '\0';
   return true;
}

static bool ss_handle_start(void *context)
{
   ss_parse_ctx_t *ctx = (ss_parse_ctx_t*)context;

   if (ctx->depth < SS_MAX_DEPTH)
      strlcpy(ctx->stack[ctx->depth], ctx->pending_key,
            sizeof(ctx->stack[0]));
   ctx->pending_key[0] = '\0';
   ctx->depth++;

   /* Array items inherit the array's name, so paths read
    * "jeu > noms > noms" for each item of the noms array. */
   if (   ctx->depth >= 2 && ctx->depth <= SS_MAX_DEPTH
       && ctx->stack[ctx->depth - 1][0] == '\0')
      strlcpy(ctx->stack[ctx->depth - 1], ctx->stack[ctx->depth - 2],
            sizeof(ctx->stack[0]));

   /* A fresh item of one of the collected arrays begins */
   if (   ss_in(ctx, 2, "noms", "noms")
       || ss_in(ctx, 2, "synopsis", "synopsis")
       || ss_in(ctx, 2, "dates", "dates")
       || ss_in(ctx, 2, "medias", "medias")
       || ss_in(ctx, 2, "classifications", "classifications"))
      ss_reset_item(ctx);
   else if (ss_in(ctx, 2, "genres", "genres"))
   {
      ctx->genre_lang_score = SS_WORST_SCORE;
      if (ctx->genre_best)
      {
         free(ctx->genre_best);
         ctx->genre_best = NULL;
      }
   }

   return true;
}

static void ss_commit_item(ss_parse_ctx_t *ctx)
{
   screenscraper_game_t *game = ctx->game;

   /* jeu > noms item: pick the best-region name */
   if (ss_in(ctx, 3, "jeu", "noms", "noms"))
   {
      int score = ss_region_score(ctx->item_region, ctx->creds->region);
      if (ctx->item_text && score < ctx->name_score)
      {
         ctx->name_score = score;
         ss_replace_str(&game->name, &ctx->item_text);
      }
   }
   /* jeu > synopsis item: pick the best-language synopsis */
   else if (ss_in(ctx, 3, "jeu", "synopsis", "synopsis"))
   {
      int score = ss_language_score(ctx->item_lang, ctx->creds->language);
      if (ctx->item_text && score < ctx->synopsis_score)
      {
         ctx->synopsis_score = score;
         ss_replace_str(&game->synopsis, &ctx->item_text);
      }
   }
   /* jeu > dates item: pick the best-region release date */
   else if (ss_in(ctx, 3, "jeu", "dates", "dates"))
   {
      int score = ss_region_score(ctx->item_region, ctx->creds->region);
      if (ctx->item_text && score < ctx->date_score)
      {
         ctx->date_score = score;
         ss_replace_str(&game->release_date, &ctx->item_text);
      }
   }
   /* jeu > classifications item */
   else if (ss_in(ctx, 3, "jeu", "classifications", "classifications"))
   {
      if (ctx->item_text)
      {
         if (string_starts_with(ctx->item_type, "ESRB"))
            ss_replace_str(&game->esrb, &ctx->item_text);
         else if (string_starts_with(ctx->item_type, "PEGI"))
            ss_replace_str(&game->pegi, &ctx->item_text);
      }
   }
   /* jeu > medias item */
   else if (ss_in(ctx, 3, "jeu", "medias", "medias"))
   {
      /* Only game-level media; skip system/company artwork */
      if (   ctx->item_url && ctx->item_type[0]
          && (   ctx->item_parent[0] == '\0'
              || string_is_equal(ctx->item_parent, "jeu")))
      {
         screenscraper_media_t *tmp = (screenscraper_media_t*)realloc(
               game->media,
               (game->media_count + 1) * sizeof(screenscraper_media_t));
         if (tmp)
         {
            screenscraper_media_t *m = &tmp[game->media_count];
            game->media              = tmp;
            m->url                   = ctx->item_url;
            m->format                = ctx->item_format;
            ctx->item_url            = NULL;
            ctx->item_format         = NULL;
            strlcpy(m->type,   ctx->item_type,   sizeof(m->type));
            strlcpy(m->region, ctx->item_region, sizeof(m->region));
            game->media_count++;
         }
      }
   }
   /* jeu > genres item: append the best-language genre name */
   else if (ss_in(ctx, 3, "jeu", "genres", "genres"))
   {
      if (ctx->genre_best)
      {
         if (!game->genre)
         {
            game->genre     = ctx->genre_best;
            ctx->genre_best = NULL;
         }
         else
         {
            size_t old_len = strlen(game->genre);
            size_t add_len = strlen(ctx->genre_best);
            char *joined   = (char*)malloc(old_len + 2 + add_len + 1);
            if (joined)
            {
               memcpy(joined, game->genre, old_len);
               memcpy(joined + old_len, ", ", 2);
               memcpy(joined + old_len + 2, ctx->genre_best, add_len + 1);
               free(game->genre);
               game->genre = joined;
            }
            free(ctx->genre_best);
            ctx->genre_best = NULL;
         }
      }
   }
}

static bool ss_handle_end(void *context)
{
   ss_parse_ctx_t *ctx = (ss_parse_ctx_t*)context;

   ss_commit_item(ctx);

   if (ctx->depth > 0)
   {
      ctx->depth--;
      if (ctx->depth < SS_MAX_DEPTH)
         ctx->stack[ctx->depth][0] = '\0';
   }
   ctx->pending_key[0] = '\0';
   return true;
}

static bool ss_handle_string(void *context, const char *s, size_t len)
{
   ss_parse_ctx_t *ctx        = (ss_parse_ctx_t*)context;
   const char *key            = ctx->pending_key;
   screenscraper_game_t *game = ctx->game;

   /* Regional/language array items (noms, synopsis, dates,
    * classifications, medias) */
   if (   ss_in(ctx, 2, "noms", "noms")
       || ss_in(ctx, 2, "synopsis", "synopsis")
       || ss_in(ctx, 2, "dates", "dates")
       || ss_in(ctx, 2, "classifications", "classifications")
       || ss_in(ctx, 2, "medias", "medias"))
   {
      if (string_is_equal(key, "region"))
      {
         size_t _len = (len < sizeof(ctx->item_region) - 1)
               ? len : sizeof(ctx->item_region) - 1;
         memcpy(ctx->item_region, s, _len);
         ctx->item_region[_len] = '\0';
      }
      else if (string_is_equal(key, "langue"))
      {
         size_t _len = (len < sizeof(ctx->item_lang) - 1)
               ? len : sizeof(ctx->item_lang) - 1;
         memcpy(ctx->item_lang, s, _len);
         ctx->item_lang[_len] = '\0';
      }
      else if (string_is_equal(key, "type"))
      {
         size_t _len = (len < sizeof(ctx->item_type) - 1)
               ? len : sizeof(ctx->item_type) - 1;
         memcpy(ctx->item_type, s, _len);
         ctx->item_type[_len] = '\0';
      }
      else if (string_is_equal(key, "parent"))
      {
         size_t _len = (len < sizeof(ctx->item_parent) - 1)
               ? len : sizeof(ctx->item_parent) - 1;
         memcpy(ctx->item_parent, s, _len);
         ctx->item_parent[_len] = '\0';
      }
      else if (string_is_equal(key, "text"))
         ss_set_str(&ctx->item_text, s, len);
      else if (string_is_equal(key, "url"))
         ss_set_str(&ctx->item_url, s, len);
      else if (string_is_equal(key, "format"))
         ss_set_str(&ctx->item_format, s, len);
   }
   /* jeu > genres > item > noms > item */
   else if (ss_in(ctx, 2, "genres", "noms"))
   {
      if (string_is_equal(key, "langue"))
      {
         size_t _len = (len < sizeof(ctx->item_lang) - 1)
               ? len : sizeof(ctx->item_lang) - 1;
         memcpy(ctx->item_lang, s, _len);
         ctx->item_lang[_len] = '\0';
      }
      else if (string_is_equal(key, "text"))
      {
         int score = ss_language_score(ctx->item_lang,
               ctx->creds->language);
         if (score < ctx->genre_lang_score)
         {
            ctx->genre_lang_score = score;
            ss_set_str(&ctx->genre_best, s, len);
         }
      }
   }
   /* Scalar members of jeu sub-objects */
   else if (ss_in(ctx, 2, "jeu", "editeur"))
   {
      if (string_is_equal(key, "text"))
         ss_set_str(&game->publisher, s, len);
   }
   else if (ss_in(ctx, 2, "jeu", "developpeur"))
   {
      if (string_is_equal(key, "text"))
         ss_set_str(&game->developer, s, len);
   }
   else if (ss_in(ctx, 2, "jeu", "joueurs"))
   {
      if (string_is_equal(key, "text"))
         ss_set_str(&game->players, s, len);
   }
   else if (ss_in(ctx, 2, "jeu", "note"))
   {
      if (string_is_equal(key, "text"))
         ss_set_str(&game->rating, s, len);
   }

   ctx->pending_key[0] = '\0';
   return true;
}

static bool ss_handle_scalar(void *context)
{
   ss_parse_ctx_t *ctx = (ss_parse_ctx_t*)context;
   ctx->pending_key[0] = '\0';
   return true;
}

static bool ss_handle_number(void *context, const char *s, size_t len)
{
   return ss_handle_scalar(context);
}

static bool ss_handle_bool(void *context, bool value)
{
   return ss_handle_scalar(context);
}

static bool ss_handle_null(void *context)
{
   return ss_handle_scalar(context);
}

void screenscraper_game_free(screenscraper_game_t *game)
{
   size_t i;

   if (!game)
      return;

   free(game->name);
   free(game->synopsis);
   free(game->genre);
   free(game->developer);
   free(game->publisher);
   free(game->players);
   free(game->rating);
   free(game->release_date);
   free(game->esrb);
   free(game->pegi);

   for (i = 0; i < game->media_count; i++)
   {
      free(game->media[i].url);
      free(game->media[i].format);
   }
   free(game->media);
   free(game);
}

screenscraper_game_t *screenscraper_parse_game_info(
      const char *data, size_t len,
      const screenscraper_creds_t *creds)
{
   ss_parse_ctx_t ctx;
   enum rjson_type end;
   rjson_t *json = NULL;
   screenscraper_game_t *game = NULL;

   if (!data || len == 0 || !creds)
      return NULL;

   if (!(json = rjson_open_buffer(data, len)))
      return NULL;

   if (!(game = (screenscraper_game_t*)calloc(1, sizeof(*game))))
   {
      rjson_free(json);
      return NULL;
   }

   memset(&ctx, 0, sizeof(ctx));
   ctx.json           = json;
   ctx.creds          = creds;
   ctx.game           = game;
   ctx.name_score     = SS_WORST_SCORE;
   ctx.synopsis_score = SS_WORST_SCORE;
   ctx.date_score     = SS_WORST_SCORE;

   rjson_set_options(json,
           RJSON_OPTION_ALLOW_UTF8BOM
         | RJSON_OPTION_ALLOW_TRAILING_DATA
         | RJSON_OPTION_REPLACE_INVALID_ENCODING);

   end = rjson_parse(json, &ctx,
         ss_handle_member,
         ss_handle_string,
         ss_handle_number,
         ss_handle_start,  /* object start */
         ss_handle_end,    /* object end   */
         ss_handle_start,  /* array start  */
         ss_handle_end,    /* array end    */
         ss_handle_bool,
         ss_handle_null);

   rjson_free(json);
   ss_reset_item(&ctx);
   if (ctx.genre_best)
      free(ctx.genre_best);

   if (end != RJSON_DONE)
   {
      screenscraper_game_free(game);
      return NULL;
   }

   /* An entry without any usable content means the lookup missed */
   if (!game->name && game->media_count == 0)
   {
      screenscraper_game_free(game);
      return NULL;
   }

   return game;
}

/* ---- Account allowance parsing ---- */

typedef struct
{
   char key[32];
   screenscraper_quota_t *quota;
   unsigned depth;
   bool in_user;
} ss_user_ctx_t;

static bool ss_user_member(void *context, const char *s, size_t len)
{
   ss_user_ctx_t *ctx = (ss_user_ctx_t*)context;
   size_t _len = (len < sizeof(ctx->key) - 1) ? len : sizeof(ctx->key) - 1;
   memcpy(ctx->key, s, _len);
   ctx->key[_len] = '\0';
   /* The account block is the object introduced by this member */
   if (string_is_equal(ctx->key, "ssuser"))
      ctx->in_user = true;
   return true;
}

static bool ss_user_value(void *context, const char *s, size_t len)
{
   ss_user_ctx_t *ctx = (ss_user_ctx_t*)context;

   if (ctx->in_user)
   {
      char val[32];
      size_t _len = (len < sizeof(val) - 1) ? len : sizeof(val) - 1;
      memcpy(val, s, _len);
      val[_len] = '\0';

      if (string_is_equal(ctx->key, "requeststoday"))
      {
         ctx->quota->used  = (unsigned)strtoul(val, NULL, 10);
         ctx->quota->valid = true;
      }
      else if (string_is_equal(ctx->key, "maxrequestsperday"))
      {
         ctx->quota->max   = (unsigned)strtoul(val, NULL, 10);
         ctx->quota->valid = true;
      }
   }

   ctx->key[0] = '\0';
   return true;
}

static bool ss_user_start(void *context)
{
   ss_user_ctx_t *ctx = (ss_user_ctx_t*)context;
   ctx->depth++;
   return true;
}

static bool ss_user_end(void *context)
{
   ss_user_ctx_t *ctx = (ss_user_ctx_t*)context;
   if (ctx->depth > 0)
      ctx->depth--;
   /* Leaving the account object */
   if (ctx->in_user && ctx->depth <= 2)
      ctx->in_user = false;
   return true;
}

static bool ss_user_bool(void *context, bool value)
{
   ((ss_user_ctx_t*)context)->key[0] = '\0';
   return true;
}

static bool ss_user_null(void *context)
{
   ((ss_user_ctx_t*)context)->key[0] = '\0';
   return true;
}

bool screenscraper_parse_user_info(const char *data, size_t len,
      screenscraper_quota_t *quota)
{
   ss_user_ctx_t ctx;
   rjson_t *json = NULL;
   enum rjson_type end;

   if (!data || len == 0 || !quota)
      return false;

   memset(&ctx, 0, sizeof(ctx));
   memset(quota, 0, sizeof(*quota));
   ctx.quota = quota;

   if (!(json = rjson_open_buffer(data, len)))
      return false;

   rjson_set_options(json,
           RJSON_OPTION_ALLOW_UTF8BOM
         | RJSON_OPTION_ALLOW_TRAILING_DATA
         | RJSON_OPTION_REPLACE_INVALID_ENCODING);

   end = rjson_parse(json, &ctx,
         ss_user_member,
         ss_user_value,
         ss_user_value,   /* numbers arrive as text too */
         ss_user_start, ss_user_end,
         ss_user_start, ss_user_end,
         ss_user_bool, ss_user_null);

   rjson_free(json);

   return (end == RJSON_DONE) && quota->valid;
}

/* ---- Media selection ---- */

typedef struct
{
   const char *primary;
   const char *secondary;
} ss_media_names_t;

static const ss_media_names_t ss_media_names[SS_MEDIA_LAST] = {
   { "box-2D",           NULL         }, /* SS_MEDIA_BOXART    */
   { "ss",               NULL         }, /* SS_MEDIA_SNAP      */
   { "sstitle",          NULL         }, /* SS_MEDIA_TITLE     */
   { "wheel",            "wheel-hd"   }, /* SS_MEDIA_LOGO      */
   { "box-3D",           NULL         }, /* SS_MEDIA_BOXART_3D */
   { "fanart",           NULL         }, /* SS_MEDIA_FANART    */
   { "screenmarquee",    "marquee"    }, /* SS_MEDIA_MARQUEE   */
   { "video-normalized", "video"      }, /* SS_MEDIA_VIDEO     */
   { "manuel",           NULL         }, /* SS_MEDIA_MANUAL    */
   { "bezel-16-9",       NULL         }, /* SS_MEDIA_BEZEL     */
};

static const char *ss_media_sub_dirs[SS_MEDIA_LAST] = {
   "Named_Boxarts",
   "Named_Snaps",
   "Named_Titles",
   "Named_Logos",
   "Named_Boxarts3D",
   "Named_Fanarts",
   "Named_Marquees",
   "Named_Videos",
   "Named_Manuals",
   "Named_Bezels",
};

static const char *ss_media_default_exts[SS_MEDIA_LAST] = {
   "png", "png", "png", "png", "png", "png", "png", "mp4", "pdf", "png"
};

const char *screenscraper_media_sub_dir(enum screenscraper_media_kind kind)
{
   if (kind < SS_MEDIA_LAST)
      return ss_media_sub_dirs[kind];
   return NULL;
}

const char *screenscraper_media_default_ext(
      enum screenscraper_media_kind kind)
{
   if (kind < SS_MEDIA_LAST)
      return ss_media_default_exts[kind];
   return "png";
}

const screenscraper_media_t *screenscraper_select_media(
      const screenscraper_game_t *game,
      enum screenscraper_media_kind kind,
      const char *preferred_region)
{
   size_t i;
   const screenscraper_media_t *best = NULL;
   int best_score                    = SS_WORST_SCORE;

   if (!game || kind >= SS_MEDIA_LAST)
      return NULL;

   for (i = 0; i < game->media_count; i++)
   {
      const screenscraper_media_t *m = &game->media[i];
      int score;
      int type_penalty = 0;

      if (string_is_equal(m->type, ss_media_names[kind].primary))
         type_penalty = 0;
      else if (   ss_media_names[kind].secondary
               && string_is_equal(m->type,
                     ss_media_names[kind].secondary))
         type_penalty = 100;
      else
         continue;

      score = type_penalty + ss_region_score(m->region, preferred_region);
      if (score < best_score)
      {
         best_score = score;
         best       = m;
      }
   }

   return best;
}

/* ---- Metadata sidecar ---- */

static void ss_json_pair(rjsonwriter_t *writer,
      const char *key, const char *value)
{
   if (string_is_empty(value))
      return;
   rjsonwriter_add_string(writer, key);
   rjsonwriter_raw(writer, ":", 1);
   rjsonwriter_add_string(writer, value);
   rjsonwriter_raw(writer, ",", 1);
   rjsonwriter_raw(writer, "\n", 1);
}

char *screenscraper_metadata_to_json(const screenscraper_game_t *game)
{
   rjsonwriter_t *writer = NULL;
   const char *buf       = NULL;
   char *result          = NULL;
   int len               = 0;

   if (!game)
      return NULL;

   if (!(writer = rjsonwriter_open_memory()))
      return NULL;

   rjsonwriter_raw(writer, "{", 1);
   rjsonwriter_raw(writer, "\n", 1);
   ss_json_pair(writer, "name",        game->name);
   ss_json_pair(writer, "description", game->synopsis);
   ss_json_pair(writer, "genre",       game->genre);
   ss_json_pair(writer, "developer",   game->developer);
   ss_json_pair(writer, "publisher",   game->publisher);
   ss_json_pair(writer, "players",     game->players);
   ss_json_pair(writer, "rating",      game->rating);
   ss_json_pair(writer, "releasedate", game->release_date);
   ss_json_pair(writer, "esrb",        game->esrb);
   ss_json_pair(writer, "pegi",        game->pegi);
   /* Source marker doubles as the always-present final member,
    * keeping the object free of trailing commas. */
   rjsonwriter_add_string(writer, "source");
   rjsonwriter_raw(writer, ":", 1);
   rjsonwriter_add_string(writer, "screenscraper");
   rjsonwriter_raw(writer, "\n", 1);
   rjsonwriter_raw(writer, "}", 1);
   rjsonwriter_raw(writer, "\n", 1);

   buf = rjsonwriter_get_memory_buffer(writer, &len);
   if (buf && len > 0)
   {
      if ((result = (char*)malloc((size_t)len + 1)))
      {
         memcpy(result, buf, (size_t)len);
         result[len] = '\0';
      }
   }

   rjsonwriter_free(writer);
   return result;
}
