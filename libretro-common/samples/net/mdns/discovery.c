/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
/* Desktop IPv4 transport example; the engine also resolves AAAA records. */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1
#else
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#endif
#include <net/net_mdns.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mswsock.h>
typedef SOCKET mdns_demo_socket_t;
#define MDNS_DEMO_INVALID INVALID_SOCKET
#define mdns_demo_close closesocket
#else
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
typedef int mdns_demo_socket_t;
#define MDNS_DEMO_INVALID (-1)
#define mdns_demo_close close
#endif

typedef struct mdns_demo
{
   mdns_demo_socket_t socket_fd;
   struct sockaddr_in group;
   net_mdns_t *mdns;
   uint8_t packet[NET_MDNS_PACKET_MAX];
#ifdef _WIN32
   LPFN_WSARECVMSG receive;
#endif
} mdns_demo_t;

static volatile sig_atomic_t mdns_demo_done;
static net_mdns_service_t mdns_demo_service;

static void mdns_demo_signal(int sig)
{ (void)sig; mdns_demo_done = 1; }

static uint64_t mdns_demo_now(void)
{
#ifdef _WIN32
   return (uint64_t)GetTickCount64();
#else
   struct timespec ts;
   if (clock_gettime(CLOCK_MONOTONIC, &ts)) return 0;
   return (uint64_t)ts.tv_sec * 1000 + (unsigned long)ts.tv_nsec / 1000000;
#endif
}

static bool mdns_demo_send(void *user, const void *packet, size_t size,
      const net_mdns_peer_t *peer)
{
   mdns_demo_t *demo = (mdns_demo_t*)user;
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

static void mdns_demo_event(void *user, enum net_mdns_event event,
      const net_mdns_service_t *service)
{
   const char *kind = event == NET_MDNS_ADDED ? "ADD" : event == NET_MDNS_CHANGED ? "CHANGE" : "REMOVE";
   (void)user;
   printf("%s %s %u.%u.%u.%u:%u %s\n", kind, service->instance,
         (unsigned)service->ipv4[0], (unsigned)service->ipv4[1],
         (unsigned)service->ipv4[2], (unsigned)service->ipv4[3], (unsigned)service->port,
         service->hostname);
   fflush(stdout);
}

static bool mdns_demo_open(mdns_demo_t *demo, const char *address)
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
   demo->socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
   if (demo->socket_fd == MDNS_DEMO_INVALID) return false;
   if (setsockopt(demo->socket_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, sizeof(one))) return false;
#if !defined(_WIN32) && defined(SO_REUSEPORT)
   if (setsockopt(demo->socket_fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one))) return false;
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
         || setsockopt(demo->socket_fd, IPPROTO_IP, IP_TTL, (const char*)&ttl, sizeof(ttl))
         || setsockopt(demo->socket_fd, IPPROTO_IP, IP_RECVTTL, (const char*)&one, sizeof(one))) return false;
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

static void mdns_demo_receive(mdns_demo_t *demo, uint64_t now)
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
   if (demo->receive(demo->socket_fd, &msg, &bytes, NULL, NULL)
         || msg.dwFlags & (MSG_TRUNC | MSG_CTRUNC)) return;
   received = (int)bytes;
   for (control = WSA_CMSG_FIRSTHDR(&msg); control; control = WSA_CMSG_NXTHDR(&msg, control))
      if (control->cmsg_level == IPPROTO_IP && control->cmsg_type == IP_TTL
            && control->cmsg_len >= WSA_CMSG_LEN(sizeof(int)))
      {
         int ttl;
         memcpy(&ttl, WSA_CMSG_DATA(control), sizeof(ttl)); hop_limit = (unsigned)ttl;
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
   if (received < 0 || msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) return;
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
}

int main(int argc, char **argv)
{
   mdns_demo_t *demo;
   const char *address;
   unsigned duration, port = 0;
   uint64_t now, end;
   bool publish;
   int status = 1;
#ifdef _WIN32
   WSADATA winsock;
   if (WSAStartup(MAKEWORD(2,2), &winsock)) return 1;
#endif
   publish = argc >= 2 && !strcmp(argv[1], "publish");
   if ((!publish && (argc != 4 || strcmp(argv[1], "browse"))) || (publish && argc != 6))
   {
      fprintf(stderr, "usage: discovery browse INTERFACE_IPV4 SECONDS\n"
            "       discovery publish INSTANCE INTERFACE_IPV4 PORT SECONDS\n");
      return 2;
   }
   address = argv[publish ? 3 : 2]; duration = (unsigned)strtoul(argv[publish ? 5 : 3], NULL, 10);
   if (publish) port = (unsigned)strtoul(argv[4], NULL, 10);
   if (!duration || duration > 86400 || (publish && (!port || port > 65535 || strlen(argv[2]) > 63))) return 2;
   demo = (mdns_demo_t*)calloc(1, sizeof(*demo));
   if (!demo) return 1;
   demo->socket_fd = MDNS_DEMO_INVALID;
   if (!mdns_demo_open(demo, address)) { fprintf(stderr, "Cannot open mDNS interface\n"); goto done; }
   now = mdns_demo_now(); end = now + (uint64_t)duration * 1000;
   demo->mdns = net_mdns_new("_pyrowave._tcp.local.", (uint32_t)now, mdns_demo_send, mdns_demo_event, demo);
   if (!demo->mdns) goto done;
   if (publish)
   {
      strcpy(mdns_demo_service.instance, argv[2]);
      sprintf(mdns_demo_service.hostname, "retroarch-%08lx", (unsigned long)(uint32_t)now);
      mdns_demo_service.port = (uint16_t)port; mdns_demo_service.has_ipv4 = true;
      inet_pton(AF_INET, address, mdns_demo_service.ipv4);
      mdns_demo_service.txt[0] = 9; memcpy(mdns_demo_service.txt + 1, "version=1", 9);
      mdns_demo_service.txt_size = 10;
      if (!net_mdns_publish(demo->mdns, &mdns_demo_service, now)) goto done;
   }
   else net_mdns_browse(demo->mdns, true, now);
   signal(SIGINT, mdns_demo_signal); signal(SIGTERM, mdns_demo_signal);
   while (!mdns_demo_done && now < end)
   {
      fd_set readable;
      struct timeval wait;
      uint32_t timeout;
      net_mdns_poll(demo->mdns, now);
      if (net_mdns_published(demo->mdns) == NET_MDNS_CONFLICT)
      { fprintf(stderr, "Name conflict; publication stopped\n"); goto done; }
      timeout = net_mdns_timeout(demo->mdns, now);
      if (timeout > 1000) timeout = 1000;
      if (timeout > end - now) timeout = (uint32_t)(end - now);
      wait.tv_sec = timeout / 1000; wait.tv_usec = (timeout % 1000) * 1000;
      FD_ZERO(&readable); FD_SET(demo->socket_fd, &readable);
      if (select((int)demo->socket_fd + 1, &readable, NULL, NULL, &wait) > 0)
         mdns_demo_receive(demo, mdns_demo_now());
      now = mdns_demo_now();
   }
   status = 0;
done:
   net_mdns_free(demo->mdns);
   if (demo->socket_fd != MDNS_DEMO_INVALID) mdns_demo_close(demo->socket_fd);
   free(demo);
#ifdef _WIN32
   WSACleanup();
#endif
   return status;
}
