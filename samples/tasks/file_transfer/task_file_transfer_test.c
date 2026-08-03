/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2026 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* Oracle for tasks/task_file_transfer.c.
 *
 * Pumps the real task_file_load_handler over the real data_transfer
 * spine, the real filestream/VFS and the real task_queue.  Only the
 * decode facades are stubbed (task_image_load_handler and, under
 * HAVE_AUDIOMIXER, the mixer), because everything this file is
 * accused of getting wrong lives between the task state machine and
 * the fill - not inside a decoder.
 *
 * What it is for.  The handler is a switch whose states advance at
 * most one step per tick, so its interesting behaviour is not "does
 * the buffer arrive" but how many ticks that takes, whether every
 * input reaches a terminal at all, and what a caller reading
 * task_get_progress() sees on the way.  A test that only checks the
 * bytes passes on all four bugs this was written to catch:
 *
 *   - a NULL path, and a parse callback that returns success without
 *     setting is_finished, both left the task running forever with no
 *     flag set - so the oracle runs to a tick cap and reports whether
 *     it hit it, rather than looping;
 *   - progress reached 100 only on the single-tick path;
 *   - the tick that opened the file read nothing, costing a frame on
 *     every load above the threshold - so ticks are counted, and the
 *     threshold is probed from both sides;
 *   - a file truncated underneath the fill must cancel, never parse a
 *     buffer whose tail was never written.
 *
 * The concurrent lane runs disjoint handles on many threads at once.
 * data_transfer synchronises nothing by design and the task layer
 * gives one handle to one task, so the lane is not looking for
 * contention on a shared handle - it is checking that the spine keeps
 * no hidden static state, which is exactly what a sanitizer build
 * can answer and inspection cannot.
 *
 * Build:  make                (SANITIZER=address,undefined, or thread)
 *         make sweep          (all three passes, both lanes)
 *                            (SANITIZER=thread with ./task_file_transfer_test conc)
 * Run:    ./task_file_transfer_test        single-threaded lanes
 *         ./task_file_transfer_test conc   concurrent instances
 *
 * Counters are thread-local: shared ones are the harness racing
 * itself, and a sanitizer will say so.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <boolean.h>
#include <queues/task_queue.h>
#include <formats/data_transfer.h>
#include <rthreads/rthreads.h>

#include "tasks/task_file_transfer.h"
#include "mp4_moov_eof_fixture.h"

void task_file_load_handler(retro_task_t *task);

/* ---- stubs for the image/mixer decode facades ---- */

static __thread int img_calls;
static int   img_finish_after = 1;   /* pretend to finish after N calls */
static int   img_fail;
static int   img_moov_at_eof;  /* still ready only on the whole file */
static int   oracle_bad;       /* any load-bearing lane failed */

bool task_image_load_handler(retro_task_t *task)
{
   nbio_handle_t *nbio = (nbio_handle_t*)task->state;
   img_calls++;
   if (img_fail)
      return false;
   if (img_moov_at_eof)
   {
      /* A non-faststart MP4: the moov index is the file's last box,
       * so the demuxer cannot open - and no still can decode - until
       * every byte is resident.  This is what most cameras write.
       * The 256 MiB prefix cap cancelled every such file above it:
       * the fill stopped at exactly the cap, settled capped(), and
       * the task delivered nothing - no thumbnail, no animation.
       * The lane below runs a file above the old cap and requires
       * completion, so a reintroduced ceiling fails it at once. */
      size_t d = 0, t = 0;
      if (nbio && nbio->xfer)
      {
         nbio_xfer_progress(nbio, &d, &t);
         if (t && d >= t)
         {
            nbio->is_finished = true;
            return false;
         }
      }
      if (nbio && nbio->status == NBIO_STATUS_TRANSFER_FINISHED)
         return false;
      return true;
   }
   /* Touch the buffer exactly as a still decoder would: read only
    * the bytes the fill has delivered.  Under a strict build a read
    * past avail() faults, which is the point. */
   if (nbio && nbio->xfer)
   {
      size_t len = 0, av;
      const uint8_t *p = nbio_xfer_ptr(nbio, &len);
      av = data_transfer_avail(nbio->xfer);
      if (p && av)
      {
         volatile uint32_t acc = 0;
         size_t i;
         for (i = 0; i < av; i += 4096)
            acc += p[i];
         acc += p[av - 1];
         (void)acc;
      }
   }
   if (nbio && nbio->status == NBIO_STATUS_TRANSFER_FINISHED)
      if (img_calls >= img_finish_after)
         return false;
   return true;
}

