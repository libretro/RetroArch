/* The Wii U HID driver (input/drivers_hid/wiiu_hid.c) across its
 * threads, against the tree's Wii U SDK headers with Cafe OS's threads,
 * events, clock and HID calls stood in for on pthreads:
 *
 * - two threads attach and detach devices through the HID attach
 *   callback, as the HID stack does, while the frontend polls;
 * - an "IOS" thread completes each HIDRead() some time later, as the
 *   real ones complete off the polling thread, with an error once the
 *   device is gone or the client deleted.
 *
 * Held to:
 *
 * - every pad initialised is registered, then deregistered, then
 *   deinitialised, once each and in that order, initialised and
 *   registered on the frontend's thread, torn down on the frontend's -
 *   or on the polling thread's, but only at shutdown;
 * - no packet reaches a pad before it is registered or once it is
 *   being torn down, and none after a failed read;
 * - a device the driver has no entry for, one whose pad fails to
 *   initialise, a mouse and a NULL device come and go leaving nothing;
 *   a detach for a handle never attached does nothing;
 * - the devices still attached at shutdown are torn down with their
 *   reads in flight, leaving no pad and no slot held.
 *
 * Pad data is never freed, only marked dead, so a pad touched after
 * its teardown is caught with or without a sanitizer.
 *
 * WIIU_HID_THREADS_BROKEN_IOS makes the IOS stand-in lose the reads in
 * flight when the client is deleted, as a broken HID stack would; the
 * driver then gives up on them after its five seconds and leaves their
 * adapters - and pads - as they are, and the test must fail. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sched.h>
#include <pthread.h>

/* No memalign on Windows; the host needs no alignment from it. The
 * macro also renames glibc's declaration, to the same signature. */
static void *harness_memalign(size_t alignment, size_t len)
{
   (void)alignment;
   return malloc(len);
}
#define memalign(a, n) harness_memalign(a, n)

#include "input/drivers_hid/wiiu_hid.c"

#define ATTACH_THREADS 2
#define CYCLES         300
#define MAX_PADS       (ATTACH_THREADS * CYCLES + 16)

#define VID_OK        0x1234
#define VID_UNKNOWN   0xdead
#define PID_OK        0x0001
#define PID_INIT_FAIL 0x0002

static retro_atomic_int_t failures;

#define CHECK(c) do { if (!(c)) { \
   fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
   retro_atomic_inc_int(&failures); } } while (0)

static pthread_t frontend_thread;
static pthread_t polling_thread;
static retro_atomic_int_t shutting_down;

static int on_frontend(void)
{
   return pthread_equal(pthread_self(), frontend_thread);
}

static int on_polling_thread(void)
{
   return pthread_equal(pthread_self(), polling_thread);
}

/* ---- logging ---- */
void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; }

/* ---- the clock ---- */
static OSSystemInfo system_info = { 248625000 };

OSSystemInfo *OSGetSystemInfo(void) { return &system_info; }

OSTime OSGetSystemTime(void)
{
   struct timespec ts;
   uint64_t ns;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   ns = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
   return (OSTime)(ns * (system_info.clockSpeed / 4) / 1000000000ull);
}

/* ---- events: the driver has the one, hid_wake ---- */
static pthread_mutex_t event_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  event_cond = PTHREAD_COND_INITIALIZER;

void OSInitEvent(OSEvent *event, BOOL value, OSEventMode mode)
{
   pthread_mutex_lock(&event_lock);
   event->value = value;
   event->mode  = mode;
   pthread_mutex_unlock(&event_lock);
}

void OSSignalEvent(OSEvent *event)
{
   pthread_mutex_lock(&event_lock);
   event->value = TRUE;
   pthread_cond_broadcast(&event_cond);
   pthread_mutex_unlock(&event_lock);
}

void OSWaitEvent(OSEvent *event)
{
   pthread_mutex_lock(&event_lock);
   while (!event->value)
      pthread_cond_wait(&event_cond, &event_lock);
   if (event->mode == OS_EVENT_MODE_AUTO)
      event->value = FALSE;
   pthread_mutex_unlock(&event_lock);
}

