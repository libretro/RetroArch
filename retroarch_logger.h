/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#ifdef ANDROID
#include <android/log.h>
#endif

#ifdef HAVE_FILE_LOGGER
extern FILE * log_fp;
#define STDERR_OUT (log_fp)
#else
#define STDERR_OUT (stderr)
#endif

#if defined(RARCH_CONSOLE) && (defined(HAVE_LOGGER) || defined(_XBOX1))
#include <logger_override.h>
#else

#ifndef RARCH_LOG
#if defined(ANDROID)
#define  RARCH_LOG(...)  __android_log_print(ANDROID_LOG_INFO,"RetroArch: ",__VA_ARGS__)
#elif defined(IS_SALAMANDER)
#define RARCH_LOG(...) do { \
      fprintf(STDERR_OUT, "RetroArch Salamander: " __VA_ARGS__); \
      fflush(STDERR_OUT); \
   } while (0)
#else
#define RARCH_LOG(...) do { \
      if (g_extern.verbose) \
      { \
         fprintf(STDERR_OUT, "RetroArch: " __VA_ARGS__); \
         fflush(STDERR_OUT); \
      } \
   } while (0)
#endif
#endif

#ifndef RARCH_LOG_OUTPUT
#if defined(ANDROID)
#define  RARCH_LOG_OUTPUT(...)  __android_log_print(ANDROID_LOG_INFO,"stderr: ",__VA_ARGS__)
#elif defined(IS_SALAMANDER)
#define RARCH_LOG_OUTPUT(...) do { \
      fprintf(STDERR_OUT, "stderr: " __VA_ARGS__); \
      fflush(STDERR_OUT); \
   } while (0)
#else
#define RARCH_LOG_OUTPUT(...) do { \
      if (g_extern.verbose) \
      { \
         fprintf(STDERR_OUT, __VA_ARGS__); \
         fflush(STDERR_OUT); \
      } \
   } while (0)
#endif
#endif

#ifndef RARCH_ERR
#if defined(ANDROID)
#define  RARCH_ERR(...)  __android_log_print(ANDROID_LOG_INFO, "RetroArch [ERROR] :: ",__VA_ARGS__)
#elif defined(IS_SALAMANDER)
#define RARCH_ERR(...) do { \
      fprintf(STDERR_OUT, "RetroArch Salamander [ERROR] :: " __VA_ARGS__); \
      fflush(STDERR_OUT); \
   } while (0)
#else
#define RARCH_ERR(...) do { \
      fprintf(STDERR_OUT, "RetroArch [ERROR] :: " __VA_ARGS__); \
      fflush(STDERR_OUT); \
   } while (0)
#endif
#endif

#ifndef RARCH_ERR_OUTPUT
#if defined(ANDROID)
#define  RARCH_ERR_OUTPUT(...)  __android_log_print(ANDROID_LOG_INFO, "stderr [ERROR] :: ",__VA_ARGS__)
#elif defined(IS_SALAMANDER)
#define RARCH_ERR_OUTPUT(...) do { \
      fprintf(STDERR_OUT, "stderr [ERROR] :: " __VA_ARGS__); \
      fflush(STDERR_OUT); \
   } while (0)
#else
#define RARCH_ERR_OUTPUT(...) do { \
      fprintf(STDERR_OUT, "stderr [ERROR] :: " __VA_ARGS__); \
      fflush(STDERR_OUT); \
   } while (0)
#endif
#endif

#ifndef RARCH_WARN
#if defined(ANDROID)
#define  RARCH_WARN(...)  __android_log_print(ANDROID_LOG_INFO, "RetroArch [WARN] :: ",__VA_ARGS__)
#elif defined(IS_SALAMANDER)
#define RARCH_WARN(...) do { \
      fprintf(STDERR_OUT, "RetroArch Salamander [WARN] :: " __VA_ARGS__); \
      fflush(STDERR_OUT); \
   } while (0)
#else
#define RARCH_WARN(...) do { \
      fprintf(STDERR_OUT, "RetroArch [WARN] :: " __VA_ARGS__); \
      fflush(STDERR_OUT); \
   } while (0)
#endif
#endif

#ifndef RARCH_WARN
#if defined(ANDROID)
#define  RARCH_WARN_OUTPUT(...)  __android_log_print(ANDROID_LOG_INFO, "stderr [WARN] :: ",__VA_ARGS__)
#elif defined(IS_SALAMANDER)
#define RARCH_WARN_OUTPUT(...) do { \
      fprintf(STDERR_OUT, "stderr [WARN] :: " __VA_ARGS__); \
      fflush(STDERR_OUT); \
   } while (0)
#else
#define RARCH_WARN_OUTPUT(...) do { \
      fprintf(STDERR_OUT, "stderr [WARN] :: " __VA_ARGS__); \
      fflush(STDERR_OUT); \
   } while (0)
#endif
#endif
#endif

#endif
