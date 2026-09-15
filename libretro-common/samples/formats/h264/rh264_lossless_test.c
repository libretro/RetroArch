/* rh264 against ffmpeg as the oracle, byte-exact: High 4:4:4
 * Predictive (the transform bypass, 4:4:4 chroma, 10-bit samples),
 * CABAC I_PCM, constrained_intra_pred.
 *
 * x264's lossless mode (-qp 0) sets qpprime_y_zero_transform_bypass_flag
 * and codes every macroblock at QP'Y 0: the residual is the sample
 * difference itself, no transform, no scaling, no DC Hadamard, plus the
 * DPCM accumulation for straight-down / straight-across intra
 * prediction (8.5.15).  rh264 used to refuse the SPS.  The corpus below
 * is built here with ffmpeg and covers CAVLC and CABAC, the 4x4 and
 * 8x8 transforms, I/P/B pictures and all-intra:
 *
 *   L1..L6  six lossless encodes decode byte-exact against ffmpeg.
 *
 * The deblocking rule (8.7.2.1): the samples of a macroblock in
 * transform-bypass mode are never modified by the filter, while the
 * other side of a shared edge still is.  No encoder mixes lossless and
 * lossy macroblocks, so the stream is made here: two slices per
 * picture, the first taken from a lossless encode, the second from a
 * lossy one of the same frames, with the second slice's header patched
 * to its real QP and the filter enabled across the boundary.  ffmpeg
 * filters the bypass side too (it is not conformant here), so on that
 * stream the oracle is split:
 *
 *   M1  the bypass slice's samples are the untouched source (the
 *       lossless decode with the filter off);
 *   M2  the lossy slice matches ffmpeg (the q side reads the unfiltered
 *       p samples, so whether p is written back does not reach it).
 *
 * Needs ffmpeg with libx264 in PATH; that is a hard requirement, a
 * missing tool is a failure, not a skip.  Fixtures are built in a
 * temporary directory and removed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>

#include <formats/rmp4.h>
#include <formats/rh264.h>

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

/* Decode an MP4 through rmp4 + rh264 and compare every output sample
 * with the reference planes: for luma row y (chroma row y/2) below
 * 'split' against ref_a, from it on against ref_b (ref_b == ref_a and
 * split == 0 for a single reference).  Returns the number of differing
 * samples, -1 on decode failure, and writes the frame count. */
static long compare(const char *mp4, const uint8_t *ref_a, size_t alen,
      const uint8_t *ref_b, int split, int *frames_out)
{
   size_t n;
   uint8_t *b = slurp(mp4, &n);
   rmp4_t *m;
   rh264_video *h;
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
   h = rh264_video_open();
   if (trk < 0 || !h || rh264_video_set_extradata(h,
            rmp4_get_track(m, trk)->codec_private,
            rmp4_get_track(m, trk)->codec_private_size))
   {
      if (h) rh264_video_close(h);
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
      got = rh264_video_decode(h, pkt.data, pkt.size);
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
            p[k] = rh264_video_plane(h, k, &st[k], &w[k], &hh[k]);
         /* above 8 bits the planes hold uint16_t samples and ffmpeg's
          * raw output is little-endian 16-bit: compare bytes */
         int bps = (rh264_video_bit_depth(h) > 8) ? 2 : 1;
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
         frames++;
      }
   }
   while (rh264_video_drain(h) == 0)
   {
      const uint8_t *p[3];
      int st[3], w[3], hh[3], k;
      for (k = 0; k < 3; k++)
         p[k] = rh264_video_plane(h, k, &st[k], &w[k], &hh[k]);
      int bps = (rh264_video_bit_depth(h) > 8) ? 2 : 1;
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
      frames++;
   }
   if (off != alen)
      bad = bad < 0 ? bad : bad + 1;   /* short: not every reference frame came out */
done:
   *frames_out = frames;
   rh264_video_close(h);
   rmp4_close(m);
   free(b);
   return bad;
}

