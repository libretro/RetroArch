/* Windowed playback of the frame-indexed animation types (animated
 * WEBP, APNG) through the shared preview session, gfx/gfx_anim_preview.c
 * - the same object the raster menu's thumbnail and the desktop
 * companions' preview panes drive.
 *
 * What was wrong, and what each check pins:
 *
 *   W1  the stream opens from a prefix (progressive open).  Animated
 *       WEBP had none: anim_stream_new_avail returned NULL with
 *       need_more clear, and the session's fallback read the WHOLE
 *       file into the reservation.
 *   W2  the session is windowed AND its resident growth stays inside
 *       the window budget, not the file length.  With the fallback the
 *       flag stayed set while the whole file sat resident, so admission
 *       charged one window for a 60 MB file.
 *   W3  every frame plays, byte-exact against a whole-buffer decode,
 *       across the feeder's window.  APNG had a progressive open but
 *       no byte cursor (media_floor/consumed returned 0), so the feeder
 *       never advanced past the 12 MB primed at open: an APNG longer
 *       than that looped early, forever.
 *   W4  a second lap after rewind reproduces the first, i.e. the
 *       stream honours a LOWERED bound (rpng's set_avail refused to
 *       fall; a windowed caller's pages do un-arrive).
 *   W5  a frame larger than the feeder's lookahead still plays: the
 *       stream names its span (next_span) and the feeder covers it.
 *       Without that the frame never fits below the wall and the
 *       animation sits there for good.
 *   S1  a still WEBP is refused from its first chunk, without the
 *       whole-file read the fallback used to make.
 *   A1  a repeat ask for the order already being emitted is honoured.
 *       rpng_apng answered false to any ask after the first frame,
 *       even for the order it was already emitting; gfx_thumbnail's
 *       worker asks per frame and swizzles on false, so every APNG
 *       frame but the first came out with R and B swapped in the
 *       raster menu, at the cost of a full-canvas pass per frame.
 *   A2  a genuine switch mid-animation is honoured too (canvas
 *       converted once), byte-exact against a decode in that order.
 *
 * Reference frames come from the whole-buffer stream over a malloc'd
 * copy of the file, so the windowed path is compared against the
 * decoder itself, not against a golden CRC file. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

#include <formats/image.h>
#include <formats/data_transfer.h>
#include "../../../gfx/gfx_anim_preview.h"

static int fails;

static void check(const char *what, int ok)
{
   printf("      %-58s %s\n", what, ok ? "ok" : "FAIL");
   if (!ok)
      fails++;
}

static double rss_mib(void)
{
   long pages = 0, dummy = 0;
   FILE *f = fopen("/proc/self/statm", "r");
   if (!f)
      return -1.0;
   if (fscanf(f, "%ld %ld", &dummy, &pages) != 2)
      pages = 0;
   fclose(f);
   return (double)pages * 4096.0 / (1024.0 * 1024.0);
}

/* Bytes of the session's file mapping that are actually resident,
 * by mincore over the reservation: exactly the quantity the window
 * bounds, and - unlike process RSS - unmoved by a sanitizer's shadow
 * and quarantine.  Pages the reservation never committed report as
 * not resident. */
static double mapping_resident_mib(const gfx_anim_preview_t *p)
{
   size_t page = (size_t)sysconf(_SC_PAGESIZE);
   uintptr_t lo = (uintptr_t)p->base & ~(uintptr_t)(page - 1);
   size_t n     = ((uintptr_t)p->base + p->len - lo + page - 1) / page;
   unsigned char *vec = (unsigned char*)malloc(n);
   size_t i, res = 0;
   if (!vec)
      return -1.0;
   if (mincore((void*)lo, n * page, vec) != 0)
   {
      free(vec);
      return -1.0;
   }
   for (i = 0; i < n; i++)
      res += (vec[i] & 1) ? 1 : 0;
   free(vec);
   return (double)res * (double)page / (1024.0 * 1024.0);
}