BOOL OSWaitEventWithTimeout(OSEvent *event, OSTime timeout)
{
   struct timespec until;
   uint64_t ns = (uint64_t)timeout * 1000000000ull
      / (system_info.clockSpeed / 4);
   BOOL got;

   clock_gettime(CLOCK_REALTIME, &until);
   until.tv_sec  += (time_t)(ns / 1000000000ull);
   until.tv_nsec += (long)(ns % 1000000000ull);
   if (until.tv_nsec >= 1000000000L)
   {
      until.tv_sec++;
      until.tv_nsec -= 1000000000L;
   }

   pthread_mutex_lock(&event_lock);
   while (!event->value)
      if (pthread_cond_timedwait(&event_cond, &event_lock, &until))
         break;
   got = event->value;
   if (got && event->mode == OS_EVENT_MODE_AUTO)
      event->value = FALSE;
   pthread_mutex_unlock(&event_lock);
   return got;
}

/* ---- the polling thread: the driver makes the one ---- */
static OSThreadEntryPointFn      thread_entry;
static OSThreadCleanupCallbackFn thread_cleanup;
static int32_t                   thread_argc;
static char                     *thread_argv;
static void                     *thread_stack;
static OSThread                 *thread_os;

BOOL OSCreateThread(OSThread *thread, OSThreadEntryPointFn entry,
      int32_t argc, char *argv, void *stack, uint32_t stack_size,
      int32_t priority, OSThreadAttributes attributes)
{
   (void)stack_size; (void)priority; (void)attributes;
   thread_os    = thread;
   thread_entry = entry;
   thread_argc  = argc;
   thread_argv  = argv;
   thread_stack = stack;
   return TRUE;
}

OSThreadCleanupCallbackFn OSSetThreadCleanupCallback(OSThread *thread,
      OSThreadCleanupCallbackFn callback)
{
   (void)thread;
   thread_cleanup = callback;
   return NULL;
}

static void *thread_trampoline(void *arg)
{
   int rc = thread_entry(thread_argc, (const char **)thread_argv);
   (void)arg;
   if (thread_cleanup)
      thread_cleanup(thread_os, thread_stack);
   return (void *)(intptr_t)rc;
}

int32_t OSResumeThread(OSThread *thread)
{
   (void)thread;
   CHECK(pthread_create(&polling_thread, NULL, thread_trampoline, NULL) == 0);
   return 1;
}

BOOL OSJoinThread(OSThread *thread, int *result)
{
   void *rc;
   (void)thread;
   pthread_join(polling_thread, &rc);
   if (result)
      *result = (int)(intptr_t)rc;
   return TRUE;
}

/* ---- the HID stack ---- */
#define MAX_HANDLES (ATTACH_THREADS * CYCLES * 2 + 16)

typedef struct read_req
{
   struct read_req *next;
   HIDCallback cb;
   void *user;
   uint8_t *buf;
   uint32_t handle;
   uint32_t len;
} read_req_t;

static pthread_mutex_t   ios_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t    ios_cond = PTHREAD_COND_INITIALIZER;
static read_req_t       *ios_head;
static read_req_t      **ios_tail = &ios_head;
static int               ios_quit;
static int               ios_client_gone;
static int               ios_hold_reads;
static pthread_t         ios_thread;
static HIDAttachCallback attach_cb;
static HIDClient        *the_client;
/* Set while a handle's device is plugged in; its reads fail without */
static retro_atomic_int_t handle_present[MAX_HANDLES];
static retro_atomic_int_t reads_failed;

int32_t HIDAddClient(HIDClient *client, HIDAttachCallback cb)
{
   the_client = client;
   attach_cb  = cb;
   return 0;
}

