/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------
 * The following license statement only applies to this file (task_subtitle.c).
 * ---------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <retro_atomic.h>
#include <retro_dirent.h>
#include <string/stdstring.h>
#include <queues/task_queue.h>
#include <streams/file_stream.h>
#include "task_subtitle.h"
#include "tasks_internal.h"

#if defined(HAVE_RMP4) || defined(HAVE_RWEBM)
#include <formats/data_transfer.h>
#endif

#ifdef HAVE_RMP4
#include <formats/rmp4.h>

struct subtitle_mp4
{
   data_transfer_t *window;
   rmp4_t *demux;
   rsubtitle_srt_span_t *spans;
   char *text;
   size_t length, avail, need_lo, need_hi, admitted;
   size_t used, capacity, count, span_capacity;
   uint32_t sample, samples;
};
#endif

#ifdef HAVE_RWEBM
#include <formats/rwebm.h>

struct subtitle_webm
{
   data_transfer_t *window;
   rwebm_t *demux;
   rsubtitle_srt_span_t *spans;
   char *text;
   size_t length, avail, need_hi, feed_from;
   size_t used, capacity, count, span_capacity;
};
#endif

struct task_subtitle_request
{
   char *path, *input;
   RFILE *file;
   rsubtitle_srt_t *track;
   size_t size, done;
   int embedded_track;
   enum task_subtitle_source source_kind;
#ifdef HAVE_RMP4
   struct subtitle_mp4 *mp4;
#endif
#ifdef HAVE_RWEBM
   struct subtitle_webm *webm;
#endif
   retro_atomic_int_t refs, cancelled, status;
};

#ifdef HAVE_RWEBM
static void subtitle_webm_free(struct subtitle_webm *webm)
{
   if (!webm) return;
   rwebm_close(webm->demux);
   data_transfer_free(webm->window);
   free(webm->spans);
   free(webm->text);
   free(webm);
}

static int subtitle_webm_open(struct subtitle_webm *webm, const char *path)
{
   const uint8_t *base;
   int more = 0;
   size_t end;
   if (!webm->window)
   {
      RFILE *file = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);
      int64_t size = file ? filestream_get_size(file) : -1;
      if (file) filestream_close(file);
      if (size <= 0 || (uint64_t)size > (size_t)-1
            || (!data_transfer_reserve_supported()
               && (uint64_t)size > RSUBTITLE_SRT_MAX_INPUT))
         return -1;
      webm->window = data_transfer_open_window(path, 4096);
      if (!webm->window) return -1;
      base = data_transfer_window_base(webm->window, &webm->length);
      if (!base || !webm->length) return -1;
      webm->avail = webm->length < 4096 ? webm->length : 4096;
      return 0;
   }
   base = data_transfer_window_base(webm->window, NULL);
   webm->demux = rwebm_open_memory_avail(base, webm->length, webm->avail, &more);
   if (webm->demux)
      return data_transfer_window_grow_keep(webm->window,
            rwebm_media_floor(webm->demux)) ? 1 : -1;
   if (!more || webm->avail == webm->length
         || webm->avail >= RSUBTITLE_SRT_MAX_INPUT)
      return -1;
   end = webm->avail + 65536;
   if (end > RSUBTITLE_SRT_MAX_INPUT) end = RSUBTITLE_SRT_MAX_INPUT;
   if (end > webm->length) end = webm->length;
   if (!data_transfer_window_extend(webm->window, end)) return -1;
   webm->avail = end;
   return 0;
}

