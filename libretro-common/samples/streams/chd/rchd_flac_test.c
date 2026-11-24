/* rchd's FLAC codecs, over images rchd's own writer builds with hunks
 * coded here:
 *
 *   - cdfl: each sector's samples land big-endian, whatever the host,
 *     with the subchannel deflated behind the FLAC frames interleaved
 *     after its sector;
 *   - flac: a hunk marked 'L' lands little-endian and one marked 'B'
 *     big-endian;
 *   - A/V: every channel lands big-endian after metadata of odd and of
 *     even length, with the sample count moving between hunks;
 *
 * each read into an aligned buffer, into one on an odd byte, and in
 * place, all three giving the same bytes. The FLAC frames are verbatim
 * subframes and the deflate streams stored blocks, which is all a
 * decoder needs to be handed known samples. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <formats/rchd.h>
#include <encodings/crc32.h>

#define CD_FRAME    2448
#define CD_SECTOR   2352
#define CD_SUBCODE    96
#define CD_FRAMES      4
#define CD_HUNK     (CD_FRAMES * CD_FRAME)
#define CD_HUNKS       3

#define FL_HUNK     4096
#define FL_HUNKS       2

#define AV_CHANNELS    2
#define AV_HUNKS       3
#define AV_HUNK     (12 + 3 + AV_CHANNELS * 736 * 2 + 8)

static int fails;

static uint8_t *image;
static size_t   image_len;

static void check(int ok, const char *what)
{
   if (!ok)
   {
      printf("FAIL: %s\n", what);
      fails++;
   }
}

static int sink(void *ctx, uint64_t offset, const void *data, size_t len)
{
   (void)ctx;
   if (offset + len > image_len)
   {
      uint8_t *grown = (uint8_t*)realloc(image, (size_t)(offset + len));
      if (!grown)
         return 0;
      memset(grown + image_len, 0, (size_t)(offset + len) - image_len);
      image     = grown;
      image_len = (size_t)(offset + len);
   }
   memcpy(image + offset, data, len);
   return 1;
}

static int drive(rchd_t *chd, int opening)
{
   for (;;)
   {
      rchd_request_t req;
      int err = opening ? rchd_open_step(chd, &req) : rchd_read_step(chd, &req);
      if (err != RCHD_PENDING)
         return err;
      if (req.source != RCHD_SOURCE_SELF || req.offset + req.length > image_len)
         return RCHD_ERROR_DATA;
      if (rchd_feed(chd, image + req.offset, req.length) != RCHD_OK)
         return RCHD_ERROR_DATA;
   }
}

/* ---- bits ---- */

typedef struct
{
   uint8_t *b;
   size_t   bit;
} bits_t;

static void put(bits_t *w, uint32_t v, unsigned n)
{
   while (n--)
   {
      if ((v >> n) & 1)
         w->b[w->bit >> 3] |= (uint8_t)(0x80 >> (w->bit & 7));
      w->bit++;
   }
}

static unsigned crc8(const uint8_t *p, size_t n)
{
   unsigned c = 0;
   size_t   i;
   int      k;
   for (i = 0; i < n; i++)
   {
      c ^= p[i];
      for (k = 0; k < 8; k++)
         c = (c & 0x80) ? ((c << 1) ^ 0x07) & 0xff : (c << 1) & 0xff;
   }
   return c;
}

static unsigned crc16_flac(const uint8_t *p, size_t n)
{
   unsigned c = 0;
   size_t   i;
   int      k;
   for (i = 0; i < n; i++)
   {
      c ^= (unsigned)p[i] << 8;
      for (k = 0; k < 8; k++)
         c = (c & 0x8000) ? ((c << 1) ^ 0x8005) & 0xffff : (c << 1) & 0xffff;
   }
   return c;
}

/* One FLAC frame of @block frames of @ch interleaved 16-bit samples, in
 * verbatim subframes, at @out (zeroed by the caller). Returns its size. */
static size_t flac_frame(uint8_t *out, const int16_t *s, unsigned block,
      unsigned ch)
{
   bits_t   w;
   unsigned c, i;

   w.b   = out;
   w.bit = 0;
   put(&w, 0x3FFE, 14); put(&w, 0, 2);
   put(&w, 7, 4);                         /* 16-bit block size follows */
   put(&w, 9, 4);                         /* 44.1 kHz */
   put(&w, ch - 1, 4);
   put(&w, 4, 3);                         /* 16 bits */
   put(&w, 0, 1);
   put(&w, 0, 8);                         /* frame 0 */
   put(&w, block - 1, 16);
   put(&w, crc8(out, w.bit >> 3), 8);
   for (c = 0; c < ch; c++)
   {
      put(&w, 1 << 1, 8);                 /* verbatim */
      for (i = 0; i < block; i++)
         put(&w, (uint16_t)s[i * ch + c], 16);
   }
   w.bit = (w.bit + 7) & ~(size_t)7;
   put(&w, crc16_flac(out, w.bit >> 3), 16);
   return w.bit >> 3;
}

