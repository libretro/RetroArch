/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rsubtitle_vtt_internal.h).
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

#ifndef LIBRETRO_RSUBTITLE_VTT_INTERNAL_H
#define LIBRETRO_RSUBTITLE_VTT_INTERNAL_H
#include <formats/rsubtitle_vtt.h>

typedef struct rsubtitle_vtt_text rsubtitle_vtt_text_t;
typedef struct rsubtitle_vtt_regions rsubtitle_vtt_regions_t;

typedef struct rsubtitle_vtt_css rsubtitle_vtt_css_t;
typedef struct rsubtitle_vtt_css_tree rsubtitle_vtt_css_tree_t;

typedef struct rsubtitle_vtt_css_rules
{
   const rsubtitle_vtt_css_tree_t *tree;
   size_t cursor, first, last;
   unsigned at_rule, top_level, prefix;
} rsubtitle_vtt_css_rules_t;

typedef struct rsubtitle_vtt_css_rule
{
   size_t first, last, body;
   unsigned at_rule;
} rsubtitle_vtt_css_rule_t;

typedef struct rsubtitle_vtt_css_declarations
{
   const rsubtitle_vtt_css_tree_t *tree;
   size_t cursor, property, first, last[3], values, curly;
   unsigned phase;
} rsubtitle_vtt_css_declarations_t;

typedef struct rsubtitle_vtt_css_declaration
{
   size_t property, first, last;
   unsigned important;
} rsubtitle_vtt_css_declaration_t;

enum rsubtitle_vtt_selector_operator
{
   RSUBTITLE_SELECTOR_PRESENT, RSUBTITLE_SELECTOR_EXACT, RSUBTITLE_SELECTOR_WORD,
   RSUBTITLE_SELECTOR_DASH, RSUBTITLE_SELECTOR_PREFIX, RSUBTITLE_SELECTOR_SUFFIX,
   RSUBTITLE_SELECTOR_SUBSTRING
};

enum rsubtitle_vtt_selector_kind
{
   RSUBTITLE_SELECTOR_TYPE, RSUBTITLE_SELECTOR_ID, RSUBTITLE_SELECTOR_CLASS,
   RSUBTITLE_SELECTOR_VOICE, RSUBTITLE_SELECTOR_LANGUAGE,
   RSUBTITLE_SELECTOR_UNIVERSAL, RSUBTITLE_SELECTOR_ROOT,
   RSUBTITLE_SELECTOR_PAST, RSUBTITLE_SELECTOR_FUTURE
};

typedef struct rsubtitle_vtt_selector_pattern rsubtitle_vtt_selector_pattern_t;
typedef struct rsubtitle_vtt_tag rsubtitle_vtt_tag_t;
typedef struct rsubtitle_vtt_normalize rsubtitle_vtt_normalize_t;

enum rsubtitle_vtt_alias_kind
{
   RSUBTITLE_ALIAS_LANGUAGE, RSUBTITLE_ALIAS_EXTLANG, RSUBTITLE_ALIAS_SCRIPT,
   RSUBTITLE_ALIAS_REGION, RSUBTITLE_ALIAS_VARIANT, RSUBTITLE_ALIAS_TAG
};

enum rsubtitle_vtt_normalize_flags
{
   RSUBTITLE_NORMALIZE_EXTLANG = 1,
   RSUBTITLE_NORMALIZE_RANGE = 2
};

typedef struct rsubtitle_vtt_language_key
{
   const char *text;
   size_t length;
   char *owned;
} rsubtitle_vtt_language_key_t;

typedef struct rsubtitle_vtt_alias_lookup
{
   const char *key;
   size_t length, lower, upper, middle, position;
   enum rsubtitle_vtt_alias_kind kind;
   unsigned begun, comparing;
   int result;
} rsubtitle_vtt_alias_lookup_t;

typedef struct rsubtitle_vtt_alias_value
{
   const char *preferred;
   size_t preferred_length;
   const char *extlang_prefix;
   size_t extlang_prefix_length;
} rsubtitle_vtt_alias_value_t;

typedef struct rsubtitle_vtt_language_match
{
   const char *range, *tag;
   size_t range_length, tag_length, range_position, tag_position;
   size_t position, subtag_length;
   unsigned phase, first, wildcard;
   int result;
} rsubtitle_vtt_language_match_t;

typedef struct rsubtitle_vtt_selector_match
{
   const rsubtitle_vtt_selector_pattern_t *pattern;
   const char *value;
   size_t length, position, matched;
   unsigned result;
} rsubtitle_vtt_selector_match_t;

