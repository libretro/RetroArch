/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (gfx_animation_sanitize_test.c).
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

/* Sanitizer sweep for gfx/gfx_animation.c.
 *
 * Two areas are worth the sanitizers' attention.
 *
 * The tween list: entries hold a pointer to the caller's float
 * and an optional userdata, and are killed by tag or expire on
 * their own.  A subject that outlives its entry, or an entry
 * that outlives its subject, is a use-after-free that a normal
 * run will not notice because the memory is usually still there.
 * So the test animates subjects on the heap and frees them while
 * the animation is in flight, which is what a menu does every
 * time a list is repopulated mid-transition.
 *
 * The tickers: horizontal and vertical forms write into a
 * caller-supplied buffer of a caller-supplied length.  Off-by-one
 * there is invisible until it is not.  The test feeds them
 * multi-byte text, buffers of every length from 1 upward, and
 * field widths from 0 up, with the destination heap-allocated
 * and exactly sized so ASan can see a single byte over.
 *
 * Run it three ways:
 *
 *     make sweep
 *
 * which is plain, then address+undefined with leak detection,
 * then thread.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <retro_miscellaneous.h>

#include <array/rbuf.h>

#include "gfx/gfx_animation.h"

static int failures;

#define CHECK(cond, msg) \
   do { if (!(cond)) { fprintf(stderr, "FAIL: %s\n", (msg)); failures++; } } while (0)

/* Counted so a run that reaches nothing cannot report success. */
static int cb_calls;
static int pushes;
static int ticker_calls;

static void on_tween_done(void *userdata)
{
   (void)userdata;
   cb_calls++;
}

/* ------------------------------------------------------------------
 * 1. push / update / expire, every easing type
 * ------------------------------------------------------------------ */
static void test_easing_types(void)
{
   int e;

   for (e = 0; e < EASING_LAST; e++)
   {
      gfx_animation_ctx_entry_t entry;
      float subject = 0.0f;
      int   i;

      memset(&entry, 0, sizeof(entry));
      entry.subject      = &subject;
      entry.target_value = 100.0f;
      entry.duration     = 16.0f;
      entry.easing_enum  = (enum gfx_animation_easing_type)e;
      entry.tag          = (uintptr_t)&subject;
      entry.cb           = on_tween_done;

      if (gfx_animation_push(&entry))
         pushes++;

      /* Drive it past its duration in small steps. */
      for (i = 0; i < 32; i++)
         gfx_animation_update(i * 1000, false, 60.0f, VIDEO_SCALE_PACK(1920, 1080));

      gfx_animation_kill_by_tag(&entry.tag);
   }

   gfx_animation_deinit();
}

/* ------------------------------------------------------------------
 * 2. subject freed while the animation is in flight
 *
 * A menu repopulating a list mid-transition does exactly this.
 * The entry has to go with the subject, or the next update writes
 * through a dangling float*.
 * ------------------------------------------------------------------ */
static void test_subject_freed_midflight(void)
{
   int i;

   for (i = 0; i < 64; i++)
   {
      gfx_animation_ctx_entry_t entry;
      float    *subject = (float*)calloc(1, sizeof(float));
      uintptr_t tag     = (uintptr_t)subject;

      if (!subject)
         return;

      memset(&entry, 0, sizeof(entry));
      entry.subject      = subject;
      entry.target_value = 1.0f;
      entry.duration     = 1000.0f;   /* long: still running when killed */
      entry.easing_enum  = EASING_LINEAR;
      entry.tag          = tag;
      entry.cb           = on_tween_done;

      if (gfx_animation_push(&entry))
         pushes++;

      /* Part-way through. */
      gfx_animation_update((uint64_t)i * 100, false, 60.0f, VIDEO_SCALE_PACK(1920, 1080));

      /* Kill first, then free -- the order the caller is obliged to
       * use.  Getting it wrong is the use-after-free this is looking
       * for; getting it right must leave nothing behind either. */
      gfx_animation_kill_by_tag(&tag);
      free(subject);

      /* Anything still referencing it would be caught here. */
      gfx_animation_update((uint64_t)i * 100 + 50, false, 60.0f, VIDEO_SCALE_PACK(1920, 1080));
   }

   gfx_animation_deinit();
}

/* ------------------------------------------------------------------
 * 3. deinit with animations still in flight
 * ------------------------------------------------------------------ */
