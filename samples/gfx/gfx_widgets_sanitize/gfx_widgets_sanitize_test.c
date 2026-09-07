/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (gfx_widgets_sanitize_test.c).
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

/* Sanitizer sweep for gfx/gfx_widgets.c.
 *
 * Third of the sanitize samples, and unlike gfx_animation_sanitize
 * this one pins no defect: gfx_widgets.c came back clean on all four
 * sanitizers the first time it was run.  It is here as a tripwire
 * rather than a regression test, on the same reasoning as any other
 * clean sweep -- the value is in the next change, not this one.
 *
 * The focus is the message queue, because it has the same shape as
 * the gfx_animation ticker bug that prompted this: a caller-supplied
 * string, internal fixed buffers, and arithmetic that has to keep
 * glyphs and bytes apart.  So it gets multi-byte text, over-long
 * text, text with nothing to word-wrap on, an embedded newline, and
 * empty strings, across every message/title pairing.
 *
 * Two details that decide whether this sweeps anything at all.
 *
 * The font metrics in stubs_retroarch.c return a non-zero,
 * proportional width.  A zero-width font makes every "does this fit"
 * test in gfx_widgets.c succeed and the layout paths are never
 * entered -- the sweep would pass having exercised nothing.
 *
 * gfx_widgets_init() returns false without doing anything if
 * gfx_display_init_first_driver() fails, and the first run of this
 * did exactly that: pushes=0, frames=0, and every check below
 * vacuously satisfied.  Hence the assertions on those counters at the
 * bottom.
 *
 * Run it three ways:
 *
 *     make sweep
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../gfx/gfx_widgets.h"
#include "../../../gfx/gfx_display.h"
#include "../../../gfx/gfx_animation.h"

static int failures;
static int pushes;
static int iterations;

#define CHECK(cond, msg) \
   do { if (!(cond)) { fprintf(stderr, "FAIL: %s\n", (msg)); failures++; } } while (0)

static gfx_display_t   s_disp;
static gfx_animation_t s_anim;
static char            s_settings[1 << 18];

static bool widgets_up(void)
{
   memset(&s_disp, 0, sizeof(s_disp));
   memset(&s_anim, 0, sizeof(s_anim));
   memset(s_settings, 0, sizeof(s_settings));

   return gfx_widgets_init(&s_disp, &s_anim, s_settings,
         (uintptr_t)&s_disp, false, 1920, 1080, false,
         "/tmp/nonexistent-assets", NULL);
}

/* A frame's worth of work: iterate, then draw.
 *
 * gfx_widgets_frame() takes a video_frame_info_t and dereferences it,
 * its ->disp_userdata and that pointer's ->dispctx without checking
 * any of them.  The real caller in video_driver.c always supplies all
 * three, so this mirrors that rather than treating it as a defect. */
static void pump(int frames)
{
   int i;
   for (i = 0; i < frames; i++)
   {
      video_frame_info_t video_info;

      gfx_widgets_iterate(&s_disp, s_settings,
            1920, 1080, false, "/tmp/nonexistent-assets", NULL, false);

      memset(&video_info, 0, sizeof(video_info));
      video_info.disp_userdata    = &s_disp;
      video_info.widgets_userdata = dispwidget_get_ptr();
      video_info.width            = 1920;
      video_info.height           = 1080;

      gfx_widgets_frame(&video_info);
      iterations++;
   }
}

static void push(const char *msg, const char *title, unsigned prio)
{
   char title_buf[256];

   if (title)
   {
      strncpy(title_buf, title, sizeof(title_buf) - 1);
      title_buf[sizeof(title_buf) - 1] = '\0';
   }
   else
      title_buf[0] = '\0';

   gfx_widgets_msg_queue_push(NULL, msg, msg ? strlen(msg) : 0,
         1000, title ? title_buf : NULL,
         MESSAGE_QUEUE_ICON_DEFAULT,
         MESSAGE_QUEUE_CATEGORY_INFO,
         prio, false, false);
   pushes++;
}

/* ------------------------------------------------------------------
 * Inputs
 * ------------------------------------------------------------------ */
static const char *const msgs[] =
{
   "",
   "x",
   "a short message",
   "a message long enough to need wrapping across more than one line "
      "of a widget that is only so wide, and then some more after that",
   /* Multi-byte: three bytes per glyph. */
   "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe3\x81\xae\xe3\x83\xa1\xe3\x83"
      "\x83\xe3\x82\xbb\xe3\x83\xbc\xe3\x82\xb8",
   /* Multi-byte and long: the combination the ticker bug needed. */
   "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa"
      "\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac"
      "\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6"
      "\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e",
   /* No spaces at all: nothing for the wrapper to break on. */
   "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
   /* A newline in the middle. */
   "first line\nsecond line that is rather longer than the first one"
};

#define N_MSGS ((int)(sizeof(msgs) / sizeof(msgs[0])))

static void test_push_and_expire(void)
{
   int i;

   if (!widgets_up())
   {
      fputs("FAIL: gfx_widgets_init() refused\n", stderr);
      failures++;
      return;
   }

   for (i = 0; i < N_MSGS; i++)
   {
      push(msgs[i], NULL, 0);
      pump(3);
   }

   pump(30);
   gfx_widgets_deinit(false);
}

/* Titles go through their own composition path. */
static void test_titles(void)
{
   int i, j;

   if (!widgets_up())
      return;

   for (i = 0; i < N_MSGS; i++)
      for (j = 0; j < N_MSGS; j++)
      {
         push(msgs[i], msgs[j], (unsigned)(i + j));
         pump(2);
      }

   pump(20);
   gfx_widgets_deinit(false);
}

/* Overflow the queue: more messages than it can hold, so the eviction
 * and free paths run. */
static void test_queue_overflow(void)
{
   int i;

   if (!widgets_up())
      return;

   for (i = 0; i < 256; i++)
   {
      push(msgs[i % N_MSGS], (i & 1) ? msgs[(i + 3) % N_MSGS] : NULL,
            (unsigned)(i % 8));
      if ((i & 7) == 0)
         pump(1);
   }

   pump(60);
   gfx_widgets_deinit(false);
}

/* init/deinit repeatedly, with messages outstanding at teardown. */
static void test_init_deinit_cycles(void)
{
   int c, i;

   for (c = 0; c < 8; c++)
   {
      if (!widgets_up())
         return;

      for (i = 0; i < N_MSGS; i++)
         push(msgs[i], msgs[N_MSGS - 1 - i], 0);

      pump(2);
      /* Deliberately does not wait for them to expire. */
      gfx_widgets_deinit((c & 1) ? true : false);
   }
}

int main(void)
{
   test_push_and_expire();
   test_titles();
   test_queue_overflow();
   test_init_deinit_cycles();

   printf("pushes=%d frames=%d\n", pushes, iterations);

   /* Asserted, not printed: a build where init refused would otherwise
    * sweep nothing and report success. */
   CHECK(pushes > 0,     "no message was ever pushed");
   CHECK(iterations > 0, "no frame was ever iterated");

   if (failures)
   {
      fprintf(stderr, "%d check(s) failed\n", failures);
      return 1;
   }
   puts("ALL OK");
   return 0;
}
