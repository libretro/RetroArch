/* The resume mark between the main thread and the audio consumer.
 *
 * On a resume the main thread marks where the core's first audio
 * starts in the ring (pipe_fade_in_at) and the consumer arms the
 * resume ramp on reaching it. A pause cancels the mark and the next
 * resume publishes a new one - both on the main thread, and either
 * can land while the consumer is reading the old one. Held here, on
 * the shipping functions from audio/audio_driver.c:
 *
 *  - a mark the consumer takes is one mark: its position and its
 *    sequence are the same mark's;
 *  - a mark published while the consumer reads is not lost: after the
 *    consumer acts on what it took, the newer mark is still pending;
 *  - a cancelled mark asks for nothing, and a clear forgets them all.
 *
 * Deterministic, not a race: the consumer's loads of the mark go
 * through a hook here, which runs a pause and a resume on the main
 * thread's side after the consumer's first, second or third load. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>

static const volatile void *hook_word;  /* the mark */
static const volatile void *hook_pos;   /* its position */
static void (*hook_fn)(void);
static int hook_skip;

static int hooked_load_acquire_int(const retro_atomic_int_t *p)
{
   int v = retro_atomic_load_acquire_int((retro_atomic_int_t*)p);
   if (hook_fn && (const volatile void*)p == hook_word && hook_skip-- == 0)
   {
      void (*fn)(void) = hook_fn;
      hook_fn = NULL;
      fn();
   }
   return v;
}
static size_t hooked_load_acquire_size(const retro_atomic_size_t *p)
{
   size_t v = retro_atomic_load_acquire_size((retro_atomic_size_t*)p);
   /* The position is read between the two loads of the mark: a gap of
    * its own. */
   if (hook_fn && (const volatile void*)p == hook_pos && hook_skip-- == 0)
   {
      void (*fn)(void) = hook_fn;
      hook_fn = NULL;
      fn();
   }
   return v;
}
#undef  retro_atomic_load_acquire_int
#define retro_atomic_load_acquire_int(p)  hooked_load_acquire_int(p)
#undef  retro_atomic_load_acquire_size
#define retro_atomic_load_acquire_size(p) hooked_load_acquire_size(p)

#include "../../../audio/audio_driver.c"

static unsigned failures;
static audio_driver_state_t *st = &audio_driver_st;

#define X_AT 1000
#define Y_AT 2000

static unsigned x_seq, y_seq;

static unsigned mark_seq(void)
{
   return (unsigned)retro_atomic_load_relaxed_int(&st->pipe_fade_in_mark) >> 1;
}

static void pause_and_resume(void)
{
   audio_driver_pipe_fade_publish(st, false, 0);
   audio_driver_pipe_fade_publish(st, true, Y_AT);
   y_seq = mark_seq();
}

static void fail(const char *what)
{
   printf("   FAIL %s\n", what);
   failures++;
}

int main(void)
{
   size_t   at;
   unsigned seq;
   int      k;

   printf("pipeline resume mark:\n");
   memset(st, 0, sizeof(*st));
   audio_driver_pipe_marks_clear(st);

   if (audio_driver_pipe_fade_pending(st, &at, &seq))
      fail("a mark pending before any was published");

   audio_driver_pipe_fade_publish(st, true, X_AT);
   if (!audio_driver_pipe_fade_pending(st, &at, &seq) || at != X_AT)
      fail("a published mark is not pending at its position");
   else
      printf("   ok   a published mark is pending at its position\n");
   st->pipe_fade_in_seen = seq;
   if (audio_driver_pipe_fade_pending(st, &at, &seq))
      fail("a mark acted on is still pending");
   else
      printf("   ok   a mark acted on is done\n");

   audio_driver_pipe_fade_publish(st, true, X_AT);
   audio_driver_pipe_fade_publish(st, false, 0);
   if (audio_driver_pipe_fade_pending(st, &at, &seq))
      fail("a cancelled mark is pending");
   else
      printf("   ok   a cancelled mark asks for nothing\n");

   audio_driver_pipe_fade_publish(st, true, X_AT);
   audio_driver_pipe_marks_clear(st);
   if (audio_driver_pipe_fade_pending(st, &at, &seq))
      fail("a mark survived the clear");
   else
      printf("   ok   a clear forgets the mark\n");

   /* A pause and a resume landing in each gap of the consumer's read. */
   for (k = 0; k < 3; k++)
   {
      bool got;
      audio_driver_pipe_fade_publish(st, true, X_AT);
      x_seq     = mark_seq();
      hook_word = &st->pipe_fade_in_mark;
      hook_pos  = &st->pipe_fade_in_at;
      hook_fn   = pause_and_resume;
      hook_skip = k;
      got       = audio_driver_pipe_fade_pending(st, &at, &seq);
      if (hook_fn)
      {
         hook_fn = NULL;
         pause_and_resume();
      }
      if (got && !(at == X_AT && seq == x_seq)
              && !(at == Y_AT && seq == y_seq))
      {
         fail("the consumer took one mark's position with another's sequence");
         continue;
      }
      /* Act on what was taken, as the consumer does on passing it. */
      if (got)
         st->pipe_fade_in_seen = seq;
      if (!(got && at == Y_AT))
      {
         size_t   at2;
         unsigned seq2;
         if (!audio_driver_pipe_fade_pending(st, &at2, &seq2) || at2 != Y_AT)
         {
            fail("the resume published during the read was lost");
            continue;
         }
         st->pipe_fade_in_seen = seq2;
      }
      printf("   ok   a pause and resume after load %d: took %s, the resume "
            "is not lost\n", k + 1, got ? (at == Y_AT ? "the new mark"
               : "the old mark") : "none");
      if (audio_driver_pipe_fade_pending(st, &at, &seq))
         fail("a mark still pending after both were acted on");
   }

   if (failures)
   {
      printf("pipeline resume mark: %u failure(s)\n", failures);
      return 1;
   }
   printf("pipeline resume mark: every mark taken whole, none lost\n");
   return 0;
}
