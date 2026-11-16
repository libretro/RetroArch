/* Regression tests for libretro-common/formats/tga/rtga.c
 *
 * Targets:
 *
 *  1. Header-level palette validation.  Pre-patch a crafted TGA
 *     with tga_indexed=1 and tga_palette_bits=255 let the indexed
 *     read loop
 *          for (j = 0; j * 8 < tga_palette_bits; ++j)
 *              raw_data[j] = tga_palette[pal_idx + j];
 *     iterate j=0..31, writing up to 32 bytes into a 4-byte stack
 *     array raw_data[4].  This is a directly reachable stack
 *     buffer overflow driven by a single attacker-controlled
 *     header byte.  Under ASan the unpatched decoder reports
 *     "stack-buffer-overflow WRITE of size 1" inside the indexed
 *     read loop.  Post-patch the header check rejects
 *     palette_bits not in {15, 16, 24, 32} and the decoder
 *     returns IMAGE_PROCESS_ERROR cleanly.
 *
 *  2. Rejection of indexed TGA with empty palette (palette_len=0).
 *     Pre-patch the indexed code clamped pal_idx to 0 and then
 *     read tga_palette[0] from a malloc(0) buffer -- OOB read.
 *
 *  3. Happy-path decode of a tiny 2x2 uncompressed RGB TGA --
 *     smoke test for the fast path.
 *
 *  4. Rejection of a TGA whose declared size > INT_MAX.  Pre-patch
 *     the (int)size cast in rtga_process_image truncated and
 *     propagated a negative "len" into the buffer-end pointer.
 *     This is hard to reproduce without > 2 GiB of memory so we
 *     simulate with a small buffer and a size_t value that would
 *     pre-patch have truncated; post-patch we get an early
 *     IMAGE_PROCESS_ERROR without even calling the decoder.
 *
 *  5. An indexed TGA whose colour map is cut short is refused after
 *     the surface is made, and the surface is freed (LeakSanitizer).
 *
 *  6. rtga_decode_rows gives the bytes rtga_process_image gives, for
 *     every type, depth and origin, raw and RLE, a file cut short,
 *     either byte order, into the caller's rows and into rtga's own,
 *     on one decoder reused across sizes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <formats/rtga.h>
#include <formats/image.h>

static int failures = 0;

/* Build a TGA header into `out` starting at offset 0.
 * Returns the header size (always 18). */
static int make_tga_header(uint8_t *out,
      uint8_t id_len,
      uint8_t cmap_type,
      uint8_t image_type,
      uint16_t cmap_start,
      uint16_t cmap_len,
      uint8_t cmap_bits,
      uint16_t width,
      uint16_t height,
      uint8_t bpp,
      uint8_t descriptor)
{
   out[0]  = id_len;
   out[1]  = cmap_type;
   out[2]  = image_type;
   out[3]  = (uint8_t)(cmap_start & 0xFF);
   out[4]  = (uint8_t)(cmap_start >> 8);
   out[5]  = (uint8_t)(cmap_len & 0xFF);
   out[6]  = (uint8_t)(cmap_len >> 8);
   out[7]  = cmap_bits;
   out[8]  = 0; out[9]  = 0; /* x origin */
   out[10] = 0; out[11] = 0; /* y origin */
   out[12] = (uint8_t)(width & 0xFF);
   out[13] = (uint8_t)(width >> 8);
   out[14] = (uint8_t)(height & 0xFF);
   out[15] = (uint8_t)(height >> 8);
   out[16] = bpp;
   out[17] = descriptor;
   return 18;
}

static void test_malicious_palette_bits(void)
{
   /* Indexed uncompressed TGA, 1x1 pixel, palette_bits = 255.
    * Pre-patch: stack-buffer-overflow on raw_data[4..31]. */
   uint8_t file[128];
   rtga_t *rtga;
   void *out = NULL;
   unsigned w = 0, h = 0;
   int rc;
   int hdr_len;
   memset(file, 0, sizeof(file));

   hdr_len = make_tga_header(file,
         /* id_len      */ 0,
         /* cmap_type   */ 1,
         /* image_type  */ 1,     /* indexed uncompressed */
         /* cmap_start  */ 0,
         /* cmap_len    */ 1,
         /* cmap_bits   */ 255,   /* THE ATTACK */
         /* width       */ 1,
         /* height      */ 1,
         /* bpp         */ 8,
         /* descriptor  */ 0);
   /* A few bytes of "palette" and one pixel index -- doesn't matter
    * what they are, we expect to fail at the header check. */
   file[hdr_len]   = 0xFF;
   file[hdr_len+1] = 0xFF;
   file[hdr_len+2] = 0x00;  /* pal_idx = 0 */

   rtga = rtga_alloc();
   rtga_set_buf_ptr(rtga, file);
   rc = rtga_process_image(rtga, &out, (size_t)hdr_len + 16, &w, &h, true);

   if (rc != IMAGE_PROCESS_ERROR)
   {
      printf("[ERROR] malicious palette_bits=255 was not rejected "
             "(rc=%d, w=%u h=%u out=%p)\n", rc, w, h, out);
      failures++;
      free(out);
   }
   else
      printf("[SUCCESS] TGA palette_bits=255 rejected at header\n");
   rtga_free(rtga);
}

