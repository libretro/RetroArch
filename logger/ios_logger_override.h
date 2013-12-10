/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#ifndef __APPLE_IOS_LOGGER_H
#define __APPLE_IOS_LOGGER_H

#include <asl.h>
#include <stdarg.h>

static inline void RARCH_LOG_V(const char *tag, const char *fmt, va_list ap)
{
   aslmsg msg = asl_new(ASL_TYPE_MSG);
   asl_set(msg, ASL_KEY_READ_UID, "-1");
   if (tag)
      asl_log(NULL, msg, ASL_LEVEL_NOTICE, "%s", tag);
   asl_vlog(NULL, msg, ASL_LEVEL_NOTICE, fmt, ap);
   asl_free(msg);
}

static inline void RARCH_LOG(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   RARCH_LOG_V(NULL, fmt, ap);
   va_end(ap);
}

static inline void RARCH_LOG_OUTPUT_V(const char *tag, const char *fmt, va_list ap)
{
   RARCH_LOG_V(tag, fmt, ap);
}

static inline void RARCH_LOG_OUTPUT(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   RARCH_LOG_OUTPUT_V(NULL, fmt, ap);
   va_end(ap);
}

static inline void RARCH_WARN_V(const char *tag, const char *fmt, va_list ap)
{
   RARCH_LOG_V(tag, fmt, ap);
}

static inline void RARCH_WARN(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   RARCH_WARN_V(NULL, fmt, ap);
   va_end(ap);
}

static inline void RARCH_ERR_V(const char *tag, const char *fmt, va_list ap)
{
   RARCH_LOG_V(tag, fmt, ap);
}

static inline void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   RARCH_ERR_V(NULL, fmt, ap);
   va_end(ap);
}

#endif
