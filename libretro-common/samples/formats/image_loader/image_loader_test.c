/* The still loader, driven the ways its callers drive it:
 *
 *   1  a whole buffer in one untimed step decodes to what
 *      image_texture_load_buffer gives, pixel for pixel
 *   2  timed steps against a fake clock: a deadline already passed
 *      still runs one iteration, and the step that completes the
 *      transfer does not go on into the pixels
 *   3  a prefix that grows: the loader waits at the frontier and
 *      finishes once the rest has arrived, with the same pixels
 *   4  an abort hook that fires fails the decode and leaks nothing
 *   5  the APNG verdict: a still, an animation, an incomplete read
 *   6  the request's channel order is answered and honoured
 *   7  image_texture_scale: the cap, the whole-factor upscale, and
 *      a half-float image left alone
 *
 * The PNG is built here - RGB, every row filter 0, the zlib stream
 * stored and split across many small IDATs so the transfer has many
 * iterations to be budgeted. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <encodings/crc32.h>
#include <formats/image.h>

#define W 24
#define H 16
#define IDAT_SPLIT 64

static int fails;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL: %s\n", msg); \
         fails++; \
      } \
      else \
         printf("ok:   %s\n", msg); \
   } while (0)

static void put_be32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v >> 24);
   p[1] = (uint8_t)(v >> 16);
   p[2] = (uint8_t)(v >>  8);
   p[3] = (uint8_t)(v      );
}

static size_t png_chunk(uint8_t *out, const char *type,
      const uint8_t *data, size_t len)
{
   uint32_t c;
   put_be32(out, (uint32_t)len);
   memcpy(out + 4, type, 4);
   if (len)
      memcpy(out + 8, data, len);
   c = encoding_crc32(0, out + 4, 4);
   if (len)
      c = encoding_crc32(c, data, len);
   put_be32(out + 8 + len, c);
   return 12 + len;
}

/* R = x, G = y, B = x ^ y */
static uint32_t expect_argb(unsigned x, unsigned y)
{
   return 0xff000000u | ((x & 0xff) << 16) | ((y & 0xff) << 8) | ((x ^ y) & 0xff);
}

static uint32_t expect_rgba(unsigned x, unsigned y)
{
   uint32_t v;
   uint8_t b[4];
   b[0] = (uint8_t)x;
   b[1] = (uint8_t)y;
   b[2] = (uint8_t)(x ^ y);
   b[3] = 0xff;
   memcpy(&v, b, 4);
   return v;
}

static uint8_t *make_png(size_t *out_len, bool apng)
{
   static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
   size_t raw_len = (size_t)H * (W * 3 + 1);
   uint8_t *raw   = (uint8_t*)malloc(raw_len);
   size_t zcap    = raw_len + (raw_len / 65535 + 2) * 5 + 8;
   uint8_t *z     = (uint8_t*)malloc(zcap);
   uint8_t *png   = (uint8_t*)malloc(zcap + 4096 + (zcap / IDAT_SPLIT + 2) * 12);
   size_t zlen = 0, off, len = 0;
   uint32_t s1 = 1, s2 = 0;
   unsigned x, y;

   if (!raw || !z || !png)
      exit(1);
   for (y = 0; y < H; y++)
   {
      uint8_t *row = raw + y * (W * 3 + 1);
      row[0] = 0;
      for (x = 0; x < W; x++)
      {
         row[1 + x * 3] = (uint8_t)x;
         row[2 + x * 3] = (uint8_t)y;
         row[3 + x * 3] = (uint8_t)(x ^ y);
      }
   }
   z[zlen++] = 0x78;
   z[zlen++] = 0x01;
   for (off = 0; off < raw_len; )
   {
      size_t blk = raw_len - off;
      if (blk > 65535)
         blk = 65535;
      z[zlen++] = (off + blk >= raw_len) ? 0x01 : 0x00;
      z[zlen++] = (uint8_t)(blk & 0xff);
      z[zlen++] = (uint8_t)(blk >> 8);
      z[zlen++] = (uint8_t)(~blk & 0xff);
      z[zlen++] = (uint8_t)(~(blk >> 8) & 0xff);
      memcpy(z + zlen, raw + off, blk);
      zlen += blk;
      off  += blk;
   }
   for (off = 0; off < raw_len; off++)
   {
      s1 = (s1 + raw[off]) % 65521;
      s2 = (s2 + s1)       % 65521;
   }
   put_be32(z + zlen, (s2 << 16) | s1);
   zlen += 4;

   memcpy(png, sig, 8);
   len = 8;
   {
      uint8_t ihdr[13];
      put_be32(ihdr,     W);
      put_be32(ihdr + 4, H);
      ihdr[8]  = 8;
      ihdr[9]  = 2;   /* RGB */
      ihdr[10] = 0;
      ihdr[11] = 0;
      ihdr[12] = 0;
      len += png_chunk(png + len, "IHDR", ihdr, 13);
   }
   if (apng)
   {
      uint8_t actl[8];
      put_be32(actl,     1);   /* frames */
      put_be32(actl + 4, 0);   /* plays  */
      len += png_chunk(png + len, "acTL", actl, 8);
   }
   for (off = 0; off < zlen; off += IDAT_SPLIT)
   {
      size_t c = zlen - off;
      if (c > IDAT_SPLIT)
         c = IDAT_SPLIT;
      len += png_chunk(png + len, "IDAT", z + off, c);
   }
   len += png_chunk(png + len, "IEND", NULL, 0);
   free(raw);
   free(z);
   *out_len = len;
   return png;
}

