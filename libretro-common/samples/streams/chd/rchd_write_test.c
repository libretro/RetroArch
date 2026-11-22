/* rchd_write: images MAME 0.139 wrote, read and written by rchd.
 *
 * The fixtures in rchd_write_fixtures.h are a compressed hard disk, the
 * diff MAME made against it and that diff after four hunk writes, and
 * an A/V image. This checks that
 *
 *  - every hunk of the hard disk reads as the generator made it, the
 *    self reference and the all-zero hunk included, with the header's
 *    fields, the metadata chain and the unit its GDDD names;
 *  - MAME's diff reads through the bound parent, both supply routes
 *    (rchd_feed, and rchd_read_pending with rchd_feed_at) reaching the
 *    parent, and the parent's hunks found by hunk rather than by the
 *    512-byte unit the metadata implies;
 *  - rchd_write_diff makes the file MAME made, byte for byte, and
 *    rchd_update_hunk turns it into the file MAME's writes did -
 *    appending, then writing over a hunk it already holds - reading
 *    back what was written;
 *  - each A/V field decodes to the frame the generator encoded, the
 *    map's CRC agreeing;
 *  - version 1 and 2 headers give their geometry as GDDD metadata and
 *    their MD5s through rchd_info; and
 *  - rchd_update_hunk refuses an image not flagged writeable, a version
 *    5 one, and one with a read armed.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <encodings/crc32.h>
#include <formats/rchd.h>

#include "rchd_write_fixtures.h"

/* as the fixtures were generated */
#define HD_HUNK    1024u
#define HD_HUNKS   8u
#define AV_W       32u
#define AV_H       8u
#define AV_CH      2u
#define AV_FIELDS  4u
#define AV_HUNK    1092u
#define AV_SAMPLES 134u

static int fails;

#define CHECK(c, what) do { if (!(c)) { printf("FAIL: %s\n", what); fails++; } } while (0)

typedef struct image
{
   uint8_t *data;
   size_t   size;
} image_t;

static int sink(void *ctx, uint64_t offset, const void *data, size_t len)
{
   image_t *im = (image_t*)ctx;
   if (offset + len > im->size)
   {
      uint8_t *grown = (uint8_t*)realloc(im->data, (size_t)(offset + len));
      if (!grown)
         return 0;
      memset(grown + im->size, 0, (size_t)(offset + len) - im->size);
      im->data = grown;
      im->size = (size_t)(offset + len);
   }
   memcpy(im->data + offset, data, len);
   return 1;
}

static void image_set(image_t *im, const unsigned char *data, size_t len)
{
   im->data = (uint8_t*)malloc(len);
   im->size = len;
   memcpy(im->data, data, len);
}

/* Satisfies the decoder from @self, or @parent for the parent's bytes.
 * With @by_offset, reads go through rchd_read_pending and
 * rchd_feed_at rather than the request and rchd_feed. */
static int drive(rchd_t *chd, int opening, const image_t *self,
      const image_t *parent, int by_offset)
{
   for (;;)
   {
      rchd_request_t req;
      const image_t *from;
      int err = opening ? rchd_open_step(chd, &req) : rchd_read_step(chd, &req);
      if (err != RCHD_PENDING)
         return err;
      if (by_offset && !opening)
      {
         rchd_request_t pend;
         if (rchd_read_pending(chd, &pend, 1) != 1
               || pend.offset != req.offset || pend.source != req.source)
            return RCHD_ERROR_STATE;
      }
      from = (req.source == RCHD_SOURCE_PARENT) ? parent : self;
      if (!from || req.offset + req.length > from->size)
         return RCHD_ERROR_DATA;
      err = (by_offset && !opening)
         ? rchd_feed_at(chd, req.offset, req.source,
               from->data + req.offset, req.length)
         : rchd_feed(chd, from->data + req.offset, req.length);
      if (err != RCHD_OK)
         return err;
   }
}

