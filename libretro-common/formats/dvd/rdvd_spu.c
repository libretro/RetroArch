/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <string.h>
#include <formats/rdvd_spu.h>

struct rdvd_spu_stream
{
   uint64_t pts;
   uint8_t *buffer;
   size_t have;
   size_t expected;
};

static unsigned rdvd_spu_u16(const uint8_t *p)
{
   return ((unsigned)p[0] << 8) | p[1];
}

bool rdvd_spu_packet_read(const uint8_t *data, size_t size, rdvd_spu_packet_t *out)
{
   unsigned control;
   if (!out)
      return false;
   memset(out, 0, sizeof(*out));
   if (!data || size < 9 || size > 65535 || rdvd_spu_u16(data) != size)
      return false;
   control = rdvd_spu_u16(data + 2);
   if (control < 4 || control > size - 5)
      return false;
   out->data = data;
   out->size = size;
   out->pts = RDVD_SPU_NO_PTS;
   out->control_offset = control;
   return true;
}

rdvd_spu_stream_t *rdvd_spu_stream_new(void)
{
   return (rdvd_spu_stream_t*)calloc(1, sizeof(rdvd_spu_stream_t));
}

void rdvd_spu_stream_free(rdvd_spu_stream_t *stream)
{
   if (!stream) return;
   free(stream->buffer);
   free(stream);
}

void rdvd_spu_stream_reset(rdvd_spu_stream_t *stream)
{
   if (!stream) return;
   stream->have = stream->expected = 0;
   stream->pts = RDVD_SPU_NO_PTS;
}

enum rdvd_spu_result rdvd_spu_stream_feed(rdvd_spu_stream_t *stream,
      const uint8_t *data, size_t size, uint64_t pts, size_t *consumed,
      rdvd_spu_packet_t *out)
{
   size_t n;
   if (consumed) *consumed = 0;
   if (out) memset(out, 0, sizeof(*out));
   if (!stream || !consumed || !out || (!data && size))
   {
      rdvd_spu_stream_reset(stream);
      return RDVD_SPU_ERROR;
   }
   if (!size)
      return RDVD_SPU_MORE;
   if (!stream->have)
   {
      stream->pts = pts;
      if (size >= 2)
      {
         n = rdvd_spu_u16(data);
         if (n < 9) goto error;
         if (size >= n)
         {
            if (!rdvd_spu_packet_read(data, n, out)) goto error;
            out->pts = pts;
            *consumed = n;
            return RDVD_SPU_READY;
         }
      }
   }
   if (!stream->buffer)
   {
      stream->buffer = (uint8_t*)malloc(65535);
      if (!stream->buffer) goto error;
   }
   if (stream->have < 2)
   {
      n = 2 - stream->have;
      if (n > size) n = size;
      memcpy(stream->buffer + stream->have, data, n);
      stream->have += n;
      *consumed = n;
      if (stream->have < 2) return RDVD_SPU_MORE;
      stream->expected = rdvd_spu_u16(stream->buffer);
      if (stream->expected < 9) goto error;
   }
   n = stream->expected - stream->have;
   if (n > size - *consumed) n = size - *consumed;
   memcpy(stream->buffer + stream->have, data + *consumed, n);
   stream->have += n;
   *consumed += n;
   if (stream->have != stream->expected) return RDVD_SPU_MORE;
   if (!rdvd_spu_packet_read(stream->buffer, stream->expected, out)) goto error;
   out->pts = stream->pts;
   stream->have = stream->expected = 0;
   return RDVD_SPU_READY;
error:
   rdvd_spu_stream_reset(stream);
   return RDVD_SPU_ERROR;
}
