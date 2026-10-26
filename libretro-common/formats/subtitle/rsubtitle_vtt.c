/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rsubtitle_vtt.c).
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


#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "rsubtitle_vtt_internal.h"
#include "rsubtitle_entities.h"

#define VTT_TEXT_LIMIT 262144u
#define VTT_TEXT_CHUNK 256u
#define VTT_POOL_CHUNK 65536u
#define VTT_LANGUAGE_BUCKETS 4096u

enum vtt_text_state
{
   VTT_TEXT_DATA, VTT_TEXT_TAG, VTT_TEXT_NAME, VTT_TEXT_CLASS_NAME,
   VTT_TEXT_ANNOTATION, VTT_TEXT_END, VTT_TEXT_TIME,
   VTT_TEXT_REFERENCE, VTT_TEXT_NUMBER_START, VTT_TEXT_NUMBER,
   VTT_TEXT_PENDING
};

enum vtt_seal_stage
{
   VTT_SEAL_HINTS, VTT_SEAL_SUFFIX, VTT_SEAL_RELATIVE
};

enum vtt_language_state
{
   VTT_LANGUAGE_NONE, VTT_LANGUAGE_HASH, VTT_LANGUAGE_LOOKUP,
   VTT_LANGUAGE_COMPARE, VTT_LANGUAGE_NORMALIZE
};

struct vtt_text_node
{
   rsubtitle_vtt_node_t view;
   const char *raw;
   size_t offset, classes, last_child;
   uint32_t subtree_end, suffix_timestamp;
   /* Sealed language nodes use storage as a prepared-key index. */
   unsigned depth, storage;
   char bytes[8];
};

struct vtt_text_class
{
   const char *text;
   size_t size;
};

struct vtt_text_language
{
   const char *raw;
   size_t raw_length;
   rsubtitle_vtt_language_key_t key;
   uint32_t hash, next;
};

struct rsubtitle_vtt_text
{
   struct vtt_text_node **nodes;
   struct vtt_text_class **classes;
   struct vtt_text_language **languages;
   rsubtitle_vtt_normalize_t *language_normal;
   uint32_t *language_buckets;
   char **pool;
   const char *input;
   rsubtitle_vtt_cue_t cue;
   size_t count, class_count, language_count, pool_size;
   size_t language_position;
   unsigned node_chunks, class_chunks, language_chunks, pool_chunks;
   size_t size, position, raw_start, base, current, seal, seal_class;
   size_t name_start, class_start, class_mark, pool_mark;
   size_t reference_start, match, match_end, entity_node;
   unsigned name_length, base_number, number_digits, pending_count, pending_index;
   uint32_t number, pending[2];
   uint32_t before_timestamp;
   uint32_t language_hash, language_lookup;
   enum vtt_text_state state, return_state;
   enum vtt_seal_stage seal_stage;
   enum vtt_language_state language_state;
   enum rsubtitle_vtt_node_kind kind;
   int active, sealed, annotation_space;
   char name[33];
};

#define VTT_REGION_LIMIT 65536u
#define VTT_REGION_CHUNK 256u
#define VTT_REGION_BUCKETS 4096u

enum vtt_region_setting
{
   VTT_REGION_UNKNOWN, VTT_REGION_ID, VTT_REGION_WIDTH, VTT_REGION_LINES,
   VTT_REGION_ANCHOR, VTT_REGION_VIEWPORT, VTT_REGION_SCROLL
};

struct vtt_region_entry
{
   rsubtitle_vtt_region_t view;
   uint32_t hash, next;
};

struct rsubtitle_vtt_regions
{
   struct vtt_region_entry **entries;
   uint32_t *buckets;
   const char *input;
   size_t count, size, position, token, colon, value;
   double number, decimal, place, first;
   uint32_t hash;
   unsigned chunks, lines, component;
   enum vtt_region_setting kind;
   int active, in_token, bad, digits, fraction, after_dot, percent;
};

static struct vtt_text_node *vtt_text_at(const rsubtitle_vtt_text_t *text, size_t index)
{
   return &text->nodes[index / VTT_TEXT_CHUNK][index % VTT_TEXT_CHUNK];
}

static struct vtt_text_class *vtt_text_class_at(const rsubtitle_vtt_text_t *text, size_t index)
{
   return &text->classes[index / VTT_TEXT_CHUNK][index % VTT_TEXT_CHUNK];
}

static struct vtt_text_language *vtt_text_language_at(
      const rsubtitle_vtt_text_t *text, size_t index)
{
   return &text->languages[index / VTT_TEXT_CHUNK][index % VTT_TEXT_CHUNK];
}

static char *vtt_text_pool_at(const rsubtitle_vtt_text_t *text, size_t offset)
{
   return text->pool[offset / VTT_POOL_CHUNK] + offset % VTT_POOL_CHUNK;
}