static rchd_t *open_image(const image_t *im)
{
   rchd_t *chd = rchd_new();
   if (chd && drive(chd, 1, im, NULL, 0) != RCHD_OK)
   {
      rchd_free(chd);
      return NULL;
   }
   return chd;
}

static int read_hunk(rchd_t *chd, uint32_t hunk, uint8_t *dst,
      const image_t *self, const image_t *parent, int by_offset)
{
   int err = rchd_read_hunk_begin(chd, hunk, dst);
   return err != RCHD_OK ? err : drive(chd, 0, self, parent, by_offset);
}

/* -------- the hard disk -------- */

static void hd_hunk(uint8_t *p, uint32_t hunk)
{
   static const char text[] = "MAME 2010 hard disk ";
   uint32_t i, x;

   if (hunk == 6)
      hunk = 2;                  /* stored as a reference to hunk 2 */
   x = 12345u + hunk * 7919u;
   if (hunk % 4 == 3)
   {
      memset(p, 0, HD_HUNK);
      return;
   }
   for (i = 0; i < HD_HUNK; i++)
   {
      x    = x * 1103515245u + 12345u;
      p[i] = (hunk % 4 == 1) ? (uint8_t)(x >> 24)
                             : (uint8_t)(text[i % 20] ^ (i / 512));
   }
}

/* the writes made to the diff, in order */
static const uint32_t diff_writes[4] = { 2, 0, 2, 7 };

static void diff_write_data(uint8_t *p, unsigned n)
{
   memset(p, 0xA0 + n, HD_HUNK);
   p[0] = (uint8_t)diff_writes[n];
}

/* What the diff holds after every write: the last one to each hunk,
 * the parent's hunk elsewhere. */
static void diff_hunk(uint8_t *p, uint32_t hunk)
{
   int n, last = -1;
   for (n = 0; n < 4; n++)
      if (diff_writes[n] == hunk)
         last = n;
   if (last < 0)
      hd_hunk(p, hunk);
   else
      diff_write_data(p, (unsigned)last);
}

static void test_parent(rchd_t *parent, const image_t *im)
{
   static uint8_t buf[HD_HUNK], want[HD_HUNK];
   const rchd_info_t     *info = rchd_info(parent);
   const rchd_metadata_t *m;
   uint32_t h, crc;
   int bad = 0;

   CHECK(info && info->version == 4, "parent is version 4");
   if (!info)
      return;
   CHECK(info->hunk_count == HD_HUNKS && info->hunk_bytes == HD_HUNK,
         "parent geometry");
   CHECK(info->flags == 0 && !info->has_parent, "parent has no flags");
   CHECK(info->unit_bytes == 512, "parent unit is GDDD's BPS");
   CHECK(rchd_metadata_count(parent) == 3, "parent has 3 metadata entries");
   m = rchd_metadata_find(parent, RCHD_META_HARD_DISK, 0);
   CHECK(m && m->flags == 1 && m->length == 30
         && !memcmp(m->data, "CYLS:5,HEADS:2,SECS:4,BPS:512", 30),
         "parent GDDD metadata");

   for (h = 0; h < HD_HUNKS; h++)
   {
      hd_hunk(want, h);
      if (read_hunk(parent, h, buf, im, NULL, 0) != RCHD_OK
            || memcmp(buf, want, HD_HUNK))
         bad++;
      if (rchd_hunk_crc32(parent, h, &crc) != RCHD_OK
            || crc != encoding_crc32(0, want, HD_HUNK))
         bad++;
   }
   CHECK(bad == 0, "parent hunks and their CRCs");
}

