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

#ifndef __RARCH_SCREENSCRAPER_H
#define __RARCH_SCREENSCRAPER_H

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define SCREENSCRAPER_API_URL "https://api.screenscraper.fr/api2"
#define SCREENSCRAPER_SOFTNAME "RetroArch"

/* Media kinds RetroArch knows how to place into the thumbnails tree.
 * The first four map onto existing thumbnail types; the rest are new
 * asset directories populated exclusively by the scraper. */
enum screenscraper_media_kind
{
   SS_MEDIA_BOXART = 0,     /* box-2D        -> Named_Boxarts   */
   SS_MEDIA_SNAP,           /* ss            -> Named_Snaps     */
   SS_MEDIA_TITLE,          /* sstitle       -> Named_Titles    */
   SS_MEDIA_LOGO,           /* wheel         -> Named_Logos     */
   SS_MEDIA_BOXART_3D,      /* box-3D        -> Named_Boxarts3D */
   SS_MEDIA_FANART,         /* fanart        -> Named_Fanarts   */
   SS_MEDIA_MARQUEE,        /* screenmarquee -> Named_Marquees  */
   SS_MEDIA_VIDEO,          /* video-normalized -> Named_Videos */
   SS_MEDIA_MANUAL,         /* manuel        -> Named_Manuals   */
   SS_MEDIA_BEZEL,          /* bezel-16-9    -> Named_Bezels    */
   SS_MEDIA_LAST
};

/* One downloadable media entry parsed from a jeuInfos response */
typedef struct
{
   char *url;
   char *format;            /* "png", "mp4", "pdf", ...          */
   char type[32];           /* raw ScreenScraper media type name */
   char region[8];
} screenscraper_media_t;

/* Game description parsed from a jeuInfos response. Regional/language
 * variants are already resolved against the caller's preferences. */
typedef struct
{
   char *name;
   char *synopsis;
   char *genre;
   char *developer;
   char *publisher;
   char *players;
   char *rating;            /* ScreenScraper note, "x/20" scale  */
   char *release_date;
   char *esrb;
   char *pegi;
   screenscraper_media_t *media;
   size_t media_count;
} screenscraper_game_t;

/* Credentials/preferences for building API requests */
typedef struct
{
   const char *devid;
   const char *devpassword;
   const char *username;    /* ssid; optional                    */
   const char *password;    /* sspassword; optional              */
   const char *region;      /* preferred media region, e.g. "us" */
   const char *language;    /* preferred text language, e.g. "en"*/
} screenscraper_creds_t;

/* True when a complete set of ScreenScraper credentials is configured
 * (account username/password and developer id/password). While signed
 * in, ScreenScraper acts as the primary playlist scraper. */
bool screenscraper_signed_in(void);

/* Daily request allowance reported by the service alongside every
 * lookup. 'valid' is false when the response carried no user block. */
typedef struct
{
   unsigned used;
   unsigned max;
   bool valid;
} screenscraper_quota_t;

/* Process-wide "the daily allowance is spent" latch. While set, the
 * scraper stops issuing lookups; it clears on the next calendar day or
 * when a fresh account query reports headroom again. */
bool screenscraper_quota_exhausted(void);
void screenscraper_set_quota_exhausted(bool exhausted);

/* True when the HTTP status/response body denotes a spent allowance
 * rather than an ordinary lookup failure. */
bool screenscraper_status_is_quota(int status,
      const char *data, size_t len);

/* Builds a urlencoded ssuserInfos.php request (account status and
 * remaining allowance). Returns length, 0 on failure. */
size_t screenscraper_build_user_info_url(char *s, size_t len,
      const screenscraper_creds_t *creds);

/* Parses an ssuserInfos.php response into 'quota'. */
bool screenscraper_parse_user_info(const char *data, size_t len,
      screenscraper_quota_t *quota);

/* Region/language preference tables. Settings store an index; these
 * return the ScreenScraper wire code ("wor", "en", ...) and a human
 * readable name for menu display. Out-of-range indices return the
 * defaults (world / English). Counts must match the def-file range
 * constants SCREENSCRAPER_REGION_LAST / SCREENSCRAPER_LANGUAGE_LAST. */
const char *screenscraper_region_code(unsigned idx);
const char *screenscraper_region_name(unsigned idx);
const char *screenscraper_language_code(unsigned idx);
const char *screenscraper_language_name(unsigned idx);

/* Maps a libretro database name (playlist db_name, with or without
 * the ".lpl" suffix) to a ScreenScraper systemeid.
 * Returns 0 when the system is unknown. */
unsigned screenscraper_system_id(const char *db_name);

/* Builds a urlencoded jeuInfos.php request. Any of crc32/rom_name/
 * rom_size may be empty/0 and is then omitted. Returns length, 0 on
 * failure (missing dev credentials). */
size_t screenscraper_build_game_info_url(char *s, size_t len,
      const screenscraper_creds_t *creds,
      unsigned system_id, const char *crc32,
      const char *rom_name, int64_t rom_size);

/* Parses a jeuInfos JSON response. Region/language selection uses the
 * preferences in 'creds'. Returns NULL on parse/API failure. */
screenscraper_game_t *screenscraper_parse_game_info(
      const char *data, size_t len,
      const screenscraper_creds_t *creds);

/* Picks the best media entry of the given kind from a parsed game,
 * honouring the preferred region. Returns NULL if absent. */
const screenscraper_media_t *screenscraper_select_media(
      const screenscraper_game_t *game,
      enum screenscraper_media_kind kind,
      const char *preferred_region);

/* Thumbnail-tree subdirectory a media kind is stored under */
const char *screenscraper_media_sub_dir(enum screenscraper_media_kind kind);

/* File extension (without dot) to use when the media entry carries no
 * usable format field */
const char *screenscraper_media_default_ext(enum screenscraper_media_kind kind);

/* Serialises the metadata portion of 'game' as a JSON object.
 * Returns a heap string owned by the caller. */
char *screenscraper_metadata_to_json(const screenscraper_game_t *game);

void screenscraper_game_free(screenscraper_game_t *game);

RETRO_END_DECLS

#endif
