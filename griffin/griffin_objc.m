/* RetroArch - A frontend for libretro.
* Copyright (C) 2011-2017 - Daniel De Matteis
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

#if defined(HAVE_COCOATOUCH) || defined(HAVE_COCOA) || defined(HAVE_COCOA_METAL)

#include "../ui/drivers/cocoa/cocoa_common.m"
#include "../gfx/drivers_context/cocoa_gl_ctx.m"

#if defined(HAVE_COCOATOUCH)

#if TARGET_OS_IOS
#include "../ui/drivers/cocoa/cocoatouch_menu.m"
#endif
#include "../ui/drivers/ui_cocoatouch.m"

#else

#if defined(HAVE_COCOA) || defined(HAVE_COCOA_METAL)
#include "../ui/drivers/cocoa/ui_cocoa_window.m"
#include "../ui/drivers/cocoa/ui_cocoa_browser_window.m"
#include "../ui/drivers/cocoa/ui_cocoa_application.m"
#include "../ui/drivers/cocoa/ui_cocoa_msg_window.m"
#include "../ui/drivers/ui_cocoa.m"
#endif

#endif

#endif

#ifdef HAVE_MFI
#include "../input/drivers_joypad/mfi_joypad.m"
#endif

#ifdef HAVE_COREAUDIO3
#include "../audio/drivers/coreaudio3.m"
#endif

#if defined(HAVE_DISCORD)
#include "../deps/discord-rpc/src/discord_register_osx.m"
#endif

#ifdef HAVE_METAL
#import "../gfx/common/metal/Context.m"
#import "../gfx/common/metal/Filter.m"
#import "../gfx/common/metal/RendererCommon.m"
#import "../gfx/common/metal/View.m"
#import "../gfx/common/metal/TexturedView.m"
#import "../gfx/common/metal/MenuDisplay.m"
#import "../gfx/common/metal_common.m"
#import "../gfx/drivers/metal.m"
#import "../menu/drivers_display/menu_display_metal.m"
#import "../gfx/drivers_font/metal_raster_font.m"
#endif