static void test_deinit_while_running(void)
{
   float subjects[16];
   int   i;

   memset(subjects, 0, sizeof(subjects));

   for (i = 0; i < 16; i++)
   {
      gfx_animation_ctx_entry_t entry;
      memset(&entry, 0, sizeof(entry));
      entry.subject      = &subjects[i];
      entry.target_value = 1.0f;
      entry.duration     = 10000.0f;
      entry.easing_enum  = EASING_OUT_BOUNCE;
      entry.tag          = (uintptr_t)&subjects[i];
      entry.cb           = on_tween_done;
      if (gfx_animation_push(&entry))
         pushes++;
   }

   gfx_animation_update(1, false, 60.0f, VIDEO_SCALE_PACK(1920, 1080));
   gfx_animation_deinit();

   /* Deinit twice: the idempotent-teardown path. */
   gfx_animation_deinit();
}

/* ------------------------------------------------------------------
 * 4. tickers, exactly-sized destinations
 *
 * dst is heap-allocated at precisely the length handed to the
 * ticker, so a single byte written past the end is an ASan report
 * rather than a quiet stomp on the next stack slot.
 * ------------------------------------------------------------------ */
static const char *const ticker_inputs[] =
{
   "",
   "a",
   "short",
   "a moderately long entry label that will not fit",
   /* Multi-byte: the slicing is per glyph, not per byte. */
   "\xc3\xa9\xc3\xa8\xc3\xaa\xc3\xab accented",
   "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e \xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88",
   "mixed \xc3\xa9 and \xe6\x97\xa5 together"
};

static void test_ticker(void)
{
   size_t i;
   int    t;
   size_t len;
   uint64_t idx;

   for (i = 0; i < sizeof(ticker_inputs) / sizeof(ticker_inputs[0]); i++)
   {
      for (t = 0; t < TICKER_TYPE_LAST; t++)
      {
         for (len = 1; len <= 24; len++)
         {
            for (idx = 0; idx < 8; idx++)
            {
               gfx_animation_ctx_ticker_t ticker;
               char *dst = (char*)malloc(len);

               if (!dst)
                  return;

               /* Prefilled with a non-NUL sentinel: the ticker may
                * legitimately write nothing (it returns false when no
                * scrolling is needed and the caller already has the
                * string), so a NUL found here has to have been put
                * there by the call, not left over from malloc. */
               memset(dst, 'X', len);

               memset(&ticker, 0, sizeof(ticker));
               ticker.s         = dst;
               ticker.s_len     = len;
               ticker.len       = len;
               ticker.idx       = idx * 37;
               ticker.str       = ticker_inputs[i];
               ticker.spacer    = NULL;
               ticker.selected  = (idx & 1) ? true : false;
               ticker.type_enum = (enum gfx_animation_ticker_type)t;

               gfx_animation_ticker(&ticker);
               ticker_calls++;

               /* The contract is bounded writes, not that anything is
                * written at all.  ASan owns the bound; all that can be
                * asserted here is that if it did write, it terminated.
                * Anything still 'X' at the front means it declined,
                * which is legitimate. */
               if (dst[0] != 'X')
                  CHECK(memchr(dst, '\0', len) != NULL,
                        "ticker wrote an unterminated string");

               free(dst);
            }
         }
      }
   }
}

static void test_ticker_smooth(void)
{
   size_t i;
   int    t;
   size_t len;
   unsigned field;

   for (i = 0; i < sizeof(ticker_inputs) / sizeof(ticker_inputs[0]); i++)
   {
      for (t = 0; t < TICKER_TYPE_LAST; t++)
      {
         for (len = 1; len <= 24; len++)
         {
            for (field = 0; field <= 64; field += 8)
            {
               gfx_animation_ctx_ticker_smooth_t ticker;
               char    *dst       = (char*)malloc(len);
               unsigned x_offset  = 0;
               unsigned dst_width = 0;

               if (!dst)
                  return;

               memset(dst, 'X', len);

               memset(&ticker, 0, sizeof(ticker));
               ticker.dst_str       = dst;
               ticker.dst_str_len   = len;
               ticker.dst_str_width = &dst_width;
               ticker.x_offset      = &x_offset;
               ticker.idx           = (uint64_t)field * 13;
               ticker.src_str       = ticker_inputs[i];
               ticker.spacer        = NULL;
               ticker.font          = NULL;
               ticker.glyph_width   = 8;
               ticker.field_width   = field;
               ticker.font_scale    = 1.0f;
               ticker.selected      = (field & 8) ? true : false;
               ticker.type_enum     = (enum gfx_animation_ticker_type)t;

               gfx_animation_ticker_smooth(&ticker);
               ticker_calls++;

               if (dst[0] != 'X')
                  CHECK(memchr(dst, '\0', len) != NULL,
                        "smooth ticker wrote an unterminated string");

               free(dst);
            }
         }
      }
   }
}

