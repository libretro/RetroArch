/* Decodes the hunk map of real CHD images through rhuff and checks the
 * result against the CRC-16 the file carries, which is an oracle the
 * decoder cannot influence.
 *
 * Two independent checks per image: the assembled map must hash to the
 * stored CRC, and the hunk offsets must partition the data region
 * exactly, leaving no gap and no overlap between the first blob and the
 * start of the map. A wrong bit width or a misread escape breaks both.
 *
 * The second half feeds deliberately corrupted maps through the same
 * path. Those must not be decoded correctly -- they must merely fail
 * without reading out of bounds, which is what the sanitizers check.
 *
 * Images come from tools/chd/chd_probe.py.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <encodings/huffman.h>

/* Reference codes stored in a map entry. */
#define MAP_TYPE_0        0
#define MAP_TYPE_3        3
#define MAP_NONE          4
#define MAP_SELF          5
#define MAP_PARENT        6
#define MAP_RLE_SMALL     7
#define MAP_RLE_LARGE     8
#define MAP_SELF_0        9
#define MAP_SELF_1       10
#define MAP_PARENT_SELF  11
#define MAP_PARENT_0     12
#define MAP_PARENT_1     13

#define MAP_ENTRY_BYTES  12
#define MAP_TREE_CODES   16
#define MAP_TREE_BITS     8

static uint32_t rd32(const uint8_t *p)
{
   return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
        | ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

static uint64_t rd_be(const uint8_t *p, int n)
{
   uint64_t v = 0;
   int      i;
   for (i = 0; i < n; i++)
      v = (v << 8) | (uint64_t)p[i];
   return v;
}

static void wr_be(uint8_t *p, uint64_t v, int n)
{
   int i;
   for (i = n - 1; i >= 0; i--)
   {
      p[i] = (uint8_t)(v & 0xff);
      v >>= 8;
   }
}

/* CRC-16/CCITT-FALSE: polynomial 0x1021, initial value 0xffff, no
 * reflection of input or output, no final xor. */
static uint16_t crc16(const uint8_t *data, size_t len)
{
   uint16_t crc = 0xffff;
   size_t   i;
   int      bit;

   for (i = 0; i < len; i++)
   {
      crc ^= (uint16_t)((uint16_t)data[i] << 8);
      for (bit = 0; bit < 8; bit++)
         crc = (uint16_t)((crc & 0x8000)
               ? ((crc << 1) ^ 0x1021) : (crc << 1));
   }
   return crc;
}

struct image
{
   uint8_t *data;
   size_t   size;
   uint64_t mapoffset;
   uint64_t logical;
   uint32_t hunkbytes;
   uint32_t unitbytes;
   uint32_t hunkcount;
   size_t   map_base;
};

/* Loads the header and the map region only. Real images run to
 * hundreds of megabytes and the hunk data is not needed to check a
 * map, so holding the whole file would make the sanitizer runs far
 * heavier than the thing being tested. im->data is indexed by absolute
 * file offset, with everything below map_base absent, so map_decode
 * still reads the offsets it would read normally. */
static int image_load(struct image *im, const char *path)
{
   FILE    *f = fopen(path, "rb");
   uint8_t  hdr[124];
   uint32_t maplength;
   size_t   want;

   if (!f)
      return 0;

   if (fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr))
   {
      fclose(f);
      return 0;
   }
   if (memcmp(hdr, "MComprHD", 8) != 0 || rd32(hdr + 12) != 5)
   {
      fclose(f);
      return 0;
   }

   im->mapoffset = rd_be(hdr + 40, 8);

   if (fseek(f, (long)im->mapoffset, SEEK_SET) != 0
         || fread(hdr + 0, 1, 0, f) != 0)
   {
      fclose(f);
      return 0;
   }
   {
      uint8_t mh[16];
      if (fread(mh, 1, sizeof(mh), f) != sizeof(mh))
      {
         fclose(f);
         return 0;
      }
      maplength = rd32(mh);
      if (maplength > (32u << 20))
      {
         fclose(f);
         return 0;
      }
      want     = (size_t)im->mapoffset + 16 + maplength;
      im->data = (uint8_t*)calloc(want, 1);
      if (!im->data)
      {
         fclose(f);
         return 0;
      }
      im->size = want;
      im->map_base = (size_t)im->mapoffset;
      memcpy(im->data + im->mapoffset, mh, sizeof(mh));
      if (fread(im->data + im->mapoffset + 16, 1, maplength, f) != maplength)
      {
         fclose(f);
         return 0;
      }
   }
   fseek(f, 0, SEEK_SET);
   if (fread(im->data, 1, 124, f) != 124)
   {
      fclose(f);
      return 0;
   }
   fclose(f);

   im->logical   = rd_be(im->data + 32, 8);
   im->mapoffset = rd_be(im->data + 40, 8);
   im->hunkbytes = rd32(im->data + 56);
   im->unitbytes = rd32(im->data + 60);
   if (!im->hunkbytes || !im->unitbytes)
      return 0;
   im->hunkcount = (uint32_t)((im->logical + im->hunkbytes - 1)
         / im->hunkbytes);
   return 1;
}

