/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rsubtitle_srt.c).
 * ---------------------------------------------------------------------------------------
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
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include <formats/rsubtitle_srt.h>
#include <formats/rsubtitle_vtt.h>
#include "rsubtitle_vtt_internal.h"

typedef struct rsubtitle_srt_entry
{
   rsubtitle_srt_cue_t cue;
   size_t order;
} rsubtitle_srt_entry_t;

typedef struct rsubtitle_vtt_entry
{
   rsubtitle_vtt_cue_t cue;
   rsubtitle_vtt_settings_t settings;
   size_t first_node, node_count, region;
} rsubtitle_vtt_entry_t;

enum srt_stage
{
   SRT_NORMALIZE, SRT_LINES, SRT_CUES, SRT_SORT, SRT_INDEX, SRT_READY, SRT_ERROR,
   SRT_VTT_HEADER, SRT_VTT_TEXT, SRT_VTT_REGIONS, SRT_VTT_CSS
};

enum vtt_line_kind
{
   VTT_BODY, VTT_IDENTIFIER, VTT_TEXT, VTT_HEADER,
   VTT_SIGNATURE, VTT_SKIP, VTT_STYLE, VTT_REGION
};

struct rsubtitle_srt
{
   char *data;
   const char *vtt_identifier, *vtt_settings;
   rsubtitle_srt_t *vtt_header;
   rsubtitle_vtt_span_t *vtt_spans;
   rsubtitle_vtt_entry_t *vtt_cues;
   rsubtitle_vtt_text_t *vtt_text;
   rsubtitle_vtt_regions_t *vtt_regions;
   rsubtitle_vtt_css_tree_t *vtt_css;
   rsubtitle_vtt_block_t *vtt_blocks;
   rsubtitle_srt_entry_t *entries, *scratch;
   rsubtitle_srt_span_t *spans;
   int64_t *index;
   int64_t vtt_origin_ns;
   rsubtitle_srt_cue_t pending;
   size_t count, capacity, size, read, written;
   size_t scan, line_begin, text_begin, text_end;
   size_t width, run, left, left_end, right, right_end, output;
   size_t index_base, index_cursor;
   size_t span_count, span_cursor;
   size_t vtt_block_count, vtt_block_capacity, vtt_block_begin, vtt_text_cursor;
   size_t vtt_region_cursor, vtt_region_lookup;
   size_t vtt_css_cursor;
   enum srt_stage stage;
   unsigned line_kind, vtt, vtt_cue_seen, vtt_metadata, vtt_flags;
   int vtt_regions_done, vtt_region_started;
   int vtt_css_done, vtt_css_started;
   int vtt_text_started, vtt_text_done;
   int vtt_arrow;
   int line_blank, text_seen, unordered;
   char *language;
   size_t language_length;
   rsubtitle_vtt_normalize_t *language_normal;
   rsubtitle_vtt_language_key_t language_key;
   int language_set;
};

static unsigned srt_utf8_scalar(const char *data, size_t size)
{
   unsigned lead = (unsigned char)data[0];
   uint32_t value, minimum;
   unsigned remaining, length, i;
   if (lead && lead < 0x80)
      return 1;
   if (lead >= 0xc2 && lead <= 0xdf)
   {
      value = lead & 0x1f;
      minimum = 0x80;
      remaining = 1;
   }
   else if (lead >= 0xe0 && lead <= 0xef)
   {
      value = lead & 0xf;
      minimum = 0x800;
      remaining = 2;
   }
   else if (lead >= 0xf0 && lead <= 0xf4)
   {
      value = lead & 7;
      minimum = 0x10000;
      remaining = 3;
   }
   else
      return 0;
   length = remaining + 1;
   if (length > size)
      return 0;
   for (i = 1; i < length; i++)
   {
      unsigned next = (unsigned char)data[i];
      if ((next & 0xc0) != 0x80)
         return 0;
      value = (value << 6) | (next & 0x3f);
   }
   if (value < minimum || value > 0x10ffff
         || (value >= 0xd800 && value <= 0xdfff))
      return 0;
   return length;
}

static int srt_number(const char **cursor, unsigned minimum,
      unsigned maximum, uint64_t limit, uint64_t *value)
{
   const char *p = *cursor;
   uint64_t result = 0;
   unsigned count = 0;
   while (*p >= '0' && *p <= '9')
   {
      unsigned digit = (unsigned)(*p - '0');
      if (count == maximum || result > limit / 10
            || (result == limit / 10 && digit > limit % 10))
         return 0;
      result = result * 10 + digit;
      count++;
      p++;
   }
   if (count < minimum)
      return 0;
   *cursor = p;
   *value = result;
   return 1;
}

static int srt_time(const char **cursor, int64_t *time_us)
{
   const char *p = *cursor;
   uint64_t hours, minutes, seconds, fraction, ms;
   uint64_t limit = ((uint64_t)-1 >> 1) / 1000;
   if (!srt_number(&p, 2, 10, limit / 3600000, &hours) || *p != ':')
      return 0;
   p++;
   if (!srt_number(&p, 2, 2, 59, &minutes) || *p != ':')
      return 0;
   p++;
   if (!srt_number(&p, 2, 2, 59, &seconds)
         || (*p != ',' && *p != '.'))
      return 0;
   p++;
   if (!srt_number(&p, 3, 3, 999, &fraction))
      return 0;
   ms = ((hours * 60 + minutes) * 60 + seconds) * 1000 + fraction;
   if (ms > limit)
      return 0;
   *time_us = (int64_t)ms * 1000;
   *cursor = p;
   return 1;
}