/* Encode 'src' with x264 ('rate' is -qp 0 / -crf N, 'pix' the chroma
 * format), decode it through rmp4 + rh264 and compare every sample
 * with ffmpeg's decode. */
static void oracle_case(const char *name, const char *src, int frames,
      const char *pix, const char *rate, const char *x264)
{
   char mp4[512], yuv[512], label[160];
   uint8_t *ref;
   size_t rlen;
   int nf = 0;
   long bad;
   snprintf(mp4, sizeof(mp4), "%s/%s.mp4", dir, name);
   snprintf(yuv, sizeof(yuv), "%s/%s.yuv", dir, name);
   snprintf(label, sizeof(label), "%s (%s %s) byte-exact vs ffmpeg",
         name, pix, x264);
   if (run("ffmpeg -v error -y -f lavfi -i \"%s\" -frames:v %d -c:v libx264 "
           "%s -pix_fmt %s %s '%s' && "
           "ffmpeg -v error -y -i '%s' -f rawvideo -pix_fmt %s '%s'",
           src, frames, rate, pix, x264, mp4, mp4, pix, yuv) != 0
       || !(ref = slurp(yuv, &rlen)))
   {
      check(label, 0);
      return;
   }
   bad = compare(mp4, ref, rlen, ref, 0, &nf);
   printf("      %d frames, %ld differing samples%s\n", nf, bad < 0 ? 0 : bad,
         bad < 0 ? " (decode refused or failed)" : "");
   check(label, bad == 0 && nf == frames);
   free(ref);
}

static void lossless_case(const char *name, const char *src,
      int frames, const char *x264)
{
   oracle_case(name, src, frames, "yuv420p", "-qp 0", x264);
}

/* ---- the mixed stream: Annex-B surgery ---------------------------- */

typedef struct { const uint8_t *b; size_t n; size_t pos; } br_t;
static unsigned br_u(br_t *r, int n)
{
   unsigned v = 0;
   while (n-- > 0)
   {
      v = (v << 1) | ((r->b[r->pos >> 3] >> (7 - (r->pos & 7))) & 1);
      r->pos++;
   }
   return v;
}
static unsigned br_ue(br_t *r)
{
   int z = 0;
   while (br_u(r, 1) == 0 && z < 32) z++;
   return z ? ((1u << z) - 1 + br_u(r, z)) : 0;
}
static int br_se(br_t *r)
{
   unsigned k = br_ue(r);
   return (k & 1) ? (int)((k + 1) / 2) : -(int)(k / 2);
}

typedef struct { uint8_t *b; size_t cap; size_t pos; } bw_t;
static void bw_bit(bw_t *w, int bit)
{
   if ((w->pos >> 3) >= w->cap)
      return;
   if (bit) w->b[w->pos >> 3] |= (uint8_t)(0x80 >> (w->pos & 7));
   w->pos++;
}
static void bw_ue(bw_t *w, unsigned v)
{
   unsigned x = v + 1;
   int n = 0, i;
   while ((x >> n) > 1) n++;
   for (i = 0; i < n; i++) bw_bit(w, 0);
   for (i = n; i >= 0; i--) bw_bit(w, (x >> i) & 1);
}
static void bw_se(bw_t *w, int v)
{
   bw_ue(w, v > 0 ? (unsigned)(2*v - 1) : (unsigned)(-2*v));
}

static size_t unescape(const uint8_t *s, size_t n, uint8_t *d)
{
   size_t i, o = 0; int z = 0;
   for (i = 0; i < n; i++)
   {
      if (z >= 2 && s[i] == 3) { z = 0; continue; }
      d[o++] = s[i];
      z = s[i] ? 0 : z + 1;
   }
   return o;
}
static size_t escape(const uint8_t *s, size_t n, uint8_t *d)
{
   size_t i, o = 0; int z = 0;
   for (i = 0; i < n; i++)
   {
      if (z >= 2 && s[i] <= 3) { d[o++] = 3; z = 0; }
      d[o++] = s[i];
      z = s[i] ? 0 : z + 1;
   }
   return o;
}

