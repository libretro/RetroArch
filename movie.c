/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "movie.h"
#include "hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "general.h"
#include "dynamic.h"

struct bsv_movie
{
   FILE *file;

   size_t *frame_pos; // A ring buffer keeping track of positions in the file for each frame.
   size_t frame_mask;
   size_t frame_ptr;

   size_t min_file_pos;

   size_t state_size;
   uint8_t *state;

   bool playback;
   bool first_rewind;
   bool did_rewind;
};

static bool init_playback(bsv_movie_t *handle, const char *path)
{
   handle->playback = true;
   handle->file = fopen(path, "rb");
   if (!handle->file)
   {
      RARCH_ERR("Couldn't open BSV file \"%s\" for playback.\n", path);
      return false;
   }

   uint32_t header[4] = {0};
   if (fread(header, sizeof(uint32_t), 4, handle->file) != 4)
   {
      RARCH_ERR("Couldn't read movie header.\n");
      return false;
   }

   // Compatibility with old implementation that used incorrect documentation.
   if (swap_if_little32(header[MAGIC_INDEX]) != BSV_MAGIC && swap_if_big32(header[MAGIC_INDEX]) != BSV_MAGIC)
   {
      RARCH_ERR("Movie file is not a valid BSV1 file.\n");
      return false;
   }

   if (swap_if_big32(header[CRC_INDEX]) != g_extern.cart_crc)
      RARCH_WARN("CRC32 checksum mismatch between ROM file and saved ROM checksum in replay file header; replay highly likely to desync on playback.\n");

   uint32_t state_size = swap_if_big32(header[STATE_SIZE_INDEX]);

   if (state_size)
   {
      handle->state = (uint8_t*)malloc(state_size);
      handle->state_size = state_size;
      if (!handle->state)
         return false;

      if (fread(handle->state, 1, state_size, handle->file) != state_size)
      {
         RARCH_ERR("Couldn't read state from movie.\n");
         return false;
      }

      if (pretro_serialize_size() == state_size)
         pretro_unserialize(handle->state, state_size);
      else
         RARCH_WARN("Movie format seems to have a different serializer version. Will most likely fail.\n");
   }

   handle->min_file_pos = sizeof(header) + state_size;

   return true;
}

static bool init_record(bsv_movie_t *handle, const char *path)
{
   handle->file = fopen(path, "wb");
   if (!handle->file)
   {
      RARCH_ERR("Couldn't open BSV \"%s\" for recording.\n", path);
      return false;
   }

   uint32_t header[4] = {0};

   // This value is supposed to show up as BSV1 in a HEX editor, big-endian.
   header[MAGIC_INDEX] = swap_if_little32(BSV_MAGIC);

   header[CRC_INDEX] = swap_if_big32(g_extern.cart_crc);

   uint32_t state_size = pretro_serialize_size();

   header[STATE_SIZE_INDEX] = swap_if_big32(state_size);
   fwrite(header, 4, sizeof(uint32_t), handle->file);

   handle->min_file_pos = sizeof(header) + state_size;
   handle->state_size = state_size;

   if (state_size)
   {
      handle->state = (uint8_t*)malloc(state_size);
      if (!handle->state)
         return false;

      pretro_serialize(handle->state, state_size);
      fwrite(handle->state, 1, state_size, handle->file);
   }

   return true;
}

void bsv_movie_free(bsv_movie_t *handle)
{
   if (handle)
   {
      if (handle->file)
         fclose(handle->file);
      free(handle->state);
      free(handle->frame_pos);
      free(handle);
   }
}

bool bsv_movie_get_input(bsv_movie_t *handle, int16_t *input)
{
   if (fread(input, sizeof(int16_t), 1, handle->file) != 1)
      return false;

   *input = swap_if_big16(*input);
   return true;
}

void bsv_movie_set_input(bsv_movie_t *handle, int16_t input)
{
   input = swap_if_big16(input);
   fwrite(&input, sizeof(int16_t), 1, handle->file);
}

bsv_movie_t *bsv_movie_init(const char *path, enum rarch_movie_type type)
{
   bsv_movie_t *handle = (bsv_movie_t*)calloc(1, sizeof(*handle));
   if (!handle)
      return NULL;

   if (type == RARCH_MOVIE_PLAYBACK)
   {
      if (!init_playback(handle, path))
         goto error;
   }
   else if (!init_record(handle, path))
      goto error;

   // Just pick something really large :D ~1 million frames rewind should do the trick.
   if (!(handle->frame_pos = (size_t*)calloc((1 << 20), sizeof(size_t))))
      goto error; 

   handle->frame_pos[0] = handle->min_file_pos;
   handle->frame_mask = (1 << 20) - 1;

   return handle;

error:
   bsv_movie_free(handle);
   return NULL;
}

void bsv_movie_set_frame_start(bsv_movie_t *handle)
{
   handle->frame_pos[handle->frame_ptr] = ftell(handle->file);
}

void bsv_movie_set_frame_end(bsv_movie_t *handle)
{
   handle->frame_ptr = (handle->frame_ptr + 1) & handle->frame_mask;

   handle->first_rewind = !handle->did_rewind;
   handle->did_rewind = false;
}

void bsv_movie_frame_rewind(bsv_movie_t *handle)
{
   handle->did_rewind = true;

   // If we're at the beginning ... :)
   if ((handle->frame_ptr <= 1) && (handle->frame_pos[0] == handle->min_file_pos))
   {
      handle->frame_ptr = 0;
      fseek(handle->file, handle->min_file_pos, SEEK_SET);
   }
   else
   {
      // First time rewind is performed, the old frame is simply replayed.
      // However, playing back that frame caused us to read data, and push data to the ring buffer.
      // Sucessively rewinding frames, we need to rewind past the read data, plus another.
      handle->frame_ptr = (handle->frame_ptr - (handle->first_rewind ? 1 : 2)) & handle->frame_mask;
      fseek(handle->file, handle->frame_pos[handle->frame_ptr], SEEK_SET);
   }

   // We rewound past the beginning. :O
   if (ftell(handle->file) <= (long)handle->min_file_pos)
   {
      // If recording, we simply reset the starting point. Nice and easy.
      if (!handle->playback)
      {
         fseek(handle->file, 4 * sizeof(uint32_t), SEEK_SET);
         pretro_serialize(handle->state, handle->state_size);
         fwrite(handle->state, 1, handle->state_size, handle->file);
      }
      else
         fseek(handle->file, handle->min_file_pos, SEEK_SET);
   }
}

