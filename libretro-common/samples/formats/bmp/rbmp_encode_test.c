/* Regression tests for the pure BMP encoder split
 * (libretro-common/formats/bmp/rbmp_encode.c + file/rbmp_file.c):
 *
 *   - rbmp_row_size() / rbmp_encode_row(): the public row primitives
 *     the stream writer is built on - sizing, pass-through vs scratch
 *     conversion, padding, argument validation.
 *
 *   - rbmp_save_image_string(): exact-size buffer encode for every
 *     source type, positive and negative pitch, unaligned widths
 *     (row padding), header field sanity.
 *   - rbmp_save_image_stream(): byte-identical output to the string
 *     encoder through a memory intfstream.
 *   - rbmp_save_image(): the deprecated path adapter writes a file
 *     byte-identical to the string encoder's buffer.
 *   - Round-trips through the in-tree decoder (rbmp_process_image)
 *     with pixel-exact comparison, including the decoder's bottom-up
 *     flip and the encoder's RGB565 -> 888 expansion.
 *
 * Build (see Makefile):
 *   make -C libretro-common/samples/formats/bmp rbmp_encode_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libretro.h>
#include <formats/rbmp.h>
#include <formats/image.h>
#include <streams/interface_stream.h>
#include <streams/file_stream.h>

static int failures = 0;

#define CHECK(cond, name) \
   do { \
      if (cond) \
         printf("[SUCCESS] %s\n", name); \
      else \
      { \
         printf("[ERROR] %s (line %d)\n", name, __LINE__); \
         failures++; \
      } \
   } while (0)

static uint32_t prng_state = 0x12345678u;
static uint32_t prng(void)
{
   prng_state ^= prng_state << 13;
   prng_state ^= prng_state >> 17;
   prng_state ^= prng_state << 5;
   return prng_state;
}

/* Decode a BMP held in memory with the in-tree decoder.  Returns the
 * malloc'd ARGB pixel array (top-down) or NULL. */
static uint32_t *decode_bmp(const uint8_t *bmp, size_t _len,
      unsigned *w, unsigned *h)
{
   void *out    = NULL;
   rbmp_t *rbmp = rbmp_alloc();

   if (!rbmp)
      return NULL;

   rbmp_set_buf_ptr(rbmp, (void*)bmp);
   {
      /* rbmp_process_image decodes incrementally; pump it. */
      int rc;
      do
      {
         rc = rbmp_process_image(rbmp, &out, _len, w, h, false);
      } while (rc == IMAGE_PROCESS_NEXT);
      if (rc != IMAGE_PROCESS_END)
      {
         free(out);
         out = NULL;
      }
   }
   rbmp_free(rbmp);
   return (uint32_t*)out;
}

/* Encode the same frame through the stream API into a memory stream
 * and require byte identity with the string encoder's output. */
static void check_stream_matches_string(const uint8_t *golden,
      size_t golden_len, const void *frame,
      unsigned width, unsigned height, unsigned pitch,
      enum rbmp_source_type type, const char *name)
{
   bool ok              = false;
   uint8_t *buf         = (uint8_t*)malloc(golden_len);
   intfstream_t *intf_s = NULL;

   if (buf)
   {
      intf_s = intfstream_open_memory(buf,
            RETRO_VFS_FILE_ACCESS_WRITE,
            RETRO_VFS_FILE_ACCESS_HINT_NONE,
            golden_len);
      if (intf_s)
      {
         ok = rbmp_save_image_stream(intf_s, frame,
               width, height, pitch, type);
         ok = ok && (intfstream_get_ptr(intf_s) == (int64_t)golden_len);
         ok = ok && !memcmp(buf, golden, golden_len);
         intfstream_close(intf_s);
         free(intf_s);
      }
      free(buf);
   }
   CHECK(ok, name);
}

