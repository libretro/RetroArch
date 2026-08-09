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
 * gfx_animation.c reaches for only sixteen
 * symbols outside itself, so it stands up almost bare.
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
 * The tickers: both forms slice UTF-8 by glyph and write into a
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
         gfx_animation_update(i * 1000, false, 60.0f, 1920, 1080);

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
      gfx_animation_update((uint64_t)i * 100, false, 60.0f, 1920, 1080);

      /* Kill first, then free -- the order the caller is obliged to
       * use.  Getting it wrong is the use-after-free this is looking
       * for; getting it right must leave nothing behind either. */
      gfx_animation_kill_by_tag(&tag);
      free(subject);

      /* Anything still referencing it would be caught here. */
      gfx_animation_update((uint64_t)i * 100 + 50, false, 60.0f, 1920, 1080);
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

   gfx_animation_update(1, false, 60.0f, 1920, 1080);
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

int main(void)
{
   test_multibyte_does_not_overflow();
   test_zero_s_len_is_rejected();
   test_easing_types();
   test_subject_freed_midflight();
   test_deinit_while_running();
   test_ticker();
   test_ticker_smooth();

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
