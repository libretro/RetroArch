/* The Wii's USB HID driver (input/drivers_hid/gekko_hid.c) taking pads
 * away, with os/gekko's threads, futex and USB calls stood in for on
 * pthreads. A transfer blocks until a report is queued, the device
 * goes or a cancel comes while it is waiting: a cancel with nothing
 * waiting is lost, as IOS loses it.
 *
 * Held to:
 *
 * - a pad unplugged while its reader waits is reaped once the reader
 *   is out, with no sleep anywhere;
 * - a pad whose cancel was lost, because it came just before the reader
 *   started its transfer, is still reaped: the worker asks again;
 * - a driver freed while a reader waits with its cancel lost returns at
 *   once; the reader frees its pad and the driver when its transfer
 *   returns, touching nothing freed (run under ASan to see that).
 *
 * GEKKO_HID_TEST_NO_RECANCEL makes the worker ask only once, and the
 * test must fail. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sched.h>
#include <pthread.h>

#include <gekko/usb.h>
#include <retro_atomic.h>
#ifdef GEKKO_HID_TEST_NO_RECANCEL
/* The first cancel of each plug only. */
static int asked;
static int cancel_once(uint32_t id, uint8_t ep)
{
   if (asked++)
      return 0;
   return gk_usb_cancel(id, ep);
}
#define gk_usb_cancel(id, ep) cancel_once(id, ep)
#endif
#include "input/drivers_hid/gekko_hid.c"
#ifdef GEKKO_HID_TEST_NO_RECANCEL
#undef gk_usb_cancel
#endif

/* A reader may be left stuck by a failure: stop there. */
#define CHECK(c) do { if (!(c)) { \
   fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
   exit(1); } } while (0)

void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; }

/* ---- os/gekko threads, futex and sleep ---- */
uint32_t gk_tb_hz = 1000000;
static retro_atomic_int_t sleeps;

struct gk_thread
{
   pthread_t    thread;
   gk_thread_fn fn;
   void        *arg;
};

static void *trampoline(void *arg)
{
   gk_thread_t *t = (gk_thread_t*)arg;
   return t->fn(t->arg);
}

gk_thread_t *gk_thread_create(gk_thread_fn fn, void *arg,
      void *stack, size_t stack_size, int prio)
{
   gk_thread_t *t = (gk_thread_t*)calloc(1, sizeof(*t));
   (void)stack; (void)stack_size; (void)prio;
   if (!t)
      return NULL;
   t->fn  = fn;
   t->arg = arg;
   if (pthread_create(&t->thread, NULL, trampoline, t))
   {
      free(t);
      return NULL;
   }
   return t;
}

void *gk_thread_join(gk_thread_t *t)
{
   void *ret = NULL;
   pthread_join(t->thread, &ret);
   free(t);
   return ret;
}

void gk_thread_detach(gk_thread_t *t)
{
   pthread_detach(t->thread);
   free(t);
}

void gk_sleep_us(uint64_t us)
{
   (void)us;
   retro_atomic_fetch_add_int(&sleeps, 1);
}

static pthread_mutex_t futex_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  futex_cond = PTHREAD_COND_INITIALIZER;

int gk_futex_wait(volatile uint32_t *addr, uint32_t expected,
      uint64_t timeout)
{
   struct timespec ts;
   int ret = GK_OK;
   clock_gettime(CLOCK_REALTIME, &ts);
   ts.tv_sec  += (time_t)(timeout / 1000000u);
   ts.tv_nsec += (long)(timeout % 1000000u) * 1000;
   if (ts.tv_nsec >= 1000000000L)
   {
      ts.tv_sec++;
      ts.tv_nsec -= 1000000000L;
   }
   pthread_mutex_lock(&futex_lock);
   if (__atomic_load_n(addr, __ATOMIC_ACQUIRE) != expected)
      ret = GK_EAGAIN;
   else if (pthread_cond_timedwait(&futex_cond, &futex_lock, &ts))
      ret = GK_ETIMEDOUT;
   pthread_mutex_unlock(&futex_lock);
   return ret;
}

int gk_futex_wake(volatile uint32_t *addr, int count)
{
   (void)addr; (void)count;
   pthread_mutex_lock(&futex_lock);
   pthread_cond_broadcast(&futex_cond);
   pthread_mutex_unlock(&futex_lock);
   return 0;
}

/* ---- os/gekko USB: one pad ---- */
#define PAD_ID 0x51
static pthread_mutex_t usb_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  usb_cond = PTHREAD_COND_INITIALIZER;
static int  plugged, waiting, reports, cancels, lost, opens, closes;
static int  gate_entry, at_gate;  /* hold the reader just before it waits */

int gk_usb_hid_list(gk_usb_dev_t *out, int max)
{
   int n = 0;
   pthread_mutex_lock(&usb_lock);
   if (plugged && max > 0)
   {
      memset(out, 0, sizeof(*out));
      out->id  = PAD_ID;
      out->vid = 0x1234;
      out->pid = 0x0001;
      n        = 1;
   }
   pthread_mutex_unlock(&usb_lock);
   return n;
}