static int vtt_text_space(unsigned c)
{
   return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

static int vtt_text_alnum(unsigned c)
{
   return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
      || (c >= '0' && c <= '9');
}

static int vtt_setting_equal(const char *input, size_t size, const char *value)
{
   return strlen(value) == size && !memcmp(input, value, size);
}

static int vtt_setting_number(const char *input, size_t size, int percentage,
      double *value)
{
   double number = 0.0, decimal = 0.0, place = 0.1;
   size_t i = 0;
   int negative = 0, fraction = 0;
   if (percentage)
   {
      if (!size || input[size - 1] != '%') return 0;
      size--;
   }
   else if (size && input[0] == '-') { negative = 1; i++; }
   if (i == size || input[i] < '0' || input[i] > '9') return 0;
   for (; i < size; i++)
   {
      unsigned c = (unsigned char)input[i];
      if (c == '.' && !fraction && i + 1 < size
            && input[i + 1] >= '0' && input[i + 1] <= '9')
         fraction = 1;
      else if (c >= '0' && c <= '9')
      {
         if (fraction) { decimal += (c - '0') * place; place *= 0.1; }
         else number = number * 10.0 + c - '0';
      }
      else return 0;
   }
   number += decimal < 1.0 ? decimal : 1.0;
   if (percentage && number > 100.0) return 0;
   *value = negative ? -number : number;
   return 1;
}

static int vtt_setting_alignment(const char *input, size_t size, int position,
      enum rsubtitle_vtt_alignment *value)
{
   if (vtt_setting_equal(input, size, "center")) *value = RSUBTITLE_VTT_CENTER;
   else if (vtt_setting_equal(input, size, position ? "line-left" : "start"))
      *value = position ? RSUBTITLE_VTT_LEFT : RSUBTITLE_VTT_START;
   else if (vtt_setting_equal(input, size, position ? "line-right" : "end"))
      *value = position ? RSUBTITLE_VTT_RIGHT : RSUBTITLE_VTT_END;
   else return 0;
   return 1;
}

static void vtt_setting_apply(const char *name, size_t name_size,
      const char *value, size_t size, rsubtitle_vtt_settings_t *settings)
{
   double number;
   if (vtt_setting_equal(name, name_size, "region"))
   {
      settings->region = value;
      settings->region_length = size;
   }
   else if (vtt_setting_equal(name, name_size, "vertical"))
   {
      if (vtt_setting_equal(value, size, "rl")) settings->direction = RSUBTITLE_VTT_VERTICAL_RL;
      else if (vtt_setting_equal(value, size, "lr")) settings->direction = RSUBTITLE_VTT_VERTICAL_LR;
      if (settings->direction != RSUBTITLE_VTT_HORIZONTAL) settings->region = NULL;
   }
   else if (vtt_setting_equal(name, name_size, "line")
         || vtt_setting_equal(name, name_size, "position"))
   {
      const char *comma = (const char*)memchr(value, ',', size);
      size_t length = comma ? (size_t)(comma - value) : size;
      int position = name[0] == 'p';
      int percentage = length && value[length - 1] == '%';
      enum rsubtitle_vtt_alignment align = position ? settings->position_align : settings->line_align;
      if (!vtt_setting_number(value, length, position || percentage, &number)) return;
      if (comma && !vtt_setting_alignment(comma + 1, size - length - 1, position, &align)) return;
      if (position)
      {
         settings->position = number; settings->position_auto = 0;
         settings->position_align = align;
      }
      else
      {
         settings->line = number; settings->line_auto = 0;
         settings->snap_to_lines = !percentage; settings->line_align = align;
         settings->region = NULL;
      }
   }
   else if (vtt_setting_equal(name, name_size, "size"))
   {
      if (!vtt_setting_number(value, size, 1, &number)) return;
      settings->size = number;
      if (number != 100.0) settings->region = NULL;
   }
   else if (vtt_setting_equal(name, name_size, "align"))
   {
      if (vtt_setting_equal(value, size, "start")) settings->align = RSUBTITLE_VTT_START;
      else if (vtt_setting_equal(value, size, "center")) settings->align = RSUBTITLE_VTT_CENTER;
      else if (vtt_setting_equal(value, size, "end")) settings->align = RSUBTITLE_VTT_END;
      else if (vtt_setting_equal(value, size, "left")) settings->align = RSUBTITLE_VTT_LEFT;
      else if (vtt_setting_equal(value, size, "right")) settings->align = RSUBTITLE_VTT_RIGHT;
   }
   if (!settings->region) settings->region_length = 0;
}

void rsubtitle_vtt_settings_parse(const char *input, rsubtitle_vtt_settings_t *settings)
{
   const char *p = input ? input : "", *end = p;
   memset(settings, 0, sizeof(*settings));
   settings->size = 100.0;
   settings->align = RSUBTITLE_VTT_CENTER;
   settings->position_align = RSUBTITLE_VTT_AUTO;
   settings->line_auto = settings->position_auto = settings->snap_to_lines = 1;
   while (*end && end - p < 128) end++;
   while (p < end)
   {
      const char *token, *colon;
      while (p < end && vtt_text_space((unsigned char)*p)) p++;
      token = p;
      while (p < end && !vtt_text_space((unsigned char)*p)) p++;
      colon = (const char*)memchr(token, ':', (size_t)(p - token));
      if (colon && colon != token && colon + 1 != p)
         vtt_setting_apply(token, (size_t)(colon - token), colon + 1,
               (size_t)(p - colon - 1), settings);
   }
}

static struct vtt_region_entry *vtt_region_at(
      const rsubtitle_vtt_regions_t *regions, size_t index)
{
   return &regions->entries[index / VTT_REGION_CHUNK][index % VTT_REGION_CHUNK];
}

rsubtitle_vtt_regions_t *rsubtitle_vtt_regions_new(void)
{
   return (rsubtitle_vtt_regions_t*)calloc(1, sizeof(rsubtitle_vtt_regions_t));
}

void rsubtitle_vtt_regions_free(rsubtitle_vtt_regions_t *regions)
{
   unsigned i;
   if (!regions) return;
   for (i = 0; i < regions->chunks; i++) free(regions->entries[i]);
   free(regions->entries);
   free(regions->buckets);
   free(regions);
}

size_t rsubtitle_vtt_regions_count(const rsubtitle_vtt_regions_t *regions)
{
   return regions ? regions->count : 0;
}

const rsubtitle_vtt_region_t *rsubtitle_vtt_regions_get(
      const rsubtitle_vtt_regions_t *regions, size_t index)
{
   return regions && index < regions->count ? &vtt_region_at(regions, index)->view : NULL;
}

int rsubtitle_vtt_regions_begin(rsubtitle_vtt_regions_t *regions,
      const char *input, size_t size)
{
   struct vtt_region_entry *entry;
   if (!regions || !input || regions->active || size > 65536
         || regions->count == VTT_REGION_LIMIT) return 0;
   if (!regions->entries)
   {
      regions->entries = (struct vtt_region_entry**)calloc(
            VTT_REGION_LIMIT / VTT_REGION_CHUNK, sizeof(*regions->entries));
      if (!regions->entries) return 0;
   }
   if (!regions->buckets)
   {
      regions->buckets = (uint32_t*)calloc(VTT_REGION_BUCKETS, sizeof(*regions->buckets));
      if (!regions->buckets) return 0;
   }
   if (regions->count == (size_t)regions->chunks * VTT_REGION_CHUNK)
   {
      struct vtt_region_entry *chunk = (struct vtt_region_entry*)malloc(
            VTT_REGION_CHUNK * sizeof(*chunk));
      if (!chunk) return 0;
      regions->entries[regions->chunks++] = chunk;
   }
   entry = vtt_region_at(regions, regions->count++);
   memset(entry, 0, sizeof(*entry));
   entry->view.identifier = "";
   entry->view.width = entry->view.anchor_y = entry->view.viewport_y = 100.0;
   entry->view.lines = 3;
   regions->input = input; regions->size = size; regions->position = 0;
   regions->active = 1; regions->in_token = 0;
   return 1;
}

static enum vtt_region_setting vtt_region_kind(const char *name, size_t size)
{
   if (vtt_setting_equal(name, size, "id")) return VTT_REGION_ID;
   if (vtt_setting_equal(name, size, "width")) return VTT_REGION_WIDTH;
   if (vtt_setting_equal(name, size, "lines")) return VTT_REGION_LINES;
   if (vtt_setting_equal(name, size, "regionanchor")) return VTT_REGION_ANCHOR;
   if (vtt_setting_equal(name, size, "viewportanchor")) return VTT_REGION_VIEWPORT;
   if (vtt_setting_equal(name, size, "scroll")) return VTT_REGION_SCROLL;
   return VTT_REGION_UNKNOWN;
}

static void vtt_region_number_reset(rsubtitle_vtt_regions_t *regions)
{
   regions->number = regions->decimal = 0.0; regions->place = 0.1;
   regions->digits = regions->fraction = regions->after_dot = regions->percent = 0;
}

static void vtt_region_number(rsubtitle_vtt_regions_t *regions, unsigned c)
{
   if (c == ',' && (regions->kind == VTT_REGION_ANCHOR || regions->kind == VTT_REGION_VIEWPORT))
   {
      if (regions->component || !regions->percent || regions->number > 100.0)
         regions->bad = 1;
      regions->first = regions->number; regions->component = 1;
      vtt_region_number_reset(regions);
   }
   else if (c >= '0' && c <= '9' && !regions->percent)
   {
      if (regions->fraction)
      {
         regions->decimal += (c - '0') * regions->place;
         regions->place *= 0.1; regions->after_dot = 1;
      }
      else
      {
         regions->number = regions->number * 10.0 + c - '0';
         if (regions->number > 101.0) regions->number = 101.0;
      }
      regions->digits = 1;
   }
   else if (c == '.' && regions->digits && !regions->fraction && !regions->percent)
      regions->fraction = 1;
   else if (c == '%' && regions->digits && !regions->percent
         && (!regions->fraction || regions->after_dot))
   {
      regions->number += regions->decimal < 1.0 ? regions->decimal : 1.0;
      regions->percent = 1;
   }
   else regions->bad = 1;
}

static void vtt_region_token(rsubtitle_vtt_regions_t *regions, size_t end)
{
   struct vtt_region_entry *entry = vtt_region_at(regions, regions->count - 1);
   rsubtitle_vtt_region_t *view = &entry->view;
   const char *value = regions->input + regions->value;
   size_t size = end - regions->value;
   if (regions->colon == (size_t)-1 || regions->colon == regions->token || !size) return;
   switch (regions->kind)
   {
      case VTT_REGION_ID:
         view->identifier = value; view->identifier_length = size; entry->hash = regions->hash;
         break;
      case VTT_REGION_LINES:
         if (!regions->bad) view->lines = regions->lines;
         break;
      case VTT_REGION_WIDTH:
         if (!regions->bad && regions->percent && regions->number <= 100.0)
            view->width = regions->number;
         break;
      case VTT_REGION_ANCHOR:
      case VTT_REGION_VIEWPORT:
         if (!regions->bad && regions->component && regions->percent && regions->number <= 100.0)
         {
            if (regions->kind == VTT_REGION_ANCHOR)
            {
               view->anchor_x = regions->first; view->anchor_y = regions->number;
            }
            else
            {
               view->viewport_x = regions->first; view->viewport_y = regions->number;
            }
         }
         break;
      case VTT_REGION_SCROLL:
         if (vtt_setting_equal(value, size, "up")) view->scroll_up = 1;
         break;
      default: break;
   }
}

int rsubtitle_vtt_regions_step(rsubtitle_vtt_regions_t *regions)
{
   unsigned c;
   if (!regions || !regions->active) return -1;
   if (regions->position == regions->size)
   {
      struct vtt_region_entry *entry = vtt_region_at(regions, regions->count - 1);
      if (regions->in_token) vtt_region_token(regions, regions->position);
      if (entry->view.identifier_length)
      {
         unsigned bucket = entry->hash & (VTT_REGION_BUCKETS - 1);
         entry->next = regions->buckets[bucket];
         regions->buckets[bucket] = (uint32_t)regions->count;
      }
      regions->active = 0;
      return 1;
   }
   c = (unsigned char)regions->input[regions->position++];
   if (vtt_text_space(c))
   {
      if (regions->in_token) vtt_region_token(regions, regions->position - 1);
      regions->in_token = 0;
      return 0;
   }
   if (!regions->in_token)
   {
      regions->in_token = 1; regions->token = regions->position - 1;
      regions->colon = (size_t)-1; regions->value = regions->position;
      regions->kind = VTT_REGION_UNKNOWN; regions->hash = UINT32_C(2166136261);
      regions->bad = 0; regions->lines = regions->component = 0;
      vtt_region_number_reset(regions);
   }
   if (regions->colon == (size_t)-1)
   {
      if (c == ':')
      {
         regions->colon = regions->position - 1; regions->value = regions->position;
         regions->kind = vtt_region_kind(regions->input + regions->token,
               regions->colon - regions->token);
      }
      return 0;
   }
   if (regions->kind == VTT_REGION_ID)
      regions->hash = (regions->hash ^ c) * UINT32_C(16777619);
   else if (regions->kind == VTT_REGION_LINES)
   {
      if (c < '0' || c > '9') regions->bad = 1;
      else if (regions->lines > (UINT_MAX - (c - '0')) / 10) regions->lines = UINT_MAX;
      else regions->lines = regions->lines * 10 + c - '0';
   }
   else if (regions->kind == VTT_REGION_WIDTH || regions->kind == VTT_REGION_ANCHOR
         || regions->kind == VTT_REGION_VIEWPORT)
      vtt_region_number(regions, c);
   return 0;
}

size_t rsubtitle_vtt_regions_first(const rsubtitle_vtt_regions_t *regions,
      const char *identifier, size_t size)
{
   size_t i;
   uint32_t hash = UINT32_C(2166136261);
   if (!regions || !regions->buckets || !identifier || !size) return 0;
   for (i = 0; i < size; i++) hash = (hash ^ (unsigned char)identifier[i]) * UINT32_C(16777619);
   return regions->buckets[hash & (VTT_REGION_BUCKETS - 1)];
}

int rsubtitle_vtt_regions_find_step(const rsubtitle_vtt_regions_t *regions,
      const char *identifier, size_t size, size_t *cursor, size_t *found)
{
   const struct vtt_region_entry *entry;
   if (!regions || !identifier || !*cursor || *cursor > regions->count)
   {
      *found = (size_t)-1;
      return 1;
   }
   entry = vtt_region_at(regions, *cursor - 1);
   if (entry->view.identifier_length == size && !memcmp(entry->view.identifier, identifier, size))
   {
      *found = *cursor - 1;
      return 1;
   }
   *cursor = entry->next;
   return 0;
}

static enum rsubtitle_vtt_node_kind vtt_text_kind(const char *name, size_t size)
{
   if (size == 1)
      switch (*name)
      {
         case 'c': return RSUBTITLE_VTT_CLASS;
         case 'b': return RSUBTITLE_VTT_BOLD;
         case 'i': return RSUBTITLE_VTT_ITALIC;
         case 'u': return RSUBTITLE_VTT_UNDERLINE;
         case 'v': return RSUBTITLE_VTT_VOICE;
      }
   if (size == 2 && !memcmp(name, "rt", 2)) return RSUBTITLE_VTT_RUBY_TEXT;
   if (size == 4 && !memcmp(name, "ruby", 4)) return RSUBTITLE_VTT_RUBY;
   if (size == 4 && !memcmp(name, "lang", 4)) return RSUBTITLE_VTT_LANGUAGE;
   return RSUBTITLE_VTT_ROOT;
}

static size_t vtt_text_add(rsubtitle_vtt_text_t *text,
      enum rsubtitle_vtt_node_kind kind)
{
   struct vtt_text_node *node, *parent = NULL;
   size_t index = text->count;
   unsigned depth = kind == RSUBTITLE_VTT_ROOT ? 0
      : vtt_text_at(text, text->base + text->current)->depth + 1;
   if (depth > 128 || index == VTT_TEXT_LIMIT) return RSUBTITLE_VTT_NO_NODE;
   if (!text->nodes)
   {
      text->nodes = (struct vtt_text_node**)calloc(VTT_TEXT_LIMIT / VTT_TEXT_CHUNK,
            sizeof(*text->nodes));
      if (!text->nodes) return RSUBTITLE_VTT_NO_NODE;
   }
   if (index == (size_t)text->node_chunks * VTT_TEXT_CHUNK)
   {
      struct vtt_text_node *chunk = (struct vtt_text_node*)malloc(VTT_TEXT_CHUNK * sizeof(*node));
      if (!chunk) return RSUBTITLE_VTT_NO_NODE;
      text->nodes[text->node_chunks++] = chunk;
   }
   node = vtt_text_at(text, index);
   memset(node, 0, sizeof(*node));
   node->raw = "";
   node->depth = depth;
   node->view.kind = kind;
   node->view.parent = kind == RSUBTITLE_VTT_ROOT ? RSUBTITLE_VTT_NO_NODE : text->current;
   node->view.first_child = node->view.next = node->last_child = RSUBTITLE_VTT_NO_NODE;
   if (kind != RSUBTITLE_VTT_ROOT)
   {
      parent = vtt_text_at(text, text->base + text->current);
      if (parent->last_child != RSUBTITLE_VTT_NO_NODE)
         vtt_text_at(text, parent->last_child)->view.next = index - text->base;
      else
         parent->view.first_child = index - text->base;
      parent->last_child = index;
   }
   text->count++;
   return index;
}

static int vtt_text_raw(rsubtitle_vtt_text_t *text, size_t start, size_t size)
{
   size_t index;
   if (!size) return 1;
   index = vtt_text_add(text, RSUBTITLE_VTT_TEXT);
   if (index == RSUBTITLE_VTT_NO_NODE) return 0;
   vtt_text_at(text, index)->raw = text->input + start;
   vtt_text_at(text, index)->view.value_length = size;
   return 1;
}

static int vtt_text_pool(rsubtitle_vtt_text_t *text, const char *bytes, size_t size)
{
   size_t offset, used;
   if (size > RSUBTITLE_SRT_MAX_INPUT - text->pool_size) return 0;
   if (!text->pool)
   {
      text->pool = (char**)calloc(RSUBTITLE_SRT_MAX_INPUT / VTT_POOL_CHUNK, sizeof(*text->pool));
      if (!text->pool) return 0;
   }
   offset = text->pool_size % VTT_POOL_CHUNK;
   used = text->pool_size - text->pool_mark;
   if (text->pool_size / VTT_POOL_CHUNK == text->pool_chunks
         || offset + size > VTT_POOL_CHUNK)
   {
      char *chunk;
      int move = offset + size > VTT_POOL_CHUNK
            || (text->pool_size / VTT_POOL_CHUNK == text->pool_chunks && used);
      if (text->pool_chunks == RSUBTITLE_SRT_MAX_INPUT / VTT_POOL_CHUNK
            || (move && used + size > VTT_POOL_CHUNK)) return 0;
      chunk = (char*)malloc(VTT_POOL_CHUNK);
      if (!chunk) return 0;
      if (move && used) memcpy(chunk, vtt_text_pool_at(text, text->pool_mark), used);
      text->pool[text->pool_chunks] = chunk;
      if (move)
      {
         text->pool_mark = (size_t)text->pool_chunks * VTT_POOL_CHUNK;
         text->pool_size = text->pool_mark + used;
      }
      text->pool_chunks++;
   }
   memcpy(vtt_text_pool_at(text, text->pool_size), bytes, size);
   text->pool_size += size;
   return 1;
}

static int vtt_text_annotation(rsubtitle_vtt_text_t *text,
      const char *bytes, size_t size)
{
   if (text->kind != RSUBTITLE_VTT_VOICE && text->kind != RSUBTITLE_VTT_LANGUAGE)
      return 1;
   if (size == 1 && vtt_text_space((unsigned char)*bytes))
   {
      text->annotation_space = text->pool_size > text->pool_mark;
      return 1;
   }
   if (text->annotation_space && !vtt_text_pool(text, " ", 1)) return 0;
   text->annotation_space = 0;
   return vtt_text_pool(text, bytes, size);
}

static int vtt_text_add_class(rsubtitle_vtt_text_t *text, size_t end)
{
   struct vtt_text_class *entry;
   if (end == text->class_start || text->kind == RSUBTITLE_VTT_ROOT) return 1;
   if (text->class_count == VTT_TEXT_LIMIT) return 0;
   if (!text->classes)
   {
      text->classes = (struct vtt_text_class**)calloc(VTT_TEXT_LIMIT / VTT_TEXT_CHUNK,
            sizeof(*text->classes));
      if (!text->classes) return 0;
   }
   if (text->class_count == (size_t)text->class_chunks * VTT_TEXT_CHUNK)
   {
      struct vtt_text_class *chunk = (struct vtt_text_class*)malloc(VTT_TEXT_CHUNK * sizeof(*entry));
      if (!chunk) return 0;
      text->classes[text->class_chunks++] = chunk;
   }
   entry = vtt_text_class_at(text, text->class_count++);
   entry->text = text->input + text->class_start;
   entry->size = end - text->class_start;
   return 1;
}

static int vtt_text_digits(const char **position, const char *end,
      unsigned maximum, uint64_t limit, uint64_t *value, unsigned *digits)
{
   uint64_t number = 0;
   unsigned count = 0;
   const char *p = *position;
   while (p < end && *p >= '0' && *p <= '9')
   {
      unsigned digit = (unsigned)(*p - '0');
      if (count == maximum || number > limit / 10
            || (number == limit / 10 && digit > limit % 10)) return 0;
      number = number * 10 + digit;
      count++; p++;
   }
   if (!count) return 0;
   *value = number; *digits = count; *position = p;
   return 1;
}

static int vtt_text_timestamp(const char *input, size_t size, int64_t *value)
{
   const char *p = input, *end = input + size;
   uint64_t a, b, c, fraction, milliseconds;
   uint64_t limit = (((uint64_t)-1) >> 1) / 1000;
   unsigned digits;
   if (!vtt_text_digits(&p, end, 10, limit / 3600000, &a, &digits)
         || p == end || *p++ != ':') return 0;
   if (!vtt_text_digits(&p, end, 2, 59, &b, &digits) || digits != 2) return 0;
   if ((size_t)(p - input) != 5 || a > 59 || (p < end && *p == ':'))
   {
      if (p == end || *p++ != ':'
            || !vtt_text_digits(&p, end, 2, 59, &c, &digits) || digits != 2) return 0;
   }
   else
   {
      c = b; b = a; a = 0;
   }
   if (p == end || *p++ != '.'
         || !vtt_text_digits(&p, end, 3, 999, &fraction, &digits)
         || digits != 3 || p != end) return 0;
   milliseconds = ((a * 60 + b) * 60 + c) * 1000 + fraction;
   if (milliseconds > limit) return 0;
   *value = (int64_t)milliseconds * 1000;
   return 1;
}

static int vtt_text_finish_tag(rsubtitle_vtt_text_t *text, size_t end)
{
   enum vtt_text_state state = text->state;
   enum rsubtitle_vtt_node_kind kind = text->kind;
   struct vtt_text_node *node;
   size_t index;
   int64_t timestamp, mapped;
   if (state == VTT_TEXT_CLASS_NAME && !vtt_text_add_class(text, end)) return 0;
   if (state == VTT_TEXT_NAME || state == VTT_TEXT_END)
      kind = vtt_text_kind(text->input + text->name_start, end - text->name_start);
   if (state == VTT_TEXT_END)
   {
      node = vtt_text_at(text, text->base + text->current);
      if (kind != RSUBTITLE_VTT_ROOT && node->view.kind == kind)
         text->current = node->view.parent;
      else if (kind == RSUBTITLE_VTT_RUBY && node->view.kind == RSUBTITLE_VTT_RUBY_TEXT)
         text->current = vtt_text_at(text, text->base + node->view.parent)->view.parent;
   }
   else if (state == VTT_TEXT_TIME)
   {
      if (vtt_text_timestamp(text->input + text->name_start, end - text->name_start, &timestamp)
            && rsubtitle_vtt_timestamp_us(&text->cue, timestamp, &mapped))
      {
         index = vtt_text_add(text, RSUBTITLE_VTT_TIMESTAMP);
         if (index == RSUBTITLE_VTT_NO_NODE) return 0;
         vtt_text_at(text, index)->view.timestamp_us = mapped;
      }
   }
   else if (kind != RSUBTITLE_VTT_ROOT
         && (kind != RSUBTITLE_VTT_RUBY_TEXT
            || vtt_text_at(text, text->base + text->current)->view.kind == RSUBTITLE_VTT_RUBY))
   {
      index = vtt_text_add(text, kind);
      if (index == RSUBTITLE_VTT_NO_NODE) return 0;
      node = vtt_text_at(text, index);
      node->classes = text->class_mark;
      node->view.class_count = text->class_count - text->class_mark;
      if (kind == RSUBTITLE_VTT_VOICE || kind == RSUBTITLE_VTT_LANGUAGE)
      {
         size_t length = text->pool_size - text->pool_mark;
         node->storage = 2;
         if (!vtt_text_pool(text, "", 1)) return 0;
         node->offset = text->pool_mark;
         node->view.value_length = length;
      }
      text->current = index - text->base;
   }
   else
   {
      text->class_count = text->class_mark;
      text->pool_size = text->pool_mark;
   }
   text->state = VTT_TEXT_DATA;
   text->raw_start = text->position;
   return 1;
}

static size_t vtt_text_entity(const char *name)
{
   size_t low = 0, high = sizeof(vtt_entities) / sizeof(vtt_entities[0]);
   while (low < high)
   {
      size_t mid = low + (high - low) / 2;
      int order = strcmp(name, vtt_entity_names + vtt_entities[mid].name);
      if (!order) return mid;
      if (order < 0) high = mid;
      else low = mid + 1;
   }
   return RSUBTITLE_VTT_NO_NODE;
}

static unsigned vtt_text_utf8(uint32_t point, char *out)
{
   if (point < 0x80) { out[0] = (char)point; return 1; }
   if (point < 0x800)
   {
      out[0] = (char)(0xc0 | (point >> 6)); out[1] = (char)(0x80 | (point & 63)); return 2;
   }
   if (point < 0x10000)
   {
      out[0] = (char)(0xe0 | (point >> 12)); out[1] = (char)(0x80 | ((point >> 6) & 63));
      out[2] = (char)(0x80 | (point & 63)); return 3;
   }
   out[0] = (char)(0xf0 | (point >> 18)); out[1] = (char)(0x80 | ((point >> 12) & 63));
   out[2] = (char)(0x80 | ((point >> 6) & 63)); out[3] = (char)(0x80 | (point & 63)); return 4;
}

static uint32_t vtt_text_number(uint32_t point)
{
   static const uint32_t c1[32] = {
      0x20ac,0x81,0x201a,0x192,0x201e,0x2026,0x2020,0x2021,
      0x2c6,0x2030,0x160,0x2039,0x152,0x8d,0x17d,0x8f,
      0x90,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,
      0x2dc,0x2122,0x161,0x203a,0x153,0x9d,0x17e,0x178
   };
   if (!point || point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff)) return 0xfffd;
   return point >= 0x80 && point <= 0x9f ? c1[point - 0x80] : point;
}

