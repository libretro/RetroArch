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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>

#include <dirent.h>
#include <linux/input.h>
#include <linux/kd.h>
#include <termios.h>
#include <unistd.h>

#include "linux_common.h"
#include "verbosity.h"
#include <retro_assert.h>
#include <retro_timers.h>
#include <rthreads/rthreads.h>

/* We can assume that pthreads are available on Linux. */
#include <pthread.h>
#include <string.h>

#define IIO_DEVICES_DIR "/sys/bus/iio/devices"
#define IIO_ILLUMINANCE_SENSOR "in_illuminance_input"
#define DEFAULT_POLL_RATE 5

/* TODO/FIXME - static globals */
static struct termios old_term, new_term;
static long old_kbmd               = 0xffff;
static bool linux_stdin_claimed    = false;

struct linux_illuminance_sensor
{
   sthread_t *thread;

   /* Poll rate in Hz (i.e. in queries per second) */
   volatile unsigned poll_rate;

   /* We store the lux reading in millilux (as an int)
    * so that we can make the access atomic and avoid locks.
    * A little bit of precision is lost, but not enough to matter.
    */
   volatile int millilux;

   /* If true, the associated thread must finish its work and exit. */
   volatile bool done;

   char path[PATH_MAX];
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
   struct sigaction sa = {0};

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

static void linux_poll_illuminance_sensor(void *data)
{
   linux_illuminance_sensor_t *sensor = data;

   if (!data)
      return;

   while (!sensor->done)
   { /* Aligned int reads are atomic on most CPUs */
        double lux;
        int millilux;
        unsigned poll_rate = sensor->poll_rate;

        /* Don't allow cancellation inside the critical section,
         * as it opens up a file; we don't want to leak it! */
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        lux = linux_read_illuminance_sensor(sensor);
        millilux = (int)(lux * 1000.0);
        retro_assert(poll_rate != 0);

        sensor->millilux = millilux;
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

        /* Allow cancellation here so that the main thread doesn't block
         * while waiting for this thread to wake up and exit. */
        retro_sleep(1000 / poll_rate);
        if (errno == EINTR)
        {
           RARCH_ERR("Illuminance sensor thread interrupted\n");
           break;
        }
   }

   RARCH_DBG("Illuminance sensor thread for %s exiting\n", sensor->path);
}

linux_illuminance_sensor_t *linux_open_illuminance_sensor(unsigned rate)
{
   DIR *devices = NULL;
   struct dirent *d = NULL;
   linux_illuminance_sensor_t *sensor = malloc(sizeof(*sensor));

   if (!sensor)
      goto error;

   sensor->millilux  = 0;
   sensor->poll_rate = rate ? rate : DEFAULT_POLL_RATE;
   sensor->thread    = NULL; /* We'll spawn a thread later, once we find a sensor */
   sensor->done      = false;

   devices = opendir(IIO_DEVICES_DIR);
   if (!devices)
   { /* If we couldn't find the IIO device directory... */
      char errmesg[PATH_MAX];
      strerror_r(errno, errmesg, sizeof(errmesg));
      RARCH_ERR("Failed to open " IIO_DEVICES_DIR ": %s\n", errmesg);
      goto error;
   }

   /*
    * Must clear errno at the start of each iteration,
    * as an error code that came from one run of the loop
    * can leak into the next iteration and hide serious errors.
    * (Ex: trying to open a file that doesn't exist,
    * which we handle here.)
    */
   errno = 0;
   for (d = readdir(devices); d != NULL; errno = 0, d = readdir(devices))
   { /* For each IIO device... */
      /* First ensure that the readdir call succeeded... */
      int err = errno;
      double lux;

      if (d->d_name[0] == '.')
         /* Skip hidden files, ".", and ".." */
         continue;

      if (err != 0)
      {
         char errmesg[PATH_MAX];
         strerror_r(err, errmesg, sizeof(errmesg));
         RARCH_ERR("readdir(" IIO_DEVICES_DIR ") failed: %s\n", errmesg);
         goto error;
      }

      /* If that worked out, look to see if this device represents an illuminance sensor */
      snprintf(sensor->path, sizeof(sensor->path), IIO_DEVICES_DIR "/%s/" IIO_ILLUMINANCE_SENSOR, d->d_name);

      lux = linux_read_illuminance_sensor(sensor);
      if (lux >= 0)
      { /* If we found an illuminance sensor that works... */
         sensor->millilux = (int)(lux * 1000.0); /* Set the first reading */
         sensor->thread = sthread_create(linux_poll_illuminance_sensor, sensor);

         if (!sensor->thread)
         {
            RARCH_ERR("Failed to spawn thread for illuminance sensor\n");
            goto error;
         }

         RARCH_LOG("Opened illuminance sensor at %s, polling at %u Hz\n", sensor->path, sensor->poll_rate);

         goto done;
      }
   }

error:
   RARCH_ERR("Failed to find an illuminance sensor\n");
   if (devices)
      closedir(devices);

   free(sensor);

   return NULL;
done:
   if (devices)
      closedir(devices);

   return sensor;
}

void linux_close_illuminance_sensor(linux_illuminance_sensor_t *sensor)
{
   if (!sensor)
      return;

   if (sensor->thread)
   {
      pthread_t thread = sthread_get_thread_id(sensor->thread);
      sensor->done = true;

      if (pthread_cancel(thread) != 0)
      {
         int err = errno;
         char errmesg[PATH_MAX];
         strerror_r(err, errmesg, sizeof(errmesg));
         RARCH_ERR("Failed to cancel illuminance sensor thread: %s\n", errmesg);
      }

      sthread_join(sensor->thread);
      /* sthread_join will free the thread */
   }

   free(sensor);
}

float linux_get_illuminance_reading(const linux_illuminance_sensor_t *sensor)
{
   int millilux;
   if (!sensor)
      return -1.0f;

   /* Reading an int is atomic on most CPUs */
   millilux = sensor->millilux;

   return (float)millilux / 1000.0f;
}


void linux_set_illuminance_sensor_rate(linux_illuminance_sensor_t *sensor, unsigned rate)
{
   if (!sensor)
      return;

   /* Set a default rate of 5 Hz if none is provided */
   rate = rate ? rate : DEFAULT_POLL_RATE;

   sensor->poll_rate = rate;
}

static double linux_read_illuminance_sensor(const linux_illuminance_sensor_t *sensor)
{
   char buffer[256];
   double illuminance = 0.0;
   int err;
   FILE* in_illuminance_input = NULL;

   if (!sensor || sensor->path[0] == '\0')
      return -1.0;

   in_illuminance_input = fopen(sensor->path, "r");
   if (!in_illuminance_input)
   {
      char errmesg[PATH_MAX];
      strerror_r(errno, errmesg, sizeof(errmesg));
      RARCH_ERR("Failed to open %s: %s\n", sensor->path, errmesg);
      illuminance = -1.0;
      goto done;
   }

   if (!fgets(buffer, sizeof(buffer), in_illuminance_input))
   { /* Read the illuminance value from the file. If that fails... */
      char errmesg[PATH_MAX];
      strerror_r(errno, errmesg, sizeof(errmesg));
      RARCH_ERR("Illuminance sensor read failed: %s\n", errmesg);
      illuminance = -1.0;
      goto done;
   }

   /* TODO: This may be locale-sensitive */
   errno = 0;
   illuminance = strtod(buffer, NULL);
   err = errno;
   if (err != 0)
   {
      char errmesg[PATH_MAX];
      strerror_r(err, errmesg, sizeof(errmesg));
      RARCH_ERR("Failed to parse input \"%s\" into a floating-point value: %s", buffer, errmesg);
      illuminance = -1.0;
      goto done;
   }

done:
   if (in_illuminance_input)
      fclose(in_illuminance_input);

   return illuminance;
}
