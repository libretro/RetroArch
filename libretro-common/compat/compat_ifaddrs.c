/*
Copyright (c) 2013, Kenneth MacKay
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <compat/strl.h>
#include <compat/ifaddrs.h>

#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

typedef struct NetlinkList
{
    struct NetlinkList *m_next;
    struct nlmsghdr *m_data;
    unsigned int m_size;
} NetlinkList;

static int netlink_socket(void)
{
   struct sockaddr_nl l_addr;
   int l_socket = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);

   if (l_socket < 0)
      return -1;

   memset(&l_addr, 0, sizeof(l_addr));
   l_addr.nl_family = AF_NETLINK;

   if (bind(l_socket, (struct sockaddr *)&l_addr, sizeof(l_addr)) < 0)
   {
      close(l_socket);
      return -1;
   }

   return l_socket;
}

static int netlink_send(int p_socket, int p_request)
{
   struct
   {
      struct nlmsghdr m_hdr;
      struct rtgenmsg m_msg;
   } l_data;
   struct sockaddr_nl l_addr;

   memset(&l_data, 0, sizeof(l_data));

   l_data.m_hdr.nlmsg_len    = NLMSG_LENGTH(sizeof(struct rtgenmsg));
   l_data.m_hdr.nlmsg_type   = p_request;
   l_data.m_hdr.nlmsg_flags  = NLM_F_ROOT | NLM_F_MATCH | NLM_F_REQUEST;
   l_data.m_hdr.nlmsg_pid    = 0;
   l_data.m_hdr.nlmsg_seq    = p_socket;
   l_data.m_msg.rtgen_family = AF_UNSPEC;

   memset(&l_addr, 0, sizeof(l_addr));
   l_addr.nl_family = AF_NETLINK;
   return (sendto(p_socket, &l_data.m_hdr, l_data.m_hdr.nlmsg_len, 0, (struct sockaddr *)&l_addr, sizeof(l_addr)));
}

static int netlink_recv(int p_socket, void *p_buffer, size_t p_len)
{
   struct msghdr l_msg;
   struct sockaddr_nl l_addr;
   struct iovec l_iov = { p_buffer, p_len };

   for (;;)
   {
      int l_result;

      l_msg.msg_name       = (void *)&l_addr;
      l_msg.msg_namelen    = sizeof(l_addr);
      l_msg.msg_iov        = &l_iov;
      l_msg.msg_iovlen     = 1;
      l_msg.msg_control    = NULL;
      l_msg.msg_controllen = 0;
      l_msg.msg_flags      = 0;

      l_result             = recvmsg(p_socket, &l_msg, 0);

      if (l_result < 0)
      {
         if (errno == EINTR)
            continue;
         return -2;
      }

      if (l_msg.msg_flags & MSG_TRUNC) /* buffer too small */
         return -1;
      return l_result;
   }
   return 0;
}

static struct nlmsghdr *ifaddrs_get_netlink_response(int p_socket,
      int *p_size, int *p_done)
{
   size_t l_size  = 4096;
   void *l_buffer = NULL;

   for (;;)
   {
      int l_read;

      free(l_buffer);
      l_buffer = malloc(l_size);
      if (!l_buffer)
         return NULL;

      l_read  = netlink_recv(p_socket, l_buffer, l_size);
      *p_size = l_read;

      if (l_read == -2)
      {
         free(l_buffer);
         return NULL;
      }

      if (l_read >= 0)
      {
         pid_t l_pid = getpid();
         struct nlmsghdr *l_hdr;

         for (l_hdr = (struct nlmsghdr *)l_buffer;
               NLMSG_OK(l_hdr, (unsigned int)l_read);
               l_hdr = (struct nlmsghdr *)NLMSG_NEXT(l_hdr, l_read))
         {
            if (   (pid_t)l_hdr->nlmsg_pid != l_pid
                  || (int)l_hdr->nlmsg_seq != p_socket)
               continue;

            if (l_hdr->nlmsg_type == NLMSG_DONE)
            {
               *p_done = 1;
               break;
            }

            if (l_hdr->nlmsg_type == NLMSG_ERROR)
            {
               free(l_buffer);
               return NULL;
            }
         }
         return l_buffer;
      }

      l_size *= 2;
   }
}

