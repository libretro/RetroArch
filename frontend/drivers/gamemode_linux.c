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
#include <stdio.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <retro_miscellaneous.h>
#include <compat/strl.h>
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
 * Performance profile hold through power-profiles-daemon, on AC only
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

#define GAMEMODE_UPOWER_NAME "org.freedesktop.UPower"
#define GAMEMODE_UPOWER_PATH "/org/freedesktop/UPower"
#define GAMEMODE_PROPS_IFACE "org.freedesktop.DBus.Properties"

/* Without UPower, the kernel's own uevents announce a plug change. Only
 * where that socket cannot be had is the power supply looked at again
 * this often instead. */
#define GAMEMODE_SYSFS_RECHECK_MS 5000

/* One word: Game Mode wanted, and whether the worker runs. Callers set
 * WANT and start a worker only when BUSY was clear, else wake it; the
 * worker clears BUSY only when WANT is still clear as it last read it,
 * so a change made while it worked is never lost and no lock is needed.
 * The worker lives only while Game Mode is on. */
#define GAMEMODE_PPD_WANT (1 << 0)
#define GAMEMODE_PPD_BUSY (1 << 1)

static retro_atomic_int_t gamemode_ppd_state;
/* Wake pipe: made on the first request, before any worker exists. */
static int                gamemode_ppd_wake[2] = { -1, -1 };

/* Owned by whichever worker holds BUSY; the CAS that hands BUSY over
 * orders them between workers. */
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

/* The first line of a small sysfs file, newline cut off. */
static bool gamemode_read_line(const char *name, const char *file,
      char *buf, size_t len)
{
   size_t n;
   FILE *f;
   char path[PATH_MAX_LENGTH];

   strlcpy(path, "/sys/class/power_supply/", sizeof(path));
   strlcat(path, name, sizeof(path));
   strlcat(path, "/", sizeof(path));
   strlcat(path, file, sizeof(path));
   if (!(f = fopen(path, "r")))
      return false;
   n = fread(buf, 1, len - 1, f);
   fclose(f);
   buf[n] = '\0';
   while (n && (buf[n - 1] == '\n' || buf[n - 1] == ' '))
      buf[--n] = '\0';
   return true;
}

/* Without UPower: on battery when a mains supply is present and none
 * is online, or when there is no mains supply and a battery is
 * discharging. A desktop with no supplies listed is on AC. */
static bool gamemode_sysfs_on_battery(void)
{
   bool mains = false, mains_online = false, discharging = false;
   DIR *dir   = opendir("/sys/class/power_supply");

   if (!dir)
      return false;
   for (;;)
   {
      char val[32];
      struct dirent *ent = readdir(dir);
      if (!ent)
         break;
      if (     ent->d_name[0] == '.'
            || !gamemode_read_line(ent->d_name, "type", val, sizeof(val)))
         continue;
      if (!strcmp(val, "Mains"))
      {
         mains = true;
         if (     gamemode_read_line(ent->d_name, "online", val, sizeof(val))
               && !strcmp(val, "1"))
            mains_online = true;
      }
      else if (!strcmp(val, "Battery"))
      {
         if (     gamemode_read_line(ent->d_name, "status", val, sizeof(val))
               && !strcmp(val, "Discharging"))
            discharging = true;
      }
   }
   closedir(dir);
   return mains ? !mains_online : discharging;
}

/* UPower's OnBattery. False when UPower does not answer, @have says
 * whether it did. */
static bool gamemode_upower_on_battery(const rdbus_t *rd,
      rdbus_connection_t *conn, bool *have)
{
   bool on_battery        = false;
   const char *iface      = GAMEMODE_UPOWER_NAME;
   const char *prop       = "OnBattery";
   rdbus_message_t *reply = NULL;
   rdbus_message_t *msg   = rd->message_new_method_call(GAMEMODE_UPOWER_NAME,
         GAMEMODE_UPOWER_PATH, GAMEMODE_PROPS_IFACE, "Get");

   *have = false;
   if (!msg)
      return false;
   if (     rd->message_append_args(msg,
               RDBUS_TYPE_STRING, &iface,
               RDBUS_TYPE_STRING, &prop,
               RDBUS_TYPE_INVALID)
         && (reply = rd->connection_send_with_reply_and_block(conn,
               msg, RDBUS_TIMEOUT_USE_DEFAULT, NULL)))
   {
      rdbus_iter_t it, var;
      if (     rd->message_iter_init(reply, &it)
            && rd->message_iter_get_arg_type(&it) == RDBUS_TYPE_VARIANT)
      {
         rd->message_iter_recurse(&it, &var);
         if (rd->message_iter_get_arg_type(&var) == RDBUS_TYPE_BOOLEAN)
         {
            rdbus_bool_t b = 0;
            rd->message_iter_get_basic(&var, &b);
            on_battery = b != 0;
            *have      = true;
         }
      }
      rd->message_unref(reply);
   }
   rd->message_unref(msg);
   return on_battery;
}

