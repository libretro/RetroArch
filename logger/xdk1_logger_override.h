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

#ifndef __MSVC_71_H
#define __MSVC_71_H

#ifdef _XBOX1
#include <xtl.h>
#endif
#include <stdarg.h>

#include "msvc/msvc_compat.h"

static inline void RARCH_LOG(const char *msg, ...)
{
   char msg_new[1024], buffer[1024];
#ifdef IS_SALAMANDER
   snprintf(msg_new, sizeof(msg_new), "RetroArch Salamander: %s", msg);
#else
   snprintf(msg_new, sizeof(msg_new), "RetroArch: %s", msg);
#endif
   va_list ap;
   va_start(ap, msg);
   wvsprintf(buffer, msg_new, ap);
   OutputDebugStringA(buffer);
   va_end(ap);
}

static inline void RARCH_LOG_OUTPUT(const char *msg, ...)
{
   char msg_new[1024], buffer[1024];
#ifdef IS_SALAMANDER
   snprintf(msg_new, sizeof(msg_new), "RetroArch Salamander: %s", msg);
#else
   snprintf(msg_new, sizeof(msg_new), "RetroArch: %s", msg);
#endif
   va_list ap;
   va_start(ap, msg);
   wvsprintf(buffer, msg_new, ap);
   OutputDebugStringA(buffer);
   va_end(ap);
}

static inline void RARCH_WARN(const char *msg, ...)
{
   char msg_new[1024], buffer[1024];
#ifdef IS_SALAMANDER
   snprintf(msg_new, sizeof(msg_new), "RetroArch Salamander [WARN] :: %s", msg);
#else
   snprintf(msg_new, sizeof(msg_new), "RetroArch [WARN] :: %s", msg);
#endif
   va_list ap;
   va_start(ap, msg);
   wvsprintf(buffer, msg_new, ap);
   OutputDebugStringA(buffer);
   va_end(ap);
}

static inline void RARCH_ERR(const char *msg, ...)
{
   char msg_new[1024], buffer[1024];
#ifdef IS_SALAMANDER
   snprintf(msg_new, sizeof(msg_new), "RetroArch Salamander [ERR] :: %s", msg);
#else
   snprintf(msg_new, sizeof(msg_new), "RetroArch [ERR] :: %s", msg);
#endif
   va_list ap;
   va_start(ap, msg);
   wvsprintf(buffer, msg_new, ap);
   OutputDebugStringA(buffer);
   va_end(ap);
}

#endif
