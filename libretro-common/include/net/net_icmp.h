/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction.  See the
 * license block in any neighboring header for the full text; it applies
 * to this file (net_icmp.h) identically.
 */

#ifndef _LIBRETRO_NET_ICMP_H
#define _LIBRETRO_NET_ICMP_H

#include <stdint.h>
#include <stddef.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Non-blocking IPv4 ICMP echo, one request in flight per handle.
 * Unprivileged datagram ICMP on Linux/macOS (raw where permitted),
 * IcmpSendEcho2 on Windows, stub elsewhere whose open() fails: callers
 * treat open failure or a silent reply exactly like an unreachable
 * host, so the capability degrades cleanly everywhere. */

typedef struct net_icmp_echo net_icmp_echo_t;

/* dest_addr: IPv4 destination, network byte order. NULL on failure,
 * including environments without ICMP access. */
net_icmp_echo_t *net_icmp_echo_open(uint32_t dest_addr);

/* One echo request; seq tags it and returns from poll. Payload under
 * 1400 bytes. -1 if a request is already in flight. */
int net_icmp_echo_send(net_icmp_echo_t *echo, uint16_t seq,
      const void *payload, size_t len);

/* 1 = reply (seq and ttl filled; ttl may be NULL); 0 = waiting;
 * -1 = error or destination unreachable. */
int net_icmp_echo_poll(net_icmp_echo_t *echo, uint16_t *seq, int *ttl);

void net_icmp_echo_close(net_icmp_echo_t *echo);

RETRO_END_DECLS

#endif
