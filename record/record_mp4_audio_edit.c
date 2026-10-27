/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "record_mp4_audio_edit.h"
#include <string.h>
#define EDIT_LIMIT (64u * 1024u * 1024u)
#define EDIT_EXTRA 98u
#define EDIT_TAG(a, b, c, d) (((uint32_t)(a) << 24) \
      | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (d))
typedef struct edit_box
{
   const uint8_t *data;
   size_t bytes, header;
   uint32_t tag;
} edit_box_t;
typedef struct edit_context
{
   uint64_t skip, duration, movie_duration;
   edit_box_t root, audio, mdia, minf, stbl, tkhd, mvhd;
   uint8_t *output;
   size_t pos, capacity;
   uint32_t samples, scale, factor;
   unsigned roll;
   bool drop_audio;
} edit_context_t;

static uint64_t edit_get(const uint8_t *p, unsigned n)
{
   uint64_t value = 0;
   unsigned i;
   for (i = 0; i < n; i++)
      value = (value << 8) | p[i];
   return value;
}

static void edit_put(uint8_t *p, uint64_t value, unsigned n)
{
   unsigned i;
   for (i = 0; i < n; i++)
      p[n - i - 1] = (uint8_t)(value >> (i * 8));
}

static bool edit_box_read(const uint8_t *p, size_t available, edit_box_t *b)
{
   uint64_t size;
   if (available < 8)
      return false;
   size = edit_get(p, 4);
   b->header = size == 1 ? 16 : 8;
   if (available < b->header)
      return false;
   if (size == 1)
      size = edit_get(p + 8, 8);
   if (size < b->header || size > available)
      return false;
   b->data = p;
   b->bytes = (size_t)size;
   b->tag = (uint32_t)edit_get(p + 4, 4);
   return true;
}

static int edit_find(const edit_box_t *parent, uint32_t tag, edit_box_t *found)
{
   edit_box_t child;
   size_t pos = parent->header;
   unsigned count = 0;
   int seen = 0;
   while (pos < parent->bytes)
   {
      if (++count > 4096 || !edit_box_read(parent->data + pos,
               parent->bytes - pos, &child))
         return -1;
      if (child.tag == tag)
      {
         if (seen)
            return -1;
         *found = child;
         seen = 1;
      }
      pos += child.bytes;
   }
   return seen;
}

static bool edit_duration(const edit_box_t *b, bool track,
      size_t *offset, unsigned *width, uint64_t *value)
{
   const uint8_t *body = b->data + b->header;
   unsigned version;
   if (b->bytes == b->header)
      return false;
   version = body[0];
   if (version > 1)
      return false;
   *offset = b->header + (version ? (track ? 28 : 24) : (track ? 20 : 16));
   *width = version ? 8 : 4;
   if (*offset > b->bytes || *width > b->bytes - *offset)
      return false;
   *value = edit_get(b->data + *offset, *width);
   return true;
}

static bool edit_scale_track(const edit_box_t *track, unsigned factor,
      uint8_t *output)
{
   edit_box_t tkhd, edits, list;
   size_t offset, pos, step;
   unsigned width, i, count;
   uint64_t value, maximum;
   int found;
   if (edit_find(track, EDIT_TAG('t', 'k', 'h', 'd'), &tkhd) != 1
         || !edit_duration(&tkhd, true, &offset, &width, &value))
      return false;
   maximum = width == 4 ? (uint32_t)-1 : (uint64_t)-1;
   if (value > maximum / factor)
      return false;
   if (output)
      edit_put(output + (tkhd.data - track->data) + offset,
            value * factor, width);
   found = edit_find(track, EDIT_TAG('e', 'd', 't', 's'), &edits);
   if (found < 0)
      return false;
   if (!found)
      return true;
   if (edit_find(&edits, EDIT_TAG('e', 'l', 's', 't'), &list) != 1
         || list.bytes - list.header < 8 || list.data[list.header] > 1)
      return false;
   width = list.data[list.header] ? 8 : 4;
   step = 2 * width + 4;
   pos = list.header + 8;
   count = (unsigned)edit_get(list.data + list.header + 4, 4);
   if (count > 4096 || count != (list.bytes - pos) / step
         || (list.bytes - pos) % step)
      return false;
   maximum = width == 4 ? (uint32_t)-1 : (uint64_t)-1;
   for (i = 0; i < count; i++, pos += step)
   {
      value = edit_get(list.data + pos, width);
      if (value > maximum / factor)
         return false;
      if (output)
         edit_put(output + (list.data - track->data) + pos,
               value * factor, width);
   }
   return true;
}

