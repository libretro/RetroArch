/* Copyright  (C) 2010-2022 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_ifinfo.c).
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

#include <stdlib.h>
#include <stdio.h>

#include <string/stdstring.h>
#include <net/net_compat.h>

#if defined(_WIN32) && !defined(_XBOX)
#ifdef _MSC_VER
#pragma comment(lib, "Iphlpapi")
#endif

#include <iphlpapi.h>

#elif !defined(VITA) && !defined(GEKKO)
#if defined(WANT_IFADDRS)
#include <compat/ifaddrs.h>
#elif !defined(HAVE_LIBNX) && !defined(_3DS)
#include <ifaddrs.h>
#ifndef WIIU
#include <net/if.h>
#endif
#endif
#endif

#include <net/net_ifinfo.h>

bool net_ifinfo_new(net_ifinfo_t *list)
{
#if defined(_WIN32) && !defined(_XBOX)
   /* Microsoft docs recommend doing it this way. */
   char buf[512];
   ULONG result;
   PIP_ADAPTER_ADDRESSES addr;
   struct net_ifinfo_entry *entry;
   size_t                interfaces = 0;
   ULONG                 flags      = GAA_FLAG_SKIP_ANYCAST
                                    | GAA_FLAG_SKIP_MULTICAST 
                                    | GAA_FLAG_SKIP_DNS_SERVER;
   ULONG                 len        = 15 * 1024;
   PIP_ADAPTER_ADDRESSES addresses  = (PIP_ADAPTER_ADDRESSES)calloc(1, len);

   list->entries                    = NULL;

   if (!addresses)
      goto failure;

   result = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, addresses, &len);
   if (result == ERROR_BUFFER_OVERFLOW)
   {
      PIP_ADAPTER_ADDRESSES new_addresses =
         (PIP_ADAPTER_ADDRESSES)realloc(addresses, len);

      if (new_addresses)
      {
         memset(new_addresses, 0, len);

         addresses = new_addresses;
         result    = GetAdaptersAddresses(AF_UNSPEC, flags, NULL,
            addresses, &len);
      }
   }
   if (result != ERROR_SUCCESS)
      goto failure;

   /* Count the number of valid interfaces first. */
   addr = addresses;

   do
   {
      PIP_ADAPTER_UNICAST_ADDRESS unicast_addr = addr->FirstUnicastAddress;

      if (!unicast_addr)
         continue;
      if (addr->OperStatus != IfOperStatusUp)
         continue;

      do
      {
         interfaces++;
      } while ((unicast_addr = unicast_addr->Next));
   } while ((addr = addr->Next));

   if (!interfaces)
      goto failure;

   if (!(list->entries =
      (struct net_ifinfo_entry*)calloc(interfaces, sizeof(*list->entries))))
      goto failure;

   list->size    = 0;
   /* Now create the entries. */
   addr          = addresses;
   entry         = list->entries;

   do
   {
      PIP_ADAPTER_UNICAST_ADDRESS unicast_addr = addr->FirstUnicastAddress;

      if (!unicast_addr)
         continue;
      if (addr->OperStatus != IfOperStatusUp)
         continue;

      buf[0] = '\0';
      if (addr->FriendlyName)
      {
         if (!WideCharToMultiByte(CP_UTF8, 0, addr->FriendlyName, -1,
               buf, sizeof(buf), NULL, NULL))
            buf[0] = '\0'; /* Empty name on conversion failure. */
      }

      do
      {
         if (getnameinfo_retro(unicast_addr->Address.lpSockaddr,
               unicast_addr->Address.iSockaddrLength,
               entry->host, sizeof(entry->host), NULL, 0, NI_NUMERICHOST))
            continue;

         strlcpy(entry->name, buf, sizeof(entry->name));

         if (++list->size >= interfaces)
            break;

         entry++;
      } while ((unicast_addr = unicast_addr->Next));

      if (list->size >= interfaces)
         break;
   } while ((addr = addr->Next));

   free(addresses);

   return true;

failure:
   free(addresses);
   net_ifinfo_free(list);

   return false;
#elif defined(VITA)
   SceNetCtlInfo info;
   if (!(list->entries = (struct net_ifinfo_entry*)calloc(2, sizeof(*list->entries))))
   {
      list->size = 0;
      return false;
   }

   strlcpy(list->entries[0].name, "lo",        sizeof(list->entries[0].name));
   strlcpy(list->entries[0].host, "127.0.0.1", sizeof(list->entries[0].host));
   list->size = 1;

   if (!sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS, &info))
   {
      strlcpy(list->entries[1].name, "wlan", sizeof(list->entries[1].name));
      strlcpy(list->entries[1].host, info.ip_address,
         sizeof(list->entries[1].host));
      list->size++;
   }

   return true;
#elif defined(HAVE_LIBNX) || defined(_3DS) || defined(GEKKO)
   uint32_t addr = 0;
   if (!(list->entries = (struct net_ifinfo_entry*)calloc(2, sizeof(*list->entries))))
   {
      list->size = 0;
      return false;
   }

   strlcpy(list->entries[0].name, "lo", sizeof(list->entries[0].name));
   strlcpy(list->entries[0].host, "127.0.0.1", sizeof(list->entries[0].host));
   list->size = 1;

#if defined(HAVE_LIBNX)
   {
      Result rc = nifmGetCurrentIpAddress(&addr);

      if (!R_SUCCEEDED(rc))
         return true;
   }
#elif defined(_3DS)
   addr = gethostid();
