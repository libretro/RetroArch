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

bool rdvd_spu_control_begin(const rdvd_spu_packet_t *packet, unsigned offset,
      rdvd_spu_control_t *out)
{
   rdvd_spu_packet_t canonical;
   unsigned next;
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (!packet || !rdvd_spu_packet_read(packet->data, packet->size, &canonical))
      return false;
   if (!offset) offset = canonical.control_offset;
   if (offset < canonical.control_offset || offset > canonical.size - 5)
      return false;
   next = rdvd_spu_u16(canonical.data + offset + 2);
   if (next < offset || next > canonical.size - 5
         || (next != offset && next < offset + 5))
      return false;
   out->packet = canonical;
   out->packet.pts = packet->pts;
   out->cursor = offset + 4;
   out->end = next == offset ? (unsigned)canonical.size : next;
   out->delay = rdvd_spu_u16(canonical.data + offset);
   out->next_offset = next;
   return true;
}

enum rdvd_spu_result rdvd_spu_control_next(rdvd_spu_control_t *control,
      rdvd_spu_event_t *out)
{
   rdvd_spu_event_t event;
   const uint8_t *p;
   unsigned args = 0, i, command;
   if (out) memset(out, 0, sizeof(*out));
   if (!control || !out || !control->packet.data)
   {
      if (control) control->stopped = true;
      return RDVD_SPU_ERROR;
   }
   if (control->stopped) return RDVD_SPU_MORE;
   if (control->cursor < control->packet.control_offset + 4
         || control->cursor >= control->end || control->end > control->packet.size)
      goto error;
   command = control->packet.data[control->cursor++];
   if (command == 0xff)
   {
      control->stopped = true;
      return RDVD_SPU_MORE;
   }
   if (command > 6)
   {
      control->stopped = true;
      return RDVD_SPU_UNSUPPORTED;
   }
   if (command == 3 || command == 4) args = 2;
   if (command == 5) args = 6;
   if (command == 6) args = 4;
   if (args > control->end - control->cursor) goto error;
   p = control->packet.data + control->cursor;
   memset(&event, 0, sizeof(event));
   event.command = command;
   if (command == 3 || command == 4)
   {
      unsigned word = rdvd_spu_u16(p);
      for (i = 0; i < 4; i++) event.values[i] = (word >> (4 * i)) & 15;
   }
   if (command == 5)
   {
      event.values[0] = ((unsigned)p[0] << 4) | (p[1] >> 4);
      event.values[1] = ((unsigned)(p[1] & 15) << 8) | p[2];
      event.values[2] = ((unsigned)p[3] << 4) | (p[4] >> 4);
      event.values[3] = ((unsigned)(p[4] & 15) << 8) | p[5];
      if (event.values[0] > event.values[1] || event.values[2] > event.values[3]) goto error;
   }
   if (command == 6)
      for (i = 0; i < 2; i++)
      {
         event.values[i] = rdvd_spu_u16(p + 2 * i);
         if (event.values[i] < 4 || event.values[i] >= control->packet.control_offset) goto error;
      }
   control->cursor += args;
   *out = event;
   return RDVD_SPU_READY;
error:
   control->stopped = true;
   return RDVD_SPU_ERROR;
}

bool rdvd_spu_decode_row(const rdvd_spu_packet_t *packet, unsigned start,
      unsigned width, uint8_t *output, size_t capacity, unsigned *next)
{
   static const unsigned thresholds[4] = {1, 4, 16, 64};
   rdvd_spu_packet_t p;
   unsigned nibble, written = 0;
   if (next) *next = 0;
   if (!packet || !output || !next || !width || width > 4096 || capacity < width
         || !rdvd_spu_packet_read(packet->data, packet->size, &p)
         || start < 4 || start >= p.control_offset)
      return false;
   nibble = start * 2;
   while (written < width)
   {
      unsigned code = 0, step, run;
      for (step = 0; step < 4 && code < thresholds[step]; step++)
      {
         unsigned value;
         if (nibble / 2 >= p.control_offset) return false;
         value = p.data[nibble / 2];
         value = nibble & 1 ? value & 15 : value >> 4;
         nibble++;
         code = (code << 4) | value;
      }
      run = code >> 2;
      if (!run) run = width - written;
      if (run > width - written) return false;
      memset(output + written, (int)(code & 3), run);
      written += run;
   }
   *next = (nibble + 1) / 2;
   return true;
}
