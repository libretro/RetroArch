/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rsubtitle_vtt.h).
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

#ifndef LIBRETRO_RSUBTITLE_VTT_H
#define LIBRETRO_RSUBTITLE_VTT_H

#include <formats/rsubtitle_srt.h>

enum rsubtitle_vtt_cue_flags
{
   RSUBTITLE_VTT_CUE_RELATIVE_TIMESTAMPS = 1
};

typedef struct rsubtitle_vtt_cue
{
   const char *identifier;
   const char *settings;
   /* The untrimmed origin for relative inline timestamps. */
   int64_t timestamp_origin_ns;
   unsigned flags;
} rsubtitle_vtt_cue_t;

typedef struct rsubtitle_vtt_span
{
   size_t identifier_offset, identifier_length;
   size_t settings_offset, settings_length;
   int64_t timestamp_origin_ns;
   unsigned flags;
} rsubtitle_vtt_span_t;

enum rsubtitle_vtt_alignment
{
   RSUBTITLE_VTT_START, RSUBTITLE_VTT_CENTER, RSUBTITLE_VTT_END,
   RSUBTITLE_VTT_LEFT, RSUBTITLE_VTT_RIGHT, RSUBTITLE_VTT_AUTO
};

enum rsubtitle_vtt_direction
{
   RSUBTITLE_VTT_HORIZONTAL, RSUBTITLE_VTT_VERTICAL_RL, RSUBTITLE_VTT_VERTICAL_LR
};

typedef struct rsubtitle_vtt_settings
{
   double line, position, size;
   const char *region;
   size_t region_length;
   enum rsubtitle_vtt_alignment align, line_align, position_align;
   enum rsubtitle_vtt_direction direction;
   unsigned line_auto, position_auto, snap_to_lines;
} rsubtitle_vtt_settings_t;

typedef struct rsubtitle_vtt_region
{
   const char *identifier;
   size_t identifier_length;
   double width, anchor_x, anchor_y, viewport_x, viewport_y;
   unsigned lines, scroll_up;
} rsubtitle_vtt_region_t;

#define RSUBTITLE_VTT_NO_NODE ((size_t)-1)

enum rsubtitle_vtt_node_kind
{
   RSUBTITLE_VTT_ROOT, RSUBTITLE_VTT_TEXT, RSUBTITLE_VTT_CLASS,
   RSUBTITLE_VTT_BOLD, RSUBTITLE_VTT_ITALIC, RSUBTITLE_VTT_UNDERLINE,
   RSUBTITLE_VTT_RUBY, RSUBTITLE_VTT_RUBY_TEXT, RSUBTITLE_VTT_VOICE,
   RSUBTITLE_VTT_LANGUAGE, RSUBTITLE_VTT_TIMESTAMP
};

enum rsubtitle_vtt_hint_flags
{
   RSUBTITLE_VTT_HINT_COLOR = 1,
   RSUBTITLE_VTT_HINT_BACKGROUND = 2,
   RSUBTITLE_VTT_HINT_BOLD = 4,
   RSUBTITLE_VTT_HINT_ITALIC = 8,
   RSUBTITLE_VTT_HINT_UNDERLINE = 16
};

/* Local presentational hints; colors are RGBA, before CSS and inheritance. */
typedef struct rsubtitle_vtt_hints
{
   uint32_t color, background;
   unsigned flags;
} rsubtitle_vtt_hints_t;

typedef struct rsubtitle_vtt_node
{
   const char *value;
   size_t value_length, class_count;
   size_t parent, first_child, next;
   int64_t timestamp_us;
   rsubtitle_vtt_hints_t hints;
   /* One-based cue-relative timestamp nodes; zero means no boundary. */
   uint32_t past_timestamp, future_timestamp;
   /* One-based cue-relative language node; zero uses the track fallback. */
   uint32_t language_node;
   enum rsubtitle_vtt_node_kind kind;
} rsubtitle_vtt_node_t;

enum rsubtitle_vtt_node_state
{
   RSUBTITLE_VTT_NODE_PAST = 1,
   RSUBTITLE_VTT_NODE_FUTURE = 2
};

enum rsubtitle_vtt_css_kind
{
   RSUBTITLE_CSS_IDENT, RSUBTITLE_CSS_FUNCTION, RSUBTITLE_CSS_AT,
   RSUBTITLE_CSS_HASH, RSUBTITLE_CSS_STRING, RSUBTITLE_CSS_BAD_STRING,
   RSUBTITLE_CSS_URL, RSUBTITLE_CSS_BAD_URL, RSUBTITLE_CSS_NUMBER,
   RSUBTITLE_CSS_PERCENT, RSUBTITLE_CSS_DIMENSION, RSUBTITLE_CSS_SPACE,
   RSUBTITLE_CSS_DELIM, RSUBTITLE_CSS_CDO, RSUBTITLE_CSS_CDC,
   RSUBTITLE_CSS_BLOCK, RSUBTITLE_CSS_ROOT
};

enum rsubtitle_vtt_css_flags
{
   RSUBTITLE_CSS_INTEGER = 1,
   RSUBTITLE_CSS_ID = 2,
   RSUBTITLE_CSS_PLUS = 4,
   RSUBTITLE_CSS_MINUS = 8
};

