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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "gamemode_linux.h"

#ifdef RARCH_HAVE_GAMEMODE_LINUX

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/syscall.h>

#include <boolean.h>

#include "../../gfx/common/dbus_runtime.h"
#include "../../verbosity.h"

#if defined(RARCH_HAVE_DBUS_RUNTIME) && defined(HAVE_THREADS)
#include <rthreads/rthreads.h>
#include <retro_atomic.h>
#define GAMEMODE_HAVE_PPD 1
#endif

/* glibc declares syscall() only for _DEFAULT_SOURCE, which a C89 build
 * does not get. */
extern long syscall(long number, ...);

/* ---------------------------------------------------------------------
 * I/O priority
 * ------------------------------------------------------------------- */

/* <linux/ioprio.h> is not in every libc's headers; the ABI is fixed. */
#define GAMEMODE_IOPRIO_WHO_PROCESS 1
#define GAMEMODE_IOPRIO_CLASS_SHIFT 13
#define GAMEMODE_IOPRIO_CLASS_BE    2
#define GAMEMODE_IOPRIO_BE0 \
   (GAMEMODE_IOPRIO_CLASS_BE << GAMEMODE_IOPRIO_CLASS_SHIFT)

/* The main thread's priority before Game Mode, put back on leaving.
 * Only the thread that toggles Game Mode touches these. */
static int  gamemode_ioprio_saved;
static bool gamemode_ioprio_set_by_us;

#if defined(SYS_ioprio_set) && defined(SYS_ioprio_get)
/* ioprio_set() acts on one thread, so every thread present gets it;
 * threads created later inherit it from their creator. Best-effort
 * levels need no privilege. False when not one thread took it. */
static bool gamemode_ioprio_apply(int prio)
{
   bool any = false;
   DIR *dir = opendir("/proc/self/task");

   if (!dir)
      return syscall(SYS_ioprio_set, GAMEMODE_IOPRIO_WHO_PROCESS, 0,
            prio) == 0;

   for (;;)
   {
      long tid;
      char *end;
      struct dirent *ent = readdir(dir);
      if (!ent)
         break;
      if (ent->d_name[0] < '0' || ent->d_name[0] > '9')
         continue;
      tid = strtol(ent->d_name, &end, 10);
      if (*end || tid <= 0)
         continue;
      /* A thread that ended since the listing is no failure. */
      if (syscall(SYS_ioprio_set, GAMEMODE_IOPRIO_WHO_PROCESS, tid,
               prio) == 0)
         any = true;
   }

   closedir(dir);
   return any;
}

static bool gamemode_ioprio_enter(void)
{
   long cur;

   if (gamemode_ioprio_set_by_us)
      return true;
   if ((cur = syscall(SYS_ioprio_get, GAMEMODE_IOPRIO_WHO_PROCESS, 0)) < 0)
      cur = 0; /* none: derived from the nice value */
   if (!gamemode_ioprio_apply(GAMEMODE_IOPRIO_BE0))
   {
      RARCH_WARN("[GameMode] Could not raise the I/O priority.\n");
      return false;
   }
   gamemode_ioprio_saved     = (int)cur;
   gamemode_ioprio_set_by_us = true;
   RARCH_LOG("[GameMode] I/O priority raised to best-effort level 0.\n");
   return true;
}

static void gamemode_ioprio_leave(void)
{
   if (!gamemode_ioprio_set_by_us)
      return;
   gamemode_ioprio_apply(gamemode_ioprio_saved);
   gamemode_ioprio_set_by_us = false;
}
#else
static bool gamemode_ioprio_enter(void) { return false; }
static void gamemode_ioprio_leave(void) { }
#endif

/* ---------------------------------------------------------------------
 * Performance profile hold through power-profiles-daemon
 * ------------------------------------------------------------------- */

#ifdef GAMEMODE_HAVE_PPD

/* The interface's current name, then the one ppd used before 0.20. */
static const char *const gamemode_ppd_names[2] = {
   "org.freedesktop.UPower.PowerProfiles",
   "net.hadess.PowerProfiles"
};
static const char *const gamemode_ppd_paths[2] = {
   "/org/freedesktop/UPower/PowerProfiles",
   "/net/hadess/PowerProfiles"
};

