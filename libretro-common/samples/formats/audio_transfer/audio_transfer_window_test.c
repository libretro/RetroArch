/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (audio_transfer_window_test.c).
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

/* Regression test for windowed .weba playback.
 *
 * The WebM audio arms decode packets where the demuxer points at them
 * in the caller's buffer, which is what lets the mixer window a large
 * file: head resident forever, a window sliding with the read
 * position, everything else reserved but not committed.  What that
 * arrangement asks of the arm is that it never touch a byte the feeder
 * has not made resident - at open, during the walk, or across a loop -
 * and none of that is visible to a caller holding the whole file.
 *
 * So this drives the real thing: data_transfer's window mode, fed with
 * the arithmetic task_audio_mixer.c uses, and the decode compared
 * against the same file read whole.  Built with DT_STRICT the pages
 * outside the window fault on touch rather than reading as zeros,
 * which is what turns "did not notice" into "did not happen".  The
 * fault check below confirms that is actually in force, so a pass
 * means something: without it, a build where the strictness had been
 * compiled out would pass this test by reading rubbish quietly.
 *
 * Unlike the data_transfer tests this one takes a path rather than
 * generating its input.  Those exercise arbitrary bytes; this needs a
 * real coded stream, and a fixture small enough to embed would be
 * about a page long - which is to say it would have no window at all,
 * and would pass without testing anything.  Point it at a .weba above
 * the mixer's threshold (AMIX_WINDOW_THRESHOLD, 8 MB) holding Vorbis
 * or Opus.
 *
 *   ./audio_transfer_window_test music.weba
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <formats/audio.h>
#include <formats/data_transfer.h>
#include <formats/rwebm.h>

#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define HAVE_FORK_CHECK 1
#endif

/* task_audio_mixer.c's own figures, so the window this test feeds is
 * shaped the way the mixer's is. */
#define KEEP      (2 * 1024 * 1024)
#define LOOKAHEAD (2 * 1024 * 1024)
#define MARGIN    (1 * 1024 * 1024)

/* Frames per read.  The buffers below are sized in samples, so what a
 * read may ask for is this divided by the channel count - not this
 * flat, which is a stereo assumption and overflows the moment a file
 * has more than CHUNK_SAMPLES/CHUNK channels.  rvorbis decodes up to
 * sixteen, and a .weba can carry them. */
#define CHUNK         4096
#define CHUNK_SAMPLES (CHUNK * 16)

/* Frames that fit in a CHUNK_SAMPLES buffer at 'ch' channels. */
static size_t chunk_frames(unsigned ch)
{
   size_t f = ch ? (size_t)CHUNK_SAMPLES / ch : 0;
   return f > CHUNK ? (size_t)CHUNK : f;
}

static int bad;

static void ok(const char *what)   { printf("[ok]   %s\n", what); }
static void fail(const char *what) { printf("[FAIL] %s\n", what); bad = 1; }

/* Decode the whole file held in memory: the reference every windowed
 * run is measured against. */
static int16_t *decode_resident(const char *path, enum audio_type_enum ty,
      size_t *frames_out, unsigned *ch_out)
{
   FILE     *f = fopen(path, "rb");
   long      sz;
   uint8_t  *buf;
   void     *ctx;
   unsigned  ch = 0;
   int16_t  *pcm = NULL;
   size_t    cap = 0, frames = 0;

   if (!f)
      return NULL;
   fseek(f, 0, SEEK_END);
   sz = ftell(f);
   fseek(f, 0, SEEK_SET);
   if (sz <= 0 || !(buf = (uint8_t*)malloc((size_t)sz)))
   {
      fclose(f);
      return NULL;
   }
   if (fread(buf, 1, (size_t)sz, f) != (size_t)sz)
   {
      fclose(f);
      free(buf);
      return NULL;
   }
   fclose(f);

   if (!(ctx = audio_transfer_new(ty)))
   {
      free(buf);
      return NULL;
   }
   audio_transfer_set_buffer_ptr(ctx, ty, buf, (size_t)sz);
   if (   !audio_transfer_start(ctx, ty)
       || !audio_transfer_info(ctx, ty, &ch, NULL, NULL)
       || ch < 1)
   {
      audio_transfer_free(ctx, ty);
      free(buf);
      return NULL;
   }
   for (;;)
   {
      size_t got = 0;
      if (frames + CHUNK > cap)
      {
         size_t   ncap = cap ? cap * 2 : ((size_t)CHUNK * 64);
         int16_t *np   = (int16_t*)realloc(pcm,
               ncap * ch * sizeof(int16_t));
         if (!np)
            break;
         pcm = np;
         cap = ncap;
      }
      audio_transfer_read_s16(ctx, ty, pcm + frames * ch,
            chunk_frames(ch), &got);
      if (!got)
         break;
      frames += got;
   }
   audio_transfer_free(ctx, ty);
   free(buf);
   *frames_out = frames;
   *ch_out     = ch;
   return pcm;
}