static int vtt_text_reference_finish(rsubtitle_vtt_text_t *text, int numeric)
{
   if ((numeric && !text->number_digits) || (!numeric && text->match == RSUBTITLE_VTT_NO_NODE))
   {
      text->position = text->reference_start + 1;
      if (text->return_state == VTT_TEXT_DATA)
      {
         if (!vtt_text_raw(text, text->reference_start, 1)) return 0;
      }
      else if (!vtt_text_annotation(text, "&", 1)) return 0;
      text->state = text->return_state;
      text->raw_start = text->position;
      return 1;
   }
   text->pending_index = 0;
   if (numeric)
   {
      text->pending_count = 1;
      text->pending[0] = vtt_text_number(text->number);
   }
   else
   {
      text->position = text->match_end;
      text->pending[0] = vtt_entities[text->match].first;
      text->pending[1] = vtt_entities[text->match].second;
      text->pending_count = text->pending[1] ? 2 : 1;
   }
   text->state = VTT_TEXT_PENDING;
   return 1;
}

rsubtitle_vtt_text_t *rsubtitle_vtt_text_new(void)
{
   return (rsubtitle_vtt_text_t*)calloc(1, sizeof(rsubtitle_vtt_text_t));
}

void rsubtitle_vtt_text_free(rsubtitle_vtt_text_t *text)
{
   unsigned i;
   if (!text) return;
   rsubtitle_vtt_normalize_free(text->language_normal);
   for (i = 0; i < text->language_count; i++) free(vtt_text_language_at(text, i)->key.owned);
   for (i = 0; i < text->language_chunks; i++) free(text->languages[i]);
   for (i = 0; i < text->node_chunks; i++) free(text->nodes[i]);
   for (i = 0; i < text->class_chunks; i++) free(text->classes[i]);
   for (i = 0; i < text->pool_chunks; i++) free(text->pool[i]);
   free(text->language_buckets); free(text->languages);
   free(text->nodes); free(text->classes); free(text->pool); free(text);
}