static void test_empty_indexed_palette(void)
{
   /* Indexed TGA with cmap_len = 0.  Pre-patch this malloc'd a
    * 0-byte palette and the indexed read was OOB. */
   uint8_t file[64];
   rtga_t *rtga;
   void *out = NULL;
   unsigned w = 0, h = 0;
   int rc;
   int hdr_len;
   memset(file, 0, sizeof(file));

   hdr_len = make_tga_header(file, 0, 1, 1, 0,
         /* cmap_len=   */ 0,
         /* cmap_bits=  */ 24,
         1, 1, 8, 0);
   file[hdr_len] = 0x00;

   rtga = rtga_alloc();
   rtga_set_buf_ptr(rtga, file);
   rc = rtga_process_image(rtga, &out, (size_t)hdr_len + 8, &w, &h, true);

   if (rc != IMAGE_PROCESS_ERROR)
   {
      printf("[ERROR] empty palette was not rejected "
             "(rc=%d out=%p)\n", rc, out);
      failures++;
      free(out);
   }
   else
      printf("[SUCCESS] TGA indexed with empty palette rejected\n");
   rtga_free(rtga);
}

static void test_happy_path_rgb(void)
{
   /* 2x2 uncompressed 24bpp BGR.  Just a smoke test. */
   uint8_t file[64];
   rtga_t *rtga;
   void *out = NULL;
   unsigned w = 0, h = 0;
   int rc;
   int hdr_len;
   int i;
   memset(file, 0, sizeof(file));

   hdr_len = make_tga_header(file, 0, 0,
         /* image_type= */ 2,    /* uncompressed RGB */
         0, 0, 0,
         /* width */ 2,
         /* height*/ 2,
         /* bpp   */ 24,
         /* descr */ 0);
   /* 4 pixels * 3 bytes */
   for (i = 0; i < 4 * 3; ++i)
      file[hdr_len + i] = (uint8_t)(0x10 * i);

   rtga = rtga_alloc();
   rtga_set_buf_ptr(rtga, file);
   /* The decoder is sliced: process_image fills a bounded run of the
    * image per call and returns IMAGE_PROCESS_NEXT until the surface is
    * complete, exactly as image_texture_load_buffer() and
    * task_image_process_transfer() drive it.  Loop to completion. */
   do
   {
      rc = rtga_process_image(rtga, &out, (size_t)hdr_len + 12, &w, &h, true);
   } while (rc == IMAGE_PROCESS_NEXT);

   if (rc != IMAGE_PROCESS_END || w != 2 || h != 2 || !out)
   {
      printf("[ERROR] happy-path decode: rc=%d w=%u h=%u out=%p\n",
            rc, w, h, out);
      failures++;
   }
   else
      printf("[SUCCESS] TGA 2x2 RGB happy path\n");
   free(out);
   rtga_free(rtga);
}

static void test_cut_palette(void)
{
   /* 2x2 indexed, a 256-entry 24-bit colour map of which 9 bytes
    * are there */
   uint8_t file[32];
   rtga_t *rtga;
   void *out = NULL;
   unsigned w = 0, h = 0;
   int rc;
   int hdr_len;
   memset(file, 0, sizeof(file));

   hdr_len = make_tga_header(file, 0, 1, 1, 0, 256, 24, 2, 2, 8, 0);

   rtga = rtga_alloc();
   rtga_set_buf_ptr(rtga, file);
   rc = rtga_process_image(rtga, &out, (size_t)hdr_len + 9, &w, &h, true);
   if (rc != IMAGE_PROCESS_ERROR || out)
   {
      printf("[ERROR] cut colour map not refused (rc=%d)\n", rc);
      failures++;
      free(out);
   }
   else
      printf("[SUCCESS] TGA with a cut colour map refused\n");
   rtga_free(rtga);
}

