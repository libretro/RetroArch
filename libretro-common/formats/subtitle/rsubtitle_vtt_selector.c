/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rsubtitle_vtt_selector.c).
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
#include "rsubtitle_vtt_internal.h"
#include "rsubtitle_languages.h"

struct rsubtitle_vtt_selector_pattern
{
   const char *value;
   uint32_t *prefix;
   size_t length, capacity, position, matched;
   enum rsubtitle_vtt_selector_operator op;
   unsigned insensitive, space, ready, begun;
};

enum vtt_tag_grammar
{
   VTT_TAG_FIRST, VTT_TAG_EXTLANG, VTT_TAG_SCRIPT, VTT_TAG_REGION,
   VTT_TAG_VARIANT, VTT_TAG_EXTENSION, VTT_TAG_PRIVATE
};

struct rsubtitle_vtt_tag
{
   uint64_t key, singletons;
   uint64_t *variants;
   const char *text;
   size_t length, position, count, capacity, clearing, probe;
   rsubtitle_vtt_alias_lookup_t alias;
   uint32_t hash;
   enum vtt_tag_grammar grammar;
   unsigned begun, whole, alpha, digit, first_digit, extlangs;
   unsigned need_value, ended, hashing, cleared;
   int result;
};

static unsigned vtt_selector_fold(unsigned c, unsigned insensitive)
{
   return insensitive && c >= 'A' && c <= 'Z' ? c + 'a' - 'A' : c;
}