/* The mixer facade, stubbed like the image one.  Built in (the
 * Makefile defines HAVE_AUDIOMIXER) so the audio arm of the type
 * switch is exercised: M4A and OPUS were absent from it while both
 * mixer push paths set them, so those tasks finished without the
 * mixer handler ever running and the sound never played.  Nothing
 * catches that unless the test can see which arm a type takes. */
static __thread int mixer_calls;

bool task_audio_mixer_load_handler(retro_task_t *task)
{
   nbio_handle_t *nbio = (nbio_handle_t*)task->state;
   mixer_calls++;
   if (nbio && nbio->status == NBIO_STATUS_TRANSFER_FINISHED)
      return false;
   return true;
}

/* ---- helpers ---- */

static __thread int cb_calls;
static __thread size_t cb_last_len;
static int cb_ret = 0;
static int cb_sets_finished = 1;

static int test_cb(void *data, size_t len)
{
   nbio_handle_t *nbio = (nbio_handle_t*)data;
   cb_calls++;
   cb_last_len = len;
   if (cb_sets_finished)
      nbio->is_finished = true;
   return cb_ret;
}

static char *mkfile(const char *name, size_t len)
{
   char *path = (char*)malloc(256);
   FILE *f;
   size_t i;
   unsigned char *buf;
   snprintf(path, 256, "/tmp/dtq_%s", name);
   if (!(f = fopen(path, "wb")))
   {
      free(path);
      return NULL;
   }
   buf = (unsigned char*)malloc(65536);
   for (i = 0; i < 65536; i++)
      buf[i] = (unsigned char)(i * 7 + 3);
   while (len)
   {
      size_t n = len > 65536 ? 65536 : len;
      fwrite(buf, 1, n, f);
      len -= n;
   }
   free(buf);
   fclose(f);
   return path;
}

/* mimics task_image_load_free / task_audio_mixer_load_free */
static void nbio_cleanup(void *ud)
{
   nbio_handle_t *nbio = (nbio_handle_t*)ud;
   if (!nbio)
      return;
   free(nbio->path);
   free(nbio->data);
   nbio_xfer_close(nbio);
   free(nbio);
}

static int pump(const char *label, const char *path, enum nbio_type type,
      int max_ticks, void (*mid)(const char *path, int tick))
{
   retro_task_t  *task = (retro_task_t*)calloc(1, sizeof(*task));
   nbio_handle_t *nbio = (nbio_handle_t*)calloc(1, sizeof(*nbio));
   int ticks = 0;
   uint8_t flg;

   nbio->path = path ? strdup(path) : NULL;
   nbio->type = type;
   nbio->cb   = test_cb;
   nbio->status = NBIO_STATUS_INIT;
   task->state   = nbio;
   task->handler = task_file_load_handler;

   cb_calls = 0; img_calls = 0; mixer_calls = 0;

   do
   {
      task_file_load_handler(task);
      if (mid)
         mid(path, ticks);
      flg = task_get_flags(task);
      ticks++;
   } while (!(flg & RETRO_TASK_FLG_FINISHED) && ticks < max_ticks);

   /* the moov-at-EOF task lane is load-bearing: an incomplete or
    * cancelled 300M run is the capped regression come back */
   if (strstr(label, "moov-at-EOF"))
   {
      if (   (flg & RETRO_TASK_FLG_CANCELLED)
          || !nbio->xfer
          || !data_transfer_complete(nbio->xfer))
      {
         printf("  [FAIL] %s: cancelled=%d complete=%d - a prefix "
                "ceiling is back\n", label,
               (flg & RETRO_TASK_FLG_CANCELLED) > 0,
               nbio->xfer ? (int)data_transfer_complete(nbio->xfer) : 0);
         oracle_bad = 1;
      }
   }
   printf("  %-28s ticks=%-4d prog=%-4d cb=%d img=%d mix=%d "
          "finished=%d cancelled=%d avail=%zu\n",
         label, ticks, (int)task_get_progress(task), cb_calls,
         img_calls, mixer_calls,
         (flg & RETRO_TASK_FLG_FINISHED) > 0 ? 1 : 0,
         (flg & RETRO_TASK_FLG_CANCELLED) > 0 ? 1 : 0,
         nbio->xfer ? data_transfer_avail(nbio->xfer) : (size_t)0);

   free(task_get_error(task));
   nbio_cleanup(nbio);
   free(task);
   return ticks;
}