int gk_usb_open(uint32_t id, gk_usb_info_t *info)
{
   memset(info, 0, sizeof(*info));
   info->vid                  = 0x1234;
   info->pid                  = 0x0001;
   info->num_ep               = 1;
   info->ep[0].address        = 0x81;
   info->ep[0].attributes     = 3;
   info->ep[0].max_packet     = 8;
   pthread_mutex_lock(&usb_lock);
   opens++;
   pthread_mutex_unlock(&usb_lock);
   return id == PAD_ID ? 0 : -1;
}

void gk_usb_close(uint32_t id)
{
   (void)id;
   pthread_mutex_lock(&usb_lock);
   closes++;
   pthread_mutex_unlock(&usb_lock);
}

int gk_usb_ctrl(uint32_t id, uint8_t type, uint8_t request, uint16_t value,
      uint16_t index, void *data, uint16_t len)
{
   (void)id; (void)type; (void)request; (void)value; (void)index;
   (void)data; (void)len;
   return 0;
}

int gk_usb_intr(uint32_t id, uint8_t ep, void *data, uint32_t len)
{
   int ret, seen;
   (void)id; (void)ep;
   pthread_mutex_lock(&usb_lock);
   if (gate_entry)
   {
      at_gate = 1;
      pthread_cond_broadcast(&usb_cond);
      while (gate_entry)
         pthread_cond_wait(&usb_cond, &usb_lock);
      at_gate = 0;
   }
   waiting = 1;
   seen    = cancels;
   pthread_cond_broadcast(&usb_cond);
   while (!reports && cancels == seen)
      pthread_cond_wait(&usb_cond, &usb_lock);
   waiting = 0;
   if (reports)
   {
      reports--;
      memset(data, 0, len);
      ret = (int)len;
   }
   else
      ret = -1;
   pthread_mutex_unlock(&usb_lock);
   return ret;
}

int gk_usb_cancel(uint32_t id, uint8_t ep)
{
   (void)id; (void)ep;
   pthread_mutex_lock(&usb_lock);
   if (waiting)
      cancels++;
   else
      lost++;
   pthread_cond_broadcast(&usb_cond);
   pthread_mutex_unlock(&usb_lock);
   return 0;
}

/* ---- the connection layer: one slot, no pad driver behind it ---- */
static retro_atomic_int_t pad_deinits, packets, disconnects;
static retro_atomic_int_t destroyed;

joypad_connection_t *pad_connection_init(unsigned pads)
{
   return (joypad_connection_t*)calloc(pads + 1, sizeof(joypad_connection_t));
}

void pad_connection_destroy(joypad_connection_t *joyconn)
{
   free(joyconn);
   retro_atomic_fetch_add_int(&destroyed, 1);
}

int32_t pad_connection_pad_init(joypad_connection_t *joyconn,
   const char *name, uint16_t vid, uint16_t pid,
   void *data, hid_driver_t *driver)
{
   (void)joyconn; (void)name; (void)vid; (void)pid; (void)data;
   (void)driver;
   return 0;
}

bool pad_connection_has_interface(joypad_connection_t *joyconn,
   unsigned idx)
{
   (void)joyconn; (void)idx;
   return true;
}

void pad_connection_pad_deinit(joypad_connection_t *joyconn, uint32_t idx)
{
   (void)joyconn; (void)idx;
   retro_atomic_fetch_add_int(&pad_deinits, 1);
}

void pad_connection_packet(joypad_connection_t *joyconn, uint32_t idx,
   uint8_t *data, uint32_t length)
{
   (void)joyconn; (void)idx; (void)data; (void)length;
   retro_atomic_fetch_add_int(&packets, 1);
}

const char *pad_connection_get_name(joypad_connection_t *joyconn,
   unsigned idx)
{
   (void)joyconn; (void)idx;
   return "pad";
}

void pad_connection_get_buttons(joypad_connection_t *joyconn,
   unsigned idx, input_bits_t *state)
{
   (void)joyconn; (void)idx;
   BIT256_CLEAR_ALL_PTR(state);
}

int16_t pad_connection_get_axis(joypad_connection_t *joyconn,
   unsigned idx, unsigned i)
{
   (void)joyconn; (void)idx; (void)i;
   return 0;
}

bool pad_connection_rumble(joypad_connection_t *s,
   unsigned pad, enum retro_rumble_effect effect, uint16_t strength)
{
   (void)s; (void)pad; (void)effect; (void)strength;
   return false;
}

void pad_connection_set_player(joypad_connection_t *joyconn,
      unsigned player)
{
   (void)joyconn; (void)player;
}

bool input_autoconfigure_connect(const char *name,
      const char *display_name, const char *phys, const char *driver,
      unsigned port, unsigned vid, unsigned pid)
{
   (void)name; (void)display_name; (void)phys; (void)driver; (void)port;
   (void)vid; (void)pid;
   return true;
}