/* One word: the hold wanted, and whether a helper is at work. Callers
 * set WANT and start a helper only when BUSY was clear; the helper
 * clears BUSY only when what it last did still matches WANT, so a
 * change made while it worked is never lost and no lock is needed. */
#define GAMEMODE_PPD_WANT (1 << 0)
#define GAMEMODE_PPD_BUSY (1 << 1)

static retro_atomic_int_t gamemode_ppd_state;

/* Owned by whichever helper holds BUSY; the CAS that hands BUSY over
 * orders them between helpers. The connection stays open between
 * helpers, since the hold lasts only as long as it does. */
static const rdbus_t      *gamemode_ppd_rd;
static rdbus_connection_t *gamemode_ppd_conn;
static uint32_t            gamemode_ppd_cookie;
static int                 gamemode_ppd_iface;
static bool                gamemode_ppd_held;
static bool                gamemode_ppd_gave_up;

static bool gamemode_ppd_unknown(const rdbus_error_t *err)
{
   return err->name
      && (   !strcmp(err->name, RDBUS_ERROR_SERVICE_UNKNOWN)
          || !strcmp(err->name, RDBUS_ERROR_NAME_HAS_NO_OWNER));
}

static void gamemode_ppd_close(const rdbus_t *rd)
{
   if (!gamemode_ppd_conn)
      return;
   rd->connection_close(gamemode_ppd_conn);
   rd->connection_unref(gamemode_ppd_conn);
   gamemode_ppd_conn = NULL;
}

static void gamemode_ppd_hold(const rdbus_t *rd)
{
   int i;
   rdbus_error_t err;
   const char *profile = "performance";
   const char *reason  = "Game Mode";
   const char *app     = "org.libretro.RetroArch";

   rd->error_init(&err);
   if (!gamemode_ppd_conn)
   {
      if (!(gamemode_ppd_conn = rd->bus_get_private(RDBUS_BUS_SYSTEM, &err)))
      {
         RARCH_LOG("[GameMode] No system bus; the performance profile is not held.\n");
         rd->error_free(&err);
         gamemode_ppd_gave_up = true;
         return;
      }
      /* A lost system bus ends the connection, not RetroArch. */
      rd->connection_set_exit_on_disconnect(gamemode_ppd_conn, 0);
   }

   for (i = 0; i < 2; i++)
   {
      rdbus_message_t *reply;
      rdbus_message_t *msg = rd->message_new_method_call(
            gamemode_ppd_names[i], gamemode_ppd_paths[i],
            gamemode_ppd_names[i], "HoldProfile");
      if (!msg)
         break;

      reply = NULL;
      if (rd->message_append_args(msg,
               RDBUS_TYPE_STRING, &profile,
               RDBUS_TYPE_STRING, &reason,
               RDBUS_TYPE_STRING, &app,
               RDBUS_TYPE_INVALID))
         reply = rd->connection_send_with_reply_and_block(gamemode_ppd_conn,
               msg, RDBUS_TIMEOUT_USE_DEFAULT, &err);
      rd->message_unref(msg);

      if (reply)
      {
         rdbus_iter_t it;
         uint32_t cookie = 0;
         if (     rd->message_iter_init(reply, &it)
               && rd->message_iter_get_arg_type(&it) == RDBUS_TYPE_UINT32)
            rd->message_iter_get_basic(&it, &cookie);
         rd->message_unref(reply);
         gamemode_ppd_cookie = cookie;
         gamemode_ppd_iface  = i;
         gamemode_ppd_held   = true;
         RARCH_LOG("[GameMode] Holding the performance power profile.\n");
         return;
      }

      if (!gamemode_ppd_unknown(&err))
      {
         /* There, but refused: no performance profile on this machine,
          * or polkit said no. Neither changes while we run. */
         RARCH_LOG("[GameMode] Power profiles daemon refused the performance profile (%s).\n",
               err.name ? err.name : "no reason given");
         rd->error_free(&err);
         gamemode_ppd_gave_up = true;
         gamemode_ppd_close(rd);
         return;
      }
      rd->error_free(&err);
   }

   RARCH_LOG("[GameMode] No power profiles daemon on the system bus; "
         "the CPU's power mode is left alone.\n");
   gamemode_ppd_gave_up = true;
   gamemode_ppd_close(rd);
}

