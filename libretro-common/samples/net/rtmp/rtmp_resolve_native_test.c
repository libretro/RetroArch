/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <net/net_resolve.h>
#include <net/net_socket.h>
#include <rthreads/rthreads.h>
#include <retro_miscellaneous.h>
#include <features/features_cpu.h>
#include <retro_timers.h>
#include <assert.h>
#include <stdio.h>

static bool dispatch(void (*work)(void*), void *job, void *data)
{
   sthread_t **thread = (sthread_t**)data;
   assert(!*thread);
   *thread = sthread_create(work, job);
   return *thread != NULL;
}

static void exercise(void *unused)
{
   unsigned i;
   (void)unused;
   for (i = 0; i < 64; i++)
   {
      sthread_t *thread = NULL;
      net_resolve_t *request;
      retro_time_t admission_deadline = cpu_features_get_time_usec() + 5000000;
      void *addresses = NULL;
      do
      {
         request = net_resolve_new("localhost", 1935, dispatch, &thread);
         assert(cpu_features_get_time_usec() < admission_deadline);
         if (!request)
            retro_sleep(1);
      } while (!request);
      assert(request && thread);
      if (i & 1)
      {
         retro_time_t deadline = cpu_features_get_time_usec() + 5000000;
         int result;
         do
         {
            result = net_resolve_poll(request, &addresses);
            assert(cpu_features_get_time_usec() < deadline);
            if (result == NET_RESOLVE_PENDING)
               retro_sleep(1);
         } while (result == NET_RESOLVE_PENDING);
         assert(result == NET_RESOLVE_READY && addresses);
         assert(((struct addrinfo*)addresses)->ai_socktype == SOCK_STREAM);
      }
      net_resolve_free(request);
      sthread_join(thread);
   }
}

int main(void)
{
   sthread_t *owners[4];
   unsigned i;
   assert(network_init());
   for (i = 0; i < 4; i++)
   {
      owners[i] = sthread_create(exercise, NULL);
      assert(owners[i]);
   }
   for (i = 0; i < 4; i++)
      sthread_join(owners[i]);
   puts("PASS native asynchronous DNS: 256 concurrent completion/cancellation cycles");
   return 0;
}
