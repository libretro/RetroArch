/* Oracle for the NAT traversal task's request serialisation
 * (tasks/task_netplay_nat_traversal.c), compiled from the shipping
 * translation unit against the real task queue.
 *
 * Only one NAT task may run at a time, because both requests mutate
 * the single shared nat_traversal_request object.  That exclusion
 * used to be enforced by blocking the calling thread - the one
 * driving the menu - until the in-flight task finished, which for
 * UPnP discovery is seconds of frozen UI.  Requests are now queued
 * and started from the task callbacks instead.
 *
 * What these lanes pin:
 *
 *   exclusion  - a push issued while a task is in flight does not
 *                start a second task, and does not block: it returns
 *                immediately, before the in-flight task has run to
 *                completion.
 *   ordering   - a close followed by an open runs as both, in that
 *                order.  Coalescing them would leave the port
 *                mapping the close was meant to release open.
 *   resumption - the queued request actually starts once the
 *                in-flight task retires, for a task that finishes
 *                through either callback (the open task's, which
 *                also reports the result, and the close task's,
 *                which exists only to resume the queue).
 *   overflow   - a request beyond the queue's depth is refused
 *                rather than silently dropped.
 *
 * The natt_* device layer is stubbed with a deterministic fake that
 * takes a fixed number of handler invocations to complete, standing
 * in for the network round trips.  The task queue is the real one,
 * so the assumption the design rests on - that a task's callback
 * runs after the task has been popped off the queue, leaving the
 * queue free for the resumed push - is exercised rather than
 * assumed.
 *
 * The serialisation lanes drive the non-threaded task queue
 * explicitly.  One lane runs the threaded queue with the fake's
 * operations made asynchronous, as natt.c's are: the device is busy
 * until a transfer's callback clears it on the main thread and wakes
 * the task, and meanwhile the task parks rather than spinning. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <string/stdstring.h>
#include <queues/task_queue.h>
#include <retro_atomic.h>
#include <retro_timers.h>
#include <features/features_cpu.h>

#include "../../../network/natt.h"
#include "../../../network/netplay/netplay.h"
#include "../../../tasks/tasks_internal.h"
#include <compat/strl.h>

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
         fprintf(stderr, __VA_ARGS__); \
         fprintf(stderr, "\n"); \
         failures++; \
      } \
   } while (0)

/* ------------------------------------------------------------------ */
/* Fake NAT device layer                                              */
/* ------------------------------------------------------------------ */

/* Handler invocations still needed before the current operation
 * reports success - the stand-in for network round trips. */
static int      fake_steps_remaining;
static unsigned fake_open_calls;
static unsigned fake_close_calls;

/* Observed order of completed operations, e.g. "CO" for a close
 * followed by an open. */
static char     fake_order[16];
static size_t   fake_order_len;

static void fake_record(char c)
{
   if (fake_order_len + 1 < sizeof(fake_order))
      fake_order[fake_order_len++] = c;
   fake_order[fake_order_len] = '\0';
}

/* Asynchronous mode, for the threaded lane: an operation leaves the
 * device busy and hands completion to a task standing in for the
 * transfer, whose callback clears busy and wakes the waiter, as
 * natt.c's callbacks do. */
static int fake_async = 0;

static void fake_transfer_handler(retro_task_t *task)
{
   retro_time_t *due = (retro_time_t*)task->state;
   if (cpu_features_get_time_usec() >= *due)
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
   else
      retro_sleep(1);
}

static void fake_transfer_done(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   struct natt_device *device = (struct natt_device*)user_data;
   (void)task_data;
   (void)err;
   free(task->state);
   task->state = NULL;
   retro_atomic_store_release_int(&device->busy, 0);
   task_queue_wake(device->waiter);
}

static void fake_complete(struct natt_device *device)
{
   retro_task_t *task;
   retro_time_t *due;
   if (     !fake_async
         || !(task = task_init())
         || !(due  = (retro_time_t*)malloc(sizeof(*due))))
   {
      retro_atomic_store_release_int(&device->busy, 0);
      return;
   }
   *due            = cpu_features_get_time_usec() + 20000;
   task->state     = due;
   task->handler   = fake_transfer_handler;
   task->callback  = fake_transfer_done;
   task->user_data = device;
   task->flags    |= RETRO_TASK_FLG_MUTE;
   retro_atomic_store_release_int(&device->busy, 1);
   task_queue_push(task);
}

/* The handler's parks, counted on the way into the real queue. */
static retro_atomic_int_t parks;
void __real_task_queue_park(retro_task_t *task, retro_time_t horizon_usec);
void __wrap_task_queue_park(retro_task_t *task, retro_time_t horizon_usec)
{
   retro_atomic_fetch_add_int(&parks, 1);
   __real_task_queue_park(task, horizon_usec);
}

bool natt_init(struct natt_discovery *discovery)
{
   return true;
}

