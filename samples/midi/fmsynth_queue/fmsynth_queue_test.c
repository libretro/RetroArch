/* The FM synth's MIDI event queue, between the threads that write
 * events and the audio thread that renders.
 *
 * - Events reach the renderer whole and in each writer's order, with
 *   one writer or two, while it renders as fast as it can.
 * - A ring that fills between blocks loses its oldest events, never
 *   the newest: a note-off sent last is applied and no voice is left
 *   on.
 * - The renderer stops at a slot a writer has claimed and not yet
 *   filled, and takes it, then the rest, in order once it is.
 * - Under writers that never pause, every event is either applied or
 *   counted as dropped - none is lost unseen, none applied twice.
 * - Nothing is logged from the audio thread; dropped events are
 *   reported once, at teardown.
 *
 * Channel volume (CC7) carries the order: each writer sends rising
 * values on its own channels, so a value going backwards is an event
 * out of order, and one on a channel or controller nobody wrote is a
 * torn event. The driver is included for its statics. */

#include <stdio.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>

#include <rthreads/rthreads.h>

#include "../../../midi/drivers/fmsynth_midi.c"

static int warnings;
void RARCH_WARN(const char *fmt, ...) { (void)fmt; warnings++; }
bool string_list_append(struct string_list *list, const char *elem,
      union string_list_elem_attr attr)
{ (void)list; (void)elem; (void)attr; return true; }