size_t rsubtitle_vtt_text_count(const rsubtitle_vtt_text_t *text)
{
   return text ? text->count : 0;
}

int rsubtitle_vtt_text_begin(rsubtitle_vtt_text_t *text, const char *input,
      size_t size, const rsubtitle_vtt_cue_t *cue)
{
   if (!text || !input || !cue || text->active || text->sealed || size > 65536) return 0;
   text->input = input; text->size = size; text->position = text->raw_start = 0;
   text->base = text->count; text->current = 0; text->cue = *cue;
   text->state = VTT_TEXT_DATA; text->active = 1;
   return vtt_text_add(text, RSUBTITLE_VTT_ROOT) != RSUBTITLE_VTT_NO_NODE;
}

static int vtt_text_reference_step(rsubtitle_vtt_text_t *text, int c)
{
   switch (text->state)
   {
      case VTT_TEXT_REFERENCE:
         if (!text->name_length && c == '#')
         {
            text->position++; text->base_number = 10; text->number_digits = 0;
            text->number = 0; text->state = VTT_TEXT_NUMBER_START;
         }
         else if (c >= 0 && text->name_length < 32
               && (vtt_text_alnum((unsigned)c) || c == ';'))
         {
            size_t match;
            text->name[text->name_length++] = (char)c; text->name[text->name_length] = 0;
            text->position++; match = vtt_text_entity(text->name);
            if (match != RSUBTITLE_VTT_NO_NODE) { text->match = match; text->match_end = text->position; }
            if (c == ';' && !vtt_text_reference_finish(text, 0)) return -1;
         }
         else if (!vtt_text_reference_finish(text, 0)) return -1;
         break;
      case VTT_TEXT_NUMBER_START:
         if (c == 'x' || c == 'X') { text->base_number = 16; text->position++; }
         text->state = VTT_TEXT_NUMBER;
         break;
      case VTT_TEXT_NUMBER:
      {
         unsigned digit = c >= '0' && c <= '9' ? (unsigned)(c - '0')
            : c >= 'a' && c <= 'f' ? (unsigned)(c - 'a' + 10)
            : c >= 'A' && c <= 'F' ? (unsigned)(c - 'A' + 10) : 16;
         if (digit < text->base_number)
         {
            if (text->number > 0x110000 / text->base_number) text->number = 0x110000;
            else text->number = text->number * text->base_number + digit;
            text->number_digits++; text->position++;
         }
         else
         {
            if (c == ';' && text->number_digits) text->position++;
            if (!vtt_text_reference_finish(text, 1)) return -1;
         }
         break;
      }
      case VTT_TEXT_PENDING:
      {
         char bytes[4];
         unsigned size = vtt_text_utf8(text->pending[text->pending_index], bytes);
         if (text->return_state == VTT_TEXT_ANNOTATION)
         {
            if (!vtt_text_annotation(text, bytes, size)) return -1;
         }
         else
         {
            struct vtt_text_node *node;
            if (!text->pending_index)
            {
               text->entity_node = vtt_text_add(text, RSUBTITLE_VTT_TEXT);
               if (text->entity_node == RSUBTITLE_VTT_NO_NODE) return -1;
               vtt_text_at(text, text->entity_node)->storage = 1;
            }
            node = vtt_text_at(text, text->entity_node);
            memcpy(node->bytes + node->view.value_length, bytes, size);
            node->view.value_length += size;
         }
         if (++text->pending_index == text->pending_count)
         {
            text->state = text->return_state;
            text->raw_start = text->position;
         }
         break;
      }
      default: return -1;
   }
   return 0;
}

