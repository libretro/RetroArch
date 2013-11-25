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

static void INTERNAL_LOG(const char *fmt, ...)
{
   aslmsg msg = asl_new(ASL_TYPE_MSG);
   asl_set(msg, ASL_KEY_READ_UID, "-1");
   asl_log(NULL, msg, ASL_LEVEL_NOTICE, fmt);
   asl_free(msg);
}

static inline void RARCH_LOG(const char *fmt, ...)
{
   INTERNAL_LOG(fmt);
}

static inline void RARCH_LOG_OUTPUT(const char *msg, ...)
{
   INTERNAL_LOG(msg);
}

static inline void RARCH_WARN(const char *msg, ...)
{
   INTERNAL_LOG(msg);
}

static inline void RARCH_ERR(const char *msg, ...)
{
   INTERNAL_LOG(msg);
}

#endif