static void test_xrgb888_roundtrip(void)
{
   unsigned w = 31, h = 17; /* unaligned width: 3-byte row padding */
   unsigned dw = 0, dh = 0;
   size_t _len = 0;
   size_t i, j;
   int ok;
   uint32_t *frame = (uint32_t*)malloc((size_t)w * h * 4);
   uint8_t *bmp    = NULL;
   uint32_t *px    = NULL;

   if (!frame)
      return;
   for (i = 0; i < (size_t)w * h; i++)
      frame[i] = prng() & 0x00FFFFFFu;

   bmp = rbmp_save_image_string(frame, w, h, w * 4,
         RBMP_SOURCE_TYPE_XRGB888, &_len);
   CHECK(bmp != NULL, "xrgb888: string encode");
   if (!bmp)
   {
      free(frame);
      return;
   }

   /* 24bpp: line = (31*3 + 3) & ~3 = 96, file = 54 + 96*17 */
   CHECK(_len == 54 + 96 * 17, "xrgb888: exact output size");
   CHECK(bmp[0] == 'B' && bmp[1] == 'M' && bmp[10] == 54
         && bmp[28] == 24, "xrgb888: header fields");
   /* padding bytes of the first row must be deterministic zeros */
   CHECK(bmp[54 + 93] == 0 && bmp[54 + 94] == 0 && bmp[54 + 95] == 0,
         "xrgb888: row padding zeroed");

   px = decode_bmp(bmp, _len, &dw, &dh);
   CHECK(px != NULL && dw == w && dh == h, "xrgb888: decode");
   ok = (px != NULL && dw == w && dh == h);
   /* Positive pitch stores rows in walk order; BMP is bottom-up, so
    * decoded row j is frame row h-1-j. */
   for (j = 0; ok && j < h; j++)
      for (i = 0; ok && i < w; i++)
         if ((px[j * w + i] & 0x00FFFFFFu)
               != (frame[(h - 1 - j) * w + i] & 0x00FFFFFFu))
            ok = 0;
   CHECK(ok, "xrgb888: pixel round-trip (bottom-up)");
   free(px);

   check_stream_matches_string(bmp, _len, frame, w, h, w * 4,
         RBMP_SOURCE_TYPE_XRGB888, "xrgb888: stream == string");

   /* Negative pitch: hand in the last row and walk upwards; the
    * decoded image must now equal the frame top-down. */
   {
      size_t _len2 = 0;
      uint8_t *bmp2 = rbmp_save_image_string(
            (const uint8_t*)frame + (size_t)(h - 1) * w * 4,
            w, h, (unsigned)-(int)(w * 4),
            RBMP_SOURCE_TYPE_XRGB888, &_len2);
      CHECK(bmp2 != NULL && _len2 == _len, "xrgb888: negative pitch encode");
      if (bmp2)
      {
         px = decode_bmp(bmp2, _len2, &dw, &dh);
         ok = (px != NULL && dw == w && dh == h);
         for (j = 0; ok && j < h; j++)
            for (i = 0; ok && i < w; i++)
               if ((px[j * w + i] & 0x00FFFFFFu)
                     != (frame[j * w + i] & 0x00FFFFFFu))
                  ok = 0;
         CHECK(ok, "xrgb888: pixel round-trip (top-down via negative pitch)");
         free(px);
         free(bmp2);
      }
   }

   free(bmp);
   free(frame);
}

