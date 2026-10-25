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

struct rsubtitle_vtt_selector_pattern
{
   const char *value;
   uint32_t *prefix;
   size_t length, capacity, position, matched;
   enum rsubtitle_vtt_selector_operator op;
   unsigned insensitive, space, ready, begun;
};

static unsigned vtt_selector_fold(unsigned c, unsigned insensitive)
{
   return insensitive && c >= 'A' && c <= 'Z' ? c + 'a' - 'A' : c;
}

static int vtt_selector_space(unsigned c)
{
   return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
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
