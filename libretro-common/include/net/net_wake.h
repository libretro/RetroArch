/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef LIBRETRO_NET_WAKE_H
#define LIBRETRO_NET_WAKE_H
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS

typedef struct net_wake net_wake_t;
/* Cold setup. POSIX nonblocking, close-on-exec pipe; NULL if unavailable. */
net_wake_t *net_wake_new(void);
/* Cold duplicate for an asynchronous notifier that can outlive its owner.
 * Keeps both pipe ends alive; only one reference may consume notifications. */
net_wake_t *net_wake_dup(const net_wake_t *wake);
/* Borrowed until free; never close it. An I/O worker may poll this descriptor
 * together with sockets. The queue owner still owns its wake/recheck epoch. */
int net_wake_read_fd(const net_wake_t *wake);
/* Producer: one nonblocking write. A full pipe already carries a pending
 * wake and counts as success. Publish queue state before notifying. */
bool net_wake_notify(net_wake_t *wake);
/* One I/O consumer: bounded nonblocking acknowledgement after readability.
 * Spurious readiness or EINTR consumes nothing and is not an error. */
bool net_wake_ack(net_wake_t *wake);
/* Cold owner, all producers and the I/O consumer quiesced. */
void net_wake_free(net_wake_t *wake);

RETRO_END_DECLS
#endif