/* Decodes the compressed map into 12-byte entries. Returns 1 on a
 * structurally complete decode; the caller checks the CRC. */
static int map_decode(const struct image *im, uint8_t *out,
      uint16_t *stored_crc, uint64_t *span, uint64_t *expect_span)
{
   static uint16_t lookup[1 << MAP_TREE_BITS];
   rhuff_dec_t     dec;
   rhuff_bits_t    bits;
   const uint8_t  *hdr;
   const uint8_t  *body;
   uint32_t        maplength;
   uint64_t        datastart;
   uint32_t        lengthbits;
   uint32_t        hunkbits;
   uint32_t        parentbits;
   uint64_t        curoffset;
   uint64_t        last_self   = 0;
   uint64_t        last_parent = 0;
   uint32_t        repeat      = 0;
   uint32_t        last_code   = 0;
   uint32_t        n;
   uint8_t        *codes;

   if (im->mapoffset + 16 > im->size)
      return 0;

   hdr        = im->data + im->mapoffset;
   maplength  = rd32(hdr);
   datastart  = rd_be(hdr + 4, 6);
   *stored_crc = (uint16_t)rd_be(hdr + 10, 2);
   lengthbits = hdr[12];
   hunkbits   = hdr[13];
   parentbits = hdr[14];

   if (im->mapoffset + 16 + (uint64_t)maplength > im->size)
      return 0;
   if (lengthbits > 24 || hunkbits > 24 || parentbits > 24)
      return 0;
   if (datastart > im->mapoffset)
      return 0;

   body = hdr + 16;

   if (rhuff_dec_init(&dec, MAP_TREE_CODES, MAP_TREE_BITS, lookup,
            RHUFF_LOOKUP_ENTRIES(MAP_TREE_BITS)) != RHUFF_OK)
      return 0;

   rhuff_bits_init(&bits, body, maplength);

   if (rhuff_read_tree_rle(&dec, &bits) != RHUFF_OK)
      return 0;

   codes = (uint8_t*)malloc(im->hunkcount);
   if (!codes)
      return 0;

   /* First pass: one reference code per hunk, with two escapes that
    * repeat the previous code. */
   for (n = 0; n < im->hunkcount; n++)
   {
      uint32_t value;

      if (repeat > 0)
      {
         codes[n] = (uint8_t)last_code;
         repeat--;
         continue;
      }

      value = rhuff_dec_decode_one(&dec, &bits);

      if (value == MAP_RLE_SMALL)
      {
         codes[n] = (uint8_t)last_code;
         repeat   = 2 + rhuff_dec_decode_one(&dec, &bits);
      }
      else if (value == MAP_RLE_LARGE)
      {
         uint32_t hi = rhuff_dec_decode_one(&dec, &bits);
         uint32_t lo = rhuff_dec_decode_one(&dec, &bits);
         codes[n] = (uint8_t)last_code;
         repeat   = 2 + 16 + (hi << 4) + lo;
      }
      else
      {
         last_code = value;
         codes[n]  = (uint8_t)value;
      }
   }

   /* Second pass: the fields each code implies, in the same bit
    * stream, plus the running offset that uncompressed and compressed
    * hunks advance. */
   curoffset = datastart;

   for (n = 0; n < im->hunkcount; n++)
   {
      uint32_t code   = codes[n];
      uint64_t offset = curoffset;
      uint32_t length = 0;
      uint32_t crc    = 0;

      switch (code)
      {
         case MAP_TYPE_0:
         case MAP_TYPE_0 + 1:
         case MAP_TYPE_0 + 2:
         case MAP_TYPE_3:
            length     = rhuff_bits_read(&bits, (int)lengthbits);
            crc        = rhuff_bits_read(&bits, 16);
            curoffset += length;
            break;

         case MAP_NONE:
            length     = im->hunkbytes;
            crc        = rhuff_bits_read(&bits, 16);
            curoffset += length;
            break;

         case MAP_SELF:
            offset    = rhuff_bits_read(&bits, (int)hunkbits);
            last_self = offset;
            break;

         case MAP_PARENT:
            offset      = rhuff_bits_read(&bits, (int)parentbits);
            last_parent = offset;
            break;

         case MAP_SELF_1:
            last_self++;
            /* falls through */
         case MAP_SELF_0:
            code   = MAP_SELF;
            offset = last_self;
            break;

         case MAP_PARENT_SELF:
            code        = MAP_PARENT;
            offset      = ((uint64_t)n * im->hunkbytes) / im->unitbytes;
            last_parent = offset;
            break;

         case MAP_PARENT_1:
            last_parent += im->hunkbytes / im->unitbytes;
            /* falls through */
         case MAP_PARENT_0:
            code   = MAP_PARENT;
            offset = last_parent;
            break;

         default:
            free(codes);
            return 0;
      }

      out[n * MAP_ENTRY_BYTES] = (uint8_t)code;
      wr_be(out + n * MAP_ENTRY_BYTES + 1,  (uint64_t)length, 3);
      wr_be(out + n * MAP_ENTRY_BYTES + 4,  offset,           6);
      wr_be(out + n * MAP_ENTRY_BYTES + 10, (uint64_t)crc,    2);
   }

   free(codes);

   *span        = curoffset - datastart;
   *expect_span = im->mapoffset - datastart;

   return 1;
}

