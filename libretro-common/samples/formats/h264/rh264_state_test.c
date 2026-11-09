/* rh264's savestates: a decoder saved between access units and loaded
 * into a fresh one decodes on exactly as the saved one does.
 *
 * Every clip is decoded straight through once, recording each picture
 * shown and the call that showed it. Then a decoder takes it again,
 * saved before every access unit k, and each state is loaded into a new
 * decoder that takes the units after it. Both must show the same
 * pictures, to the byte, at the same calls:
 *
 *   S1  the loaded decoder continues as the straight decode (for the
 *       next WINDOW units: what it predicts from the state shows by then);
 *   S2  so does the saved one, through the whole clip (saving changes
 *       nothing);
 *   S3  a fresh decoder's state, loaded mid-stream, starts it over;
 *   S4  short, foreign or wrong-layout states are refused, and the
 *       decoder refusing one decodes the stream again from the top;
 *       and without the state the rest of the stream decodes otherwise,
 *       so a state left unread would show;
 *   S5  a decoder that loaded a state saves it again byte for byte,
 *       as run-ahead and rewind compare states.
 *
 * Every unit is tagged with its index, and a picture shown is compared
 * with its tag: a loaded decoder gives the pictures it holds the tags
 * they went in with.
 *
 * The clips cover B pictures and pyramids, several references,
 * long-term-free sliding windows over a long GOP, CABAC and CAVLC,
 * several slices, weighted prediction, macroblock pairs, 10 bits.
 * Each runs on one context, on four in rotation, and with a pool, where
 * a picture is still open after the call that took it and the calls
 * that show pictures depend on the pool's progress (so there only the
 * pictures and their order are compared).
 *
 * Needs ffmpeg with libx264 in PATH; a missing tool is a failure. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>

#include <formats/rmp4.h>
#include <formats/rh264.h>
#ifdef HAVE_THREADS
#include <rthreads/tpool.h>
#endif

static int fails;
static char dir[256];
static int g_contexts = 1;
static int g_threads  = 0;
static void *g_pool   = NULL;
/* With a pool the call that shows a picture depends on how far the
 * pool has got, as it does in a straight decode: there the pictures
 * and their order are compared, not the calls. */
static int g_timing   = 1;

