/* MIT: Copyright (c) 2026 Cole Springer and The RetroArch team.
 * ASF field walk adapted from WaxFlow container/asf, commit 2f448c9435fd.
 * Borrowed payload cursor and bounded assembly are original RetroArch code.
 * See COPYING.MIT.WaxFlow and PROVENANCE.md. */
#include "rwma_internal.h"
#include <formats/rwma_stream.h>

static const uint8_t asf_header[16] = {0x30, 0x26, 0xb2, 0x75, 0x8e, 0x66, 0xcf, 0x11,
                                       0xa6, 0xd9, 0,    0xaa, 0,    0x62, 0xce, 0x6c};
static const uint8_t asf_data[16] = {0x36, 0x26, 0xb2, 0x75, 0x8e, 0x66, 0xcf, 0x11,
                                     0xa6, 0xd9, 0,    0xaa, 0,    0x62, 0xce, 0x6c};
static const uint8_t asf_file[16] = {0xa1, 0xdc, 0xab, 0x8c, 0x47, 0xa9, 0xcf, 0x11,
                                     0x8e, 0xe4, 0,    0xc0, 0x0c, 0x20, 0x53, 0x65};
static const uint8_t asf_stream[16] = {0x91, 0x07, 0xdc, 0xb7, 0xb7, 0xa9, 0xcf, 0x11,
                                       0x8e, 0xe6, 0,    0xc0, 0x0c, 0x20, 0x53, 0x65};
static const uint8_t asf_audio[16] = {0x40, 0x9e, 0x69, 0xf8, 0x4d, 0x5b, 0xcf, 0x11,
                                      0xa8, 0xfd, 0,    0x80, 0x5f, 0x5c, 0x44, 0x2b};
static const uint8_t asf_spread[16] = {0x50, 0xcd, 0xc3, 0xbf, 0x8f, 0x61, 0xcf, 0x11,
                                       0x8b, 0xb2, 0,    0xaa, 0,    0xb4, 0xe2, 0x20};

typedef struct stream_xma_cursor
{
   size_t packet, next;
   unsigned position, left, active;
} stream_xma_cursor;

struct rwma_stream
{
   const uint8_t *data, *object;
   rwma_t *decoder;
   uint8_t *assembly;
   size_t size, avail, start, end, cursor, packet_start, payload_end, payload_cursor;
   size_t object_size, object_pos, capacity, assembled, assembly_size;
   size_t compressed_at, compressed_end, demand;
   unsigned channels, rate, align, packet_size, stream_number;
   unsigned payloads, num_type, off_type, rep_type, pay_type;
   unsigned multi, asf, object_number, failed;
   rwma_t *xma[4];
   rwma_frame xma_frames[4];
   stream_xma_cursor xma_cursor[4];
   uint64_t xma_begin, xma_length, xma_skip, xma_left;
   uint32_t channel_mask;
   unsigned xma_streams, xma_ready;
};

static uint32_t stream_u32(const uint8_t *p)
{
   return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
          ((uint32_t)p[3] << 24);
}
static uint64_t stream_u64(const uint8_t *p)
{
   return stream_u32(p) | ((uint64_t)stream_u32(p + 4) << 32);
}
static size_t stream_available(const rwma_stream *s)
{
   return s->avail ? RWMA_MIN(s->avail, s->size) : s->size;
}
#include "rwma_xma_stream.h"

void rwma_stream_close(rwma_stream *s)
{
   unsigned i;
   if (!s)
      return;
   rwma_free(s->decoder);
   for (i = 0; i < 4; i++)
      rwma_free(s->xma[i]);
   free(s->assembly);
   free(s);
}

