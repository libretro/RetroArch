/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <stdio.h>
#include <stdarg.h>

#ifdef _MSC_VER
#include <compat/msvc.h>
#endif

#ifdef ANDROID
#include <android/log.h>
#endif

#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <compat/fopen_utf8.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef RARCH_INTERNAL
#include "frontend/frontend_driver.h"
#endif

#include "file_path_special.h"
#include "verbosity.h"

#ifdef HAVE_QT
#include "ui/ui_companion_driver.h"
#endif

/* If this is non-NULL. RARCH_LOG and friends
 * will write to this file. */
static FILE *log_file_fp         = NULL;
static void* log_file_buf        = NULL;
static bool main_verbosity       = false;
static bool log_file_initialized = false;

void verbosity_enable(void)
{
   main_verbosity = true;
#ifdef RARCH_INTERNAL
   if (!log_file_initialized)
      frontend_driver_attach_console();
#endif
}

void verbosity_disable(void)
{
   main_verbosity = false;
#ifdef RARCH_INTERNAL
   if (!log_file_initialized)
      frontend_driver_detach_console();
#endif
}

bool verbosity_is_enabled(void)
{
   return main_verbosity;
}

bool *verbosity_get_ptr(void)
{
   return &main_verbosity;
}

void *retro_main_log_file(void)
{
   return log_file_fp;
}

void retro_main_log_file_init(const char *path)
{
   if (log_file_initialized)
      return;

   log_file_fp          = stderr;
   if (path == NULL)
      return;

   log_file_fp          = (FILE*)fopen_utf8(path, "wb");
   log_file_initialized = true;

   /* TODO: this is only useful for a few platforms, find which and add ifdef */
   log_file_buf = calloc(1, 0x4000);
   setvbuf(log_file_fp, (char*)log_file_buf, _IOFBF, 0x4000);
}

void retro_main_log_file_deinit(void)
{
   if (log_file_fp && log_file_fp != stderr)
      fclose(log_file_fp);
   if (log_file_buf) free(log_file_buf);
   log_file_fp = NULL;
   log_file_buf = NULL;
}

#if !defined(HAVE_LOGGER)
void RARCH_LOG_V(const char *tag, const char *fmt, va_list ap)
{
#if TARGET_OS_IPHONE
#if TARGET_IPHONE_SIMULATOR
   vprintf(fmt, ap);
#else
   static aslclient asl_client;
   static int asl_initialized = 0;
   if (!asl_initialized)
   {
      asl_client      = asl_open(
            file_path_str(FILE_PATH_PROGRAM_NAME),
            "com.apple.console",
            ASL_OPT_STDERR | ASL_OPT_NO_DELAY);
      asl_initialized = 1;
   }
   aslmsg msg = asl_new(ASL_TYPE_MSG);
   asl_set(msg, ASL_KEY_READ_UID, "-1");
   if (tag)
      asl_log(asl_client, msg, ASL_LEVEL_NOTICE, "%s", tag);
   asl_vlog(asl_client, msg, ASL_LEVEL_NOTICE, fmt, ap);
   asl_free(msg);
#endif
#elif defined(_XBOX1)
   {
      /* FIXME: Using arbitrary string as fmt argument is unsafe. */
      char msg_new[1024];
      char buffer[1024];

      msg_new[0] = buffer[0] = '\0';
      snprintf(msg_new, sizeof(msg_new), "%s: %s %s",
            file_path_str(FILE_PATH_PROGRAM_NAME),
            tag ? tag : "",
            fmt);
      wvsprintf(buffer, msg_new, ap);
      OutputDebugStringA(buffer);
   }
#elif defined(ANDROID)
   {
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
   }
#else

   {
#ifdef HAVE_QT
      char buffer[1024];
#endif
#ifdef HAVE_FILE_LOGGER
      FILE *fp = (FILE*)retro_main_log_file();
#else
      FILE *fp = stderr;
#endif

#ifdef HAVE_QT
      buffer[0] = '\0';
      vsnprintf(buffer, sizeof(buffer), fmt, ap);

      if (fp)
      {
         fprintf(fp, "%s %s", tag ? tag : file_path_str(FILE_PATH_LOG_INFO), buffer);
         fflush(fp);
      }

      ui_companion_driver_log_msg(buffer);
#else
      if (fp)
      {
         fprintf(fp, "%s ",
               tag ? tag : file_path_str(FILE_PATH_LOG_INFO));
         vfprintf(fp, fmt, ap);
         fflush(fp);
      }
#endif
   }
#endif
}

void RARCH_LOG_BUFFER(uint8_t *data, size_t size)
{
   unsigned i, offset;
   int padding = size % 16;
   uint8_t buf[16];

   RARCH_LOG("== %d-byte buffer ==================\n", size);

   for(i = 0, offset = 0; i < size; i++)
   {
      buf[offset] = data[i];
      offset++;

      if (offset == 16)
      {
         offset = 0;
         RARCH_LOG("%02x%02x%02x%02x%02x%02x%02x%02x  %02x%02x%02x%02x%02x%02x%02x%02x\n",
            buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7],
            buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
      }
   }
   if(padding)
   {
      for(i = padding; i < 16; i++)
         buf[i] = 0xff;
      RARCH_LOG("%02x%02x%02x%02x%02x%02x%02x%02x  %02x%02x%02x%02x%02x%02x%02x%02x\n",
         buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7],
         buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
   }
   RARCH_LOG("==================================\n");
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

void RARCH_LOG_OUTPUT(const char *msg, ...)
{
   va_list ap;
   va_start(ap, msg);
   RARCH_LOG_OUTPUT_V(file_path_str(FILE_PATH_LOG_INFO), msg, ap);
   va_end(ap);
}

void RARCH_WARN(const char *fmt, ...)
{
   va_list ap;

   if (!verbosity_is_enabled())
      return;

   va_start(ap, fmt);
   RARCH_WARN_V(file_path_str(FILE_PATH_LOG_WARN), fmt, ap);
   va_end(ap);
}

void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;

   if (!verbosity_is_enabled())
      return;

   va_start(ap, fmt);
   RARCH_ERR_V(file_path_str(FILE_PATH_LOG_ERROR), fmt, ap);
   va_end(ap);
}
#endif

