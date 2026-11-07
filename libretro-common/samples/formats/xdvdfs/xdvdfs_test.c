/* xdvdfs_test: makes up Xbox and Xbox 360 disc images and checks that
 * formats/xdvdfs finds their game partition, looks files up in their
 * directory trees and reads the title of the executable they boot -
 * default.xbe or default.xex - and of an XEX or STFS package on its
 * own.
 *
 * The images are a trimmed XISO and whole discs with the game
 * partition where XGD1, XGD2 and XGD3 keep it, held sparsely: a few
 * runs of octets, zeros elsewhere. Ranges are supplied short now and
 * then. Every octet the reader asks for is counted, and the counts are
 * checked to be exactly what an operation needs: an entry's head and
 * as much of its name as decides the order, an executable's head and
 * the fields that lead to the title - nothing read ahead. A file that
 * is no Xbox executable is told after its head alone. Broken trees,
 * executables and supplies must fail without hanging. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <formats/xdvdfs.h>

#define SECTOR   2048
#define MAX_RUNS 16
#define RUN_SIZE (16 * SECTOR)

static int failures = 0;
static uint32_t rng_state = 0x1234567u;

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

/* An image: runs of octets at offsets, zeros between them */
typedef struct
{
   uint64_t size;
   uint64_t at[MAX_RUNS];
   uint8_t *run[MAX_RUNS];
   unsigned n;
   bool     shorten;
   unsigned requests;
   size_t   asked;
} image_t;

static uint8_t *at(image_t *im, uint64_t off)
{
   unsigned i;
   for (i = 0; i < im->n; i++)
      if (off >= im->at[i] && off < im->at[i] + RUN_SIZE)
         return im->run[i] + (size_t)(off - im->at[i]);
   im->at[im->n]  = off - off % SECTOR;
   im->run[im->n] = (uint8_t*)calloc(1, RUN_SIZE);
   return im->run[im->n++] + (size_t)(off % SECTOR);
}

static void put(image_t *im, uint64_t off, const void *d, size_t n)
{
   memcpy(at(im, off), d, n);
}

static void put_be32(image_t *im, uint64_t off, uint32_t v)
{
   uint8_t b[4];
   b[0] = (uint8_t)(v >> 24);
   b[1] = (uint8_t)(v >> 16);
   b[2] = (uint8_t)(v >> 8);
   b[3] = (uint8_t)v;
   put(im, off, b, 4);
}

static void put32(image_t *im, uint64_t off, uint32_t v)
{
   uint8_t b[4];
   b[0] = (uint8_t)v;
   b[1] = (uint8_t)(v >> 8);
   b[2] = (uint8_t)(v >> 16);
   b[3] = (uint8_t)(v >> 24);
   put(im, off, b, 4);
}

static size_t pull(image_t *im, const xdvdfs_request_t *req)
{
   size_t n = req->length, i;
   im->requests++;
   im->asked += req->length;
   if (req->offset >= im->size)
      return 0;
   if (n > im->size - req->offset)
      n = (size_t)(im->size - req->offset);
   if (im->shorten && n > 1 && (rnd() & 1))
      n = 1 + rnd() % (n - 1);
   for (i = 0; i < n; i++)
   {
      uint64_t off = req->offset + i;
      unsigned r;
      req->dst[i] = 0;
      for (r = 0; r < im->n; r++)
         if (off >= im->at[r] && off < im->at[r] + RUN_SIZE)
            req->dst[i] = im->run[r][off - im->at[r]];
   }
   return n;
}

static void image_free(image_t *im)
{
   unsigned i;
   for (i = 0; i < im->n; i++)
      free(im->run[i]);
}

/* An entry at @off in its directory: subtrees @l and @r in four-octet
 * units, the file at @sector of @size */
