/* Copyright  (C) 2010-2022 The RetroArch team
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <net/net_compat.h>
#include <net/net_socket.h>
#include <retro_timers.h>
#include <compat/strl.h>

#if defined(_XBOX)
struct hostent *gethostbyname(const char *name)
{
   static struct in_addr addr;
   static struct hostent he = {0};
   WSAEVENT event;
   XNDNS          *dns = NULL;
   struct hostent *ret = NULL;

   if (!name)
      return NULL;

   event = WSACreateEvent();

   XNetDnsLookup(name, event, &dns);
   if (!dns)
      goto done;

   WaitForSingleObject((HANDLE)event, INFINITE);

   if (dns->iStatus)
      goto done;

   memcpy(&addr, dns->aina, sizeof(addr));

   he.h_name      = NULL;
   he.h_aliases   = NULL;
   he.h_addrtype  = AF_INET;
   he.h_length    = sizeof(addr);
   he.h_addr_list = &he.h_addr;
   he.h_addr      = (char*)&addr;

   ret = &he;

done:
   WSACloseEvent(event);
   if (dns)
      XNetDnsRelease(dns);

   return ret;
}

#elif defined(VITA)
#define COMPAT_NET_INIT_SIZE 512*1024
#define MAX_NAME 512

typedef uint32_t in_addr_t;

struct in_addr
{
   in_addr_t s_addr;
};

static void *_net_compat_net_memory = NULL;

char *inet_ntoa(struct SceNetInAddr in)
{
   static char ip_addr[16];

   if (!inet_ntop_compat(AF_INET, &in, ip_addr, sizeof(ip_addr)))
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
   static char sname[MAX_NAME]      = {0};
   static struct SceNetInAddr saddr = {0};
   static char *addrlist[2]         = {(char *) &saddr, NULL };
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

#elif defined(_3DS)
#include <malloc.h>
#include <3ds/types.h>
#include <3ds/services/soc.h>

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000

static u32* _net_compat_net_memory = NULL;
#endif

#if defined(_WIN32)
int inet_aton(const char *cp, struct in_addr *inp)
{
	uint32_t addr = 0;
#ifndef _XBOX
	if (cp == 0 || inp == 0)
		return -1;
#endif

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
#if defined(HAVE_SOCKET_LEGACY) || defined(WIIU)
   struct addrinfo default_hints = {0};

   if (!hints)
      hints            = &default_hints;
   if (!hints->ai_family)
      hints->ai_family = AF_INET;

   if (!node)
      node = (hints->ai_flags & AI_PASSIVE) ? "0.0.0.0" : "127.0.0.1";
#endif

#ifdef HAVE_SOCKET_LEGACY
   {
      struct addrinfo    *info = (struct addrinfo*)calloc(1, sizeof(*info));
      struct sockaddr_in *addr = (struct sockaddr_in*)malloc(sizeof(*addr));
      struct hostent     *host = gethostbyname(node);

      if (!info || !addr || !host || !host->h_addr)
      {
         free(addr);
         free(info);

         return -1;
      }

      info->ai_family   = AF_INET;
      info->ai_socktype = hints->ai_socktype;
      info->ai_protocol = hints->ai_protocol;
      info->ai_addrlen  = sizeof(*addr);
      info->ai_addr     = (struct sockaddr*)addr;

      addr->sin_family      = AF_INET;
      if (service)
         addr->sin_port     = inet_htons((uint16_t)strtoul(service, NULL, 10));
#ifdef VITA
      addr->sin_addr.s_addr = inet_addr(host->h_addr);
#else
      memcpy(&addr->sin_addr, host->h_addr, sizeof(addr->sin_addr));
#endif

      *res = info;

      return 0;
   }
#else
   return getaddrinfo(node, service, hints, res);
#endif
}

void freeaddrinfo_retro(struct addrinfo *res)
{
#ifdef HAVE_SOCKET_LEGACY
   if (res)
   {
      free(res->ai_addr);
      free(res);
   }
#else
   freeaddrinfo(res);
#endif
}

#if defined(WIIU)
#include <malloc.h>

static OSThread wiiu_net_cmpt_thread;

static void wiiu_net_cmpt_thread_cleanup(OSThread *thread, void *stack)
{
   free(stack);
}

static int wiiu_net_cmpt_thread_entry(int argc, const char** argv)
{
   const int buf_size = WIIU_RCVBUF + WIIU_SNDBUF;
   void* buf = memalign(128, buf_size);
   if (!buf) return -1;

   somemopt(1, buf, buf_size, 0);

   free(buf);
   return 0;
}
#endif

#if defined(GEKKO)
static char localip[16] = {0};
static char gateway[16] = {0};
static char netmask[16] = {0};
#endif

/**
 * network_init:
 *
 * Platform specific socket library initialization.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool network_init(void)
{
   static bool inited = false;

#if defined(_WIN32)
   WSADATA wsaData;
#elif defined(__PSL1GHT__) || defined(__PS3__)
   int timeout_count = 10;
#elif defined(WIIU)
   void* stack;
#endif

   if (inited)
      return true;

#if defined(_WIN32)
   if (WSAStartup(MAKEWORD(2, 2), &wsaData))
      goto failure;
#elif defined(__PSL1GHT__) || defined(__PS3__)
   sysModuleLoad(SYSMODULE_NET);
   netInitialize();
   if (netCtlInit() < 0)
      goto failure;

   for (;;)
   {
      int state;

      if (netCtlGetState(&state) < 0)
         goto failure;
      if (state == NET_CTL_STATE_IPObtained)
         break;

      if (!(timeout_count--))
         goto failure;

      retro_sleep(500);
   }
#elif defined(VITA)
   if (sceNetShowNetstat() == SCE_NET_ERROR_ENOTINIT)
   {
      SceNetInitParam param;

      _net_compat_net_memory = malloc(COMPAT_NET_INIT_SIZE);
      if (!_net_compat_net_memory)
         goto failure;

      param.memory           = _net_compat_net_memory;
      param.size             = COMPAT_NET_INIT_SIZE;
      param.flags            = 0;
      if (sceNetInit(&param) < 0)
         goto failure;
      if (sceNetCtlInit() < 0)
         goto failure;
   }
#elif defined(GEKKO)
   if (if_config(localip, netmask, gateway, true, 10) < 0)
      goto failure;
#elif defined(WIIU)
   stack = malloc(4096);
   if (!stack)
      goto failure;

   socket_lib_init();

   if (OSCreateThread(&wiiu_net_cmpt_thread, wiiu_net_cmpt_thread_entry,
         0, NULL, stack+4096, 4096, 3,
         OS_THREAD_ATTRIB_AFFINITY_ANY))
   {
      OSSetThreadName(&wiiu_net_cmpt_thread, "Network compat thread");
      OSSetThreadDeallocator(&wiiu_net_cmpt_thread,
         wiiu_net_cmpt_thread_cleanup);
      OSResumeThread(&wiiu_net_cmpt_thread);
   }
   else
   {
      free(stack);
      goto failure;
   }
#elif defined(_3DS)
   _net_compat_net_memory = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
   if (!_net_compat_net_memory)
      goto failure;

   /* WIFI init */
   if (socInit(_net_compat_net_memory, SOC_BUFFERSIZE))
      goto failure;