static int stream_wave(rwma_stream *s, rwma_config *cfg)
{
   size_t at = 12, available = stream_available(s), limit;
   unsigned found = 0;
   uint64_t riff = (uint64_t)stream_u32(s->data + 4) + 8;
   if (riff > s->size || riff < 12)
      return RWMA_BAD_DATA;
   limit = (size_t)riff;
   while (at <= limit && limit - at >= 8)
   {
      uint32_t bytes;
      size_t next;
      if (at > available || available - at < 8)
         return RWMA_STREAM_WAIT;
      bytes = stream_u32(s->data + at + 4);
      if (bytes > limit - at - 8)
         return RWMA_BAD_DATA;
      next = at + 8 + bytes;
      if (!memcmp(s->data + at, "fmt ", 4))
      {
         int status;
         if (next > available)
            return RWMA_STREAM_WAIT;
         if (bytes >= 52 && s->data[at + 8] == 0x66 && s->data[at + 9] == 1)
            status = stream_xma_format(s, cfg, s->data + at + 8);
         else
            status = rwma_parse_wave(cfg, s->data + at + 8, bytes);
         if (status)
            return status;
         if (cfg->codec == RWMA_PRO && cfg->extra_size >= 6)
            s->channel_mask = stream_u32(cfg->extra + 2);
         if (!memcmp(s->data + 8, "XWMA", 4))
         {
            static const uint8_t v2[6] = {0, 0, 0, 0, 31, 0};
            static const uint8_t pro[18] = {16, 0, 0, 0, 0, 0,   0, 0, 0,
                                            0,  0, 0, 0, 0, 224, 0, 0, 0};
            if (cfg->codec == RWMA_V2 && cfg->extra_size < sizeof(v2))
            {
               cfg->extra = v2;
               cfg->extra_size = sizeof(v2);
            }
            else if (cfg->codec == RWMA_PRO && cfg->extra_size < sizeof(pro))
            {
               cfg->extra = pro;
               cfg->extra_size = sizeof(pro);
            }
         }
         found = 1;
      }
      else if (!memcmp(s->data + at, "data", 4))
      {
         if (!found)
            return RWMA_BAD_DATA;
         s->start = at + 8;
         s->end = next;
         return RWMA_OK;
      }
      if ((bytes & 1) && next == limit)
         return RWMA_BAD_DATA;
      at = next + (bytes & 1);
   }
   return RWMA_BAD_DATA;
}

static int stream_asf(rwma_stream *s, rwma_config *cfg)
{
   size_t at = 30, available = stream_available(s), header;
   uint64_t length;
   unsigned count, i, found = 0;
   if (available < 30)
      return RWMA_STREAM_WAIT;
   length = stream_u64(s->data + 16);
   if (length > s->size || length < 30 || length > 16u * 1024 * 1024)
      return RWMA_BAD_DATA;
   header = (size_t)length;
   if (header > available)
      return RWMA_STREAM_WAIT;
   count = stream_u32(s->data + 24);
   if (count > (header - 30) / 24)
      return RWMA_BAD_DATA;
   for (i = 0; i < count; i++)
   {
      const uint8_t *p, *body;
      size_t bytes;
      uint64_t object;
      if (at > header || header - at < 24)
         return RWMA_BAD_DATA;
      p = s->data + at;
      object = stream_u64(p + 16);
      if (object < 24 || object > header - at)
         return RWMA_BAD_DATA;
      body = p + 24;
      bytes = (size_t)object - 24;
      if (!memcmp(p, asf_file, 16))
      {
         unsigned minimum, maximum;
         if (bytes < 80)
            return RWMA_BAD_DATA;
         minimum = stream_u32(body + 68);
         maximum = stream_u32(body + 72);
         if (minimum != maximum || !minimum || minimum > 1024 * 1024)
            return RWMA_UNSUPPORTED;
         s->packet_size = minimum;
      }
      else if (!found && !memcmp(p, asf_stream, 16) && bytes >= 54 &&
               !memcmp(body, asf_audio, 16))
      {
         uint32_t setup = stream_u32(body + 40), error = stream_u32(body + 44);
         unsigned flags = body[48] | ((unsigned)body[49] << 8);
         int status;
         if (flags & 0x8000)
            return RWMA_UNSUPPORTED;
         if (setup > bytes - 54 || error > bytes - 54 - setup)
            return RWMA_BAD_DATA;
         if (!memcmp(body + 16, asf_spread, 16) && error && body[54 + setup] > 1)
            return RWMA_UNSUPPORTED;
         status = rwma_parse_wave(cfg, body + 54, setup);
         if (!status)
         {
            found = 1;
            s->stream_number = flags & 127;
            if (cfg->codec == RWMA_PRO && cfg->extra_size >= 6)
               s->channel_mask = stream_u32(cfg->extra + 2);
         }
         else if (status != RWMA_UNSUPPORTED)
            return status;
      }
      at += (size_t)object;
   }
   if (!found || !s->packet_size)
      return RWMA_UNSUPPORTED;
   at = header;
   while (at <= s->size && s->size - at >= 24)
   {
      uint64_t bytes;
      if (at > available || available - at < 24)
         return RWMA_STREAM_WAIT;
      bytes = stream_u64(s->data + at + 16);
      if (bytes < 24 || bytes > s->size - at)
         return RWMA_BAD_DATA;
      if (!memcmp(s->data + at, asf_data, 16))
      {
         if (bytes < 50)
            return RWMA_BAD_DATA;
         s->start = at + 50;
         s->end = at + (size_t)bytes;
         s->asf = 1;
         return RWMA_OK;
      }
      at += (size_t)bytes;
   }
   return RWMA_BAD_DATA;
}