static void shrink_at_tick1(const char *path, int tick)
{
   if (tick == 1 && path)
      if (truncate(path, 300 * 1024) != 0)
         printf("  [warn] truncate failed; shrink lane is inert\n");
}

/* ---- concurrency: N handlers, N threads, disjoint handles ---- */

struct conc_arg { char *path; enum nbio_type type; int iters; };

static void conc_worker(void *ud)
{
   struct conc_arg *a = (struct conc_arg*)ud;
   int i;
   for (i = 0; i < a->iters; i++)
   {
      retro_task_t  *task = (retro_task_t*)calloc(1, sizeof(*task));
      nbio_handle_t *nbio = (nbio_handle_t*)calloc(1, sizeof(*nbio));
      int ticks = 0;
      nbio->path = strdup(a->path);
      nbio->type = a->type;
      nbio->cb   = test_cb;
      task->state = nbio;
      task->handler = task_file_load_handler;
      while (!(task_get_flags(task) & RETRO_TASK_FLG_FINISHED)
            && ticks++ < 4096)
         task_file_load_handler(task);
      free(task_get_error(task));
      nbio_cleanup(nbio);
      free(task);
   }
}

/* head + a 'free' box of pad_to total bytes + tail: a valid MP4 whose
 * moov is the last box, at whatever size the lane needs. */
static char *mk_moov_eof_mp4(const char *name, size_t pad_to)
{
   char *path = (char*)malloc(256);
   FILE *f;
   size_t body, i;
   unsigned char hdr[8], z[65536];
   if (!path)
      return NULL;
   snprintf(path, 256, "/tmp/dtq_%s.mp4", name);
   if (!(f = fopen(path, "wb")))
   {
      free(path);
      return NULL;
   }
   body   = pad_to - sizeof(mp4_head) - sizeof(mp4_tail) - 8;
   hdr[0] = (unsigned char)(((body + 8) >> 24) & 0xff);
   hdr[1] = (unsigned char)(((body + 8) >> 16) & 0xff);
   hdr[2] = (unsigned char)(((body + 8) >>  8) & 0xff);
   hdr[3] = (unsigned char)( (body + 8)        & 0xff);
   hdr[4] = 'f'; hdr[5] = 'r'; hdr[6] = 'e'; hdr[7] = 'e';
   memset(z, 0, sizeof(z));
   fwrite(mp4_head, 1, sizeof(mp4_head), f);
   fwrite(hdr, 1, 8, f);
   for (i = 0; i < body; i += sizeof(z))
      fwrite(z, 1, (body - i) < sizeof(z) ? (body - i) : sizeof(z), f);
   fwrite(mp4_tail, 1, sizeof(mp4_tail), f);
   fclose(f);
   return path;
}

/* The premise check, against the real rmp4 demuxer.  Returns nonzero
 * on failure; skipped when the image stack is not linked in. */
