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
 * Also checks direct canvas output, disposal, blending, rewind, channel
 * order and failed-decode ownership without a temporary SOURCE image.
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

/* Inspect output ownership and count compositor writes to the canvas.
 * rpng itself stays separately compiled and writes decoded pixels normally. */
static uintptr_t watched_begin, watched_end;
static size_t canvas_copy_bytes, canvas_clear_bytes;
static void *watch_copy(void *dst, const void *src, size_t n)
{
   if ((uintptr_t)dst >= watched_begin && (uintptr_t)dst < watched_end)
      canvas_copy_bytes += n;
   return memcpy(dst, src, n);
}
static void *watch_clear(void *dst, int value, size_t n)
{
   if ((uintptr_t)dst >= watched_begin && (uintptr_t)dst < watched_end)
      canvas_clear_bytes += n;
   return memset(dst, value, n);
}
#define memcpy watch_copy
#define memset watch_clear
#include "../../../formats/png/rpng_apng.c"
#undef memcpy
#undef memset

#define FILE_CAP (1 << 16)
#define PIX      0xAAu   /* every byte of every frame pixel */

static int failures = 0;
static uint8_t pixel[4] = { PIX, PIX, PIX, PIX };
static unsigned disposal = 1, blending = 0;

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
   memset(pixel, PIX, sizeof(pixel));
   disposal = 1;
   blending = 0;
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
   d[24] = (uint8_t)disposal;   /* dispose: background */
   d[25] = (uint8_t)blending;   /* blend: source       */
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
         z[zn + i] = (uint8_t)(((done + i) % row) ? pixel[(((done + i) % row) - 1) % 4] : 0);
      zn   += n;
      done += n;
   }
   for (i = 0; i < raw_n; i++)
   {
      a = (a + ((i % row) ? pixel[((i % row) - 1) % 4] : 0)) % 65521;
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
      check(!s->pool.buf[RPNG_POOL_OUT], name, "SOURCE allocated a temporary image");
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

/* Independent byte-channel reference: offset replacement, all disposal
 * modes, transparent SOURCE, OVER, both output orders and rewind. */
static void test_canvas_output(void)
{
   static const unsigned rect[5][4] = {
      {5,4,0,0}, {2,2,1,1}, {1,1,4,3}, {2,1,0,2}, {5,4,0,0}
   };
   static const uint8_t colors[5][4] = {
      {10,40,90,255}, {220,30,60,128}, {70,80,90,0},
      {240,110,20,128}, {1,2,3,0}
   };
   static const unsigned disposals[5] = {0,2,1,0,0};
   const char *name = "direct canvas output";
   unsigned i, x, y, c, order, pass;
   size_t prefix = 0;
   int before = failures;
   begin();
   put_ihdr(5, 4);
   put_actl(5);
   for (i = 0; i < 5; i++)
   {
      disposal = disposals[i];
      blending = i == 3;
      memcpy(pixel, colors[i], 4);
      put_fctl(rect[i][0], rect[i][1], rect[i][2], rect[i][3]);
      if (i) put_fdat(rect[i][0], rect[i][1]);
      else put_idat(5, 4);
      if (!i) prefix = file_len;
   }
   put_iend();
   for (order = 0; order < 2; order++)
   {
      int need_more = 0, dur = 0;
      rpng_apng_stream_t *stream = rpng_apng_stream_open_avail(
            file_buf, file_len, prefix, &need_more);
      check(stream != NULL, name, "progressive open failed");
      if (!stream) continue;
      rpng_apng_stream_set_argb(stream, (int)order);
      for (pass = 0; pass < 2; pass++)
      {
         uint8_t expected[4][5][4], saved[4][5][4];
         memset(expected, 0, sizeof(expected));
         memset(saved, 0, sizeof(saved));
         for (i = 0; i < 5; i++)
         {
            const uint32_t *actual;
            if (i || pass) rpng_apng_stream_set_avail(stream, file_len);
            if (i && disposals[i-1] == 2)
               memcpy(expected, saved, sizeof(expected));
            else if (i && disposals[i-1] == 1)
               for (y = 0; y < rect[i-1][1]; y++)
                  for (x = 0; x < rect[i-1][0]; x++)
                     memset(expected[y+rect[i-1][3]][x+rect[i-1][2]], 0, 4);
            if (disposals[i] == 2) memcpy(saved, expected, sizeof(saved));
            for (y = 0; y < rect[i][1]; y++)
               for (x = 0; x < rect[i][0]; x++)
               {
                  uint8_t *dst = expected[y+rect[i][3]][x+rect[i][2]];
                  if (i != 3) memcpy(dst, colors[i], 4);
                  else
                  {
                     unsigned alpha = colors[i][3];
                     for (c = 0; c < 3; c++)
                        dst[c] = (uint8_t)((colors[i][c]*alpha
                              + dst[c]*(255-alpha) + 127)/255);
                     dst[3] = (uint8_t)(alpha + (dst[3]*(255-alpha)+127)/255);
                  }
               }
            actual = rpng_apng_stream_next(stream, &dur);
            check(actual != NULL && dur == 100, name, "frame/duration lost");
            if (!actual) break;
            for (y = 0; y < 4; y++)
               for (x = 0; x < 5; x++)
               {
                  const uint8_t *p = expected[y][x];
                  uint32_t word = ((uint32_t)p[3]<<24) | ((uint32_t)p[1]<<8);
                  word |= order ? ((uint32_t)p[0]<<16) | p[2]
                                : ((uint32_t)p[2]<<16) | p[0];
                  check(actual[y*5+x] == word, name, "canvas pixel mismatch");
               }
            if (!pass && i < 3)
               check(!stream->pool.buf[RPNG_POOL_OUT], name,
                     "SOURCE allocated a temporary image");
            if (i == 3)
               check(stream->pool.buf[RPNG_POOL_OUT] != NULL, name,
                     "OVER did not retain its separate blend input");
         }
         check(rpng_apng_stream_next(stream, &dur) == NULL, name, "missing EOF");
         rpng_apng_stream_rewind(stream);
      }
      rpng_apng_stream_close(stream);
   }
   if (before == failures) printf("[ OK ] %s\n", name);
}

/* A rejected decode must never put a borrowed canvas rectangle into
 * the output pool or free it as though it were a temporary image. */
static void test_bad_canvas_decode(void)
{
   const char *name = "failed canvas decode ownership";
   unsigned mode;
   int before = failures;
   for (mode = 0; mode < 2; mode++)
   {
      size_t chunk;
      int dur;
      rpng_apng_stream_t *stream;
      begin();
      put_ihdr(5, 4);
      put_actl(1);
      blending = mode;
      put_fctl(5, 4, 0, 0);
      chunk = file_len;
      put_idat(5, 4);
      /* Reserved deflate block type after the two-byte zlib header. */
      file_buf[chunk+10] = 7;
      put_iend();
      stream = rpng_apng_stream_open(file_buf, file_len);
      check(stream != NULL, name, "open failed");
      if (!stream) continue;
      check(rpng_apng_stream_next(stream, &dur) == NULL, name,
            "invalid deflate accepted");
      rpng_apng_stream_rewind(stream);
      rpng_apng_stream_close(stream);
   }
   if (before == failures) printf("[ OK ] %s\n", name);
}

/* Compare the optimized stream with one whose disposal is explicitly
 * applied before each decode. Count only writes to the optimized canvas,
 * not decoder stores or reference work. */
static void test_disposal_overlap(void)
{
   static const unsigned target[11][4] = {
      {2,2,2,1}, {4,4,1,0}, {2,2,3,1},
      {2,2,2,1}, {2,2,2,1}, {1,1,6,4},
      {1,1,2,1}, {4,1,1,1}, {1,4,2,0}, {2,2,2,2}, {2,1,2,2}
   };
   const char *name = "covered disposal writes";
   unsigned mode, scenario, i;
   int before = failures;
   for (mode = 1; mode <= 2; mode++)
      for (scenario = 0; scenario < 11; scenario++)
      {
         rpng_apng_stream_t *stream, *reference;
         unsigned px = scenario == 10 ? 1 : 2;
         unsigned pw = scenario == 10 ? 4 : 2;
         unsigned ph = scenario == 10 ? 3 : 2;
         begin();
         put_ihdr(7, 5);
         put_actl(4);
         for (i = 0; i < 4; i++)
         {
            unsigned w = 7, h = 5, x = 0, y = 0;
            disposal = 0;
            blending = 0;
            pixel[0] = (uint8_t)(20 + i*50);
            pixel[1] = (uint8_t)(200 - i*30);
            pixel[2] = 90;
            pixel[3] = i == 2 ? 0 : 128;
            if (i == 1) { w=pw; h=ph; x=px; y=1; disposal=mode; }
            if (i == 2)
            {
               w=target[scenario][0]; h=target[scenario][1];
               x=target[scenario][2]; y=target[scenario][3];
               disposal = scenario == 3 ? 2 : 0;
               blending = scenario == 4;
            }
            if (i == 3) { w=1; h=1; x=0; y=0; blending=1; }
            put_fctl(w, h, x, y);
            if (i) put_fdat(w, h);
            else put_idat(w, h);
         }
         put_iend();
         stream = rpng_apng_stream_open(file_buf, file_len);
         reference = rpng_apng_stream_open(file_buf, file_len);
         check(stream && reference, name, "open failed");
         if (stream && reference)
            for (i = 0; i < 4; i++)
            {
               const uint32_t *actual, *expected;
               int dur;
               apng_apply_prev_dispose(reference, NULL);
               reference->have_prev = 0;
               expected = rpng_apng_stream_next(reference, &dur);
               watched_begin = (uintptr_t)stream->canvas;
               watched_end = watched_begin + 7*5*sizeof(uint32_t);
               canvas_copy_bytes = canvas_clear_bytes = 0;
               actual = rpng_apng_stream_next(stream, &dur);
               watched_begin = watched_end = 0;
               check(actual && expected, name, "frame decode failed");
               if (actual && expected)
                  check(!memcmp(actual, expected, 7*5*sizeof(uint32_t)),
                        name, "disposal changed pixels");
               if (i == 2)
               {
                  unsigned x, y, pixels = 0;
                  size_t bytes;
                  for (y = 1; y < 1+ph; y++)
                     for (x = px; x < px+pw; x++)
                        if (scenario == 3 || scenario == 4
                              || x < target[scenario][2]
                              || x >= target[scenario][2] + target[scenario][0]
                              || y < target[scenario][3]
                              || y >= target[scenario][3] + target[scenario][1])
                           pixels++;
                  bytes = pixels * sizeof(uint32_t);
                  check(canvas_copy_bytes == (mode == 2 ? bytes : 0),
                        name, "unexpected restore copy");
                  check(canvas_clear_bytes == (mode == 1 ? bytes : 0),
                        name, "unexpected background clear");
               }
            }
         rpng_apng_stream_close(stream);
         rpng_apng_stream_close(reference);
      }
   if (before == failures) printf("[ OK ] %s\n", name);
}

int main(void)
{
   test_valid();
   test_canvas_output();
   test_disposal_overlap();
   test_bad_canvas_decode();
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
