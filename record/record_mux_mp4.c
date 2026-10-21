/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_mux_mp4.h"
#include <stdlib.h>
#include <string.h>
#include <formats/rav1_sequence.h>
#include <formats/rac3.h>

#define RMP4W_LIMIT (64 * 1024 * 1024)
#define RMP4W_HEADER 65536
struct record_mux_mp4
{
   uint64_t end[2];
   uint8_t *data;
   size_t capacity, size, pos, limit;
   uint32_t sequence;
   unsigned rate, channels, ac3_mode;
   enum record_mp4_codec codec;
   bool failed, pending, finished, seen[2];
};
typedef struct rmp4w_nal
{
   const uint8_t *data;
   size_t size;
} rmp4w_nal_t;

static void rmp4w_bytes(record_mux_mp4_t *m, const void *data, size_t size)
{
   if (m->failed || size > m->capacity - m->pos)
   {
      m->failed = true;
      return;
   }
   if (size) memcpy(m->data + m->pos, data, size);
   m->pos += size;
}
static void rmp4w_uint(record_mux_mp4_t *m, uint64_t value, unsigned bytes)
{
   uint8_t buffer[8];
   unsigned i;
   for (i = 0; i < bytes; i++) buffer[bytes - i - 1] = (uint8_t)(value >> (i * 8));
   rmp4w_bytes(m, buffer, bytes);
}
static void rmp4w_zero(record_mux_mp4_t *m, unsigned count)
{
   static const uint8_t zero[32] = {0};
   while (count)
   {
      unsigned n = count > sizeof(zero) ? sizeof(zero) : count;
      rmp4w_bytes(m, zero, n); count -= n;
   }
}
static size_t rmp4w_box(record_mux_mp4_t *m, const char *type)
{
   size_t start = m->pos;
   rmp4w_uint(m, 0, 4); rmp4w_bytes(m, type, 4);
   return start;
}
static void rmp4w_end(record_mux_mp4_t *m, size_t start)
{
   size_t end = m->pos;
   if (m->failed || start > end || end - start > UINT32_MAX)
   {
      m->failed = true;
      return;
   }
   m->pos = start; rmp4w_uint(m, end - start, 4); m->pos = end;
}
static size_t rmp4w_full(record_mux_mp4_t *m, const char *type, uint32_t flags)
{
   size_t start = rmp4w_box(m, type);
   rmp4w_uint(m, flags, 4);
   return start;
}
static void rmp4w_matrix(record_mux_mp4_t *m)
{
   unsigned i;
   for (i = 0; i < 9; i++) rmp4w_uint(m, i == 8 ? 0x40000000 : i % 4 == 0 ? 0x10000 : 0, 4);
}

/* Annex B start codes and trailing_zero_8bits are outside the NAL. */
static size_t rmp4w_start(const uint8_t *data, size_t size, size_t offset, size_t *length)
{
   size_t i;
   for (i = offset; i + 3 <= size; i++)
      if (!data[i] && !data[i+1])
      {
         if (data[i+2] == 1)
         {
            *length = 3;
            return i;
         }
         if (i + 4 <= size && !data[i+2] && data[i+3] == 1)
         {
            *length = 4;
            return i;
         }
      }
   return size;
}
static int rmp4w_nal_next(const uint8_t *data, size_t size, size_t *offset, rmp4w_nal_t *nal)
{
   size_t prefix, start, next, end, unused;
   if (*offset == size) return 0;
   start = rmp4w_start(data, size, *offset, &prefix);
   if (start == size) return -1;
   for (next = *offset; next < start; next++) if (data[next]) return -1;
   start += prefix;
   next = rmp4w_start(data, size, start, &unused); end = next;
   while (end > start && !data[end-1]) end--;
   if (end == start || end - start > UINT32_MAX || (data[start] & 0x80)) return -1;
   nal->data = data + start; nal->size = end - start; *offset = next;
   return 1;
}