#else
   addr = net_gethostip();
#endif
   if (addr)
   {
      uint8_t *addr8 = (uint8_t*)&addr;
      strlcpy(list->entries[1].name,
#if defined(HAVE_LIBNX)
         "switch"
#elif defined(_3DS)
         "wlan"
#else
         "gekko"
#endif
      , sizeof(list->entries[1].name));
      snprintf(list->entries[1].host, sizeof(list->entries[1].host),
         "%d.%d.%d.%d",
         (int)addr8[0], (int)addr8[1], (int)addr8[2], (int)addr8[3]);
      list->size++;
   }

   return true;
#else
   struct ifaddrs *addr;
   struct net_ifinfo_entry *entry;
   size_t         interfaces = 0;
   struct ifaddrs *addresses = NULL;

   list->entries             = NULL;

   if (getifaddrs(&addresses) || !addresses)
      goto failure;

   /* Count the number of valid interfaces first. */
   addr                      = addresses;

   do
   {
      if (!addr->ifa_addr)
         continue;
#ifndef WIIU
      if (!(addr->ifa_flags & IFF_UP))
         continue;
#endif

      switch (addr->ifa_addr->sa_family)
      {
         case AF_INET:
#ifdef HAVE_INET6
         case AF_INET6:
#endif
            interfaces++;
            break;
         default:
            break;
      }
   } while ((addr = addr->ifa_next));

   if (!interfaces)
      goto failure;

   list->entries =
      (struct net_ifinfo_entry*)calloc(interfaces, sizeof(*list->entries));
   if (!list->entries)
      goto failure;
   list->size    = 0;

   /* Now create the entries. */
   addr  = addresses;
   entry = list->entries;

   do
   {
      socklen_t addrlen;

      if (!addr->ifa_addr)
         continue;
#ifndef WIIU
      if (!(addr->ifa_flags & IFF_UP))
         continue;
#endif

      switch (addr->ifa_addr->sa_family)
      {
         case AF_INET:
            addrlen = sizeof(struct sockaddr_in);
            break;
#ifdef HAVE_INET6
         case AF_INET6:
            addrlen = sizeof(struct sockaddr_in6);
            break;
#endif
         default:
            continue;
      }

      if (getnameinfo_retro(addr->ifa_addr, addrlen,
            entry->host, sizeof(entry->host), NULL, 0, NI_NUMERICHOST))
         continue;

      if (addr->ifa_name)
         strlcpy(entry->name, addr->ifa_name, sizeof(entry->name));

      if (++list->size >= interfaces)
         break;

      entry++;
   } while ((addr = addr->ifa_next));

   freeifaddrs(addresses);

   return true;

failure:
   freeifaddrs(addresses);
   net_ifinfo_free(list);

   return false;
#endif
}

void net_ifinfo_free(net_ifinfo_t *list)
{
   free(list->entries);

   list->entries = NULL;
   list->size    = 0;
}

bool net_ifinfo_best(const char *dst, void *src, bool ipv6)
{
   bool ret = false;

/* TODO/FIXME: Implement for other platforms, if necessary. */
#if defined(_WIN32) && !defined(_XBOX)
   if (!ipv6)
   {
      /* Courtesy of MiniUPnP: https://github.com/miniupnp/miniupnp */
      DWORD index;
#ifdef __WINRT__
      struct sockaddr_in dst_addr = {0};
#endif
      ULONG dst_ip               = (ULONG)inet_addr(dst);

      if (!src)
         return false;
      if (dst_ip == INADDR_NONE || dst_ip == INADDR_ANY)
         return false;

#ifdef __WINRT__
      dst_addr.sin_family      = AF_INET;
      dst_addr.sin_addr.s_addr = dst_ip;
      if (GetBestInterfaceEx((struct sockaddr*)&dst_addr, &index) == NO_ERROR)
#else
      if (GetBestInterface(dst_ip, &index) == NO_ERROR)
#endif
      {
         /* Microsoft docs recommend doing it this way. */
         ULONG                 len       = 15 * 1024;
         PIP_ADAPTER_ADDRESSES addresses =
            (PIP_ADAPTER_ADDRESSES)calloc(1, len);

         if (addresses)
         {
            ULONG flags  = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
               GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_FRIENDLY_NAME;
            ULONG result = GetAdaptersAddresses(AF_INET, flags, NULL,
               addresses, &len);

            if (result == ERROR_BUFFER_OVERFLOW)
            {
               PIP_ADAPTER_ADDRESSES new_addresses =
                  (PIP_ADAPTER_ADDRESSES)realloc(addresses, len);

               if (new_addresses)
               {
                  memset(new_addresses, 0, len);

                  addresses = new_addresses;
                  result    = GetAdaptersAddresses(AF_INET, flags, NULL,
                     addresses, &len);
               }
            }

            if (result == NO_ERROR)
            {
               PIP_ADAPTER_ADDRESSES addr = addresses;

               do
               {
                  if (addr->IfIndex == index)
                  {
                     if (addr->FirstUnicastAddress)
                     {
                        struct sockaddr_in *addr_unicast =
                           (struct sockaddr_in*)
                              addr->FirstUnicastAddress->Address.lpSockaddr;

                        memcpy(src, &addr_unicast->sin_addr,
                           sizeof(addr_unicast->sin_addr));

                        ret = true;
                     }

                     break;
                  }
               } while ((addr = addr->Next));
            }

            free(addresses);
         }
      }
   }
#endif

   return ret;
}
