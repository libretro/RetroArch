/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#ifndef __RARCH_LOGGER_H
#define __RARCH_LOGGER_H

#include <stdarg.h>

#if defined(RARCH_DUMMY_LOG)
#define LOG_FILE (stderr)
#elif defined(HAVE_FILE_LOGGER) && defined(RARCH_INTERNAL)
#define LOG_FILE (g_extern.log_file)
#else
#define LOG_FILE (stderr)
#endif

#if defined(RARCH_CONSOLE) && defined(HAVE_LOGGER)
#include <logger_override.h>
#elif defined(IOS)
#include "logger/ios_logger_override.h"
#elif defined(_XBOX1)
#include "logger/xdk1_logger_override.h"
#elif defined(IS_SALAMANDER)
#include "logger/salamander_logger_override.h"
#elif defined(ANDROID) && defined(HAVE_LOGGER)
#include "logger/android_logger_override.h"
#else

#if defined(RARCH_DUMMY_LOG) || !defined(RARCH_INTERNAL)
#define RARCH_LOG_VERBOSE (true)
#else
#define RARCH_LOG_VERBOSE g_extern.verbosity
#endif

#ifndef RARCH_LOG
#undef RARCH_LOG_V
#define RARCH_LOG(...) do { \
      if (RARCH_LOG_VERBOSE) \
      { \
         fprintf(LOG_FILE, "RetroArch: " __VA_ARGS__); \
         fflush(LOG_FILE); \
      } \
   } while (0)
#define RARCH_LOG_V(tag, fmt, vp) do { \
      if (RARCH_LOG_VERBOSE) \
      { \
         fprintf(LOG_FILE, "RetroArch: " tag); \
         vfprintf(LOG_FILE, fmt, vp); \
         fflush(LOG_FILE); \
      } \
   } while (0)
#endif

#ifndef RARCH_LOG_OUTPUT
#undef RARCH_LOG_OUTPUT_V
#define RARCH_LOG_OUTPUT(...) do { \
         fprintf(LOG_FILE, __VA_ARGS__); \
         fflush(LOG_FILE); \
   } while (0)
#define RARCH_LOG_OUTPUT_V(tag, fmt, vp) do { \
         fprintf(LOG_FILE, "RetroArch: " tag); \
         vfprintf(LOG_FILE, fmt, vp); \
         fflush(LOG_FILE); \
   } while (0)
#endif

#ifndef RARCH_ERR
#undef RARCH_ERR_V
#define RARCH_ERR(...) do { \
      fprintf(LOG_FILE, "RetroArch [ERROR] :: " __VA_ARGS__); \
      fflush(LOG_FILE); \
   } while (0)
#define RARCH_ERR_V(tag, fmt, vp) do { \
      fprintf(LOG_FILE, "RetroArch [ERROR] :: " tag); \
      vfprintf(LOG_FILE, fmt, vp); \
      fflush(LOG_FILE); \
   } while (0)
#endif

#ifndef RARCH_WARN
#undef RARCH_WARN_V
#define RARCH_WARN(...) do { \
      fprintf(LOG_FILE, "RetroArch [WARN] :: " __VA_ARGS__); \
      fflush(LOG_FILE); \
   } while (0)
#define RARCH_WARN_V(tag, fmt, vp) do { \
      fprintf(LOG_FILE, "RetroArch [WARN] :: " tag); \
      vfprintf(LOG_FILE, fmt, vp); \
      fflush(LOG_FILE); \
   } while (0)
#endif

#endif
#endif

