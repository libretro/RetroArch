/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef ANDROID_IMAGE_NDK_H
#define ANDROID_IMAGE_NDK_H
#include "android_image_queue.h"
RETRO_BEGIN_DECLS
/* Cold worker factory. Runtime-loaded API 26 media functions, PRIVATE images
 * with GPU sampling usage. Returns NULL with no adoption on unavailable APIs
 * or allocation/native failure. Window is borrowed until queue destruction;
 * stop the decoder using it before closing/freeing the queue. No CPU mapping.
 * The reader/library remain owned through every returned image and GPU fence.
 */
android_image_queue_t *android_image_ndk_new(
      unsigned width, unsigned height, unsigned slots, void **window);
RETRO_END_DECLS
#endif
