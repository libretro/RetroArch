/* rh264's hardware path against a backend that checks it.
 *
 * On the hardware path rh264 keeps the sequencing - which pictures are
 * held for reference under which numbers, which slot each is in, and
 * the order they are shown - and a backend reconstructs. This sample
 * stands a fake backend in for the GPU, one that reconstructs nothing
 * and checks everything it is told:
 *
 *   - each picture comes as its slice NAL units behind start codes;
 *   - a picture never goes into a slot a reference or a picture still
 *     waiting to be shown is in, and names no reference in its own;
 *   - every reference names a slot holding a reference picture, with
 *     the frame number and order counts that picture was decoded with;
 *   - only a decoded picture is shown, and once.
 *
 * The order the pictures are shown in is held against the container's
 * own: every sample's presentation time is in the MP4, so the shown
 * pictures' times must rise and every sample must be shown - which the
 * software decoder of the same stream must do as well, picture for
 * picture. Streams from x264 cover CAVLC and CABAC, B-pyramids
 * (adaptive reference marking), several references, weighted
 * prediction, slices, open GOPs and cropping; with catch-up on, the
 * non-reference pictures are passed over on both paths alike. An
 * interlaced stream, and one without its parameter sets, stay in
 * software.
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
#include <formats/video_hw_frame.h>

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

/* ---- the fake backend ---- */

#define FAKE_SLOTS 32
#define MAX_SAMPLES 512

typedef struct
{
   int opened, closed;
   unsigned slots;
   int width, height;
   /* per slot: what was decoded into it */
   int held[FAKE_SLOTS];      /* decoded and a reference             */
   int pending[FAKE_SLOTS];   /* decoded and not yet shown            */
   int fn[FAKE_SLOTS], poc0[FAKE_SLOTS], poc1[FAKE_SLOTS];
   int sample[FAKE_SLOTS];    /* decode index of its picture          */
   int ndecoded;
   int errors;
   char first_error[160];
   /* the shown pictures' decode indices, in order */
   int shown[MAX_SAMPLES];
   int nshown;
   int abandoned;
} fake_t;

static fake_t fake;
static int inject_busy;
static int decode_busy, output_busy;
/* the sample the next decode call is for, as the test feeds them */
static int cur_sample;
static unsigned slot_limit = FAKE_SLOTS;

static void fake_err(const char *fmt, ...)
{
   va_list ap;
   if (!fake.errors++)
   {
      va_start(ap, fmt);
      vsnprintf(fake.first_error, sizeof(fake.first_error), fmt, ap);
      va_end(ap);
   }
}

static void *fake_open(void *user, const rh264_hw_sps_t *sps,
      unsigned slots)
{
   (void)user;
   if (slots < 2 || slots > slot_limit)
      return NULL;
   fake.opened++;
   fake.slots  = slots;
   fake.width  = sps->width;
   fake.height = sps->height;
   return &fake;
}

