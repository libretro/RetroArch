/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <stdlib.h>
#include <errno.h>
#include <signal.h>

#include <dirent.h>
#include <linux/input.h>
#include <linux/kd.h>
#include <termios.h>
#include <unistd.h>

#include "linux_common.h"
#include "verbosity.h"
#include <retro_assert.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#ifdef HAVE_THREADS
#include <rthreads/retro_eventcount.h>
#endif
#include <features/features_cpu.h>

#include <retro_dirent.h>
#include <streams/file_stream.h>
#include <string.h>
#include <string/rstrtod.h>

/* Overridable so a test can point it at a fake sysfs tree. */
#ifndef IIO_DEVICES_DIR
#define IIO_DEVICES_DIR "/sys/bus/iio/devices"
#endif
#define IIO_ILLUMINANCE_SENSOR "in_illuminance_input"
#define DEFAULT_POLL_RATE 5
/* The rate comes from the core, through
 * RETRO_ENVIRONMENT_SET_SENSOR_STATE, so it is whatever a core asks
 * for. The sensor cannot usefully answer faster than 1000 Hz, since
 * each reading is an open/read/close of a sysfs file. */
#define MAX_POLL_RATE     1000

/* TODO/FIXME - static globals */
static struct termios old_term, new_term;
static long old_kbmd               = 0xffff;
static bool linux_stdin_claimed    = false;

struct linux_illuminance_sensor
{
#ifdef HAVE_THREADS
   sthread_t *thread;
   /* The poll thread waits here between readings, so a rate change or
    * close wakes it at once instead of after the rest of a sleep. No
    * lock: the rate, the count of rate changes and done are one atomic
    * word each, stored before the notify. */
   retro_eventcount_t wake;
   retro_atomic_int_t rate_gen;
   retro_atomic_int_t done;
#endif

   /* Poll rate in Hz (i.e. in queries per second) */
   retro_atomic_int_t poll_rate;

   /* The lux reading in millilux, so it can be published atomically
    * to the input driver without a lock. A little precision is lost,
    * not enough to matter. */
   retro_atomic_int_t millilux;

   char path[PATH_MAX_LENGTH];
};
static double linux_read_illuminance_sensor(const linux_illuminance_sensor_t *sensor);

void linux_terminal_restore_input(void)
{
   if (old_kbmd == 0xffff)
      return;

   if (ioctl(0, KDSKBMODE, old_kbmd) < 0)
      return;

   tcsetattr(0, TCSAFLUSH, &old_term);
   old_kbmd = 0xffff;

   linux_stdin_claimed = false;
}

/* Disables input */
static bool linux_terminal_init(void)
{
   if (old_kbmd != 0xffff)
      return false;

   if (tcgetattr(0, &old_term) < 0)
      return false;

   new_term              = old_term;
   new_term.c_lflag     &= ~(ECHO | ICANON | ISIG);
   new_term.c_iflag     &= ~(ISTRIP | IGNCR | ICRNL | INLCR | IXOFF | IXON);
   new_term.c_cc[VMIN]   = 0;
   new_term.c_cc[VTIME]  = 0;

   /* Be careful about recovering the terminal. */
   if (ioctl(0, KDGKBMODE, &old_kbmd) < 0)
      return false;

   if (tcsetattr(0, TCSAFLUSH, &new_term) < 0)
      return false;

   return true;
}

/* We need to disable use of stdin command interface if
 * stdin is supposed to be used for input. */
void linux_terminal_claim_stdin(void)
{
   linux_stdin_claimed = true;
}

bool linux_terminal_grab_stdin(void *data)
{
   return linux_stdin_claimed;
}

static void linux_terminal_restore_signal(int sig)
{
   linux_terminal_restore_input();
   kill(getpid(), sig);
}