#else
   signal(SIGPIPE, SIG_IGN); /* Do not like SIGPIPE killing our app. */
#endif

   inited = true;

   return true;

#if defined(_WIN32) || defined(__PSL1GHT__) || defined(__PS3__) || defined(VITA) || defined(GEKKO) || defined(WIIU) || defined(_3DS)
failure:
   network_deinit();

   return false;
#endif
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
#elif defined(__PSL1GHT__) || defined(__PS3__)
   netCtlTerm();
   netFinalizeNetwork();
   sysModuleUnload(SYSMODULE_NET);
#elif defined(VITA)
   sceNetCtlTerm();
   sceNetTerm();

   free(_net_compat_net_memory);
   _net_compat_net_memory = NULL;
#elif defined(GEKKO) && !defined(HW_DOL)
   net_deinit();
#elif defined(_3DS)
   socExit();

   free(_net_compat_net_memory);
   _net_compat_net_memory = NULL;
#endif
}

uint16_t inet_htons(uint16_t hostshort)
{
#if defined(VITA)
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

bool addr_6to4(struct sockaddr_storage *addr)
{
#ifdef HAVE_INET6
   /* ::ffff:a.b.c.d */
   static const uint16_t preffix[] = {0,0,0,0,0,0xffff};
   uint32_t address;
   uint16_t port;
   struct sockaddr_in6 *addr6 = (struct sockaddr_in6*)addr;
   struct sockaddr_in  *addr4 = (struct sockaddr_in*)addr;

   switch (addr->ss_family)
   {
      case AF_INET:
         /* No need to convert. */
         return true;
      case AF_INET6:
         /* Is the address provided an IPv4? */
         if (!memcmp(&addr6->sin6_addr, preffix, sizeof(preffix)))
            break;
      default:
         /* We don't know how to handle this. */
         return false;
   }

   memcpy(&address, ((uint8_t*)&addr6->sin6_addr) + sizeof(preffix),
      sizeof(address));
   port = addr6->sin6_port;

   memset(addr, 0, sizeof(*addr));
   addr4->sin_family = AF_INET;
   addr4->sin_port   = port;
   memcpy(&addr4->sin_addr, &address, sizeof(addr4->sin_addr));
#endif

   return true;
}

struct in_addr6_compat
{
   unsigned char ip_addr[16];
};

#ifdef _XBOX

#ifndef IM_IN6ADDRSZ
#define	IM_IN6ADDRSZ	16
#endif

#ifndef IM_INT16SZ
#define	IM_INT16SZ		2
#endif

#ifndef IM_INADDRSZ
#define	IM_INADDRSZ		4
#endif
/* Taken from https://github.com/skywind3000/easenet/blob/master/inetbase.c
 */

/* convert presentation format to network format */
static const char *
inet_ntop4x(const unsigned char *src, char *dst, size_t size)
{
   char tmp[64];
   size_t len = snprintf(tmp,
         sizeof(tmp),
         "%u.%u.%u.%u", src[0], src[1], src[2], src[3]);

   if (len >= size)
      goto error;

   memcpy(dst, tmp, len + 1);
   return dst;

error:
   errno = ENOSPC;
   return NULL;
}

/* convert presentation format to network format */
static const char *
inet_ntop6x(const unsigned char *src, char *dst, size_t size)
{
   char tmp[64], *tp;
   int i, inc;
   struct { int base, len; } best, cur;
   unsigned int words[IM_IN6ADDRSZ / IM_INT16SZ];

   memset(words, '\0', sizeof(words));
   best.base = best.len = 0;
   cur.base  = cur.len  = 0;

   for (i = 0; i < IM_IN6ADDRSZ; i++)
      words[i / 2] |= (src[i] << ((1 - (i % 2)) << 3));

   best.base = -1;
   cur.base  = -1;

   for (i = 0; i < (IM_IN6ADDRSZ / IM_INT16SZ); i++)
   {
      if (words[i] == 0)
      {
         if (cur.base == -1)
         {
            cur.base = i;
            cur.len  = 1;
         }
         else cur.len++;
      }
      else
      {
         if (cur.base != -1)
         {
            if (best.base == -1 || cur.len > best.len)
               best = cur;
            cur.base = -1;
         }
      }
   }
   if (cur.base != -1)
   {
      if (best.base == -1 || cur.len > best.len)
         best = cur;
   }
   if (best.base != -1 && best.len < 2)
      best.base = -1;

   tp = tmp;
   for (i = 0; i < (IM_IN6ADDRSZ / IM_INT16SZ); i++)
   {
      if (best.base != -1 && i >= best.base &&
            i < (best.base + best.len))
      {
         if (i == best.base)
            *tp++ = ':';
         continue;
      }

      if (i != 0)
         *tp++ = ':';
      if (i == 6 && best.base == 0 &&
            (best.len == 6 || (best.len == 5 && words[5] == 0xffff)))
      {
         if (!inet_ntop4x(src+12, tp, sizeof(tmp) - (tp - tmp)))
            return NULL;
         tp += strlen(tp);
         break;
      }
      inc = sprintf(tp, "%x", words[i]);
      tp += inc;
   }

   if (best.base != -1 && (best.base + best.len) ==
         (IM_IN6ADDRSZ / IM_INT16SZ))
      *tp++ = ':';

   *tp++ = '\0';

   if ((size_t)(tp - tmp) > size)
      goto error;

   memcpy(dst, tmp, tp - tmp);
   return dst;

error:
   errno = ENOSPC;
   return NULL;
}

/* convert network format to presentation format */
/* another inet_ntop, supports AF_INET/AF_INET6 */
static const char *isockaddr_ntop(int af,
      const void *src, char *dst, size_t size)
{
   switch (af)
   {
      case AF_INET:
         return inet_ntop4x((const unsigned char*)src, dst, size);
#ifdef AF_INET6
      case AF_INET6:
         return inet_ntop6x((const unsigned char*)src, dst, size);
#endif
      default:
         if (af == -6)
            return inet_ntop6x((const unsigned char*)src, dst, size);
         errno = EAFNOSUPPORT;
         return NULL;
   }
}
#endif

const char *inet_ntop_compat(int af, const void *src, char *dst, socklen_t cnt)
{
#if defined(VITA)
   return sceNetInetNtop(af,src,dst,cnt);
#elif defined(WIIU)
   return inet_ntop(af, src, dst, cnt);
#elif defined(GEKKO)
   if (af == AF_INET)
   {
      const char *addr_str = inet_ntoa(*(struct in_addr*)src);

      if (addr_str)
      {
         strlcpy(dst, addr_str, cnt);

         return dst;
      }
   }

   return NULL;
#elif defined(_XBOX)
   return isockaddr_ntop(af, src, dst, cnt);
#elif defined(_WIN32)
   if (af == AF_INET)
   {
      struct sockaddr_in in;
      memset(&in, 0, sizeof(in));
      in.sin_family = AF_INET;
      memcpy(&in.sin_addr, src, sizeof(struct in_addr));
      getnameinfo((struct sockaddr *)&in, sizeof(struct
               sockaddr_in), dst, cnt, NULL, 0, NI_NUMERICHOST);
      return dst;
   }
#if defined(AF_INET6) && !defined(HAVE_SOCKET_LEGACY)
   else if (af == AF_INET6)
   {
      struct sockaddr_in6 in;
      memset(&in, 0, sizeof(in));
      in.sin6_family = AF_INET6;
      memcpy(&in.sin6_addr, src, sizeof(struct in_addr6_compat));
      getnameinfo((struct sockaddr *)&in, sizeof(struct
               sockaddr_in6), dst, cnt, NULL, 0, NI_NUMERICHOST);
      return dst;
   }
#endif
   else
      return NULL;
#else
   return inet_ntop(af, src, dst, cnt);
#endif
}

bool udp_send_packet(const char *host,
      uint16_t port, const char *msg)
{
   char port_buf[16]           = {0};
   struct addrinfo hints       = {0};
   struct addrinfo *res        = NULL;
   const struct addrinfo *tmp  = NULL;
   int fd                      = -1;
   bool ret                    = true;

   hints.ai_socktype           = SOCK_DGRAM;

   snprintf(port_buf, sizeof(port_buf), "%hu", (unsigned short)port);

   if (getaddrinfo_retro(host, port_buf, &hints, &res) != 0)
      return false;

   /* Send to all possible targets.
    * "localhost" might resolve to several different IPs. */
   tmp = (const struct addrinfo*)res;
   while (tmp)
   {
      ssize_t len, ret_len;

      fd = socket(tmp->ai_family, tmp->ai_socktype, tmp->ai_protocol);
      if (fd < 0)
      {
         ret = false;
         goto end;
      }

      len     = strlen(msg);
      ret_len = sendto(fd, msg, len, 0, tmp->ai_addr, tmp->ai_addrlen);

      if (ret_len < len)
      {
         ret = false;
         goto end;
      }

      socket_close(fd);
      fd = -1;
      tmp = tmp->ai_next;
   }

end:
   freeaddrinfo_retro(res);
   if (fd >= 0)
      socket_close(fd);
   return ret;
}