#if defined(HAVE_RMP4)
#include <formats/image.h>
static int real_rmp4_needs_whole_file(const char *path)
{
   FILE *f = fopen(path, "rb");
   long  n;
   uint8_t *b;
   void *s;
   int need = 0, dur = 0, frames = 0, bad = 0;
   if (!f)
      return 0;
   fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
   if (!(b = (uint8_t*)malloc((size_t)n)))
   {
      fclose(f);
      return 0;
   }
   if (fread(b, 1, (size_t)n, f) != (size_t)n)
   {
      fclose(f);
      free(b);
      return 0;
   }
   fclose(f);

   s = image_transfer_anim_stream_new_avail(b, (size_t)n, 1u << 20,
         IMAGE_TYPE_MP4, &need);
   if (s || !need)
   {
      printf("  [FAIL] rmp4 opened a moov-at-EOF file from a 1 MiB "
             "prefix (stream=%p need_more=%d): the cap's premise "
             "would hold and nothing forbids reintroducing it\n",
            s, need);
      if (s)
         image_transfer_anim_stream_free(s, IMAGE_TYPE_MP4);
      bad = 1;
   }
   else
   {
      s = image_transfer_anim_stream_new_avail(b, (size_t)n, (size_t)n,
            IMAGE_TYPE_MP4, &need);
      if (!s)
      {
         printf("  [FAIL] rmp4 cannot open the fixture even fully "
                "resident (need_more=%d)\n", need);
         bad = 1;
      }
      else
      {
         while (image_transfer_anim_stream_next(s, IMAGE_TYPE_MP4,
                  &dur) && frames < 16)
            frames++;
         if (frames < 2)
         {
            printf("  [FAIL] fixture decoded %d frames fully "
                   "resident\n", frames);
            bad = 1;
         }
         else
            printf("  real rmp4: prefix -> need_more, whole file -> "
                   "%d frames decoded (premise pinned)\n", frames);
         image_transfer_anim_stream_free(s, IMAGE_TYPE_MP4);
      }
   }
   free(b);
   return bad;
}
#else
static int real_rmp4_needs_whole_file(const char *path)
{
   (void)path;
   printf("  [skip] real-demuxer lane: built without HAVE_RMP4\n");
   return 0;
}
#endif