/* The stack overflow this sample was written to find.
 *
 * gfx_animation_ticker() told utf8cpy() the destination was
 * PATH_MAX_LENGTH bytes regardless of what the caller had actually
 * provided.  Every caller in tree provides less -- NAME_MAX_LENGTH
 * mostly, 64 bytes in one RGUI case -- so the guard
 * `str_len <= ticker->len`, which compares GLYPHS, let a multi-byte
 * string through and utf8cpy wrote its BYTES past the end.
 *
 * RGUI's shape, with a Japanese playlist title: 100 glyphs at three
 * bytes each into a NAME_MAX_LENGTH buffer.
 */
static void test_multibyte_does_not_overflow(void)
{
   char     dst[NAME_MAX_LENGTH];
   char     src[512];
   gfx_animation_ctx_ticker_t ticker;
   unsigned i;

   for (i = 0; i < 100; i++)
      memcpy(src + i * 3, "\xe6\x97\xa5", 3);
   src[300] = '\0';

   memset(dst, 0, sizeof(dst));
   memset(&ticker, 0, sizeof(ticker));
   ticker.s         = dst;
   ticker.s_len     = sizeof(dst);
   ticker.len       = 100;              /* glyphs, and 100 <= 100 */
   ticker.str       = src;
   ticker.type_enum = TICKER_TYPE_BOUNCE;

   gfx_animation_ticker(&ticker);
   ticker_calls++;

   CHECK(strlen(dst) < sizeof(dst),
         "ticker overran a NAME_MAX_LENGTH buffer with multi-byte text");

   /* Same again through the ellipsis branch. */
   memset(dst, 0, sizeof(dst));
   ticker.len      = 50;                /* 100 glyphs > 50: truncates */
   ticker.selected = false;
   gfx_animation_ticker(&ticker);
   ticker_calls++;

   CHECK(strlen(dst) < sizeof(dst),
         "ticker overran the buffer on the ellipsis path");
}

/* A caller that forgot to set s_len must get nothing written, not an
 * unbounded write: utf8cpy computes its clamp as (len - 1), so zero
 * underflows to SIZE_MAX. */
static void test_zero_s_len_is_rejected(void)
{
   char dst[16];
   gfx_animation_ctx_ticker_t ticker;

   memset(dst, 'X', sizeof(dst));
   memset(&ticker, 0, sizeof(ticker));
   ticker.s         = dst;
   ticker.s_len     = 0;
   ticker.len       = 8;
   ticker.str       = "a string that will not fit in eight glyphs";
   ticker.type_enum = TICKER_TYPE_BOUNCE;

   gfx_animation_ticker(&ticker);
   ticker_calls++;

   CHECK(dst[0] == 'X', "a zero s_len still wrote to the destination");
}


/* ------------------------------------------------------------------
 * 8. widget tweens change hands
 *
 * Under the threaded video wrapper the worker owns the widget tweens
 * in an instance of its own and ticks it; without it they tick in the
 * main list. The hand-over moves them both ways and must neither drop
 * a tween, move a menu tween, nor tick one instance from the other.
 * ------------------------------------------------------------------ */
static void push_one(float *subject, bool widget, int *count)
{
   gfx_animation_ctx_entry_t entry;
   memset(&entry, 0, sizeof(entry));
   *subject           = 0.0f;
   entry.subject      = subject;
   entry.target_value = 1.0f;
   entry.duration     = 1000.0f;   /* ms */
   entry.easing_enum  = EASING_LINEAR;
   entry.tag          = (uintptr_t)subject;
   entry.cb           = on_tween_done;
   if (widget ? gfx_animation_push_widget(&entry) : gfx_animation_push(&entry))
   {
      pushes++;
      (*count)++;
   }
}

