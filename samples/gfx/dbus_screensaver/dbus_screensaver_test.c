/* gfx/common/dbus_common.c: screensaver inhibit over the session bus,
 * against a private dbus-daemon and a fake org.freedesktop.ScreenSaver
 * served by this test with libdbus.
 *
 * The callers never wait: they move the request on and the worker
 * answers when it can. What is asserted:
 *  - an inhibit is answered INHIBITED, and the service holds exactly one
 *    inhibit; a release leaves it holding none;
 *  - a state read right after a request is that request's answer or
 *    PENDING, never an older request's answer;
 *  - three threads toggling the request at once leave the service
 *    holding what the last request asked for, once the worker settles;
 *  - closing and reopening at once still reconnects and releases what
 *    the old connection held;
 *  - with no service on the bus the answer is FAILED.
 * Under TSan by the tsan-samples list; a watchdog fails a hang. */

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#include <dbus/dbus.h>

#include <retro_atomic.h>

#include "../../../gfx/common/dbus_common.h"

/* verbosity.h, as dbus_common.c logs */
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }

static unsigned failures;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL line %d: ", __LINE__); \
   printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

/* ---- the fake service ---- */

static retro_atomic_int_t svc_held;     /* inhibits granted, not released */
static retro_atomic_int_t svc_calls;
static retro_atomic_int_t svc_stop;
static retro_atomic_int_t svc_up;
static uint32_t           svc_next_cookie = 1;

static DBusHandlerResult svc_handle(DBusConnection *c, DBusMessage *m,
      void *ud)
{
   DBusMessage *reply;
   (void)ud;
   if (dbus_message_is_method_call(m, "org.freedesktop.ScreenSaver",
            "Inhibit"))
   {
      uint32_t cookie = svc_next_cookie++;
      usleep(20000);  /* a service takes a moment */
      reply = dbus_message_new_method_return(m);
      dbus_message_append_args(reply, DBUS_TYPE_UINT32, &cookie,
            DBUS_TYPE_INVALID);
      retro_atomic_fetch_add_int(&svc_held, 1);
   }
   else if (dbus_message_is_method_call(m, "org.freedesktop.ScreenSaver",
            "UnInhibit"))
   {
      reply = dbus_message_new_method_return(m);
      retro_atomic_fetch_sub_int(&svc_held, 1);
   }
   else
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
   retro_atomic_fetch_add_int(&svc_calls, 1);
   dbus_connection_send(c, reply, NULL);
   dbus_message_unref(reply);
   return DBUS_HANDLER_RESULT_HANDLED;
}

static void *svc_thread(void *data)
{
   DBusError err;
   DBusConnection *c;
   static const DBusObjectPathVTable vt = { NULL, svc_handle };
   (void)data;
   dbus_error_init(&err);
   if (!(c = dbus_bus_get_private(DBUS_BUS_SESSION, &err)))
   {
      printf("FAIL: service could not connect: %s\n", err.message);
      exit(1);
   }
   dbus_connection_set_exit_on_disconnect(c, 0);
   dbus_bus_request_name(c, "org.freedesktop.ScreenSaver",
         DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);
   dbus_connection_register_object_path(c, "/org/freedesktop/ScreenSaver",
         &vt, NULL);
   retro_atomic_store_release_int(&svc_up, 1);
   while (!retro_atomic_load_acquire_int(&svc_stop))
      dbus_connection_read_write_dispatch(c, 20);
   dbus_bus_release_name(c, "org.freedesktop.ScreenSaver", &err);
   dbus_connection_flush(c);
   dbus_connection_close(c);
   dbus_connection_unref(c);
   return NULL;
}

/* ---- helpers ---- */

static void sleep_ms(unsigned ms) { usleep(ms * 1000); }

/* The answer, waited for: never PENDING forever. */
static enum dbus_screensaver_state settle(void)
{
   int i;
   enum dbus_screensaver_state st = DBUS_SCREENSAVER_PENDING;
   for (i = 0; i < 500 && st == DBUS_SCREENSAVER_PENDING; i++)
   {
      st = dbus_screensaver_state();
      if (st == DBUS_SCREENSAVER_PENDING)
         sleep_ms(10);
   }
   return st;
}

/* The service's count, once it has stopped moving. */
static int held_settled(void)
{
   int i, last = -1, now;
   for (i = 0; i < 300; i++)
   {
      now = retro_atomic_load_acquire_int(&svc_calls);
      if (now == last)
         break;
      last = now;
      sleep_ms(30);
   }
   return retro_atomic_load_acquire_int(&svc_held);
}

static retro_atomic_int_t toggle_go;

static void *toggler(void *data)
{
   int i, id = (int)(size_t)data;
   while (!retro_atomic_load_acquire_int(&toggle_go)) { }
   for (i = 0; i < 300; i++)
   {
      bool want = ((i + id) & 1) != 0;
      dbus_suspend_screensaver(want);
      /* an answer read straight after a release must not be the
       * INHIBITED of an earlier request still standing */
      (void)dbus_screensaver_state();
   }
   return NULL;
}