int main(int argc, char **argv)
{
   char *small, *thresh, *big, *huge, *tiny;
   int  conc = (argc > 1 && !strcmp(argv[1], "conc"));

   task_queue_init(true, NULL);

   tiny   = mkfile("tiny",    64);
   small  = mkfile("small",   512 * 1024);
   thresh = mkfile("thresh",  1024 * 1024);
   big    = mkfile("big",     5 * 1024 * 1024);
   huge   = mkfile("huge",    48 * 1024 * 1024);

   if (conc)
   {
      /* concurrent instances: disjoint handles, shared spine code */
      sthread_t *th[32];
      struct conc_arg a[32];
      int i;
      char *paths[4]; paths[0]=small; paths[1]=thresh; paths[2]=big; paths[3]=tiny;
      for (i = 0; i < 32; i++)
      {
         a[i].path  = paths[i & 3];
         a[i].type  = (i & 1) ? NBIO_TYPE_PNG : NBIO_TYPE_WEBM;
         a[i].iters = 6;
         th[i] = sthread_create(conc_worker, &a[i]);
      }
      for (i = 0; i < 32; i++)
         sthread_join(th[i]);
      printf("  concurrent instances: 32 threads x 6 loads done\n");
   }
   else
   {
      puts("-- prefix spine through the real handler --");
      pump("tiny 64B / NONE",        tiny,   NBIO_TYPE_NONE, 64, NULL);
      pump("small 512K / PNG",       small,  NBIO_TYPE_PNG,  64, NULL);
      pump("at-threshold 1M / PNG",  thresh, NBIO_TYPE_PNG,  64, NULL);
      pump("big 5M / PNG (time)",    big,    NBIO_TYPE_PNG,  4096, NULL);
      pump("big 5M / WEBM (bytes)",  big,    NBIO_TYPE_WEBM, 4096, NULL);
      pump("huge 48M / PNG",         huge,   NBIO_TYPE_PNG,  65536, NULL);
      pump("huge 48M / WEBM",        huge,   NBIO_TYPE_WEBM, 65536, NULL);
      puts("-- non-faststart video: index at EOF --");
      /* Two lanes against the same regression, from opposite ends.
       *
       * The REAL-DEMUXER lane pins the premise the 256 MiB cap got
       * wrong.  It writes a genuine h264 MP4 - real ftyp/mdat/moov,
       * real coded frames - with a ~300 MiB free box pushing moov to
       * EOF, exactly the layout cameras produce, and asks the real
       * rmp4 (via image_transfer) to open it avail-aware at a 1 MiB
       * prefix.  rmp4 must answer need_more, because the index is
       * simply not resident yet; with the whole file resident it
       * must open and decode real frames.  Anyone tempted to
       * reintroduce a prefix ceiling on the argument that "a still
       * only needs a few per cent" is refuted by this lane directly:
       * here is a well-formed file whose still needs 100%.
       *
       * The TASK lane pins the mechanism: a still that becomes ready
       * only at EOF, over a file larger than the old cap, must
       * finish uncancelled with every byte resident.  On the capped
       * tree it stopped at exactly 268435456 bytes and cancelled -
       * no thumbnail, no animation, which is how the regression
       * shipped. */
      {
         char *late = mk_moov_eof_mp4("moov_eof", 300u << 20);
         if (late)
         {
            oracle_bad |= real_rmp4_needs_whole_file(late);
            img_moov_at_eof = 1;
            pump("300M MP4 moov-at-EOF",  late, NBIO_TYPE_MP4,
                  400000, NULL);
            img_moov_at_eof = 0;
            remove(late); free(late);
         }
      }
      puts("-- audio types must reach the mixer arm --");
      pump("MP3",                    small,  NBIO_TYPE_MP3,  64, NULL);
      pump("FLAC",                   small,  NBIO_TYPE_FLAC, 64, NULL);
      pump("OGG",                    small,  NBIO_TYPE_OGG,  64, NULL);
      pump("MOD",                    small,  NBIO_TYPE_MOD,  64, NULL);
      pump("WAV",                    small,  NBIO_TYPE_WAV,  64, NULL);
      pump("M4A",                    small,  NBIO_TYPE_M4A,  64, NULL);
      pump("OPUS",                   small,  NBIO_TYPE_OPUS, 64, NULL);
      puts("   (mix=0 on any row above means that type never reaches "
           "the mixer)");
      puts("-- failure paths --");
      pump("missing path",           "/tmp/dtq_nope", NBIO_TYPE_PNG, 64, NULL);
      pump("NULL path",              NULL,   NBIO_TYPE_PNG,  64, NULL);
      {
         char *z = mkfile("zero", 0);
         pump("zero-length file",    z,      NBIO_TYPE_PNG,  64, NULL);
         remove(z); free(z);
      }
      pump("shrink under fill",      big,    NBIO_TYPE_WEBM, 4096,
            shrink_at_tick1);
      cb_ret = -1;
      pump("parse cb rejects",       small,  NBIO_TYPE_NONE, 64, NULL);
      cb_ret = 0;
      img_fail = 1;
      pump("image handler ends",     small,  NBIO_TYPE_PNG,  64, NULL);
      img_fail = 0;
      cb_sets_finished = 0;
      pump("cb leaves is_finished",  small,  NBIO_TYPE_NONE, 64, NULL);
      cb_sets_finished = 1;
   }

   remove(tiny); remove(small); remove(thresh); remove(big); remove(huge);
   free(tiny); free(small); free(thresh); free(big); free(huge);
   task_queue_deinit();
   if (oracle_bad)
      puts("FAILED");
   return oracle_bad;
}
