/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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
#include "../defines/ps3_defines.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <retro_miscellaneous.h>
#include <net/net_compat.h>

#include "verbosity.h"

#if !defined(PC_DEVELOPMENT_IP_ADDRESS)
#error "An IP address for the PC logging server was not set in the Makefile, cannot continue."
#endif

#if !defined(PC_DEVELOPMENT_UDP_PORT)
#error "An UDP port for the PC logging server was not set in the Makefile, cannot continue."
#endif

static int g_sid;
static struct sockaddr_in target;
static char sendbuf[4096];
#ifdef VITA
static void *net_memory = NULL;
#define NET_INIT_SIZE 512*1024
#endif

static int network_interface_up(struct sockaddr_in *target, int index,
      const char *ip_address, unsigned udp_port, int *s)
{
   (void)index;

#if defined(VITA)
   if (sceNetShowNetstat() == PSP2_NET_ERROR_ENOTINIT)
   {
      SceNetInitParam initparam;
      net_memory = malloc(NET_INIT_SIZE);

      initparam.memory = net_memory;
      initparam.size = NET_INIT_SIZE;
      initparam.flags = 0;

      sceNetInit(&initparam);
   }
   *s                 = sceNetSocket("RA_netlogger", PSP2_NET_AF_INET, PSP2_NET_SOCK_DGRAM, 0);
   target->sin_family = PSP2_NET_AF_INET;
   target->sin_port   = sceNetHtons(udp_port);

   sceNetInetPton(PSP2_NET_AF_INET, ip_address, &target->sin_addr);
#else

#if defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)
   int ret = 0;

   int state, timeout_count = 10;
   ret = cellNetCtlInit();
   if (ret < 0)
      return -1;

   for (;;)
   {
      ret = cellNetCtlGetState(&state);
      if (ret < 0)
         return -1;

      if (state == CELL_NET_CTL_STATE_IPObtained)
         break;

      retro_sleep(500);
      timeout_count--;
      if (index && timeout_count < 0)
         return 0;
   }
#elif defined(GEKKO)
   char t[16];
   if (if_config(t, NULL, NULL, TRUE) < 0)
      ret = -1;
#endif
   if (ret < 0)
      return -1;

   *s                 = socket(AF_INET, SOCK_DGRAM, 0);

   target->sin_family = AF_INET;
   target->sin_port   = htons(udp_port);
#ifdef GEKKO
   target->sin_len    = 8;
#endif

   inet_pton(AF_INET, ip_address, &target->sin_addr);
#endif
   return 0;
}

static int network_interface_down(struct sockaddr_in *target, int *s)
{
   int ret = 0;
#if defined(_WIN32) && !defined(_XBOX360)
   /* WinSock has headers from the stone age. */
   ret = closesocket(*s);
#elif defined(__CELLOS_LV2__)
   ret = socketclose(*s);
#elif defined(VITA)
   if (net_memory)
      free(net_memory);
   sceNetSocketClose(*s);
#else
   ret = close(*s);
#endif
#if defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)
   cellNetCtlTerm();
#elif defined(GEKKO) && !defined(HW_DOL)
   net_deinit();
#endif
   return ret;
}

void logger_init (void)
{
   if (network_interface_up(&target, 1,
         PC_DEVELOPMENT_IP_ADDRESS,PC_DEVELOPMENT_UDP_PORT, &g_sid) < 0)
      printf("Could not initialize network logger interface.\n");
}

void logger_shutdown (void)
{
   if (network_interface_down(&target, &g_sid) < 0)
      printf("Could not deinitialize network logger interface.\n");
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
   sendto(g_sid,sendbuf,len,MSG_DONTWAIT,(struct sockaddr*)&target,sizeof(target));
}