static int srt_timing(const char *line, rsubtitle_srt_cue_t *cue)
{
   const char *p = line;
   while (*p == ' ' || *p == '\t')
      p++;
   if (!srt_time(&p, &cue->start_us))
      return 0;
   while (*p == ' ' || *p == '\t')
      p++;
   if (strncmp(p, "-->", 3))
      return 0;
   p += 3;
   while (*p == ' ' || *p == '\t')
      p++;
   if (!srt_time(&p, &cue->end_us) || cue->end_us <= cue->start_us)
      return 0;
   return !*p || *p == ' ' || *p == '\t';
}

static int srt_index(const char *line)
{
   const char *p = line;
   if (!*p)
      return 0;
   while (*p >= '0' && *p <= '9')
      p++;
   return !*p;
}

static int srt_append(rsubtitle_srt_t *track,
      const rsubtitle_srt_cue_t *cue)
{
   rsubtitle_srt_entry_t *entry;
   if (track->count == track->capacity)
   {
      size_t capacity = track->capacity ? track->capacity * 2 : 32;
      void *next;
      if (capacity > 65536 || capacity > (size_t)-1 / sizeof(*entry))
         return 0;
      next = realloc(track->entries, capacity * sizeof(*entry));
      if (!next)
         return 0;
      track->entries = (rsubtitle_srt_entry_t*)next;
      if (track->vtt)
      {
         next = realloc(track->vtt_cues, capacity * sizeof(*track->vtt_cues));
         if (!next)
            return 0;
         track->vtt_cues = (rsubtitle_vtt_entry_t*)next;
      }
      track->capacity = capacity;
   }
   if (track->count && track->entries[track->count - 1].cue.start_us > cue->start_us)
      track->unordered = 1;
   entry = &track->entries[track->count];
   entry->cue = *cue;
   if (track->vtt)
   {
      track->vtt_cues[track->count].cue.identifier = track->vtt_identifier;
      track->vtt_cues[track->count].cue.settings = track->vtt_settings;
      track->vtt_cues[track->count].cue.timestamp_origin_ns = track->vtt_origin_ns;
      track->vtt_cues[track->count].cue.flags = track->vtt_flags;
   }
   entry->order = track->count++;
   return 1;
}

static int srt_compare(const void *left, const void *right)
{
   const rsubtitle_srt_entry_t *a = (const rsubtitle_srt_entry_t*)left;
   const rsubtitle_srt_entry_t *b = (const rsubtitle_srt_entry_t*)right;
   if (a->cue.start_us != b->cue.start_us)
      return a->cue.start_us < b->cue.start_us ? -1 : 1;
   return a->order < b->order ? -1 : a->order != b->order;
}

rsubtitle_srt_t *rsubtitle_srt_begin(char *data, size_t size)
{
   rsubtitle_srt_t *track;
   char *storage;
   if (!data || !size || size > RSUBTITLE_SRT_MAX_INPUT)
   {
      free(data);
      return NULL;
   }
   storage = (char*)realloc(data, size + 1);
   if (!storage)
   {
      free(data);
      return NULL;
   }
   track = (rsubtitle_srt_t*)calloc(1, sizeof(*track));
   if (!track)
   {
      free(storage);
      return NULL;
   }
   track->data = storage;
   track->size = size;
   track->line_blank = 1;
   if (size >= 3 && (unsigned char)storage[0] == 0xef
         && (unsigned char)storage[1] == 0xbb && (unsigned char)storage[2] == 0xbf)
      track->read = 3;
   return track;
}

rsubtitle_srt_t *rsubtitle_vtt_begin(char *data, size_t size)
{
   rsubtitle_srt_t *track = rsubtitle_srt_begin(data, size);
   if (track)
   {
      track->vtt = 1;
      track->line_kind = VTT_SIGNATURE;
   }
   return track;
}

const rsubtitle_vtt_cue_t *rsubtitle_vtt_cue_info(
      const rsubtitle_srt_t *track, size_t index)
{
   if (!track || !track->vtt || track->stage != SRT_READY || index >= track->count)
      return NULL;
   return &track->vtt_cues[track->entries[index].order].cue;
}

size_t rsubtitle_vtt_region_count(const rsubtitle_srt_t *track)
{
   if (!track || !track->vtt || track->stage != SRT_READY) return 0;
   return track->vtt_header ? rsubtitle_vtt_region_count(track->vtt_header)
      : rsubtitle_vtt_regions_count(track->vtt_regions);
}

const rsubtitle_vtt_region_t *rsubtitle_vtt_region(
      const rsubtitle_srt_t *track, size_t index)
{
   if (index >= rsubtitle_vtt_region_count(track)) return NULL;
   return track->vtt_header ? rsubtitle_vtt_region(track->vtt_header, index)
      : rsubtitle_vtt_regions_get(track->vtt_regions, index);
}

const rsubtitle_vtt_region_t *rsubtitle_vtt_cue_region(
      const rsubtitle_srt_t *track, size_t cue)
{
   if (!rsubtitle_vtt_cue_info(track, cue)) return NULL;
   return rsubtitle_vtt_region(track, track->vtt_cues[track->entries[cue].order].region);
}

const rsubtitle_vtt_settings_t *rsubtitle_vtt_cue_settings(
      const rsubtitle_srt_t *track, size_t cue)
{
   if (!rsubtitle_vtt_cue_info(track, cue)) return NULL;
   return &track->vtt_cues[track->entries[cue].order].settings;
}

size_t rsubtitle_vtt_node_count(const rsubtitle_srt_t *track, size_t cue)
{
   if (!rsubtitle_vtt_cue_info(track, cue)) return 0;
   return track->vtt_cues[track->entries[cue].order].node_count;
}