bool linux_terminal_disable_input(void)
{
   struct sigaction sa;

   /* Avoid accidentally typing stuff. */
   if (!isatty(0))
      return false;

   if (!linux_terminal_init())
      return false;

   if (ioctl(0, KDSKBMODE, K_MEDIUMRAW) < 0)
   {
      tcsetattr(0, TCSAFLUSH, &old_term);
      return false;
   }

   memset(&sa, 0, sizeof(sa));
   sa.sa_handler = linux_terminal_restore_signal;
   sa.sa_flags   = SA_RESTART | SA_RESETHAND;
   sigemptyset(&sa.sa_mask);

   /* Trap some standard termination codes so we
    * can restore the keyboard before we lose control. */
   sigaction(SIGABRT, &sa, NULL);
   sigaction(SIGBUS,  &sa, NULL);
   sigaction(SIGFPE,  &sa, NULL);
   sigaction(SIGILL,  &sa, NULL);
   sigaction(SIGQUIT, &sa, NULL);
   sigaction(SIGSEGV, &sa, NULL);

   atexit(linux_terminal_restore_input);

   return true;
}

#ifdef HAVE_THREADS
/* Reads the sensor once per period, on a schedule rather than by
 * sleeping a period after each read, so the rate holds however long a
 * read takes; a reader that falls behind skips ahead rather than
 * bursting to catch up. Between readings it waits on the sensor's
 * eventcount, which close and rate changes notify: close returns as
 * soon as any read in progress does, and a new rate applies at once. */
static void linux_poll_illuminance_sensor(void *data)
{
   linux_illuminance_sensor_t *sensor = (linux_illuminance_sensor_t*)data;
   retro_time_t next;

   if (!sensor)
      return;

   next = cpu_features_get_time_usec();

   while (!retro_atomic_load_acquire_int(&sensor->done))
   {
      double       lux;
      retro_time_t now;
      /* The count before the rate: a change that lands between the two
       * is seen as one and read again, never missed. */
      int          gen    = retro_atomic_load_acquire_int(&sensor->rate_gen);
      retro_time_t period = 1000000
         / retro_atomic_load_acquire_int(&sensor->poll_rate);

      lux = linux_read_illuminance_sensor(sensor);
      retro_atomic_store_release_int(&sensor->millilux,
            (int)(lux * 1000.0));

      now   = cpu_features_get_time_usec();
      next += period;
      if (next <= now)
         next = now + period;

      while (now < next)
      {
         int key = retro_eventcount_prepare_wait(&sensor->wake);
         if (     retro_atomic_load_acquire_int(&sensor->done)
               || retro_atomic_load_acquire_int(&sensor->rate_gen) != gen)
         {
            retro_eventcount_cancel_wait(&sensor->wake);
            break;
         }
         retro_eventcount_commit_wait_timeout(&sensor->wake, key, next - now);
         now = cpu_features_get_time_usec();
      }

      /* A new rate reads now and schedules from here. */
      if (retro_atomic_load_acquire_int(&sensor->rate_gen) != gen)
         next = now;
   }

   RARCH_DBG("Illuminance sensor thread for %s exiting.\n", sensor->path);
}
#endif

