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

#ifndef __RARCH_VERBOSITY_H
#define __RARCH_VERBOSITY_H

#include <stdarg.h>
#include <stdlib.h>

#include <boolean.h>
#include <retro_common_api.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef ORBIS
#include <debugnet.h>
#endif

RETRO_BEGIN_DECLS

#define FILE_PATH_LOG_DBG   "[DEBUG]"
#define FILE_PATH_LOG_INFO  "[INFO]"
#define FILE_PATH_LOG_ERROR "[ERROR]"
#define FILE_PATH_LOG_WARN  "[WARN]"

bool verbosity_is_enabled(void);

void verbosity_enable(void);

void verbosity_disable(void);

void verbosity_set_log_level(unsigned level);

bool *verbosity_get_ptr(void);

void retro_main_log_file_deinit(void);

void retro_main_log_file_init(const char *path, bool append);

bool is_logging_to_file(void);

#if defined(HAVE_LOGGER)

void logger_init (void);
void logger_shutdown (void);
void logger_send (const char *__format,...);
void logger_send_v(const char *__format, va_list args);

#ifdef IS_SALAMANDER

#ifdef ORBIS
#define RARCH_DBG(...) do { \
   debugNetPrintf(DEBUGNET_DEBUG,"" __VA_ARGS__); \
} while (0)

#define RARCH_LOG(...) do { \
   debugNetPrintf(DEBUGNET_INFO,"" __VA_ARGS__); \
} while (0)

#define RARCH_LOG_V(tag, fmt, vp) do { \
   debugNetPrintf(DEBUGNET_DEBUG,tag,fmt,vp); \
} while (0)

#define RARCH_ERR(...) do { \
   debugNetPrintf(DEBUGNET_ERROR,"" __VA_ARGS__); \
} while (0)

#define RARCH_ERR_V(tag, fmt, vp) do { \
   debugNetPrintf(DEBUGNET_ERROR,tag,fmt,vp); \
} while (0)

#define RARCH_WARN(...) do { \
   debugNetPrintf(DEBUGNET_INFO,"" __VA_ARGS__); \
} while (0)

#define RARCH_WARN_V(tag, fmt, vp) do { \
   debugNetPrintf(DEBUGNET_DEBUG,tag,fmt,vp); \
} while (0)

#define RARCH_LOG_OUTPUT(...) do { \
   debugNetPrintf(DEBUGNET_INFO,"" __VA_ARGS__); \
} while (0)

#define RARCH_LOG_OUTPUT_V(tag, fmt, vp) do { \
   debugNetPrintf(DEBUGNET_INFO,tag,fmt,vp); \
} while (0)

#else
#define RARCH_DBG(...) do { \
   logger_send("RetroArch Salamander: " __VA_ARGS__); \
} while (0)

#define RARCH_LOG(...) do { \
   logger_send("RetroArch Salamander: " __VA_ARGS__); \
} while (0)

#define RARCH_LOG_V(tag, fmt, vp) do { \
   logger_send("RetroArch Salamander: " tag); \
   logger_send_v(fmt, vp); \
} while (0)

#define RARCH_LOG_OUTPUT(...) do { \
   logger_send("[OUTPUT] " __VA_ARGS__); \
} while (0)

#define RARCH_LOG_OUTPUT_V(tag, fmt, vp) do { \
   logger_send("[OUTPUT] " tag); \
   logger_send_v(fmt, vp); \
} while (0)

#define RARCH_ERR(...) do { \
   logger_send("[ERROR] " __VA_ARGS__); \
} while (0)

#define RARCH_ERR_V(tag, fmt, vp) do { \
   logger_send("[ERROR] " tag); \
   logger_send_v(fmt, vp); \
} while (0)

#define RARCH_WARN(...) do { \
   logger_send("[WARN] " __VA_ARGS__); \
} while (0)

#define RARCH_WARN_V(tag, fmt, vp) do { \
   logger_send("[WARN] " tag); \
   logger_send_v(fmt, vp); \
} while (0)
#endif
#else /* IS_SALAMANDER */

