/* ciso_test: writes CISO files from disc images it makes up and checks
 * that formats/ciso reads each back as its image, octet for octet.
 *
 * The images have blocks left out at the start, between runs and as
 * single blocks, block sizes from 32 KiB to 4 MiB, and are read whole
 * and at random offsets and lengths, with ranges supplied short now
 * and then. Every request a read makes must point into the read's
 * destination, one per run of stored blocks. A map stored to its last
 * entry checks the map built over the head in place. Heads that are not CISO
 * - a PSP CSO among them, which shares the magic - must not open. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <formats/ciso.h>

#define HEAD     0x8000
#define MAP_SIZE (HEAD - 8)

static int failures = 0;
static uint32_t rng_state;

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

typedef struct
{
   uint8_t *file;
   size_t   file_len;
   bool     shorten;
   unsigned requests;
} src_t;

static size_t pull(src_t *s, const ciso_request_t *req)
{
   size_t n = req->length;
   s->requests++;
   if (req->offset >= s->file_len)
      return 0;
   if (n > s->file_len - req->offset)
      n = (size_t)(s->file_len - req->offset);
   if (s->shorten && n > 1 && (rnd() & 3) == 0)
      n = 1 + rnd() % (n - 1);
   memcpy(req->dst, s->file + req->offset, n);
   return n;
}

/* A CISO of @n blocks of @bs, block i stored when used[i]; @disc is
 * the image it stands for, zero in the blocks left out. */
static void make(uint32_t bs, const uint8_t *used, uint32_t n,
      src_t *s, uint8_t **disc)
{
   uint32_t i, stored = 0;
   uint8_t *f, *d;
   size_t   at;

   for (i = 0; i < n; i++)
      stored += used[i];
   f = (uint8_t*)calloc(1, HEAD + (size_t)stored * bs);
   d = (uint8_t*)calloc(1, (size_t)n * bs);
   memcpy(f, "CISO", 4);
   f[4] = (uint8_t)bs;
   f[5] = (uint8_t)(bs >> 8);
   f[6] = (uint8_t)(bs >> 16);
   f[7] = (uint8_t)(bs >> 24);
   at   = HEAD;
   for (i = 0; i < n; i++)
   {
      size_t k;
      f[8 + i] = used[i];
      if (!used[i])
         continue;
      for (k = 0; k < bs; k++)
         d[(size_t)i * bs + k] = (uint8_t)(rnd() | 1);
      memcpy(f + at, d + (size_t)i * bs, bs);
      at += bs;
   }
   s->file     = f;
   s->file_len = at;
   s->requests = 0;
   *disc       = d;
}

static ciso_t *open_src(src_t *s, int *err)
{
   ciso_request_t req;
   ciso_t *c = ciso_new();
   while ((*err = ciso_open_step(c, &req)) == CISO_PENDING)
      ciso_feed(c, pull(s, &req));
   return c;
}

/* Reads @len at @off and counts the requests made outside @dst */
static int read_src(ciso_t *c, src_t *s, uint64_t off, uint8_t *dst,
      size_t len, unsigned *outside)
{
   ciso_request_t req;
   int err;
   if ((err = ciso_read_begin(c, off, dst, len)) != CISO_OK)
      return err;
   while ((err = ciso_read_step(c, &req)) == CISO_PENDING)
   {
      if (req.dst < dst || req.dst + req.length > dst + len)
         (*outside)++;
      ciso_feed(c, pull(s, &req));
   }
   return err;
}

