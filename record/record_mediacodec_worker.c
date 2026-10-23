/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "record_mediacodec_worker.h"
#include <stdlib.h>

struct record_mediacodec_worker
{
   uint64_t audio_end_sample;
   int64_t end_us[2];
   record_mediacodec_encoder_t *encoder[2];
   record_audio_queue_t *pcm;
   record_mux_mediacodec_t *mux;
   size_t consumed;
   unsigned channels, rate;
   bool format[2], drained[2], ended[2];
   bool active, closing, video_eos, audio_eos, pcm_end;
   bool started, finished, failed;
};

record_mediacodec_worker_t *record_mediacodec_worker_new(
      record_mediacodec_encoder_t *video, record_mediacodec_encoder_t *audio,
      record_audio_queue_t *pcm, record_mux_mediacodec_t *mux,
      unsigned channels, unsigned rate)
{
   record_mediacodec_worker_t *w;
   if (!video || !mux || (!!audio != !!pcm)
         || (audio && (!channels || channels > 2 || !rate)))
      return NULL;
   w = (record_mediacodec_worker_t *)calloc(1, sizeof(*w));
   if (!w)
      return NULL;
   w->encoder[0] = video;
   w->encoder[1] = audio;
   w->pcm = pcm;
   w->mux = mux;
   w->channels = channels;
   w->rate = rate;
   w->drained[1] = w->ended[1] = !audio;
   return w;
}

bool record_mediacodec_worker_finish(record_mediacodec_worker_t *w,
      int64_t video_end_us, uint64_t audio_end_sample)
{
   uint64_t seconds, remainder, maximum = ((uint64_t)1 << 63) - 1;
   if (!w || w->failed || video_end_us <= 0)
      return false;
   if (w->closing)
      return w->end_us[0] == video_end_us
            && w->audio_end_sample == audio_end_sample;
   if (w->encoder[1])
   {
      seconds = audio_end_sample / w->rate;
      remainder = (audio_end_sample % w->rate) * 1000000 / w->rate;
      if (!audio_end_sample || seconds > maximum / 1000000
            || seconds * 1000000 > maximum - remainder)
         return false;
      w->end_us[1] = (int64_t)(seconds * 1000000 + remainder);
   }
   w->end_us[0] = video_end_us;
   w->audio_end_sample = audio_end_sample;
   w->closing = true;
   return true;
}

static int record_mcw_output(record_mediacodec_worker_t *w, unsigned at)
{
   record_mediacodec_packet_t packet;
   int result;
   if (w->drained[at])
      return 0;
   result = record_mediacodec_encoder_pump(w->encoder[at], &packet);
   switch (result)
   {
      case RECORD_MC_BUSY:
         return 0;
      case RECORD_MC_PROGRESS:
         return 1;
      case RECORD_MC_FORMAT:
         if (w->format[at] || !record_mux_mediacodec_track(w->mux, at != 0,
                   record_mediacodec_encoder_format(w->encoder[at])))
            return -1;
         w->format[at] = true;
         return 1;
      case RECORD_MC_PACKET:
         if (!w->format[at])
            return -1;
         if (!w->started)
            return 0;
         if (!record_mux_mediacodec_write(w->mux, at != 0, &packet)
               || !record_mediacodec_encoder_release(w->encoder[at]))
            return -1;
         return 1;
      case RECORD_MC_END:
         if (!w->format[at] || !w->closing
               || !(at ? w->audio_eos : w->video_eos))
            return -1;
         w->drained[at] = true;
         return 1;
      default:
         return -1;
   }
}

static int record_mcw_input(record_mediacodec_worker_t *w)
{
   const int16_t *samples = NULL;
   size_t frames = 0, accepted = 0;
   uint64_t timestamp = 0;
   int result;
   if (!w->encoder[1] || w->audio_eos)
      return 0;
   if (!w->pcm_end)
   {
      result = record_audio_queue_peek(w->pcm, &samples, &frames, &timestamp);
      if (result == RECORD_AUDIO_BUSY)
         return 0;
      if (result == RECORD_AUDIO_END)
         w->pcm_end = true;
      else if (result == RECORD_AUDIO_OK)
      {
         if (w->consumed >= frames || frames > (size_t)-1 / w->channels
               || timestamp > (uint64_t)-1 - frames
               || (w->closing && timestamp + frames > w->audio_end_sample))
            return -1;
         result = record_mediacodec_encoder_audio_submit(w->encoder[1],
               samples + w->consumed * w->channels, frames - w->consumed,
               timestamp + w->consumed, &accepted);
         if (result < 0 || result > 1 || accepted > frames - w->consumed
               || (!result && accepted))
            return -1;
         w->consumed += accepted;
         if (w->consumed == frames)
         {
            if (!record_audio_queue_ack(w->pcm))
               return -1;
            w->consumed = 0;
         }
         return result ? 1 : 0;
      }
      else
         return -1;
   }
   if (!w->closing)
      return 0;
   result = record_mediacodec_encoder_audio_finish(
         w->encoder[1], w->audio_end_sample);
   if (result < 0 || result > 2)
      return -1;
   if (result == 1)
      w->audio_eos = true;
   return result ? 1 : 0;
}

int record_mediacodec_worker_step(record_mediacodec_worker_t *w)
{
   unsigned at;
   int result;
   bool progress = false;
   if (!w || w->failed)
      return RECORD_MC_WORKER_ERROR;
   if (w->finished)
      return RECORD_MC_WORKER_END;
   w->active = true;
   if (w->closing && !w->video_eos)
   {
      if (!record_mediacodec_encoder_finish(w->encoder[0]))
         goto failed;
      w->video_eos = true;
      progress = true;
   }
   result = record_mcw_input(w);
   if (result < 0)
      goto failed;
   progress |= result != 0;
   for (at = 0; at < 2; at++)
   {
      result = record_mcw_output(w, at);
      if (result < 0)
         goto failed;
      progress |= result != 0;
   }
   if (!w->started)
   {
      result = record_mux_mediacodec_start(w->mux);
      if (result < 0)
         goto failed;
      if (result)
      {
         w->started = true;
         progress = true;
      }
   }
   for (at = 0; at < 2; at++)
      if (w->started && w->drained[at] && !w->ended[at])
      {
         if (!record_mux_mediacodec_end(w->mux, at != 0, w->end_us[at]))
            goto failed;
         w->ended[at] = true;
         progress = true;
      }
   if (w->ended[0] && w->ended[1])
   {
      result = record_mux_mediacodec_finish(w->mux);
      if (result < 0)
         goto failed;
      if (result)
      {
         w->finished = true;
         return RECORD_MC_WORKER_END;
      }
   }
   return progress ? RECORD_MC_WORKER_PROGRESS : RECORD_MC_WORKER_BUSY;
failed:
   w->failed = true;
   return RECORD_MC_WORKER_ERROR;
}

void record_mediacodec_worker_abort(record_mediacodec_worker_t *w)
{
   if (w)
      w->failed = true;
}

bool record_mediacodec_worker_free(record_mediacodec_worker_t *w)
{
   if (w && w->active && !w->failed && !w->finished)
      return false;
   free(w);
   return true;
}
