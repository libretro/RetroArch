/* Copyright  (C) 2010-2019 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (logiqx_dat.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_LOGIQX_DAT_H__
#define __LIBRETRO_SDK_FORMAT_LOGIQX_DAT_H__

#include <retro_common_api.h>
#include <retro_miscellaneous.h>

#include <boolean.h>

RETRO_BEGIN_DECLS

/* Trivial handler for DAT files in Logiqx XML format
 * (http://www.logiqx.com/). Provides bare minimum
 * functionality - predominantly concerned with obtaining
 * description text for specific arcade ROM images.
 *
 * Note: Also supports the following alternative DAT
 * formats, since they are functionally identical to
 * Logiqx XML (but with different element names):
 * > MAME List XML
 * > MAME 'Software List' */

/* Prevent direct access to logiqx_dat_t members */
typedef struct logiqx_dat logiqx_dat_t;

/* Holds all metadata for a single game entry
 * in the DAT file (minimal at present - may be
 * expanded with individual internal ROM data
 * if required) */
typedef struct
{
   char name[PATH_MAX_LENGTH];
   char description[PATH_MAX_LENGTH];
   char year[8];
   char manufacturer[128];
   bool is_bios;
   bool is_runnable;
} logiqx_dat_game_info_t;

/* Validation */

/* Performs rudimentary validation of the specified
 * Logiqx XML DAT file path (not rigorous - just
 * enough to prevent obvious errors).
 * Also provides access to file size (DAT files can
 * be very large, so it is useful to have this information
 * on hand - i.e. so we can check that the system has
 * enough free memory to load the file). */ 
bool logiqx_dat_path_is_valid(const char *path, uint64_t *file_size);

/* File initialisation/de-initialisation */

/* Loads specified Logiqx XML DAT file from disk.
 * Returned logiqx_dat_t object must be free'd using
 * logiqx_dat_free().
 * Returns NULL if file is invalid or a read error
 * occurs. */
logiqx_dat_t *logiqx_dat_init(const char *path);

/* Frees specified DAT file */
void logiqx_dat_free(logiqx_dat_t *dat_file);

/* Game information access */

/* Sets/resets internal node pointer to the first
 * entry in the DAT file */
void logiqx_dat_set_first(logiqx_dat_t *dat_file);

/* Fetches game information for the current entry
 * in the DAT file and increments the internal node
 * pointer.
 * Returns false if the end of the DAT file has been
 * reached (in which case 'game_info' will be invalid) */
bool logiqx_dat_get_next(
      logiqx_dat_t *dat_file, logiqx_dat_game_info_t *game_info);

/* Fetches information for the specified game.
 * Returns false if game does not exist, or arguments
 * are invalid. */
bool logiqx_dat_search(
      logiqx_dat_t *dat_file, const char *game_name,
      logiqx_dat_game_info_t *game_info);

RETRO_END_DECLS

#endif