const rsubtitle_vtt_node_t *rsubtitle_vtt_node(const rsubtitle_srt_t *track,
      size_t cue, size_t node)
{
   const rsubtitle_vtt_entry_t *entry;
   if (node >= rsubtitle_vtt_node_count(track, cue)) return NULL;
   entry = &track->vtt_cues[track->entries[cue].order];
   return rsubtitle_vtt_text_node(track->vtt_text, entry->first_node + node);
}

const char *rsubtitle_vtt_node_class(const rsubtitle_srt_t *track,
      size_t cue, size_t node, size_t index, size_t *length)
{
   const rsubtitle_vtt_entry_t *entry;
   if (length) *length = 0;
   if (node >= rsubtitle_vtt_node_count(track, cue)) return NULL;
   entry = &track->vtt_cues[track->entries[cue].order];
   return rsubtitle_vtt_text_class(track->vtt_text, entry->first_node + node, index, length);
}

int rsubtitle_vtt_take_language(rsubtitle_srt_t *track, char *language, size_t length)
{
   if (!track || !language || length > 65536 || track->language_set
         || track->stage == SRT_READY || track->stage == SRT_ERROR)
   {
      free(language);
      return 0;
   }
   track->language_normal = rsubtitle_vtt_normalize_new();
   if (!track->language_normal || !rsubtitle_vtt_normalize_begin(
            track->language_normal, language, length, RSUBTITLE_NORMALIZE_EXTLANG))
   {
      rsubtitle_vtt_normalize_free(track->language_normal);
      track->language_normal = NULL;
      free(language);
      return 0;
   }
   track->language = language;
   track->language_length = length;
   track->language_set = 1;
   return 1;
}

const char *rsubtitle_vtt_track_language(const rsubtitle_srt_t *track, size_t *length)
{
   if (length) *length = 0;
   if (!track || track->stage != SRT_READY) return NULL;
   if (length) *length = track->language_length;
   return track->language ? track->language : "";
}

const char *rsubtitle_vtt_track_language_key(const rsubtitle_srt_t *track, size_t *length)
{
   if (length) *length = 0;
   if (!track || track->stage != SRT_READY) return NULL;
   if (length) *length = track->language_key.length;
   return track->language_key.text ? track->language_key.text : "";
}

const char *rsubtitle_vtt_node_language(const rsubtitle_srt_t *track,
      size_t cue, size_t index, const char *fallback, size_t fallback_length,
      size_t *length)
{
   const rsubtitle_vtt_node_t *node = rsubtitle_vtt_node(track, cue, index);
   if (length)
      *length = 0;
   if (!node || (!fallback && fallback_length)
         || node->kind == RSUBTITLE_VTT_TEXT || node->kind == RSUBTITLE_VTT_TIMESTAMP)
      return NULL;
   if (node->language_node)
   {
      node = rsubtitle_vtt_node(track, cue, node->language_node - 1);
      if (!node || node->kind != RSUBTITLE_VTT_LANGUAGE)
         return NULL;
      if (length)
         *length = node->value_length;
      return node->value;
   }
   if (!fallback)
      return rsubtitle_vtt_track_language(track, length);
   if (length)
      *length = fallback_length;
   return fallback;
}

const char *rsubtitle_vtt_node_language_key(const rsubtitle_srt_t *track,
      size_t cue, size_t index, const char *fallback, size_t fallback_length,
      size_t *length)
{
   const rsubtitle_vtt_node_t *node = rsubtitle_vtt_node(track, cue, index);
   if (length) *length = 0;
   if (!node || (!fallback && fallback_length)
         || node->kind == RSUBTITLE_VTT_TEXT || node->kind == RSUBTITLE_VTT_TIMESTAMP)
      return NULL;
   if (node->language_node)
   {
      const rsubtitle_vtt_entry_t *entry;
      if (node->language_node > rsubtitle_vtt_node_count(track, cue)) return NULL;
      entry = &track->vtt_cues[track->entries[cue].order];
      return rsubtitle_vtt_text_language_key(track->vtt_text,
            entry->first_node + node->language_node - 1, length);
   }
   if (!fallback)
      return rsubtitle_vtt_track_language_key(track, length);
   if (length) *length = fallback_length;
   return fallback;
}

unsigned rsubtitle_vtt_node_state(const rsubtitle_srt_t *track,
      size_t cue, size_t index, int64_t position_us)
{
   const rsubtitle_vtt_node_t *node = rsubtitle_vtt_node(track, cue, index);
   const rsubtitle_vtt_node_t *timestamp;
   unsigned state = 0;
   if (!node || node->kind == RSUBTITLE_VTT_TEXT || node->kind == RSUBTITLE_VTT_TIMESTAMP)
      return 0;
   if (node->past_timestamp)
   {
      timestamp = rsubtitle_vtt_node(track, cue, node->past_timestamp - 1);
      if (timestamp && timestamp->timestamp_us < position_us) state |= RSUBTITLE_VTT_NODE_PAST;
   }
   if (node->future_timestamp)
   {
      timestamp = rsubtitle_vtt_node(track, cue, node->future_timestamp - 1);
      if (timestamp && timestamp->timestamp_us > position_us) state |= RSUBTITLE_VTT_NODE_FUTURE;
   }
   return state;
}

