/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <dynamic/dylib.h>
#include "../../../gfx/common/mediacodec_session.h"
#include "mediacodec_session_test_types.h"
static AMediaCodec codec;
static AMediaFormat config_format, output_format;
static ANativeWindow window;
static int lookup, missing, libs, codecs, formats, names, stage, input_busy,
      output_mode, rendered, released, bad_crop;
static uint8_t bytes[8];
static uint64_t queued_pts;
static uint32_t queued_flags;
int mediacodec_inventory_select(JNIEnv *env, const char *mime, int encoder,
      unsigned w, unsigned h, int profile, char *name, size_t capacity)
{
   (void)env;
   assert(!strcmp(mime, "video/hevc") && !encoder && w == 320 && h == 192
         && profile == 2 && capacity >= 16);
   strcpy(name, "c2.test.hevc");
   return stage == 9 ? 0 : 1;
}
static AMediaCodec *make(const char *name)
{
   assert(!strcmp(name, "c2.test.hevc"));
   if (stage == 1)
      return NULL;
   codecs++;
   return &codec;
}
static media_status_t configure(AMediaCodec *c, const AMediaFormat *f,
      ANativeWindow *w, AMediaCrypto *crypto, uint32_t flags)
{
   assert(c == &codec && f == &config_format && w == &window && !crypto
         && !flags);
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
   return 0;
}
static media_status_t delete_codec(AMediaCodec *c)
{
   assert(c == &codec);
   codecs--;
   return 0;
}
static media_status_t name(AMediaCodec *c, char **out)
{
   assert(c == &codec);
   *out = stage == 2 ? "wrong" : "c2.test.hevc";
   names++;
   return stage == 8 ? -99 : 0;
}
static void release_name(AMediaCodec *c, char *n)
{
   assert(c == &codec && n);
   names--;
}
static ssize_t input(AMediaCodec *c, int64_t timeout)
{
   assert(c == &codec && !timeout);
   return input_busy ? -1 : 3;
}
static uint8_t *buffer(AMediaCodec *c, size_t index, size_t *capacity)
{
   assert(c == &codec && index == 3);
   *capacity = sizeof(bytes);
   return bytes;
}
static media_status_t queue(AMediaCodec *c, size_t index, off_t offset,
      size_t size, uint64_t pts, uint32_t flags)
{
   assert(c == &codec && index == 3 && !offset && size <= 8);
   queued_pts = pts;
   queued_flags = flags;
   return stage == 6 ? -99 : 0;
}
static ssize_t output(
      AMediaCodec *c, AMediaCodecBufferInfo *info, int64_t timeout)
{
   assert(c == &codec && !timeout);
   if (!output_mode)
      return -1;
   if (output_mode == 1)
      return -2;
   if (output_mode == 2)
      return -3;
   info->offset = 0;
   info->size = output_mode == 4 ? 0 : 4;
   info->presentationTimeUs = output_mode == 6 ? -1 : 12345;
   info->flags = output_mode == 7               ? 2
         : output_mode == 4 || output_mode == 5 ? 4
                                                : 0;
   if (output_mode == 8)
      info->presentationTimeUs = (int64_t)(((uint64_t)1 << 63) - 1);
   return 7;
}
static media_status_t release(AMediaCodec *c, size_t index, bool draw)
{
   assert(c == &codec && index == 7 && !draw);
   released++;
   return 0;
}
static media_status_t render(AMediaCodec *c, size_t index, int64_t ns)
{
   assert(c == &codec && index == 7 && ns == 12345000);
   rendered++;
   return stage == 7 ? -99 : 0;
}
static AMediaFormat *get_format(AMediaCodec *c)
{
   assert(c == &codec);
   formats++;
   return &output_format;
}
static AMediaFormat *format_new(void)
{
   if (stage == 3)
      return NULL;
   formats++;
   return &config_format;
}
static media_status_t format_free(AMediaFormat *f)
{
   assert(f == &config_format || f == &output_format);
   formats--;
   return 0;
}
static void set_string(AMediaFormat *f, const char *key, const char *value)
{
   assert(f == &config_format && !strcmp(key, "mime")
         && !strcmp(value, "video/hevc"));
}
static int configured_standard, configured_transfer, configured_range;