static uint32_t crc32_buf(const void *p, size_t n)
{
   static uint32_t tab[256];
   const uint8_t *b = (const uint8_t*)p;
   uint32_t c = 0xFFFFFFFFu;
   size_t i;
   if (!tab[1])
   {
      uint32_t k, j;
      for (k = 0; k < 256; k++)
      {
         uint32_t v = k;
         for (j = 0; j < 8; j++)
            v = (v & 1) ? (0xEDB88320u ^ (v >> 1)) : (v >> 1);
         tab[k] = v;
      }
   }
   for (i = 0; i < n; i++)
      c = tab[(c ^ b[i]) & 0xFF] ^ (c >> 8);
   return c ^ 0xFFFFFFFFu;
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

/* Whole-buffer reference: CRC of every composited canvas, in order. */
static uint32_t *reference(const uint8_t *buf, size_t len,
      enum image_type_enum type, int *count, unsigned *w, unsigned *h)
{
   void *s = image_transfer_anim_stream_new((void*)buf, len, type);
   uint32_t *crcs = NULL;
   int n = 0, cap = 0, loops = 0, dur;
   const uint32_t *px;
   *count = 0;
   if (!s)
      return NULL;
   image_transfer_anim_stream_get_info(s, type, w, h, &n, &loops);
   n = 0;
   /* Same order the windowed run asks for. */
   image_transfer_anim_stream_set_argb(s, type, 1);
   while ((px = image_transfer_anim_stream_next(s, type, &dur)))
   {
      if (n >= cap)
      {
         cap  = cap ? cap * 2 : 64;
         crcs = (uint32_t*)realloc(crcs, (size_t)cap * sizeof(*crcs));
         if (!crcs)
            break;
      }
      crcs[n++] = crc32_buf(px, (size_t)*w * *h * 4);
   }
   image_transfer_anim_stream_free(s, type);
   *count = n;
   return crcs;
}

/* The bound must be an exact store.  A windowing feeder lowers it when
 * it takes pages back (and at every lap, when the window rewinds); a
 * stream that refuses to lower keeps decoding frames off pages that
 * are no longer there.  Over a fully resident buffer the lowered bound
 * is the only thing standing between next() and the second frame, so
 * "NULL now, the right frame once raised" proves the store. */
static void lowered_bound(const uint8_t *buf, size_t len,
      enum image_type_enum type, const uint32_t *ref, unsigned w, unsigned h)
{
   void *s = image_transfer_anim_stream_new((void*)buf, len, type);
   const uint32_t *px;
   int dur, ok_first, ok_wall, ok_back;
   if (!s)
   {
      check("W6 whole-buffer stream opens", 0);
      return;
   }
   image_transfer_anim_stream_set_argb(s, type, 1);
   px       = image_transfer_anim_stream_next(s, type, &dur);
   ok_first = px && crc32_buf(px, (size_t)w * h * 4) == ref[0];
   image_transfer_anim_stream_set_avail(s, type, 64);
   px       = image_transfer_anim_stream_next(s, type, &dur);
   ok_wall  = (px == NULL);
   image_transfer_anim_stream_set_avail(s, type, len);
   px       = image_transfer_anim_stream_next(s, type, &dur);
   ok_back  = px && crc32_buf(px, (size_t)w * h * 4) == ref[1];
   image_transfer_anim_stream_free(s, type);
   check("W6 lowered bound refuses the next frame (nothing consumed)",
         ok_first && ok_wall);
   check("W6 raised again, the same frame follows", ok_back);
}

/* Channel order: the ask is per frame in the raster menu's worker, so
 * a stream must answer "yes" to the order it is already emitting, and
 * a switch must convert the canvas rather than be refused. */
static void channel_order(const uint8_t *buf, size_t len,
      enum image_type_enum type, const uint32_t *ref, unsigned w, unsigned h)
{
   void *s;
   const uint32_t *px;
   uint32_t rgba2 = 0;
   int dur, ok = 1, ok_switch = 0;

   /* Reference for the third frame in the default (R,G,B,A) order. */
   if ((s = image_transfer_anim_stream_new((void*)buf, len, type)))
   {
      int i;
      for (i = 0; i < 3; i++)
         if (!(px = image_transfer_anim_stream_next(s, type, &dur)))
            break;
      if (i == 3)
         rgba2 = crc32_buf(px, (size_t)w * h * 4);
      image_transfer_anim_stream_free(s, type);
   }

   if (!(s = image_transfer_anim_stream_new((void*)buf, len, type)))
   {
      check("A1 whole-buffer stream opens", 0);
      return;
   }
   ok = image_transfer_anim_stream_set_argb(s, type, 1);
   px = image_transfer_anim_stream_next(s, type, &dur);
   ok = ok && px && crc32_buf(px, (size_t)w * h * 4) == ref[0];
   /* the repeat ask, as gfx_thumbnail_anim_job_step makes it */
   ok = ok && image_transfer_anim_stream_set_argb(s, type, 1);
   px = image_transfer_anim_stream_next(s, type, &dur);
   ok = ok && px && crc32_buf(px, (size_t)w * h * 4) == ref[1];
   check("A1 repeat ask for the current order is honoured", ok);
   ok_switch = image_transfer_anim_stream_set_argb(s, type, 0);
   px = image_transfer_anim_stream_next(s, type, &dur);
   ok_switch = ok_switch && px && rgba2
         && crc32_buf(px, (size_t)w * h * 4) == rgba2;
   check("A2 mid-animation switch converts the canvas (byte-exact)",
         ok_switch);
   image_transfer_anim_stream_free(s, type);
}

/* One pass over the windowed session, feeding before every frame as
 * both consumers do.  Returns frames decoded; mismatches counted. */
static int play_pass(gfx_anim_preview_t *p, const uint32_t *ref,
      int nref, int *mismatch, double *peak, double *peak_map)
{
   int n = 0;
   for (;;)
   {
      const uint32_t *px;
      int dur = 0;
      bool argb = false;
      double r;
      if (!gfx_anim_preview_feed(p))
      {
         printf("      feed failed at frame %d\n", n);
         break;
      }
      px = gfx_anim_preview_next(p, &dur, &argb);
      if (!px)
         break;
      if (n < nref && crc32_buf(px, (size_t)p->width * p->height * 4)
            != ref[n])
         (*mismatch)++;
      n++;
      r = rss_mib();
      if (r > *peak)
         *peak = r;
      r = mapping_resident_mib(p);
      if (r > *peak_map)
         *peak_map = r;
      if (n > nref + 8)
         break;                   /* runaway: never ends a pass */
   }
   return n;
}

static void run_anim(const char *path, const char *label, int big_frame)
{
   enum image_type_enum type = image_texture_get_type(path);
   uint8_t *buf;
   size_t len = 0;
   uint32_t *ref;
   int nref = 0, n1, n2, mm1 = 0, mm2 = 0;
   unsigned w = 0, h = 0;
   double rss0, peak, budget, peak_map = 0.0;
   gfx_anim_preview_t *p;
   struct stat st;

   printf("  %s\n", label);
   if (stat(path, &st) != 0 || st.st_size <= 0)
   {
      check("F0 fixture readable", 0);
      return;
   }
   if (!(buf = slurp(path, &len)))
   {
      check("F0 fixture readable", 0);
      return;
   }
   ref = reference(buf, len, type, &nref, &w, &h);
   if (nref >= 3)
   {
      lowered_bound(buf, len, type, ref, w, h);
      channel_order(buf, len, type, ref, w, h);
   }
   free(buf);
   printf("      reference: %d frames %ux%u, file %.1f MiB\n",
         nref, w, h, (double)len / (1024.0 * 1024.0));
   check("F1 whole-buffer reference decodes >= 2 frames", nref >= 2);
   if (nref < 2)
   {
      free(ref);
      return;
   }

   rss0 = rss_mib();
   peak = rss0;
   p    = gfx_anim_preview_open(path, -1);
   check("W1 progressive open succeeds", p != NULL);
   if (!p)
   {
      free(ref);
      return;
   }
   /* Ask for transparent huge pages on the reservation, as a host with
    * THP=always (the GitHub runners) grants them unasked: a touch at a
    * window edge then faults in a whole 2 MiB page, so residency runs
    * up to two huge pages past what the window commits.  Asking for it
    * here makes every host behave like the worst one, and the budget
    * below carries that allowance explicitly instead of the check
    * passing on a madvise-mode box and failing in CI. */
#ifdef MADV_HUGEPAGE
   {
      size_t page = (size_t)sysconf(_SC_PAGESIZE);
      uintptr_t lo = (uintptr_t)p->base & ~(uintptr_t)(page - 1);
      size_t n  = ((uintptr_t)p->base + p->len - lo + page - 1) & ~(page - 1);
      (void)madvise((void*)lo, n, MADV_HUGEPAGE);
   }
#endif
   check("W1 windowed (reservation available here)",
         gfx_anim_preview_windowed(p));
   check("W1 media floor and consumed are a real cursor",
         image_transfer_anim_stream_media_floor(p->stream, p->type) > 0
         && image_transfer_anim_stream_consumed(p->stream, p->type) > 0);

   n1 = play_pass(p, ref, nref, &mm1, &peak, &peak_map);
   printf("      pass 1: %d frames, %d mismatches, RSS %.1f -> %.1f MiB\n",
         n1, mm1, rss0, peak);
   check("W3 pass 1 plays every frame", n1 == nref);
   check("W3 pass 1 byte-exact vs whole-buffer decode", mm1 == 0);

   gfx_anim_preview_rewind(p);
   n2 = play_pass(p, ref, nref, &mm2, &peak, &peak_map);
   printf("      pass 2: %d frames, %d mismatches, RSS peak %.1f MiB\n",
         n2, mm2, peak);
   check("W4 pass 2 (rewound) plays every frame", n2 == nref);
   check("W4 pass 2 byte-exact", mm2 == 0);

   /* Resident pages of the file mapping: the permanent head, the
    * lookahead, the margin behind the decoder, plus one paced feed
    * budget of slack, two huge pages of edge rounding (see the
    * MADV_HUGEPAGE above), and the oversized frame where there is one.
    * The file is well past that; a whole-file load reports the file
    * length. */
   budget = (double)(GFX_ANIM_PREVIEW_WINDOW_KEEP
         + GFX_ANIM_PREVIEW_WINDOW_AHEAD + GFX_ANIM_PREVIEW_WINDOW_BACK
         + GFX_ANIM_PREVIEW_FEED_BUDGET) / (1024.0 * 1024.0)
         + (big_frame ? 12.0 : 0.0) + 2.0 * 2.0 + 2.0;
   printf("      mapping resident peak %.1f MiB, budget %.1f MiB, "
          "file %.1f MiB\n",
         peak_map, budget, (double)len / (1024.0 * 1024.0));
   check("W2 mapping residency inside the window budget, not the file",
         peak_map >= 0.0 && peak_map < budget
         && budget < (double)len / (1024.0 * 1024.0));
   if (big_frame)
      check("W5 frame larger than the lookahead played (via next_span)",
            n1 == nref && mm1 == 0);

   gfx_anim_preview_close(p);
   free(ref);
}

/* Decoder-level checks only (no window: the file is tiny) on a
 * hand-built APNG whose frames dispose to PREVIOUS through blended
 * sub-frames, so the saved region is live across the order switch. */
static void run_order(const char *path, const char *label)
{
   enum image_type_enum type = image_texture_get_type(path);
   uint8_t *buf;
   size_t len = 0;
   uint32_t *ref;
   int nref = 0;
   unsigned w = 0, h = 0;

   printf("  %s\n", label);
   if (!(buf = slurp(path, &len)))
   {
      check("F0 fixture readable", 0);
      return;
   }
   ref = reference(buf, len, type, &nref, &w, &h);
   printf("      reference: %d frames %ux%u\n", nref, w, h);
   check("F1 whole-buffer reference decodes >= 4 frames", nref >= 4);
   if (nref >= 4)
   {
      /* The switch lands after frame 1, whose DISPOSE_PREVIOUS region
       * is restored onto the canvas before frame 2: that restore is
       * what must come back in the new order. */
      channel_order(buf, len, type, ref, w, h);
   }
   free(ref);
   free(buf);
}

static void run_still(const char *path, const char *label)
{
   uint8_t *buf;
   size_t len = 0;
   void *s;
   int need = -1;
   gfx_anim_preview_t *p;

   printf("  %s\n", label);
   if (!(buf = slurp(path, &len)))
   {
      check("F0 fixture readable", 0);
      return;
   }
   /* From a 64-byte prefix the answer must already be conclusive:
    * VP8X is the first chunk and carries the animation flag. */
   s = image_transfer_anim_stream_new_avail(buf, len, 64, IMAGE_TYPE_WEBP,
         &need, NULL, NULL);
   check("S1 still refused from the first chunk (NULL, need_more=0)",
         s == NULL && need == 0);
   if (s)
      image_transfer_anim_stream_free(s, IMAGE_TYPE_WEBP);
   free(buf);
   p = gfx_anim_preview_open(path, -1);
   check("S1 session open reports a still", p == NULL);
   if (p)
      gfx_anim_preview_close(p);
}


/* V1  a video longer than the fixed window span plays to the end
 *     with its resident bytes bounded by the bitrate-sized feed, not
 *     by the 4 + 8 + 8 MiB constants. The session reports its own
 *     count (head plus moving window); mincore over the reservation
 *     confirms the OS agrees. Prints both peaks so the number is in
 *     the log. */
static void run_video_window(const char *path, const char *label)
{
   gfx_anim_preview_t *p;
   int frames = 0, dur = 0;
   bool argb = false;
   size_t peak_session = 0;
   double peak_pages = 0.0;
   size_t fixed_span = GFX_ANIM_PREVIEW_WINDOW_KEEP + GFX_ANIM_PREVIEW_WINDOW_AHEAD
                     + GFX_ANIM_PREVIEW_WINDOW_BACK;
   size_t bound;

   printf("  %s\n", label);
   p = gfx_anim_preview_open(path, -1);
   if (!p)
   {
      check("V1 video opens through the session", 0);
      return;
   }
   check("V1 session is windowed", gfx_anim_preview_windowed(p));
   printf("      feed ahead=%.2f MiB back=%.2f MiB (fixed span %.0f MiB)\n",
         p->feed_ahead / (1024.0 * 1024.0), p->feed_back / (1024.0 * 1024.0),
         fixed_span / (1024.0 * 1024.0));
   /* what the feed may hold: head + back + ahead, plus one feed
    * budget of slack for the extend that runs past the target */
   bound = GFX_ANIM_PREVIEW_WINDOW_KEEP + p->feed_back + p->feed_ahead
         + GFX_ANIM_PREVIEW_FEED_BUDGET;

   while (frames < 100000)
   {
      size_t r;
      double m;
      if (!gfx_anim_preview_feed(p))
         break;
      if (!gfx_anim_preview_next(p, &dur, &argb))
         break;
      frames++;
      r = gfx_anim_preview_resident_bytes(p);
      if (r > peak_session)
         peak_session = r;
      if ((frames & 63) == 0)
      {
         m = mapping_resident_mib(p);
         if (m > peak_pages)
            peak_pages = m;
      }
   }
   printf("      frames=%d peak resident: session=%.2f MiB pages=%.2f MiB "
          "(file %.2f MiB)\n", frames,
          peak_session / (1024.0 * 1024.0), peak_pages,
          p->len / (1024.0 * 1024.0));
   check("V1 every frame played", frames >= 1700);
   check("V1 resident stayed within head + bitrate-sized window",
         peak_session <= bound);
   check("V1 resident stayed below the fixed span", peak_session < fixed_span);
   gfx_anim_preview_close(p);
}

int main(int argc, char **argv)
{
   int i;
   if (argc < 2)
   {
      printf("usage: %s <fixture dir>\n", argv[0]);
      return 2;
   }
   for (i = 1; i < argc; i++)
   {
      char path[1024];
      snprintf(path, sizeof(path), "%s/anim_lossless.webp", argv[i]);
      run_anim(path, "anim_lossless.webp (one frame > lookahead)", 1);
      snprintf(path, sizeof(path), "%s/anim_lossless.png", argv[i]);
      run_anim(path, "anim_lossless.png (APNG)", 0);
      snprintf(path, sizeof(path), "%s/anim_dispose_prev.png", argv[i]);
      run_order(path, "anim_dispose_prev.png (APNG, DISPOSE_PREVIOUS)");
      snprintf(path, sizeof(path), "%s/still_lossless.webp", argv[i]);
      run_still(path, "still_lossless.webp");
      snprintf(path, sizeof(path), "%s/long_video.mp4", argv[i]);
      run_video_window(path, "long_video.mp4 (60 s at 4 Mbit/s, past the fixed span)");
   }
   printf("\n%s (%d failure%s)\n", fails ? "FAIL" : "PASS", fails,
         fails == 1 ? "" : "s");
   return fails ? 1 : 0;
}
