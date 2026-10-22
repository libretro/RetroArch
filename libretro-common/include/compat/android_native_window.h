/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef RETRO_COMPAT_ANDROID_NATIVE_WINDOW_H
#define RETRO_COMPAT_ANDROID_NATIVE_WINDOW_H
/* NDK r29 declares ANativeWindow_clearFrameRate as plain inline. Under GNU89
 * semantics that emits an external definition, even when the newer-API helper
 * is unused. Scope static inline to this SDK header only; preserve caller
 * macros and leave project functions and C99/C++ semantics untouched. */
#if !defined(__cplusplus) && (!defined(__STDC_VERSION__) || __STDC_VERSION__<199901L)
#pragma push_macro("inline")
#undef inline
#define inline static __inline__
#include <android/native_window.h>
#undef inline
#pragma pop_macro("inline")
#else
#include <android/native_window.h>
#endif
#endif
