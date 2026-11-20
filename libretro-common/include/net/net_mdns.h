/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef LIBRETRO_NET_MDNS_H
#define LIBRETRO_NET_MDNS_H

#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

#define NET_MDNS_TXT_MAX 512
#define NET_MDNS_NAME_MAX 256
#define NET_MDNS_PACKET_MAX 9000
#define NET_MDNS_SERVICE_MAX 32

typedef struct net_mdns net_mdns_t;

typedef struct net_mdns_peer
{
   uint8_t address[16];
   uint16_t port;
   unsigned family; /* 4 or 6; IPv6 scope belongs to the transport/link. */
} net_mdns_peer_t;

typedef struct net_mdns_service
{
   char instance[64];
   char hostname[NET_MDNS_NAME_MAX];
   uint8_t ipv4[4];
   uint8_t ipv6[16];
   uint8_t txt[NET_MDNS_TXT_MAX]; /* DNS-SD length-prefixed strings. */
   size_t txt_size;
   uint16_t port;
   bool has_ipv4, has_ipv6;
} net_mdns_service_t;

enum net_mdns_event
{
   NET_MDNS_ADDED,
   NET_MDNS_CHANGED,
   NET_MDNS_REMOVED
};

enum net_mdns_publish_state
{
   NET_MDNS_IDLE,
   NET_MDNS_PROBING,
   NET_MDNS_ESTABLISHED,
   NET_MDNS_CONFLICT
};

/* packet and service are borrowed until the callback returns. A NULL peer
 * means multicast to 224.0.0.251/ff02::fb, UDP 5353, hop limit 255.
 * Callbacks must not reenter or destroy this context. */
typedef bool (*net_mdns_send_t)(void *user, const void *packet, size_t size,
      const net_mdns_peer_t *peer);
typedef void (*net_mdns_event_t)(void *user, enum net_mdns_event event,
      const net_mdns_service_t *service);

RETRO_BEGIN_DECLS

/* Socket-neutral, bounded engine; compile net_mdns.c with HAVE_MDNS.
 * One context per link and service type, e.g. "_pyrowave._tcp.local.".
 * The caller owns nonblocking I/O, interface membership, and scheduling.
 * now_ms is a caller-provided monotonic clock, never a wall clock.
 * seed supplies non-cryptographic timing jitter. No threads are required. */
net_mdns_t *net_mdns_new(const char *service_type, uint32_t seed,
      net_mdns_send_t send, net_mdns_event_t event, void *user);
void net_mdns_free(net_mdns_t *mdns);
void net_mdns_browse(net_mdns_t *mdns, bool enabled, uint64_t now_ms);

/* Hostname is a single ASCII DNS host label, without .local. Instance is
 * one DNS-SD label (up to 63 UTF-8 bytes; dots are literal, not separators).
 * Address flags select the addresses of THIS link. Copies the configuration.
 * On a name conflict publication stops; choose a new name and publish again. */
bool net_mdns_publish(net_mdns_t *mdns, const net_mdns_service_t *service,
      uint64_t now_ms);
void net_mdns_unpublish(net_mdns_t *mdns);
enum net_mdns_publish_state net_mdns_published(const net_mdns_t *mdns);

/* The transport must reject off-link packets and dispatch only this link.
 * Supply the received IP TTL / IPv6 hop limit; values other than 255 are
 * rejected. No socket or platform headers enter this API. Invalid packets
 * have no cache/publication side effects. Returns false for invalid input. */
bool net_mdns_receive(net_mdns_t *mdns, const void *packet, size_t size,
      const net_mdns_peer_t *peer, unsigned hop_limit, uint64_t now_ms);
/* Bounded work: at most one browse, probe/announcement, and reply per call.
 * Drive from an existing event loop; this function never waits or reads time. */
void net_mdns_poll(net_mdns_t *mdns, uint64_t now_ms);
/* Milliseconds until scheduled work, or UINT32_MAX when no work is pending.
 * Combine with the transport's readable event; never use a busy polling loop. */
uint32_t net_mdns_timeout(const net_mdns_t *mdns, uint64_t now_ms);
size_t net_mdns_service_count(const net_mdns_t *mdns);
/* Borrowed until the next receive/poll/browse call. */
const net_mdns_service_t *net_mdns_service_at(const net_mdns_t *mdns, size_t index);

RETRO_END_DECLS
#endif
