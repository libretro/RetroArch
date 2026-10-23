/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_ANDROID_CAPTURE_TEST_TYPES_H
#define RECORD_ANDROID_CAPTURE_TEST_TYPES_H
#include <stdint.h>
typedef struct AHardwareBuffer_Desc
{
   uint32_t width, height, layers, format;
   uint64_t usage;
   uint32_t stride, rfu0;
   uint64_t rfu1;
} AHardwareBuffer_Desc;
typedef struct AHardwareBuffer
{
   AHardwareBuffer_Desc desc;
   unsigned refs;
} AHardwareBuffer;
#define AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM 1
#define AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT 0x16
#define AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM 0x2b
#define AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE 256
#define AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT 16384
#endif