static void entry(image_t *im, uint64_t dir, uint32_t off, uint16_t l,
      uint16_t r, uint32_t sector, uint32_t size, uint8_t attr,
      const char *name)
{
   uint8_t h[14];
   h[0]  = (uint8_t)l;
   h[1]  = (uint8_t)(l >> 8);
   h[2]  = (uint8_t)r;
   h[3]  = (uint8_t)(r >> 8);
   h[12] = attr;
   h[13] = (uint8_t)strlen(name);
   put(im, dir + off, h, 4);
   put32(im, dir + off + 4, sector);
   put32(im, dir + off + 8, size);
   put(im, dir + off + 12, h + 12, 2);
   put(im, dir + off + 14, name, strlen(name));
}

/* An XEX at @at: @n optional headers, the execution info's last,
 * with @title, disc @disc of @discs */
static void put_xex(image_t *im, uint64_t at, uint32_t n, uint32_t title,
      uint8_t disc, uint8_t discs)
{
   uint32_t i;
   uint32_t head = 24 + n * 8 + 32;
   uint8_t  d[2];
   put(im, at, "XEX2", 4);
   put_be32(im, at + 8, head);
   put_be32(im, at + 20, n);
   for (i = 0; i + 1 < n; i++)
   {
      put_be32(im, at + 24 + i * 8, 0x00010000u + i);
      put_be32(im, at + 24 + i * 8 + 4, 0x11111111u);
   }
   put_be32(im, at + 24 + i * 8, 0x00040006u);
   put_be32(im, at + 24 + i * 8 + 4, head - 24);
   put_be32(im, at + head - 24 + 12, title);
   d[0] = disc;
   d[1] = discs;
   put(im, at + head - 24 + 18, d, 2);
}

/* The disc: a game partition at @base holding
 *    ALPHA.TXT  default.xbe  media/{a.bin, intro.wmv}  Zed
 * with default.xbe's title @title, or for an Xbox 360 disc default.xex
 * in its place, disc 1 of 2 */
static void make_disc_for(image_t *im, uint64_t base, uint64_t size,
      uint32_t title, bool x360)
{
   uint64_t root  = base + 33 * SECTOR;
   uint64_t media = base + 40 * SECTOR;
   uint64_t xbe   = base + 50 * SECTOR;

   memset(im, 0, sizeof(*im));
   im->size = size;
   put(im, base + 32 * SECTOR, "MICROSOFT*XBOX*MEDIA", 20);
   put32(im, base + 32 * SECTOR + 0x14, 33);
   put32(im, base + 32 * SECTOR + 0x18, SECTOR);
   put(im, base + 32 * SECTOR + 0x7EC, "MICROSOFT*XBOX*MEDIA", 20);

   /* default.xbe at 0, ALPHA.TXT at 28 left of it, media at 52 right
    * of it, Zed at 72 right of media */
   entry(im, root, 0,  7, 13, 50, 0x1000, 0x20,
         x360 ? "default.xex" : "default.xbe");
   entry(im, root, 28, 0, 0,  60, 5,      0x20, "ALPHA.TXT");
   entry(im, root, 52, 0, 18, 40, SECTOR, 0x10, "media");
   entry(im, root, 72, 0, 0,  61, 7,      0x20, "Zed");
   entry(im, media, 0,  0, 5, 62, 9,      0x20, "a.bin");
   entry(im, media, 20, 0, 0, 63, 0x777,  0x20, "intro.wmv");

   if (x360)
   {
      put_xex(im, xbe, 3, title, 1, 2);
      return;
   }
   put(im, xbe, "XBEH", 4);
   put32(im, xbe + 0x104, 0x10000);
   put32(im, xbe + 0x118, 0x10000 + 0x178);
   put32(im, xbe + 0x178 + 8, title);
}

static void make_disc(image_t *im, uint64_t base, uint64_t size,
      uint32_t title)
{
   make_disc_for(im, base, size, title, false);
}

static int open_image(image_t *im, xdvdfs_t **out)
{
   xdvdfs_request_t req;
   int err;
   *out = xdvdfs_new(im->size);
   while ((err = xdvdfs_open_step(*out, &req)) == XDVDFS_PENDING)
      xdvdfs_feed(*out, pull(im, &req));
   return err;
}