int rwma_stream_open(rwma_stream **out, const uint8_t *data, size_t size, size_t avail)
{
   rwma_stream *s;
   rwma_config cfg;
   int status;
   if (!out)
      return RWMA_BAD_DATA;
   *out = NULL;
   if (!data || size < 12)
      return RWMA_BAD_DATA;
   if (avail && avail < 12)
      return RWMA_STREAM_WAIT;
   s = (rwma_stream *)calloc(1, sizeof(*s));
   if (!s)
      return RWMA_NO_MEMORY;
   s->data = data;
   s->size = size;
   s->avail = avail;
   if (!memcmp(data, "RIFF", 4) &&
       (!memcmp(data + 8, "WAVE", 4) || !memcmp(data + 8, "XWMA", 4)))
      status = stream_wave(s, &cfg);
   else if (size >= 16 && stream_available(s) >= 16 && !memcmp(data, asf_header, 16))
      status = stream_asf(s, &cfg);
   else if (avail && avail < 16 && size >= 16)
      status = RWMA_STREAM_WAIT;
   else
      status = RWMA_UNSUPPORTED;
   if (!status && cfg.codec == RWMA_PRO && cfg.extra_size >= 6)
      s->channel_mask = stream_u32(cfg.extra + 2);
   if (!status && s->xma_streams)
   {
      unsigned i;
      if ((s->end - s->start) % 2048 || s->end - s->start < 2048 * s->xma_streams)
         status = RWMA_BAD_DATA;
      for (i = 0; !status && i < s->xma_streams; i++)
      {
         rwma_config lane = cfg;
         lane.channels = RWMA_MIN(2, cfg.channels - i * 2);
         status = rwma_create(&s->xma[i], &lane);
         s->xma_cursor[i].packet = s->start + i * 2048;
      }
   }
   else if (!status)
      status = rwma_create(&s->decoder, &cfg);
   if (status)
   {
      rwma_stream_close(s);
      return status;
   }
   s->channels = cfg.channels;
   s->rate = cfg.sample_rate;
   s->align = cfg.block_align;
   s->capacity = 16u * cfg.block_align;
   if (s->asf || s->xma_streams)
   {
      if (s->xma_streams)
         s->capacity = s->xma_streams * 4096;
      s->assembly = (uint8_t *)malloc(s->capacity);
      if (!s->assembly)
      {
         rwma_stream_close(s);
         return RWMA_NO_MEMORY;
      }
   }
   s->cursor = s->start;
   *out = s;
   return RWMA_OK;
}

static unsigned stream_field(const uint8_t *data, size_t end, size_t *at, unsigned type,
                             int *error)
{
   static const unsigned lengths[4] = {0, 1, 2, 4};
   unsigned bytes = lengths[type & 3], i;
   uint32_t value = 0;
   if (*at > end || bytes > end - *at)
   {
      *error = 1;
      return 0;
   }
   for (i = 0; i < bytes; i++)
      value |= (uint32_t)data[(*at)++] << (i * 8);
   return value;
}

