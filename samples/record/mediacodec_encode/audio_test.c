/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifdef NDEBUG
#error AAC ownership acceptance requires assertions
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dynamic/dylib.h>
#include "../../../record/record_mediacodec_encoder.h"
#include "mediacodec_encode_test_types.h"
static AMediaCodec codec;
static AMediaFormat config, output_format;
static int stage, missing, lookup, libraries, codecs, formats, queues;
static int input_busy, input_owned, output_owned, output_mode, input_calls;
static int allocation_fail;
static int stops, stop_fault;
static unsigned expected_rate, expected_channels;
static size_t capacity = 8, queued_bytes;
static uint64_t queued_pts;
static uint32_t queued_flags;
static uint8_t input_data[20000], output_data[8];
static int16_t large_pcm[10000];
static const int16_t pcm[8] = {1, 2, 3, 4, 5, 6, 7, 8};
static void *test_allocate(size_t count, size_t bytes)
{
   return allocation_fail ? NULL : calloc(count, bytes);
}
int mediacodec_inventory_select_encoder(JNIEnv *env,
      const mediacodec_encoder_request_t *request, char *name, size_t size)
{
   (void)env;
   (void)request;
   (void)name;
   (void)size;
   assert(0);
   return 0;
}
static AMediaCodec *make(const char *mime)
{
   assert(!strcmp(mime, "audio/mp4a-latm"));
   if (stage == 2)
      return NULL;
   codecs++;
   return &codec;
}
static media_status_t configure(AMediaCodec *c, const AMediaFormat *f,
      ANativeWindow *window, AMediaCrypto *crypto, uint32_t flags)
{
   assert(c == &codec && f == &config && !window && !crypto && flags == 1);
   return stage == 4 ? -99 : 0;
}
static media_status_t start(AMediaCodec *c)
{
   assert(c == &codec);
   return stage == 5 ? -99 : 0;
}
static media_status_t stop(AMediaCodec *c)
{
   assert(c == &codec);
   stops++;
   return stop_fault ? -99 : 0;
}
static media_status_t destroy(AMediaCodec *c)
{
   assert(c == &codec);
   codecs--;
   input_owned = output_owned = 0;
   return 0;
}
static AMediaFormat *format_new(void)
{
   if (stage == 3)
      return NULL;
   formats++;
   return &config;
}
static media_status_t format_free(AMediaFormat *f)
{
   assert(f == &config || f == &output_format);
   formats--;
   return 0;
}
static void set_string(AMediaFormat *f, const char *key, const char *value)
{
   assert(f == &config && !strcmp(key, "mime")
         && !strcmp(value, "audio/mp4a-latm"));
}
static void set_integer(AMediaFormat *f, const char *key, int32_t value)
{
   assert(f == &config);
   if (!strcmp(key, "sample-rate"))
      assert(value == (int32_t)expected_rate);
   else if (!strcmp(key, "channel-count"))
      assert(value == (int32_t)expected_channels);
   else if (!strcmp(key, "bitrate"))
      assert(value == 128000);
   else if (!strcmp(key, "aac-profile") || !strcmp(key, "pcm-encoding"))
      assert(value == 2);
   else
   {
      assert(!strcmp(key, "max-input-size"));
      assert(value == 16384);
   }
}
static bool get_integer(AMediaFormat *f, const char *key, int32_t *value)
{
   assert(f == &output_format);
   if (!strcmp(key, "sample-rate"))
      *value = (int32_t)expected_rate + (stage == 7);
   else
   {
      assert(!strcmp(key, "channel-count"));
      *value = (int32_t)expected_channels + (stage == 8);
   }
   return true;
}
static ssize_t input(AMediaCodec *c, int64_t timeout)
{
   assert(c == &codec && !timeout && !input_owned);
   input_calls++;
   if (stage == 14)
      return -99;
   if (input_busy)
      return -1;
   input_owned = 1;
   return 2;
}
static uint8_t *input_buffer(AMediaCodec *c, size_t index, size_t *size)
{
   assert(c == &codec && index == 2 && input_owned);
   *size = stage == 10 ? 1 : capacity;
   return stage == 9 ? NULL : input_data;
}
#if defined(__USE_FILE_OFFSET64) && !defined(__LP64__)
typedef int32_t test_offset_t;
#else
typedef off_t test_offset_t;
#endif
static media_status_t queue(AMediaCodec *c, size_t index, test_offset_t offset,
      size_t size, uint64_t pts, uint32_t flags)
{
   assert(c == &codec && index == 2 && input_owned && !offset
         && size <= capacity);
   assert(flags == 0 || (flags == 4 && !size));
   if (stage == 11)
      return -99;
   queues++;
   queued_bytes = size;
   queued_pts = pts;
   queued_flags = flags;
   input_owned = 0;
   return 0;
}
static ssize_t output(
      AMediaCodec *c, AMediaCodecBufferInfo *info, int64_t timeout)
{
   assert(c == &codec && !timeout && !output_owned);
   if (!output_mode)
      return -1;
   if (output_mode == 1)
      return -2;
   output_owned = 1;
   info->offset = 1;
   info->size = output_mode == 3 ? 0 : 3;
   info->presentationTimeUs = 0;
   info->flags = output_mode == 3 ? 4 : 0;
   return 3;
}
static uint8_t *output_buffer(AMediaCodec *c, size_t index, size_t *size)
{
   assert(c == &codec && index == 3 && output_owned);
   *size = sizeof(output_data);
   return stage == 12 ? NULL : output_data;
}
static media_status_t release(AMediaCodec *c, size_t index, bool render)
{
   assert(c == &codec && index == 3 && output_owned && !render);
   if (stage == 13)
      return -99;
   output_owned = 0;
   return 0;
}
static AMediaFormat *get_format(AMediaCodec *c)
{
   assert(c == &codec);
   if (stage == 6)
      return NULL;
   formats++;
   return &output_format;
}
static unsigned char asc[2] = {0x12, 0x10};
static int config_fault;
static bool get_buffer(AMediaFormat *f, const char *key,
      void **data, size_t *bytes)
{
   assert(f == &output_format && !strcmp(key, "csd-0"));
   *data = config_fault == 2 ? NULL : asc;
   *bytes = config_fault == 3 ? 0 : sizeof(asc);
   return config_fault != 1;
}
dylib_t dylib_load(const char *name)
{
   assert(!strcmp(name, "libmediandk.so"));
   lookup = 0;
   if (stage == 1)
      return NULL;
   libraries++;
   return (void *)1;
}
void dylib_close(dylib_t lib)
{
   assert(lib && !codecs && !formats);
   libraries--;
}
function_t dylib_proc(dylib_t lib, const char *name)
{
   assert(lib);
   if (++lookup == missing)
      return NULL;
#define FN(text, fn)                                                           \
   if (!strcmp(name, text))                                                    \
   return (function_t)fn
   FN("AMediaCodec_createEncoderByType", make);
   FN("AMediaCodec_configure", configure);
   FN("AMediaCodec_start", start);
   FN("AMediaCodec_stop", stop);
   FN("AMediaCodec_delete", destroy);
   FN("AMediaCodec_dequeueOutputBuffer", output);
   FN("AMediaCodec_getOutputBuffer", output_buffer);
   FN("AMediaCodec_releaseOutputBuffer", release);
   FN("AMediaCodec_getOutputFormat", get_format);
   FN("AMediaFormat_new", format_new);
   FN("AMediaFormat_delete", format_free);
   FN("AMediaFormat_setString", set_string);
   FN("AMediaFormat_setInt32", set_integer);
   FN("AMediaFormat_getInt32", get_integer);
   FN("AMediaFormat_getBuffer", get_buffer);
   FN("AMediaCodec_dequeueInputBuffer", input);
   FN("AMediaCodec_getInputBuffer", input_buffer);
   FN("AMediaCodec_queueInputBuffer", queue);
#undef FN
   assert(0);
   return NULL;
}
#define calloc test_allocate
#include "../../../record/record_mediacodec_encoder.c"
#undef calloc
static void balanced(void)
{
   assert(!libraries && !codecs && !formats && !input_owned && !output_owned);
}
static record_mediacodec_encoder_t *open_audio(unsigned rate, unsigned channels)
{
   expected_rate = rate;
   expected_channels = channels;
   return record_mediacodec_encoder_audio_new(rate, channels, 128000);
}
int main(void)
{
   record_mediacodec_encoder_t *s;
   record_mediacodec_packet_t packet;
   size_t accepted, config_bytes;
   const void *config_data;
   int i, before;
   for (missing = 1; missing <= 18; missing++)
   {
      assert(!open_audio(44100, 2));
      balanced();
   }
   missing = 0;
   for (stage = 1; stage <= 5; stage++)
   {
      assert(!open_audio(44100, 2));
      balanced();
   }
   stage = 0;
   allocation_fail = 1;
   assert(!open_audio(44100, 2));
   balanced();
   allocation_fail = 0;
   assert(!open_audio(44100, 6) && !open_audio(0, 2));
   balanced();
   s = open_audio(44100, 2);
   assert(s && !record_mediacodec_encoder_surface(s));
   assert(!record_mediacodec_encoder_finish(s));
   input_busy = 1;
   before = queues;
   assert(!record_mediacodec_encoder_audio_submit(s, pcm, 4, 100, &accepted)
         && !accepted && queues == before);
   input_busy = 0;
   assert(record_mediacodec_encoder_audio_submit(s, pcm, 4, 100, &accepted) == 1
         && accepted == 2);
   assert(queued_pts == 2267 && queued_bytes == 8
         && !memcmp(input_data, pcm, 8));
   assert(record_mediacodec_encoder_audio_submit(s, pcm + 4, 2, 102, &accepted)
               == 1
         && accepted == 2);
   assert(queued_pts == 2312 && !memcmp(input_data, pcm + 4, 8));
   for (i = 0; i < 2; i++)
   {
      memset(input_data, 0xa5, sizeof(input_data));
      assert(record_mediacodec_encoder_audio_submit(s, pcm, 2, 108, &accepted)
                  == 1
            && !accepted);
      assert(queued_pts == (i ? 2403u : 2358u) && queued_bytes == 8
            && !input_data[0] && !input_data[7] && input_data[8] == 0xa5);
   }
   assert(record_mediacodec_encoder_audio_submit(s, pcm, 2, 108, &accepted) == 1
         && accepted == 2 && queued_pts == 2448);
   config_data = asc;
   config_bytes = 99;
   assert(!record_mediacodec_encoder_audio_config(s, &config_data,
            &config_bytes) && !config_data && !config_bytes);
   output_mode = 1;
   assert(record_mediacodec_encoder_pump(s, &packet) == RECORD_MC_FORMAT);
   assert(record_mediacodec_encoder_audio_config(s, &config_data,
            &config_bytes) && config_data == asc && config_bytes == 2);
   for (config_fault = 1; config_fault <= 3; config_fault++)
      assert(!record_mediacodec_encoder_audio_config(s, &config_data,
               &config_bytes) && !config_data && !config_bytes);
   config_fault = 0;
   assert(!record_mediacodec_encoder_audio_config(s, NULL, &config_bytes));
   assert(!record_mediacodec_encoder_audio_config(s, &config_data, NULL));
   output_mode = 2;
   assert(record_mediacodec_encoder_pump(s, &packet) == RECORD_MC_PACKET
         && packet.size == 3);
   assert(record_mediacodec_encoder_release(s));
   assert(record_mediacodec_encoder_audio_finish(s, 114) == 2
         && queued_pts == 2494);
   assert(record_mediacodec_encoder_audio_finish(s, 114) == 2
         && queued_pts == 2539);
   assert(record_mediacodec_encoder_audio_finish(s, 114) == 1
         && queued_pts == 2585 && !queued_bytes && queued_flags == 4);
   before = queues;
   assert(record_mediacodec_encoder_audio_finish(s, 114) == 1
         && queues == before);
   output_mode = 3;
   assert(record_mediacodec_encoder_pump(s, &packet) == RECORD_MC_END);
   record_mediacodec_encoder_free(s);
   balanced();
   s = open_audio(48000, 1);
   input_busy = 1;
   assert(!record_mediacodec_encoder_audio_finish(s, 0));
   input_busy = 0;
   assert(record_mediacodec_encoder_audio_finish(s, 0) == 1);
   record_mediacodec_encoder_free(s);
   balanced();
   s = open_audio(44100, 2);
   assert(record_mediacodec_encoder_audio_finish(s, 4) == 2);
   assert(record_mediacodec_encoder_audio_finish(s, 4) == 2);
   assert(
         record_mediacodec_encoder_audio_finish(s, 4) == 1 && queued_pts == 90);
   record_mediacodec_encoder_free(s);
   balanced();
   capacity = sizeof(input_data);
   s = open_audio(44100, 2);
   assert(record_mediacodec_encoder_audio_submit(
                s, large_pcm, 5000, 0, &accepted)
               == 1
         && accepted == 4096 && queued_bytes == 16384);
   assert(record_mediacodec_encoder_audio_submit(
                s, large_pcm + 8192, 904, 4096, &accepted)
               == 1
         && accepted == 904 && queued_pts == 92879);
   assert(record_mediacodec_encoder_audio_finish(s, 5000) == 1
         && queued_pts == 113378);
   record_mediacodec_encoder_free(s);
   balanced();
   capacity = 8;
   s = open_audio(44100, 2);
   for (i = 0; i < 10000; i++)
   {
      assert(record_mediacodec_encoder_audio_submit(
                   s, pcm, 1, (uint64_t)i, &accepted)
                  == 1
            && accepted == 1);
      assert(queued_pts == (uint64_t)i * 1000000 / 44100);
   }
   record_mediacodec_encoder_free(s);
   balanced();
   for (i = 6; i <= 14; i++)
   {
      s = open_audio(44100, 2);
      stage = i;
      if (i <= 8)
      {
         output_mode = 1;
         assert(record_mediacodec_encoder_pump(s, &packet) == RECORD_MC_ERROR);
      }
      else if (i == 12 || i == 13)
      {
         stage = 0;
         output_mode = 1;
         assert(record_mediacodec_encoder_pump(s, &packet) == RECORD_MC_FORMAT);
         stage = i;
         output_mode = 2;
         if (i == 12)
            assert(record_mediacodec_encoder_pump(s, &packet)
                  == RECORD_MC_ERROR);
         else
         {
            assert(record_mediacodec_encoder_pump(s, &packet)
                  == RECORD_MC_PACKET);
            assert(!record_mediacodec_encoder_release(s));
         }
      }
      else
         assert(record_mediacodec_encoder_audio_submit(s, pcm, 2, 0, &accepted)
                     == -1
               && !accepted);
      record_mediacodec_encoder_free(s);
      balanced();
      stage = 0;
   }
   s = open_audio(44100, 2);
   before = input_calls;
   assert(record_mediacodec_encoder_audio_submit(
                s, pcm, 2, ((uint64_t)-1) >> 1, &accepted)
               == -1
         && input_calls == before);
   record_mediacodec_encoder_free(s);
   balanced();
   s = open_audio(44100, 2);
   assert(record_mediacodec_encoder_audio_submit(s, pcm, 2, 0, &accepted) == 1);
   before = input_calls;
   assert(record_mediacodec_encoder_audio_submit(s, pcm, 2, 1, &accepted) == -1
         && input_calls == before);
   record_mediacodec_encoder_free(s);
   balanced();
   s = open_audio(44100, 2);
   input_busy = 1;
   assert(!record_mediacodec_encoder_audio_finish(s, 4));
   before = input_calls;
   assert(record_mediacodec_encoder_audio_finish(s, 5) == -1
         && input_calls == before);
   record_mediacodec_encoder_free(s);
   balanced();
   input_busy = 0;
   for (i = 0; i < 2; i++)
   {
      s = open_audio(44100, 2);
      assert(s);
      assert(record_mediacodec_encoder_audio_submit(s, pcm, 2, 0, &accepted)
            == 1);
      stop_fault = i;
      before = stops;
      assert(record_mediacodec_encoder_stop(s) == !stop_fault);
      assert(!record_mediacodec_encoder_audio_config(s, &config_data,
               &config_bytes) && !config_data && !config_bytes);
      assert(record_mediacodec_encoder_stop(s) == !stop_fault);
      assert(codecs == 1 && formats == 1 && libraries == 1);
      accepted = 99;
      assert(record_mediacodec_encoder_audio_submit(s, pcm, 2, 2, &accepted)
            == -1 && !accepted);
      assert(record_mediacodec_encoder_audio_finish(s, 2) == -1);
      record_mediacodec_encoder_free(s);
      assert(stops == before + 1);
      balanced();
      stop_fault = 0;
   }
   puts("PASS: worker AAC PCM ownership, partial/BUSY input, bounded silence, "
        "sample-clock timestamps, EOS, native faults and format validation; "
        "APIs mocked");
   return 0;
}