static void test_oversized_size(void)
{
   /* A size > INT_MAX should be rejected before the (int)size
    * cast that pre-patch produced a negative buffer-length. */
   uint8_t file[32];
   rtga_t *rtga;
   void *out = NULL;
   unsigned w = 0, h = 0;
   int rc;
   memset(file, 0, sizeof(file));
   /* The file contents don't matter -- we expect an early-out. */

   rtga = rtga_alloc();
   rtga_set_buf_ptr(rtga, file);
   rc = rtga_process_image(rtga, &out, (size_t)0x100000000ULL,
         &w, &h, true);

   if (rc != IMAGE_PROCESS_ERROR)
   {
      printf("[ERROR] oversized size not rejected (rc=%d)\n", rc);
      failures++;
      free(out);
   }
   else
      printf("[SUCCESS] TGA with size > INT_MAX rejected\n");
   rtga_free(rtga);
}

/* ---- 5: rtga_decode_rows against rtga_process_image ---- */

static uint32_t rng_state = 12345;

static unsigned rng(void)
{
   rng_state = rng_state * 1103515245u + 12345u;
   return (rng_state >> 16) & 0x7FFF;
}

struct rows_dst
{
   uint8_t *pic;      /* the caller's rows, or NULL for rtga's own */
   uint8_t *copy;     /* where done() puts rtga's own rows */
   unsigned w, h;
   int      asked, told, bad_order;
   int      refuse;
};

static bool rows_size(void *user, unsigned w, unsigned h)
{
   struct rows_dst *d = (struct rows_dst*)user;
   if (d->refuse)
      return false;
   d->w    = w;
   d->h    = h;
   d->pic  = (uint8_t*)malloc((size_t)w * h * 4);
   d->copy = (uint8_t*)malloc((size_t)w * h * 4);
   return d->pic && d->copy;
}

static uint8_t *rows_row(void *user, unsigned y)
{
   struct rows_dst *d = (struct rows_dst*)user;
   d->asked++;
   return (y & 1) ? NULL : d->pic + (size_t)y * d->w * 4;
}

static void rows_done(void *user, unsigned y, const uint8_t *px)
{
   struct rows_dst *d = (struct rows_dst*)user;
   d->told++;
   if (y >= d->h)
      d->bad_order++;
   else
      memcpy(d->copy + (size_t)y * d->w * 4, px, (size_t)d->w * 4);
}

/* A TGA of the given type, bpp and origin with random pixels; RLE
 * types get runs and literals.  Returns the length. */
static size_t make_tga(uint8_t *out, int type, int bpp, int w, int h,
      int top_down, int pal_bits)
{
   int    indexed = ((type & 7) == 1);
   int    pal_len = indexed ? 1 + (int)(rng() % 300) : 0;
   int    pix_bytes = indexed ? 1 : bpp / 8;
   int    npix    = w * h;
   size_t n       = (size_t)make_tga_header(out, (uint8_t)(rng() % 3),
         (uint8_t)indexed, (uint8_t)type, 0, (uint16_t)pal_len,
         (uint8_t)(indexed ? pal_bits : 0), (uint16_t)w, (uint16_t)h,
         (uint8_t)(indexed ? 8 : bpp), (uint8_t)(top_down ? 0x20 : 0));
   int    i, k;
   for (i = 0; i < out[0]; i++)
      out[n++] = (uint8_t)rng();
   for (i = 0; i < pal_len * ((pal_bits + 7) / 8); i++)
      out[n++] = (uint8_t)rng();
   if (type < 8)
   {
      for (i = 0; i < npix * pix_bytes; i++)
         out[n++] = (uint8_t)rng();
      return n;
   }
   for (i = 0; i < npix; )
   {
      int run = 1 + (int)(rng() % 128);
      if (run > npix - i)
         run = npix - i;
      if (rng() & 1)
      {
         out[n++] = (uint8_t)(0x80 | (run - 1));
         for (k = 0; k < pix_bytes; k++)
            out[n++] = (uint8_t)rng();
      }
      else
      {
         out[n++] = (uint8_t)(run - 1);
         for (k = 0; k < run * pix_bytes; k++)
            out[n++] = (uint8_t)rng();
      }
      i += run;
   }
   return n;
}