static void test_mame_diff(rchd_t *parent, const image_t *parent_im)
{
   static uint8_t buf[HD_HUNK], want[HD_HUNK];
   image_t  im;
   rchd_t  *diff;
   uint32_t h;
   int      route, bad = 0;

   image_set(&im, mame_hd_diff, sizeof(mame_hd_diff));
   diff = open_image(&im);
   CHECK(diff != NULL, "MAME's diff opens");
   if (!diff)
   {
      free(im.data);
      return;
   }
   CHECK(rchd_info(diff)->flags == (RCHD_FLAG_HAS_PARENT | RCHD_FLAG_WRITEABLE),
         "MAME's diff is writeable and has a parent");
   CHECK(rchd_parent_sha1_matches(diff, rchd_info(parent)->sha1),
         "MAME's diff names its parent");
   CHECK(rchd_read_hunk_begin(diff, 1, buf) == RCHD_OK
         && drive(diff, 0, &im, parent_im, 0) == RCHD_ERROR_NO_PARENT,
         "a parent hunk needs the parent bound");
   CHECK(rchd_set_parent(diff, parent) == RCHD_OK, "parent binds");

   for (route = 0; route < 2; route++)
      for (h = 0; h < HD_HUNKS; h++)
      {
         diff_hunk(want, h);
         if (read_hunk(diff, h, buf, &im, parent_im, route) != RCHD_OK
               || memcmp(buf, want, HD_HUNK))
            bad++;
      }
   CHECK(bad == 0, "MAME's diff reads through its parent, both routes");
   rchd_free(diff);
   free(im.data);
}

static void test_write_diff(rchd_t *parent, const image_t *parent_im)
{
   static uint8_t buf[HD_HUNK], want[HD_HUNK];
   image_t  im = { NULL, 0 };
   rchd_t  *diff;
   uint64_t end;
   unsigned n;
   uint32_t h;
   int      bad = 0;

   /* the combined SHA-1 MAME computed, from its own diff's header */
   CHECK(rchd_write_diff(parent, mame_hd_diff_new + 48, sink, &im) == RCHD_OK,
         "rchd_write_diff");
   CHECK(im.size == sizeof(mame_hd_diff_new)
         && !memcmp(im.data, mame_hd_diff_new, im.size),
         "new diff is the file MAME made");

   diff = open_image(&im);
   CHECK(diff != NULL, "new diff opens");
   if (!diff)
   {
      free(im.data);
      return;
   }
   rchd_set_parent(diff, parent);
   for (h = 0; h < HD_HUNKS; h++)
   {
      hd_hunk(want, h);
      if (read_hunk(diff, h, buf, &im, parent_im, 0) != RCHD_OK
            || memcmp(buf, want, HD_HUNK))
         bad++;
   }
   CHECK(bad == 0, "new diff reads as its parent");

   end = im.size;
   for (n = 0; n < 4; n++)
   {
      diff_write_data(buf, n);
      if (rchd_update_hunk(diff, diff_writes[n], buf, &end, sink, &im)
            != RCHD_OK)
         bad++;
      /* the hunk reads back at once, before anything else is read */
      if (read_hunk(diff, diff_writes[n], want, &im, parent_im, 0) != RCHD_OK
            || memcmp(want, buf, HD_HUNK))
         bad++;
   }
   CHECK(bad == 0, "rchd_update_hunk writes read back");
   CHECK(end == im.size, "file end follows the appends");
   CHECK(im.size == sizeof(mame_hd_diff)
         && !memcmp(im.data, mame_hd_diff, im.size),
         "written diff is the file MAME's writes made");

   for (h = 0; h < HD_HUNKS; h++)
   {
      diff_hunk(want, h);
      if (read_hunk(diff, h, buf, &im, parent_im, 0) != RCHD_OK
            || memcmp(buf, want, HD_HUNK))
         bad++;
   }
   CHECK(bad == 0, "written diff reads back");

   /* refusals: a read armed, and an image not flagged writeable */
   CHECK(rchd_read_hunk_begin(diff, 1, buf) == RCHD_OK
         && rchd_update_hunk(diff, 1, buf, &end, sink, &im) == RCHD_ERROR_STATE,
         "no update while a read is armed");
   drive(diff, 0, &im, parent_im, 0);
   CHECK(rchd_update_hunk(parent, 1, buf, &end, sink, &im)
         == RCHD_ERROR_UNSUPPORTED, "no update to an image not writeable");
   rchd_free(diff);
   free(im.data);
}

