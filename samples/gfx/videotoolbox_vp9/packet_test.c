/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../../../gfx/common/videotoolbox_vp9_packet.h"

static uint8_t storage[4096], input[1024];
static videotoolbox_vp9_packet_t packet;

/* Independent demux-style index reader verifies every payload boundary. */
static void check_index(unsigned frames, size_t bytes)
{
   size_t index, offset = 0, size;
   unsigned i, j;
   uint8_t marker = storage[packet.size - 1];
   assert(packet.ready && packet.count == frames);
   if (frames == 1)
   {
      assert(packet.size == bytes);
      return;
   }
   assert((marker & 0xe0) == 0xc0 && (unsigned)(marker & 7) + 1 == frames);
   assert(((marker >> 3) & 3) + 1 == 4);
   index = packet.size - 2 - frames * 4;
   assert(storage[index++] == marker);
   for (i = 0; i < frames; i++)
   {
      size = 0;
      for (j = 0; j < 4; j++)
         size |= (size_t)storage[index++] << (j * 8);
      assert(size == bytes && storage[offset] == (uint8_t)i);
      offset += size;
   }
   assert(index == packet.size - 1 && offset == frames * bytes);
}

int main(void)
{
   unsigned i, count;
   size_t before;
   assert(!videotoolbox_vp9_packet_init(NULL, storage, sizeof(storage)));
   assert(!videotoolbox_vp9_packet_init(&packet, storage, 34));
   assert(videotoolbox_vp9_packet_init(&packet, storage, sizeof(storage)));
   assert(videotoolbox_vp9_packet_seal(&packet) < 0);
   for (count = 1; count <= 8; count++)
   {
      videotoolbox_vp9_packet_reset(&packet);
      for (i = 0; i < count; i++)
      {
         memset(input, (int)i, 257);
         assert(!videotoolbox_vp9_packet_append(
               &packet, input, 257, i == count - 1, 31));
         memset(input, 0xff, 257);
      }
      check_index(count, 257);
      assert(packet.display_slot == 31);
      before = packet.size;
      assert(videotoolbox_vp9_packet_append(&packet, input, 1, true, 0) == 1);
      assert(!videotoolbox_vp9_packet_seal(&packet) && packet.size == before);
      check_index(count, 257);
   }
   videotoolbox_vp9_packet_reset(&packet);
   for (i = 0; i < 8; i++)
   {
      input[0] = (uint8_t)i;
      assert(!videotoolbox_vp9_packet_append(&packet, input, 1, false, -1));
   }
   check_index(8, 1);
   assert(packet.display_slot == -1);
   videotoolbox_vp9_packet_reset(&packet);
   input[0] = 0;
   assert(!videotoolbox_vp9_packet_append(&packet, input, 1, false, -1));
   assert(!packet.ready && !videotoolbox_vp9_packet_seal(&packet));
   check_index(1, 1);
   assert(packet.display_slot == -1);
   videotoolbox_vp9_packet_reset(&packet);
   assert(videotoolbox_vp9_packet_append(&packet, input, 0, false, -1) < 0);
   assert(videotoolbox_vp9_packet_append(&packet, input, 1, true, 32) < 0);
   assert(videotoolbox_vp9_packet_append(
                &packet, input, sizeof(storage), false, -1)
          < 0);
   assert(!packet.size && !packet.count);
   assert(videotoolbox_vp9_packet_init(&packet, storage, 35));
   input[0] = 0;
   assert(!videotoolbox_vp9_packet_append(&packet, input, 1, false, -1));
   assert(videotoolbox_vp9_packet_append(&packet, input, 1, false, -1) == 1);
   assert(packet.size == 1 && packet.count == 1 && packet.ready);
   assert(!videotoolbox_vp9_packet_seal(&packet));
   check_index(1, 1);
   puts("PASS: VP9 owned batching, independent Annex B index, display/backpressure and hidden terminal input");
   return 0;
}