static NetlinkList *ifaddr_new_list_item(struct nlmsghdr *p_data,
   unsigned int p_size)
{
   NetlinkList *l_item = (NetlinkList*)malloc(sizeof(NetlinkList));
   if (!l_item)
      return NULL;

   l_item->m_next = NULL;
   l_item->m_data = p_data;
   l_item->m_size = p_size;
   return l_item;
}

static void ifaddr_free_result_list(NetlinkList *p_list)
{
   NetlinkList *l_cur;

   while (p_list)
   {
      l_cur = p_list;
      p_list = p_list->m_next;
      free(l_cur->m_data);
      free(l_cur);
   }
}

static NetlinkList *ifaddrs_get_resultlist(int p_socket, int p_request)
{
   int l_size;
   NetlinkList *l_list = NULL;
   NetlinkList *l_end  = NULL;
   int l_done          = 0;

   if (netlink_send(p_socket, p_request) < 0)
      return NULL;

   while (!l_done)
   {
      NetlinkList *l_item    = NULL;
      struct nlmsghdr *l_hdr = ifaddrs_get_netlink_response(
      p_socket, &l_size, &l_done);
      if (!l_hdr)
         goto error;

      l_item = ifaddr_new_list_item(l_hdr, l_size);
      if (!l_item)
         goto error;

      if (!l_list)
         l_list        = l_item;
      else
         l_end->m_next = l_item;
      l_end            = l_item;
   }

   return l_list;

error:
   ifaddr_free_result_list(l_list);
   return NULL;
}

static size_t ifaddrs_max_size(size_t a, size_t b) { return (a > b ? a : b); }

static size_t ifaddrs_calc_addr_len(sa_family_t p_family, int p_dataSize)
{
   switch(p_family)
   {
      case AF_INET:
         return sizeof(struct sockaddr_in);
      case AF_INET6:
         return sizeof(struct sockaddr_in6);
      case AF_PACKET:
         return ifaddrs_max_size(sizeof(struct sockaddr_ll), offsetof(struct sockaddr_ll, sll_addr) + p_dataSize);
      default:
         break;
   }

   return ifaddrs_max_size(sizeof(struct sockaddr), offsetof(struct sockaddr, sa_data) + p_dataSize);
}

static void ifaddrs_make_sock_addr(sa_family_t p_family,
   struct sockaddr *p_dest, void *p_data, size_t p_size)
{
   switch(p_family)
   {
      case AF_INET:
         memcpy(&((struct sockaddr_in*)p_dest)->sin_addr, p_data, p_size);
         break;
      case AF_INET6:
         memcpy(&((struct sockaddr_in6*)p_dest)->sin6_addr, p_data, p_size);
         break;
      case AF_PACKET:
         memcpy(((struct sockaddr_ll*)p_dest)->sll_addr, p_data, p_size);
         ((struct sockaddr_ll*)p_dest)->sll_halen = p_size;
         break;
      default:
         memcpy(p_dest->sa_data, p_data, p_size);
         break;
   }
   p_dest->sa_family = p_family;
}

static void ifaddrs_add_to_end(struct ifaddrs **p_resultList,
   struct ifaddrs *p_entry)
{
   if (!*p_resultList)
      *p_resultList = p_entry;
   else
   {
      struct ifaddrs *l_cur = *p_resultList;
      while (l_cur->ifa_next)
         l_cur = l_cur->ifa_next;
      l_cur->ifa_next = p_entry;
   }
}

static int ifaddrs_interpret_link(struct nlmsghdr *p_hdr,
   struct ifaddrs **p_resultList)
{
   char *l_index, *l_addr, *l_name, *l_data;
   struct ifaddrs *l_entry  = NULL;
   struct rtattr *l_rta     = NULL;
   struct ifinfomsg *l_info = (struct ifinfomsg *)NLMSG_DATA(p_hdr);
   size_t l_nameSize        = 0;
   size_t l_addrSize        = 0;
   size_t l_dataSize        = 0;
   size_t l_rtaSize         = NLMSG_PAYLOAD(p_hdr, sizeof(struct ifinfomsg));

   for (l_rta = IFLA_RTA(l_info); RTA_OK(l_rta, l_rtaSize);
         l_rta = RTA_NEXT(l_rta, l_rtaSize))
   {
      size_t l_rtaDataSize = RTA_PAYLOAD(l_rta);
      switch(l_rta->rta_type)
      {
         case IFLA_ADDRESS:
         case IFLA_BROADCAST:
            l_addrSize += NLMSG_ALIGN(ifaddrs_calc_addr_len(AF_PACKET, l_rtaDataSize));
            break;
         case IFLA_IFNAME:
            l_nameSize += NLMSG_ALIGN(l_rtaSize + 1);
            break;
         case IFLA_STATS:
            l_dataSize += NLMSG_ALIGN(l_rtaSize);
            break;
         default:
            break;
      }
   }

