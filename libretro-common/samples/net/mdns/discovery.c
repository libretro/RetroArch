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
#include <net/net_mdns_socket.h>
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
#endif

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

int main(int argc, char **argv)
{
   net_mdns_socket_t *transport;
   net_mdns_t *mdns;
   const char *address;
   unsigned duration, port = 0;
   uint64_t now, end;
   bool publish;
   int status = 1;
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
   now = mdns_demo_now(); end = now + (uint64_t)duration * 1000;
   transport = net_mdns_socket_new("_pyrowave._tcp.local.", address, (uint32_t)now, mdns_demo_event, NULL);
   if (!transport) { fprintf(stderr, "Cannot open mDNS interface\n"); return 1; }
   mdns = net_mdns_socket_engine(transport);
   if (publish)
   {
      strcpy(mdns_demo_service.instance, argv[2]);
      sprintf(mdns_demo_service.hostname, "retroarch-%08lx", (unsigned long)(uint32_t)now);
      mdns_demo_service.port = (uint16_t)port; mdns_demo_service.has_ipv4 = true;
      net_mdns_socket_address(transport, mdns_demo_service.ipv4);
      mdns_demo_service.txt[0] = 9; memcpy(mdns_demo_service.txt + 1, "version=1", 9);
      mdns_demo_service.txt_size = 10;
      if (!net_mdns_publish(mdns, &mdns_demo_service, now)) goto done;
   }
   else net_mdns_browse(mdns, true, now);
   signal(SIGINT, mdns_demo_signal); signal(SIGTERM, mdns_demo_signal);
   while (!mdns_demo_done && now < end)
   {
      /* Only this command-line harness waits. Production polling is bounded
       * and is supplied the frontend's existing iteration timestamp. */
      net_mdns_socket_poll(transport, now);
      if (net_mdns_published(mdns) == NET_MDNS_CONFLICT)
      { fprintf(stderr, "Name conflict; publication stopped\n"); goto done; }
#ifdef _WIN32
      Sleep(10);
#else
      {
         struct timespec wait;
         wait.tv_sec = 0; wait.tv_nsec = 10000000;
         nanosleep(&wait, NULL);
      }
#endif
      now = mdns_demo_now();
   }
   status = 0;
done:
   net_mdns_socket_free(transport);
   return status;
}