static int fake_decode(void *dec, const rh264_hw_pic_t *pic)
{
   unsigned i, j;
   fake_t *f = (fake_t*)dec;
   (void)f;
   if (inject_busy && !decode_busy)
   {
      decode_busy = 1;
      return 1;
   }
   decode_busy = 0;
   if (pic->slot < 0 || (unsigned)pic->slot >= fake.slots)
   {
      fake_err("picture %d: slot %d out of range", cur_sample, pic->slot);
      return -1;
   }
   if (!pic->num_slices || !pic->sps || !pic->pps)
      fake_err("picture %d: no slices or parameter sets", cur_sample);
   for (i = 0; i < pic->num_slices; i++)
   {
      const uint8_t *s = pic->data + pic->slice_offsets[i];
      int nut;
      if (     pic->slice_offsets[i] + 4 > pic->size
            || s[0] != 0 || s[1] != 0 || s[2] != 1)
      {
         fake_err("picture %d: slice %u has no start code", cur_sample, i);
         break;
      }
      nut = s[3] & 0x1f;
      if (nut != (pic->idr ? 5 : 1))
         fake_err("picture %d: slice %u is NAL type %d", cur_sample, i, nut);
   }
   if (fake.pending[pic->slot])
      fake_err("picture %d: slot %d still waits to be shown", cur_sample,
            pic->slot);
   if (pic->idr && pic->num_refs)
      fake_err("picture %d: an IDR with %u references", cur_sample,
            pic->num_refs);
   for (i = 0; i < pic->num_refs; i++)
   {
      const rh264_hw_ref_t *r = &pic->refs[i];
      if (r->slot < 0 || (unsigned)r->slot >= fake.slots)
      {
         fake_err("picture %d: reference slot %d", cur_sample, r->slot);
         continue;
      }
      if (r->slot == pic->slot)
         fake_err("picture %d: decoded into its reference's slot %d",
               cur_sample, r->slot);
      if (!fake.held[r->slot])
         fake_err("picture %d: slot %d holds no reference", cur_sample,
               r->slot);
      if (r->poc[0] != fake.poc0[r->slot] || r->poc[1] != fake.poc1[r->slot])
         fake_err("picture %d: slot %d order counts %d/%d, decoded %d/%d",
               cur_sample, r->slot, r->poc[0], r->poc[1],
               fake.poc0[r->slot], fake.poc1[r->slot]);
      if (!r->long_term && r->frame_num != fake.fn[r->slot])
         fake_err("picture %d: slot %d frame_num %d, decoded %d", cur_sample,
               r->slot, r->frame_num, fake.fn[r->slot]);
      for (j = 0; j < i; j++)
         if (pic->refs[j].slot == r->slot)
            fake_err("picture %d: slot %d named twice", cur_sample, r->slot);
   }
   /* the slot's reference, if any, is gone: it is decoded over */
   fake.held[pic->slot]    = pic->reference;
   fake.pending[pic->slot] = 1;
   fake.fn[pic->slot]      = pic->frame_num;
   fake.poc0[pic->slot]    = pic->poc[0];
   fake.poc1[pic->slot]    = pic->poc[1];
   fake.sample[pic->slot]  = cur_sample;
   fake.ndecoded++;
   return 0;
}

static void fake_abandon(void *owner, uint64_t done, uint64_t value)
{
   (void)owner; (void)done; (void)value;
   fake.abandoned++;
}

static int fake_output(void *dec, unsigned slot, video_hw_frame_t *out)
{
   (void)dec;
   if (inject_busy && !output_busy)
   {
      output_busy = 1;
      return 1;
   }
   output_busy = 0;
   if (slot >= fake.slots || !fake.pending[slot])
   {
      fake_err("slot %u shown without a picture waiting", slot);
      return -1;
   }
   fake.pending[slot] = 0;
   if (fake.nshown < MAX_SAMPLES)
      fake.shown[fake.nshown++] = fake.sample[slot];
   memset(out, 0, sizeof(*out));
   out->image      = slot + 1;
   out->done_value = (uint64_t)fake.nshown;
   out->width      = (unsigned)fake.width;
   out->height     = (unsigned)fake.height;
   out->abandon    = fake_abandon;
   return 0;
}

static int readbacks, fail_readback;
static int fake_readback(void *dec, unsigned slot, uint8_t *planes[3])
{
   video_hw_frame_t out;
   size_t y = (size_t)fake.width * fake.height;
   size_t c = (size_t)((fake.width + 1) / 2) * ((fake.height + 1) / 2);
   readbacks++;
   if (fail_readback || fake_output(dec, slot, &out) != 0)
      return -1;
   memset(planes[0], 19, y);
   memset(planes[1], 71, c);
   memset(planes[2], 137, c);
   return 0;
}

static void fake_close(void *dec)
{
   (void)dec;
   fake.closed = 1;
}

static rh264_hw_ops_t fake_ops = {
   NULL, fake_open, fake_decode, fake_output, fake_close, fake_readback
};

/* ---- the streams ---- */

