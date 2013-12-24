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

#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <stdarg.h>

#define BUFSIZE	(64 * 1024)
#define TCPDUMP_STACKSIZE	(16 * 1024)
#define TCPDUMP_PRIO	(2048)

void logger_init (void);
void logger_shutdown (void);
void logger_send (const char *__format,...);
void logger_send_v(const char *__format, va_list args);

#endif
