/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef RPYROWAVE_GPU_PACKET_VECTORS_H
#define RPYROWAVE_GPU_PACKET_VECTORS_H

static int rpy_gpu_fault_prepare(const rpyrowave_gpu_layout_t *layout, uint8_t *workspace,
      const uint8_t *frame, size_t frame_bytes, unsigned kind,
      rpyrowave_packet_parameters_t *parameters)
{
   static const uint8_t missing_sign[12] =
   { 1, 0, 3, 0, 128, 0, 0, 0, 1, 0, 0, 255 };
   uint8_t *payload = workspace + layout->wire_offset;
   if (frame_bytes < 16 || frame_bytes > layout->maximum_bytes)
      return 0;
   memcpy(payload, frame, frame_bytes);
   if (!kind)
   {
      unsigned mask = frame[8] | ((unsigned)frame[9] << 8);
      unsigned groups = 0;
      for (; mask; mask >>= 1)
         groups += mask & 1;
      if (!groups || frame_bytes < 16 + groups * 3)
         return 0;
      memset(payload + 16, 255, groups * 2);
      memset(payload + 16 + groups * 2, 15, groups);
   }
   else
   {
      payload[3] &= 0x8f;
      payload[4] = 1;
      payload[5] = payload[6] = 0;
      memcpy(payload + 8, missing_sign, sizeof(missing_sign));
      frame_bytes = 20;
   }
   return !rpyrowave_gpu_index_frame(layout, workspace, layout->workspace_bytes,
         frame_bytes, 0, parameters);
}
#endif
