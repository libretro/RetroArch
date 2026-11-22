/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#if defined(HAVE_MDNS) && ((defined(_WIN32) && !defined(__WINRT__) && !defined(_XBOX) \
      && (!defined(_WIN32_WINNT) || _WIN32_WINNT >= 0x0600)) \
      || defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__))
#if defined(__linux__)
/* BSD multicast APIs are hidden by strict POSIX feature selection. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#elif defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#endif
#include <net/net_mdns_socket.h>
#include <net/net_compat.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mswsock.h>
/* ws2ipdef.h before the Windows 10 1703 SDK lacks it. */
#ifndef IP_RECVTTL
#define IP_RECVTTL 21
#endif
typedef SOCKET mdns_socket_socket_t;
#define MDNS_SOCKET_INVALID INVALID_SOCKET
#define mdns_socket_close closesocket
#else
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
typedef int mdns_socket_socket_t;
#define MDNS_SOCKET_INVALID (-1)
#define mdns_socket_close close
#endif

struct net_mdns_socket
{
   mdns_socket_socket_t socket_fd;
   struct sockaddr_in group;
   net_mdns_t *mdns;
   uint8_t address[4];
   net_mdns_event_t event;
   void *user;
   uint8_t packet[NET_MDNS_PACKET_MAX];
#ifdef _WIN32
   LPFN_WSARECVMSG receive;
   /* The host rejects IP_RECVTTL; group arrivals stand in for TTL 255. */
   bool ttl_unreported;
#endif
};

static bool mdns_socket_send(void *user, const void *packet, size_t size,
      const net_mdns_peer_t *peer)
{
   net_mdns_socket_t *demo = (net_mdns_socket_t*)user;
   struct sockaddr_in to = demo->group;
   int result;
   if (peer)
   {
      if (peer->family != 4) return false;
      memcpy(&to.sin_addr, peer->address, 4); to.sin_port = htons(peer->port);
   }
   result = (int)sendto(demo->socket_fd, (const char*)packet, (int)size, 0,
         (const struct sockaddr*)&to, sizeof(to));
   return result >= 0 && (size_t)result == size;
}

static void mdns_socket_event(void *user, enum net_mdns_event event,
      const net_mdns_service_t *service)
{
   net_mdns_socket_t *demo = (net_mdns_socket_t*)user;
   if (demo->event) demo->event(demo->user, event, service);
}

static bool mdns_socket_open(net_mdns_socket_t *demo, const char *address)
{
   struct sockaddr_in bind_addr;
   struct ip_mreq membership;
   int one = 1, ttl = 255;
   struct in_addr local;
#ifdef _WIN32
   int multicast_ttl = 255;
   GUID receive_id = WSAID_WSARECVMSG;
   DWORD bytes;
   u_long nonblock = 1;
#else
   unsigned char multicast_ttl = 255;
   int flags;
#endif
   memset(&local, 0, sizeof(local));
   if (inet_pton(AF_INET, address, &local) != 1) return false;
   memcpy(demo->address, &local, 4);
   demo->socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
   if (demo->socket_fd == MDNS_SOCKET_INVALID) return false;
   if (setsockopt(demo->socket_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, sizeof(one))) return false;
#if !defined(_WIN32) && defined(SO_REUSEPORT)
   if (setsockopt(demo->socket_fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one))) return false;
#endif
#if defined(__linux__) && defined(IP_MULTICAST_ALL)
   {
      int all = 0;
      /* Keep this context's cache scoped to its selected multicast link. */
      if (setsockopt(demo->socket_fd, IPPROTO_IP, IP_MULTICAST_ALL, &all, sizeof(all))) return false;
   }
#endif
   memset(&bind_addr, 0, sizeof(bind_addr));
   bind_addr.sin_family = AF_INET; bind_addr.sin_port = htons(5353);
   if (bind(demo->socket_fd, (const struct sockaddr*)&bind_addr, sizeof(bind_addr))) return false;
   memset(&membership, 0, sizeof(membership));
   inet_pton(AF_INET, "224.0.0.251", &membership.imr_multiaddr);
   membership.imr_interface = local;
   if (setsockopt(demo->socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&membership, sizeof(membership))
         || setsockopt(demo->socket_fd, IPPROTO_IP, IP_MULTICAST_IF, (const char*)&local, sizeof(local))
         || setsockopt(demo->socket_fd, IPPROTO_IP, IP_MULTICAST_TTL, (const char*)&multicast_ttl, sizeof(multicast_ttl))
         || setsockopt(demo->socket_fd, IPPROTO_IP, IP_TTL, (const char*)&ttl, sizeof(ttl))) return false;
