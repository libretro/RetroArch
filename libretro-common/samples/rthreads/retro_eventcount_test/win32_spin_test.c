/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#if defined(_WIN32)
#include <windows.h>
#include <rthreads/retro_eventcount.h>
#include <retro_miscellaneous.h>

static void native_relax(void)
{
   retro_cpu_relax();
}
static void notify_during_spin(void);
static int native_pointer_cas(retro_atomic_ptr_t *address,
      void *expected, void *desired)
{
   return retro_atomic_cas_ptr(address, expected, desired);
}
static int publish_during_detach(retro_atomic_ptr_t *address,
      void *expected, void *desired);
#undef retro_cpu_relax
#define retro_cpu_relax() notify_during_spin()
#undef retro_atomic_cas_ptr
#define retro_atomic_cas_ptr(p, e, d) publish_during_detach(p, e, d)
#include "../../../rthreads/retro_eventcount.c"
#undef retro_cpu_relax
#undef retro_atomic_cas_ptr

#if defined(RETRO_EC_ADDR_WIN32)
static retro_eventcount_t *notify_target, *push_target, *unlock_target;
static struct ec_waiter *push_node;
static unsigned failures, notifications, pushes, unlocks;
#endif

static int publish_during_detach(retro_atomic_ptr_t *address,
      void *expected, void *desired)
{
#if defined(RETRO_EC_ADDR_WIN32)
   if (push_target && address == &push_target->waitlist && !desired)
   {
      retro_eventcount_t *target = push_target;
      push_target = NULL;
      pushes++;
      ec_list_push(target, push_node);
   }
#endif
   return native_pointer_cas(address, expected, desired);
}

static void notify_during_spin(void)
{
#if defined(RETRO_EC_ADDR_WIN32)
   if (notify_target)
   {
      retro_eventcount_t *target = notify_target;
      notify_target = NULL;
      notifications++;
      if (retro_atomic_load_acquire_ptr(&target->waitlist))
         failures++;
      retro_eventcount_notify(target);
   }
   if (unlock_target)
   {
      retro_eventcount_t *target = unlock_target;
      unlock_target = NULL;
      unlocks++;
      if (!(ec_head(target) & EC_HEAD_LOCK))
         failures++;
      ec_list_unlock(target);
   }
#endif
   native_relax();
}

int main(void)
{
#if defined(RETRO_EC_ADDR_WIN32)
   retro_eventcount_t ec;
   struct ec_waiter first, second;
   unsigned i;
   if (!retro_eventcount_init(&ec)) return 1;
   if (ec.cond)
   {
      retro_eventcount_free(&ec);
      puts("skip: Windows waiter-list backend unavailable");
      return 0;
   }
   ec_g.spin = 8;
   for (i = 0; i < 2; i++)
   {
      int key = retro_eventcount_prepare_wait(&ec);
      notify_target = &ec;
      if (i)
      {
         if (!retro_eventcount_commit_wait_timeout(&ec, key, 1000))
            failures++;
      }
      else
         retro_eventcount_commit_wait(&ec, key);
      if (notify_target || retro_atomic_load_acquire_int(&ec.waiters)
            || retro_atomic_load_acquire_ptr(&ec.waitlist))
         failures++;
   }
   if (ec_g.sleep == EC_SLEEP_EVENT || ec_g.hires)
      if (TlsGetValue(ec_g.tls_event))
         failures++;
   if (ec_g.hires && TlsGetValue(ec_g.tls_timer))
      failures++;
   if (notifications != 2)
      failures++;

   memset(&first, 0, sizeof(first));
   memset(&second, 0, sizeof(second));
   retro_atomic_int_init(&first.flags, 0);
   retro_atomic_int_init(&second.flags, 0);
   ec_list_push(&ec, &first);
   push_target = &ec;
   push_node = &second;
   ec_wake_all(&ec);
   if (push_target || pushes != 1 || ec_head(&ec)
         || retro_atomic_load_acquire_int(&first.flags) != EC_W_WOKEN
         || retro_atomic_load_acquire_int(&second.flags) != EC_W_WOKEN)
      failures++;

   retro_atomic_store_relaxed_int(&first.flags, 0);
   ec_list_push(&ec, &first);
   ec_list_lock(&ec);
   unlock_target = &ec;
   ec_wake_all(&ec);
   if (unlock_target || unlocks != 1 || ec_head(&ec)
         || retro_atomic_load_acquire_int(&first.flags) != EC_W_WOKEN)
      failures++;
   retro_eventcount_free(&ec);
   if (!failures)
      puts("PASS Windows short handoffs and wake-list publication races");
   return failures != 0;
#else
   (void)notify_during_spin;
   (void)publish_during_detach;
   return 0;
#endif
}
#else
int main(void)
{
   puts("skip: Windows spin handoff test");
   return 0;
}
#endif
