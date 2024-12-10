/*  RetroArch - A frontend for libretro.
 *  Copyright (c) 2024 Aleksander Mazur
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

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <retro_endianness.h>
#include "record_wav.h"
#include "../../verbosity.h"

/**************************************/

/** WAV substructure. */
typedef struct
{
   uint16_t tag;        /**< Format tag. */
   uint16_t channels;   /**< Number of channels. */
   uint32_t sps;        /**< Samples per second. */
   uint32_t bps;        /**< Bits per sample. */
   uint16_t block;      /**< Bits per block (one sample for all channels). */
   uint16_t sample;     /**< Bits per sample (single channel). */
} waveformatex_t;

/** WAV header. */
typedef struct
{
   char riff[4];        /**< "RIFF" header tag. */
   uint32_t riff_size;  /**< RIFF file size.  */
   char fourcc[4];      /**< "WAVE" tag. */
   char fmt_tag[4];     /**< "fmt " chunk tag. */
   uint32_t fmt_size;   /**< Size of the following "fmt " chunk. */
   waveformatex_t fmt;  /**< Content of "fmt " chunk. */
   char data_tag[4];    /**< "data" chunk tag. */
   uint32_t data_size;  /**< Size of the following "data" chunk. */
} wav_hdr_t;

/** Our private context related to a single recording. */
typedef struct
{
   FILE *f;
   unsigned frame;      /**< Bytes per sample * number of channels. */
   uint32_t length;     /**< Bytes of audio data recorded so far. */
} record_wav_t;

#define WAV_MAX_LENGTH (0xFFFFFFFF - sizeof(wav_hdr_t))

/**************************************/

static bool wav_write_hdr(record_wav_t *handle, unsigned channels, unsigned samplerate)
{
   wav_hdr_t header = { "RIFF", };
   uint32_t length;

   if (!handle || !handle->f)
      return false;

   handle->frame       = 2 * channels;
   /* set initial size to 4 hours; to be fixed inside record_wav_finalize */
   length              = 4 * 60 * 60 * samplerate * handle->frame;
   header.riff_size    = swap_if_big32(sizeof(wav_hdr_t) - offsetof(wav_hdr_t, fourcc) + length);
   memcpy(header.fourcc, "WAVE", sizeof(header.fourcc));
   memcpy(header.fmt_tag, "fmt ", sizeof(header.fmt_tag));
   header.fmt_size     = swap_if_big32(sizeof(header.fmt));
   header.fmt.tag      = swap_if_big16(1);
   header.fmt.channels = swap_if_big16(channels);
   header.fmt.sps      = swap_if_big16(samplerate);
   header.fmt.bps      = swap_if_big16(samplerate * handle->frame);
   header.fmt.block    = swap_if_big16(handle->frame);
   header.fmt.sample   = swap_if_big16(16);
   memcpy(header.data_tag, "data", sizeof(header.data_tag));
   header.data_size    = swap_if_big32(length);

   return fwrite(&header, sizeof(header), 1, handle->f) == 1;
}

static bool write_le32_at(FILE *f, long offset, uint32_t value)
{
   if (fseek(f, offset, SEEK_SET))
      return false;
   value = swap_if_big32(value);
   if (fwrite(&value, sizeof(value), 1, f) != 1)
      return false;

   return true;
}

static bool wav_fix_hdr_and_close(record_wav_t *handle)
{
   if (!handle || !handle->f)
      return false;

   if (!write_le32_at(handle->f, offsetof(wav_hdr_t, riff_size),
         sizeof(wav_hdr_t) - offsetof(wav_hdr_t, fourcc) + handle->length))
      return false;
   if (!write_le32_at(handle->f, offsetof(wav_hdr_t, data_size),
         handle->length))
      return false;
   fclose(handle->f);
   handle->f = NULL;
   return true;
}

/**************************************/

static void *record_wav_new(const struct record_params *params)
{
   record_wav_t *handle = calloc(1, sizeof(*handle));
   if (!handle)
      return NULL;

   do
   {
      handle->f = fopen(params->filename, "wb");
      if (!handle->f)
      {
         RARCH_ERR("[WAV]: Cannot create %s: %s\n",
               params->filename, strerror(errno));
         break;
      }

      if (!wav_write_hdr(handle, params->channels, params->samplerate))
      {
         RARCH_ERR("[WAV]: Cannot write header to %s: %s\n",
               params->filename, strerror(errno));
         break;
      }

      return handle;
   } while (0);

   free(handle);
   return NULL;
}

static bool record_wav_push_audio(void *data,
      const struct record_audio_data *audio_data)
{
   record_wav_t *handle = (record_wav_t*)data;
   size_t frames, max, bytes;

   if (!handle || !audio_data || !handle->f)
      return false;

   frames = audio_data->frames;
   max = (WAV_MAX_LENGTH - handle->length) / handle->frame;
   if (frames > max)
      frames = max;
   bytes = frames * handle->frame;
   if (fwrite(audio_data->data, bytes, 1, handle->f) != 1)
      return false;
   handle->length += bytes;
   if (frames == max)
   {
      /* cannot append more data */
      RARCH_LOG("[WAV]: Size limit reached\n");
      if (!wav_fix_hdr_and_close(handle))
         return false;
   }
   return true;
}

static bool record_wav_finalize(void *data)
{
   record_wav_t *handle = (record_wav_t*)data;
   if (!handle)
      return false;

   return wav_fix_hdr_and_close(handle);
}

static void record_wav_free(void *data)
{
   record_wav_t *handle = (record_wav_t*)data;
   if (!handle)
      return;

   if (handle->f)
      fclose(handle->f);
   free(handle);
}

const record_driver_t record_wav = {
   record_wav_new,
   record_wav_free,
   NULL,
   record_wav_push_audio,
   record_wav_finalize,
   "wav",
};