bool natt_device_next(struct natt_discovery *discovery,
      struct natt_device *device)
{
   /* Returning false means discovery is OVER - the handler ends the
    * task.  "Still searching" is a device with no description: the
    * handler skips it and comes back on its next invocation, which
    * is what makes the fake take a known number of ticks. */
   /* natt.c resets what it parses, not the waiter or busy. */
   uint32_t waiter = device->waiter;
   if (fake_steps_remaining > 0)
   {
      fake_steps_remaining--;
      memset(device, 0, sizeof(*device));
      device->waiter = waiter;
      return true;
   }
   memset(device, 0, sizeof(*device));
   device->waiter = waiter;
   /* Enough of a device for the handler to accept: a description, a
    * service type, and a loopback address that find_local_address()
    * can connect a UDP socket to in order to learn a local address. */
   strlcpy_lit(device->desc, "fake-device", sizeof(device->desc));
   strlcpy_lit(device->service_type, "fake-service",
         sizeof(device->service_type));
   strlcpy_lit(device->control, "fake-control", sizeof(device->control));
   device->addr.sin_family      = AF_INET;
   device->addr.sin_port        = htons(1900);
   device->addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   retro_atomic_store_release_int(&device->busy, 0);
   return true;
}

void natt_device_end(struct natt_discovery *discovery)
{
}

bool natt_query_device(struct natt_device *device)
{
   fake_complete(device);
   return true;
}

bool natt_external_address(struct natt_device *device)
{
   device->ext_addr.sin_family = AF_INET;
   fake_complete(device);
   return true;
}

bool natt_open_port(struct natt_device *device,
      struct natt_request *request, enum natt_forward_type forward_type)
{
   fake_open_calls++;
   fake_record('O');
   fake_complete(device);
   /* The real natt_open_port publishes the device on the request;
    * the close path validates against it. */
   request->device  = device;
   request->success = true;
   return true;
}

bool natt_close_port(struct natt_device *device,
      struct natt_request *request)
{
   fake_close_calls++;
   fake_record('C');
   fake_complete(device);
   request->success = true;
   return true;
}

/* ------------------------------------------------------------------ */
/* Netplay stub                                                       */
/* ------------------------------------------------------------------ */

static unsigned finished_notifications;

bool netplay_driver_ctl(enum rarch_netplay_ctl_state state, void *data)
{
   if (state == RARCH_NETPLAY_CTL_FINISHED_NAT_TRAVERSAL)
      finished_notifications++;
   return true;
}

/* ------------------------------------------------------------------ */

static struct nat_traversal_data req;

static void reset_all(void)
{
   /* Drain anything left over so lanes do not leak state into each
    * other. */
   task_queue_check();

   memset(&req, 0, sizeof(req));
   fake_steps_remaining   = 0;
   fake_open_calls        = 0;
   fake_close_calls       = 0;
   fake_order_len         = 0;
   fake_order[0]          = '\0';
   finished_notifications = 0;
}

static bool any_nat_task_finder(retro_task_t *task, void *userdata)
{
   return true;
}

/* True while any task is still queued or running. */
static bool queue_busy(void)
{
   task_finder_data_t find_data;
   find_data.func     = any_nat_task_finder;
   find_data.userdata = NULL;
   return task_queue_find(&find_data);
}

/* Run the queue until it is idle, or the guard trips. */
static unsigned pump(unsigned max_ticks)
{
   unsigned ticks = 0;
   while (ticks < max_ticks)
   {
      task_queue_check();
      ticks++;
      if (!queue_busy())
         break;
   }
   return ticks;
}

/* ------------------------------------------------------------------ */
/* Lanes                                                              */
/* ------------------------------------------------------------------ */

static void lane_open_completes(void)
{
   unsigned had = failures;

   reset_all();
   fake_steps_remaining = 3;

   CHECK(task_push_netplay_nat_traversal(&req, 55435),
         "open push refused");
   pump(64);

   CHECK(fake_open_calls == 1, "expected one open, got %u",
         fake_open_calls);
   CHECK(finished_notifications == 1,
         "expected one completion notification, got %u",
         finished_notifications);
   CHECK(req.status == NAT_TRAVERSAL_STATUS_OPENED,
         "expected OPENED, got %d", (int)req.status);

   if (failures == had)
      fprintf(stderr, "[pass] open lane\n");
}

