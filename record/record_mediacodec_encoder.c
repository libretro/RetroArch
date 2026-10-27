/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
#include "record_mediacodec_encoder.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#if defined(__ANDROID__) || defined(RECORD_MEDIACODEC_TEST)
#include "../gfx/common/mediacodec_encoder_format.h"
#include <dynamic/dylib.h>
#ifdef RECORD_MEDIACODEC_TEST
#include "mediacodec_encode_test_types.h"
#else
#include <compat/android_native_window.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#endif
typedef AMediaCodec *(*record_mc_new_fn)(const char *);
typedef media_status_t (*record_mc_action_fn)(AMediaCodec *);
typedef media_status_t (*record_mc_configure_fn)(AMediaCodec *,
      const AMediaFormat *, ANativeWindow *, AMediaCrypto *, uint32_t);
typedef media_status_t (*record_mc_surface_fn)(AMediaCodec *, ANativeWindow **);
typedef void (*record_mc_window_free_fn)(ANativeWindow *);
typedef media_status_t (*record_mc_name_fn)(AMediaCodec *, char **);
typedef void (*record_mc_name_free_fn)(AMediaCodec *, char *);
typedef ssize_t (*record_mc_output_fn)(
      AMediaCodec *, AMediaCodecBufferInfo *, int64_t);
typedef uint8_t *(*record_mc_buffer_fn)(AMediaCodec *, size_t, size_t *);
typedef media_status_t (*record_mc_release_fn)(AMediaCodec *, size_t, bool);
typedef AMediaFormat *(*record_mc_format_fn)(AMediaCodec *);
typedef AMediaFormat *(*record_mc_format_new_fn)(void);
typedef media_status_t (*record_mc_format_free_fn)(AMediaFormat *);
typedef void (*record_mc_string_fn)(AMediaFormat *, const char *, const char *);
typedef void (*record_mc_integer_fn)(AMediaFormat *, const char *, int32_t);
typedef void (*record_mc_float_fn)(AMediaFormat *, const char *, float);
typedef void (*record_mc_set_buffer_fn)(
      AMediaFormat *, const char *, const void *, size_t);
typedef bool (*record_mc_get_buffer_fn)(
      AMediaFormat *, const char *, void **, size_t *);
typedef bool (*record_mc_get_integer_fn)(
      AMediaFormat *, const char *, int32_t *);
typedef ssize_t (*record_mc_input_fn)(AMediaCodec *, int64_t);
/* The codec queue keeps its long-sized offset in large-file builds. */
#if defined(__USE_FILE_OFFSET64) && !defined(__LP64__)
typedef int32_t record_mc_offset_t;
#else
typedef off_t record_mc_offset_t;
#endif
typedef char record_mc_offset_abi[(sizeof(record_mc_offset_t) == sizeof(long))
            ? 1
            : -1];
typedef media_status_t (*record_mc_queue_fn)(
      AMediaCodec *, size_t, record_mc_offset_t, size_t, uint64_t, uint32_t);
#if defined(__ANDROID__) && defined(__clang__) && __ANDROID_API__ >= 29
#define RECORD_MC_ABI(type, symbol)                                            \
   __extension__ typedef char symbol##_record_abi                              \
         [__builtin_types_compatible_p(type, __typeof__(&symbol)) ? 1 : -1]
