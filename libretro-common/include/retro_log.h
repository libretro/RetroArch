/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifdef _XBOX1
#include <xtl.h>
#endif
#include <stdarg.h>
#include <stdio.h>

#ifdef __MACH__
#include <TargetConditionals.h>
#if TARGET_IPHONE_SIMULATOR
#include <stdio.h>
#else
#include <asl.h>
#endif
#endif

#ifdef ANDROID
#include <android/log.h>
#endif

#include <retro_inline.h>
#include <boolean.h>
#include <compat/posix_string.h>
#include <compat/strl.h>

#if defined(HAVE_FILE_LOGGER) && defined(RARCH_INTERNAL) && !defined(IS_JOYCONFIG)

#ifdef __cplusplus
extern "C"
#endif

FILE *rarch_main_log_file(void);

#define LOG_FILE (rarch_main_log_file())

#else
#define LOG_FILE (stderr)
#endif

#if defined(IS_SALAMANDER)
#define PROGRAM_NAME "RetroArch Salamander"
#elif defined(RARCH_INTERNAL)
#define PROGRAM_NAME "RetroArch"
#elif defined(MARCH_INTERNAL)
#define PROGRAM_NAME "MicroArch"
#else
#define PROGRAM_NAME "N/A"
#endif

#if defined(RARCH_INTERNAL) && !defined(ANDROID)

#ifdef __cplusplus
extern "C" {
#endif
bool rarch_main_verbosity(void);

#ifdef __cplusplus
}
#endif

#define RARCH_LOG_VERBOSE (rarch_main_verbosity())
#else
#define RARCH_LOG_VERBOSE (true)
#endif

#if defined(RARCH_CONSOLE) && defined(HAVE_LOGGER) && defined(RARCH_INTERNAL)
#include <logger_override.h>
#else
static INLINE void RARCH_LOG_V(const char *tag, const char *fmt, va_list ap)
{
   if (!RARCH_LOG_VERBOSE)
      return;
#if TARGET_OS_IPHONE && defined(RARCH_INTERNAL)
#if TARGET_IPHONE_SIMULATOR
   vprintf(fmt, ap);
#else
   aslmsg msg = asl_new(ASL_TYPE_MSG);
   asl_set(msg, ASL_KEY_READ_UID, "-1");
   if (tag)
      asl_log(NULL, msg, ASL_LEVEL_NOTICE, "%s", tag);
   asl_vlog(NULL, msg, ASL_LEVEL_NOTICE, fmt, ap);
   asl_free(msg);
#endif
#elif defined(_XBOX1)
   /* FIXME: Using arbitrary string as fmt argument is unsafe. */
   char msg_new[1024], buffer[1024];
   snprintf(msg_new, sizeof(msg_new), "%s: %s %s",
         PROGRAM_NAME,
         tag ? tag : "",
         fmt);
   wvsprintf(buffer, msg_new, ap);
   OutputDebugStringA(buffer);
#elif defined(ANDROID) && defined(HAVE_LOGGER) && defined(RARCH_INTERNAL)
   int prio = ANDROID_LOG_INFO;
   if (tag)
   {
      if (!strcmp("[WARN]", tag))
         prio = ANDROID_LOG_WARN;
      else if (!strcmp("[ERROR]", tag))
         prio = ANDROID_LOG_ERROR;
   }
   __android_log_vprint(prio, PROGRAM_NAME, fmt, ap);
#else
   fprintf(LOG_FILE, "%s %s :: ", PROGRAM_NAME, tag ? tag : "[INFO]");
   vfprintf(LOG_FILE, fmt, ap); 
#endif
}

static INLINE void RARCH_LOG(const char *fmt, ...)
{
   va_list ap;

   if (!RARCH_LOG_VERBOSE)
      return;

   va_start(ap, fmt);
   RARCH_LOG_V("[INFO]", fmt, ap);
   va_end(ap);
}

static INLINE void RARCH_LOG_OUTPUT_V(const char *tag,
      const char *msg, va_list ap)
{
   RARCH_LOG_V(tag, msg, ap);
}

static INLINE void RARCH_LOG_OUTPUT(const char *msg, ...)
{
   va_list ap;
   va_start(ap, msg);
   RARCH_LOG_OUTPUT_V("[INFO]", msg, ap);
   va_end(ap);
}

static INLINE void RARCH_WARN_V(const char *tag, const char *fmt, va_list ap)
{
   RARCH_LOG_V(tag, fmt, ap);
}

static INLINE void RARCH_WARN(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   RARCH_WARN_V("[WARN]", fmt, ap);
   va_end(ap);
}

static INLINE void RARCH_ERR_V(const char *tag, const char *fmt, va_list ap)
{
   RARCH_LOG_V(tag, fmt, ap);
}

static INLINE void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   RARCH_ERR_V("[ERROR]", fmt, ap);
   va_end(ap);
}
#endif

#endif