typedef struct
{
   int n;                       /* samples                              */
   int64_t pts[MAX_SAMPLES];    /* per sample, decode order             */
   int shown;                   /* pictures shown                       */
   int dropped;                 /* passed over under catch-up           */
   int hw;                      /* rh264_video_set_hw said yes           */
   int failed;                  /* a decode or drain refused            */
   int order[MAX_SAMPLES];      /* hw: decode index of each shown       */
   int materialized;
} result_t;

static int deferred_output(rh264_video *v, result_t *res)
{
   int w, h, stride, s, chosen = -1, before = readbacks;
   if (rh264_video_frame_size(v, &w, &h) != 0
         || w != fake.width || h != fake.height)
      return -1;
   if (rh264_video_plane(v, 0, NULL, NULL, NULL))
   {
      /* A backend without spare capacity retains eager CPU readback. */
      if (rh264_video_readback(v) != 0 || readbacks != before)
         return -1;
      res->materialized++;
      return 0;
   }
   if (res->shown % 3 == 0)
   {
      const uint8_t *pixels;
      if (rh264_video_readback(v) != 0)
         return -1;
      pixels = rh264_video_plane(v, 0, &stride, &w, &h);
      if (!pixels || pixels[0] != 19 || stride != w)
         return -1;
      if (rh264_video_readback(v) != 0 || readbacks != before + 1)
         return -1;
      res->materialized++;
   }
   else
   {
      /* The consumer deliberately drops the next picture in display order.
       * Its backend slot may now be reused without a readback callback. */
      for (s = 0; s < (int)fake.slots; s++)
         if (fake.pending[s] && (chosen < 0
               || res->pts[fake.sample[s]] < res->pts[fake.sample[chosen]]))
            chosen = s;
      if (chosen < 0 || readbacks != before)
         return -1;
      fake.pending[chosen] = 0;
   }
   return 0;
}

/* Decodes @mp4, on the backend when @hw, with catch-up on over samples
 * [skip_lo, skip_hi). */