static int subtitle_webm_step(task_subtitle_request_t *request)
{
   struct subtitle_webm *webm = request->webm;
   const rwebm_track *track;
   rwebm_packet packet;
   int result;
   int64_t first, last, duration;
   uint64_t limit = (~(uint64_t)0) >> 1;
   size_t lo, hi, end, i, written;
   if (!webm)
   {
      request->webm = (struct subtitle_webm*)calloc(1, sizeof(*webm));
      return request->webm ? 0 : -1;
   }
   if (!webm->demux)
   {
      result = subtitle_webm_open(webm, request->path);
      if (result != 1) return result;
      track = rwebm_get_track(webm->demux, request->embedded_track);
      if (!track || track->type != RWEBM_TRACK_OTHER
            || strcmp(track->codec_id, "S_TEXT/UTF8"))
         return -1;
      return 0;
   }
   if (webm->need_hi)
   {
      data_transfer_window_skip(webm->window, webm->feed_from);
      data_transfer_window_advance(webm->window, webm->feed_from);
      if (webm->avail < webm->feed_from) webm->avail = webm->feed_from;
      end = webm->need_hi - webm->avail > 65536
         ? webm->avail + 65536 : webm->need_hi;
      if (!data_transfer_window_extend(webm->window, end)) return -1;
      webm->avail = end;
      rwebm_set_avail(webm->demux, end);
      if (end == webm->need_hi) webm->need_hi = 0;
      return 0;
   }
   result = rwebm_read_packet_track(webm->demux, request->embedded_track, &packet);
   if (result == RWEBM_READ_YIELD) return 0;
   if (result == RWEBM_READ_AGAIN)
   {
      webm->feed_from = rwebm_consumed(webm->demux);
      rwebm_read_span(webm->demux, &lo, &hi);
      if (!hi)
      {
         hi = webm->avail > webm->feed_from ? webm->avail : webm->feed_from;
         hi = webm->length - hi < 64 ? webm->length : hi + 64;
      }
      if (hi <= webm->avail || hi > webm->length
            || hi < webm->feed_from
            || hi - webm->feed_from > RSUBTITLE_SRT_MAX_INPUT)
         return -1;
      webm->need_hi = hi;
      return 0;
   }
   if (result < 0) return -1;
   if (!result)
   {
      char *text = webm->text;
      rsubtitle_srt_span_t *spans = webm->spans;
      webm->text = NULL; webm->spans = NULL;
      request->track = rsubtitle_srt_begin_cues(text, webm->used, spans, webm->count);
      return request->track ? 1 : -1;
   }
   if (packet.track != request->embedded_track || !packet.size) return 0;
   track = rwebm_get_track(webm->demux, request->embedded_track);
   duration = packet.duration_ns;
   if (duration == -1 && track->default_duration_ns <= limit)
      duration = (int64_t)track->default_duration_ns;
   if (duration < 0 || (!duration && packet.duration_ns == -1)
         || packet.timestamp_continuation || packet.size > 65536
         || track->codec_delay_ns > limit
         || packet.timestamp < -(int64_t)limit + (int64_t)track->codec_delay_ns)
      return -1;
   first = packet.timestamp - (int64_t)track->codec_delay_ns;
   if (first > (int64_t)limit - duration) return -1;
   last = first + duration;
   if (first < 0) first = 0;
   if (last / 1000 <= first / 1000) return 0;
   if (webm->count == 65536 || webm->used > RSUBTITLE_SRT_MAX_INPUT - packet.size - 1)
      return -1;
   end = webm->used + packet.size + 1;
   if (end > webm->capacity)
   {
      size_t capacity = webm->capacity ? webm->capacity * 2 : 65536;
      char *text;
      if (capacity < end) capacity = end;
      if (capacity > RSUBTITLE_SRT_MAX_INPUT) capacity = RSUBTITLE_SRT_MAX_INPUT;
      text = (char*)realloc(webm->text, capacity);
      if (!text) return -1;
      webm->text = text; webm->capacity = capacity;
   }
   if (webm->count == webm->span_capacity)
   {
      size_t capacity = webm->span_capacity ? webm->span_capacity * 2 : 64;
      rsubtitle_srt_span_t *spans = (rsubtitle_srt_span_t*)realloc(webm->spans,
            capacity * sizeof(*spans));
      if (!spans) return -1;
      webm->spans = spans; webm->span_capacity = capacity;
   }
   for (i = 0, written = 0; i < packet.size; i++)
   {
      char c = (char)packet.data[i];
      if (c == '\r')
      {
         c = '\n';
         if (i + 1 < packet.size && packet.data[i + 1] == '\n') i++;
      }
      webm->text[webm->used + written++] = c;
   }
   webm->text[webm->used + written] = 0;
   webm->spans[webm->count].start_us = first / 1000;
   webm->spans[webm->count].end_us = last / 1000;
   webm->spans[webm->count].offset = webm->used;
   webm->spans[webm->count++].length = written;
   webm->used += written + 1;
   return 0;
}
#endif

#ifdef HAVE_RMP4
static void subtitle_mp4_free(struct subtitle_mp4 *mp4)
{
   if (!mp4) return;
   rmp4_close(mp4->demux);
   data_transfer_free(mp4->window);
   free(mp4->spans);
   free(mp4->text);
   free(mp4);
}