#ifdef ORBIS
#define RARCH_DBG(...) do { \
   debugNetPrintf(DEBUGNET_DEBUG,"" __VA_ARGS__); \
} while (0)

#define RARCH_LOG(...) do { \
   debugNetPrintf(DEBUGNET_INFO,"" __VA_ARGS__); \
} while (0)

#define RARCH_LOG_V(tag, fmt, vp) do { \
   debugNetPrintf(DEBUGNET_DEBUG,tag,fmt,vp); \
} while (0)

#define RARCH_ERR(...) do { \
   debugNetPrintf(DEBUGNET_ERROR,"" __VA_ARGS__); \
} while (0)

#define RARCH_ERR_V(tag, fmt, vp) do { \
   debugNetPrintf(DEBUGNET_ERROR,tag,fmt,vp); \
} while (0)

#define RARCH_WARN(...) do { \
   debugNetPrintf(DEBUGNET_INFO,"" __VA_ARGS__); \
} while (0)

#define RARCH_WARN_V(tag, fmt, vp) do { \
   debugNetPrintf(DEBUGNET_DEBUG,tag,fmt,vp); \
} while (0)

#define RARCH_LOG_OUTPUT(...) do { \
   debugNetPrintf(DEBUGNET_INFO,"" __VA_ARGS__); \
} while (0)

#define RARCH_LOG_OUTPUT_V(tag, fmt, vp) do { \
   debugNetPrintf(DEBUGNET_INFO,tag,fmt,vp); \
} while (0)

#else
#define RARCH_DBG(...) do { \
   logger_send("" __VA_ARGS__); \
} while (0)

#define RARCH_LOG(...) do { \
   logger_send("" __VA_ARGS__); \
} while (0)

#define RARCH_LOG_V(tag, fmt, vp) do { \
   logger_send("" tag); \
   logger_send_v(fmt, vp); \
} while (0)

#define RARCH_ERR(...) do { \
   logger_send("[ERROR] " __VA_ARGS__); \
} while (0)

#define RARCH_ERR_V(tag, fmt, vp) do { \
   logger_send("[ERROR] " tag); \
   logger_send_v(fmt, vp); \
} while (0)

#define RARCH_WARN(...) do { \
   logger_send("[WARN] " __VA_ARGS__); \
} while (0)

#define RARCH_WARN_V(tag, fmt, vp) do { \
   logger_send("[WARN] :: " tag); \
   logger_send_v(fmt, vp); \
} while (0)

#define RARCH_LOG_OUTPUT(...) do { \
   logger_send("[OUTPUT] " __VA_ARGS__); \
} while (0)

#define RARCH_LOG_OUTPUT_V(tag, fmt, vp) do { \
   logger_send("[OUTPUT] " tag); \
   logger_send_v(fmt, vp); \
} while (0)
#endif
#endif
#define RARCH_LOG_BUFFER(...) do { } while (0)

#else /* HAVE_LOGGER */
void RARCH_LOG_V(const char *tag, const char *fmt, va_list ap);
void RARCH_DBG(const char *fmt, ...);
void RARCH_LOG(const char *fmt, ...);
void RARCH_LOG_BUFFER(uint8_t *buffer, size_t size);
void RARCH_LOG_OUTPUT(const char *msg, ...);
void RARCH_WARN(const char *fmt, ...);
void RARCH_ERR(const char *fmt, ...);

#define RARCH_LOG_OUTPUT_V RARCH_LOG_V
#define RARCH_WARN_V RARCH_LOG_V
#define RARCH_ERR_V RARCH_LOG_V
#endif /* HAVE_LOGGER */

void rarch_log_file_init(
      bool log_to_file,
      bool log_to_file_timestamp,
      const char *log_dir);

void rarch_log_file_deinit(void);

void rarch_log_file_set_override(const char *path);


RETRO_END_DECLS

#endif
