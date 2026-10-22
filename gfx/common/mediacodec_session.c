/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif
#include "mediacodec_session.h"
#include <stdlib.h>
#include <string.h>
#if defined(__ANDROID__) || defined(MEDIACODEC_SESSION_TEST)
#include <dynamic/dylib.h>
#ifdef MEDIACODEC_SESSION_TEST
#include "mediacodec_session_test_types.h"
#else
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#endif
/* Match the NDK's compatibility offset even with FILE_OFFSET_BITS=64 on
 * 32-bit Android. The public function keeps the original long-sized ABI. */
#if defined(__USE_FILE_OFFSET64) && !defined(__LP64__)
typedef int32_t codec_offset_t;
#else
typedef off_t codec_offset_t;
#endif
typedef char
      codec_offset_abi_check[(sizeof(codec_offset_t) == sizeof(long)) ? 1 : -1];
typedef AMediaCodec *(*codec_new_fn)(const char *);
typedef media_status_t (*codec_action_fn)(AMediaCodec *);
typedef media_status_t (*codec_config_fn)(AMediaCodec *, const AMediaFormat *,
      ANativeWindow *, AMediaCrypto *, uint32_t);
typedef media_status_t (*codec_name_fn)(AMediaCodec *, char **);
typedef void (*codec_name_free_fn)(AMediaCodec *, char *);
typedef ssize_t (*codec_input_fn)(AMediaCodec *, int64_t);
typedef uint8_t *(*codec_buffer_fn)(AMediaCodec *, size_t, size_t *);
typedef media_status_t (*codec_queue_fn)(
      AMediaCodec *, size_t, codec_offset_t, size_t, uint64_t, uint32_t);
typedef ssize_t (*codec_output_fn)(
      AMediaCodec *, AMediaCodecBufferInfo *, int64_t);
typedef media_status_t (*codec_release_fn)(AMediaCodec *, size_t, bool);
typedef media_status_t (*codec_render_fn)(AMediaCodec *, size_t, int64_t);
typedef AMediaFormat *(*codec_format_fn)(AMediaCodec *);
typedef AMediaFormat *(*format_new_fn)(void);
typedef media_status_t (*format_free_fn)(AMediaFormat *);
typedef void (*format_string_fn)(AMediaFormat *, const char *, const char *);
typedef void (*format_integer_fn)(AMediaFormat *, const char *, int32_t);
typedef void (*format_buffer_fn)(
      AMediaFormat *, const char *, const void *, size_t);
typedef bool (*format_get_fn)(AMediaFormat *, const char *, int32_t *);
#if defined(__ANDROID__) && defined(__clang__) && __ANDROID_API__ >= 29
#define CODEC_ABI(type, symbol) \
   __extension__ typedef char symbol##_abi \
         [__builtin_types_compatible_p(type, __typeof__(&symbol)) ? 1 : -1]