int rsubtitle_vtt_timestamp_us(const rsubtitle_vtt_cue_t *cue,
      int64_t inline_us, int64_t *time_us)
{
   int64_t origin = 0;
   uint64_t limit = ((uint64_t)-1) >> 1;
   if (!cue || !time_us || inline_us < 0
         || (cue->flags & ~RSUBTITLE_VTT_CUE_RELATIVE_TIMESTAMPS))
      return 0;
   if (cue->flags & RSUBTITLE_VTT_CUE_RELATIVE_TIMESTAMPS)
   {
      origin = cue->timestamp_origin_ns / 1000;
      if (cue->timestamp_origin_ns - origin * 1000 > 0)
         origin++;
   }
   if (origin > 0 && (uint64_t)inline_us > limit - (uint64_t)origin)
      return 0;
   *time_us = inline_us + origin;
   return 1;
}

size_t rsubtitle_vtt_css_node_count(const rsubtitle_srt_t *track)
{
   if (!track || !track->vtt || track->stage != SRT_READY) return 0;
   return track->vtt_header ? rsubtitle_vtt_css_node_count(track->vtt_header)
      : rsubtitle_vtt_css_tree_count(track->vtt_css);
}

const rsubtitle_vtt_css_node_t *rsubtitle_vtt_css_node(
      const rsubtitle_srt_t *track, size_t index)
{
   if (index >= rsubtitle_vtt_css_node_count(track)) return NULL;
   return track->vtt_header ? rsubtitle_vtt_css_node(track->vtt_header, index)
      : rsubtitle_vtt_css_tree_node(track->vtt_css, index);
}

size_t rsubtitle_vtt_block_count(const rsubtitle_srt_t *track)
{
   if (!track || !track->vtt || track->stage != SRT_READY)
      return 0;
   return track->vtt_header ? rsubtitle_vtt_block_count(track->vtt_header)
      : track->vtt_block_count;
}

const rsubtitle_vtt_block_t *rsubtitle_vtt_block(
      const rsubtitle_srt_t *track, size_t index)
{
   if (index >= rsubtitle_vtt_block_count(track))
      return NULL;
   return track->vtt_header ? rsubtitle_vtt_block(track->vtt_header, index)
      : &track->vtt_blocks[index];
}

rsubtitle_srt_t *rsubtitle_srt_begin_cues(char *data, size_t size,
      rsubtitle_srt_span_t *spans, size_t count)
{
   rsubtitle_srt_t *track;
   if (!data || !size || size > RSUBTITLE_SRT_MAX_INPUT
         || !spans || !count || count > 65536)
   {
      free(data);
      free(spans);
      return NULL;
   }
   track = (rsubtitle_srt_t*)calloc(1, sizeof(*track));
   if (!track)
   {
      free(data);
      free(spans);
      return NULL;
   }
   track->data = data;
   track->size = size;
   track->spans = spans;
   track->span_count = count;
   track->stage = SRT_CUES;
   return track;
}

rsubtitle_srt_t *rsubtitle_vtt_begin_cues(char *data, size_t size,
      rsubtitle_srt_span_t *spans, size_t count, rsubtitle_vtt_span_t *metadata,
      char *header, size_t header_size)
{
   rsubtitle_srt_t *track;
   if (!header || !header_size || size > RSUBTITLE_SRT_MAX_INPUT
         || header_size > RSUBTITLE_SRT_MAX_INPUT - size)
   {
      free(data);
      free(spans);
      free(metadata);
      free(header);
      return NULL;
   }
   track = rsubtitle_srt_begin_cues(data, size, spans, count);
   if (!track)
   {
      free(metadata);
      free(header);
      return NULL;
   }
   track->vtt = 1;
   track->vtt_spans = metadata;
   track->vtt_settings = "";
   track->vtt_header = rsubtitle_vtt_begin(header, header_size);
   if (!track->vtt_header)
   {
      rsubtitle_srt_free(track);
      return NULL;
   }
   track->stage = SRT_VTT_HEADER;
   return track;
}

static void srt_merge_run(rsubtitle_srt_t *track)
{
   track->left = track->run;
   track->left_end = track->run + track->width;
   if (track->left_end > track->count)
      track->left_end = track->count;
   track->right = track->left_end;
   track->right_end = track->right + track->width;
   if (track->right_end > track->count)
      track->right_end = track->count;
   track->output = track->run;
}

static int srt_complete_cue(rsubtitle_srt_t *track)
{
   if (!track->text_seen)
      return 0;
   track->pending.text = track->data + track->text_begin;
   track->pending.length = track->text_end - track->text_begin;
   if (track->pending.length > 65536 || !srt_append(track, &track->pending))
      return 0;
   track->data[track->text_end] = '\0';
   track->line_kind = 0;
   return 1;
}

static int srt_prepare_index(rsubtitle_srt_t *track)
{
   if (track->vtt && !track->vtt_css_done)
   {
      track->stage = SRT_VTT_CSS;
      return 1;
   }
   if (track->vtt && !track->vtt_regions_done)
   {
      track->stage = SRT_VTT_REGIONS;
      return 1;
   }
   if (track->vtt && track->count && !track->vtt_text_done)
   {
      track->stage = SRT_VTT_TEXT;
      return 1;
   }
   track->stage = SRT_INDEX;
   if (track->unordered)
   {
      track->scratch = (rsubtitle_srt_entry_t*)malloc(
            track->count * sizeof(*track->scratch));
      if (!track->scratch)
         return 0;
      track->stage = SRT_SORT;
      track->width = 1;
      srt_merge_run(track);
   }
   return 1;
}