/* Rewrite an IDR slice header (CAVLC, frame_mbs_only, POC type 2,
 * log2_max_frame_num 4, no weighted prediction - what x264 emits for
 * these encodes): slice_qp_delta becomes 'qp', the deblocking filter
 * is enabled with offsets +3/+3, the slice data follows bit-exact. */
static size_t patch_slice(const uint8_t *nal, size_t n, int qp,
      uint8_t *out, size_t cap, unsigned *first_mb)
{
   uint8_t *rb = (uint8_t*)malloc(n), *nb;
   size_t rn, i, pos_qp, pos_end, obits, on;
   br_t r;
   bw_t w;
   int idc;
   if (!rb)
      return 0;
   rn = unescape(nal, n, rb);
   r.b = rb; r.n = rn; r.pos = 8;
   *first_mb = br_ue(&r);            /* first_mb_in_slice   */
   br_ue(&r);                        /* slice_type          */
   br_ue(&r);                        /* pic_parameter_set_id*/
   br_u(&r, 4);                      /* frame_num           */
   br_ue(&r);                        /* idr_pic_id          */
   br_u(&r, 1); br_u(&r, 1);         /* no_output_of_prior, long_term_ref */
   pos_qp = r.pos;
   br_se(&r);                        /* slice_qp_delta      */
   idc = (int)br_ue(&r);             /* disable_deblocking_filter_idc */
   if (idc != 1) { br_se(&r); br_se(&r); }
   pos_end = r.pos;
   nb = (uint8_t*)calloc(1, rn + 16);
   if (!nb) { free(rb); return 0; }
   w.b = nb; w.cap = rn + 16; w.pos = 0;
   for (i = 0; i < pos_qp; i++) bw_bit(&w, (rb[i >> 3] >> (7 - (i & 7))) & 1);
   bw_se(&w, qp); bw_ue(&w, 0); bw_se(&w, 3); bw_se(&w, 3);
   for (i = pos_end; i < rn * 8; i++) bw_bit(&w, (rb[i >> 3] >> (7 - (i & 7))) & 1);
   obits = w.pos;
   on = (obits + 7) >> 3;
   /* the rbsp trailing bits are inside the copied tail; the byte pad is zero */
   if (on * 2 + 4 > cap) { free(rb); free(nb); return 0; }
   on = escape(nb, on, out);
   free(rb); free(nb);
   return on;
}

/* Annex-B: for each NAL, call fn(nal, len). */
static int for_each_nal(const uint8_t *b, size_t n,
      int (*fn)(const uint8_t *nal, size_t len, void *ud), void *ud)
{
   size_t i = 0, start = 0;
   int have = 0;
   for (i = 0; i + 2 < n; i++)
   {
      if (b[i] == 0 && b[i+1] == 0 && b[i+2] == 1)
      {
         if (have)
         {
            size_t e = i;
            while (e > start && b[e-1] == 0) e--;
            if (fn(b + start, e - start, ud)) return -1;
         }
         start = i + 3; have = 1; i += 2;
      }
   }
   if (have && fn(b + start, n - start, ud)) return -1;
   return 0;
}

typedef struct { uint8_t *out; size_t cap, len; int nslice; const uint8_t *lossy; size_t lossy_len; } mix_t;

typedef struct { int want, seen; const uint8_t *nal; size_t len; } pick_t;

static int lossy_pick(const uint8_t *nal, size_t len, void *ud)
{
   /* the k-th slice of the lossy stream, k = target index */
   pick_t *st = (pick_t*)ud;
   if ((nal[0] & 31) != 5) return 0;
   if (st->seen++ == st->want) { st->nal = nal; st->len = len; }
   return 0;
}

