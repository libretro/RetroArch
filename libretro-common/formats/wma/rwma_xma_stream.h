/* Copyright (C) 2026 The RetroArch team. MIT license. */
#ifndef LIBRETRO_RWMA_XMA_STREAM_H
#define LIBRETRO_RWMA_XMA_STREAM_H

static int stream_xma_format(rwma_stream *s, rwma_config *cfg, const uint8_t *fmt)
{
   unsigned block = stream_u32(fmt + 28);
   uint32_t encoded = stream_u32(fmt + 24), begin = stream_u32(fmt + 32);
   memset(cfg, 0, sizeof(*cfg));
   cfg->codec = RWMA_XMA_RAW;
   cfg->channels = fmt[2] | ((unsigned)fmt[3] << 8);
   cfg->sample_rate = stream_u32(fmt + 4);
   cfg->block_align = 2048;
   s->xma_streams = fmt[18] | ((unsigned)fmt[19] << 8);
   s->channel_mask = stream_u32(fmt + 20);
   if (!cfg->channels || cfg->channels > 8 ||
       s->xma_streams != (cfg->channels + 1) / 2 || !block || block % 2048 ||
       block < 2048 * s->xma_streams ||
       (cfg->sample_rate != 24000 && cfg->sample_rate != 32000 &&
        cfg->sample_rate != 44100 && cfg->sample_rate != 48000) ||
       fmt[14] != 16 || fmt[15] || encoded < 512 || begin > encoded - 512)
      return RWMA_UNSUPPORTED;
   /* One priming frame plus half of the minimum 128-sample subframe. */
   s->xma_begin = (uint64_t)begin + 512 + 64;
   s->xma_length = stream_u32(fmt + 36);
   if (!s->xma_length)
      s->xma_length = encoded - begin - 512;
   if (s->xma_length > encoded - begin - 512)
      return RWMA_BAD_DATA;
   s->xma_skip = s->xma_begin;
   s->xma_left = s->xma_length;
   return RWMA_OK;
}

static int stream_xma_packet(rwma_stream *s, size_t packet)
{
   if (packet >= s->end)
      return RWMA_STREAM_END;
   if (packet < s->start || s->end - packet < 2048)
      return RWMA_BAD_DATA;
   if (packet + 2048 > stream_available(s))
   {
      s->demand = packet + 2048;
      return RWMA_STREAM_WAIT;
   }
   return RWMA_OK;
}

static int stream_xma_advance(rwma_stream *s, size_t *packet, unsigned *position)
{
   size_t distance;
   int status = stream_xma_packet(s, *packet);
   if (status)
      return status;
   distance = ((size_t)s->data[*packet + 3] + 1) * 2048;
   if (distance >= s->end - *packet)
      return RWMA_BAD_DATA;
   *packet += distance;
   *position = 32;
   return stream_xma_packet(s, *packet);
}

static int stream_xma_frame(rwma_stream *s, unsigned index)
{
   stream_xma_cursor c = s->xma_cursor[index];
   unsigned steps, length, position;
   size_t packet;
   rwma_reader r;
   int status;
   for (steps = 0; !c.left && steps < 256; steps++)
   {
      const uint8_t *header;
      size_t distance;
      unsigned offset;
      if (c.active)
         c.packet = c.next;
      status = stream_xma_packet(s, c.packet);
      if (status)
      {
         s->xma_cursor[index] = c;
         return status;
      }
      header = s->data + c.packet;
      c.left = header[0] >> 2;
      offset = ((header[0] & 3u) << 13) | ((unsigned)header[1] << 5) | (header[2] >> 3);
      distance = ((size_t)header[3] + 1) * 2048;
      c.next = distance >= s->end - c.packet ? s->end : c.packet + distance;
      c.position = 32 + offset;
      c.active = 1;
      if (c.left && c.position >= 16384)
         return RWMA_BAD_DATA;
   }
   if (!c.left)
   {
      s->xma_cursor[index] = c;
      return RWMA_STREAM_WAIT;
   }
   packet = c.packet;
   position = c.position;
   if (position <= 16384 - 15)
   {
      r.p = s->data + packet;
      r.pos = position;
      r.end = 16384;
      r.error = 0;
      length = rwma_read(&r, 15);
   }
   else
   {
      unsigned i;
      length = 0;
      for (i = 0; i < 15; i++)
      {
         if (position == 16384)
         {
            status = stream_xma_advance(s, &packet, &position);
            if (status)
               return status;
         }
         length = (length << 1) |
                  ((s->data[packet + position / 8] >> (7 - (position & 7))) & 1);
         position++;
      }
   }
   if (length < 18 || length == 32767)
      return RWMA_BAD_DATA;
   if (length <= 16384 - c.position)
      status = rwma_decode_xma(s->xma[index], s->data + c.packet, 2048, c.position,
                               length, &s->xma_frames[index]);
   else
   {
      size_t copied = 0;
      unsigned left = length;
      if (c.left != 1)
         return RWMA_BAD_DATA;
      packet = c.packet;
      position = c.position;
      while (left)
      {
         unsigned count;
         if (position == 16384)
         {
            status = stream_xma_advance(s, &packet, &position);
            if (status)
               return status;
         }
         count = RWMA_MIN(left, 16384 - position);
         r.p = s->data + packet;
         r.pos = position;
         r.end = 16384;
         r.error = 0;
         if (rwma_append(s->assembly + index * 4096, &copied, 32768, &r, count))
            return RWMA_BAD_DATA;
         position += count;
         left -= count;
      }
      status = rwma_decode_xma(s->xma[index], s->assembly + index * 4096,
                               (length + 7) / 8, 0, length, &s->xma_frames[index]);
   }
   if (status)
      return status;
   c.position += length;
   c.left--;
   s->xma_cursor[index] = c;
   s->demand = 0;
   return RWMA_OK;
}

static int stream_xma_next(rwma_stream *s, rwma_frame *out)
{
   unsigned step;
   if (!s->xma_left)
      return RWMA_STREAM_END;
   for (step = 0; step < 256; step++)
   {
      unsigned i, c = 0, skip, take, available;
      while (s->xma_ready < s->xma_streams)
      {
         int status = stream_xma_frame(s, s->xma_ready);
         if (status == RWMA_STREAM_END)
         {
            status = rwma_drain_xma(s->xma[s->xma_ready], &s->xma_frames[s->xma_ready]);
            if (!status && !s->xma_frames[s->xma_ready].samples)
               return RWMA_BAD_DATA;
         }
         if (status)
            return status;
         s->xma_ready++;
      }
      available = s->xma_frames[0].samples;
      skip = s->xma_skip > available ? available : (unsigned)s->xma_skip;
      s->xma_skip -= skip;
      take = available - skip;
      if (take > s->xma_left)
         take = (unsigned)s->xma_left;
      for (i = 0; i < s->xma_streams; i++)
      {
         unsigned lane;
         if (s->xma_frames[i].samples != available)
            return RWMA_BAD_DATA;
         for (lane = 0; lane < s->xma_frames[i].channels; lane++)
            out->planes[c++] = s->xma_frames[i].planes[lane] + skip;
      }
      s->xma_ready = 0;
      if (c != s->channels)
         return RWMA_BAD_DATA;
      if (take)
      {
         out->samples = take;
         out->channels = c;
         out->sample_rate = s->rate;
         s->xma_left -= take;
         return RWMA_OK;
      }
   }
   return RWMA_STREAM_WAIT;
}
#endif