static bool rmp4w_av1_config(record_mux_mp4_t *m, const record_mp4_config_t *c)
{
   rav1_obu_t obu, sequence;
   rav1_sequence_t info;
   size_t offset = 0, box;
   int result;
   memset(&sequence, 0, sizeof(sequence));
   while ((result = rav1_obu_next(&obu, (const uint8_t*)c->headers, c->headers_size, &offset, false)) > 0)
      if (obu.type == 1)
      {
         if (sequence.data) return false;
         sequence = obu;
      }
   if (result < 0 || !sequence.data || !rav1_sequence_parse(&info, sequence.payload, sequence.payload_size)
         || info.bit_depth != c->bit_depth || info.monochrome
         || !info.subsampling_x || !info.subsampling_y
         || info.max_width != c->width || info.max_height != c->height) return false;
   if (c->color_primaries && (c->color_primaries != info.color_primaries
         || c->transfer_characteristics != info.transfer_characteristics
         || c->matrix_coefficients != info.matrix_coefficients
         || c->full_range != !!info.color_range)) return false;
   box = rmp4w_box(m, "av1C");
   rmp4w_uint(m, 0x81, 1);
   rmp4w_uint(m, (info.profile << 5) | info.operating_points[0].level, 1);
   rmp4w_uint(m, (info.operating_points[0].tier << 7) | ((info.bit_depth > 8) << 6)
         | (info.subsampling_x << 3) | (info.subsampling_y << 2) | info.chroma_sample_position, 1);
   rmp4w_uint(m, 0, 1); rmp4w_bytes(m, sequence.data, sequence.size);
   rmp4w_end(m, box);
   box = rmp4w_box(m, "colr"); rmp4w_bytes(m, "nclx", 4);
   rmp4w_uint(m, info.color_primaries, 2); rmp4w_uint(m, info.transfer_characteristics, 2);
   rmp4w_uint(m, info.matrix_coefficients, 2); rmp4w_uint(m, info.color_range ? 128 : 0, 1);
   rmp4w_end(m, box);
   return !m->failed;
}
static bool rmp4w_nal_config(record_mux_mp4_t *m, const record_mp4_config_t *c)
{
   rmp4w_nal_t nal, sets[3];
   size_t offset = 0, box;
   unsigned i, kind, count = c->codec == RECORD_MP4_H264 ? 2 : 3;
   int result;
   memset(sets, 0, sizeof(sets));
   while ((result = rmp4w_nal_next((const uint8_t*)c->headers, c->headers_size, &offset, &nal)) > 0)
   {
      if (c->codec == RECORD_MP4_H264)
      {
         kind = nal.data[0] & 31;
         if (kind != 7 && kind != 8) continue;
         i = kind == 7 ? 0 : 1;
      }
      else
      {
         if (nal.size < 2 || !(nal.data[1] & 7)) return false;
         kind = (nal.data[0] >> 1) & 63;
         if (kind < 32 || kind > 34) continue;
         i = kind - 32;
      }
      if (sets[i].data || nal.size > 65535) return false;
      sets[i] = nal;
   }
   if (result < 0) return false;
   for (i = 0; i < count; i++) if (!sets[i].data) return false;
   if (c->codec == RECORD_MP4_H264)
   {
      unsigned profile;
      if (c->bit_depth != 8 || sets[0].size < 4 || sets[1].size < 2) return false;
      profile = sets[0].data[1];
      box = rmp4w_box(m, "avcC");
      rmp4w_uint(m, 1, 1); rmp4w_bytes(m, sets[0].data + 1, 3);
      rmp4w_uint(m, 255, 1); rmp4w_uint(m, 225, 1);
      rmp4w_uint(m, sets[0].size, 2); rmp4w_bytes(m, sets[0].data, sets[0].size);
      rmp4w_uint(m, 1, 1); rmp4w_uint(m, sets[1].size, 2); rmp4w_bytes(m, sets[1].data, sets[1].size);
      if (profile == 100 || profile == 110 || profile == 122 || profile == 144)
      {
         rmp4w_uint(m, 253, 1); rmp4w_uint(m, 248, 1); rmp4w_uint(m, 248, 1); rmp4w_uint(m, 0, 1);
      }
   }
   else
   {
      uint8_t ptl[13];
      unsigned got = 0, zeros = 0;
      for (offset = 2; offset < sets[1].size && got < sizeof(ptl); offset++)
      {
         unsigned byte = sets[1].data[offset];
         if (zeros >= 2 && byte == 3)
         {
            if (offset + 1 == sets[1].size || sets[1].data[offset+1] > 3) return false;
            zeros = 0; continue;
         }
         ptl[got++] = (uint8_t)byte; zeros = byte ? 0 : zeros + 1;
      }
      if (got != sizeof(ptl) || ((ptl[0] >> 1) & 7) == 7) return false;
      box = rmp4w_box(m, "hvcC");
      rmp4w_uint(m, 1, 1); rmp4w_bytes(m, ptl + 1, 12);
      rmp4w_uint(m, 0xf000, 2); rmp4w_uint(m, 0xfc, 1);
      rmp4w_uint(m, 0xfd, 1); rmp4w_uint(m, 0xf8 | (c->bit_depth - 8), 1);
      rmp4w_uint(m, 0xf8 | (c->bit_depth - 8), 1); rmp4w_uint(m, 0, 2);
      rmp4w_uint(m, ((((ptl[0] >> 1) & 7) + 1) << 3) | ((ptl[0] & 1) << 2) | 3, 1);
      rmp4w_uint(m, 3, 1);
      for (i = 0; i < 3; i++)
      {
         rmp4w_uint(m, 0x80 | (32 + i), 1); rmp4w_uint(m, 1, 2);
         rmp4w_uint(m, sets[i].size, 2); rmp4w_bytes(m, sets[i].data, sets[i].size);
      }
   }
   rmp4w_end(m, box);
   return !m->failed;
}

