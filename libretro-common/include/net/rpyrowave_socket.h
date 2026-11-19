/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef LIBRETRO_COMMON_RPYROWAVE_SOCKET_H
#define LIBRETRO_COMMON_RPYROWAVE_SOCKET_H
#include <stdint.h>
#include <formats/rpyrowave_stream.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS
typedef struct rpyrowave_socket rpyrowave_socket_t;
typedef struct rpyrowave_endpoint rpyrowave_endpoint_t;

enum rpyrowave_endpoint_result
{
   RPYROWAVE_ENDPOINT_ERROR = -1,
   RPYROWAVE_ENDPOINT_PENDING,
   RPYROWAVE_ENDPOINT_READY
};

enum rpyrowave_endpoint_readiness
{
   RPYROWAVE_ENDPOINT_READABLE = 1,
   RPYROWAVE_ENDPOINT_WRITABLE = 2,
   RPYROWAVE_ENDPOINT_EXCEPTION = 4
};

/* Cold startup after network initialization. address is a native IPv4/IPv6
 * sockaddr, borrowed only during this call, never a hostname. listen=1 binds
 * one peer; listen=0 starts one nonblocking connect. All storage is allocated
 * here. No resolver, clock, thread, retry loop or wait is hidden inside.
 * One owner serializes all calls and unregisters readiness before freeing.
 * The caller owns admission policy, readiness registration and cancellation. */
rpyrowave_endpoint_t *rpyrowave_endpoint_new(const void *address,
      size_t address_bytes, unsigned listen);
/* Register READABLE for a listener or WRITABLE/EXCEPTION for a connector.
 * step(0) can collect an immediately completed connect. Otherwise step only
 * on reported readiness. READY transfers one connected adapter; no allocation
 * occurs in step. ERROR and READY are terminal; never step after either.
 * PENDING/ERROR clear connected and consume no caller-owned socket. */
int rpyrowave_endpoint_step(rpyrowave_endpoint_t *endpoint, unsigned readiness,
      rpyrowave_socket_t **connected);
/* Readiness registration and getsockname only. Invalid after READY/ERROR. */
uintptr_t rpyrowave_endpoint_handle(const rpyrowave_endpoint_t *endpoint);
/* Cancels pending startup. A previously transferred adapter stays owned by
 * its recipient. No stream may still use an adapter when that adapter frees. */
void rpyrowave_endpoint_free(rpyrowave_endpoint_t *endpoint);

/* Cold adoption of a connected TCP socket, after platform network init.
 * Use uintptr_t to preserve Winsock's native handle width. Success transfers
 * ownership and sets nonblocking I/O, TCP_NODELAY and nonblocking close.
 * Failure consumes nothing. Socket settings may already have changed.
 * Resolution stays with the frontend's startup state machine. */
rpyrowave_socket_t *rpyrowave_socket_new(uintptr_t handle);
void rpyrowave_socket_io(rpyrowave_socket_t *socket, rpyrowave_stream_io_t *io);
/* For readiness registration only; do not change the owned socket's flags. */
uintptr_t rpyrowave_socket_handle(const rpyrowave_socket_t *socket);
/* Free the stream using this adapter first, so its final lends are retired. */
void rpyrowave_socket_free(rpyrowave_socket_t *socket);
RETRO_END_DECLS
#endif
