/* The video streams' linear scRGB output for HDR sources.
 *
 * Fixtures: 64x48 neutral grey at 10-bit luma 600, three frames each -
 * HEVC Main10 in MP4 and VP9 profile 2 in WebM, each tagged PQ (BT.2020,
 * transfer 16) and tagged SDR (BT.709). A flat frame comes through the
 * codecs to within a code or two, so a stream that is asked for half
 * floats and decodes into the caller's frame has to put the PQ EOTF of
 * that grey, over 80 nits, in every pixel, alpha 1.0. The SDR twins,
 * asked the same, keep their 32-bit paths; a stream decoding into its
 * own canvas keeps them too, whatever it is asked. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include <formats/image.h>
#include <formats/rmp4_video.h>
#include <formats/rwebm_video.h>
#include <streams/file_stream.h>

#define W 64
#define H 48
#define GREY_Y 600

static unsigned failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; \
   fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
   fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

static double half_to_double(uint16_t h)
{
   int      e = (h >> 10) & 0x1f;
   unsigned m = h & 0x3ff;
   double   v = e ? ldexp(1.0 + m / 1024.0, e - 15) : ldexp(m / 1024.0, -14);
   return (h & 0x8000) ? -v : v;
}

static double pq_scrgb(int code)
{
   const double m1 = 0.1593017578125, m2 = 78.84375;
   const double c1 = 0.8359375, c2 = 18.8515625, c3 = 18.6875;
   double e   = code / 1023.0;
   double ep  = pow(e, 1.0 / m2);
   double num = ep - c1 > 0.0 ? ep - c1 : 0.0;
   return 10000.0 * pow(num / (c2 - c3 * ep), 1.0 / m1) / 80.0;
}

static void *load(const char *path, size_t *len)
{
   void   *buf = NULL;
   int64_t n   = 0;
   if (!filestream_read_file(path, &buf, &n) || n <= 0)
   {
      fprintf(stderr, "cannot read %s (run from the sample directory)\n",
            path);
      exit(2);
   }
   *len = (size_t)n;
   return buf;
}

typedef struct
{
   const char *path;
   enum image_type_enum type;
   int hdr;
} fixture_t;

static void *stream_open(const fixture_t *f, const void *buf, size_t len)
{
   return f->type == IMAGE_TYPE_MP4
      ? (void*)rmp4_video_stream_open((const uint8_t*)buf, len)
      : (void*)rwebm_video_stream_open((const uint8_t*)buf, len);
}

static void stream_close(const fixture_t *f, void *s)
{
   if (f->type == IMAGE_TYPE_MP4)
      rmp4_video_stream_close((rmp4_video_stream_t*)s);
   else
      rwebm_video_stream_close((rwebm_video_stream_t*)s);
}

static const uint32_t *stream_next(const fixture_t *f, void *s)
{
   int d = 0;
   return f->type == IMAGE_TYPE_MP4
      ? rmp4_video_stream_next((rmp4_video_stream_t*)s, &d)
      : rwebm_video_stream_next((rwebm_video_stream_t*)s, &d);
}

static void lane(const fixture_t *f)
{
   size_t    len;
   void     *buf = load(f->path, &len);
   uint16_t *out = (uint16_t*)calloc((size_t)W * H, 8);
   const uint32_t *frame;
   void     *s;
   unsigned  had = failures, k, i;
   double    worst = 0.0, ref = pq_scrgb(
         (298 * (GREY_Y - 64) + 128) >> 8);

   /* Into the caller's frame, asked for half floats. */
   s = stream_open(f, buf, len);
   CHECK(s != NULL, "%s: no stream", f->path);
   if (!s)
      goto done;
   CHECK(image_transfer_anim_stream_is_hdr(s, f->type) == (f->hdr != 0),
         "%s: is_hdr says %d", f->path,
         image_transfer_anim_stream_is_hdr(s, f->type));
   image_transfer_anim_stream_set_want_fp16(s, f->type, true);
   CHECK(image_transfer_anim_stream_set_output(s, f->type, (uint32_t*)out),
         "%s: no output path", f->path);
   for (k = 0; k < 3; k++)
   {
      frame = stream_next(f, s);
      CHECK(frame == (const uint32_t*)out, "%s: frame %u not in the "
            "caller's buffer", f->path, k);
      CHECK(image_transfer_anim_stream_is_fp16(s, f->type) == (f->hdr != 0),
            "%s: frame %u is_fp16 says %d", f->path, k,
            image_transfer_anim_stream_is_fp16(s, f->type));
      if (f->hdr && frame)
         for (i = 0; i < (unsigned)(W * H); i += 37)
         {
            int c;
            for (c = 0; c < 3; c++)
            {
               double e = fabs(half_to_double(out[i * 4 + c]) - ref) / ref;
               if (e > worst)
                  worst = e;
            }
            CHECK(out[i * 4 + 3] == 0x3c00, "%s: alpha not 1.0", f->path);
         }
   }
   if (f->hdr)
      CHECK(worst < 0.04, "%s: grey off the PQ EOTF by %.4f relative "
            "(expected %.4f scRGB)", f->path, worst, ref);
   stream_close(f, s);

   /* Into the stream's own canvas: asked, but nowhere to put 8 bytes a
    * pixel, so the 32-bit path. */
   s = stream_open(f, buf, len);
   if (s)
   {
      image_transfer_anim_stream_set_want_fp16(s, f->type, true);
      frame = stream_next(f, s);
      CHECK(frame && !image_transfer_anim_stream_is_fp16(s, f->type),
            "%s: half floats written into the stream's own canvas",
            f->path);
      stream_close(f, s);
   }
