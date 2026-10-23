/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <VideoToolbox/VideoToolbox.h>
static unsigned av1_test_opens;
unsigned videotoolbox_av1_test_session_opens(void)
{
   return av1_test_opens;
}
static OSStatus av1_test_session_create(CFAllocatorRef allocator,
      CMVideoFormatDescriptionRef format, CFDictionaryRef specification,
      CFDictionaryRef attributes,
      const VTDecompressionOutputCallbackRecord *callback,
      VTDecompressionSessionRef *session)
{
   av1_test_opens++;
   return VTDecompressionSessionCreate(
         allocator, format, specification, attributes, callback, session);
}
#define VTDecompressionSessionCreate av1_test_session_create