static void gamemode_ppd_release(const rdbus_t *rd)
{
   rdbus_message_t *msg;

   if (!gamemode_ppd_held)
      return;
   gamemode_ppd_held = false;

   msg = rd->message_new_method_call(
         gamemode_ppd_names[gamemode_ppd_iface],
         gamemode_ppd_paths[gamemode_ppd_iface],
         gamemode_ppd_names[gamemode_ppd_iface], "ReleaseProfile");
   if (msg)
   {
      rdbus_message_t *reply = NULL;
      if (rd->message_append_args(msg,
               RDBUS_TYPE_UINT32, &gamemode_ppd_cookie,
               RDBUS_TYPE_INVALID))
         reply = rd->connection_send_with_reply_and_block(gamemode_ppd_conn,
               msg, RDBUS_TIMEOUT_USE_DEFAULT, NULL);
      if (reply)
         rd->message_unref(reply);
      rd->message_unref(msg);
   }

   /* Closing would drop the hold anyway; the explicit release keeps
    * ppd's log tidy. Nothing needs the connection until the next hold. */
   gamemode_ppd_close(rd);
   RARCH_LOG("[GameMode] Released the performance power profile.\n");
}

static void gamemode_ppd_thread(void *data)
{
   int state;
   (void)data;
   sthread_setname("ra-gamemode");

   if (!gamemode_ppd_rd && !(gamemode_ppd_rd = rdbus_get()))
   {
      RARCH_LOG("[GameMode] libdbus is not available; the performance profile is not held.\n");
      gamemode_ppd_gave_up = true;
   }

   for (;;)
   {
      bool want;

      state = retro_atomic_load_acquire_int(&gamemode_ppd_state);
      want  = (state & GAMEMODE_PPD_WANT) != 0;

      if (gamemode_ppd_rd)
      {
         if (want && !gamemode_ppd_held && !gamemode_ppd_gave_up)
            gamemode_ppd_hold(gamemode_ppd_rd);
         else if (!want)
            gamemode_ppd_release(gamemode_ppd_rd);
      }

      /* Done if WANT is still what was just served. */
      if (retro_atomic_cas_int(&gamemode_ppd_state, state,
               state & ~GAMEMODE_PPD_BUSY))
         break;
   }
}

static bool gamemode_ppd_request(bool want)
{
   sthread_t *thread;

   for (;;)
   {
      int cur  = retro_atomic_load_acquire_int(&gamemode_ppd_state);
      int next = (cur & ~GAMEMODE_PPD_WANT)
               | (want ? GAMEMODE_PPD_WANT : 0) | GAMEMODE_PPD_BUSY;
      if (cur == next)
         return true;
      if (!retro_atomic_cas_int(&gamemode_ppd_state, cur, next))
         continue;
      if (cur & GAMEMODE_PPD_BUSY)
         return true; /* the helper at work picks the change up */
      break;
   }

   if (!(thread = sthread_create(gamemode_ppd_thread, NULL)))
   {
      retro_atomic_store_release_int(&gamemode_ppd_state,
            want ? GAMEMODE_PPD_WANT : 0);
      return false;
   }
   sthread_detach(thread);
   return true;
}
#endif

bool gamemode_linux_set(bool on, bool exiting)
{
   bool ok;

   if (!on)
   {
      gamemode_ioprio_leave();
#ifdef GAMEMODE_HAVE_PPD
      /* Never started a helper: nothing to release. On exit the hold
       * ends with the process's bus connection. */
      if (     !exiting
            && retro_atomic_load_acquire_int(&gamemode_ppd_state))
         gamemode_ppd_request(false);
#endif
      (void)exiting;
      return true;
   }

   ok = gamemode_ioprio_enter();
#ifdef GAMEMODE_HAVE_PPD
   if (gamemode_ppd_request(true))
      ok = true;
#endif
   return ok;
}

#endif
