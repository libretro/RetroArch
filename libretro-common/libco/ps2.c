#define LIBCO_C
#include "libco.h"

#include <stdlib.h>
#include <stdint.h>
#include <kernel.h>

/* Since cothread_t is a void pointer it must contain an address. We can't return a reference to a local variable
 * because it would go out of scope, so we create a static variable instead so we can return a reference to it.
 */
static int32_t active_thread_id = -1;
extern void *_gp;

cothread_t co_active()
{
  active_thread_id = GetThreadId();
  return &active_thread_id;
}

cothread_t co_create(unsigned int size, void (*entrypoint)(void))
{
   /* Similar scenario as with active_thread_id except there will only be one active_thread_id while there could be many
    * new threads each with their own handle, so we create them on the heap instead and delete them manually when they're
    * no longer needed in co_delete().
    */
   ee_thread_t thread;
   int32_t new_thread_id;
   cothread_t handle       = malloc(sizeof(cothread_t));
   void *threadStack       = (void *)malloc(size);

   if (!threadStack)
      return -1;

   thread.stack_size		   = size;
   thread.gp_reg			   = &_gp;
   thread.func				   = (void *)entrypoint;
   thread.stack			   = threadStack;
   thread.option			   = 0;
   thread.initial_priority = 1;

   new_thread_id           = CreateThread(&thread);
   StartThread(new_thread_id, NULL);
   *(uint32_t *)handle     = new_thread_id;
   return handle;
}

void co_delete(cothread_t handle)
{
   TerminateThread(*(uint32_t *)handle);
   DeleteThread(*(uint32_t *)handle);
   free(handle);
}

void co_switch(cothread_t handle)
{
   WakeupThread(*(uint32_t *)handle);
   /* Sleep the currently active thread so the new thread can start */
   SleepThread();
}
