/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef MEDIACODEC_INVENTORY_H
#define MEDIACODEC_INVENTORY_H
#include <jni.h>
#include <stddef.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
/* Cold JNI admission, API 29+. Returns 1 with an explicit non-alias hardware
 * codec name, 0 for unsupported/unknown, -1 for runtime failure. No capability
 * is advertised by this query: session creation and GPU import are separate.
 * profile is an Android CodecProfileLevel value, not a codec bitstream value.
 * Output is cleared on every non-success result. An incoming JNI exception is
 * left untouched; exceptions raised by this operation are cleared. */
int mediacodec_inventory_select(JNIEnv *env, const char *mime, int encoder,
      unsigned width, unsigned height, int profile, char *name,
      size_t capacity);
RETRO_END_DECLS
#endif