static void test_widget_handover(void)
{
   float menu_subject, widget_subject, late_subject, timer;
   gfx_timer_ctx_entry_t timer_entry;
   gfx_animation_ctx_ticker_t ticker;
   char dst[8];
   uintptr_t late_tag;
   int n        = 0;
   int cb_start;

   gfx_animation_deinit();
   /* Unowned: widget tweens share the main list */
   CHECK(anim_widgets_get_ptr() == anim_get_ptr(),
         "handover: widgets start on the main list");
   push_one(&menu_subject, false, &n);
   push_one(&widget_subject, true, &n);
   CHECK(n == 2, "handover: both pushes taken");
   gfx_animation_update(1000000, false, 1.0f, VIDEO_SCALE_PACK(640, 480));
   CHECK(RBUF_LEN(anim_get_ptr()->list) == 2,
         "handover: both tweens in the main list while unowned");

   /* To the worker: only the widget tween moves */
   gfx_animation_widgets_own(true);
   CHECK(anim_widgets_get_ptr() != anim_get_ptr(),
         "handover: worker owns an instance of its own");
   CHECK(RBUF_LEN(anim_get_ptr()->list) == 1,
         "handover: menu tween stays in the main list");
   CHECK(RBUF_LEN(anim_widgets_get_ptr()->list) == 1,
         "handover: widget tween moved to the worker");

   /* Each instance ticks only its own */
   gfx_animation_update(1500000, false, 1.0f, VIDEO_SCALE_PACK(640, 480));
   CHECK(menu_subject > 0.45f && menu_subject < 0.55f,
         "handover: main tick advances the menu tween");
   CHECK(widget_subject == 0.0f,
         "handover: main tick leaves the worker's tween alone");
   gfx_animation_update_widgets(1250000, 1.0f, VIDEO_SCALE_PACK(640, 480));
   CHECK(widget_subject > 0.2f && widget_subject < 0.3f,
         "handover: worker tick advances the widget tween on the shared clock");

   /* Pushes, kills and timers go to the owner */
   push_one(&late_subject, true, &n);
   CHECK(RBUF_LEN(anim_widgets_get_ptr()->list) == 2,
         "handover: widget push lands with the worker");
   timer_entry.cb       = NULL;
   timer_entry.userdata = NULL;
   timer_entry.duration = 100.0f;
   gfx_animation_timer_start_widget(&timer, &timer_entry);
   CHECK(RBUF_LEN(anim_widgets_get_ptr()->list) == 3,
         "handover: widget timer lands with the worker");
   late_tag = (uintptr_t)&late_subject;
   gfx_animation_kill_widget_by_tag(&late_tag);
   CHECK(RBUF_LEN(anim_widgets_get_ptr()->list) == 2,
         "handover: widget kill finds the worker's tween");

   /* The ticker flag is the instance's own */
   memset(&ticker, 0, sizeof(ticker));
   ticker.s     = dst;
   ticker.s_len = sizeof(dst);
   ticker.len   = 4;
   ticker.str   = "a label longer than four";
   ticker.idx   = 0;
   ticker.selected = true;
   ticker.type_enum = TICKER_TYPE_BOUNCE;
   anim_get_ptr()->flags &= ~GFX_ANIM_FLAG_TICKER_IS_ACTIVE;
   gfx_animation_ticker_widget(&ticker);
   ticker_calls++;
   CHECK(anim_widgets_get_ptr()->flags & GFX_ANIM_FLAG_TICKER_IS_ACTIVE,
         "handover: widget ticker marks the worker's instance");
   CHECK(!(anim_get_ptr()->flags & GFX_ANIM_FLAG_TICKER_IS_ACTIVE),
         "handover: widget ticker leaves the main instance alone");

   /* Back to the main list, mid-flight: carries on and completes */
   gfx_animation_widgets_own(false);
   CHECK(anim_widgets_get_ptr() == anim_get_ptr(),
         "handover: widgets back on the main list");
   CHECK(RBUF_LEN(anim_get_ptr()->list) == 3,
         "handover: widget tween and timer rejoin the menu tween");
   cb_start = cb_calls;
   gfx_animation_update(3000000, false, 1.0f, VIDEO_SCALE_PACK(640, 480));
   CHECK(widget_subject == 1.0f && menu_subject == 1.0f,
         "handover: both tweens complete on the main tick");
   CHECK(cb_calls - cb_start == 2,
         "handover: both completion callbacks ran once");
   CHECK(RBUF_LEN(anim_get_ptr()->list) == 0,
         "handover: nothing left behind");

   /* Update bookkeeping outlives a deinit, as the statics did */
   {
      retro_time_t last = anim_get_ptr()->last_ticker_update;
      gfx_animation_deinit();
      CHECK(anim_get_ptr()->last_ticker_update == last,
            "handover: deinit keeps the ticker bookkeeping");
   }
}

