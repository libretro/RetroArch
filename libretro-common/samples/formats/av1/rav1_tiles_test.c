/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <formats/rav1_tiles.h>

static unsigned failures;

#define CHECK(x)                                                               \
   do                                                                          \
   {                                                                           \
      if (!(x))                                                                \
      {                                                                        \
         fprintf(stderr, "line %u: %s\n", (unsigned)__LINE__, #x);             \
         failures++;                                                           \
      }                                                                        \
   } while (0)

static void reject(const uint8_t *data, size_t size,
                   const rav1_tile_layout_t *layout, bool frame)
{
   rav1_tile_group_t group, zero;
   rav1_tile_t tile, tile_zero;
   memset(&zero, 0, sizeof(zero));
   memset(&tile_zero, 0, sizeof(tile_zero));
   memset(&group, 255, sizeof(group));
   CHECK(!rav1_tile_group_init(&group, data, size, layout, frame));
   CHECK(!memcmp(&group, &zero, sizeof(group)));
   memset(&tile, 255, sizeof(tile));
   CHECK(rav1_tile_group_next(&tile, &group) == -1);
   CHECK(!memcmp(&tile, &tile_zero, sizeof(tile)));
}

static void test_ranges(void)
{
   static const uint8_t partial[] = {0xb0, 1, 0xa1, 0xa2, 0xb1};
   static const uint8_t one[] = {0xa8, 0x61};
   static const uint8_t outside[] = {0xb8, 0x61};
   static const uint8_t reverse[] = {0xc8, 0x61};
   static const uint8_t bad_padding[] = {0xb1, 1, 0xa1, 0xa2, 0xb1};
   rav1_tile_layout_t layout = {2, 2, 1, 1, 1};
   rav1_tile_group_t group, saved;
   rav1_tile_t tile;
   CHECK(
       rav1_tile_group_init(&group, partial, sizeof(partial), &layout, false));
   CHECK(group.first == 1 && group.last == 2 && group.offset == 1);
   CHECK(rav1_tile_group_next(&tile, &group) == 1);
   CHECK(tile.index == 1 && tile.row == 0 && tile.column == 1 &&
         tile.data == partial + 2 && tile.size == 2);
   CHECK(rav1_tile_group_next(&tile, &group) == 1);
   CHECK(tile.index == 2 && tile.row == 1 && tile.column == 0 &&
         tile.data == partial + 4 && tile.size == 1);
   saved = group;
   CHECK(rav1_tile_group_next(&tile, &group) == 0 && !tile.data && !tile.size);
   CHECK(!memcmp(&saved, &group, sizeof(group)));
   CHECK(rav1_tile_group_init(&group, one, sizeof(one), &layout, false));
   CHECK(group.first == 1 && group.last == 1);
   CHECK(rav1_tile_group_next(&tile, &group) == 1 && tile.data == one + 1 &&
         tile.size == 1);
   reject(partial, sizeof(partial), &layout, true);
   reject(partial, 4, &layout, false);
   reject(reverse, sizeof(reverse), &layout, false);
   reject(bad_padding, sizeof(bad_padding), &layout, false);
   layout.columns = 3;
   layout.columns_log2 = 2;
   layout.rows = 1;
   layout.rows_log2 = 0;
   reject(outside, sizeof(outside), &layout, false);
}

static void test_sizes(void)
{
   uint8_t data[16];
   rav1_tile_layout_t layout = {2, 1, 1, 0, 1};
   rav1_tile_group_t group, saved;
   rav1_tile_t tile;
   unsigned width;
   size_t size, i;
   for (width = 1; width <= 4; width++)
   {
      memset(data, 0, sizeof(data));
      layout.size_bytes = width;
      data[1] = 1;
      data[1 + width] = 0xaa;
      data[2 + width] = 0xbb;
      data[3 + width] = 0xcc;
      size = 4 + width;
      CHECK(rav1_tile_group_init(&group, data, size, &layout, true));
      CHECK(rav1_tile_group_next(&tile, &group) == 1 && tile.size == 2 &&
            tile.data == data + 1 + width);
      CHECK(rav1_tile_group_next(&tile, &group) == 1 && tile.size == 1 &&
            tile.data == data + 3 + width);
      for (i = 0; i < size; i++)
         reject(data, i, &layout, true);
      memset(data + 1, 255, width);
      reject(data, size, &layout, false);
   }
   memset(data, 0, sizeof(data));
   data[2] = 1; /* Little-endian length 257 rather than 2. */
   layout.size_bytes = 2;
   reject(data, sizeof(data), &layout, false);
   layout.columns = 1;
   layout.columns_log2 = 0;
   layout.size_bytes = 0;
   CHECK(rav1_tile_group_init(&group, data, sizeof(data), &layout, true));
   CHECK(!group.offset);
   CHECK(rav1_tile_group_next(&tile, &group) == 1 && tile.data == data &&
         tile.size == sizeof(data));
   group.next = 0;
   group.offset = sizeof(data) + 1;
   saved = group;
   CHECK(rav1_tile_group_next(&tile, &group) == -1 && !tile.data);
   CHECK(!memcmp(&saved, &group, sizeof(group)));
}

static void test_limits(void)
{
   static uint8_t data[8192];
   static const uint8_t tail[] = {0xff, 0xf7, 0xff, 0x80, 0, 0xaa, 0xbb};
   rav1_tile_layout_t layout = {64, 64, 6, 6, 1};
   rav1_tile_group_t group;
   rav1_tile_t tile;
   unsigned i;
   memset(data, 0, sizeof(data));
   CHECK(rav1_tile_group_init(&group, data, sizeof(data), &layout, false));
   for (i = 0; i < 4096; i++)
   {
      CHECK(rav1_tile_group_next(&tile, &group) == 1);
      CHECK(tile.index == i && tile.row == i / 64 && tile.column == i % 64);
      CHECK(tile.size == 1 && tile.data == data + 2 + 2 * i - (i == 4095));
   }
   CHECK(rav1_tile_group_next(&tile, &group) == 0);
   CHECK(rav1_tile_group_init(&group, tail, sizeof(tail), &layout, false));
   CHECK(group.first == 4094 && group.last == 4095);
   CHECK(rav1_tile_group_next(&tile, &group) == 1 && tile.index == 4094 &&
         tile.data == tail + 5 && tile.size == 1 && tile.row == 63 &&
         tile.column == 62);
   CHECK(rav1_tile_group_next(&tile, &group) == 1 && tile.index == 4095 &&
         tile.data == tail + 6 && tile.size == 1 && tile.row == 63 &&
         tile.column == 63);
   reject(tail, 3, &layout, false);
   reject(tail, 6, &layout, false);
   layout.rows = 65;
   layout.rows_log2 = 7;
   reject(data, sizeof(data), &layout, false);
   layout.rows = 64;
   layout.rows_log2 = 2;
   reject(data, sizeof(data), &layout, false);
   layout.rows_log2 = 6;
   layout.size_bytes = 0;
   reject(data, sizeof(data), &layout, false);
   layout.size_bytes = 5;
   reject(data, sizeof(data), &layout, false);
   reject(NULL, sizeof(data), &layout, false);
   reject(data, sizeof(data), NULL, false);
   CHECK(!rav1_tile_group_init(NULL, data, sizeof(data), &layout, false));
   CHECK(rav1_tile_group_next(&tile, NULL) == -1);
   CHECK(rav1_tile_group_next(NULL, &group) == -1);
}

static unsigned long hash_bytes(const uint8_t *data, size_t size)
{
   uint32_t hash = 2166136261u;
   size_t i;
   for (i = 0; i < size; i++)
      hash = (hash ^ data[i]) * 16777619u;
   return (unsigned long)hash;
}

static int inspect(int argc, char **argv)
{
   uint8_t *data;
   FILE *file;
   long size;
   int result = 1, ret;
   rav1_tile_layout_t layout;
   rav1_tile_group_t group;
   rav1_tile_t tile;
   if (argc != 7)
      return 1;
   layout.columns = (unsigned)strtoul(argv[2], NULL, 10);
   layout.rows = (unsigned)strtoul(argv[3], NULL, 10);
   layout.columns_log2 = (unsigned)strtoul(argv[4], NULL, 10);
   layout.rows_log2 = (unsigned)strtoul(argv[5], NULL, 10);
   layout.size_bytes = (unsigned)strtoul(argv[6], NULL, 10);
   file = fopen(argv[1], "rb");
   if (!file)
      return 1;
   if (fseek(file, 0, SEEK_END) || (size = ftell(file)) <= 0 ||
       size > 32 * 1024 * 1024 || fseek(file, 0, SEEK_SET))
   {
      fclose(file);
      return 1;
   }
   data = (uint8_t *)malloc((size_t)size);
   if (data && fread(data, 1, (size_t)size, file) == (size_t)size &&
       rav1_tile_group_init(&group, data, (size_t)size, &layout, true))
   {
      while ((ret = rav1_tile_group_next(&tile, &group)) == 1)
         printf("%u %u %u %lu %lu %08lx\n", tile.index, tile.row, tile.column,
                (unsigned long)(tile.data - data), (unsigned long)tile.size,
                hash_bytes(tile.data, tile.size));
      result = ret ? 1 : 0;
   }
   free(data);
   fclose(file);
   return result;
}

int main(int argc, char **argv)
{
   if (argc > 1)
      return inspect(argc, argv);
   test_ranges();
   test_sizes();
   test_limits();
   printf("AV1 borrowed tile groups: %u failures\n", failures);
   return failures ? 1 : 0;
}