static int check(const char *path)
{
   struct image im;
   uint8_t     *map;
   uint16_t     stored = 0;
   uint16_t     got;
   uint64_t     span = 0;
   uint64_t     expect = 0;
   int          ok;

   memset(&im, 0, sizeof(im));
   if (!image_load(&im, path))
   {
      printf("  %-44s could not load\n", path);
      free(im.data);
      return 0;
   }

   map = (uint8_t*)calloc(im.hunkcount, MAP_ENTRY_BYTES);
   if (!map)
   {
      free(im.data);
      return 0;
   }

   ok = map_decode(&im, map, &stored, &span, &expect);
   if (ok)
   {
      got = crc16(map, (size_t)im.hunkcount * MAP_ENTRY_BYTES);
      ok  = (got == stored) && (span == expect);
      /* Printed through double rather than a 64-bit integer type: C89
       * has no long long and no format for one. */
      printf("  %-44s hunks=%-4u crc %04x/%04x  span %.0f/%.0f  %s\n",
            path, im.hunkcount, got, stored,
            (double)span, (double)expect, ok ? "ok" : "FAIL");
   }
   else
      printf("  %-44s decode failed\n", path);

   free(map);
   free(im.data);
   return ok;
}

/* Corrupts a copy of a real image and requires only that decoding stays
 * inside its buffers. Producing a wrong map is a fine outcome; reading
 * out of bounds is not, and that is what the sanitizers catch. */
static int fuzz(const char *path, unsigned long iterations)
{
   struct image im;
   uint8_t     *orig;
   unsigned long i;
   unsigned long state = 0x9E3779B9UL;
   unsigned long survived = 0;

   memset(&im, 0, sizeof(im));
   if (!image_load(&im, path))
      return 0;

   orig = (uint8_t*)malloc(im.size);
   memcpy(orig, im.data, im.size);

   for (i = 0; i < iterations; i++)
   {
      uint8_t *map;
      uint16_t stored = 0;
      uint64_t span = 0;
      uint64_t expect = 0;
      size_t   pos;
      int      hits;
      int      k;

      memcpy(im.data, orig, im.size);

      /* Flip bits anywhere in the map header or its body, so bit
       * widths, the tree and the code stream all get hit. */
      state = state * 1103515245UL + 12345UL;
      hits  = 1 + (int)((state >> 16) % 6);
      for (k = 0; k < hits; k++)
      {
         state = state * 1103515245UL + 12345UL;
         pos   = im.map_base
               + (size_t)((state >> 8) % (im.size - im.map_base));
         state = state * 1103515245UL + 12345UL;
         im.data[pos] ^= (uint8_t)(1u << ((state >> 12) & 7));
      }

      /* Hunk count comes from the header and a corrupt header can make
       * it huge; cap the allocation the same way a real reader would. */
      if (im.hunkcount > (1u << 20))
         continue;

      map = (uint8_t*)calloc(im.hunkcount, MAP_ENTRY_BYTES);
      if (!map)
         continue;
      map_decode(&im, map, &stored, &span, &expect);
      free(map);
      survived++;
   }

   free(orig);
   free(im.data);
   printf("  %-44s %lu corrupted maps decoded without a fault\n",
         path, survived);
   return 1;
}

int main(int argc, char **argv)
{
   static const char *images[] = {
      "v5_zlib.chd", "v5_lzma.chd", "v5_huff.chd", "v5_flac.chd",
      "v5_multi.chd", "child.chd", "selfy.chd",
      "corpus/Adventures_of_Lomax_The__USA_.chd",
      "corpus/Dead_or_Alive__USA_.chd",
      "corpus/Silent_Hill__Europe___EnFrDeEsIt_.chd",
      "corpus/Street_Fighter_Collection__USA___Disc_2_.chd"
   };
   unsigned long iters = (argc > 1) ? strtoul(argv[1], NULL, 10) : 20000;
   size_t   i;
   int      fail = 0;

   printf("map decode against stored CRC-16 and data span\n");
   for (i = 0; i < sizeof(images) / sizeof(images[0]); i++)
      if (!check(images[i]))
         fail = 1;

   printf("corrupted maps\n");
   fuzz("v5_multi.chd", iters);
   fuzz("child.chd", iters);

   printf("%s\n", fail ? "FAIL" : "PASS");
   return fail;
}
