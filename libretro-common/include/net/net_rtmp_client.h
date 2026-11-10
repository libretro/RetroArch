/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef NET_RTMP_CLIENT_H
#define NET_RTMP_CLIENT_H
#include <net/net_rtmp_socket.h>
#include <net/net_resolve.h>

typedef struct net_rtmp_client net_rtmp_client_t;
typedef struct net_rtmp_client_config
{
   const char *url;
   const uint8_t *random;
   net_resolve_dispatch_t dispatch;
   void *userdata;
   size_t max_reply;
   uint32_t time;
   unsigned streams;
   bool legacy_commands;
} net_rtmp_client_config_t;

enum net_rtmp_client_state
{
   NET_RTMP_CLIENT_FAILED = -1, NET_RTMP_CLIENT_RESOLVING,
   NET_RTMP_CLIENT_CONNECTING, NET_RTMP_CLIENT_TLS,
   NET_RTMP_CLIENT_NEGOTIATING, NET_RTMP_CLIENT_READY,
   NET_RTMP_CLIENT_DRAINING, NET_RTMP_CLIENT_CLOSING, NET_RTMP_CLIENT_FINISHED
};

typedef struct net_rtmp_client_view
{
   net_rtmp_session_t *session;
   int fd;
   bool read, write, again;
} net_rtmp_client_view_t;

RETRO_BEGIN_DECLS
/* One I/O owner, networking already initialized. Copies rtmp[s]://host/app/name
 * (up to 4096 bytes); name retains nested paths and query bytes. Supports
 * bracketed IPv6 and explicit ports; no userinfo or fragments. The resolver
 * executor owns notification/lifetime; cancellation never joins it. */
net_rtmp_client_t *net_rtmp_client_new(const net_rtmp_client_config_t *config);
/* Serializes connect capabilities before resolver dispatch; no retained caps
 * pointers. Invalid capabilities fail setup before starting any resolver. */
net_rtmp_client_t *net_rtmp_client_new_caps(const net_rtmp_client_config_t *config,
      const net_rtmp_codec_caps_t *caps);
/* One bounded setup/transport step, no wait or internal deadline. The caller
 * schedules readiness, resolver completion, retries and cancellation. again
 * requests local work; otherwise read/write name socket interests. No fd while
 * resolving. The session stays borrowed until free, including after failure,
 * so the producer can retire media after cancellation. Use media/finish only
 * after READY; do not drive its wire APIs alongside this owner. */
enum net_rtmp_client_state net_rtmp_client_step(net_rtmp_client_t *client,
      uint32_t receive_time, net_rtmp_client_view_t *view);
/* Retires transport grants and closes promptly, retaining producer borrows. */
void net_rtmp_client_cancel(net_rtmp_client_t *client);
/* Refuses active setup/output or outstanding media. Cancel first on failure;
 * normal FINISHED includes session deletion and TLS flush before socket close.
 * Plain TCP half-closes and drains replies to EOF; the caller owns its timeout. */
bool net_rtmp_client_free(net_rtmp_client_t *client);
RETRO_END_DECLS
#endif