static int vtt_time(const char **cursor, int64_t *time_us)
{
   const char *p = *cursor, *start = p;
   uint64_t first, second, seconds, fraction, hours = 0, minutes, ms;
   uint64_t limit = ((uint64_t)-1 >> 1) / 1000;
   size_t digits;
   if (!srt_number(&p, 1, 10, limit / 3600000, &first))
      return 0;
   digits = (size_t)(p - start);
   if (*p++ != ':' || !srt_number(&p, 2, 2, 59, &second))
      return 0;
   if (*p == ':')
   {
      hours = first;
      minutes = second;
      p++;
      if (!srt_number(&p, 2, 2, 59, &seconds))
         return 0;
   }
   else
   {
      if (digits != 2 || first > 59)
         return 0;
      minutes = first;
      seconds = second;
   }
   if (*p++ != '.' || !srt_number(&p, 3, 3, 999, &fraction))
      return 0;
   ms = ((hours * 60 + minutes) * 60 + seconds) * 1000 + fraction;
   if (ms > limit)
      return 0;
   *time_us = (int64_t)ms * 1000;
   *cursor = p;
   return 1;
}

static int vtt_timing(const char *line, rsubtitle_srt_cue_t *cue,
      const char **settings)
{
   const char *p = line;
   while (*p == ' ' || *p == '\t' || *p == '\f')
      p++;
   if (!vtt_time(&p, &cue->start_us))
      return 0;
   while (*p == ' ' || *p == '\t' || *p == '\f')
      p++;
   if (strncmp(p, "-->", 3))
      return 0;
   p += 3;
   while (*p == ' ' || *p == '\t' || *p == '\f')
      p++;
   if (!vtt_time(&p, &cue->end_us) || cue->end_us <= cue->start_us)
      return 0;
   while (*p == ' ' || *p == '\t' || *p == '\f')
      p++;
   *settings = p;
   return 1;
}

static int vtt_global(rsubtitle_srt_t *track)
{
   rsubtitle_vtt_block_t *block;
   if (track->vtt_block_count == track->vtt_block_capacity)
   {
      size_t capacity = track->vtt_block_capacity
         ? track->vtt_block_capacity * 2 : 16;
      void *next;
      if (capacity > 65536)
         return 0;
      next = realloc(track->vtt_blocks, capacity * sizeof(*track->vtt_blocks));
      if (!next)
         return 0;
      track->vtt_blocks = (rsubtitle_vtt_block_t*)next;
      track->vtt_block_capacity = capacity;
   }
   block = &track->vtt_blocks[track->vtt_block_count++];
   block->text = track->data + track->vtt_block_begin;
   block->length = track->text_end - track->vtt_block_begin;
   block->css_root = RSUBTITLE_VTT_NO_NODE;
   block->kind = track->line_kind == VTT_STYLE
      ? RSUBTITLE_VTT_STYLE : RSUBTITLE_VTT_REGION;
   track->data[track->text_end] = 0;
   track->line_kind = VTT_BODY;
   return 1;
}

static unsigned vtt_global_kind(const char *line)
{
   const char *tail;
   unsigned kind;
   if (!strncmp(line, "STYLE", 5))
   {
      tail = line + 5;
      kind = VTT_STYLE;
   }
   else if (!strncmp(line, "REGION", 6))
   {
      tail = line + 6;
      kind = VTT_REGION;
   }
   else
      return 0;
   while (*tail == ' ' || *tail == '\t')
      tail++;
   return *tail ? 0 : kind;
}

static int vtt_lines_step(rsubtitle_srt_t *track)
{
   size_t end = track->scan, next, length;
   char *line;
   if (end < track->written && track->data[end] != '\n')
   {
      if (end - track->line_begin >= 2 && track->data[end - 2] == '-'
            && track->data[end - 1] == '-' && track->data[end] == '>')
         track->vtt_arrow = 1;
      track->scan++;
      length = track->scan - track->line_begin;
      if (track->line_kind == VTT_SKIP)
         return length <= RSUBTITLE_SRT_MAX_INPUT;
      return length <= ((track->line_kind == VTT_TEXT
            || track->line_kind >= VTT_STYLE) ? 65536 : 128);
   }
   if (end == track->written && end == track->line_begin)
   {
      if (track->line_kind == VTT_SIGNATURE)
         return 0;
      if (track->line_kind == VTT_TEXT && track->text_seen && !srt_complete_cue(track))
         return 0;
      if (track->line_kind >= VTT_STYLE && !vtt_global(track))
         return 0;
      return srt_prepare_index(track);
   }
   next = end < track->written ? end + 1 : end;
   line = track->data + track->line_begin;
   length = end - track->line_begin;
   if (track->line_kind >= VTT_STYLE)
   {
      if (!length)
      {
         if (!vtt_global(track))
            return 0;
      }
      else
      {
         if (end - track->vtt_block_begin > 65536)
            return 0;
         if (track->vtt_arrow)
            track->line_kind = VTT_SKIP;
         track->text_end = end;
      }
   }
   else if (track->line_kind == VTT_SKIP)
   {
      if (!length)
         track->line_kind = VTT_BODY;
   }
   else if (track->line_kind == VTT_TEXT && !track->vtt_arrow)
   {
      if (!length)
      {
         if (track->text_seen && !srt_complete_cue(track))
            return 0;
         track->line_kind = VTT_BODY;
      }
      else
      {
         if (end - track->text_begin > 65536)
            return 0;
         track->text_seen = 1;
         track->text_end = end;
      }
   }
   else
   {
      if (track->line_kind == VTT_TEXT)
      {
         if (track->text_seen && !srt_complete_cue(track))
            return 0;
         track->line_kind = VTT_BODY;
      }
      track->data[end] = 0;
      if (track->line_kind == VTT_SIGNATURE)
      {
         if (length < 6 || memcmp(line, "WEBVTT", 6) || track->vtt_arrow
               || (line[6] && line[6] != ' ' && line[6] != '\t'))
               return 0;
         track->line_kind = VTT_HEADER;
      }
      else if (track->line_kind == VTT_HEADER)
      {
         if (!length)
            track->line_kind = VTT_BODY;
      }
      else if (!length)
         track->line_kind = VTT_BODY;
      else if (track->vtt_arrow)
      {
         if (track->line_kind != VTT_IDENTIFIER)
            track->vtt_identifier = NULL;
         if (!vtt_timing(line, &track->pending, &track->vtt_settings))
            track->line_kind = VTT_SKIP;
         else
         {
            track->vtt_cue_seen = 1;
            track->line_kind = VTT_TEXT;
            track->text_begin = track->text_end = next;
            track->text_seen = 0;
         }
      }
      else if (track->line_kind == VTT_IDENTIFIER)
         track->line_kind = VTT_SKIP;
      else
      {
         unsigned kind = vtt_global_kind(line);
         track->vtt_identifier = NULL;
         track->vtt_settings = NULL;
         if (!strncmp(line, "NOTE", 4)
               && (!line[4] || line[4] == ' ' || line[4] == '\t'))
            track->line_kind = VTT_SKIP;
         else if (kind)
         {
            track->line_kind = track->vtt_cue_seen ? VTT_SKIP : kind;
            track->vtt_block_begin = track->text_end = next;
         }
         else
         {
            track->vtt_identifier = line;
            track->line_kind = VTT_IDENTIFIER;
         }
      }
   }
   track->line_begin = track->scan = next;
   track->vtt_arrow = 0;
   return 1;
}