linux_illuminance_sensor_t *linux_open_illuminance_sensor(unsigned rate)
{
   RDIR *device = NULL;
   linux_illuminance_sensor_t *sensor = (linux_illuminance_sensor_t *)
      calloc(1, sizeof(*sensor));

   if (!sensor)
      goto error;

   rate              = rate ? rate : DEFAULT_POLL_RATE;
   if (rate > MAX_POLL_RATE)
      rate           = MAX_POLL_RATE;
   retro_atomic_int_init(&sensor->poll_rate, (int)rate);
   sensor->path[0]   = '\0';
   retro_atomic_store_release_int(&sensor->millilux, 0);
#ifdef HAVE_THREADS
   sensor->thread       = NULL; /* spawned once a sensor is found */
   retro_atomic_int_init(&sensor->rate_gen, 0);
   retro_atomic_int_init(&sensor->done, 0);
   if (!retro_eventcount_init(&sensor->wake))
      goto error;
#endif

   device = retro_opendir(IIO_DEVICES_DIR);
   if (!device)
      goto error;

   while (retro_readdir(device))
   { /* For each IIO device... */
      const char *name = retro_dirent_get_name(device);
      double lux = 0.0;

      if (!name)
      {
         RARCH_ERR("Error reading " IIO_DEVICES_DIR ".\n");
         goto error;
      }

      if (name[0] == '.')
         /* Skip hidden files, ".", and ".." */
         continue;

      /* If that worked out, look to see if this device represents an illuminance sensor */
      snprintf(sensor->path, sizeof(sensor->path), IIO_DEVICES_DIR "/%s/" IIO_ILLUMINANCE_SENSOR, name);

      lux = linux_read_illuminance_sensor(sensor);
      if (lux >= 0)
      { /* If we found an illuminance sensor that works... */
         /* Set the first reading */
         retro_atomic_store_release_int(&sensor->millilux,
               (int)(lux * 1000.0));
#ifdef HAVE_THREADS
         if (!(sensor->thread = sthread_create(
                     linux_poll_illuminance_sensor, sensor)))
         {
            RARCH_ERR("Failed to spawn thread for illuminance sensor.\n");
            goto error;
         }
#endif
         /* Without threads, the first reading above is all the sensor
          * reports. */

         RARCH_LOG("Opened illuminance sensor at %s, polling at %u Hz.\n", sensor->path, rate);
         retro_closedir(device);
         return sensor;
      }
   }

error:
   RARCH_ERR("Failed to find an illuminance sensor in " IIO_DEVICES_DIR ".\n");
   retro_closedir(device);

#ifdef HAVE_THREADS
   if (sensor)
      retro_eventcount_free(&sensor->wake);
#endif
   free(sensor);

   return NULL;
}

void linux_close_illuminance_sensor(linux_illuminance_sensor_t *sensor)
{
   if (!sensor)
      return;

#ifdef HAVE_THREADS
   if (sensor->thread)
   {
      /* Wakes the thread out of its wait between readings; the join
       * then lasts no longer than a read already in progress. */
      retro_atomic_store_release_int(&sensor->done, 1);
      retro_eventcount_notify(&sensor->wake);

      sthread_join(sensor->thread);
      /* sthread_join will free the thread */
   }
   retro_eventcount_free(&sensor->wake);
#endif

   free(sensor);
}

float linux_get_illuminance_reading(const linux_illuminance_sensor_t *sensor)
{
   int millilux;
   if (!sensor)
      return -1.0f;

   millilux = retro_atomic_load_acquire_int(&sensor->millilux);

   return (float)millilux / 1000.0f;
}


void linux_set_illuminance_sensor_rate(linux_illuminance_sensor_t *sensor, unsigned rate)
{
   if (!sensor)
      return;

   /* Set a default rate of 5 Hz if none is provided */
   rate = rate ? rate : DEFAULT_POLL_RATE;
   if (rate > MAX_POLL_RATE)
      rate = MAX_POLL_RATE;

   retro_atomic_store_release_int(&sensor->poll_rate, (int)rate);
#ifdef HAVE_THREADS
   /* The rate, then the count that says it changed, then the wake. */
   retro_atomic_fetch_add_int(&sensor->rate_gen, 1);
   retro_eventcount_notify(&sensor->wake);
#endif
}

static double linux_read_illuminance_sensor(const linux_illuminance_sensor_t *sensor)
{
   char buffer[256];
   double illuminance = 0.0;
   RFILE *in_illuminance_input = NULL;
   int err = 0;

   if (!sensor || sensor->path[0] == '\0')
      return -1.0;

   in_illuminance_input = filestream_open(sensor->path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!in_illuminance_input)
   {
      RARCH_ERR("Failed to open \"%s\".\n", sensor->path);
      return -1.0;
   }

   /* Read the illuminance value from the file. If that fails... */
   if (!filestream_gets(in_illuminance_input, buffer, sizeof(buffer)))
   {
      RARCH_ERR("Illuminance sensor read failed.\n");
      filestream_close(in_illuminance_input);
      return -1.0;
   }

   filestream_close(in_illuminance_input);

   /* Clear any existing error so we'll know if strtod fails */
   errno = 0;

   /* TODO: This may be locale-sensitive */
   illuminance = rstrtod(buffer, NULL);
   err = errno;
   if (err != 0)
   {
      RARCH_ERR("Failed to parse input \"%s\" into a floating-point value: %s.\n", buffer, strerror(err));
      return -1.0;
   }

   return illuminance;
}
