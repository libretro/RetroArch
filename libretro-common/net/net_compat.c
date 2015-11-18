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

#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#include <net/net_compat.h>
#include <compat/strl.h>

#if defined(VITA)
static void *_net_compat_net_memory = NULL;
#define COMPAT_NET_INIT_SIZE 512*1024
#define INET_ADDRSTRLEN sizeof(struct sockaddr_in)
#define MAX_NAME 512

typedef uint32_t in_addr_t;

struct in_addr
{
   in_addr_t s_addr;
};

char *inet_ntoa(struct SceNetInAddr in)
{
	static char ip_addr[INET_ADDRSTRLEN + 1];

	if(sceNetInetNtop(AF_INET, &in, ip_addr, INET_ADDRSTRLEN) == NULL)
		strlcpy(ip_addr, "Invalid", sizeof(ip_addr));

	return ip_addr;
}

struct SceNetInAddr inet_aton(const char *ip_addr)
{
   SceNetInAddr inaddr;

   sceNetInetPton(AF_INET, ip_addr, &inaddr);	
   return inaddr;
}

unsigned int inet_addr(const char *cp)
{
   return inet_aton(cp).s_addr;
}

struct hostent *gethostbyname(const char *name)
{
   int err;
   static struct hostent ent;
   static char sname[MAX_NAME] = "";
   static struct SceNetInAddr saddr = { 0 };
   static char *addrlist[2] = { (char *) &saddr, NULL };
   int rid = sceNetResolverCreate("resolver", NULL, 0);

   if(rid < 0)
      return NULL;

   err = sceNetResolverStartNtoa(rid, name, &saddr, 0,0,0);
   sceNetResolverDestroy(rid);
   if(err < 0)
      return NULL;

   addrlist[0]     = inet_ntoa(saddr);
   ent.h_name      = sname;
   ent.h_aliases   = 0;
   ent.h_addrtype  = AF_INET;
   ent.h_length    = sizeof(struct in_addr);
   ent.h_addr_list = addrlist;
   ent.h_addr      = addrlist[0];

   return &ent;
}

int retro_epoll_fd;

#endif

int getaddrinfo_retro(const char *node, const char *service,
      const struct addrinfo *hints,
      struct addrinfo **res)
{
#ifdef HAVE_SOCKET_LEGACY
   struct sockaddr_in *in_addr = NULL;
   struct addrinfo *info = (struct addrinfo*)calloc(1, sizeof(*info));
   if (!info)
      goto error;

   info->ai_family = AF_INET;
   info->ai_socktype = hints->ai_socktype;

   in_addr = (struct sockaddr_in*)calloc(1, sizeof(*in_addr));

   if (!in_addr)
      goto error;

   info->ai_addrlen    = sizeof(*in_addr);
   in_addr->sin_family = AF_INET;
   in_addr->sin_port   = htons(strtoul(service, NULL, 0));

   if (!node && (hints->ai_flags & AI_PASSIVE))
      in_addr->sin_addr.s_addr = INADDR_ANY;
   else if (node && isdigit(*node))
      in_addr->sin_addr.s_addr = inet_addr(node);
   else if (node && !isdigit(*node))
   {
      struct hostent *host = (struct hostent*)gethostbyname(node);

      if (!host || !host->h_addr_list[0])
         goto error;

      in_addr->sin_addr.s_addr = inet_addr(host->h_addr_list[0]);
   }
   else
      goto error;

   info->ai_addr = (struct sockaddr*)in_addr;
   *res = info;

   return 0;

error:
   if (in_addr)
      free(in_addr);
   if (info)
      free(info);
   return -1;
#else
   return getaddrinfo(node, service, hints, res);
#endif
}

void freeaddrinfo_retro(struct addrinfo *res)
{
#ifdef HAVE_SOCKET_LEGACY
   free(res->ai_addr);
   free(res);
#else
   freeaddrinfo(res);
#endif
}

bool socket_nonblock(int fd)
{
#if defined(__CELLOS_LV2__) || defined(VITA)
   int i = 1;
   setsockopt(fd, SOL_SOCKET, SO_NBIO, &i, sizeof(int));
   return true;
#elif defined(_WIN32)
   u_long mode = 1;
   return ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
   return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK) == 0;