/* -------- A/V -------- */

/* the raw field the generator encoded, padded with zeros to the hunk */
static void av_field(uint8_t *p, uint32_t f)
{
   uint32_t samples  = AV_SAMPLES - (f & 1);
   uint32_t metasize = 7 + f;
   uint32_t i, c, x, y;

   memset(p, 0, AV_HUNK);
   p[0] = 'c'; p[1] = 'h'; p[2] = 'a'; p[3] = 'v';
   p[4] = (uint8_t)metasize;
   p[5] = AV_CH;
   p[6] = (uint8_t)(samples >> 8);
   p[7] = (uint8_t)samples;
   p[9] = AV_W;
   p[11] = AV_H;
   p += 12;
   for (i = 0; i < metasize; i++)
      *p++ = (uint8_t)(0x40 + f * 3 + i);
   for (c = 0; c < AV_CH; c++)
      for (i = 0; i < samples; i++)
      {
         int32_t  v = (int32_t)((i * (c ? 211u : 97u) + f * 1009u) % 4001u) * 4
                    - 8000 + (int32_t)(i * 37 % 11) - 5;
         uint16_t s = (uint16_t)(int16_t)v;
         *p++ = (uint8_t)(s >> 8);
         *p++ = (uint8_t)s;
      }
   for (y = 0; y < AV_H; y++)
      for (x = 0; x < AV_W; x++)
      {
         *p++ = (uint8_t)((x & 1) ? 128 + y * 3 - f : 128 - x + f);
         *p++ = (uint8_t)(16 + ((x * 3 + y * 5 + f * 11) % 200)
               + ((x * y + f) % 7));
      }
}

static void test_av(void)
{
   static uint8_t buf[AV_HUNK], want[AV_HUNK];
   image_t          im;
   rchd_t          *av;
   rchd_av_frame_t  frame;
   uint32_t         f, crc;
   int              bad = 0;

   image_set(&im, mame_av, sizeof(mame_av));
   av = open_image(&im);
   CHECK(av && rchd_info(av)->compressors[0] == RCHD_CODEC_AVHUFF
         && rchd_info(av)->hunk_bytes == AV_HUNK, "A/V image opens");
   if (!av)
   {
      free(im.data);
      return;
   }
   for (f = 0; f < AV_FIELDS; f++)
   {
      av_field(want, f);
      if (read_hunk(av, f, buf, &im, NULL, 0) != RCHD_OK
            || memcmp(buf, want, AV_HUNK))
         bad++;
      if (rchd_hunk_crc32(av, f, &crc) != RCHD_OK
            || crc != encoding_crc32(0, want, AV_HUNK))
         bad++;
      if (rchd_av_parse(buf, AV_HUNK, &frame) != RCHD_OK
            || frame.width != AV_W || frame.height != AV_H
            || frame.channels != AV_CH
            || frame.samples != AV_SAMPLES - (f & 1)
            || frame.meta_size != 7 + f)
         bad++;
   }
   CHECK(bad == 0, "A/V fields decode as encoded");
   rchd_free(av);
   free(im.data);
}

/* -------- versions 1 and 2 -------- */

static void be32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
   p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

/* Two cylinders, one head, two sectors, one sector to a hunk, stored
 * uncompressed after the header and an 8-byte map. */