/* Raw deflate holding @n bytes in one stored block. */
static size_t deflate_stored(uint8_t *out, const uint8_t *in, unsigned n)
{
   out[0] = 0x01;                         /* final, stored */
   out[1] = (uint8_t)n;
   out[2] = (uint8_t)(n >> 8);
   out[3] = (uint8_t)~n;
   out[4] = (uint8_t)(~n >> 8);
   memcpy(out + 5, in, n);
   return 5 + n;
}

static void be16(uint8_t *p, unsigned v)
{
   p[0] = (uint8_t)(v >> 8);
   p[1] = (uint8_t)v;
}

static int16_t sample(unsigned seed, unsigned i)
{
   return (int16_t)(uint16_t)(seed * 0x9E37u + i * 0x0101u + (i >> 3) * 0x3313u);
}

/* ---- reading ---- */

static void read_all(const char *name, const uint8_t *expect,
      uint32_t hunks, uint32_t hunk_bytes)
{
   rchd_t  *chd = rchd_new();
   uint8_t *buf = (uint8_t*)malloc(hunk_bytes + 1);
   uint32_t h;
   char     what[128];

   sprintf(what, "%s: opens", name);
   check(chd && buf && drive(chd, 1) == RCHD_OK, what);
   if (!chd || !buf)
      exit(1);
   sprintf(what, "%s: geometry", name);
   check(rchd_info(chd)->hunk_count == hunks
         && rchd_info(chd)->hunk_bytes == hunk_bytes, what);

   for (h = 0; h < hunks; h++)
   {
      const uint8_t *want = expect + (size_t)h * hunk_bytes;
      const uint8_t *in_place;
      int            off;

      for (off = 0; off <= 1; off++)
      {
         memset(buf, 0xA5, hunk_bytes + 1);
         sprintf(what, "%s: hunk %u into a buffer %s", name, (unsigned)h,
               off ? "on an odd byte" : "aligned");
         check(rchd_read_hunk_begin(chd, h, buf + off) == RCHD_OK
               && drive(chd, 0) == RCHD_OK
               && !memcmp(buf + off, want, hunk_bytes), what);
      }

      sprintf(what, "%s: hunk %u in place", name, (unsigned)h);
      check(rchd_read_hunk_begin(chd, h, NULL) == RCHD_OK
            && drive(chd, 0) == RCHD_OK
            && (in_place = rchd_hunk_cache(chd)) != NULL
            && !memcmp(in_place, want, hunk_bytes), what);
   }

   rchd_free(chd);
   free(buf);
}

static rchd_writer_t *writer(uint32_t codec, uint32_t hunks,
      uint32_t hunk_bytes, uint32_t unit_bytes)
{
   uint32_t comp[4];

   comp[0] = codec;
   comp[1] = comp[2] = comp[3] = RCHD_CODEC_NONE;
   free(image);
   image     = NULL;
   image_len = 0;
   return rchd_write_new_coded((uint64_t)hunks * hunk_bytes, hunk_bytes,
         unit_bytes, comp, sink, NULL);
}

/* ---- cdfl ---- */

static void cdfl(void)
{
   static int16_t s[CD_FRAMES * CD_SECTOR / 2];
   static uint8_t blob[CD_HUNK + 256];
   static uint8_t sub[CD_FRAMES * CD_SUBCODE];
   static uint8_t expect[CD_HUNKS * CD_HUNK];
   rchd_writer_t *w = writer(RCHD_CODEC_CD_FLAC, CD_HUNKS, CD_HUNK, CD_FRAME);
   unsigned       h, i;

   check(w != NULL, "cdfl: a writer");
   if (!w)
      return;
   for (h = 0; h < CD_HUNKS; h++)
   {
      uint8_t *x = expect + (size_t)h * CD_HUNK;
      size_t   len;

      for (i = 0; i < CD_FRAMES * CD_SECTOR / 2; i++)
         s[i] = sample(h + 1, i);
      for (i = 0; i < sizeof(sub); i++)
         sub[i] = (uint8_t)(h * 31 + i * 7);

      /* A block of a hunk's whole payload over four: one frame here. */
      memset(blob, 0, sizeof(blob));
      len  = flac_frame(blob, s, CD_FRAMES * CD_SECTOR / 4, 2);
      len += deflate_stored(blob + len, sub, sizeof(sub));

      for (i = 0; i < CD_FRAMES * CD_SECTOR / 2; i++)
      {
         unsigned f = i / (CD_SECTOR / 2);
         be16(x + f * CD_FRAME + (i % (CD_SECTOR / 2)) * 2, (uint16_t)s[i]);
      }
      for (i = 0; i < CD_FRAMES; i++)
         memcpy(x + i * CD_FRAME + CD_SECTOR, sub + i * CD_SUBCODE,
               CD_SUBCODE);

      check(rchd_write_hunk_coded(w, 0, blob, (uint32_t)len,
               encoding_crc16_ccitt(0xffff, x, CD_HUNK)) == RCHD_OK,
            "cdfl: a hunk written");
   }
   check(rchd_write_finish(w) == RCHD_OK, "cdfl: the image finished");
   rchd_write_free(w);
   read_all("cdfl", expect, CD_HUNKS, CD_HUNK);
}

/* ---- flac ---- */

