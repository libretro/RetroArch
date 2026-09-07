/* tinyalsa's write and wait loops against a device that is full, slow,
 * gone, or being signalled.
 *
 * The driver talks to the kernel through two calls, ioctl() and
 * poll(), so both are interposed with -Wl,--wrap and scripted, and the
 * struct pcm is built by hand from the fields the loops read. That
 * lets the cases reach the states no real device gives on demand:
 *
 *  - A full device in non-blocking mode refuses each write with -1
 *    (EAGAIN on the fd). The write must return what it managed and
 *    stop - not add the -1 to its count, walk its source pointer
 *    backwards past the caller's buffer, grow its remaining size and
 *    loop, which is what it did; ASan sees the backwards read.
 *  - The first write on an unstarted device takes every frame. It must
 *    report them, or the caller writes the same frames again.
 *  - A blocking write against a device that never signals must return
 *    within the bound rather than hold the thread on poll(-1).
 *  - A poll interrupted by a signal is retried, not reported.
 *  - wait_writable() against a device short of space that never
 *    signals returns 0 within the lap bound.
 *
 * A watchdog aborts any case that does not return. Includes the
 * driver's translation unit so the shipping loops are what run. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <poll.h>
#include <sys/ioctl.h>

#include <boolean.h>
#include <retro_atomic.h>

#include "../../../audio/drivers/tinyalsa.c"

/* So the suite still compiles against a driver without the bound, and
 * fails at run time where it should rather than at build time. */
#ifndef TINYALSA_WAIT_WRITABLE_LAPS
#define TINYALSA_WAIT_WRITABLE_LAPS 8
#endif

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: ", __FILE__, __LINE__); \
         printf(__VA_ARGS__); \
         printf("\n"); \
         failures++; \
      } \
   } while (0)

/* --- scripted kernel ------------------------------------------------- */

enum dev_mode
{
   DEV_ACCEPT = 0,   /* every write taken in full, poll says ready */
   DEV_FULL,         /* writes refused (EAGAIN), poll never signals */
   DEV_DEAD          /* writes fail (EIO), poll never signals */
};

static int      dev_mode        = DEV_ACCEPT;
static unsigned n_writes        = 0;
static unsigned n_polls         = 0;
static unsigned frames_taken    = 0;
static int      eintr_polls     = 0;   /* how many polls fail EINTR first */
static unsigned hw_ptr_script   = 0;   /* what SYNC_PTR reports */
static unsigned appl_ptr_script = 0;

extern int __real_ioctl(int fd, unsigned long req, ...);
extern int __real_poll(struct pollfd *fds, nfds_t n, int timeout);

int __wrap_ioctl(int fd, unsigned long req, ...)
{
   va_list ap;
   void *arg;
   (void)fd;
   va_start(ap, req);
   arg = va_arg(ap, void*);
   va_end(ap);

   switch (req)
   {
      case SNDRV_PCM_IOCTL_WRITEI_FRAMES:
      {
         struct snd_xferi *x = (struct snd_xferi*)arg;
         n_writes++;
         if (dev_mode == DEV_FULL)  { errno = EAGAIN; return -1; }
         if (dev_mode == DEV_DEAD)  { errno = EIO;    return -1; }
         x->result     = x->frames;
         frames_taken += (unsigned)x->frames;
         return 0;
      }
      case SNDRV_PCM_IOCTL_SYNC_PTR:
      {
         struct snd_pcm_sync_ptr *sp = (struct snd_pcm_sync_ptr*)arg;
         sp->s.status.hw_ptr = hw_ptr_script;
         sp->c.control.appl_ptr = appl_ptr_script;
         sp->s.status.state  = SNDRV_PCM_STATE_RUNNING;
         return 0;
      }
      default:
         /* prepare, start, pause, hw/sw params: succeed */
         return 0;
   }
}