static int compare_rows(rtga_t *dec, const uint8_t *file, size_t len,
      bool rgba, const char *what)
{
   rtga_t *ref = rtga_alloc();
   void *out   = NULL;
   unsigned w = 0, h = 0, rw = 0, rh = 0, y, x;
   struct rtga_rows rows;
   struct rows_dst d;
   int rc, bad = 0;

   rtga_set_buf_ptr(ref, (void*)file);
   do
   {
      rc = rtga_process_image(ref, &out, len, &w, &h, rgba);
   } while (rc == IMAGE_PROCESS_NEXT);
   rtga_free(ref);

   memset(&d, 0, sizeof(d));
   rows.size = rows_size;
   rows.row  = rows_row;
   rows.done = rows_done;
   rows.user = &d;
   if (!rtga_decode_rows(dec, file, len, rgba, &rows, &rw, &rh))
      bad = (rc == IMAGE_PROCESS_END);
   else if (rc != IMAGE_PROCESS_END || rw != w || rh != h
         || d.asked != (int)h || d.told != (int)h || d.bad_order)
      bad = 1;
   else
   {
      for (y = 0; y < h && !bad; y++)
         for (x = 0; x < w; x++)
         {
            uint32_t v = ((const uint32_t*)out)[(size_t)y * w + x];
            const uint8_t *p = ((y & 1) ? d.copy : d.pic)
               + ((size_t)y * w + x) * 4;
            const uint8_t *c = d.copy + ((size_t)y * w + x) * 4;
            if (     p[0] != (uint8_t)v         || p[1] != (uint8_t)(v >> 8)
                  || p[2] != (uint8_t)(v >> 16) || p[3] != (uint8_t)(v >> 24)
                  || memcmp(p, c, 4))
            {
               bad = 1;
               break;
            }
         }
   }
   if (bad)
      printf("[ERROR] decode_rows differs: %s (%ux%u)\n", what, w, h);
   free(out);
   free(d.pic);
   free(d.copy);
   return bad;
}

static void test_decode_rows(void)
{
   static const int types[] = { 1, 2, 3, 9, 10, 11 };
   static const int sizes[][2] = { {1, 1}, {7, 3}, {64, 5}, {300, 17}, {2, 40} };
   uint8_t *file = (uint8_t*)malloc(1 << 20);
   rtga_t  *dec  = rtga_alloc();
   char what[96];
   int t, b, z, o, rgba, cut, cases = 0, bad = 0;

   for (t = 0; t < 6; t++)
      for (b = 8; b <= 32; b += 8)
         for (z = 0; z < 5; z++)
            for (o = 0; o < 2; o++)
               for (cut = 0; cut < 2; cut++)
                  for (rgba = 0; rgba < 2; rgba++)
                  {
                     int    indexed = ((types[t] & 7) == 1);
                     int    pal_bits;
                     size_t len;
                     if (types[t] == 3 || types[t] == 11)
                     {
                        if (b > 16)
                           continue;
                     }
                     else if (!indexed && b < 24)
                        continue;
                     pal_bits = (b == 8) ? 15 : (b == 16 ? 16 : b);
                     rng_state = (uint32_t)(t * 1000 + b * 100 + z * 10 + o);
                     len = make_tga(file, types[t], b, sizes[z][0],
                           sizes[z][1], o, pal_bits);
                     if (cut)
                        len -= len / 3;
                     sprintf(what, "type %d bpp %d %s%s%s", types[t], b,
                           o ? "top-down" : "bottom-up",
                           cut ? " cut short" : "", rgba ? " rgba" : " bgra");
                     bad += compare_rows(dec, file, len, rgba ? true : false,
                           what);
                     cases++;
                  }

   {
      struct rtga_rows rows;
      struct rows_dst d;
      unsigned w, h;
      size_t len = make_tga(file, 2, 32, 4, 4, 0, 0);
      memset(&d, 0, sizeof(d));
      d.refuse  = 1;
      rows.size = rows_size;
      rows.row  = rows_row;
      rows.done = rows_done;
      rows.user = &d;
      if (rtga_decode_rows(dec, file, len, true, &rows, &w, &h)
            || d.asked || w != 4 || h != 4)
      {
         printf("[ERROR] decode_rows went on after size() said no\n");
         bad++;
      }
      if (rtga_decode_rows(dec, file, 10, true, &rows, &w, &h) || w || h)
      {
         printf("[ERROR] decode_rows took a cut header\n");
         bad++;
      }
   }

   if (bad)
      failures++;
   else
      printf("[SUCCESS] rtga_decode_rows matches rtga_process_image (%d files)\n",
            cases);
   rtga_free(dec);
   free(file);
}

int main(void)
{
   test_malicious_palette_bits();
   test_empty_indexed_palette();
   test_happy_path_rgb();
   test_oversized_size();
   test_cut_palette();
   test_decode_rows();

   if (failures)
   {
      printf("\n%d rtga test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll rtga regression tests passed.\n");
   return 0;
}
