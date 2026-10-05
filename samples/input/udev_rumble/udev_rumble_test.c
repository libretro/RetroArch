/* The udev joypad driver writes rumble from a thread of its own.
 *
 * Uploading an effect is answered by whatever is behind the pad, and
 * for a virtual pad made through uinput that is another program: the
 * frontend's thread, which used to make the call, waited for it. Now
 * the frontend's thread notes the strength wanted and returns.
 *
 * The real driver is included here and given a pad whose descriptor
 * is a pipe: the events it writes are read back from the other end,
 * and the upload ioctl is stood in for by one that can be made slow.
 * Built twice, with threads and without: without, the same code runs
 * on the caller's thread as it used to. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/input.h>

#include <boolean.h>

/* the upload, stood in for: it can take a while, as a virtual pad's
 * can, and notes what it was given */
static int      slow_ms;
/* written on the writer's thread, read on the test's: only through
 * atomic stores and N() below */
static int      uploads;
static int      upload_strong, upload_weak, upload_id_in;
static int      next_id = 1;        /* the writer's thread's alone */

static int test_ioctl(int fd, unsigned long req, void *arg)
{
   if (req == EVIOCSFF)
   {
      struct ff_effect *e = (struct ff_effect*)arg;
      if (slow_ms)
      {
         struct timespec ts;
         ts.tv_sec  = 0;
         ts.tv_nsec = slow_ms * 1000000L;
         nanosleep(&ts, NULL);
      }
      __atomic_store_n(&upload_id_in, e->id, __ATOMIC_SEQ_CST);
      if (e->id < 0)
         e->id = (short)next_id++;
      __atomic_store_n(&upload_strong, e->u.rumble.strong_magnitude, __ATOMIC_SEQ_CST);
      __atomic_store_n(&upload_weak, e->u.rumble.weak_magnitude, __ATOMIC_SEQ_CST);
      __sync_fetch_and_add(&uploads, 1);
      return 0;
   }
   errno = ENOTTY;
   return -1;
}
#define ioctl(fd, req, arg) test_ioctl((fd), (unsigned long)(req), (void*)(arg))

#include "input/drivers_joypad/udev_joypad.c"

/* ---- what the driver links against --------------------------------- */

static settings_t stub_settings;
settings_t *config_get_ptr(void) { return &stub_settings; }
/* the settings a driver asks for by name, read from the ones above */
#include "../input_config_stubs.h"
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
bool input_autoconfigure_connect(const char *name, const char *display_name,
      const char *phys, const char *driver, unsigned port, unsigned vid,
      unsigned pid)
{ (void)name; (void)display_name; (void)phys; (void)driver; (void)port; (void)vid; (void)pid; return true; }
bool input_autoconfigure_disconnect(unsigned port, const char *name)
{ (void)port; (void)name; return true; }
const char *input_config_get_device_name(unsigned port) { (void)port; return NULL; }

/* ---- the test ------------------------------------------------------- */

/* (read on this thread, written on the writer's) */
#define N(v) __atomic_load_n(&(v), __ATOMIC_SEQ_CST)