int __wrap_poll(struct pollfd *fds, nfds_t n, int timeout)
{
   (void)n;
   n_polls++;
   if (eintr_polls > 0)
   {
      eintr_polls--;
      errno = EINTR;
      return -1;
   }
   if (dev_mode == DEV_ACCEPT)
   {
      fds[0].revents = POLLOUT;
      return 1;
   }
   /* Nothing will ever be ready: honour the timeout, so a -1 timeout
    * would sleep forever - which is exactly the case under test. */
   if (timeout < 0)
   {
      for (;;)
         sleep(1);
   }
   {
      struct timespec ts;
      ts.tv_sec  = timeout / 1000;
      ts.tv_nsec = (long)(timeout % 1000) * 1000000L;
      nanosleep(&ts, NULL);
   }
   fds[0].revents = 0;
   return 0;
}

/* --- watchdog -------------------------------------------------------- */

static retro_atomic_int_t stage = RETRO_ATOMIC_INT_INITIALIZER(0);
#define STAGE(v) retro_atomic_store_release_int(&stage, (v))

static void *watchdog(void *arg)
{
   int last = retro_atomic_load_acquire_int(&stage);
   int cur;
   (void)arg;
   for (;;)
   {
      sleep(8);
      cur = retro_atomic_load_acquire_int(&stage);
      if (cur == last && cur >= 0)
      {
         fprintf(stderr, "WATCHDOG: stage %d never returned\n", cur);
         abort();
      }
      last = cur;
      if (cur < 0)
         return NULL;
   }
}

