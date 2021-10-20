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

#if defined(__PSL1GHT__) || defined(__PS3__)
#include "defines/ps3_defines.h"
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
#include <time.h>

#ifdef _MSC_VER
#include <compat/msvc.h>
#endif

#ifdef ANDROID
#include <android/log.h>
#endif

#if defined(_WIN32)

#if defined(_XBOX)
#include <Xtl.h>
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#endif

#include <file/file_path.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <compat/fopen_utf8.h>
#include <time/rtime.h>
#include <retro_miscellaneous.h>

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

typedef struct verbosity_state
{
#ifdef HAVE_LIBNX
   Mutex mtx;
#endif
   /* If this is non-NULL. RARCH_LOG and friends
    * will write to this file. */
   FILE *fp;
   void *buf;

   char override_path[PATH_MAX_LENGTH];
   bool verbosity;
   bool initialized;
   bool override_active;
} verbosity_state_t;

/* TODO/FIXME - static public global variables */
static verbosity_state_t main_verbosity_st;
static unsigned verbosity_log_level           = 
DEFAULT_FRONTEND_LOG_LEVEL;

#ifdef HAVE_LIBNX
#ifdef NXLINK
/* TODO/FIXME - global referenced in platform_switch.c - not
 * thread-safe */
bool nxlink_connected = false;
#endif /* NXLINK */

#endif /* HAVE_LIBNX */

void verbosity_set_log_level(unsigned level)
{
   verbosity_log_level = level;
}

void verbosity_enable(void)
{
   verbosity_state_t *g_verbosity = &main_verbosity_st;

   g_verbosity->verbosity         = true;
#ifdef RARCH_INTERNAL
   if (!g_verbosity->initialized)
      frontend_driver_attach_console();
#endif
}

void verbosity_disable(void)
{
   verbosity_state_t *g_verbosity = &main_verbosity_st;

   g_verbosity->verbosity         = false;
#ifdef RARCH_INTERNAL
   if (!g_verbosity->initialized)
      frontend_driver_detach_console();
#endif
}

bool verbosity_is_enabled(void)
{
   verbosity_state_t *g_verbosity = &main_verbosity_st;
   return g_verbosity->verbosity;
}

bool is_logging_to_file(void)
{
   verbosity_state_t *g_verbosity = &main_verbosity_st;
   return g_verbosity->initialized;
}

bool *verbosity_get_ptr(void)
{
   verbosity_state_t *g_verbosity = &main_verbosity_st;
   return &g_verbosity->verbosity;
}

void retro_main_log_file_init(const char *path, bool append)
{
   FILE *tmp                      = NULL;
   verbosity_state_t *g_verbosity = &main_verbosity_st;
   if (g_verbosity->initialized)
      return;

#ifdef HAVE_LIBNX
   mutexInit(&g_verbosity->mtx);
#endif

   g_verbosity->fp      = stderr;
   if (!path)
      return;

   tmp                  = (FILE*)fopen_utf8(path, append ? "ab" : "wb");

   if (!tmp)
   {
      RARCH_ERR("Failed to open system event log file: %s\n", path);
      return;
   }

   g_verbosity->fp          = tmp;
   g_verbosity->initialized = true;

   /* TODO: this is only useful for a few platforms, find which and add ifdef */
   g_verbosity->buf         = calloc(1, 0x4000);
   setvbuf(g_verbosity->fp, (char*)g_verbosity->buf, _IOFBF, 0x4000);
}

void retro_main_log_file_deinit(void)
{
   verbosity_state_t *g_verbosity = &main_verbosity_st;

   if (g_verbosity->fp && g_verbosity->initialized)
   {
      fclose(g_verbosity->fp);
      g_verbosity->fp       = NULL;
   }
   if (g_verbosity->buf)
      free(g_verbosity->buf);
   g_verbosity->buf         = NULL;
   g_verbosity->initialized = false;
}

#if !defined(HAVE_LOGGER)
void RARCH_LOG_V(const char *tag, const char *fmt, va_list ap)
{
   verbosity_state_t *g_verbosity = &main_verbosity_st;
   const char              *tag_v = tag ? tag : FILE_PATH_LOG_INFO;

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

   if (g_verbosity->initialized)
   {
      vfprintf(g_verbosity->fp, fmt, ap);
      fflush(g_verbosity->fp);
   }
   else
      __android_log_vprint(prio, FILE_PATH_PROGRAM_NAME, fmt, ap);
#else
   FILE *fp = (FILE*)g_verbosity->fp;
#if defined(HAVE_QT) || defined(__WINRT__)
   char buffer[256];
   buffer[0] = '\0';

   /* Ensure null termination and line break in error case */
   if (vsnprintf(buffer, sizeof(buffer), fmt, ap) < 0)
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
   mutexLock(&g_verbosity->mtx);
#endif
   if (fp)
   {
      fprintf(fp, "%s ", tag_v);
      vfprintf(fp, fmt, ap);
      fflush(fp);
   }
#if defined(HAVE_LIBNX)
   mutexUnlock(&g_verbosity->mtx);
#endif

#endif
#endif
}

