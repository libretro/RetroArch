/* formats/mp4/rmp4.c's movie fragments (moof/traf/trun), against
 * fragmented MP4s built here byte by byte:
 *
 *  - valid: two samples addressed through a trun data offset come
 *    back as packets holding exactly their bytes;
 *  - wrap: a tfhd base-data-offset near 2^64 whose sample end wraps
 *    round to inside the file gives no packet pointing outside it;
 *  - countless: a trun declaring 2^32-1 samples of no size, with no
 *    per-sample fields, opens at once rather than walking them all.
 *
 * Plain C and the C library only. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <formats/rmp4.h>

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { \
   printf("FAIL: " __VA_ARGS__); printf("\n"); failures++; } } while (0)

/* ---- a box writer ---- */
typedef struct { uint8_t b[4096]; size_t n; } buf_t;

static void put8(buf_t *w, unsigned v)  { w->b[w->n++] = (uint8_t)v; }
static void put32(buf_t *w, uint32_t v)
{ put8(w, v >> 24); put8(w, v >> 16); put8(w, v >> 8); put8(w, v); }
static void put64(buf_t *w, uint64_t v)
{ put32(w, (uint32_t)(v >> 32)); put32(w, (uint32_t)v); }
static void putz(buf_t *w, size_t n) { while (n--) put8(w, 0); }
static void put4cc(buf_t *w, const char *s)
{ put8(w, s[0]); put8(w, s[1]); put8(w, s[2]); put8(w, s[3]); }

/* Opens a box; box_end() writes its size */
static size_t box(buf_t *w, const char *type)
{ size_t at = w->n; put32(w, 0); put4cc(w, type); return at; }
static void box_end(buf_t *w, size_t at)
{
   uint32_t sz = (uint32_t)(w->n - at);
   w->b[at] = sz >> 24; w->b[at + 1] = sz >> 16;
   w->b[at + 2] = sz >> 8; w->b[at + 3] = sz;
}

/* ftyp + moov for one audio track (ID 1, timescale 1000) with an empty
 * sample table and trex defaults of @dflt_size bytes, @dflt_dur ticks */
static void head(buf_t *w, uint32_t dflt_size, uint32_t dflt_dur)
{
   size_t moov, trak, mdia, minf, stbl, b, mvex;
   b = box(w, "ftyp"); put4cc(w, "iso6"); put32(w, 0); put4cc(w, "iso6");
   box_end(w, b);
   moov = box(w, "moov");
   b = box(w, "mvhd"); putz(w, 12); put32(w, 1000); put32(w, 0); putz(w, 80);
   box_end(w, b);
   trak = box(w, "trak");
   b = box(w, "tkhd"); putz(w, 12); put32(w, 1); putz(w, 68); box_end(w, b);
   mdia = box(w, "mdia");
   b = box(w, "mdhd"); putz(w, 12); put32(w, 1000); putz(w, 8); box_end(w, b);
   b = box(w, "hdlr"); putz(w, 8); put4cc(w, "soun"); putz(w, 13);
   box_end(w, b);
   minf = box(w, "minf");
   stbl = box(w, "stbl");
   b = box(w, "stsd"); putz(w, 8); box_end(w, b);
   box_end(w, stbl); box_end(w, minf); box_end(w, mdia); box_end(w, trak);
   mvex = box(w, "mvex");
   b = box(w, "trex"); putz(w, 4); put32(w, 1); put32(w, 1);
   put32(w, dflt_dur); put32(w, dflt_size); put32(w, 0); box_end(w, b);
   box_end(w, mvex);
   box_end(w, moov);
}

/* Every packet the demuxer gives, checked to lie inside the file */
static int read_all(rmp4_t *m, const buf_t *w, const char *name,
      const uint8_t **first)
{
   rmp4_packet pkt;
   int n = 0, r;
   while ((r = rmp4_read_packet(m, &pkt)) == 1)
   {
      CHECK(pkt.data >= w->b && pkt.size <= w->n
            && (size_t)(pkt.data - w->b) <= w->n - pkt.size,
            "%s: packet %d points outside the file", name, n);
      if (!n && first)
         *first = pkt.data;
      if (++n > 16)
         break;
   }
   return n;
}