static void gamemode_ppd_hold(const rdbus_t *rd)
{
   int i;
   rdbus_error_t err;
   const char *profile = "performance";
   const char *reason  = "Game Mode";
   const char *app     = "org.libretro.RetroArch";

   rd->error_init(&err);
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
         return;
      }
      rd->error_free(&err);
   }

   RARCH_LOG("[GameMode] No power profiles daemon on the system bus; "
         "the CPU's power mode is left alone.\n");
   gamemode_ppd_gave_up = true;
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
   RARCH_LOG("[GameMode] Released the performance power profile.\n");
}

static void gamemode_ppd_close(const rdbus_t *rd)
{
   if (!gamemode_ppd_conn)
      return;
   rd->connection_close(gamemode_ppd_conn);
   rd->connection_unref(gamemode_ppd_conn);
   gamemode_ppd_conn = NULL;
}

/* Opens the system bus and starts following UPower. False without a
 * bus; @fd is the socket to wait on. */
static bool gamemode_ppd_open(const rdbus_t *rd, int *fd)
{
   rdbus_error_t err;

   rd->error_init(&err);
   if (!(gamemode_ppd_conn = rd->bus_get_private(RDBUS_BUS_SYSTEM, &err)))
   {
      rd->error_free(&err);
      return false;
   }
   /* A lost system bus ends the connection, not RetroArch. */
   rd->connection_set_exit_on_disconnect(gamemode_ppd_conn, 0);
   if (!rd->connection_get_unix_fd(gamemode_ppd_conn, fd))
   {
      gamemode_ppd_close(rd);
      return false;
   }
   rd->bus_add_match(gamemode_ppd_conn,
         "type='signal',sender='" GAMEMODE_UPOWER_NAME "',"
         "path='" GAMEMODE_UPOWER_PATH "',"
         "interface='" GAMEMODE_PROPS_IFACE "',member='PropertiesChanged'",
         &err);
   if (rd->error_is_set(&err))
      rd->error_free(&err);
   return true;
}

/* The kernel's uevent broadcast (the group udev itself listens to),
 * which an unprivileged process may receive. -1 where it cannot be had,
 * a container without it say. */
static int gamemode_uevent_open(void)
{
   struct sockaddr_nl sa;
   int s = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);

   if (s < 0)
      return -1;
   fcntl(s, F_SETFD, FD_CLOEXEC);
   fcntl(s, F_SETFL, O_NONBLOCK);
   memset(&sa, 0, sizeof(sa));
   sa.nl_family = AF_NETLINK;
   sa.nl_groups = 1;
   if (bind(s, (struct sockaddr*)&sa, sizeof(sa)) != 0)
   {
      close(s);
      return -1;
   }
   return s;
}

/* Reads every queued uevent; true when one was for a power supply (a
 * charger plugged or pulled, a battery changing state). A message is
 * "ACTION@devpath" then KEY=VALUE strings, each NUL-terminated. At
 * worst a stray message costs one sysfs read. */
static bool gamemode_uevent_drain(int s)
{
   char buf[4096];
   bool hit = false;

   for (;;)
   {
      char *p, *end;
      ssize_t n = recv(s, buf, sizeof(buf) - 1, 0);
      if (n < 0)
      {
         /* Events were dropped: one of them may have been ours. */
         if (errno == ENOBUFS)
         {
            hit = true;
            continue;
         }
         if (errno == EINTR)
            continue;
         break;
      }
      if (n == 0)
         break;
      buf[n] = '\0';
      end    = buf + n;
      for (p = buf; p < end; p += strlen(p) + 1)
         if (!strcmp(p, "SUBSYSTEM=power_supply"))
            hit = true;
   }
   return hit;
}

