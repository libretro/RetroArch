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
/* windows.h defines function-like min()/max() unless NOMINMAX is set,
 * which the C++ headers this TU takes in (Qt, the standard library) are
 * not written to survive.  Every RetroArch source here reaches windows.h
 * through retro_miscellaneous.h, so NOMINMAX is set before the first. */
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

#ifdef HAVE_OPENXR
/* Vendored OpenXR loader, built into this TU instead of as a separate
 * CMake artifact.  C++17. */
#include "../deps/openxr/src/common/object_info.cpp"
#include "../deps/openxr/src/common/filesystem_utils.cpp"
#include "../deps/openxr/src/loader/loader_json.cpp"
#if defined(ANDROID)
#include "../deps/openxr/src/loader/android_utilities.cpp"
#endif
#include "../deps/openxr/src/loader/api_layer_interface.cpp"
#include "../deps/openxr/src/loader/loader_core.cpp"
#include "../deps/openxr/src/loader/loader_init_data.cpp"
#include "../deps/openxr/src/loader/loader_instance.cpp"
#include "../deps/openxr/src/loader/loader_logger.cpp"
#include "../deps/openxr/src/loader/loader_logger_recorders.cpp"
#include "../deps/openxr/src/loader/loader_properties.cpp"
#include "../deps/openxr/src/loader/manifest_file.cpp"
#include "../deps/openxr/src/loader/runtime_interface.cpp"
#include "../deps/openxr/src/loader/xr_generated_loader.cpp"
#endif
