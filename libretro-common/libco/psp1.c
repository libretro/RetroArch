#define LIBCO_C
#include "libco.h"

#include <stdlib.h>
#include <pspthreadman.h>

/* Since cothread_t is a void pointer it must contain an address. We can't return a reference to a local variable
 * because it would go out of scope, so we create a static variable instead so we can return a reference to it.
 */
static SceUID active_thread_id = 0;

cothread_t co_active()
{
  active_thread_id = sceKernelGetThreadId();
  return &active_thread_id;
}

cothread_t co_create(unsigned int size, void (*entrypoint)(void))
{
  /* Similar scenario as with active_thread_id except there will only be one active_thread_id while there could be many
   * new threads each with their own handle, so we create them on the heap instead and delete them manually when they're
   * no longer needed in co_delete().
   */
  cothread_t handle = malloc(sizeof(cothread_t));

  /* SceKernelThreadEntry has a different signature than entrypoint, but in practice this seems to work */
  SceUID new_thread_id = sceKernelCreateThread("cothread", (SceKernelThreadEntry)entrypoint, 0x12, size, 0, NULL);
  sceKernelStartThread(new_thread_id, 0, NULL);

  *(SceUID *)handle = new_thread_id;
  return handle;
}

void co_delete(cothread_t handle)
{
  sceKernelTerminateDeleteThread(*(SceUID *)handle);
  free(handle);
}

void co_switch(cothread_t handle)
{
  sceKernelWakeupThread(*(SceUID *)handle);
  /* Sleep the currently active thread so the new thread can start */
  sceKernelSleepThread();
}