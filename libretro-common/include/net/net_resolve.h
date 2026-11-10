/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef NET_RESOLVE_H
#define NET_RESOLVE_H

#include <retro_common_api.h>
#include <boolean.h>

typedef struct net_resolve net_resolve_t;
typedef bool (*net_resolve_dispatch_t)(void (*work)(void*), void *job,
      void *userdata);

enum net_resolve_result
{
   NET_RESOLVE_ERROR = -1,
   NET_RESOLVE_PENDING,
   NET_RESOLVE_READY
};

RETRO_BEGIN_DECLS
/* Networking must already be initialized. Copies the raw hostname (up to 1023
 * bytes, no IPv6 brackets) since a canceled lookup can outlive its caller.
 * dispatch must queue work exactly once on a resolver worker, or return
 * false without invoking it. Never run it inline on the main/frame thread.
 * Four live requests maximum, including canceled jobs not yet retired.
 * NULL also reports capacity/contended admission; caller schedules any retry.
 * The executor owns worker lifetime; this module never starts or joins threads. */
net_resolve_t *net_resolve_new(const char *hostname, unsigned port,
      net_resolve_dispatch_t dispatch, void *userdata);
/* One caller owns each request; serialize poll/free. No work/wait.
 * On READY, addresses is a borrowed addrinfo chain, compatible
 * with socket_connect_begin. It stays immutable until free; never freeaddrinfo
 * it or modify its links. PENDING/ERROR set addresses to NULL. */
int net_resolve_poll(net_resolve_t *request, void **addresses);
/* Drops the caller's ownership immediately, even during a system lookup.
 * A queued worker skips resolution after cancellation. An executing resolver
 * retains the job and reclaims its addresses after returning. Call once. */
void net_resolve_free(net_resolve_t *request);
RETRO_END_DECLS
#endif
