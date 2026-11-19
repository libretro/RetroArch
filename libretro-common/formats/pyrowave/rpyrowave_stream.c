/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <formats/rpyrowave_stream.h>
#include <stdlib.h>
#include <string.h>

enum rpyrowave_stream_state
{
   RPYS_SEND_CONFIG, RPYS_WAIT_CONFIG, RPYS_WAIT_READY, RPYS_WAIT_ACCEPT,
   RPYS_SEND_READY, RPYS_SEND_REJECT, RPYS_ACTIVE, RPYS_DONE, RPYS_REJECTED,
   RPYS_FAILED, RPYS_CANCELLED
};

typedef struct rpyrowave_stream_header
{
   rpyrowave_stream_record_t record;
   uint32_t payload;
   uint32_t offset;
   unsigned flags;
} rpyrowave_stream_header_t;

typedef struct rpyrowave_stream_lend
{
   rpyrowave_stream_record_t record;
   const uint8_t *data;
   void *token;
   uint32_t offset;
   unsigned flags;
   unsigned active;
   unsigned dropped;
} rpyrowave_stream_lend_t;

struct rpyrowave_stream
{
   rpyrowave_stream_io_t io;
   rpyrowave_stream_callbacks_t callbacks;
   rpyrowave_stream_config_t config;
   rpyrowave_stream_lend_t video;
   rpyrowave_stream_lend_t audio;
   rpyrowave_stream_header_t incoming;
   rpyrowave_stream_header_t outgoing;
   uint64_t generation;
   uint64_t last_video;
   uint64_t last_timestamp;
   uint64_t next_audio;
   uint8_t control[RPYROWAVE_STREAM_CONFIG_BYTES];
   uint8_t read_header[RPYROWAVE_STREAM_HEADER_BYTES];
   uint8_t write_header[RPYROWAVE_STREAM_HEADER_BYTES];
   uint8_t discard[RPYROWAVE_STREAM_FRAGMENT_BYTES];
   size_t read_header_bytes;
   size_t read_payload_bytes;
   size_t write_header_bytes;
   size_t write_payload_bytes;
   const uint8_t *write_payload;
   unsigned state;
   unsigned sender;
   unsigned finishing;
   unsigned reason;
};

static uint32_t rpyrowave_stream_load32(const uint8_t *p)
{
   return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint64_t rpyrowave_stream_load64(const uint8_t *p)
{
   return (uint64_t)rpyrowave_stream_load32(p) | (uint64_t)rpyrowave_stream_load32(p + 4) << 32;
}

static void rpyrowave_stream_store32(uint8_t *p, uint32_t value)
{
   p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8);
   p[2] = (uint8_t)(value >> 16); p[3] = (uint8_t)(value >> 24);
}

static void rpyrowave_stream_store64(uint8_t *p, uint64_t value)
{
   rpyrowave_stream_store32(p, (uint32_t)value);
   rpyrowave_stream_store32(p + 4, (uint32_t)(value >> 32));
}

static uint32_t rpyrowave_stream_vui(const rpyrowave_sequence_t *s)
{
   return s->chroma_444 | s->primaries_bt2020 << 1 | s->transfer_pq << 2
      | s->transform_bt2020 << 3 | s->limited_range << 4 | s->chroma_left << 5;
}

int rpyrowave_stream_config_valid(const rpyrowave_stream_config_t *c)
{
   unsigned channels = 0;
   uint32_t mask;
   if (!c || !rpyrowave_block_layout(&c->sequence, 0, NULL)
         || c->sequence.primaries_bt2020 > 1 || c->sequence.transfer_pq > 1
         || c->sequence.transform_bt2020 > 1 || c->sequence.limited_range > 1
         || c->sequence.chroma_left > 1 || c->bit_depth < 8 || c->bit_depth > 16
         || c->maximum_frame_bytes < 8 || c->maximum_frame_bytes > RPYROWAVE_STREAM_MAX_FRAME_BYTES
         || !c->fps_numerator || c->fps_numerator > 1000000
         || !c->fps_denominator || c->fps_denominator > 1000000) return RPYROWAVE_INVALID;
   if (!c->audio_rate)
      return c->audio_channels || c->audio_channel_mask || c->maximum_audio_frames
         ? RPYROWAVE_INVALID : RPYROWAVE_OK;
   if (c->audio_rate < 8000 || c->audio_rate > 384000 || !c->audio_channels || c->audio_channels > 8
         || !c->maximum_audio_frames || c->maximum_audio_frames > 4096
         || c->maximum_audio_frames * c->audio_channels * 2 > RPYROWAVE_STREAM_MAX_PAYLOAD
         || (c->audio_channel_mask & ~0x3ffffU)) return RPYROWAVE_INVALID;
   for (mask = c->audio_channel_mask; mask; mask >>= 1) channels += mask & 1;
   return channels == c->audio_channels ? RPYROWAVE_OK : RPYROWAVE_INVALID;
}

