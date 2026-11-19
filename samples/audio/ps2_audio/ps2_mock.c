/* ps2sdk on the host: kernel semaphores on a mutex and condition
 * variable, alarms on a thread each, and audsrv's ring drained by an
 * "IOP" thread one 512-sample block at a time, which sends the
 * fill-buffer callback after each block while the threshold is free -
 * as the IOP side does. Interrupts off is one global lock the alarm
 * thread takes around its handler. */
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "kernel.h"
#include "audsrv.h"
#include "ps2_mock.h"

#define MAX_SEMAS 8
static struct { int live, count, max; } semas[MAX_SEMAS];
static pthread_mutex_t sem_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  sem_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t intr     = PTHREAD_MUTEX_INITIALIZER;

static unsigned waits_, alarms_rung_, alarms_released_;
static int      sema_live_;

s32 CreateSema(ee_sema_t *s)
{
   int i;
   pthread_mutex_lock(&sem_lock);
   for (i = 0; i < MAX_SEMAS; i++)
      if (!semas[i].live)
      {
         semas[i].live  = 1;
         semas[i].count = s->init_count;
         semas[i].max   = s->max_count;
         sema_live_++;
         pthread_mutex_unlock(&sem_lock);
         return i + 1;
      }
   pthread_mutex_unlock(&sem_lock);
   return -1;
}
s32 DeleteSema(s32 id)
{
   pthread_mutex_lock(&sem_lock);
   if (id > 0 && id <= MAX_SEMAS && semas[id - 1].live)
   {
      semas[id - 1].live = 0;
      sema_live_--;
   }
   pthread_cond_broadcast(&sem_cond);
   pthread_mutex_unlock(&sem_lock);
   return 0;
}
s32 SignalSema(s32 id)
{
   s32 r = -1;
   pthread_mutex_lock(&sem_lock);
   if (id > 0 && id <= MAX_SEMAS && semas[id - 1].live
         && semas[id - 1].count < semas[id - 1].max)
   {
      semas[id - 1].count++;
      r = 0;
   }
   pthread_cond_broadcast(&sem_cond);
   pthread_mutex_unlock(&sem_lock);
   return r;
}
s32 iSignalSema(s32 id) { return SignalSema(id); }
s32 WaitSema(s32 id)
{
   pthread_mutex_lock(&sem_lock);
   waits_++;
   while (semas[id - 1].live && !semas[id - 1].count)
      pthread_cond_wait(&sem_cond, &sem_lock);
   if (semas[id - 1].count)
      semas[id - 1].count--;
   pthread_mutex_unlock(&sem_lock);
   return id;
}
s32 PollSema(s32 id)
{
   s32 r = -1;
   pthread_mutex_lock(&sem_lock);
   if (semas[id - 1].live && semas[id - 1].count)
   {
      semas[id - 1].count--;
      r = id;
   }
   pthread_mutex_unlock(&sem_lock);
   return r;
}

int DIntr(void) { pthread_mutex_lock(&intr); return 1; }
int EIntr(void) { pthread_mutex_unlock(&intr); return 1; }

/* One alarm at a time */
static struct
{
   pthread_t thread;
   int       armed, released;
   unsigned  usec;
   void    (*cb)(s32, u16, void*);
   void     *arg;
} alarm_;

static void *alarm_thread(void *p)
{
   struct timespec ts;
   (void)p;
   ts.tv_sec  = alarm_.usec / 1000000;
   ts.tv_nsec = (long)(alarm_.usec % 1000000) * 1000;
   nanosleep(&ts, NULL);
   pthread_mutex_lock(&intr);
   if (!alarm_.released)
   {
      alarms_rung_++;
      alarm_.cb(1, 0, alarm_.arg);
   }
   pthread_mutex_unlock(&intr);
   return NULL;
}
s32 SetAlarm(u16 t, void (*cb)(s32, u16, void*), void *arg)
{
   if (alarm_.armed)
      pthread_join(alarm_.thread, NULL);
   alarm_.armed    = 1;
   alarm_.released = 0;
   alarm_.usec     = (unsigned)t * 64; /* H-SYNC ticks */
   alarm_.cb       = cb;
   alarm_.arg      = arg;
   pthread_create(&alarm_.thread, NULL, alarm_thread, NULL);
   return 1;
}
s32 iReleaseAlarm(s32 id)
{
   (void)id;
   alarm_.released = 1;
   alarms_released_++;
   return 0;
}
void mock_alarm_join(void)
{
   if (alarm_.armed)
      pthread_join(alarm_.thread, NULL);
   alarm_.armed = 0;
}