RECORD_MC_ABI(record_mc_new_fn, AMediaCodec_createCodecByName);
RECORD_MC_ABI(record_mc_new_fn, AMediaCodec_createEncoderByType);
RECORD_MC_ABI(record_mc_action_fn, AMediaCodec_start);
RECORD_MC_ABI(record_mc_action_fn, AMediaCodec_stop);
RECORD_MC_ABI(record_mc_action_fn, AMediaCodec_delete);
RECORD_MC_ABI(record_mc_action_fn, AMediaCodec_signalEndOfInputStream);
RECORD_MC_ABI(record_mc_configure_fn, AMediaCodec_configure);
RECORD_MC_ABI(record_mc_surface_fn, AMediaCodec_createInputSurface);
RECORD_MC_ABI(record_mc_window_free_fn, ANativeWindow_release);
RECORD_MC_ABI(record_mc_name_fn, AMediaCodec_getName);
RECORD_MC_ABI(record_mc_name_free_fn, AMediaCodec_releaseName);
RECORD_MC_ABI(record_mc_output_fn, AMediaCodec_dequeueOutputBuffer);
RECORD_MC_ABI(record_mc_buffer_fn, AMediaCodec_getOutputBuffer);
RECORD_MC_ABI(record_mc_release_fn, AMediaCodec_releaseOutputBuffer);
RECORD_MC_ABI(record_mc_format_fn, AMediaCodec_getOutputFormat);
RECORD_MC_ABI(record_mc_format_new_fn, AMediaFormat_new);
RECORD_MC_ABI(record_mc_format_free_fn, AMediaFormat_delete);
RECORD_MC_ABI(record_mc_string_fn, AMediaFormat_setString);
RECORD_MC_ABI(record_mc_integer_fn, AMediaFormat_setInt32);
RECORD_MC_ABI(record_mc_float_fn, AMediaFormat_setFloat);
RECORD_MC_ABI(record_mc_get_integer_fn, AMediaFormat_getInt32);
RECORD_MC_ABI(record_mc_set_buffer_fn, AMediaFormat_setBuffer);
RECORD_MC_ABI(record_mc_get_buffer_fn, AMediaFormat_getBuffer);
RECORD_MC_ABI(record_mc_input_fn, AMediaCodec_dequeueInputBuffer);
RECORD_MC_ABI(record_mc_buffer_fn, AMediaCodec_getInputBuffer);
RECORD_MC_ABI(record_mc_queue_fn, AMediaCodec_queueInputBuffer);
#undef RECORD_MC_ABI
#endif
struct record_mediacodec_encoder
{
   uint64_t next_sample, end_sample;
   dylib_t library, android_library;
   AMediaCodec *codec;
   AMediaFormat *config, *format;
   ANativeWindow *surface;
   record_mc_action_fn stop, delete_codec, finish;
   record_mc_window_free_fn window_free;
   record_mc_format_free_fn format_free;
   record_mc_output_fn output;
   record_mc_buffer_fn buffer;
   record_mc_release_fn release;
   record_mc_format_fn get_format;
   record_mc_get_integer_fn get_integer;
   record_mc_get_buffer_fn get_buffer;
   record_mc_input_fn input;
   record_mc_buffer_fn input_buffer;
   record_mc_queue_fn queue;
   record_mediacodec_packet_t packet;
   size_t index;
   unsigned rate, channels, width, height, color, range;
   int profile, base_profile;
   unsigned char hdr_static[25];
   bool started, failed, held, input_eos, output_eos;
   bool audio, seen_input, closing, stop_attempted, stopped;
};
bool record_mediacodec_encoder_stop(record_mediacodec_encoder_t *s)
{
   if (!s)
      return false;
   if (s->stop_attempted)
      return s->stopped;
   s->stop_attempted = true;
   s->failed = true;
   s->held = false;
   memset(&s->packet, 0, sizeof(s->packet));
   if (!s->started || s->stop(s->codec) != AMEDIA_OK)
      return false;
   s->started = false;
   s->stopped = true;
   return true;
}
void record_mediacodec_encoder_free(record_mediacodec_encoder_t *s)
{
   if (!s)
      return;
   if (s->codec)
   {
      if (s->started && !s->stop_attempted)
         s->stop(s->codec);
      s->delete_codec(s->codec);
   }
   if (s->surface)
      s->window_free(s->surface);
   if (s->format)
      s->format_free(s->format);
   if (s->config)
      s->format_free(s->config);
   if (s->android_library)
      dylib_close(s->android_library);
   if (s->library)
      dylib_close(s->library);
   free(s);
}
record_mediacodec_encoder_t *record_mediacodec_encoder_new(JNIEnv *env,
      const mediacodec_encoder_request_t *request, unsigned interval)
{
   record_mediacodec_encoder_t *s;
   char selected[256], *actual = NULL;
   record_mc_new_fn make;
   record_mc_configure_fn configure;
   record_mc_action_fn start;
   record_mc_surface_fn surface;
   record_mc_name_fn name;
   record_mc_name_free_fn name_free;
   record_mc_format_new_fn format_new;
   record_mc_string_fn set_string;
   record_mc_integer_fn set_integer;
   record_mc_float_fn set_float;
   record_mc_set_buffer_fn set_buffer;
   int matches;
   if (!mediacodec_encoder_color_valid(request) || interval > INT_MAX
         || mediacodec_inventory_select_encoder(
                  env, request, selected, sizeof(selected))
               != 1)
      return NULL;
   s = (record_mediacodec_encoder_t *)calloc(1, sizeof(*s));
   if (!s)
      return NULL;
   s->width = request->width;
   s->height = request->height;
   s->color = request->color;
   s->range = request->range;
   s->profile = request->profile;
   s->base_profile = !strcmp(request->mime, "video/x-vnd.on2.vp9") ? 4 : 2;
   memcpy(s->hdr_static, request->hdr_static, sizeof(s->hdr_static));
   s->library = dylib_load("libmediandk.so");
   s->android_library = dylib_load("libandroid.so");
   if (!s->library || !s->android_library)
      goto failed;
#define RECORD_MC_LOAD(target, type, symbol)                                   \
   do                                                                          \
   {                                                                           \
      target = (type)dylib_proc(s->library, symbol);                           \
      if (!target)                                                             \
         goto failed;                                                          \
   } while (0)
   RECORD_MC_LOAD(make, record_mc_new_fn, "AMediaCodec_createCodecByName");
   RECORD_MC_LOAD(configure, record_mc_configure_fn, "AMediaCodec_configure");
   RECORD_MC_LOAD(start, record_mc_action_fn, "AMediaCodec_start");
   RECORD_MC_LOAD(s->stop, record_mc_action_fn, "AMediaCodec_stop");
   RECORD_MC_LOAD(s->delete_codec, record_mc_action_fn, "AMediaCodec_delete");
   RECORD_MC_LOAD(
         s->finish, record_mc_action_fn, "AMediaCodec_signalEndOfInputStream");
   RECORD_MC_LOAD(
         surface, record_mc_surface_fn, "AMediaCodec_createInputSurface");
   RECORD_MC_LOAD(name, record_mc_name_fn, "AMediaCodec_getName");
   RECORD_MC_LOAD(name_free, record_mc_name_free_fn, "AMediaCodec_releaseName");
   RECORD_MC_LOAD(
         s->output, record_mc_output_fn, "AMediaCodec_dequeueOutputBuffer");
   RECORD_MC_LOAD(
         s->buffer, record_mc_buffer_fn, "AMediaCodec_getOutputBuffer");
   RECORD_MC_LOAD(
         s->release, record_mc_release_fn, "AMediaCodec_releaseOutputBuffer");
   RECORD_MC_LOAD(
         s->get_format, record_mc_format_fn, "AMediaCodec_getOutputFormat");
   RECORD_MC_LOAD(format_new, record_mc_format_new_fn, "AMediaFormat_new");
   RECORD_MC_LOAD(
         s->format_free, record_mc_format_free_fn, "AMediaFormat_delete");
   RECORD_MC_LOAD(set_string, record_mc_string_fn, "AMediaFormat_setString");
   RECORD_MC_LOAD(set_integer, record_mc_integer_fn, "AMediaFormat_setInt32");
   RECORD_MC_LOAD(set_float, record_mc_float_fn, "AMediaFormat_setFloat");
   RECORD_MC_LOAD(
         s->get_integer, record_mc_get_integer_fn, "AMediaFormat_getInt32");
   RECORD_MC_LOAD(
         s->get_buffer, record_mc_get_buffer_fn, "AMediaFormat_getBuffer");
   RECORD_MC_LOAD(
         set_buffer, record_mc_set_buffer_fn, "AMediaFormat_setBuffer");
#undef RECORD_MC_LOAD
   s->window_free = (record_mc_window_free_fn)dylib_proc(
         s->android_library, "ANativeWindow_release");
   if (!s->window_free)
      goto failed;
   s->codec = make(selected);
   if (!s->codec)
      goto failed;
   if (name(s->codec, &actual) != AMEDIA_OK || !actual)
   {
      if (actual)
         name_free(s->codec, actual);
      goto failed;
   }
   matches = !strcmp(selected, actual);
   name_free(s->codec, actual);
   if (!matches || !(s->config = format_new()))
      goto failed;
   set_string(s->config, "mime", request->mime);
   set_integer(s->config, "width", (int32_t)request->width);
   set_integer(s->config, "height", (int32_t)request->height);
   set_integer(s->config, "profile", request->profile);
   set_integer(s->config, "level", request->level);
   set_integer(s->config, "color-standard", request->color ? 6 : 1);
   set_integer(s->config, "color-transfer", request->color ? 6 : 3);
   set_integer(s->config, "color-range", (int32_t)request->range);
   if (request->color)
      set_buffer(s->config, "hdr-static-info", request->hdr_static, 25);
   set_integer(s->config, "bitrate", (int32_t)request->bitrate);
   set_integer(s->config, "color-format", 0x7f000789);
   set_integer(s->config, "i-frame-interval", (int32_t)interval);
   set_float(s->config, "frame-rate", (float)request->frame_rate);
   if (configure(
             s->codec, s->config, NULL, NULL, AMEDIACODEC_CONFIGURE_FLAG_ENCODE)
         != AMEDIA_OK)
      goto failed;
   if (surface(s->codec, &s->surface) != AMEDIA_OK || !s->surface)
      goto failed;
   s->started = true;
   if (start(s->codec) != AMEDIA_OK)
      goto failed;
   return s;
failed:
   record_mediacodec_encoder_free(s);
   return NULL;
}
record_mediacodec_encoder_t *record_mediacodec_encoder_audio_new(
      unsigned rate, unsigned channels, unsigned bitrate)
{
   record_mediacodec_encoder_t *s;
   record_mc_new_fn make;
   record_mc_configure_fn configure;
   record_mc_action_fn start;
   record_mc_format_new_fn format_new;
   record_mc_string_fn set_string;
   record_mc_integer_fn set_integer;
   if (!rate || rate > INT_MAX || !bitrate || bitrate > INT_MAX
         || (channels != 1 && channels != 2))
      return NULL;
   s = (record_mediacodec_encoder_t *)calloc(1, sizeof(*s));
   if (!s)
      return NULL;
   s->audio = true;
   s->rate = rate;
   s->channels = channels;
   s->library = dylib_load("libmediandk.so");
   if (!s->library)
      goto failed;
#define RECORD_AAC_LOAD(target, type, symbol)                                  \
   do                                                                          \
   {                                                                           \
      target = (type)dylib_proc(s->library, symbol);                           \
      if (!target)                                                             \
         goto failed;                                                          \
   } while (0)
   RECORD_AAC_LOAD(make, record_mc_new_fn, "AMediaCodec_createEncoderByType");
   RECORD_AAC_LOAD(configure, record_mc_configure_fn, "AMediaCodec_configure");
   RECORD_AAC_LOAD(start, record_mc_action_fn, "AMediaCodec_start");
   RECORD_AAC_LOAD(s->stop, record_mc_action_fn, "AMediaCodec_stop");
   RECORD_AAC_LOAD(s->delete_codec, record_mc_action_fn, "AMediaCodec_delete");
   RECORD_AAC_LOAD(
         s->output, record_mc_output_fn, "AMediaCodec_dequeueOutputBuffer");
   RECORD_AAC_LOAD(
         s->buffer, record_mc_buffer_fn, "AMediaCodec_getOutputBuffer");
   RECORD_AAC_LOAD(
         s->release, record_mc_release_fn, "AMediaCodec_releaseOutputBuffer");
   RECORD_AAC_LOAD(
         s->get_format, record_mc_format_fn, "AMediaCodec_getOutputFormat");
   RECORD_AAC_LOAD(format_new, record_mc_format_new_fn, "AMediaFormat_new");
   RECORD_AAC_LOAD(
         s->format_free, record_mc_format_free_fn, "AMediaFormat_delete");
   RECORD_AAC_LOAD(set_string, record_mc_string_fn, "AMediaFormat_setString");
   RECORD_AAC_LOAD(set_integer, record_mc_integer_fn, "AMediaFormat_setInt32");
   RECORD_AAC_LOAD(
         s->get_integer, record_mc_get_integer_fn, "AMediaFormat_getInt32");
   RECORD_AAC_LOAD(
         s->get_buffer, record_mc_get_buffer_fn, "AMediaFormat_getBuffer");
   RECORD_AAC_LOAD(
         s->input, record_mc_input_fn, "AMediaCodec_dequeueInputBuffer");
   RECORD_AAC_LOAD(
         s->input_buffer, record_mc_buffer_fn, "AMediaCodec_getInputBuffer");
   RECORD_AAC_LOAD(
         s->queue, record_mc_queue_fn, "AMediaCodec_queueInputBuffer");
#undef RECORD_AAC_LOAD
   s->codec = make("audio/mp4a-latm");
   if (!s->codec || !(s->config = format_new()))
      goto failed;
   set_string(s->config, "mime", "audio/mp4a-latm");
   set_integer(s->config, "sample-rate", (int32_t)rate);
   set_integer(s->config, "channel-count", (int32_t)channels);
   set_integer(s->config, "bitrate", (int32_t)bitrate);
   set_integer(s->config, "aac-profile", 2);
   set_integer(s->config, "pcm-encoding", 2);
   set_integer(s->config, "max-input-size", 16384);
   if (configure(
             s->codec, s->config, NULL, NULL, AMEDIACODEC_CONFIGURE_FLAG_ENCODE)
         != AMEDIA_OK)
      goto failed;
   s->started = true;
   if (start(s->codec) != AMEDIA_OK)
      goto failed;
   return s;
failed:
   record_mediacodec_encoder_free(s);
   return NULL;
}
static bool record_mc_sample_time(unsigned rate, uint64_t sample, int64_t *time)
{
   uint64_t maximum = ((uint64_t)-1) >> 1;
   uint64_t whole = sample / rate, fraction = (sample % rate) * 1000000 / rate;
   if (whole > maximum / 1000000 || fraction > maximum - whole * 1000000)
      return false;
   *time = (int64_t)(whole * 1000000 + fraction);
   return true;
}
static int record_mc_audio_input(record_mediacodec_encoder_t *s,
      const int16_t *samples, uint64_t count, uint64_t timestamp, bool eos,
      size_t *accepted)
{
   ssize_t index;
   uint8_t *data;
   size_t capacity = 0, frames, stride = s->channels * sizeof(int16_t);
   int64_t pts;
   if (!record_mc_sample_time(s->rate, timestamp, &pts))
      goto failed;
   index = s->input(s->codec, 0);
   if (index == AMEDIACODEC_INFO_TRY_AGAIN_LATER)
      return 0;
   if (index < 0)
      goto failed;
   frames = 0;
   if (!eos)
   {
      data = s->input_buffer(s->codec, (size_t)index, &capacity);
      frames = capacity / stride;
      if (frames > 4096)
         frames = 4096;
      if (frames > count)
         frames = (size_t)count;
      if (!data || !frames)
         goto failed;
      if (samples)
         memcpy(data, samples, frames * stride);
      else
         memset(data, 0, frames * stride);
   }
   if (s->queue(s->codec, (size_t)index, 0, frames * stride, (uint64_t)pts,
             eos ? AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM : 0)
         != AMEDIA_OK)
      goto failed;
   if (eos)
      s->input_eos = true;
   else
   {
      s->next_sample = timestamp + frames;
      s->seen_input = true;
      if (samples)
         *accepted = frames;
   }
   return 1;
failed:
   s->failed = true;
   return -1;
}
int record_mediacodec_encoder_audio_submit(record_mediacodec_encoder_t *s,
      const int16_t *samples, size_t frames, uint64_t timestamp,
      size_t *accepted)
{
   int64_t end;
   if (accepted)
      *accepted = 0;
   if (!s || !s->audio || s->failed)
      return -1;
   if (!accepted || !samples || !frames || s->closing
         || frames > (size_t)-1 / (s->channels * sizeof(int16_t))
         || (uint64_t)frames > (uint64_t)-1 - timestamp
         || !record_mc_sample_time(s->rate, timestamp + frames, &end)
         || (s->seen_input && timestamp < s->next_sample))
   {
      s->failed = true;
      return -1;
   }
   if (s->seen_input && timestamp > s->next_sample)
      return record_mc_audio_input(s, NULL, timestamp - s->next_sample,
            s->next_sample, false, accepted);
   return record_mc_audio_input(s, samples, frames, timestamp, false, accepted);
}
int record_mediacodec_encoder_audio_finish(
      record_mediacodec_encoder_t *s, uint64_t end_sample)
{
   int64_t end;
   size_t accepted = 0;
   int result;
   if (!s || !s->audio || s->failed)
      return -1;
   if (!record_mc_sample_time(s->rate, end_sample, &end)
         || (s->seen_input && end_sample < s->next_sample)
         || (s->closing && end_sample != s->end_sample))
   {
      s->failed = true;
      return -1;
   }
   s->closing = true;
   s->end_sample = end_sample;
   if (s->input_eos)
      return 1;
   if (end_sample > s->next_sample)
   {
      result = record_mc_audio_input(s, NULL, end_sample - s->next_sample,
            s->next_sample, false, &accepted);
      return result == 1 ? 2 : result;
   }
   return record_mc_audio_input(s, NULL, 0, end_sample, true, &accepted);
}
void *record_mediacodec_encoder_surface(record_mediacodec_encoder_t *s)
{
   return s && !s->failed && !s->input_eos ? s->surface : NULL;
}
void *record_mediacodec_encoder_format(record_mediacodec_encoder_t *s)
{
   return s && !s->failed ? s->format : NULL;
}
bool record_mediacodec_encoder_audio_config(record_mediacodec_encoder_t *s,
      const void **data, size_t *bytes)
{
   void *buffer = NULL;
   size_t size = 0;
   if (data)
      *data = NULL;
   if (bytes)
      *bytes = 0;
   if (!s || !data || !bytes || s->failed || !s->audio || !s->format
         || !s->get_buffer(s->format, "csd-0", &buffer, &size)
         || !buffer || !size)
      return false;
   *data = buffer;
   *bytes = size;
   return true;
}
bool record_mediacodec_encoder_finish(record_mediacodec_encoder_t *s)
{
   if (!s || s->failed || s->audio)
      return false;
   if (s->input_eos)
      return true;
   if (s->finish(s->codec) != AMEDIA_OK)
   {
      s->failed = true;
      return false;
   }
   s->input_eos = true;
   return true;
}
bool record_mediacodec_encoder_release(record_mediacodec_encoder_t *s)
{
   if (!s || s->failed || !s->held)
      return false;
   if (s->release(s->codec, s->index, false) != AMEDIA_OK)
   {
      s->failed = true;
      return false;
   }
   s->output_eos = !!(s->packet.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
   s->held = false;
   memset(&s->packet, 0, sizeof(s->packet));
   return true;
}
int record_mediacodec_encoder_pump(
      record_mediacodec_encoder_t *s, record_mediacodec_packet_t *packet)
{
   AMediaCodecBufferInfo info;
   ssize_t index;
   uint8_t *data;
   size_t capacity = 0;
   if (packet)
      memset(packet, 0, sizeof(*packet));
   if (!s || !packet || s->failed)
      return RECORD_MC_ERROR;
   if (s->held)
   {
      *packet = s->packet;
      return RECORD_MC_PACKET;
   }
   if (s->output_eos)
      return RECORD_MC_END;
   memset(&info, 0, sizeof(info));
   index = s->output(s->codec, &info, 0);
   if (index == AMEDIACODEC_INFO_TRY_AGAIN_LATER)
      return RECORD_MC_BUSY;
   if (index == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED)
      return RECORD_MC_PROGRESS;
   if (index == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED)
   {
      AMediaFormat *format = s->get_format(s->codec);
      if (!format)
         goto failed;
      if (s->audio)
      {
         int32_t rate = 0, channels = 0;
         if (!s->get_integer(format, "sample-rate", &rate)
               || !s->get_integer(format, "channel-count", &channels)
               || rate != (int32_t)s->rate || channels != (int32_t)s->channels)
         {
            s->format_free(format);
            goto failed;
         }
      }
      else
      {
         static const char *const keys[] = {"width", "height", "profile",
               "color-standard", "color-transfer", "color-range"};
         int32_t expected[6], value;
         unsigned at;
         void *metadata = NULL;
         size_t size = 0;
         expected[0] = (int32_t)s->width;
         expected[1] = (int32_t)s->height;
         expected[2] = s->profile;
         expected[3] = s->color ? 6 : 1;
         expected[4] = s->color ? 6 : 3;
         expected[5] = (int32_t)s->range;
         for (at = 0; at < 6; at++)
         {
            if (!s->get_integer(format, keys[at], &value)
                  || (value != expected[at]
                        && !(at == 2 && s->color && value == s->base_profile)))
            {
               s->format_free(format);
               goto failed;
            }
         }
         if (s->color
               && (!s->get_buffer(format, "hdr-static-info", &metadata, &size)
                     || size != 25 || !metadata
                     || memcmp(metadata, s->hdr_static, 25)))
         {
            s->format_free(format);
            goto failed;
         }
      }
      if (s->format)
         s->format_free(s->format);
      s->format = format;
      return RECORD_MC_FORMAT;
   }
   if (index < 0)
      goto failed;
   s->index = (size_t)index;
   s->held = true;
   if (info.offset < 0 || info.size < 0 || (info.flags & ~7u)
         || (info.presentationTimeUs < 0
               && !(info.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG))
         || ((info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM)
               && !s->input_eos))
      goto failed;
   data = s->buffer(s->codec, s->index, &capacity);
   if ((size_t)info.offset > capacity
         || (size_t)info.size > capacity - (size_t)info.offset
         || (info.size && (!data || !s->format)))
      goto failed;
   s->packet.data = info.size ? data + info.offset : NULL;
   s->packet.size = (size_t)info.size;
   s->packet.pts_us = info.presentationTimeUs;
   s->packet.flags = info.flags;
   if (!info.size)
   {
      if (!record_mediacodec_encoder_release(s))
         return RECORD_MC_ERROR;
      return s->output_eos ? RECORD_MC_END : RECORD_MC_PROGRESS;
   }
   *packet = s->packet;
   return RECORD_MC_PACKET;
failed:
   s->failed = true;
   return RECORD_MC_ERROR;
}
#else
record_mediacodec_encoder_t *record_mediacodec_encoder_audio_new(
      unsigned rate, unsigned channels, unsigned bitrate)
{
   (void)rate;
   (void)channels;
   (void)bitrate;
   return NULL;
}
int record_mediacodec_encoder_audio_submit(record_mediacodec_encoder_t *s,
      const int16_t *samples, size_t frames, uint64_t timestamp,
      size_t *accepted)
{
   (void)s;
   (void)samples;
   (void)frames;
   (void)timestamp;
   if (accepted)
      *accepted = 0;
   return -1;
}
int record_mediacodec_encoder_audio_finish(
      record_mediacodec_encoder_t *s, uint64_t end_sample)
{
   (void)s;
   (void)end_sample;
   return -1;
}
record_mediacodec_encoder_t *record_mediacodec_encoder_new(JNIEnv *env,
      const mediacodec_encoder_request_t *request, unsigned interval)
{
   (void)env;
   (void)request;
   (void)interval;
   return NULL;
}
void *record_mediacodec_encoder_surface(record_mediacodec_encoder_t *s)
{
   (void)s;
   return NULL;
}
void *record_mediacodec_encoder_format(record_mediacodec_encoder_t *s)
{
   (void)s;
   return NULL;
}
bool record_mediacodec_encoder_audio_config(record_mediacodec_encoder_t *s,
      const void **data, size_t *bytes)
{
   (void)s;
   if (data)
      *data = NULL;
   if (bytes)
      *bytes = 0;
   return false;
}
int record_mediacodec_encoder_pump(
      record_mediacodec_encoder_t *s, record_mediacodec_packet_t *packet)
{
   (void)s;
   if (packet)
      memset(packet, 0, sizeof(*packet));
   return RECORD_MC_ERROR;
}
bool record_mediacodec_encoder_release(record_mediacodec_encoder_t *s)
{
   (void)s;
   return false;
}
bool record_mediacodec_encoder_finish(record_mediacodec_encoder_t *s)
{
   (void)s;
   return false;
}
bool record_mediacodec_encoder_stop(record_mediacodec_encoder_t *s)
{
   (void)s;
   return false;
}
void record_mediacodec_encoder_free(record_mediacodec_encoder_t *s)
{
   (void)s;
}
#endif
