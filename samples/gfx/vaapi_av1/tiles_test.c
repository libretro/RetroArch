/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef NDEBUG
#error VA AV1 tile conformance requires assertions
#endif
#include <assert.h>
#include <stdio.h>
#include "../../../gfx/common/vaapi_av1_tiles.h"
static VASliceParameterBufferAV1 slices[4096], saved[4096];
static rav1_hw_picture_t picture;
static uint8_t maximum[8195];
static uint8_t packet[] = {0x32, 9, 0, 2, 'a', 'b', 'c', 0, 'd', 'e', 'f'};
static const unsigned offsets[] = {4, 8, 9};
static const unsigned lengths[] = {3, 1, 2};
static unsigned count;

static void setup(void)
{
   memset(&picture, 0, sizeof(picture));
   picture.tiles.layout.columns = 3;
   picture.tiles.layout.columns_log2 = 2;
   picture.tiles.layout.rows = 1;
   picture.tiles.layout.size_bytes = 1;
   assert(rav1_tile_group_init(&picture.tile_group, packet + 2,
         sizeof(packet) - 2, &picture.tiles.layout, true));
   memset(slices, 0xa5, sizeof(slices));
   memcpy(saved, slices, sizeof(saved));
   count = 1234;
}
static void unchanged(void)
{
   assert(count == 1234 && !memcmp(slices, saved, sizeof(slices)));
}
static void check_record(
      unsigned index, unsigned at, unsigned size, unsigned columns)
{
   VASliceParameterBufferAV1 expected;
   memset(&expected, 0, sizeof(expected));
   expected.slice_data_offset = at;
   expected.slice_data_size = size;
   expected.slice_data_flag = VA_SLICE_DATA_FLAG_ALL;
   expected.tile_row = (uint16_t)(index / columns);
   expected.tile_column = (uint16_t)(index % columns);
   assert(!memcmp(&slices[index], &expected, sizeof(expected)));
}
int main(void)
{
   unsigned i;
   setup();
   assert(vaapi_av1_tiles(
         slices, 4096, &count, packet, sizeof(packet), &picture));
   assert(count == 3);
   for (i = 0; i < count; i++)
      check_record(i, offsets[i], lengths[i], 3);
   assert(!memcmp(
         slices + count, saved + count, (4096 - count) * sizeof(*slices)));
   setup();
   assert(
         !vaapi_av1_tiles(slices, 2, &count, packet, sizeof(packet), &picture));
   unchanged();
   assert(!vaapi_av1_tiles(
         slices, 4097, &count, packet, sizeof(packet), &picture));
   unchanged();
   picture.tile_group.next = 1;
   assert(!vaapi_av1_tiles(
         slices, 4096, &count, packet, sizeof(packet), &picture));
   unchanged();
   setup();
   picture.tile_group.first = 1;
   assert(!vaapi_av1_tiles(
         slices, 4096, &count, packet, sizeof(packet), &picture));
   unchanged();
   setup();
   picture.tile_group.data = packet;
   assert(!vaapi_av1_tiles(
         slices, 4096, &count, packet, sizeof(packet), &picture));
   unchanged();
   setup();
   packet[7] = 255;
   assert(!vaapi_av1_tiles(
         slices, 4096, &count, packet, sizeof(packet), &picture));
   unchanged();
   packet[7] = 0;
   assert(!vaapi_av1_tiles((VASliceParameterBufferAV1 *)&picture, 4096, &count,
         packet, sizeof(packet), &picture));
   unchanged();
   assert(!vaapi_av1_tiles(
         slices, 4096, (unsigned *)slices, packet, sizeof(packet), &picture));
   unchanged();
   picture.header.show_existing = 1;
   assert(!vaapi_av1_tiles(
         slices, 4096, &count, packet, sizeof(packet), &picture));
   unchanged();
   memset(&picture, 0, sizeof(picture));
   maximum[0] = 0x32;
   maximum[1] = 0x80;
   maximum[2] = 0x40;
   maximum[3] = 0;
   for (i = 4; i < sizeof(maximum); i++)
      maximum[i] = (i & 1) ? 0x5a : 0;
   picture.tiles.layout.columns = picture.tiles.layout.rows = 64;
   picture.tiles.layout.columns_log2 = picture.tiles.layout.rows_log2 = 6;
   picture.tiles.layout.size_bytes = 1;
   assert(rav1_tile_group_init(&picture.tile_group, maximum + 3,
         sizeof(maximum) - 3, &picture.tiles.layout, true));
   assert(vaapi_av1_tiles(
         slices, 4096, &count, maximum, sizeof(maximum), &picture));
   assert(count == 4096);
   for (i = 0; i < count; i++)
      check_record(i, i == 4095 ? 8194 : 5 + 2 * i, 1, 64);
   puts("PASS: typed VA AV1 tile spans, reserved bytes, bounded capacity and "
        "unchanged refusal; no native calls");
   return 0;
}
