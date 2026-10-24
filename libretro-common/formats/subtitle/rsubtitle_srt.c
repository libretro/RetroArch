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

typedef struct rsubtitle_srt_entry
{
   rsubtitle_srt_cue_t cue;
   size_t order;
} rsubtitle_srt_entry_t;

enum srt_stage
{
   SRT_NORMALIZE, SRT_LINES, SRT_SORT, SRT_INDEX, SRT_READY, SRT_ERROR
};

struct rsubtitle_srt
{
   char *data;
   rsubtitle_srt_entry_t *entries, *scratch;
   int64_t *index;
   rsubtitle_srt_cue_t pending;
   size_t count, capacity, size, read, written;
   size_t scan, line_begin, text_begin, text_end;
   size_t width, run, left, left_end, right, right_end, output;
   size_t index_base, index_cursor;
   enum srt_stage stage;
   unsigned line_kind;
   int line_blank, text_seen, unordered;
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
      track->capacity = capacity;
   }
   if (track->count && track->entries[track->count - 1].cue.start_us > cue->start_us)
      track->unordered = 1;
   entry = &track->entries[track->count];
   entry->cue = *cue;
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

int rsubtitle_srt_iterate(rsubtitle_srt_t *track, size_t work)
{
   if (!track)
      return -1;
   while (work-- && track->stage != SRT_READY && track->stage != SRT_ERROR)
   {
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
            if (!srt_lines_step(track))
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
   free(track->index);
   free(track->entries);
   free(track->scratch);
   free(track->data);
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