static int mix_emit(const uint8_t *nal, size_t len, void *ud)
{
   mix_t *m = (mix_t*)ud;
   const uint8_t *src = nal;
   size_t slen = len, pn;
   unsigned fm = 0;
   uint8_t pat[65536];
   if ((nal[0] & 31) == 5)
   {
      int k = m->nslice++;
      int lossy = (k & 1);
      if (lossy)
      {
         pick_t st;
         st.want = k; st.seen = 0; st.nal = NULL; st.len = 0;
         for_each_nal(m->lossy, m->lossy_len, lossy_pick, &st);
         if (!st.nal) return -1;
         src = st.nal; slen = st.len;
      }
      pn = patch_slice(src, slen, lossy ? 27 : 0, pat, sizeof(pat), &fm);
      if (!pn || (lossy && fm == 0) || (!lossy && fm != 0)) return -1;
      src = pat; slen = pn;
   }
   if (m->len + 4 + slen > m->cap) return -1;
   memcpy(m->out + m->len, "\0\0\0\1", 4); m->len += 4;
   memcpy(m->out + m->len, src, slen); m->len += slen;
   return 0;
}

static void mixed_case(void)
{
   char ll[512], lo[512], mix[512], mixmp4[512], srcyuv[512], ffyuv[512];
   uint8_t *a, *b, *ref_src, *ref_ff;
   size_t an, bn, sn, fn;
   mix_t m;
   FILE *f;
   int nf = 0;
   long bad;
   snprintf(ll, sizeof(ll), "%s/ll2.h264", dir);
   snprintf(lo, sizeof(lo), "%s/lossy2.h264", dir);
   snprintf(mix, sizeof(mix), "%s/mix.h264", dir);
   snprintf(mixmp4, sizeof(mixmp4), "%s/mix.mp4", dir);
   snprintf(srcyuv, sizeof(srcyuv), "%s/ll2.yuv", dir);
   snprintf(ffyuv, sizeof(ffyuv), "%s/mix.yuv", dir);
   /* the same three frames, two slices each: lossless (filter off,
    * so its decode is the source), and CQP 27 with the filter on */
   if (run("ffmpeg -v error -y -f lavfi -i testsrc2=s=96x80:r=10 -frames:v 3 "
           "-c:v libx264 -preset ultrafast -qp 0 -g 1 -x264-params slices=2 "
           "-pix_fmt yuv420p '%s/ll2.mp4' && "
           "ffmpeg -v error -y -i '%s/ll2.mp4' -c:v copy -bsf:v h264_mp4toannexb '%s' && "
           "ffmpeg -v error -y -i '%s/ll2.mp4' -f rawvideo -pix_fmt yuv420p '%s' && "
           "ffmpeg -v error -y -f lavfi -i testsrc2=s=96x80:r=10 -frames:v 3 "
           "-c:v libx264 -preset ultrafast -qp 30 -g 1 "
           "-x264-params slices=2:deblock=6,6:aq-mode=0 -pix_fmt yuv420p '%s/lossy2.mp4' && "
           "ffmpeg -v error -y -i '%s/lossy2.mp4' -c:v copy -bsf:v h264_mp4toannexb '%s'",
           dir, dir, ll, dir, srcyuv, dir, dir, lo) != 0)
   {
      check("M0 mixed-stream fixtures built", 0);
      return;
   }
   a = slurp(ll, &an); b = slurp(lo, &bn);
   if (!a || !b)
   {
      check("M0 mixed-stream fixtures built", 0);
      free(a); free(b);
      return;
   }
   m.cap = an * 2 + bn * 2 + 4096;
   m.out = (uint8_t*)malloc(m.cap);
   m.len = 0; m.nslice = 0; m.lossy = b; m.lossy_len = bn;
   if (!m.out || for_each_nal(a, an, mix_emit, &m) != 0 || m.nslice != 6)
   {
      check("M0 mixed stream assembled (6 slices)", 0);
      free(a); free(b); free(m.out);
      return;
   }
   f = fopen(mix, "wb");
   if (!f || fwrite(m.out, 1, m.len, f) != m.len)
   {
      if (f) fclose(f);
      check("M0 mixed stream written", 0);
      free(a); free(b); free(m.out);
      return;
   }
   fclose(f);
   free(a); free(b); free(m.out);
   check("M0 mixed stream assembled (6 slices)", 1);
   if (run("ffmpeg -v error -y -i '%s' -c:v copy '%s' && "
           "ffmpeg -v error -y -i '%s' -f rawvideo -pix_fmt yuv420p '%s'",
           mix, mixmp4, mixmp4, ffyuv) != 0)
   {
      check("M0 ffmpeg decodes the mixed stream", 0);
      return;
   }
   ref_src = slurp(srcyuv, &sn);
   ref_ff  = slurp(ffyuv, &fn);
   if (!ref_src || !ref_ff || sn != fn)
   {
      check("M0 references readable", 0);
      free(ref_src); free(ref_ff);
      return;
   }
   /* luma rows 0..47 are the bypass slice: must be the source; rows
    * 48.. are the lossy slice: must match ffmpeg */
   bad = compare(mixmp4, ref_src, sn, ref_ff, 48, &nf);
   printf("      %d frames, %ld differing samples\n", nf, bad < 0 ? 0 : bad);
   check("M1/M2 bypass slice untouched by the filter, lossy slice == ffmpeg",
         bad == 0 && nf == 3);
   free(ref_src); free(ref_ff);
}

