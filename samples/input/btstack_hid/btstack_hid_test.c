/* The BTstack HID driver's thread (input/drivers_hid/btstack_hid.c),
 * built on the host as C89 against a stand-in for libBTstack, which
 * the test answers through dylib_load() and dylib_proc(), and - built
 * with -D__APPLE__ - for CoreFoundation's run loop (cf_sdk/), which the
 * test runs on the driver's thread. It is held to:
 *
 * - power on starting the thread, which opens BTstack and turns the
 *   radio on;
 * - power off asked before the thread has its run loop returning at
 *   once, the thread left running, and asked again once it has one,
 *   turning the radio off, ending the loop and joining the thread,
 *   with the run loop's source and timer released;
 * - off Apple, where the thread ends once the radio is on, power off
 *   joining it. */
#include "../../../input/drivers_hid/btstack_hid.c"

#include <pthread.h>
#include <stdarg.h>

static unsigned failures;
#define CHECK(c, what) do { if (!(c)) { failures++; \
   printf("FAIL: %s\n", what); } } while (0)

void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; }
void input_pad_connect(unsigned port, input_device_driver_t *driver)
{ (void)port; (void)driver; }
bool input_device_ignored(uint16_t vid, uint16_t pid)
{ (void)vid; (void)pid; return false; }
bool input_autoconfigure_disconnect(unsigned port, const char *name)
{ (void)port; (void)name; return true; }

/* ---- libBTstack ----------------------------------------------------- */

static pthread_mutex_t bt_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  bt_cond = PTHREAD_COND_INITIALIZER;
static int  open_held;      /* bt_open() waits while this is set */
static int  open_entered;
static int  power_sent[2];  /* HCI_POWER_OFF, HCI_POWER_ON */
static const hci_cmd_t power_mode_cmd;

static int fake_bt_open(void)
{
   pthread_mutex_lock(&bt_lock);
   open_entered = 1;
   pthread_cond_broadcast(&bt_cond);
   while (open_held)
      pthread_cond_wait(&bt_cond, &bt_lock);
   pthread_mutex_unlock(&bt_lock);
   return 0;
}

static int fake_bt_send_cmd(const hci_cmd_t *cmd, ...)
{
   va_list ap;
   int mode;
   va_start(ap, cmd);
   mode = va_arg(ap, int);
   va_end(ap);
   if (cmd == &power_mode_cmd && (mode == HCI_POWER_OFF || mode == HCI_POWER_ON))
   {
      pthread_mutex_lock(&bt_lock);
      power_sent[mode]++;
      pthread_mutex_unlock(&bt_lock);
   }
   return 0;
}

static void fake_run_loop_init(RUN_LOOP_TYPE type) { (void)type; }
static void fake_register(btstack_packet_handler_t h) { (void)h; }
static void fake_nop(void) { }

/* every other symbol: a function never called, or a command never sent */
static const hci_cmd_t other_cmd;

void *dylib_load(const char *path) { (void)path; return (void*)&bt_lock; }
void dylib_close(dylib_t lib) { (void)lib; }
function_t dylib_proc(dylib_t lib, const char *proc)
{
   (void)lib;
   if (!strcmp(proc, "bt_open"))
      return (function_t)fake_bt_open;
   if (!strcmp(proc, "bt_send_cmd"))
      return (function_t)fake_bt_send_cmd;
   if (!strcmp(proc, "run_loop_init"))
      return (function_t)fake_run_loop_init;
   if (!strcmp(proc, "bt_register_packet_handler"))
      return (function_t)fake_register;
   if (!strcmp(proc, "btstack_set_power_mode"))
      return (function_t)(void*)&power_mode_cmd;
   if (!strncmp(proc, "hci_", 4))
      return (function_t)(void*)&other_cmd;
   return (function_t)fake_nop;
}

#ifdef __APPLE__
/* ---- CoreFoundation's run loop, one, on the driver's thread ---------- */

struct __CFRunLoop { int unused; };
struct __CFRunLoopSource { CFRunLoopSourceContext ctx; int signalled; };
struct __CFRunLoopTimer { CFRunLoopTimerCallBack cb; int added; };