static int stream_packet(rwma_stream *s)
{
   const uint8_t *p;
   size_t at = 0, end;
   unsigned flags, properties, length, padding;
   int error = 0;
   if (s->cursor >= s->end)
      return RWMA_STREAM_END;
   if (s->packet_size > s->end - s->cursor)
      return RWMA_BAD_DATA;
   s->demand = s->cursor + s->packet_size;
   if (s->demand > stream_available(s))
      return RWMA_STREAM_WAIT;
   s->packet_start = s->cursor;
   p = s->data + s->cursor;
   end = s->packet_size;
   if (p[0] & 128)
   {
      if (p[0] & 0x70)
         return RWMA_UNSUPPORTED;
      at = 1 + (p[0] & 15);
   }
   flags = stream_field(p, end, &at, 1, &error);
   properties = stream_field(p, end, &at, 1, &error);
   length = stream_field(p, end, &at, (flags >> 5) & 3, &error);
   (void)stream_field(p, end, &at, (flags >> 1) & 3, &error);
   padding = stream_field(p, end, &at, (flags >> 3) & 3, &error);
   (void)stream_field(p, end, &at, 3, &error);
   (void)stream_field(p, end, &at, 2, &error);
   if (((properties >> 6) & 3) != 1)
      return RWMA_UNSUPPORTED;
   s->multi = flags & 1;
   s->payloads = 1;
   s->pay_type = 0;
   if (s->multi)
   {
      unsigned value = stream_field(p, end, &at, 1, &error);
      s->payloads = value & 63;
      s->pay_type = value >> 6;
   }
   if (length && length < end)
      end = length;
   if (error || padding > end || at > end - padding)
      return RWMA_BAD_DATA;
   s->num_type = (properties >> 4) & 3;
   s->off_type = (properties >> 2) & 3;
   s->rep_type = properties & 3;
   s->payload_cursor = s->packet_start + at;
   s->payload_end = s->packet_start + end - padding;
   s->cursor += s->packet_size;
   s->demand = 0;
   return RWMA_OK;
}

static int stream_object(rwma_stream *s)
{
   unsigned steps;
   for (steps = 0; steps < 4096; steps++)
   {
      size_t at, bytes, rep_at, rep_size;
      unsigned stream, number, offset, object;
      int error = 0, status;
      if (s->compressed_at < s->compressed_end)
      {
         at = s->compressed_at;
         bytes = s->data[at++];
         if (!bytes || bytes > s->compressed_end - at)
            return RWMA_BAD_DATA;
         s->object = s->data + at;
         s->object_size = bytes;
         s->object_pos = 0;
         s->compressed_at = at + bytes;
         return RWMA_OK;
      }
      if (!s->payloads)
      {
         status = stream_packet(s);
         if (status == RWMA_STREAM_END && s->assembled)
            return RWMA_BAD_DATA;
         if (status)
            return status;
      }
      at = s->payload_cursor;
      stream = stream_field(s->data, s->payload_end, &at, 1, &error) & 127;
      number = stream_field(s->data, s->payload_end, &at, s->num_type, &error);
      offset = stream_field(s->data, s->payload_end, &at, s->off_type, &error);
      rep_size = stream_field(s->data, s->payload_end, &at, s->rep_type, &error);
      if (error || at > s->payload_end || rep_size > s->payload_end - at)
         return RWMA_BAD_DATA;
      rep_at = at;
      at += rep_size;
      bytes = s->multi ? stream_field(s->data, s->payload_end, &at, s->pay_type, &error)
                       : s->payload_end - at;
      if (error || at > s->payload_end || bytes > s->payload_end - at)
         return RWMA_BAD_DATA;
      s->payload_cursor = at + bytes;
      s->payloads--;
      if (stream != s->stream_number)
         continue;
      if (rep_size == 1)
      {
         s->compressed_at = at;
         s->compressed_end = at + bytes;
         continue;
      }
      if (rep_size < 8)
         return RWMA_UNSUPPORTED;
      object = stream_u32(s->data + rep_at);
      if (!object || object > s->capacity || offset > object || bytes > object - offset)
         return RWMA_BAD_DATA;
      if (!offset && bytes == object)
      {
         if (s->assembled)
            return RWMA_BAD_DATA;
         s->object = s->data + at;
         s->object_size = bytes;
         s->object_pos = 0;
         return RWMA_OK;
      }
      if (!offset)
      {
         if (s->assembled)
            return RWMA_BAD_DATA;
         s->object_number = number;
         s->assembly_size = object;
      }
      if (offset != s->assembled || number != s->object_number ||
          object != s->assembly_size)
         return RWMA_BAD_DATA;
      memcpy(s->assembly + offset, s->data + at, bytes);
      s->assembled += bytes;
      if (s->assembled == object)
      {
         s->assembled = 0;
         s->object = s->assembly;
         s->object_size = object;
         s->object_pos = 0;
         return RWMA_OK;
      }
   }
   return RWMA_STREAM_WAIT;
}