extern const char *animation_test_wideglyph;

static void test_line_ticker(void)
{
   gfx_animation_t anim;
   gfx_animation_ctx_line_ticker_t ticker;
   char out[64], *scratch = NULL, *previous;
   size_t capacity = 0, previous_capacity;
   size_t i, len;
   int type;

   memset(&anim, 0, sizeof(anim));
   memset(&ticker, 0, sizeof(ticker));
   ticker.s                = out;
   ticker.len              = sizeof(out);
   ticker.line_len         = 16;
   ticker.max_lines        = 2;
   ticker.wrap_scratch     = &scratch;
   ticker.wrap_scratch_cap = &capacity;
   ticker.str              = "one\ntwo\nthree\nfour";
   ticker.type_enum        = TICKER_TYPE_BOUNCE;
   CHECK(gfx_animation_line_ticker(&anim, &ticker),
         "line ticker: overflowing text animates");
   CHECK(strcmp(out, "one\ntwo") == 0,
         "line ticker: first window");
   CHECK(anim.flags & GFX_ANIM_FLAG_TICKER_IS_ACTIVE,
         "line ticker: marks the supplied animation instance");
   previous          = scratch;
   previous_capacity = capacity;
   ticker.idx        = TICKER_LINE_DISPLAY_TICKS(16) * 2;
   gfx_animation_line_ticker(&anim, &ticker);
   CHECK(strcmp(out, "two\nthree") == 0,
         "line ticker: bounce advances one line");
   CHECK(scratch == previous && capacity == previous_capacity,
         "line ticker: reuses caller scratch storage");
   ticker.type_enum = TICKER_TYPE_LOOP;
   ticker.idx       = TICKER_LINE_DISPLAY_TICKS(16);
   gfx_animation_line_ticker(&anim, &ticker);
   CHECK(strcmp(out, "two\nthree") == 0,
         "line ticker: loop advances one line");
   ticker.str = "short";
   anim.flags = 0;
   CHECK(!gfx_animation_line_ticker(&anim, &ticker)
         && strcmp(out, "short") == 0 && !anim.flags,
         "line ticker: fitting text stays still");

   for (i = 0; i < sizeof(ticker_inputs) / sizeof(ticker_inputs[0]); i++)
      for (type = 0; type < TICKER_TYPE_LAST; type++)
         for (len = 1; len <= 24; len++)
         {
            char *dst = (char*)malloc(len);
            if (!dst)
               continue;
            memset(dst, 'X', len);
            ticker.str       = ticker_inputs[i];
            ticker.s         = dst;
            ticker.len       = len;
            ticker.idx       = len * 7;
            ticker.type_enum = (enum gfx_animation_ticker_type)type;
            gfx_animation_line_ticker(&anim, &ticker);
            ticker_calls++;
            CHECK(memchr(dst, '\0', len) != NULL,
                  "line ticker: bounded destination is terminated");
            free(dst);
         }
   free(scratch);
}

