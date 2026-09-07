/* Copyright (C) 2010-2021 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (cocoa_defines.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __COCOA_COMMON_DEFINES_H
#define __COCOA_COMMON_DEFINES_H

#include <AvailabilityMacros.h>
#ifdef __MACH__
#include <TargetConditionals.h>
#endif

/* TARGET_OS_OSX is Apple's own name for "macOS, not iOS or tvOS" and
 * is what this tree tests, alongside TARGET_OS_IPHONE. It is only in
 * the 10.12 and later SDKs, so on an older one it is filled in here:
 * on Apple, anything that is not the iPhone family is macOS. That is
 * the definition Apple's own header uses and it needs nothing but
 * TARGET_OS_IPHONE, which every SDK that has ever had an iPhone in it
 * defines - and whose absence, on a 10.4 or 10.5 SDK that predates
 * the iPhone entirely, means macOS by the same reasoning. tvOS and
 * watchOS set TARGET_OS_IPHONE, so they land on the right side too.
 *
 * The simulator is the exception that has to be named: iOS SDKs
 * before iOS 9 set TARGET_OS_IPHONE to 0 for a simulator build and
 * marked it with TARGET_IPHONE_SIMULATOR instead, so a bare
 * TARGET_OS_IPHONE test would call the old simulator a Mac. Both
 * simulator spellings are checked, the modern one and that one.
 *
 * Not defined off Apple, where #if reads it as 0.
 *
 * No line continuations here: this file has CRLF endings and a
 * backslash does not continue across one. */
#if defined(__APPLE__) && !defined(TARGET_OS_OSX)
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#define TARGET_OS_OSX 0
#elif defined(TARGET_OS_SIMULATOR) && TARGET_OS_SIMULATOR
#define TARGET_OS_OSX 0
#elif defined(TARGET_IPHONE_SIMULATOR) && TARGET_IPHONE_SIMULATOR
#define TARGET_OS_OSX 0
#else
#define TARGET_OS_OSX 1
#endif
#endif

#ifndef MAC_OS_X_VERSION_10_12
#define MAC_OS_X_VERSION_10_12 101200
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_12
#define HAS_MACOSX_10_12 0
#define NSEventModifierFlagCommand NSCommandKeyMask
#define NSEventModifierFlagControl NSControlKeyMask
#define NSEventModifierFlagHelp NSHelpKeyMask
#define NSEventModifierFlagNumericPad NSNumericPadKeyMask
#define NSEventModifierFlagOption NSAlternateKeyMask
#define NSEventModifierFlagShift NSShiftKeyMask
#define NSCompositingOperationSourceOver NSCompositeSourceOver
#define NSEventMaskApplicationDefined NSApplicationDefinedMask
#define NSEventTypeApplicationDefined NSApplicationDefined
#define NSEventTypeCursorUpdate NSCursorUpdate
#define NSEventTypeMouseMoved NSMouseMoved
#define NSEventTypeMouseEntered NSMouseEntered
#define NSEventTypeMouseExited NSMouseExited
#define NSEventTypeLeftMouseDown NSLeftMouseDown
#define NSEventTypeRightMouseDown NSRightMouseDown
#define NSEventTypeOtherMouseDown NSOtherMouseDown
#define NSEventTypeLeftMouseUp NSLeftMouseUp
#define NSEventTypeRightMouseUp NSRightMouseUp
#define NSEventTypeOtherMouseUp NSOtherMouseUp
#define NSEventTypeLeftMouseDragged NSLeftMouseDragged
#define NSEventTypeRightMouseDragged NSRightMouseDragged
#define NSEventTypeOtherMouseDragged NSOtherMouseDragged
#define NSEventTypeScrollWheel NSScrollWheel
#define NSEventTypeKeyDown NSKeyDown
#define NSEventTypeKeyUp NSKeyUp
#define NSEventTypeFlagsChanged NSFlagsChanged
#define NSEventMaskAny NSAnyEventMask
#define NSWindowStyleMaskBorderless NSBorderlessWindowMask
#define NSWindowStyleMaskClosable NSClosableWindowMask
#define NSWindowStyleMaskFullScreen NSFullScreenWindowMask
#define NSWindowStyleMaskMiniaturizable NSMiniaturizableWindowMask
#define NSWindowStyleMaskResizable NSResizableWindowMask
#define NSWindowStyleMaskTitled NSTitledWindowMask
#define NSAlertStyleCritical NSCriticalAlertStyle
#define NSAlertStyleInformational NSInformationalAlertStyle
#define NSAlertStyleWarning  NSWarningAlertStyle
#define NSEventModifierFlagCapsLock NSAlphaShiftKeyMask
#define NSControlSizeRegular NSRegularControlSize
#else
#define HAS_MACOSX_10_12 1
#endif

/* Window-related names the AppKit glue needs whatever SDK it is built
 * against.  NSFullScreenWindowMask (10.7) is an enum, so a #define is
 * not enough to know whether the SDK has it; the bit is spelled out
 * and tested against -[NSWindow styleMask] at runtime instead.  On a
 * system that predates native full-screen the bit is never set, which
 * is the answer wanted there. */
#define RARCH_NSWINDOWSTYLEMASK_FULLSCREEN (1 << 14)