static int vtt_span_step(rsubtitle_srt_t *track)
{
   const rsubtitle_vtt_span_t *span = &track->vtt_spans[track->span_cursor];
   size_t offset = track->vtt_metadata ? span->settings_offset : span->identifier_offset;
   size_t size = track->vtt_metadata ? span->settings_length : span->identifier_length;
   const char *text;
   if ((span->flags & ~RSUBTITLE_VTT_CUE_RELATIVE_TIMESTAMPS)
         || size > 128 || offset >= track->size || size >= track->size - offset
         || track->data[offset + size])
      return 0;
   text = track->data + offset;
   if (track->scan < size)
   {
      unsigned length = srt_utf8_scalar(text + track->scan, size - track->scan);
      if (!length || text[track->scan] == '\n' || text[track->scan] == '\r'
            || (size - track->scan >= 3
               && !memcmp(text + track->scan, "-->", 3)))
         return 0;
      track->scan += length;
      return 1;
   }
   if (!track->vtt_metadata)
      track->vtt_identifier = size ? text : NULL;
   else
      track->vtt_settings = text;
   track->vtt_origin_ns = span->timestamp_origin_ns;
   track->vtt_flags = span->flags;
   track->vtt_metadata++;
   track->scan = 0;
   return 1;
}

static int srt_cues_step(rsubtitle_srt_t *track)
{
   const rsubtitle_srt_span_t *span;
   if (track->span_cursor == track->span_count)
   {
      free(track->spans);
      track->spans = NULL;
      free(track->vtt_spans);
      track->vtt_spans = NULL;
      return srt_prepare_index(track);
   }
   if (track->vtt_spans && track->vtt_metadata < 2)
      return vtt_span_step(track);
   span = &track->spans[track->span_cursor];
   if (!track->scan)
   {
      if (!span->length || span->length > 65536
            || span->offset >= track->size
            || span->length >= track->size - span->offset
            || track->data[span->offset + span->length]
            || span->end_us <= span->start_us)
         return 0;
      track->pending.start_us = span->start_us;
      track->pending.end_us = span->end_us;
      track->pending.text = track->data + span->offset;
      track->pending.length = span->length;
   }
   if (track->scan < span->length)
   {
      unsigned length = srt_utf8_scalar(track->pending.text + track->scan,
            span->length - track->scan);
      if (!length)
         return 0;
      track->scan += length;
      return 1;
   }
   if (!srt_append(track, &track->pending))
      return 0;
   track->span_cursor++;
   track->scan = 0;
   track->vtt_metadata = 0;
   return 1;
}

static int srt_lines_step(rsubtitle_srt_t *track)
{
   size_t end = track->scan;
   size_t next;
   char *line;
   if (end < track->written && track->data[end] != '\n')
   {
      if (track->data[end] != ' ' && track->data[end] != '\t')
         track->line_blank = 0;
      track->scan++;
      return track->scan - track->line_begin
         <= (track->line_kind == 2 ? 65536 : 128);
   }
   if (end == track->written && end == track->line_begin)
   {
      if ((track->line_kind == 2 && !srt_complete_cue(track))
            || track->line_kind == 1 || !track->count)
         return 0;
      return srt_prepare_index(track);
   }
   next = end < track->written ? end + 1 : end;
   line = track->data + track->line_begin;
   if (track->line_kind == 2)
   {
      if (track->line_blank)
      {
         if (!srt_complete_cue(track))
            return 0;
      }
      else
      {
         track->text_seen = 1;
         track->text_end = end;
         if (end - track->text_begin > 65536)
            return 0;
      }
   }
   else if (!track->line_blank || track->line_kind == 1)
   {
      track->data[end] = '\0';
      if (!track->line_kind && srt_index(line))
         track->line_kind = 1;
      else
      {
         if (!srt_timing(line, &track->pending))
            return 0;
         track->line_kind = 2;
         track->text_begin = track->text_end = next;
         track->text_seen = 0;
      }
   }
   track->line_begin = track->scan = next;
   track->line_blank = 1;
   return 1;
}