static int subtitle_mp4_open(struct subtitle_mp4 *mp4, const char *path)
{
   const uint8_t *base;
   if (!mp4->window)
   {
      RFILE *file = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);
      int64_t size = file ? filestream_get_size(file) : -1;
      if (file) filestream_close(file);
      if (size <= 0 || (uint64_t)size > (size_t)-1
            || (!data_transfer_reserve_supported()
               && (uint64_t)size > RSUBTITLE_SRT_MAX_INPUT))
         return -1;
      mp4->window = data_transfer_open_window(path, 4096);
      if (!mp4->window) return -1;
      base = data_transfer_window_base(mp4->window, &mp4->length);
      if (!base || !mp4->length) return -1;
      mp4->avail = mp4->length < 4096 ? mp4->length : 4096;
      mp4->admitted = mp4->avail;
      return 0;
   }
   if (mp4->need_hi > mp4->need_lo)
   {
      size_t end = mp4->need_hi;
      if (end - mp4->need_lo > 65536) end = mp4->need_lo + 65536;
      if (mp4->admitted > RSUBTITLE_SRT_MAX_INPUT - (end - mp4->need_lo)
            || !data_transfer_window_ensure(mp4->window, mp4->need_lo, end))
         return -1;
      mp4->admitted += end - mp4->need_lo;
      mp4->need_lo = end;
      if (end < mp4->need_hi) return 0;
      mp4->avail = end;
   }
   {
      int more = 0;
      base = data_transfer_window_base(mp4->window, NULL);
      mp4->demux = rmp4_open_memory_avail(base, mp4->length, mp4->avail,
            &more, &mp4->need_lo, &mp4->need_hi);
      if (mp4->demux) return 1;
      if (!more || mp4->need_hi <= mp4->need_lo
            || mp4->need_hi > mp4->length
            || mp4->need_hi - mp4->need_lo > RSUBTITLE_SRT_MAX_INPUT)
         return -1;
   }
   return 0;
}

static int subtitle_tx3g_text(struct subtitle_mp4 *mp4, size_t offset,
      size_t length, size_t *written)
{
   unsigned char prefix[2], *encoded = NULL;
   char *out;
   size_t i, n, used = 0, room;
   bool little;
   if (length < 2 || !data_transfer_window_peek(mp4->window, offset, prefix, 2))
      return -1;
   n = ((size_t)prefix[0] << 8) | prefix[1];
   if (n > length - 2) return -1;
   if (!n) { *written = 0; return 1; }
   room = n * 2 + 1;
   if (mp4->used > RSUBTITLE_SRT_MAX_INPUT - room) return -1;
   if (mp4->used + room > mp4->capacity)
   {
      size_t capacity = mp4->capacity ? mp4->capacity * 2 : 65536;
      if (capacity < mp4->used + room) capacity = mp4->used + room;
      if (capacity > RSUBTITLE_SRT_MAX_INPUT) capacity = RSUBTITLE_SRT_MAX_INPUT;
      out = (char*)realloc(mp4->text, capacity);
      if (!out) return -1;
      mp4->text = out; mp4->capacity = capacity;
   }
   out = mp4->text + mp4->used;
   if (!data_transfer_window_peek(mp4->window, offset + 2, out, n)) return -1;
   little = n >= 2 && (unsigned char)out[0] == 0xff && (unsigned char)out[1] == 0xfe;
   if (little || (n >= 2 && (unsigned char)out[0] == 0xfe && (unsigned char)out[1] == 0xff))
   {
      if (n & 1) return -1;
      encoded = (unsigned char*)malloc(n);
      if (!encoded) return -1;
      memcpy(encoded, out, n);
      for (i = 2; i < n; i += 2)
      {
         unsigned scalar = little ? encoded[i] | (encoded[i + 1] << 8)
            : (encoded[i] << 8) | encoded[i + 1];
         if (scalar >= 0xd800 && scalar <= 0xdbff)
         {
            unsigned low;
            if (n - i < 4) { free(encoded); return -1; }
            i += 2;
            low = little ? encoded[i] | (encoded[i + 1] << 8)
               : (encoded[i] << 8) | encoded[i + 1];
            if (low < 0xdc00 || low > 0xdfff) { free(encoded); return -1; }
            scalar = 0x10000 + ((scalar - 0xd800) << 10) + low - 0xdc00;
         }
         else if (scalar >= 0xdc00 && scalar <= 0xdfff)
         { free(encoded); return -1; }
         if (scalar < 0x80) out[used++] = (char)scalar;
         else if (scalar < 0x800)
         {
            out[used++] = (char)(0xc0 | (scalar >> 6));
            out[used++] = (char)(0x80 | (scalar & 63));
         }
         else if (scalar < 0x10000)
         {
            out[used++] = (char)(0xe0 | (scalar >> 12));
            out[used++] = (char)(0x80 | ((scalar >> 6) & 63));
            out[used++] = (char)(0x80 | (scalar & 63));
         }
         else
         {
            out[used++] = (char)(0xf0 | (scalar >> 18));
            out[used++] = (char)(0x80 | ((scalar >> 12) & 63));
            out[used++] = (char)(0x80 | ((scalar >> 6) & 63));
            out[used++] = (char)(0x80 | (scalar & 63));
         }
      }
      free(encoded);
   }
   else used = n;
   if (used > 65536) return -1;
   n = used; used = 0;
   for (i = 0; i < n; i++)
   {
      if (out[i] == '\r')
      {
         out[used++] = '\n';
         if (i + 1 < n && out[i + 1] == '\n') i++;
      }
      else out[used++] = out[i];
   }
   out[used] = 0;
   *written = used;
   return 1;
}

