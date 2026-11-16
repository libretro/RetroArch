/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <retro_spsc.h>
static unsigned copies;
static size_t counted_read(retro_spsc_t *queue, void *output, size_t bytes)
{
   copies++;
   return retro_spsc_read(queue, output, bytes);
}
#define retro_spsc_read counted_read
#include "../../../record/record_audio_queue.c"
#undef retro_spsc_read
#ifdef RECORD_AUDIO_LEND_NEGATIVE
#define record_audio_queue_peek_lend record_audio_queue_peek
#endif

static void contiguous(void)
{
   record_audio_queue_t *queue = record_audio_queue_new(2, 4, 96);
   int16_t source[8] = {1, 2, 3, 4, 5, 6, 7, 8};
   const int16_t *loan, *again;
   uint64_t timestamp;
   size_t frames;
   unsigned i, pushed = 1;
   int result;
   assert(queue);
   assert(record_audio_queue_push(queue, source, 4, 0) == RECORD_AUDIO_OK);
   copies = 0;
   assert(record_audio_queue_peek_lend(queue, &loan, &frames, &timestamp) == RECORD_AUDIO_OK);
   assert(!copies && frames == 4 && !timestamp);
   assert(loan != queue->staging && loan != source);
   memset(source, 0, sizeof(source));
   while ((result = record_audio_queue_push(queue, source, 4, pushed * 4)) == RECORD_AUDIO_OK)
   {
      pushed++;
      assert(pushed < 8);
   }
   assert(result == RECORD_AUDIO_BUSY && pushed > 1 && !record_audio_queue_free(queue));
   assert(record_audio_queue_peek(queue, &again, &frames, &timestamp) == RECORD_AUDIO_OK);
   assert(again == loan && !copies);
   for (i = 0; i < 8; i++) assert(loan[i] == (int16_t)(i + 1));
   assert(record_audio_queue_finish(queue) && record_audio_queue_ack(queue));
   for (i = 1; i < pushed; i++)
   {
      unsigned sample;
      assert(record_audio_queue_peek_lend(queue, &loan, &frames, &timestamp) == RECORD_AUDIO_OK);
      assert(frames == 4 && timestamp == i * 4);
      for (sample = 0; sample < 8; sample++) assert(!loan[sample]);
      assert(record_audio_queue_ack(queue));
   }
   assert(record_audio_queue_peek_lend(queue, &loan, &frames, &timestamp) == RECORD_AUDIO_END);
   assert(record_audio_queue_free(queue));
}

static void wrapped(void)
{
   record_audio_queue_t *queue = record_audio_queue_new(2, 4, 48);
   const int16_t *loan, *again;
   int16_t source[6];
   size_t frames;
   uint64_t timestamp;
   unsigned packet, sample, wrapped = 0;
   assert(queue && queue->data.capacity == 64);
   for (packet = 0; packet < 32; packet++)
   {
      size_t start = ((size_t)packet * 28 + 16) & 63;
      bool split = start + sizeof(source) > 64;
      for (sample = 0; sample < 6; sample++) source[sample] = (int16_t)(packet * 7 + sample);
      assert(record_audio_queue_push(queue, source, 3, packet * 3) == RECORD_AUDIO_OK);
      copies = 0;
      assert(record_audio_queue_peek_lend(queue, &loan, &frames, &timestamp) == RECORD_AUDIO_OK);
      assert(frames == 3 && timestamp == packet * 3);
      assert(copies == (unsigned)split);
      assert((loan == queue->staging) == split);
      assert(record_audio_queue_peek_lend(queue, &again, &frames, &timestamp) == RECORD_AUDIO_OK);
      assert(again == loan && copies == (unsigned)split);
      for (sample = 0; sample < 6; sample++) assert(loan[sample] == source[sample]);
      assert(record_audio_queue_ack(queue) && !record_audio_queue_ack(queue));
      if (split) wrapped++;
   }
   assert(wrapped && record_audio_queue_finish(queue));
   assert(record_audio_queue_free(queue));
}

int main(void)
{
   contiguous();
   wrapped();
   puts("PASS: PCM ring loans, stable BUSY ownership, wrapped staging and EOF retirement");
   return 0;
}
