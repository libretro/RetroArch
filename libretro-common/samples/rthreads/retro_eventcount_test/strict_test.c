/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <rthreads/retro_eventcount.h>

int main(void)
{
   retro_eventcount_t ec;
   bool initialized = retro_eventcount_init(&ec);
#if defined(RETRO_EVENTCOUNT_FORCE_SCOND) || defined(RETRO_EVENTCOUNT_FORCE_SEM)
   if (initialized)
   {
      fputs("FAIL: strict mode accepted a lock-based backend\n", stderr);
      return 1;
   }
#elif defined(__linux__) || defined(_WIN32)
   int key;
   if (!initialized || ec.lock || ec.cond)
      return 1;
   key = retro_eventcount_prepare_wait(&ec);
   if (retro_eventcount_commit_wait_timeout(&ec, key, 0))
      return 1;
   key = retro_eventcount_prepare_wait(&ec);
   retro_eventcount_notify(&ec);
   if (!retro_eventcount_commit_wait_timeout(&ec, key, 100000))
      return 1;
#endif
   if (ec.lock || ec.cond)
      return 1;
   retro_eventcount_free(&ec);
   puts("PASS strict eventcount backend selection");
   return 0;
}
