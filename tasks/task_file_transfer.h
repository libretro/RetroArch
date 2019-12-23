/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef TASKS_FILE_TRANSFER_H
#define TASKS_FILE_TRANSFER_H

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_miscellaneous.h>

#include <queues/message_queue.h>

#include "../msg_hash.h"

RETRO_BEGIN_DECLS

enum nbio_status_enum
{
   NBIO_STATUS_INIT = 0,
   NBIO_STATUS_TRANSFER,
   NBIO_STATUS_TRANSFER_PARSE,
   NBIO_STATUS_TRANSFER_FINISHED
};

enum nbio_type
{
   NBIO_TYPE_NONE = 0,
   NBIO_TYPE_JPEG,
   NBIO_TYPE_PNG,
   NBIO_TYPE_TGA,
   NBIO_TYPE_BMP,
   NBIO_TYPE_OGG,
   NBIO_TYPE_FLAC,
   NBIO_TYPE_MP3,
   NBIO_TYPE_MOD,
   NBIO_TYPE_WAV
};

enum nbio_status_flags
{
   NBIO_FLAG_NONE = 0,
   NBIO_FLAG_IMAGE_SUPPORTS_RGBA
};

typedef int (*transfer_cb_t)(void *data, size_t len);

typedef struct nbio_handle
{
   enum nbio_type type;
   bool is_finished;
   unsigned status;
   unsigned pos_increment;
   uint32_t status_flags;
   void *data;
   char *path;
   struct nbio_t *handle;
   msg_queue_t *msg_queue;
   transfer_cb_t  cb;
} nbio_handle_t;

typedef struct
{
   enum msg_hash_enums enum_idx;
   char path[PATH_MAX_LENGTH];
   void *user_data;
} file_transfer_t;

RETRO_END_DECLS

#endif