   l_entry = (struct ifaddrs*)malloc(sizeof(struct ifaddrs) + sizeof(int) + l_nameSize + l_addrSize + l_dataSize);
   if (!l_entry)
      return -1;

   memset(l_entry, 0, sizeof(struct ifaddrs));
   l_entry->ifa_name = "";

   l_index = ((char *)l_entry) + sizeof(struct ifaddrs);
   l_name  = l_index + sizeof(int);
   l_addr  = l_name + l_nameSize;
   l_data  = l_addr + l_addrSize;

   /* save the interface index so we can look 
    * it up when handling the addresses. */
   memcpy(l_index, &l_info->ifi_index, sizeof(int));

   l_entry->ifa_flags = l_info->ifi_flags;

   l_rtaSize          = NLMSG_PAYLOAD(p_hdr, sizeof(struct ifinfomsg));

   for (l_rta = IFLA_RTA(l_info); RTA_OK(l_rta, l_rtaSize);
         l_rta = RTA_NEXT(l_rta, l_rtaSize))
   {
      void      *l_rtaData = RTA_DATA(l_rta);
      size_t l_rtaDataSize = RTA_PAYLOAD(l_rta);

      switch(l_rta->rta_type)
      {
         case IFLA_ADDRESS:
         case IFLA_BROADCAST:
            {
               size_t l_addr_len = ifaddrs_calc_addr_len(AF_PACKET, l_rtaDataSize);
               ifaddrs_make_sock_addr(AF_PACKET, (struct sockaddr *)l_addr, l_rtaData, l_rtaDataSize);
               ((struct sockaddr_ll *)l_addr)->sll_ifindex = l_info->ifi_index;
               ((struct sockaddr_ll *)l_addr)->sll_hatype = l_info->ifi_type;
               if (l_rta->rta_type == IFLA_ADDRESS)
                  l_entry->ifa_addr      = (struct sockaddr *)l_addr;
               else
                  l_entry->ifa_broadaddr = (struct sockaddr *)l_addr;
               l_addr += NLMSG_ALIGN(l_addr_len);
            }
            break;
         case IFLA_IFNAME:
            memcpy(l_name, l_rtaData, l_rtaDataSize);
            l_name[l_rtaDataSize] = '\0';
            l_entry->ifa_name = l_name;
            break;
         case IFLA_STATS:
            memcpy(l_data, l_rtaData, l_rtaDataSize);
            l_entry->ifa_data = l_data;
            break;
         default:
            break;
      }
   }

   ifaddrs_add_to_end(p_resultList, l_entry);
   return 0;
}

static struct ifaddrs *ifaddrs_find_interface(int p_index,
      struct ifaddrs **p_links, int p_numLinks)
{
   int l_num             = 0;
   struct ifaddrs *l_cur = *p_links;

   while (l_cur && l_num < p_numLinks)
   {
      int l_index;
      char *l_indexPtr = ((char *)l_cur) + sizeof(struct ifaddrs);

      memcpy(&l_index, l_indexPtr, sizeof(int));
      if (l_index == p_index)
         return l_cur;

      l_cur = l_cur->ifa_next;
      ++l_num;
   }
   return NULL;
}

static int ifaddrs_interpret_addr(struct nlmsghdr *p_hdr,
      struct ifaddrs **p_resultList, int p_numLinks)
{
   char *l_name, *l_addr;
   struct rtattr *l_rta;
   struct ifaddrs *l_entry;
   size_t l_rtaSize;
   size_t l_nameSize           = 0;
   size_t l_addrSize           = 0;
   int l_addedNetmask          = 0;
   struct ifaddrmsg *l_info    = (struct ifaddrmsg *)NLMSG_DATA(p_hdr);
   struct ifaddrs *l_interface = ifaddrs_find_interface(l_info->ifa_index, p_resultList, p_numLinks);

   if (l_info->ifa_family == AF_PACKET)
      return 0;

   l_rtaSize = NLMSG_PAYLOAD(p_hdr, sizeof(struct ifaddrmsg));

