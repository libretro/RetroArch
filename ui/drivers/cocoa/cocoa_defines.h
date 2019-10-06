/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2019 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __COCOA_COMMON_DEFINES_H
#define __COCOA_COMMON_DEFINES_H

#include <AvailabilityMacros.h>

#ifndef MAC_OS_X_VERSION_10_12
#define MAC_OS_X_VERSION_10_12 101200
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_12
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
#endif

#endif