static int decode(const char *mp4, int hw, int skip_lo, int skip_hi,
      int with_extradata, result_t *res)
{
   size_t n;
   uint8_t *b = slurp(mp4, &n);
   rmp4_t *m;
   rmp4_packet pkt;
   rh264_video *v;
   int i, trk = -1;

   memset(res, 0, sizeof(*res));
   if (!b)
      return -1;
   if (!(m = rmp4_open_memory(b, n)))
   {
      free(b);
      return -1;
   }
   for (i = 0; i < rmp4_num_tracks(m); i++)
      if (rmp4_get_track(m, i)->type == RMP4_TRACK_VIDEO)
      { trk = i; break; }
   v = rh264_video_open();
   if (trk < 0 || !v)
   {
      rh264_video_close(v);
      rmp4_close(m);
      free(b);
      return -1;
   }
   if (with_extradata)
      rh264_video_set_extradata(v, rmp4_get_track(m, trk)->codec_private,
            rmp4_get_track(m, trk)->codec_private_size);
   memset(&fake, 0, sizeof(fake));
   readbacks = 0;
   if (hw >= 2)
   {
      rh264_video_set_hw_readback(v, 1);
      rh264_video_set_deferred_readback(v, hw >= 3);
      if (hw >= 3 && rh264_video_readback(v) == 0)
         res->failed = 1;
   }
   else if (hw)
      res->hw = rh264_video_set_hw(v, 1);

   for (;;)
   {
      int r = rmp4_read_packet(m, &pkt);
      if (r != 1)
         break;
      if (pkt.track != trk)
         continue;
      if (res->n < MAX_SAMPLES)
         res->pts[res->n] = pkt.timestamp;
      cur_sample = res->n;
      rh264_video_set_skip_nonref(v,
            res->n >= skip_lo && res->n < skip_hi);
      {
         unsigned tries = 0;
         do
            r = rh264_video_decode(v, pkt.data, pkt.size);
         while (r == 2 && ++tries < 100);
         if (r == 2)
            r = -1;
      }
      res->n++;
      if (rh264_video_dropped(v))
         res->dropped++;
      if (r < 0)
      {
         res->failed = 1;
         break;
      }
      if (r == 1)
      {
         if (hw >= 3 && deferred_output(v, res) != 0)
         { res->failed = 1; break; }
         res->shown++;
      }
   }
   while (!res->failed)
   {
      int r = rh264_video_drain(v);
      if (r == 2)
         continue;
      if (r != 0)
         break;
      if (hw >= 3 && deferred_output(v, res) != 0)
      { res->failed = 1; break; }
      res->shown++;
   }
   if (hw >= 2)
      res->hw = rh264_video_is_hw(v);
   if (hw >= 3 && !res->failed && rh264_video_readback(v) == 0)
      res->failed = 1;
   if (res->hw)
   {
      video_hw_frame_t fr;
      /* the frame of the last picture shown is still to be had */
      if (hw < 2 && res->shown && rh264_video_hw_frame(v, &fr) != 0)
         res->failed = 1;
      if (hw < 2 && res->shown && rh264_video_plane(v, 0, NULL, NULL, NULL))
         res->failed = 1;
      if (hw == 2 && res->shown)
      {
         int plane, stride, w, h;
         static const uint8_t expected[3] = {19, 71, 137};
         for (plane = 0; plane < 3; plane++)
         {
            const uint8_t *pixels = rh264_video_plane(v, plane, &stride, &w, &h);
            size_t j, count = (size_t)(plane ? (fake.width + 1) / 2 : fake.width)
               * (plane ? (fake.height + 1) / 2 : fake.height);
            if (!pixels || w != (plane ? (fake.width + 1) / 2 : fake.width)
                  || h != (plane ? (fake.height + 1) / 2 : fake.height) || stride != w)
            {
               res->failed = 1;
               break;
            }
            for (j = 0; j < count; j++)
               if (pixels[j] != expected[plane])
                  res->failed = 1;
         }
         if (rh264_video_hw_frame(v, &fr) == 0)
            res->failed = 1;
      }
      memcpy(res->order, fake.shown, sizeof(res->order));
   }
   if (res->hw && hw == 1 && !res->failed)
   {
      int pass;
      int decoded = fake.ndecoded;
      int shown = fake.nshown;
      for (pass = 0; pass < 3; pass++)
      {
         int r = 0;
         rmp4_rewind(m);
         if (!rh264_video_restart_hw(&v) || fake.opened != 1 || fake.closed)
         {
            res->failed = 1;
            break;
         }
         while (rmp4_read_packet(m, &pkt) == 1)
         {
            unsigned tries = 0;
            if (pkt.track != trk)
               continue;
            cur_sample = 0;
            rh264_video_set_skip_nonref(v, 0);
            do
               r = rh264_video_decode(v, pkt.data, pkt.size);
            while (r == 2 && ++tries < 100);
            break;
         }
         if (r == 0)
            do
               r = rh264_video_drain(v);
            while (r == 2);
         else if (r == 1)
            r = 0;
         if (r != 0 || fake.errors)
            res->failed = 1;
      }
      check("three hardware restarts retain the backend session",
            !res->failed && fake.opened == 1 && !fake.closed);
      fake.ndecoded = decoded;
      fake.nshown = shown;
   }
   rh264_video_close(v);
   rmp4_close(m);
   free(b);
   return 0;
}

