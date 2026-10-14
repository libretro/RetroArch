/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <string.h>
#include <retro_atomic.h>
#include <formats/rdvd_stream.h>
bool rdvd_stream_init(rdvd_stream_t *stream, rdvd_source_t *source,
      rdvd_vfs_t *vfs, tpool_t *pool, rmpeg1_ps_t *ps, uint8_t *buffer, size_t capacity)
{
#if defined(HAVE_THREADS) && defined(RETRO_ATOMIC_LOCK_FREE)
   rdvd_source_backend_t *backend;
#endif
   if (!stream) return false;
   memset(stream, 0, sizeof(*stream));
#if !defined(HAVE_THREADS) || !defined(RETRO_ATOMIC_LOCK_FREE)
   (void)source; (void)vfs; (void)pool; (void)ps; (void)buffer; (void)capacity;
   return false;
#else
   backend = rdvd_vfs_backend(vfs);
   if (!source || source->pending || source->failed || source->cancelled
         || !backend || backend->owner || !pool || !buffer || !capacity
         || source->map.sizes != backend->map.sizes
         || source->map.segments != backend->map.segments
         || !rdvd_demux_init(&stream->demux, ps)) return false;
   stream->source = source; stream->vfs = vfs; stream->pool = pool;
   stream->buffer = buffer; stream->capacity = capacity < 65536 ? capacity : 65536;
   return true;
#endif
}
enum rdvd_stream_status rdvd_stream_next(rdvd_stream_t *stream, rdvd_demux_packet_t *out)
{
   rdvd_source_backend_t *backend;
   uint64_t received;
   if (!out)
      return stream && stream->source && stream->source->pending
         ? RDVD_STREAM_WAIT : RDVD_STREAM_ERROR;
   memset(out, 0, sizeof(*out));
   if (!stream || !stream->source || !stream->demux.ps) return RDVD_STREAM_ERROR;
   backend = rdvd_vfs_backend(stream->vfs);
   if (stream->cancelled)
   {
      if (stream->source->pending && !rdvd_source_poll(stream->source, backend))
      {
         rdvd_vfs_schedule(stream->vfs, stream->pool);
         return RDVD_STREAM_WAIT;
      }
      return RDVD_STREAM_CANCELLED;
   }
   if (stream->failed) return RDVD_STREAM_ERROR;
   if (stream->demux.borrowing && rdvd_demux_next(&stream->demux, out))
      return RDVD_STREAM_PACKET;
   if (rmpeg1_ps_ended(stream->demux.ps)) return RDVD_STREAM_END;
   if (stream->source->pending)
   {
      if (!rdvd_source_poll(stream->source, backend))
      {
         rdvd_vfs_schedule(stream->vfs, stream->pool);
         return RDVD_STREAM_WAIT;
      }
      received = stream->source->completed - stream->before_read;
      if (stream->source->failed || !received || received > stream->capacity
            || !rdvd_demux_feed(&stream->demux, stream->buffer, (size_t)received))
      {
         stream->failed = true;
         return RDVD_STREAM_ERROR;
      }
      return RDVD_STREAM_WAIT;
   }
   if (stream->source->failed) return RDVD_STREAM_ERROR;
   if (!stream->source->remaining) return RDVD_STREAM_END;
   stream->before_read = stream->source->completed;
   if (!rdvd_source_submit(stream->source, backend, stream->buffer, stream->capacity))
      return stream->source->failed ? RDVD_STREAM_ERROR : RDVD_STREAM_WAIT;
   rdvd_vfs_schedule(stream->vfs, stream->pool);
   return RDVD_STREAM_WAIT;
}
void rdvd_stream_cancel(rdvd_stream_t *stream)
{
   if (!stream || !stream->source) return;
   stream->cancelled = true;
   rdvd_source_cancel(stream->source);
   rdvd_demux_reset(&stream->demux);
}
