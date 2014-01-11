/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#ifndef __MSVC_71_H
#define __MSVC_71_H

#ifdef _XBOX1
#include <xtl.h>
#endif
#include <stdarg.h>

#include "msvc/msvc_compat.h"

// FIXME: Using arbitrary string as fmt argument is unsafe.
static inline void RARCH_LOG_V(const char *tag, const char *fmt, va_list ap)
{
   char msg_new[1024], buffer[1024];
#ifdef IS_SALAMANDER
   snprintf(msg_new, sizeof(msg_new), "RetroArch Salamander: %s%s", tag ? tag : "", fmt);
#else
   snprintf(msg_new, sizeof(msg_new), "RetroArch: %s%s", tag ? tag : "", fmt);
#endif
   wvsprintf(buffer, msg_new, ap);
   OutputDebugStringA(buffer);
}

static inline void RARCH_LOG(const char *fmt, ...)
{
   char buffer[1024];
   va_list ap;
   va_start(ap, fmt);
   wvsprintf(buffer, fmt, ap);
   OutputDebugStringA(buffer);
   va_end(ap);
}

static inline void RARCH_LOG_OUTPUT_V(const char *tag, const char *msg, va_list ap)
{
   RARCH_LOG_V(tag, msg, ap);
}

static inline void RARCH_LOG_OUTPUT(const char *msg, ...)
{
   va_list ap;
   va_start(ap, msg);
   RARCH_LOG_V(NULL, msg, ap);
   va_end(ap);
}

static inline void RARCH_WARN_V(const char *tag, const char *fmt, va_list ap)
{
   char msg_new[1024], buffer[1024];
#ifdef IS_SALAMANDER
   snprintf(msg_new, sizeof(msg_new), "RetroArch Salamander [WARN] :: %s%s", tag ? tag : "", fmt);
#else
   snprintf(msg_new, sizeof(msg_new), "RetroArch [WARN] :: %s%s", tag ? tag : "", fmt);
#endif
   wvsprintf(buffer, msg_new, ap);
   OutputDebugStringA(buffer);
}

static inline void RARCH_WARN(const char *fmt, ...)
{
   char buffer[1024];
   va_list ap;
   va_start(ap, fmt);
   wvsprintf(buffer, fmt, ap);
   OutputDebugStringA(buffer);
   va_end(ap);
}

static inline void RARCH_ERR_V(const char *tag, const char *fmt, ...)
{
   char msg_new[1024];
#ifdef IS_SALAMANDER
   snprintf(msg_new, sizeof(msg_new), "RetroArch Salamander [ERR] :: %s%s", tag ? tag : "", fmt);
#else
   snprintf(msg_new, sizeof(msg_new), "RetroArch [ERR] :: %s%s", tag ? tag : "", fmt);
#endif
   OutputDebugStringA(fmt);
}

static inline void RARCH_ERR(const char *fmt, ...)
{
   char buffer[1024];
   va_list ap;
   va_start(ap, fmt);
   wvsprintf(buffer, fmt, ap);
   OutputDebugStringA(buffer);
   va_end(ap);
}

#endif
