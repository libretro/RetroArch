/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VULKAN_ENCODE_PACKET_H
#define RARCH_VULKAN_ENCODE_PACKET_H
#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
enum vulkan_encode_result
{
   VULKAN_ENCODE_ERROR = -1,
   VULKAN_ENCODE_OK,
   VULKAN_ENCODE_BUSY,
   VULKAN_ENCODE_END
};
typedef struct vulkan_encode_packet
{
   const void *data;
   size_t size;
   uint64_t timestamp, duration;
   unsigned slot;
   bool keyframe;
} vulkan_encode_packet_t;

#endif