static bool edit_prepare(edit_context_t *c, uint64_t frames)
{
   edit_box_t track, mdia, handler, mdhd, stsd, stsz, group;
   const uint8_t *body;
   size_t pos = c->root.header, offset;
   uint64_t value, whole, fraction;
   uint32_t movie_scale, rate, a, b, remainder;
   unsigned count = 0, width;
   bool found = false;
   if (edit_find(&c->root, EDIT_TAG('m', 'v', 'h', 'd'), &c->mvhd) != 1
         || !edit_duration(&c->mvhd, false, &offset, &width, &value))
      return false;
   movie_scale = (uint32_t)edit_get(c->mvhd.data + offset - 4, 4);
   if (!movie_scale)
      return false;
   c->movie_duration = 0;
   c->factor = 1;
   c->scale = movie_scale;
   while (pos < c->root.bytes)
   {
      if (++count > 4096 || !edit_box_read(c->root.data + pos,
               c->root.bytes - pos, &track))
         return false;
      pos += track.bytes;
      if (track.tag != EDIT_TAG('t', 'r', 'a', 'k'))
         continue;
      if (edit_find(&track, EDIT_TAG('m', 'd', 'i', 'a'), &mdia) != 1
            || edit_find(&mdia, EDIT_TAG('h', 'd', 'l', 'r'), &handler) != 1
            || handler.bytes - handler.header < 12)
         return false;
      if (edit_get(handler.data + handler.header + 8, 4)
            != EDIT_TAG('s', 'o', 'u', 'n'))
      {
         if (edit_find(&track, EDIT_TAG('t', 'k', 'h', 'd'), &group) != 1
               || !edit_duration(&group, true, &offset, &width, &value))
            return false;
         if (value > c->movie_duration)
            c->movie_duration = value;
         continue;
      }
      if (found)
         return false;
      found = true;
      c->audio = track;
      c->mdia = mdia;
      if (edit_find(&track, EDIT_TAG('e', 'd', 't', 's'), &group) != 0
            || edit_find(&track, EDIT_TAG('t', 'k', 'h', 'd'), &c->tkhd) != 1
            || edit_find(&mdia, EDIT_TAG('m', 'd', 'h', 'd'), &mdhd) != 1
            || edit_find(&mdia, EDIT_TAG('m', 'i', 'n', 'f'), &c->minf) != 1
            || edit_find(&c->minf, EDIT_TAG('s', 't', 'b', 'l'), &c->stbl) != 1
            || edit_find(&c->stbl, EDIT_TAG('s', 't', 's', 'd'), &stsd) != 1
            || edit_find(&c->stbl, EDIT_TAG('s', 't', 's', 'z'), &stsz) != 1
            || edit_find(&c->stbl, EDIT_TAG('s', 'g', 'p', 'd'), &group) != 0
            || edit_find(&c->stbl, EDIT_TAG('s', 'b', 'g', 'p'), &group) != 0)
         return false;
      if (stsd.bytes - stsd.header < 16 || stsz.bytes - stsz.header < 12)
         return false;
      body = stsd.data + stsd.header;
      if (edit_get(body + 4, 4) != 1
            || edit_get(body + 12, 4) != EDIT_TAG('m', 'p', '4', 'a'))
         return false;
      c->samples = (uint32_t)edit_get(stsz.data + stsz.header + 8, 4);
      if (!c->samples || c->roll > c->samples
            || !edit_duration(&mdhd, false, &offset, &width, &value)
            || (!c->drop_audio
                  && (c->skip > value || frames > value - c->skip)))
         return false;
      rate = (uint32_t)edit_get(mdhd.data + offset - 4, 4);
      if (!rate)
         return false;
      if ((frames % rate) * movie_scale % rate)
      {
         a = movie_scale;
         b = rate;
         while (b)
         {
            remainder = a % b;
            a = b;
            b = remainder;
         }
         c->factor = rate / a;
         if (movie_scale > (uint32_t)-1 / c->factor)
            return false;
         c->scale = movie_scale * c->factor;
      }
      whole = frames / rate;
      fraction = (frames % rate) * c->scale / rate;
      if (whole > ((uint64_t)-1 - fraction) / c->scale)
         return false;
      c->duration = whole * c->scale + fraction;
      if (!edit_duration(&c->tkhd, true, &offset, &width, &value)
            || (width == 4 && c->duration > (uint32_t)-1))
         return false;
   }
   if (c->movie_duration > (uint64_t)-1 / c->factor)
      return false;
   c->movie_duration *= c->factor;
   pos = c->root.header;
   while (pos < c->root.bytes)
   {
      if (!edit_box_read(c->root.data + pos, c->root.bytes - pos, &track))
         return false;
      if (track.tag == EDIT_TAG('t', 'r', 'a', 'k')
            && track.data != c->audio.data
            && !edit_scale_track(&track, c->factor, NULL))
         return false;
      if (c->factor != 1 && track.tag == EDIT_TAG('m', 'v', 'e', 'x'))
         return false;
      pos += track.bytes;
   }
   if (c->duration > c->movie_duration)
      c->movie_duration = c->duration;
   if (!found || (c->drop_audio && !c->movie_duration)
         || !edit_duration(&c->mvhd, false, &offset, &width, &value)
         || (width == 4 && c->movie_duration > (uint32_t)-1))
      return false;
   return true;
}

