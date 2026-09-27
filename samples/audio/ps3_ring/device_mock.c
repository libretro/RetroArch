/* The libaudio surface behind ps3_audio.c, on host primitives.
 *
 * The lightweight mutex and condition are modelled with their real
 * precondition rather than as bare wrappers: sysLwCondWait() requires
 * the caller to hold the lwcond's own mutex and fails at once with
 * EPERM otherwise. That is what turned the driver's bounded waits into
 * a few microseconds of spinning and dropped the audio, so the mock
 * refuses the same way and the suite can tell the two apart. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rthreads/rthreads.h>
#include <retro_atomic.h>

#include <audio/audio.h>
#include <sys/thread.h>
#include <sys/event_queue.h>
#include <lv2/mutex.h>
#include <lv2/cond.h>

#include "../../../audio/audio_driver.h"
#include "device_mock.h"

#define MOCK_PERIOD_US 1000

retro_atomic_size_t mock_blocks;
retro_atomic_size_t mock_silent;
retro_atomic_size_t mock_cond_eperm;

static int      port_open;
static int      port_started;

void mock_device_reset(void)
{
   retro_atomic_size_init(&mock_blocks, 0);
   retro_atomic_size_init(&mock_silent, 0);
   retro_atomic_size_init(&mock_cond_eperm, 0);
   port_open = port_started = 0;
}

/* ---- the lightweight mutex, with an owner so the cond can check it ---- */
struct mock_lwmutex
{
   slock_t *lock;
   retro_atomic_int_t held;
};

int sysLwMutexCreate(sys_lwmutex_t *mutex, const sys_lwmutex_attr_t *attr)
{
   struct mock_lwmutex *m = (struct mock_lwmutex*)calloc(1, sizeof(*m));
   (void)attr;
   if (!m || !(m->lock = slock_new()))
   {
      free(m);
      return -1;
   }
   retro_atomic_int_init(&m->held, 0);
   *mutex = (sys_lwmutex_t)(uintptr_t)m;
   return 0;
}

int sysLwMutexDestroy(sys_lwmutex_t *mutex)
{
   struct mock_lwmutex *m = (struct mock_lwmutex*)(uintptr_t)*mutex;
   if (!m)
      return -1;
   slock_free(m->lock);
   free(m);
   *mutex = 0;
   return 0;
}

int sysLwMutexLock(sys_lwmutex_t *mutex, uint32_t timeout)
{
   struct mock_lwmutex *m = (struct mock_lwmutex*)(uintptr_t)*mutex;
   (void)timeout;
   if (!m)
      return -1;
   slock_lock(m->lock);
   retro_atomic_store_release_int(&m->held, 1);
   return 0;
}

int sysLwMutexUnlock(sys_lwmutex_t *mutex)
{
   struct mock_lwmutex *m = (struct mock_lwmutex*)(uintptr_t)*mutex;
   if (!m)
      return -1;
   retro_atomic_store_release_int(&m->held, 0);
   slock_unlock(m->lock);
   return 0;
}

/* ---- the condition, tied to that mutex ---- */
struct mock_lwcond
{
   scond_t             *cond;
   struct mock_lwmutex *m;
};

int sysLwCondCreate(sys_lwcond_t *cond, sys_lwmutex_t *mutex,
      const sys_lwcond_attr_t *attr)
{
   struct mock_lwcond *c = (struct mock_lwcond*)calloc(1, sizeof(*c));
   (void)attr;
   if (!c || !(c->cond = scond_new()))
   {
      free(c);
      return -1;
   }
   c->m    = (struct mock_lwmutex*)(uintptr_t)*mutex;
   *cond   = (sys_lwcond_t)(uintptr_t)c;
   return 0;
}

int sysLwCondDestroy(sys_lwcond_t *cond)
{
   struct mock_lwcond *c = (struct mock_lwcond*)(uintptr_t)*cond;
   if (!c)
      return -1;
   scond_free(c->cond);
   free(c);
   *cond = 0;
   return 0;
}

