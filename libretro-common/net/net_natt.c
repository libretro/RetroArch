/* Copyright  (C) 2016-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_natt.c).
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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <net/net_compat.h>
#include <net/net_ifinfo.h>
#include <retro_miscellaneous.h>

#include <string/stdstring.h>
#include <net/net_natt.h>

#if HAVE_MINIUPNPC
#include <miniupnpc/miniwget.h>
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>

#if MINIUPNPC_API_VERSION < 16
#undef HAVE_MINIUPNPC
#endif
#endif

#if HAVE_MINIUPNPC
static struct UPNPUrls urls;
static struct IGDdatas data;
#endif

void natt_init(void)
{
#ifndef HAVE_SOCKET_LEGACY
#if HAVE_MINIUPNPC
   struct UPNPDev * devlist;
   struct UPNPDev * dev;
   char * descXML;
   int descXMLsize = 0;
   int upnperror = 0;
   memset(&urls, 0, sizeof(struct UPNPUrls));
   memset(&data, 0, sizeof(struct IGDdatas));
   devlist = upnpDiscover(2000, NULL, NULL, 0, 0, 2, &upnperror);
   if (devlist)
   {
      dev = devlist;
      while (dev)
      {
         if (strstr (dev->st, "InternetGatewayDevice"))
            break;
         dev = dev->pNext;
      }
      if (!dev)
         dev = devlist;

      descXML = (char *) miniwget(dev->descURL, &descXMLsize, 0, NULL);
      if (descXML)
      {
         parserootdesc(descXML, descXMLsize, &data);
         free (descXML);
         descXML = 0;
         GetUPNPUrls (&urls, &data, dev->descURL, 0);
      }
      freeUPNPDevlist(devlist);
   }
#endif
#endif
}

bool natt_new(struct natt_status *status)
{
   memset(status, 0, sizeof(struct natt_status));
   return true;
}

void natt_free(struct natt_status *status)
{
   /* Nothing */
}

static bool natt_open_port(struct natt_status *status,
      struct sockaddr *addr, socklen_t addrlen, enum socket_protocol proto)
{
#ifndef HAVE_SOCKET_LEGACY
#if HAVE_MINIUPNPC
   int r;
   char host[PATH_MAX_LENGTH], ext_host[PATH_MAX_LENGTH],
        port_str[6], ext_port_str[6];
   struct addrinfo hints         = {0};
   const char *proto_str         = NULL;
   struct addrinfo *ext_addrinfo = NULL;

   /* if NAT traversal is uninitialized or unavailable, oh well */
   if (!urls.controlURL || !urls.controlURL[0])
      return false;

   /* figure out the internal info */
   if (getnameinfo(addr, addrlen, host, PATH_MAX_LENGTH,
            port_str, 6, NI_NUMERICHOST|NI_NUMERICSERV) != 0)
      return false;

   proto_str = (proto == SOCKET_PROTOCOL_UDP) ? "UDP" : "TCP";

   /* add the port mapping */
   r = UPNP_AddAnyPortMapping(urls.controlURL,
         data.first.servicetype, port_str,
         port_str, host, "retroarch",
         proto_str, NULL, "3600", ext_port_str);

   if (r != 0)
   {
      /* try the older AddPortMapping */
      memcpy(ext_port_str, port_str, 6);
      r = UPNP_AddPortMapping(urls.controlURL,
            data.first.servicetype, port_str,
            port_str, host, "retroarch",
            proto_str, NULL, "3600");
   }
   if (r != 0)
      return false;

   /* get the external IP */
   r = UPNP_GetExternalIPAddress(urls.controlURL,
         data.first.servicetype, ext_host);
   if (r != 0)
      return false;

   /* update the status */
   if (getaddrinfo_retro(ext_host,
            ext_port_str, &hints, &ext_addrinfo) != 0)
      return false;

   if (ext_addrinfo->ai_family == AF_INET &&
       ext_addrinfo->ai_addrlen >= sizeof(struct sockaddr_in))
   {
      status->have_inet4     = true;
      status->ext_inet4_addr = *((struct sockaddr_in *)
            ext_addrinfo->ai_addr);
   }
#if defined(AF_INET6) && !defined(HAVE_SOCKET_LEGACY)
   else if (ext_addrinfo->ai_family == AF_INET6 &&
            ext_addrinfo->ai_addrlen >= sizeof(struct sockaddr_in6))
   {
      status->have_inet6     = true;
      status->ext_inet6_addr = *((struct sockaddr_in6 *)
            ext_addrinfo->ai_addr);
   }
#endif
   else
   {
      freeaddrinfo_retro(ext_addrinfo);
      return false;
   }

   freeaddrinfo_retro(ext_addrinfo);
   return true;

#else
   return false;
#endif
#else
   return false;
#endif
}

bool natt_open_port_any(struct natt_status *status,
      uint16_t port, enum socket_protocol proto)
{
#if !defined(HAVE_SOCKET_LEGACY) && (!defined(SWITCH) || defined(SWITCH) && defined(HAVE_LIBNX))
   size_t i;
   char port_str[6];
   struct net_ifinfo list;
   struct addrinfo hints = {0}, *addr;
   bool ret              = false;

   snprintf(port_str, sizeof(port_str), "%hu", port);

   /* get our interfaces */
   if (!net_ifinfo_new(&list))
      return false;

   /* loop through them */
   for (i = 0; i < list.size; i++)
   {
      struct net_ifinfo_entry *entry = list.entries + i;

      /* ignore localhost */
      if (  string_is_equal(entry->host, "127.0.0.1") ||
            string_is_equal(entry->host, "::1"))
         continue;

      /* make a request for this host */
      if (getaddrinfo_retro(entry->host, port_str, &hints, &addr) == 0)
      {
         ret = natt_open_port(status, addr->ai_addr,
               addr->ai_addrlen, proto) || ret;
         freeaddrinfo_retro(addr);
      }
   }

   net_ifinfo_free(&list);

   return ret;

#else
   return false;
#endif
}

bool natt_read(struct natt_status *status)
{
   /* MiniUPNPC is always synchronous, so there's nothing to read here.
    * Reserved for future backends. */
   return false;
}

#if 0
/* If we want to remove redirects in the future, this is a
 * sample of how to do that. */

void upnp_rem_redir (int port)
{
   int t;
   char port_str[16];

   printf("TB : upnp_rem_redir (%d)\n", port);

   if(urls.controlURL[0] == '\0')
   {
      printf("TB : the init was not done !\n");
      return;
   }

   snprintf(port_str, sizeof(port_str), "%d", port);
   UPNP_DeletePortMapping(urls.controlURL,
         data.first.servicetype, port_str, "TCP", NULL);
}
#endif