CODEC_ABI(codec_new_fn, AMediaCodec_createCodecByName);
CODEC_ABI(codec_config_fn, AMediaCodec_configure);
CODEC_ABI(codec_action_fn, AMediaCodec_start);
CODEC_ABI(codec_action_fn, AMediaCodec_stop);
CODEC_ABI(codec_action_fn, AMediaCodec_delete);
CODEC_ABI(codec_name_fn, AMediaCodec_getName);
CODEC_ABI(codec_name_free_fn, AMediaCodec_releaseName);
CODEC_ABI(codec_input_fn, AMediaCodec_dequeueInputBuffer);
CODEC_ABI(codec_buffer_fn, AMediaCodec_getInputBuffer);
CODEC_ABI(codec_queue_fn, AMediaCodec_queueInputBuffer);
CODEC_ABI(codec_output_fn, AMediaCodec_dequeueOutputBuffer);
CODEC_ABI(codec_release_fn, AMediaCodec_releaseOutputBuffer);
CODEC_ABI(codec_render_fn, AMediaCodec_releaseOutputBufferAtTime);
CODEC_ABI(codec_format_fn, AMediaCodec_getOutputFormat);
CODEC_ABI(format_new_fn, AMediaFormat_new);
CODEC_ABI(format_free_fn, AMediaFormat_delete);
CODEC_ABI(format_string_fn, AMediaFormat_setString);
CODEC_ABI(format_integer_fn, AMediaFormat_setInt32);
CODEC_ABI(format_buffer_fn, AMediaFormat_setBuffer);
CODEC_ABI(format_get_fn, AMediaFormat_getInt32);
#undef CODEC_ABI
#endif
struct mediacodec_session
{
   dylib_t library;
   AMediaCodec *codec;
   AMediaFormat *config;
   codec_action_fn stop, delete_codec;
   codec_input_fn input;
   codec_buffer_fn buffer;
   codec_queue_fn queue;
   codec_output_fn output;
   codec_release_fn release;
   codec_render_fn render;
   codec_format_fn get_format;
   format_free_fn delete_format;
   format_get_fn get_integer;
   mediacodec_output_format_t format;
   bool started, failed, input_eos, output_eos;
};
void mediacodec_session_free(mediacodec_session_t *s)
{
   if (!s)
      return;
   if (s->codec)
   {
      if (s->started)
         s->stop(s->codec);
      s->delete_codec(s->codec);
   }
   if (s->config)
      s->delete_format(s->config);
   if (s->library)
      dylib_close(s->library);
   free(s);
}