int sysLwCondSignal(sys_lwcond_t *cond)
{
   struct mock_lwcond *c = (struct mock_lwcond*)(uintptr_t)*cond;
   if (!c)
      return -1;
   scond_signal(c->cond);
   return 0;
}

/* The precondition that matters: without the lwcond's mutex held this
 * does not sleep, it fails. Counted, so the suite can say so. */
int sysLwCondWait(sys_lwcond_t *cond, uint32_t timeout)
{
   struct mock_lwcond *c = (struct mock_lwcond*)(uintptr_t)*cond;
   if (!c)
      return -1;
   if (!retro_atomic_load_acquire_int(&c->m->held))
   {
      retro_atomic_fetch_add_size(&mock_cond_eperm, 1);
      return -1;          /* EPERM, as the real one does */
   }
   scond_wait_timeout(c->cond, c->m->lock, timeout ? timeout : 100000);
   return 0;
}

/* ---- threads ---- */
static sthread_t *worker;

int sysThreadCreate(sys_ppu_thread_t *id, void (*entry)(void *arg),
      void *arg, int prio, size_t stacksize, int flags, char *name)
{
   (void)prio; (void)stacksize; (void)flags; (void)name;
   if (!(worker = sthread_create(entry, arg)))
      return -1;
   *id = (sys_ppu_thread_t)(uintptr_t)worker;
   return 0;
}

void sysThreadExit(int code) { (void)code; }

int sysThreadJoin(sys_ppu_thread_t id, uint64_t *exitcode)
{
   (void)id;
   if (exitcode)
      *exitcode = 0;
   if (worker)
      sthread_join(worker);
   worker = NULL;
   return 0;
}

/* ---- the port, and the event the output thread waits on ---- */
int audioInit(void) { return 0; }
int audioQuit(void) { return 0; }

int audioPortOpen(audioPortParam *param, uint32_t *portNum)
{
   if (!param || (param->numChannels != 2 && param->numChannels != 6
            && param->numChannels != 8))
      return -1;
   port_open = 1;
   *portNum  = 1;
   return 0;
}

int audioPortClose(uint32_t portNum) { (void)portNum; port_open = 0; return 0; }
int audioPortStart(uint32_t portNum) { (void)portNum; port_started = 1; return 0; }
int audioPortStop(uint32_t portNum)  { (void)portNum; port_started = 0; return 0; }

int audioCreateNotifyEventQueue(sys_event_queue_t *id, sys_ipc_key_t *key)
{
   *id = 7; *key = 7;
   return 0;
}
int audioSetNotifyEventQueue(sys_ipc_key_t key)    { (void)key; return 0; }
int audioRemoveNotifyEventQueue(sys_ipc_key_t key) { (void)key; return 0; }

/* One event per period, which is what paces the output thread. */
int sysEventQueueReceive(sys_event_queue_t id, sys_event_t *event,
      uint32_t timeout)
{
   (void)id; (void)timeout;
   if (event)
      memset(event, 0, sizeof(*event));
   usleep(MOCK_PERIOD_US);
   return 0;
}

/* A block handed to the device: counted, and told apart from silence so
 * the suite can see a starved output thread. */
int audioAddData(uint32_t portNum, float *data, uint32_t frames, float volume)
{
   uint32_t i;
   int      silent = 1;
   (void)portNum; (void)volume;
   if (data)
   {
      for (i = 0; i < frames * 2; i++)
         if (data[i] != 0.0f)
         {
            silent = 0;
            break;
         }
   }
   if (silent)
      retro_atomic_fetch_add_size(&mock_silent, 1);
   retro_atomic_fetch_add_size(&mock_blocks, 1);
   return 0;
}

/* The frontend's requested layout; this suite only exercises stereo. */
uint32_t audio_driver_requested_layout(void) { return AUDIO_LAYOUT_STEREO; }
