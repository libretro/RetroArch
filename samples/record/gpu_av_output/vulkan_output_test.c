/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "vulkan_mock_worker.h"
#include "vulkan_mock_file.h"
#include "../../../record/record_gpu_output_vulkan.c"
#define CHECK(x)                                                                       \
   do                                                                                  \
   {                                                                                   \
      if (!(x))                                                                        \
      {                                                                                \
         fprintf(stderr, "FAIL %u: %s\n", (unsigned)__LINE__, #x);                     \
         return 1;                                                                     \
      }                                                                                \
   } while (0)
static const uint8_t headers[] = {0,    0, 0, 1, 0x67, 0x42, 0,   0x1e,
                                  0x80, 0, 0, 0, 1,    0x68, 0x80};
static void setup(record_gpu_output_vulkan_config_t *c, record_gpu_worker_vulkan_t *w)
{
   memset(c, 0, sizeof(*c));
   memset(w, 0, sizeof(*w));
   c->video.headers = headers;
   c->video.headers_size = sizeof(headers);
   c->video.max_packet_size = 65536;
   c->video.width = c->video.height = 64;
   c->video.timescale = 60;
   c->video.bit_depth = 8;
   w->frames = 4;
   w->duration = 1;
   file_delay = file_writes = 0;
   file_open_fail = file_push_fail = file_step_fail = file_finish_fail =
       file_cancel_fail = false;
}
static int drain(record_gpu_output_vulkan_t *p)
{
   unsigned i;
   int result = RECORD_VULKAN_OUTPUT_BUSY;
   for (i = 0; i < 10000; i++)
   {
      result = record_gpu_output_vulkan_step(p);
      if (result == RECORD_VULKAN_OUTPUT_END || result == RECORD_VULKAN_OUTPUT_ERROR)
         break;
   }
   return result;
}
static int wrapped_audio(void)
{
   static const unsigned char padding[32768] = {0};
   record_gpu_output_vulkan_config_t c;
   record_gpu_worker_vulkan_t w;
   record_gpu_output_vulkan_t *p;
   int16_t samples[18];
   unsigned channels, i;
   size_t offset;
   for (channels = 3; channels <= 6; channels += 3)
   {
      setup(&c, &w);
      c.audio_rate = 48000;
      c.audio_layout = channels == 3 ? 7 : 0x60f;
      c.audio_kbps = 448;
      c.audio_fifo_frames = 1536;
      p = record_gpu_output_vulkan_new(&w, &c, "unused");
      CHECK(p && p->channels == channels);
      offset = p->audio.capacity / p->frame_bytes * p->frame_bytes;
      CHECK(offset < p->audio.capacity && p->audio.capacity - offset < p->frame_bytes);
      CHECK(retro_spsc_write(&p->audio, padding, offset) == offset);
      CHECK(retro_spsc_skip(&p->audio, offset) == offset);
      for (i = 0; i < 3 * channels; i++)
         samples[i] = (int16_t)(i * 3031 - 24000);
      CHECK(record_gpu_output_vulkan_audio(p, samples, 3) == RECORD_VULKAN_OUTPUT_PROGRESS);
      CHECK(record_gpu_output_vulkan_audio_step(p) == RECORD_VULKAN_OUTPUT_PROGRESS && p->staged == 3);
      for (i = 0; i < 3 * channels; i++)
         CHECK(p->pcm[i] == samples[i] * (1.0f / 32768.0f));
      CHECK(!retro_spsc_read_avail(&p->audio));
      CHECK(record_gpu_output_vulkan_audio_finish(p) && record_gpu_output_vulkan_abort(p));
      CHECK(record_gpu_output_vulkan_free(p));
   }
   return 0;
}
int main(void)
{
   record_gpu_output_vulkan_config_t c;
   record_gpu_worker_vulkan_t w;
   record_gpu_output_vulkan_t *p;
   int16_t samples[4] = {32767, -32768, 123, -456};
   int16_t *block;
   unsigned i;
   const void *borrow;
   setup(&c, &w);
   w.busy = 1;
   p = record_gpu_output_vulkan_new(&w, &c, "unused");
   CHECK(p != NULL);
   CHECK(!record_gpu_output_vulkan_retry_needed(NULL));
   w.retry = true;
   CHECK(record_gpu_output_vulkan_retry_needed(p));
   w.busy = 0;
   CHECK(record_gpu_output_vulkan_retry_needed(p));
   w.retry = false;
   CHECK(!record_gpu_output_vulkan_retry_needed(p));
   CHECK(record_gpu_output_vulkan_step(p) == RECORD_VULKAN_OUTPUT_PROGRESS);
   CHECK(record_gpu_output_vulkan_abort(p));
   CHECK(record_gpu_output_vulkan_retry_needed(p));
   CHECK(record_gpu_output_vulkan_step(p) == RECORD_VULKAN_OUTPUT_ERROR);
   CHECK(!record_gpu_output_vulkan_retry_needed(p));
   CHECK(record_gpu_output_vulkan_free(p));
   setup(&c, &w);
   file_delay = 3;
   p = record_gpu_output_vulkan_new(&w, &c, "unused");
   CHECK(p != NULL);
   CHECK(!record_gpu_output_vulkan_free(p));
   CHECK(record_gpu_output_vulkan_step(p) == RECORD_VULKAN_OUTPUT_PROGRESS);
   CHECK(record_gpu_output_vulkan_retry_needed(p));
   w.busy = 1;
   CHECK(record_gpu_output_vulkan_retry_needed(p));
   w.busy = 0;
   borrow = p->file->data;
   CHECK(borrow != NULL && !w.calls);
   for (i = 0; i < 3; i++)
      CHECK(record_gpu_output_vulkan_step(p) == RECORD_VULKAN_OUTPUT_BUSY && !w.calls &&
            p->file->data == borrow);
   CHECK(drain(p) == RECORD_VULKAN_OUTPUT_END && w.made == 4 && w.releases == 4 &&
         file_writes == 5);
   w.retry = true;
   CHECK(!record_gpu_output_vulkan_retry_needed(p));
   CHECK(record_gpu_output_vulkan_step(p) == RECORD_VULKAN_OUTPUT_END &&
         record_gpu_output_vulkan_free(p));
   setup(&c, &w);
   w.frames = 0;
   c.audio_rate = 48000;
   c.audio_layout = 3;
   c.audio_kbps = 192;
   c.audio_fifo_frames = 1536;
   p = record_gpu_output_vulkan_new(&w, &c, "unused");
   CHECK(p != NULL);
   for (i = 0; i < 20 && record_gpu_output_vulkan_step(p) != RECORD_VULKAN_OUTPUT_BUSY;
        i++)
   {
   }
   CHECK(i < 20 && p->video_end && !p->audio_end);
   w.retry = true;
   CHECK(!record_gpu_output_vulkan_retry_needed(p));
   CHECK(record_gpu_output_vulkan_audio(p, samples, 1) ==
         RECORD_VULKAN_OUTPUT_PROGRESS);
   CHECK(record_gpu_output_vulkan_step(p) == RECORD_VULKAN_OUTPUT_PROGRESS);
   CHECK(record_gpu_output_vulkan_step(p) == RECORD_VULKAN_OUTPUT_BUSY);
   CHECK(!record_gpu_output_vulkan_retry_needed(p));
   CHECK(record_gpu_output_vulkan_audio_finish(p));
   CHECK(drain(p) == RECORD_VULKAN_OUTPUT_END && record_gpu_output_vulkan_free(p));
   setup(&c, &w);
   c.audio_rate = 44100;
   c.audio_layout = 3;
   c.audio_kbps = 192;
   c.audio_fifo_frames = 1536;
   p = record_gpu_output_vulkan_new(&w, &c, "unused");
   CHECK(p != NULL);
   block = (int16_t *)calloc(1536 * 2, sizeof(*block));
   CHECK(block != NULL);
   CHECK(record_gpu_output_vulkan_audio(p, block, 1536) ==
         RECORD_VULKAN_OUTPUT_PROGRESS);
   CHECK(record_gpu_output_vulkan_audio(p, samples, 1) == RECORD_VULKAN_OUTPUT_BUSY);
   for (i = 0; i < 50 && record_gpu_output_vulkan_audio(p, samples, 1) ==
                             RECORD_VULKAN_OUTPUT_BUSY;
        i++)
      CHECK(record_gpu_output_vulkan_step(p) != RECORD_VULKAN_OUTPUT_ERROR);
   CHECK(i < 50 && record_gpu_output_vulkan_audio_finish(p));
   CHECK(record_gpu_output_vulkan_audio(p, samples, 1) == RECORD_VULKAN_OUTPUT_ERROR);
   CHECK(drain(p) == RECORD_VULKAN_OUTPUT_END && p->audio_pts == 3072 && w.made == 4);
   CHECK(record_gpu_output_vulkan_free(p));
   free(block);
   setup(&c, &w);
   file_delay = 2;
   p = record_gpu_output_vulkan_new(&w, &c, "unused");
   CHECK(p != NULL);
   CHECK(record_gpu_output_vulkan_step(p) == RECORD_VULKAN_OUTPUT_PROGRESS);
   file_cancel_fail = true;
   CHECK(!record_gpu_output_vulkan_abort(p) && !record_gpu_output_vulkan_free(p));
   file_cancel_fail = false;
   CHECK(record_gpu_output_vulkan_abort(p));
   CHECK(drain(p) == RECORD_VULKAN_OUTPUT_ERROR && !w.calls);
   CHECK(record_gpu_output_vulkan_error(p) == RECORD_VULKAN_OUTPUT_ERROR_ABORTED &&
         record_gpu_output_vulkan_free(p));
   for (i = 0; i < 6; i++)
   {
      setup(&c, &w);
      p = record_gpu_output_vulkan_new(&w, &c, "unused");
      CHECK(p != NULL);
      if (i == 0)
         file_push_fail = true;
      if (i == 1)
         file_step_fail = true;
      if (i == 2)
         file_finish_fail = true;
      if (i == 3)
         w.fail = true;
      if (i == 4)
         w.release_fail = true;
      if (i == 5)
         w.duration = (uint64_t)1 << 32;
      CHECK(drain(p) == RECORD_VULKAN_OUTPUT_ERROR);
      CHECK(record_gpu_output_vulkan_error(p) ==
            (i < 3   ? RECORD_VULKAN_OUTPUT_ERROR_FILE
             : i < 5 ? RECORD_VULKAN_OUTPUT_ERROR_VIDEO
                     : RECORD_VULKAN_OUTPUT_ERROR_MUX));
      CHECK(i >= 3 || record_gpu_output_vulkan_file_error(p) != 0);
      CHECK(record_gpu_output_vulkan_free(p));
      if (w.held)
      {
         w.release_fail = false;
         CHECK(record_gpu_worker_vulkan_release(&w));
      }
   }
   setup(&c, &w);
   c.audio_rate = 48000;
   c.audio_kbps = 192;
   c.audio_layout = 3;
   c.audio_fifo_frames = 1536;
   p = record_gpu_output_vulkan_new(&w, &c, "unused");
   CHECK(p != NULL);
   p->audio_pts = (uint64_t)-1 - 1535;
   CHECK(record_gpu_output_vulkan_audio(p, samples, 2) ==
         RECORD_VULKAN_OUTPUT_PROGRESS);
   CHECK(record_gpu_output_vulkan_audio_finish(p) &&
         drain(p) == RECORD_VULKAN_OUTPUT_ERROR);
   CHECK(record_gpu_output_vulkan_error(p) == RECORD_VULKAN_OUTPUT_ERROR_AUDIO &&
         record_gpu_output_vulkan_free(p));
   setup(&c, &w);
   c.audio_rate = 48000;
   c.audio_kbps = 192;
   c.audio_layout = 3;
   c.audio_fifo_frames = 1536;
   c.video.max_packet_size = 768;
   p = record_gpu_output_vulkan_new(&w, &c, "unused");
   CHECK(p != NULL);
   CHECK(record_gpu_output_vulkan_audio_finish(p) &&
         record_gpu_output_vulkan_abort(p) && record_gpu_output_vulkan_free(p));
   c.video.max_packet_size = 767;
   CHECK(!record_gpu_output_vulkan_new(&w, &c, "unused"));
   setup(&c, &w);
   c.audio_rate = 12345;
   CHECK(!record_gpu_output_vulkan_new(&w, &c, "unused"));
   setup(&c, &w);
   file_open_fail = true;
   CHECK(!record_gpu_output_vulkan_new(&w, &c, "unused"));
   CHECK(wrapped_audio() == 0);
   puts("PASS: GPU/PCM output backpressure, short audio tail, error ownership and "
        "abort");
   return 0;
}
