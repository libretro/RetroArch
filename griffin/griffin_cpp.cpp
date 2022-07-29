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

#include "../ui/drivers/qt/gridview.cpp"
#include "../ui/drivers/qt/qt_dialogs.cpp"
#include "../ui/drivers/qt/qt_widgets.cpp"
#include "../ui/drivers/qt/qt_playlist.cpp"
#include "../ui/drivers/qt/qt_downloads.cpp"
#ifdef HAVE_MENU
#include "../ui/drivers/qt/qt_options.cpp"
#include "../ui/drivers/qt/moc_options.cpp"
#include "../ui/drivers/qt/moc_settingswidgets.cpp"
#endif
#include "../ui/drivers/moc_ui_qt.cpp"
#include "../ui/drivers/qt/moc_coreinfodialog.cpp"
#include "../ui/drivers/qt/moc_coreoptionsdialog.cpp"
#include "../ui/drivers/qt/moc_filedropwidget.cpp"
#include "../ui/drivers/qt/moc_gridview.cpp"
#include "../ui/drivers/qt/moc_playlistentrydialog.cpp"
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#include "../ui/drivers/qt/moc_shaderparamsdialog.cpp"
#endif
#include "../ui/drivers/qt/moc_ui_qt_load_core_window.cpp"
#include "../ui/drivers/qt/moc_viewoptionsdialog.cpp"
#endif

/*============================================================
VIDEO DRIVER
============================================================ */
#ifdef HAVE_VULKAN
#include "../gfx/drivers_shader/shader_vulkan.cpp"
#endif

#if defined(HAVE_OPENGL_CORE)
#include "../gfx/drivers_shader/shader_gl3.cpp"
#endif

#if defined(HAVE_SPIRV_CROSS)
#if defined(ENABLE_HLSL)
#include "../deps/SPIRV-Cross/spirv_hlsl.cpp"
#endif
#include "../deps/SPIRV-Cross/spirv_cross.cpp"
#include "../deps/SPIRV-Cross/spirv_cfg.cpp"
#include "../deps/SPIRV-Cross/spirv_glsl.cpp"
#include "../deps/SPIRV-Cross/spirv_msl.cpp"
#include "../deps/SPIRV-Cross/spirv_parser.cpp"
#include "../deps/SPIRV-Cross/spirv_cross_parsed_ir.cpp"
#ifdef HAVE_SLANG
#include "../gfx/drivers_shader/glslang_util_cxx.cpp"
#include "../gfx/drivers_shader/slang_process.cpp"
#include "../gfx/drivers_shader/slang_reflection.cpp"
#endif
#endif

/*============================================================
FONTS
============================================================ */
#if defined(HAVE_DISCORD)
#include "../deps/discord-rpc/src/discord_rpc.cpp"
#include "../deps/discord-rpc/src/rpc_connection.cpp"
#include "../deps/discord-rpc/src/serialization.cpp"

#if defined(_WIN32)
#include "../deps/discord-rpc/src/connection_win.cpp"
#endif
#if defined(__unix__) || defined(__APPLE__)
#include "../deps/discord-rpc/src/connection_unix.cpp"
#endif
#endif
