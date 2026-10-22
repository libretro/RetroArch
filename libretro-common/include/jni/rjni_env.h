/* Copyright (C) 2010-2026 The RetroArch team.
 * SPDX-License-Identifier: MIT */
#ifndef __LIBRETRO_SDK_RJNI_ENV_H__
#define __LIBRETRO_SDK_RJNI_ENV_H__
#include <jni.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS
/* The calling thread's JNIEnv, attaching it to the VM on first use.
 * Returns NULL if no VM is registered or the attach fails. A thread
 * already attached by the VM (a Java-created thread) is recognised and
 * left attached at exit; only threads this function attached are
 * detached by the TLS destructor. */
JNIEnv *rjni_env(void);
RETRO_END_DECLS
#endif