int rwma_stream_next(rwma_stream *s, rwma_frame *frame)
{
   unsigned steps;
   if (!frame)
      return RWMA_BAD_DATA;
   memset(frame, 0, sizeof(*frame));
   if (!s || s->failed)
      return RWMA_BAD_DATA;
   if (s->xma_streams)
   {
      int status = stream_xma_next(s, frame);
      if (status < 0)
      {
         s->failed = 1;
         memset(frame, 0, sizeof(*frame));
      }
      return status;
   }
   for (steps = 0; steps < 256; steps++)
   {
      const uint8_t *p;
      size_t bytes, used = 0;
      int status;
      if (s->asf)
      {
         if (s->object_pos == s->object_size)
         {
            s->object = NULL;
            s->object_pos = s->object_size = 0;
            status = stream_object(s);
            if (status < 0)
               goto invalid;
            if (status)
               return status;
         }
         p = s->object + s->object_pos;
         bytes = s->object_size - s->object_pos;
      }
      else
      {
         if (s->cursor == s->end)
            return RWMA_STREAM_END;
         bytes = s->end - s->cursor;
         if (bytes < s->align)
            goto invalid;
         s->demand = s->cursor + s->align;
         if (s->demand > stream_available(s))
            return RWMA_STREAM_WAIT;
         p = s->data + s->cursor;
      }
      status = rwma_decode_packet(s->decoder, p, bytes, &used, frame);
      if (status || used > bytes)
         goto invalid;
      if (s->asf)
         s->object_pos += used;
      else
         s->cursor += used;
      s->demand = 0;
      if (frame->samples)
         return RWMA_OK;
   }
   return RWMA_STREAM_WAIT;
invalid:
   s->failed = 1;
   memset(frame, 0, sizeof(*frame));
   return RWMA_BAD_DATA;
}

void rwma_stream_rewind(rwma_stream *s)
{
   unsigned i;
   if (!s)
      return;
   rwma_reset(s->decoder);
   s->xma_ready = 0;
   s->xma_skip = s->xma_begin;
   s->xma_left = s->xma_length;
   for (i = 0; i < s->xma_streams; i++)
   {
      rwma_reset(s->xma[i]);
      memset(&s->xma_cursor[i], 0, sizeof(s->xma_cursor[i]));
      s->xma_cursor[i].packet = s->start + i * 2048;
   }
   s->cursor = s->start;
   s->packet_start = s->start;
   s->payloads = s->failed = 0;
   s->assembled = s->object_pos = s->object_size = 0;
   s->compressed_at = s->compressed_end = s->demand = 0;
   s->object = NULL;
}
void rwma_stream_set_avail(rwma_stream *s, size_t avail)
{
   if (s)
      s->avail = avail;
}
void rwma_stream_info(const rwma_stream *s, unsigned *channels, unsigned *rate)
{
   if (channels)
      *channels = s ? s->channels : 0;
   if (rate)
      *rate = s ? s->rate : 0;
}
size_t rwma_stream_tell(const rwma_stream *s)
{
   if (!s)
      return 0;
   if (s->xma_streams)
   {
      size_t tell = s->end;
      unsigned i;
      for (i = 0; i < s->xma_streams; i++)
      {
         const stream_xma_cursor *c = &s->xma_cursor[i];
         size_t at = c->active && !c->left ? c->next : c->packet;
         if (at < tell)
            tell = at;
      }
      return tell;
   }
   return s->asf && (s->payloads || s->object_pos < s->object_size ||
                     s->compressed_at < s->compressed_end)
              ? s->packet_start
              : s->cursor;
}
size_t rwma_stream_read_end(const rwma_stream *s)
{
   return s ? s->demand : 0;
}
uint32_t rwma_stream_channel_mask(const rwma_stream *s)
{
   return s ? s->channel_mask : 0;
}
