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

#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <rthreads/retro_eventcount.h>

#include "../../verbosity.h"

#define DBUS_SS_NAME   "org.freedesktop.ScreenSaver"
#define DBUS_SS_PATH   "/org/freedesktop/ScreenSaver"

/* The request, one word: whether a connection is wanted open, whether
 * the screensaver is wanted inhibited, the connection's generation -
 * moved on by every open - and a sequence moved on by every change, so
 * the same open and want asked twice is still two requests. */
#define DBUS_SS_OPEN       (1u << 0)
#define DBUS_SS_WANT       (1u << 1)
#define DBUS_SS_GEN_SHIFT  2
#define DBUS_SS_GEN_MASK   (0x3ffu << DBUS_SS_GEN_SHIFT)
#define DBUS_SS_SEQ_SHIFT  12
#define DBUS_SS_SEQ_ONE    (1u << DBUS_SS_SEQ_SHIFT)
/* The answer: the request it answers, low two bits replaced by the
 * state. One the request has since moved past reads as pending. */
#define DBUS_SS_TAG(req)   ((req) & ~3u)

/* The worker, one for the process, started on first use. Callers store
 * a new request and wake it; it works through the latest request it
 * finds, never holding anything across a D-Bus call, and stores the
 * answer tagged with that request. Opening and closing the connection
 * happen on the worker, one after the other. */
typedef struct dbus_ss_ctl
{
   retro_eventcount_t ec;
   retro_atomic_int_t req;
   retro_atomic_int_t answer;
} dbus_ss_ctl_t;

static retro_atomic_ptr_t dbus_ss_ctl_ptr;

static dbus_ss_ctl_t *dbus_ss_ctl_get(void)
{
   return (dbus_ss_ctl_t*)retro_atomic_load_acquire_ptr(&dbus_ss_ctl_ptr);
}

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

static void dbus_ss_publish(dbus_ss_ctl_t *ctl, unsigned req, int state)
{
   retro_atomic_store_release_int(&ctl->answer,
         (int)(DBUS_SS_TAG(req) | (unsigned)state));
}

static void dbus_ss_worker(void *data)
{
   rdbus_error_t err;
   dbus_ss_ctl_t *ctl       = (dbus_ss_ctl_t*)data;
   const rdbus_t *rd        = NULL;
   rdbus_connection_t *conn = NULL;
   uint32_t cookie          = 0;
   unsigned done            = 0;

   sthread_setname("ra-dbus-ss");

   for (;;)
   {
      unsigned req;
      bool open, want, new_gen;

      /* the next request */
      for (;;)
      {
         int key;
         if ((req = (unsigned)retro_atomic_load_acquire_int(&ctl->req))
               != done)
            break;
         key = retro_eventcount_prepare_wait(&ctl->ec);
         if ((req = (unsigned)retro_atomic_load_acquire_int(&ctl->req))
               != done)
         {
            retro_eventcount_cancel_wait(&ctl->ec);
            break;
         }
         retro_eventcount_commit_wait(&ctl->ec, key);
      }

      open    = (req & DBUS_SS_OPEN) != 0;
      want    = (req & DBUS_SS_WANT) != 0;
      new_gen = (req & DBUS_SS_GEN_MASK) != (done & DBUS_SS_GEN_MASK);

      /* A new generation, or closing: let go of the old connection. */
      if (conn && (!open || new_gen))
      {
         if (cookie)
            dbus_ss_uninhibit(rd, conn, cookie);
         cookie = 0;
         rd->connection_close(conn);
         rd->connection_unref(conn);
         conn   = NULL;
      }

      if (open && new_gen)
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

      done = req;

      if (!open)
         continue;

      if (want)
      {
         if (!conn)
            dbus_ss_publish(ctl, req, DBUS_SCREENSAVER_FAILED);
         else if (cookie || dbus_ss_inhibit(rd, conn, &cookie))
         {
            RARCH_LOG("[DBus] Suspended screensaver via DBus.\n");
            dbus_ss_publish(ctl, req, DBUS_SCREENSAVER_INHIBITED);
         }
         else
         {
            RARCH_LOG("[DBus] The session bus has no screensaver to suspend.\n");
            dbus_ss_publish(ctl, req, DBUS_SCREENSAVER_FAILED);
         }
      }
      else if (cookie)
      {
         dbus_ss_uninhibit(rd, conn, cookie);
         cookie = 0;
      }
   }
}