static void lane_push_while_busy_does_not_block(void)
{
   unsigned had = failures;

   reset_all();
   /* Long enough that the task is unmistakably still in flight. */
   fake_steps_remaining = 8;

   CHECK(task_push_netplay_nat_traversal(&req, 55435),
         "first open push refused");

   /* One tick: the task has started but cannot have finished. */
   task_queue_check();
   CHECK(fake_open_calls == 0, "task finished too early to test this");

   /* The second push must return without running the first task to
    * completion - that is the whole point.  If it blocked, the open
    * would already have happened by the time it returns. */
   CHECK(task_push_netplay_nat_traversal(&req, 55436),
         "queued open push refused");
   CHECK(fake_open_calls == 0,
         "push blocked until the in-flight task finished");

   pump(128);
   CHECK(fake_open_calls == 2,
         "expected the queued open to run too, got %u opens",
         fake_open_calls);

   if (failures == had)
      fprintf(stderr, "[pass] non-blocking push lane\n");
}

static void lane_close_then_open_order(void)
{
   unsigned had = failures;

   reset_all();
   fake_steps_remaining = 4;

   /* An open runs to completion first, so a close has something to
    * close. */
   CHECK(task_push_netplay_nat_traversal(&req, 55435), "open refused");
   pump(64);
   CHECK(req.status == NAT_TRAVERSAL_STATUS_OPENED, "not opened");

   /* Now start another open and, while it is in flight, ask for a
    * close and then another open.  All three must run, in order. */
   fake_steps_remaining = 8;
   CHECK(task_push_netplay_nat_traversal(&req, 55437),
         "second open refused");
   task_queue_check();

   CHECK(task_push_netplay_nat_close(&req), "queued close refused");
   CHECK(task_push_netplay_nat_traversal(&req, 55438),
         "queued open refused");

   pump(256);

   /* "O" from the first open, then the in-flight open, the close,
    * and the trailing open - in exactly that order.  A design that
    * coalesced the close away would show "OOO". */
   CHECK(strcmp(fake_order, "OOCO") == 0,
         "operation order was \"%s\", expected \"OOCO\"", fake_order);

   if (failures == had)
      fprintf(stderr, "[pass] ordering lane\n");
}

static void lane_overflow_refused(void)
{
   unsigned had = failures;
   unsigned i;
   unsigned accepted = 0;

   reset_all();
   fake_steps_remaining = 16;

   CHECK(task_push_netplay_nat_traversal(&req, 55435), "open refused");
   task_queue_check();

   /* Queue depth is finite: past it, pushes must report failure
    * rather than drop the request on the floor. */
   for (i = 0; i < 16; i++)
      if (task_push_netplay_nat_traversal(&req, (uint16_t)(55500 + i)))
         accepted++;

   CHECK(accepted > 0, "no request could be queued at all");
   CHECK(accepted < 16, "queue accepted every request - overflow is "
         "not being refused");

   pump(512);
   /* Everything accepted must actually have run: the in-flight one
    * plus each queued one. */
   CHECK(fake_open_calls == accepted + 1,
         "ran %u opens for %u accepted queued requests",
         fake_open_calls, accepted);

   if (failures == had)
      fprintf(stderr, "[pass] overflow lane\n");
}

/* Threaded queue, asynchronous device: each wait on a transfer parks
 * until the transfer's callback wakes it - well inside the one-second
 * watchdog - and discovery parks between looks. */
static void lane_threaded_waits_park(void)
{
   unsigned     had = failures;
   retro_time_t start;
   retro_time_t took;
   int          n;

   reset_all();
   task_queue_deinit();
   task_queue_init(true, NULL);
   fake_async           = 1;
   fake_steps_remaining = 2;
   retro_atomic_store_release_int(&parks, 0);

   start = cpu_features_get_time_usec();
   CHECK(task_push_netplay_nat_traversal(&req, 55435),
         "open push refused");
   while (queue_busy() && cpu_features_get_time_usec() - start < 5000000)
   {
      task_queue_check();
      retro_sleep(1);
   }
   took = cpu_features_get_time_usec() - start;
   n    = retro_atomic_load_acquire_int(&parks);

   CHECK(fake_open_calls == 1, "expected one open, got %u",
         fake_open_calls);
   CHECK(req.status == NAT_TRAVERSAL_STATUS_OPENED,
         "expected OPENED, got %d", (int)req.status);
   /* Two discovery looks and three transfers, each a park or more. */
   CHECK(n >= 5, "the task parked %d times", n);
   CHECK(took < 900000,
         "the transfers' callbacks did not wake the task (%lld us)",
         (long long)took);

   fake_async = 0;
   task_queue_deinit();
   task_queue_init(false, NULL);
   if (failures == had)
      fprintf(stderr, "[pass] threaded lane: %d parks, %lld us\n", n,
            (long long)took);
}

int main(void)
{
   task_queue_init(false, NULL);

   lane_open_completes();
   lane_push_while_busy_does_not_block();
   lane_close_then_open_order();
   lane_overflow_refused();
   lane_threaded_waits_park();

   task_queue_deinit();

   if (failures)
   {
      fprintf(stderr, "FAIL nat_task_serialisation_test: %u failures\n",
            failures);
      return 1;
   }
   fprintf(stderr, "PASS nat_task_serialisation_test\n");
   return 0;
}
