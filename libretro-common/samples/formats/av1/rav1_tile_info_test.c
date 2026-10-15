/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <formats/rav1_tile_info.h>

static unsigned failures, position;
static uint8_t data[512];
static rav1_tile_info_t info, zero;

#define CHECK(x)                                                               \
   do                                                                          \
   {                                                                           \
      if (!(x))                                                                \
      {                                                                        \
         fprintf(stderr, "line %u: %s\n", (unsigned)__LINE__, #x);             \
         failures++;                                                           \
      }                                                                        \
   } while (0)

static void put(unsigned value, unsigned count)
{
   unsigned i;
   for (i = count; i; i--)
   {
      if ((value >> (i - 1)) & 1)
         data[position / 8] |= 1 << (7 - position % 8);
      position++;
   }
}

static void put_ns(unsigned n, unsigned value)
{
   unsigned w = 0, m;
   while (n >> w)
      w++;
   m = (1u << w) - n;
   if (value < m)
      put(value, w - 1);
   else
      put(value + m, w);
}

static rav1_bits_t reader(size_t size, unsigned start)
{
   rav1_bits_t bits;
   bits.data = data;
   bits.size = size;
   bits.byte = start / 8;
   bits.bit = start % 8;
   bits.failed = 0;
   return bits;
}

static void reject(size_t size, unsigned start, unsigned width, unsigned height)
{
   rav1_bits_t bits = reader(size, start), saved = bits;
   memset(&info, 255, sizeof(info));
   CHECK(!rav1_tile_info_parse(&info, &bits, width, height, false));
   CHECK(!memcmp(&info, &zero, sizeof(info)));
   CHECK(bits.data == saved.data && bits.size == saved.size &&
         bits.byte == saved.byte && bits.bit == saved.bit && !bits.failed);
}

static void test_ns(void)
{
   static const uint8_t known[] = {
       0x1c}; /* ns(5): 0 -> 00, 1 -> 01, 3 -> 110. */
   rav1_bits_t bits;
   unsigned n, value;
   bits = reader(1, 0);
   bits.data = known;
   CHECK(rav1_bits_ns(&bits, 5) == 0);
   CHECK(rav1_bits_ns(&bits, 5) == 1);
   CHECK(rav1_bits_ns(&bits, 5) == 3 && bits.bit == 7);
   for (n = 1; n <= 65; n++)
      for (value = 0; value < n; value++)
      {
         position = 3;
         memset(data, 0, sizeof(data));
         put_ns(n, value);
         bits = reader((position + 7) / 8, 3);
         CHECK(rav1_bits_ns(&bits, n) == value && !bits.failed);
         CHECK(bits.byte * 8 + bits.bit == position);
      }
   bits = reader(0, 0);
   CHECK(!rav1_bits_ns(&bits, 0) && bits.failed);
   bits = reader(0, 0);
   CHECK(!rav1_bits_ns(&bits, 65537) && bits.failed);
   bits = reader(0, 0);
   CHECK(!rav1_bits_ns(&bits, 5) && bits.failed);
}

static void test_uniform(void)
{
   rav1_bits_t bits;
   unsigned sb;
   memset(data, 0, sizeof(data));
   data[0] = 0xd9;
   bits = reader(1, 0);
   CHECK(rav1_tile_info_parse(&info, &bits, 192, 128, false));
   CHECK(info.layout.columns == 2 && info.layout.rows == 2);
   CHECK(info.layout.columns_log2 == 1 && info.layout.rows_log2 == 1);
   CHECK(info.context_update_tile_id == 2 && info.layout.size_bytes == 2);
   CHECK(info.mi_col_starts[0] == 0 && info.mi_col_starts[1] == 32 &&
         info.mi_col_starts[2] == 48);
   CHECK(info.mi_row_starts[0] == 0 && info.mi_row_starts[1] == 16 &&
         info.mi_row_starts[2] == 32);
   CHECK(bits.byte == 1 && !bits.bit);
   reject(0, 0, 192, 128);
   data[0] = 0x80;
   for (sb = 0; sb < 2; sb++)
   {
      bits = reader(1, 0);
      CHECK(rav1_tile_info_parse(&info, &bits, 1, 1, sb != 0));
      CHECK(info.layout.columns == 1 && info.layout.rows == 1 &&
            !info.layout.size_bytes);
      CHECK(info.mi_col_starts[1] == 2 && info.mi_row_starts[1] == 2 &&
            bits.bit == 1);
   }
   reject(1, 0, 0, 1);
   reject(1, 0, 1, 0);
   reject(1, 0, 65537, 1);
   bits = reader(1, 0);
   CHECK(!rav1_tile_info_parse(&info, NULL, 1, 1, false));
   CHECK(!rav1_tile_info_parse(NULL, &bits, 1, 1, false));
}

static void test_nonuniform(void)
{
   rav1_bits_t bits;
   unsigned ctx, i;
   size_t size;
   for (ctx = 5; ctx <= 6; ctx++)
   {
      memset(data, 0, sizeof(data));
      position = 3;
      put(0, 1);
      put_ns(5, 1);
      put_ns(3, 0);
      put_ns(2, 1);
      put_ns(3, 0);
      put_ns(2, 1);
      put(ctx, 3);
      put(3, 2);
      size = (position + 7) / 8;
      if (ctx == 6)
      {
         reject(size, 3, 320, 192);
         continue;
      }
      bits = reader(size, 3);
      CHECK(rav1_tile_info_parse(&info, &bits, 320, 192, false));
      CHECK(!info.uniform_spacing && info.layout.columns == 3 &&
            info.layout.rows == 2);
      CHECK(info.layout.columns_log2 == 2 && info.layout.rows_log2 == 1);
      CHECK(info.context_update_tile_id == 5 && info.layout.size_bytes == 4);
      CHECK(info.mi_col_starts[1] == 32 && info.mi_col_starts[2] == 48 &&
            info.mi_col_starts[3] == 80);
      CHECK(info.mi_row_starts[1] == 16 && info.mi_row_starts[2] == 48);
      CHECK(bits.byte * 8 + bits.bit == position);
      for (i = 0; i < size; i++)
         reject(i, 3, 320, 192);
   }
   memset(data, 0, sizeof(data));
   position = 0;
   put(0, 1);
   for (i = 65; i; i--)
      put_ns(i < 64 ? i : 64, 0);
   reject((position + 7) / 8, 0, 4160, 64);
   memset(data, 0, sizeof(data));
   bits = reader(1, 0);
   CHECK(rav1_tile_info_parse(&info, &bits, 8, 8, false));
   CHECK(info.layout.columns == 1 && info.layout.rows == 1 && bits.bit == 1);
}

static void test_limits(void)
{
   rav1_bits_t bits;
   unsigned sb, i;
   for (sb = 0; sb < 2; sb++)
   {
      memset(data, 0, sizeof(data));
      position = 0;
      put(1, 1);
      put(3, 2);
      put(7, 3);
      put(4095, 12);
      put(3, 2);
      bits = reader((position + 7) / 8, 0);
      CHECK(rav1_tile_info_parse(&info, &bits, 65536, 65536, sb != 0));
      CHECK(info.layout.columns == 64 && info.layout.rows == 64);
      CHECK(info.layout.size_bytes == 4 && info.context_update_tile_id == 4095);
      for (i = 0; i <= 64; i++)
         CHECK(info.mi_col_starts[i] == i * 256 &&
               info.mi_row_starts[i] == i * 256);
      CHECK(bits.byte * 8 + bits.bit == position);
      memset(data, 0, sizeof(data));
      position = 0;
      put(1, 1);
      put(0, 1);
      put(0, 1);
      put(0, 9);
      put(0, 2);
      bits = reader((position + 7) / 8, 0);
      CHECK(rav1_tile_info_parse(&info, &bits, 65536, 65536, sb != 0));
      CHECK(info.layout.columns == 16 && info.layout.rows == 32 &&
            info.layout.size_bytes == 1);
   }
}

static unsigned long hash_bytes(const uint8_t *bytes, size_t size)
{
   uint32_t hash = 2166136261u;
   size_t i;
   for (i = 0; i < size; i++)
      hash = (hash ^ bytes[i]) * 16777619u;
   return (unsigned long)hash;
}

static int inspect(int argc, char **argv)
{
   FILE *file;
   uint8_t *bytes;
   long size;
   unsigned start, group_start, width, height, sb, i;
   int ret, result = 1;
   rav1_bits_t bits;
   rav1_tile_group_t group;
   rav1_tile_t tile;
   if (argc != 7)
      return 1;
   file = fopen(argv[1], "rb");
   if (!file)
      return 1;
   if (fseek(file, 0, SEEK_END) || (size = ftell(file)) <= 0 ||
       size > 32 * 1024 * 1024 || fseek(file, 0, SEEK_SET))
   {
      fclose(file);
      return 1;
   }
   bytes = (uint8_t *)malloc((size_t)size);
   start = (unsigned)strtoul(argv[2], NULL, 10);
   group_start = (unsigned)strtoul(argv[3], NULL, 10);
   width = (unsigned)strtoul(argv[4], NULL, 10);
   height = (unsigned)strtoul(argv[5], NULL, 10);
   sb = (unsigned)strtoul(argv[6], NULL, 10);
   bits = reader((size_t)size, start);
   bits.data = bytes;
   if (bytes && group_start < (unsigned long)size &&
       fread(bytes, 1, (size_t)size, file) == (size_t)size &&
       rav1_tile_info_parse(&info, &bits, width, height, sb != 0) &&
       rav1_tile_group_init(&group, bytes + group_start, size - group_start,
                            &info.layout, true))
   {
      printf("I %u %u %u %u %u %u %lu\n", info.layout.columns, info.layout.rows,
             info.layout.columns_log2, info.layout.rows_log2,
             info.layout.size_bytes, info.context_update_tile_id,
             (unsigned long)(bits.byte * 8 + bits.bit));
      printf("C");
      for (i = 0; i <= info.layout.columns; i++)
         printf(" %u", info.mi_col_starts[i]);
      printf("\nR");
      for (i = 0; i <= info.layout.rows; i++)
         printf(" %u", info.mi_row_starts[i]);
      printf("\n");
      while ((ret = rav1_tile_group_next(&tile, &group)) == 1)
         printf("T %u %u %u %lu %lu %08lx\n", tile.index, tile.row, tile.column,
                (unsigned long)(tile.data - bytes), (unsigned long)tile.size,
                hash_bytes(tile.data, tile.size));
      result = ret ? 1 : 0;
   }
   free(bytes);
   fclose(file);
   return result;
}

int main(int argc, char **argv)
{
   if (argc > 1)
      return inspect(argc, argv);
   test_ns();
   test_uniform();
   test_nonuniform();
   test_limits();
   printf("AV1 tile info: %u failures\n", failures);
   return failures ? 1 : 0;
}