int rsubtitle_vtt_text_step(rsubtitle_vtt_text_t *text)
{
   int c;
   if (!text || text->sealed) return -1;
   if (!text->active) return 1;
   c = text->position < text->size ? (unsigned char)text->input[text->position] : -1;
   if (text->state >= VTT_TEXT_REFERENCE) return vtt_text_reference_step(text, c);
   switch (text->state)
   {
      case VTT_TEXT_DATA:
         if (c < 0 || c == '<' || c == '&')
         {
            if (text->position != text->raw_start)
            {
               if (!vtt_text_raw(text, text->raw_start, text->position - text->raw_start)) return -1;
               text->raw_start = text->position;
               break;
            }
            if (c < 0) { text->active = 0; return 1; }
            if (c == '&')
            {
               text->reference_start = text->position++;
               text->return_state = VTT_TEXT_DATA;
               text->state = VTT_TEXT_REFERENCE;
               text->name_length = 0; text->match = RSUBTITLE_VTT_NO_NODE;
               break;
            }
            text->position++;
            text->state = VTT_TEXT_TAG; text->kind = RSUBTITLE_VTT_ROOT;
            text->class_mark = text->class_count; text->pool_mark = text->pool_size;
            text->annotation_space = 0; text->name_start = text->position;
         }
         else text->position++;
         break;
      case VTT_TEXT_TAG:
         if (c < 0 || c == '>')
         {
            size_t end = text->position;
            if (c == '>') text->position++;
            if (!vtt_text_finish_tag(text, end)) return -1;
         }
         else if (c == '/') { text->name_start = ++text->position; text->state = VTT_TEXT_END; }
         else if (c >= '0' && c <= '9') { text->position++; text->state = VTT_TEXT_TIME; }
         else if (vtt_text_space((unsigned)c)) { text->position++; text->state = VTT_TEXT_ANNOTATION; }
         else if (c == '.') { text->class_start = ++text->position; text->state = VTT_TEXT_CLASS_NAME; }
         else { text->position++; text->state = VTT_TEXT_NAME; }
         break;
      case VTT_TEXT_NAME:
         if (c < 0 || c == '>')
         {
            size_t end = text->position;
            if (c == '>') text->position++;
            if (!vtt_text_finish_tag(text, end)) return -1;
         }
         else if (c == '.' || vtt_text_space((unsigned)c))
         {
            text->kind = vtt_text_kind(text->input + text->name_start, text->position - text->name_start);
            text->position++;
            text->class_start = text->position;
            text->state = c == '.' ? VTT_TEXT_CLASS_NAME : VTT_TEXT_ANNOTATION;
         }
         else text->position++;
         break;
      case VTT_TEXT_CLASS_NAME:
         if (c < 0 || c == '>')
         {
            size_t end = text->position;
            if (c == '>') text->position++;
            if (!vtt_text_finish_tag(text, end)) return -1;
         }
         else if (c == '.' || vtt_text_space((unsigned)c))
         {
            if (!vtt_text_add_class(text, text->position)) return -1;
            text->position++; text->class_start = text->position;
            if (c != '.') text->state = VTT_TEXT_ANNOTATION;
         }
         else text->position++;
         break;
      case VTT_TEXT_ANNOTATION:
         if (c < 0 || c == '>')
         {
            size_t end = text->position;
            if (c == '>') text->position++;
            if (!vtt_text_finish_tag(text, end)) return -1;
         }
         else if (c == '&')
         {
            text->reference_start = text->position++;
            text->return_state = VTT_TEXT_ANNOTATION; text->state = VTT_TEXT_REFERENCE;
            text->name_length = 0; text->match = RSUBTITLE_VTT_NO_NODE;
         }
         else
         {
            if (!vtt_text_annotation(text, text->input + text->position, 1)) return -1;
            text->position++;
         }
         break;
      case VTT_TEXT_END:
      case VTT_TEXT_TIME:
         if (c < 0 || c == '>')
         {
            size_t end = text->position;
            if (c == '>') text->position++;
            if (!vtt_text_finish_tag(text, end)) return -1;
         }
         else text->position++;
         break;
      default: return -1;
   }
   return 0;
}