/* Decode through a real window, feeding it the way the mixer's task
 * tick does.  'starve' skips the feed, which is the control: under
 * DT_STRICT that must not survive.  Returns frames decoded, or 0. */
static size_t decode_windowed(const char *path, enum audio_type_enum ty,
      int16_t *ref, size_t ref_frames, unsigned ch, int starve, int *mismatch)
{
   data_transfer_t *dt;
   const uint8_t   *base;
   size_t           flen = 0, avail, floor_ = 0, frames = 0;
   rwebm_t         *probe;
   void            *ctx;
   static int16_t   out[CHUNK_SAMPLES];

   *mismatch = 0;
   if (!(dt = data_transfer_open_window(path, KEEP)))
      return 0;
   base = data_transfer_window_base(dt, &flen);

   /* The feeder's first tick: parse the container header bounded by
    * the head, and grow the head to cover the media floor - below it
    * is header material the demuxer keeps borrowed pointers into. */
   probe = rwebm_open_memory_avail(base, flen,
         flen < KEEP ? flen : KEEP, NULL);
   if (!probe)
   {
      data_transfer_free(dt);
      return 0;
   }
   floor_ = rwebm_media_floor(probe);
   rwebm_close(probe);
   if (   floor_ + MARGIN > KEEP
       && !data_transfer_window_grow_keep(dt, floor_ + MARGIN))
   {
      data_transfer_free(dt);
      return 0;
   }
   avail = (floor_ + MARGIN > KEEP) ? floor_ + MARGIN : KEEP;
   if (avail > flen)
      avail = flen;

   if (!(ctx = audio_transfer_new(ty)))
   {
      data_transfer_free(dt);
      return 0;
   }
   audio_transfer_set_buffer_ptr(ctx, ty, (void*)base, flen);
   audio_transfer_set_avail(ctx, ty, avail);
   if (!audio_transfer_start(ctx, ty))
   {
      audio_transfer_free(ctx, ty);
      data_transfer_free(dt);
      return 0;
   }
   /* The starvation check is run without a reference and so without a
    * channel count; take it from the context rather than guessing. */
   if (!ch)
      audio_transfer_info(ctx, ty, &ch, NULL, NULL);
   if (!ch)
   {
      audio_transfer_free(ctx, ty);
      data_transfer_free(dt);
      return 0;
   }

   for (;;)
   {
      size_t got = 0, tell;
      audio_transfer_read_s16(ctx, ty, out, chunk_frames(ch), &got);
      if (got)
      {
         if (   ref
             && !*mismatch
             && (   frames + got > ref_frames
                 || memcmp(out, ref + frames * ch,
                       got * ch * sizeof(int16_t))))
            *mismatch = 1;
         frames += got;
      }
      tell = audio_transfer_buffer_tell(ctx, ty);
      if (!starve)
         data_transfer_window_feed(dt, tell, LOOKAHEAD, MARGIN);
      if (tell + LOOKAHEAD > avail)
      {
         avail = tell + LOOKAHEAD;
         if (avail > flen)
            avail = flen;
         audio_transfer_set_avail(ctx, ty, avail);
      }
      if (!got)
         break;
   }

   /* A loop seek runs on the audio thread with no feeder tick between,
    * so its landing has to be resident already.  Replay a little from
    * the head; under DT_STRICT a rewind into released pages faults. */
   if (audio_transfer_seek(ctx, ty, 0))
   {
      size_t looped = 0;
      while (looped < CHUNK * 16)
      {
         size_t got = 0, tell;
         audio_transfer_read_s16(ctx, ty, out, chunk_frames(ch), &got);
         if (!got)
            break;
         looped += got;
         tell = audio_transfer_buffer_tell(ctx, ty);
         if (!starve)
            data_transfer_window_feed(dt, tell, LOOKAHEAD, MARGIN);
      }
   }

   audio_transfer_free(ctx, ty);
   data_transfer_free(dt);
   return frames;
}

