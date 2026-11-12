/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VIDEOTOOLBOX_ENCODE_HARDWARE_H
#define RETROARCH_VIDEOTOOLBOX_ENCODE_HARDWARE_H

#if defined(__APPLE__) && !defined(VIDEOTOOLBOX_ENCODE_TEST_TYPES)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#define VT_ENCODE_MOBILE
#endif
#endif

#if !defined(VT_ENCODE_MOBILE) \
      || (defined(__IPHONE_OS_VERSION_MAX_ALLOWED) && __IPHONE_OS_VERSION_MAX_ALLOWED >= 170400) \
      || (defined(__TV_OS_VERSION_MAX_ALLOWED) && __TV_OS_VERSION_MAX_ALLOWED >= 170400) \
      || (defined(__VISION_OS_VERSION_MAX_ALLOWED) && __VISION_OS_VERSION_MAX_ALLOWED >= 10100)
#define VT_ENCODE_HAS_HARDWARE_KEYS
#endif

#if defined(VT_ENCODE_MOBILE) && defined(VT_ENCODE_HAS_HARDWARE_KEYS)
#define VT_ENCODE_HARDWARE_KEYS_AVAILABLE \
   __builtin_available(iOS 17.4, tvOS 17.4, macCatalyst 17.4, visionOS 1.1, *)
#elif defined(VT_ENCODE_HAS_HARDWARE_KEYS)
#define VT_ENCODE_HARDWARE_KEYS_AVAILABLE true
#else
#define VT_ENCODE_HARDWARE_KEYS_AVAILABLE false
#endif

#if defined(VT_ENCODE_MOBILE) || defined(VIDEOTOOLBOX_ENCODE_HARDWARE_LIST_TEST)
/* Cold admission only. Return a retained, advertised hardware encoder ID;
 * the caller must independently confirm the session selected that same ID. */
static INLINE CFStringRef videotoolbox_encode_hardware_id(
      CFStringRef requested, uint32_t codec_type)
{
#if defined(VIDEOTOOLBOX_ENCODE_HARDWARE_LIST_TEST) \
      || (defined(__IPHONE_OS_VERSION_MAX_ALLOWED) && __IPHONE_OS_VERSION_MAX_ALLOWED >= 130000) \
      || (defined(__TV_OS_VERSION_MAX_ALLOWED) && __TV_OS_VERSION_MAX_ALLOWED >= 130000) \
      || defined(__VISION_OS_VERSION_MAX_ALLOWED)
#ifndef VIDEOTOOLBOX_ENCODE_HARDWARE_LIST_TEST
   if (__builtin_available(iOS 13.0, tvOS 13.0, macCatalyst 13.1, visionOS 1.0, *))
#endif
   {
      CFArrayRef list = NULL;
      CFStringRef selected = NULL;
      CFIndex i, count;
      OSStatus status = VTCopyVideoEncoderList(NULL, &list);
      if (status || !list)
      {
         if (list)
            CFRelease(list);
         return NULL;
      }
      count = CFArrayGetCount(list);
      if (count >= 0 && count <= 4096)
         for (i = 0; i < count; i++)
         {
            CFDictionaryRef entry = (CFDictionaryRef)CFArrayGetValueAtIndex(list, i);
            CFTypeRef hardware, codec, identifier;
            int64_t value = 0;
            if (!entry || CFGetTypeID(entry) != CFDictionaryGetTypeID())
               continue;
            hardware = CFDictionaryGetValue(entry, kVTVideoEncoderList_IsHardwareAccelerated);
            codec = CFDictionaryGetValue(entry, kVTVideoEncoderList_CodecType);
            identifier = CFDictionaryGetValue(entry, kVTVideoEncoderList_EncoderID);
            if (!hardware || CFGetTypeID(hardware) != CFBooleanGetTypeID()
                  || !CFBooleanGetValue((CFBooleanRef)hardware)
                  || !codec || CFGetTypeID(codec) != CFNumberGetTypeID()
                  || !CFNumberGetValue((CFNumberRef)codec, kCFNumberSInt64Type, &value)
                  || value != (int64_t)codec_type
                  || !identifier || CFGetTypeID(identifier) != CFStringGetTypeID()
                  || !CFStringGetLength((CFStringRef)identifier)
                  || (requested && !CFEqual(identifier, requested)))
               continue;
            selected = (CFStringRef)CFRetain(identifier);
            break;
         }
      CFRelease(list);
      return selected;
   }
#endif
   (void)requested;
   (void)codec_type;
   return NULL;
}
#endif
#endif
