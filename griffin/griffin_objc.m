/* RetroArch - A frontend for libretro.
* Copyright (C) 2011-2015 - Daniel De Matteis
*
* RetroArch is free software: you can redistribute it and/or modify it under the terms
* of the GNU General Public License as published by the Free Software Found-
* ation, either version 3 of the License, or (at your option) any later version.
*
* RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
* PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with RetroArch.
* If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef IOS
#include <Availability.h>
#else
#include <AvailabilityMacros.h>
#endif

#ifndef __IPHONE_OS_VERSION_MAX_ALLOWED
#define __IPHONE_OS_VERSION_MAX_ALLOWED 00000
#endif

#if defined(HAVE_COCOATOUCH) || defined(HAVE_COCOA)
#include "../gfx/drivers_context/apple_cocoa_gl.m"
#include "../apple/common/apple_cocoa_common.m"

#if defined(HAVE_COCOATOUCH)

#if TARGET_OS_IPHONE
#include "../apple/iOS/menu.m"
#include "../apple/iOS/browser.m"
#include "../apple/iOS/platform.m"
#include "../ui/drivers/ui_cocoatouch.m"
#endif

#elif defined(HAVE_COCOA)

#include "../apple/OSX/platform.m"
#include "../apple/OSX/settings.m"
#endif

#endif

#ifdef HAVE_MFI
#include "../input/drivers_hid/mfi_hid.m"
#endif
