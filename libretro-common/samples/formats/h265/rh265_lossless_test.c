/* rh265 against ffmpeg as the oracle, byte-exact.
 *
 * HEVC decoding is specified to the sample: for any conforming stream
 * the decoder's output is ffmpeg's to the byte, lossless or not. Each
 * case is built here with x265 and decoded four ways - on one thread;
 * with four picture contexts in rotation, one after the other; with
 * four pictures decoding concurrently on a pool; and concurrently
 * with every row's publication held back at random - and every way
 * must give ffmpeg's frames. The cases cover transquant bypass, the
 * wavefront's substreams, deblocking and SAO, B-frames with a
 * pyramid, and 10-bit samples.
 *
 * RH265_FILE=path decodes one file against its own ffmpeg reference
 * at one thread and, with RH265_FILE_THREADS=N, concurrently.
 *
 * Needs ffmpeg with libx265 in PATH; that is a hard requirement, a
 * missing ffmpeg is a failure. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>

#include <formats/rmp4.h>
#include <formats/rh265.h>
#ifdef HAVE_THREADS
#include <rthreads/tpool.h>
#endif

static int fails;
static char dir[256];

static void check(const char *what, int ok)
{
   printf("  %-64s %s\n", what, ok ? "ok" : "FAIL");
   if (!ok)
      fails++;
}

static uint8_t *slurp(const char *path, size_t *len)
{
   FILE *f = fopen(path, "rb");
   uint8_t *b;
   long n;
   if (!f)
      return NULL;
   fseek(f, 0, SEEK_END);
   n = ftell(f);
   fseek(f, 0, SEEK_SET);
   if (n <= 0 || !(b = (uint8_t*)malloc((size_t)n)))
   {
      fclose(f);
      return NULL;
   }
   if (fread(b, 1, (size_t)n, f) != (size_t)n)
   {
      fclose(f);
      free(b);
      return NULL;
   }
   fclose(f);
   *len = (size_t)n;
   return b;
}

static int run(const char *fmt, ...)
{
   char cmd[2048];
   va_list ap;
   va_start(ap, fmt);
   vsnprintf(cmd, sizeof(cmd), fmt, ap);
   va_end(ap);
   return system(cmd);
}

/* Decode an MP4 through rmp4 + rh265 and compare every output sample
 * with the reference planes: for luma row y (chroma row y/2) below
 * 'split' against ref_a, from it on against ref_b (ref_b == ref_a and
 * split == 0 for a single reference).  Returns the number of differing
 * samples, -1 on decode failure, and writes the frame count. */
/* Picture contexts in rotation for the decode under test. The
 * pictures still decode one after the other, so the output at four
 * must be the output at one to the byte: a difference means some piece
 * of picture state was left in the decoder rather than the context,
 * and would be shared between pictures decoding concurrently. Every
 * case is decoded at both. */
static int g_contexts = 1;
/* Pool threads for the decode under test; 0 is the calling thread. With
 * a pool the pictures decode concurrently, a picture reading from one
 * still decoding waits for the rows it needs, and the output must be
 * the single-thread output to the byte - a difference is a row read
 * before it was final, or published before it was. */
static int g_threads = 0;
static void *g_pool = NULL;

