/* The AVFoundation camera's authorization and session state, asked and
 * answered without waiting: a start before the answer, after it, a
 * refusal, a failed setup, an answer for a camera already freed, and
 * a start racing the answer on two threads. */
#include <stdio.h>
#include <string.h>
#include <rthreads/rthreads.h>
#include "camera/drivers/avfoundation_state.h"

static unsigned failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); failures++; } } while (0)

static avf_state_t race;
static retro_atomic_int_t go, started;
static int race_gen;

static void race_start(void *data)
{
   (void)data;
   while (!retro_atomic_load_acquire_int(&go)) { }
   if (avf_state_start(&race) == AVF_START_NOW)
      retro_atomic_fetch_add_int(&started, 1);
}

static void race_answer(void *data)
{
   (void)data;
   while (!retro_atomic_load_acquire_int(&go)) { }
   if (avf_state_current(&race, race_gen) && avf_state_settle(&race, true))
      retro_atomic_fetch_add_int(&started, 1);
}

int main(void)
{
   avf_state_t s;
   int gen, old;
   unsigned i;
   memset(&s, 0, sizeof(s));

   /* Started before access is answered: started by the answer */
   gen = avf_state_begin(&s);
   CHECK(avf_state_phase(&s) == AVF_PENDING);
   CHECK(avf_state_start(&s) == AVF_START_LATER);
   CHECK(avf_state_current(&s, gen));
   CHECK(avf_state_settle(&s, true));
   CHECK(avf_state_phase(&s) == AVF_READY);
   CHECK(avf_state_start(&s) == AVF_START_NOW);
   avf_state_end(&s);
   CHECK(!avf_state_current(&s, gen));

   /* Answered first: the answer starts nothing, the start does */
   gen = avf_state_begin(&s);
   CHECK(!avf_state_settle(&s, true));
   CHECK(avf_state_start(&s) == AVF_START_NOW);
   avf_state_end(&s);

   /* Stopped before the answer: nothing starts */
   gen = avf_state_begin(&s);
   CHECK(avf_state_start(&s) == AVF_START_LATER);
   avf_state_stop(&s);
   CHECK(!avf_state_settle(&s, true));
   avf_state_end(&s);

   /* Refused, or no session: no start, before or after */
   gen = avf_state_begin(&s);
   CHECK(avf_state_start(&s) == AVF_START_LATER);
   CHECK(!avf_state_settle(&s, false));
   CHECK(avf_state_phase(&s) == AVF_FAILED);
   CHECK(avf_state_start(&s) == AVF_START_FAIL);
   CHECK(avf_state_phase(&s) == AVF_FAILED);
   avf_state_end(&s);

   /* An answer for a camera freed since, and the next camera's */
   old = avf_state_begin(&s);
   avf_state_end(&s);
   gen = avf_state_begin(&s);
   CHECK(!avf_state_current(&s, old) && avf_state_current(&s, gen));
   CHECK(avf_state_phase(&s) == AVF_PENDING);
   CHECK(avf_state_start(&s) == AVF_START_LATER);
   avf_state_end(&s);

   /* A start and the answer at once: the session starts, whichever
    * comes first */
   for (i = 0; i < 2000; i++)
   {
      sthread_t *a, *b;
      memset(&race, 0, sizeof(race));
      retro_atomic_int_init(&go, 0);
      retro_atomic_int_init(&started, 0);
      race_gen = avf_state_begin(&race);
      a = sthread_create(race_start, NULL);
      b = sthread_create(race_answer, NULL);
      CHECK(a && b);
      retro_atomic_store_release_int(&go, 1);
      if (a) sthread_join(a);
      if (b) sthread_join(b);
      CHECK(retro_atomic_load_acquire_int(&started) >= 1);
   }

   printf("AVFoundation camera state: %u failures\n", failures);
   return failures ? 1 : 0;
}