static void test_line_ticker_smooth(void)
{
   gfx_animation_t anim;
   gfx_animation_ctx_line_ticker_smooth_t ticker;
   font_data_t font;
   char out[64], top[64], bottom[64];
   float y = 0, top_y = 0, bottom_y = 0, top_alpha = 0, bottom_alpha = 0;
   size_t i, len;
   int type, fade, wide;

   memset(&anim, 0, sizeof(anim));
   memset(&ticker, 0, sizeof(ticker));
   memset(&font, 0, sizeof(font));
   font.metrics.height           = 12;
   ticker.font                   = &font;
   ticker.font_scale             = 1.0f;
   ticker.field_dims             = VIDEO_SCALE_PACK(128, 24);
   ticker.dst_str                = out;
   ticker.dst_str_len            = sizeof(out);
   ticker.y_offset               = &y;
   ticker.top_fade_str           = top;
   ticker.top_fade_str_len       = sizeof(top);
   ticker.top_fade_y_offset      = &top_y;
   ticker.top_fade_alpha         = &top_alpha;
   ticker.bottom_fade_str        = bottom;
   ticker.bottom_fade_str_len    = sizeof(bottom);
   ticker.bottom_fade_y_offset   = &bottom_y;
   ticker.bottom_fade_alpha      = &bottom_alpha;
   ticker.fade_enabled           = true;
   ticker.src_str                = "short";
   CHECK(gfx_animation_line_ticker_smooth(&anim, &ticker)
         && strcmp(out, "short") == 0 && y == 0
         && !*top && !*bottom && top_alpha == 0 && bottom_alpha == 0,
         "smooth line ticker: fitting text has no offsets or fades");
   ticker.src_str = "one\ntwo\nthree\nfour";
   ticker.idx = TICKER_LINE_SMOOTH_SCROLL_TICKS(16) - 1;
   CHECK(gfx_animation_line_ticker_smooth(&anim, &ticker)
         && strcmp(out, "one\ntwo") == 0,
         "smooth line ticker: static first window");

   for (wide = 0; wide < 2; wide++)
      for (i = 0; i < sizeof(ticker_inputs) / sizeof(ticker_inputs[0]); i++)
         for (type = 0; type < TICKER_TYPE_LAST; type++)
            for (fade = 0; fade < 2; fade++)
               for (len = 1; len <= 24; len++)
               {
                  char *dst = (char*)calloc(len, 1);
                  char *fade_top = (char*)calloc(len, 1);
                  char *fade_bottom = (char*)calloc(len, 1);
                  if (!dst || !fade_top || !fade_bottom)
                  {
                     free(dst);
                     free(fade_top);
                     free(fade_bottom);
                     continue;
                  }
                  animation_test_wideglyph  = wide ? "\xE4\xB8\xAD" : NULL;
                  ticker.src_str            = ticker_inputs[i];
                  ticker.dst_str            = dst;
                  ticker.dst_str_len        = len;
                  ticker.top_fade_str       = fade_top;
                  ticker.top_fade_str_len   = len;
                  ticker.bottom_fade_str    = fade_bottom;
                  ticker.bottom_fade_str_len = len;
                  ticker.type_enum          = (enum gfx_animation_ticker_type)type;
                  ticker.fade_enabled       = fade != 0;
                  ticker.idx                = len * 37;
                  gfx_animation_line_ticker_smooth(&anim, &ticker);
                  ticker_calls++;
                  CHECK(memchr(dst, '\0', len) != NULL,
                        "smooth line ticker: destination is terminated");
                  if (fade)
                  {
                     CHECK(memchr(fade_top, '\0', len) != NULL
                           && memchr(fade_bottom, '\0', len) != NULL,
                           "smooth line ticker: fade strings are terminated");
                     CHECK(top_alpha >= 0 && top_alpha <= 1
                           && bottom_alpha >= 0 && bottom_alpha <= 1,
                           "smooth line ticker: fade alphas stay in range");
                  }
                  free(dst);
                  free(fade_top);
                  free(fade_bottom);
               }
   animation_test_wideglyph = NULL;
}

extern unsigned animation_test_font_generation;
extern unsigned animation_test_wrap_calls;
extern unsigned animation_test_allocations;
extern bool animation_test_fail_malloc;