int32_t HIDDelClient(HIDClient *client)
{
   (void)client;
   pthread_mutex_lock(&ios_lock);
   ios_client_gone = 1;
   ios_hold_reads = 0;
#ifdef WIIU_HID_THREADS_BROKEN_IOS
   /* The reads in flight are lost: never completed */
   while (ios_head)
   {
      read_req_t *r = ios_head;
      ios_head = r->next;
      free(r);
   }
   ios_tail = &ios_head;
#endif
   pthread_cond_broadcast(&ios_cond);
   pthread_mutex_unlock(&ios_lock);
   return 0;
}

int32_t HIDRead(uint32_t handle, uint8_t *buf, uint32_t len,
      HIDCallback cb, void *user)
{
   read_req_t *r;
   if (!cb)
      return 0;
   CHECK(on_polling_thread());
   r         = (read_req_t *)calloc(1, sizeof(*r));
   r->cb     = cb;
   r->user   = user;
   r->buf    = buf;
   r->handle = handle;
   r->len    = len;
   pthread_mutex_lock(&ios_lock);
   *ios_tail = r;
   ios_tail  = &r->next;
   pthread_cond_signal(&ios_cond);
   pthread_mutex_unlock(&ios_lock);
   return 0;
}

static void *ios_main(void *arg)
{
   uint32_t seq = 0;
   (void)arg;
   for (;;)
   {
      read_req_t *r;
      int32_t err;
      int i, spin;

      pthread_mutex_lock(&ios_lock);
      while ((!ios_head || ios_hold_reads) && !ios_quit)
         pthread_cond_wait(&ios_cond, &ios_lock);
      if (!ios_head)
      {
         pthread_mutex_unlock(&ios_lock);
         return NULL;
      }
      r        = ios_head;
      ios_head = r->next;
      if (!ios_head)
         ios_tail = &ios_head;
      err      = ios_client_gone ? -107 : 0;
      pthread_mutex_unlock(&ios_lock);

      /* Some time later, the order of things shaken up */
      spin = (int)(++seq % 7);
      for (i = 0; i < spin; i++)
         sched_yield();

      if (!err && !retro_atomic_load_acquire_int(&handle_present[r->handle]))
         err = -111;
      if (err)
         retro_atomic_inc_int(&reads_failed);
      else
         memset(r->buf, (int)(seq & 0xff), r->len);
      r->cb(r->handle, err, r->buf, err ? 0 : r->len, r->user);
      free(r);
   }
}

int32_t HIDGetDescriptor(uint32_t handle, uint8_t type, uint8_t index,
      uint16_t lang, uint8_t *buf, uint32_t len, HIDCallback cb, void *user)
{
   (void)handle; (void)type; (void)index; (void)lang; (void)buf;
   (void)len; (void)cb; (void)user;
   return 0;
}
int32_t HIDGetReport(uint32_t h, uint8_t t, uint8_t id, uint8_t *b,
      uint32_t l, HIDCallback cb, void *u)
{ (void)h; (void)t; (void)id; (void)b; (void)l; (void)cb; (void)u; return 0; }
int32_t HIDSetReport(uint32_t h, uint8_t t, uint8_t id, uint8_t *b,
      uint32_t l, HIDCallback cb, void *u)
{ (void)h; (void)t; (void)id; (void)b; (void)l; (void)cb; (void)u; return 0; }
int32_t HIDSetIdle(uint32_t h, uint8_t i, uint8_t d, HIDCallback cb, void *u)
{ (void)h; (void)i; (void)d; (void)cb; (void)u; return 0; }
int32_t HIDSetProtocol(uint32_t h, uint8_t i, uint8_t p, HIDCallback cb,
      void *u)
{ (void)h; (void)i; (void)p; (void)cb; (void)u; return 0; }
int32_t HIDWrite(uint32_t h, uint8_t *b, uint32_t l, HIDCallback cb, void *u)
{ (void)h; (void)b; (void)l; (void)cb; (void)u; return 0; }