static long compare(const char *mp4, const uint8_t *ref_a, size_t alen,
      const uint8_t *ref_b, int split, int *frames_out)
{
   size_t n;
   uint8_t *b = slurp(mp4, &n);
   rmp4_t *m;
   rh265_video *h;
   rmp4_packet pkt;
   int trk = -1, i, frames = 0;
   long bad = 0;
   size_t off = 0;
   if (!b)
      return -1;
   if (!(m = rmp4_open_memory(b, n)))
   {
      free(b);
      return -1;
   }
   for (i = 0; i < rmp4_num_tracks(m); i++)
      if (rmp4_get_track(m, i)->type == RMP4_TRACK_VIDEO)
      {
         trk = i;
         break;
      }
   h = rh265_video_open();
   if (h && g_contexts > 1)
      rh265_video_set_contexts(h, g_contexts);
   if (h && g_threads > 1 && g_pool)
      rh265_video_set_thread_pool(h, g_pool, g_threads);
   if (trk < 0 || !h || rh265_video_set_extradata(h,
            rmp4_get_track(m, trk)->codec_private,
            rmp4_get_track(m, trk)->codec_private_size))
   {
      if (h) rh265_video_close(h);
      rmp4_close(m);
      free(b);
      return -1;
   }
   for (;;)
   {
      int r = rmp4_read_packet(m, &pkt), got;
      if (r != 1)
         break;
      if (pkt.track != trk)
         continue;
      got = rh265_video_decode(h, pkt.data, pkt.size);
      if (getenv("RH265_TRACE"))
         fprintf(stderr, "TRACE threads=%d sample=%zu got=%d\n", g_threads, pkt.size, got);
      if (got < 0)
      {
         bad = -1;
         goto done;
      }
      if (got == 1)
      {
         const uint8_t *p[3];
         int st[3], w[3], hh[3], k;
         for (k = 0; k < 3; k++)
            p[k] = rh265_video_plane(h, k, &st[k], &w[k], &hh[k]);
         /* above 8 bits the planes hold uint16_t samples and ffmpeg's
          * raw output is little-endian 16-bit: compare bytes */
         int bps = (rh265_video_bit_depth(h) > 8) ? 2 : 1;
         for (k = 0; k < 3; k++)
         {
            int y, x, sp = k ? split / 2 : split;
            for (y = 0; y < hh[k]; y++)
            {
               const uint8_t *ref = (y < sp) ? ref_a : ref_b;
               if (off + (size_t)w[k]*bps > alen)
               {
                  bad = -1;
                  goto done;
               }
               for (x = 0; x < w[k]*bps; x++)
                  if (p[k][(size_t)y*st[k]*bps + x] != ref[off + x])
                     bad++;
               off += (size_t)w[k]*bps;
            }
         }
         if (getenv("RH265_FRAMEDIFF"))
            fprintf(stderr, "FRAME %d diff so far %ld\n", frames, bad);
         if (getenv("RH265_DUMP") && frames == atoi(getenv("RH265_DUMP")))
         {
            /* luma of this frame, one line per macroblock row: the
             * count of samples differing from the reference */
            size_t o = 0; int fy; int y, x;
            for (fy = 0; fy < frames; fy++)
               for (k = 0; k < 3; k++) o += (size_t)w[k] * hh[k] * bps;
            for (y = 0; y < hh[0]; y += 16)
            {
               long d = 0; int yy;
               for (yy = y; yy < y + 16 && yy < hh[0]; yy++)
                  for (x = 0; x < w[0]*bps; x++)
                     if (p[0][(size_t)yy*st[0]*bps + x] != ref_a[o + (size_t)yy*w[0]*bps + x]) d++;
               fprintf(stderr, "ROW %2d diff %ld\n", y / 16, d);
               if (getenv("RH265_PIX") && d)
               {
                  int shown = 0;
                  for (yy = y; yy < y + 16 && yy < hh[0]; yy++)
                     for (x = 0; x < w[0]*bps && shown < 6; x++)
                        if (p[0][(size_t)yy*st[0]*bps + x] != ref_a[o + (size_t)yy*w[0]*bps + x])
                        {
                           fprintf(stderr, "   (%d,%d) ours %d ref %d\n", x, yy,
                                 p[0][(size_t)yy*st[0]*bps + x], ref_a[o + (size_t)yy*w[0]*bps + x]);
                           shown++;
                        }
               }
            }
         }
         frames++;
      }
   }
   while (rh265_video_drain(h) == 0)
   {
      const uint8_t *p[3];
      int st[3], w[3], hh[3], k;
      for (k = 0; k < 3; k++)
         p[k] = rh265_video_plane(h, k, &st[k], &w[k], &hh[k]);
      int bps = (rh265_video_bit_depth(h) > 8) ? 2 : 1;
      for (k = 0; k < 3; k++)
      {
         int y, x, sp = k ? split / 2 : split;
         for (y = 0; y < hh[k]; y++)
         {
            const uint8_t *ref = (y < sp) ? ref_a : ref_b;
            if (off + (size_t)w[k]*bps > alen)
            {
               bad = -1;
               goto done;
            }
            for (x = 0; x < w[k]*bps; x++)
               if (p[k][(size_t)y*st[k]*bps + x] != ref[off + x])
                  bad++;
            off += (size_t)w[k]*bps;
         }
      }
      if (getenv("RH265_FRAMEDIFF"))
         fprintf(stderr, "FRAME %d (drained) diff so far %ld\n", frames, bad);
      if (getenv("RH265_DUMP") && frames == atoi(getenv("RH265_DUMP")))
      {
         /* luma of this frame against reference frames f-1, f and f+1,
          * whole-frame counts: an order slip shows as a match elsewhere */
         int cand, y, x;
         for (cand = frames - 1; cand <= frames + 1; cand++)
         {
            size_t o = 0; long dd = 0; int fy;
            for (fy = 0; fy < cand; fy++)
               for (k = 0; k < 3; k++) o += (size_t)w[k] * hh[k] * bps;
            if (o + (size_t)w[0]*hh[0]*bps > alen) { fprintf(stderr, "ref %d: past end\n", cand); continue; }
            for (y = 0; y < hh[0]; y++)
               for (x = 0; x < w[0]*bps; x++)
                  if (p[0][(size_t)y*st[0]*bps + x] != ref_a[o + (size_t)y*w[0]*bps + x]) dd++;
            fprintf(stderr, "frame %d vs ref %d: %ld luma differ\n", frames, cand, dd);
         }
      }
      frames++;
   }
   if (off != alen)
      bad = bad < 0 ? bad : bad + 1;   /* short: not every reference frame came out */
done:
   *frames_out = frames;
   rh265_video_close(h);
   rmp4_close(m);
   free(b);
   return bad;
}