static int vtt_selector_space(unsigned c)
{
   return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

rsubtitle_vtt_tag_t *rsubtitle_vtt_tag_new(void)
{
   return (rsubtitle_vtt_tag_t*)calloc(1, sizeof(rsubtitle_vtt_tag_t));
}

void rsubtitle_vtt_tag_free(rsubtitle_vtt_tag_t *tag)
{
   if (!tag)
      return;
   free(tag->variants);
   free(tag);
}

int rsubtitle_vtt_tag_begin(rsubtitle_vtt_tag_t *tag,
      const char *text, size_t length)
{
   uint64_t *variants;
   size_t capacity;
   if (!tag)
      return 0;
   variants = tag->variants;
   capacity = tag->capacity;
   memset(tag, 0, sizeof(*tag));
   tag->variants = variants;
   tag->capacity = capacity;
   if ((!text && length) || length > 65536)
      return 0;
   tag->text   = text ? text : "";
   tag->length = length;
   tag->begun  = 1;
   tag->whole  = 1;
   tag->alpha  = tag->digit = 1;
   tag->hash   = 2166136261U;
   if (!length)
      tag->result = 2;
   return rsubtitle_vtt_alias_begin(&tag->alias, RSUBTITLE_ALIAS_TAG,
         tag->text, length);
}

static int vtt_tag_variant(rsubtitle_vtt_tag_t *tag)
{
   size_t capacity = 8;
   if (!tag->cleared)
   {
      while (capacity < (tag->length / 5 + 1) * 2)
         capacity *= 2;
      if (capacity != tag->capacity)
      {
         uint64_t *variants = (uint64_t*)malloc(capacity * sizeof(*variants));
         if (!variants)
            return -1;
         free(tag->variants);
         tag->variants = variants;
         tag->capacity = capacity;
      }
   }
   tag->probe   = tag->hash & (tag->capacity - 1);
   tag->hashing = 1;
   return 0;
}

static int vtt_tag_singleton(rsubtitle_vtt_tag_t *tag)
{
   unsigned c = (unsigned)tag->key;
   unsigned index;
   uint64_t bit;
   if (c == 'x')
      tag->grammar = VTT_TAG_PRIVATE;
   else
   {
      index = c >= 'a' ? c - 'a' + 10 : c - '0';
      bit = (uint64_t)1 << index;
      if (tag->singletons & bit)
         return 2;
      tag->singletons |= bit;
      tag->grammar = VTT_TAG_EXTENSION;
   }
   tag->need_value = 1;
   return 0;
}

static int vtt_tag_subtag(rsubtitle_vtt_tag_t *tag)
{
   size_t n = tag->count;
   if (tag->grammar == VTT_TAG_FIRST)
   {
      if (n == 1 && tag->key == 'x')
         return vtt_tag_singleton(tag);
      if (!tag->alpha || n < 2)
         return 2;
      tag->grammar = n <= 3 ? VTT_TAG_EXTLANG : VTT_TAG_SCRIPT;
      return 0;
   }
   if (tag->grammar == VTT_TAG_EXTLANG)
   {
      if (n == 3 && tag->alpha && tag->extlangs < 3)
      {
         tag->extlangs++;
         return 0;
      }
      tag->grammar = VTT_TAG_SCRIPT;
   }
   if (tag->grammar == VTT_TAG_SCRIPT)
   {
      tag->grammar = VTT_TAG_REGION;
      if (n == 4 && tag->alpha)
         return 0;
   }
   if (tag->grammar == VTT_TAG_REGION)
   {
      tag->grammar = VTT_TAG_VARIANT;
      if ((n == 2 && tag->alpha) || (n == 3 && tag->digit))
         return 0;
   }
   if (tag->grammar == VTT_TAG_VARIANT)
   {
      if (n >= 5 || (n == 4 && tag->first_digit))
         return vtt_tag_variant(tag);
      return n == 1 ? vtt_tag_singleton(tag) : 2;
   }
   if (tag->grammar == VTT_TAG_PRIVATE)
   {
      tag->need_value = 0;
      return 0;
   }
   if (n > 1)
      tag->need_value = 0;
   else if (!tag->need_value)
      return vtt_tag_singleton(tag);
   else
      return 2;
   return 0;
}

static void vtt_tag_reset_subtag(rsubtitle_vtt_tag_t *tag)
{
   tag->key         = 0;
   tag->count       = 0;
   tag->alpha       = tag->digit = 1;
   tag->first_digit = 0;
   tag->hash        = 2166136261U;
}

int rsubtitle_vtt_tag_step(rsubtitle_vtt_tag_t *tag)
{
   unsigned c, alpha, digit;
   int status;
   if (!tag || !tag->begun)
      return -1;
   if (tag->result)
      return tag->result;
   if (tag->whole)
   {
      status = rsubtitle_vtt_alias_step(&tag->alias);
      if (status == 1)
         tag->result = 1;
      else if (status == 2)
         tag->whole = 0;
      return tag->result;
   }
   if (tag->hashing)
   {
      if (!tag->cleared)
      {
         tag->variants[tag->clearing++] = 0;
         if (tag->clearing == tag->capacity)
            tag->cleared = 1;
      }
      else if (tag->variants[tag->probe] == tag->key)
         tag->result = 2;
      else if (!tag->variants[tag->probe])
      {
         tag->variants[tag->probe] = tag->key;
         tag->hashing = 0;
         vtt_tag_reset_subtag(tag);
      }
      else
         tag->probe = (tag->probe + 1) & (tag->capacity - 1);
      return tag->result;
   }
   if (tag->ended)
   {
      tag->result = tag->need_value ? 2 : 1;
      return tag->result;
   }
   c = tag->position < tag->length ? (unsigned char)tag->text[tag->position] : 0;
   if (tag->position == tag->length || c == '-')
   {
      if (!tag->count)
         tag->result = 2;
      else
      {
         tag->ended = tag->position == tag->length;
         tag->result = vtt_tag_subtag(tag);
         if (!tag->hashing)
            vtt_tag_reset_subtag(tag);
         tag->position++;
      }
      return tag->result;
   }
   c = vtt_selector_fold(c, 1);
   alpha = c >= 'a' && c <= 'z';
   digit = c >= '0' && c <= '9';
   if ((!alpha && !digit) || tag->count == 8)
      tag->result = 2;
   else
   {
      tag->alpha &= alpha;
      tag->digit &= digit;
      if (!tag->count)
         tag->first_digit = digit;
      tag->count++;
      tag->position++;
      tag->key = (tag->key << 8) | c;
      tag->hash = (tag->hash ^ c) * 16777619U;
   }
   return tag->result;
}

int rsubtitle_vtt_alias_begin(rsubtitle_vtt_alias_lookup_t *lookup,
      enum rsubtitle_vtt_alias_kind kind, const char *key, size_t length)
{
   if (!lookup)
      return 0;
   memset(lookup, 0, sizeof(*lookup));
   if ((unsigned)kind > RSUBTITLE_ALIAS_TAG || (!key && length))
      return 0;
   lookup->key    = key ? key : "";
   lookup->length = length;
   lookup->kind   = kind;
   lookup->upper  = sizeof(vtt_language_aliases) / sizeof(vtt_language_aliases[0]);
   lookup->begun  = 1;
   if (!length || length > 64)
      lookup->result = 2;
   return 1;
}

int rsubtitle_vtt_alias_step(rsubtitle_vtt_alias_lookup_t *lookup)
{
   const struct vtt_language_alias *row;
   unsigned a, b;
   if (!lookup || !lookup->begun)
      return -1;
   if (lookup->result)
      return lookup->result;
   if (!lookup->comparing)
   {
      if (lookup->lower == lookup->upper)
      {
         lookup->result = 2;
         return 2;
      }
      lookup->middle = lookup->lower + (lookup->upper - lookup->lower) / 2;
      row = &vtt_language_aliases[lookup->middle];
      if (row->kind < (unsigned)lookup->kind)
         lookup->lower = lookup->middle + 1;
      else if (row->kind > (unsigned)lookup->kind)
         lookup->upper = lookup->middle;
      else
      {
         lookup->position = 0;
         lookup->comparing = 1;
      }
      return 0;
   }
   row = &vtt_language_aliases[lookup->middle];
   a = (unsigned char)vtt_language_strings[row->key + lookup->position];
   b = lookup->position < lookup->length
      ? vtt_selector_fold((unsigned char)lookup->key[lookup->position], 1) : 0;
   if (!b && lookup->position < lookup->length)
      lookup->result = 2;
   else if (a < b)
   {
      lookup->lower = lookup->middle + 1;
      lookup->comparing = 0;
   }
   else if (a > b)
   {
      lookup->upper = lookup->middle;
      lookup->comparing = 0;
   }
   else if (!a)
      lookup->result = 1;
   else
      lookup->position++;
   return lookup->result;
}

int rsubtitle_vtt_alias_result(const rsubtitle_vtt_alias_lookup_t *lookup,
      rsubtitle_vtt_alias_value_t *value)
{
   const struct vtt_language_alias *row;
   if (!value)
      return 0;
   memset(value, 0, sizeof(*value));
   if (!lookup || !lookup->begun || lookup->result != 1)
      return 0;
   row = &vtt_language_aliases[lookup->middle];
   value->preferred = vtt_language_strings + row->preferred;
   value->preferred_length = strlen(value->preferred);
   value->extlang_prefix = vtt_language_strings + row->extlang_prefix;
   value->extlang_prefix_length = strlen(value->extlang_prefix);
   return 1;
}

int rsubtitle_vtt_language_begin(rsubtitle_vtt_language_match_t *match,
      const char *range, size_t range_length, const char *tag, size_t tag_length)
{
   if (!match || (!range && range_length) || (!tag && tag_length)
         || range_length > 65536 || tag_length > 65536)
      return 0;
   memset(match, 0, sizeof(*match));
   match->range        = range ? range : "";
   match->tag          = tag ? tag : "";
   match->range_length = range_length;
   match->tag_length   = tag_length;
   match->first        = 1;
   return 1;
}

int rsubtitle_vtt_language_step(rsubtitle_vtt_language_match_t *match)
{
   const char *text;
   size_t length, r, t, i;
   unsigned c, equal;
   if (!match || !match->range || !match->tag)
      return -1;
   if (match->result)
      return match->result;
   if (match->phase < 2)
   {
      text   = match->phase ? match->tag : match->range;
      length = match->phase ? match->tag_length : match->range_length;
      if (match->position == length)
      {
         if (length && !match->subtag_length)
            match->result = -1;
         else
         {
            match->phase++;
            match->position = match->subtag_length = 0;
            match->first = 1;
            match->wildcard = 0;
         }
         return match->result;
      }
      c = (unsigned char)text[match->position++];
      if (c == '-')
      {
         if (!match->subtag_length)
            match->result = -1;
         match->subtag_length = 0;
         match->wildcard = 0;
         match->first = 0;
      }
      else if (!match->phase && c == '*' && !match->subtag_length)
      {
         match->subtag_length = 1;
         match->wildcard = 1;
      }
      else
      {
         c = vtt_selector_fold(c, 1);
         if (match->wildcard || ++match->subtag_length > 8
               || !((c >= 'a' && c <= 'z')
                  || (!match->first && c >= '0' && c <= '9')))
            match->result = -1;
      }
      return match->result;
   }
   if (!match->range_length)
      match->result = match->tag_length ? 2 : 1;
   else if (match->range_position == match->range_length)
      match->result = 1;
   else if (!match->tag_length)
      match->result = 2;
   if (match->result)
      return match->result;
   r = match->range_position;
   while (r < match->range_length && match->range[r] != '-')
      r++;
   if (!match->first && match->range[match->range_position] == '*')
   {
      match->range_position = r + (r < match->range_length);
      return 0;
   }
   if (match->tag_position == match->tag_length)
   {
      match->result = 2;
      return 2;
   }
   t = match->tag_position;
   while (t < match->tag_length && match->tag[t] != '-')
      t++;
   length = r - match->range_position;
   equal  = length == t - match->tag_position;
   for (i = 0; equal && i < length; i++)
      equal = vtt_selector_fold((unsigned char)match->range[match->range_position + i], 1)
         == vtt_selector_fold((unsigned char)match->tag[match->tag_position + i], 1);
   if (equal || match->range[match->range_position] == '*')
      match->range_position = r + (r < match->range_length);
   else if (match->first || t - match->tag_position == 1)
      match->result = 2;
   match->tag_position = t + (t < match->tag_length);
   match->first = 0;
   return match->result;
}

rsubtitle_vtt_selector_pattern_t *rsubtitle_vtt_selector_pattern_new(void)
{
   return (rsubtitle_vtt_selector_pattern_t*)calloc(1, sizeof(rsubtitle_vtt_selector_pattern_t));
}

void rsubtitle_vtt_selector_pattern_free(rsubtitle_vtt_selector_pattern_t *pattern)
{
   if (!pattern) return;
   free(pattern->prefix); free(pattern);
}

int rsubtitle_vtt_selector_pattern_begin(rsubtitle_vtt_selector_pattern_t *pattern,
      const char *value, size_t length, enum rsubtitle_vtt_selector_operator op,
      unsigned insensitive)
{
   if (!pattern || (!value && length) || length > 65536 || insensitive > 1
         || op < RSUBTITLE_SELECTOR_PRESENT || op > RSUBTITLE_SELECTOR_SUBSTRING)
      return 0;
   pattern->ready = pattern->begun = 0;
   if ((op == RSUBTITLE_SELECTOR_WORD || op == RSUBTITLE_SELECTOR_SUBSTRING)
         && length > pattern->capacity)
   {
      uint32_t *prefix = (uint32_t*)realloc(pattern->prefix, length * sizeof(*prefix));
      if (!prefix) return 0;
      pattern->prefix = prefix; pattern->capacity = length;
   }
   pattern->value = value; pattern->length = length; pattern->op = op;
   pattern->position = pattern->matched = 0;
   pattern->insensitive = insensitive; pattern->space = 0; pattern->begun = 1;
   return 1;
}

int rsubtitle_vtt_selector_pattern_step(rsubtitle_vtt_selector_pattern_t *pattern)
{
   unsigned c;
   if (!pattern || !pattern->begun) return -1;
   if (pattern->ready) return 1;
   if (pattern->position == pattern->length) { pattern->ready = 1; return 1; }
   c = (unsigned char)pattern->value[pattern->position];
   if (pattern->op != RSUBTITLE_SELECTOR_WORD && pattern->op != RSUBTITLE_SELECTOR_SUBSTRING)
   {
      pattern->position++;
      return 0;
   }
   if (vtt_selector_space(c)) pattern->space = 1;
   if (pattern->position && vtt_selector_fold(c, pattern->insensitive)
         == vtt_selector_fold((unsigned char)pattern->value[pattern->matched], pattern->insensitive))
      pattern->matched++;
   else if (pattern->matched)
   {
      pattern->matched = pattern->prefix[pattern->matched - 1];
      return 0;
   }
   pattern->prefix[pattern->position++] = (uint32_t)pattern->matched;
   return 0;
}

int rsubtitle_vtt_selector_match_begin(rsubtitle_vtt_selector_match_t *match,
      const rsubtitle_vtt_selector_pattern_t *pattern, const char *value, size_t length)
{
   if (!match || !pattern || !pattern->ready || (!value && length)) return 0;
   memset(match, 0, sizeof(*match));
   match->pattern = pattern; match->value = value; match->length = length;
   if (pattern->op == RSUBTITLE_SELECTOR_PRESENT) match->result = 1;
   else if (pattern->length > length) match->result = 2;
   else if (pattern->op == RSUBTITLE_SELECTOR_WORD || pattern->op == RSUBTITLE_SELECTOR_SUBSTRING)
   {
      if (!pattern->length || (pattern->op == RSUBTITLE_SELECTOR_WORD && pattern->space))
         match->result = 2;
   }
   else if ((pattern->op == RSUBTITLE_SELECTOR_PREFIX || pattern->op == RSUBTITLE_SELECTOR_SUFFIX)
         && !pattern->length) match->result = 2;
   else if (pattern->op == RSUBTITLE_SELECTOR_EXACT && pattern->length != length)
      match->result = 2;
   else if (pattern->op == RSUBTITLE_SELECTOR_DASH && pattern->length != length
         && value[pattern->length] != '-') match->result = 2;
   return 1;
}

int rsubtitle_vtt_selector_match_step(rsubtitle_vtt_selector_match_t *match)
{
   const rsubtitle_vtt_selector_pattern_t *pattern;
   size_t offset;
   unsigned c;
   if (!match || !(pattern = match->pattern) || !pattern->ready) return -1;
   if (match->result) return (int)match->result;
   if (pattern->op != RSUBTITLE_SELECTOR_WORD && pattern->op != RSUBTITLE_SELECTOR_SUBSTRING)
   {
      if (match->position == pattern->length) { match->result = 1; return 1; }
      offset = pattern->op == RSUBTITLE_SELECTOR_SUFFIX ? match->length - pattern->length : 0;
      c = vtt_selector_fold((unsigned char)match->value[offset + match->position], pattern->insensitive);
      if (c != vtt_selector_fold((unsigned char)pattern->value[match->position], pattern->insensitive))
         match->result = 2;
      else match->position++;
      return (int)match->result;
   }
   if (match->position == match->length) { match->result = 2; return 2; }
   c = vtt_selector_fold((unsigned char)match->value[match->position], pattern->insensitive);
   if (c == vtt_selector_fold((unsigned char)pattern->value[match->matched], pattern->insensitive))
      match->matched++;
   else if (match->matched)
   {
      match->matched = pattern->prefix[match->matched - 1];
      return 0;
   }
   match->position++;
   if (match->matched == pattern->length)
   {
      offset = match->position - pattern->length;
      if (pattern->op == RSUBTITLE_SELECTOR_SUBSTRING
            || ((!offset || vtt_selector_space((unsigned char)match->value[offset - 1]))
               && (match->position == match->length
                  || vtt_selector_space((unsigned char)match->value[match->position]))))
         match->result = 1;
      else match->matched = pattern->prefix[match->matched - 1];
   }
   return (int)match->result;
}

int rsubtitle_vtt_selector_subject_begin(rsubtitle_vtt_selector_subject_t *subject,
      const rsubtitle_vtt_selector_pattern_t *pattern, const rsubtitle_srt_t *track,
      size_t cue, size_t index, enum rsubtitle_vtt_selector_kind kind, int64_t position_us)
{
   static const char *names[] = { "", "", "c", "b", "i", "u", "ruby", "rt", "v", "lang", "" };
   const rsubtitle_vtt_node_t *node = rsubtitle_vtt_node(track, cue, index);
   const char *value = NULL;
   size_t length = 0;
   if (!subject || !node || kind < RSUBTITLE_SELECTOR_TYPE || kind > RSUBTITLE_SELECTOR_FUTURE)
      return 0;
   if (kind <= RSUBTITLE_SELECTOR_LANGUAGE && (!pattern || !pattern->ready)) return 0;
   if (kind <= RSUBTITLE_SELECTOR_CLASS && (pattern->op != RSUBTITLE_SELECTOR_EXACT || pattern->insensitive))
      return 0;
   memset(subject, 0, sizeof(*subject));
   subject->track = track; subject->cue = cue; subject->node = index;
   subject->kind = kind; subject->pattern = pattern;
   if (node->kind == RSUBTITLE_VTT_TEXT || node->kind == RSUBTITLE_VTT_TIMESTAMP)
      subject->result = 2;
   else if (kind == RSUBTITLE_SELECTOR_UNIVERSAL) subject->result = 1;
   else if (kind == RSUBTITLE_SELECTOR_ROOT) subject->result = node->kind == RSUBTITLE_VTT_ROOT ? 1 : 2;
   else if (kind == RSUBTITLE_SELECTOR_PAST || kind == RSUBTITLE_SELECTOR_FUTURE)
   {
      unsigned state = kind == RSUBTITLE_SELECTOR_PAST ? RSUBTITLE_VTT_NODE_PAST : RSUBTITLE_VTT_NODE_FUTURE;
      subject->result = rsubtitle_vtt_node_state(track, cue, index, position_us) & state ? 1 : 2;
   }
   else if (kind == RSUBTITLE_SELECTOR_TYPE)
   {
      if (node->kind == RSUBTITLE_VTT_ROOT) subject->result = 2;
      else { value = names[node->kind]; length = strlen(value); }
   }
   else if (kind == RSUBTITLE_SELECTOR_ID)
   {
      if (node->kind != RSUBTITLE_VTT_ROOT) subject->result = 2;
      else subject->phase = 1;
   }
   else if (kind == RSUBTITLE_SELECTOR_CLASS) subject->phase = 2;
   else if ((kind == RSUBTITLE_SELECTOR_VOICE && node->kind == RSUBTITLE_VTT_VOICE)
         || (kind == RSUBTITLE_SELECTOR_LANGUAGE && node->kind == RSUBTITLE_VTT_LANGUAGE))
   {
      value = node->value; length = node->value_length;
   }
   else subject->result = 2;
   if (!subject->result && !subject->phase)
      return rsubtitle_vtt_selector_match_begin(&subject->match, pattern, value, length);
   return 1;
}

int rsubtitle_vtt_selector_subject_step(rsubtitle_vtt_selector_subject_t *subject)
{
   const char *value;
   size_t length;
   int status;
   if (!subject || !subject->track) return -1;
   if (subject->result) return (int)subject->result;
   if (subject->phase == 1)
   {
      const rsubtitle_vtt_cue_t *cue = rsubtitle_vtt_cue_info(subject->track, subject->cue);
      if (!cue->identifier || !*cue->identifier) { subject->result = 2; return 2; }
      if (cue->identifier[subject->length]) { subject->length++; return 0; }
      if (!rsubtitle_vtt_selector_match_begin(&subject->match, subject->pattern, cue->identifier, subject->length))
         return -1;
      subject->phase = 0;
      return 0;
   }
   if (subject->phase == 2)
   {
      value = rsubtitle_vtt_node_class(subject->track, subject->cue, subject->node,
            subject->class_index++, &length);
      if (!value) { subject->result = 2; return 2; }
      if (!rsubtitle_vtt_selector_match_begin(&subject->match, subject->pattern, value, length)) return -1;
      subject->phase = 3;
      return 0;
   }
   status = rsubtitle_vtt_selector_match_step(&subject->match);
   if (status == 2 && subject->phase == 3) { subject->phase = 2; return 0; }
   if (status > 0) subject->result = (unsigned)status;
   return status;
}
