/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef LIBRETRO_RDVD_STREAM_H
#define LIBRETRO_RDVD_STREAM_H
#include <formats/rdvd_demux.h>
#include <formats/rdvd_vfs.h>
RETRO_BEGIN_DECLS
enum rdvd_stream_status
{
   RDVD_STREAM_WAIT = 0, RDVD_STREAM_PACKET, RDVD_STREAM_END,
   RDVD_STREAM_ERROR, RDVD_STREAM_CANCELLED
};
typedef struct rdvd_stream
{
   rdvd_demux_t demux;
   rdvd_source_t *source;
   rdvd_vfs_t *vfs;
   tpool_t *pool;
   uint8_t *buffer;
   size_t capacity;
   uint64_t before_read;
   bool failed, cancelled;
} rdvd_stream_t;
/* Exclusively borrows an initialized source, adapter, dedicated I/O pool,
 * parser and consumer buffer. All stay alive until terminal status. Source
 * must have no pending read. Initialization resets the parser. */
bool rdvd_stream_init(rdvd_stream_t *stream, rdvd_source_t *source,
      rdvd_vfs_t *vfs, tpool_t *pool, rmpeg1_ps_t *ps, uint8_t *buffer, size_t capacity);
/* One owner-side step: pulls one packet or submits/polls a read. No waits
 * or I/O on this thread. Packet views expire on the next step/cancel.
 * Buffer is not reused until demux input is drained. END denotes source or
 * program end, not decoder completion or validated DVD conformance. */
enum rdvd_stream_status rdvd_stream_next(rdvd_stream_t *stream, rdvd_demux_packet_t *out);
/* Invalidates packet views and stops new reads. Keep polling to CANCELLED
 * before releasing pending storage/handles or destroying the I/O pool. */
void rdvd_stream_cancel(rdvd_stream_t *stream);
RETRO_END_DECLS
#endif
