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

#if defined(__APPLE__) && defined(__MACH__)
#include "../frontend/drivers/platform_darwin.m"
#endif

#if defined(HAVE_COCOATOUCH) || defined(HAVE_COCOA)
#include "../input/drivers/cocoa_input.m"
#include "../gfx/drivers_context/cocoa_gl_ctx.m"
#include "../ui/drivers/cocoa/cocoa_common.m"

#if defined(HAVE_COCOATOUCH)

#if TARGET_OS_IPHONE
#include "../ui/drivers/cocoa/cocoatouch_menu.m"

#include "../ui/drivers/ui_cocoatouch.m"
#endif

#elif defined(HAVE_COCOA)
#include "../ui/drivers/ui_cocoa.m"
#endif

#endif

#ifdef HAVE_MFI
#include "../input/drivers_hid/mfi_hid.m"
#endif