int main(void)
{
   if (system("ffmpeg -version >/dev/null 2>&1") != 0)
   {
      printf("rh264_lossless_test: ffmpeg is required (with libx264)\n");
      return 1;
   }
   snprintf(dir, sizeof(dir), "/tmp/rh264_lossless_%ld", (long)getpid());
   if (run("mkdir -p '%s'", dir) != 0)
      return 2;

   printf("rh264 transform bypass (x264 -qp 0), byte-exact vs ffmpeg:\n");
   lossless_case("uf_i",      "testsrc2=s=96x80:r=10",    6, "-preset ultrafast -g 1");
   lossless_case("uf_ip",     "testsrc2=s=96x80:r=10",   10, "-preset ultrafast");
   lossless_case("med_ibp",   "testsrc2=s=96x80:r=10",   12, "-preset medium");
   lossless_case("med_cavlc", "testsrc2=s=96x80:r=10",   12, "-preset medium -x264-params cabac=0");
   lossless_case("mandel_med","mandelbrot=s=112x96:r=10", 8, "-preset medium");
   lossless_case("mandel_i",  "mandelbrot=s=112x96:r=10", 4, "-preset ultrafast -g 1");
   /* Noise is cheaper raw than coded: x264 lossless makes every
    * macroblock I_PCM, which under CABAC pins where the samples start
    * relative to the arithmetic decoder's read position (the engine
    * prefetches; the samples begin after the stop bit, byte aligned). */
   lossless_case("pcm_cabac", "nullsrc=s=96x80:r=10,geq=lum='random(1)*255':cb='random(2)*255':cr='random(3)*255'",
         2, "-preset medium -g 1");
   printf("deblocking across a bypass / lossy slice boundary (8.7.2.1):\n");
   mixed_case();
   /* 4:4:4 (ChromaArrayType 3): Cb and Cr decoded as luma.  Intra and
    * inter, CAVLC and CABAC, 4x4 and 8x8 transforms, weighted
    * bi-prediction with a B-pyramid, I_PCM, the deblocking filter on
    * (luma-style on the chroma planes), lossy and lossless. */
   /* constrained_intra_pred: an inter-coded neighbour is not available
    * to intra prediction (8.3.1.2) - the up-right and up-left ones too,
    * and in CABAC the contexts still see it.  Intra macroblocks amid
    * inter ones in P / B pictures exercise every neighbour position. */
   printf("constrained_intra_pred, byte-exact vs ffmpeg:\n");
   oracle_case("cip_cabac",  "testsrc2=s=176x144:r=15",   8, "yuv420p", "-crf 20",
         "-preset medium -x264-params constrained-intra=1");
   oracle_case("cip_cavlc",  "testsrc2=s=176x144:r=15",   8, "yuv420p", "-crf 20",
         "-preset medium -x264-params constrained-intra=1:cabac=0");
   /* High 10 / High 4:2:2 / High 4:4:4 Predictive at 10 bits: uint16_t
    * planes, the QpBdOffset domain, scaled deblocking thresholds and
    * weighted-prediction offsets, ten-bit I_PCM, the 16-bit kernel
    * instantiation (no SIMD) end to end. */
   printf("10-bit (uint16_t planes), byte-exact vs ffmpeg:\n");
   oracle_case("hb_i_cavlc",   "testsrc2=s=96x80:r=10",    3, "yuv420p10le", "-crf 18",
         "-preset medium -g 1 -x264-params cabac=0:deblock=2,2");
   oracle_case("hb_ipb_cabac", "testsrc2=s=96x80:r=10",    6, "yuv420p10le", "-crf 22",
         "-preset medium -x264-params deblock=1,1");
   oracle_case("hb_wp_bpyr",   "testsrc2=s=96x80:r=10,fade=in:0:6", 8, "yuv420p10le", "-crf 20",
         "-preset slow -x264-params weightp=2:bframes=3:b-pyramid=normal");
   oracle_case("hb_422",       "testsrc2=s=96x80:r=10",    6, "yuv422p10le", "-crf 20",
         "-preset medium");
   oracle_case("hb_444_ll",    "mandelbrot=s=112x96:r=10", 6, "yuv444p10le", "-qp 0",
         "-preset medium");
   oracle_case("hb_pcm",       "nullsrc=s=96x80:r=10,geq=lum='random(1)*1023':cb='random(2)*1023':cr='random(3)*1023'",
         2, "yuv420p10le", "-qp 0", "-preset medium -g 1");
   printf("High 4:4:4 Predictive, 4:4:4 chroma, byte-exact vs ffmpeg:\n");
   oracle_case("c444_i_cavlc",   "testsrc2=s=96x80:r=10",    4, "yuv444p", "-crf 16",
         "-preset medium -g 1 -x264-params cabac=0:deblock=2,2");
   oracle_case("c444_i_cabac8",  "mandelbrot=s=112x96:r=10", 3, "yuv444p", "-crf 16",
         "-preset medium -g 1");
   oracle_case("c444_ipb_cavlc", "testsrc2=s=96x80:r=10",    6, "yuv444p", "-crf 22",
         "-preset medium -x264-params cabac=0:no-8x8dct=1");
   oracle_case("c444_ipb_cabac", "mandelbrot=s=112x96:r=10", 6, "yuv444p", "-crf 22",
         "-preset medium -x264-params deblock=1,1");
   oracle_case("c444_wp_bpyr",   "testsrc2=s=96x80:r=10,fade=in:0:6", 8, "yuv444p", "-crf 20",
         "-preset slow -x264-params weightp=2:bframes=3:b-pyramid=normal:deblock=3,3");
   oracle_case("c444_lossless",  "mandelbrot=s=112x96:r=10", 6, "yuv444p", "-qp 0",
         "-preset medium");
   oracle_case("c444_ll_cavlc",  "mandelbrot=s=112x96:r=10", 3, "yuv444p", "-qp 0",
         "-preset medium -g 1 -x264-params cabac=0");

   run("rm -rf '%s'", dir);
   printf("rh264_lossless_test: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
         fails, fails == 1 ? "" : "s");
   return fails ? 1 : 0;
}
