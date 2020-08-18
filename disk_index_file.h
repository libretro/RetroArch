/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (disk_index_file.h).
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

#ifndef __DISK_INDEX_FILE_H
#define __DISK_INDEX_FILE_H

#include <retro_common_api.h>
#include <libretro.h>
#include <retro_miscellaneous.h>

#include <boolean.h>

RETRO_BEGIN_DECLS

/* Holds all parameters required for recording
 * the last disk image selected via the disk
 * control interface */
typedef struct
{
   unsigned image_index;
   char image_path[PATH_MAX_LENGTH];
   char file_path[PATH_MAX_LENGTH];
   bool modified;
} disk_index_file_t;

/******************/
/* Initialisation */
/******************/

/* Initialises existing disk index record, loading
 * current parameters if a record file exists.
 * Returns false if arguments are invalid. */
bool disk_index_file_init(
      disk_index_file_t *disk_index_file,
      const char *content_path,
      const char *dir_savefile);

/***********/
/* Setters */
/***********/

/* Sets image index and path */
void disk_index_file_set(
      disk_index_file_t *disk_index_file,
      unsigned image_index,
      const char *image_path);

/**********/
/* Saving */
/**********/

/* Saves specified disk index file to disk */
bool disk_index_file_save(disk_index_file_t *disk_index_file);

RETRO_END_DECLS

#endif