static int subtitle_mp4_step(task_subtitle_request_t *request)
{
   struct subtitle_mp4 *mp4 = request->mp4;
   size_t lo, hi, written;
   int64_t first, last;
   if (!mp4)
   {
      request->mp4 = (struct subtitle_mp4*)calloc(1, sizeof(*mp4));
      return request->mp4 ? 0 : -1;
   }
   if (!mp4->demux)
   {
      const rmp4_track *track;
      int result = subtitle_mp4_open(mp4, request->path);
      if (result != 1) return result;
      track = rmp4_get_track(mp4->demux, request->embedded_track);
      if (!track || track->type != RMP4_TRACK_OTHER || strcmp(track->codec_id, "tx3g")
            || !rmp4_track_pts(mp4->demux, request->embedded_track, &mp4->samples)
            || mp4->samples > 65536)
         return -1;
      return 0;
   }
   if (mp4->sample == mp4->samples)
   {
      char *text = mp4->text;
      rsubtitle_srt_span_t *spans = mp4->spans;
      mp4->text = NULL; mp4->spans = NULL;
      request->track = rsubtitle_srt_begin_cues(text, mp4->used, spans, mp4->count);
      return request->track ? 1 : -1;
   }
   if (!rmp4_track_sample_span(mp4->demux, request->embedded_track,
            mp4->sample, &lo, &hi)
         || !rmp4_track_text_time(mp4->demux, request->embedded_track,
            mp4->sample, &first, &last))
      return -1;
   mp4->sample++;
   if (last / 1000 <= first / 1000) return 0;
   if (subtitle_tx3g_text(mp4, lo, hi - lo, &written) < 0) return -1;
   if (!written) return 0;
   if (mp4->count == mp4->span_capacity)
   {
      size_t capacity = mp4->span_capacity ? mp4->span_capacity * 2 : 64;
      rsubtitle_srt_span_t *spans = (rsubtitle_srt_span_t*)realloc(mp4->spans,
            capacity * sizeof(*spans));
      if (!spans) return -1;
      mp4->spans = spans; mp4->span_capacity = capacity;
   }
   mp4->spans[mp4->count].start_us = first / 1000;
   mp4->spans[mp4->count].end_us = last / 1000;
   mp4->spans[mp4->count].offset = mp4->used;
   mp4->spans[mp4->count++].length = written;
   mp4->used += written + 1;
   return 0;
}
#endif

static void subtitle_task_close(task_subtitle_request_t *request)
{
   if (request->file)
      filestream_close(request->file);
   request->file = NULL;
#ifdef HAVE_RMP4
   subtitle_mp4_free(request->mp4);
   request->mp4 = NULL;
#endif
#ifdef HAVE_RWEBM
   subtitle_webm_free(request->webm);
   request->webm = NULL;
#endif
   free(request->input);
   request->input = NULL;
}

static void subtitle_task_unref(task_subtitle_request_t *request)
{
   if (retro_atomic_fetch_sub_int(&request->refs, 1) != 1)
      return;
   rsubtitle_srt_free(request->track);
   free(request->path);
   free(request);
}

