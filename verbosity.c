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
#include <defines/ps3_defines.h>
#endif

#ifdef __MACH__
#include <TargetConditionals.h>
#include <Availability.h>
#if TARGET_IPHONE_SIMULATOR
#include <stdio.h>
#else
#if __IPHONE_OS_VERSION_MIN_REQUIRED > __IPHONE_10_0 || __TV_OS_VERSION_MIN_REQUIRED > __TVOS_10_0
#include <os/log.h>
#else
#include <asl.h>
#endif
#endif
#endif

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#ifdef _MSC_VER
#include <compat/msvc.h>
#endif
#include <compat/strl.h>

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
#include "file_path_special.h"

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
   /* If this is non-NULL, RARCH_LOG and friends
    * will write to this file. */
   FILE *fp;   /* pointer-sized: keep near top */
   void *buf;  /* pointer-sized: keep near top */

   /* Booleans grouped together to avoid padding holes */
   bool verbosity;
   bool initialized;
   bool override_active;

   /* Large array last: avoids padding before it */
   char override_path[PATH_MAX_LENGTH];
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
   main_verbosity_st.verbosity = true;
#ifdef RARCH_INTERNAL
   if (!main_verbosity_st.initialized)
      frontend_driver_attach_console();
#endif
}

void verbosity_disable(void)
{
   main_verbosity_st.verbosity = false;
#ifdef RARCH_INTERNAL
   if (!main_verbosity_st.initialized)
      frontend_driver_detach_console();
#endif
}

bool verbosity_is_enabled(void)
{
   return main_verbosity_st.verbosity;
}

bool is_logging_to_file(void)
{
   return main_verbosity_st.initialized;
}

bool *verbosity_get_ptr(void)
{
   return &main_verbosity_st.verbosity;
}

void retro_main_log_file_init(const char *path, bool append)
{
   FILE *tmp;

   if (main_verbosity_st.initialized)
      return;

#ifdef HAVE_LIBNX
   mutexInit(&main_verbosity_st.mtx);
#endif

   /* Default to stderr; only override when a valid path is given */
   main_verbosity_st.fp = stderr;

   if (!path)
      return;

   tmp = (FILE*)fopen_utf8(path, append ? "ab" : "wb");

   if (!tmp)
   {
      RARCH_ERR("Failed to open system event log file: \"%s\".\n", path);
      return;
   }

   main_verbosity_st.fp          = tmp;
   main_verbosity_st.initialized = true;

   /* TODO: this is only useful for a few platforms, find which and add ifdef */
   main_verbosity_st.buf = calloc(1, 0x4000);
   setvbuf(main_verbosity_st.fp,
           (char*)main_verbosity_st.buf, _IOFBF, 0x4000);
}

void retro_main_log_file_deinit(void)
{
   if (main_verbosity_st.fp && main_verbosity_st.initialized)
   {
      fclose(main_verbosity_st.fp);
      main_verbosity_st.fp = NULL;
   }
   free(main_verbosity_st.buf);
   main_verbosity_st.buf         = NULL;
   main_verbosity_st.initialized = false;
}