/* Moves the request on with @update, which returns the request it
 * makes of the current one, or the current one for no change. Returns
 * the request in force afterwards. Any thread may ask. */
static unsigned dbus_ss_request(dbus_ss_ctl_t *ctl,
      unsigned (*update)(unsigned cur, int arg), int arg)
{
   for (;;)
   {
      unsigned cur  = (unsigned)retro_atomic_load_acquire_int(&ctl->req);
      unsigned next = update(cur, arg);
      if (next == cur)
         return cur;
      next = (next & (DBUS_SS_SEQ_ONE - 1)) + (cur & ~(DBUS_SS_SEQ_ONE - 1))
           + DBUS_SS_SEQ_ONE;
      if (retro_atomic_cas_int(&ctl->req, (int)cur, (int)next))
      {
         retro_eventcount_notify(&ctl->ec);
         return next;
      }
   }
}

static unsigned dbus_ss_update_open(unsigned cur, int arg)
{
   (void)arg;
   if (cur & DBUS_SS_OPEN)
      return cur;
   return DBUS_SS_OPEN
      | (((cur & DBUS_SS_GEN_MASK) + (1u << DBUS_SS_GEN_SHIFT))
            & DBUS_SS_GEN_MASK);
}

static unsigned dbus_ss_update_close(unsigned cur, int arg)
{
   (void)arg;
   if (!(cur & DBUS_SS_OPEN))
      return cur;
   return cur & DBUS_SS_GEN_MASK;
}

static unsigned dbus_ss_update_want(unsigned cur, int want)
{
   if (     !(cur & DBUS_SS_OPEN)
         || ((cur & DBUS_SS_WANT) != 0) == (want != 0))
      return cur;
   return (cur & (DBUS_SS_OPEN | DBUS_SS_GEN_MASK))
      | (want ? DBUS_SS_WANT : 0);
}

void dbus_ensure_connection(void)
{
   dbus_ss_ctl_t *ctl = dbus_ss_ctl_get();

   if (!ctl)
   {
      sthread_t *thread;
      if (!(ctl = (dbus_ss_ctl_t*)calloc(1, sizeof(*ctl))))
         return;
      if (!retro_eventcount_init(&ctl->ec))
      {
         free(ctl);
         return;
      }
      if (!(thread = sthread_create(dbus_ss_worker, ctl)))
      {
         retro_eventcount_free(&ctl->ec);
         free(ctl);
         return;
      }
      /* Lives as long as the process; the block stays with it. */
      sthread_detach(thread);
      retro_atomic_store_release_ptr(&dbus_ss_ctl_ptr, ctl);
   }

   dbus_ss_request(ctl, dbus_ss_update_open, 0);
}

void dbus_close_connection(void)
{
   dbus_ss_ctl_t *ctl = dbus_ss_ctl_get();

   if (ctl)
      dbus_ss_request(ctl, dbus_ss_update_close, 0);
}

bool dbus_suspend_screensaver(bool enable)
{
   dbus_ss_ctl_t *ctl = dbus_ss_ctl_get();

   if (!ctl)
      return false;
   return (dbus_ss_request(ctl, dbus_ss_update_want, enable ? 1 : 0)
         & DBUS_SS_OPEN) != 0;
}

enum dbus_screensaver_state dbus_screensaver_state(void)
{
   unsigned req, answer;
   dbus_ss_ctl_t *ctl = dbus_ss_ctl_get();

   if (!ctl)
      return DBUS_SCREENSAVER_FAILED;
   req = (unsigned)retro_atomic_load_acquire_int(&ctl->req);
   if (!(req & DBUS_SS_OPEN))
      return DBUS_SCREENSAVER_FAILED;
   answer = (unsigned)retro_atomic_load_acquire_int(&ctl->answer);
   if (DBUS_SS_TAG(answer) != DBUS_SS_TAG(req))
      return DBUS_SCREENSAVER_PENDING;
   return (enum dbus_screensaver_state)(answer & 3u);
}

#endif
