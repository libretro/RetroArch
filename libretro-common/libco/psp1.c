#define LIBCO_C
#include "libco.h"

#include <stdlib.h>
#include <pspthreadman.h>

typedef void (*entrypoint_t)(void);

cothread_t co_active(void)
{
  return (void *)sceKernelGetThreadId();
}

static int thread_wrap(unsigned int argc, void *argp)
{
  entrypoint_t entrypoint = *(entrypoint_t *) argp;
  sceKernelSleepThread();
  entrypoint();
  return 0;
}

cothread_t co_create(unsigned int size, void (*entrypoint)(void))
{
  SceUID new_thread_id = sceKernelCreateThread("cothread", thread_wrap, 0x12, size, 0, NULL);
  sceKernelStartThread(new_thread_id, sizeof (entrypoint), &entrypoint);
  return (void *) new_thread_id;
}

void co_delete(cothread_t handle)
{
  SceUID id = (SceUID) handle;
  sceKernelTerminateDeleteThread(id);
}

void co_switch(cothread_t handle)
{
  SceUID id = (SceUID) handle;
  sceKernelWakeupThread(id);
  /* Sleep the currently active thread so the new thread can start */
  sceKernelSleepThread();
}