#if !defined(HAVE_LOGGER)
void RARCH_LOG_V(const char *tag, const char *fmt, va_list ap)
{
#if defined(_XBOX1) || defined(__WINRT__)
   char buffer[256];
   int _len;
   const char *tag_v = tag ? tag : FILE_PATH_LOG_INFO;
   buffer[0] = '\0';
   _len = snprintf(buffer, sizeof(buffer),
         "%s: %s ", FILE_PATH_PROGRAM_NAME, tag_v);

   if (_len > 0 && _len < (int)sizeof(buffer))
   {
#if defined(__WINRT__)
      vsnprintf(buffer + _len, sizeof(buffer) - (size_t)_len, fmt, ap);
#else
      wvsprintf(buffer + _len, fmt, ap);
#endif
   }
#ifdef _DEBUG
   OutputDebugStringA(buffer);
#endif
   if (main_verbosity_st.initialized && main_verbosity_st.fp)
   {
      fputs(buffer, main_verbosity_st.fp);
      fflush(main_verbosity_st.fp);
   }

#elif defined(ANDROID)
   {
      FILE *fp = main_verbosity_st.fp;
      if (main_verbosity_st.initialized && fp)
      {
         /* Already logging to file: single write path, no Android log overhead */
         vfprintf(fp, fmt, ap);
         fflush(fp);
      }
      else
      {
         int prio = ANDROID_LOG_INFO;
         if (tag)
         {
            if (memcmp(tag, FILE_PATH_LOG_WARN, sizeof(FILE_PATH_LOG_WARN)) == 0)
               prio = ANDROID_LOG_WARN;
            else if (memcmp(tag, FILE_PATH_LOG_ERROR, sizeof(FILE_PATH_LOG_ERROR)) == 0)
               prio = ANDROID_LOG_ERROR;
         }
         __android_log_vprint(prio, FILE_PATH_PROGRAM_NAME, fmt, ap);
      }
   }

#else
   {
      FILE       *fp    = main_verbosity_st.fp;
      const char *tag_v = tag ? tag : FILE_PATH_LOG_INFO;

#if defined(HAVE_QT) || defined(__WINRT__)
      {
         char buffer[1024];
         int r;
         buffer[0] = '\0';
         r = vsnprintf(buffer, sizeof(buffer), fmt, ap);
         if (r < 0)
         {
            buffer[sizeof(buffer) - 1] = '\0';
            if (buffer[0] != '\0')
               buffer[sizeof(buffer) - 2] = '\n';
            else
            {
               buffer[0] = '\n';
               buffer[1] = '\0';
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
      }

#else

#if TARGET_OS_MAC
      {
         int     r;
         va_list ap_cp;
         char   *buffer = NULL;
         va_copy(ap_cp, ap);
         r = vasprintf(&buffer, fmt, ap_cp);
         va_end(ap_cp);

         if (r < 0 || !buffer)
         {
            free(buffer);
            buffer = (char*)malloc(2);
            if (!buffer)
               goto apple_log_done;
            buffer[0] = '\n';
            buffer[1] = '\0';
         }

#if TARGET_OS_OSX
         printf("%s %s", tag_v, buffer);
         if (fp)
         {
            fprintf(fp, "%s %s", tag_v, buffer);
            fflush(fp);
         }

#else
         {
#if TARGET_OS_SIMULATOR
            fprintf(stderr, "%s %s", tag_v, buffer);
#elif defined(__IPHONE_10_0) && (__IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_10_0)
            os_log(OS_LOG_DEFAULT, "%s %s", tag_v, buffer);
#elif defined(__TV_OS_VERSION_MIN_REQUIRED) && defined(__TVOS_10_0) \
         && (__TV_OS_VERSION_MIN_REQUIRED >= __TVOS_10_0)
            os_log(OS_LOG_DEFAULT, "%s %s", tag_v, buffer);
#else
            {
               static aslclient asl_client      = NULL;
               static int       asl_initialized = 0;
               aslmsg           msg;

#if defined(HAVE_LIBNX)
               mutexLock(&g_verbosity->mtx);
#endif
               if (!asl_initialized)
               {
                  asl_client      = asl_open(FILE_PATH_PROGRAM_NAME,
                                    "com.apple.console",
                                    ASL_OPT_STDERR | ASL_OPT_NO_DELAY);
                  asl_initialized = 1;
               }
#if defined(HAVE_LIBNX)
               mutexUnlock(&g_verbosity->mtx);
#endif
               msg = asl_new(ASL_TYPE_MSG);
               asl_set(msg, ASL_KEY_READ_UID, "-1");
               asl_log(asl_client, msg, ASL_LEVEL_NOTICE,
               "%s %s", tag_v, buffer);
               asl_free(msg);
            }
#endif

            if (fp)
            {
               fprintf(fp, "%s %s", tag_v, buffer);
               fflush(fp);
            }
         }
#endif

         free(buffer);
      }

apple_log_done:;

#else
      {
#  if defined(HAVE_LIBNX)
         mutexLock(&main_verbosity_st.mtx);
#  endif

         if (fp)
         {
            /* Write tag and message in one fprintf call to reduce
             * write() syscall overhead vs. separate fprintf+vfprintf */
            fprintf(fp, "%s ", tag_v);
            vfprintf(fp, fmt, ap);
            fflush(fp);
         }

#  if defined(HAVE_LIBNX)
         mutexUnlock(&main_verbosity_st.mtx);
#  endif
      }
#endif
#endif
   }
#endif
}

void RARCH_LOG_BUFFER(uint8_t *data, size_t len)
{
   size_t i;
   size_t offset     = 0;
   const uint8_t *end = data + len;
   uint8_t buf[16];

   RARCH_LOG("== %d-byte buffer ==================\n", (int)len);

   /* Zero-init once; tail padding written below */
   for (i = 0; i < 16; i++)
      buf[i] = 0;

   for (i = 0; i < len; i++)
   {
      buf[offset++] = data[i];
      if (offset == 16)
      {
         offset = 0;
         RARCH_LOG("%02x%02x%02x%02x%02x%02x%02x%02x  %02x%02x%02x%02x%02x%02x%02x%02x\n",
            buf[0], buf[1], buf[2],  buf[3],  buf[4],  buf[5],  buf[6],  buf[7],
            buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
      }
   }

   /* Flush partial last row (offset > 0 means leftover bytes) */
   if (offset > 0)
   {
      /* Pad remainder with 0xff to match original sentinel */
      for (i = offset; i < 16; i++)
         buf[i] = 0xff;
      RARCH_LOG("%02x%02x%02x%02x%02x%02x%02x%02x  %02x%02x%02x%02x%02x%02x%02x%02x\n",
         buf[0], buf[1], buf[2],  buf[3],  buf[4],  buf[5],  buf[6],  buf[7],
         buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
   }
   RARCH_LOG("==================================\n");
   (void)end; /* suppress unused-variable warning */
}

void RARCH_DBG(const char *fmt, ...)
{
   va_list ap;
#ifndef _DEBUG
   if (!main_verbosity_st.verbosity || verbosity_log_level > 0)
      return;
#endif
   va_start(ap, fmt);
   RARCH_LOG_V(FILE_PATH_LOG_DBG, fmt, ap);
   va_end(ap);
}

void RARCH_LOG(const char *fmt, ...)
{
   va_list ap;
#ifndef _DEBUG
   if (!main_verbosity_st.verbosity || verbosity_log_level > 1)
      return;
#endif
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
#ifndef _DEBUG
   if (!main_verbosity_st.verbosity || verbosity_log_level > 2)
      return;
#endif
   va_start(ap, fmt);
   RARCH_WARN_V(FILE_PATH_LOG_WARN, fmt, ap);
   va_end(ap);
}

void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;
#ifndef _DEBUG
   if (!main_verbosity_st.verbosity)
      return;
#endif
   va_start(ap, fmt);
   RARCH_ERR_V(FILE_PATH_LOG_ERROR, fmt, ap);
   va_end(ap);
}
#endif

size_t rarch_log_file_set_override(const char *path)
{
   main_verbosity_st.override_active = true;
   return strlcpy(main_verbosity_st.override_path, path,
         sizeof(main_verbosity_st.override_path));
}

void rarch_log_file_init(
      bool log_to_file,
      bool log_to_file_timestamp,
      const char *log_dir
      )
{
   char log_directory[DIR_MAX_LENGTH];
   char log_file_path[PATH_MAX_LENGTH];
   static bool log_file_created              = false;
   static char timestamped_log_file_name[64] = {0};
   bool logging_to_file                      = main_verbosity_st.initialized;

   /* If this is the first run, generate a timestamped log
    * file name (do this even when not outputting timestamped
    * log files, since user may decide to switch at any moment...) */
   if (!timestamped_log_file_name[0])
   {
      struct tm tm_;
      time_t cur_time = time(NULL);

      rtime_localtime(&cur_time, &tm_);
#ifdef DJGPP
      strftime(timestamped_log_file_name,
            sizeof(timestamped_log_file_name),
            "RA%d%H%M.log", &tm_);
#else
      strftime(timestamped_log_file_name,
            sizeof(timestamped_log_file_name),
            "retroarch__%Y_%m_%d__%H_%M_%S.log", &tm_);
#endif
   }

   /* If nothing has changed, do nothing */
   if (  (!log_to_file && !logging_to_file)
       || (log_to_file && logging_to_file))
      return;

   /* If we are currently logging to file and wish to stop,
    * deinitialise existing logger... */
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
   if (main_verbosity_st.fp)
      retro_main_log_file_deinit();

   /* Get directory/file paths */
   if (main_verbosity_st.override_active)
   {
      /* Get log directory */
      const char *override_path = main_verbosity_st.override_path;
      const char *last_slash    = find_last_slash(override_path);

      if (last_slash)
      {
         char tmp_buf[PATH_MAX_LENGTH] = {0};
         size_t __len                  = last_slash + 1 - override_path;

         if ((__len > 1) && (__len < PATH_MAX_LENGTH))
            strlcpy(tmp_buf, override_path, __len * sizeof(char));
         strlcpy(log_directory, tmp_buf, sizeof(log_directory));
      }

      /* Get log file path */
      strlcpy(log_file_path, override_path, sizeof(log_file_path));
   }
   else if (log_dir && *log_dir)
   {
      /* Get log directory */
      strlcpy(log_directory, log_dir, sizeof(log_directory));

      /* Get log file path */
      fill_pathname_join_special(log_file_path,
            log_dir,
            log_to_file_timestamp
            ? timestamped_log_file_name
            : FILE_PATH_DEFAULT_EVENT_LOG,
            sizeof(log_file_path));
   }
   else
       log_file_path[0] = '\0';

   /* Attempt to initialise log file */
   if (log_file_path[0])
   {
      /* Create log directory, if required */
      if (     log_directory[0]
            && !path_is_directory(log_directory)
            && !path_mkdir(log_directory))
      {
         /* Re-enable console logging and output error message */
         retro_main_log_file_init(NULL, false);
         RARCH_ERR("Failed to create system event log directory: %s\n", log_directory);
         return;
      }

      /* When RetroArch is launched, log file is overwritten.
       * On subsequent calls within the same session, it is appended to. */
      retro_main_log_file_init(log_file_path, log_file_created);
      if (main_verbosity_st.initialized)
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
   /* De-initialise existing logger, if currently logging to file */
   if (main_verbosity_st.initialized)
      retro_main_log_file_deinit();

   /* If logging is currently disabled... */
   if (!main_verbosity_st.fp) /* ...initialise logging to console */
      retro_main_log_file_init(NULL, false);
}
