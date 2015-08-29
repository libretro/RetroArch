/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#if defined(__CELLOS_LV2__) || defined(__PSL1GHT__)
#include "../../defines/ps3_defines.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <retro_miscellaneous.h>
#include <net/net_compat.h>

#include "logger.h"

#if !defined(PC_DEVELOPMENT_IP_ADDRESS)
#error "An IP address for the PC logging server was not set in the Makefile, cannot continue."
#endif

#if !defined(PC_DEVELOPMENT_UDP_PORT)
#error "An UDP port for the PC logging server was not set in the Makefile, cannot continue."
#endif

static int g_sid;
static int sock;
static struct sockaddr_in target;
static char sendbuf[4096];

void logger_init (void)
{
   if (network_interface_up(&target, 1,
         PC_DEVELOPMENT_IP_ADDRESS,PC_DEVELOPMENT_UDP_PORT, &g_sid) < 0)
   {
      printf("Could not initialize network logger interface.\n");
   }
}

void logger_shutdown (void)
{
   if (network_interface_down(&target, &g_sid) < 0)
   {
      printf("Could not deinitialize network logger interface.\n");
   }
}

void logger_send(const char *__format,...)
{
   va_list args;

   va_start(args,__format);
   logger_send_v(__format, args);
   va_end(args);
}

void logger_send_v(const char *__format, va_list args)
{
   int len;
   vsnprintf(sendbuf,4000,__format, args);
   len = strlen(sendbuf);
   sendto(sock,sendbuf,len,MSG_DONTWAIT,(struct sockaddr*)&target,sizeof(target));
}