static void image_lane(uint32_t bs, const uint8_t *used, uint32_t n,
      unsigned runs, const char *what)
{
   char     name[128];
   src_t    s;
   uint8_t *disc, *got;
   uint64_t size = (uint64_t)n * bs;
   unsigned outside = 0, i;
   int      err;
   ciso_t  *c;

   make(bs, used, n, &s, &disc);
   s.shorten = false;
   c = open_src(&s, &err);
   sprintf(name, "%s: opens", what);
   check(err == CISO_OK, name);
   sprintf(name, "%s: disc size", what);
   check(ciso_disc_size(c) == size && ciso_block_size(c) == bs, name);

   got        = (uint8_t*)malloc((size_t)size + 16);
   s.requests = 0;
   err        = read_src(c, &s, 0, got, (size_t)size + 16, &outside);
   sprintf(name, "%s: whole image", what);
   check(   err == CISO_OK
         && ciso_read_progress(c) == size
         && !memcmp(got, disc, (size_t)size), name);
   sprintf(name, "%s: one request per stored run", what);
   check(s.requests == runs, name);

   s.shorten = true;
   for (i = 0; i < 400; i++)
   {
      uint64_t off = rnd() % size;
      size_t   len = 1 + rnd() % (3 * bs);
      size_t   want = len;
      if (want > size - off)
         want = (size_t)(size - off);
      err = read_src(c, &s, off, got, len, &outside);
      if (     err != CISO_OK
            || ciso_read_progress(c) != want
            || memcmp(got, disc + off, want))
      {
         sprintf(name, "%s: read %u at %u", what, (unsigned)len,
               (unsigned)off);
         check(0, name);
         break;
      }
   }
   sprintf(name, "%s: reads land in place", what);
   check(!outside, name);

   ciso_free(c);
   free(got);
   free(disc);
   free(s.file);
}

/* Blocks 0, MAP_SIZE - 2 and MAP_SIZE - 1 stored: the last entries of
 * the map are the ones built over the end of the head. */
static void edge_lane(void)
{
   const uint32_t bs = 0x8000;
   src_t    s;
   uint8_t *got = (uint8_t*)malloc(3 * bs);
   uint64_t tail = (uint64_t)(MAP_SIZE - 2) * bs;
   unsigned outside = 0;
   size_t   k;
   int      err;
   ciso_t  *c;

   s.file_len = HEAD + 3 * (size_t)bs;
   s.file     = (uint8_t*)calloc(1, s.file_len);
   s.shorten  = false;
   s.requests = 0;
   memcpy(s.file, "CISO", 4);
   s.file[5]                = (uint8_t)(bs >> 8);
   s.file[8]                = 1;
   s.file[8 + MAP_SIZE - 2] = 1;
   s.file[8 + MAP_SIZE - 1] = 1;
   for (k = HEAD; k < s.file_len; k++)
      s.file[k] = (uint8_t)(rnd() | 1);

   c = open_src(&s, &err);
   check(err == CISO_OK, "edge: opens");
   check(ciso_disc_size(c) == (uint64_t)MAP_SIZE * bs, "edge: disc size");
   err = read_src(c, &s, tail - 16, got, 2 * bs + 16, &outside);
   check(   err == CISO_OK && ciso_read_progress(c) == 2 * bs + 16
         && got[0] == 0 && got[15] == 0
         && !memcmp(got + 16, s.file + HEAD + bs, 2 * bs),
         "edge: last two blocks");
   err = read_src(c, &s, 0, got, bs, &outside);
   check(err == CISO_OK && !memcmp(got, s.file + HEAD, bs),
         "edge: first block");
   check(!outside, "edge: reads land in place");
   ciso_free(c);
   free(s.file);
   free(got);
}

static void head_lane(const uint8_t *head, size_t len, const char *what)
{
   src_t   s;
   int     err;
   ciso_t *c;

   s.file     = (uint8_t*)head;
   s.file_len = len;
   s.shorten  = false;
   s.requests = 0;
   c          = open_src(&s, &err);
   check(err < 0, what);
   check(ciso_read_begin(c, 0, NULL, 0) == CISO_ERROR_STATE, what);
   ciso_free(c);
}

