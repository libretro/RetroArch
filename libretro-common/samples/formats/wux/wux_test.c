/* wux_test: writes WUX images of disc images it makes up and checks
 * that formats/wux reads them back as the disc image, octet for octet.
 *
 * The images keep a sector the disc holds more than once, once, and
 * keep the array out of the disc's order in places, with sector sizes
 * a power of two and not. Ranges are supplied short now and then.
 * Every request is checked: opening reads the header alone; the index
 * is read a window at a time, only where a read needs it; a read's
 * data lands in its own destination, one request per run of sectors
 * the array keeps in order. A header that is not one, a sector size
 * out of range, an array past the file's end and nothing supplied must
 * fail. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <formats/wux.h>

#define SECTORS 700   /* more than the reader's index window */

static int failures = 0;
static uint32_t rng_state = 0x5EED1234u;

static uint32_t rnd(void)
{
   rng_state ^= rng_state << 13;
   rng_state ^= rng_state >> 17;
   rng_state ^= rng_state << 5;
   return rng_state;
}

static void check(int ok, const char *what)
{
   if (!ok)
   {
      failures++;
      printf("FAIL  %s\n", what);
   }
}

static void le32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)v;
   p[1] = (uint8_t)(v >> 8);
   p[2] = (uint8_t)(v >> 16);
   p[3] = (uint8_t)(v >> 24);
}

/* a WUX, the disc it stands for, and how the reader asked for it */
typedef struct
{
   uint8_t *data;
   size_t   size;
   uint8_t *disc;
   size_t   disc_size;
   uint32_t ss;
   bool     shorten;
   unsigned requests;
   unsigned index_requests;
   size_t   asked;
} file_t;

static size_t pull(file_t *f, const wux_request_t *req)
{
   size_t n = req->length;
   f->requests++;
   f->asked += req->length;
   if (req->offset >= f->size)
      return 0;
   if (n > f->size - req->offset)
      n = f->size - (size_t)req->offset;
   if (f->shorten && n > 1 && (rnd() & 3) == 0)
      n = 1 + rnd() % (n - 1);
   memcpy(req->dst, f->data + req->offset, n);
   return n;
}

/* The disc: @sectors of @ss octets, the last @tail long when not 0.
 * Every seventh sector repeats the one before it, and sectors 100
 * through 119 are kept in reverse order; the rest are in order. */
static void make_wux(file_t *f, uint32_t ss, uint32_t sectors, uint32_t tail)
{
   uint32_t *where = (uint32_t*)malloc(sectors * sizeof(uint32_t));
   uint32_t *order = (uint32_t*)malloc(sectors * sizeof(uint32_t));
   uint32_t  kept  = 0;
   uint32_t  s, k;
   size_t    at;

   f->ss        = ss;
   f->disc_size = (size_t)ss * sectors - (tail ? ss - tail : 0);
   f->disc      = (uint8_t*)malloc((size_t)ss * sectors);
   for (s = 0; s < sectors; s++)
   {
      if (s % 7 == 6)
         memcpy(f->disc + (size_t)s * ss, f->disc + (size_t)(s - 1) * ss, ss);
      else
         for (k = 0; k < ss; k++)
            f->disc[(size_t)s * ss + k] = (uint8_t)rnd();
   }
   memset(f->disc + f->disc_size, 0, (size_t)ss * sectors - f->disc_size);

   /* the distinct sectors, in the array's order */
   for (s = 0; s < sectors; s++)
      if (s % 7 != 6)
         order[kept++] = s;
   for (k = 0; k < 10; k++)
   {
      uint32_t a = kept, b = kept, t;
      for (s = 0; s < kept; s++)
      {
         if (order[s] == 100 + k)
            a = s;
         if (order[s] == 119 - k)
            b = s;
      }
      /* a repeated sector is not in the array */
      if (a < kept && b < kept)
      {
         t = order[a]; order[a] = order[b]; order[b] = t;
      }
   }
   for (k = 0; k < kept; k++)
      where[order[k]] = k;
   for (s = 0; s < sectors; s++)
      if (s % 7 == 6)
         where[s] = where[s - 1];

   at       = 32 + (size_t)sectors * 4;
   at       = (at + ss - 1) / ss * ss;
   f->size  = at + (size_t)kept * ss;
   f->data  = (uint8_t*)calloc(1, f->size);
   memcpy(f->data, "WUX0", 4);
   le32(f->data + 4, 0x1099D02Eu);
   le32(f->data + 8, ss);
   le32(f->data + 16, (uint32_t)f->disc_size);
   le32(f->data + 20, 0);
   for (s = 0; s < sectors; s++)
      le32(f->data + 32 + s * 4, where[s]);
   for (k = 0; k < kept; k++)
      memcpy(f->data + at + (size_t)k * ss, f->disc + (size_t)order[k] * ss, ss);
   f->shorten        = false;
   f->requests       = 0;
   f->index_requests = 0;
   f->asked          = 0;
   free(where);
   free(order);
}

static void free_wux(file_t *f)
{
   free(f->data);
   free(f->disc);
}

static int open_wux(file_t *f, wux_t **out)
{
   wux_request_t req;
   int err;
   *out = wux_new(f->size);
   while ((err = wux_open_step(*out, &req)) == WUX_PENDING)
      wux_feed(*out, pull(f, &req));
   return err;
}