static void test_line_layout_cache(void)
{
   gfx_animation_t anim;
   gfx_animation_ctx_line_ticker_t stepped;
   gfx_animation_ctx_line_ticker_smooth_t smooth;
   font_data_t font;
   char source[] = "one\ntwo\nthree\nfour";
   char out[64], top[64] = {0}, bottom[64] = {0}, *scratch = NULL;
   char cache_key[64], expected[64], long_source[PATH_MAX_LENGTH + 32];
   size_t capacity = 0;
   float y = 0, top_y = 0, bottom_y = 0, top_alpha = 0, bottom_alpha = 0;
   unsigned wraps, allocations, i;

   gfx_animation_deinit();
   memset(&anim, 0, sizeof(anim));
   memset(&stepped, 0, sizeof(stepped));
   stepped.str              = source;
   stepped.s                = out;
   stepped.len              = sizeof(out);
   stepped.line_len         = 16;
   stepped.max_lines        = 2;
   stepped.wrap_scratch     = &scratch;
   stepped.wrap_scratch_cap = &capacity;
   gfx_animation_line_ticker(&anim, &stepped);
   wraps       = animation_test_wrap_calls;
   allocations = animation_test_allocations;
   for (i = 0; i < 10000; i++)
   {
      stepped.idx = i;
      gfx_animation_line_ticker(&anim, &stepped);
   }
   CHECK(animation_test_wrap_calls == wraps
         && animation_test_allocations == allocations,
         "stepped layout: repeated frames neither wrap nor allocate");
   stepped.idx = 0;
   source[0] = 'O';
   gfx_animation_line_ticker(&anim, &stepped);
   CHECK(animation_test_wrap_calls == wraps + 1
         && strcmp(out, "One\ntwo") == 0,
         "stepped layout: in-place source changes invalidate cached text");
   wraps = animation_test_wrap_calls;
   stepped.line_len = 17;
   gfx_animation_line_ticker(&anim, &stepped);
   CHECK(animation_test_wrap_calls == wraps + 1,
         "stepped layout: width changes invalidate wrapping");
   stepped.str = "red\ngreen\nblue";
   wraps = animation_test_wrap_calls;
   animation_test_fail_malloc = true;
   gfx_animation_line_ticker(&anim, &stepped);
   CHECK(animation_test_wrap_calls == wraps + 1
         && strcmp(out, "red\ngreen") == 0 && !animation_test_fail_malloc,
         "stepped layout: allocation failure retains scratch output");
   for (i = 0; i < 100; i++)
   {
      snprintf(cache_key, sizeof(cache_key), "label-%u\nsecond\nthird", i);
      snprintf(expected, sizeof(expected), "label-%u\nsecond", i);
      stepped.str = cache_key;
      gfx_animation_line_ticker(&anim, &stepped);
      CHECK(strcmp(out, expected) == 0,
            "stepped layout: eviction and reused source buffers stay correct");
   }
   for (i = 0; i < sizeof(long_source) - 1; i++)
      long_source[i] = (i & 1) ? ' ' : 'a';
   long_source[sizeof(long_source) - 1] = '\0';
   stepped.str = long_source;
   wraps = animation_test_wrap_calls;
   gfx_animation_line_ticker(&anim, &stepped);
   CHECK(animation_test_wrap_calls == wraps + 1 && *out,
         "stepped layout: oversized source wraps only once on fallback");

   memset(&smooth, 0, sizeof(smooth));
   memset(&font, 0, sizeof(font));
   font.metrics.height          = 12;
   smooth.font                  = &font;
   smooth.font_scale            = 1.0f;
   smooth.field_dims            = VIDEO_SCALE_PACK(128, 24);
   smooth.src_str               = source;
   smooth.dst_str               = out;
   smooth.dst_str_len           = sizeof(out);
   smooth.y_offset              = &y;
   smooth.top_fade_str          = top;
   smooth.top_fade_str_len      = sizeof(top);
   smooth.top_fade_y_offset     = &top_y;
   smooth.top_fade_alpha        = &top_alpha;
   smooth.bottom_fade_str       = bottom;
   smooth.bottom_fade_str_len   = sizeof(bottom);
   smooth.bottom_fade_y_offset  = &bottom_y;
   smooth.bottom_fade_alpha     = &bottom_alpha;
   smooth.fade_enabled          = true;
   gfx_animation_line_ticker_smooth(&anim, &smooth);
   wraps       = animation_test_wrap_calls;
   allocations = animation_test_allocations;
   for (i = 0; i < 10000; i++)
   {
      smooth.idx = i;
      gfx_animation_line_ticker_smooth(&anim, &smooth);
   }
   CHECK(animation_test_wrap_calls == wraps
         && animation_test_allocations == allocations,
         "smooth layout: repeated frames neither wrap nor allocate");
   smooth.field_dims = VIDEO_SCALE_PACK(128, 36);
   gfx_animation_line_ticker_smooth(&anim, &smooth);
   CHECK(animation_test_wrap_calls == wraps,
         "smooth layout: field height does not change line breaks");
   animation_test_font_generation++;
   gfx_animation_line_ticker_smooth(&anim, &smooth);
   CHECK(animation_test_wrap_calls == ++wraps,
         "smooth layout: recycled font generations invalidate layout");
   smooth.font_scale = 2.0f;
   gfx_animation_line_ticker_smooth(&anim, &smooth);
   CHECK(animation_test_wrap_calls == ++wraps,
         "smooth layout: font scaling invalidates layout");
   animation_test_wideglyph = "\xE4\xB8\xAD";
   gfx_animation_line_ticker_smooth(&anim, &smooth);
   CHECK(animation_test_wrap_calls == ++wraps,
         "smooth layout: wideglyph wrapping invalidates layout");
   animation_test_wideglyph = NULL;
   printf("layout cache: 20000 warm frames, zero wraps and allocations\n");
   free(scratch);
   gfx_animation_deinit();
}

/* The smooth ticker's width cache, keyed by the string's bytes: a
 * label edited in place, one a prefix of another and two of a length
 * that differ only in their bytes each get their own widths, three
 * hundred labels round the cache come back right, and warm frames
 * allocate nothing. */

