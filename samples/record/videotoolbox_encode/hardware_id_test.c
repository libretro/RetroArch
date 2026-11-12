/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <boolean.h>
#include <retro_inline.h>

typedef struct object object_t;
typedef object_t *CFTypeRef;
typedef object_t *CFArrayRef;
typedef object_t *CFDictionaryRef;
typedef object_t *CFStringRef;
typedef object_t *CFNumberRef;
typedef object_t *CFBooleanRef;
typedef long CFIndex;
typedef int OSStatus;
struct object
{
   unsigned type, refs;
   int64_t number;
   const char *text;
   object_t *values[3];
};
#define kVTVideoEncoderList_IsHardwareAccelerated 0
#define kVTVideoEncoderList_CodecType 1
#define kVTVideoEncoderList_EncoderID 2
#define kCFNumberSInt64Type 1
static object_t list, entry, hardware, codec, identifier, requested;
static CFIndex count = 1;
static OSStatus list_status;
static unsigned visits;
static CFTypeRef CFRetain(CFTypeRef object)
{
   assert(object && object->refs);
   object->refs++;
   return object;
}
static void CFRelease(CFTypeRef object)
{
   assert(object && object->refs > 1);
   object->refs--;
}
static OSStatus VTCopyVideoEncoderList(void *options, CFArrayRef *output)
{
   (void)options;
   *output = CFRetain(&list);
   return list_status;
}
static CFIndex CFArrayGetCount(CFArrayRef array)
{
   assert(array == &list);
   return count;
}
static CFTypeRef CFArrayGetValueAtIndex(CFArrayRef array, CFIndex index)
{
   assert(array == &list && index == 0);
   visits++;
   return &entry;
}
static unsigned CFGetTypeID(CFTypeRef object)
{
   return object->type;
}
static unsigned CFDictionaryGetTypeID(void)
{
   return 1;
}
static unsigned CFBooleanGetTypeID(void)
{
   return 2;
}
static unsigned CFNumberGetTypeID(void)
{
   return 3;
}
static unsigned CFStringGetTypeID(void)
{
   return 4;
}
static CFTypeRef CFDictionaryGetValue(CFDictionaryRef object, unsigned key)
{
   assert(object == &entry && key < 3);
   return object->values[key];
}
static bool CFBooleanGetValue(CFBooleanRef value)
{
   return value->number != 0;
}
static bool CFNumberGetValue(CFNumberRef value, unsigned type, void *output)
{
   assert(type == kCFNumberSInt64Type);
   *(int64_t*)output = value->number;
   return true;
}
static unsigned CFStringGetLength(CFStringRef value)
{
   return (unsigned)strlen(value->text);
}
static bool CFEqual(CFTypeRef first, CFStringRef second)
{
   return !strcmp(first->text, second->text);
}
#define VIDEOTOOLBOX_ENCODE_HARDWARE_LIST_TEST
#include "../../../gfx/common/videotoolbox_encode_hardware.h"

int main(void)
{
   CFStringRef selected;
   list.refs = entry.refs = hardware.refs = codec.refs = identifier.refs = requested.refs = 1;
   entry.type = 1; hardware.type = 2; codec.type = 3; identifier.type = requested.type = 4;
   hardware.number = 1; codec.number = 7;
   identifier.text = requested.text = "hardware-encoder";
   entry.values[0] = &hardware;
   entry.values[1] = &codec;
   entry.values[2] = &identifier;
   selected = videotoolbox_encode_hardware_id(NULL, 7);
   assert(selected == &identifier && identifier.refs == 2 && list.refs == 1);
   CFRelease(selected);
   selected = videotoolbox_encode_hardware_id(&requested, 7);
   assert(selected == &identifier);
   CFRelease(selected);
   requested.text = "other-encoder";
   assert(!videotoolbox_encode_hardware_id(&requested, 7));
   assert(!videotoolbox_encode_hardware_id(NULL, 8));
   hardware.number = 0;
   assert(!videotoolbox_encode_hardware_id(NULL, 7));
   hardware.number = 1;
   hardware.type = 3;
   assert(!videotoolbox_encode_hardware_id(NULL, 7));
   hardware.type = 2;
   identifier.text = "";
   assert(!videotoolbox_encode_hardware_id(NULL, 7));
   identifier.text = "hardware-encoder";
   entry.values[2] = NULL;
   assert(!videotoolbox_encode_hardware_id(NULL, 7));
   entry.values[2] = &identifier;
   list_status = -1;
   assert(!videotoolbox_encode_hardware_id(NULL, 7) && list.refs == 1);
   list_status = 0;
   visits = 0;
   count = 4097;
   assert(!videotoolbox_encode_hardware_id(NULL, 7) && !visits);
   count = -1;
   assert(!videotoolbox_encode_hardware_id(NULL, 7) && !visits);
   assert(identifier.refs == 1 && list.refs == 1);
   puts("PASS advertised hardware encoder ID, codec admission, malformed lists and retained ownership");
   return 0;
}