static void test_v12(uint32_t version)
{
   uint32_t seclen = (version == 2) ? 256 : 512;
   uint32_t hlen   = (version == 2) ? 80 : 76;
   uint32_t data   = hlen + 5 * 8;
   image_t  im;
   rchd_t  *chd;
   const rchd_metadata_t *m;
   char     want[64];
   uint32_t i;

   im.size = data + 4 * seclen;
   im.data = (uint8_t*)calloc(1, im.size);
   memcpy(im.data, "MComprHD", 8);
   be32(im.data + 8, hlen);
   be32(im.data + 12, version);
   be32(im.data + 24, 1);           /* sectors to a hunk */
   be32(im.data + 28, 4);           /* hunks */
   be32(im.data + 32, 2);           /* cylinders */
   be32(im.data + 36, 1);           /* heads */
   be32(im.data + 40, 2);           /* sectors */
   for (i = 0; i < 16; i++)
   {
      im.data[44 + i] = (uint8_t)(0x10 + i);    /* MD5 */
      im.data[60 + i] = (uint8_t)(0x80 + i);    /* parent MD5 */
   }
   if (version == 2)
      be32(im.data + 76, seclen);
   for (i = 0; i < 4; i++)
   {
      /* a 44-bit offset and a 20-bit length in one word */
      uint8_t *e  = im.data + hlen + i * 8;
      uint32_t at = data + i * seclen;
      be32(e, seclen << 12);
      be32(e + 4, at);
   }
   memcpy(im.data + hlen + 4 * 8, "EndOfLis", 8);

   chd = open_image(&im);
   CHECK(chd != NULL, version == 2 ? "v2 image opens" : "v1 image opens");
   if (chd)
   {
      sprintf(want, "CYLS:2,HEADS:1,SECS:2,BPS:%u", (unsigned)seclen);
      m = rchd_metadata_find(chd, RCHD_META_HARD_DISK, 0);
      CHECK(m && m->length == strlen(want) + 1 && !memcmp(m->data, want, m->length),
            version == 2 ? "v2 geometry as GDDD" : "v1 geometry as GDDD");
      CHECK(rchd_info(chd)->md5[0] == 0x10 && rchd_info(chd)->md5[15] == 0x1f
            && rchd_info(chd)->parent_md5[0] == 0x80
            && rchd_info(chd)->parent_md5[15] == 0x8f,
            version == 2 ? "v2 MD5s" : "v1 MD5s");
      rchd_free(chd);
   }
   free(im.data);
}

/* a version 5 image is not one rchd_update_hunk writes */
static void test_v5_refused(void)
{
   static uint8_t hunk[HD_HUNK];
   image_t        im = { NULL, 0 };
   rchd_writer_t *w  = rchd_write_new(HD_HUNK, HD_HUNK, 512, sink, &im);
   rchd_t        *chd;
   uint64_t       end;

   memset(hunk, 0x5a, sizeof(hunk));
   if (w)
   {
      rchd_write_hunk(w, hunk, HD_HUNK);
      rchd_write_finish(w);
      rchd_write_free(w);
   }
   chd = open_image(&im);
   CHECK(chd != NULL, "v5 image opens");
   if (chd)
   {
      end = im.size;
      CHECK(rchd_update_hunk(chd, 0, hunk, &end, sink, &im)
            == RCHD_ERROR_UNSUPPORTED, "no update to a v5 image");
      rchd_free(chd);
   }
   free(im.data);
}

int main(void)
{
   image_t parent_im;
   rchd_t *parent;

   image_set(&parent_im, mame_hd_parent, sizeof(mame_hd_parent));
   parent = open_image(&parent_im);
   CHECK(parent != NULL, "parent opens");
   if (parent)
   {
      test_parent(parent, &parent_im);
      test_mame_diff(parent, &parent_im);
      test_write_diff(parent, &parent_im);
      rchd_free(parent);
   }
   free(parent_im.data);

   test_av();
   test_v12(1);
   test_v12(2);
   test_v5_refused();

   if (fails)
   {
      printf("rchd_write_test: %d failure(s)\n", fails);
      return 1;
   }
   printf("rchd_write_test: PASS\n");
   return 0;
}
