/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "record_mediacodec_worker.h"
#include <stdlib.h>

struct record_mediacodec_worker
{
   uint64_t audio_end_sample, coded_frames;
   int64_t end_us[2];
   record_mediacodec_encoder_t *encoder[2];
   record_audio_queue_t *pcm;
   record_mux_mediacodec_t *mux;
   record_aac_probe_t *probe;
   size_t consumed, prefix_sent;
   unsigned channels, rate;
   bool format[2], drained[2], ended[2];
   bool active, closing, video_eos, audio_eos, pcm_end;
   bool started, finished, failed;
};

static bool record_mcw_samples_us(uint64_t frames, unsigned rate,
      int64_t *timestamp)
{
   uint64_t maximum = ((uint64_t)1 << 63) - 1;
   uint64_t seconds = frames / rate;
   uint64_t remainder = (frames % rate) * 1000000 / rate;
   if (seconds > maximum / 1000000
         || seconds * 1000000 > maximum - remainder)
      return false;
   *timestamp = (int64_t)(seconds * 1000000 + remainder);
   return true;
}

bool record_mediacodec_worker_calibrate(record_mediacodec_worker_t *w,
      record_aac_probe_t *probe)
{
   if (!w || !probe || !w->encoder[1] || w->probe || w->active
         || w->closing || w->failed)
      return false;
   w->probe = probe;
   return true;
}

bool record_mediacodec_worker_audio_edit(
      const record_mediacodec_worker_t *w, uint64_t *skip,
      uint64_t *frames, unsigned *roll)
{
   unsigned delay;
   if (!w || !skip || !frames || !roll || !w->finished || w->failed
         || !w->probe || !record_aac_probe_delay(w->probe, &delay))
      return false;
   *skip = RECORD_AAC_PROBE_FRAMES + (uint64_t)delay;
   *frames = w->audio_end_sample;
   *roll = delay ? (delay - 1) / 1024 + 1 : 1;
   return true;
}

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
   if (!w || w->failed || video_end_us <= 0)
      return false;
   if (w->closing)
      return w->end_us[0] == video_end_us
            && w->audio_end_sample == audio_end_sample;
   if (w->encoder[1])
   {
      if ((!audio_end_sample && !w->probe)
            || (w->probe && audio_end_sample
                  > (uint64_t)-1 - RECORD_AAC_PROBE_FRAMES)
            || !record_mcw_samples_us(audio_end_sample, w->rate,
                  &w->end_us[1]))
         return false;
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
         if (at && w->probe)
         {
            const void *config = NULL;
            size_t bytes = 0;
            if (!record_mediacodec_encoder_audio_config(w->encoder[at],
                      &config, &bytes)
                  || !record_aac_probe_config(w->probe, config, bytes))
               return -1;
         }
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
         if (at && w->probe && !(packet.flags & 2))
         {
            if (w->coded_frames > (uint64_t)-1 - 1024
                  || !record_mcw_samples_us(w->coded_frames, w->rate,
                        &packet.pts_us)
                  || !record_aac_probe_packet(w->probe, packet.data,
                        packet.size))
               return -1;
            w->coded_frames += 1024;
         }
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
   if (w->probe && w->prefix_sent < RECORD_AAC_PROBE_FRAMES)
   {
      samples = record_aac_probe_input(w->probe, w->prefix_sent, &frames);
      if (!samples || !frames
            || frames > RECORD_AAC_PROBE_FRAMES - w->prefix_sent)
         return -1;
      result = record_mediacodec_encoder_audio_submit(w->encoder[1],
            samples, frames, w->prefix_sent, &accepted);
      if (result < 0 || result > 1 || accepted > frames
            || (!result && accepted))
         return -1;
      w->prefix_sent += accepted;
      return result ? 1 : 0;
   }
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
               || (w->probe && timestamp + frames
                     > (uint64_t)-1 - RECORD_AAC_PROBE_FRAMES)
               || (w->closing && timestamp + frames > w->audio_end_sample))
            return -1;
         result = record_mediacodec_encoder_audio_submit(w->encoder[1],
               samples + w->consumed * w->channels, frames - w->consumed,
               timestamp + w->consumed
                     + (w->probe ? RECORD_AAC_PROBE_FRAMES : 0), &accepted);
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
         w->encoder[1], w->audio_end_sample
               + (w->probe ? RECORD_AAC_PROBE_FRAMES : 0));
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
   bool calibrated = true;
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
   if (w->probe)
   {
      result = record_aac_probe_step(w->probe);
      if (result < 0 || (!result && w->drained[1]))
         goto failed;
      calibrated = result == 2;
      progress |= result == 1;
      if (w->drained[1] && calibrated)
      {
         unsigned delay;
         uint64_t skip;
         if (!record_aac_probe_delay(w->probe, &delay))
            goto failed;
         skip = RECORD_AAC_PROBE_FRAMES + (uint64_t)delay;
         if ((w->audio_end_sample && (w->coded_frames < skip
                     || w->coded_frames - skip < w->audio_end_sample))
               || !record_mcw_samples_us(w->coded_frames, w->rate,
                     &w->end_us[1]))
            goto failed;
      }
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
      if (w->started && w->drained[at] && !w->ended[at]
            && (!at || calibrated))
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