#ifdef _WIN32
   /* Windows before 10 1703 rejects IP_RECVTTL. Routers never forward
    * 224.0.0.0/24, so a datagram addressed to the group is on-link. */
   if (setsockopt(demo->socket_fd, IPPROTO_IP, IP_RECVTTL, (const char*)&one, sizeof(one)))
   {
      if (setsockopt(demo->socket_fd, IPPROTO_IP, IP_PKTINFO, (const char*)&one, sizeof(one))) return false;
      demo->ttl_unreported = true;
   }
#else
   if (setsockopt(demo->socket_fd, IPPROTO_IP, IP_RECVTTL, (const char*)&one, sizeof(one))) return false;
#endif
   memset(&demo->group, 0, sizeof(demo->group));
   demo->group.sin_family = AF_INET; demo->group.sin_port = htons(5353);
   demo->group.sin_addr = membership.imr_multiaddr;
#ifdef _WIN32
   if (ioctlsocket(demo->socket_fd, FIONBIO, &nonblock)
         || WSAIoctl(demo->socket_fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
            &receive_id, sizeof(receive_id), &demo->receive, sizeof(demo->receive),
            &bytes, NULL, NULL)) return false;
#else
   flags = fcntl(demo->socket_fd, F_GETFL, 0);
   if (flags < 0 || fcntl(demo->socket_fd, F_SETFL, flags | O_NONBLOCK)) return false;
#endif
   return true;
}

static bool mdns_socket_receive(net_mdns_socket_t *demo, uint64_t now)
{
   struct sockaddr_in from;
   net_mdns_peer_t peer;
   unsigned hop_limit = 0;
   int received;
#ifdef _WIN32
   WSABUF buffer;
   WSAMSG msg;
   WSACMSGHDR *control;
   DWORD bytes = 0;
   union { WSACMSGHDR align; char bytes[128]; } ancillary;
   memset(&msg, 0, sizeof(msg));
   buffer.buf = (char*)demo->packet; buffer.len = sizeof(demo->packet);
   msg.name = (struct sockaddr*)&from; msg.namelen = sizeof(from);
   msg.lpBuffers = &buffer; msg.dwBufferCount = 1;
   msg.Control.buf = ancillary.bytes; msg.Control.len = sizeof(ancillary.bytes);
   if (demo->receive(demo->socket_fd, &msg, &bytes, NULL, NULL)) return false;
   if (msg.dwFlags & (MSG_TRUNC | MSG_CTRUNC)) return true;
   received = (int)bytes;
   for (control = WSA_CMSG_FIRSTHDR(&msg); control; control = WSA_CMSG_NXTHDR(&msg, control))
      if (control->cmsg_level == IPPROTO_IP && control->cmsg_type == IP_TTL
            && control->cmsg_len >= WSA_CMSG_LEN(sizeof(int)))
      {
         int ttl;
         memcpy(&ttl, WSA_CMSG_DATA(control), sizeof(ttl)); hop_limit = (unsigned)ttl;
      }
      else if (demo->ttl_unreported && control->cmsg_level == IPPROTO_IP
            && control->cmsg_type == IP_PKTINFO
            && control->cmsg_len >= WSA_CMSG_LEN(sizeof(IN_PKTINFO)))
      {
         IN_PKTINFO info;
         memcpy(&info, WSA_CMSG_DATA(control), sizeof(info));
         if (info.ipi_addr.s_addr == demo->group.sin_addr.s_addr) hop_limit = 255;
      }
#else
   struct msghdr msg;
   struct iovec buffer;
   struct cmsghdr *control;
   union { struct cmsghdr align; char bytes[128]; } ancillary;
   memset(&msg, 0, sizeof(msg));
   buffer.iov_base = demo->packet; buffer.iov_len = sizeof(demo->packet);
   msg.msg_name = &from; msg.msg_namelen = sizeof(from);
   msg.msg_iov = &buffer; msg.msg_iovlen = 1;
   msg.msg_control = ancillary.bytes; msg.msg_controllen = sizeof(ancillary.bytes);
   received = (int)recvmsg(demo->socket_fd, &msg, 0);
   if (received < 0) return false;
   if (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) return true;
   for (control = CMSG_FIRSTHDR(&msg); control; control = CMSG_NXTHDR(&msg, control))
      if (control->cmsg_level == IPPROTO_IP && (control->cmsg_type == IP_TTL
               || control->cmsg_type == IP_RECVTTL) && control->cmsg_len >= CMSG_LEN(1))
      {
         int ttl = 0;
         if (control->cmsg_len >= CMSG_LEN(sizeof(ttl))) memcpy(&ttl, CMSG_DATA(control), sizeof(ttl));
         else ttl = *(const unsigned char*)CMSG_DATA(control);
         hop_limit = (unsigned)ttl;
      }
#endif
   memset(&peer, 0, sizeof(peer));
   memcpy(peer.address, &from.sin_addr, 4); peer.port = ntohs(from.sin_port); peer.family = 4;
   net_mdns_receive(demo->mdns, demo->packet, (size_t)received, &peer, hop_limit, now);
   return true;
}