bool input_autoconfigure_disconnect(unsigned port, const char *name)
{
   (void)port; (void)name;
   retro_atomic_fetch_add_int(&disconnects, 1);
   return true;
}

/* ---- the lanes ---- */

static double now_s(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

/* Waits, at most two seconds, for @cond to hold. */
#define WAIT_FOR(cond) do { double w_ = now_s() + 2.0; \
   while (!(cond) && now_s() < w_) sched_yield(); } while (0)

static int usb_count(int *n)
{
   int r;
   pthread_mutex_lock(&usb_lock);
   r = *n;
   pthread_mutex_unlock(&usb_lock);
   return r;
}

static int reader_waiting(void)
{
   int r;
   pthread_mutex_lock(&usb_lock);
   r = waiting;
   pthread_mutex_unlock(&usb_lock);
   return r;
}

static int reader_at_gate(void)
{
   int r;
   pthread_mutex_lock(&usb_lock);
   r = at_gate;
   pthread_mutex_unlock(&usb_lock);
   return r;
}

static void set_plugged(int value)
{
   pthread_mutex_lock(&usb_lock);
   plugged = value;
   pthread_mutex_unlock(&usb_lock);
}

static void set_gate(int value)
{
   pthread_mutex_lock(&usb_lock);
   gate_entry = value;
   pthread_cond_broadcast(&usb_cond);
   pthread_mutex_unlock(&usb_lock);
}

static void queue_report(void)
{
   pthread_mutex_lock(&usb_lock);
   reports++;
   pthread_cond_broadcast(&usb_cond);
   pthread_mutex_unlock(&usb_lock);
}

static int reaped(int n)
{
   return retro_atomic_load_acquire_int(&pad_deinits) == n;
}

static gekko_hid_t *plug_in(void)
{
   gekko_hid_t *hid;
#ifdef GEKKO_HID_TEST_NO_RECANCEL
   asked = 0;
#endif
   set_plugged(1);
   hid = (gekko_hid_t*)gekko_hid_init();
   CHECK(hid != NULL);
   if (!hid)
      exit(1);
   WAIT_FOR(reader_waiting());
   CHECK(reader_waiting());
   return hid;
}

int main(void)
{
   gekko_hid_t *hid;
   int base;

   /* unplugged while the reader waits */
   hid = plug_in();
   queue_report();
   WAIT_FOR(retro_atomic_load_acquire_int(&packets) == 1 && reader_waiting());
   CHECK(retro_atomic_load_acquire_int(&packets) == 1);
   set_plugged(0);
   hid_wake(hid);
   WAIT_FOR(reaped(1));
   CHECK(reaped(1) && retro_atomic_load_acquire_int(&disconnects) == 1
         && usb_count(&closes) == 1);
   gekko_hid_free(hid);
   CHECK(retro_atomic_load_acquire_int(&destroyed) == 1);
   printf("unplugged while waiting: reaped, %d sleeps\n",
         retro_atomic_load_acquire_int(&sleeps));

   /* unplugged with the cancel lost: the reader had a report in hand */
   hid = plug_in();
   set_gate(1);
   queue_report();
   WAIT_FOR(reader_at_gate());
   CHECK(reader_at_gate());
   base = usb_count(&lost);
   set_plugged(0);
   hid_wake(hid);
   WAIT_FOR(usb_count(&lost) > base);
   CHECK(usb_count(&lost) > base);
   /* stop is caught before the reader waits again, or a later cancel */
   set_gate(0);
   WAIT_FOR(reader_waiting());
   {
      /* the reader waits again: only another cancel gets it out */
      unsigned tries;
      for (tries = 0; tries < 3 && !reaped(2); tries++)
      {
         hid_wake(hid);
         WAIT_FOR(reaped(2));
      }
   }
   CHECK(reaped(2) && retro_atomic_load_acquire_int(&disconnects) == 2);
   gekko_hid_free(hid);
   CHECK(retro_atomic_load_acquire_int(&destroyed) == 2);
   printf("unplugged with a lost cancel: reaped\n");

   /* freed while the reader waits on a transfer whose cancel was lost */
   hid = plug_in();
   set_gate(1);
   queue_report();
   WAIT_FOR(reader_at_gate());
   base = usb_count(&lost);
   gekko_hid_free(hid);
   CHECK(usb_count(&lost) > base);
   CHECK(retro_atomic_load_acquire_int(&destroyed) == 2);
   set_gate(0);
   WAIT_FOR(reader_waiting());
   queue_report();
   WAIT_FOR(retro_atomic_load_acquire_int(&destroyed) == 3);
   CHECK(retro_atomic_load_acquire_int(&destroyed) == 3);
   printf("freed with the reader waiting: it freed the driver itself\n");

   CHECK(!retro_atomic_load_acquire_int(&sleeps));
   printf("PASS: gekko_hid pad removal\n");
   return 0;
}