static void hw_case(const char *name, const char *src, int frames,
      const char *x264, int expect_hw)
{
   char mp4[512], label[160];
   result_t sw, hw;
   int i, rising = 1, slots_fit;

   snprintf(mp4, sizeof(mp4), "%s/%s.mp4", dir, name);
   if (run("ffmpeg -v error -y -f lavfi -i \"%s\" -frames:v %d -c:v libx264 "
           "-pix_fmt yuv420p %s '%s'", src, frames, x264, mp4) != 0)
   {
      snprintf(label, sizeof(label), "%s: encoded", name);
      check(label, 0);
      return;
   }
   printf("%s (%s):\n", name, x264);

   decode(mp4, 0, -1, -1, 1, &sw);
   decode(mp4, 1, -1, -1, 1, &hw);
   snprintf(label, sizeof(label), "%s: software shows every sample", name);
   check(label, !sw.failed && sw.shown == sw.n && sw.n == frames);
   snprintf(label, sizeof(label), "%s: %s", name,
         expect_hw ? "decodes on the backend" : "stays in software");
   check(label, hw.hw == expect_hw);
   if (!expect_hw)
   {
      snprintf(label, sizeof(label), "%s: shows every sample all the same",
            name);
      check(label, !hw.failed && hw.shown == hw.n);
      return;
   }
   slots_fit = fake.slots <= FAKE_SLOTS;
   if (fake.errors)
      printf("      backend: %d error%s, the first: %s\n", fake.errors,
            fake.errors == 1 ? "" : "s", fake.first_error);
   snprintf(label, sizeof(label), "%s: the backend was told nothing wrong",
         name);
   check(label, slots_fit && !fake.errors);
   printf("      %d samples, %d decoded, %d shown by the backend, %d by the decoder\n",
         hw.n, fake.ndecoded, fake.nshown, hw.shown);
   snprintf(label, sizeof(label), "%s: every picture decoded and shown once",
         name);
   check(label, !hw.failed && fake.ndecoded == hw.n
         && fake.nshown == hw.n && hw.shown == hw.n);
   for (i = 1; i < fake.nshown; i++)
      if (hw.pts[hw.order[i]] <= hw.pts[hw.order[i - 1]])
         rising = 0;
   snprintf(label, sizeof(label),
         "%s: shown in presentation order, as software", name);
   check(label, rising && hw.shown == sw.shown);
   snprintf(label, sizeof(label), "%s: closed with the decoder", name);
   check(label, fake.closed && !fake.abandoned);

   /* catch-up over the middle third: the same pictures passed over */
   {
      int lo = frames / 3, hi = 2 * frames / 3;
      decode(mp4, 0, lo, hi, 1, &sw);
      decode(mp4, 1, lo, hi, 1, &hw);
      rising = 1;
      for (i = 1; i < fake.nshown; i++)
         if (hw.pts[hw.order[i]] <= hw.pts[hw.order[i - 1]])
            rising = 0;
      printf("      catch-up: %d of %d passed over\n", hw.dropped, hw.n);
      snprintf(label, sizeof(label),
            "%s: catch-up passes the same pictures over", name);
      check(label, !hw.failed && !fake.errors && rising
            && hw.dropped == sw.dropped
            && hw.shown == sw.shown && hw.shown == hw.n - hw.dropped);
   }
}

