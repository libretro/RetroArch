/* preview_audio_decode_test: the REAL mixer decode chain over a
 * budget-fed window - audio_mixer -> audio_transfer -> rmp4 -> raac -
 * with the feeder modelling gfx_thumbnail_animate's fixed discipline.
 *
 * The sibling harness in this directory links the real gfx_thumbnail.c
 * and the real audio_mixer.c but stubs the audio DRIVER edge, so the
 * mixer's decode path never ran there - and the crash lived exactly
 * in what the stub modelled: the decoder outrunning the feeder.  For
 * the third time in this sample's history, the defect sat inside the
 * component a harness had replaced with a model of it.  This binary
 * closes that hole: it decodes AAC access units for real, out of a
 * window whose committed span is fed by the same budgeted calls the
 * menu tick makes, at budgets small enough that the decoder reaches
 * the frontier.
 *
 * What it pinned when written (all one family - the resident bound
 * that windowed audio decode rides on):
 *
 *  - audio_transfer_set_avail compiled to a no-op without HAVE_RWEBM,
 *    so an M4A voice's every bound - including the one that keeps
 *    rmp4's open off unpopulated pages - was silently dropped.  The
 *    -norwebm build of this test exists for that: SEGV in
 *    rmp4_open_memory_avail on the unfixed tree.
 *  - The trailing-moov path handed the open avail == the file length
 *    so the walk could reach the moov island, and left it there: no
 *    wall at all over the packets.  A starving feeder budget let the
 *    decoder cross the frontier and read access units off pages the
 *    window never committed - SIGSEGV inside raac_decode_frame, the
 *    crash this test was written from (mingw-w64, non-threaded audio,
 *    menu preview of a trailing-moov MP4).
 *  - rmp4_set_avail / rwebm_set_avail refused to lower ("bytes never
 *    un-arrive"), so the fix's post-open clamp - and every post-lap
 *    drop of the bound - was silently ignored.
 *  - An empty read at the wall returned AUDIO_PROCESS_END, which a
 *    looping mixer voice answers with a mid-file rewind, and a second
 *    stall with releasing the voice: fixing the wall without fixing
 *    that would have converted the crash into the preview audio dying
 *    a second in.
 *
 * Lanes (driven by the Makefile):
 *   island mode  - trailing-moov fixtures: moov island committed, the
 *                  open sees the full length, then the wall is clamped
 *                  to the head and follows the frontier, exactly as
 *                  the fixed gfx_thumbnail_anim_audio_begin does.
 *   budget       - bytes the feeder may commit per tick.  1024 makes
 *                  the decoder outrun the feeder within seconds (the
 *                  crash lane); production is 256 KiB.
 *
 * Passing means: no sanitizer fault, the voice is never FINISHED,
 * PCM actually comes out, and rewinds only happen at real laps -
 * consecutive REPEATED callbacks are at least LAP_GAP_FLOOR_SECS of
 * mixed output apart.  On the unfixed tree the starved lane dies in
 * raac_decode_frame; with only the wall fixed it fails the rewind-gap
 * and FINISHED checks instead. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <boolean.h>
#include <audio/audio_mixer.h>
#include <formats/data_transfer.h>

#define MIX_FRAMES          1024
#define MIX_RATE            48000
#define KEEP                (2u << 20)
#define LOOKAHEAD           (2u << 20)
#define MARGIN              (1u << 20)
#define TICKS_DEFAULT       20000
#define LAP_GAP_FLOOR_SECS  5.0

static int          repeats, finishes;
static long         frames_mixed;
static long         last_repeat_frames = -1;
static double       min_gap_secs       = 1e9;
static int          check_tell_drop;   /* set by a REPEATED callback  */
static size_t       tell_after_repeat  = 0;

static void stop_cb(audio_mixer_sound_t *s, unsigned reason)
{
   (void)s;
   if (reason == AUDIO_MIXER_SOUND_REPEATED)
   {
      if (last_repeat_frames >= 0)
      {
         double gap = (double)(frames_mixed - last_repeat_frames)
               / MIX_RATE;
         if (gap < min_gap_secs)
            min_gap_secs = gap;
      }
      last_repeat_frames = frames_mixed;
      check_tell_drop = 1;
      repeats++;
   }
   if (reason == AUDIO_MIXER_SOUND_FINISHED)
      finishes++;
}

