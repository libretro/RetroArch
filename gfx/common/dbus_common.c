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

#include <stdlib.h>
#include <stdint.h>

#include "dbus_common.h"

#ifdef RARCH_HAVE_DBUS_SCREENSAVER

#include <rthreads/rthreads.h>

#include "../../verbosity.h"

#define DBUS_SS_NAME   "org.freedesktop.ScreenSaver"
#define DBUS_SS_PATH   "/org/freedesktop/ScreenSaver"

/* The worker, one for the process, started on first use. The main
 * thread writes the request - open, want and a generation that moves on
 * every open - under the lock, which the worker holds only to read the
 * request or publish an answer, never across a D-Bus call. Opening and
 * closing the connection happen on the worker, one after the other. */
typedef struct dbus_ss_ctl
{
   slock_t *lock;
   scond_t *cond;
   unsigned gen;
   int      open;
   int      want;
   int      state;
} dbus_ss_ctl_t;

static dbus_ss_ctl_t *dbus_ss_ctl;

/* Inhibit, and on success the cookie it returned. */
static bool dbus_ss_inhibit(const rdbus_t *rd, rdbus_connection_t *conn,
      uint32_t *cookie)
{
   bool ok                = false;
   const char *app        = "RetroArch";
   const char *reason     = "Playing a game";
   rdbus_message_t *reply = NULL;
   rdbus_message_t *msg   = rd->message_new_method_call(DBUS_SS_NAME,
         DBUS_SS_PATH, DBUS_SS_NAME, "Inhibit");

   if (!msg)
      return false;

   if (     rd->message_append_args(msg,
               RDBUS_TYPE_STRING, &app,
               RDBUS_TYPE_STRING, &reason,
               RDBUS_TYPE_INVALID)
         && (reply = rd->connection_send_with_reply_and_block(conn,
               msg, RDBUS_TIMEOUT_USE_DEFAULT, NULL)))
   {
      rdbus_iter_t it;
      if (     rd->message_iter_init(reply, &it)
            && rd->message_iter_get_arg_type(&it) == RDBUS_TYPE_UINT32)
      {
         rd->message_iter_get_basic(&it, cookie);
         ok = true;
      }
      rd->message_unref(reply);
   }

   rd->message_unref(msg);
   return ok;
}

static void dbus_ss_uninhibit(const rdbus_t *rd, rdbus_connection_t *conn,
      uint32_t cookie)
{
   rdbus_message_t *reply = NULL;
   rdbus_message_t *msg   = rd->message_new_method_call(DBUS_SS_NAME,
         DBUS_SS_PATH, DBUS_SS_NAME, "UnInhibit");

   if (!msg)
      return;
   if (     rd->message_append_args(msg,
               RDBUS_TYPE_UINT32, &cookie,
               RDBUS_TYPE_INVALID)
         && (reply = rd->connection_send_with_reply_and_block(conn,
               msg, RDBUS_TIMEOUT_USE_DEFAULT, NULL)))
      rd->message_unref(reply);
   rd->message_unref(msg);
}

/* Publishes an answer, unless the main thread has asked something new
 * since it was worked out. */
static void dbus_ss_publish(dbus_ss_ctl_t *ctl, unsigned gen, int want,
      int state)
{
   slock_lock(ctl->lock);
   if (ctl->gen == gen && ctl->want == want)
      ctl->state = state;
   slock_unlock(ctl->lock);
}