int rpyrowave_stream_time(uint64_t ticks, uint32_t numerator, uint32_t denominator, uint64_t *microseconds)
{
   uint64_t scale;
   uint64_t whole;
   uint64_t tail;
   uint64_t quotient;
   if (!microseconds || !numerator || numerator > 1000000 || !denominator || denominator > 1000000)
      return RPYROWAVE_INVALID;
   scale = (uint64_t)denominator * 1000000;
   quotient = ticks / numerator;
   if (quotient > (uint64_t)-1 / scale) return RPYROWAVE_INVALID;
   whole = quotient * scale;
   tail = (ticks % numerator) * scale / numerator;
   if (whole > (uint64_t)-1 - tail) return RPYROWAVE_INVALID;
   *microseconds = whole + tail;
   return RPYROWAVE_OK;
}

static void rpyrowave_stream_pack_config(uint8_t *p, const rpyrowave_stream_config_t *c)
{
   memset(p, 0, RPYROWAVE_STREAM_CONFIG_BYTES);
   rpyrowave_stream_store32(p, c->sequence.width);
   rpyrowave_stream_store32(p + 4, c->sequence.height);
   rpyrowave_stream_store32(p + 8, c->bit_depth);
   rpyrowave_stream_store32(p + 12, rpyrowave_stream_vui(&c->sequence));
   rpyrowave_stream_store32(p + 16, c->maximum_frame_bytes);
   rpyrowave_stream_store32(p + 20, c->fps_numerator);
   rpyrowave_stream_store32(p + 24, c->fps_denominator);
   rpyrowave_stream_store32(p + 28, c->audio_rate);
   rpyrowave_stream_store32(p + 32, c->audio_channels);
   rpyrowave_stream_store32(p + 36, c->audio_channel_mask);
   rpyrowave_stream_store32(p + 40, c->audio_rate ? 1 : 0);
   rpyrowave_stream_store32(p + 44, c->maximum_audio_frames);
}

static int rpyrowave_stream_unpack_config(const uint8_t *p, rpyrowave_stream_config_t *c)
{
   uint32_t vui = rpyrowave_stream_load32(p + 12);
   if (vui & ~63U || rpyrowave_stream_load64(p + 48) || rpyrowave_stream_load64(p + 56))
      return RPYROWAVE_INVALID;
   memset(c, 0, sizeof(*c));
   c->sequence.width = rpyrowave_stream_load32(p);
   c->sequence.height = rpyrowave_stream_load32(p + 4);
   c->bit_depth = rpyrowave_stream_load32(p + 8);
   c->sequence.chroma_444 = vui & 1;
   c->sequence.primaries_bt2020 = (vui >> 1) & 1;
   c->sequence.transfer_pq = (vui >> 2) & 1;
   c->sequence.transform_bt2020 = (vui >> 3) & 1;
   c->sequence.limited_range = (vui >> 4) & 1;
   c->sequence.chroma_left = (vui >> 5) & 1;
   c->maximum_frame_bytes = rpyrowave_stream_load32(p + 16);
   c->fps_numerator = rpyrowave_stream_load32(p + 20);
   c->fps_denominator = rpyrowave_stream_load32(p + 24);
   c->audio_rate = rpyrowave_stream_load32(p + 28);
   c->audio_channels = rpyrowave_stream_load32(p + 32);
   c->audio_channel_mask = rpyrowave_stream_load32(p + 36);
   c->maximum_audio_frames = rpyrowave_stream_load32(p + 44);
   if (rpyrowave_stream_load32(p + 40) != (c->audio_rate ? 1U : 0U)) return RPYROWAVE_UNSUPPORTED;
   return rpyrowave_stream_config_valid(c);
}