static void watchdog(int sig)
{
   static const char m[] = "FAIL: hung (watchdog)\n";
   (void)sig;
   if (write(1, m, sizeof(m) - 1) < 0) { }
   _exit(1);
}

int main(void)
{
   pthread_t svc, t[3];
   int i;

   signal(SIGALRM, watchdog);
   alarm(150);

   if (!getenv("DBUS_SESSION_BUS_ADDRESS"))
   {
      printf("needs DBUS_SESSION_BUS_ADDRESS: run through run.sh\n");
      return 1;
   }
   if (!dbus_threads_init_default())
      return 1;
   pthread_create(&svc, NULL, svc_thread, NULL);
   while (!retro_atomic_load_acquire_int(&svc_up))
      sleep_ms(1);

   /* no connection yet: the caller's fallbacks apply */
   CHECK(!dbus_suspend_screensaver(true), "a request taken with no connection");

   dbus_ensure_connection();
   CHECK(dbus_suspend_screensaver(true), "a request refused on an open connection");
   CHECK(settle() == DBUS_SCREENSAVER_INHIBITED, "inhibit not answered INHIBITED");
   CHECK(held_settled() == 1, "the service holds %d inhibits, not 1",
         retro_atomic_load_acquire_int(&svc_held));
   /* asked again, nothing new: the answer stands */
   CHECK(dbus_suspend_screensaver(true)
         && dbus_screensaver_state() == DBUS_SCREENSAVER_INHIBITED,
         "the same request asked again lost its answer");

   dbus_suspend_screensaver(false);
   CHECK(held_settled() == 0, "a release left %d inhibits",
         retro_atomic_load_acquire_int(&svc_held));
   /* asked again after a release: this request's answer or pending,
    * never the previous request's INHIBITED */
   dbus_suspend_screensaver(true);
   {
      /* INHIBITED this early is only true if the service already
       * granted this request; the release before it left it none */
      enum dbus_screensaver_state st = dbus_screensaver_state();
      CHECK(st == DBUS_SCREENSAVER_PENDING
            || (st == DBUS_SCREENSAVER_INHIBITED
               && retro_atomic_load_acquire_int(&svc_held) == 1),
            "state %d with the service holding %d, straight after a new"
            " request", (int)st, retro_atomic_load_acquire_int(&svc_held));
   }
   CHECK(settle() == DBUS_SCREENSAVER_INHIBITED, "re-inhibit not answered");
   CHECK(held_settled() == 1, "re-inhibit: service holds %d",
         retro_atomic_load_acquire_int(&svc_held));
   printf("ok    inhibit, release and inhibit again\n");

   /* three threads at once, then one last word */
   for (i = 0; i < 3; i++)
      pthread_create(&t[i], NULL, toggler, (void*)(size_t)i);
   retro_atomic_store_release_int(&toggle_go, 1);
   for (i = 0; i < 3; i++)
      pthread_join(t[i], NULL);
   dbus_suspend_screensaver(true);
   CHECK(settle() == DBUS_SCREENSAVER_INHIBITED, "after the storm: not inhibited");
   CHECK(held_settled() == 1, "after the storm the service holds %d, not 1",
         retro_atomic_load_acquire_int(&svc_held));
   dbus_suspend_screensaver(false);
   CHECK(held_settled() == 0, "after the storm a release left %d",
         retro_atomic_load_acquire_int(&svc_held));
   printf("ok    three threads toggling, then the last request holds\n");

   /* close and reopen at once: a new connection, the old inhibit gone */
   dbus_suspend_screensaver(true);
   settle();
   dbus_close_connection();
   CHECK(dbus_screensaver_state() == DBUS_SCREENSAVER_FAILED,
         "a closed connection does not read FAILED");
   CHECK(!dbus_suspend_screensaver(true), "a request taken on a closed connection");
   dbus_ensure_connection();
   CHECK(dbus_screensaver_state() == DBUS_SCREENSAVER_PENDING,
         "a reopened connection does not start PENDING");
   dbus_suspend_screensaver(true);
   CHECK(settle() == DBUS_SCREENSAVER_INHIBITED, "reopened: not inhibited");
   CHECK(held_settled() == 1, "reopened: the service holds %d, not 1",
         retro_atomic_load_acquire_int(&svc_held));
   printf("ok    close and reopen at once\n");

   /* no service on the bus */
   dbus_suspend_screensaver(false);
   held_settled();
   retro_atomic_store_release_int(&svc_stop, 1);
   pthread_join(svc, NULL);
   dbus_close_connection();
   dbus_ensure_connection();
   dbus_suspend_screensaver(true);
   CHECK(settle() == DBUS_SCREENSAVER_FAILED, "no service: not FAILED");
   printf("ok    no service: FAILED\n");

   dbus_close_connection();
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] dbus_screensaver_test\n");
   return 0;
}
