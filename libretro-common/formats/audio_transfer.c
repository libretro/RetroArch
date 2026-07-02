/* Copyright  (C) 2010-2024 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (audio_transfer.c).
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>

#include <formats/audio.h>
#include <compat/strcasestr.h>
#include <string/stdstring.h>

#ifdef HAVE_RFLAC
#include <formats/rflac.h>
#endif

/* One transfer context per codec. Each backend keeps only what it needs;
 * the enum 'type' handed to every entry point selects which arm runs, the
 * same switch-dispatch pattern formats/image_transfer.c uses. */

#ifdef HAVE_RFLAC
struct audio_transfer_flac
{
   const void *data;    /* encoded bytes from set_buffer_ptr (caller-owned) */
   size_t      size;
   rflac      *handle;  /* opened decoder, NULL until start() succeeds      */
};
#endif

enum audio_type_enum audio_decode_get_type(const char *path)
{
   if (string_is_empty(path))
      return AUDIO_TYPE_NONE;
   if (strcasestr(path, ".flac"))
      return AUDIO_TYPE_FLAC;
   if (strcasestr(path, ".ogg"))
      return AUDIO_TYPE_VORBIS;
   if (strcasestr(path, ".mp3"))
      return AUDIO_TYPE_MP3;
   if (strcasestr(path, ".wav"))
      return AUDIO_TYPE_WAV;
   return AUDIO_TYPE_NONE;
}

void *audio_transfer_new(enum audio_type_enum type)
{
   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
         return calloc(1, sizeof(struct audio_transfer_flac));
#endif
      case AUDIO_TYPE_WAV:
      case AUDIO_TYPE_VORBIS:
      case AUDIO_TYPE_MP3:
      case AUDIO_TYPE_NONE:
      default:
         break;
   }
   return NULL;
}

void audio_transfer_set_buffer_ptr(void *data, enum audio_type_enum type,
      void *ptr, size_t len)
{
   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         if (fl)
         {
            fl->data = ptr;
            fl->size = len;
         }
         break;
      }
#endif
      case AUDIO_TYPE_WAV:
      case AUDIO_TYPE_VORBIS:
      case AUDIO_TYPE_MP3:
      case AUDIO_TYPE_NONE:
      default:
         break;
   }
}

bool audio_transfer_start(void *data, enum audio_type_enum type)
{
   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         if (!fl || !fl->data)
            return false;
         fl->handle = rflac_open_memory(fl->data, fl->size, NULL);
         return fl->handle != NULL;
      }
#endif
      case AUDIO_TYPE_WAV:
      case AUDIO_TYPE_VORBIS:
      case AUDIO_TYPE_MP3:
      case AUDIO_TYPE_NONE:
      default:
         break;
   }
   return false;
}

bool audio_transfer_is_valid(void *data, enum audio_type_enum type)
{
   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         return (fl && fl->handle);
      }
#endif
      case AUDIO_TYPE_WAV:
      case AUDIO_TYPE_VORBIS:
      case AUDIO_TYPE_MP3:
      case AUDIO_TYPE_NONE:
      default:
         break;
   }
   return false;
}

bool audio_transfer_info(void *data, enum audio_type_enum type,
      unsigned *channels, unsigned *rate, uint64_t *total_frames)
{
   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         if (!fl || !fl->handle)
            return false;
         if (channels)
            *channels     = (unsigned)fl->handle->channels;
         if (rate)
            *rate         = (unsigned)fl->handle->sampleRate;
         if (total_frames)
            *total_frames = (uint64_t)fl->handle->totalPCMFrameCount;
         return true;
      }
#endif
      case AUDIO_TYPE_WAV:
      case AUDIO_TYPE_VORBIS:
      case AUDIO_TYPE_MP3:
      case AUDIO_TYPE_NONE:
      default:
         break;
   }
   return false;
}

int audio_transfer_read_s16(void *data, enum audio_type_enum type,
      int16_t *out, size_t frames, size_t *frames_out)
{
   size_t produced = 0;

   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         if (!fl || !fl->handle)
            return AUDIO_PROCESS_ERROR;
         produced = (size_t)rflac_read_pcm_frames_s16(
               fl->handle, (rflac_uint64)frames, out);
         break;
      }
#endif
      case AUDIO_TYPE_WAV:
      case AUDIO_TYPE_VORBIS:
      case AUDIO_TYPE_MP3:
      case AUDIO_TYPE_NONE:
      default:
         return AUDIO_PROCESS_ERROR;
   }

   if (frames_out)
      *frames_out = produced;
   return (produced == 0) ? AUDIO_PROCESS_END : AUDIO_PROCESS_NEXT;
}

int audio_transfer_read_f32(void *data, enum audio_type_enum type,
      float *out, size_t frames, size_t *frames_out)
{
   size_t produced = 0;

   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         if (!fl || !fl->handle)
            return AUDIO_PROCESS_ERROR;
         produced = (size_t)rflac_read_pcm_frames_f32(
               fl->handle, (rflac_uint64)frames, out);
         break;
      }
#endif
      case AUDIO_TYPE_WAV:
      case AUDIO_TYPE_VORBIS:
      case AUDIO_TYPE_MP3:
      case AUDIO_TYPE_NONE:
      default:
         return AUDIO_PROCESS_ERROR;
   }

   if (frames_out)
      *frames_out = produced;
   return (produced == 0) ? AUDIO_PROCESS_END : AUDIO_PROCESS_NEXT;
}

bool audio_transfer_seek(void *data, enum audio_type_enum type,
      uint64_t frame)
{
   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         if (!fl || !fl->handle)
            return false;
         return rflac_seek_to_pcm_frame(fl->handle,
               (rflac_uint64)frame) != 0;
      }
#endif
      case AUDIO_TYPE_WAV:
      case AUDIO_TYPE_VORBIS:
      case AUDIO_TYPE_MP3:
      case AUDIO_TYPE_NONE:
      default:
         break;
   }
   return false;
}

void audio_transfer_free(void *data, enum audio_type_enum type)
{
   if (!data)
      return;

   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         if (fl->handle)
            rflac_close(fl->handle);
         break;
      }
#endif
      case AUDIO_TYPE_WAV:
      case AUDIO_TYPE_VORBIS:
      case AUDIO_TYPE_MP3:
      case AUDIO_TYPE_NONE:
      default:
         break;
   }

   free(data);
}