/* counts the data requests that land outside @dst */
static int read_disc(file_t *f, wux_t *w, uint64_t off, uint8_t *dst,
      size_t len, unsigned *outside)
{
   wux_request_t req;
   int err;
   if ((err = wux_read_begin(w, off, dst, len)) != WUX_OK)
      return err;
   while ((err = wux_read_step(w, &req)) == WUX_PENDING)
   {
      if (req.offset < 32 + (uint64_t)SECTORS * 4 && req.offset >= 32)
         f->index_requests++;
      else if (req.dst < dst || req.dst + req.length > dst + len)
         (*outside)++;
      wux_feed(w, pull(f, &req));
   }
   return err;
}

static void read_lane(uint32_t ss, uint32_t tail)
{
   file_t   f;
   wux_t   *w;
   uint8_t *got;
   unsigned outside = 0;
   unsigned i;
   int      pass;
   char     what[64];

   for (pass = 0; pass < 2; pass++)
   {
      make_wux(&f, ss, SECTORS, tail);
      f.shorten = pass == 1;
      got       = (uint8_t*)malloc(f.disc_size + 16);
      check(open_wux(&f, &w) == WUX_OK, "opens");
      if (!pass)
         check(f.asked == 32 && f.requests == 1, "open reads the header alone");
      check(wux_disc_size(w) == f.disc_size, "disc size");

      sprintf(what, "whole disc, sector size %u", (unsigned)ss);
      check(   read_disc(&f, w, 0, got, f.disc_size + 16, &outside) == WUX_OK
            && wux_read_progress(w) == f.disc_size
            && !memcmp(got, f.disc, f.disc_size), what);

      for (i = 0; i < 400; i++)
      {
         uint64_t off  = rnd() % f.disc_size;
         size_t   len  = 1 + rnd() % (5 * ss);
         size_t   want = len > f.disc_size - off
            ? (size_t)(f.disc_size - off) : len;
         if (     read_disc(&f, w, off, got, len, &outside) != WUX_OK
               || wux_read_progress(w) != want
               || memcmp(got, f.disc + off, want))
         {
            check(0, "random reads");
            break;
         }
      }
      check(!outside, "data lands in the read's destination");
      check(   read_disc(&f, w, f.disc_size, got, 10, &outside) == WUX_OK
            && !wux_read_progress(w), "a read past the end is empty");
      wux_free(w);
      free(got);
      free_wux(&f);
   }

   /* requests: sectors 0 through 5 are in order and 6 repeats 5, so
    * the first seven sectors are one window and two runs */
   make_wux(&f, ss, SECTORS, tail);
   got = (uint8_t*)malloc(7 * ss);
   open_wux(&f, &w);
   f.requests = 0;
   f.asked    = 0;
   check(   read_disc(&f, w, 0, got, 7 * ss, &outside) == WUX_OK
         && !memcmp(got, f.disc, 7 * ss)
         && f.requests == 3 && f.asked == 256 * 4 + 7 * ss,
         "one index window, one request per run");
   /* the same window serves the next read */
   f.requests = 0;
   check(   read_disc(&f, w, 2 * ss + 3, got, 10, &outside) == WUX_OK
         && f.requests == 1, "the window is kept");
   /* the last sectors: a window of what is left */
   f.asked = 0;
   check(   read_disc(&f, w, f.disc_size - 1, got, 1, &outside) == WUX_OK
         && got[0] == f.disc[f.disc_size - 1]
         && f.asked == (size_t)(SECTORS - (SECTORS - 1)) * 4 + 1,
         "the last window is as long as the index");
   wux_free(w);
   free(got);
   free_wux(&f);
}

static void broken_lanes(void)
{
   file_t        f;
   wux_t        *w;
   wux_request_t req;
   uint8_t       buf[16];
   unsigned      outside = 0;

   make_wux(&f, 0x100, 16, 0);
   check(wux_is_wux(f.data), "is a WUX");
   f.data[3] = '1';
   check(!wux_is_wux(f.data), "is not a WUX");
   check(open_wux(&f, &w) == WUX_ERROR_DATA, "bad magic");
   check(   wux_read_begin(w, 0, buf, 1) == WUX_ERROR_STATE,
         "read before open");
   wux_free(w);
   f.data[3] = '0';

   le32(f.data + 8, 0x80);
   check(open_wux(&f, &w) == WUX_ERROR_DATA, "sector size too small");
   wux_free(w);
   le32(f.data + 8, 0x10000000u);
   check(open_wux(&f, &w) == WUX_ERROR_DATA, "sector size too large");
   wux_free(w);
   le32(f.data + 8, 0x100);

   le32(f.data + 20, 1);
   check(open_wux(&f, &w) == WUX_ERROR_DATA, "array past the file's end");
   wux_free(w);
   le32(f.data + 20, 0);

   /* a sector named past the array */
   le32(f.data + 32, 0x7FFF);
   check(   open_wux(&f, &w) == WUX_OK
         && read_disc(&f, w, 0, buf, 16, &outside) == WUX_ERROR_DATA,
         "a sector past the file's end");
   wux_free(w);
   free_wux(&f);

   make_wux(&f, 0x100, 16, 0);
   w = wux_new(f.size);
   check(wux_feed(w, 1) == WUX_ERROR_STATE, "feed with nothing asked");
   check(wux_open_step(w, &req) == WUX_PENDING, "open asks");
   check(   wux_feed(w, 0) == WUX_ERROR_DATA
         && wux_open_step(w, &req) == WUX_ERROR_DATA,
         "nothing supplied fails");
   wux_free(w);
   free_wux(&f);
}

int main(void)
{
   read_lane(0x8000, 0);
   read_lane(0x100, 0x40);
   read_lane(0x180, 0);
   broken_lanes();

   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("wux_test: all passed\n");
   return 0;
}
