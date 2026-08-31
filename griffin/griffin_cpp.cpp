/* RetroArch - A frontend for libretro.
* Copyright (C) 2011-2017 - Daniel De Matteis
* Copyright (C) 2016-2019 - Brad Parker
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

#if defined(_WIN32) && !defined(_XBOX)
#define WIN32_LEAN_AND_MEAN
/* windows.h defines function-like min()/max() unless NOMINMAX is set, and
 * vendored SPIRV-Cross is not written to survive that: spirv_common.hpp
 * calls std::numeric_limits<int32_t>::min() unparenthesized.  Every
 * RetroArch source in this TU reaches windows.h through
 * retro_miscellaneous.h, so NOMINMAX has to be set here, before the first
 * one.  It used to be set by accident -- shader_vulkan.cpp was included
 * first and its vk_sdk_platform.h defines it -- and moving that file to
 * griffin.c broke the MSVC C++ lanes. */
#define NOMINMAX
#endif

#if defined(_MSC_VER)
#include <string.h>
#include <compat/posix_string.h>
#endif

#if defined(HAVE_OPENGL) && defined(HAVE_ANGLE)
#ifndef HAVE_OPENGLES
#define HAVE_OPENGLES  1
#endif
#if !defined(HAVE_OPENGLES3) && !defined(HAVE_OPENGLES2)
#define HAVE_OPENGLES3 1
#endif
#ifndef HAVE_EGL
#define HAVE_EGL       1
#endif
#endif

/*============================================================
UI
============================================================ */
#if defined(HAVE_QT)
#ifndef __APPLE__
#define HAVE_MAIN /* also requires defining in frontend.c */
#endif

#undef mkdir

#include "../ui/drivers/ui_qt.cpp"

#include "../ui/drivers/ui_qt_widgets.cpp"
#include "../ui/drivers/moc_ui_qt.cpp"
#include "../ui/drivers/moc_ui_qt_widgets.cpp"
#endif

/*============================================================
VIDEO DRIVER
============================================================ */
#if defined(HAVE_OPENGL_CORE) && defined(HAVE_SLANG)
#include "../gfx/drivers_shader/shader_gl3.cpp"
#endif

/* Tripwire for the invariant above: if any header included before this
 * point has defined the windows.h min()/max() macros, the vendored
 * SPIRV-Cross sources below will fail with a C2589/C2059 cascade that
 * points at SPIRV-Cross rather than at the cause.  Fail here instead. */
#if defined(min) || defined(max)
#error "windows.h min()/max() macros are defined: NOMINMAX was lost before the vendored SPIRV-Cross includes."
#endif

#if defined(HAVE_SPIRV_CROSS)
#if defined(HAVE_HLSL)
#include "../deps/SPIRV-Cross/spirv_hlsl.cpp"
#endif
#include "../deps/SPIRV-Cross/spirv_cross.cpp"
#include "../deps/SPIRV-Cross/spirv_cfg.cpp"
#include "../deps/SPIRV-Cross/spirv_glsl.cpp"
#include "../deps/SPIRV-Cross/spirv_msl.cpp"
#include "../deps/SPIRV-Cross/spirv_parser.cpp"
/* The C API wrapper compiles its backend sections only when these
 * are defined truthy.  INVARIANT: each SPIRV_CROSS_C_API_* macro must
 * be truthy exactly when the matching backend source is amalgamated
 * above - a wrapper section compiled against an absent backend is an
 * undefined-symbol link failure on every lane lacking that backend's
 * feature flag (spirv_hlsl.cpp is HAVE_HLSL-gated; glsl and msl are
 * unconditional here). */
#ifndef SPIRV_CROSS_C_API_GLSL
#define SPIRV_CROSS_C_API_GLSL 1
#endif
#ifndef SPIRV_CROSS_C_API_HLSL
#if defined(HAVE_HLSL)
#define SPIRV_CROSS_C_API_HLSL 1
#else
#define SPIRV_CROSS_C_API_HLSL 0
#endif
#endif
#ifndef SPIRV_CROSS_C_API_MSL
#define SPIRV_CROSS_C_API_MSL 1
#endif
#ifndef SPIRV_CROSS_C_API_CPP
#define SPIRV_CROSS_C_API_CPP 0
#endif
#ifndef SPIRV_CROSS_C_API_REFLECT
#define SPIRV_CROSS_C_API_REFLECT 0
#endif
#include "../deps/SPIRV-Cross/spirv_cross_c.cpp"
#include "../deps/SPIRV-Cross/spirv_cross_parsed_ir.cpp"
#ifdef HAVE_SLANG

#endif
#endif

#ifdef WANT_GLSLANG
#ifdef _WIN32
#include "../deps/glslang/glslang/glslang/OSDependent/Windows/ossource.cpp"
#endif

#if defined(__linux__)
#include "../deps/glslang/glslang/glslang/OSDependent/Unix/ossource.cpp"
#endif
#endif
