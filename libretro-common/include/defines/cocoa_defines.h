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
#define RARCH_RETAIN(x)         (x)
#define RARCH_RELEASE(x)        ((void)0)
#define RARCH_AUTORELEASE(x)    ((void)0)
#define RARCH_SUPER_DEALLOC()   ((void)0)
#else
#define RARCH_RETAIN(x)         [(x) retain]
#define RARCH_RELEASE(x)        [(x) release]
#define RARCH_AUTORELEASE(x)    [(x) autorelease]
#define RARCH_SUPER_DEALLOC()   [super dealloc]
#endif

#endif