static void flac(void)
{
   static int16_t s[FL_HUNK / 2];
   static uint8_t blob[FL_HUNK + 256];
   static uint8_t expect[FL_HUNKS * FL_HUNK];
   rchd_writer_t *w = writer(RCHD_CODEC_FLAC, FL_HUNKS, FL_HUNK, 512);
   unsigned       h, i;

   check(w != NULL, "flac: a writer");
   if (!w)
      return;
   for (h = 0; h < FL_HUNKS; h++)
   {
      uint8_t *x   = expect + (size_t)h * FL_HUNK;
      int      big = (h == 1);
      size_t   len;

      for (i = 0; i < FL_HUNK / 2; i++)
      {
         s[i] = sample(h + 7, i);
         x[i * 2]     = (uint8_t)(big ? (uint16_t)s[i] >> 8 : (uint16_t)s[i]);
         x[i * 2 + 1] = (uint8_t)(big ? (uint16_t)s[i] : (uint16_t)s[i] >> 8);
      }
      memset(blob, 0, sizeof(blob));
      blob[0] = big ? 'B' : 'L';
      len = 1 + flac_frame(blob + 1, s, FL_HUNK / 4, 2);

      check(rchd_write_hunk_coded(w, 0, blob, (uint32_t)len,
               encoding_crc16_ccitt(0xffff, x, FL_HUNK)) == RCHD_OK,
            "flac: a hunk written");
   }
   check(rchd_write_finish(w) == RCHD_OK, "flac: the image finished");
   rchd_write_free(w);
   read_all("flac", expect, FL_HUNKS, FL_HUNK);
}

/* ---- A/V ---- */

/* A video tree for 272 codes of up to 16 bits, its lengths 5 bits
 * each: two codes of one bit, the rest absent. */
static void av_tree(bits_t *w)
{
   unsigned i;
   put(w, 1, 5); put(w, 1, 5);            /* escape: a length of 1 */
   put(w, 1, 5); put(w, 1, 5);
   for (i = 2; i < 256 + 16; i++)
      put(w, 0, 5);
   w->bit = (w->bit + 7) & ~(size_t)7;
}

static void av(void)
{
   static const unsigned counts[AV_HUNKS] = { 735, 736, 735 };
   static const unsigned metas[AV_HUNKS]  = { 3, 2, 3 };
   static int16_t s[736];
   static uint8_t blob[AV_HUNK + 1024];
   static uint8_t expect[AV_HUNKS * AV_HUNK];
   rchd_writer_t *w = writer(RCHD_CODEC_AVHUFF, AV_HUNKS, AV_HUNK, AV_HUNK);
   unsigned       h, c, i;

   check(w != NULL, "A/V: a writer");
   if (!w)
      return;
   for (h = 0; h < AV_HUNKS; h++)
   {
      uint8_t *x    = expect + (size_t)h * AV_HUNK;
      uint8_t *xo;
      unsigned n    = counts[h];
      unsigned meta = metas[h];
      size_t   at;
      bits_t   v;

      memset(blob, 0, sizeof(blob));
      memset(x, 0, AV_HUNK);
      blob[0] = (uint8_t)meta;
      blob[1] = AV_CHANNELS;
      be16(blob + 2, n);
      be16(blob + 4, 0);                  /* no picture */
      be16(blob + 6, 0);
      be16(blob + 8, 0xffff);             /* FLAC audio */
      at = 10 + AV_CHANNELS * 2;
      for (i = 0; i < meta; i++)
         blob[at++] = (uint8_t)(0xC0 + h * 4 + i);

      memcpy(x, "chav", 4);
      x[4] = (uint8_t)meta;
      x[5] = AV_CHANNELS;
      be16(x + 6, n);
      memcpy(x + 12, blob + 10 + AV_CHANNELS * 2, meta);
      xo = x + 12 + meta;

      for (c = 0; c < AV_CHANNELS; c++)
      {
         size_t len;
         for (i = 0; i < n; i++)
         {
            s[i] = sample(h * 4 + c + 20, i);
            be16(xo + i * 2, (uint16_t)s[i]);
         }
         xo  += n * 2;
         len  = flac_frame(blob + at, s, n, 1);
         be16(blob + 10 + c * 2, (unsigned)len);
         at  += len;
      }

      v.b   = blob + at;
      v.bit = 8;                          /* a byte skipped */
      av_tree(&v);
      av_tree(&v);
      av_tree(&v);
      at   += v.bit >> 3;

      check(rchd_write_hunk_coded(w, 0, blob, (uint32_t)at,
               encoding_crc16_ccitt(0xffff, x, AV_HUNK)) == RCHD_OK,
            "A/V: a hunk written");
   }
   check(rchd_write_finish(w) == RCHD_OK, "A/V: the image finished");
   rchd_write_free(w);
   read_all("A/V", expect, AV_HUNKS, AV_HUNK);
}

int main(void)
{
   cdfl();
   flac();
   av();
   free(image);

   if (fails)
   {
      printf("rchd_flac_test: %d failure(s)\n", fails);
      return 1;
   }
   printf("rchd_flac_test: PASS\n");
   return 0;
}