static void set_integer(AMediaFormat *f, const char *key, int32_t value)
{
   assert(f == &config_format && key && value > 0);
   if (!strcmp(key, "color-standard"))
      configured_standard = value;
   if (!strcmp(key, "color-transfer"))
      configured_transfer = value;
   if (!strcmp(key, "color-range"))
      configured_range = value;
}
static void set_buffer(
      AMediaFormat *f, const char *key, const void *data, size_t size)
{
   assert(f == &config_format && !strcmp(key, "csd-0") && data && size == 4);
}
static bool get_integer(AMediaFormat *f, const char *key, int32_t *value)
{
   assert(f == &output_format);
   if (!strcmp(key, "width"))
   {
      *value = 320;
      return true;
   }
   if (!strcmp(key, "height"))
   {
      *value = 192;
      return true;
   }
   if (!strcmp(key, "crop-right") && bad_crop)
   {
      *value = 320;
      return true;
   }
   return false;
}
dylib_t dylib_load(const char *path)
{
   assert(!strcmp(path, "libmediandk.so"));
   lookup = 0;
   libs++;
   return &codec;
}
void dylib_close(dylib_t lib)
{
   assert(lib == &codec);
   libs--;
}
function_t dylib_proc(dylib_t lib, const char *symbol)
{
   assert(lib == &codec);
   if (++lookup == missing)
      return NULL;
#define FN(s, f) \
   if (!strcmp(symbol, s)) \
   return (function_t)f
   FN("AMediaCodec_createCodecByName", make);
   FN("AMediaCodec_configure", configure);
   FN("AMediaCodec_start", start);
   FN("AMediaCodec_stop", stop);
   FN("AMediaCodec_delete", delete_codec);
   FN("AMediaCodec_getName", name);
   FN("AMediaCodec_releaseName", release_name);
   FN("AMediaCodec_dequeueInputBuffer", input);
   FN("AMediaCodec_getInputBuffer", buffer);
   FN("AMediaCodec_queueInputBuffer", queue);
   FN("AMediaCodec_dequeueOutputBuffer", output);
   FN("AMediaCodec_releaseOutputBuffer", release);
   FN("AMediaCodec_releaseOutputBufferAtTime", render);
   FN("AMediaCodec_getOutputFormat", get_format);
   FN("AMediaFormat_new", format_new);
   FN("AMediaFormat_delete", format_free);
   FN("AMediaFormat_setString", set_string);
   FN("AMediaFormat_setInt32", set_integer);
   FN("AMediaFormat_setBuffer", set_buffer);
   FN("AMediaFormat_getInt32", get_integer);
#undef FN
   assert(0);
   return NULL;
}
static void balanced(void)
{
   assert(!libs && !codecs && !formats && !names);
}

