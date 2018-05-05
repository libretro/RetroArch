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

#if defined(HAVE_ZLIB) || defined(HAVE_7ZIP)
#define HAVE_COMPRESSION 1
#endif

#if defined(_MSC_VER)
#include <string.h>
#include <compat/posix_string.h>
#endif

/*============================================================
MENU
============================================================ */
#ifdef HAVE_XUI
#include "../menu/drivers/xui.cpp"
#endif

/*============================================================
UI
============================================================ */
#if defined(HAVE_QT)
#define HAVE_MAIN /* also requires defining in frontend.c */
#include "../ui/drivers/ui_qt.cpp"

#include "../ui/drivers/qt/ui_qt_window.cpp"
#include "../ui/drivers/qt/ui_qt_load_core_window.cpp"
#include "../ui/drivers/qt/ui_qt_browser_window.cpp"
#include "../ui/drivers/qt/ui_qt_msg_window.cpp"
#include "../ui/drivers/qt/ui_qt_application.cpp"
#endif

/*============================================================
VIDEO DRIVER
============================================================ */
#ifdef HAVE_VULKAN
#include "../gfx/drivers_shader/shader_vulkan.cpp"
#endif

#ifdef HAVE_SPIRV_CROSS
#include "../deps/SPIRV-Cross/spirv_cross.cpp"
#include "../deps/SPIRV-Cross/spirv_cfg.cpp"
#include "../deps/SPIRV-Cross/spirv_glsl.cpp"
#include "../deps/SPIRV-Cross/spirv_hlsl.cpp"
#include "../deps/SPIRV-Cross/spirv_msl.cpp"
#ifdef HAVE_SLANG
#include "../gfx/drivers_shader/glslang_util.cpp"
#include "../gfx/drivers_shader/slang_preprocess.cpp"
#include "../gfx/drivers_shader/slang_process.cpp"
#include "../gfx/drivers_shader/slang_reflection.cpp"
#endif
#endif

/*============================================================
FONTS
============================================================ */
#if defined(_XBOX360)
#include "../gfx/drivers_font/xdk360_fonts.cpp"
#endif

#ifdef WANT_GLSLANG
#ifdef _WIN32
#include "../deps/glslang/glslang/glslang/OSDependent/Windows/ossource.cpp"
#endif

#if defined(__linux__)
#include "../deps/glslang/glslang/glslang/OSDependent/Unix/ossource.cpp"
#endif
#endif