static bool rmp4w_ac3(record_mux_mp4_t *m, const uint8_t *p, size_t size, unsigned *frame_size)
{
   rac3_frame_info_t info;
   unsigned fscod, code;
   if (rac3_parse_frame_info(p, size, &info) != RAC3_OK
         || info.kind != RAC3_KIND_AC3 || info.bsid > 8) return false;
   fscod = p[4] >> 6; code = (p[4] & 63) >> 1;
   *frame_size = info.frame_bytes;
   m->rate = info.sample_rate; m->channels = info.channels;
   m->ac3_mode = (fscod << 22) | (info.bsid << 17) | (info.bsmod << 14)
      | (info.acmod << 11) | (info.lfe << 10) | (code << 5);
   return true;
}

static bool rmp4w_track(record_mux_mp4_t *m, const record_mp4_config_t *c, unsigned track)
{
   bool audio = track == 2;
   size_t trak, box, mdia, minf, stbl, stsd, entry, dinf, dref;
   trak = rmp4w_box(m, "trak");
   box = rmp4w_full(m, "tkhd", 7);
   rmp4w_zero(m, 8); rmp4w_uint(m, track, 4); rmp4w_zero(m, 16);
   rmp4w_zero(m, 4); rmp4w_uint(m, audio ? 0x100 : 0, 2); rmp4w_zero(m, 2);
   rmp4w_matrix(m); rmp4w_uint(m, audio ? 0 : c->width << 16, 4);
   rmp4w_uint(m, audio ? 0 : c->height << 16, 4); rmp4w_end(m, box);
   mdia = rmp4w_box(m, "mdia");
   box = rmp4w_full(m, "mdhd", 0); rmp4w_zero(m, 8);
   rmp4w_uint(m, audio ? m->rate : c->timescale, 4); rmp4w_zero(m, 4);
   rmp4w_uint(m, 0x55c4, 2); rmp4w_zero(m, 2); rmp4w_end(m, box);
   box = rmp4w_full(m, "hdlr", 0); rmp4w_zero(m, 4);
   rmp4w_bytes(m, audio ? "soun" : "vide", 4); rmp4w_zero(m, 12);
   rmp4w_bytes(m, audio ? "Audio" : "Video", 6); rmp4w_end(m, box);
   minf = rmp4w_box(m, "minf");
   box = rmp4w_full(m, audio ? "smhd" : "vmhd", audio ? 0 : 1);
   rmp4w_zero(m, audio ? 4 : 8); rmp4w_end(m, box);
   dinf = rmp4w_box(m, "dinf"); dref = rmp4w_full(m, "dref", 0); rmp4w_uint(m, 1, 4);
   box = rmp4w_full(m, "url ", 1); rmp4w_end(m, box); rmp4w_end(m, dref); rmp4w_end(m, dinf);
   stbl = rmp4w_box(m, "stbl"); stsd = rmp4w_full(m, "stsd", 0); rmp4w_uint(m, 1, 4);
   entry = rmp4w_box(m, audio ? "ac-3" : c->codec == RECORD_MP4_H264 ? "avc3" : c->codec == RECORD_MP4_HEVC ? "hev1" : "av01");
   rmp4w_zero(m, 6); rmp4w_uint(m, 1, 2);
   if (audio)
   {
      rmp4w_zero(m, 8); rmp4w_uint(m, m->channels, 2); rmp4w_uint(m, 16, 2);
      rmp4w_zero(m, 4); rmp4w_uint(m, m->rate << 16, 4);
      box = rmp4w_box(m, "dac3"); rmp4w_uint(m, m->ac3_mode, 3); rmp4w_end(m, box);
   }
   else
   {
      rmp4w_zero(m, 16); rmp4w_uint(m, c->width, 2); rmp4w_uint(m, c->height, 2);
      rmp4w_uint(m, 0x480000, 4); rmp4w_uint(m, 0x480000, 4);
      rmp4w_zero(m, 4); rmp4w_uint(m, 1, 2); rmp4w_zero(m, 32);
      rmp4w_uint(m, 24, 2); rmp4w_uint(m, 65535, 2);
      if (!(c->codec == RECORD_MP4_AV1 ? rmp4w_av1_config(m, c) : rmp4w_nal_config(m, c))) return false;
      if (c->codec != RECORD_MP4_AV1 && c->color_primaries)
      {
         box = rmp4w_box(m, "colr"); rmp4w_bytes(m, "nclx", 4);
         rmp4w_uint(m, c->color_primaries, 2);
         rmp4w_uint(m, c->transfer_characteristics, 2);
         rmp4w_uint(m, c->matrix_coefficients, 2);
         rmp4w_uint(m, c->full_range ? 128 : 0, 1);
         rmp4w_end(m, box);
      }
   }
   rmp4w_end(m, entry); rmp4w_end(m, stsd);
   box = rmp4w_full(m, "stts", 0); rmp4w_zero(m, 4); rmp4w_end(m, box);
   box = rmp4w_full(m, "stsc", 0); rmp4w_zero(m, 4); rmp4w_end(m, box);
   box = rmp4w_full(m, "stsz", 0); rmp4w_zero(m, 8); rmp4w_end(m, box);
   box = rmp4w_full(m, "stco", 0); rmp4w_zero(m, 4); rmp4w_end(m, box);
   rmp4w_end(m, stbl); rmp4w_end(m, minf); rmp4w_end(m, mdia); rmp4w_end(m, trak);
   return !m->failed;
}

