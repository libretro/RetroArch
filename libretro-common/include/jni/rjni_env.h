/* Copyright (C) 2010-2026 The RetroArch team.
 * SPDX-License-Identifier: MIT */
#ifndef __LIBRETRO_SDK_RJNI_ENV_H__
#define __LIBRETRO_SDK_RJNI_ENV_H__
#include <jni.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS
/* Calling thread's JNIEnv, attaching on demand; NULL without a VM or on failure. */
JNIEnv *rjni_env(void);
RETRO_END_DECLS
#endif