static unsigned failures;
#define CHECK(cond, ...) \
   do { if (!(cond)) { printf("FAIL line %d: ", __LINE__); \
        printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

static void watchdog(int sig)
{
   (void)sig;
   printf("FAIL: hung\n");
   fflush(stdout);
   _exit(1);
}

static void send(fmsynth_t *fm, uint8_t a, uint8_t b, uint8_t c)
{
   uint8_t     data[3];
   midi_event_t ev;
   data[0]       = a;
   data[1]       = b;
   data[2]       = c;
   ev.data       = data;
   ev.data_size  = 3;
   ev.delta_time = 0;
   midi_fmsynth.write(fm, &ev);
}

static unsigned active_voices(fmsynth_t *fm)
{
   unsigned i, n = 0;
   for (i = 0; i < FMSYNTH_MAX_VOICES; i++)
      if (fm->voices[i].active)
         n++;
   return n;
}

/* ---- writers and the renderer, at once ---- */

#define VALUES 127   /* CC7 values 1..127 per channel */

typedef struct
{
   fmsynth_t *fm;
   unsigned   first, count;   /* channels */
} writer_t;

static retro_atomic_int_t writers_done;

static void writer(void *data)
{
   writer_t *w = (writer_t*)data;
   unsigned v, c;
   for (v = 1; v <= VALUES; v++)
   {
      for (c = w->first; c < w->first + w->count; c++)
         send(w->fm, (uint8_t)(0xB0 | c), 7, (uint8_t)v);
      if (!(v & 15))
         usleep(50);
   }
   retro_atomic_fetch_add_int(&writers_done, 1);
}

static void case_concurrent(unsigned nwriters)
{
   static float out[2 * 64];
   writer_t  w[2];
   sthread_t *t[2];
   unsigned  i, c, blocks = 0, backwards = 0, torn = 0;
   uint8_t   last[FMSYNTH_NUM_CHANNELS];
   fmsynth_t *fm = (fmsynth_t*)midi_fmsynth.init(NULL, NULL);

   for (c = 0; c < FMSYNTH_NUM_CHANNELS; c++)
   {
      fm->volume[c] = 0;
      last[c]       = 0;
   }
   retro_atomic_store_release_int(&writers_done, 0);
   for (i = 0; i < nwriters; i++)
   {
      w[i].fm    = fm;
      w[i].count = FMSYNTH_NUM_CHANNELS / nwriters;
      w[i].first = i * w[i].count;
      t[i]       = sthread_create(writer, &w[i]);
   }

   /* The audio thread: render, look, render. */
   for (;;)
   {
      bool done = retro_atomic_load_acquire_int(&writers_done)
         == (int)nwriters;
      midi_fmsynth.render(fm, out, 64, 48000);
      blocks++;
      for (c = 0; c < FMSYNTH_NUM_CHANNELS; c++)
      {
         if (fm->volume[c] < last[c])
            backwards++;
         if (fm->volume[c] > VALUES)
            torn++;
         last[c] = fm->volume[c];
      }
      /* the controllers nobody wrote stay as they were */
      for (c = 0; c < FMSYNTH_NUM_CHANNELS; c++)
         if (fm->expression[c] != 127 || fm->pan[c] != 64)
            torn++;
      if (done)
         break;
   }
   for (i = 0; i < nwriters; i++)
      sthread_join(t[i]);
   midi_fmsynth.render(fm, out, 64, 48000);

   CHECK(!backwards, "%u volume steps went backwards (%u writer(s))",
         backwards, nwriters);
   CHECK(!torn, "%u torn events (%u writer(s))", torn, nwriters);
   for (c = 0; c < FMSYNTH_NUM_CHANNELS; c++)
      CHECK(fm->volume[c] == VALUES,
            "channel %u ended at %u, not the last value sent", c,
            fm->volume[c]);
   CHECK(!warnings, "the audio thread logged");
   printf("ok    %u writer(s), %u blocks rendered alongside\n",
         nwriters, blocks);
   midi_fmsynth.free(fm);
   warnings = 0;
}

/* ---- every event applied or counted dropped ---- */

#define STRESS_NOTES 20000

static void note_writer(void *data)
{
   writer_t *w = (writer_t*)data;
   unsigned i;
   for (i = 0; i < STRESS_NOTES; i++)
      send(w->fm, (uint8_t)(0x90 | w->first), (uint8_t)(36 + (i % 48)), 100);
   retro_atomic_fetch_add_int(&writers_done, 1);
}

static void case_stress(void)
{
   static float out[2 * 16];
   writer_t  w[2];
   sthread_t *t[2];
   unsigned  i, applied, dropped;
   fmsynth_t *fm = (fmsynth_t*)midi_fmsynth.init(NULL, NULL);

   retro_atomic_store_release_int(&writers_done, 0);
   for (i = 0; i < 2; i++)
   {
      w[i].fm    = fm;
      w[i].first = i ? 3 : 1;
      w[i].count = 1;
      t[i]       = sthread_create(note_writer, &w[i]);
   }
   while (retro_atomic_load_acquire_int(&writers_done) < 2)
      midi_fmsynth.render(fm, out, 16, 48000);
   for (i = 0; i < 2; i++)
      sthread_join(t[i]);
   midi_fmsynth.render(fm, out, 16, 48000);

   /* a note-on that is applied takes a voice and counts one */
   applied = fm->note_counter;
   dropped = (unsigned)retro_atomic_load_acquire_int(&fm->event_dropped);
   CHECK(applied + dropped == 2 * STRESS_NOTES,
         "%u applied + %u dropped of %u sent", applied, dropped,
         2 * STRESS_NOTES);
   CHECK(!warnings, "the audio thread logged");
   printf("ok    unthrottled writers: %u applied, %u dropped\n",
         applied, dropped);
   midi_fmsynth.free(fm);
   warnings = 0;
}

/* ---- a slot a writer has claimed and not finished ---- */

static void case_unfinished(void)
{
   static float out[2 * 16];
   fmsynth_t *fm   = (fmsynth_t*)midi_fmsynth.init(NULL, NULL);
   /* A writer takes its number and is held before it fills the slot. */
   unsigned  n     = (unsigned)retro_atomic_fetch_add_int(&fm->event_head, 1);
   unsigned  slot  = n % FMSYNTH_EVENT_QUEUE;
   uint32_t  words[FMSYNTH_EVENT_MAX / 4];
   uint8_t   msg[3] = { 0x92, 40, 100 };
   unsigned  i;

   retro_atomic_store_release_int(&fm->event_slot[slot].seq, (int)(n * 2u + 1u));
   /* Others write after it, and finish. */
   send(fm, 0x92, 41, 100);
   send(fm, 0x92, 42, 100);
   midi_fmsynth.render(fm, out, 16, 48000);
   CHECK(fm->note_counter == 0,
         "%u events were applied past one not yet written", fm->note_counter);

   /* It finishes: all three, in order. */
   memset(words, 0, sizeof(words));
   memcpy(words, msg, sizeof(msg));
   retro_atomic_store_relaxed_int(&fm->event_slot[slot].len, 3);
   for (i = 0; i < FMSYNTH_EVENT_MAX / 4; i++)
      retro_atomic_store_relaxed_int(&fm->event_slot[slot].data[i], (int)words[i]);
   retro_atomic_store_release_int(&fm->event_slot[slot].seq, (int)(n * 2u + 2u));
   midi_fmsynth.render(fm, out, 16, 48000);
   CHECK(fm->note_counter == 3, "%u of 3 applied once written",
         fm->note_counter);
   for (i = 0; i < FMSYNTH_MAX_VOICES; i++)
      if (fm->voices[i].active && fm->voices[i].note == 40)
         CHECK(fm->voices[i].age == 1,
               "the event written last but claimed first was not applied first");
   CHECK(!retro_atomic_load_acquire_int(&fm->event_dropped),
         "an event that was only late was counted as dropped");
   midi_fmsynth.free(fm);
   warnings = 0;
   printf("ok    the renderer waits for a slot still being written\n");
}

/* ---- a full ring keeps the newest ---- */

static void case_overflow(void)
{
   static float out[2 * 64];
   unsigned  i;
   fmsynth_t *fm = (fmsynth_t*)midi_fmsynth.init(NULL, NULL);

   /* note on, a ring and a half of controllers, note off - all before
    * the next block */
   send(fm, 0x90, 60, 100);
   for (i = 0; i < FMSYNTH_EVENT_QUEUE + FMSYNTH_EVENT_QUEUE / 2; i++)
      send(fm, 0xB1, 7, (uint8_t)(i & 0x7F));
   send(fm, 0x80, 60, 0);
   midi_fmsynth.render(fm, out, 64, 48000);

   CHECK(!active_voices(fm), "the note-off sent last was lost: %u voice(s) on",
         active_voices(fm));
   CHECK(fm->volume[1] == (uint8_t)((FMSYNTH_EVENT_QUEUE
               + FMSYNTH_EVENT_QUEUE / 2 - 1) & 0x7F),
         "the newest controller value was not applied");
   CHECK((unsigned)retro_atomic_load_acquire_int(&fm->event_dropped)
         == FMSYNTH_EVENT_QUEUE / 2 + 2,
         "%d dropped, not the %u oldest",
         retro_atomic_load_acquire_int(&fm->event_dropped),
         FMSYNTH_EVENT_QUEUE / 2 + 2);
   CHECK(!warnings, "rendering logged the drops");
   midi_fmsynth.free(fm);
   CHECK(warnings == 1, "teardown reported the drops %d time(s), not once",
         warnings);
   warnings = 0;
   printf("ok    a full ring keeps the newest, reported at teardown\n");
}

int main(void)
{
   signal(SIGALRM, watchdog);
   alarm(60);
   case_overflow();
   case_unfinished();
   case_concurrent(1);
   case_concurrent(2);
   case_stress();
   alarm(0);
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] fmsynth_queue_test\n");
   return 0;
}
