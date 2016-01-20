/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#ifdef _XBOX1
#include <xtl.h>
#endif

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

#include <string/stdstring.h>

#if defined(HAVE_FILE_LOGGER)
#define LOG_FILE (retro_main_log_file())
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

#include "verbosity.h"

/* If this is non-NULL. RARCH_LOG and friends 
 * will write to this file. */
static FILE *log_file;

bool *retro_main_verbosity(void)
{
   static bool main_verbosity;
   return &main_verbosity;
}

FILE *retro_main_log_file(void)
{
   return log_file;
}

void retro_main_log_file_init(const char *path)
{
   log_file     = stderr;
   if (path == NULL)
      return;

   log_file = fopen(path, "wb");
}

void retro_main_log_file_deinit(void)
{
   if (log_file && log_file != stderr)
      fclose(log_file);
   log_file = NULL;
}

#if !defined(HAVE_LOGGER)
static bool RARCH_LOG_VERBOSE(void)
{
   bool *verbose = NULL;
   verbose = retro_main_verbosity();
   if (!verbose)
      return false;
   return *verbose;
}

void RARCH_LOG_V(const char *tag, const char *fmt, va_list ap)
{
#if TARGET_OS_IPHONE
   static int asl_inited = 0;
#if !TARGET_IPHONE_SIMULATOR
static aslclient asl_client;
#endif
#endif

   if (!RARCH_LOG_VERBOSE())
      return;
#if TARGET_OS_IPHONE
#if TARGET_IPHONE_SIMULATOR
   vprintf(fmt, ap);
#else
   if (!asl_inited)
   {
      asl_client = asl_open("RetroArch", "com.apple.console", ASL_OPT_STDERR | ASL_OPT_NO_DELAY);
      asl_inited = 1;
   }
   aslmsg msg = asl_new(ASL_TYPE_MSG);
   asl_set(msg, ASL_KEY_READ_UID, "-1");
   if (tag)
      asl_log(asl_client, msg, ASL_LEVEL_NOTICE, "%s", tag);
   asl_vlog(asl_client, msg, ASL_LEVEL_NOTICE, fmt, ap);
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
#elif defined(ANDROID)
   int prio = ANDROID_LOG_INFO;
   if (tag)
   {
      if (string_is_equal("[WARN]", tag))
         prio = ANDROID_LOG_WARN;
      else if (string_is_equal("[ERROR]", tag))
         prio = ANDROID_LOG_ERROR;
   }
   __android_log_vprint(prio, PROGRAM_NAME, fmt, ap);
#else
   fprintf(LOG_FILE, "%s %s :: ", PROGRAM_NAME, tag ? tag : "[INFO]");
   vfprintf(LOG_FILE, fmt, ap);
   fflush(LOG_FILE);
#endif
}

void RARCH_LOG(const char *fmt, ...)
{
   va_list ap;

   if (!RARCH_LOG_VERBOSE())
      return;

   va_start(ap, fmt);
   RARCH_LOG_V("[INFO]", fmt, ap);
   va_end(ap);
}

void RARCH_LOG_OUTPUT_V(const char *tag, const char *msg, va_list ap)
{
   RARCH_LOG_V(tag, msg, ap);
}

void RARCH_LOG_OUTPUT(const char *msg, ...)
{
   va_list ap;
   va_start(ap, msg);
   RARCH_LOG_OUTPUT_V("[INFO]", msg, ap);
   va_end(ap);
}

void RARCH_WARN_V(const char *tag, const char *fmt, va_list ap)
{
   RARCH_LOG_V(tag, fmt, ap);
}

void RARCH_WARN(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   RARCH_WARN_V("[WARN]", fmt, ap);
   va_end(ap);
}

void RARCH_ERR_V(const char *tag, const char *fmt, va_list ap)
{
   RARCH_LOG_V(tag, fmt, ap);
}

void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   RARCH_ERR_V("[ERROR]", fmt, ap);
   va_end(ap);
}
#endif

