/* Copyright  (C) 2010-2016 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_compat.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <net/net_compat.h>
#include <net/net_socket.h>
#include <compat/strl.h>

#if defined(_XBOX)
/* TODO - implement h_length and h_addrtype */
struct hostent
{
   int h_addrtype;     /* host address type   */
   int h_length;       /* length of addresses */
   char **h_addr_list; /* list of addresses   */
};

struct hostent *gethostbyname(const char *name)
{
   WSAEVENT event;
   static struct hostent he;
   static struct in_addr addr;
   static char *addr_ptr;
   XNDNS *dns     = NULL;

   he.h_addr_list = &addr_ptr;
   addr_ptr       = (char*)&addr;

   if (!name)
      return NULL;

   event = WSACreateEvent();
   XNetDnsLookup(name, event, &dns);
   if (!dns)
      goto error;

   WaitForSingleObject((HANDLE)event, INFINITE);
   if (dns->iStatus)
      goto error;

   memcpy(&addr, dns->aina, sizeof(addr));

   WSACloseEvent(event);
   XNetDnsRelease(dns);

   return &he;

error:
   if (event)
      WSACloseEvent(event);
   return NULL;
}
#elif defined(VITA)
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

   inet_ptrton(AF_INET, ip_addr, &inaddr);
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
#elif defined(_WIN32)
int inet_aton(const char *cp, struct in_addr *inp)
{
	uint32_t addr = 0;
	if (cp == 0 || inp == 0)
		return -1;

	addr = inet_addr(cp);
	if (addr == INADDR_NONE || addr == INADDR_ANY)
		return -1;

	inp->s_addr = addr;
   return 1;
}
#endif

int getaddrinfo_retro(const char *node, const char *service,
      struct addrinfo *hints, struct addrinfo **res)
{
   struct sockaddr_in *in_addr = NULL;
   struct addrinfo *info       = NULL;

   (void)in_addr;
   (void)info;

#if defined(_WIN32) || defined(HAVE_SOCKET_LEGACY)
   hints->ai_family    = AF_INET;
#else
   hints->ai_family    = AF_UNSPEC;
#endif

#ifdef HAVE_SOCKET_LEGACY
   info = (struct addrinfo*)calloc(1, sizeof(*info));
   if (!info)
      goto error;

   info->ai_family     = AF_INET;
   info->ai_socktype   = hints->ai_socktype;
   in_addr             = (struct sockaddr_in*)
      calloc(1, sizeof(*in_addr));

   if (!in_addr)
      goto error;

   info->ai_addrlen    = sizeof(*in_addr);
   in_addr->sin_family = AF_INET;
   in_addr->sin_port   = inet_htons(strtoul(service, NULL, 0));

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
   *res          = info;

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
   int timeout_count = 10;

   cellSysmoduleLoadModule(CELL_SYSMODULE_NET);
   sys_net_initialize_network();

   if (cellNetCtlInit() < 0)
      return false;

   for (;;)
   {
      int state;
      if (cellNetCtlGetState(&state) < 0)
         return false;

      if (state == CELL_NET_CTL_STATE_IPObtained)
         break;

      retro_sleep(500);
      timeout_count--;
      if (timeout_count < 0)
         return 0;
   }
#elif defined(VITA)
   SceNetInitParam initparam;

   if (sceNetShowNetstat() == PSP2_NET_ERROR_ENOTINIT)
   {
      _net_compat_net_memory = malloc(COMPAT_NET_INIT_SIZE);

      initparam.memory       = _net_compat_net_memory;
      initparam.size         = COMPAT_NET_INIT_SIZE;
      initparam.flags        = 0;

      sceNetInit(&initparam);

      sceNetCtlInit();
   }
   
   retro_epoll_fd = sceNetEpollCreate("epoll", 0);
#elif defined(GEKKO)
   char t[16];
   if (if_config(t, NULL, NULL, TRUE) < 0)
      return false;
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
   cellNetCtlTerm();
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
#elif defined(GEKKO) && !defined(HW_DOL)
   net_deinit();
#endif
}

uint16_t inet_htons(uint16_t hostshort)
{
#ifdef VITA
   return sceNetHtons(hostshort);
#else
   return htons(hostshort);
#endif
}

int inet_ptrton(int af, const char *src, void *dst)
{
#if defined(VITA)
   return sceNetInetPton(af, src, dst);	
#elif defined(GEKKO) || defined(_WIN32)
   /* TODO/FIXME - should use InetPton on Vista and later */
   return inet_aton(src, (struct in_addr*)dst);
#else
   return inet_pton(af, src, dst);
#endif
}