static void rpyrowave_stream_pack_header(uint8_t *p, const rpyrowave_stream_header_t *h)
{
   memset(p, 0, RPYROWAVE_STREAM_HEADER_BYTES);
   p[0] = 'R'; p[1] = 'P'; p[2] = 'W'; p[3] = 'S'; p[4] = 1;
   p[6] = (uint8_t)h->record.kind; p[7] = (uint8_t)h->flags;
   rpyrowave_stream_store32(p + 8, h->payload);
   rpyrowave_stream_store32(p + 12, h->record.bytes);
   rpyrowave_stream_store32(p + 16, h->offset);
   rpyrowave_stream_store64(p + 24, h->record.generation);
   rpyrowave_stream_store64(p + 32, h->record.serial);
   rpyrowave_stream_store64(p + 40, h->record.timestamp);
   rpyrowave_stream_store64(p + 48, h->record.duration);
}

static int rpyrowave_stream_unpack_header(const uint8_t *p, rpyrowave_stream_header_t *h)
{
   if (p[0] != 'R' || p[1] != 'P' || p[2] != 'W' || p[3] != 'S' || p[4] != 1 || p[5]
         || p[6] < RPYROWAVE_STREAM_CONFIG || p[6] > RPYROWAVE_STREAM_REJECT
         || rpyrowave_stream_load32(p + 20) || rpyrowave_stream_load64(p + 56)) return RPYROWAVE_INVALID;
   memset(h, 0, sizeof(*h));
   h->record.kind = p[6]; h->flags = p[7];
   h->payload = rpyrowave_stream_load32(p + 8);
   h->record.bytes = rpyrowave_stream_load32(p + 12);
   h->offset = rpyrowave_stream_load32(p + 16);
   h->record.generation = rpyrowave_stream_load64(p + 24);
   h->record.serial = rpyrowave_stream_load64(p + 32);
   h->record.timestamp = rpyrowave_stream_load64(p + 40);
   h->record.duration = rpyrowave_stream_load64(p + 48);
   if (!h->record.generation || h->payload > RPYROWAVE_STREAM_MAX_PAYLOAD
         || h->record.bytes > RPYROWAVE_STREAM_MAX_FRAME_BYTES || h->offset > h->record.bytes
         || h->payload > h->record.bytes - h->offset
         || h->record.timestamp > (uint64_t)-1 - h->record.duration) return RPYROWAVE_INVALID;
   if (h->record.kind == RPYROWAVE_STREAM_VIDEO)
   {
      unsigned flags = (!h->offset ? 1U : 0U) | (h->offset + h->payload == h->record.bytes ? 2U : 0U);
      return h->record.serial && h->record.duration && h->record.bytes >= 8 && h->payload
         && !((h->record.bytes | h->offset | h->payload) & 3) && h->flags == flags
         ? RPYROWAVE_OK : RPYROWAVE_INVALID;
   }
   if (h->offset || h->record.bytes != h->payload) return RPYROWAVE_INVALID;
   if (h->record.kind == RPYROWAVE_STREAM_AUDIO)
      return h->payload && h->record.duration && !(h->flags & ~4U) ? RPYROWAVE_OK : RPYROWAVE_INVALID;
   if (h->flags || h->record.serial || h->record.timestamp || h->record.duration) return RPYROWAVE_INVALID;
   if (h->record.kind == RPYROWAVE_STREAM_CONFIG)
      return h->payload == RPYROWAVE_STREAM_CONFIG_BYTES ? RPYROWAVE_OK : RPYROWAVE_INVALID;
   if (h->record.kind == RPYROWAVE_STREAM_REJECT)
      return h->payload == 4 ? RPYROWAVE_OK : RPYROWAVE_INVALID;
   return !h->payload ? RPYROWAVE_OK : RPYROWAVE_INVALID;
}

static void rpyrowave_stream_event(rpyrowave_stream_t *s, unsigned event, unsigned reason)
{
   if (s->callbacks.event) s->callbacks.event(s->callbacks.user, event, &s->config, s->generation, reason);
}

