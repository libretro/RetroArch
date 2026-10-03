/* Stand-in for the macOS/iOS SDK header: only the CoreFoundation
 * declarations gfx/drivers_font_renderer/coretext.c uses, so the
 * compile matrix can check that file in an Apple shape. */
#ifndef STUB_CF_H
#define STUB_CF_H
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
typedef long CFIndex;
typedef unsigned short UniChar;
typedef const void *CFTypeRef;
typedef const struct __CFString *CFStringRef;
typedef const struct __CFDictionary *CFDictionaryRef;
typedef const struct __CFAllocator *CFAllocatorRef;
typedef const struct __CFBoolean *CFBooleanRef;
typedef const struct __CFAttributedString *CFAttributedStringRef;
typedef struct { int v; } CFDictionaryKeyCallBacks;
typedef struct { int v; } CFDictionaryValueCallBacks;
extern const CFBooleanRef kCFBooleanTrue;
extern const CFDictionaryKeyCallBacks kCFTypeDictionaryKeyCallBacks;
extern const CFDictionaryValueCallBacks kCFTypeDictionaryValueCallBacks;
void CFRelease(CFTypeRef);
CFTypeRef CFRetain(CFTypeRef);
CFStringRef CFStringCreateWithCharacters(CFAllocatorRef, const UniChar *, CFIndex);
CFDictionaryRef CFDictionaryCreate(CFAllocatorRef, const void **, const void **, CFIndex, const CFDictionaryKeyCallBacks *, const CFDictionaryValueCallBacks *);
CFAttributedStringRef CFAttributedStringCreate(CFAllocatorRef, CFStringRef, CFDictionaryRef);
#endif