/* audsrv: a ring the IOP thread drains while running */
#define RING 16384
static pthread_mutex_t a_lock = PTHREAD_MUTEX_INITIALIZER;
static int  queued, iop_run, draining = 1, threshold;
static audsrv_callback_t fill_cb;
static void *fill_arg;
static pthread_t iop;
static unsigned callbacks_;

static void *iop_thread(void *p)
{
   (void)p;
   for (;;)
   {
      struct timespec ts = { 0, 2000000 }; /* a block every 2 ms */
      audsrv_callback_t cb = NULL;
      void *arg = NULL;
      nanosleep(&ts, NULL);
      pthread_mutex_lock(&a_lock);
      if (!iop_run)
      {
         pthread_mutex_unlock(&a_lock);
         break;
      }
      if (draining)
      {
         queued -= 2048;
         if (queued < 0)
            queued = 0;
         if (threshold > 0 && RING - queued >= threshold && fill_cb)
         {
            cb  = fill_cb;
            arg = fill_arg;
            callbacks_++;
         }
      }
      pthread_mutex_unlock(&a_lock);
      if (cb)
         cb(arg); /* on the RPC server thread, outside audsrv's state */
   }
   return NULL;
}
void mock_iop_start(void)
{
   iop_run = 1;
   pthread_create(&iop, NULL, iop_thread, NULL);
}
void mock_iop_stop(void)
{
   pthread_mutex_lock(&a_lock);
   iop_run = 0;
   pthread_mutex_unlock(&a_lock);
   pthread_join(iop, NULL);
}
void mock_set_draining(int on)
{
   pthread_mutex_lock(&a_lock);
   draining = on;
   pthread_mutex_unlock(&a_lock);
}
int mock_fillbuf_threshold(void) { return threshold; }

int audsrv_set_format(struct audsrv_fmt_t *f) { (void)f; return 0; }
int audsrv_set_volume(int v) { (void)v; return 0; }
int audsrv_available(void)
{
   int r;
   pthread_mutex_lock(&a_lock);
   r = RING - queued;
   pthread_mutex_unlock(&a_lock);
   return r;
}
int audsrv_queued(void)
{
   int r;
   pthread_mutex_lock(&a_lock);
   r = queued;
   pthread_mutex_unlock(&a_lock);
   return r;
}
int audsrv_play_audio(const char *c, int b)
{
   (void)c;
   pthread_mutex_lock(&a_lock);
   if (b > RING - queued)
      b = RING - queued;
   queued += b;
   pthread_mutex_unlock(&a_lock);
   return b;
}
int audsrv_stop_audio(void) { return 0; }
int audsrv_on_fillbuf(int amount, audsrv_callback_t cb, void *arg)
{
   pthread_mutex_lock(&a_lock);
   threshold = amount;
   fill_cb   = cb;
   fill_arg  = arg;
   pthread_mutex_unlock(&a_lock);
   return 0;
}

/* The counts, each read and reset under the lock its writer holds */
unsigned mock_waits(void) { unsigned v; pthread_mutex_lock(&sem_lock); v = waits_; pthread_mutex_unlock(&sem_lock); return v; }
int      mock_sema_live(void) { int v; pthread_mutex_lock(&sem_lock); v = sema_live_; pthread_mutex_unlock(&sem_lock); return v; }
unsigned mock_alarms_rung(void) { unsigned v; pthread_mutex_lock(&intr); v = alarms_rung_; pthread_mutex_unlock(&intr); return v; }
unsigned mock_callbacks(void) { unsigned v; pthread_mutex_lock(&a_lock); v = callbacks_; pthread_mutex_unlock(&a_lock); return v; }
void mock_reset_counts(void)
{
   pthread_mutex_lock(&sem_lock); waits_ = 0; pthread_mutex_unlock(&sem_lock);
   pthread_mutex_lock(&intr); alarms_rung_ = 0; pthread_mutex_unlock(&intr);
   pthread_mutex_lock(&a_lock); callbacks_ = 0; pthread_mutex_unlock(&a_lock);
}
void mock_empty_ring(void) { pthread_mutex_lock(&a_lock); queued = 0; pthread_mutex_unlock(&a_lock); }