static void test_valid(void)
{
   buf_t w;
   size_t moof, traf, b, trun_doff, mdat_payload;
   const uint8_t *first = NULL;
   rmp4_t *m;
   int n;

   memset(&w, 0, sizeof(w));
   head(&w, 0, 10);
   moof = box(&w, "moof");
   b = box(&w, "mfhd"); putz(&w, 4); put32(&w, 1); box_end(&w, b);
   traf = box(&w, "traf");
   b = box(&w, "tfhd"); put32(&w, 0); put32(&w, 1); box_end(&w, b);
   b = box(&w, "trun"); put32(&w, 0x000201); put32(&w, 2);
   trun_doff = w.n; put32(&w, 0);              /* data offset, below */
   put32(&w, 4); put32(&w, 4);                 /* sample sizes       */
   box_end(&w, b);
   box_end(&w, traf); box_end(&w, moof);
   b = box(&w, "mdat");
   mdat_payload = w.n;
   put32(&w, 0x11223344); put32(&w, 0x55667788);
   box_end(&w, b);
   /* the offset is from the moof's start */
   {
      uint32_t d = (uint32_t)(mdat_payload - moof);
      w.b[trun_doff] = d >> 24; w.b[trun_doff + 1] = d >> 16;
      w.b[trun_doff + 2] = d >> 8; w.b[trun_doff + 3] = d;
   }

   m = rmp4_open_memory(w.b, w.n);
   CHECK(m != NULL, "valid: opens");
   if (!m)
      return;
   n = read_all(m, &w, "valid", &first);
   CHECK(n == 2, "valid: %d packets, not 2", n);
   CHECK(first == w.b + mdat_payload, "valid: the first packet is the mdat's");
   rmp4_close(m);
   printf("valid: ok\n");
}

static void test_wrap(void)
{
   buf_t w;
   size_t moof, traf, b;
   rmp4_t *m;

   memset(&w, 0, sizeof(w));
   head(&w, 0, 10);
   moof = box(&w, "moof");
   b = box(&w, "mfhd"); putz(&w, 4); put32(&w, 1); box_end(&w, b);
   traf = box(&w, "traf");
   /* base-data-offset 2^64 - 16: plus a 32-byte sample, the end wraps
    * to 16, inside the file */
   b = box(&w, "tfhd"); put32(&w, 0x000001); put32(&w, 1);
   put64(&w, (uint64_t)0 - 16); box_end(&w, b);
   b = box(&w, "trun"); put32(&w, 0x000200); put32(&w, 1); put32(&w, 32);
   box_end(&w, b);
   box_end(&w, traf); box_end(&w, moof);
   b = box(&w, "mdat"); putz(&w, 64); box_end(&w, b);

   m = rmp4_open_memory(w.b, w.n);
   if (m)
   {
      read_all(m, &w, "wrap", NULL);
      rmp4_close(m);
   }
   printf("wrap: ok\n");
}

static void test_countless(void)
{
   buf_t w;
   size_t moof, traf, b;
   clock_t t0;
   double secs;
   rmp4_t *m;

   memset(&w, 0, sizeof(w));
   head(&w, 0, 1);
   moof = box(&w, "moof");
   b = box(&w, "mfhd"); putz(&w, 4); put32(&w, 1); box_end(&w, b);
   traf = box(&w, "traf");
   b = box(&w, "tfhd"); put32(&w, 0); put32(&w, 1); box_end(&w, b);
   b = box(&w, "trun"); put32(&w, 0); put32(&w, 0xFFFFFFFFu); box_end(&w, b);
   box_end(&w, traf); box_end(&w, moof);

   t0   = clock();
   m    = rmp4_open_memory(w.b, w.n);
   secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
   CHECK(secs < 1.0, "countless: open took %.1f s", secs);
   if (m)
   {
      read_all(m, &w, "countless", NULL);
      rmp4_close(m);
   }
   printf("countless: ok (%.3f s)\n", secs);
}

int main(void)
{
   test_valid();
   test_wrap();
   test_countless();
   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("rmp4_fragment: all passed\n");
   return 0;
}