static void test_rgb565_roundtrip(void)
{
   unsigned w = 13, h = 7;
   unsigned dw = 0, dh = 0;
   size_t _len = 0;
   size_t i, j;
   int ok;
   uint16_t *frame = (uint16_t*)malloc((size_t)w * h * 2);
   uint8_t *bmp    = NULL;
   uint32_t *px    = NULL;

   if (!frame)
      return;
   for (i = 0; i < (size_t)w * h; i++)
      frame[i] = (uint16_t)prng();

   bmp = rbmp_save_image_string(frame, w, h, w * 2,
         RBMP_SOURCE_TYPE_RGB565, &_len);
   CHECK(bmp != NULL, "rgb565: string encode");
   if (!bmp)
   {
      free(frame);
      return;
   }

   px = decode_bmp(bmp, _len, &dw, &dh);
   ok = (px != NULL && dw == w && dh == h);
   for (j = 0; ok && j < h; j++)
   {
      for (i = 0; ok && i < w; i++)
      {
         uint16_t p = frame[(h - 1 - j) * w + i];
         uint8_t b5 = (p >>  0) & 0x1f;
         uint8_t g6 = (p >>  5) & 0x3f;
         uint8_t r5 = (p >> 11) & 0x1f;
         uint32_t expect = 0
               | ((uint32_t)((r5 << 3) | (r5 >> 2)) << 16)
               | ((uint32_t)((g6 << 2) | (g6 >> 4)) <<  8)
               | ((uint32_t)((b5 << 3) | (b5 >> 2)) <<  0);
         if ((px[j * w + i] & 0x00FFFFFFu) != expect)
            ok = 0;
      }
   }
   CHECK(ok, "rgb565: pixel round-trip with 565 expansion");
   free(px);

   check_stream_matches_string(bmp, _len, frame, w, h, w * 2,
         RBMP_SOURCE_TYPE_RGB565, "rgb565: stream == string");

   free(bmp);
   free(frame);
}

static void test_bgr24_roundtrip(void)
{
   unsigned w = 3, h = 5; /* 9-byte rows, 3 bytes padding */
   unsigned dw = 0, dh = 0;
   size_t _len = 0;
   size_t i, j;
   int ok;
   /* Tightly-sized source: exactly w*3 per row, no slack, so any
    * stride overread trips ASan. */
   uint8_t *frame = (uint8_t*)malloc((size_t)w * h * 3);
   uint8_t *bmp   = NULL;
   uint32_t *px   = NULL;

   if (!frame)
      return;
   for (i = 0; i < (size_t)w * h * 3; i++)
      frame[i] = (uint8_t)prng();

   bmp = rbmp_save_image_string(frame, w, h, w * 3,
         RBMP_SOURCE_TYPE_BGR24, &_len);
   CHECK(bmp != NULL && _len == 54 + 12 * 5, "bgr24: unaligned encode");
   if (!bmp)
   {
      free(frame);
      return;
   }
   CHECK(bmp[54 + 9] == 0 && bmp[54 + 10] == 0 && bmp[54 + 11] == 0,
         "bgr24: padding zeroed");

   px = decode_bmp(bmp, _len, &dw, &dh);
   ok = (px != NULL && dw == w && dh == h);
   for (j = 0; ok && j < h; j++)
   {
      for (i = 0; ok && i < w; i++)
      {
         const uint8_t *s = frame + ((h - 1 - j) * w + i) * 3;
         uint32_t expect  = ((uint32_t)s[2] << 16)
               | ((uint32_t)s[1] << 8) | s[0];
         if ((px[j * w + i] & 0x00FFFFFFu) != expect)
            ok = 0;
      }
   }
   CHECK(ok, "bgr24: pixel round-trip");
   free(px);

   check_stream_matches_string(bmp, _len, frame, w, h, w * 3,
         RBMP_SOURCE_TYPE_BGR24, "bgr24: stream == string");

   free(bmp);
   free(frame);

   /* 4-byte-aligned width: stride == padded row size, which drives the
    * stream encoder's whole-block single-write fast path.  Its output
    * must stay byte-identical to the string encoder. */
   {
      unsigned aw = 8, ah = 3;
      uint8_t *aframe = (uint8_t*)malloc((size_t)aw * ah * 3);
      if (aframe)
      {
         size_t alen = 0;
         uint8_t *abmp;
         for (i = 0; i < (size_t)aw * ah * 3; i++)
            aframe[i] = (uint8_t)prng();
         abmp = rbmp_save_image_string(aframe, aw, ah, aw * 3,
               RBMP_SOURCE_TYPE_BGR24, &alen);
         CHECK(abmp != NULL && alen == 54 + (size_t)aw * 3 * ah,
               "bgr24 aligned: exact size, no padding");
         if (abmp)
         {
            check_stream_matches_string(abmp, alen, aframe, aw, ah,
                  aw * 3, RBMP_SOURCE_TYPE_BGR24,
                  "bgr24 aligned: whole-block stream == string");
            free(abmp);
         }
         free(aframe);
      }
   }
}

