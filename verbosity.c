/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#if defined(_WIN32) && !defined(_XBOX)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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

#include "verbosity.h"

#ifdef HAVE_QT
#include "ui/ui_companion_driver.h"
#endif

#ifdef RARCH_INTERNAL
#include "config.def.h"
#else
#define DEFAULT_FRONTEND_LOG_LEVEL 1
#endif

#if defined(IS_SALAMANDER)
#define FILE_PATH_PROGRAM_NAME "RetroArch Salamander"
#else
#define FILE_PATH_PROGRAM_NAME "RetroArch"
#endif

/* If this is non-NULL. RARCH_LOG and friends
 * will write to this file. */
static FILE *log_file_fp            = NULL;
static void* log_file_buf           = NULL;
static unsigned verbosity_log_level = DEFAULT_FRONTEND_LOG_LEVEL;
static bool main_verbosity          = false;
static bool log_file_initialized    = false;

#ifdef HAVE_LIBNX
static Mutex logging_mtx;
#ifdef NXLINK
bool nxlink_connected = false;
#endif /* NXLINK */
#endif /* HAVE_LIBNX */

void verbosity_set_log_level(unsigned level)
{
   verbosity_log_level = level;
}

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

bool is_logging_to_file(void)
{
   return log_file_initialized;
}

bool *verbosity_get_ptr(void)
{
   return &main_verbosity;
}

void *retro_main_log_file(void)
{
   return log_file_fp;
}

void retro_main_log_file_init(const char *path, bool append)
{
   if (log_file_initialized)
      return;

#ifdef HAVE_LIBNX
   mutexInit(&logging_mtx);
#endif

   log_file_fp          = stderr;
   if (path == NULL)
      return;

   log_file_fp          = (FILE*)fopen_utf8(path, append ? "ab" : "wb");

   if (!log_file_fp)
   {
      log_file_fp       = stderr;
      RARCH_ERR("Failed to open system event log file: %s\n", path);
      return;
   }

   log_file_initialized = true;

#if !defined(PS2) /* TODO: PS2 IMPROVEMENT */
   /* TODO: this is only useful for a few platforms, find which and add ifdef */
   log_file_buf = calloc(1, 0x4000);
   setvbuf(log_file_fp, (char*)log_file_buf, _IOFBF, 0x4000);
#endif
}

void retro_main_log_file_deinit(void)
{
   if (log_file_fp && log_file_initialized)
   {
      fclose(log_file_fp);
      log_file_fp = NULL;
   }
   if (log_file_buf)
      free(log_file_buf);
   log_file_buf = NULL;

   log_file_initialized = false;
}

#if !defined(HAVE_LOGGER)
void RARCH_LOG_V(const char *tag, const char *fmt, va_list ap)
{
   if (verbosity_log_level > 1)
      return;

   {
      const char *tag_v = tag ? tag : FILE_PATH_LOG_INFO;
#if TARGET_OS_IPHONE
#if TARGET_IPHONE_SIMULATOR
      vprintf(fmt, ap);
#else
      static aslclient asl_client;
      static int asl_initialized = 0;
      if (!asl_initialized)
      {
         asl_client      = asl_open(
               FILE_PATH_PROGRAM_NAME,
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
      /* FIXME: Using arbitrary string as fmt argument is unsafe. */
      char msg_new[256];
      char buffer[256];

      msg_new[0] = buffer[0] = '\0';
      snprintf(msg_new, sizeof(msg_new), "%s: %s %s",
            FILE_PATH_PROGRAM_NAME, tag_v, fmt);
      wvsprintf(buffer, msg_new, ap);
      OutputDebugStringA(buffer);
#elif defined(ANDROID)
      int prio = ANDROID_LOG_INFO;
      if (tag)
      {
         if (string_is_equal(FILE_PATH_LOG_WARN, tag))
            prio = ANDROID_LOG_WARN;
         else if (string_is_equal(FILE_PATH_LOG_ERROR, tag))
            prio = ANDROID_LOG_ERROR;
      }

      if (log_file_initialized)
      {
         vfprintf(log_file_fp, fmt, ap);
         fflush(log_file_fp);
      }
      else
         __android_log_vprint(prio, FILE_PATH_PROGRAM_NAME, fmt, ap);
#else
      FILE *fp = (FILE*)log_file_fp;
#if defined(HAVE_QT) || defined(__WINRT__)
      int ret;
      char buffer[256];
      buffer[0] = '\0';
      ret = vsnprintf(buffer, sizeof(buffer), fmt, ap);

      /* ensure null termination and line break in error case */
      if (ret < 0)
      {
         int end;
         buffer[sizeof(buffer) - 1]  = '\0';
         end = strlen(buffer) - 1;
         if (end >= 0)
            buffer[end] = '\n';
         else
         {
            buffer[0]   = '\n';
            buffer[1]   = '\0';
         }
      }

      if (fp)
      {
         fprintf(fp, "%s %s", tag_v, buffer);
         fflush(fp);
      }

#if defined(HAVE_QT)
      ui_companion_driver_log_msg(buffer);
#endif

#if defined(__WINRT__)
      OutputDebugStringA(buffer);
#endif
#else
#if defined(HAVE_LIBNX)
      mutexLock(&logging_mtx);
#endif
      if (fp)
      {
         fprintf(fp, "%s ", tag_v);
         vfprintf(fp, fmt, ap);
         fflush(fp);
      }
#if defined(HAVE_LIBNX)
      mutexUnlock(&logging_mtx);
#endif

#endif
#endif
   }
}

void RARCH_LOG_BUFFER(uint8_t *data, size_t size)
{
   unsigned i, offset;
   int padding     = size % 16;
   uint8_t buf[16] = {0};

   if (verbosity_log_level > 1)
      return;

   RARCH_LOG("== %d-byte buffer ==================\n", (int)size);

   for (i = 0, offset = 0; i < size; i++)
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

   if (padding)
   {
      for (i = padding; i < 16; i++)
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

   if (!main_verbosity)
      return;
   if (verbosity_log_level > 1)
      return;

   va_start(ap, fmt);
   RARCH_LOG_V(FILE_PATH_LOG_INFO, fmt, ap);
   va_end(ap);
}

void RARCH_LOG_OUTPUT(const char *msg, ...)
{
   va_list ap;
   va_start(ap, msg);
   RARCH_LOG_OUTPUT_V(FILE_PATH_LOG_INFO, msg, ap);
   va_end(ap);
}

void RARCH_WARN(const char *fmt, ...)
{
   va_list ap;

   if (!main_verbosity)
      return;
   if (verbosity_log_level > 2)
      return;

   va_start(ap, fmt);
   RARCH_WARN_V(FILE_PATH_LOG_WARN, fmt, ap);
   va_end(ap);
}

void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;

   if (!main_verbosity)
      return;

   va_start(ap, fmt);
   RARCH_ERR_V(FILE_PATH_LOG_ERROR, fmt, ap);
   va_end(ap);
}
#endif