   for (l_rta = IFA_RTA(l_info); RTA_OK(l_rta, l_rtaSize);
         l_rta = RTA_NEXT(l_rta, l_rtaSize))
   {
      size_t l_rtaDataSize = RTA_PAYLOAD(l_rta);

      switch(l_rta->rta_type)
      {
         case IFA_ADDRESS:
         case IFA_LOCAL:
            if ((l_info->ifa_family == AF_INET || l_info->ifa_family == AF_INET6) && !l_addedNetmask)
            {
               /* make room for netmask */
               l_addrSize += NLMSG_ALIGN(ifaddrs_calc_addr_len(l_info->ifa_family, l_rtaDataSize));
               l_addedNetmask = 1;
            }
         case IFA_BROADCAST:
            l_addrSize += NLMSG_ALIGN(ifaddrs_calc_addr_len(l_info->ifa_family, l_rtaDataSize));
            break;
         case IFA_LABEL:
            l_nameSize += NLMSG_ALIGN(l_rtaSize + 1);
            break;
         default:
            break;
      }
   }

   l_entry = (struct ifaddrs*)malloc(sizeof(struct ifaddrs) + l_nameSize + l_addrSize);
   if (!l_entry)
      return -1;

   memset(l_entry, 0, sizeof(struct ifaddrs));
   l_entry->ifa_name = (l_interface ? l_interface->ifa_name : "");

   l_name = ((char *)l_entry) + sizeof(struct ifaddrs);
   l_addr = l_name + l_nameSize;

   l_entry->ifa_flags = l_info->ifa_flags;
   if (l_interface)
      l_entry->ifa_flags |= l_interface->ifa_flags;

   l_rtaSize = NLMSG_PAYLOAD(p_hdr, sizeof(struct ifaddrmsg));
   for (l_rta = IFA_RTA(l_info); RTA_OK(l_rta, l_rtaSize);
         l_rta = RTA_NEXT(l_rta, l_rtaSize))
   {
      void *l_rtaData      = RTA_DATA(l_rta);
      size_t l_rtaDataSize = RTA_PAYLOAD(l_rta);
      switch(l_rta->rta_type)
      {
         case IFA_ADDRESS:
         case IFA_BROADCAST:
         case IFA_LOCAL:
            {
               size_t l_addrLen = ifaddrs_calc_addr_len(l_info->ifa_family, l_rtaDataSize);
               ifaddrs_make_sock_addr(l_info->ifa_family, (struct sockaddr *)l_addr, l_rtaData, l_rtaDataSize);
               if (l_info->ifa_family == AF_INET6)
               {
                  if (IN6_IS_ADDR_LINKLOCAL((struct in6_addr *)l_rtaData) 
                   || IN6_IS_ADDR_MC_LINKLOCAL((struct in6_addr *)l_rtaData))
                     ((struct sockaddr_in6 *)l_addr)->sin6_scope_id = l_info->ifa_index;
               }

               if (l_rta->rta_type == IFA_ADDRESS)
               {
                  /* Apparently in a point-to-point network IFA_ADDRESS
                   * contains the dest address and IFA_LOCAL contains 
                   * the local address */
                  if (l_entry->ifa_addr)
                     l_entry->ifa_dstaddr = (struct sockaddr *)l_addr;
                  else
                     l_entry->ifa_addr    = (struct sockaddr *)l_addr;
               }
               else if (l_rta->rta_type == IFA_LOCAL)
               {
                  if (l_entry->ifa_addr)
                     l_entry->ifa_dstaddr = l_entry->ifa_addr;
                  l_entry->ifa_addr = (struct sockaddr *)l_addr;
               }
               else
                  l_entry->ifa_broadaddr = (struct sockaddr *)l_addr;
               l_addr += NLMSG_ALIGN(l_addrLen);
               break;
            }
         case IFA_LABEL:
            strlcpy(l_name, l_rtaData, l_rtaDataSize + 1);
            l_entry->ifa_name = l_name;
            break;
         default:
            break;
      }
   }