static int pixels_match(const struct texture_image *img, bool rgba)
{
   unsigned x, y;
   if (!img->pixels || img->width != W || img->height != H)
      return 0;
   for (y = 0; y < H; y++)
      for (x = 0; x < W; x++)
         if (img->pixels[y * W + x] != (rgba ? expect_rgba(x, y) : expect_argb(x, y)))
            return 0;
   return 1;
}

/* Every read advances the fake clock one quantum */
static int64_t fake_now_v = 1000000;
static int64_t fake_now(void)
{
   fake_now_v += 250;
   return fake_now_v;
}

static int aborts;
static bool abort_now(void *ud)
{
   (void)ud;
   aborts++;
   return true;
}

int main(void)
{
   size_t len = 0, apng_len = 0;
   uint8_t *png  = make_png(&len, false);
   uint8_t *apng = make_png(&apng_len, true);
   struct texture_image ref, img;
   image_texture_request_t req;
   image_loader_t *l;

   memset(&ref, 0, sizeof(ref));
   memset(&req, 0, sizeof(req));
   CHECK(image_texture_load_buffer(&ref, IMAGE_TYPE_PNG, png, len)
         && pixels_match(&ref, false), "reference decode through image_texture_load_buffer");

   /* 1: whole buffer, one untimed step */
   memset(&img, 0, sizeof(img));
   l = image_loader_new(IMAGE_TYPE_PNG, &req);
   CHECK(l && image_loader_start(l, png, len, len), "loader started on a whole buffer");
   CHECK(image_loader_step(l, NULL, 0) == IMAGE_LOADER_DONE, "untimed step runs to DONE");
   CHECK(image_loader_finish(l, &img) && pixels_match(&img, false)
         && !img.pix10 && !img.fp16 && !img.supports_rgba,
         "finished image equals the reference, format fields clear");
   image_loader_free(l);
   image_texture_free(&img);

   /* 2: timed steps */
   {
      int steps = 0, transfer_steps = 0, first_pixels_step = -1;
      enum image_loader_state st = IMAGE_LOADER_RUNNING;
      memset(&img, 0, sizeof(img));
      l = image_loader_new(IMAGE_TYPE_PNG, &req);
      image_loader_start(l, png, len, len);
      /* a deadline already behind the clock: one iteration a step, so
       * the transfer takes at least as many steps as there are IDATs */
      while (st == IMAGE_LOADER_RUNNING && steps < 10000)
      {
         st = image_loader_step(l, fake_now, fake_now_v);
         steps++;
      }
      CHECK(st == IMAGE_LOADER_DONE, "timed steps reach DONE");
      CHECK(steps > (int)(len / IDAT_SPLIT), "a spent deadline still runs one iteration a step");
      CHECK(image_loader_finish(l, &img) && pixels_match(&img, false),
            "timed decode equals the reference");
      image_loader_free(l);
      image_texture_free(&img);

      /* a generous deadline: the transfer completes in one step and
       * that step stops there; the pixels come from the next */
      memset(&img, 0, sizeof(img));
      l = image_loader_new(IMAGE_TYPE_PNG, &req);
      image_loader_start(l, png, len, len);
      steps = 0;
      st    = IMAGE_LOADER_RUNNING;
      while (st == IMAGE_LOADER_RUNNING && steps < 100)
      {
         st = image_loader_step(l, fake_now, fake_now_v + 100000000);
         steps++;
         if (st == IMAGE_LOADER_RUNNING)
            transfer_steps++;
         else if (first_pixels_step < 0)
            first_pixels_step = steps;
      }
      CHECK(st == IMAGE_LOADER_DONE && transfer_steps == 1 && first_pixels_step == 2,
            "the step that completes the transfer ends there; the next brings the pixels");
      CHECK(image_loader_finish(l, &img) && pixels_match(&img, false),
            "two-step decode equals the reference");
      image_loader_free(l);
      image_texture_free(&img);
   }

   /* 3: a prefix that grows */
   {
      size_t avail = 8 + 25 + 40;   /* signature, IHDR, into the first IDAT */
      enum image_loader_state st;
      memset(&img, 0, sizeof(img));
      CHECK(image_loader_ready(IMAGE_TYPE_PNG, png, avail), "a PNG may start once its header is resident");
      CHECK(!image_loader_ready(IMAGE_TYPE_PNG, png, 8), "and not before");
      l = image_loader_new(IMAGE_TYPE_PNG, &req);
      CHECK(image_loader_start(l, png, len, avail), "loader started on a prefix");
      st = image_loader_step(l, NULL, 0);
      CHECK(st == IMAGE_LOADER_WAIT, "an untimed step waits at the frontier");
      CHECK(image_loader_png_probe(l) == -1, "no APNG verdict over a partial read");
      image_loader_set_avail(l, len / 2);
      st = image_loader_step(l, NULL, 0);
      CHECK(st == IMAGE_LOADER_WAIT, "waits again at the next frontier");
      image_loader_set_avail(l, (size_t)-1);
      st = image_loader_step(l, NULL, 0);
      CHECK(st == IMAGE_LOADER_DONE, "finishes once the rest has arrived");
      CHECK(image_loader_png_probe(l) == 0, "a complete still PNG is no animation");
      CHECK(image_loader_finish(l, &img) && pixels_match(&img, false),
            "prefix decode equals the reference");
      image_loader_free(l);
      image_texture_free(&img);
   }

   /* 4: abort */
   l = image_loader_new(IMAGE_TYPE_PNG, &req);
   image_loader_start(l, png, len, len);
   image_loader_set_abort(l, abort_now, NULL);
   CHECK(image_loader_step(l, NULL, 0) == IMAGE_LOADER_ERROR && aborts == 1,
         "an abort hook fails the decode at its first ask");
   CHECK(!image_loader_finish(l, &img), "nothing to finish after an abort");
   image_loader_free(l);

   /* 5: APNG */
   l = image_loader_new(IMAGE_TYPE_PNG, &req);
   image_loader_start(l, apng, apng_len, apng_len);
   CHECK(image_loader_png_probe(l) == 1, "an acTL makes the verdict animated");
   image_loader_free(l);

   /* 6: channel order */
   memset(&img, 0, sizeof(img));
   req.rgba = true;
   l = image_loader_new(IMAGE_TYPE_PNG, &req);
   image_loader_start(l, png, len, len);
   image_loader_step(l, NULL, 0);
   CHECK(image_loader_finish(l, &img) && img.supports_rgba && pixels_match(&img, true),
         "rgba requested: answered and honoured");
   image_loader_free(l);
   image_texture_free(&img);
   req.rgba = false;

   /* 7: scaling */
   {
      struct texture_image s;
      unsigned i;
      memset(&s, 0, sizeof(s));
      s.width  = 8;
      s.height = 8;
      s.pixels = (uint32_t*)malloc(64 * sizeof(uint32_t));
      for (i = 0; i < 64; i++)
         s.pixels[i] = 0xff336699u;
      CHECK(image_texture_scale(&s, 0, 4) && s.width == 4 && s.height == 4
            && s.pixels[0] == 0xff336699u && s.pixels[15] == 0xff336699u,
            "the cap reduces 8x8 to 4x4 and a flat image stays flat");
      CHECK(image_texture_scale(&s, 8, 0) && s.width == 8 && s.height == 8
            && s.pixels[63] == 0xff336699u,
            "an edge under the threshold is upscaled by a whole factor");
      CHECK(!image_texture_scale(&s, 0, 16), "within the cap: untouched");
      s.fp16 = true;
      CHECK(!image_texture_scale(&s, 64, 4), "half floats are left at their size");
      free(s.pixels);
   }

   image_texture_free(&ref);
   free(png);
   free(apng);
   printf("%s\n", fails ? "FAILED" : "PASS");
   return fails ? 1 : 0;
}