typedef struct rsubtitle_vtt_css_token
{
   const char *text;
   size_t length;
   double number;
   enum rsubtitle_vtt_css_kind kind;
   unsigned flags;
   uint32_t delimiter;
} rsubtitle_vtt_css_token_t;

typedef struct rsubtitle_vtt_css_node
{
   rsubtitle_vtt_css_token_t token;
   size_t parent, first_child, next;
} rsubtitle_vtt_css_node_t;

enum rsubtitle_vtt_block_kind
{
   RSUBTITLE_VTT_STYLE,
   RSUBTITLE_VTT_REGION
};

typedef struct rsubtitle_vtt_block
{
   const char *text;
   size_t length, css_root;
   enum rsubtitle_vtt_block_kind kind;
} rsubtitle_vtt_block_t;

RETRO_BEGIN_DECLS

/* Consumes UTF-8 input. Uses the shared incremental cue index and limits.
 * Header lines: 128 bytes; cue text and global blocks: 65536 bytes each. */
rsubtitle_srt_t *rsubtitle_vtt_begin(char *data, size_t size);
/* Consumes all malloc-allocated inputs, including on failure. Metadata may
 * be NULL; its offsets name NUL-terminated strings in data. Header contains
 * global blocks. */
rsubtitle_srt_t *rsubtitle_vtt_begin_cues(char *data, size_t size,
      rsubtitle_srt_span_t *spans, size_t count, rsubtitle_vtt_span_t *metadata,
      char *header, size_t header_size);
/* Metadata remains borrowed until rsubtitle_srt_free(). */
const rsubtitle_vtt_cue_t *rsubtitle_vtt_cue_info(
      const rsubtitle_srt_t *track, size_t index);
/* Map a nonnegative inline timestamp to the media clock. Submicrosecond
 * origins round up; returns 0 for invalid input or overflow. */
int rsubtitle_vtt_timestamp_us(const rsubtitle_vtt_cue_t *cue,
      int64_t inline_us, int64_t *time_us);
/* Parsed during loading; percentages use the range 0..100. Region retains
 * the requested identifier; cue_region() supplies the resolved definition. */
const rsubtitle_vtt_settings_t *rsubtitle_vtt_cue_settings(
      const rsubtitle_srt_t *track, size_t cue);
/* Node zero is the cue root. Links are cue-relative; strings are borrowed
 * spans, not necessarily NUL-terminated. Limits: 128 levels, 262144 nodes. */
size_t rsubtitle_vtt_node_count(const rsubtitle_srt_t *track, size_t cue);
const rsubtitle_vtt_node_t *rsubtitle_vtt_node(const rsubtitle_srt_t *track,
      size_t cue, size_t node);
/* Consumes malloc-allocated applicable BCP47 language, including on failure.
 * Set once before parsing completes, on the loading worker. The raw span is
 * retained; comparison keys are prepared within iterate's work budget. */
int rsubtitle_vtt_take_language(rsubtitle_srt_t *track, char *language, size_t length);
const char *rsubtitle_vtt_track_language(const rsubtitle_srt_t *track, size_t *length);
const char *rsubtitle_vtt_track_language_key(const rsubtitle_srt_t *track, size_t *length);
/* Raw applicable language for internal nodes. NULL fallback uses the track;
 * a non-NULL empty span explicitly overrides it. */
const char *rsubtitle_vtt_node_language(const rsubtitle_srt_t *track,
      size_t cue, size_t node, const char *fallback, size_t fallback_length,
      size_t *length);
/* Load-prepared comparison key; NULL fallback uses the track key.
 * Malformed annotations retain raw text, never become an empty language. */
const char *rsubtitle_vtt_node_language_key(const rsubtitle_srt_t *track,
      size_t cue, size_t node, const char *fallback, size_t fallback_length,
      size_t *length);
/* Strict clock comparisons; leaf nodes cannot match CSS pseudo-classes. */
unsigned rsubtitle_vtt_node_state(const rsubtitle_srt_t *track,
      size_t cue, size_t node, int64_t position_us);
const char *rsubtitle_vtt_node_class(const rsubtitle_srt_t *track,
      size_t cue, size_t node, size_t index, size_t *length);
/* Regions are parsed and bound during loading; identifiers are borrowed spans.
 * Line counts beyond the unsigned range saturate. */
size_t rsubtitle_vtt_region_count(const rsubtitle_srt_t *track);
const rsubtitle_vtt_region_t *rsubtitle_vtt_region(
      const rsubtitle_srt_t *track, size_t index);
const rsubtitle_vtt_region_t *rsubtitle_vtt_cue_region(
      const rsubtitle_srt_t *track, size_t cue);
/* CSS components are owned by the track. Links and css_root are track-wide
 * indices. Limits: 128 levels, 262144 nodes and 16 MiB normalized strings. */
size_t rsubtitle_vtt_css_node_count(const rsubtitle_srt_t *track);
const rsubtitle_vtt_css_node_t *rsubtitle_vtt_css_node(
      const rsubtitle_srt_t *track, size_t index);
size_t rsubtitle_vtt_block_count(const rsubtitle_srt_t *track);
const rsubtitle_vtt_block_t *rsubtitle_vtt_block(
      const rsubtitle_srt_t *track, size_t index);

RETRO_END_DECLS

#endif