/* ---- the pads ---- */
#define PAD_LIVE       0x11
#define PAD_REGISTERED 0x22
#define PAD_TORN       0x33
#define PAD_DEAD       0x44

typedef struct test_pad
{
   retro_atomic_int_t stage;
   retro_atomic_int_t packets;
   uint32_t handle;
} test_pad_t;

static test_pad_t         pad_pool[MAX_PADS];
static retro_atomic_int_t pads_made;
static retro_atomic_int_t pads_registered;
static retro_atomic_int_t pads_deregistered;
static retro_atomic_int_t pads_dead;
static retro_atomic_int_t slots_held;

static void *test_pad_init(void *data, uint32_t slot, hid_driver_t *driver)
{
   wiiu_adapter_t *adapter = (wiiu_adapter_t *)data;
   test_pad_t *pad;
   int n;
   (void)slot; (void)driver;
   CHECK(on_frontend());
   if (adapter->product_id == PID_INIT_FAIL)
      return NULL;
   n = retro_atomic_fetch_add_int(&pads_made, 1);
   if (n >= MAX_PADS)
      return NULL;
   pad         = &pad_pool[n];
   pad->handle = adapter->handle;
   retro_atomic_store_release_int(&pad->stage, PAD_LIVE);
   return pad;
}

static void test_pad_deinit(void *data)
{
   test_pad_t *pad = (test_pad_t *)data;
   CHECK(on_frontend()
         || (on_polling_thread()
            && retro_atomic_load_acquire_int(&shutting_down)));
   CHECK(retro_atomic_load_acquire_int(&pad->stage) == PAD_TORN);
   retro_atomic_store_release_int(&pad->stage, PAD_DEAD);
   retro_atomic_inc_int(&pads_dead);
}

static void test_pad_packet(void *data, uint8_t *packet, uint16_t size)
{
   test_pad_t *pad = (test_pad_t *)data;
   (void)packet;
   CHECK(size > 0);
   CHECK(retro_atomic_load_acquire_int(&pad->stage) == PAD_REGISTERED);
   retro_atomic_inc_int(&pad->packets);
}

static pad_connection_interface_t test_pad_iface;

static joypad_connection_entry_t test_entry = {
   "test pad", VID_OK, 0, &test_pad_iface
};

joypad_connection_entry_t *find_connection_entry(uint16_t vid, uint16_t pid,
      const char *name)
{
   (void)pid; (void)name;
   return vid == VID_OK ? &test_entry : NULL;
}

/* ---- the frontend's pad slots ---- */
wiiu_joypad_t         joypad_state;
input_device_driver_t hidpad_driver;

int pad_connection_find_vacant_pad(joypad_connection_t *joyconn)
{
   (void)joyconn;
   CHECK(on_frontend());
   retro_atomic_inc_int(&slots_held);
   return 0;
}

void pad_connection_release_slot(joypad_connection_t *joyconn)
{
   (void)joyconn;
   retro_atomic_dec_int(&slots_held);
}

void pad_connection_pad_register(joypad_connection_t *joyconn,
      pad_connection_interface_t *iface, void *pad_data, void *handle,
      input_device_driver_t *input_driver, int slot)
{
   test_pad_t *pad = (test_pad_t *)pad_data;
   (void)joyconn; (void)iface; (void)handle; (void)input_driver; (void)slot;
   CHECK(on_frontend());
   CHECK(retro_atomic_load_acquire_int(&pad->stage) == PAD_LIVE);
   retro_atomic_store_release_int(&pad->stage, PAD_REGISTERED);
   retro_atomic_inc_int(&pads_registered);
}

void pad_connection_pad_deregister(joypad_connection_t *joyconn,
      pad_connection_interface_t *iface, void *pad_data)
{
   test_pad_t *pad = (test_pad_t *)pad_data;
   (void)joyconn; (void)iface;
   CHECK(on_frontend()
         || (on_polling_thread()
            && retro_atomic_load_acquire_int(&shutting_down)));
   CHECK(retro_atomic_load_acquire_int(&pad->stage) == PAD_REGISTERED);
   retro_atomic_store_release_int(&pad->stage, PAD_TORN);
   retro_atomic_inc_int(&pads_deregistered);
   /* The slot goes with the pad, as the real deregister gives it back */
   retro_atomic_dec_int(&slots_held);
}

