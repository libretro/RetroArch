/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (m3u_file.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_FORMAT_M3U_FILE_H__
#define __LIBRETRO_SDK_FORMAT_M3U_FILE_H__

#include <retro_common_api.h>

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>

RETRO_BEGIN_DECLS

/* Trivial handler for M3U playlist files */

/* M3U file extension */
#define M3U_FILE_EXT "m3u"

/* Prevent direct access to m3u_file_t members */
typedef struct content_m3u_file m3u_file_t;

/* Holds all metadata for a single M3U file entry */
typedef struct
{
   char *path;
   char *full_path;
   char *label;
} m3u_file_entry_t;

/* Defines entry label formatting when
 * writing M3U files to disk */
enum m3u_file_label_type
{
   M3U_FILE_LABEL_NONE = 0,
   M3U_FILE_LABEL_NONSTD,
   M3U_FILE_LABEL_EXTSTD,
   M3U_FILE_LABEL_RETRO
};

/* File Initialisation / De-Initialisation */

/* Creates and initialises an M3U file
 * - If 'path' refers to an existing file,
 *   contents is parsed
 * - If path does not exist, an empty M3U file
 *   is created
 * - Returned m3u_file_t object must be free'd using
 *   m3u_file_free()
 * - Returns NULL in the event of an error */
m3u_file_t *m3u_file_init(const char *path);

/* Frees specified M3U file */
void m3u_file_free(m3u_file_t *m3u_file);

/* Getters */

/* Returns M3U file path */
char *m3u_file_get_path(m3u_file_t *m3u_file);

/* Returns number of entries in M3U file */
size_t m3u_file_get_size(m3u_file_t *m3u_file);

/* Fetches specified M3U file entry
 * - Returns false if 'idx' is invalid, or internal
 *   entry is NULL */
bool m3u_file_get_entry(
      m3u_file_t *m3u_file, size_t idx, m3u_file_entry_t **entry);

/* Setters */

/* Adds specified entry to the M3U file
 * - Returns false if path is invalid, or
 *   memory could not be allocated for the
 *   entry */
bool m3u_file_add_entry(
      m3u_file_t *m3u_file, const char *path, const char *label);

/* Removes all entries in M3U file */
void m3u_file_clear(m3u_file_t *m3u_file);

/* Saving */

/* Saves M3U file to disk
 * - Setting 'label_type' to M3U_FILE_LABEL_NONE
 *   just outputs entry paths - this the most
 *   common format supported by most cores
 * - Returns false in the event of an error */
bool m3u_file_save(
      m3u_file_t *m3u_file, enum m3u_file_label_type label_type);

/* Utilities */

/* Sorts M3U file entries in alphabetical order */
void m3u_file_qsort(m3u_file_t *m3u_file);

/* Returns true if specified path corresponds
 * to an M3U file (simple convenience function) */
bool m3u_file_is_m3u(const char *path);

RETRO_END_DECLS

#endif
