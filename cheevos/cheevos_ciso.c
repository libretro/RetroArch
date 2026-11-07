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

/* rc_hash reads a CISO file through these as the disc image it stands
 * for. formats/ciso does no file I/O: what it asks for is read here,
 * on the descriptor path, straight to where it asks - which is always
 * the buffer rc_hash gave. */

#include <stdio.h>
#include <stdlib.h>

#include <boolean.h>
#include <formats/ciso.h>
#include <streams/file_stream.h>

#include "../deps/rcheevos/include/rc_consoles.h"
#include "../deps/rcheevos/include/rc_hash.h"

#include "cheevos_ciso.h"

typedef struct
{
   int64_t  pos;      /* in the disc image */
   uint64_t file_pos; /* where the file stands; seeks to it are skipped */
   ciso_t  *ciso;
   RFILE   *file;
} rcheevos_ciso_t;

/* Runs an operation's step until it is done, reading what it asks */
static int rcheevos_ciso_pump(rcheevos_ciso_t *handle, bool opening)
{
   for (;;)
   {
      ciso_request_t req;
      int64_t        got = 0;
      int            err = opening
            ? ciso_open_step(handle->ciso, &req)
            : ciso_read_step(handle->ciso, &req);

      if (err != CISO_PENDING)
         return err;
      if (     handle->file_pos == req.offset
            || filestream_seek(handle->file, (int64_t)req.offset,
               RETRO_VFS_SEEK_POSITION_START) == 0)
         got = filestream_read(handle->file, req.dst, (int64_t)req.length);
      /* unknown after a failed read: seek next time */
      handle->file_pos = got > 0 ? req.offset + (uint64_t)got : ~(uint64_t)0;
      ciso_feed(handle->ciso, got > 0 ? (size_t)got : 0);
   }
}

static void RC_CCONV rcheevos_ciso_close(void *file_handle)
{
   rcheevos_ciso_t *handle = (rcheevos_ciso_t*)file_handle;

   if (handle)
   {
      ciso_free(handle->ciso);
      if (handle->file)
         filestream_close(handle->file);
      free(handle);
   }
}

static void* RC_CCONV rcheevos_ciso_open(const char *path)
{
   rcheevos_ciso_t *handle = (rcheevos_ciso_t*)malloc(sizeof(*handle));

   if (!handle)
      return NULL;
   handle->pos      = 0;
   handle->file_pos = 0;
   handle->ciso     = NULL;
   /* the descriptor path: no stdio buffer between the file and the
    * buffers rc_hash reads into */
   if (     !(handle->file = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
               RETRO_VFS_FILE_ACCESS_HINT_SEQUENTIAL_BULK))
         || !(handle->ciso = ciso_new())
         || rcheevos_ciso_pump(handle, true) != CISO_OK)
   {
      rcheevos_ciso_close(handle);
      return NULL;
   }
   return handle;
}

static void RC_CCONV rcheevos_ciso_seek(void *file_handle, int64_t offset,
      int origin)
{
   rcheevos_ciso_t *handle = (rcheevos_ciso_t*)file_handle;

   switch (origin)
   {
      case SEEK_SET:
         handle->pos = offset;
         break;
      case SEEK_CUR:
         handle->pos += offset;
         break;
      case SEEK_END:
         handle->pos = (int64_t)ciso_disc_size(handle->ciso) + offset;
         break;
   }
   if (handle->pos < 0)
      handle->pos = 0;
}

static int64_t RC_CCONV rcheevos_ciso_tell(void *file_handle)
{
   return ((rcheevos_ciso_t*)file_handle)->pos;
}

static size_t RC_CCONV rcheevos_ciso_read(void *file_handle, void *buffer,
      size_t requested_bytes)
{
   rcheevos_ciso_t *handle = (rcheevos_ciso_t*)file_handle;
   size_t           got;

   if (ciso_read_begin(handle->ciso, (uint64_t)handle->pos,
            buffer, requested_bytes) != CISO_OK)
      return 0;
   rcheevos_ciso_pump(handle, false);
   /* what came before a failure is still read */
   got          = ciso_read_progress(handle->ciso);
   handle->pos += (int64_t)got;
   return got;
}

uint32_t rcheevos_ciso_get_console_id(const char *path)
{
   uint8_t          magic[8];
   uint32_t         console_id = RC_CONSOLE_UNKNOWN;
   rcheevos_ciso_t *handle     = (rcheevos_ciso_t*)rcheevos_ciso_open(path);

   if (!handle)
      return console_id;
   rcheevos_ciso_seek(handle, 0x18, SEEK_SET);
   if (rcheevos_ciso_read(handle, magic, sizeof(magic)) == sizeof(magic))
   {
      if (     magic[0] == 0x5D && magic[1] == 0x1C
            && magic[2] == 0x9E && magic[3] == 0xA3)
         console_id = RC_CONSOLE_WII;
      else if (magic[4] == 0xC2 && magic[5] == 0x33
            && magic[6] == 0x9F && magic[7] == 0x3D)
         console_id = RC_CONSOLE_GAMECUBE;
   }
   rcheevos_ciso_close(handle);
   return console_id;
}

void rcheevos_ciso_filereader_init(void)
{
   struct rc_hash_filereader filereader;

   filereader.open  = rcheevos_ciso_open;
   filereader.seek  = rcheevos_ciso_seek;
   filereader.tell  = rcheevos_ciso_tell;
   filereader.read  = rcheevos_ciso_read;
   filereader.close = rcheevos_ciso_close;
   rc_hash_init_custom_filereader(&filereader);
}