static void gamemode_ppd_thread(void *data)
{
   int fd              = -1;
   int nl              = -1;
   bool on_battery     = false;
   bool no_upower      = false;
   bool recheck        = true;
   bool logged_battery = false;
   const rdbus_t *rd   = NULL;
   (void)data;
   sthread_setname("ra-gamemode");

   if (!gamemode_ppd_rd && !(gamemode_ppd_rd = rdbus_get()))
      RARCH_LOG("[GameMode] libdbus is not available; the performance profile is not held.\n");

   for (;;)
   {
      struct pollfd pfd[3];
      int nfd, ret, timeout;
      int state = retro_atomic_load_acquire_int(&gamemode_ppd_state);
      bool want = (state & GAMEMODE_PPD_WANT) != 0;

      if (!want && rd)
      {
         gamemode_ppd_release(rd);
         gamemode_ppd_close(rd);
         rd = NULL;
      }
      if (!rd && nl >= 0)
      {
         close(nl);
         nl        = -1;
         no_upower = false;
      }

      if (want && !rd && gamemode_ppd_rd && !gamemode_ppd_gave_up)
      {
         if (gamemode_ppd_open(gamemode_ppd_rd, &fd))
         {
            rd             = gamemode_ppd_rd;
            recheck        = true;
            logged_battery = false;
         }
         else
         {
            RARCH_LOG("[GameMode] No system bus; the performance profile is not held.\n");
            gamemode_ppd_gave_up = true;
         }
      }

      if (!rd)
      {
         /* Nothing to hold or follow: done if WANT is still what was
          * just served, else serve the change. */
         if (retro_atomic_cas_int(&gamemode_ppd_state, state,
                  state & ~GAMEMODE_PPD_BUSY))
            break;
         continue;
      }

      /* Whatever the bus sent meanwhile: a UPower property change
       * means the power source may have changed. */
      {
         rdbus_message_t *m;
         rd->connection_read_write(gamemode_ppd_conn, 0);
         while ((m = rd->connection_pop_message(gamemode_ppd_conn)))
         {
            if (rd->message_is_signal(m, GAMEMODE_PROPS_IFACE,
                     "PropertiesChanged"))
               recheck = true;
            rd->message_unref(m);
         }
      }

      if (recheck)
      {
         if (!no_upower)
         {
            bool have_upower;
            on_battery = gamemode_upower_on_battery(rd, gamemode_ppd_conn,
                  &have_upower);
            /* Asked once; from then on the kernel says when to look. */
            if (!have_upower)
            {
               no_upower = true;
               nl        = gamemode_uevent_open();
            }
         }
         if (no_upower)
            on_battery = gamemode_sysfs_on_battery();
         recheck = false;
      }

      if (on_battery)
      {
         if (gamemode_ppd_held || !logged_battery)
            RARCH_LOG("[GameMode] On battery power; the performance profile is not held.\n");
         logged_battery = true;
         gamemode_ppd_release(rd);
      }
      else
      {
         logged_battery = false;
         if (!gamemode_ppd_held && !gamemode_ppd_gave_up)
            gamemode_ppd_hold(rd);
      }

      pfd[0].fd      = gamemode_ppd_wake[0];
      pfd[0].events  = POLLIN;
      pfd[0].revents = 0;
      nfd            = 1;
      timeout        = -1;
      /* Refused or absent: no plug change can matter, so only a leave
       * is waited for. */
      if (!gamemode_ppd_gave_up)
      {
         pfd[1].fd      = fd;
         pfd[1].events  = POLLIN;
         pfd[1].revents = 0;
         nfd            = 2;
         if (nl >= 0)
         {
            pfd[2].fd      = nl;
            pfd[2].events  = POLLIN;
            pfd[2].revents = 0;
            nfd            = 3;
         }
         else if (no_upower)
            timeout = GAMEMODE_SYSFS_RECHECK_MS;
      }

      if ((ret = poll(pfd, nfd, timeout)) < 0)
         continue;
      if (ret == 0)
         recheck = true; /* the fallback's time is up */
      if (pfd[0].revents & POLLIN)
      {
         char buf[64];
         while (read(gamemode_ppd_wake[0], buf, sizeof(buf)) > 0) { }
      }
      if (nfd == 3 && (pfd[2].revents & POLLIN)
            && gamemode_uevent_drain(nl))
         recheck = true;
      if (nfd >= 2 && (pfd[1].revents & (POLLHUP | POLLERR)))
      {
         /* The bus went away, and the hold with it. */
         gamemode_ppd_held    = false;
         gamemode_ppd_gave_up = true;
         gamemode_ppd_close(rd);
         rd = NULL;
      }
   }
}

static bool gamemode_ppd_request(bool want)
{
   sthread_t *thread;

   if (gamemode_ppd_wake[0] < 0)
   {
      if (pipe(gamemode_ppd_wake) != 0)
      {
         gamemode_ppd_wake[0] = gamemode_ppd_wake[1] = -1;
         return false;
      }
      fcntl(gamemode_ppd_wake[0], F_SETFL, O_NONBLOCK);
      fcntl(gamemode_ppd_wake[1], F_SETFL, O_NONBLOCK);
      fcntl(gamemode_ppd_wake[0], F_SETFD, FD_CLOEXEC);
      fcntl(gamemode_ppd_wake[1], F_SETFD, FD_CLOEXEC);
   }

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
      {
         /* The worker picks the change up once woken. */
         char c = 0;
         if (write(gamemode_ppd_wake[1], &c, 1) < 0) { }
         return true;
      }
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