static void test_argb8888_roundtrip(void)
{
   unsigned w = 8, h = 4;
   unsigned dw = 0, dh = 0;
   size_t _len = 0;
   size_t i, j;
   int ok;
   uint32_t *frame = (uint32_t*)malloc((size_t)w * h * 4);
   uint8_t *bmp    = NULL;
   uint32_t *px    = NULL;

   if (!frame)
      return;
   for (i = 0; i < (size_t)w * h; i++)
      frame[i] = 0xFF000000u | (prng() & 0x00FFFFFFu);

   bmp = rbmp_save_image_string(frame, w, h, w * 4,
         RBMP_SOURCE_TYPE_ARGB8888, &_len);
   CHECK(bmp != NULL && _len == 54 + (size_t)w * 4 * h,
         "argb8888: exact 32bpp size");
   if (!bmp)
   {
      free(frame);
      return;
   }
   CHECK(bmp[28] == 32, "argb8888: 32bpp header");

   px = decode_bmp(bmp, _len, &dw, &dh);
   ok = (px != NULL && dw == w && dh == h);
   for (j = 0; ok && j < h; j++)
      for (i = 0; ok && i < w; i++)
         if ((px[j * w + i] & 0x00FFFFFFu)
               != (frame[(h - 1 - j) * w + i] & 0x00FFFFFFu))
            ok = 0;
   CHECK(ok, "argb8888: pixel round-trip");
   free(px);

   check_stream_matches_string(bmp, _len, frame, w, h, w * 4,
         RBMP_SOURCE_TYPE_ARGB8888, "argb8888: stream == string");

   free(bmp);
   free(frame);
}

/* The deprecated path adapter must produce a file byte-identical to
 * the string encoder's buffer. */
static void test_path_adapter(void)
{
   unsigned w = 16, h = 9;
   size_t _len = 0;
   size_t i;
   int ok = 0;
   const char *path = "/tmp/rbmp_encode_test.bmp";
   uint8_t *frame   = (uint8_t*)malloc((size_t)w * h * 3);
   uint8_t *bmp     = NULL;

   if (!frame)
      return;
   for (i = 0; i < (size_t)w * h * 3; i++)
      frame[i] = (uint8_t)prng();

   bmp = rbmp_save_image_string(frame, w, h, w * 3,
         RBMP_SOURCE_TYPE_BGR24, &_len);

   if (bmp && rbmp_save_image(path, frame, w, h, w * 3,
            RBMP_SOURCE_TYPE_BGR24))
   {
      int64_t file_len = 0;
      uint8_t *file    = NULL;
      if (filestream_read_file(path, (void**)&file, &file_len))
      {
         ok = (file_len == (int64_t)_len)
               && !memcmp(file, bmp, _len);
         free(file);
      }
      filestream_delete(path);
   }
   CHECK(ok, "path adapter: file == string encoder output");

   free(bmp);
   free(frame);
}

/* The public row primitives back both the string encoder and (via
 * file/rbmp_file.c) the stream writer; lock their contract directly. */
