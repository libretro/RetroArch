/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifdef NDEBUG
#error Encoder ownership acceptance requires assertions
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dynamic/dylib.h>
#include "../../../record/record_mediacodec_encoder.h"
#include "mediacodec_encode_test_types.h"
static AMediaCodec codec;
static AMediaFormat config, output_formats[2];
static ANativeWindow window;
static int stage, lookup, missing, libraries, codecs, windows, formats, names;
static int output_mode, metadata, owned, output_calls, releases, finishes;
static int stops, stop_fault;
static unsigned serial;
static const char *test_mime = "video/hevc";
static unsigned hdr_mode, format_fault, normalized_profile;
static unsigned char hdr_saved[25];
static uint8_t payload[16];
static int allocation_fail;
static void *test_allocate(size_t count, size_t bytes)
{
   return allocation_fail ? NULL : calloc(count, bytes);
}
int mediacodec_inventory_select_encoder(JNIEnv *env,
      const mediacodec_encoder_request_t *request, char *name, size_t capacity)
{
   (void)env;
   assert(request && !strcmp(request->mime, test_mime) && request->width == 320
         && request->height == 192 && request->profile == (hdr_mode ? 4096 : 2)
         && capacity >= 16 && request->level == 64 && request->range == 2
         && request->color == hdr_mode);
   strcpy(name, "c2.test.encoder");
   return stage == 3 ? 0 : 1;
}
static AMediaCodec *make(const char *name)
{
   assert(!strcmp(name, "c2.test.encoder"));
   if (stage == 4)
      return NULL;
   codecs++;
   return &codec;
}
static media_status_t configure(AMediaCodec *c, const AMediaFormat *f,
      ANativeWindow *w, AMediaCrypto *crypto, uint32_t flags)
{
   assert(c == &codec && f == &config && !w && !crypto && flags == 1);
   return stage == 8 ? -99 : 0;
}
static media_status_t create_surface(AMediaCodec *c, ANativeWindow **out)
{
   assert(c == &codec);
   if (stage == 9)
      return -99;
   *out = &window;
   windows++;
   return stage == 10 ? -99 : 0;
}
static void window_free(ANativeWindow *w)
{
   assert(w == &window && !codecs);
   windows--;
}
static media_status_t start(AMediaCodec *c)
{
   assert(c == &codec && windows == 1);
   return stage == 11 ? -99 : 0;
}
static media_status_t stop(AMediaCodec *c)
{
   assert(c == &codec && windows == 1);
   stops++;
   return stop_fault ? -99 : 0;
}
static media_status_t delete_codec(AMediaCodec *c)
{
   assert(c == &codec);
   codecs--;
   owned = 0;
   return 0;
}
static media_status_t name(AMediaCodec *c, char **out)
{
   assert(c == &codec);
   *out = stage == 6 ? "wrong" : "c2.test.encoder";
   names++;
   return stage == 5 ? -99 : 0;
}
static void name_free(AMediaCodec *c, char *n)
{
   assert(c == &codec && n);
   names--;
}
static AMediaFormat *format_new(void)
{
   if (stage == 7)
      return NULL;
   formats++;
   return &config;
}
static media_status_t format_free(AMediaFormat *f)
{
   assert(f == &config || f == &output_formats[0] || f == &output_formats[1]);
   formats--;
   return 0;
}
static void set_string(AMediaFormat *f, const char *key, const char *value)
{
   assert(f == &config && !strcmp(key, "mime") && !strcmp(value, test_mime));
}
static void set_integer(AMediaFormat *f, const char *key, int32_t value)
{
   assert(f == &config);
   if (!strcmp(key, "width"))
      assert(value == 320);
   else if (!strcmp(key, "height"))
      assert(value == 192);
   else if (!strcmp(key, "profile"))
      assert(value == (hdr_mode ? 4096 : 2));
   else if (!strcmp(key, "level"))
      assert(value == 64);
   else if (!strcmp(key, "color-standard"))
      assert(value == (hdr_mode ? 6 : 1));
   else if (!strcmp(key, "color-transfer"))
      assert(value == (hdr_mode ? 6 : 3));
   else if (!strcmp(key, "color-range"))
      assert(value == 2);
   else if (!strcmp(key, "bitrate"))
      assert(value == 1000000);
   else if (!strcmp(key, "color-format"))
      assert(value == 0x7f000789);
   else
   {
      assert(!strcmp(key, "i-frame-interval"));
      assert(value == 2);
   }
}
static void set_float(AMediaFormat *f, const char *key, float value)
{
   assert(f == &config && !strcmp(key, "frame-rate") && value > 29.96f
         && value < 29.98f);
}
static void set_buffer(
      AMediaFormat *f, const char *key, const void *data, size_t size)
{
   assert(f == &config && !strcmp(key, "hdr-static-info") && hdr_mode && data
         && size == 25 && !((const unsigned char *)data)[0]);
   memcpy(hdr_saved, data, 25);
}
static bool get_integer(AMediaFormat *f, const char *key, int32_t *value)
{
   static const char *const keys[] = {"width", "height", "profile",
         "color-standard", "color-transfer", "color-range"};
   int32_t values[6];
   unsigned i;
   assert(f == &output_formats[0] || f == &output_formats[1]);
   values[0] = 320;
   values[1] = 192;
   values[2] = hdr_mode
         ? (normalized_profile
                       ? (!strcmp(test_mime, "video/x-vnd.on2.vp9") ? 4 : 2)
                       : 4096)
         : 2;
   values[3] = hdr_mode ? 6 : 1;
   values[4] = hdr_mode ? 6 : 3;
   values[5] = 2;
   for (i = 0; i < 6; i++)
      if (!strcmp(key, keys[i]))
      {
         if (format_fault == i * 2 + 1)
            return false;
         *value = values[i] + (format_fault == i * 2 + 2 ? 1 : 0);
         return true;
      }
   assert(0);
   return false;
}
static bool get_buffer(
      AMediaFormat *f, const char *key, void **data, size_t *size)
{
   assert((f == &output_formats[0] || f == &output_formats[1]) && hdr_mode
         && !strcmp(key, "hdr-static-info"));
   *data = format_fault == 15 ? NULL : hdr_saved;
   *size = format_fault == 14 ? 24 : 25;
   return format_fault != 13;
}
static ssize_t output(
      AMediaCodec *c, AMediaCodecBufferInfo *info, int64_t timeout)
{
   assert(c == &codec && !timeout && !owned);
   output_calls++;
   if (output_mode <= 0)
      return output_mode == -1  ? -2
            : output_mode == -2 ? -3
            : output_mode == -3 ? -99
                                : -1;
   owned = 1;
   info->offset = metadata == 1 ? -1 : metadata == 2 ? 17 : 3;
   info->size = metadata == 3 ? -1 : metadata == 4 ? 14 : 5;
   if (output_mode == 2 || output_mode == 5)
      info->size = 0;
   info->presentationTimeUs = metadata == 5 ? -1 : 33367;
   info->flags = metadata == 6                  ? 8
         : output_mode == 2 || output_mode == 3 ? 4
         : output_mode == 4                     ? 2
                                                : 1;
   return 3;
}
static uint8_t *buffer(AMediaCodec *c, size_t index, size_t *capacity)
{
   assert(c == &codec && index == 3 && owned);
   *capacity = sizeof(payload);
   return stage == 15 ? NULL : payload;
}
static media_status_t release(AMediaCodec *c, size_t index, bool render)
{
   assert(c == &codec && index == 3 && owned && !render);
   releases++;
   if (stage == 13)
      return -99;
   owned = 0;
   return 0;
}
static media_status_t finish(AMediaCodec *c)
{
   assert(c == &codec);
   finishes++;
   return stage == 14 ? -99 : 0;
}
static AMediaFormat *get_format(AMediaCodec *c)
{
   assert(c == &codec);
   if (stage == 12)
      return NULL;
   formats++;
   return &output_formats[serial++ & 1];
}
dylib_t dylib_load(const char *path)
{
   if (!strcmp(path, "libmediandk.so"))
   {
      lookup = 0;
      if (stage == 1)
         return NULL;
      libraries++;
      return (void *)1;
   }
   assert(!strcmp(path, "libandroid.so"));
   if (stage == 2)
      return NULL;
   libraries++;
   return (void *)2;
}
void dylib_close(dylib_t library)
{
   assert(library && !codecs && !windows && !formats && !names);
   libraries--;
}
function_t dylib_proc(dylib_t library, const char *symbol)
{
   assert(library);
   if (++lookup == missing)
      return NULL;
#define FN(text, function)                                                     \
   if (!strcmp(symbol, text))                                                  \
   return (function_t)function
   FN("AMediaCodec_createCodecByName", make);
   FN("AMediaCodec_configure", configure);
   FN("AMediaCodec_start", start);
   FN("AMediaCodec_stop", stop);
   FN("AMediaCodec_delete", delete_codec);
   FN("AMediaCodec_signalEndOfInputStream", finish);
   FN("AMediaCodec_createInputSurface", create_surface);
   FN("AMediaCodec_getName", name);
   FN("AMediaCodec_releaseName", name_free);
   FN("AMediaCodec_dequeueOutputBuffer", output);
   FN("AMediaCodec_getOutputBuffer", buffer);
   FN("AMediaCodec_releaseOutputBuffer", release);
   FN("AMediaCodec_getOutputFormat", get_format);
   FN("AMediaFormat_new", format_new);
   FN("AMediaFormat_delete", format_free);
   FN("AMediaFormat_setString", set_string);
   FN("AMediaFormat_setInt32", set_integer);
   FN("AMediaFormat_setFloat", set_float);
   FN("AMediaFormat_setBuffer", set_buffer);
   FN("AMediaFormat_getInt32", get_integer);
   FN("AMediaFormat_getBuffer", get_buffer);
   FN("ANativeWindow_release", window_free);
#undef FN
   assert(0);
   return NULL;
}
#define calloc test_allocate
#include "../../../record/record_mediacodec_encoder.c"
#undef calloc
static void balanced(void)
{
   assert(!libraries && !codecs && !windows && !formats && !names && !owned);
}
static record_mediacodec_encoder_t *open_encoder(void)
{
   mediacodec_encoder_request_t request;
   memset(&request, 0, sizeof(request));
   request.mime = test_mime;
   request.width = 320;
   request.height = 192;
   request.profile = hdr_mode ? 4096 : 2;
   request.level = 64;
   request.range = 2;
   request.color = hdr_mode;
   if (hdr_mode)
   {
      request.hdr_static_size = 25;
      request.hdr_static[17] = 0xe8;
      request.hdr_static[18] = 3;
      request.hdr_static[19] = 1;
      request.hdr_static[21] = 0xe8;
      request.hdr_static[22] = 3;
      request.hdr_static[23] = 0x90;
      request.hdr_static[24] = 1;
   }
   request.bitrate = 1000000;
   request.frame_rate = 30000.0 / 1001.0;
   return record_mediacodec_encoder_new(NULL, &request, 2);
}
static void format_ready(record_mediacodec_encoder_t *s)
{
   record_mediacodec_packet_t packet;
   output_mode = -1;
   assert(record_mediacodec_encoder_pump(s, &packet) == RECORD_MC_FORMAT);
   assert(record_mediacodec_encoder_format(s) && !packet.data && formats == 2);
}
int main(void)
{
   record_mediacodec_encoder_t *s;
   record_mediacodec_packet_t packet, again;
   int i, before;
   for (missing = 1; missing <= 22; missing++)
   {
      assert(!open_encoder());
      balanced();
   }
   missing = 0;
   for (stage = 1; stage <= 11; stage++)
   {
      assert(!open_encoder());
      balanced();
   }
   stage = 0;
   allocation_fail = 1;
   assert(!open_encoder());
   balanced();
   allocation_fail = 0;
   s = open_encoder();
   assert(s && record_mediacodec_encoder_surface(s) == &window);
   output_mode = 0;
   assert(record_mediacodec_encoder_pump(s, &packet) == RECORD_MC_BUSY);
   format_ready(s);
   format_ready(s);
   output_mode = -2;
   assert(record_mediacodec_encoder_pump(s, &packet) == RECORD_MC_PROGRESS);
   output_mode = 5;
   assert(record_mediacodec_encoder_pump(s, &packet) == RECORD_MC_PROGRESS
         && !owned && !packet.data);
   output_mode = 1;
   assert(record_mediacodec_encoder_pump(s, &packet) == RECORD_MC_PACKET);
   assert(packet.data == payload + 3 && packet.size == 5
         && packet.pts_us == 33367);
   before = output_calls;
   for (i = 0; i < 100; i++)
   {
      assert(record_mediacodec_encoder_pump(s, &again) == RECORD_MC_PACKET);
      assert(again.data == packet.data && again.size == packet.size);
   }
   assert(output_calls == before && owned);
   assert(record_mediacodec_encoder_release(s) && !owned);
   assert(!record_mediacodec_encoder_release(s));
   before = finishes;
   assert(record_mediacodec_encoder_finish(s));
   assert(record_mediacodec_encoder_finish(s) && finishes == before + 1);
   assert(!record_mediacodec_encoder_surface(s));
   output_mode = 3;
   assert(record_mediacodec_encoder_pump(s, &packet) == RECORD_MC_PACKET);
   assert(packet.size && packet.flags == 4);
   assert(record_mediacodec_encoder_release(s));
   assert(record_mediacodec_encoder_pump(s, &packet) == RECORD_MC_END);
   record_mediacodec_encoder_free(s);
   balanced();
   s = open_encoder();
   format_ready(s);
   assert(record_mediacodec_encoder_finish(s));
   output_mode = 2;
   assert(
         record_mediacodec_encoder_pump(s, &packet) == RECORD_MC_END && !owned);
   record_mediacodec_encoder_free(s);
   balanced();
   for (metadata = 1; metadata <= 6; metadata++)
   {
      s = open_encoder();
      format_ready(s);
      output_mode = 1;
      assert(record_mediacodec_encoder_pump(s, &packet) == RECORD_MC_ERROR);
      assert(!packet.data && !record_mediacodec_encoder_surface(s));
      assert(record_mediacodec_encoder_pump(s, &packet) == RECORD_MC_ERROR);
      record_mediacodec_encoder_free(s);
      balanced();
   }
   metadata = 0;
   for (i = 12; i <= 15; i++)
   {
      s = open_encoder();
      if (i != 12)
         format_ready(s);
      stage = i;
      if (i == 12)
      {
         output_mode = -1;
         assert(record_mediacodec_encoder_pump(s, &packet) == RECORD_MC_ERROR);
      }
      else if (i == 14)
         assert(!record_mediacodec_encoder_finish(s));
      else
      {
         output_mode = 1;
         if (i == 13)
         {
            assert(record_mediacodec_encoder_pump(s, &packet)
                  == RECORD_MC_PACKET);
            assert(!record_mediacodec_encoder_release(s));
         }
         else
            assert(record_mediacodec_encoder_pump(s, &packet)
                  == RECORD_MC_ERROR);
      }
      record_mediacodec_encoder_free(s);
      balanced();
      stage = 0;
   }
   s = open_encoder();
   output_mode = 1;
   assert(record_mediacodec_encoder_pump(s, &packet) == RECORD_MC_ERROR);
   record_mediacodec_encoder_free(s);
   balanced();
   s = open_encoder();
   format_ready(s);
   output_mode = 3;
   assert(record_mediacodec_encoder_pump(s, &packet) == RECORD_MC_ERROR);
   record_mediacodec_encoder_free(s);
   balanced();
   s = open_encoder();
   output_mode = -3;
   assert(record_mediacodec_encoder_pump(s, &packet) == RECORD_MC_ERROR);
   record_mediacodec_encoder_free(s);
   balanced();
   s = open_encoder();
   format_ready(s);
   output_mode = 4;
   metadata = 5;
   assert(record_mediacodec_encoder_pump(s, &packet) == RECORD_MC_PACKET);
   assert(packet.flags == 2 && packet.pts_us == -1);
   record_mediacodec_encoder_free(s);
   balanced();
   metadata = 0;
   for (hdr_mode = 0; hdr_mode < 2; hdr_mode++)
   {
      for (format_fault = 1; format_fault <= (hdr_mode ? 16u : 12u);
            format_fault++)
      {
         s = open_encoder();
         assert(s);
         if (format_fault == 16)
            hdr_saved[1] ^= 1;
         output_mode = -1;
         assert(record_mediacodec_encoder_pump(s, &packet) == RECORD_MC_ERROR);
         assert(record_mediacodec_encoder_pump(s, &packet) == RECORD_MC_ERROR);
         record_mediacodec_encoder_free(s);
         balanced();
      }
   }
   hdr_mode = 1;
   format_fault = 0;
   normalized_profile = 1;
   for (i = 0; i < 3; i++)
   {
      test_mime = i == 0 ? "video/hevc"
            : i == 1     ? "video/av01"
                         : "video/x-vnd.on2.vp9";
      s = open_encoder();
      assert(s);
      format_ready(s);
      record_mediacodec_encoder_free(s);
      balanced();
   }
   test_mime = "video/hevc";
   normalized_profile = hdr_mode = 0;
   assert(!record_mediacodec_encoder_stop(NULL));
   for (i = 0; i < 4; i++)
   {
      s = open_encoder();
      assert(s);
      format_ready(s);
      output_mode = 1;
      assert(record_mediacodec_encoder_pump(s, &packet) == RECORD_MC_PACKET);
      if (i == 2)
      {
         stage = 13;
         assert(!record_mediacodec_encoder_release(s));
         stage = 0;
      }
      if (i == 3)
         assert(record_mediacodec_encoder_finish(s));
      before = stops;
      stop_fault = i == 1;
      assert(record_mediacodec_encoder_stop(s) == !stop_fault);
      assert(stops == before + 1 && codecs == 1 && windows == 1
            && libraries == 2 && formats == 2);
      assert(record_mediacodec_encoder_stop(s) == !stop_fault);
      assert(!record_mediacodec_encoder_surface(s));
      assert(!record_mediacodec_encoder_format(s));
      assert(record_mediacodec_encoder_pump(s, &packet) == RECORD_MC_ERROR
            && !packet.data && !packet.size);
      assert(!record_mediacodec_encoder_release(s));
      assert(!record_mediacodec_encoder_finish(s));
      record_mediacodec_encoder_free(s);
      assert(stops == before + 1);
      balanced();
      stop_fault = 0;
   }
   puts("PASS: Surface encoder startup faults, borrowed packets, format "
        "changes, "
        "zero-timeout output, bounds, EOS and cleanup; native APIs mocked");
   return 0;
}