typedef struct rsubtitle_vtt_selector_subject
{
   const rsubtitle_srt_t *track;
   const rsubtitle_vtt_selector_pattern_t *pattern;
   rsubtitle_vtt_selector_match_t match;
   size_t cue, node, class_index, length;
   enum rsubtitle_vtt_selector_kind kind;
   unsigned phase, result;
} rsubtitle_vtt_selector_subject_t;

RETRO_BEGIN_DECLS
const char *rsubtitle_vtt_text_language_key(const rsubtitle_vtt_text_t *text,
      size_t index, size_t *length);
rsubtitle_vtt_normalize_t *rsubtitle_vtt_normalize_new(void);
void rsubtitle_vtt_normalize_free(rsubtitle_vtt_normalize_t *normal);
/* Comparison keys only: never serialize into language metadata.
 * RANGE accepts extended ranges and CSS empty language; EXTLANG adds prefixes.
 * Input/output limit is 65536 bytes; expansion errors never truncate. */
int rsubtitle_vtt_normalize_begin(rsubtitle_vtt_normalize_t *normal,
      const char *text, size_t length, unsigned flags);
/* -1 error, 0 pending, 1 ready, 2 malformed; bounded work per step. */
int rsubtitle_vtt_normalize_step(rsubtitle_vtt_normalize_t *normal);
/* Borrowed until begin/free; unchanged results retain the input pointer. */
const char *rsubtitle_vtt_normalize_result(
      const rsubtitle_vtt_normalize_t *normal, size_t *length);
/* Consumes a ready result without copying; caller frees key.owned. */
int rsubtitle_vtt_normalize_take(rsubtitle_vtt_normalize_t *normal,
      rsubtitle_vtt_language_key_t *key);
rsubtitle_vtt_tag_t *rsubtitle_vtt_tag_new(void);
void rsubtitle_vtt_tag_free(rsubtitle_vtt_tag_t *tag);
/* Borrows at most 65536 bytes; checks grammar, not registry membership. */
int rsubtitle_vtt_tag_begin(rsubtitle_vtt_tag_t *tag,
      const char *text, size_t length);
/* One byte, alias step or variant-table probe: -1 error, 0 pending,
 * 1 well-formed, 2 malformed. Empty text is untagged, not a language tag. */
int rsubtitle_vtt_tag_step(rsubtitle_vtt_tag_t *tag);
/* Borrows key until completion. A miss does not invalidate a language tag. */
int rsubtitle_vtt_alias_begin(rsubtitle_vtt_alias_lookup_t *lookup,
      enum rsubtitle_vtt_alias_kind kind, const char *key, size_t length);
/* One byte or kind comparison: -1 invalid, 0 pending, 1 found, 2 absent. */
int rsubtitle_vtt_alias_step(rsubtitle_vtt_alias_lookup_t *lookup);
/* Static borrowed spans; an empty preferred value leaves a recognized tag unchanged. */
int rsubtitle_vtt_alias_result(const rsubtitle_vtt_alias_lookup_t *lookup,
      rsubtitle_vtt_alias_value_t *value);
/* Inputs borrow canonicalized BCP47 text. Empty range matches untagged text. */
int rsubtitle_vtt_language_begin(rsubtitle_vtt_language_match_t *match,
      const char *range, size_t range_length, const char *tag, size_t tag_length);
/* RFC4647 extended filtering: -1 invalid, 0 pending, 1 match, 2 no match.
 * One validation byte or at most one eight-byte subtag pair; no allocation. */
int rsubtitle_vtt_language_step(rsubtitle_vtt_language_match_t *match);
rsubtitle_vtt_css_t *rsubtitle_vtt_css_new(void);
void rsubtitle_vtt_css_free(rsubtitle_vtt_css_t *css);
/* Borrows a normalized STYLE block, at most 65536 bytes. */
int rsubtitle_vtt_css_begin(rsubtitle_vtt_css_t *css, const char *input, size_t size);
/* One bounded step: -1 error, 0 pending, 1 token, 2 end. Token text is
 * borrowed until the next step. Numbers carry their unit in text. */
int rsubtitle_vtt_css_step(rsubtitle_vtt_css_t *css, rsubtitle_vtt_css_token_t *token);
rsubtitle_vtt_css_tree_t *rsubtitle_vtt_css_tree_new(void);
void rsubtitle_vtt_css_tree_free(rsubtitle_vtt_css_tree_t *tree);
int rsubtitle_vtt_css_tree_begin(rsubtitle_vtt_css_tree_t *tree,
      const char *input, size_t size, size_t *root);
int rsubtitle_vtt_css_tree_step(rsubtitle_vtt_css_tree_t *tree);
size_t rsubtitle_vtt_css_tree_count(const rsubtitle_vtt_css_tree_t *tree);
const rsubtitle_vtt_css_node_t *rsubtitle_vtt_css_tree_node(
      const rsubtitle_vtt_css_tree_t *tree, size_t index);
