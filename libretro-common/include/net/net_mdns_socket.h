/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef LIBRETRO_NET_MDNS_SOCKET_H
#define LIBRETRO_NET_MDNS_SOCKET_H
#include <net/net_mdns.h>

typedef struct net_mdns_socket net_mdns_socket_t;
RETRO_BEGIN_DECLS
/* Desktop IPv4 link transport. NULL address selects the multicast route's
 * local address. Cold construction; unsupported platforms return NULL. */
net_mdns_socket_t *net_mdns_socket_new(const char *type, const char *address,
      uint32_t seed, net_mdns_event_t event, void *user);
net_mdns_t *net_mdns_socket_engine(net_mdns_socket_t *socket_context);
bool net_mdns_socket_address(const net_mdns_socket_t *socket_context, uint8_t address[4]);
/* Main/event-loop owner; nonblocking, at most eight input packets per call.
 * now_ms comes from the owner's existing monotonic clock. */
void net_mdns_socket_poll(net_mdns_socket_t *socket_context, uint64_t now_ms);
void net_mdns_socket_free(net_mdns_socket_t *socket_context);
RETRO_END_DECLS
#endif
