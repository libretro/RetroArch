/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2025 - The RetroArch team
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

/* ScreenScraper, display half.
 *
 * Reading a metadata sidecar is a local-file operation: the JSON has
 * already been fetched and written to disk by an earlier scrape, quite
 * possibly on a different machine. Menus that show scraped facts - the
 * Ozone and XMB metadata panels, playlist sublabels, the database info
 * list - therefore need nothing more than this file, so it is compiled
 * unconditionally.
 *
 * The other half (network/screenscraper.c and tasks/task_screenscraper.c)
 * builds the API requests, parses jeuInfos/ssuserInfos and tracks the
 * daily allowance. That genuinely needs networking and stays behind
 * HAVE_NETWORKING, which is why the two live in separate translation
 * units rather than one file full of #ifdefs.
 *
 * Consequence worth remembering: a target built without networking can
 * still display a thumbnails/ tree copied across from a PC, it just
 * cannot produce one.
 */

#include <stdlib.h>
#include <string.h>

#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#include <compat/strl.h>
#include <formats/rjson.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <lists/dir_list.h>
#include <lists/string_list.h>

#include "screenscraper.h"

#include "../configuration.h"

/* Whether a ScreenScraper account is configured. Display code calls
 * this to decide whether to offer the scraper-only media types, so it
 * has to resolve in every build - without the client there is nothing
 * to be signed in to, and the answer is a definite no. */
bool screenscraper_signed_in(void)
{
#ifdef HAVE_NETWORKING
   settings_t *settings = config_get_ptr();
   if (!settings)
      return false;
   return    !string_is_empty(settings->arrays.screenscraper_username)
          && !string_is_empty(settings->arrays.screenscraper_password)
          && !string_is_empty(settings->arrays.screenscraper_devid)
          && !string_is_empty(settings->arrays.screenscraper_devpassword);
#else
   return false;
#endif
}

/* ---- Metadata sidecar ---- */

size_t screenscraper_metadata_path(char *s, size_t len,
      const char *dir_thumbnails, const char *db_name,
      const char *img_name)
{
   char base[PATH_MAX_LENGTH];
   char dir[PATH_MAX_LENGTH];
   char file_name[NAME_MAX_LENGTH];

   if (   string_is_empty(dir_thumbnails)
       || string_is_empty(db_name)
       || string_is_empty(img_name))
      return 0;

   strlcpy(file_name, img_name, sizeof(file_name));
   path_remove_extension(file_name);
   strlcat(file_name, ".json", sizeof(file_name));

   fill_pathname_join_special(base, dir_thumbnails, db_name, sizeof(base));
   fill_pathname_join_special(dir, base, "Metadata", sizeof(dir));
   return fill_pathname_join_special(s, dir, file_name, len);
}

typedef struct
{
   char key[32];
   screenscraper_meta_t *meta;
} ss_meta_ctx_t;

static bool ss_meta_member(void *context, const char *s, size_t len)
{
   ss_meta_ctx_t *ctx = (ss_meta_ctx_t*)context;
   size_t _len = (len < sizeof(ctx->key) - 1) ? len : sizeof(ctx->key) - 1;
   memcpy(ctx->key, s, _len);
   ctx->key[_len] = '\0';
   return true;
}

static bool ss_meta_value(void *context, const char *s, size_t len)
{
   ss_meta_ctx_t *ctx        = (ss_meta_ctx_t*)context;
   screenscraper_meta_t *m   = ctx->meta;
   char *dst                 = NULL;
   size_t dst_size           = 0;

   if (string_is_equal(ctx->key, "name"))
   { dst = m->name;        dst_size = sizeof(m->name);        }
   else if (string_is_equal(ctx->key, "description"))
   { dst = m->description; dst_size = sizeof(m->description); }
   else if (string_is_equal(ctx->key, "genre"))
   { dst = m->genre;       dst_size = sizeof(m->genre);       }
   else if (string_is_equal(ctx->key, "developer"))
   { dst = m->developer;   dst_size = sizeof(m->developer);   }
   else if (string_is_equal(ctx->key, "publisher"))
   { dst = m->publisher;   dst_size = sizeof(m->publisher);   }
   else if (string_is_equal(ctx->key, "players"))
   { dst = m->players;     dst_size = sizeof(m->players);     }
   else if (string_is_equal(ctx->key, "rating"))
   { dst = m->rating;      dst_size = sizeof(m->rating);      }
   else if (string_is_equal(ctx->key, "releasedate"))
   { dst = m->releasedate; dst_size = sizeof(m->releasedate); }
   else if (string_is_equal(ctx->key, "esrb"))
   { dst = m->esrb;        dst_size = sizeof(m->esrb);        }
   else if (string_is_equal(ctx->key, "pegi"))
   { dst = m->pegi;        dst_size = sizeof(m->pegi);        }

   if (dst && dst_size > 0)
   {
      size_t i;
      size_t out  = 0;
      size_t _len = (len < dst_size - 1) ? len : dst_size - 1;

      /* Synopses arrive with the service's own paragraph breaks still
       * in them. The menus draw these as single runs of text, where a
       * control character has no glyph and comes out as an empty box,
       * so fold every control code down to a space and collapse the
       * runs that produces. Bytes >= 0x80 are left alone: they are
       * UTF-8 continuation bytes, not control codes. */
      for (i = 0; i < _len; i++)
      {
         unsigned char c = (unsigned char)s[i];

         if (c < 0x20 || c == 0x7F)
            c = ' ';

         if (c == ' ' && (out == 0 || dst[out - 1] == ' '))
            continue;

         dst[out++] = (char)c;
      }

      /* Drop a trailing space left by the collapse above */
      while (out > 0 && dst[out - 1] == ' ')
         out--;

      dst[out] = '\0';
      m->valid = true;
   }

   ctx->key[0] = '\0';
   return true;
}