/* Encode 'src' with x265 ('pix' the chroma
 * format), decode it through rmp4 + rh265 and compare every sample
 * with ffmpeg's decode. */
static void oracle_case(const char *name, const char *src, int frames,
      const char *pix, const char *x265)
{
   char mp4[512], yuv[512], label[160];
   uint8_t *ref;
   size_t rlen;
   int nf = 0;
   long bad;
   snprintf(mp4, sizeof(mp4), "%s/%s.mp4", dir, name);
   snprintf(yuv, sizeof(yuv), "%s/%s.yuv", dir, name);
   snprintf(label, sizeof(label), "%s (%s %s) byte-exact vs ffmpeg",
         name, pix, x265);
   if (run("ffmpeg -v error -y -f lavfi -i \"%s\" -frames:v %d -c:v libx265 "
           "-pix_fmt %s %s -tag:v hvc1 '%s' && "
           "ffmpeg -v error -y -i '%s' -fps_mode passthrough -f rawvideo -pix_fmt %s '%s'",
           src, frames, pix, x265, mp4, mp4, pix, yuv) != 0
       || !(ref = slurp(yuv, &rlen)))
   {
      check(label, 0);
      return;
   }
   bad = compare(mp4, ref, rlen, ref, 0, &nf);
   printf("      %d frames, %ld differing samples%s\n", nf, bad < 0 ? 0 : bad,
         bad < 0 ? " (decode refused or failed)" : "");
   check(label, bad == 0 && nf == frames);
   g_contexts = 4;
   bad = compare(mp4, ref, rlen, ref, 0, &nf);
   g_contexts = 1;
   printf("      %d frames, %ld differing samples with 4 contexts in rotation\n",
         nf, bad < 0 ? 0 : bad);
   check("  same with 4 picture contexts in rotation", bad == 0 && nf == frames);
   if (g_pool)
   {
      g_threads = 4;
      bad = compare(mp4, ref, rlen, ref, 0, &nf);
      printf("      %d frames, %ld differing samples with 4 pictures decoding concurrently\n",
            nf, bad < 0 ? 0 : bad);
      check("  same with 4 pictures decoding concurrently", bad == 0 && nf == frames);
      /* and with every row's publication held back at random, so the
       * readers wait for their rows instead of finding them */
      rh265_video_set_publish_delay(200);
      bad = compare(mp4, ref, rlen, ref, 0, &nf);
      rh265_video_set_publish_delay(0);
      g_threads = 0;
      printf("      %d frames, %ld differing samples concurrently with rows held back\n",
            nf, bad < 0 ? 0 : bad);
      check("  same concurrently with every row's publication delayed", bad == 0 && nf == frames);
   }
   free(ref);
}