static unsigned failures;
#define CHECK(cond, ...) do { \
   if (!(cond)) { printf("   FAIL "); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

static double now_ms(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

/* The pad in slot 0: a pipe the driver writes its events into. */
static int pad_rd = -1;

static void pad_plug(void)
{
   int fds[2];
   if (pipe(fds) != 0)
      exit(2);
   pad_rd = fds[0];
   fcntl(pad_rd, F_SETFL, fcntl(pad_rd, F_GETFL) | O_NONBLOCK);
   memset(&udev_pads[0], 0, sizeof(udev_pads[0]));
   udev_pads[0].sensor_fd   = -1;
   udev_pads[0].num_effects = 16;
   udev_pad_set_fd(0, fds[1]);
   udev_rumble_attach(0, fds[1]);
}

/* Waits for one event the driver wrote to the pad. */
static bool pad_event(struct input_event *ev, int timeout_ms)
{
   struct pollfd pfd;
   pfd.fd     = pad_rd;
   pfd.events = POLLIN;
   if (poll(&pfd, 1, timeout_ms) <= 0)
      return false;
   return read(pad_rd, ev, sizeof(*ev)) == (ssize_t)sizeof(*ev);
}

static void settle(int want_uploads)
{
   int i;
   for (i = 0; i < 400 && __atomic_load_n(&uploads, __ATOMIC_SEQ_CST) < want_uploads; i++)
      usleep(5000);
   usleep(20000);
}

int main(void)
{
   struct input_event ev;
   double t0, took;
   int i, before, strong_id;

   udev_rumble_start();
#ifdef HAVE_THREADS
   CHECK(udev_rumble_writer != NULL, "the rumble writer did not start");
#endif
   pad_plug();

   /* a pad slow to answer: the call returns at once, and the strength
    * still arrives, with the effect played */
   slow_ms = 60;
   t0      = now_ms();
   CHECK(udev_set_rumble(0, RETRO_RUMBLE_STRONG, 0x8000), "set_rumble refused a pad that can rumble");
   took    = now_ms() - t0;
#ifdef HAVE_THREADS
   CHECK(took < 20.0, "the call took %.1f ms with a pad that takes 60 ms to answer", took);
#endif
   settle(1);
   CHECK(N(uploads) == 1 && N(upload_strong) == 0x8000 && N(upload_id_in) == -1,
         "the strength did not arrive: %d upload(s), strength 0x%x", N(uploads), N(upload_strong));
   CHECK(pad_event(&ev, 500) && ev.type == EV_FF && ev.value == 1,
         "the effect was not played");
   strong_id = ev.code;
#ifdef HAVE_THREADS
   printf("   ok   a pad that takes 60 ms to answer: the call returns in %.2f ms and the strength still arrives\n", took);
#else
   printf("   ok   without threads the strength is written by the caller, as before (%.0f ms)\n", took);
#endif

   /* many strengths while the pad is busy: it is given the latest, not
    * each in turn */
   before = N(uploads);
   t0     = now_ms();
   for (i = 1; i <= 20; i++)
      udev_set_rumble(0, RETRO_RUMBLE_STRONG, (uint16_t)(0x1000 + i));
   took   = now_ms() - t0;
   settle(before + 1);
   usleep(200000);
   CHECK(N(upload_strong) == 0x1000 + 20, "the last strength written is 0x%x, want 0x%x",
         N(upload_strong), 0x1000 + 20);
   CHECK(N(upload_id_in) == strong_id, "a strength was uploaded as a new effect, not as the one playing");
#ifdef HAVE_THREADS
   CHECK(N(uploads) - before < 20, "all %d strengths were written one by one", N(uploads) - before);
   CHECK(took < 20.0, "twenty calls took %.1f ms", took);
   printf("   ok   twenty strengths in %.2f ms: %d written, the last one last\n", took, N(uploads) - before);
#else
   printf("   ok   twenty strengths: the last one last\n");
#endif
   slow_ms = 0;
   CHECK(!pad_event(&ev, 50), "the effect was played again for a change of strength");

   /* stopped: played with a value of nought, nothing uploaded */
   before = N(uploads);
   udev_set_rumble(0, RETRO_RUMBLE_STRONG, 0);
   CHECK(pad_event(&ev, 500) && ev.type == EV_FF && ev.code == strong_id && ev.value == 0,
         "the effect was not stopped");
   CHECK(N(uploads) == before, "stopping uploaded an effect");
   printf("   ok   a strength of nought stops the effect and uploads nothing\n");

   /* the other motor is another effect */
   udev_set_rumble(0, RETRO_RUMBLE_WEAK, 0x4000);
   CHECK(pad_event(&ev, 500) && ev.value == 1 && ev.code != strong_id && N(upload_weak) == 0x4000,
         "the weak motor is not an effect of its own");
   udev_set_rumble(0, RETRO_RUMBLE_WEAK, 0);
   CHECK(pad_event(&ev, 500) && ev.value == 0, "the weak motor was not stopped");
   printf("   ok   the weak motor is an effect of its own\n");

#ifndef HAVE_LAKKA_SWITCH
   /* the gain is written by the writer too */
   CHECK(udev_set_rumble_gain(0, 50), "the gain was refused");
   CHECK(pad_event(&ev, 500) && ev.type == EV_FF && ev.code == FF_GAIN
         && ev.value > 0x7000 && ev.value < 0x9000, "the gain did not arrive");
   printf("   ok   the gain arrives the same way\n");
#endif

   /* the pad goes and another comes in its slot: the new one is
    * written to, its effects its own */
   {
      int old_rd = pad_rd;
      int old_fd = udev_pads[0].fd;
      udev_rumble_detach(0);
      close(old_fd);
      udev_pad_set_fd(0, -1);
      usleep(50000);
      /* nothing holds the old pad open any more */
      CHECK(read(old_rd, &ev, sizeof(ev)) == 0,
            "the pad that went is still held open by the driver");
      close(old_rd);
      pad_plug();
      udev_set_rumble(0, RETRO_RUMBLE_STRONG, 0x2000);
      CHECK(pad_event(&ev, 500) && ev.value == 1, "the new pad in the slot is not written to");
      CHECK(N(upload_id_in) == -1 && N(upload_strong) == 0x2000,
            "the new pad was given the old one's effect");
      printf("   ok   a pad that goes is let go of, and the next one in its slot is written to afresh\n");
   }

   /* stopped: the writer ends and lets go of what it held */
   {
      int old_fd = udev_pads[0].fd;
      udev_rumble_stop();
      close(old_fd);
      udev_pad_set_fd(0, -1);
      while (read(pad_rd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) { }
      CHECK(read(pad_rd, &ev, sizeof(ev)) == 0,
            "after the driver stops a pad is still held open");
      close(pad_rd);
      printf("   ok   stopping the driver ends the writer and lets go of the pads\n");
   }

   if (failures)
   {
      printf("FAIL udev_rumble_test: %u\n", failures);
      return 1;
   }
   printf("PASS udev_rumble_test\n");
   return 0;
}