#endif
}

int socket_close(int fd)
{ 
#if defined(_WIN32) && !defined(_XBOX360)
   /* WinSock has headers from the stone age. */
   return closesocket(fd);
#elif defined(__CELLOS_LV2__)
   return socketclose(fd);
#elif defined(VITA)
   return sceNetSocketClose(fd);
#else
   return close(fd);
#endif
}

int socket_select(int nfds, fd_set *readfs, fd_set *writefds,
      fd_set *errorfds, struct timeval *timeout)
{
#if defined(__CELLOS_LV2__)
   return socketselect(nfds, readfs, writefds, errorfds, timeout);
#elif defined(VITA)
   SceNetEpollEvent ev = {0};

   ev.events = PSP2_NET_EPOLLIN | PSP2_NET_EPOLLHUP;
   ev.data.fd = nfds;

   if((sceNetEpollControl(retro_epoll_fd, PSP2_NET_EPOLL_CTL_ADD, nfds, &ev)))
   {
      int ret = sceNetEpollWait(retro_epoll_fd, &ev, 1, 0);
      sceNetEpollControl(retro_epoll_fd, PSP2_NET_EPOLL_CTL_DEL, nfds, NULL);
      return ret;
   }
   return 0;
#else
   return select(nfds, readfs, writefds, errorfds, timeout);
#endif
}

int socket_send_all_blocking(int fd, const void *data_, size_t size)
{
   const uint8_t *data = (const uint8_t*)data_;

   while (size)
   {
      ssize_t ret = send(fd, (const char*)data, size, 0);
      if (ret <= 0)
         return false;

      data += ret;
      size -= ret;
   }

   return true;
}

int socket_receive_all_blocking(int fd, void *data_, size_t size)
{
   const uint8_t *data = (const uint8_t*)data_;

   while (size)
   {
      ssize_t ret = recv(fd, (char*)data, size, 0);
      if (ret <= 0)
         return false;

      data += ret;
      size -= ret;
   }

   return true;
}

/**
 * network_init:
 *
 * Platform specific socket library initialization.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool network_init(void)
{
#ifdef _WIN32
   WSADATA wsaData;
#endif
   static bool inited = false;
   if (inited)
      return true;

#if defined(_WIN32)
   if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
   {
      network_deinit();
      return false;
   }
#elif defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)
   cellSysmoduleLoadModule(CELL_SYSMODULE_NET);
   sys_net_initialize_network();
#elif defined(VITA)
   SceNetInitParam initparam;
   /* Init Net */
   if (sceNetShowNetstat() == PSP2_NET_ERROR_ENOTINIT)
   {
      _net_compat_net_memory = malloc(COMPAT_NET_INIT_SIZE);

      initparam.memory = _net_compat_net_memory;
      initparam.size = COMPAT_NET_INIT_SIZE;
      initparam.flags = 0;

      sceNetInit(&initparam);
      //printf("sceNetInit(): 0x%08X\n", ret);

      /* Init NetCtl */
      sceNetCtlInit();
   }
   else
   {
      //printf("Net is already initialized.\n");
   }
   
   retro_epoll_fd = sceNetEpollCreate("epoll", 0);
   //printf("Epoll %x\n",retro_epoll_fd);
#else
   signal(SIGPIPE, SIG_IGN); /* Do not like SIGPIPE killing our app. */
#endif

   inited = true;
   return true;
}

/**
 * network_deinit:
 *
 * Deinitialize platform specific socket libraries.
 **/
void network_deinit(void)
{
#if defined(_WIN32)
   WSACleanup();
#elif defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)
   sys_net_finalize_network();
   cellSysmoduleUnloadModule(CELL_SYSMODULE_NET);
#elif defined(VITA)
   sceNetCtlTerm();
   sceNetTerm();

   if (_net_compat_net_memory)
   {
      free(_net_compat_net_memory);
      _net_compat_net_memory = NULL;
   }
#endif
}