static void vtt_text_color_hint(rsubtitle_vtt_hints_t *hints,
      const char *name, size_t size)
{
   static const struct
   {
      const char *name;
      size_t size;
      uint32_t color;
   } colors[] = {
      {"white", 5, UINT32_C(0xffffffff)},
      {"lime", 4, UINT32_C(0x00ff00ff)},
      {"cyan", 4, UINT32_C(0x00ffffff)},
      {"red", 3, UINT32_C(0xff0000ff)},
      {"yellow", 6, UINT32_C(0xffff00ff)},
      {"magenta", 7, UINT32_C(0xff00ffff)},
      {"blue", 4, UINT32_C(0x0000ffff)},
      {"black", 5, UINT32_C(0x000000ff)}
   };
   size_t i;
   int background = size > 3 && !memcmp(name, "bg_", 3);
   if (background) { name += 3; size -= 3; }
   if (size < 3 || size > 7) return;
   for (i = 0; i < sizeof(colors) / sizeof(colors[0]); i++)
      if (size == colors[i].size && !memcmp(name, colors[i].name, size))
      {
         if (background)
         {
            hints->background = colors[i].color;
            hints->flags |= RSUBTITLE_VTT_HINT_BACKGROUND;
         }
         else
         {
            hints->color = colors[i].color;
            hints->flags |= RSUBTITLE_VTT_HINT_COLOR;
         }
         return;
      }
}