int main(int argc, char **argv)
{
   const char *path;
   size_t      budget;
   int         island;
   data_transfer_t *dt;
   const uint8_t   *base;
   size_t           blen = 0;
   audio_mixer_sound_t *snd;
   audio_mixer_voice_t *v;
   float  out[MIX_FRAMES * 2];
   size_t wall = 0;
   double energy = 0.0;
   int    i, failed = 0;

   int ticks;
   if (argc < 3)
   {
      fprintf(stderr,
            "usage: %s file.mp4 budget [island] [ticks]\n", argv[0]);
      return 2;
   }
   path   = argv[1];
   budget = (size_t)strtoul(argv[2], NULL, 0);
   island = (argc > 3 && !strcmp(argv[3], "island"));
   ticks  = (argc > 4) ? atoi(argv[4]) : TICKS_DEFAULT;

   audio_mixer_init(MIX_RATE);

   if (!(dt = data_transfer_open_window(path, KEEP)))
   {
      fprintf(stderr, "FAIL: data_transfer_open_window(%s)\n", path);
      return 1;
   }
   base = data_transfer_window_base(dt, &blen);

   if (island)
   {
      /* The gfx path commits the trailing metadata island before the
       * stream opens; the last MiB covers these fixtures' moov. */
      size_t lo = (blen > (1u << 20)) ? blen - (1u << 20) : 0;
      if (!data_transfer_window_ensure(dt, lo, blen))
      {
         fprintf(stderr, "FAIL: island ensure\n");
         return 1;
      }
   }

   if (!(snd = audio_mixer_load_m4a((void*)base, blen)))
   {
      fprintf(stderr, "FAIL: audio_mixer_load_m4a\n");
      return 1;
   }
   /* The bound handed to the open: full length when a trailing moov
    * must be reachable, the head otherwise - gfx_thumbnail's
    * anim_audio_hi at add_stream, verbatim. */
   audio_mixer_sound_set_avail(snd,
         island ? blen : ((KEEP < blen) ? KEEP : blen));

   if (!(v = audio_mixer_play(snd, true, 1.0f, "sinc",
               RESAMPLER_QUALITY_DONTCARE, stop_cb)))
   {
      fprintf(stderr, "FAIL: audio_mixer_play\n");
      return 1;
   }

   /* The island open needed the full length; the packet path must not
    * keep it.  Clamp to the head, as the fixed gfx path does the
    * moment add_stream returns. */
   wall = (KEEP < blen) ? KEEP : blen;
   audio_mixer_voice_set_avail(v, wall);

   for (i = 0; i < ticks && !finishes; i++)
   {
      size_t tell, res = 0, hi;
      unsigned n;

      memset(out, 0, sizeof(out));
      audio_mixer_mix(out, MIX_FRAMES, 1.0f, false);
      frames_mixed += MIX_FRAMES;
      for (n = 0; n < MIX_FRAMES * 2; n++)
         energy += fabsf(out[n]);

      /* The fixed feeder discipline: budgeted feed straddling the
       * decoder's compressed tell, then the wall follows the resident
       * frontier - both directions. */
      tell = audio_mixer_voice_buffer_tell(v);
      if (check_tell_drop)
      {
         /* A loop just happened: the decoder is back at the head, and
          * the byte tell the feeder anchors on must say so.  A tell
          * frozen at the file end (rmp4_consumed reporting its
          * monotonic high-water across rmp4_rewind) parked the window
          * on the tail while the decoder re-walked the head into
          * decommitted pages: audio after the first lap degenerated
          * into a stutter-loop of whatever stayed resident. */
         if (tell > tell_after_repeat)
            tell_after_repeat = tell;
         check_tell_drop = 0;
      }
      if (!data_transfer_window_feed_budget(dt, tell, LOOKAHEAD, MARGIN,
               budget, &res))
      {
         fprintf(stderr, "FAIL: window feed at tick %d\n", i);
         failed = 1;
         break;
      }
      hi = tell + LOOKAHEAD;
      if (hi > blen)
         hi = blen;
      if (hi > res)
         hi = res;
      if (hi != wall)
      {
         wall = hi;
         audio_mixer_voice_set_avail(v, wall);
      }
   }

   if (finishes)
   {
      fprintf(stderr, "FAIL: voice FINISHED (%d) - a stall was taken "
            "for end of stream and a second one released the voice\n",
            finishes);
      failed = 1;
   }
   if (energy <= 0.0)
   {
      fprintf(stderr, "FAIL: no PCM produced in %d ticks\n", ticks);
      failed = 1;
   }
   if (repeats > 0 && tell_after_repeat > blen / 4)
   {
      fprintf(stderr, "FAIL: tell %zu right after a loop (buffer %zu) "
            "- the feeder was never told about the rewind\n",
            tell_after_repeat, blen);
      failed = 1;
   }
   if (repeats > 1 && min_gap_secs < LAP_GAP_FLOOR_SECS)
   {
      fprintf(stderr, "FAIL: rewinds %.2fs apart - mid-file rewinds, "
            "not laps (a wall stall taken for end of stream)\n",
            min_gap_secs);
      failed = 1;
   }

   printf("%s: ticks=%d repeats=%d min_gap=%.1fs energy=%.0f%s\n",
         path, i, repeats,
         (min_gap_secs > 1e8) ? 0.0 : min_gap_secs, energy,
         failed ? " FAIL" : " ok");

   audio_mixer_done();
   data_transfer_free(dt);
   return failed;
}

/* ==== frontend edges the linked sources want; nothing more ==== */

void config_userdata_free(void *u) { (void)u; }
int config_userdata_get_float(void *u, const char *k, float *v, float d)
{ (void)u; (void)k; if (v) *v = d; return 0; }
int config_userdata_get_int(void *u, const char *k, int *v, int d)
{ (void)u; (void)k; if (v) *v = d; return 0; }
int config_userdata_get_float_array(void *u, const char *k, float **v,
      unsigned *n) { (void)u; (void)k; (void)v; (void)n; return 0; }
int config_userdata_get_int_array(void *u, const char *k, int **v,
      unsigned *n) { (void)u; (void)k; (void)v; (void)n; return 0; }
int config_userdata_get_string(void *u, const char *k, char **v,
      const char *d) { (void)u; (void)k; (void)v; (void)d; return 0; }

#include <sys/stat.h>
bool path_is_directory(const char *p)
{ struct stat st; return p && stat(p, &st) == 0 && S_ISDIR(st.st_mode); }
bool path_mkdir(const char *d) { (void)d; return false; }