const CFStringRef kCFRunLoopCommonModes = NULL;

static struct __CFRunLoop the_loop;
static pthread_mutex_t cf_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cf_cond = PTHREAD_COND_INITIALIZER;
static CFRunLoopSourceRef loop_source;
static CFRunLoopTimerRef  loop_timer;
static int loop_woken, loop_stopped, loop_running;
static int sources_live, timers_live;

CFRunLoopRef CFRunLoopGetCurrent(void) { return &the_loop; }

/* Sleeps until woken; then performs a signalled source, fires a timer
 * added, and returns once stopped. */
void CFRunLoopRun(void)
{
   pthread_mutex_lock(&cf_lock);
   loop_stopped = 0;
   loop_running = 1;
   pthread_cond_broadcast(&cf_cond);
   while (!loop_stopped)
   {
      CFRunLoopSourceRef s;
      CFRunLoopTimerRef  t;
      while (!loop_woken && !loop_stopped && !(loop_timer && loop_timer->added))
         pthread_cond_wait(&cf_cond, &cf_lock);
      loop_woken = 0;
      s = (loop_source && loop_source->signalled) ? loop_source : NULL;
      t = (loop_timer && loop_timer->added) ? loop_timer : NULL;
      pthread_mutex_unlock(&cf_lock);
      if (s)
      {
         s->signalled = 0;
         s->ctx.perform(s->ctx.info);
      }
      else if (t)
      {
         t->added = 0;
         t->cb(t, NULL);
      }
      pthread_mutex_lock(&cf_lock);
   }
   loop_running = 0;
   pthread_mutex_unlock(&cf_lock);
}

void CFRunLoopStop(CFRunLoopRef rl)
{
   (void)rl;
   pthread_mutex_lock(&cf_lock);
   loop_stopped = 1;
   pthread_cond_broadcast(&cf_cond);
   pthread_mutex_unlock(&cf_lock);
}

void CFRunLoopWakeUp(CFRunLoopRef rl)
{
   (void)rl;
   pthread_mutex_lock(&cf_lock);
   loop_woken = 1;
   pthread_cond_broadcast(&cf_cond);
   pthread_mutex_unlock(&cf_lock);
}

CFRunLoopSourceRef CFRunLoopSourceCreate(CFAllocatorRef a, CFIndex order,
      CFRunLoopSourceContext *ctx)
{
   struct __CFRunLoopSource *s = (struct __CFRunLoopSource*)calloc(1, sizeof(*s));
   (void)a; (void)order;
   s->ctx = *ctx;
   pthread_mutex_lock(&cf_lock);
   sources_live++;
   pthread_mutex_unlock(&cf_lock);
   return s;
}

void CFRunLoopAddSource(CFRunLoopRef rl, CFRunLoopSourceRef s,
      CFStringRef mode)
{
   (void)rl; (void)mode;
   pthread_mutex_lock(&cf_lock);
   loop_source = s;
   pthread_mutex_unlock(&cf_lock);
}

void CFRunLoopSourceSignal(CFRunLoopSourceRef s)
{
   pthread_mutex_lock(&cf_lock);
   s->signalled = 1;
   pthread_mutex_unlock(&cf_lock);
}

void CFRunLoopSourceInvalidate(CFRunLoopSourceRef s)
{
   pthread_mutex_lock(&cf_lock);
   if (loop_source == s)
      loop_source = NULL;
   pthread_mutex_unlock(&cf_lock);
}

CFAbsoluteTime CFAbsoluteTimeGetCurrent(void) { return 0.0; }

CFRunLoopTimerRef CFRunLoopTimerCreate(CFAllocatorRef a, CFAbsoluteTime fire,
      CFTimeInterval interval, CFOptionFlags flags, CFIndex order,
      CFRunLoopTimerCallBack cb, CFRunLoopTimerContext *ctx)
{
   struct __CFRunLoopTimer *t = (struct __CFRunLoopTimer*)calloc(1, sizeof(*t));
   (void)a; (void)fire; (void)interval; (void)flags; (void)order; (void)ctx;
   t->cb = cb;
   pthread_mutex_lock(&cf_lock);
   timers_live++;
   pthread_mutex_unlock(&cf_lock);
   return t;
}

