/* Copyright  (C) 2016-2020 The RetroArch team
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
static struct IGDdatas data = {0};
static struct UPNPUrls urls = {0};
#endif

/*
      natt_open_port_any(ntsd->nat_traversal_state,
            ntsd->port, SOCKET_PROTOCOL_TCP);
*/

void natt_init(struct natt_status *status,
      uint16_t port, enum socket_protocol proto)
{
#if !defined(HAVE_SOCKET_LEGACY) && HAVE_MINIUPNPC
   struct UPNPDev *devlist =
      upnpDiscover(2000, NULL, NULL, 0, 0, 2, NULL);
   struct UPNPDev *dev     = devlist;

   while (dev)
   {
      if (strstr(dev->st, "InternetGatewayDevice"))
      {
         int len;
         char *desc = (char *) miniwget(dev->descURL,
            &len, 0, NULL);

         if (desc)
         {
            memset(&data, 0, sizeof(data));
            FreeUPNPUrls(&urls);

            parserootdesc(desc, len, &data);
            free(desc);

            GetUPNPUrls(&urls, &data, dev->descURL, 0);

            if(natt_open_port_any(status, port, proto))
                break;
         }
      }

      dev = dev->pNext;
   }

   freeUPNPDevlist(devlist);
#endif
}

void natt_deinit(struct natt_status *status,
      enum socket_protocol proto)
{
#if !defined(HAVE_SOCKET_LEGACY) && HAVE_MINIUPNPC
   natt_close_port(status, proto);
   natt_free(status);

   memset(&data, 0, sizeof(data));
   FreeUPNPUrls(&urls);
#endif
}

bool natt_new(struct natt_status *status)
{
   memset(status, 0, sizeof(*status));
   return true;
}

void natt_free(struct natt_status *status)
{
   /* Invalidate the state */
   memset(status, 0, sizeof(*status));
}

static bool natt_open_port(struct natt_status *status,
      struct sockaddr *addr, socklen_t addrlen, enum socket_protocol proto)
{
#if !defined(HAVE_SOCKET_LEGACY) && HAVE_MINIUPNPC
   int r;
   char host[256], ext_host[256],
        port_str[6], ext_port_str[6];
   const char *proto_str = (proto == SOCKET_PROTOCOL_UDP) ? 
      "UDP" : "TCP";
   struct natt_status tmp        = {0};
   struct addrinfo hints         = {0};
   struct addrinfo *ext_addrinfo = NULL;

   /* if NAT traversal is uninitialized or unavailable, oh well */
   if (!status)
      return false;

   if (string_is_empty(urls.controlURL))
      return false;

   /* figure out the internal info */
   if (getnameinfo(addr, addrlen, host, sizeof(host),
         port_str, sizeof(port_str), NI_NUMERICHOST | NI_NUMERICSERV))
      return false;

   /* get the external IP */
   r = UPNP_GetExternalIPAddress(urls.controlURL, data.first.servicetype,
      ext_host);
   if (r)
      return false;

   /* add the port mapping */
   r = UPNP_AddAnyPortMapping(urls.controlURL, data.first.servicetype,
      port_str, port_str, host, "retroarch",
      proto_str, NULL, "0", ext_port_str);
   if (r)
   {
      /* try the older AddPortMapping */
      r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
         port_str, port_str, host, "retroarch",
         proto_str, NULL, "0");
      if (r)
         return false;

      memcpy(ext_port_str, port_str, sizeof(ext_port_str));
   }

   /* update the status */
   if (getaddrinfo_retro(ext_host, ext_port_str,
         &hints, &ext_addrinfo) || !ext_addrinfo)
      goto failure;

   if (ext_addrinfo->ai_family == AF_INET &&
      ext_addrinfo->ai_addrlen >= sizeof(status->ext_inet4_addr))
   {
      status->have_inet4 = true;
      memcpy(&status->ext_inet4_addr, ext_addrinfo->ai_addr,
         sizeof(status->ext_inet4_addr));
   }
#if defined(AF_INET6) && !defined(_3DS)
   else if (ext_addrinfo->ai_family == AF_INET6 &&
      ext_addrinfo->ai_addrlen >= sizeof(status->ext_inet6_addr))
   {
      status->have_inet6 = true;
      memcpy(&status->ext_inet6_addr, ext_addrinfo->ai_addr,
         sizeof(status->ext_inet6_addr));
   }
#endif
   else
   {
      freeaddrinfo_retro(ext_addrinfo);
      goto failure;
   }

   freeaddrinfo_retro(ext_addrinfo);

   return true;

failure:
   tmp.have_inet4 = true;
   tmp.ext_inet4_addr.sin_family = AF_INET;
   sscanf(ext_port_str, "%hu", &tmp.ext_inet4_addr.sin_port);

   natt_close_port(&tmp, proto);
#endif

   return false;
}

bool natt_open_port_any(struct natt_status *status,
      uint16_t port, enum socket_protocol proto)
{
#if !defined(HAVE_SOCKET_LEGACY) && (!defined(SWITCH) || defined(HAVE_LIBNX))
   size_t i;
   struct net_ifinfo list;
   struct addrinfo *addr;
   char port_str[6];
   struct addrinfo hints = {0};

   /* get our interfaces */
   if (!net_ifinfo_new(&list))
      return false;

   /* loop through them */
   snprintf(port_str, sizeof(port_str), "%hu", port);
   for (i = 0; i < list.size; i++)
   {
      struct net_ifinfo_entry *entry = &list.entries[i];

      /* ignore localhost */
      if (string_is_equal(entry->host, "127.0.0.1"))
         continue;

      /* ignore IPv6 for now */
      if (strchr(entry->host, ':'))
         continue;

      addr = NULL;
      if (getaddrinfo_retro(entry->host, port_str, &hints, &addr) ||
            !addr)
         continue;

      /* make a request for this host */
      if (natt_open_port(status, addr->ai_addr, addr->ai_addrlen,
            proto))
      {
         freeaddrinfo_retro(addr);
         net_ifinfo_free(&list);

         return true;
      }

      freeaddrinfo_retro(addr);
   }

   net_ifinfo_free(&list);
#endif

   return false;
}

bool natt_close_port(struct natt_status *status,
      enum socket_protocol proto)
{
#if !defined(HAVE_SOCKET_LEGACY) && HAVE_MINIUPNPC
   const struct sockaddr *addr;
   socklen_t addrlen;
   char port_str[6];
   const char *proto_str = (proto == SOCKET_PROTOCOL_UDP) ? 
      "UDP" : "TCP";

   if (!status)
      return false;

   if (string_is_empty(urls.controlURL))
      return false;

   /* Grab our external port */
   if (status->have_inet4)
   {
      addr    = (struct sockaddr *) &status->ext_inet4_addr;
      addrlen = sizeof(status->ext_inet4_addr);
   }
#if defined(AF_INET6) && !defined(_3DS)
   else if (status->have_inet6)
   {
      addr    = (struct sockaddr *) &status->ext_inet6_addr;
      addrlen = sizeof(status->ext_inet6_addr);
   }
#endif
   else
      return false;

   if (getnameinfo(addr, addrlen, NULL, 0,
         port_str, sizeof(port_str), NI_NUMERICSERV))
      return false;

   /* Request the device to remove our port forwarding. */
   return !UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype,
      port_str, proto_str, NULL);
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