void pad_connection_pad_refresh(joypad_connection_t *joyconn,
      pad_connection_interface_t *iface, void *device_data, void *handle,
      input_device_driver_t *input_driver)
{
   (void)joyconn; (void)iface; (void)device_data; (void)handle;
   (void)input_driver;
}

bool pad_connection_acquire(joypad_connection_t *joyconn)
{
   (void)joyconn;
   return false;
}

void pad_connection_release(joypad_connection_t *joyconn)
{
   (void)joyconn;
}

/* ---- the attaching threads ---- */
static retro_atomic_int_t attach_done;

static void plug(uint32_t handle, uint16_t vid, uint16_t pid,
      uint8_t protocol, int expect)
{
   HIDDevice dev;
   memset(&dev, 0, sizeof(dev));
   dev.handle             = handle;
   dev.vid                = vid;
   dev.pid                = pid;
   dev.protocol           = protocol;
   dev.max_packet_size_rx = 64;
   dev.max_packet_size_tx = 64;
   retro_atomic_store_release_int(&handle_present[handle], 1);
   CHECK(attach_cb(the_client, &dev, HID_DEVICE_ATTACH) == expect);
}

static void unplug(uint32_t handle, uint16_t vid, uint16_t pid)
{
   HIDDevice dev;
   memset(&dev, 0, sizeof(dev));
   dev.handle = handle;
   dev.vid    = vid;
   dev.pid    = pid;
   retro_atomic_store_release_int(&handle_present[handle], 0);
   attach_cb(the_client, &dev, HID_DEVICE_DETACH);
}

static void *attach_main(void *arg)
{
   uint32_t base = 1 + (uint32_t)(intptr_t)arg * CYCLES * 2;
   int i, j;

   for (i = 0; i < CYCLES; i++)
   {
      uint32_t handle = base + (uint32_t)i * 2;
      uint16_t vid    = (i % 5 == 3) ? VID_UNKNOWN : VID_OK;
      uint16_t pid    = (i % 7 == 4) ? PID_INIT_FAIL : PID_OK;

      plug(handle, vid, pid, 0, DEVICE_USED);
      /* A mouse: not the driver's */
      if (i % 11 == 0)
         plug(handle + 1, VID_OK, PID_OK, 2, DEVICE_UNUSED);
      for (j = 0; j < (i % 13) * 4; j++)
         sched_yield();
      /* The last stays plugged in, for shutdown to tear down */
      if (i != CYCLES - 1)
         unplug(handle, vid, pid);
   }
   retro_atomic_inc_int(&attach_done);
   return NULL;
}