done:
   if (failures == had)
      printf("[ok]   %s: %s\n", f->path, f->hdr
            ? "linear scRGB into the caller's frame, on the PQ EOTF"
            : "SDR keeps its 32-bit path when asked");
   free(out);
   free(buf);
}

/* The still a thumbnail is first shown from: image_transfer_process
 * of a video, asked for half floats or not, as task_image drives it. */
static void still_lane(const fixture_t *f, int want)
{
   size_t    len;
   void     *buf = load(f->path, &len);
   void     *h   = image_transfer_new(f->type);
   uint32_t *px  = NULL;
   unsigned  w = 0, hh = 0, had = failures;
   int       r, guard = 0;
   double    ref = pq_scrgb((298 * (GREY_Y - 64) + 128) >> 8);
   int       expect = want && f->hdr;

   CHECK(h != NULL, "%s: no transfer", f->path);
   if (!h)
   {
      free(buf);
      return;
   }
   image_transfer_set_buffer_ptr(h, f->type, buf, len);
   image_transfer_set_want_fp16(h, f->type, want ? true : false);
   CHECK(image_transfer_start(h, f->type), "%s: start failed", f->path);
   while (image_transfer_iterate(h, f->type) && guard++ < 100000)
      ;
   guard = 0;
   do
      r = image_transfer_process(h, f->type, &px, len, &w, &hh, true);
   while (r == IMAGE_PROCESS_NEXT && guard++ < 100000);
   CHECK(r == IMAGE_PROCESS_END && px && w == W && hh == H,
         "%s: still not decoded (%d, %ux%u)", f->path, r, w, hh);
   CHECK(image_transfer_is_fp16(h, f->type) == (expect != 0),
         "%s: still is_fp16 says %d, asked %d", f->path,
         image_transfer_is_fp16(h, f->type), want);
   if (px && expect)
   {
      const uint16_t *p = (const uint16_t*)px
         + ((size_t)(H / 2) * W + W / 2) * 4;
      double e = fabs(half_to_double(p[0]) - ref) / ref;
      CHECK(e < 0.04 && p[3] == 0x3c00, "%s: still off the PQ EOTF by "
            "%.4f relative", f->path, e);
   }
   else if (px)
      /* 32-bit words: a half-float 1.0 alpha (0x3c00) in the top bits
       * would mean half floats where none were asked for. */
      CHECK((px[0] >> 24) == 0xff, "%s: still is not 32-bit words "
            "(0x%08x)", f->path, (unsigned)px[0]);
   free(px);
   image_transfer_free(h, f->type);
   free(buf);
   if (failures == had)
      printf("[ok]   %s still, %s: %s\n", f->path,
            want ? "asked" : "not asked",
            expect ? "half floats on the PQ EOTF" : "32-bit words");
}

int main(void)
{
   static const fixture_t fx[] = {
      { "fixtures/hdr_hevc.mp4", IMAGE_TYPE_MP4,  1 },
      { "fixtures/sdr_hevc.mp4", IMAGE_TYPE_MP4,  0 },
      { "fixtures/hdr_vp9.webm", IMAGE_TYPE_WEBM, 1 },
      { "fixtures/sdr_vp9.webm", IMAGE_TYPE_WEBM, 0 }
   };
   unsigned i;
   for (i = 0; i < sizeof(fx) / sizeof(fx[0]); i++)
   {
      lane(&fx[i]);
      still_lane(&fx[i], 1);
      still_lane(&fx[i], 0);
   }
   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] video_hdr_fp16_test\n");
   return 0;
}