static void test_row_primitives(void)
{
   uint8_t line[64];
   uint8_t row24[9];   /* 3px BGR24 payload, no slack */
   uint32_t row32[3];
   const uint8_t *out;
   size_t i;

   CHECK(rbmp_row_size(3, RBMP_SOURCE_TYPE_BGR24) == 12,
         "row_size: 3px BGR24 pads 9 -> 12");
   CHECK(rbmp_row_size(4, RBMP_SOURCE_TYPE_BGR24) == 12,
         "row_size: 4px BGR24 already aligned");
   CHECK(rbmp_row_size(3, RBMP_SOURCE_TYPE_ARGB8888) == 12,
         "row_size: 32bpp rows never pad");
   CHECK(rbmp_row_size(3, RBMP_SOURCE_TYPE_RGB565) == 12,
         "row_size: RGB565 sized as 24bpp output");

   for (i = 0; i < sizeof(row24); i++)
      row24[i] = (uint8_t)(i + 1);

   /* Aligned-stride BGR24 and ARGB pass the source row through. */
   out = rbmp_encode_row(line, row24, 4, 12, RBMP_SOURCE_TYPE_BGR24);
   CHECK(out != NULL && out != line,
         "encode_row: aligned BGR24 passes through");
   row32[0] = 0xFF112233u; row32[1] = 0xFF445566u; row32[2] = 0xFF778899u;
   out = rbmp_encode_row(line, row32, 3, 12, RBMP_SOURCE_TYPE_ARGB8888);
   CHECK(out == (const uint8_t*)row32,
         "encode_row: ARGB passes through");

   /* Unaligned BGR24 converts into the scratch with zeroed padding. */
   memset(line, 0xAA, sizeof(line));
   out = rbmp_encode_row(line, row24, 3, 9, RBMP_SOURCE_TYPE_BGR24);
   CHECK(out == line && !memcmp(line, row24, 9)
         && line[9] == 0 && line[10] == 0 && line[11] == 0,
         "encode_row: unaligned BGR24 pads into scratch");

   /* XRGB converts to BGR byte order in the scratch. */
   memset(line, 0xAA, sizeof(line));
   out = rbmp_encode_row(line, row32, 3, 12, RBMP_SOURCE_TYPE_XRGB888);
   CHECK(out == line && line[0] == 0x33 && line[1] == 0x22
         && line[2] == 0x11 && line[9] == 0,
         "encode_row: XRGB888 converted and padded");

   CHECK(rbmp_encode_row(NULL, row24, 3, 9, RBMP_SOURCE_TYPE_BGR24) == NULL
         && rbmp_encode_row(line, NULL, 3, 9, RBMP_SOURCE_TYPE_BGR24) == NULL
         && rbmp_encode_row(line, row24, 0, 9, RBMP_SOURCE_TYPE_BGR24) == NULL,
         "encode_row: invalid arguments rejected");
}

static void test_invalid_args(void)
{
   size_t _len = 0;
   uint8_t px[4] = {0, 0, 0, 0};
   CHECK(!rbmp_save_image_string(NULL, 1, 1, 4,
            RBMP_SOURCE_TYPE_ARGB8888, &_len),
         "invalid: NULL frame rejected");
   CHECK(!rbmp_save_image_string(px, 0, 1, 4,
            RBMP_SOURCE_TYPE_ARGB8888, &_len),
         "invalid: zero width rejected");
   CHECK(!rbmp_save_image_string(px, 1, 0, 4,
            RBMP_SOURCE_TYPE_ARGB8888, &_len),
         "invalid: zero height rejected");
   CHECK(!rbmp_save_image_string(px, 1, 1, 4,
            RBMP_SOURCE_TYPE_ARGB8888, NULL),
         "invalid: NULL out_len rejected");
   CHECK(!rbmp_save_image_stream(NULL, px, 1, 1, 4,
            RBMP_SOURCE_TYPE_ARGB8888),
         "invalid: NULL stream rejected");
}

int main(void)
{
   test_xrgb888_roundtrip();
   test_rgb565_roundtrip();
   test_bgr24_roundtrip();
   test_argb8888_roundtrip();
   test_path_adapter();
   test_row_primitives();
   test_invalid_args();

   if (failures)
   {
      printf("%d FAILURE(S)\n", failures);
      return 1;
   }

   printf("ALL TESTS PASSED\n");
   return 0;
}
