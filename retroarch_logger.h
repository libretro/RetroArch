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

#if defined(RARCH_CONSOLE) && (defined(HAVE_LOGGER) || defined(HAVE_FILE_LOGGER) || defined(_XBOX1))
#include <logger_override.h>
#else

#ifndef RARCH_LOG
#if defined(ANDROID)
#define  RARCH_LOG(...)  __android_log_print(ANDROID_LOG_INFO,"RetroArch: ",__VA_ARGS__)
#elif defined(IS_SALAMANDER)
#define RARCH_LOG(...) do { \
      fprintf(stderr, "RetroArch Salamander: " __VA_ARGS__); \
      fflush(stderr); \
   } while (0)
#else
#define RARCH_LOG(...) do { \
      if (g_extern.verbose) \
      { \
         fprintf(stderr, "RetroArch: " __VA_ARGS__); \
         fflush(stderr); \
      } \
   } while (0)
#endif
#endif

#ifndef RARCH_LOG_OUTPUT
#if defined(ANDROID)
#define  RARCH_LOG_OUTPUT(...)  __android_log_print(ANDROID_LOG_INFO,"stderr: ",__VA_ARGS__)
#elif defined(IS_SALAMANDER)
#define RARCH_LOG_OUTPUT(...) do { \
      fprintf(stderr, "stderr: " __VA_ARGS__); \
      fflush(stderr); \
   } while (0)
#else
#define RARCH_LOG_OUTPUT(...) do { \
      if (g_extern.verbose) \
      { \
         fprintf(stderr, "stderr: " __VA_ARGS__); \
         fflush(stderr); \
      } \
   } while (0)
#endif
#endif

#ifndef RARCH_ERR
#if defined(ANDROID)
#define  RARCH_ERR(...)  __android_log_print(ANDROID_LOG_INFO, "RetroArch [ERROR] :: ",__VA_ARGS__)
#elif defined(IS_SALAMANDER)
#define RARCH_ERR(...) do { \
      fprintf(stderr, "RetroArch Salamander [ERROR] :: " __VA_ARGS__); \
      fflush(stderr); \
   } while (0)
#else
#define RARCH_ERR(...) do { \
      fprintf(stderr, "RetroArch [ERROR] :: " __VA_ARGS__); \
      fflush(stderr); \
   } while (0)
#endif
#endif

#ifndef RARCH_ERR_OUTPUT
#if defined(ANDROID)
#define  RARCH_ERR_OUTPUT(...)  __android_log_print(ANDROID_LOG_INFO, "stderr [ERROR] :: ",__VA_ARGS__)
#elif defined(IS_SALAMANDER)
#define RARCH_ERR_OUTPUT(...) do { \
      fprintf(stderr, "stderr [ERROR] :: " __VA_ARGS__); \
      fflush(stderr); \
   } while (0)
#else
#define RARCH_ERR_OUTPUT(...) do { \
      fprintf(stderr, "stderr [ERROR] :: " __VA_ARGS__); \
      fflush(stderr); \
   } while (0)
#endif
#endif

#ifndef RARCH_WARN
#if defined(ANDROID)
#define  RARCH_WARN(...)  __android_log_print(ANDROID_LOG_INFO, "RetroArch [WARN] :: ",__VA_ARGS__)
#elif defined(IS_SALAMANDER)
#define RARCH_WARN(...) do { \
      fprintf(stderr, "RetroArch Salamander [WARN] :: " __VA_ARGS__); \
      fflush(stderr); \
   } while (0)
#else
#define RARCH_WARN(...) do { \
      fprintf(stderr, "RetroArch [WARN] :: " __VA_ARGS__); \
      fflush(stderr); \
   } while (0)
#endif
#endif

#ifndef RARCH_WARN
#if defined(ANDROID)
#define  RARCH_WARN_OUTPUT(...)  __android_log_print(ANDROID_LOG_INFO, "stderr [WARN] :: ",__VA_ARGS__)
#elif defined(IS_SALAMANDER)
#define RARCH_WARN_OUTPUT(...) do { \
      fprintf(stderr, "stderr [WARN] :: " __VA_ARGS__); \
      fflush(stderr); \
   } while (0)
#else
#define RARCH_WARN_OUTPUT(...) do { \
      fprintf(stderr, "stderr [WARN] :: " __VA_ARGS__); \
      fflush(stderr); \
   } while (0)
#endif
#endif
#endif

#endif
