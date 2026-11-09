/* SPDX-License-Identifier: MIT */
/* Laced block parsing against hand-built SimpleBlocks: the three lacing
 * styles, the packets they must lend, and malformed size tables. */
#include <assert.h>
#include <string.h>
#include <formats/rwebm.h>

#define LACE_XIPH  0x02
#define LACE_FIXED 0x04
#define LACE_EBML  0x06

/* One byte past any fixture stays zero: a parser that reads a missing
 * lace count sees a plausible count of zero and hands out a packet. */
static uint8_t file_data[512];
static size_t file_used;

static size_t begin(uint32_t id, unsigned bytes)
{
   size_t mark;
   while (bytes--)
      file_data[file_used++] = (uint8_t)(id >> (bytes * 8));
   mark = file_used;
   file_data[file_used++] = 0x40;
   file_data[file_used++] = 0;
   return mark;
}

static void end(size_t mark)
{
   size_t size = file_used - mark - 2;
   file_data[mark] = (uint8_t)(0x40 | (size >> 8));
   file_data[mark + 1] = (uint8_t)size;
}

static void text(uint32_t id, const char *value, size_t length)
{
   size_t mark = begin(id, 1);
   memcpy(file_data + file_used, value, length);
   file_used += length;
   end(mark);
}

static void block(unsigned flags, const uint8_t *body, size_t length)
{
   size_t mark = begin(0xa3u, 1);
   file_data[file_used++] = 0x81;
   file_data[file_used++] = 0;
   file_data[file_used++] = 0;
   file_data[file_used++] = (uint8_t)flags;
   memcpy(file_data + file_used, body, length);
   file_used += length;
   end(mark);
}

/* Lay out a file holding `body` laced with `flags`, optionally followed
 * by a plain block.  Returns the payload's offset within file_data. */
static size_t build(unsigned flags, const uint8_t *body, size_t length,
      int follow)
{
   static const uint8_t plain[] = { 0x5a, 0xa5 };
   size_t segment, tracks, entry, cluster, payload;
   memset(file_data, 0, sizeof(file_data));
   file_used = 0;
   end(begin(0x1a45dfa3u, 4));
   segment = begin(0x18538067u, 4);
   tracks = begin(0x1654ae6bu, 4);
   entry = begin(0xaeu, 1);
   text(0xd7u, "\001", 1);
   text(0x83u, "\002", 1);
   text(0x86u, "A_OPUS", 6);
   end(entry);
   end(tracks);
   cluster = begin(0x1f43b675u, 4);
   text(0xe7u, "\000", 1);
   payload = file_used + 7;
   block(flags, body, length);
   if (follow)
      block(0, plain, sizeof(plain));
   end(cluster);
   end(segment);
   return payload;
}

/* The block must lend exactly these frames, in file order, from the
 * caller's buffer starting at body[skip]. */
static void expect(unsigned flags, const uint8_t *body, size_t length,
      size_t skip, const uint32_t *sizes, unsigned count)
{
   rwebm_packet pkt;
   size_t at = build(flags, body, length, 0) + skip;
   rwebm_t *webm = rwebm_open_memory(file_data, file_used);
   unsigned i;
   assert(webm);
   for (i = 0; i < count; i++)
   {
      assert(rwebm_read_packet(webm, &pkt) == 1);
      assert(pkt.data == file_data + at && pkt.size == sizes[i]);
      at += sizes[i];
   }
   assert(rwebm_read_packet(webm, &pkt) == 0);
   rwebm_close(webm);
}

/* The block must vanish, and the block after it must still arrive. */
static void reject(unsigned flags, const uint8_t *body, size_t length)
{
   rwebm_packet pkt;
   rwebm_t *webm;
   build(flags, body, length, 0);
   webm = rwebm_open_memory(file_data, file_used);
   assert(webm);
   assert(rwebm_read_packet(webm, &pkt) == 0);
   rwebm_close(webm);
   build(flags, body, length, 1);
   webm = rwebm_open_memory(file_data, file_used);
   assert(webm);
   assert(rwebm_read_packet(webm, &pkt) == 1);
   assert(pkt.size == 2 && pkt.data[0] == 0x5a);
   assert(rwebm_read_packet(webm, &pkt) == 0);
   rwebm_close(webm);
}

int main(void)
{
   /* count 3; sizes 2 and 3; remainder 4 */
   static const uint8_t xiph[] = { 2, 2, 3, 1,1, 2,2,2, 3,3,3,3 };
   static const uint8_t fixed[] = { 2, 1,1, 2,2, 3,3 };
   /* count 3; first 2; delta +1 (0xc0 = 64, bias 63) */
   static const uint8_t ebml[] = { 2, 0x82, 0xc0, 1,1, 2,2,2, 3,3,3,3 };
   /* a single frame has no size table at all */
   static const uint8_t ebml_one[] = { 0, 0x81, 0x82, 0x83 };
   /* first 4, then a delta reaching 0xfffffffe: on a 32-bit size_t the
    * running total wraps back inside the block. */
   static const uint8_t ebml_wrap[] = { 2, 0x84,
      0x03, 0x00, 0x00, 0xff, 0xff, 0xff, 0xf9,
      1,1,1,1,1,1,1,1,1,1 };
   /* first 4, then a delta reaching exactly 2^32 + 2. */
   static const uint8_t ebml_trunc[] = { 2, 0x84,
      0x03, 0x00, 0x00, 0xff, 0xff, 0xff, 0xfd,
      1,1,1,1,1,1,1,1,1,1 };
   static const uint8_t xiph_long[] = { 1, 200, 1, 2 };
   static const uint32_t xiph_sizes[] = { 2, 3, 4 };
   static const uint32_t fixed_sizes[] = { 2, 2, 2 };
   static const uint32_t ebml_one_sizes[] = { 3 };

   expect(LACE_XIPH, xiph, sizeof(xiph), 3, xiph_sizes, 3);
   expect(LACE_FIXED, fixed, sizeof(fixed), 1, fixed_sizes, 3);
   expect(LACE_EBML, ebml, sizeof(ebml), 3, xiph_sizes, 3);
   expect(LACE_EBML, ebml_one, sizeof(ebml_one), 1, ebml_one_sizes, 1);

   /* No lace count at all. */
   reject(LACE_XIPH, xiph, 0);
   reject(LACE_FIXED, xiph, 0);
   reject(LACE_EBML, xiph, 0);
   /* Size tables that run off the block or do not fit in 32 bits. */
   reject(LACE_XIPH, xiph_long, sizeof(xiph_long));
   reject(LACE_EBML, ebml_wrap, sizeof(ebml_wrap));
   reject(LACE_EBML, ebml_trunc, sizeof(ebml_trunc));
   return 0;
}
