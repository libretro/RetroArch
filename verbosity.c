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

#include "file_path_special.h"
#include "verbosity.h"

/* If this is non-NULL. RARCH_LOG and friends 
 * will write to this file. */
static FILE *log_file      = NULL;
static bool main_verbosity = false;

void verbosity_enable(void)
{
   main_verbosity = true;
}

void verbosity_disable(void)
{
   main_verbosity = false;
}

bool verbosity_is_enabled(void)
{
   return main_verbosity;
}

bool *verbosity_get_ptr(void)
{
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
void RARCH_LOG_V(const char *tag, const char *fmt, va_list ap)
{
#if TARGET_OS_IPHONE
   static int asl_inited = 0;
#if !TARGET_IPHONE_SIMULATOR
static aslclient asl_client;
#endif
#else
   FILE *fp = NULL;
#endif

   if (!verbosity_is_enabled())
      return;
#if TARGET_OS_IPHONE
#if TARGET_IPHONE_SIMULATOR
   vprintf(fmt, ap);
#else
   if (!asl_inited)
   {
      asl_client = asl_open(file_path_str(FILE_PATH_PROGRAM_NAME), "com.apple.console", ASL_OPT_STDERR | ASL_OPT_NO_DELAY);
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
         file_path_str(FILE_PATH_PROGRAM_NAME),
         tag ? tag : "",
         fmt);
   wvsprintf(buffer, msg_new, ap);
   OutputDebugStringA(buffer);
#elif defined(ANDROID)
   int prio = ANDROID_LOG_INFO;
   if (tag)
   {
      if (string_is_equal(file_path_str(FILE_PATH_LOG_WARN), tag))
         prio = ANDROID_LOG_WARN;
      else if (string_is_equal(file_path_str(FILE_PATH_LOG_ERROR), tag))
         prio = ANDROID_LOG_ERROR;
   }
   __android_log_vprint(prio,
         file_path_str(FILE_PATH_PROGRAM_NAME),
         fmt,
         ap);
#else

#ifdef HAVE_FILE_LOGGER
   fp = retro_main_log_file();
#else
   fp = stderr;
#endif
   fprintf(fp, "%s %s :: ",
         file_path_str(FILE_PATH_PROGRAM_NAME),
         tag ? tag : file_path_str(FILE_PATH_LOG_INFO));
   vfprintf(fp, fmt, ap);
   fflush(fp);
#endif
}

void RARCH_LOG(const char *fmt, ...)
{
   va_list ap;

   if (!verbosity_is_enabled())
      return;

   va_start(ap, fmt);
   RARCH_LOG_V(file_path_str(FILE_PATH_LOG_INFO), fmt, ap);
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
   RARCH_LOG_OUTPUT_V(file_path_str(FILE_PATH_LOG_INFO), msg, ap);
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
   RARCH_WARN_V(file_path_str(FILE_PATH_LOG_WARN), fmt, ap);
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
   RARCH_ERR_V(file_path_str(FILE_PATH_LOG_ERROR), fmt, ap);
   va_end(ap);
}
#endif

