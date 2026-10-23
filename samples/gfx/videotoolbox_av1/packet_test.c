/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../../../gfx/common/videotoolbox_av1_packet.h"
#ifdef NDEBUG
#error Assertions are required for this test
#endif

int main(void)
{
   static const uint8_t frame[] = {0x32, 2, 0x11, 0x22};
   static const uint8_t unsized[] = {0x30, 0x11, 0x22};
   static const uint8_t repeat[] = {0x1a, 1, 0x80};
   static const uint8_t layered[] = {0x36, 0x20, 1, 0};
   static const uint8_t extension[] = {0x36, 0, 1, 0x55};
   uint8_t storage[256], snapshot[256], large[130];
   videotoolbox_av1_packet_t packet;
   rav1_obu_t obu;
   size_t offset;
   unsigned i;
   assert(!videotoolbox_av1_packet_init(&packet, NULL, sizeof(storage)));
   assert(videotoolbox_av1_packet_init(&packet, storage, sizeof(storage)));
   assert(!videotoolbox_av1_packet_append(
         &packet, unsized, sizeof(unsized), false, -1));
   assert(packet.count == 1 && packet.size == sizeof(frame)
         && !memcmp(storage, frame, sizeof(frame)) && !packet.ready);
   assert(!videotoolbox_av1_packet_append(
         &packet, repeat, sizeof(repeat), true, 7));
   assert(packet.ready && packet.display_slot == 7 && packet.count == 2);
   memcpy(snapshot, storage, packet.size);
   assert(videotoolbox_av1_packet_append(&packet, frame, sizeof(frame), true, 1)
         == 1);
   assert(!memcmp(storage, snapshot, packet.size));
   offset = 0;
   assert(rav1_obu_next(&obu, storage, packet.size, &offset, false) == 1);
   assert(obu.type == 6 && obu.payload_size == 2);
   assert(rav1_obu_next(&obu, storage, packet.size, &offset, false) == 1);
   assert(obu.type == 3 && obu.payload[0] == 0x80 && offset == packet.size);
   videotoolbox_av1_packet_reset(&packet);
   for (i = 0; i < 8; i++)
      assert(!videotoolbox_av1_packet_append(
            &packet, frame, sizeof(frame), false, -1));
   assert(packet.ready && packet.count == 8 && packet.display_slot == -1);
   assert(videotoolbox_av1_packet_init(&packet, storage, 6));
   assert(!videotoolbox_av1_packet_append(
         &packet, frame, sizeof(frame), false, -1));
   assert(videotoolbox_av1_packet_append(
                &packet, repeat, sizeof(repeat), true, 0)
         == 1);
   assert(packet.ready && packet.count == 1 && packet.size == 4);
   videotoolbox_av1_packet_reset(&packet);
   assert(videotoolbox_av1_packet_append(
                &packet, layered, sizeof(layered), true, 0)
         < 0);
   assert(videotoolbox_av1_packet_append(
                &packet, repeat, sizeof(repeat), false, -1)
         < 0);
   assert(videotoolbox_av1_packet_append(
                &packet, frame, sizeof(frame) - 1, true, 0)
         < 0);
   assert(!packet.count && !packet.size && !packet.ready);
   assert(!videotoolbox_av1_packet_append(
         &packet, extension, sizeof(extension), true, 0));
   assert(!memcmp(storage, extension, sizeof(extension)));
   assert(videotoolbox_av1_packet_init(&packet, storage, sizeof(storage)));
   memcpy(storage, unsized, sizeof(unsized));
   assert(!videotoolbox_av1_packet_append(
         &packet, storage, sizeof(unsized), true, 0));
   assert(!memcmp(storage, frame, sizeof(frame)));
   memset(large, 0x55, sizeof(large));
   large[0] = 0x30;
   assert(videotoolbox_av1_packet_init(&packet, storage, 131));
   assert(videotoolbox_av1_packet_append(&packet, large, sizeof(large), true, 0)
         < 0);
   assert(!packet.size && !packet.count && !packet.ready);
   assert(videotoolbox_av1_packet_init(&packet, storage, 132));
   assert(!videotoolbox_av1_packet_append(
         &packet, large, sizeof(large), true, 31));
   assert(packet.size == 132 && storage[0] == 0x32 && storage[1] == 0x81
         && storage[2] == 1);
   assert(!memcmp(storage + 3, large + 1, 129));
   puts("PASS: bounded AV1 packet staging, sized OBUs, retry and aliasing");
   return 0;
}
