/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* Thread elevation through RealtimeKit on the system bus: real-time
 * round-robin up to rtkit's MaxRealtimePriority, else a nice value down
 * to its MinNiceLevel. Brokered - rtkit acts on a thread id - and
 * asynchronous: the exchange runs on a detached thread of its own, so
 * nothing waits on the bus, and the outcome is only logged. */

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#if defined(HAVE_DBUS) && defined(HAVE_THREADS) && defined(__linux__)

#include <stdint.h>
#include <sys/resource.h>
#include <dbus/dbus.h>

#include <boolean.h>
#include <rthreads/rthreads.h>
#include <retro_atomic.h>

#include "../thread_elevation.h"
#include "../../verbosity.h"

#ifndef RLIMIT_RTTIME
#define RLIMIT_RTTIME 15
#endif

#define DBUS_RTKIT_NAME  "org.freedesktop.RealtimeKit1"
#define DBUS_RTKIT_PATH  "/org/freedesktop/RealtimeKit1"

/* The one request in flight: non-zero while a helper thread owns it,
 * and the thread and chain position it was started for. */
static retro_atomic_int_t dbus_rtkit_busy;
static uint64_t           dbus_rtkit_tid;
static unsigned           dbus_rtkit_next;

/* A property of rtkit's own interface, read into 'out' when it carries
 * the expected type. */
static bool dbus_rtkit_get_property(DBusConnection *conn,
      const char *prop, int type, void *out)
{
   bool ok             = false;
   const char *iface   = DBUS_RTKIT_NAME;
   DBusMessage *reply  = NULL;
   DBusMessage *msg    = dbus_message_new_method_call(DBUS_RTKIT_NAME,
         DBUS_RTKIT_PATH, "org.freedesktop.DBus.Properties", "Get");

   if (!msg)
      return false;

   if (     dbus_message_append_args(msg,
               DBUS_TYPE_STRING, &iface,
               DBUS_TYPE_STRING, &prop,
               DBUS_TYPE_INVALID)
         && (reply = dbus_connection_send_with_reply_and_block(conn,
               msg, DBUS_TIMEOUT_USE_DEFAULT, NULL)))
   {
      DBusMessageIter it;
      DBusMessageIter var;
      if (     dbus_message_iter_init(reply, &it)
            && dbus_message_iter_get_arg_type(&it) == DBUS_TYPE_VARIANT)
      {
         dbus_message_iter_recurse(&it, &var);
         if (dbus_message_iter_get_arg_type(&var) == type)
         {
            dbus_message_iter_get_basic(&var, out);
            ok = true;
         }
      }
      dbus_message_unref(reply);
   }

   dbus_message_unref(msg);
   return ok;
}

/* One call on rtkit's interface taking the thread id and one argument
 * of 'type'; true when rtkit answered without an error. */
static bool dbus_rtkit_call(DBusConnection *conn, const char *method,
      dbus_uint64_t tid, int type, const void *arg)
{
   bool ok            = false;
   DBusMessage *reply = NULL;
   DBusMessage *msg   = dbus_message_new_method_call(DBUS_RTKIT_NAME,
         DBUS_RTKIT_PATH, DBUS_RTKIT_NAME, method);

   if (!msg)
      return false;

   if (     dbus_message_append_args(msg,
               DBUS_TYPE_UINT64, &tid,
               type, arg,
               DBUS_TYPE_INVALID)
         && (reply = dbus_connection_send_with_reply_and_block(conn,
               msg, DBUS_TIMEOUT_USE_DEFAULT, NULL)))
   {
      ok = dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_METHOD_RETURN;
      dbus_message_unref(reply);
   }

   dbus_message_unref(msg);
   return ok;
}