static int lookup(image_t *im, xdvdfs_t *x, const char *path,
      xdvdfs_extent_t *ext)
{
   xdvdfs_request_t req;
   int err;
   if ((err = xdvdfs_lookup_begin(x, path)) != XDVDFS_OK)
      return err;
   while ((err = xdvdfs_lookup_step(x, &req, ext)) == XDVDFS_PENDING)
      xdvdfs_feed(x, pull(im, &req));
   return err;
}

static int title_of(image_t *im, xdvdfs_t *x, const xdvdfs_extent_t *exe,
      xdvdfs_title_t *t)
{
   xdvdfs_request_t req;
   int err;
   if ((err = xdvdfs_title_begin(x, exe)) != XDVDFS_OK)
      return err;
   while ((err = xdvdfs_title_step(x, &req, t)) == XDVDFS_PENDING)
      xdvdfs_feed(x, pull(im, &req));
   return err;
}

static int title(image_t *im, xdvdfs_t *x, uint32_t *id)
{
   xdvdfs_title_t t;
   int err = title_of(im, x, NULL, &t);
   if (err == XDVDFS_OK)
      *id = t.platform == XDVDFS_XBOX ? t.id : 0;
   return err;
}

static void disc_lane(uint64_t base, uint64_t size, const char *what)
{
   char            name[160];
   image_t         im;
   xdvdfs_extent_t ext;
   xdvdfs_t       *x;
   uint32_t        id = 0;
   int             pass;

   for (pass = 0; pass < 2; pass++)
   {
      make_disc(&im, base, size, 0x4D530004u);
      im.shorten = pass == 1;

      sprintf(name, "%s%s: opens", what, pass ? " (short)" : "");
      check(open_image(&im, &x) == XDVDFS_OK, name);
      sprintf(name, "%s: partition", what);
      check(xdvdfs_partition(x) == base, name);

      im.asked = 0;
      sprintf(name, "%s: title", what);
      check(title(&im, x, &id) == XDVDFS_OK && id == 0x4D530004u, name);
      /* default.xex looked for at the root entry and right of it,
       * default.xbe found at the root entry, then the XBE's head, its
       * base and certificate addresses, the ID */
      sprintf(name, "%s: title reads only what it needs", what);
      check(pass || im.asked == 25 + 25 + 25 + 24 + 24 + 4, name);

      sprintf(name, "%s: nested lookup", what);
      check(   lookup(&im, x, "\\MEDIA/Intro.WMV", &ext) == XDVDFS_OK
            && ext.offset == base + 63 * (uint64_t)SECTOR
            && ext.size == 0x777, name);
      sprintf(name, "%s: left and right", what);
      check(   lookup(&im, x, "alpha.txt", &ext) == XDVDFS_OK
            && ext.size == 5
            && lookup(&im, x, "zed", &ext) == XDVDFS_OK
            && ext.size == 7, name);
      sprintf(name, "%s: missing files", what);
      check(   lookup(&im, x, "zz", &ext) == XDVDFS_ERROR_NOT_FOUND
            && lookup(&im, x, "default.xb", &ext) == XDVDFS_ERROR_NOT_FOUND
            && lookup(&im, x, "default.xbe2", &ext) == XDVDFS_ERROR_NOT_FOUND
            && lookup(&im, x, "alpha.txt/x", &ext) == XDVDFS_ERROR_NOT_FOUND
            && lookup(&im, x, "media/nope", &ext) == XDVDFS_ERROR_NOT_FOUND,
            name);

      xdvdfs_free(x);
      image_free(&im);
   }
}

