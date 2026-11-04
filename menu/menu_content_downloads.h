/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

#ifndef __MENU_CONTENT_DOWNLOADS_H
#define __MENU_CONTENT_DOWNLOADS_H

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Content Downloader: which entries of a category folder finished
 * downloading.  An archive is extracted and then deleted when
 * auto-extract is on, and its contents land under names taken from
 * inside it, so the entry's own name is not on disk afterwards.  Each
 * finished download is therefore also recorded, one name per line, in
 * MENU_CONTENT_DOWNLOADS_RECORD inside the category folder. */
#define MENU_CONTENT_DOWNLOADS_RECORD ".downloaded"

/* Reads @dir's record.  Returns NULL when there is none; free() the
 * result. */
char *menu_content_downloads_load(const char *dir);

/* True when @name is in the folder, or in @record (as loaded above,
 * may be NULL). */
bool menu_content_downloads_present(const char *dir, const char *record,
      const char *name);

/* Adds @name to @dir's record, once. */
bool menu_content_downloads_add(const char *dir, const char *name);

RETRO_END_DECLS

#endif