int main(void)
{
   result_t r;
   char mp4[512];

   if (system("ffmpeg -version >/dev/null 2>&1") != 0)
   {
      printf("rh264_hw_test: ffmpeg is required (with libx264)\n");
      return 1;
   }
   snprintf(dir, sizeof(dir), "/tmp/rh264_hw_%ld", (long)getpid());
   if (run("mkdir -p '%s'", dir) != 0)
      return 2;
   rh264_set_hw_backend(&fake_ops);

   hw_case("baseline", "testsrc2=s=176x144:r=25", 40,
         "-profile:v baseline -preset medium -g 12", 1);
   hw_case("main_bpyr", "testsrc2=s=176x144:r=25", 60,
         "-profile:v main -preset slow -x264-params bframes=3:b-pyramid=normal:ref=4:keyint=30", 1);
   hw_case("high_strict", "mandelbrot=s=192x128:r=25", 60,
         "-profile:v high -preset slow -x264-params bframes=3:b-pyramid=strict:ref=6:weightp=2:keyint=24", 1);
   hw_case("slices_wp", "testsrc2=s=192x160:r=25,fade=in:0:20", 48,
         "-profile:v high -preset medium -x264-params slices=4:weightp=2:bframes=2", 1);
   hw_case("open_gop", "testsrc2=s=176x144:r=25", 72,
         "-profile:v high -preset medium -x264-params open-gop=1:keyint=16:bframes=3:b-pyramid=normal", 1);
   hw_case("cavlc_refs", "mandelbrot=s=176x144:r=25", 48,
         "-profile:v main -preset medium -x264-params cabac=0:ref=8:bframes=1", 1);
   hw_case("cropped", "testsrc2=s=170x122:r=25", 24,
         "-profile:v high -preset medium -x264-params bframes=2", 1);
   if (fake.width != 170 || fake.height != 122)
      printf("      backend opened for %dx%d\n", fake.width, fake.height);
   check("cropped: the backend is told the picture shown",
         fake.width == 170 && fake.height == 122);
   hw_case("interlaced", "testsrc2=s=176x144:r=25", 16,
         "-profile:v high -preset medium -x264-params tff=1:bframes=2", 0);

   /* no parameter sets before the first picture: nothing to open on */
   snprintf(mp4, sizeof(mp4), "%s/baseline.mp4", dir);
   decode(mp4, 1, -1, -1, 0, &r);
   check("no parameter sets yet: stays in software", !r.hw);

   decode(mp4, 2, -1, -1, 1, &r);
   check("readback: lazy hardware activation, I420 planes and drain",
         r.hw && !r.failed && r.shown == r.n && !fake.errors && fake.closed);
   slot_limit = fake.slots;
   decode(mp4, 3, -1, -1, 1, &r);
   check("deferred: no spare backend slot retains hardware with eager planes",
         r.hw && !r.failed && r.shown == r.n && !fake.errors && readbacks == r.shown);
   slot_limit = FAKE_SLOTS;
   decode(mp4, 3, -1, -1, 1, &r);
   check("deferred: all outputs, only requested CPU copies, repeated access cached",
         r.hw && !r.failed && r.shown == r.n && !fake.errors
         && r.materialized == (r.shown + 2) / 3 && readbacks == r.materialized);
   printf("      deferred: %d outputs, %d readbacks\n", r.shown, readbacks);
   snprintf(mp4, sizeof(mp4), "%s/main_bpyr.mp4", dir);
   decode(mp4, 3, -1, -1, 1, &r);
   check("deferred: B-frame/reference sequencing survives discarded outputs and drain",
         r.hw && !r.failed && r.shown == r.n && !fake.errors
         && readbacks == r.materialized && r.materialized < r.shown);
   snprintf(mp4, sizeof(mp4), "%s/cropped.mp4", dir);
   decode(mp4, 3, -1, -1, 1, &r);
   check("deferred: cropped metadata and readback dimensions agree",
         r.hw && !r.failed && r.shown == r.n && !fake.errors);
   fail_readback = 1;
   decode(mp4, 3, -1, -1, 1, &r);
   check("deferred: backend readback failure reaches the caller",
         r.hw && r.failed && readbacks == 1 && fake.closed);
   fail_readback = 0;
   snprintf(mp4, sizeof(mp4), "%s/baseline.mp4", dir);
   fake_ops.readback = NULL;
   decode(mp4, 2, -1, -1, 1, &r);
   check("readback: backend without readback stays in software",
         !r.hw && !r.failed && r.shown == r.n);
   fake_ops.readback = fake_readback;
   rh264_set_hw_backend(NULL);
   decode(mp4, 1, -1, -1, 1, &r);
   check("no backend: stays in software", !r.hw && !r.failed && r.shown == r.n);

   inject_busy = 1;
   rh264_set_hw_backend(&fake_ops);
   decode_busy = output_busy = 0;
   hw_case("busy_baseline", "testsrc2=s=176x144:r=25", 24,
         "-profile:v baseline -preset medium -x264-params bframes=0", 1);
   hw_case("busy_bpyr", "testsrc2=s=176x144:r=25", 48,
         "-profile:v main -preset slow -x264-params bframes=3:b-pyramid=normal:ref=4", 1);
   hw_case("busy_slices", "testsrc2=s=192x160:r=25", 24,
         "-profile:v high -preset medium -x264-params slices=4:bframes=2", 1);
   inject_busy = 0;
   run("rm -rf '%s'", dir);
   printf("rh264_hw_test: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
         fails, fails == 1 ? "" : "s");
   return fails ? 1 : 0;
}