static void x360_lanes(void)
{
   image_t         im;
   xdvdfs_t       *x;
   xdvdfs_title_t  t;
   xdvdfs_extent_t ext;
   int             pass;

   for (pass = 0; pass < 2; pass++)
   {
      make_disc_for(&im, 0xFD90000, 0xFD90000 + 0x100000, 0x41560817u, true);
      im.shorten = pass == 1;
      check(open_image(&im, &x) == XDVDFS_OK
            && xdvdfs_partition(x) == 0xFD90000, "360 XGD2: opens");
      im.asked = 0;
      check(   title_of(&im, x, NULL, &t) == XDVDFS_OK
            && t.id == 0x41560817u && t.platform == XDVDFS_XBOX360
            && t.disc == 1 && t.discs == 2, "360 XGD2: title of default.xex");
      /* default.xex at the root entry, the XEX's head, its three
       * optional headers, the execution info */
      check(pass || im.asked == 25 + 24 + 3 * 8 + 24,
            "360 XGD2: title reads only what it needs");
      xdvdfs_free(x);
      image_free(&im);
   }

   /* an XEX whose execution info is past the first run of headers */
   memset(&im, 0, sizeof(im));
   im.size = 0x10000;
   put_xex(&im, 0, 40, 0x4D5307E6u, 0, 0);
   x = xdvdfs_new(im.size);
   ext.offset = 0;
   ext.size   = 0x10000;
   im.asked   = 0;
   check(   title_of(&im, x, &ext, &t) == XDVDFS_OK
         && t.id == 0x4D5307E6u && t.platform == XDVDFS_XBOX360,
         "XEX on its own, many optional headers");
   check(im.asked == 24 + 40 * 8 + 24, "XEX on its own: headers once each");
   xdvdfs_free(x);
   image_free(&im);

   /* an STFS package */
   memset(&im, 0, sizeof(im));
   im.size = 0x10000;
   put(&im, 0, "LIVE", 4);
   put_be32(&im, 0x360, 0x584108A1u);
   {
      uint8_t d[4] = { 2, 1, 2, 3 };
      put(&im, 0x364, d, 4);
   }
   x = xdvdfs_new(im.size);
   im.asked = 0;
   check(   title_of(&im, x, &ext, &t) == XDVDFS_OK
         && t.id == 0x584108A1u && t.disc == 2 && t.discs == 3
         && im.asked == 24 + 8, "STFS package");
   xdvdfs_free(x);
   image_free(&im);

   /* not an Xbox executable: an Atari 8-bit one */
   memset(&im, 0, sizeof(im));
   im.size = 0x1000;
   {
      static const uint8_t atari[6] = { 0xFF, 0xFF, 0x00, 0x20, 0xFF, 0x20 };
      put(&im, 0, atari, sizeof(atari));
   }
   x = xdvdfs_new(im.size);
   im.asked = 0;
   check(   title_of(&im, x, &ext, &t) == XDVDFS_ERROR_NOT_FOUND
         && im.asked == 24, "other executable told by its head");
   xdvdfs_free(x);
   image_free(&im);

   /* an empty root: one sector of padding */
   make_disc(&im, 0, 0x100000, 1);
   {
      uint8_t ff[64];
      memset(ff, 0xFF, sizeof(ff));
      put(&im, 33 * SECTOR, ff, sizeof(ff));
   }
   open_image(&im, &x);
   check(title_of(&im, x, NULL, &t) == XDVDFS_ERROR_NOT_FOUND,
         "empty root holds nothing");
   xdvdfs_free(x);
   image_free(&im);

   /* a root too small to hold an entry is no partition */
   make_disc(&im, 0, 0x100000, 1);
   put32(&im, 32 * SECTOR + 0x18, 0);
   check(open_image(&im, &x) == XDVDFS_ERROR_NOT_FOUND, "empty root size");
   xdvdfs_free(x);
   image_free(&im);
}

