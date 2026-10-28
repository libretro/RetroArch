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

#ifndef _LINUX_FF_H
#define _LINUX_FF_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/input.h>

#include <boolean.h>
#include <retro_inline.h>

/* A vibration motor behind an evdev node, driven with a periodic sine
 * effect: what a handheld whose motor is not part of a gamepad (the
 * OpenDingux ones) offers. Each call is a syscall to the motor's
 * driver, so a joypad driver makes them from its output writer
 * (output_writer.h), never on a frame. Nothing is allocated. */

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

/* /dev/input/event0 to this, looked through for a motor */
#define LINUX_FF_EVENT_NODES 32
/* the largest magnitude of a periodic effect */
#define LINUX_FF_MAGNITUDE_MAX 0x7FFF

#define LINUX_FF_BITS_LONG (8 * sizeof(unsigned long))

struct linux_ff_motor
{
   int      id;        /* the uploaded effect, or -1 */
   uint16_t strength;  /* the strength it was uploaded with */
   bool     playing;
};

static INLINE void linux_ff_motor_init(struct linux_ff_motor *m)
{
   m->id       = -1;
   m->strength = 0;
   m->playing  = false;
}

static INLINE bool linux_ff_has(const unsigned long *bits, unsigned bit)
{
   return (bits[bit / LINUX_FF_BITS_LONG] >> (bit % LINUX_FF_BITS_LONG)) & 1;
}

/* The first event node that plays a periodic sine and holds @effects
 * of them at once, open for writing; -1 for none. @gain: it takes
 * FF_GAIN. */
static INLINE int linux_ff_open_sine(int effects, bool *gain)
{
   unsigned n;

   *gain = false;
   for (n = 0; n < LINUX_FF_EVENT_NODES; n++)
   {
      unsigned long ff[(FF_MAX + LINUX_FF_BITS_LONG) / LINUX_FF_BITS_LONG];
      char path[32];
      int capacity = 0;
      int fd;

      snprintf(path, sizeof(path), "/dev/input/event%u", n);
      if ((fd = open(path, O_RDWR | O_CLOEXEC)) < 0)
         continue;
      memset(ff, 0, sizeof(ff));
      if (     ioctl(fd, EVIOCGBIT(EV_FF, sizeof(ff)), ff) >= 0
            && linux_ff_has(ff, FF_PERIODIC)
            && linux_ff_has(ff, FF_SINE)
            && ioctl(fd, EVIOCGEFFECTS, &capacity) >= 0
            && capacity >= effects)
      {
         *gain = linux_ff_has(ff, FF_GAIN);
         return fd;
      }
      close(fd);
   }
   return -1;
}

static INLINE bool linux_ff_event(int fd, uint16_t code, int32_t value)
{
   struct input_event ie;
   memset(&ie, 0, sizeof(ie));
   ie.type  = EV_FF;
   ie.code  = code;
   ie.value = value;
   return write(fd, &ie, sizeof(ie)) == (ssize_t)sizeof(ie);
}

/* @percent of the motor's strength, 0 to 100 */
static INLINE bool linux_ff_gain(int fd, unsigned percent)
{
   if (percent > 100)
      percent = 100;
   return linux_ff_event(fd, FF_GAIN, (int32_t)(0xFFFFu * percent / 100));
}

/* Runs @m at @strength, 0 to 0xFFFF, as a sine of @period_ms. A new
 * strength is uploaded over the effect, which the kernel changes as
 * it plays; 0 stops it. False, and @m as it was, if a call failed. */
static INLINE bool linux_ff_motor_set(int fd, struct linux_ff_motor *m,
      uint16_t period_ms, uint16_t strength)
{
   if (!strength)
   {
      if (m->playing)
      {
         if (!linux_ff_event(fd, (uint16_t)m->id, 0))
            return false;
         m->playing = false;
      }
      return true;
   }

   if (m->id < 0 || strength != m->strength)
   {
      struct ff_effect e;
      memset(&e, 0, sizeof(e));
      e.type                 = FF_PERIODIC;
      e.id                   = (int16_t)m->id;
      e.u.periodic.waveform  = FF_SINE;
      e.u.periodic.period    = period_ms;
      e.u.periodic.magnitude = (int16_t)(
            (LINUX_FF_MAGNITUDE_MAX * (uint32_t)strength) / 0xFFFFu);
      if (ioctl(fd, EVIOCSFF, &e) < 0)
         return false;
      m->id       = e.id;
      m->strength = strength;
   }

   if (!m->playing)
   {
      if (!linux_ff_event(fd, (uint16_t)m->id, 1))
         return false;
      m->playing = true;
   }
   return true;
}

/* Stops @m and lets the kernel have its effect back */
static INLINE void linux_ff_motor_erase(int fd, struct linux_ff_motor *m)
{
   if (m->id < 0)
      return;
   if (m->playing)
      linux_ff_event(fd, (uint16_t)m->id, 0);
   ioctl(fd, EVIOCRMFF, m->id);
   linux_ff_motor_init(m);
}

#endif