void RARCH_LOG_BUFFER(uint8_t *data, size_t size)
{
   unsigned i, offset;
   int padding     = size % 16;
   uint8_t buf[16] = {0};

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

void RARCH_DBG(const char *fmt, ...)
{
   va_list ap;
   verbosity_state_t *g_verbosity = &main_verbosity_st;

   if (!g_verbosity->verbosity)
      return;
   if (verbosity_log_level > 0)
      return;

   va_start(ap, fmt);
   RARCH_LOG_V(FILE_PATH_LOG_DBG, fmt, ap);
   va_end(ap);
}

void RARCH_LOG(const char *fmt, ...)
{
   va_list ap;
   verbosity_state_t *g_verbosity = &main_verbosity_st;

   if (!g_verbosity->verbosity)
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
   verbosity_state_t *g_verbosity = &main_verbosity_st;

   if (!g_verbosity->verbosity)
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
   verbosity_state_t *g_verbosity = &main_verbosity_st;

   if (!g_verbosity->verbosity)
      return;

   va_start(ap, fmt);
   RARCH_ERR_V(FILE_PATH_LOG_ERROR, fmt, ap);
   va_end(ap);
}
#endif

void rarch_log_file_set_override(const char *path)
{
   verbosity_state_t *g_verbosity = &main_verbosity_st;

   g_verbosity->override_active   = true;
   strlcpy(g_verbosity->override_path, path,
         sizeof(g_verbosity->override_path));
}

void rarch_log_file_init(
      bool log_to_file,
      bool log_to_file_timestamp,
      const char *log_dir
      )
{
   char log_directory[PATH_MAX_LENGTH];
   char log_file_path[PATH_MAX_LENGTH];
   verbosity_state_t *g_verbosity            = &main_verbosity_st;
   static bool log_file_created              = false;
   static char timestamped_log_file_name[64] = {0};
   bool logging_to_file                      = g_verbosity->initialized;

   log_directory[0]                          = '\0';
   log_file_path[0]                          = '\0';

   /* If this is the first run, generate a timestamped log
    * file name (do this even when not outputting timestamped
    * log files, since user may decide to switch at any moment...) */
   if (string_is_empty(timestamped_log_file_name))
   {
      char format[256];
      struct tm tm_;
      time_t cur_time = time(NULL);

      rtime_localtime(&cur_time, &tm_);

      format[0] = '\0';
      strftime(format, sizeof(format), "retroarch__%Y_%m_%d__%H_%M_%S", &tm_);
      fill_pathname_noext(timestamped_log_file_name, format,
            ".log",
            sizeof(timestamped_log_file_name));
   }

   /* If nothing has changed, do nothing */
   if ((!log_to_file && !logging_to_file) ||
       (log_to_file && logging_to_file))
      return;

   /* If we are currently logging to file and wish to stop,
    * de-initialise existing logger... */
   if (!log_to_file && logging_to_file)
   {
      retro_main_log_file_deinit();
      /* ...and revert to console */
      retro_main_log_file_init(NULL, false);
      return;
   }

   /* If we reach this point, then we are not currently
    * logging to file, and wish to do so */

   /* > Check whether we are already logging to console */
   /* De-initialise existing logger */
   if (g_verbosity->fp)
      retro_main_log_file_deinit();

   /* > Get directory/file paths */
   if (g_verbosity->override_active)
   {
      /* Get log directory */
      const char *override_path        = g_verbosity->override_path;
      const char *last_slash           = find_last_slash(override_path);

      if (last_slash)
      {
         char tmp_buf[PATH_MAX_LENGTH] = {0};
         size_t path_length            = last_slash + 1 - override_path;

         if ((path_length > 1) && (path_length < PATH_MAX_LENGTH))
            strlcpy(tmp_buf, override_path, path_length * sizeof(char));
         strlcpy(log_directory, tmp_buf, sizeof(log_directory));
      }

      /* Get log file path */
      strlcpy(log_file_path, override_path, sizeof(log_file_path));
   }
   else if (!string_is_empty(log_dir))
   {
      /* Get log directory */
      strlcpy(log_directory, log_dir, sizeof(log_directory));

      /* Get log file path */
      fill_pathname_join(log_file_path,
            log_dir,
            log_to_file_timestamp
            ? timestamped_log_file_name
            : "retroarch.log",
            sizeof(log_file_path));
   }

   /* > Attempt to initialise log file */
   if (!string_is_empty(log_file_path))
   {
      /* Create log directory, if required */
      if (!string_is_empty(log_directory))
      {
         if (!path_is_directory(log_directory))
         {
            if (!path_mkdir(log_directory))
            {
               /* Re-enable console logging and output error message */
               retro_main_log_file_init(NULL, false);
               RARCH_ERR("Failed to create system event log directory: %s\n", log_directory);
               return;
            }
         }
      }

      /* When RetroArch is launched, log file is overwritten.
       * On subsequent calls within the same session, it is appended to. */
      retro_main_log_file_init(log_file_path, log_file_created);
      if (g_verbosity->initialized)
         log_file_created = true;
      return;
   }

   /* If we reach this point, then something went wrong...
    * Just fall back to console logging */
   retro_main_log_file_init(NULL, false);
   RARCH_ERR("Failed to initialise system event file logging...\n");
}

void rarch_log_file_deinit(void)
{
   verbosity_state_t *g_verbosity = &main_verbosity_st;

   /* De-initialise existing logger, if currently logging to file */
   if (g_verbosity->initialized)
      retro_main_log_file_deinit();

   /* If logging is currently disabled... */
   if (!g_verbosity->fp) /* ...initialise logging to console */
      retro_main_log_file_init(NULL, false);
}
