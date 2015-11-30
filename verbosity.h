/*  RetroArch - A frontend for libretro.
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

#ifndef __RARCH_VERBOSITY_H
#define __RARCH_VERBOSITY_H

#include <stdio.h>
#include <stdarg.h>

#include <boolean.h>
#include <compat/posix_string.h>
#include <compat/strl.h>

#ifdef __cplusplus
extern "C" {
#endif

bool *retro_main_verbosity(void);

FILE *retro_main_log_file(void);

void retro_main_log_file_deinit(void);

void retro_main_log_file_init(const char *path);

#if defined(HAVE_LOGGER)

#define BUFSIZE	(64 * 1024)
#define TCPDUMP_STACKSIZE	(16 * 1024)
#define TCPDUMP_PRIO	(2048)

void logger_init (void);
void logger_shutdown (void);
void logger_send (const char *__format,...);
void logger_send_v(const char *__format, va_list args);

#ifdef IS_SALAMANDER

#define RARCH_LOG(...) do { \
   logger_send("RetroArch Salamander: " __VA_ARGS__); \
} while(0)

#define RARCH_LOG_V(tag, fmt, vp) do { \
   logger_send("RetroArch Salamander: " tag); \
   logger_send_v(fmt, vp); \
} while (0)

#define RARCH_LOG_OUTPUT(...) do { \
   logger_send("RetroArch Salamander [OUTPUT] :: " __VA_ARGS__); \
} while(0)

#define RARCH_LOG_OUTPUT_V(tag, fmt, vp) do { \
   logger_send("RetroArch Salamander [OUTPUT] :: " tag); \
   logger_send_v(fmt, vp); \
} while (0)

#define RARCH_ERR(...) do { \
   logger_send("RetroArch Salamander [ERROR] :: " __VA_ARGS__); \
} while(0)

#define RARCH_ERR_V(tag, fmt, vp) do { \
   logger_send("RetroArch Salamander [ERROR] :: " tag); \
   logger_send_v(fmt, vp); \
} while (0)

#define RARCH_WARN(...) do { \
   logger_send("RetroArch Salamander [WARN] :: " __VA_ARGS__); \
} while(0)

#define RARCH_WARN_V(tag, fmt, vp) do { \
   logger_send("RetroArch Salamander [WARN] :: " tag); \
   logger_send_v(fmt, vp); \
} while (0)

#else

#define RARCH_LOG(...) do { \
   logger_send("RetroArch: " __VA_ARGS__); \
} while(0)

#define RARCH_LOG_V(tag, fmt, vp) do { \
   logger_send("RetroArch: " tag); \
   logger_send_v(fmt, vp); \
} while (0)

#define RARCH_ERR(...) do { \
   logger_send("RetroArch [ERROR] :: " __VA_ARGS__); \
} while(0)

#define RARCH_ERR_V(tag, fmt, vp) do { \
   logger_send("RetroArch [ERROR] :: " tag); \
   logger_send_v(fmt, vp); \
} while (0)

#define RARCH_WARN(...) do { \
   logger_send("RetroArch [WARN] :: " __VA_ARGS__); \
} while(0)

#define RARCH_WARN_V(tag, fmt, vp) do { \
   logger_send("RetroArch [WARN] :: " tag); \
   logger_send_v(fmt, vp); \
} while (0)

#define RARCH_LOG_OUTPUT(...) do { \
   logger_send("RetroArch [OUTPUT] :: " __VA_ARGS__); \
} while(0)

#define RARCH_LOG_OUTPUT_V(tag, fmt, vp) do { \
   logger_send("RetroArch [OUTPUT] :: " tag); \
   logger_send_v(fmt, vp); \
} while (0)

#endif
#else
void RARCH_LOG_V(const char *tag, const char *fmt, va_list ap);
void RARCH_LOG(const char *fmt, ...);
void RARCH_LOG_OUTPUT_V(const char *tag, const char *msg, va_list ap);
void RARCH_LOG_OUTPUT(const char *msg, ...);
void RARCH_WARN_V(const char *tag, const char *fmt, va_list ap);
void RARCH_WARN(const char *fmt, ...);
void RARCH_ERR_V(const char *tag, const char *fmt, va_list ap);
void RARCH_ERR(const char *fmt, ...);
#endif


#ifdef __cplusplus
}
#endif

#endif