static double now_ms(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

/* --- fixture --------------------------------------------------------- */

static struct snd_pcm_sync_ptr sync_storage;

static void make_device(tinyalsa_t *ta, struct pcm *pcm, bool nonblock,
      bool running)
{
   memset(ta,  0, sizeof(*ta));
   memset(pcm, 0, sizeof(*pcm));
   memset(&sync_storage, 0, sizeof(sync_storage));

   pcm->fd                 = 0;
   pcm->flags              = PCM_OUT;   /* non-blocking lives on the fd, not a flag */
   pcm->running            = running ? 1 : 0;
   pcm->prepared           = 1;
   pcm->buffer_size        = 4096;
   pcm->boundary           = 4096 * 1024;
   pcm->config.rate        = 48000;
   pcm->config.period_size = 1024;
   pcm->config.channels    = 2;
   pcm->config.format      = PCM_FORMAT_S16_LE;
   pcm->sync_ptr           = &sync_storage;
   pcm->mmap_status        = &sync_storage.s.status;
   pcm->mmap_control       = &sync_storage.c.control;

   ta->pcm         = pcm;
   ta->buffer_size = pcm->buffer_size * 4;   /* bytes, s16 stereo */
   ta->frame_bits  = 32;
   ta->nonblock    = nonblock;
   ta->has_float   = false;
}

static void reset_script(int mode)
{
   dev_mode        = mode;
   n_writes        = 0;
   n_polls         = 0;
   frames_taken    = 0;
   eintr_polls     = 0;
   hw_ptr_script   = 0;
   appl_ptr_script = 0;
}

int main(void)
{
   pthread_t dog;
   tinyalsa_t ta;
   struct pcm pcm;
   /* Heap-allocated so a read before its start is a real ASan
    * finding, not a silent stack read. */
   size_t   src_bytes = 512 * 4;
   uint8_t *src       = (uint8_t*)malloc(src_bytes);
   ssize_t  got;
   double   t0, t1;

   memset(src, 0x5a, src_bytes);
   pthread_create(&dog, NULL, watchdog, NULL);

   /* 1. Non-blocking, device full: returns what it managed (nothing),
    *    once, and does not touch memory outside src. */
   STAGE(1);
   make_device(&ta, &pcm, true, true);
   reset_script(DEV_FULL);
   got = tinyalsa_write(&ta, src, src_bytes);
   CHECK(got == 0, "full non-blocking device: expected 0 written, got %ld", (long)got);
   CHECK(n_writes == 1, "full non-blocking device: expected 1 write attempt, saw %u", n_writes);

   /* 2. Non-blocking, device accepting: everything goes, reported. */
   STAGE(2);
   make_device(&ta, &pcm, true, true);
   reset_script(DEV_ACCEPT);
   got = tinyalsa_write(&ta, src, src_bytes);
   CHECK(got == 512, "accepting device: expected 512 frames, got %ld", (long)got);
   CHECK(frames_taken == 512, "accepting device: kernel took %u frames", frames_taken);

   /* 3. First write on an unstarted device: every frame taken, and
    *    every frame reported - exactly one kernel write. */
   STAGE(3);
   make_device(&ta, &pcm, false, false);
   reset_script(DEV_ACCEPT);
   got = tinyalsa_write(&ta, src, src_bytes);
   CHECK(got == 512, "first write: expected 512 frames reported, got %ld", (long)got);
   CHECK(n_writes == 1, "first write: the opening chunk was written %u times", n_writes);
   CHECK(pcm.running == 1, "first write: device not marked running");

   /* 4. Blocking, device never signals: returns within the bound. */
   STAGE(4);
   make_device(&ta, &pcm, false, true);
   reset_script(DEV_FULL);
   t0  = now_ms();
   got = tinyalsa_write(&ta, src, src_bytes);
   t1  = now_ms();
   CHECK(got == 0, "blocking write on a stalled device: expected 0, got %ld", (long)got);
   CHECK(t1 - t0 < 2000.0, "blocking write on a stalled device: %f ms is not bounded", t1 - t0);
   CHECK(n_polls >= 1, "blocking write did not wait for the device");

   /* 5. Blocking, a signal interrupts the first two polls, then the
    *    device is ready: the write completes. */
   STAGE(5);
   make_device(&ta, &pcm, false, true);
   reset_script(DEV_ACCEPT);
   eintr_polls = 2;
   got = tinyalsa_write(&ta, src, src_bytes);
   CHECK(got == 512, "EINTR: expected 512 frames, got %ld", (long)got);
   CHECK(n_polls >= 3, "EINTR: polls were not retried (%u polls)", n_polls);

   /* 6. Blocking, device gone (EIO): reported as an error, not looped. */
   STAGE(6);
   make_device(&ta, &pcm, false, true);
   reset_script(DEV_DEAD);
   dev_mode = DEV_ACCEPT;      /* let poll say ready ... */
   {
      /* ... but make the write fail: script the write arm only */
      got = -12345;
   }
   dev_mode = DEV_DEAD;
   /* poll never signals in DEV_DEAD, so this is bounded by the wait */
   t0  = now_ms();
   got = tinyalsa_write(&ta, src, src_bytes);
   t1  = now_ms();
   CHECK(got <= 0, "dead device: expected nothing written or -1, got %ld", (long)got);
   CHECK(t1 - t0 < 2000.0, "dead device: %f ms is not bounded", t1 - t0);

   /* 7. wait_writable with less space than asked for and a device that
    *    never signals: returns 0 within the lap bound. */
   STAGE(7);
   make_device(&ta, &pcm, false, true);
   reset_script(DEV_FULL);
   hw_ptr_script   = 0;
   appl_ptr_script = 4096 - 16;   /* 16 frames free */
   t0  = now_ms();
   CHECK(tinyalsa_wait_writable(&ta, 1024 * 4) == 0,
         "wait_writable on a stalled device did not return 0");
   t1  = now_ms();
   CHECK(t1 - t0 < 3000.0, "wait_writable on a stalled device: %f ms is not bounded", t1 - t0);
   CHECK(n_polls <= TINYALSA_WAIT_WRITABLE_LAPS + 1,
         "wait_writable: %u polls exceeds the lap bound", n_polls);

   /* 8. wait_writable with space already free: returns it at once. */
   STAGE(8);
   make_device(&ta, &pcm, false, true);
   reset_script(DEV_FULL);
   hw_ptr_script   = 0;
   appl_ptr_script = 0;           /* whole buffer free */
   CHECK(tinyalsa_wait_writable(&ta, 1024 * 4) == 4096 * 4,
         "wait_writable with a free buffer did not report it");
   CHECK(n_polls == 0, "wait_writable polled a device that already had space");

   STAGE(-1);
   free(src);
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("tinyalsa write/wait: full, first, stalled, interrupted and dead devices all return\n");
   return 0;
}