static uint64_t now_ms(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

int main(void)
{
   pthread_t attachers[ATTACH_THREADS];
   wiiu_hid_t *hid;
   uint64_t deadline;
   int i, live, settled;

   test_pad_iface.init           = test_pad_init;
   test_pad_iface.deinit         = test_pad_deinit;
   test_pad_iface.packet_handler = test_pad_packet;
   joypad_state.max_slot         = 1;

   frontend_thread = pthread_self();
   CHECK(pthread_create(&ios_thread, NULL, ios_main, NULL) == 0);

   hid = (wiiu_hid_t *)wiiu_hid_init();
   CHECK(hid != NULL);
   if (!hid)
      return 1;

   /* Neither a NULL device nor a detach for a handle never seen is
    * anything to the driver */
   CHECK(attach_cb(the_client, NULL, HID_DEVICE_ATTACH) == DEVICE_UNUSED);
   unplug(MAX_HANDLES - 1, VID_OK, PID_OK);

   for (i = 0; i < ATTACH_THREADS; i++)
      CHECK(pthread_create(&attachers[i], NULL, attach_main,
               (void *)(intptr_t)i) == 0);

   /* The frontend polls until the devices are all in and those gone
    * are gone: a pad for each of the attachers' last, and nothing torn
    * down left to be. */
   deadline = now_ms() + 20000;
   settled  = 0;
   while (now_ms() < deadline)
   {
      wiiu_hid_poll(hid);
      if (retro_atomic_load_acquire_int(&attach_done) == ATTACH_THREADS)
      {
         int made = retro_atomic_load_acquire_int(&pads_made);
         live     = retro_atomic_load_acquire_int(&pads_registered)
            - retro_atomic_load_acquire_int(&pads_deregistered);
         if (live == ATTACH_THREADS
               && retro_atomic_load_acquire_int(&pads_dead)
                  == made - ATTACH_THREADS)
         {
            /* and each still plugged in reading */
            int reading = 0;
            for (i = 0; i < made; i++)
               if (pad_pool[i].handle % (CYCLES * 2) == CYCLES * 2 - 1
                     && retro_atomic_load_acquire_int(&pad_pool[i].stage)
                     == PAD_REGISTERED
                     && retro_atomic_load_acquire_int(
                        &pad_pool[i].packets) > 0)
                  reading++;
            if (reading == ATTACH_THREADS)
            {
               settled = 1;
               break;
            }
         }
      }
      sched_yield();
   }
   CHECK(settled);

   for (i = 0; i < ATTACH_THREADS; i++)
      pthread_join(attachers[i], NULL);

   /* The supported devices that came and went: each made a pad */
   CHECK(retro_atomic_load_acquire_int(&pads_made) > ATTACH_THREADS);
   CHECK(retro_atomic_load_acquire_int(&slots_held) == ATTACH_THREADS);

   /* Keep both live pads' next reads queued until the client is deleted. */
   pthread_mutex_lock(&ios_lock);
   ios_hold_reads = 1;
   pthread_mutex_unlock(&ios_lock);
   deadline = now_ms() + 20000;
   live = 0;
   while (now_ms() < deadline)
   {
      read_req_t *r;
      live = 0;
      pthread_mutex_lock(&ios_lock);
      for (r = ios_head; r; r = r->next)
         live++;
      pthread_mutex_unlock(&ios_lock);
      if (live == ATTACH_THREADS)
         break;
      sched_yield();
   }
   CHECK(live == ATTACH_THREADS);

   retro_atomic_store_release_int(&shutting_down, 1);
   wiiu_hid_free(hid);

   pthread_mutex_lock(&ios_lock);
   ios_quit = 1;
   pthread_cond_broadcast(&ios_cond);
   pthread_mutex_unlock(&ios_lock);
   pthread_join(ios_thread, NULL);

   CHECK(retro_atomic_load_acquire_int(&pads_registered)
         == retro_atomic_load_acquire_int(&pads_made));
   CHECK(retro_atomic_load_acquire_int(&pads_deregistered)
         == retro_atomic_load_acquire_int(&pads_registered));
   CHECK(retro_atomic_load_acquire_int(&pads_dead)
         == retro_atomic_load_acquire_int(&pads_made));
   CHECK(retro_atomic_load_acquire_int(&slots_held) == 0);
   for (i = 0; i < retro_atomic_load_acquire_int(&pads_made); i++)
      CHECK(retro_atomic_load_acquire_int(&pad_pool[i].stage) == PAD_DEAD);
   CHECK(retro_atomic_load_acquire_int(&reads_failed) > 0);

   printf("pads %d, reads failed %d\n",
         retro_atomic_load_acquire_int(&pads_made),
         retro_atomic_load_acquire_int(&reads_failed));

   if (retro_atomic_load_acquire_int(&failures))
   {
      fprintf(stderr, "%d failure(s)\n",
            retro_atomic_load_acquire_int(&failures));
      return 1;
   }
   printf("wiiu_hid_threads: ok\n");
   return 0;
}
