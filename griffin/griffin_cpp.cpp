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
#ifndef __APPLE__
#define HAVE_MAIN /* also requires defining in frontend.c */
#endif

#undef mkdir

#include "../ui/drivers/ui_qt.cpp"

#include "../ui/drivers/qt/ui_qt_window.cpp"
#include "../ui/drivers/qt/ui_qt_load_core_window.cpp"
#include "../ui/drivers/qt/ui_qt_browser_window.cpp"
#include "../ui/drivers/qt/ui_qt_msg_window.cpp"
#include "../ui/drivers/qt/ui_qt_application.cpp"
#include "../ui/drivers/qt/gridview.cpp"
#include "../ui/drivers/qt/shaderparamsdialog.cpp"
#include "../ui/drivers/qt/coreoptionsdialog.cpp"
#include "../ui/drivers/qt/filedropwidget.cpp"
#include "../ui/drivers/qt/coreinfodialog.cpp"
#include "../ui/drivers/qt/playlistentrydialog.cpp"
#include "../ui/drivers/qt/viewoptionsdialog.cpp"
#include "../ui/drivers/qt/qt_playlist.cpp"
#include "../ui/drivers/qt/updateretroarch.cpp"
#include "../ui/drivers/qt/thumbnaildownload.cpp"
#include "../ui/drivers/qt/thumbnailpackdownload.cpp"
#include "../ui/drivers/qt/playlistthumbnaildownload.cpp"
#include "../ui/drivers/moc_ui_qt.cpp"
#include "../ui/drivers/qt/moc_coreinfodialog.cpp"
#include "../ui/drivers/qt/moc_coreoptionsdialog.cpp"
#include "../ui/drivers/qt/moc_filedropwidget.cpp"
#include "../ui/drivers/qt/moc_gridview.cpp"
#include "../ui/drivers/qt/moc_playlistentrydialog.cpp"
#include "../ui/drivers/qt/moc_shaderparamsdialog.cpp"
#include "../ui/drivers/qt/moc_ui_qt_load_core_window.cpp"
#include "../ui/drivers/qt/moc_viewoptionsdialog.cpp"
#endif

/*============================================================
VIDEO DRIVER
============================================================ */
#ifdef HAVE_VULKAN
#include "../gfx/drivers_shader/shader_vulkan.cpp"
#endif

#if defined(HAVE_OPENGL) && defined(HAVE_OPENGL_CORE)
#include "../gfx/drivers_shader/shader_gl_core.cpp"
#endif

#if defined(HAVE_SPIRV_CROSS)
#if defined(ENABLE_HLSL)
#include "../deps/SPIRV-Cross/spirv_hlsl.cpp"
#endif
#include "../deps/SPIRV-Cross/spirv_cross.cpp"
#include "../deps/SPIRV-Cross/spirv_cfg.cpp"
#include "../deps/SPIRV-Cross/spirv_glsl.cpp"
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

#if defined(HAVE_DISCORD)
#include "../deps/discord-rpc/src/discord_rpc.cpp"
#include "../deps/discord-rpc/src/rpc_connection.cpp"
#include "../deps/discord-rpc/src/serialization.cpp"

#if defined(_WIN32)
#include "../deps/discord-rpc/src/discord_register_win.cpp"
#include "../deps/discord-rpc/src/connection_win.cpp"
#endif
#if defined(__linux__)
#include "../deps/discord-rpc/src/discord_register_linux.cpp"
#endif
#if defined(__unix__) || defined(__APPLE__)
#include "../deps/discord-rpc/src/connection_unix.cpp"
#endif
#endif