/* the power-off report never comes here: the timer fires at once */
void CFRunLoopAddTimer(CFRunLoopRef rl, CFRunLoopTimerRef t, CFStringRef mode)
{
   (void)rl; (void)mode;
   pthread_mutex_lock(&cf_lock);
   loop_timer = t;
   t->added   = 1;
   pthread_cond_broadcast(&cf_cond);
   pthread_mutex_unlock(&cf_lock);
}

void CFRunLoopTimerInvalidate(CFRunLoopTimerRef t)
{
   pthread_mutex_lock(&cf_lock);
   t->added = 0;
   if (loop_timer == t)
      loop_timer = NULL;
   pthread_mutex_unlock(&cf_lock);
}

void CFRelease(CFTypeRef p)
{
   pthread_mutex_lock(&cf_lock);
   if (p && p == (CFTypeRef)btstack_stop_timer)
      timers_live--;
   else if (p)
      sources_live--;
   pthread_mutex_unlock(&cf_lock);
   free((void*)p);
}

static void wait_loop_running(void)
{
   pthread_mutex_lock(&cf_lock);
   while (!loop_running)
      pthread_cond_wait(&cf_cond, &cf_lock);
   pthread_mutex_unlock(&cf_lock);
}
#endif

static void wait_open_entered(void)
{
   pthread_mutex_lock(&bt_lock);
   while (!open_entered)
      pthread_cond_wait(&bt_cond, &bt_lock);
   pthread_mutex_unlock(&bt_lock);
}

int main(void)
{
   open_held = 1;
   btstack_set_poweron(true);
   CHECK(btstack_loaded, "BTstack is not loaded");
   CHECK(btstack_thread != NULL, "power on starts no thread");
   wait_open_entered();

#ifdef __APPLE__
   /* the thread is still opening BTstack: no run loop to stop yet */
   btstack_set_poweron(false);
   CHECK(btstack_thread != NULL,
         "power off before the run loop takes the thread away");

   pthread_mutex_lock(&bt_lock);
   open_held = 0;
   pthread_cond_broadcast(&bt_cond);
   pthread_mutex_unlock(&bt_lock);
   wait_loop_running();
   CHECK(power_sent[HCI_POWER_ON] == 1, "the radio is not turned on");

   btstack_set_poweron(false);
   CHECK(btstack_thread == NULL, "power off does not join the thread");
   CHECK(power_sent[HCI_POWER_OFF] == 1, "the radio is not turned off");
   CHECK(!loop_running, "the run loop is still running");
   CHECK(btstack_quit_source == NULL && btstack_run_loop == NULL
         && btstack_stop_timer == NULL,
         "the thread leaves its run loop's objects behind");
   CHECK(sources_live == 0 && timers_live == 0,
         "the run loop's source or timer is not released");
#else
   pthread_mutex_lock(&bt_lock);
   open_held = 0;
   pthread_cond_broadcast(&bt_cond);
   pthread_mutex_unlock(&bt_lock);
   btstack_set_poweron(false);
   CHECK(btstack_thread == NULL, "power off does not join the thread");
   CHECK(power_sent[HCI_POWER_ON] == 1, "the radio is not turned on");
#endif

   /* and on again */
   btstack_set_poweron(true);
   CHECK(btstack_thread != NULL, "power on again starts no thread");
#ifdef __APPLE__
   wait_loop_running();
#endif
   btstack_set_poweron(false);
   CHECK(btstack_thread == NULL, "power off again does not join the thread");

   if (failures)
   {
      printf("FAIL btstack_hid_test: %u\n", failures);
      return 1;
   }
#ifdef __APPLE__
   printf("PASS btstack_hid_test (run loop): power on and off, off before"
         " the loop returns at once, the loop's source and timer released\n");
#else
   printf("PASS btstack_hid_test (no run loop): power on and off\n");
#endif
   return 0;
}