static void srt_sort_step(rsubtitle_srt_t *track)
{
   if (track->left == track->left_end && track->right == track->right_end)
   {
      track->run = track->right_end;
      if (track->run == track->count)
      {
         rsubtitle_srt_entry_t *old = track->entries;
         track->entries = track->scratch;
         track->scratch = old;
         if (track->width >= (track->count + 1) / 2)
         {
            free(track->scratch);
            track->scratch = NULL;
            track->stage = SRT_INDEX;
            return;
         }
         track->width *= 2;
         track->run = 0;
      }
      srt_merge_run(track);
      return;
   }
   if (track->left < track->left_end
         && (track->right == track->right_end
            || srt_compare(&track->entries[track->left],
               &track->entries[track->right]) <= 0))
      track->scratch[track->output++] = track->entries[track->left++];
   else
      track->scratch[track->output++] = track->entries[track->right++];
}

static void srt_vtt_css_step(rsubtitle_srt_t *track)
{
   if (!track->vtt_header && track->vtt_css_cursor < track->vtt_block_count)
   {
      rsubtitle_vtt_block_t *block = &track->vtt_blocks[track->vtt_css_cursor];
      if (block->kind != RSUBTITLE_VTT_STYLE || !block->length)
         track->vtt_css_cursor++;
      else if (!track->vtt_css)
      {
         track->vtt_css = rsubtitle_vtt_css_tree_new();
         if (!track->vtt_css) track->stage = SRT_ERROR;
      }
      else if (!track->vtt_css_started)
      {
         if (!rsubtitle_vtt_css_tree_begin(track->vtt_css,
                  block->text, block->length, &block->css_root))
            track->stage = SRT_ERROR;
         else track->vtt_css_started = 1;
      }
      else
      {
         int status = rsubtitle_vtt_css_tree_step(track->vtt_css);
         if (status < 0) track->stage = SRT_ERROR;
         else if (status)
         {
            track->vtt_css_cursor++;
            track->vtt_css_started = 0;
         }
      }
      return;
   }
   track->vtt_css_done = 1;
   if (!srt_prepare_index(track)) track->stage = SRT_ERROR;
}

static void srt_vtt_regions_step(rsubtitle_srt_t *track)
{
   if (!track->vtt_header && track->vtt_region_cursor < track->vtt_block_count)
   {
      const rsubtitle_vtt_block_t *block = &track->vtt_blocks[track->vtt_region_cursor];
      if (block->kind != RSUBTITLE_VTT_REGION || !block->length)
         track->vtt_region_cursor++;
      else if (!track->vtt_regions)
      {
         track->vtt_regions = rsubtitle_vtt_regions_new();
         if (!track->vtt_regions) track->stage = SRT_ERROR;
      }
      else if (!track->vtt_region_started)
      {
         if (!rsubtitle_vtt_regions_begin(track->vtt_regions, block->text, block->length))
            track->stage = SRT_ERROR;
         else track->vtt_region_started = 1;
      }
      else
      {
         int status = rsubtitle_vtt_regions_step(track->vtt_regions);
         if (status < 0) track->stage = SRT_ERROR;
         else if (status)
         {
            track->vtt_region_cursor++;
            track->vtt_region_started = 0;
         }
      }
      return;
   }
   track->vtt_regions_done = 1;
   if (!srt_prepare_index(track)) track->stage = SRT_ERROR;
}

static void srt_vtt_text_step(rsubtitle_srt_t *track)
{
   int status;
   if (!track->vtt_text)
   {
      track->vtt_text = rsubtitle_vtt_text_new();
      if (!track->vtt_text) track->stage = SRT_ERROR;
      return;
   }
   if (track->vtt_text_cursor < track->count)
   {
      const rsubtitle_srt_entry_t *cue = &track->entries[track->vtt_text_cursor];
      rsubtitle_vtt_entry_t *entry = &track->vtt_cues[cue->order];
      rsubtitle_vtt_regions_t *regions = track->vtt_header
         ? track->vtt_header->vtt_regions : track->vtt_regions;
      if (!track->vtt_text_started)
      {
         rsubtitle_vtt_settings_parse(entry->cue.settings, &entry->settings);
         entry->first_node = rsubtitle_vtt_text_count(track->vtt_text);
         entry->region = (size_t)-1;
         track->vtt_region_lookup = rsubtitle_vtt_regions_first(regions,
               entry->settings.region, entry->settings.region_length);
         track->vtt_text_started = 2;
      }
      else if (track->vtt_text_started == 2)
      {
         if (rsubtitle_vtt_regions_find_step(regions, entry->settings.region,
                  entry->settings.region_length, &track->vtt_region_lookup, &entry->region))
         {
            if (!rsubtitle_vtt_text_begin(track->vtt_text, cue->cue.text,
                     cue->cue.length, &entry->cue))
               track->stage = SRT_ERROR;
            else track->vtt_text_started = 1;
         }
      }
      else
      {
         status = rsubtitle_vtt_text_step(track->vtt_text);
         if (status < 0) track->stage = SRT_ERROR;
         else if (status)
         {
            entry->node_count = rsubtitle_vtt_text_count(track->vtt_text) - entry->first_node;
            track->vtt_text_started = 0;
            track->vtt_text_cursor++;
         }
      }
      return;
   }
   status = rsubtitle_vtt_text_seal(track->vtt_text);
   if (status < 0) track->stage = SRT_ERROR;
   else if (status)
   {
      track->vtt_text_done = 1;
      if (!srt_prepare_index(track)) track->stage = SRT_ERROR;
   }
}

