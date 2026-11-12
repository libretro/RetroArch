/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_METAL_PLATFORM_H
#define RECORD_METAL_PLATFORM_H
#include <TargetConditionals.h>
#include <Availability.h>

#if (TARGET_OS_OSX && defined(__MAC_OS_X_VERSION_MAX_ALLOWED) \
      && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101400) \
      || (TARGET_OS_IPHONE && defined(__IPHONE_OS_VERSION_MAX_ALLOWED) \
      && __IPHONE_OS_VERSION_MAX_ALLOWED >= 130000) \
      || (TARGET_OS_TV && defined(__TV_OS_VERSION_MAX_ALLOWED) \
      && __TV_OS_VERSION_MAX_ALLOWED >= 130000)
#define RECORD_METAL_NATIVE_SDK
#endif

#define RECORD_METAL_NATIVE_AVAILABLE \
   __attribute__((availability(macos,introduced=10.14), \
      availability(ios,introduced=13.0), availability(tvos,introduced=13.0)))
#define RECORD_METAL_NATIVE_RUNTIME \
   __builtin_available(macOS 10.14, iOS 13.0, tvOS 13.0, *)
#endif