record_mux_mp4_t *record_mux_mp4_new(const record_mp4_config_t *c)
{
   record_mux_mp4_t *m;
   size_t box, moov, mvex;
   unsigned i, bytes;
   if (!c || !c->headers || !c->headers_size || c->headers_size > RMP4W_HEADER
         || !c->max_packet_size || c->max_packet_size > RMP4W_LIMIT
         || c->codec < RECORD_MP4_H264 || c->codec > RECORD_MP4_AV1
         || !c->width || !c->height || c->width > 16384 || c->height > 16384
         || !c->timescale || (c->bit_depth != 8 && c->bit_depth != 10)
         || (!c->ac3_header && c->ac3_header_size)) return NULL;
   if (c->color_primaries || c->transfer_characteristics
         || c->matrix_coefficients || c->full_range)
      if (!c->color_primaries || c->color_primaries > 65535
            || !c->transfer_characteristics || c->transfer_characteristics > 65535
            || !c->matrix_coefficients || c->matrix_coefficients > 65535)
         return NULL;
   m = (record_mux_mp4_t*)calloc(1, sizeof(*m));
   if (!m) return NULL;
   m->capacity = c->max_packet_size + 4096 + c->headers_size;
   m->limit = c->max_packet_size; m->codec = c->codec;
   m->data = (uint8_t*)malloc(m->capacity);
   if (!m->data || (c->ac3_header && !rmp4w_ac3(m, (const uint8_t*)c->ac3_header, c->ac3_header_size, &bytes))) goto fail;
   box = rmp4w_box(m, "ftyp"); rmp4w_bytes(m, "iso6", 4); rmp4w_uint(m, 1, 4);
   rmp4w_bytes(m, "iso6mp41", 8);
   if (c->codec == RECORD_MP4_AV1) rmp4w_bytes(m, "av01", 4);
   rmp4w_end(m, box);
   moov = rmp4w_box(m, "moov"); box = rmp4w_full(m, "mvhd", 0);
   rmp4w_zero(m, 8); rmp4w_uint(m, 1000, 4); rmp4w_zero(m, 4);
   rmp4w_uint(m, 0x10000, 4); rmp4w_uint(m, 0x100, 2); rmp4w_zero(m, 10);
   rmp4w_matrix(m); rmp4w_zero(m, 24); rmp4w_uint(m, m->rate ? 3 : 2, 4); rmp4w_end(m, box);
   if (!rmp4w_track(m, c, 1) || (m->rate && !rmp4w_track(m, c, 2))) goto fail;
   mvex = rmp4w_box(m, "mvex");
   for (i = 1; i <= (m->rate ? 2u : 1u); i++)
   {
      box = rmp4w_full(m, "trex", 0); rmp4w_uint(m, i, 4);
      rmp4w_uint(m, 1, 4); rmp4w_zero(m, 12); rmp4w_end(m, box);
   }
   rmp4w_end(m, mvex); rmp4w_end(m, moov);
   if (m->failed) goto fail;
   m->size = m->pos; m->pending = true; return m;
fail:
   record_mux_mp4_free(m); return NULL;
}