int rsubtitle_srt_iterate(rsubtitle_srt_t *track, size_t work)
{
   if (!track)
      return -1;
   while (work-- && track->stage != SRT_READY && track->stage != SRT_ERROR)
   {
      if (track->language_normal)
      {
         int status = rsubtitle_vtt_normalize_step(track->language_normal);
         if (status < 0)
            track->stage = SRT_ERROR;
         else if (status)
         {
            track->language_key.text = track->language;
            track->language_key.length = track->language_length;
            if (status == 1 && !rsubtitle_vtt_normalize_take(
                     track->language_normal, &track->language_key))
               track->stage = SRT_ERROR;
            rsubtitle_vtt_normalize_free(track->language_normal);
            track->language_normal = NULL;
         }
         continue;
      }
      switch (track->stage)
      {
         case SRT_NORMALIZE:
            if (track->read < track->size)
            {
               unsigned i, length = srt_utf8_scalar(track->data + track->read,
                     track->size - track->read);
               if (!length)
               {
                  track->stage = SRT_ERROR;
                  break;
               }
               if (track->data[track->read] == '\r')
               {
                  track->read++;
                  if (track->read < track->size && track->data[track->read] == '\n')
                     track->read++;
                  track->data[track->written++] = '\n';
               }
               else
               {
                  for (i = 0; i < length; i++)
                     track->data[track->written++] = track->data[track->read++];
               }
            }
            else
            {
               track->data[track->written] = '\0';
               track->stage = SRT_LINES;
            }
            break;
         case SRT_LINES:
            if (!(track->vtt ? vtt_lines_step(track) : srt_lines_step(track)))
               track->stage = SRT_ERROR;
            break;
         case SRT_VTT_REGIONS:
            srt_vtt_regions_step(track);
            break;
         case SRT_VTT_CSS:
            srt_vtt_css_step(track);
            break;
         case SRT_VTT_TEXT:
            srt_vtt_text_step(track);
            break;
         case SRT_VTT_HEADER:
         {
            int status = rsubtitle_srt_iterate(track->vtt_header, 1);
            if (status < 0 || (status && rsubtitle_srt_count(track->vtt_header)))
               track->stage = SRT_ERROR;
            else if (status)
               track->stage = SRT_CUES;
            break;
         }
         case SRT_CUES:
            if (!srt_cues_step(track))
               track->stage = SRT_ERROR;
            break;
         case SRT_SORT:
            srt_sort_step(track);
            break;
         case SRT_INDEX:
            if (!track->index)
            {
               track->index_base = 1;
               while (track->index_base < track->count)
                  track->index_base *= 2;
               track->index_cursor = track->index_base * 2;
               track->index = (int64_t*)malloc(track->index_cursor
                     * sizeof(*track->index));
               if (!track->index)
                  track->stage = SRT_ERROR;
            }
            else if (track->index_cursor > 1)
            {
               size_t i = --track->index_cursor;
               if (i >= track->index_base)
               {
                  size_t cue = i - track->index_base;
                  track->index[i] = cue < track->count
                     ? track->entries[cue].cue.end_us : 0;
               }
               else
                  track->index[i] = track->index[2 * i] > track->index[2 * i + 1]
                     ? track->index[2 * i] : track->index[2 * i + 1];
            }
            else
               track->stage = SRT_READY;
            break;
         default:
            break;
      }
   }
   return track->stage == SRT_READY ? 1 : track->stage == SRT_ERROR ? -1 : 0;
}

rsubtitle_srt_t *rsubtitle_srt_take(char *data, size_t size)
{
   rsubtitle_srt_t *track = rsubtitle_srt_begin(data, size);
   int status;
   if (!track)
      return NULL;
   do
   {
      status = rsubtitle_srt_iterate(track, 65536);
   } while (!status);
   if (status < 0)
   {
      rsubtitle_srt_free(track);
      return NULL;
   }
   return track;
}

void rsubtitle_srt_free(rsubtitle_srt_t *track)
{
   if (!track)
      return;
   rsubtitle_srt_free(track->vtt_header);
   rsubtitle_vtt_text_free(track->vtt_text);
   rsubtitle_vtt_regions_free(track->vtt_regions);
   rsubtitle_vtt_css_tree_free(track->vtt_css);
   free(track->vtt_spans);
   free(track->vtt_cues);
   free(track->vtt_blocks);
   free(track->index);
   free(track->entries);
   free(track->scratch);
   free(track->spans);
   free(track->data);
   rsubtitle_vtt_normalize_free(track->language_normal);
   free(track->language_key.owned);
   free(track->language);
   free(track);
}

size_t rsubtitle_srt_count(const rsubtitle_srt_t *track)
{
   return track && track->stage == SRT_READY ? track->count : 0;
}

const rsubtitle_srt_cue_t *rsubtitle_srt_cue(
      const rsubtitle_srt_t *track, size_t index)
{
   return track && track->stage == SRT_READY && index < track->count
      ? &track->entries[index].cue : NULL;
}

size_t rsubtitle_srt_find_from(const rsubtitle_srt_t *track,
      int64_t time_us, size_t first)
{
   size_t count = rsubtitle_srt_count(track);
   size_t node;
   if (first >= count)
      return count;
   node = track->index_base + first;
   do
   {
      while (!(node & 1))
         node /= 2;
      if (track->index[node] > time_us)
      {
         while (node < track->index_base)
         {
            node *= 2;
            if (track->index[node] <= time_us)
               node++;
         }
         first = node - track->index_base;
         return first < count && track->entries[first].cue.start_us <= time_us
            ? first : count;
      }
      node++;
   } while (node & (node - 1));
   return count;
}

size_t rsubtitle_srt_find(const rsubtitle_srt_t *track, int64_t time_us)
{
   return rsubtitle_srt_find_from(track, time_us, 0);
}
