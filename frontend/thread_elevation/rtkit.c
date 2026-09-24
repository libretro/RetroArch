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
 * asynchronous: the exchange, loading libdbus included, runs on a
 * detached thread of its own, so nothing waits on the bus or the
 * dynamic loader, and the outcome is only logged. */

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../gfx/common/dbus_runtime.h"

#if defined(RARCH_HAVE_DBUS_RUNTIME) && defined(HAVE_THREADS) && defined(__linux__)

#include <stdint.h>
#include <string.h>
#include <sys/resource.h>

#include <boolean.h>
#include <compat/strl.h>
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
 * the expected type. A failed call leaves its reason in 'err'. */
static bool dbus_rtkit_get_property(const rdbus_t *rd,
      rdbus_connection_t *conn, const char *prop, int type, void *out,
      rdbus_error_t *err)
{
   bool ok                = false;
   const char *iface      = DBUS_RTKIT_NAME;
   rdbus_message_t *reply = NULL;
   rdbus_message_t *msg   = rd->message_new_method_call(DBUS_RTKIT_NAME,
         DBUS_RTKIT_PATH, "org.freedesktop.DBus.Properties", "Get");

   if (!msg)
      return false;

   if (     rd->message_append_args(msg,
               RDBUS_TYPE_STRING, &iface,
               RDBUS_TYPE_STRING, &prop,
               RDBUS_TYPE_INVALID)
         && (reply = rd->connection_send_with_reply_and_block(conn,
               msg, RDBUS_TIMEOUT_USE_DEFAULT, err)))
   {
      rdbus_iter_t it;
      rdbus_iter_t var;
      if (     rd->message_iter_init(reply, &it)
            && rd->message_iter_get_arg_type(&it) == RDBUS_TYPE_VARIANT)
      {
         rd->message_iter_recurse(&it, &var);
         if (rd->message_iter_get_arg_type(&var) == type)
         {
            rd->message_iter_get_basic(&var, out);
            ok = true;
         }
      }
      rd->message_unref(reply);
   }

   rd->message_unref(msg);
   return ok;
}

/* One call on rtkit's interface taking the thread id and one argument
 * of 'type'; true when rtkit granted it. A refusal leaves its reason
 * in 'err'. */
static bool dbus_rtkit_call(const rdbus_t *rd, rdbus_connection_t *conn,
      const char *method, uint64_t tid, int type, const void *arg,
      rdbus_error_t *err)
{
   bool ok                = false;
   rdbus_message_t *reply = NULL;
   rdbus_message_t *msg   = rd->message_new_method_call(DBUS_RTKIT_NAME,
         DBUS_RTKIT_PATH, DBUS_RTKIT_NAME, method);

   if (!msg)
      return false;

   if (     rd->message_append_args(msg,
               RDBUS_TYPE_UINT64, &tid,
               type, arg,
               RDBUS_TYPE_INVALID)
         && (reply = rd->connection_send_with_reply_and_block(conn,
               msg, RDBUS_TIMEOUT_USE_DEFAULT, err)))
   {
      ok = true;
      rd->message_unref(reply);
   }

   rd->message_unref(msg);
   return ok;
}

/* Keeps the first refusal's reason for the log line, then clears the
 * error for the next call. */
static void dbus_rtkit_note(const rdbus_t *rd, rdbus_error_t *err,
      char *why, size_t why_len)
{
   if (!why[0] && err->name)
      strlcpy(why, err->name, why_len);
   rd->error_free(err);
}