static int vtt_text_language_store(rsubtitle_vtt_text_t *text,
      const rsubtitle_vtt_node_t *node, const rsubtitle_vtt_language_key_t *key)
{
   struct vtt_text_language *entry;
   if (!text->language_buckets)
   {
      text->language_buckets = (uint32_t*)calloc(VTT_LANGUAGE_BUCKETS,
            sizeof(*text->language_buckets));
      if (!text->language_buckets) return 0;
   }
   if (!text->languages)
   {
      text->languages = (struct vtt_text_language**)calloc(VTT_TEXT_LIMIT / VTT_TEXT_CHUNK,
            sizeof(*text->languages));
      if (!text->languages) return 0;
   }
   if (text->language_count == (size_t)text->language_chunks * VTT_TEXT_CHUNK)
   {
      struct vtt_text_language *chunk = (struct vtt_text_language*)malloc(
            VTT_TEXT_CHUNK * sizeof(*chunk));
      if (!chunk) return 0;
      text->languages[text->language_chunks++] = chunk;
   }
   entry = vtt_text_language_at(text, text->language_count++);
   entry->raw = node->value;
   entry->raw_length = node->value_length;
   entry->key = *key;
   entry->hash = text->language_hash;
   entry->next = text->language_buckets[text->language_hash % VTT_LANGUAGE_BUCKETS];
   text->language_buckets[text->language_hash % VTT_LANGUAGE_BUCKETS] = (uint32_t)text->language_count;
   return 1;
}

static int vtt_text_language_step(rsubtitle_vtt_text_t *text, struct vtt_text_node *node)
{
   rsubtitle_vtt_language_key_t key;
   const struct vtt_text_language *entry;
   int status;
   switch (text->language_state)
   {
      case VTT_LANGUAGE_NONE:
         if (!node->view.value_length)
         {
            node->storage = 0;
            return 1;
         }
         text->language_hash = 2166136261U;
         text->language_position = 0;
         text->language_state = VTT_LANGUAGE_HASH;
         break;
      case VTT_LANGUAGE_HASH:
         if (text->language_position < node->view.value_length)
         {
            text->language_hash ^= (unsigned char)node->view.value[text->language_position++];
            text->language_hash *= 16777619U;
         }
         else
         {
            text->language_lookup = text->language_buckets
               ? text->language_buckets[text->language_hash % VTT_LANGUAGE_BUCKETS] : 0;
            text->language_state = VTT_LANGUAGE_LOOKUP;
         }
         break;
      case VTT_LANGUAGE_LOOKUP:
         if (!text->language_lookup)
         {
            if (!text->language_normal)
            {
               text->language_normal = rsubtitle_vtt_normalize_new();
               if (!text->language_normal) return -1;
            }
            if (!rsubtitle_vtt_normalize_begin(text->language_normal, node->view.value,
                     node->view.value_length, RSUBTITLE_NORMALIZE_EXTLANG)) return -1;
            text->language_state = VTT_LANGUAGE_NORMALIZE;
         }
         else
         {
            entry = vtt_text_language_at(text, text->language_lookup - 1);
            if (entry->hash == text->language_hash && entry->raw_length == node->view.value_length)
            {
               text->language_position = 0;
               text->language_state = VTT_LANGUAGE_COMPARE;
            }
            else
               text->language_lookup = entry->next;
         }
         break;
      case VTT_LANGUAGE_COMPARE:
         entry = vtt_text_language_at(text, text->language_lookup - 1);
         if (text->language_position == node->view.value_length)
         {
            node->storage = entry->key.owned ? text->language_lookup : 0;
            text->language_state = VTT_LANGUAGE_NONE;
            return 1;
         }
         if (entry->raw[text->language_position] != node->view.value[text->language_position])
         {
            text->language_lookup = entry->next;
            text->language_state = VTT_LANGUAGE_LOOKUP;
         }
         else
            text->language_position++;
         break;
      case VTT_LANGUAGE_NORMALIZE:
         status = rsubtitle_vtt_normalize_step(text->language_normal);
         if (status <= 0) return status;
         key.text = node->view.value;
         key.length = node->view.value_length;
         key.owned = NULL;
         if (status == 1 && !rsubtitle_vtt_normalize_take(text->language_normal, &key)) return -1;
         if (!vtt_text_language_store(text, &node->view, &key))
         {
            free(key.owned);
            return -1;
         }
         node->storage = key.owned ? (unsigned)text->language_count : 0;
         text->language_state = VTT_LANGUAGE_NONE;
         return 1;
   }
   return 0;
}