static bool edit_emit(edit_context_t *c, const void *data, size_t size)
{
   if (size > c->capacity - c->pos)
      return false;
   memcpy(c->output + c->pos, data, size);
   c->pos += size;
   return true;
}

static bool edit_add(edit_context_t *c, bool groups)
{
   uint8_t data[54];
   memset(data, 0, sizeof(data));
   if (groups)
   {
      edit_put(data, 26, 4);
      memcpy(data + 4, "sgpd", 4);
      data[8] = 1;
      memcpy(data + 12, "roll", 4);
      edit_put(data + 16, 2, 4);
      edit_put(data + 20, 1, 4);
      edit_put(data + 24, (uint16_t)(0u - c->roll), 2);
      edit_put(data + 26, 28, 4);
      memcpy(data + 30, "sbgp", 4);
      memcpy(data + 38, "roll", 4);
      edit_put(data + 42, 1, 4);
      edit_put(data + 46, c->samples, 4);
      edit_put(data + 50, 1, 4);
      return edit_emit(c, data, sizeof(data));
   }
   edit_put(data, 44, 4);
   memcpy(data + 4, "edts", 4);
   edit_put(data + 8, 36, 4);
   memcpy(data + 12, "elst", 4);
   data[16] = 1;
   edit_put(data + 20, 1, 4);
   edit_put(data + 24, c->duration, 8);
   edit_put(data + 32, c->skip, 8);
   edit_put(data + 40, 0x00010000u, 4);
   return edit_emit(c, data, 44);
}

static bool edit_write(edit_context_t *c, const edit_box_t *b, unsigned depth)
{
   size_t start = c->pos, pos = b->header, offset;
   unsigned width, count = 0;
   uint64_t value;
   edit_box_t child;
   bool parent = b->data == c->root.data || b->data == c->audio.data
         || b->data == c->mdia.data || b->data == c->minf.data
         || b->data == c->stbl.data;
   if (c->drop_audio && b->data == c->audio.data)
   {
      if (!edit_emit(c, b->data, b->bytes))
         return false;
      memcpy(c->output + start + 4, "free", 4);
      return true;
   }
   if (!parent)
   {
      if (!edit_emit(c, b->data, b->bytes))
         return false;
      if (b->tag == EDIT_TAG('t', 'r', 'a', 'k')
            && !edit_scale_track(b, c->factor, c->output + start))
         return false;
      if (b->data == c->tkhd.data || b->data == c->mvhd.data)
      {
         if (!edit_duration(b, b->data == c->tkhd.data, &offset, &width, &value))
            return false;
         edit_put(c->output + start + offset,
               b->data == c->tkhd.data ? c->duration : c->movie_duration, width);
         if (b->data == c->mvhd.data)
            edit_put(c->output + start + offset - 4, c->scale, 4);
      }
      return true;
   }
   if (depth > 4 || !edit_emit(c, b->data, b->header))
      return false;
   while (pos < b->bytes)
   {
      if (++count > 4096 || !edit_box_read(b->data + pos, b->bytes - pos, &child)
            || !edit_write(c, &child, depth + 1))
         return false;
      if (child.data == c->tkhd.data && !edit_add(c, false))
         return false;
      pos += child.bytes;
   }
   if (b->data == c->stbl.data && !edit_add(c, true))
      return false;
   if (b->header == 16)
      edit_put(c->output + start + 8, c->pos - start, 8);
   else
      edit_put(c->output + start, c->pos - start, 4);
   return true;
}

bool record_mp4_audio_edit(const void *moov, size_t bytes,
      uint64_t skip, uint64_t frames, unsigned roll,
      void *output, size_t capacity, size_t *written)
{
   edit_context_t c;
   size_t needed;
   uintptr_t in = (uintptr_t)moov, out = (uintptr_t)output;
   if (!written)
      return false;
   *written = 0;
   if (!moov || bytes > EDIT_LIMIT || skip >> 63
         || !roll || roll > 32768)
      return false;
   memset(&c, 0, sizeof(c));
   if (!edit_box_read((const uint8_t *)moov, bytes, &c.root)
         || c.root.bytes != bytes || c.root.tag != EDIT_TAG('m', 'o', 'o', 'v'))
      return false;
   c.skip = skip;
   c.roll = roll;
   c.drop_audio = frames == 0;
   needed = bytes + (c.drop_audio ? 0 : EDIT_EXTRA);
   if (!edit_prepare(&c, frames))
      return false;
   if (!output)
   {
      if (capacity)
         return false;
      *written = needed;
      return true;
   }
   if (capacity < needed || (out >= in ? out - in < bytes
            : in - out < needed))
      return false;
   c.output = (uint8_t *)output;
   c.capacity = capacity;
   if (!edit_write(&c, &c.root, 0) || c.pos != needed)
      return false;
   *written = c.pos;
   return true;
}