static void rpyrowave_stream_release_all(rpyrowave_stream_t *s)
{
   rpyrowave_stream_lend_t *lends[2];
   unsigned i;
   lends[0] = &s->video; lends[1] = &s->audio;
   for (i = 0; i < 2; i++)
   {
      rpyrowave_stream_lend_t *lend = lends[i];
      if (!lend->active) continue;
      lend->active = 0;
      if (s->sender) s->callbacks.sent(s->callbacks.user, &lend->record, lend->data, lend->token, 0);
      else if (!lend->dropped) s->callbacks.cancel(s->callbacks.user, &lend->record, (void*)lend->data);
      lend->data = NULL;
   }
}

static int rpyrowave_stream_fail(rpyrowave_stream_t *s, unsigned reason)
{
   s->state = RPYS_FAILED;
   rpyrowave_stream_release_all(s);
   rpyrowave_stream_event(s, RPYROWAVE_STREAM_BROKEN, reason);
   return -1;
}

static rpyrowave_stream_t *rpyrowave_stream_new(const rpyrowave_stream_io_t *io,
      const rpyrowave_stream_callbacks_t *callbacks)
{
   rpyrowave_stream_t *s;
   if (!io || !io->read || !io->write || !callbacks) return NULL;
   s = (rpyrowave_stream_t*)calloc(1, sizeof(*s));
   if (s) { s->io = *io; s->callbacks = *callbacks; }
   return s;
}

rpyrowave_stream_t *rpyrowave_stream_sender_new(const rpyrowave_stream_config_t *config,
      uint64_t generation, const rpyrowave_stream_io_t *io, const rpyrowave_stream_callbacks_t *callbacks)
{
   rpyrowave_stream_t *s;
   if (rpyrowave_stream_config_valid(config) || !generation || !callbacks || !callbacks->sent) return NULL;
   s = rpyrowave_stream_new(io, callbacks);
   if (!s) return NULL;
   s->config = *config;
   s->generation = generation;
   s->sender = 1;
   s->state = RPYS_SEND_CONFIG;
   rpyrowave_stream_pack_config(s->control, config);
   return s;
}

rpyrowave_stream_t *rpyrowave_stream_receiver_new(const rpyrowave_stream_io_t *io,
      const rpyrowave_stream_callbacks_t *callbacks)
{
   rpyrowave_stream_t *s;
   if (!callbacks || !callbacks->acquire || !callbacks->commit || !callbacks->cancel || !callbacks->event) return NULL;
   s = rpyrowave_stream_new(io, callbacks);
   if (s) s->state = RPYS_WAIT_CONFIG;
   return s;
}

int rpyrowave_stream_accept(rpyrowave_stream_t *s, unsigned reason)
{
   if (!s || s->sender || s->state != RPYS_WAIT_ACCEPT) return RPYROWAVE_STREAM_ERROR;
   s->reason = reason;
   if (reason) rpyrowave_stream_store32(s->control, reason);
   s->state = reason ? RPYS_SEND_REJECT : RPYS_SEND_READY;
   return RPYROWAVE_STREAM_PROGRESS;
}

static int rpyrowave_stream_audio_valid(const rpyrowave_stream_config_t *c,
      const rpyrowave_stream_record_t *r, uint64_t previous, uint64_t *end)
{
   uint64_t start_time;
   uint64_t end_time;
   uint32_t frames;
   if (!c->audio_rate || !r->bytes || r->bytes % (c->audio_channels * 2)) return 0;
   frames = r->bytes / (c->audio_channels * 2);
   if (frames > c->maximum_audio_frames || r->serial < previous || r->serial > (uint64_t)-1 - frames) return 0;
   *end = r->serial + frames;
   if (rpyrowave_stream_time(r->serial, c->audio_rate, 1, &start_time)
         || rpyrowave_stream_time(*end, c->audio_rate, 1, &end_time)) return 0;
   return start_time == r->timestamp && end_time - start_time == r->duration;
}

