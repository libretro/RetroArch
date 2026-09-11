/* Copyright  (C) 2010-2025 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (audio_bitstream.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>

#include <formats/iec61937.h>
#ifdef HAVE_RDTS
#include <formats/rdts.h>
#endif
#ifdef HAVE_RAC3
#include <formats/rac3.h>
#endif

#include "audio_bitstream.h"

/* Bursts held before a submit is refused. Four is a tenth of a second
 * at AC-3's 1536 frames and 48 kHz, which is enough for a source that
 * reads ahead in bursts and little enough that the device rather than
 * this queue is what paces the stream. */
#define AUDIO_BITSTREAM_QUEUE 4

/* The largest burst any carried format takes: E-AC-3's, at four times
 * the audio rate. */
#define AUDIO_BITSTREAM_BURST_MAX IEC61937_EAC3_BURST_BYTES

struct audio_bitstream
{
   enum audio_bitstream_kind kind;
   unsigned rate;
   unsigned burst_frames;         /* PCM frames one burst stands for */
   size_t   burst_bytes;
   /* The queue holds wrapped bursts, not frames: the wrapping is the
    * submitting thread's work, so a read - which runs where the
    * device is written - copies and nothing else. */
   uint8_t *queue;                /* QUEUE * burst_bytes */
   unsigned head, count;
   unsigned gaps;
   /* One pause burst, built once. Its own length is what it accounts
    * for; a receiver takes it as "nothing here" and keeps its lock. */
   uint8_t  pause[IEC61937_PAUSE_BURST_BYTES];
   size_t   pause_bytes;
};

size_t audio_bitstream_burst_bytes(const audio_bitstream_t *bs)
{
   return bs ? bs->burst_bytes : 0;
}

unsigned audio_bitstream_burst_frames(const audio_bitstream_t *bs)
{
   return bs ? bs->burst_frames : 0;
}

unsigned audio_bitstream_queued(const audio_bitstream_t *bs)
{
   return bs ? bs->count : 0;
}

unsigned audio_bitstream_capacity(const audio_bitstream_t *bs)
{
   return bs ? AUDIO_BITSTREAM_QUEUE : 0;
}

unsigned audio_bitstream_gaps(const audio_bitstream_t *bs)
{
   return bs ? bs->gaps : 0;
}

bool audio_bitstream_writable(const audio_bitstream_t *bs)
{
   return bs && bs->count < AUDIO_BITSTREAM_QUEUE;
}

audio_bitstream_t *audio_bitstream_new(enum audio_bitstream_kind kind,
      unsigned rate)
{
   audio_bitstream_t *bs;
   unsigned frames;
   size_t   bytes;

   if (!rate)
      return NULL;

   switch (kind)
   {
#ifdef HAVE_RAC3
      case AUDIO_BITSTREAM_AC3:
         frames = 1536;
         bytes  = IEC61937_AC3_BURST_BYTES;
         break;
      case AUDIO_BITSTREAM_EAC3:
         /* Four 1536-frame periods: the burst is sent at four times
          * the audio rate, so it stands for 1536 frames of audio
          * while occupying four periods of device time. */
         frames = 1536 * 4;
         bytes  = IEC61937_EAC3_BURST_BYTES;
         break;
#endif
#ifdef HAVE_RDTS
      case AUDIO_BITSTREAM_DTS:
         /* Settled by the first frame: a DTS frame is 512, 1024 or
          * 2048 PCM frames and a stream does not change which. */
         frames = 0;
         bytes  = 0;
         break;
#endif
      default:
         return NULL;
   }

   if (!(bs = (audio_bitstream_t*)calloc(1, sizeof(*bs))))
      return NULL;

   bs->kind         = kind;
   bs->rate         = rate;
   bs->burst_frames = frames;
   bs->burst_bytes  = bytes;
   bs->pause_bytes  = iec61937_pause_burst(0, bs->pause, sizeof(bs->pause));

   if (bytes)
   {
      if (!(bs->queue = (uint8_t*)calloc(AUDIO_BITSTREAM_QUEUE, bytes)))
      {
         free(bs);
         return NULL;
      }
   }
   return bs;
}

void audio_bitstream_free(audio_bitstream_t *bs)
{
   if (!bs)
      return;
   free(bs->queue);
   free(bs);
}

/* The slot a submit writes into, once the burst size is known. */
static uint8_t *audio_bitstream_slot(audio_bitstream_t *bs)
{
   unsigned at = (bs->head + bs->count) % AUDIO_BITSTREAM_QUEUE;
   return bs->queue + (size_t)at * bs->burst_bytes;
}

