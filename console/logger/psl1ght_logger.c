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

#include <net/net.h>

#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "logger.h"

#if !defined(PC_DEVELOPMENT_IP_ADDRESS)
#error "An IP address for the PC logging server was not set in the Makefile, cannot continue."
#endif

#if !defined(PC_DEVELOPMENT_UDP_PORT)
#error "An UDP port for the PC logging server was not set in the Makefile, cannot continue."
#endif

int s;
struct sockaddr_in server;

#define INITSTRING	"Logging Started\n"
#define BYESTRING	"Logging Stopped\n"

void logger_init (void)
{
   s = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
   memset(&server, 0, sizeof(server));
   server.sin_len = sizeof(server);
   server.sin_family = AF_INET;
   inet_pton(AF_INET, PC_DEVELOPMENT_IP_ADDRESS, &server.sin_addr);
   server.sin_port = htons(PC_DEVELOPMENT_UDP_PORT);

   sendto(s, INITSTRING, strlen(INITSTRING), 0, (struct sockaddr*)&server, sizeof(server));
}

void logger_shutdown (void)
{
   sendto(s, BYESTRING, strlen(BYESTRING), 0, (struct sockaddr*)&server, sizeof(server));
   close(s);
}

void logger_send(const char *format,...)
{
   if(s == -1)
      return;

   char logBuffer[1024];
   int max = sizeof(logBuffer);
   va_list va;
   va_start(va, format);

   int wrote = vsnprintf(logBuffer, max, format, va);

   if(wrote > max)
      wrote = max;

   va_end(va);
   sendto(s, logBuffer, wrote, 0, (struct sockaddr *)&server, sizeof(server));
}