int rpyrowave_stream_offer(rpyrowave_stream_t *s, const rpyrowave_stream_record_t *record,
      const void *data, void *token)
{
   rpyrowave_stream_lend_t *lend;
   uint64_t audio_end = 0;
   if (!s || !s->sender || !record || !data || record->generation != s->generation
         || (record->kind != RPYROWAVE_STREAM_VIDEO && record->kind != RPYROWAVE_STREAM_AUDIO))
      return RPYROWAVE_STREAM_ERROR;
   if (s->state != RPYS_ACTIVE || s->finishing)
      return s->state < RPYS_DONE && !s->finishing ? RPYROWAVE_STREAM_BUSY : RPYROWAVE_STREAM_ERROR;
   lend = record->kind == RPYROWAVE_STREAM_VIDEO ? &s->video : &s->audio;
   if (lend->active) return RPYROWAVE_STREAM_BUSY;
   if (record->timestamp > (uint64_t)-1 - record->duration) return RPYROWAVE_STREAM_ERROR;
   if (record->kind == RPYROWAVE_STREAM_VIDEO)
   {
      rpyrowave_sequence_t sequence;
      if (!record->serial || record->serial <= s->last_video || record->timestamp < s->last_timestamp
            || !record->duration || record->bytes < 8 || record->bytes > s->config.maximum_frame_bytes
            || (record->bytes & 3) || rpyrowave_read_sequence(data, 8, &sequence)
            || sequence.width != s->config.sequence.width || sequence.height != s->config.sequence.height
            || rpyrowave_stream_vui(&sequence) != rpyrowave_stream_vui(&s->config.sequence))
         return RPYROWAVE_STREAM_ERROR;
   }
   else if (!rpyrowave_stream_audio_valid(&s->config, record, s->next_audio, &audio_end))
      return RPYROWAVE_STREAM_ERROR;
   lend->record = *record;
   lend->data = (const uint8_t*)data;
   lend->token = token;
   lend->offset = 0;
   lend->active = 1;
   lend->flags = record->kind == RPYROWAVE_STREAM_AUDIO && record->serial > s->next_audio ? 4 : 0;
   if (record->kind == RPYROWAVE_STREAM_VIDEO)
   {
      s->last_video = record->serial;
      s->last_timestamp = record->timestamp;
   }
   else
   {
      s->next_audio = audio_end;
      if (s->outgoing.record.kind == RPYROWAVE_STREAM_VIDEO && !s->write_header_bytes)
         s->outgoing.record.kind = 0;
   }
   return RPYROWAVE_STREAM_PROGRESS;
}

static void rpyrowave_stream_select_write(rpyrowave_stream_t *s)
{
   rpyrowave_stream_header_t *h = &s->outgoing;
   if (h->record.kind) return;
   memset(h, 0, sizeof(*h));
   h->record.generation = s->generation;
   if (s->state == RPYS_SEND_CONFIG)
   {
      h->record.kind = RPYROWAVE_STREAM_CONFIG;
      h->payload = h->record.bytes = RPYROWAVE_STREAM_CONFIG_BYTES;
      s->write_payload = s->control;
   }
   else if (s->state == RPYS_SEND_READY) h->record.kind = RPYROWAVE_STREAM_READY;
   else if (s->state == RPYS_SEND_REJECT)
   {
      h->record.kind = RPYROWAVE_STREAM_REJECT;
      h->payload = h->record.bytes = 4;
      s->write_payload = s->control;
   }
   else if (s->sender && s->state == RPYS_ACTIVE)
   {
      rpyrowave_stream_lend_t *lend = s->audio.active ? &s->audio : &s->video;
      if (lend->active)
      {
         h->record = lend->record;
         h->offset = lend->offset;
         h->payload = lend->record.bytes - lend->offset;
         if (h->record.kind == RPYROWAVE_STREAM_VIDEO)
         {
            if (h->payload > RPYROWAVE_STREAM_FRAGMENT_BYTES) h->payload = RPYROWAVE_STREAM_FRAGMENT_BYTES;
            h->flags = (!h->offset ? 1U : 0U) | (h->offset + h->payload == h->record.bytes ? 2U : 0U);
         }
         else h->flags = lend->flags;
         s->write_payload = lend->data + lend->offset;
      }
      else if (s->finishing) h->record.kind = RPYROWAVE_STREAM_END;
   }
   if (h->record.kind)
   {
      rpyrowave_stream_pack_header(s->write_header, h);
      s->write_header_bytes = s->write_payload_bytes = 0;
   }
}

