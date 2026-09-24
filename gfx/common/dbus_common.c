/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_DBUS
#include <dbus/dbus.h>
/* TODO/FIXME - static globals */
static DBusConnection* dbus_connection      = NULL;
static unsigned int dbus_screensaver_cookie = 0;
#endif

#include "../../verbosity.h"
#include "dbus_common.h"

#ifdef HAVE_DBUS_RTKIT
#include <stdint.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <rthreads/rthreads.h>
#include <retro_atomic.h>

#ifndef RLIMIT_RTTIME
#define RLIMIT_RTTIME 15
#endif

#define DBUS_RTKIT_NAME  "org.freedesktop.RealtimeKit1"
#define DBUS_RTKIT_PATH  "/org/freedesktop/RealtimeKit1"

/* Non-zero while a helper thread owns the one request in flight. */
static retro_atomic_int_t dbus_rtkit_busy;
#endif

void dbus_ensure_connection(void)
{
#ifdef HAVE_DBUS
    DBusError err;

    dbus_error_init(&err);

    dbus_connection = dbus_bus_get_private(DBUS_BUS_SESSION, &err);

    if (dbus_error_is_set(&err))
    {
        RARCH_LOG("[DBus] Failed to get DBus connection. Screensaver will not be suspended via DBus.\n");
        dbus_error_free(&err);
    }

    if (dbus_connection)
        dbus_connection_set_exit_on_disconnect(dbus_connection, true);
#endif
}

void dbus_close_connection(void)
{
#ifdef HAVE_DBUS
   if (!dbus_connection)
      return;

   dbus_connection_close(dbus_connection);
   dbus_connection_unref(dbus_connection);
   dbus_connection = NULL;
#endif
}

bool dbus_screensaver_inhibit(void)
{
   bool ret           = false;
#ifdef HAVE_DBUS
   const char *app    = "RetroArch";
   const char *reason = "Playing a game";
   DBusMessage   *msg = NULL;
   DBusMessage *reply = NULL;

   if (!dbus_connection)
      return false; /* DBus connection was not obtained */

   if (dbus_screensaver_cookie > 0)
      return true; /* Already inhibited */

   msg = dbus_message_new_method_call("org.freedesktop.ScreenSaver",
         "/org/freedesktop/ScreenSaver",
         "org.freedesktop.ScreenSaver",
         "Inhibit");

   if (!msg)
      return false;

   if (!dbus_message_append_args(msg,
            DBUS_TYPE_STRING, &app,
            DBUS_TYPE_STRING, &reason,
            DBUS_TYPE_INVALID))
   {
      dbus_message_unref(msg);
      return false;
   }

   reply = dbus_connection_send_with_reply_and_block(dbus_connection,
         msg, 300, NULL);

   if (reply)
   {
      if (!dbus_message_get_args(reply, NULL,
               DBUS_TYPE_UINT32, &dbus_screensaver_cookie,
               DBUS_TYPE_INVALID))
         dbus_screensaver_cookie = 0;
      else
         ret = true;

      dbus_message_unref(reply);
   }

   dbus_message_unref(msg);

   if (dbus_screensaver_cookie == 0)
   {
      RARCH_ERR("[DBus] Failed to suspend screensaver via DBus.\n");
   }
   else
   {
      RARCH_LOG("[DBus] Suspended screensaver via DBus.\n");
   }

#endif

   return ret;
}

void dbus_screensaver_uninhibit(void)
{
#ifdef HAVE_DBUS
   DBusMessage *msg = NULL;

   if (!dbus_connection)
      return;

   if (dbus_screensaver_cookie == 0)
      return;

   msg = dbus_message_new_method_call("org.freedesktop.ScreenSaver",
         "/org/freedesktop/ScreenSaver",
         "org.freedesktop.ScreenSaver",
         "UnInhibit");
   if (!msg)
       return;

   dbus_message_append_args(msg,
         DBUS_TYPE_UINT32, &dbus_screensaver_cookie,
         DBUS_TYPE_INVALID);

   if (dbus_connection_send(dbus_connection, msg, NULL))
      dbus_connection_flush(dbus_connection);
   dbus_message_unref(msg);

   dbus_screensaver_cookie = 0;
#endif
}

/* Returns false when fallback should be attempted */
bool dbus_suspend_screensaver(bool enable)
{
#ifdef HAVE_DBUS
   if (enable)
      return dbus_screensaver_inhibit();
   dbus_screensaver_uninhibit();
#endif
   return false;
}

#ifdef HAVE_DBUS_RTKIT
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
   DBusError err;
   struct rlimit rl;
   dbus_int32_t max_prio  = 0;
   dbus_int32_t min_nice  = 0;
   dbus_int64_t rttime    = 0;
   dbus_uint64_t tid      = (dbus_uint64_t)(uintptr_t)data;
   DBusConnection *conn;

   sthread_setname("ra-rtkit");
   dbus_error_init(&err);

   if (!(conn = dbus_bus_get_private(DBUS_BUS_SYSTEM, &err)))
   {
      RARCH_LOG("[DBus] RealtimeKit not reached: no system bus.\n");
      dbus_error_free(&err);
      retro_atomic_store_release_int(&dbus_rtkit_busy, 0);
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
         goto end;
      }
   }

   RARCH_LOG("[DBus] RealtimeKit refused thread %lu or is not running.\n",
         (unsigned long)tid);

end:
   dbus_connection_close(conn);
   dbus_connection_unref(conn);
   retro_atomic_store_release_int(&dbus_rtkit_busy, 0);
}

bool dbus_rtkit_raise_current_thread(void)
{
   sthread_t *thread;
   uintptr_t tid = (uintptr_t)syscall(SYS_gettid);

   if (retro_atomic_exchange_int(&dbus_rtkit_busy, 1))
      return false;

   dbus_threads_init_default();
   if (!(thread = sthread_create(dbus_rtkit_thread, (void*)tid)))
   {
      retro_atomic_store_release_int(&dbus_rtkit_busy, 0);
      return false;
   }
   sthread_detach(thread);
   return true;
}
#endif