int rsubtitle_vtt_text_seal(rsubtitle_vtt_text_t *text)
{
   struct vtt_text_node *node;
   if (!text || text->active) return -1;
   if (text->seal_stage == VTT_SEAL_SUFFIX)
   {
      uint32_t after, suffix;
      if (!text->seal)
      {
         text->seal_stage = VTT_SEAL_RELATIVE;
         return 0;
      }
      node = vtt_text_at(text, --text->seal);
      after = node->last_child == RSUBTITLE_VTT_NO_NODE
         ? (uint32_t)(text->seal + 1)
         : vtt_text_at(text, node->last_child)->subtree_end;
      node->subtree_end = after;
      if (after < text->count && vtt_text_at(text, after)->view.kind != RSUBTITLE_VTT_ROOT)
         node->view.past_timestamp = vtt_text_at(text, after)->suffix_timestamp;
      suffix = text->seal + 1 < text->count
            && vtt_text_at(text, text->seal + 1)->view.kind != RSUBTITLE_VTT_ROOT
         ? vtt_text_at(text, text->seal + 1)->suffix_timestamp : 0;
      if (node->view.kind == RSUBTITLE_VTT_TIMESTAMP && (!suffix
               || node->view.timestamp_us < vtt_text_at(text, suffix - 1)->view.timestamp_us))
         suffix = (uint32_t)(text->seal + 1);
      node->suffix_timestamp = suffix;
      return 0;
   }
   if (text->seal == text->count)
   {
      if (text->seal_stage == VTT_SEAL_HINTS)
      {
         text->seal_stage = VTT_SEAL_SUFFIX;
         return 0;
      }
      rsubtitle_vtt_normalize_free(text->language_normal);
      text->language_normal = NULL;
      text->sealed = 1;
      return 1;
   }
   node = vtt_text_at(text, text->seal);
   if (text->seal_stage == VTT_SEAL_RELATIVE)
   {
      if (node->view.kind == RSUBTITLE_VTT_LANGUAGE)
      {
         int status = vtt_text_language_step(text, node);
         if (status <= 0) return status;
      }
      if (node->view.kind == RSUBTITLE_VTT_ROOT) text->base = text->seal;
      if (node->view.kind == RSUBTITLE_VTT_LANGUAGE)
         node->view.language_node = (uint32_t)(text->seal - text->base + 1);
      else if (node->view.parent != RSUBTITLE_VTT_NO_NODE)
         node->view.language_node = vtt_text_at(text,
               text->base + node->view.parent)->view.language_node;
      if (node->view.past_timestamp) node->view.past_timestamp -= (uint32_t)text->base;
      if (node->view.future_timestamp) node->view.future_timestamp -= (uint32_t)text->base;
      text->seal++;
      return 0;
   }
   if (!text->seal_class)
      switch (node->view.kind)
      {
         case RSUBTITLE_VTT_BOLD: node->view.hints.flags |= RSUBTITLE_VTT_HINT_BOLD; break;
         case RSUBTITLE_VTT_ITALIC: node->view.hints.flags |= RSUBTITLE_VTT_HINT_ITALIC; break;
         case RSUBTITLE_VTT_UNDERLINE: node->view.hints.flags |= RSUBTITLE_VTT_HINT_UNDERLINE; break;
         default: break;
      }
   if (text->seal_class < node->view.class_count)
   {
      const struct vtt_text_class *entry = vtt_text_class_at(text,
            node->classes + text->seal_class++);
      vtt_text_color_hint(&node->view.hints, entry->text, entry->size);
      return 0;
   }
   node->view.value = node->storage == 1 ? node->bytes
      : node->storage == 2 ? vtt_text_pool_at(text, node->offset) : node->raw;
   if (node->view.kind == RSUBTITLE_VTT_ROOT) text->before_timestamp = 0;
   node->view.future_timestamp = text->before_timestamp;
   if (node->view.kind == RSUBTITLE_VTT_TIMESTAMP && (!text->before_timestamp
            || node->view.timestamp_us > vtt_text_at(text, text->before_timestamp - 1)->view.timestamp_us))
      text->before_timestamp = (uint32_t)(text->seal + 1);
   text->seal++;
   text->seal_class = 0;
   return 0;
}

const rsubtitle_vtt_node_t *rsubtitle_vtt_text_node(
      const rsubtitle_vtt_text_t *text, size_t index)
{
   return text && text->sealed && index < text->count ? &vtt_text_at(text, index)->view : NULL;
}

const char *rsubtitle_vtt_text_language_key(const rsubtitle_vtt_text_t *text,
      size_t index, size_t *length)
{
   const struct vtt_text_node *node;
   if (length) *length = 0;
   if (!text || !text->sealed || index >= text->count) return NULL;
   node = vtt_text_at(text, index);
   if (node->view.kind != RSUBTITLE_VTT_LANGUAGE) return NULL;
   if (node->storage)
   {
      const struct vtt_text_language *entry;
      if (node->storage > text->language_count) return NULL;
      entry = vtt_text_language_at(text, node->storage - 1);
      if (length) *length = entry->key.length;
      return entry->key.text;
   }
   if (length) *length = node->view.value_length;
   return node->view.value;
}

const char *rsubtitle_vtt_text_class(const rsubtitle_vtt_text_t *text,
      size_t node, size_t index, size_t *length)
{
   const struct vtt_text_class *entry;
   if (length) *length = 0;
   if (!text || !text->sealed || node >= text->count
         || index >= vtt_text_at(text, node)->view.class_count) return NULL;
   entry = vtt_text_class_at(text, vtt_text_at(text, node)->classes + index);
   if (length) *length = entry->size;
   return entry->text;
}