static void rpyrowave_stream_write_complete(rpyrowave_stream_t *s)
{
   unsigned kind = s->outgoing.record.kind;
   if (kind == RPYROWAVE_STREAM_VIDEO || kind == RPYROWAVE_STREAM_AUDIO)
   {
      rpyrowave_stream_lend_t *lend = kind == RPYROWAVE_STREAM_VIDEO ? &s->video : &s->audio;
      lend->offset += s->outgoing.payload;
      if (lend->offset == lend->record.bytes)
      {
         lend->active = 0;
         s->callbacks.sent(s->callbacks.user, &lend->record, lend->data, lend->token, 1);
         lend->data = NULL;
      }
   }
   else if (kind == RPYROWAVE_STREAM_CONFIG) s->state = RPYS_WAIT_READY;
   else if (kind == RPYROWAVE_STREAM_READY)
   {
      s->state = RPYS_ACTIVE;
      rpyrowave_stream_event(s, RPYROWAVE_STREAM_STARTED, 0);
   }
   else if (kind == RPYROWAVE_STREAM_END)
   {
      s->state = RPYS_DONE;
      rpyrowave_stream_event(s, RPYROWAVE_STREAM_ENDED, 0);
   }
   else if (kind == RPYROWAVE_STREAM_REJECT)
   {
      s->state = RPYS_REJECTED;
      rpyrowave_stream_event(s, RPYROWAVE_STREAM_REJECTED, s->reason);
   }
   s->outgoing.record.kind = 0;
}

static int rpyrowave_stream_write_step(rpyrowave_stream_t *s)
{
   size_t wanted;
   const uint8_t *data;
   int written;
   rpyrowave_stream_select_write(s);
   if (!s->outgoing.record.kind) return 0;
   if (s->write_header_bytes < RPYROWAVE_STREAM_HEADER_BYTES)
   {
      data = s->write_header + s->write_header_bytes;
      wanted = RPYROWAVE_STREAM_HEADER_BYTES - s->write_header_bytes;
   }
   else
   {
      data = s->write_payload + s->write_payload_bytes;
      wanted = s->outgoing.payload - s->write_payload_bytes;
   }
   written = s->io.write(s->io.user, data, wanted);
   if (written < 0 || (size_t)written > wanted) return rpyrowave_stream_fail(s, RPYROWAVE_STREAM_IO_ERROR);
   if (!written) return 0;
   if (s->write_header_bytes < RPYROWAVE_STREAM_HEADER_BYTES) s->write_header_bytes += written;
   else s->write_payload_bytes += written;
   if (s->write_header_bytes == RPYROWAVE_STREAM_HEADER_BYTES && s->write_payload_bytes == s->outgoing.payload)
      rpyrowave_stream_write_complete(s);
   return 1;
}

static int rpyrowave_stream_acquire(rpyrowave_stream_t *s, rpyrowave_stream_lend_t *lend,
      const rpyrowave_stream_record_t *record)
{
   void *data = NULL;
   size_t capacity = 0;
   int result;
   lend->record = *record;
   lend->offset = 0;
   lend->data = NULL;
   lend->dropped = 0;
   result = s->callbacks.acquire(s->callbacks.user, record, &data, &capacity);
   if (result < 0) return rpyrowave_stream_fail(s, RPYROWAVE_STREAM_SINK_ERROR);
   lend->active = 1;
   lend->dropped = result == 0;
   lend->data = (const uint8_t*)data;
   if (result && (!data || capacity < record->bytes)) return rpyrowave_stream_fail(s, RPYROWAVE_STREAM_SINK_ERROR);
   return 1;
}