int main(void)
{
   mediacodec_config_t config;
   mediacodec_session_t *s;
   int i, before;
   int64_t pts;
   static const uint8_t csd[] = {0, 0, 0, 1};
   memset(&config, 0, sizeof(config));
   config.mime = "video/hevc";
   config.width = 320;
   config.height = 192;
   config.profile = 2;
   config.max_input = 1024;
   config.csd[0] = csd;
   config.csd_size[0] = sizeof(csd);
   for (i = 1; i <= 20; i++)
   {
      missing = i;
      assert(!mediacodec_session_new(NULL, &config, &window));
      balanced();
   }
   missing = 0;
   for (i = 1; i <= 9; i++)
      if (i != 6 && i != 7)
      {
         stage = i;
         assert(!mediacodec_session_new(NULL, &config, &window));
         balanced();
      }
   stage = 0;
   s = mediacodec_session_new(NULL, &config, &window);
   assert(s);
   input_busy = 1;
   assert(!mediacodec_session_submit(s, csd, sizeof(csd), 99, false));
   input_busy = 0;
   assert(mediacodec_session_submit(s, csd, sizeof(csd), 99, false) == 1
         && queued_pts == 99 && !queued_flags && !memcmp(bytes, csd, 4));
   assert(!mediacodec_session_pump(s, &pts));
   output_mode = 1;
   assert(mediacodec_session_pump(s, &pts) == 2);
   assert(mediacodec_session_format(s)->right == 320
         && mediacodec_session_format(s)->serial == 1
         && !mediacodec_session_format(s)->color_transfer);
   output_mode = 3;
   assert(mediacodec_session_pump(s, &pts) == 1 && pts == 12345
         && rendered == 1);
   assert(mediacodec_session_submit(s, NULL, 0, 100, true) == 1
         && queued_flags == 4);
   output_mode = 5;
   assert(mediacodec_session_pump(s, &pts) == 1);
   assert(mediacodec_session_pump(s, &pts) == 3);
   mediacodec_session_free(s);
   balanced();
   s = mediacodec_session_new(NULL, &config, &window);
   assert(s);
   output_mode = 4;
   assert(mediacodec_session_pump(s, &pts) == 3 && released == 1);
   mediacodec_session_free(s);
   balanced();
   s = mediacodec_session_new(NULL, &config, &window);
   assert(s);
   output_mode = 6;
   assert(mediacodec_session_pump(s, &pts) == -1 && released == 2);
   assert(mediacodec_session_pump(s, &pts) == -1);
   mediacodec_session_free(s);
   balanced();
   s = mediacodec_session_new(NULL, &config, &window);
   assert(s);
   bad_crop = 1;
   output_mode = 1;
   assert(mediacodec_session_pump(s, &pts) == -1);
   mediacodec_session_free(s);
   balanced();
   s = mediacodec_session_new(NULL, &config, &window);
   assert(s);
   assert(mediacodec_session_submit(s, csd, 9, 1, false) == -1);
   assert(mediacodec_session_pump(s, &pts) == -1);
   mediacodec_session_free(s);
   balanced();
   before = rendered;
   output_mode = 2;
   s = mediacodec_session_new(NULL, &config, &window);
   assert(s && mediacodec_session_pump(s, &pts) == 0 && !pts);
   output_mode = 7;
   assert(mediacodec_session_pump(s, &pts) == 0 && !pts);
   assert(rendered == before);
   mediacodec_session_free(s);
   balanced();
   output_mode = 8;
   s = mediacodec_session_new(NULL, &config, &window);
   assert(s && mediacodec_session_pump(s, &pts) == -1 && !pts);
   assert(mediacodec_session_pump(s, &pts) == -1 && rendered == before);
   mediacodec_session_free(s);
   balanced();
   config.color_standard = 6;
   config.color_transfer = 6;
   config.color_range = 2;
   s = mediacodec_session_new(NULL, &config, &window);
   assert(s);
   assert(configured_standard == 6 && configured_transfer == 6
         && configured_range == 2);
   mediacodec_session_free(s);
   balanced();
   config.color_transfer = 7;
   config.color_range = 1;
   s = mediacodec_session_new(NULL, &config, &window);
   assert(s);
   assert(configured_transfer == 7 && configured_range == 1);
   mediacodec_session_free(s);
   balanced();
   config.color_standard = 3;
   assert(!mediacodec_session_new(NULL, &config, &window));
   balanced();
   config.color_standard = 6;
   config.color_transfer = 5;
   assert(!mediacodec_session_new(NULL, &config, &window));
   balanced();
   config.color_transfer = 6;
   config.color_range = 3;
   assert(!mediacodec_session_new(NULL, &config, &window));
   balanced();
   puts("PASS: MediaCodec startup faults, exact identity, zero-timeout input, "
        "format crop, Surface PTS, EOS, color hints and sticky errors");
   return 0;
}