static void dbus_rtkit_thread(void *data)
{
   bool granted          = false;
   DBusError err;
   struct rlimit rl;
   dbus_int32_t max_prio  = 0;
   dbus_int32_t min_nice  = 0;
   dbus_int64_t rttime    = 0;
   dbus_uint64_t tid      = (dbus_uint64_t)dbus_rtkit_tid;
   unsigned next          = dbus_rtkit_next;
   DBusConnection *conn;

   (void)data;

   sthread_setname("ra-rtkit");
   dbus_error_init(&err);

   if (!(conn = dbus_bus_get_private(DBUS_BUS_SYSTEM, &err)))
   {
      RARCH_LOG("[DBus] RealtimeKit not reached: no system bus.\n");
      dbus_error_free(&err);
      retro_atomic_store_release_int(&dbus_rtkit_busy, 0);
      thread_elevation_continue((uint64_t)tid, next);
      return;
   }
   dbus_connection_set_exit_on_disconnect(conn, false);

   /* rtkit only grants real time to a process whose RLIMIT_RTTIME
    * hard limit is within its RTTimeUSecMax; a thread that then runs
    * that long without blocking gets SIGXCPU from the kernel. */
   if (     dbus_rtkit_get_property(conn, "MaxRealtimePriority",
               DBUS_TYPE_INT32, &max_prio)
         && dbus_rtkit_get_property(conn, "RTTimeUSecMax",
               DBUS_TYPE_INT64, &rttime)
         && max_prio > 0
         && rttime   > 0
         && getrlimit(RLIMIT_RTTIME, &rl) == 0)
   {
      dbus_uint32_t prio = (dbus_uint32_t)(max_prio < 50 ? max_prio : 50);
      if (rl.rlim_max == RLIM_INFINITY || rl.rlim_max > (rlim_t)rttime)
      {
         rl.rlim_cur = (rlim_t)rttime;
         rl.rlim_max = (rlim_t)rttime;
         setrlimit(RLIMIT_RTTIME, &rl);
      }
      if (dbus_rtkit_call(conn, "MakeThreadRealtime", tid,
               DBUS_TYPE_UINT32, &prio))
      {
         RARCH_LOG("[DBus] RealtimeKit made thread %lu real-time at priority %u.\n",
               (unsigned long)tid, (unsigned)prio);
         granted = true;
         goto end;
      }
   }

   if (dbus_rtkit_get_property(conn, "MinNiceLevel",
            DBUS_TYPE_INT32, &min_nice))
   {
      dbus_int32_t nice_level = min_nice > -11 ? min_nice : -11;
      if (     nice_level < 0
            && dbus_rtkit_call(conn, "MakeThreadHighPriority", tid,
               DBUS_TYPE_INT32, &nice_level))
      {
         RARCH_LOG("[DBus] RealtimeKit raised thread %lu to nice %d.\n",
               (unsigned long)tid, (int)nice_level);
         granted = true;
         goto end;
      }
   }

   RARCH_LOG("[DBus] RealtimeKit refused thread %lu or is not running.\n",
         (unsigned long)tid);

end:
   dbus_connection_close(conn);
   dbus_connection_unref(conn);
   retro_atomic_store_release_int(&dbus_rtkit_busy, 0);
   if (!granted)
      thread_elevation_continue((uint64_t)tid, next);
}

static enum thread_elevation_result dbus_rtkit_raise(uint64_t tid,
      unsigned next)
{
   sthread_t *thread;

   /* One request in flight: a second is refused, and the chain carries
    * on to whatever follows. */
   if (retro_atomic_exchange_int(&dbus_rtkit_busy, 1))
      return THREAD_ELEVATION_REFUSED;

   /* Written before the helper exists, read only by it. */
   dbus_rtkit_tid  = tid;
   dbus_rtkit_next = next;

   dbus_threads_init_default();
   if (!(thread = sthread_create(dbus_rtkit_thread, NULL)))
   {
      retro_atomic_store_release_int(&dbus_rtkit_busy, 0);
      return THREAD_ELEVATION_REFUSED;
   }
   sthread_detach(thread);
   return THREAD_ELEVATION_PENDING;
}

const thread_elevation_backend_t thread_elevation_rtkit = {
   dbus_rtkit_raise,
   "RealtimeKit",
   true
};

#endif
