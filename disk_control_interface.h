/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (disk_control_interface.h).
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

#ifndef __DISK_CONTROL_INTERFACE_H
#define __DISK_CONTROL_INTERFACE_H

#include <retro_common_api.h>
#include <libretro.h>

#include <boolean.h>

#include "disk_index_file.h"

RETRO_BEGIN_DECLS

/* Holds all all objects to operate the disk
 * control interface */
typedef struct
{
   struct retro_disk_control_ext_callback cb; /* ptr alignment */
   disk_index_file_t index_record;            /* unsigned alignment */
   unsigned initial_num_images;
   bool record_enabled;
} disk_control_interface_t;

/*****************/
/* Configuration */
/*****************/

/* Set v0 disk interface callback functions */
void disk_control_set_callback(
      disk_control_interface_t *disk_control,
      const struct retro_disk_control_callback *cb);

/* Set v1+ disk interface callback functions */
void disk_control_set_ext_callback(
      disk_control_interface_t *disk_control,
      const struct retro_disk_control_ext_callback *cb);

/**********/
/* Status */
/**********/

/* Returns true if core supports basic disk
 * control functionality
 * - set_eject_state
 * - get_eject_state
 * - get_image_index
 * - set_image_index
 * - get_num_images */
bool disk_control_enabled(
      disk_control_interface_t *disk_control);

/* Returns true if core supports disk append
 * functionality
 * - replace_image_index
 * - add_image_index */
bool disk_control_append_enabled(
      disk_control_interface_t *disk_control);

/* Returns true if core supports image
 * labels
 * - get_image_label */
bool disk_control_image_label_enabled(
      disk_control_interface_t *disk_control);

/* Returns true if core supports setting
 * initial disk index
 * - set_initial_image
 * - get_image_path */
bool disk_control_initial_image_enabled(
      disk_control_interface_t *disk_control);

/***********/
/* Getters */
/***********/

/* Returns true if disk is currently ejected */
bool disk_control_get_eject_state(
      disk_control_interface_t *disk_control);

/* Returns number of disk images registered
 * by the core */
unsigned disk_control_get_num_images(
      disk_control_interface_t *disk_control);

/* Returns currently selected disk image index */
unsigned disk_control_get_image_index(
      disk_control_interface_t *disk_control);

/* Fetches core-provided disk image label
 * (label is set to an empty string if core
 * does not support image labels) */
void disk_control_get_image_label(
      disk_control_interface_t *disk_control,
      unsigned index, char *label, size_t len);

/***********/
/* Setters */
/***********/

/* Sets the eject state of the virtual disk tray */
bool disk_control_set_eject_state(
      disk_control_interface_t *disk_control,
      bool eject, bool verbosity);

/* Sets currently selected disk index
 * NOTE: Will fail if disk is not currently ejected */
bool disk_control_set_index(
      disk_control_interface_t *disk_control,
      unsigned index, bool verbosity);

/* Increments selected disk index */
bool disk_control_set_index_next(
      disk_control_interface_t *disk_control,
      bool verbosity);

/* Decrements selected disk index */
bool disk_control_set_index_prev(
      disk_control_interface_t *disk_control,
      bool verbosity);

/* Appends specified image file to disk image list */
bool disk_control_append_image(
      disk_control_interface_t *disk_control,
      const char *image_path);

/*****************************/
/* 'Initial index' functions */
/*****************************/

/* Attempts to set current core's initial disk index.
 * > disk_control->record_enabled will be set to
 *   'false' if core does not support initial
 *   index functionality
 * > disk_control->index_record will be loaded
 *   from file (if an existing record is found)
 * NOTE: Must be called immediately before
 * loading content */
bool disk_control_set_initial_index(
      disk_control_interface_t *disk_control,
      const char *content_path,
      const char *dir_savefile);

/* Checks that initial index has been set correctly
 * and provides user notification.
 * > Sets disk_control->initial_num_images if
 *   if functionality is supported by core
 * NOTE: Must be called immediately after
 * loading content */
bool disk_control_verify_initial_index(
      disk_control_interface_t *disk_control,
      bool verbosity);

/* Saves current disk index to file, if supported
 * by current core */
bool disk_control_save_image_index(
      disk_control_interface_t *disk_control);

RETRO_END_DECLS

#endif