static bool ss_meta_noop(void *context)
{
   ((ss_meta_ctx_t*)context)->key[0] = '\0';
   return true;
}

static bool ss_meta_bool(void *context, bool value)
{
   return ss_meta_noop(context);
}

bool screenscraper_metadata_load(const char *path,
      screenscraper_meta_t *meta)
{
   ss_meta_ctx_t ctx;
   rjson_t *json    = NULL;
   char *buf        = NULL;
   int64_t buf_len  = 0;
   enum rjson_type end;

   if (!meta)
      return false;

   memset(meta, 0, sizeof(*meta));

   if (string_is_empty(path) || !path_is_valid(path))
      return false;
   if (!filestream_read_file(path, (void**)&buf, &buf_len) || !buf)
      return false;

   if (!(json = rjson_open_buffer(buf, (size_t)buf_len)))
   {
      free(buf);
      return false;
   }

   memset(&ctx, 0, sizeof(ctx));
   ctx.meta = meta;

   rjson_set_options(json,
           RJSON_OPTION_ALLOW_UTF8BOM
         | RJSON_OPTION_ALLOW_TRAILING_DATA
         | RJSON_OPTION_REPLACE_INVALID_ENCODING);

   end = rjson_parse(json, &ctx,
         ss_meta_member,
         ss_meta_value,
         ss_meta_value,
         ss_meta_noop, ss_meta_noop,
         ss_meta_noop, ss_meta_noop,
         ss_meta_bool, ss_meta_noop);

   rjson_free(json);
   free(buf);

   return (end == RJSON_DONE) && meta->valid;
}

bool screenscraper_metadata_load_entry(const char *dir_thumbnails,
      const char *db_name, const char *img_name,
      screenscraper_meta_t *meta)
{
   char path[PATH_MAX_LENGTH];

   if (!screenscraper_metadata_path(path, sizeof(path),
            dir_thumbnails, db_name, img_name))
   {
      if (meta)
         memset(meta, 0, sizeof(*meta));
      return false;
   }

   return screenscraper_metadata_load(path, meta);
}

int screenscraper_rating_stars(const char *rating)
{
   int value;

   /* Absent, or something that is not a plain number: "unrated" */
   if (string_is_empty(rating) || !ISDIGIT((unsigned char)*rating))
      return -1;

   value = atoi(rating);

   if (value < 0)
      return -1;
   if (value > 20)
      value = 20;

   /* ScreenScraper rates on a 0-20 scale, so a star is worth four
    * points. Integer (value + 2) / 4 is value/4 rounded to the
    * nearest whole star with halves rounding up:
    *   0 -> 0, 1 -> 0, 2 -> 1, 17 -> 4, 18 -> 5, 20 -> 5. */
   return (value + 2) / 4;
}

bool screenscraper_extended_media_allowed(void)
{
   /* The media types no other source provides, so their presence is
    * proof a scrape reached this thumbnail tree at some point. */
   static const char *ss_dirs[] = {
      "Named_Boxarts3D",
      "Named_Fanarts",
      "Named_Marquees",
      "Named_Videos"
   };
   settings_t *settings        = config_get_ptr();
   struct string_list *systems = NULL;
   const char *dir_thumbnails  = NULL;
   bool found                  = false;
   size_t i;

   /* Signed in: offer the types whether or not anything has been
    * scraped yet, since the next scrape can fill them. */
   if (screenscraper_signed_in())
      return true;

   /* Not signed in - but a tree copied in from elsewhere, or left
    * behind after signing out, is still perfectly usable, so go by
    * what is actually on disk. */
   if (!settings)
      return false;

   dir_thumbnails = settings->paths.directory_thumbnails;

   if (   string_is_empty(dir_thumbnails)
       || !path_is_directory(dir_thumbnails))
      return false;

   if (!(systems = dir_list_new(dir_thumbnails, NULL, true, false,
               false, false)))
      return false;

   for (i = 0; i < systems->size && !found; i++)
   {
      size_t j;
      const char *system_dir = systems->elems[i].data;

      if (string_is_empty(system_dir) || !path_is_directory(system_dir))
         continue;

      for (j = 0; j < ARRAY_SIZE(ss_dirs); j++)
      {
         char probe[PATH_MAX_LENGTH];
         fill_pathname_join_special(probe, system_dir, ss_dirs[j],
               sizeof(probe));
         if (path_is_directory(probe))
         {
            found = true;
            break;
         }
      }
   }

   string_list_free(systems);

   return found;
}