static bool mdns_socket_route_to(const char *destination, char address[16])
{
   mdns_socket_socket_t fd;
   struct sockaddr_in group, local;
#ifdef _WIN32
   int bytes = sizeof(local);
   u_long nonblock = 1;
#else
   socklen_t bytes = sizeof(local);
#endif
   bool ok = false;
   fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
   if (fd == MDNS_SOCKET_INVALID) return false;
#ifdef _WIN32
   if (ioctlsocket(fd, FIONBIO, &nonblock)) goto done;
#else
   if (fcntl(fd, F_SETFL, O_NONBLOCK)) goto done;
#endif
   memset(&group, 0, sizeof(group)); group.sin_family = AF_INET;
   group.sin_port = htons(5353); inet_pton(AF_INET, destination, &group.sin_addr);
   if (connect(fd, (struct sockaddr*)&group, sizeof(group))
         || getsockname(fd, (struct sockaddr*)&local, &bytes) || !local.sin_addr.s_addr) goto done;
   ok = inet_ntop(AF_INET, &local.sin_addr, address, 16) != NULL;
done:
   mdns_socket_close(fd);
   return ok;
}

static bool mdns_socket_route(char address[16])
{
   /* UDP connect performs only a local route lookup: no datagram is sent.
    * Windows may choose loopback among its equal multicast routes; prefer
    * the default unicast route, then try the multicast route on isolated LANs. */
   if (mdns_socket_route_to("192.0.2.1", address) && strncmp(address, "127.", 4)) return true;
   return mdns_socket_route_to("224.0.0.251", address) && strncmp(address, "127.", 4);
}

net_mdns_socket_t *net_mdns_socket_new(const char *type, const char *address,
      uint32_t seed, net_mdns_event_t event, void *user)
{
   net_mdns_socket_t *context;
   char local[16];
   if (!network_init()) return NULL;
   if (!address)
   {
      if (!mdns_socket_route(local)) return NULL;
      address = local;
   }
   context = (net_mdns_socket_t*)calloc(1, sizeof(*context));
   if (!context) return NULL;
   context->socket_fd = MDNS_SOCKET_INVALID;
   context->event = event; context->user = user;
   if (!mdns_socket_open(context, address)) { net_mdns_socket_free(context); return NULL; }
   context->mdns = net_mdns_new(type, seed, mdns_socket_send, mdns_socket_event, context);
   if (!context->mdns) { net_mdns_socket_free(context); return NULL; }
   return context;
}

net_mdns_t *net_mdns_socket_engine(net_mdns_socket_t *context)
{ return context ? context->mdns : NULL; }

bool net_mdns_socket_address(const net_mdns_socket_t *context, uint8_t address[4])
{
   if (!context || !address) return false;
   memcpy(address, context->address, 4); return true;
}

void net_mdns_socket_poll(net_mdns_socket_t *context, uint64_t now)
{
   unsigned i;
   if (!context) return;
   for (i = 0; i < 8; i++) if (!mdns_socket_receive(context, now)) break;
   net_mdns_poll(context->mdns, now);
}

void net_mdns_socket_free(net_mdns_socket_t *context)
{
   if (!context) return;
   net_mdns_free(context->mdns);
   if (context->socket_fd != MDNS_SOCKET_INVALID) mdns_socket_close(context->socket_fd);
   free(context);
}
#else
#include <net/net_mdns_socket.h>
net_mdns_socket_t *net_mdns_socket_new(const char *type, const char *address,
      uint32_t seed, net_mdns_event_t event, void *user)
{ (void)type; (void)address; (void)seed; (void)event; (void)user; return NULL; }
net_mdns_t *net_mdns_socket_engine(net_mdns_socket_t *context)
{ (void)context; return NULL; }
bool net_mdns_socket_address(const net_mdns_socket_t *context, uint8_t address[4])
{ (void)context; (void)address; return false; }
void net_mdns_socket_poll(net_mdns_socket_t *context, uint64_t now)
{ (void)context; (void)now; }
void net_mdns_socket_free(net_mdns_socket_t *context)
{ (void)context; }
#endif
