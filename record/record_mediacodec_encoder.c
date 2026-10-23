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
#if defined(__ANDROID__) && defined(__clang__) && __ANDROID_API__ >= 29
#define RECORD_MC_ABI(type, symbol)                                            \
   __extension__ typedef char symbol##_record_abi                              \
         [__builtin_types_compatible_p(type, __typeof__(&symbol)) ? 1 : -1]
RECORD_MC_ABI(record_mc_new_fn, AMediaCodec_createCodecByName);
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
#undef RECORD_MC_ABI
#endif
struct record_mediacodec_encoder
{
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
   record_mediacodec_packet_t packet;
   size_t index;
   bool started, failed, held, input_eos, output_eos;
};
void record_mediacodec_encoder_free(record_mediacodec_encoder_t *s)
{
   if (!s)
      return;
   if (s->codec)
   {
      if (s->started)
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
   int matches;
   if (interval > INT_MAX
         || mediacodec_inventory_select_encoder(
                  env, request, selected, sizeof(selected))
               != 1)
      return NULL;
   s = (record_mediacodec_encoder_t *)calloc(1, sizeof(*s));
   if (!s)
      return NULL;
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
void *record_mediacodec_encoder_surface(record_mediacodec_encoder_t *s)
{
   return s && !s->failed && !s->input_eos ? s->surface : NULL;
}
void *record_mediacodec_encoder_format(record_mediacodec_encoder_t *s)
{
   return s && !s->failed ? s->format : NULL;
}
bool record_mediacodec_encoder_finish(record_mediacodec_encoder_t *s)
{
   if (!s || s->failed)
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
void record_mediacodec_encoder_free(record_mediacodec_encoder_t *s)
{
   (void)s;
}
#endif
