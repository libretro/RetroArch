/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
#include "record_mux_mediacodec.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#if defined(__ANDROID__) || defined(RECORD_MEDIACODEC_MUX_TEST)
#include <dynamic/dylib.h>
#ifdef RECORD_MEDIACODEC_MUX_TEST
#include "mediacodec_encode_test_types.h"
#else
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <media/NdkMediaMuxer.h>
#endif
typedef AMediaMuxer *(*record_mcm_new_fn)(int, OutputFormat);
typedef media_status_t (*record_mcm_action_fn)(AMediaMuxer *);
typedef ssize_t (*record_mcm_track_fn)(AMediaMuxer *, const AMediaFormat *);
typedef media_status_t (*record_mcm_write_fn)(
      AMediaMuxer *, size_t, const uint8_t *, const AMediaCodecBufferInfo *);
typedef bool (*record_mcm_string_fn)(
      AMediaFormat *, const char *, const char **);
#if defined(__ANDROID__) && defined(__clang__)
#define RECORD_MCM_ABI(type, symbol)                                           \
   __extension__ typedef char symbol##_record_abi                              \
         [__builtin_types_compatible_p(type, __typeof__(&symbol)) ? 1 : -1]
RECORD_MCM_ABI(record_mcm_new_fn, AMediaMuxer_new);
RECORD_MCM_ABI(record_mcm_action_fn, AMediaMuxer_start);
RECORD_MCM_ABI(record_mcm_action_fn, AMediaMuxer_stop);
RECORD_MCM_ABI(record_mcm_action_fn, AMediaMuxer_delete);
RECORD_MCM_ABI(record_mcm_track_fn, AMediaMuxer_addTrack);
RECORD_MCM_ABI(record_mcm_write_fn, AMediaMuxer_writeSampleData);
RECORD_MCM_ABI(record_mcm_string_fn, AMediaFormat_getString);
#undef RECORD_MCM_ABI
#endif
struct record_mux_mediacodec
{
   int64_t maximum[2], end[2];
   dylib_t library;
   AMediaMuxer *mux;
   record_mcm_action_fn start, stop, destroy;
   record_mcm_track_fn track;
   record_mcm_write_fn write;
   record_mcm_string_fn string;
   size_t index[2];
   bool audio, bound[2], seen[2], ended[2];
   bool started, stop_called, finished, failed;
};
void record_mux_mediacodec_free(record_mux_mediacodec_t *m)
{
   if (!m)
      return;
   if (m->mux)
   {
      if (m->started && !m->stop_called)
         m->stop(m->mux);
      m->destroy(m->mux);
   }
   if (m->library)
      dylib_close(m->library);
   free(m);
}
record_mux_mediacodec_t *record_mux_mediacodec_new(int fd, bool audio)
{
   record_mux_mediacodec_t *m;
   record_mcm_new_fn make;
   if (fd < 0)
      return NULL;
   m = (record_mux_mediacodec_t *)calloc(1, sizeof(*m));
   if (!m)
      return NULL;
   m->audio = audio;
   m->library = dylib_load("libmediandk.so");
   if (!m->library)
      goto failed;
#define RECORD_MCM_LOAD(target, type, symbol)                                  \
   do                                                                          \
   {                                                                           \
      target = (type)dylib_proc(m->library, symbol);                           \
      if (!target)                                                             \
         goto failed;                                                          \
   } while (0)
   RECORD_MCM_LOAD(make, record_mcm_new_fn, "AMediaMuxer_new");
   RECORD_MCM_LOAD(m->start, record_mcm_action_fn, "AMediaMuxer_start");
   RECORD_MCM_LOAD(m->stop, record_mcm_action_fn, "AMediaMuxer_stop");
   RECORD_MCM_LOAD(m->destroy, record_mcm_action_fn, "AMediaMuxer_delete");
   RECORD_MCM_LOAD(m->track, record_mcm_track_fn, "AMediaMuxer_addTrack");
   RECORD_MCM_LOAD(
         m->write, record_mcm_write_fn, "AMediaMuxer_writeSampleData");
   RECORD_MCM_LOAD(m->string, record_mcm_string_fn, "AMediaFormat_getString");
#undef RECORD_MCM_LOAD
   m->mux = make(fd, AMEDIAMUXER_OUTPUT_FORMAT_MPEG_4);
   if (!m->mux)
      goto failed;
   return m;
failed:
   record_mux_mediacodec_free(m);
   return NULL;
}
bool record_mux_mediacodec_track(
      record_mux_mediacodec_t *m, bool audio, void *format)
{
   unsigned at = audio ? 1 : 0;
   const char *mime = NULL;
   ssize_t index;
   if (!m || m->failed || m->started || m->bound[at] || (audio && !m->audio))
      return false;
   if (!format || !m->string((AMediaFormat *)format, "mime", &mime) || !mime)
      goto failed;
   if (audio ? strcmp(mime, "audio/mp4a-latm")
             : strcmp(mime, "video/avc") && strcmp(mime, "video/hevc")
                     && strcmp(mime, "video/av01"))
      goto failed;
   index = m->track(m->mux, (const AMediaFormat *)format);
   if (index < 0 || (m->bound[at ^ 1] && m->index[at ^ 1] == (size_t)index))
      goto failed;
   m->index[at] = (size_t)index;
   m->bound[at] = true;
   return true;
failed:
   m->failed = true;
   return false;
}
int record_mux_mediacodec_start(record_mux_mediacodec_t *m)
{
   if (!m || m->failed || m->finished)
      return -1;
   if (m->started)
      return 1;
   if (!m->bound[0] || (m->audio && !m->bound[1]))
      return 0;
   m->started = true;
   if (m->start(m->mux) != AMEDIA_OK)
   {
      m->failed = true;
      return -1;
   }
   return 1;
}
bool record_mux_mediacodec_write(record_mux_mediacodec_t *m, bool audio,
      const record_mediacodec_packet_t *packet)
{
   unsigned at = audio ? 1 : 0;
   AMediaCodecBufferInfo info;
   if (!m || m->failed || !m->started || m->finished || !m->bound[at]
         || m->ended[at])
      return false;
   if (!packet || (packet->size && !packet->data) || packet->size > INT_MAX
         || (packet->flags & ~7u))
      goto failed;
   if (packet->flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG)
      return true;
   if (!packet->size || packet->pts_us < 0
         || (!audio && !m->seen[at] && !(packet->flags & 1)))
      goto failed;
   memset(&info, 0, sizeof(info));
   info.size = (int32_t)packet->size;
   info.presentationTimeUs = packet->pts_us;
   info.flags = packet->flags & 1;
   if (m->write(m->mux, m->index[at], packet->data, &info) != AMEDIA_OK)
      goto failed;
   if (!m->seen[at] || packet->pts_us > m->maximum[at])
      m->maximum[at] = packet->pts_us;
   m->seen[at] = true;
   return true;
failed:
   m->failed = true;
   return false;
}
bool record_mux_mediacodec_end(
      record_mux_mediacodec_t *m, bool audio, int64_t end_us)
{
   static const uint8_t empty = 0;
   unsigned at = audio ? 1 : 0;
   AMediaCodecBufferInfo info;
   if (!m || m->failed || !m->started || m->finished || !m->bound[at])
      return false;
   if (m->ended[at])
      return m->end[at] == end_us;
   if (!m->seen[at] || end_us <= m->maximum[at])
      goto failed;
   memset(&info, 0, sizeof(info));
   info.presentationTimeUs = end_us;
   info.flags = AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM;
   if (m->write(m->mux, m->index[at], &empty, &info) != AMEDIA_OK)
      goto failed;
   m->end[at] = end_us;
   m->ended[at] = true;
   return true;
failed:
   m->failed = true;
   return false;
}
int record_mux_mediacodec_finish(record_mux_mediacodec_t *m)
{
   if (!m || m->failed)
      return -1;
   if (m->finished)
      return 1;
   if (!m->started || !m->ended[0] || (m->audio && !m->ended[1]))
      return 0;
   m->stop_called = true;
   if (m->stop(m->mux) != AMEDIA_OK)
   {
      m->failed = true;
      return -1;
   }
   m->finished = true;
   return 1;
}
#else
record_mux_mediacodec_t *record_mux_mediacodec_new(int fd, bool audio)
{
   (void)fd;
   (void)audio;
   return NULL;
}
bool record_mux_mediacodec_track(
      record_mux_mediacodec_t *m, bool audio, void *format)
{
   (void)m;
   (void)audio;
   (void)format;
   return false;
}
int record_mux_mediacodec_start(record_mux_mediacodec_t *m)
{
   (void)m;
   return -1;
}
bool record_mux_mediacodec_write(record_mux_mediacodec_t *m, bool audio,
      const record_mediacodec_packet_t *packet)
{
   (void)m;
   (void)audio;
   (void)packet;
   return false;
}
bool record_mux_mediacodec_end(
      record_mux_mediacodec_t *m, bool audio, int64_t end_us)
{
   (void)m;
   (void)audio;
   (void)end_us;
   return false;
}
int record_mux_mediacodec_finish(record_mux_mediacodec_t *m)
{
   (void)m;
   return -1;
}
void record_mux_mediacodec_free(record_mux_mediacodec_t *m)
{
   (void)m;
}
#endif