bool audio_bitstream_submit(audio_bitstream_t *bs,
      const uint8_t *frame, size_t len)
{
   if (!bs || !frame || !len)
      return false;

   switch (bs->kind)
   {
#ifdef HAVE_RDTS
      case AUDIO_BITSTREAM_DTS:
      {
         rdts_frame_info_t info;
         unsigned type = 0, pcm = 0;
         uint8_t  core[IEC61937_AC3_BURST_BYTES * 2];
         size_t   n;

         if (rdts_parse_frame_info(frame, len, &info) != RDTS_OK)
            return false;
         if (!rdts_burst_type(&info, &type, &pcm))
            return false;
         if (info.core_bytes > sizeof(core))
            return false;

         /* The first frame settles the burst, and the queue with it. */
         if (!bs->burst_bytes)
         {
            bs->burst_frames = pcm;
            bs->burst_bytes  = (size_t)pcm * 4;
            if (!(bs->queue = (uint8_t*)calloc(AUDIO_BITSTREAM_QUEUE,
                        bs->burst_bytes)))
            {
               bs->burst_bytes = 0;
               return false;
            }
         }
         else if (pcm != bs->burst_frames)
            return false;   /* the stream changed its frame size */

         if (bs->count >= AUDIO_BITSTREAM_QUEUE)
            return false;

         /* Whatever packing it arrived in, a receiver takes the plain
          * one. */
         n = rdts_to_core(&info, frame, len, core, sizeof(core));
         if (!n)
            return false;
         if (!iec61937_wrap_dts(core, n, bs->burst_frames,
                  audio_bitstream_slot(bs), bs->burst_bytes))
            return false;
         bs->count++;
         return true;
      }
#endif
#ifdef HAVE_RAC3
      case AUDIO_BITSTREAM_AC3:
      {
         rac3_frame_info_t info;
         if (bs->count >= AUDIO_BITSTREAM_QUEUE)
            return false;
         if (rac3_parse_frame_info(frame, len, &info) != RAC3_OK)
            return false;
         if (info.kind != RAC3_KIND_AC3 || info.frame_bytes > len)
            return false;
         if (!iec61937_wrap_ac3(frame, info.frame_bytes, info.bsmod,
                  audio_bitstream_slot(bs), bs->burst_bytes))
            return false;
         bs->count++;
         return true;
      }
      case AUDIO_BITSTREAM_EAC3:
      {
         if (bs->count >= AUDIO_BITSTREAM_QUEUE)
            return false;
         /* The caller hands over one access unit: an independent
          * frame and whatever depends on it, which is what the burst
          * carries. */
         if (!iec61937_wrap_eac3(frame, len,
                  audio_bitstream_slot(bs), bs->burst_bytes))
            return false;
         bs->count++;
         return true;
      }
#endif
      default:
         break;
   }
   return false;
}

size_t audio_bitstream_read(audio_bitstream_t *bs, uint8_t *out, size_t len,
      size_t *pcm_frames)
{
   size_t done = 0, frames = 0;

   if (pcm_frames)
      *pcm_frames = 0;
   if (!bs || !out)
      return 0;
   /* Before the first frame a DTS source has no burst size, so there
    * is nothing to pace yet and nothing to send: a receiver has not
    * been given a stream to lock to. */
   if (!bs->burst_bytes)
      return 0;

   /* What is queued, in whole bursts, as far as the room goes. */
   while (bs->count && done + bs->burst_bytes <= len)
   {
      memcpy(out + done, bs->queue + (size_t)bs->head * bs->burst_bytes,
            bs->burst_bytes);
      bs->head = (bs->head + 1) % AUDIO_BITSTREAM_QUEUE;
      bs->count--;
      frames += bs->burst_frames;
      done   += bs->burst_bytes;
   }

   /* Nothing was queued at all: one pause burst, and only one. A
    * receiver locked to this stream must not be handed silence - it
    * drops the lock and mutes for a second or more - so a gap is
    * filled with the burst the standard has for it, padded to the
    * period with zeroes the way a burst's own padding is. One,
    * because a caller that offered room for twenty has room, not
    * twenty periods of gap to fill: the device asks again on its next
    * pass, and a source that is still late gets another then. */
   if (!done && bs->burst_bytes <= len)
   {
      memcpy(out, bs->pause, bs->pause_bytes);
      memset(out + bs->pause_bytes, 0, bs->burst_bytes - bs->pause_bytes);
      /* A pause burst occupies its own length and no more; the time
       * the missing audio would have taken is not claimed. */
      frames += (unsigned)(bs->pause_bytes / 4);
      bs->gaps++;
      done   += bs->burst_bytes;
   }

   if (pcm_frames)
      *pcm_frames = frames;
   return done;
}
