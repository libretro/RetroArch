/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <retro_posix_source.h>
#include <net/net_resolve.h>
#include <net/net_socket.h>
#include <retro_atomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <compat/msvc.h>

struct net_resolve
{
   struct addrinfo *addresses;
   char *hostname;
   retro_atomic_int_t references;
   retro_atomic_int_t canceled;
   retro_atomic_int_t result;
   unsigned port;
};

static retro_atomic_int_t net_resolve_live;

static void net_resolve_release(net_resolve_t *request)
{
   if (retro_atomic_fetch_sub_int(&request->references, 1) == 1)
   {
      if (request->addresses)
         freeaddrinfo_retro(request->addresses);
      free(request);
      retro_atomic_fetch_sub_int(&net_resolve_live, 1);
   }
}

static void net_resolve_work(void *data)
{
   net_resolve_t *request = (net_resolve_t*)data;
   struct addrinfo hints = {0};
   struct addrinfo *addresses = NULL;
   char service[6];
   int result = NET_RESOLVE_ERROR;
   if (!retro_atomic_load_acquire_int(&request->canceled))
   {
#if defined(HAVE_SOCKET_LEGACY) || defined(WIIU)
      hints.ai_family = AF_INET;
#else
      hints.ai_family = AF_UNSPEC;
#endif
      hints.ai_socktype = SOCK_STREAM;
      hints.ai_flags = AI_NUMERICSERV;
      snprintf(service, sizeof(service), "%u", request->port);
      if (!getaddrinfo_retro(request->hostname, service, &hints, &addresses)
            && addresses)
         result = NET_RESOLVE_READY;
   }
   request->addresses = addresses;
   retro_atomic_store_release_int(&request->result, result);
   net_resolve_release(request);
}

net_resolve_t *net_resolve_new(const char *hostname, unsigned port,
      net_resolve_dispatch_t dispatch, void *userdata)
{
   net_resolve_t *request;
   size_t length;
   int live;
   if (!hostname || !port || port > 65535 || !dispatch)
      return NULL;
   for (length = 0; length < 1024 && hostname[length]; length++)
   {
   }
   if (!length || length == 1024)
      return NULL;
   live = retro_atomic_load_acquire_int(&net_resolve_live);
   if (live >= 4 || !retro_atomic_cas_int(&net_resolve_live, live, live + 1))
      return NULL;
   request = (net_resolve_t*)calloc(1, sizeof(*request) + length + 1);
   if (!request)
   {
      retro_atomic_fetch_sub_int(&net_resolve_live, 1);
      return NULL;
   }
   request->port = port;
   request->hostname = (char*)(request + 1);
   memcpy(request->hostname, hostname, length + 1);
   retro_atomic_store_release_int(&request->references, 2);
   if (!dispatch(net_resolve_work, request, userdata))
   {
      net_resolve_release(request);
      net_resolve_release(request);
      return NULL;
   }
   return request;
}

int net_resolve_poll(net_resolve_t *request, void **addresses)
{
   int result;
   if (!addresses)
      return NET_RESOLVE_ERROR;
   *addresses = NULL;
   if (!request)
      return NET_RESOLVE_ERROR;
   result = retro_atomic_load_acquire_int(&request->result);
   if (result == NET_RESOLVE_READY)
      *addresses = request->addresses;
   return result;
}

void net_resolve_free(net_resolve_t *request)
{
   if (!request)
      return;
   retro_atomic_store_release_int(&request->canceled, 1);
   net_resolve_release(request);
}