static int rpyrowave_stream_receive_header(rpyrowave_stream_t *s)
{
   rpyrowave_stream_header_t *h = &s->incoming;
   unsigned kind = h->record.kind;
   if (kind == RPYROWAVE_STREAM_CONFIG)
      return !s->sender && s->state == RPYS_WAIT_CONFIG ? 1 : rpyrowave_stream_fail(s, RPYROWAVE_STREAM_BAD_WIRE);
   if (h->record.generation != s->generation) return rpyrowave_stream_fail(s, RPYROWAVE_STREAM_BAD_WIRE);
   if (s->sender)
      return s->state == RPYS_WAIT_READY && (kind == RPYROWAVE_STREAM_READY || kind == RPYROWAVE_STREAM_REJECT)
         ? 1 : rpyrowave_stream_fail(s, RPYROWAVE_STREAM_BAD_WIRE);
   if (s->state != RPYS_ACTIVE) return rpyrowave_stream_fail(s, RPYROWAVE_STREAM_BAD_WIRE);
   if (kind == RPYROWAVE_STREAM_END)
      return !s->video.active ? 1 : rpyrowave_stream_fail(s, RPYROWAVE_STREAM_BAD_WIRE);
   if (kind == RPYROWAVE_STREAM_VIDEO)
   {
      if (h->record.bytes > s->config.maximum_frame_bytes) return rpyrowave_stream_fail(s, RPYROWAVE_STREAM_BAD_WIRE);
      if (!h->offset)
      {
         if (s->video.active || h->record.serial <= s->last_video || h->record.timestamp < s->last_timestamp)
            return rpyrowave_stream_fail(s, RPYROWAVE_STREAM_BAD_WIRE);
         return rpyrowave_stream_acquire(s, &s->video, &h->record);
      }
      if (!s->video.active || h->offset != s->video.offset || h->record.serial != s->video.record.serial
            || h->record.timestamp != s->video.record.timestamp || h->record.duration != s->video.record.duration
            || h->record.bytes != s->video.record.bytes) return rpyrowave_stream_fail(s, RPYROWAVE_STREAM_BAD_WIRE);
      return 1;
   }
   if (kind == RPYROWAVE_STREAM_AUDIO)
   {
      uint64_t end;
      if (!rpyrowave_stream_audio_valid(&s->config, &h->record, s->next_audio, &end)
            || h->flags != (h->record.serial > s->next_audio ? 4U : 0U))
         return rpyrowave_stream_fail(s, RPYROWAVE_STREAM_BAD_WIRE);
      return rpyrowave_stream_acquire(s, &s->audio, &h->record);
   }
   return rpyrowave_stream_fail(s, RPYROWAVE_STREAM_BAD_WIRE);
}

static int rpyrowave_stream_commit(rpyrowave_stream_t *s, rpyrowave_stream_lend_t *lend)
{
   if (lend->dropped)
   {
      if (s->callbacks.dropped) s->callbacks.dropped(s->callbacks.user, &lend->record);
   }
   else if (s->callbacks.commit(s->callbacks.user, &lend->record, (void*)lend->data) != 1)
      return rpyrowave_stream_fail(s, RPYROWAVE_STREAM_SINK_ERROR);
   lend->active = 0;
   lend->data = NULL;
   return 1;
}

static int rpyrowave_stream_read_complete(rpyrowave_stream_t *s)
{
   rpyrowave_stream_header_t *h = &s->incoming;
   unsigned kind = h->record.kind;
   if (kind == RPYROWAVE_STREAM_CONFIG)
   {
      if (rpyrowave_stream_unpack_config(s->control, &s->config)) return rpyrowave_stream_fail(s, RPYROWAVE_STREAM_BAD_WIRE);
      s->generation = h->record.generation;
      s->state = RPYS_WAIT_ACCEPT;
      rpyrowave_stream_event(s, RPYROWAVE_STREAM_CONFIGURE, 0);
   }
   else if (kind == RPYROWAVE_STREAM_READY)
   {
      s->state = RPYS_ACTIVE;
      rpyrowave_stream_event(s, RPYROWAVE_STREAM_STARTED, 0);
   }
   else if (kind == RPYROWAVE_STREAM_REJECT)
   {
      s->reason = rpyrowave_stream_load32(s->control);
      if (!s->reason) return rpyrowave_stream_fail(s, RPYROWAVE_STREAM_BAD_WIRE);
      s->state = RPYS_REJECTED;
      rpyrowave_stream_event(s, RPYROWAVE_STREAM_REJECTED, s->reason);
   }
   else if (kind == RPYROWAVE_STREAM_END)
   {
      s->state = RPYS_DONE;
      rpyrowave_stream_event(s, RPYROWAVE_STREAM_ENDED, 0);
   }
   else if (kind == RPYROWAVE_STREAM_VIDEO)
   {
      s->video.offset += h->payload;
      if (s->video.offset == s->video.record.bytes)
      {
         s->last_video = s->video.record.serial;
         s->last_timestamp = s->video.record.timestamp;
         if (rpyrowave_stream_commit(s, &s->video) < 0) return -1;
      }
   }
   else
   {
      s->next_audio = h->record.serial + h->record.bytes / (s->config.audio_channels * 2);
      if (rpyrowave_stream_commit(s, &s->audio) < 0) return -1;
   }
   s->read_header_bytes = s->read_payload_bytes = 0;
   return 1;
}