static void bad_heads(void)
{
   uint8_t *h = (uint8_t*)calloc(1, HEAD);

   memcpy(h, "CISO", 4);
   h[6] = 2;          /* 128 KiB */
   h[8] = 1;
   head_lane(h, HEAD - 1, "short head refused");

   h[0] = 'X';
   head_lane(h, HEAD, "bad magic refused");
   h[0] = 'C';

   h[6] = 0;
   head_lane(h, HEAD, "block size 0 refused");
   h[4] = 0x18;
   head_lane(h, HEAD, "block size off the sector refused");
   h[4] = 0;
   h[6] = 2;

   h[100] = 2;
   head_lane(h, HEAD, "map octet 2 refused");
   h[100] = 0;

   h[8] = 0;
   head_lane(h, HEAD, "no stored block refused");

   /* a PSP CSO: header size, total size, block size, version */
   memset(h + 4, 0, 32);
   h[4]  = 0x18;
   h[10] = 0x40;
   h[17] = 0x08;
   h[20] = 1;
   head_lane(h, HEAD, "PSP CSO refused");

   free(h);
}

static void state_lane(void)
{
   static const uint8_t used[4] = { 1, 1, 0, 1 };
   ciso_request_t req;
   src_t    s;
   uint8_t *disc;
   uint8_t  buf[64];
   unsigned outside = 0;
   int      err;
   ciso_t  *c = ciso_new();

   check(ciso_feed(c, 1) == CISO_ERROR_STATE, "feed with nothing asked");
   check(ciso_read_begin(c, 0, buf, 1) == CISO_ERROR_STATE,
         "read before open");
   check(ciso_open_step(c, &req) == CISO_PENDING, "open asks");
   check(ciso_feed(c, 0) == CISO_ERROR_DATA, "nothing supplied fails");
   check(ciso_open_step(c, &req) == CISO_ERROR_DATA, "and stays failed");
   ciso_free(c);

   make(0x8000, used, 4, &s, &disc);
   s.shorten = false;
   c = open_src(&s, &err);
   check(err == CISO_OK, "state image opens");
   ciso_read_begin(c, 0, buf, sizeof(buf));
   check(ciso_read_step(c, &req) == CISO_PENDING, "read asks");
   check(ciso_feed(c, 0) == CISO_ERROR_DATA, "read supplied nothing");
   check(ciso_read_step(c, &req) == CISO_ERROR_DATA, "read failed");
   check(ciso_read_step(c, &req) == CISO_ERROR_STATE, "read disarmed");
   check(read_src(c, &s, 4 * 0x8000, buf, sizeof(buf), &outside)
         == CISO_OK && ciso_read_progress(c) == 0, "read past the end");
   check(read_src(c, &s, 0, buf, sizeof(buf), &outside) == CISO_OK
         && !memcmp(buf, disc, sizeof(buf)), "rearmed read");
   ciso_free(c);
   free(disc);
   free(s.file);
}

int main(void)
{
   static uint8_t used[64];
   unsigned i;

   rng_state = 0x2545F491u;

   for (i = 0; i < 64; i++)
      used[i] = 1;
   image_lane(0x8000, used, 64, 1, "all stored, 32 KiB");

   memset(used, 0, sizeof(used));
   used[0] = used[1] = used[2] = 1;
   used[5] = 1;
   used[7] = used[8] = 1;
   used[20] = 1;
   image_lane(0x8000, used, 21, 4, "runs and gaps, 32 KiB");
   image_lane(0x200000, used, 21, 4, "runs and gaps, 2 MiB");

   used[0] = 0;
   used[1] = 0;
   image_lane(0x10000, used, 21, 4, "block 0 left out, 64 KiB");

   memset(used, 0, sizeof(used));
   for (i = 0; i < 9; i += 2)
      used[i] = 1;
   image_lane(0x400000, used, 9, 5, "alternating, 4 MiB");
   image_lane(0x18000, used, 9, 5, "alternating, 96 KiB");

   edge_lane();

   bad_heads();
   state_lane();

   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("ciso_test: all passed\n");
   return 0;
}
