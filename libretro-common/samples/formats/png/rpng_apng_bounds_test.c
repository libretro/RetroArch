/* Regression tests for libretro-common/formats/png/rpng_apng.c
 *
 * The APNG indexer validates each fcTL frame rectangle against the
 * canvas, and the canvas against the one IHDR, before anything is
 * composited.  Each file below is built in memory (stored deflate
 * blocks, so no compressor is needed) and must either decode or be
 * refused at open; a malformed file that opens is a failure on its
 * own, so the lane discriminates without a sanitizer as well as under
 * one.
 *
 * Subtests:
 *   1. A valid two-frame file decodes, and the second frame lands
 *      where its fcTL places it.
 *   2. fcTL x_off + width that wraps 32 bits is refused.
 *   3. fcTL y_off + height that wraps 32 bits is refused.
 *   4. A second IHDR after the frames were checked is refused.
 *   5. A second acTL is refused (the first frame table is kept, so
 *      LeakSanitizer sees nothing).
 *   6. Progressive: a second IHDR arriving through set_avail after the
 *      canvas was allocated leaves the canvas and the playable frames
 *      as they were.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <formats/rpng.h>
#include <encodings/crc32.h>

#define FILE_CAP (1 << 16)
#define PIX      0xAAu   /* every byte of every frame pixel */

static int failures = 0;

static uint8_t  file_buf[FILE_CAP];
static size_t   file_len;
static uint32_t file_seq;

static void put32be(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v >> 24);
   p[1] = (uint8_t)(v >> 16);
   p[2] = (uint8_t)(v >> 8);
   p[3] = (uint8_t)v;
}

static void put_chunk(const char *type, const uint8_t *data, uint32_t n)
{
   uint8_t *c = file_buf + file_len;
   put32be(c, n);
   memcpy(c + 4, type, 4);
   if (n)
      memcpy(c + 8, data, n);
   put32be(c + 8 + n, encoding_crc32(0, c + 4, n + 4));
   file_len += 12 + n;
}