int main(void)
{
   /* RH265_FILE=path RH265_REF=path.yuv: decode one file against its
    * ffmpeg reference, at one thread and, with RH265_FILE_THREADS,
    * concurrently - for a file that misbehaves in the field. */
   if (getenv("RH265_FILE"))
   {
      /* The reference is ffmpeg's decode of the same stream, its
       * pictures and no more: without -fps_mode passthrough ffmpeg
       * duplicates pictures to hold the declared rate against the
       * timestamps, and the last picture of a file then compares
       * against a copy of the one before it. RH265_REF names a
       * reference already made that way. */
      size_t rlen = 0;
      const char *rf = getenv("RH265_REF");
      char made[512];
      uint8_t *ref;
      if (!rf)
      {
         snprintf(made, sizeof(made), "/tmp/rh265_file_ref_%ld.yuv", (long)getpid());
         if (run("ffmpeg -v error -y -i '%s' -fps_mode passthrough "
                 "-f rawvideo -pix_fmt yuv420p '%s'", getenv("RH265_FILE"), made))
            return 2;
         rf = made;
      }
      ref = slurp(rf, &rlen);
      int nf = 0;
      long bad;
      const char *te = getenv("RH265_FILE_THREADS");
      if (!ref)
         return 2;
      #ifdef HAVE_THREADS
      g_pool = tpool_create_with_stack_size(3, 512 * 1024);
      #endif
      bad = compare(getenv("RH265_FILE"), ref, rlen, ref, 0, &nf);
      printf("one thread: %d frames, %ld differing samples, %d reads short of their rows\n",
            nf, bad, rh265_video_ref_wait_misses());
      g_contexts = 4;
      bad = compare(getenv("RH265_FILE"), ref, rlen, ref, 0, &nf);
      g_contexts = 1;
      printf("4 contexts, one thread: %d frames, %ld differing samples\n", nf, bad);
      if (te && g_pool)
      {
         g_threads = atoi(te);
         bad = compare(getenv("RH265_FILE"), ref, rlen, ref, 0, &nf);
         printf("%d threads: %d frames, %ld differing samples\n", g_threads, nf, bad);
      }
      free(ref);
      if (rf == made)
         remove(made);
      return bad != 0;
   }
   if (system("ffmpeg -version >/dev/null 2>&1") != 0)
   {
      printf("rh265_lossless_test: ffmpeg is required (with libx265)\n");
      return 1;
   }
   snprintf(dir, sizeof(dir), "/tmp/rh265_lossless_%ld", (long)getpid());
   if (run("mkdir -p '%s'", dir) != 0)
      return 2;

   #ifdef HAVE_THREADS
   g_pool = tpool_create_with_stack_size(3, 512 * 1024);
   #endif
   if (!g_pool)
      printf("no thread pool: the concurrent decodes are skipped\n");
   printf("rh265 byte-exact vs ffmpeg - one thread, four contexts, four pictures concurrently, rows held back:\n");
   /* Lossless: transquant bypass, the residual the coded levels and
    * the loop filters leaving the CU alone; every sample the source's.
    * The last mixes bypass CUs with coded ones, so that the filters
    * run against a bypass side. */
   oracle_case("ll_420",        "mandelbrot=s=176x144:r=10", 3, "yuv420p",
         "-x265-params lossless=1:wpp=0:frame-threads=1");
   oracle_case("ll_420_wpp",    "mandelbrot=s=176x144:r=10", 3, "yuv420p",
         "-x265-params lossless=1:wpp=1:frame-threads=1");
   oracle_case("ll_420_10",     "mandelbrot=s=176x144:r=10", 3, "yuv420p10le",
         "-x265-params lossless=1:wpp=0:frame-threads=1");
   oracle_case("ll_420_b",      "mandelbrot=s=176x144:r=10", 6, "yuv420p",
         "-x265-params lossless=1:bframes=2:wpp=0:frame-threads=1");
   oracle_case("mixed_bypass",  "mandelbrot=s=176x144:r=10", 4, "yuv420p",
         "-x265-params cu-lossless=1:crf=22:sao=1:deblock=1:wpp=0:frame-threads=1");
   /* constrained intra prediction: intra CUs in P and B pictures take
    * no reference sample from an inter-coded neighbour */
   oracle_case("cip_pb",        "mandelbrot=s=176x144:r=10", 8, "yuv420p",
         "-x265-params constrained-intra=1:crf=22:sao=1:deblock=1:bframes=2:wpp=0:frame-threads=1");
   /* Lossy: deblocking and SAO on, B-frames and a B-pyramid, the
    * temporal predictor; the decode is specified to the sample and
    * must match ffmpeg's. */
   oracle_case("lossy_sao_b",   "mandelbrot=s=176x144:r=10", 8, "yuv420p",
         "-x265-params crf=22:sao=1:deblock=1:bframes=3:b-pyramid=1:wpp=0:frame-threads=1");
   oracle_case("lossy_sao_wpp", "mandelbrot=s=176x144:r=10", 8, "yuv420p",
         "-x265-params crf=22:sao=1:deblock=1:bframes=2:wpp=1:frame-threads=1");
   oracle_case("lossy_10_b",    "mandelbrot=s=176x144:r=10", 8, "yuv420p10le",
         "-x265-params crf=22:sao=1:deblock=1:bframes=2:wpp=0:frame-threads=1");
   /* Taller: five CTB rows and a partial sixth, P and B referencing
    * across them, the row hook and the reads that wait on it doing
    * real work; and a small-CTB stream, every row a quantisation
    * group boundary. */
   oracle_case("lossy_tall",    "mandelbrot=s=208x360:r=10", 12, "yuv420p",
         "-x265-params crf=24:sao=1:deblock=1:bframes=3:b-pyramid=1:wpp=0:frame-threads=1");
   oracle_case("lossy_ctu16",   "mandelbrot=s=176x144:r=10", 6, "yuv420p",
         "-x265-params crf=24:ctu=16:sao=1:deblock=1:bframes=2:wpp=0:frame-threads=1");
   #ifdef HAVE_THREADS
   if (g_pool)
      tpool_destroy((tpool_t*)g_pool);
   #endif
   printf("rh265_lossless_test: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
         fails, fails == 1 ? "" : "s");
   return fails ? 1 : 0;
}
