#include <rthreads/rthreads.h>

uintptr_t sthread_get_thread_id(sthread_t *thread)
{
   return thread ? 1 : 0;
}

void slock_unlock(slock_t *lock)
{
}

void slock_lock(slock_t *lock)
{
}

void slock_free(slock_t *lock)
{
}

slock_t *slock_new(void)
{
   static uintptr_t ctr = 1;
   return (slock_t*)ctr++;
}