static void broken_lanes(void)
{
   image_t         im;
   xdvdfs_extent_t ext;
   xdvdfs_request_t req;
   xdvdfs_t       *x;
   uint32_t        id;

   /* too small for any partition: nothing is asked for */
   memset(&im, 0, sizeof(im));
   im.size = 32 * SECTOR + 8;
   check(open_image(&im, &x) == XDVDFS_ERROR_NOT_FOUND && !im.requests,
         "no room for a partition");
   xdvdfs_free(x);

   /* no magic anywhere */
   memset(&im, 0, sizeof(im));
   im.size = 0x20000000;
   check(open_image(&im, &x) == XDVDFS_ERROR_NOT_FOUND && im.requests == 6,
         "no partition: each place looked at once");
   check(xdvdfs_title_begin(x, NULL) == XDVDFS_ERROR_STATE,
         "title before open");
   xdvdfs_free(x);
   image_free(&im);

   /* a tree that loops */
   make_disc(&im, 0, 0x100000, 1);
   entry(&im, 33 * SECTOR, 72, 0, 13, 61, 7, 0x20, "Zed");
   open_image(&im, &x);
   check(lookup(&im, x, "zzz", &ext) == XDVDFS_ERROR_DATA,
         "looping tree ends");
   xdvdfs_free(x);
   image_free(&im);

   /* a name running off its directory */
   make_disc(&im, 0, 0x100000, 1);
   put32(&im, 32 * SECTOR + 0x18, 20);
   open_image(&im, &x);
   check(lookup(&im, x, "default.xbe", &ext) == XDVDFS_ERROR_DATA,
         "entry past its directory");
   xdvdfs_free(x);
   image_free(&im);

   /* an XBE that is not one, and a certificate outside it */
   make_disc(&im, 0, 0x100000, 1);
   put(&im, 50 * SECTOR, "XBEX", 4);
   open_image(&im, &x);
   check(title(&im, x, &id) == XDVDFS_ERROR_NOT_FOUND,
         "no executable's magic");
   put(&im, 50 * SECTOR, "XBEH", 4);
   put32(&im, 50 * SECTOR + 0x118, 0x10000 + 0x1000);
   check(title(&im, x, &id) == XDVDFS_ERROR_DATA, "certificate outside");
   put32(&im, 50 * SECTOR + 0x118, 0x1000);
   check(title(&im, x, &id) == XDVDFS_ERROR_DATA, "certificate below base");
   xdvdfs_free(x);
   image_free(&im);

   /* supplies */
   make_disc(&im, 0, 0x100000, 1);
   x = xdvdfs_new(im.size);
   check(xdvdfs_feed(x, 1) == XDVDFS_ERROR_STATE, "feed with nothing asked");
   check(xdvdfs_open_step(x, &req) == XDVDFS_PENDING, "open asks");
   check(xdvdfs_feed(x, 0) == XDVDFS_ERROR_DATA, "nothing supplied fails");
   check(xdvdfs_open_step(x, &req) == XDVDFS_ERROR_DATA, "and stays failed");
   xdvdfs_free(x);
   open_image(&im, &x);
   xdvdfs_lookup_begin(x, "media/a.bin");
   check(xdvdfs_lookup_step(x, &req, &ext) == XDVDFS_PENDING, "lookup asks");
   xdvdfs_feed(x, 0);
   check(xdvdfs_lookup_step(x, &req, &ext) == XDVDFS_ERROR_DATA
         && xdvdfs_lookup_step(x, &req, &ext) == XDVDFS_ERROR_STATE,
         "lookup failed, then disarmed");
   check(lookup(&im, x, "media/a.bin", &ext) == XDVDFS_OK && ext.size == 9,
         "rearmed lookup");
   xdvdfs_free(x);
   image_free(&im);
}

int main(void)
{
   disc_lane(0,          0x100000,              "XISO");
   disc_lane(0x18300000, 0x18300000 + 0x100000, "XGD1");
   disc_lane(0xFD90000,  0xFD90000 + 0x100000,  "XGD2");
   disc_lane(0x2080000,  0x2080000 + 0x100000,  "XGD3");
   disc_lane(0xFB20,     0xFB20 + 0x100000,     "0xFB20");
   x360_lanes();
   broken_lanes();

   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("xdvdfs_test: all passed\n");
   return 0;
}
