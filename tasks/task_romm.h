/*  RetroArch - A frontend for libretro.
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

#ifndef __TASK_ROMM_H
#define __TASK_ROMM_H

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <queues/task_queue.h>

RETRO_BEGIN_DECLS

#define ROMM_PAGE_SIZE 100

/* A menu entry made from a RomM response.  @path is what the entry
 * carries (see below), @label is what it shows. */
typedef bool (*romm_list_cb_t)(void *data, const char *path,
      const char *label, bool next_page);

/* The menu passes RomM state around in entry paths:
 *   list context  "<offset>|<platform id>|<system>|<platform fs slug>"
 *                 (empty: the list of platforms)
 *   ROM context   "<rom id>|<1 if several files>|<cover path>|<file name>"
 * Everything in them came from the server and is checked before it is
 * used as a path component. */
bool romm_name_is_safe(const char *s);

/* @system and @fs_slug may be NULL when the caller has no use for them */
bool romm_list_ctx_split(const char *ctx, unsigned *offset, unsigned *id,
      char *system, size_t system_size, char *fs_slug, size_t slug_size);

bool romm_rom_ctx_split(const char *ctx, unsigned *id, bool *multi,
      char *cover, size_t cover_size, char *fs_name, size_t name_size);

/* An archive member name: relative, no empty, "." or ".." segment, and
 * nothing a path separator or a drive could be made of ("/" splits the
 * segments, a trailing one marks a directory). */
bool romm_entry_is_safe(const char *name);

/* Picks what the playlist gets from the members of a multi-file ROM, the
 * way RomM does for its own playlists. With several discs (descriptors
 * .cue/.gdi/.ccd/.mds, whose tracks are not discs, or else disc images
 * such as .chd/.iso/.bin) in RomM's order, "(Disc N)" first, it is the
 * .m3u listing them: @m3u gets its text and @target @m3u_name, for the
 * caller to write. With one disc it is that member, otherwise the largest
 * one. Returns false if nothing fits. */
bool romm_archive_target(const char *const *names, const uint32_t *sizes,
      size_t count, const char *m3u_name, char *target, size_t target_size,
      char **m3u);

/* Walks a /api/platforms (@list_ctx empty) or /api/roms response and
 * calls @cb for each usable entry, then once more for the next page
 * when the library has more. Returns false on malformed JSON. */
bool romm_list_parse(const char *json, size_t len, const char *list_ctx,
      romm_list_cb_t cb, void *data);

#ifdef HAVE_NETWORKING
void *task_push_romm_list(const char *list_ctx,
      retro_task_callback_t cb, void *user_data);

void task_push_romm_download(const char *list_ctx, const char *rom_ctx);
#endif

RETRO_END_DECLS

#endif