/* Formal delegate protocols arrived in the 10.6 SDK; before that the
 * delegate methods are informal and adopting a protocol that the SDK
 * does not declare is a compile error.  These expand to the protocol
 * adoption where the SDK has it and to nothing where it does not; the
 * methods themselves are the same either way and AppKit finds them by
 * selector on every release. */
#ifndef MAC_OS_X_VERSION_10_6
#define MAC_OS_X_VERSION_10_6 1060
#endif
#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
#define RARCH_PROTO_NSAPPLICATIONDELEGATE , NSApplicationDelegate
#define RARCH_PROTO_NSWINDOWDELEGATE      <NSWindowDelegate>
#else
#define RARCH_PROTO_NSAPPLICATIONDELEGATE
#define RARCH_PROTO_NSWINDOWDELEGATE
#endif

/* The two pasteboard types the drop handler reads.  These are the
 * values of the 10.0 constants NSColorPboardType and
 * NSFilenamesPboardType, which the 10.14 SDK marks deprecated; the
 * strings themselves are what AppKit delivers on every release, so
 * they are spelled out here and no SDK has anything to say about it. */
#define RARCH_PBOARD_TYPE_COLOR     @"NSColorPboardType"
#define RARCH_PBOARD_TYPE_FILENAMES @"NSFilenamesPboardType"

/* -[NSView convertRectToBacking:] is 10.7; declaring it in a category
 * on SDKs that predate it gives the compiler the signature so the call
 * can be written as a plain message and the runtime answer, checked
 * with respondsToSelector:, decides whether it is sent.  Nothing is
 * implemented here: the real method is used where it exists. */
#if TARGET_OS_OSX && defined(MAC_OS_X_VERSION_MAX_ALLOWED) && (MAC_OS_X_VERSION_MAX_ALLOWED < 1070) && defined(__OBJC__)
#import <AppKit/NSView.h>
@interface NSView (RARCHBackingCompat)
- (NSRect)convertRectToBacking:(NSRect)rect;
@end
#endif

/* ARC vs MRR macros.  Under ARC, release/autorelease are forbidden
 * (compile error); under MRR (Manual Retain-Release / MRC) they are
 * required.  These macros expand to the right thing for the current
 * translation unit regardless of which mode it is compiled under.
 *
 * Previous convention used `#ifndef HAVE_COCOA_METAL` as a proxy
 * for 'are we under ARC?' because HAVE_COCOA_METAL builds happened
 * to enable ARC via CLANG_ENABLE_OBJC_ARC=YES in BaseConfig.xcconfig.
 * That's brittle - RetroArch_PPC.xcodeproj turns ARC off explicitly
 * even on the Metal branch, and the qb/make build is always MRR
 * regardless.  __has_feature(objc_arc) is the canonical discriminator.
 *
 * GCC 4.0 (Xcode 3.1) doesn't support __has_feature; polyfill to 0
 * so the MRR branch is selected (which is correct for GCC 4.0 - it
 * predates ARC entirely). */
#ifndef __has_feature
#define __has_feature(x) 0
#endif

#if __has_feature(objc_arc)
/* Ownership qualifier for an ivar that backs an 'assign' object
 * property: ARC requires the ivar to say it does not retain, and no
 * other compiler knows the word. */
#define RARCH_UNSAFE_UNRETAINED __unsafe_unretained
#define RARCH_RETAIN(x)         (x)
#define RARCH_RELEASE(x)        ((void)0)
#define RARCH_AUTORELEASE(x)    ((void)0)
#define RARCH_SUPER_DEALLOC()   ((void)0)
#define RARCH_DISPATCH_RELEASE(x) ((void)0)
/* Autorelease pool scope.  NSAutoreleasePool is a compile error under
 * ARC and @autoreleasepool is not a keyword for GCC, so the scope is
 * opened and closed through these; the body between them is the same
 * for both memory models. */
#define RARCH_AUTORELEASEPOOL_BEGIN @autoreleasepool {
#define RARCH_AUTORELEASEPOOL_END   }
#else
#define RARCH_UNSAFE_UNRETAINED
#define RARCH_RETAIN(x)         [(x) retain]
#define RARCH_RELEASE(x)        [(x) release]
#define RARCH_AUTORELEASE(x)    [(x) autorelease]
#define RARCH_SUPER_DEALLOC()   [super dealloc]
#define RARCH_AUTORELEASEPOOL_BEGIN { NSAutoreleasePool *rarch_pool_ = [[NSAutoreleasePool alloc] init];
#define RARCH_AUTORELEASEPOOL_END   [rarch_pool_ release]; }
/* GCD object release for MRR.  Use dispatch_release() rather than
 * [x release] because the latter is a compile error on pre-10.8 SDKs
 * where OS_OBJECT_USE_OBJC is 0 and dispatch_queue_t is a plain C
 * handle rather than an Objective-C object.  dispatch_release works
 * uniformly across all MRR-capable SDKs.  NULL-guarded because
 * dispatch_release(NULL) is explicitly undefined, unlike
 * [nil release] which is a defined no-op. */
#define RARCH_DISPATCH_RELEASE(x) do { if (x) dispatch_release(x); } while (0)
#endif

#endif
