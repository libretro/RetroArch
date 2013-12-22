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

#if defined(__CELLOS_LV2__)
#include "../../ps3/sdk_defines.h"
#ifndef __PSL1GHT__
#include <netex/net.h>
#include <cell/sysmodule.h>
#include <netex/libnetctl.h>
#include <sys/timer.h>
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#elif defined(GEKKO)
#include <network.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#ifndef GEKKO
#include "../../netplay_compat.h"
#endif

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

#ifdef GEKKO
#define sendto(s, msg, len, flags, addr, tolen) net_sendto(s, msg, len, 0, addr, 8)
#define socket(domain, type, protocol) net_socket(domain, type, protocol)

static int inet_pton(int af, const char *src, void *dst)
{
   if (af != AF_INET)
      return -1;

   return inet_aton (src, dst);
}
#endif

static int if_up_with(int index)
{
   (void)index;
#ifdef __CELLOS_LV2__
   int timeout_count = 10;
   int state;
   int ret;

   ret = cellNetCtlInit();
   if (ret < 0)
   {
      printf("cellNetCtlInit() failed(%x)\n", ret);
      return (-1);
   }

   for (;;)
   {
      ret = cellNetCtlGetState(&state);
      if (ret < 0)
      {
         printf("cellNetCtlGetState() failed(%x)\n", ret);
         return (-1);
      }
      if (state == CELL_NET_CTL_STATE_IPObtained)
         break;

      sys_timer_usleep(500 * 1000);
      timeout_count--;
      if (index && timeout_count < 0)
      {
         printf("if_up_with(%d) timeout\n", index);
         return (0);
      }
   }
#elif defined(GEKKO)
   char t[16];
   if (if_config(t, NULL, NULL, TRUE) < 0)
   {
      return (-1);
   }
#endif

   sock=socket(AF_INET, SOCK_DGRAM, 0);

   target.sin_family = AF_INET;
   target.sin_port = htons(PC_DEVELOPMENT_UDP_PORT);
#ifdef GEKKO
   target.sin_len = 8;
#endif

   inet_pton(AF_INET, PC_DEVELOPMENT_IP_ADDRESS, &target.sin_addr);

   return (0);
}

static int if_down(int sid)
{
   (void)sid;
#ifdef __CELLOS_LV2__
   cellNetCtlTerm();
#elif defined(GEKKO) && !defined(HW_DOL)
   net_deinit();
#endif
   return (0);
}

void logger_init (void)
{
   g_sid = if_up_with(1);
}

void logger_shutdown (void)
{
   if_down(g_sid);
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
   vsnprintf(sendbuf,4000,__format, args);

   int len=strlen(sendbuf);
   sendto(sock,sendbuf,len,MSG_DONTWAIT,(struct sockaddr*)&target,sizeof(target));
}