static void begin(void)
{
   static const uint8_t sig[8] =
      { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
   memcpy(file_buf, sig, 8);
   file_len = 8;
   file_seq = 0;
}

static void put_ihdr(uint32_t w, uint32_t h)
{
   uint8_t d[13];
   put32be(d, w);
   put32be(d + 4, h);
   d[8]  = 8;   /* bit depth           */
   d[9]  = 6;   /* RGBA                */
   d[10] = 0;
   d[11] = 0;
   d[12] = 0;
   put_chunk("IHDR", d, 13);
}

static void put_actl(uint32_t frames)
{
   uint8_t d[8];
   put32be(d, frames);
   put32be(d + 4, 0);
   put_chunk("acTL", d, 8);
}

static void put_fctl(uint32_t w, uint32_t h, uint32_t x, uint32_t y)
{
   uint8_t d[26];
   memset(d, 0, sizeof(d));
   put32be(d,      file_seq++);
   put32be(d + 4,  w);
   put32be(d + 8,  h);
   put32be(d + 12, x);
   put32be(d + 16, y);
   d[21] = 1;   /* delay 1/10 s        */
   d[23] = 10;
   d[24] = 1;   /* dispose: background */
   d[25] = 0;   /* blend: source       */
   put_chunk("fcTL", d, 26);
}

/* A zlib stream of stored blocks: h rows of filter 0 then w RGBA
 * pixels, every byte PIX.  Returns its length. */
static uint32_t zlib_rows(uint8_t *z, uint32_t w, uint32_t h)
{
   uint32_t row    = 1 + 4 * w;
   uint32_t raw_n  = row * h;
   uint32_t a      = 1;
   uint32_t b      = 0;
   uint32_t zn     = 2;
   uint32_t done   = 0;
   uint32_t i;

   z[0] = 0x78;
   z[1] = 0x01;
   while (done < raw_n)
   {
      uint32_t n = raw_n - done;
      if (n > 65535)
         n = 65535;
      z[zn++] = (uint8_t)((done + n == raw_n) ? 1 : 0);
      z[zn++] = (uint8_t)n;
      z[zn++] = (uint8_t)(n >> 8);
      z[zn++] = (uint8_t)~n;
      z[zn++] = (uint8_t)(~n >> 8);
      for (i = 0; i < n; i++)
         z[zn + i] = (uint8_t)(((done + i) % row) ? PIX : 0);
      zn   += n;
      done += n;
   }
   for (i = 0; i < raw_n; i++)
   {
      a = (a + ((i % row) ? PIX : 0)) % 65521;
      b = (b + a) % 65521;
   }
   put32be(z + zn, (b << 16) | a);
   return zn + 4;
}

static void put_idat(uint32_t w, uint32_t h)
{
   static uint8_t z[FILE_CAP / 2];
   put_chunk("IDAT", z, zlib_rows(z, w, h));
}

static void put_fdat(uint32_t w, uint32_t h)
{
   static uint8_t z[FILE_CAP / 2];
   put32be(z, file_seq++);
   put_chunk("fdAT", z, 4 + zlib_rows(z + 4, w, h));
}

static void put_iend(void)
{
   put_chunk("IEND", NULL, 0);
}

static void check(int ok, const char *name, const char *what)
{
   if (!ok)
   {
      printf("[FAIL] %s: %s\n", name, what);
      failures++;
   }
}

/* Open a private copy of the file (the stream borrows the buffer) and
 * require it to be refused. */
static void expect_refused(const char *name)
{
   uint8_t *copy = (uint8_t*)malloc(file_len);
   rpng_apng_stream_t *s;
   memcpy(copy, file_buf, file_len);
   s = rpng_apng_stream_open(copy, file_len);
   check(s == NULL, name, "malformed file was opened");
   if (s)
      rpng_apng_stream_close(s);
   else
      printf("[ OK ] %s\n", name);
   free(copy);
}

static void test_valid(void)
{
   const char *name = "valid two-frame file";
   uint8_t *copy;
   rpng_apng_stream_t *s;
   const uint32_t *px;
   unsigned w = 0, h = 0;
   int frames = 0, loops = -1, dur = 0;
   int before = failures;

   begin();
   put_ihdr(16, 16);
   put_actl(2);
   put_fctl(16, 16, 0, 0);
   put_idat(16, 16);
   put_fctl(8, 8, 8, 8);
   put_fdat(8, 8);
   put_iend();

   copy = (uint8_t*)malloc(file_len);
   memcpy(copy, file_buf, file_len);
   s = rpng_apng_stream_open(copy, file_len);
   check(s != NULL, name, "open failed");
   if (s)
   {
      rpng_apng_stream_get_info(s, &w, &h, &frames, &loops);
      check(w == 16 && h == 16 && frames == 2, name, "wrong stream info");
      px = rpng_apng_stream_next(s, &dur);
      check(px && px[0] == 0xAAAAAAAAu, name, "frame 0 not decoded");
      /* Frame 0 is disposed to background, then the 8x8 frame 1 is
       * composited at (8,8): the top-left is clear, the bottom-right
       * quarter is frame 1. */
      px = rpng_apng_stream_next(s, &dur);
      check(px != NULL, name, "frame 1 not decoded");
      if (px)
      {
         check(px[0] == 0,                    name, "frame 0 not disposed");
         check(px[8 * 16 + 8]  == 0xAAAAAAAAu, name, "frame 1 misplaced");
         check(px[15 * 16 + 15] == 0xAAAAAAAAu, name, "frame 1 clipped");
         check(px[7 * 16 + 7]  == 0,           name, "frame 1 overdrawn");
      }
      rpng_apng_stream_close(s);
   }
   free(copy);
   if (failures == before)
      printf("[ OK ] %s\n", name);
}

static void test_x_wrap(void)
{
   /* 0xFFFFFFF8 + 16 wraps to 8, which fits a 16-wide canvas. */
   begin();
   put_ihdr(16, 16);
   put_actl(2);
   put_fctl(16, 16, 0, 0);
   put_idat(16, 16);
   put_fctl(16, 16, 0xFFFFFFF8u, 0);
   put_fdat(16, 16);
   put_iend();
   expect_refused("fcTL x_off + width wraps");
}

static void test_y_wrap(void)
{
   begin();
   put_ihdr(16, 16);
   put_actl(2);
   put_fctl(16, 16, 0, 0);
   put_idat(16, 16);
   put_fctl(16, 16, 0, 0xFFFFFFF8u);
   put_fdat(16, 16);
   put_iend();
   expect_refused("fcTL y_off + height wraps");
}

static void test_second_ihdr(void)
{
   /* The frame fits the first IHDR's 16x16; the canvas would be
    * allocated from the second one's 2x2. */
   begin();
   put_ihdr(16, 16);
   put_actl(1);
   put_fctl(16, 16, 0, 0);
   put_fdat(16, 16);
   put_ihdr(2, 2);
   put_iend();
   expect_refused("second IHDR");
}

static void test_second_actl(void)
{
   begin();
   put_ihdr(16, 16);
   put_actl(1);
   put_fctl(16, 16, 0, 0);
   put_idat(16, 16);
   put_actl(1);
   put_fctl(16, 16, 0, 0);
   put_fdat(16, 16);
   put_iend();
   expect_refused("second acTL");
}

static void test_progressive_second_ihdr(void)
{
   const char *name = "progressive second IHDR";
   uint8_t *copy;
   rpng_apng_stream_t *s;
   size_t cut;
   unsigned w = 0, h = 0;
   int frames = 0, loops = 0, dur = 0, need_more = 0, k;
   int before = failures;

   /* A 4x4 canvas and frame is resident first; then a 16x16 IHDR and a
    * 16x16 frame arrive behind the allocated canvas. */
   begin();
   put_ihdr(4, 4);
   put_actl(2);
   put_fctl(4, 4, 0, 0);
   put_fdat(4, 4);
   cut = file_len;
   put_ihdr(16, 16);
   put_fctl(16, 16, 0, 0);
   put_fdat(16, 16);
   put_iend();

   copy = (uint8_t*)malloc(file_len);
   memcpy(copy, file_buf, file_len);
   s = rpng_apng_stream_open_avail(copy, file_len, cut, &need_more);
   check(s != NULL, name, "progressive open failed");
   if (s)
   {
      rpng_apng_stream_set_avail(s, file_len);
      rpng_apng_stream_get_info(s, &w, &h, &frames, &loops);
      check(w == 4 && h == 4, name, "canvas changed after allocation");
      for (k = 0; k < 4; k++)
         if (!rpng_apng_stream_next(s, &dur))
            rpng_apng_stream_rewind(s);
      rpng_apng_stream_close(s);
   }
   free(copy);
   if (failures == before)
      printf("[ OK ] %s\n", name);
}

int main(void)
{
   test_valid();
   test_x_wrap();
   test_y_wrap();
   test_second_ihdr();
   test_second_actl();
   test_progressive_second_ihdr();

   if (failures)
   {
      printf("\n%d failure(s)\n", failures);
      return 1;
   }
   printf("\nAll rpng_apng bounds tests passed.\n");
   return 0;
}