int record_mux_mp4_push(record_mux_mp4_t *m, unsigned track, const void *input,
      size_t size, uint64_t timestamp, uint32_t duration, bool keyframe)
{
   const uint8_t *data = (const uint8_t*)input;
   size_t moof, traf, box, sample_at, data_at, mdat, offset;
   unsigned ac3_size;
   int result;
   rmp4w_nal_t nal;
   if (!m || m->finished) return RECORD_MP4_ERROR;
   if (m->pending) return RECORD_MP4_BUSY;
   if (!data || !size || size > m->limit || track < 1 || track > (m->rate ? 2u : 1u)
         || !duration || timestamp > UINT64_MAX - duration || m->sequence == UINT32_MAX
         || (m->seen[track-1] && timestamp < m->end[track-1])
         || (track == 1 && !m->seen[0] && !keyframe)) return RECORD_MP4_ERROR;
   if (track == 2)
   {
      record_mux_mp4_t parsed;
      memset(&parsed, 0, sizeof(parsed));
      if (!rmp4w_ac3(&parsed, data, size, &ac3_size) || ac3_size != size || duration != 1536
            || parsed.ac3_mode != m->ac3_mode) return RECORD_MP4_ERROR;
   }
   m->pos = 0; m->failed = false;
   moof = rmp4w_box(m, "moof"); box = rmp4w_full(m, "mfhd", 0);
   rmp4w_uint(m, m->sequence + 1, 4); rmp4w_end(m, box);
   traf = rmp4w_box(m, "traf"); box = rmp4w_full(m, "tfhd", 0x20000);
   rmp4w_uint(m, track, 4); rmp4w_end(m, box);
   box = rmp4w_full(m, "tfdt", 0x1000000); rmp4w_uint(m, timestamp, 8); rmp4w_end(m, box);
   box = rmp4w_full(m, "trun", 0x701); rmp4w_uint(m, 1, 4);
   data_at = m->pos; rmp4w_zero(m, 4); rmp4w_uint(m, duration, 4);
   sample_at = m->pos; rmp4w_zero(m, 4);
   rmp4w_uint(m, track == 2 || keyframe ? 0x2000000 : 0x1010000, 4);
   rmp4w_end(m, box); rmp4w_end(m, traf); rmp4w_end(m, moof);
   mdat = rmp4w_box(m, "mdat"); offset = 0;
   if (track == 2) rmp4w_bytes(m, data, size);
   else if (m->codec == RECORD_MP4_AV1)
   {
      rav1_obu_t obu;
      while ((result = rav1_obu_next(&obu, data, size, &offset, false)) > 0)
      {
         if (obu.type == 2 || obu.type == 15) continue;
         if (obu.type == 0 || obu.type >= 8) return RECORD_MP4_ERROR;
         rmp4w_bytes(m, obu.data, obu.size);
      }
      if (result < 0) return RECORD_MP4_ERROR;
   }
   else
   {
      while ((result = rmp4w_nal_next(data, size, &offset, &nal)) > 0)
      {
         if (m->codec == RECORD_MP4_HEVC && (nal.size < 2 || !(nal.data[1] & 7))) return RECORD_MP4_ERROR;
         rmp4w_uint(m, nal.size, 4); rmp4w_bytes(m, nal.data, nal.size);
      }
      if (result < 0) return RECORD_MP4_ERROR;
   }
   rmp4w_end(m, mdat);
   if (m->failed || m->pos == mdat + 8 || m->pos - mdat - 8 > m->limit) return RECORD_MP4_ERROR;
   m->size = m->pos; m->pos = data_at; rmp4w_uint(m, mdat + 8, 4);
   m->pos = sample_at; rmp4w_uint(m, m->size - mdat - 8, 4);
   m->pos = m->size;
   m->sequence++; m->end[track-1] = timestamp + duration;
   m->seen[track-1] = m->pending = true;
   return RECORD_MP4_OK;
}
bool record_mux_mp4_get(record_mux_mp4_t *m, const void **data, size_t *size)
{
   if (!m || !data || !size || !m->pending) return false;
   *data = m->data; *size = m->size; return true;
}
bool record_mux_mp4_release(record_mux_mp4_t *m)
{
   if (!m || !m->pending) return false;
   m->pending = false; return true;
}
bool record_mux_mp4_finish(record_mux_mp4_t *m)
{
   if (!m || m->pending) return false;
   m->finished = true; return true;
}
void record_mux_mp4_free(record_mux_mp4_t *m)
{
   if (!m) return;
   free(m->data); free(m);
}