static unsigned cached_width(font_data_t *font, const char *str)
{
   gfx_animation_ctx_ticker_smooth_t ticker;
   char     dst[64];
   unsigned x_offset  = 0;
   unsigned dst_width = 0;
   memset(&ticker, 0, sizeof(ticker));
   ticker.dst_str       = dst;
   ticker.dst_str_len   = sizeof(dst);
   ticker.dst_str_width = &dst_width;
   ticker.x_offset      = &x_offset;
   ticker.src_str       = str;
   ticker.font          = font;
   ticker.field_width   = 4096;
   ticker.font_scale    = 1.0f;
   gfx_animation_ticker_smooth(&ticker);
   if (strcmp(dst, str))
      return 0;
   return dst_width;
}

static void test_width_cache(void)
{
   font_data_t font;
   char        label[32];
   char        many[300][24];
   unsigned    allocations;
   unsigned    i, pass;

   memset(&font, 0, sizeof(font));
   strcpy(label, "abcde");
   CHECK(cached_width(&font, label) == 40, "width cache: first label");
   strcpy(label, "ab\xe4\xb8\xad");
   CHECK(cached_width(&font, label) == 32,
         "width cache: same length, other bytes");
   strcpy(label, "ab\xe4\xb8\xadz");
   CHECK(cached_width(&font, label) == 40,
         "width cache: the last label and one more byte");
   strcpy(label, "ab");
   CHECK(cached_width(&font, label) == 16,
         "width cache: a prefix of the last label");
   CHECK(cached_width(&font, "abcde") == 40,
         "width cache: the first label again");

   for (i = 0; i < 300; i++)
   {
      unsigned n = 1 + i % 20, k;
      for (k = 0; k < n; k++)
         many[i][k] = (char)('a' + (i + k) % 26);
      many[i][n] = '\0';
   }
   for (pass = 0; pass < 2; pass++)
      for (i = 0; i < 300; i++)
         CHECK(cached_width(&font, many[i]) == 8 * (1 + i % 20),
               "width cache: labels round the cache");

   cached_width(&font, many[7]);
   allocations = animation_test_allocations;
   for (i = 0; i < 1000; i++)
      cached_width(&font, many[7]);
   CHECK(animation_test_allocations == allocations,
         "width cache: warm frames allocate nothing");
   gfx_animation_deinit();
}

static void test_clipped_label_boundary(void)
{
   gfx_animation_ctx_ticker_t ticker;
   char dst[64];
   memset(&ticker, 0, sizeof(ticker));
   ticker.s = dst;
   ticker.s_len = sizeof(dst);
   ticker.str = "ab\xe4\xb8\xad\xf0\x9f\x98\x80";
   ticker.len = 4;
   CHECK(!gfx_animation_ticker(&ticker), "fitting unselected label stays still");
   CHECK(strcmp(dst, ticker.str) == 0, "fitting UTF-8 label is copied whole");
   ticker.str = "ab\xe4\xb8\xad\xf0\x9f\x98\x80xyz";
   CHECK(!gfx_animation_ticker(&ticker), "clipped unselected label stays still");
   CHECK(strcmp(dst, "a...") == 0, "overflowing label has an ellipsis");
   ticker.len = 6;
   gfx_animation_ticker(&ticker);
   CHECK(strcmp(dst, "ab\xe4\xb8\xad...") == 0,
         "clipped UTF-8 label preserves complete glyphs");
}

int main(void)
{
   test_clipped_label_boundary();
   test_multibyte_does_not_overflow();
   test_zero_s_len_is_rejected();
   test_easing_types();
   test_subject_freed_midflight();
   test_deinit_while_running();
   test_ticker();
   test_ticker_smooth();
   test_line_ticker();
   test_line_ticker_smooth();
   test_widget_handover();
   test_line_layout_cache();
   test_width_cache();

   printf("pushes=%d callbacks=%d ticker calls=%d\n",
         pushes, cb_calls, ticker_calls);

   /* Asserted, not merely printed: a build where the animation list
    * silently refused every push, or where the tickers were never
    * entered, would otherwise report a clean sweep having swept
    * nothing. */
   CHECK(pushes > 0,       "no animation was ever pushed");
   CHECK(ticker_calls > 0, "no ticker was ever run");

   if (failures)
   {
      fprintf(stderr, "%d check(s) failed\n", failures);
      return 1;
   }
   puts("ALL OK");
   return 0;
}