static void check(const char *what, int ok)
{
   printf("  %-64s %s\n", what, ok ? "ok" : "FAIL");
   if (!ok)
      fails++;
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

/* The clip's video samples in decode order, and its avcC. */
typedef struct
{
   uint8_t  *file;
   rmp4_t   *mp4;
   const uint8_t *avcc;
   size_t    avcc_len;
   int       n;
   const uint8_t *data[1024];
   size_t    size[1024];
} clip_t;

static int clip_open(clip_t *c, const char *path)
{
   size_t len;
   int i, trk = -1;
   rmp4_packet pkt;
   memset(c, 0, sizeof(*c));
   if (!(c->file = slurp(path, &len)) || !(c->mp4 = rmp4_open_memory(c->file, len)))
      return -1;
   for (i = 0; i < rmp4_num_tracks(c->mp4); i++)
      if (rmp4_get_track(c->mp4, i)->type == RMP4_TRACK_VIDEO)
         trk = i;
   if (trk < 0)
      return -1;
   c->avcc     = rmp4_get_track(c->mp4, trk)->codec_private;
   c->avcc_len = rmp4_get_track(c->mp4, trk)->codec_private_size;
   while (c->n < 1024 && rmp4_read_packet(c->mp4, &pkt) == 1)
   {
      uint8_t *d;
      if (pkt.track != trk)
         continue;
      /* the packet's memory is the reader's until the next read */
      if (!(d = (uint8_t*)malloc(pkt.size ? pkt.size : 1)))
         return -1;
      memcpy(d, pkt.data, pkt.size);
      c->data[c->n] = d;
      c->size[c->n] = pkt.size;
      c->n++;
   }
   return c->n > 0 ? 0 : -1;
}

static void clip_close(clip_t *c)
{
   int i;
   for (i = 0; i < c->n; i++)
      free((void*)c->data[i]);
   if (c->mp4)
      rmp4_close(c->mp4);
   free(c->file);
}

/* What a decode shows: per picture, the call that showed it (unit
 * index, or n + drain count) and a hash of every visible sample. */
typedef struct
{
   int      n;
   int      call[2048];
   uint64_t hash[2048];
   uint64_t tag[2048];
} shown_t;

static void shown_add(shown_t *s, rh264_video *h, int call)
{
   uint64_t x = 1469598103934665603ull;
   int k, bps = rh264_video_bit_depth(h) > 8 ? 2 : 1;
   for (k = 0; k < 3; k++)
   {
      int st, w, hh, y, i;
      const uint8_t *p = rh264_video_plane(h, k, &st, &w, &hh);
      if (!p)
         continue;
      for (y = 0; y < hh; y++)
         for (i = 0; i < w * bps; i++)
         {
            x ^= p[(size_t)y * st * bps + i];
            x *= 1099511628211ull;
         }
   }
   if (s->n < 2048)
   {
      s->call[s->n] = call;
      s->hash[s->n] = x;
      s->tag[s->n]  = rh264_video_tag(h);
   }
   s->n++;
}

static rh264_video *dec_open(const clip_t *c)
{
   rh264_video *h = rh264_video_open();
   if (!h)
      return NULL;
   if (g_contexts > 1)
      rh264_video_set_contexts(h, g_contexts);
   if (g_threads > 1 && g_pool)
      rh264_video_set_thread_pool(h, g_pool, g_threads);
   if (rh264_video_set_extradata(h, c->avcc, c->avcc_len) != 0)
   {
      rh264_video_close(h);
      return NULL;
   }
   return h;
}

/* Units [from, to) into @h; at to == n, the drain too. -1 on error. */
static int dec_run(rh264_video *h, const clip_t *c, int from, int to, shown_t *s)
{
   int i, d = 0;
   for (i = from; i < to; i++)
   {
      int r;
      rh264_video_set_tag(h, (uint64_t)i);
      r = rh264_video_decode(h, c->data[i], c->size[i]);
      if (r < 0)
         return -1;
      if (r == 1)
         shown_add(s, h, i);
   }
   if (to == c->n)
      while (rh264_video_drain(h) == 0)
         shown_add(s, h, c->n + d++);
   return 0;
}

static int shown_same(const shown_t *a, const shown_t *b, int from)
{
   int i;
   if (a->n != b->n || a->n > 2048)
   {
      if (getenv("RH264_STATE_TRACE"))
         fprintf(stderr, "  %d pictures shown, %d expected\n", a->n, b->n);
      return 0;
   }
   for (i = from; i < a->n; i++)
      if (     (g_timing && a->call[i] != b->call[i])
            || a->hash[i] != b->hash[i] || a->tag[i] != b->tag[i])
      {
         if (getenv("RH264_STATE_TRACE"))
            fprintf(stderr, "  picture %d: call %d hash %016llx, expected call %d hash %016llx\n",
                  i, a->call[i], (unsigned long long)a->hash[i],
                  b->call[i], (unsigned long long)b->hash[i]);
         return 0;
      }
   return 1;
}

static shown_t g_ref, g_a, g_b;

/* Units a loaded decoder takes before it is compared: enough for every
 * picture it predicts from the state to have been shown. */
#define WINDOW 12

/* B's pictures against the straight decode's from picture @first on. */
static int shown_from(const shown_t *b, const shown_t *ref, int first)
{
   int i;
   if (first + b->n > ref->n || b->n > 2048)
      return 0;
   for (i = 0; i < b->n; i++)
      if ((g_timing && b->call[i] != ref->call[first + i])
            || b->hash[i] != ref->hash[first + i]
            || b->tag[i] != ref->tag[first + i])
      {
         if (getenv("RH264_STATE_TRACE"))
            fprintf(stderr, "  picture %d: call %d hash %016llx, expected call %d hash %016llx\n",
                  first + i, b->call[i], (unsigned long long)b->hash[i],
                  ref->call[first + i], (unsigned long long)ref->hash[first + i]);
         return 0;
      }
   return 1;
}

/* One decoder takes the stream, saved at every unit (S2: and shows what
 * the straight decode shows); each state goes into a new decoder, which
 * takes the next WINDOW units (S1) and saves the same bytes (S5).
 * Returns the units where a state was given. */
static int split_all(const clip_t *c, int *bad_loaded, int *bad_saved,
      int *bad_resaved)
{
   int k, saved = 0;
   rh264_video *a = dec_open(c);
   *bad_loaded = *bad_saved = *bad_resaved = 0;
   memset(&g_a, 0, sizeof(g_a));
   if (!a)
   {
      (*bad_saved)++;
      return 0;
   }
   for (k = 0; k < c->n; k++)
   {
      rh264_video *b;
      uint8_t *st, *again;
      size_t n;
      int to = k + WINDOW < c->n ? k + WINDOW : c->n;
      if (k && (n = rh264_video_state_size(a)) != 0
            && (b = dec_open(c)) != NULL)
      {
         st    = (uint8_t*)malloc(n);
         again = (uint8_t*)malloc(n);
         if (!st || !again || rh264_video_save_state(a, st, n) != n
               || rh264_video_load_state(b, st, n) != 0)
            (*bad_loaded)++;
         else
         {
            saved++;
            if (rh264_video_state_size(b) != n
                  || rh264_video_save_state(b, again, n) != n
                  || memcmp(again, st, n))
               (*bad_resaved)++;
            memset(&g_b, 0, sizeof(g_b));
            if (dec_run(b, c, k, to, &g_b) != 0 || !shown_from(&g_b, &g_ref, g_a.n))
            {
               (*bad_loaded)++;
               if (getenv("RH264_STATE_TRACE"))
                  fprintf(stderr, "split %d: loaded decoder differs\n", k);
            }
         }
         free(st);
         free(again);
         rh264_video_close(b);
      }
      if (dec_run(a, c, k, k + 1, &g_a) != 0)
         break;
   }
   if (k < c->n || !shown_same(&g_a, &g_ref, 0))
      (*bad_saved)++;
   rh264_video_close(a);
   return saved;
}

/* S3 and S4 at unit @k. */
static void refusals(const clip_t *c, int k)
{
   rh264_video *a = dec_open(c), *b = dec_open(c), *z = dec_open(c);
   uint8_t *st = NULL, *zs = NULL;
   size_t n = 0, zn = 0;
   int ok;
   memset(&g_a, 0, sizeof(g_a));
   if (!a || !b || !z || dec_run(a, c, 0, k, &g_a) != 0
         || !(n = rh264_video_state_size(a)) || !(st = (uint8_t*)malloc(n))
         || rh264_video_save_state(a, st, n) != n
         || !(zn = rh264_video_state_size(z)) || !(zs = (uint8_t*)malloc(zn))
         || rh264_video_save_state(z, zs, zn) != zn)
   {
      check("states to refuse", 0);
      goto done;
   }
   check("save refuses a short buffer", rh264_video_save_state(a, st, n - 1) == 0);

   /* S3: a fresh decoder's state over one mid-stream, then the stream
    * from the top */
   memset(&g_b, 0, sizeof(g_b));
   ok = rh264_video_load_state(b, st, n) == 0
         && rh264_video_load_state(b, zs, zn) == 0
         && dec_run(b, c, 0, c->n, &g_b) == 0 && shown_same(&g_b, &g_ref, 0);
   check("S3 a fresh decoder's state starts it over", ok);

   /* S4: refused, and the decoder decodes again from the top */
   ok = rh264_video_load_state(b, st, n - 1) == -1;
   st[0] ^= 1;
   ok = ok && rh264_video_load_state(b, st, n) == -1;
   st[0] ^= 1;
   st[8] ^= 0x40;   /* the frame layout */
   ok = ok && rh264_video_load_state(b, st, n) == -1;
   st[8] ^= 0x40;
   check("S4 short, foreign and wrong-layout states are refused", ok);
   memset(&g_b, 0, sizeof(g_b));
   check("S4 after a refusal the stream decodes from the top",
         dec_run(b, c, 0, c->n, &g_b) == 0 && shown_same(&g_b, &g_ref, 0));

   /* the test sees a missing state: a decoder taking the rest of the
    * stream without it does not continue as the straight decode */
   rh264_video_close(b);
   b = dec_open(c);
   g_b = g_a;
   ok = b && (dec_run(b, c, k, c->n, &g_b) != 0 || !shown_same(&g_b, &g_ref, 0));
   check("without the state the rest does not decode the same", ok);
done:
   free(st);
   free(zs);
   if (a) rh264_video_close(a);
   if (b) rh264_video_close(b);
   if (z) rh264_video_close(z);
}

static void state_case(const char *name, const char *src, int frames,
      const char *pix, const char *x264)
{
   char mp4[512], label[192];
   clip_t c;
   int mode;
   snprintf(mp4, sizeof(mp4), "%s/%s.mp4", dir, name);
   printf("%s (%s %s):\n", name, pix, x264);
   if (run("ffmpeg -v error -y -f lavfi -i \"%s\" -frames:v %d -c:v libx264 "
           "-pix_fmt %s %s '%s'", src, frames, pix, x264, mp4) != 0
         || clip_open(&c, mp4) != 0)
   {
      check("fixture", 0);
      clip_close(&c);
      return;
   }
   for (mode = 0; mode < 3; mode++)
   {
      rh264_video *h;
      int saved, bl, bs, br;
      const char *what = mode == 0 ? "one context" : mode == 1
            ? "4 contexts" : "a pool of 4";
      if (mode == 2 && !g_pool)
         continue;
      g_contexts = mode ? 4 : 1;
      g_threads  = mode == 2 ? 4 : 0;
      g_timing   = mode != 2;
      memset(&g_ref, 0, sizeof(g_ref));
      h = dec_open(&c);
      if (!h || dec_run(h, &c, 0, c.n, &g_ref) != 0 || g_ref.n != frames)
      {
         snprintf(label, sizeof(label), "%s: straight decode, %d of %d pictures",
               what, g_ref.n, frames);
         check(label, 0);
         if (h) rh264_video_close(h);
         continue;
      }
      rh264_video_close(h);
      saved = split_all(&c, &bl, &bs, &br);
      printf("      %s: states at %d of %d units\n", what, saved, c.n - 1);
      snprintf(label, sizeof(label), "S1 %s: loaded decoders continue exactly", what);
      check(label, bl == 0 && saved >= (c.n - 1) / 2);
      snprintf(label, sizeof(label), "S2 %s: saved decoders continue exactly", what);
      check(label, bs == 0);
      snprintf(label, sizeof(label), "S5 %s: a loaded decoder saves the same bytes", what);
      check(label, br == 0);
      if (mode == 0)
         refusals(&c, c.n / 2);
   }
   g_contexts = 1;
   g_threads  = 0;
   g_timing   = 1;
   clip_close(&c);
}

int main(void)
{
   snprintf(dir, sizeof(dir), "/tmp/rh264_state_%ld", (long)getpid());
   if (run("mkdir -p '%s'", dir) != 0)
      return 1;
#ifdef HAVE_THREADS
   g_pool = tpool_create_with_stack_size(3, 512 * 1024);
#endif
   state_case("bpyr_cabac", "testsrc2=s=176x144:r=15", 40, "yuv420p",
         "-crf 24 -preset medium -x264-params bframes=3:b-pyramid=normal:ref=4:keyint=300");
   state_case("ref_cavlc", "testsrc2=s=176x144:r=15", 40, "yuv420p",
         "-crf 24 -preset medium -x264-params cabac=0:bframes=2:ref=3:keyint=300");
   state_case("slices_wp", "testsrc2=s=176x144:r=15,fade=in:0:20", 30, "yuv420p",
         "-crf 22 -preset slow -x264-params slices=3:weightp=2:bframes=2:ref=2:8x8dct=1");
   state_case("mbaff", "testsrc2=s=176x144:r=15", 24, "yuv420p",
         "-crf 24 -preset medium -x264-params interlaced=1:bframes=2:ref=3");
   state_case("idr_cut", "testsrc2=s=176x144:r=15", 36, "yuv420p",
         "-crf 26 -preset fast -x264-params keyint=10:min-keyint=10:bframes=2");
   /* no B pictures: picture order type 2, from frame_num, which wraps
    * several times over the clip */
   state_case("poc2_wrap", "testsrc2=s=96x80:r=10", 72, "yuv420p",
         "-crf 26 -preset fast -x264-params bframes=0:ref=2:keyint=infinite");
   state_case("hb10", "testsrc2=s=96x80:r=10", 16, "yuv420p10le",
         "-crf 22 -preset medium -x264-params bframes=2:ref=3");
#ifdef HAVE_THREADS
   if (g_pool)
      tpool_destroy((tpool_t*)g_pool);
#endif
   run("rm -rf '%s'", dir);
   printf("rh264_state_test: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
         fails, fails == 1 ? "" : "s");
   return fails ? 1 : 0;
}
