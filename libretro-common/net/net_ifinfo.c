/* Copyright  (C) 2010-2016 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (compat_fnmatch.c).
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

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>

#include <net_ifinfo.h>

void net_ifinfo_free(net_ifinfo_t *list)
{
   unsigned k;

   if (!list)
      return;

   for (k = 0; k < list->size; k++)
   {
      struct net_ifinfo_entry *ptr = 
         (struct net_ifinfo_entry*)&list->entries[k];

      if (!ptr)
         continue;

      if (*ptr->name)
         free(ptr->name);
      if (*ptr->host)
         free(ptr->host);

      ptr->name = NULL;
      ptr->host = NULL;
   }
   free(list->entries);
   free(list);
}

bool net_ifinfo_new(net_ifinfo_t *list)
{
   unsigned k              = 0;
   struct ifaddrs *ifa     = NULL;
   struct ifaddrs *ifaddr  = NULL;

   if (getifaddrs(&ifaddr) == -1)
      goto error;

   if (!list)
      goto error;

   for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
   {
      char host[NI_MAXHOST];
      int s = 0;

      if (!ifa->ifa_addr)
         continue;

      if (ifa->ifa_addr->sa_family != AF_INET)
         continue;

      s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
            host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);

      if (s != 0)
         goto error;

      {
         struct net_ifinfo_entry *ptr = (struct net_ifinfo_entry*)
            realloc(list->entries, (k+1) * sizeof(struct net_ifinfo_entry));

         if (!ptr)
            goto error;

         list->entries = ptr;
      }

      list->entries[k].name  = strdup(ifa->ifa_name);
      list->entries[k].host  = strdup(host);
      k++;
      list->size = k;
   }

   freeifaddrs(ifaddr);

   return true;

error:
   freeifaddrs(ifaddr);
   net_ifinfo_free(list);

   return false;
}