/* Confirm the window is load-bearing, by doing the run without feeding
 * it and requiring that not to survive.  A test whose instrument is
 * not connected reports success for the wrong reason, and this is the
 * only way to tell from inside.
 *
 * A file that fits inside the head has no window to starve and is
 * skipped: it would survive unfed for a reason that says nothing about
 * whether the pages are enforced. */
#ifdef HAVE_FORK_CHECK
static void strictness_check(const char *path, enum audio_type_enum ty,
      size_t flen)
{
   pid_t pid;
   int   st = 0;
   if (flen <= KEEP)
   {
      printf("[skip] window enforcement: %lu bytes fits the head, so"
             " there is no window here - use a file above the mixer's"
             " 8 MB threshold to exercise one\n", (unsigned long)flen);
      return;
   }
   pid = fork();
   if (pid < 0)
   {
      printf("[skip] window enforcement: fork failed\n");
      return;
   }
   if (pid == 0)
   {
      int m = 0;
      decode_windowed(path, ty, NULL, 0, 0, 1, &m);
      _exit(0);
   }
   if (waitpid(pid, &st, 0) < 0)
   {
      printf("[skip] window enforcement: waitpid failed\n");
      return;
   }
   if (WIFSIGNALED(st))
      ok("window enforcement: an unfed window faults, so a pass means"
            " the decode stayed inside it");
   else
      printf("[skip] window enforcement: an unfed window survived, so"
             " the pages are not enforced on this build and the pass"
             " below is weaker than it looks\n");
}
#endif

int main(int argc, char **argv)
{
   enum audio_type_enum ty;
   int16_t *ref;
   size_t   ref_frames = 0, win_frames;
   unsigned ch = 0;
   int      mismatch = 0;

   if (argc < 2)
   {
      printf("usage: %s <file.weba>\n"
             "  needs a WebM audio file (Vorbis or Opus) above the\n"
             "  mixer's 8 MB windowing threshold; see the note at the\n"
             "  top of this file for why one is not generated here.\n",
             argv[0]);
      return 0;
   }

   /* A .weba names no codec - the track inside does - so the type
    * comes from the container header rather than the extension, the
    * way audio_mixer_load_weba resolves it. */
   {
      FILE   *f = fopen(argv[1], "rb");
      uint8_t head[65536];
      size_t  n;
      if (!f)
      {
         printf("[skip] %s: cannot open\n", argv[1]);
         return 0;
      }
      n = fread(head, 1, sizeof(head), f);
      fclose(f);
      ty = audio_transfer_webm_audio_type(head, n);
      if (ty == AUDIO_TYPE_NONE)
         ty = audio_decode_get_type(argv[1]);
      if (ty == AUDIO_TYPE_NONE)
      {
         printf("[skip] %s: not a WebM audio type this build decodes\n",
               argv[1]);
         return 0;
      }
      printf("[info] %s: %s in WebM\n", argv[1],
            ty == AUDIO_TYPE_OPUS ? "Opus" : "Vorbis");
   }

   if (!data_transfer_reserve_supported())
   {
      printf("[skip] no address-space reservation on this build:"
             " the window would hold the whole file\n");
      return 0;
   }

   if (!(ref = decode_resident(argv[1], ty, &ref_frames, &ch))
         || !ref_frames)
   {
      fail("resident decode");
      printf("FAILED\n");
      return 1;
   }
   printf("[info] %s: %u ch, %lu frames resident\n", argv[1], ch,
         (unsigned long)ref_frames);

#ifdef HAVE_FORK_CHECK
   {
      FILE *f = fopen(argv[1], "rb");
      long  sz = 0;
      if (f)
      {
         fseek(f, 0, SEEK_END);
         sz = ftell(f);
         fclose(f);
      }
      strictness_check(argv[1], ty, sz > 0 ? (size_t)sz : 0);
   }
#endif

   win_frames = decode_windowed(argv[1], ty, ref, ref_frames, ch, 0,
         &mismatch);
   if (!win_frames)
      fail("windowed decode produced nothing");
   else if (win_frames != ref_frames)
      fail("windowed decode length differs from resident");
   else if (mismatch)
      fail("windowed decode differs from resident");
   else
      ok("windowed decode matches resident, and the loop replays");

   free(ref);
   printf("%s\n", bad ? "FAILED" : "PASS");
   return bad;
}
