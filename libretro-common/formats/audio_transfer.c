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
#ifdef HAVE_RVORBIS
#include <formats/rvorbis.h>
#endif
#ifdef HAVE_RMP3
#include <formats/rmp3.h>
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

#ifdef HAVE_RVORBIS
struct audio_transfer_vorbis
{
   const void *data;
   size_t      size;
   rvorbis    *handle;
   int         channels; /* cached from rvorbis_get_info at start           */
};
#endif

#ifdef HAVE_RMP3
struct audio_transfer_mp3
{
   const void *data;
   size_t      size;
   rmp3        handle;   /* dr_mp3 initialises this in place (by value)      */
   int         inited;   /* handle is embedded, so track init state a flag   */
};
#endif

#if defined(HAVE_RVORBIS) || defined(HAVE_RMP3)
/* Vorbis and MP3 are float-internal codecs with no integer decode API, so the
 * s16 read decodes float and quantises at the boundary (round-half-away from
 * zero, clamped). This is a single quantisation, not an int16<->float
 * round-trip. */
static void audio_transfer_f32_to_s16(int16_t *out, const float *in, size_t samples)
{
   size_t i;
   for (i = 0; i < samples; i++)
   {
      float s = in[i] * 32768.0f;
      int   q = (int)(s + ((s >= 0.0f) ? 0.5f : -0.5f));
      if (q >  32767)
         q =  32767;
      if (q < -32768)
         q = -32768;
      out[i] = (int16_t)q;
   }
}
#endif

#ifdef HAVE_RVORBIS
static size_t audio_transfer_vorbis_read_s16(struct audio_transfer_vorbis *v,
      int16_t *out, size_t frames)
{
   float    chunk[512];
   size_t   done       = 0;
   int      ch         = v->channels;
   int      cap_frames = (ch > 0) ? (int)((sizeof(chunk) / sizeof(chunk[0])) / ch) : 0;

   if (cap_frames <= 0)
      return 0;

   while (done < frames)
   {
      int want = (int)(frames - done);
      int req  = (want < cap_frames) ? want : cap_frames;
      int got  = rvorbis_get_samples_float_interleaved(v->handle, ch, chunk, req * ch);

      if (got <= 0)
         break;

      audio_transfer_f32_to_s16(out + (done * (size_t)ch), chunk,
            (size_t)got * (size_t)ch);
      done += (size_t)got;
      if (got < req)
         break;
   }
   return done;
}
#endif

#ifdef HAVE_RMP3
static size_t audio_transfer_mp3_read_s16(struct audio_transfer_mp3 *m,
      int16_t *out, size_t frames)
{
   float    chunk[512];
   size_t   done       = 0;
   int      ch         = (int)m->handle.channels;
   int      cap_frames = (ch > 0) ? (int)((sizeof(chunk) / sizeof(chunk[0])) / ch) : 0;

   if (cap_frames <= 0)
      return 0;

   while (done < frames)
   {
      size_t want = frames - done;
      size_t req  = (want < (size_t)cap_frames) ? want : (size_t)cap_frames;
      size_t got  = (size_t)rmp3_read_f32(&m->handle, (uint64_t)req, chunk);

      if (got == 0)
         break;

      audio_transfer_f32_to_s16(out + (done * (size_t)ch), chunk,
            got * (size_t)ch);
      done += got;
      if (got < req)
         break;
   }
   return done;
}
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
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
         return calloc(1, sizeof(struct audio_transfer_vorbis));
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
         return calloc(1, sizeof(struct audio_transfer_mp3));
#endif
      case AUDIO_TYPE_WAV:
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
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         if (v)
         {
            v->data = ptr;
            v->size = len;
         }
         break;
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         if (m)
         {
            m->data = ptr;
            m->size = len;
         }
         break;
      }
#endif
      case AUDIO_TYPE_WAV:
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
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         int err = 0;
         if (!v || !v->data)
            return false;
         v->handle = rvorbis_open_memory((const unsigned char*)v->data,
               (int)v->size, &err, NULL);
         if (!v->handle)
            return false;
         v->channels = rvorbis_get_info(v->handle).channels;
         return true;
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         if (!m || !m->data)
            return false;
         m->inited = (rmp3_init_memory(&m->handle, m->data, m->size, NULL) != 0);
         return m->inited != 0;
      }
#endif
      case AUDIO_TYPE_WAV:
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
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         return (v && v->handle);
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         return (m && m->inited);
      }
#endif
      case AUDIO_TYPE_WAV:
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
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         rvorbis_info info;
         if (!v || !v->handle)
            return false;
         info = rvorbis_get_info(v->handle);
         if (channels)
            *channels     = (unsigned)info.channels;
         if (rate)
            *rate         = (unsigned)info.sample_rate;
         if (total_frames) /* streaming; length not tracked here */
            *total_frames = 0;
         return true;
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         if (!m || !m->inited)
            return false;
         if (channels)
            *channels     = (unsigned)m->handle.channels;
         if (rate)
            *rate         = (unsigned)m->handle.sampleRate;
         if (total_frames) /* streaming; length not tracked here */
            *total_frames = 0;
         return true;
      }
#endif
      case AUDIO_TYPE_WAV:
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
               fl->handle, (uint64_t)frames, out);
         break;
      }
#endif
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         if (!v || !v->handle)
            return AUDIO_PROCESS_ERROR;
         produced = audio_transfer_vorbis_read_s16(v, out, frames);
         break;
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         if (!m || !m->inited)
            return AUDIO_PROCESS_ERROR;
         produced = audio_transfer_mp3_read_s16(m, out, frames);
         break;
      }
#endif
      case AUDIO_TYPE_WAV:
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
               fl->handle, (uint64_t)frames, out);
         break;
      }
#endif
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         int got;
         if (!v || !v->handle)
            return AUDIO_PROCESS_ERROR;
         got = rvorbis_get_samples_float_interleaved(v->handle, v->channels,
               out, (int)(frames * (size_t)v->channels));
         produced = (got > 0) ? (size_t)got : 0;
         break;
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         if (!m || !m->inited)
            return AUDIO_PROCESS_ERROR;
         produced = (size_t)rmp3_read_f32(&m->handle, (uint64_t)frames, out);
         break;
      }
#endif
      case AUDIO_TYPE_WAV:
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
               (uint64_t)frame) != 0;
      }
#endif
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         if (!v || !v->handle)
            return false;
         if (frame == 0) /* loop-to-start: seek_start always succeeds */
         {
            rvorbis_seek_start(v->handle);
            return true;
         }
         return rvorbis_seek(v->handle, (unsigned int)frame) != 0;
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         if (!m || !m->inited)
            return false;
         return rmp3_seek_to_frame(&m->handle, (uint64_t)frame) != 0;
      }
#endif
      case AUDIO_TYPE_WAV:
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
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         if (v->handle)
            rvorbis_close(v->handle);
         break;
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         if (m->inited)
            rmp3_uninit(&m->handle);
         break;
      }
#endif
      case AUDIO_TYPE_WAV:
      case AUDIO_TYPE_NONE:
      default:
         break;
   }

   free(data);
}
