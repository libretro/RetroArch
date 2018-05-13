/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2016-2017 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef _VULKAN_VKSYM_H
#define _VULKAN_VKSYM_H

#include <boolean.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_WAYLAND
#define VK_USE_PLATFORM_WAYLAND_KHR
#endif

#ifdef HAVE_MIR
#define VK_USE_PLATFORM_MIR_KHR
#endif

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#ifdef ANDROID
#define VK_USE_PLATFORM_ANDROID_KHR
#endif

#ifdef HAVE_XCB
#define VK_USE_PLATFORM_XCB_KHR
#endif

#ifdef HAVE_XLIB
#define VK_USE_PLATFORM_XLIB_KHR
#endif

#ifdef HAVE_COCOA
#define VK_USE_PLATFORM_MACOS_MVK
#endif

#ifdef HAVE_COCOATOUCH
#define VK_USE_PLATFORM_IOS_MVK
#endif

#include <vulkan/vulkan_symbol_wrapper.h>

#endif