static int rpyrowave_stream_read_step(rpyrowave_stream_t *s)
{
   uint8_t *data;
   size_t wanted;
   int received;
   if (s->read_header_bytes < RPYROWAVE_STREAM_HEADER_BYTES)
   {
      data = s->read_header + s->read_header_bytes;
      wanted = RPYROWAVE_STREAM_HEADER_BYTES - s->read_header_bytes;
   }
   else
   {
      rpyrowave_stream_lend_t *lend = s->incoming.record.kind == RPYROWAVE_STREAM_VIDEO ? &s->video : &s->audio;
      wanted = s->incoming.payload - s->read_payload_bytes;
      if (s->incoming.record.kind == RPYROWAVE_STREAM_CONFIG || s->incoming.record.kind == RPYROWAVE_STREAM_REJECT)
         data = s->control + s->read_payload_bytes;
      else if (lend->dropped)
      {
         data = s->discard;
         if (wanted > sizeof(s->discard)) wanted = sizeof(s->discard);
      }
      else data = (uint8_t*)lend->data + lend->offset + s->read_payload_bytes;
   }
   received = s->io.read(s->io.user, data, wanted);
   if (received == -2) return rpyrowave_stream_fail(s, RPYROWAVE_STREAM_DISCONNECTED);
   if (received < 0 || (size_t)received > wanted) return rpyrowave_stream_fail(s, RPYROWAVE_STREAM_IO_ERROR);
   if (!received) return 0;
   if (s->read_header_bytes < RPYROWAVE_STREAM_HEADER_BYTES)
   {
      s->read_header_bytes += received;
      if (s->read_header_bytes == RPYROWAVE_STREAM_HEADER_BYTES)
      {
         if (rpyrowave_stream_unpack_header(s->read_header, &s->incoming))
            return rpyrowave_stream_fail(s, RPYROWAVE_STREAM_BAD_WIRE);
         if (rpyrowave_stream_receive_header(s) < 0) return -1;
         if (!s->incoming.payload) return rpyrowave_stream_read_complete(s);
      }
   }
   else
   {
      s->read_payload_bytes += received;
      if (s->read_payload_bytes == s->incoming.payload) return rpyrowave_stream_read_complete(s);
   }
   return 1;
}

int rpyrowave_stream_pump(rpyrowave_stream_t *s)
{
   unsigned iteration;
   unsigned progressed = 0;
   if (!s || s->state == RPYS_FAILED) return RPYROWAVE_STREAM_ERROR;
   if (s->state >= RPYS_DONE) return RPYROWAVE_STREAM_FINISHED;
   for (iteration = 0; iteration < 8; iteration++)
   {
      int written = rpyrowave_stream_write_step(s);
      int received;
      if (written < 0) return RPYROWAVE_STREAM_ERROR;
      if (s->state >= RPYS_DONE) break;
      received = rpyrowave_stream_read_step(s);
      if (received < 0) return RPYROWAVE_STREAM_ERROR;
      if (written || received) progressed = 1;
      if (s->state >= RPYS_DONE || (!written && !received)) break;
   }
   return s->state >= RPYS_DONE ? RPYROWAVE_STREAM_FINISHED
      : progressed ? RPYROWAVE_STREAM_PROGRESS : RPYROWAVE_STREAM_BUSY;
}

int rpyrowave_stream_finish(rpyrowave_stream_t *s)
{
   if (!s || !s->sender || s->state != RPYS_ACTIVE) return RPYROWAVE_STREAM_ERROR;
   s->finishing = 1;
   return RPYROWAVE_STREAM_PROGRESS;
}

int rpyrowave_stream_wants_write(const rpyrowave_stream_t *s)
{
   if (!s || s->state >= RPYS_DONE) return 0;
   return s->outgoing.record.kind || s->state == RPYS_SEND_CONFIG
      || s->state == RPYS_SEND_READY || s->state == RPYS_SEND_REJECT
      || (s->state == RPYS_ACTIVE && s->sender && (s->video.active || s->audio.active || s->finishing));
}

void rpyrowave_stream_abort(rpyrowave_stream_t *s)
{
   if (!s || s->state >= RPYS_DONE) return;
   s->state = RPYS_CANCELLED;
   rpyrowave_stream_release_all(s);
   rpyrowave_stream_event(s, RPYROWAVE_STREAM_CANCELLED, 0);
}

void rpyrowave_stream_free(rpyrowave_stream_t *s)
{
   rpyrowave_stream_abort(s);
   free(s);
}