static void subtitle_task_finish(retro_task_t *task, int status)
{
   task_subtitle_request_t *request = (task_subtitle_request_t*)task->state;
   subtitle_task_close(request);
   if (status < 0)
   {
      rsubtitle_srt_free(request->track);
      request->track = NULL;
   }
   retro_atomic_store_release_int(&request->status, status);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void subtitle_task_handler(retro_task_t *task)
{
   task_subtitle_request_t *request = (task_subtitle_request_t*)task->state;
   if (retro_atomic_load_acquire_int(&request->cancelled)
         || (task_get_flags(task) & RETRO_TASK_FLG_CANCELLED))
   {
      subtitle_task_finish(task, -1);
      return;
   }
   if (request->track)
   {
      nbio_budget_t budget;
      int status = 0;
      task_nbio_slice_open(&budget);
      while (!status && task_nbio_slice_within_budget(&budget, 0, 0))
      {
         if (retro_atomic_load_acquire_int(&request->cancelled))
            status = -1;
         else
            status = rsubtitle_srt_iterate(request->track, 4096);
      }
      task_nbio_slice_close(&budget);
      if (status)
         subtitle_task_finish(task, status);
      return;
   }
   if (request->source_kind == TASK_SUBTITLE_WEBM)
   {
#ifdef HAVE_RWEBM
      int status = subtitle_webm_step(request);
      if (status < 0) subtitle_task_finish(task, -1);
      else if (status > 0) subtitle_task_close(request);
#else
      subtitle_task_finish(task, -1);
#endif
      return;
   }
   if (request->embedded_track >= 0)
   {
#ifdef HAVE_RMP4
      int status = subtitle_mp4_step(request);
      if (status < 0) subtitle_task_finish(task, -1);
      else if (status > 0) subtitle_task_close(request);
#else
      subtitle_task_finish(task, -1);
#endif
      return;
   }
   if (!request->file)
   {
      int64_t size;
      request->file = filestream_open(request->path,
            RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
      size = request->file ? filestream_get_size(request->file) : -1;
      if (size <= 0 || (uint64_t)size > RSUBTITLE_SRT_MAX_INPUT)
      {
         subtitle_task_finish(task, -1);
         return;
      }
      request->size = (size_t)size;
      request->input = (char*)malloc(request->size + 1);
      if (!request->input)
         subtitle_task_finish(task, -1);
      return;
   }
   {
      size_t bytes = request->size - request->done;
      int64_t got;
      if (bytes > 65536)
         bytes = 65536;
      got = filestream_read(request->file, request->input + request->done, bytes);
      if (got <= 0 || (uint64_t)got > bytes)
      {
         subtitle_task_finish(task, -1);
         return;
      }
      request->done += (size_t)got;
      if (request->done == request->size)
      {
         char *input = request->input;
         request->input = NULL;
         subtitle_task_close(request);
         request->track = rsubtitle_srt_begin(input, request->size);
         if (!request->track)
            subtitle_task_finish(task, -1);
      }
   }
}

static void subtitle_task_cleanup(retro_task_t *task)
{
   task_subtitle_request_t *request = (task_subtitle_request_t*)task->state;
   if (!retro_atomic_load_acquire_int(&request->status))
   {
      subtitle_task_close(request);
      rsubtitle_srt_free(request->track);
      request->track = NULL;
      retro_atomic_store_release_int(&request->status, -1);
   }
   subtitle_task_unref(request);
}

static task_subtitle_request_t *subtitle_task_create(const char *path, int embedded_track,
      enum task_subtitle_source source_kind)
{
   task_subtitle_request_t *request;
   retro_task_t *task;
   size_t length;
   if (!path || !*path)
      return NULL;
   length = strlen(path);
   if (length == (size_t)-1)
      return NULL;
   request = (task_subtitle_request_t*)calloc(1, sizeof(*request));
   if (!request)
      return NULL;
   request->path = (char*)malloc(length + 1);
   if (!request->path)
   {
      free(request);
      return NULL;
   }
   memcpy(request->path, path, length + 1);
   request->embedded_track = embedded_track;
   request->source_kind = source_kind;
   task = task_init();
   if (!task)
   {
      free(request->path);
      free(request);
      return NULL;
   }
   retro_atomic_int_init(&request->refs, 2);
   retro_atomic_int_init(&request->cancelled, 0);
   retro_atomic_int_init(&request->status, 0);
   task->state = request;
   task->handler = subtitle_task_handler;
   task->cleanup = subtitle_task_cleanup;
   task_set_flags(task, RETRO_TASK_FLG_MUTE, true);
   if (!task_queue_push(task))
   {
      free(request->path);
      free(request);
      free(task);
      return NULL;
   }
   return request;
}

task_subtitle_request_t *task_subtitle_load(const char *path)
{
   return subtitle_task_create(path, -1, TASK_SUBTITLE_SIDECAR);
}

task_subtitle_request_t *task_subtitle_load_embedded(const char *path, int track)
{
   return track >= 0 ? subtitle_task_create(path, track, TASK_SUBTITLE_MP4) : NULL;
}

task_subtitle_request_t *task_subtitle_load_entry(const task_subtitle_entry_t *entry)
{
   if (!entry) return NULL;
   if (entry->source == TASK_SUBTITLE_SIDECAR)
      return task_subtitle_load(entry->path);
   if (entry->source == TASK_SUBTITLE_WEBM && entry->track >= 0)
      return subtitle_task_create(entry->path, entry->track, TASK_SUBTITLE_WEBM);
   if (entry->source == TASK_SUBTITLE_MP4)
      return task_subtitle_load_embedded(entry->path, entry->track);
   return NULL;
}

int task_subtitle_status(const task_subtitle_request_t *request)
{
   return request ? retro_atomic_load_acquire_int(&request->status) : -1;
}

rsubtitle_srt_t *task_subtitle_take(task_subtitle_request_t *request)
{
   rsubtitle_srt_t *track;
   if (task_subtitle_status(request) != 1)
      return NULL;
   track = request->track;
   request->track = NULL;
   retro_atomic_store_release_int(&request->status, 2);
   return track;
}

void task_subtitle_release(task_subtitle_request_t *request)
{
   if (!request)
      return;
   retro_atomic_store_release_int(&request->cancelled, 1);
   subtitle_task_unref(request);
}

#define SUBTITLE_CATALOG_LIMIT 64

struct task_subtitle_catalog
{
   struct RDIR *directory;
   char *folder, *stem, *movie;
   task_subtitle_entry_t entries[SUBTITLE_CATALOG_LIMIT];
   unsigned embedded_cursor;
   bool directory_done;
#ifdef HAVE_RMP4
   struct subtitle_mp4 *mp4;
#endif
#ifdef HAVE_RWEBM
   struct subtitle_webm *webm;
#endif
   size_t count, stem_length;
   retro_atomic_int_t refs, cancelled, status;
};

static void subtitle_catalog_unref(task_subtitle_catalog_t *catalog)
{
   size_t i;
   if (retro_atomic_fetch_sub_int(&catalog->refs, 1) != 1)
      return;
   for (i = 0; i < catalog->count; i++)
      free((void*)catalog->entries[i].path);
   free(catalog->folder);
   free(catalog->stem);
   free(catalog->movie);
   free(catalog);
}

static void subtitle_catalog_finish(retro_task_t *task, int status)
{
   task_subtitle_catalog_t *catalog = (task_subtitle_catalog_t*)task->state;
   retro_closedir(catalog->directory);
   catalog->directory = NULL;
#ifdef HAVE_RMP4
   subtitle_mp4_free(catalog->mp4);
   catalog->mp4 = NULL;
#endif
#ifdef HAVE_RWEBM
   subtitle_webm_free(catalog->webm);
   catalog->webm = NULL;
#endif
   retro_atomic_store_release_int(&catalog->status, status);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static bool subtitle_catalog_add(task_subtitle_catalog_t *catalog,
      const char *name)
{
   size_t length = strlen(name), folder_length, index, i;
   char *path;
   if (length < catalog->stem_length + 4
#if defined(_WIN32)
         || !string_starts_with_case_insensitive(name, catalog->stem)
#else
         || memcmp(name, catalog->stem, catalog->stem_length)
#endif
         || name[catalog->stem_length] != '.'
         || !string_is_equal_noncase(name + length - 4, ".srt"))
      return true;
   if (retro_dirent_is_dir(catalog->directory, NULL))
      return true;
   folder_length = strlen(catalog->folder);
   if (length > 32768 - folder_length)
      return true;
   path = (char*)malloc(folder_length + length + 1);
   if (!path)
      return false;
   memcpy(path, catalog->folder, folder_length);
   memcpy(path + folder_length, name, length + 1);
   for (index = 0; index < catalog->count; index++)
      if (strcmp(path, catalog->entries[index].path) < 0)
         break;
   if (index == SUBTITLE_CATALOG_LIMIT)
   {
      free(path);
      return true;
   }
   if (catalog->count == SUBTITLE_CATALOG_LIMIT)
      free((void*)catalog->entries[catalog->count - 1].path);
   else
      catalog->count++;
   for (i = catalog->count - 1; i > index; i--)
      catalog->entries[i] = catalog->entries[i - 1];
   catalog->entries[index].path = path;
   catalog->entries[index].source = TASK_SUBTITLE_SIDECAR;
   catalog->entries[index].track = -1;
   return true;
}

#ifdef HAVE_RWEBM
static int subtitle_catalog_webm(task_subtitle_catalog_t *catalog)
{
   const rwebm_track *track;
   char *path;
   size_t length;
   if (!catalog->webm)
   {
      catalog->webm = (struct subtitle_webm*)calloc(1, sizeof(*catalog->webm));
      return catalog->webm ? 0 : -1;
   }
   if (!catalog->webm->demux)
      return subtitle_webm_open(catalog->webm, catalog->movie) < 0 ? 1 : 0;
   if (catalog->embedded_cursor >= (unsigned)rwebm_num_tracks(catalog->webm->demux)
         || catalog->count == SUBTITLE_CATALOG_LIMIT)
      return 1;
   track = rwebm_get_track(catalog->webm->demux, catalog->embedded_cursor);
   if (track && track->type == RWEBM_TRACK_OTHER && !strcmp(track->codec_id, "S_TEXT/UTF8"))
   {
      length = strlen(catalog->movie);
      path = (char*)malloc(length + 1);
      if (!path) return -1;
      memcpy(path, catalog->movie, length + 1);
      catalog->entries[catalog->count].path = path;
      catalog->entries[catalog->count].source = TASK_SUBTITLE_WEBM;
      catalog->entries[catalog->count++].track = (int)catalog->embedded_cursor;
   }
   catalog->embedded_cursor++;
   return 0;
}
#endif

static int subtitle_catalog_embedded(task_subtitle_catalog_t *catalog)
{
#ifdef HAVE_RMP4
   const rmp4_track *track;
   const char *extension = strrchr(catalog->movie, '.');
   int result;
   char *path;
   size_t length;
#endif
#ifdef HAVE_RWEBM
   const char *webm_extension = strrchr(catalog->movie, '.');
   if (webm_extension && (string_is_equal_noncase(webm_extension, ".webm")
            || string_is_equal_noncase(webm_extension, ".mkv")))
      return subtitle_catalog_webm(catalog);
#endif
#ifdef HAVE_RMP4
   if (!extension || (!string_is_equal_noncase(extension, ".mp4")
            && !string_is_equal_noncase(extension, ".mov")
            && !string_is_equal_noncase(extension, ".m4v")))
      return 1;
   if (!catalog->mp4)
   {
      catalog->mp4 = (struct subtitle_mp4*)calloc(1, sizeof(*catalog->mp4));
      return catalog->mp4 ? 0 : -1;
   }
   if (!catalog->mp4->demux)
   {
      result = subtitle_mp4_open(catalog->mp4, catalog->movie);
      return result < 0 ? 1 : 0;
   }
   if (catalog->embedded_cursor >= (unsigned)rmp4_num_tracks(catalog->mp4->demux)
         || catalog->count == SUBTITLE_CATALOG_LIMIT)
      return 1;
   track = rmp4_get_track(catalog->mp4->demux, catalog->embedded_cursor);
   if (track && track->type == RMP4_TRACK_OTHER && !strcmp(track->codec_id, "tx3g"))
   {
      length = strlen(catalog->movie);
      path = (char*)malloc(length + 1);
      if (!path) return -1;
      memcpy(path, catalog->movie, length + 1);
      catalog->entries[catalog->count].path = path;
      catalog->entries[catalog->count].source = TASK_SUBTITLE_MP4;
      catalog->entries[catalog->count++].track = (int)catalog->embedded_cursor;
   }
   catalog->embedded_cursor++;
   return 0;
#else
   (void)catalog;
   return 1;
#endif
}

static void subtitle_catalog_handler(retro_task_t *task)
{
   task_subtitle_catalog_t *catalog = (task_subtitle_catalog_t*)task->state;
   nbio_budget_t budget;
   unsigned entries = 0;
   int status = 0;
   if (retro_atomic_load_acquire_int(&catalog->cancelled)
         || (task_get_flags(task) & RETRO_TASK_FLG_CANCELLED))
   {
      subtitle_catalog_finish(task, -1);
      return;
   }
   if (catalog->directory_done)
   {
      status = subtitle_catalog_embedded(catalog);
      if (status) subtitle_catalog_finish(task, status);
      return;
   }
   if (!catalog->directory)
   {
      if (retro_atomic_load_acquire_int(&catalog->cancelled))
      {
         subtitle_catalog_finish(task, -1);
         return;
      }
      catalog->directory = retro_opendir(*catalog->folder ? catalog->folder : ".");
      if (!catalog->directory)
      {
         catalog->directory_done = true;
         return;
      }
   }
   task_nbio_slice_open(&budget);
   while (entries++ < 32 && task_nbio_slice_within_budget(&budget, 0, 0))
   {
      const char *name;
      if (retro_atomic_load_acquire_int(&catalog->cancelled)
            || (task_get_flags(task) & RETRO_TASK_FLG_CANCELLED))
      {
         status = -1;
         break;
      }
      if (!retro_readdir(catalog->directory))
      {
         status = 1;
         break;
      }
      name = retro_dirent_get_name(catalog->directory);
      if (name && !subtitle_catalog_add(catalog, name))
      {
         status = -1;
         break;
      }
   }
   task_nbio_slice_close(&budget);
   if (status == 1)
   {
      retro_closedir(catalog->directory);
      catalog->directory = NULL;
      catalog->directory_done = true;
   }
   else if (status < 0)
      subtitle_catalog_finish(task, status);
}

static void subtitle_catalog_cleanup(retro_task_t *task)
{
   task_subtitle_catalog_t *catalog = (task_subtitle_catalog_t*)task->state;
   if (!retro_atomic_load_acquire_int(&catalog->status))
      subtitle_catalog_finish(task, -1);
   subtitle_catalog_unref(catalog);
}

task_subtitle_catalog_t *task_subtitle_discover(const char *movie_path)
{
   task_subtitle_catalog_t *catalog;
   retro_task_t *task;
   size_t length, start, end;
   if (!movie_path || !*movie_path || (length = strlen(movie_path)) > 32768)
      return NULL;
   start = length;
   while (start && movie_path[start - 1] != '/' && movie_path[start - 1] != '\\')
      start--;
   end = length;
   while (end > start && movie_path[end - 1] != '.')
      end--;
   if (end > start)
      end--;
   else
      end = length;
   if (end == start)
      return NULL;
   catalog = (task_subtitle_catalog_t*)calloc(1, sizeof(*catalog));
   if (!catalog)
      return NULL;
   retro_atomic_int_init(&catalog->refs, 1);
   retro_atomic_int_init(&catalog->cancelled, 0);
   retro_atomic_int_init(&catalog->status, 0);
   catalog->movie = (char*)malloc(length + 1);
   catalog->folder = (char*)malloc(start + 1);
   catalog->stem_length = end - start;
   catalog->stem = (char*)malloc(catalog->stem_length + 1);
   if (!catalog->folder || !catalog->stem || !catalog->movie)
   {
      subtitle_catalog_unref(catalog);
      return NULL;
   }
   memcpy(catalog->movie, movie_path, length + 1);
   memcpy(catalog->folder, movie_path, start);
   catalog->folder[start] = '\0';
   memcpy(catalog->stem, movie_path + start, catalog->stem_length);
   catalog->stem[catalog->stem_length] = '\0';
   task = task_init();
   if (!task)
   {
      subtitle_catalog_unref(catalog);
      return NULL;
   }
   task->state = catalog;
   task->handler = subtitle_catalog_handler;
   task->cleanup = subtitle_catalog_cleanup;
   task_set_flags(task, RETRO_TASK_FLG_MUTE, true);
   retro_atomic_fetch_add_int(&catalog->refs, 1);
   if (!task_queue_push(task))
   {
      free(task);
      subtitle_catalog_unref(catalog);
      subtitle_catalog_unref(catalog);
      return NULL;
   }
   return catalog;
}

int task_subtitle_catalog_status(const task_subtitle_catalog_t *catalog)
{
   return catalog ? retro_atomic_load_acquire_int(&catalog->status) : -1;
}

size_t task_subtitle_catalog_count(const task_subtitle_catalog_t *catalog)
{
   return task_subtitle_catalog_status(catalog) == 1 ? catalog->count : 0;
}

const char *task_subtitle_catalog_path(const task_subtitle_catalog_t *catalog,
      size_t index)
{
   return index < task_subtitle_catalog_count(catalog) ? catalog->entries[index].path : NULL;
}

const task_subtitle_entry_t *task_subtitle_catalog_entry(
      const task_subtitle_catalog_t *catalog, size_t index)
{
   return index < task_subtitle_catalog_count(catalog) ? &catalog->entries[index] : NULL;
}

void task_subtitle_catalog_release(task_subtitle_catalog_t *catalog)
{
   if (!catalog)
      return;
   retro_atomic_store_release_int(&catalog->cancelled, 1);
   subtitle_catalog_unref(catalog);
}