int rsubtitle_vtt_css_rules_begin(rsubtitle_vtt_css_rules_t *rules,
      const rsubtitle_vtt_css_tree_t *tree, size_t root);
/* Consumes one component: -1 invalid, 0 pending, 1 rule, 2 end.
 * Spans and bodies borrow indices from the completed component tree. */
int rsubtitle_vtt_css_rules_step(rsubtitle_vtt_css_rules_t *rules,
      rsubtitle_vtt_css_rule_t *rule);
int rsubtitle_vtt_css_declarations_begin(rsubtitle_vtt_css_declarations_t *decls,
      const rsubtitle_vtt_css_tree_t *tree, size_t body);
/* One component per step; statuses and borrowed spans match rules_step.
 * Property grammars and nested rules are evaluated separately. */
int rsubtitle_vtt_css_declarations_step(rsubtitle_vtt_css_declarations_t *decls,
      rsubtitle_vtt_css_declaration_t *declaration);
rsubtitle_vtt_selector_pattern_t *rsubtitle_vtt_selector_pattern_new(void);
void rsubtitle_vtt_selector_pattern_free(rsubtitle_vtt_selector_pattern_t *pattern);
/* Borrows normalized selector text. Prepare while loading, at most 65536 bytes. */
int rsubtitle_vtt_selector_pattern_begin(rsubtitle_vtt_selector_pattern_t *pattern,
      const char *value, size_t length, enum rsubtitle_vtt_selector_operator op,
      unsigned insensitive);
/* Preparation: -1 invalid, 0 pending, 1 ready. One byte or prefix fallback. */
int rsubtitle_vtt_selector_pattern_step(rsubtitle_vtt_selector_pattern_t *pattern);
int rsubtitle_vtt_selector_match_begin(rsubtitle_vtt_selector_match_t *match,
      const rsubtitle_vtt_selector_pattern_t *pattern, const char *value, size_t length);
/* Matching: -1 invalid, 0 pending, 1 matches, 2 does not match. No allocation.
 * Keep the prepared pattern and borrowed target unchanged while matching. */
int rsubtitle_vtt_selector_match_step(rsubtitle_vtt_selector_match_t *match);
int rsubtitle_vtt_selector_subject_begin(rsubtitle_vtt_selector_subject_t *subject,
      const rsubtitle_vtt_selector_pattern_t *pattern, const rsubtitle_srt_t *track,
      size_t cue, size_t node, enum rsubtitle_vtt_selector_kind kind, int64_t position_us);
int rsubtitle_vtt_selector_subject_step(rsubtitle_vtt_selector_subject_t *subject);
rsubtitle_vtt_regions_t *rsubtitle_vtt_regions_new(void);
void rsubtitle_vtt_regions_free(rsubtitle_vtt_regions_t *regions);
int rsubtitle_vtt_regions_begin(rsubtitle_vtt_regions_t *regions,
      const char *input, size_t size);
int rsubtitle_vtt_regions_step(rsubtitle_vtt_regions_t *regions);
size_t rsubtitle_vtt_regions_count(const rsubtitle_vtt_regions_t *regions);
const rsubtitle_vtt_region_t *rsubtitle_vtt_regions_get(
      const rsubtitle_vtt_regions_t *regions, size_t index);
size_t rsubtitle_vtt_regions_first(const rsubtitle_vtt_regions_t *regions,
      const char *identifier, size_t size);
int rsubtitle_vtt_regions_find_step(const rsubtitle_vtt_regions_t *regions,
      const char *identifier, size_t size, size_t *cursor, size_t *found);
void rsubtitle_vtt_settings_parse(const char *input, rsubtitle_vtt_settings_t *settings);
rsubtitle_vtt_text_t *rsubtitle_vtt_text_new(void);
void rsubtitle_vtt_text_free(rsubtitle_vtt_text_t *text);
int rsubtitle_vtt_text_begin(rsubtitle_vtt_text_t *text, const char *input,
      size_t size, const rsubtitle_vtt_cue_t *cue);
int rsubtitle_vtt_text_step(rsubtitle_vtt_text_t *text);
int rsubtitle_vtt_text_seal(rsubtitle_vtt_text_t *text);
size_t rsubtitle_vtt_text_count(const rsubtitle_vtt_text_t *text);
const rsubtitle_vtt_node_t *rsubtitle_vtt_text_node(
      const rsubtitle_vtt_text_t *text, size_t index);
const char *rsubtitle_vtt_text_class(const rsubtitle_vtt_text_t *text,
      size_t node, size_t index, size_t *length);
RETRO_END_DECLS
#endif