static void dbus_rtkit_thread(void *data)
{
   char why[128];
   rdbus_error_t err;
   struct rlimit rl;
   int32_t max_prio          = 0;
   int32_t min_nice          = 0;
   int64_t rttime            = 0;
   uint64_t tid              = dbus_rtkit_tid;
   unsigned next             = dbus_rtkit_next;
   bool granted              = false;
   const rdbus_t *rd         = NULL;
   rdbus_connection_t *conn  = NULL;

   (void)data;
   why[0] = '\0';
   sthread_setname("ra-rtkit");

   if (!(rd = rdbus_get()))
   {
      RARCH_LOG("[DBus] RealtimeKit not reached: libdbus is not available.\n");
      goto done;
   }

   rd->error_init(&err);
   if (!(conn = rd->bus_get_private(RDBUS_BUS_SYSTEM, &err)))
   {
      RARCH_LOG("[DBus] RealtimeKit not reached: no system bus.\n");
      rd->error_free(&err);
      goto done;
   }
   rd->connection_set_exit_on_disconnect(conn, 0);

   /* The first call settles whether rtkit is there at all. */
   if (!dbus_rtkit_get_property(rd, conn, "MaxRealtimePriority",
            RDBUS_TYPE_INT32, &max_prio, &err))
   {
      if (     err.name
            && (   !strcmp(err.name, RDBUS_ERROR_SERVICE_UNKNOWN)
                || !strcmp(err.name, RDBUS_ERROR_NAME_HAS_NO_OWNER)))
         RARCH_LOG("[DBus] RealtimeKit is not running on the system bus.\n");
      else
         RARCH_LOG("[DBus] RealtimeKit did not answer for thread %lu (%s).\n",
               (unsigned long)tid, err.name ? err.name : "no reason given");
      rd->error_free(&err);
      goto close;
   }

   /* rtkit only grants real time to a process whose RLIMIT_RTTIME
    * hard limit is within its RTTimeUSecMax; a thread that then runs
    * that long without blocking gets SIGXCPU from the kernel. */
   if (     max_prio > 0
         && dbus_rtkit_get_property(rd, conn, "RTTimeUSecMax",
               RDBUS_TYPE_INT64, &rttime, &err)
         && rttime > 0
         && getrlimit(RLIMIT_RTTIME, &rl) == 0)
   {
      uint32_t prio = (uint32_t)(max_prio < 50 ? max_prio : 50);
      if (rl.rlim_max == RLIM_INFINITY || rl.rlim_max > (rlim_t)rttime)
      {
         rl.rlim_cur = (rlim_t)rttime;
         rl.rlim_max = (rlim_t)rttime;
         setrlimit(RLIMIT_RTTIME, &rl);
      }
      if (dbus_rtkit_call(rd, conn, "MakeThreadRealtime", tid,
               RDBUS_TYPE_UINT32, &prio, &err))
      {
         RARCH_LOG("[DBus] RealtimeKit made thread %lu real-time at priority %u.\n",
               (unsigned long)tid, (unsigned)prio);
         granted = true;
         goto close;
      }
   }
   dbus_rtkit_note(rd, &err, why, sizeof(why));

   if (dbus_rtkit_get_property(rd, conn, "MinNiceLevel",
            RDBUS_TYPE_INT32, &min_nice, &err))
   {
      int32_t nice_level = min_nice > -11 ? min_nice : -11;
      if (     nice_level < 0
            && dbus_rtkit_call(rd, conn, "MakeThreadHighPriority", tid,
               RDBUS_TYPE_INT32, &nice_level, &err))
      {
         RARCH_LOG("[DBus] RealtimeKit raised thread %lu to nice %d.\n",
               (unsigned long)tid, (int)nice_level);
         granted = true;
         goto close;
      }
   }
   dbus_rtkit_note(rd, &err, why, sizeof(why));

   RARCH_LOG("[DBus] RealtimeKit refused thread %lu (%s).\n",
         (unsigned long)tid, why[0] ? why : "no reason given");

close:
   rd->connection_close(conn);
   rd->connection_unref(conn);
done:
   retro_atomic_store_release_int(&dbus_rtkit_busy, 0);
   if (!granted)
      thread_elevation_continue(tid, next);
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
   true,
   false
};

#endif