static void dbus_ss_worker(void *data)
{
   rdbus_error_t err;
   dbus_ss_ctl_t *ctl       = (dbus_ss_ctl_t*)data;
   const rdbus_t *rd        = NULL;
   rdbus_connection_t *conn = NULL;
   uint32_t cookie          = 0;
   unsigned done_gen        = 0;
   int done_open            = 0;
   int done_want            = 0;

   sthread_setname("ra-dbus-ss");

   for (;;)
   {
      unsigned gen;
      int open;
      int want;

      slock_lock(ctl->lock);
      while (     ctl->gen  == done_gen
               && ctl->open == done_open
               && ctl->want == done_want)
         scond_wait(ctl->cond, ctl->lock);
      gen  = ctl->gen;
      open = ctl->open;
      want = ctl->want;
      slock_unlock(ctl->lock);

      /* A new generation, or closing: let go of the old connection. */
      if (conn && (!open || gen != done_gen))
      {
         if (cookie)
            dbus_ss_uninhibit(rd, conn, cookie);
         cookie = 0;
         rd->connection_close(conn);
         rd->connection_unref(conn);
         conn   = NULL;
      }

      if (open && gen != done_gen)
      {
         if (!rd && !(rd = rdbus_get()))
            RARCH_LOG("[DBus] libdbus is not available; the screensaver is not suspended through D-Bus.\n");
         else
         {
            rd->error_init(&err);
            if ((conn = rd->bus_get_private(RDBUS_BUS_SESSION, &err)))
               /* A lost session bus ends the connection, not RetroArch. */
               rd->connection_set_exit_on_disconnect(conn, 0);
            else
               RARCH_LOG("[DBus] No session bus; the screensaver is not suspended through D-Bus.\n");
            rd->error_free(&err);
         }
      }

      done_gen  = gen;
      done_open = open;
      done_want = want;

      if (!open)
         continue;

      if (want)
      {
         if (!conn)
            dbus_ss_publish(ctl, gen, want, DBUS_SCREENSAVER_FAILED);
         else if (cookie || dbus_ss_inhibit(rd, conn, &cookie))
         {
            RARCH_LOG("[DBus] Suspended screensaver via DBus.\n");
            dbus_ss_publish(ctl, gen, want, DBUS_SCREENSAVER_INHIBITED);
         }
         else
         {
            RARCH_LOG("[DBus] The session bus has no screensaver to suspend.\n");
            dbus_ss_publish(ctl, gen, want, DBUS_SCREENSAVER_FAILED);
         }
      }
      else if (cookie)
      {
         dbus_ss_uninhibit(rd, conn, cookie);
         cookie = 0;
      }
   }
}

void dbus_ensure_connection(void)
{
   dbus_ss_ctl_t *ctl = dbus_ss_ctl;

   if (!ctl)
   {
      sthread_t *thread;
      if (!(ctl = (dbus_ss_ctl_t*)calloc(1, sizeof(*ctl))))
         return;
      ctl->lock = slock_new();
      ctl->cond = scond_new();
      if (     !ctl->lock || !ctl->cond
            || !(thread = sthread_create(dbus_ss_worker, ctl)))
      {
         if (ctl->cond)
            scond_free(ctl->cond);
         if (ctl->lock)
            slock_free(ctl->lock);
         free(ctl);
         return;
      }
      /* Lives as long as the process; the block stays with it. */
      sthread_detach(thread);
      dbus_ss_ctl = ctl;
   }

   slock_lock(ctl->lock);
   if (!ctl->open)
   {
      ctl->gen++;
      ctl->open  = 1;
      ctl->want  = 0;
      ctl->state = DBUS_SCREENSAVER_PENDING;
      scond_signal(ctl->cond);
   }
   slock_unlock(ctl->lock);
}

void dbus_close_connection(void)
{
   dbus_ss_ctl_t *ctl = dbus_ss_ctl;

   if (!ctl)
      return;
   slock_lock(ctl->lock);
   ctl->open = 0;
   ctl->want = 0;
   scond_signal(ctl->cond);
   slock_unlock(ctl->lock);
}

bool dbus_suspend_screensaver(bool enable)
{
   bool asked         = false;
   dbus_ss_ctl_t *ctl = dbus_ss_ctl;

   if (!ctl)
      return false;
   slock_lock(ctl->lock);
   if (ctl->open)
   {
      asked = true;
      if (ctl->want != (int)enable)
      {
         ctl->want = enable ? 1 : 0;
         if (enable)
            ctl->state = DBUS_SCREENSAVER_PENDING;
         scond_signal(ctl->cond);
      }
   }
   slock_unlock(ctl->lock);
   return asked;
}

enum dbus_screensaver_state dbus_screensaver_state(void)
{
   int state          = DBUS_SCREENSAVER_FAILED;
   dbus_ss_ctl_t *ctl = dbus_ss_ctl;

   if (!ctl)
      return DBUS_SCREENSAVER_FAILED;
   slock_lock(ctl->lock);
   if (ctl->open)
      state = ctl->state;
   slock_unlock(ctl->lock);
   return (enum dbus_screensaver_state)state;
}

#endif
