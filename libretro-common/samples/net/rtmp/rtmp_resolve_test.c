/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <net/net_resolve.h>
#include <net/net_socket.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct work_item
{
   void (*work)(void*);
   void *job;
};

static struct work_item work[4];
static unsigned queued, lookups, releases;
static bool reject, lookup_failed, allocation_failed;
static net_resolve_t *cancel_in_lookup;
static const char *expected_host = "localhost";

static int test_getaddrinfo(const char *host, const char *service,
      struct addrinfo *hints, struct addrinfo **result)
{
   lookups++;
   assert(!strcmp(host, expected_host) && !strcmp(service, "1935"));
   assert(hints->ai_socktype == SOCK_STREAM);
   assert(hints->ai_flags & AI_NUMERICSERV);
   if (cancel_in_lookup)
   {
      net_resolve_free(cancel_in_lookup);
      cancel_in_lookup = NULL;
   }
   *result = (struct addrinfo*)calloc(1, sizeof(**result));
   assert(*result);
   return lookup_failed ? -1 : 0;
}

static void test_freeaddrinfo(struct addrinfo *result)
{
   assert(result);
   releases++;
   free(result);
}

static void *test_calloc(size_t count, size_t size)
{
   return allocation_failed ? NULL : calloc(count, size);
}

#define calloc test_calloc
#define getaddrinfo_retro test_getaddrinfo
#define freeaddrinfo_retro test_freeaddrinfo
#include "../../../net/net_resolve.c"
#undef calloc

static bool dispatch(void (*fn)(void*), void *job, void *data)
{
   assert(data == work);
   if (reject)
      return false;
   assert(queued < 4);
   work[queued].work = fn;
   work[queued++].job = job;
   return true;
}

static void drain(void)
{
   unsigned i, count = queued;
   queued = 0;
   for (i = 0; i < count; i++)
      work[i].work(work[i].job);
}

int main(void)
{
   net_resolve_t *request, *requests[4];
   void *addresses = NULL, *again;
   char host[16] = "localhost";
   char oversized[1024];
   unsigned i, count;
   request = net_resolve_new(host, 1935, dispatch, work);
   assert(request && queued == 1 && !lookups);
   memset(host, 'x', sizeof(host));
   assert(net_resolve_poll(request, &addresses) == NET_RESOLVE_PENDING);
   assert(!addresses && !lookups);
   drain();
   assert(net_resolve_poll(request, &addresses) == NET_RESOLVE_READY);
   assert(addresses && lookups == 1 && !releases);
   assert(net_resolve_poll(request, &again) == NET_RESOLVE_READY);
   assert(again == addresses);
   net_resolve_free(request);
   assert(releases == 1);

   count = lookups;
   for (i = 0; i < 4; i++)
   {
      requests[i] = net_resolve_new("localhost", 1935, dispatch, work);
      assert(requests[i]);
      net_resolve_free(requests[i]);
   }
   assert(!net_resolve_new("localhost", 1935, dispatch, work));
   drain();
   assert(lookups == count);
   request = net_resolve_new("localhost", 1935, dispatch, work);
   assert(request);
   cancel_in_lookup = request;
   drain();
   assert(!cancel_in_lookup && releases == 2);

   lookup_failed = true;
   request = net_resolve_new("localhost", 1935, dispatch, work);
   assert(request);
   drain();
   assert(net_resolve_poll(request, &addresses) == NET_RESOLVE_ERROR);
   assert(!addresses);
   net_resolve_free(request);
   assert(releases == 3);
   lookup_failed = false;
   reject = true;
   for (i = 0; i < 8; i++)
      assert(!net_resolve_new("localhost", 1935, dispatch, work));
   reject = false;
   allocation_failed = true;
   for (i = 0; i < 8; i++)
      assert(!net_resolve_new("localhost", 1935, dispatch, work));
   allocation_failed = false;
   memset(oversized, 'x', sizeof(oversized));
   assert(!net_resolve_new(oversized, 1935, dispatch, work));
   assert(!net_resolve_new(NULL, 1935, dispatch, work));
   assert(!net_resolve_new("", 1935, dispatch, work));
   assert(!net_resolve_new("localhost", 0, dispatch, work));
   assert(!net_resolve_new("localhost", 65536, dispatch, work));
   assert(!net_resolve_new("localhost", 1935, NULL, work));
   assert(net_resolve_poll(NULL, &addresses) == NET_RESOLVE_ERROR);
   assert(net_resolve_poll(NULL, NULL) == NET_RESOLVE_ERROR);
   assert(!addresses);
   net_resolve_free(NULL);
   assert(!retro_atomic_load_acquire_int(&net_resolve_live));
   puts("PASS resolver ownership, cancellation, bounded admission and dispatch refusal");
   return 0;
}