mediacodec_session_t *mediacodec_session_new(
      JNIEnv *env, const mediacodec_config_t *config, void *window)
{
   mediacodec_session_t *s;
   char selected[256], *actual = NULL;
   unsigned i;
   codec_new_fn make;
   codec_config_fn configure;
   codec_action_fn start;
   codec_name_fn name;
   codec_name_free_fn release_name;
   format_new_fn format_new;
   format_string_fn set_string;
   format_integer_fn set_integer;
   format_buffer_fn set_buffer;
   if (!config || !window || !config->max_input
         || config->max_input > 64u * 1024u * 1024u)
      return NULL;
   if ((config->color_standard && config->color_standard != 1
             && config->color_standard != 2 && config->color_standard != 4
             && config->color_standard != 6)
         || (config->color_transfer && config->color_transfer != 1
               && config->color_transfer != 3 && config->color_transfer != 6
               && config->color_transfer != 7)
         || config->color_range < 0 || config->color_range > 2)
      return NULL;
   for (i = 0; i < 3; i++)
      if (config->csd_size[i] > 1024u * 1024u
            || (config->csd_size[i] && !config->csd[i]))
         return NULL;
   if (mediacodec_inventory_select(env, config->mime, 0, config->width,
             config->height, config->profile, selected, sizeof(selected))
         != 1)
      return NULL;
   s = (mediacodec_session_t *)calloc(1, sizeof(*s));
   if (!s)
      return NULL;
   s->library = dylib_load("libmediandk.so");
   if (!s->library)
      goto failed;
#define LOAD(target, type, symbol) \
   do \
   { \
      target = (type)dylib_proc(s->library, symbol); \
      if (!target) \
         goto failed; \
   } while (0)
   LOAD(make, codec_new_fn, "AMediaCodec_createCodecByName");
   LOAD(configure, codec_config_fn, "AMediaCodec_configure");
   LOAD(start, codec_action_fn, "AMediaCodec_start");
   LOAD(s->stop, codec_action_fn, "AMediaCodec_stop");
   LOAD(s->delete_codec, codec_action_fn, "AMediaCodec_delete");
   LOAD(name, codec_name_fn, "AMediaCodec_getName");
   LOAD(release_name, codec_name_free_fn, "AMediaCodec_releaseName");
   LOAD(s->input, codec_input_fn, "AMediaCodec_dequeueInputBuffer");
   LOAD(s->buffer, codec_buffer_fn, "AMediaCodec_getInputBuffer");
   LOAD(s->queue, codec_queue_fn, "AMediaCodec_queueInputBuffer");
   LOAD(s->output, codec_output_fn, "AMediaCodec_dequeueOutputBuffer");
   LOAD(s->release, codec_release_fn, "AMediaCodec_releaseOutputBuffer");
   LOAD(s->render, codec_render_fn, "AMediaCodec_releaseOutputBufferAtTime");
   LOAD(s->get_format, codec_format_fn, "AMediaCodec_getOutputFormat");
   LOAD(format_new, format_new_fn, "AMediaFormat_new");
   LOAD(s->delete_format, format_free_fn, "AMediaFormat_delete");
   LOAD(set_string, format_string_fn, "AMediaFormat_setString");
   LOAD(set_integer, format_integer_fn, "AMediaFormat_setInt32");
   LOAD(set_buffer, format_buffer_fn, "AMediaFormat_setBuffer");
   LOAD(s->get_integer, format_get_fn, "AMediaFormat_getInt32");
#undef LOAD
   s->codec = make(selected);
   if (!s->codec)
      goto failed;
   if (name(s->codec, &actual) != AMEDIA_OK || !actual)
   {
      if (actual)
         release_name(s->codec, actual);
      goto failed;
   }
   i = strcmp(actual, selected) == 0;
   release_name(s->codec, actual);
   if (!i)
      goto failed;
   s->config = format_new();
   if (!s->config)
      goto failed;
   set_string(s->config, "mime", config->mime);
   set_integer(s->config, "width", (int32_t)config->width);
   set_integer(s->config, "height", (int32_t)config->height);
   set_integer(s->config, "profile", config->profile);
   set_integer(s->config, "max-input-size", (int32_t)config->max_input);
   if (config->color_standard)
      set_integer(s->config, "color-standard", config->color_standard);
   if (config->color_transfer)
      set_integer(s->config, "color-transfer", config->color_transfer);
   if (config->color_range)
      set_integer(s->config, "color-range", config->color_range);
   for (i = 0; i < 3; i++)
      if (config->csd_size[i])
      {
         static const char *keys[] = {"csd-0", "csd-1", "csd-2"};
         set_buffer(s->config, keys[i], config->csd[i], config->csd_size[i]);
      }
   if (configure(s->codec, s->config, (ANativeWindow *)window, NULL, 0)
         != AMEDIA_OK)
      goto failed;
   s->started = true;
   if (start(s->codec) != AMEDIA_OK)
      goto failed;
   s->format.width = s->format.right = config->width;
   s->format.height = s->format.bottom = config->height;
   return s;
failed:
   mediacodec_session_free(s);
   return NULL;
}
int mediacodec_session_submit(mediacodec_session_t *s, const void *data,
      size_t bytes, int64_t pts_us, bool eos)
{
   ssize_t index;
   size_t capacity = 0;
   uint8_t *buffer;
   if (!s || s->failed)
      return -1;
   if (s->input_eos || pts_us < 0 || (bytes && !data) || (!bytes && !eos))
      goto failed;
   index = s->input(s->codec, 0);
   if (index == AMEDIACODEC_INFO_TRY_AGAIN_LATER)
      return 0;
   if (index < 0)
      goto failed;
   buffer = s->buffer(s->codec, (size_t)index, &capacity);
   if ((bytes && !buffer) || bytes > capacity)
      goto failed;
   if (bytes)
      memcpy(buffer, data, bytes);
   if (s->queue(s->codec, (size_t)index, 0, bytes, (uint64_t)pts_us,
             eos ? AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM : 0)
         != AMEDIA_OK)
      goto failed;
   if (eos)
      s->input_eos = true;
   return 1;
failed:
   s->failed = true;
   return -1;
}
static bool mediacodec_read_format(mediacodec_session_t *s)
{
   AMediaFormat *format = s->get_format(s->codec);
   mediacodec_output_format_t next;
   int32_t width = 0, height = 0, left = 0, top = 0, right, bottom, value;
   bool valid = false;
   if (!format)
      return false;
   memset(&next, 0, sizeof(next));
   if (!s->get_integer(format, "width", &width)
         || !s->get_integer(format, "height", &height) || width <= 0
         || height <= 0 || width > 16384 || height > 16384
         || s->format.serial == ~0u)
      goto done;
   right = width - 1;
   bottom = height - 1;
   s->get_integer(format, "crop-left", &left);
   s->get_integer(format, "crop-top", &top);
   s->get_integer(format, "crop-right", &right);
   s->get_integer(format, "crop-bottom", &bottom);
   if (left < 0 || top < 0 || right < left || bottom < top || right >= width
         || bottom >= height)
      goto done;
   next.width = (unsigned)width;
   next.height = (unsigned)height;
   next.left = (unsigned)left;
   next.top = (unsigned)top;
   next.right = (unsigned)right + 1;
   next.bottom = (unsigned)bottom + 1;
   next.serial = s->format.serial + 1;
   if (s->get_integer(format, "color-standard", &value))
      next.color_standard = value;
   if (s->get_integer(format, "color-transfer", &value))
      next.color_transfer = value;
   if (s->get_integer(format, "color-range", &value))
      next.color_range = value;
   s->format = next;
   valid = true;
done:
   s->delete_format(format);
   return valid;
}
int mediacodec_session_pump(mediacodec_session_t *s, int64_t *pts_us)
{
   AMediaCodecBufferInfo info;
   ssize_t index;
   bool eos;
   if (pts_us)
      *pts_us = 0;
   if (!s || s->failed)
      return -1;
   if (s->output_eos)
      return 3;
   memset(&info, 0, sizeof(info));
   index = s->output(s->codec, &info, 0);
   if (index == AMEDIACODEC_INFO_TRY_AGAIN_LATER
         || index == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED)
      return 0;
   if (index == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED)
   {
      if (mediacodec_read_format(s))
         return 2;
      goto failed;
   }
   if (index < 0)
      goto failed;
   if (info.offset < 0 || info.size < 0)
   {
      s->release(s->codec, (size_t)index, false);
      goto failed;
   }
   eos = !!(info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
   if ((info.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG)
         || (eos && !info.size))
   {
      if (s->release(s->codec, (size_t)index, false) != AMEDIA_OK)
         goto failed;
      if (eos)
      {
         s->output_eos = true;
         return 3;
      }
      return 0;
   }
   if (info.presentationTimeUs < 0
         || (uint64_t)info.presentationTimeUs
               > (((uint64_t)1 << 63) - 1) / 1000)
   {
      s->release(s->codec, (size_t)index, false);
      goto failed;
   }
   if (s->render(s->codec, (size_t)index, info.presentationTimeUs * 1000)
         != AMEDIA_OK)
      goto failed;
   if (pts_us)
      *pts_us = info.presentationTimeUs;
   if (eos)
      s->output_eos = true;
   return 1;
failed:
   s->failed = true;
   return -1;
}
const mediacodec_output_format_t *mediacodec_session_format(
      const mediacodec_session_t *s)
{
   return s ? &s->format : NULL;
}
#else
mediacodec_session_t *mediacodec_session_new(
      JNIEnv *env, const mediacodec_config_t *config, void *window)
{
   (void)env;
   (void)config;
   (void)window;
   return NULL;
}
int mediacodec_session_submit(mediacodec_session_t *s, const void *data,
      size_t bytes, int64_t pts_us, bool eos)
{
   (void)s;
   (void)data;
   (void)bytes;
   (void)pts_us;
   (void)eos;
   return -1;
}
int mediacodec_session_pump(mediacodec_session_t *s, int64_t *pts_us)
{
   (void)s;
   if (pts_us)
      *pts_us = 0;
   return -1;
}
const mediacodec_output_format_t *mediacodec_session_format(
      const mediacodec_session_t *s)
{
   (void)s;
   return NULL;
}
void mediacodec_session_free(mediacodec_session_t *s)
{
   (void)s;
}
#endif