   if (          l_entry->ifa_addr
          && (   l_entry->ifa_addr->sa_family == AF_INET
          ||     l_entry->ifa_addr->sa_family == AF_INET6))
   {
      unsigned i;
      char l_mask[16];
      unsigned l_maxPrefix = (l_entry->ifa_addr->sa_family == AF_INET
            ? 32 : 128);
      unsigned l_prefix    = (l_info->ifa_prefixlen > l_maxPrefix
            ? l_maxPrefix : l_info->ifa_prefixlen);

      l_mask[0] = '\0';

      for (i = 0; i < (l_prefix/8); ++i)
         l_mask[i] = 0xff;
      if (l_prefix % 8)
         l_mask[i] = 0xff << (8 - (l_prefix % 8));

      ifaddrs_make_sock_addr(l_entry->ifa_addr->sa_family,
            (struct sockaddr *)l_addr, l_mask, l_maxPrefix / 8);
      l_entry->ifa_netmask = (struct sockaddr *)l_addr;
   }

   ifaddrs_add_to_end(p_resultList, l_entry);
   return 0;
}

static int ifaddrs_interpret_links(int p_socket,
      NetlinkList *p_netlinkList, struct ifaddrs **p_resultList)
{
   int l_numLinks = 0;
   pid_t l_pid    = getpid();

   for (; p_netlinkList; p_netlinkList = p_netlinkList->m_next)
   {
      struct nlmsghdr *l_hdr = NULL;
      unsigned int l_nlsize  = p_netlinkList->m_size;

      for (l_hdr = p_netlinkList->m_data; NLMSG_OK(l_hdr, l_nlsize);
           l_hdr = NLMSG_NEXT(l_hdr, l_nlsize))
      {
         if (     (pid_t)l_hdr->nlmsg_pid != l_pid 
               ||   (int)l_hdr->nlmsg_seq != p_socket)
            continue;

         if (l_hdr->nlmsg_type == NLMSG_DONE)
            break;

         if (l_hdr->nlmsg_type == RTM_NEWLINK)
         {
            if (ifaddrs_interpret_link(l_hdr, p_resultList) == -1)
               return -1;
            ++l_numLinks;
         }
      }
   }
   return l_numLinks;
}

static int ifaddrs_interpret_addrs(int p_socket,
      NetlinkList *p_netlinkList,
      struct ifaddrs **p_resultList, int p_numLinks)
{
   pid_t l_pid = getpid();
   for (; p_netlinkList; p_netlinkList = p_netlinkList->m_next)
   {
      struct nlmsghdr *l_hdr = NULL;
      unsigned int l_nlsize  = p_netlinkList->m_size;

      for (l_hdr = p_netlinkList->m_data; NLMSG_OK(l_hdr, l_nlsize);
            l_hdr = NLMSG_NEXT(l_hdr, l_nlsize))
      {
         if (     (pid_t)l_hdr->nlmsg_pid != l_pid 
               || (int)l_hdr->nlmsg_seq   != p_socket)
            continue;

         if (l_hdr->nlmsg_type == NLMSG_DONE)
            break;

         if (l_hdr->nlmsg_type == RTM_NEWADDR)
         {
            if (ifaddrs_interpret_addr(l_hdr, p_resultList, p_numLinks) == -1)
               return -1;
         }
      }
   }
   return 0;
}

int getifaddrs(struct ifaddrs **ifap)
{
   int l_numLinks;
   NetlinkList *l_linkResults;
   NetlinkList *l_addrResults;
   int l_socket   = 0;
   int l_result   = 0;
   if (!ifap)
      return -1;

   *ifap    = NULL;
   l_socket = netlink_socket();

   if (l_socket < 0)
      return -1;

   l_linkResults = ifaddrs_get_resultlist(l_socket, RTM_GETLINK);
   if (!l_linkResults)
   {
      close(l_socket);
      return -1;
   }

   l_addrResults = ifaddrs_get_resultlist(l_socket, RTM_GETADDR);
   if (!l_addrResults)
   {
      close(l_socket);
      ifaddr_free_result_list(l_linkResults);
      return -1;
   }

   l_numLinks = ifaddrs_interpret_links(l_socket, l_linkResults, ifap);

   if (     l_numLinks == -1
         || ifaddrs_interpret_addrs(l_socket, l_addrResults, ifap, l_numLinks) == -1)
      l_result = -1;

   ifaddr_free_result_list(l_linkResults);
   ifaddr_free_result_list(l_addrResults);
   close(l_socket);
   return l_result;
}

void freeifaddrs(struct ifaddrs *ifa)
{
   struct ifaddrs *l_cur = NULL;

   while (ifa)
   {
      l_cur = ifa;
      ifa   = ifa->ifa_next;
      free(l_cur);
   }
}
