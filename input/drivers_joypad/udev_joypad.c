/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2015 - Hans-Kristian Arntzen
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

#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <poll.h>
#include <libudev.h>
#if defined(__linux__)
#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>
#elif defined(__FreeBSD__)
#include <dev/evdev/input.h>
#else
#include <linux/input.h>
#endif

#include <retro_inline.h>
#include <retro_atomic.h>
#include <compat/strl.h>
#include <string/stdstring.h>

#include "../input_driver.h"
#include "../common/linux_not_joystick.h"
#include "../common/output_writer.h"
#include "../common/linux_pad_members.h"
#include "../common/input_ignore_devices.h"
#include "../common/linux_pad_leds.h"

#include "../../configuration.h"
#include "../../config.def.h"

#include "../../tasks/tasks_internal.h"

#include "../../verbosity.h"

/* Udev/evdev Linux joypad driver.
 * More complex and extremely low level,
 * but only Linux driver which can support joypad rumble.
 *
 * Uses udev for device detection + hotplug.
 *
 * Code adapted from SDL 2.0's implementation.
 */

#define UDEV_NUM_BUTTONS 64
#define NUM_AXES 32

#ifndef NUM_HATS
#define NUM_HATS 4
#endif

#define test_bit(nr, addr) \
   (((1UL << ((nr) % (sizeof(long) * CHAR_BIT))) & ((addr)[(nr) / (sizeof(long) * CHAR_BIT)])) != 0)
#define NBITS(x) ((((x) - 1) / (sizeof(long) * CHAR_BIT)) + 1)

#ifndef INPUT_PROP_ACCELEROMETER
#define INPUT_PROP_ACCELEROMETER 0x06
#endif

#define UDEV_DS3_PRESSURES 10

#define SENSOR_AXES 6 /* ABS_X..ABS_RZ on sensor node, maps 1:1 to RETRO_SENSOR_* 0-5 */
#define DEG_TO_RAD_F 0.017453293f

struct udev_joypad
{
   /* Everything the per-frame poll and the per-query button and axis
    * reads touch comes first, so a connected pad's live state sits in
    * the leading two cache lines of its slot: the descriptor, the
    * button bitmask, the hats, the axes and the trigger-range flags.
    * The absinfo table is next (read on every axis event), and the
    * identification strings, the keycode maps and the sensor state -
    * touched at hotplug and on the rare sensor read - trail behind. */
   int fd;
   /* the buttons, the hats' directions and the first axes as the reads
    * answer them, kept with each event: lent to the frontend's readers
    * between two polls, nothing copied */
   input_pad_view_t view;
   int8_t hats[NUM_HATS][2];
   int16_t axes[NUM_AXES];
   /* Deal with analog triggers that report -32767 to 32767 */
   bool neg_trigger[NUM_AXES];

   struct input_absinfo absinfo[NUM_AXES]; /* TODO/FIXME - unsure of alignment */
   bool axis_reported[NUM_AXES];
   dev_t device;  /* TODO/FIXME - unsure of alignment */

   char *path;

   int num_effects;
   int32_t vid;
   int32_t pid;
   /* Maps keycodes -> button/axes */
   uint8_t button_bind[KEY_MAX];
   uint8_t axes_bind[ABS_MAX];
   /* what the frontend last asked for; what has been written is the
    * writer's to know (udev_rumble_out) */
   uint16_t strength[2];
   unsigned rumble_gain;

   char ident[NAME_MAX_LENGTH];
   char phys[NAME_MAX_LENGTH];
   /* the pad's HID device, which its other nodes share */
   char hid_syspath[256];
   /* the device its lights are under: the HID device, or an Xbox 360
    * pad's USB device (xpad) */
   char led_scope[256];
   uint8_t led_count;   /* lights found for the writer */
   uint8_t led_tries;   /* looks for them while there are none */

   /* Sensor (IMU) support: sibling evdev node for accelerometer/gyroscope */
   int sensor_fd;
   char *sensor_path;
   struct input_absinfo sensor_absinfo[SENSOR_AXES];
   float sensor_data[SENSOR_AXES]; /* indexed by RETRO_SENSOR_* ID */
   bool sensor_accel_enabled;
   bool sensor_gyro_enabled;
   bool sensor_has_accel; /* sensor node has ABS_X/Y/Z or ABS_RX/RY/RZ */
   bool sensor_has_gyro;  /* sensor node has ABS_RX/RY/RZ */
   bool sensor_accel_on_rxyz_codes; /* wiimote: accel is reported on ABS_RX/RY/RZ instead of ABS_X/Y/Z. */

   /* A DualShock 3's button pressures, which its evdev node does not
    * have: read from the pad's hidraw node into the axes from
    * pressure_axis on. */
   bool pressure;
   bool usb;
   uint8_t pressure_axis;
   int pressure_fd;

   /* A DualShock 4's or DualSense's touchpad: a sibling evdev node of
    * its own, its first two multitouch slots read into the view */
   int touch_fd;
   char *touch_path;
   int32_t touch_min[2];
   int32_t touch_span[2];
   uint8_t touch_slot;
};

struct joypad_udev_entry
{
   const char *devnode;
   struct udev_list_entry *item;
};

/* TODO/FIXME - static globals */
static struct udev *udev_joypad_fd             = NULL;
static struct udev_monitor *udev_joypad_mon    = NULL;
static struct udev_joypad udev_pads[MAX_USERS];
/* One bit per slot with an open descriptor, kept in step with fd by
 * udev_pad_set_fd(); the poll walks only these instead of reading fd
 * out of all MAX_USERS slots - a cache line apiece - every frame. */
static uint32_t udev_pads_active;

static void udev_pad_set_fd(unsigned p, int fd)
{
   udev_pads[p].fd = fd;
   if (fd >= 0)
      udev_pads_active |=  (1u << p);
   else
      udev_pads_active &= ~(1u << p);
}

/* Rumble is written off the frontend's thread.
 *
 * Uploading an effect (EVIOCSFF) is answered by whatever is behind the
 * pad. For a controller the kernel drives that is quick. For a
 * virtual pad, made through uinput by another program, the call waits
 * for that program to answer it, and the frame waited with it.
 *
 * The frontend's thread now only notes the strength asked for, and a
 * thread of the driver's own writes it. Nothing is queued: the writer
 * reads the strength wanted when it gets to it, so a pad that is slow
 * to answer is given the latest and not each one in turn.
 *
 * There is no lock. Both wanted strengths share one atomic word. A
 * pad's descriptor reaches the writer as a dup of it left in a slot: either
 * thread takes it out with an exchange, and whoever takes it out is
 * the one to close it. A pad that goes has its slot emptied and then
 * its generation raised; the writer, seeing the generation change,
 * closes the descriptor it holds and takes what the slot has. The
 * thread is the shared one of ../common/output_writer.h.
 *
 * Without threads, or if the thread cannot be started, the same code
 * runs where it always did. */
struct udev_rumble_out
{
   int      fd;            /* the writer's own descriptor, or -1 */
   int      gen;           /* the pad generation it is for */
   int      gain;          /* the gain written, or -1 */
   int      attempted_gain;
   int64_t  gain_due;
   int      effects[2];    /* [0] - strong, [1] - weak */
   bool     has_set_ff[2];
   uint16_t configured[2];
   uint16_t playing[2];
   uint16_t attempted[2];
   bool     retry[2];
   int64_t  due[2];
};

static struct udev_rumble_out udev_rumble_out[MAX_USERS];
static retro_atomic_int_t udev_rumble_want[MAX_USERS];
static retro_atomic_int_t udev_rumble_want_gain[MAX_USERS];
static retro_atomic_int_t udev_rumble_slot[MAX_USERS];
static retro_atomic_int_t udev_rumble_gen[MAX_USERS];
static input_output_writer_t *udev_rumble_writer = NULL;
static int64_t udev_rumble_next; /* writer-owned, or frontend fallback */

/* Linux documents FF durations above 0x7fff ms as unspecified. */
#define UDEV_RUMBLE_DURATION_MS 0x7fff
#define UDEV_RUMBLE_REFRESH_MS  30000
#define UDEV_RUMBLE_RETRY_MS    1000

static int64_t udev_rumble_now(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* One pad: its descriptor taken or let go if the pad changed, then
 * what is wanted of it written if it differs from what was. On the
 * writer's thread; on the frontend's only when there is no writer. */
static void udev_rumble_write(unsigned p)
{
   unsigned effect;
   struct udev_rumble_out *o = &udev_rumble_out[p];
   int gen                   = retro_atomic_load_acquire_int(&udev_rumble_gen[p]);
   int gain;
   uint32_t want;
   int64_t now;

   if (gen != o->gen)
   {
      if (o->fd >= 0)
         close(o->fd);
      memset(o, 0, sizeof(*o));
      o->fd   = -1;
      o->gen  = gen;
      o->gain = -1;
   }
   if (o->fd < 0)
   {
      o->fd = retro_atomic_exchange_int(&udev_rumble_slot[p], -1);
      if (o->fd < 0)
         return;
   }

#ifndef HAVE_LAKKA_SWITCH
   gain = retro_atomic_load_acquire_int(&udev_rumble_want_gain[p]);
   if (gain < 0 || gain == o->gain)
      o->gain_due = 0;
   if (gain >= 0 && gain != o->gain
         && (gain != o->attempted_gain || !o->gain_due
            || udev_rumble_now() >= o->gain_due))
   {
      struct input_event ie;
      memset(&ie, 0, sizeof(ie));
      ie.type  = EV_FF;
      ie.code  = FF_GAIN;
      ie.value = 0xFFFF * (gain / 100.0);
      o->attempted_gain = gain;
      if (write(o->fd, &ie, sizeof(ie)) < (ssize_t)sizeof(ie))
      {
         RARCH_ERR("[udev] Failed to set rumble gain on pad #%u.\n", p);
         o->gain_due = udev_rumble_now() + UDEV_RUMBLE_RETRY_MS;
      }
      else
      {
         o->gain     = gain;
         o->gain_due = 0;
      }
   }
#else
   (void)gain;
#endif

   now  = udev_rumble_now();
   want = (uint32_t)retro_atomic_load_acquire_int(&udev_rumble_want[p]);
   for (effect = 0; effect < 2; effect++)
   {
      uint16_t strength = (uint16_t)(want >> (effect * 16));
      uint16_t old      = o->playing[effect];

      if (o->retry[effect] && strength == o->attempted[effect]
            && now < o->due[effect])
         continue;
      if (!o->retry[effect] && strength == old
            && (!strength || now < o->due[effect]))
         continue;
      o->retry[effect]     = false;
      o->attempted[effect] = strength;

      if (strength && strength != o->configured[effect])
      {
         /* Create new or update old playing state. */
         struct ff_effect e      = {0};
         struct ff_replay replay = {UDEV_RUMBLE_DURATION_MS, 0};

         e.type   = FF_RUMBLE;
         e.id     = o->has_set_ff[effect] ? o->effects[effect] : -1;
         e.replay = replay;
         if (effect == RETRO_RUMBLE_STRONG)
            e.u.rumble.strong_magnitude = strength;
         else
            e.u.rumble.weak_magnitude   = strength;

         if (ioctl(o->fd, EVIOCSFF, &e) < 0)
         {
            RARCH_ERR("[udev] Failed to set rumble effect on pad #%u.\n", p);
            o->retry[effect] = true;
            o->due[effect]   = udev_rumble_now() + UDEV_RUMBLE_RETRY_MS;
            continue;
         }

         o->effects[effect]    = e.id;
         o->has_set_ff[effect] = true;
         o->configured[effect] = strength;
      }
      /* It seems that we can update strength with EVIOCSFF atomically. */
      if ((!!strength) != (!!old) || (strength && now >= o->due[effect]))
      {
         struct input_event play;

         memset(&play, 0, sizeof(play));
         play.type  = EV_FF;
         play.code  = o->effects[effect];
         play.value = !!strength;

         if (write(o->fd, &play, sizeof(play)) < (ssize_t)sizeof(play))
         {
            o->retry[effect] = true;
            o->due[effect] = udev_rumble_now() + UDEV_RUMBLE_RETRY_MS;
            RARCH_ERR("[udev] Failed to play rumble effect #%u on pad #%u.\n",
                  effect, p);
            continue;
         }
         /* from the pass's own clock, read before anything went */
         o->due[effect] = strength ? now + UDEV_RUMBLE_REFRESH_MS : 0;
      }
      o->playing[effect] = strength;
      if (!strength)
         o->due[effect] = 0;
   }
}

/* On the writer's thread, after each wake; at the last, what it wrote
 * to is its own to close. */
static void udev_led_write(unsigned p);
static void udev_led_close_all(void);
static void udev_led_init(void);

static void udev_rumble_writer_cb(void *userdata, bool last)
{
   unsigned p;

   if (!last)
   {
      for (p = 0; p < MAX_USERS; p++)
      {
         udev_rumble_write(p);
         udev_led_write(p);
      }
      return;
   }
   udev_led_close_all();
   for (p = 0; p < MAX_USERS; p++)
   {
      if (udev_rumble_out[p].fd >= 0)
         close(udev_rumble_out[p].fd);
      udev_rumble_out[p].fd = -1;
   }
}

static int udev_rumble_timeout(void *userdata)
{
   unsigned p, effect;
   int64_t next = 0;
   int64_t now;
   (void)userdata;
   for (p = 0; p < MAX_USERS; p++)
      if (udev_rumble_out[p].fd >= 0)
      {
         int64_t gain_due = udev_rumble_out[p].gain_due;
         if (gain_due && (!next || gain_due < next))
            next = gain_due;
         for (effect = 0; effect < 2; effect++)
         {
            int64_t due = udev_rumble_out[p].due[effect];
            if (due && (!next || due < next))
               next = due;
         }
      }
   udev_rumble_next = next;
   if (!next)
      return -1;
   now = udev_rumble_now();
   if (next <= now)
      return 0;
   return next - now > INT_MAX ? INT_MAX : (int)(next - now);
}

/* Without a writer, service only when a deadline has elapsed. */
static void udev_rumble_poll(void)
{
   if (!udev_rumble_writer && udev_rumble_next
         && udev_rumble_now() >= udev_rumble_next)
   {
      udev_rumble_writer_cb(NULL, false);
      udev_rumble_timeout(NULL);
   }
}

/* There is something for the writer to look at. Where there is no
 * writer it is looked at here. */
static void udev_rumble_wake(unsigned p)
{
   if (udev_rumble_writer)
      input_output_writer_wake(udev_rumble_writer);
   else
   {
      udev_rumble_write(p);
      udev_rumble_timeout(NULL);
   }
}

/* A pad that can rumble has arrived in slot @p: a descriptor of its
 * own is left for the writer. */
static void udev_rumble_attach(unsigned p, int fd)
{
   int old;
   int own = dup(fd);

   retro_atomic_store_release_int(&udev_rumble_want[p], 0);
   old = retro_atomic_exchange_int(&udev_rumble_slot[p], own);
   if (old >= 0)
      close(old);
   udev_rumble_wake(p);
}

/* The pad in slot @p is going. */
static void udev_rumble_detach(unsigned p)
{
   int old = retro_atomic_exchange_int(&udev_rumble_slot[p], -1);
   if (old >= 0)
      close(old);
   retro_atomic_store_release_int(&udev_rumble_want[p], 0);
   retro_atomic_store_release_int(&udev_rumble_want_gain[p], -1);
   retro_atomic_inc_int(&udev_rumble_gen[p]);
   udev_rumble_wake(p);
}

static void udev_rumble_start(void)
{
   unsigned p;
   udev_rumble_next = 0;
   for (p = 0; p < MAX_USERS; p++)
   {
      memset(&udev_rumble_out[p], 0, sizeof(udev_rumble_out[p]));
      udev_rumble_out[p].fd   = -1;
      udev_rumble_out[p].gain = -1;
      udev_rumble_out[p].gen  = retro_atomic_load_acquire_int(&udev_rumble_gen[p]);
      retro_atomic_store_release_int(&udev_rumble_slot[p], -1);
      retro_atomic_store_release_int(&udev_rumble_want_gain[p], -1);
      retro_atomic_store_release_int(&udev_rumble_want[p], 0);
   }
   udev_led_init();
   udev_rumble_writer = input_output_writer_new_timed(udev_rumble_writer_cb,
         udev_rumble_timeout, NULL);
}

static void udev_led_detach(unsigned p);

static void udev_rumble_stop(void)
{
   unsigned p;
   bool writer = udev_rumble_writer != NULL;
   input_output_writer_free(udev_rumble_writer);
   udev_rumble_writer = NULL;
   /* the writer's last call closed its lights */
   if (!writer)
      udev_led_close_all();
   for (p = 0; p < MAX_USERS; p++)
      udev_led_detach(p);
   udev_rumble_next   = 0;
   for (p = 0; p < MAX_USERS; p++)
   {
      int old = retro_atomic_exchange_int(&udev_rumble_slot[p], -1);
      if (old >= 0)
         close(old);
      if (udev_rumble_out[p].fd >= 0)
         close(udev_rumble_out[p].fd);
      udev_rumble_out[p].fd = -1;
   }
}


/* Player lights, through the pad's LED class devices (linux_pad_leds.h).
 *
 * Writing one can wait on the pad: a Switch pad's is a request it
 * answers. So the lights are written by the rumble writer, as rumble
 * is. When a pad arrives its lights are found and opened here, and
 * each descriptor is left in a slot for the writer, packed with which
 * light it is; the player wanted is an atomic of its own. A pad that
 * goes empties its slots and raises the rumble generation, at which
 * the writer closes what it holds. The player wanted is kept by port
 * slot, pad or not: the frontend tells every slot, so a pad that
 * arrives is lit as it starts.
 *
 * The files are root's unless a udev rule gives them to the user; a
 * pad whose lights cannot be opened is left as it is. */
#define UDEV_LEDS_MAX           8
/* a light bar's multi_intensity, which goes with its brightness */
#define UDEV_LED_RGB_INTENSITY  15
#define UDEV_LED_PACK(fd, led, n) (((fd) << 8) | ((led) << 4) | (n))

struct udev_led_out
{
   int     gen;                 /* the pad generation it is for */
   int     applied;             /* the player written, -2 for none */
   bool    lit;                 /* lights this driver lit */
   bool    has_players;
   uint8_t count;
   uint8_t led[UDEV_LEDS_MAX];
   uint8_t n[UDEV_LEDS_MAX];
   int     fd[UDEV_LEDS_MAX];
};

static struct udev_led_out udev_led_out[MAX_USERS];
/* where the lights are listed; a test's directory in its place */
static const char *udev_led_dir = "/sys/class/leds";
static retro_atomic_int_t udev_led_slot[MAX_USERS][UDEV_LEDS_MAX];
static retro_atomic_int_t udev_led_want[MAX_USERS];

static void udev_led_out_reset(struct udev_led_out *o, int gen)
{
   unsigned i;
   for (i = 0; i < o->count; i++)
      close(o->fd[i]);
   memset(o, 0, sizeof(*o));
   o->gen     = gen;
   o->applied = -2;
}

static void udev_led_write_one(int fd, const char *text)
{
   ssize_t len = (ssize_t)strlen(text);
   if (lseek(fd, 0, SEEK_SET) < 0 || write(fd, text, len) != len)
      RARCH_DBG("[udev] A player light was not written: %s.\n",
            strerror(errno));
}

/* On the writer's thread; on the frontend's only when there is none */
static void udev_led_write(unsigned p)
{
   unsigned i;
   int want;
   struct udev_led_out *o = &udev_led_out[p];
   int gen                = retro_atomic_load_acquire_int(&udev_rumble_gen[p]);

   if (gen != o->gen)
      udev_led_out_reset(o, gen);
   for (i = 0; i < UDEV_LEDS_MAX && o->count < UDEV_LEDS_MAX; i++)
   {
      int v = retro_atomic_exchange_int(&udev_led_slot[p][i], -1);
      if (v < 0)
         continue;
      o->fd[o->count]  = v >> 8;
      o->led[o->count] = (uint8_t)((v >> 4) & 15);
      o->n[o->count]   = (uint8_t)(v & 15);
      if (     o->led[o->count] == LINUX_PAD_LED_DS_PLAYER
            || o->led[o->count] == LINUX_PAD_LED_SW_PLAYER
            || o->led[o->count] == LINUX_PAD_LED_PS3_PLAYER)
         o->has_players = true;
      o->count++;
      o->applied = -2;
   }
   want = retro_atomic_load_acquire_int(&udev_led_want[p]);
   if (!o->count || want == o->applied)
      return;
   o->applied = want;
   /* lights it never lit are left alone */
   if (want < 1 && !o->lit)
      return;
   for (i = 0; i < o->count; i++)
   {
      char text[16];
      uint8_t rgb[3];
      enum linux_pad_led led = (enum linux_pad_led)o->led[i];
      int value;
      if (led == UDEV_LED_RGB_INTENSITY)
         continue;
      value = linux_pad_led_value(led, o->n[i], want, o->has_players, rgb);
      if (value < 0)
         continue;
      if (led == LINUX_PAD_LED_RGB)
      {
         unsigned j;
         /* its colour first, then how bright */
         for (j = 0; j < o->count; j++)
            if (o->led[j] == UDEV_LED_RGB_INTENSITY)
            {
               snprintf(text, sizeof(text), "%u %u %u\n",
                     rgb[0], rgb[1], rgb[2]);
               udev_led_write_one(o->fd[j], text);
            }
      }
      snprintf(text, sizeof(text), "%d\n", value);
      udev_led_write_one(o->fd[i], text);
   }
   o->lit = want >= 1;
}

static void udev_led_close_all(void)
{
   unsigned p;
   for (p = 0; p < MAX_USERS; p++)
      udev_led_out_reset(&udev_led_out[p], udev_led_out[p].gen);
}

static void udev_led_init(void)
{
   unsigned p, i;
   for (p = 0; p < MAX_USERS; p++)
   {
      memset(&udev_led_out[p], 0, sizeof(udev_led_out[p]));
      udev_led_out[p].gen     = retro_atomic_load_acquire_int(&udev_rumble_gen[p]);
      udev_led_out[p].applied = -2;
      retro_atomic_store_release_int(&udev_led_want[p], 0);
      for (i = 0; i < UDEV_LEDS_MAX; i++)
         retro_atomic_store_release_int(&udev_led_slot[p][i], -1);
   }
}

/* The lights left for the writer in slot @p, taken back */
static void udev_led_detach(unsigned p)
{
   unsigned i;
   for (i = 0; i < UDEV_LEDS_MAX; i++)
   {
      int v = retro_atomic_exchange_int(&udev_led_slot[p][i], -1);
      if (v >= 0)
         close(v >> 8);
   }
}

static void udev_led_leave(unsigned p, unsigned *at, int fd,
      unsigned led, unsigned n)
{
   int old;
   if (*at >= UDEV_LEDS_MAX)
   {
      close(fd);
      return;
   }
   old = retro_atomic_exchange_int(&udev_led_slot[p][(*at)++],
         UDEV_LED_PACK(fd, led, n));
   if (old >= 0)
      close(old >> 8);
}

/* Whether the light @name is under @scope, a /sys/devices path: its
 * link in /sys/class/leds is "../../devices/..." */
static bool udev_led_under(const char *scope, size_t scope_len,
      const char *name)
{
   char file[512], link[512];
   ssize_t len;
   if (scope_len < 6 || strncmp(scope, "/sys/", 5))
      return false;
   snprintf(file, sizeof(file), "%s/%s", udev_led_dir, name);
   if ((len = readlink(file, link, sizeof(link) - 1)) < 6)
      return false;
   link[len] = '\0';
   return   !strncmp(link, "../../", 6)
         && (size_t)len > scope_len + 1
         && !strncmp(link + 5, scope + 4, scope_len - 4)
         && link[5 + scope_len - 4] == '/';
}

/* The lights under the pad in slot @p, opened and left for the writer.
 * Read where a pad arrives, and a few times after while it has none: a
 * driver can add a pad's lights just after its input nodes. */
static void udev_led_attach(unsigned p)
{
   DIR *dir;
   struct dirent *e;
   unsigned at             = 0;
   unsigned xpads          = 0;
   bool denied             = false;
   struct udev_joypad *pad = &udev_pads[p];
   size_t scope_len        = strlen(pad->led_scope);

   pad->led_tries++;
   if (!scope_len || !(dir = opendir(udev_led_dir)))
      return;
   /* xpad puts a wireless receiver's four pads' rings under its one USB
    * device, and which is whose is not told: none is taken then */
   while ((e = readdir(dir)))
   {
      unsigned n;
      if (     linux_pad_led_of(e->d_name, &n) == LINUX_PAD_LED_XPAD
            && udev_led_under(pad->led_scope, scope_len, e->d_name))
         xpads++;
   }
   rewinddir(dir);
   while ((e = readdir(dir)))
   {
      char file[512];
      unsigned n;
      int fd;
      enum linux_pad_led led = linux_pad_led_of(e->d_name, &n);

      if (     led == LINUX_PAD_LED_NONE
            || (led == LINUX_PAD_LED_XPAD && xpads > 1)
            || !udev_led_under(pad->led_scope, scope_len, e->d_name))
         continue;
      snprintf(file, sizeof(file), "%s/%s/brightness", udev_led_dir,
            e->d_name);
      if ((fd = open(file, O_WRONLY | O_CLOEXEC)) < 0)
      {
         denied = denied || errno == EACCES || errno == EPERM;
         continue;
      }
      udev_led_leave(p, &at, fd, led, n);
      if (led == LINUX_PAD_LED_RGB)
      {
         snprintf(file, sizeof(file), "%s/%s/multi_intensity",
               udev_led_dir, e->d_name);
         if ((fd = open(file, O_WRONLY | O_CLOEXEC)) >= 0)
            udev_led_leave(p, &at, fd, UDEV_LED_RGB_INTENSITY, 0);
      }
   }
   closedir(dir);

   pad->led_count = (uint8_t)at;
   if (at)
   {
      RARCH_LOG("[udev] Pad #%u: %u light(s) for its player.\n", p, at);
      if (udev_rumble_writer)
         input_output_writer_wake(udev_rumble_writer);
      else
         udev_led_write(p);
   }
   else if (denied)
   {
      pad->led_tries = 255;
      RARCH_LOG("[udev] Pad #%u: its player lights are not writable;"
            " pkg/linux/70-retroarch-player-lights.rules makes them so.\n", p);
   }
}

/* The player pad @pad's lights are to show; 0 none */
static void udev_joypad_set_player(unsigned pad, unsigned player)
{
   if (pad >= MAX_USERS)
      return;
   if (retro_atomic_load_acquire_int(&udev_led_want[pad]) == (int)player)
      return;
   retro_atomic_store_release_int(&udev_led_want[pad], (int)player);
   if (udev_rumble_writer)
      input_output_writer_wake(udev_rumble_writer);
   else
      udev_led_write(pad);
}

static INLINE int16_t udev_compute_axis(const struct input_absinfo *info, int value)
{
   int range = info->maximum - info->minimum;
   int axis  = (value - info->minimum) * 0xffffll / range - 0x7fffll;

   if (axis > 0x7fff)
      return 0x7fff;
   else if (axis < -0x7fff)
      return -0x7fff;
   return axis;
}

static int udev_find_vacant_pad(void)
{
   int i;
   for (i = 0; i < MAX_USERS; i++)
      if (udev_pads[i].fd < 0)
         return i;
   return -1;
}

static int udev_open_joystick(const char *path)
{
   unsigned long evbit[NBITS(EV_MAX)]   = {0};
   unsigned long keybit[NBITS(KEY_MAX)] = {0};
   unsigned long absbit[NBITS(ABS_MAX)] = {0};
   int fd = open(path, O_RDWR | O_NONBLOCK);

   if (fd < 0)
   {
      RARCH_DBG("[udev] Could not open \"%s\": %s.\n", path, strerror(errno));
      return fd;
   }

   if (  (ioctl(fd, EVIOCGBIT(0,      sizeof(evbit)),  evbit)  < 0) ||
         (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) < 0) ||
         (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit) < 0))
      goto error;

   /* Has to at least support EV_KEY interface. */
   if (!test_bit(EV_KEY, evbit))
      goto error;

   return fd;

error:
   close(fd);
   return -1;
}

#ifndef HAVE_LAKKA_SWITCH
static bool udev_set_rumble_gain(unsigned i, unsigned gain)
{
   struct udev_joypad *pad = (struct udev_joypad*)&udev_pads[i];

   /* Does not support > 100 gains */
   if ((pad->fd < 0) ||
       (gain > 100))
      return false;

   if (pad->rumble_gain == gain)
      return true;

   /* written by the rumble writer */
   retro_atomic_store_release_int(&udev_rumble_want_gain[i], (int)gain);
   udev_rumble_wake(i);
   pad->rumble_gain = gain;

   return true;
}
#endif

static void udev_open_sensor_node(struct udev_joypad *pad,
      const char *devnode, unsigned p)
{
   unsigned long propbit[NBITS(INPUT_PROP_MAX)] = {0};
   unsigned long absbit[NBITS(ABS_MAX)]         = {0};
   int fd = open(devnode, O_RDONLY | O_NONBLOCK);

   if (fd < 0)
      return;

   ioctl(fd, EVIOCGPROP(sizeof(propbit)), propbit);

   if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit) >= 0)
   {
      int a;
      bool has_xyz  = test_bit(ABS_X,  absbit)
                   && test_bit(ABS_Y,  absbit)
                   && test_bit(ABS_Z,  absbit);
      bool has_rxyz = test_bit(ABS_RX, absbit)
                   && test_bit(ABS_RY, absbit)
                   && test_bit(ABS_RZ, absbit);

      if (!has_xyz && !has_rxyz)
      {
         /* Node has no usable sensor axes at all — skip it. */
         close(fd);
         return;
      }

      /* Standard layout: ABS_X/Y/Z = accel, ABS_RX/Y/Z = gyro.
       * Wiimote layout:  ABS_RX/Y/Z = accel only (no gyro, no ABS_X/Y/Z). */
      if (!has_xyz && has_rxyz)
      {
         /* Rotational-axis accelerometer (Wiimote).
          * Read absinfo from ABS_RX/Y/Z into sensor_absinfo 0/1/2 */
         pad->sensor_accel_on_rxyz_codes = true;
         pad->sensor_has_accel           = true;
         pad->sensor_has_gyro            = false;

         for (a = 0; a < 3; a++)
         {
            struct input_absinfo *ai = &pad->sensor_absinfo[a];
            if (ioctl(fd, EVIOCGABS(ABS_RX + a), ai) < 0)
            {
               ai->value      = 0;
               ai->minimum    = 0;
               ai->maximum    = 0;
               ai->fuzz       = 0;
               ai->flat       = 0;
               ai->resolution = 0;
            }
            if (ai->resolution == 0)
               ai->resolution = 88;
         }
      }
      else
      {
         /* Standard layout. */
         pad->sensor_accel_on_rxyz_codes = false;
         pad->sensor_has_accel           = has_xyz;
         pad->sensor_has_gyro            = has_rxyz;

         for (a = 0; a < SENSOR_AXES; a++)
         {
            if (test_bit(a, absbit))
               ioctl(fd, EVIOCGABS(a), &pad->sensor_absinfo[a]);
         }
      }
   }
   else
   {
      /* Could not read ABS capabilities — not a sensor. */
      close(fd);
      return;
   }

   pad->sensor_fd   = fd;
   pad->sensor_path = strdup(devnode);

   RARCH_LOG("[udev] Pad #%u: found sensor at %s "
         "(accel=%s%s, gyro=%s).\n",
         p, devnode,
         pad->sensor_has_accel ? "yes" : "no",
         pad->sensor_accel_on_rxyz_codes ? "[RX/RY/RZ]" : "",
         pad->sensor_has_gyro  ? "yes" : "no");
}

/* An event of the touchpad's node into the pad's view */
static void udev_touch_event(struct udev_joypad *pad,
      uint16_t type, uint16_t code, int32_t value)
{
   input_pad_view_t *view = &pad->view;
   unsigned slot          = pad->touch_slot;

   if (type == EV_KEY)
   {
      if (code == BTN_LEFT)
         view->touch_down = (uint8_t)(value
               ? (view->touch_down |  INPUT_PAD_TOUCH_PRESS)
               : (view->touch_down & ~INPUT_PAD_TOUCH_PRESS));
      return;
   }
   if (type != EV_ABS)
      return;
   if (code == ABS_MT_SLOT)
   {
      pad->touch_slot = (uint8_t)(value < 0 ? 0xff : value > 0xfe ? 0xfe : value);
      return;
   }
   if (slot >= INPUT_PAD_VIEW_TOUCHES)
      return;
   switch (code)
   {
      case ABS_MT_TRACKING_ID:
         if (value < 0)
            view->touch_down &= (uint8_t)~(1u << slot);
         else
            view->touch_down |= (uint8_t)(1u << slot);
         break;
      case ABS_MT_POSITION_X:
      case ABS_MT_POSITION_Y:
         {
            unsigned a  = (code == ABS_MT_POSITION_Y);
            int32_t pos = pad->touch_span[a]
               ? (int32_t)(((int64_t)(value - pad->touch_min[a]) * 0xfffe)
                     / pad->touch_span[a]) - 0x7fff
               : 0;
            if (pos < -0x7fff)
               pos = -0x7fff;
            else if (pos > 0x7fff)
               pos = 0x7fff;
            view->touch[slot] = a
               ? ((view->touch[slot] & 0xffff) | ((uint32_t)(uint16_t)(int16_t)pos << 16))
               : ((view->touch[slot] & 0xffff0000u) | (uint16_t)(int16_t)pos);
         }
         break;
      default:
         break;
   }
}

static void udev_open_touch_node(struct udev_joypad *pad, const char *path,
      unsigned p)
{
   struct input_absinfo info;
   int fd;
   unsigned a;

   if ((fd = open(path, O_RDONLY | O_NONBLOCK)) < 0)
      return;
   for (a = 0; a < 2; a++)
   {
      if (ioctl(fd, EVIOCGABS(a ? ABS_MT_POSITION_Y : ABS_MT_POSITION_X), &info) < 0
            || info.maximum <= info.minimum)
      {
         close(fd);
         return;
      }
      pad->touch_min[a]  = info.minimum;
      pad->touch_span[a] = info.maximum - info.minimum;
   }
   pad->touch_fd          = fd;
   pad->touch_path        = strdup(path);
   pad->touch_slot        = 0;
   pad->view.touch_down   = 0;
   pad->view.touchpad     = true;
   RARCH_LOG("[udev] Pad #%u: touchpad at %s.\n", p, path);
}

static void udev_close_touch_node(struct udev_joypad *pad)
{
   if (pad->touch_fd >= 0)
      close(pad->touch_fd);
   if (pad->touch_path)
      free(pad->touch_path);
   pad->touch_fd        = -1;
   pad->touch_path      = NULL;
   pad->view.touchpad   = false;
   pad->view.touch_down = 0;
}

/* The DualShock 3's input report bytes with the pressure of cross,
 * circle, square, triangle, L1, R1, up, down, left and right: the
 * order SDL gives them in, as axes after the pad's own. */
static const uint8_t udev_ds3_pressure_byte[UDEV_DS3_PRESSURES] = {
   24, 23, 25, 22, 20, 21, 14, 16, 17, 15
};

/* Axis @i's value in the lent state, as axis() answers it. */
static void udev_joypad_view_axis(struct udev_joypad *pad, unsigned i)
{
   int16_t val;
   if (i >= INPUT_PAD_VIEW_AXES)
      return;
   val = pad->axes[i];
   /* Deal with analog triggers that report -32767 to 32767 */
   if (pad->neg_trigger[i])
      val = (val + 0x7fff) / 2;
   pad->view.axes[i] = val;
}

/* Hat @h's four directions in the lent state, as button() answers
 * them: up, down, left, right from the low bit. */
static void udev_joypad_view_hat(struct udev_joypad *pad, unsigned h)
{
   unsigned bits = 0;
   if (h >= INPUT_PAD_VIEW_HATS)
      return;
   if (pad->hats[h][1] < 0) bits |= 1;
   if (pad->hats[h][1] > 0) bits |= 2;
   if (pad->hats[h][0] < 0) bits |= 4;
   if (pad->hats[h][0] > 0) bits |= 8;
   pad->view.hats = (uint16_t)((pad->view.hats & ~(0xf << (h * 4)))
         | (bits << (h * 4)));
}

static void udev_pressure_attach(struct udev_joypad *pad, int fd,
      unsigned *axes)
{
   pad->pressure      = true;
   pad->pressure_fd   = fd;
   pad->pressure_axis = (uint8_t)*axes;
   *axes             += UDEV_DS3_PRESSURES;
}

/* The newest report waiting is the pad's state. */
static void udev_pressure_read(struct udev_joypad *pad)
{
   uint8_t report[2][64];
   ssize_t len;
   int n;
   int cur    = 0;
   int newest = -1;

   /* an input report is kept, anything else read over */
   while ((len = read(pad->pressure_fd, report[cur],
               sizeof(report[0]))) > 0)
   {
      if (len > 25 && report[cur][0] == 0x01)
      {
         newest = cur;
         cur   ^= 1;
      }
   }
   if (newest >= 0)
   {
      for (n = 0; n < UDEV_DS3_PRESSURES; n++)
      {
         unsigned b = report[newest][udev_ds3_pressure_byte[n]];
         pad->axes[pad->pressure_axis + n] = (int16_t)((b << 7) | (b >> 1));
         udev_joypad_view_axis(pad, pad->pressure_axis + n);
      }
   }
   /* gone, or no longer ours: the buttons go on without */
   if (len == 0 || (len < 0 && errno != EAGAIN && errno != EINTR))
   {
      close(pad->pressure_fd);
      pad->pressure = false;
      for (n = 0; n < UDEV_DS3_PRESSURES; n++)
      {
         pad->axes[pad->pressure_axis + n] = 0;
         udev_joypad_view_axis(pad, pad->pressure_axis + n);
      }
   }
}

/* The pad's other nodes, under its HID device: listed once, and given
 * their roles (linux_pad_members.h). */
typedef struct udev_pad_nodes
{
   linux_pad_node_t    node[LINUX_PAD_MEMBERS_MAX];
   char                path[LINUX_PAD_MEMBERS_MAX][64];
   linux_pad_members_t members;
} udev_pad_nodes_t;

static void udev_find_members(struct udev_device *gamepad_dev,
      struct udev_joypad *pad, uint64_t own, udev_pad_nodes_t *n)
{
   size_t count = 0;
   struct udev_enumerate *enumerate;
   struct udev_list_entry *item;
   struct udev_device *hid_parent = udev_device_get_parent_with_subsystem_devtype(
         gamepad_dev, "hid", NULL);
   /* a DualShock 3's pressures are in its reports alone */
   bool want_reports = pad->vid == 0x054c && pad->pid == 0x0268;
   const char *syspath = hid_parent ? udev_device_get_syspath(hid_parent) : NULL;

   pad->hid_syspath[0] = '\0';
   if (syspath)
      strlcpy(pad->hid_syspath, syspath, sizeof(pad->hid_syspath));
   if (hid_parent && (enumerate = udev_enumerate_new(udev_joypad_fd)))
   {
      udev_enumerate_add_match_parent(enumerate, hid_parent);
      udev_enumerate_add_match_subsystem(enumerate, "input");
      udev_enumerate_add_match_subsystem(enumerate, "hidraw");
      udev_enumerate_scan_devices(enumerate);
      udev_list_entry_foreach(item, udev_enumerate_get_list_entry(enumerate))
      {
         struct udev_device *dev;
         const char *devnode, *subsystem, *sysname, *tag;
         linux_pad_node_t *node;
         if (count >= LINUX_PAD_MEMBERS_MAX)
            break;
         if (!(dev = udev_device_new_from_syspath(udev_joypad_fd,
                     udev_list_entry_get_name(item))))
            continue;
         devnode   = udev_device_get_devnode(dev);
         subsystem = udev_device_get_subsystem(dev);
         sysname   = udev_device_get_sysname(dev);
         node      = &n->node[count];
         node->hidraw = subsystem && string_is_equal(subsystem, "hidraw");
         node->event  = subsystem && string_is_equal(subsystem, "input")
            && sysname && string_starts_with_size(sysname, "event",
                  STRLEN_CONST("event"));
         if (devnode && (node->hidraw || node->event)
               && strlen(devnode) < sizeof(n->path[0]))
         {
            node->rdev     = (uint64_t)udev_device_get_devnum(dev);
            tag            = udev_device_get_property_value(dev,
                  "ID_INPUT_ACCELEROMETER");
            node->accel    = tag && string_is_equal(tag, "1");
            tag            = udev_device_get_property_value(dev,
                  "ID_INPUT_TOUCHPAD");
            node->touchpad = tag && string_is_equal(tag, "1");
            strlcpy(n->path[count], devnode, sizeof(n->path[0]));
            count++;
         }
         udev_device_unref(dev);
      }
      udev_enumerate_unref(enumerate);
   }
   linux_pad_members_choose(n->node, count, own, want_reports, &n->members);
}

/* The DualShock 3's pressures, from its hidraw node */
static void udev_open_reports(struct udev_joypad *pad,
      const udev_pad_nodes_t *n, unsigned p, unsigned *axes)
{
   const char *devnode;
   int fd;
   if (n->members.reports < 0 || *axes + UDEV_DS3_PRESSURES > NUM_AXES)
      return;
   devnode = n->path[n->members.reports];
   if ((fd = open(devnode, O_RDONLY | O_NONBLOCK | O_CLOEXEC)) >= 0)
   {
      udev_pressure_attach(pad, fd, axes);
      RARCH_LOG("[udev] Pad #%u: button pressures from %s.\n", p, devnode);
   }
   else
      RARCH_LOG("[udev] Pad #%u: no button pressures, %s cannot be"
            " read (%s).\n", p, devnode, strerror(errno));
}

/* The motion sensors - the first node with axes - and the touchpad */
static void udev_open_members(struct udev_joypad *pad,
      const udev_pad_nodes_t *n, unsigned p)
{
   unsigned i;
   for (i = 0; n->members.sensors[i] >= 0 && pad->sensor_fd < 0; i++)
      udev_open_sensor_node(pad, n->path[n->members.sensors[i]], p);
   if (n->members.touchpad >= 0)
      udev_open_touch_node(pad, n->path[n->members.touchpad], p);
}

static int udev_add_pad(struct udev_device *dev, unsigned p, int fd, const char *path)
{
   int i;
   struct stat st;
   int ret                              = 0;
   const char *buf                      = NULL;
   unsigned buttons                     = 0;
   unsigned axes                        = 0;
   struct udev_device *parent           = NULL;
   struct udev_joypad *pad              = (struct udev_joypad*)&udev_pads[p];
   udev_pad_nodes_t nodes;
   struct input_id inputid              = {0};
   unsigned long keybit[NBITS(KEY_MAX)] = {0};
   unsigned long absbit[NBITS(ABS_MAX)] = {0};
   unsigned long ffbit[NBITS(FF_MAX)]   = {0};
   const char *device_name              = input_config_get_device_name(p);
   size_t physlen                       = 0;

   if (!device_name || !*device_name)
      pad->ident[0] = '\0';
   else
      strlcpy(pad->ident, device_name, sizeof(pad->ident));

   /* Failed to get pad name */
   if (ioctl(fd, EVIOCGNAME(sizeof(pad->ident)), pad->ident) < 0)
      return -1;

   pad->vid = pad->pid = 0;

   if (ioctl(fd, EVIOCGID, &inputid) >= 0)
   {
      pad->vid = inputid.vendor;
      pad->pid = inputid.product;
      pad->usb = (inputid.bustype == BUS_USB);
   }

   /* Not a controller, whatever udev says: leave the slot free. */
   if (linux_input_is_not_joystick(pad->ident, pad->vid, pad->pid))
   {
      RARCH_LOG("[udev] Ignoring \"%s\" (%04x:%04x, %s): tagged as a"
            " joystick, but it is not one.\n",
            pad->ident, pad->vid, pad->pid, path);
      pad->ident[0] = '\0';
      pad->vid      = pad->pid = 0;
      return -2;
   }
   /* Another program reads it and stands in for it (Steam Input) */
   if (input_device_ignored((uint16_t)pad->vid, (uint16_t)pad->pid))
   {
      RARCH_LOG("[udev] Ignoring \"%s\" (%04x:%04x, %s): listed in"
            " SDL_GAMECONTROLLER_IGNORE_DEVICES.\n",
            pad->ident, pad->vid, pad->pid, path);
      pad->ident[0] = '\0';
      pad->vid      = pad->pid = 0;
      return -2;
   }
   if (ioctl(fd, EVIOCGPHYS(sizeof(pad->phys)), pad->phys) < 0)
      pad->phys[0] = '\0';  /* Clear if unavailable */
   else
      physlen = strlen(pad->phys);

   if (ioctl(fd, EVIOCGUNIQ(sizeof(pad->phys)-physlen), pad->phys+physlen) < 0)
       pad->phys[physlen] = '\0';  /* Clear if unavailable */

   if (fstat(fd, &st) < 0)
      return -1;

   if ((ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) < 0) ||
            (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit) < 0))
      return -1;

   /* Go through all possible keycodes, check if they are used,
    * and map them to button/axes/hat indices.
    */
   for (i = KEY_UP; i <= KEY_DOWN && buttons < UDEV_NUM_BUTTONS; i++)
      if (test_bit(i, keybit))
         pad->button_bind[i] = buttons++;
   for (i = BTN_MISC; i < KEY_MAX && buttons < UDEV_NUM_BUTTONS; i++)
      if (test_bit(i, keybit))
         pad->button_bind[i] = buttons++;
   /* The following two ranges are scanned and added after the above
    * ranges to maintain compatibility with existing key maps.
    */
   for (i = 0; i < KEY_UP && buttons < UDEV_NUM_BUTTONS; i++)
      if (test_bit(i, keybit))
         pad->button_bind[i] = buttons++;
   for (i = KEY_DOWN + 1; i < BTN_MISC && buttons < UDEV_NUM_BUTTONS; i++)
      if (test_bit(i, keybit))
         pad->button_bind[i] = buttons++;
   for (i = 0; i < ABS_MISC && axes < NUM_AXES; i++)
   {
      /* Skip hats for now. */
      if (i == ABS_HAT0X)
      {
         i = ABS_HAT3Y;
         continue;
      }

      if (test_bit(i, absbit))
      {
         struct input_absinfo *abs = &pad->absinfo[axes];

         if (ioctl(fd, EVIOCGABS(i), abs) < 0)
            continue;
         if (abs->maximum > abs->minimum)
         {
            pad->axes[axes]   = udev_compute_axis(abs, abs->value);
            /* Deal with analog triggers that report -32767 to 32767
               by testing if the axis initial value is negative, allowing for
               for some slop (1300 =~ 4%) in an axis centred around 0.
               The actual work is done in udev_joypad_axis.
               All bets are off if you're sitting on it. Reinitialise it by unpluging
               and plugging back in. */
            if (     (i == ABS_Z || i == ABS_RZ)
                  && udev_compute_axis(abs, abs->value) < -1300)
              pad->neg_trigger[axes] = true;
            udev_joypad_view_axis(pad, axes);
            pad->axes_bind[i] = axes++;
         }
      }
   }

   udev_find_members(dev, pad, (uint64_t)st.st_rdev, &nodes);
   udev_open_reports(pad, &nodes, p, &axes);

   pad->device = st.st_rdev;
   udev_pad_set_fd(p, fd);
   pad->path   = strdup(path);

   udev_open_members(pad, &nodes, p);

   /* its lights: under its HID device, or an Xbox 360 pad's under its
    * USB device */
   pad->led_scope[0] = '\0';
   pad->led_count    = 0;
   pad->led_tries    = 0;
   if (*pad->hid_syspath)
      strlcpy(pad->led_scope, pad->hid_syspath, sizeof(pad->led_scope));
   else
   {
      struct udev_device *usb = udev_device_get_parent_with_subsystem_devtype(
            dev, "usb", "usb_device");
      const char *syspath     = usb ? udev_device_get_syspath(usb) : NULL;
      if (syspath)
         strlcpy(pad->led_scope, syspath, sizeof(pad->led_scope));
   }
   udev_led_attach(p);

   if (*pad->ident)
   {
      input_autoconfigure_connect(
               pad->ident,
               NULL,
               pad->phys,
               udev_joypad.ident,
               p,
               pad->vid,
               pad->pid);

      ret = 1;
   }

   /* Check for rumble features. */
   if (ioctl(fd, EVIOCGBIT(EV_FF, sizeof(ffbit)), ffbit) >= 0)
   {
      if (test_bit(FF_RUMBLE, ffbit))
         RARCH_LOG("[udev] Pad #%u (%s) supports force feedback.\n",
               p, path);

      if (ioctl(fd, EVIOCGEFFECTS, &pad->num_effects) >= 0)
         RARCH_LOG(
               "[udev] Pad #%u (%s) supports %d force feedback effects.\n",
               p, path, pad->num_effects);

      /* the rumble writer is given a descriptor of its own */
      if (test_bit(FF_RUMBLE, ffbit) && pad->num_effects >= 2)
         udev_rumble_attach(p, fd);
   }

#ifndef HAVE_LAKKA_SWITCH
   /* Set rumble gain here, if supported */
   if (test_bit(FF_RUMBLE, ffbit))
   {
      unsigned rumble_gain = input_config_get_rumble_gain();
      udev_set_rumble_gain(p, rumble_gain);
   }
#endif

   return ret;
}

static void udev_check_device(struct udev_device *dev, const char *path)
{
   int i;
   int ret;
   int pad, fd;
   struct stat st;

   if (stat(path, &st) < 0)
      return;

   for (i = 0; i < MAX_USERS; i++)
   {
      if (st.st_rdev == udev_pads[i].device)
      {
         RARCH_LOG(
               "[udev] Device ID %u is already plugged.\n",
               (unsigned)st.st_rdev);
         return;
      }
   }

   if ((pad = udev_find_vacant_pad()) < 0)
      return;

   if ((fd = udev_open_joystick(path)) < 0)
      return;

   /* -1: could not be read. -2: read, and found not to be a
    * controller. Neither keeps the descriptor. */
   if ((ret = udev_add_pad(dev, pad, fd, path)) < 0)
   {
      if (ret == -1)
         RARCH_ERR("[udev] Failed to add pad: %s.\n", path);
      close(fd);
   }
}

static void udev_free_pad(unsigned pad)
{
   udev_led_detach(pad);
   udev_rumble_detach(pad);
   if (udev_pads[pad].fd >= 0)
      close(udev_pads[pad].fd);
   if (udev_pads[pad].sensor_fd >= 0)
      close(udev_pads[pad].sensor_fd);
   udev_close_touch_node(&udev_pads[pad]);
   if (udev_pads[pad].pressure)
      close(udev_pads[pad].pressure_fd);

   if (udev_pads[pad].path)
      free(udev_pads[pad].path);
   if (udev_pads[pad].sensor_path)
      free(udev_pads[pad].sensor_path);
   udev_pads[pad].path = NULL;
   if (*udev_pads[pad].ident)
      udev_pads[pad].ident[0] = '\0';

   memset(&udev_pads[pad], 0, sizeof(udev_pads[pad]));

   udev_pad_set_fd(pad, -1);
   udev_pads[pad].sensor_fd = -1;
   udev_pads[pad].touch_fd  = -1;
}

static void udev_joypad_remove_device(const char *path)
{
   int i;

   for (i = 0; i < MAX_USERS; i++)
   {
      if (     (udev_pads[i].path && *udev_pads[i].path)
            &&  string_is_equal(udev_pads[i].path, path))
      {
         input_autoconfigure_disconnect(i, udev_pads[i].ident);
         udev_free_pad(i);
         break;
      }
   }
}

/* A node that comes or goes after its pad: its motion sensors or its
 * touchpad, as udev_find_members() would have given it a role */
static void udev_hotplug_member(struct udev_device *dev, const char *devnode,
      bool add)
{
   int i;
   struct udev_device *parent;
   const char *syspath, *sysname, *tag;
   linux_pad_node_t node;
   linux_pad_members_t m;

   if (!add)
   {
      for (i = 0; i < MAX_USERS; i++)
      {
         struct udev_joypad *pad = &udev_pads[i];
         if (     pad->touch_fd >= 0 && pad->touch_path
               && string_is_equal(pad->touch_path, devnode))
            udev_close_touch_node(pad);
         else if (pad->sensor_fd >= 0 && pad->sensor_path
               && string_is_equal(pad->sensor_path, devnode))
         {
            close(pad->sensor_fd);
            pad->sensor_fd = -1;
            free(pad->sensor_path);
            pad->sensor_path          = NULL;
            pad->sensor_accel_enabled = false;
            pad->sensor_gyro_enabled  = false;
            pad->sensor_has_accel     = false;
            pad->sensor_has_gyro      = false;
            pad->sensor_accel_on_rxyz_codes = false;
            RARCH_LOG("[udev] Pad #%u: sensor removed.\n", i);
         }
      }
      return;
   }

   sysname = udev_device_get_sysname(dev);
   if (     !sysname || !string_starts_with_size(sysname, "event",
               STRLEN_CONST("event"))
         || !(parent  = udev_device_get_parent_with_subsystem_devtype(
               dev, "hid", NULL))
         || !(syspath = udev_device_get_syspath(parent)))
      return;

   memset(&node, 0, sizeof(node));
   node.event    = true;
   node.rdev     = (uint64_t)udev_device_get_devnum(dev);
   tag           = udev_device_get_property_value(dev, "ID_INPUT_ACCELEROMETER");
   node.accel    = tag && string_is_equal(tag, "1");
   tag           = udev_device_get_property_value(dev, "ID_INPUT_TOUCHPAD");
   node.touchpad = tag && string_is_equal(tag, "1");

   for (i = 0; i < MAX_USERS; i++)
   {
      struct udev_joypad *pad = &udev_pads[i];
      if (     pad->fd < 0 || !*pad->hid_syspath
            || !string_is_equal(pad->hid_syspath, syspath))
         continue;
      linux_pad_members_choose(&node, 1, (uint64_t)pad->device, false, &m);
      if (m.touchpad == 0)
      {
         if (pad->touch_fd < 0)
            udev_open_touch_node(pad, devnode, i);
      }
      else if (m.sensors[0] == 0 && pad->sensor_fd < 0)
         udev_open_sensor_node(pad, devnode, i);
      return;
   }
}

static void udev_joypad_destroy(void)
{
   int i;

   /* the writer first: it closes what it wrote to */
   udev_rumble_stop();

   for (i = 0; i < MAX_USERS; i++)
      udev_free_pad(i);

   if (udev_joypad_mon)
      udev_monitor_unref(udev_joypad_mon);

   if (udev_joypad_fd)
      udev_unref(udev_joypad_fd);

   udev_joypad_mon = NULL;
   udev_joypad_fd  = NULL;
}

static bool udev_set_rumble(unsigned i,
      enum retro_rumble_effect effect, uint16_t strength)
{
   struct udev_joypad *pad = (struct udev_joypad*)&udev_pads[i];

   if (pad->fd < 0)
      return false;
   if (pad->num_effects < 2)
      return false;
   if (effect != RETRO_RUMBLE_STRONG && effect != RETRO_RUMBLE_WEAK)
      return false;

   /* noted here, written by the rumble writer */
   if (pad->strength[effect] != strength)
   {
      pad->strength[effect] = strength;
      retro_atomic_store_release_int(&udev_rumble_want[i],
            (int)((uint32_t)pad->strength[RETRO_RUMBLE_STRONG]
               | ((uint32_t)pad->strength[RETRO_RUMBLE_WEAK] << 16)));
      udev_rumble_wake(i);
   }

   return true;
}

static bool udev_joypad_poll_hotplug_available(struct udev_monitor *dev)
{
   struct pollfd fds;

   fds.fd      = udev_monitor_get_fd(dev);
   fds.events  = POLLIN;
   fds.revents = 0;

   return (poll(&fds, 1, 0) == 1) && (fds.revents & POLLIN);
}

static void udev_joypad_poll(void)
{
   unsigned p;
   static unsigned led_tick;

   udev_rumble_poll();

   /* lights a driver adds just after a pad's input nodes */
   if (!(++led_tick & 63))
      for (p = 0; p < MAX_USERS; p++)
         if (     (udev_pads_active & (1u << p))
               && !udev_pads[p].led_count && udev_pads[p].led_tries < 4)
            udev_led_attach(p);

   while (udev_joypad_mon && udev_joypad_poll_hotplug_available(udev_joypad_mon))
   {
      struct udev_device *dev = udev_monitor_receive_device(udev_joypad_mon);

      if (dev)
      {
         const char *val     = udev_device_get_property_value(dev, "ID_INPUT_JOYSTICK");
         const char *action  = udev_device_get_action(dev);
         const char *devnode = udev_device_get_devnode(dev);

         if (val && string_is_equal(val, "1") && devnode)
         {
            /* Hotplug add */
            if (string_is_equal(action, "add"))
               udev_check_device(dev, devnode);
            /* Hotplug removal */
            else if (string_is_equal(action, "remove"))
               udev_joypad_remove_device(devnode);
            /* Device change */
            else if  (string_is_equal(action, "change"))
            {
               udev_joypad_remove_device(devnode);
               udev_check_device(dev, devnode);
            }
         }
         /* one of a pad's other nodes, coming or going */
         else if (devnode && action)
         {
            if (string_is_equal(action, "add"))
               udev_hotplug_member(dev, devnode, true);
            else if (string_is_equal(action, "remove"))
               udev_hotplug_member(dev, devnode, false);
         }

         udev_device_unref(dev);
      }
   }

   for (p = 0; p < MAX_USERS; p++)
   {
      int i;
      ssize_t _len;
      struct input_event events[32];
      struct udev_joypad *pad = &udev_pads[p];

      if (!(udev_pads_active & (1u << p)))
         continue;

      while ((_len = read(pad->fd, events, sizeof(events))) > 0)
      {
         _len /= sizeof(*events);
         for (i = 0; i < _len; i++)
         {
            uint16_t type = events[i].type;
            uint16_t code = events[i].code;
            int32_t value = events[i].value;

            switch (type)
            {
               case EV_KEY:
                  if (code > 0 && code < KEY_MAX)
                  {
                     if (value)
                        BIT256_SET(pad->view.buttons, pad->button_bind[code]);
                     else
                        BIT256_CLEAR(pad->view.buttons, pad->button_bind[code]);
                  }
                  break;

               case EV_ABS:
                  if (code >= ABS_MISC)
                     break;

                  switch (code)
                  {
                     case ABS_HAT0X:
                     case ABS_HAT0Y:
                     case ABS_HAT1X:
                     case ABS_HAT1Y:
                     case ABS_HAT2X:
                     case ABS_HAT2Y:
                     case ABS_HAT3X:
                     case ABS_HAT3Y:
                        code                           -= ABS_HAT0X;
                        pad->hats[code >> 1][code & 1]  = value;
                        udev_joypad_view_hat(pad, code >> 1);
                        break;
                     default:
                        {
                           unsigned axis   = pad->axes_bind[code];
                           pad->axes[axis] = udev_compute_axis(
                                 &pad->absinfo[axis], value);
                           /* A pad opened before its first report reads
                            * every axis at minimum; a centred first
                            * report means a stick, not a trigger. */
                           if (     pad->neg_trigger[axis]
                                 && !pad->axis_reported[axis]
                                 && abs(pad->axes[axis]) < 0x2000)
                              pad->neg_trigger[axis] = false;
                           pad->axis_reported[axis] = true;
                           udev_joypad_view_axis(pad, axis);
                           break;
                        }
                  }
                  break;

               default:
                  break;
            }
         }
      }

      if (pad->pressure)
         udev_pressure_read(pad);

      if (pad->touch_fd >= 0)
      {
         struct input_event tevents[32];
         ssize_t tlen;
         while ((tlen = read(pad->touch_fd, tevents, sizeof(tevents))) > 0)
         {
            tlen /= sizeof(*tevents);
            for (i = 0; i < tlen; i++)
               udev_touch_event(pad, tevents[i].type, tevents[i].code,
                     tevents[i].value);
         }
      }

      /* Read sensor events from sibling IMU node */
      if (  pad->sensor_fd >= 0
         && (pad->sensor_accel_enabled || pad->sensor_gyro_enabled))
      {
         struct input_event sevents[32];
         ssize_t slen;
         while ((slen = read(pad->sensor_fd, sevents, sizeof(sevents))) > 0)
         {
            int si;
            slen /= sizeof(*sevents);
            for (si = 0; si < slen; si++)
            {
               uint16_t code;
               int res;

               if (sevents[si].type != EV_ABS)
                  continue;

               code = sevents[si].code;

               /* Wiimote / rotational-axis accel:
                * ABS_RX(3)->ACCEL_X(0), ABS_RY(4)->ACCEL_Y(1), ABS_RZ(5)->ACCEL_Z(2).
                * Remap so sensor_data is indexed by RETRO_SENSOR_* IDs. */
               if (pad->sensor_accel_on_rxyz_codes)
               {
                  if (code == ABS_RX)      code = 0;
                  else if (code == ABS_RY) code = 1;
                  else if (code == ABS_RZ) code = 2;
                  else continue; /* no other axes expected on this node */
               }
               else if (code >= SENSOR_AXES)
                  continue;

               /* Normalise raw value to SI units.
                * resolution field = units per g (accel) or units per deg/s (gyro).
                * When resolution == 0 (hid-wiimote doesn't set it), fall back to
                * using absinfo.maximum as the full-scale value so that
                * sensor_data ends up in g (± 1.0 at maximum deflection). */
               res = pad->sensor_absinfo[code].resolution;
               if (res > 0)
                  pad->sensor_data[code] =
                        (float)sevents[si].value / (float)res;
               else
               {
                  int maxval = pad->sensor_absinfo[code].maximum;
                  if (maxval > 0)
                     pad->sensor_data[code] =
                           (float)sevents[si].value / (float)maxval;
                  else
                     pad->sensor_data[sevents[si].code] =
                           (float)sevents[si].value;
               }
            }
         }
      }
   }
}

static void *udev_joypad_init(void *data)
{
   int i;
   unsigned sorted_count = 0;
   struct udev_list_entry *devs     = NULL;
   struct udev_list_entry *item     = NULL;
   struct udev_enumerate *enumerate = NULL;
   struct joypad_udev_entry sorted[MAX_USERS];

   for (i = 0; i < MAX_USERS; i++)
   {
      udev_pad_set_fd(i, -1);
      udev_pads[i].sensor_fd = -1;
      udev_pads[i].touch_fd  = -1;
   }
   udev_rumble_start();

   if (!(udev_joypad_fd = udev_new()))
      return NULL;

   if ((udev_joypad_mon = udev_monitor_new_from_netlink(udev_joypad_fd, "udev")))
   {
      udev_monitor_filter_add_match_subsystem_devtype(
            udev_joypad_mon, "input", NULL);
      udev_monitor_enable_receiving(udev_joypad_mon);
   }

   if (!(enumerate = udev_enumerate_new(udev_joypad_fd)))
      goto error;

   udev_enumerate_add_match_property(enumerate, "ID_INPUT_JOYSTICK", "1");
   udev_enumerate_add_match_subsystem(enumerate, "input");
   udev_enumerate_scan_devices(enumerate);
   if (!(devs = udev_enumerate_get_list_entry(enumerate)))
      RARCH_DBG("[udev] Couldn't open any joypads. Are permissions set correctly for /dev/input/event* and /run/udev/?\n");

   udev_list_entry_foreach(item, devs)
   {
      const char         *name = udev_list_entry_get_name(item);
      struct udev_device  *dev = udev_device_new_from_syspath(udev_joypad_fd, name);
      const char      *devnode = udev_device_get_devnode(dev);
#if defined(DEBUG)
      struct udev_list_entry *list_entry = NULL;
      RARCH_DBG("[udev] udev_joypad_init entry name=%s devnode=%s\n", name, devnode);
      udev_list_entry_foreach(list_entry, udev_device_get_properties_list_entry(dev))
         RARCH_DBG("[udev] udev_joypad_init property %s=%s\n",
                       udev_list_entry_get_name(list_entry),
                       udev_list_entry_get_value(list_entry));
#endif

      if (devnode)
         udev_check_device(dev, devnode);
      udev_device_unref(dev);
   }

   udev_enumerate_unref(enumerate);

   return (void*)-1;

error:
   udev_joypad_destroy();
   return NULL;
}

static int32_t udev_joypad_button_state(
      const struct udev_joypad *pad,
      unsigned port, uint16_t joykey)
{
   unsigned hat_dir = GET_HAT_DIR(joykey);

   if (hat_dir)
   {
      unsigned h = GET_HAT(joykey);
      if (h < NUM_HATS)
      {
         switch (hat_dir)
         {
            case HAT_LEFT_MASK:
               return (pad->hats[h][0] < 0);
            case HAT_RIGHT_MASK:
               return (pad->hats[h][0] > 0);
            case HAT_UP_MASK:
               return (pad->hats[h][1] < 0);
            case HAT_DOWN_MASK:
               return (pad->hats[h][1] > 0);
            default:
               break;
         }
      }
      /* hat requested and no hat button down */
   }
   else if (joykey < UDEV_NUM_BUTTONS)
      return BIT256_GET(pad->view.buttons, joykey) ? 1 : 0;
   return 0;
}

static int32_t udev_joypad_button(unsigned port, uint16_t joykey)
{
   const struct udev_joypad *pad        = (const struct udev_joypad*)
      &udev_pads[port];
   if (port >= MAX_USERS)
      return 0;
   return udev_joypad_button_state(pad, port, joykey);
}

static void udev_joypad_get_buttons(unsigned port, input_bits_t *state)
{
	const struct udev_joypad *pad = (const struct udev_joypad*)
      &udev_pads[port];

	if (pad)
   {
		*state = pad->view.buttons;
	}
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t udev_joypad_axis_state(
      const struct udev_joypad *pad,
      unsigned port, uint32_t joyaxis)
{
   if (AXIS_NEG_GET(joyaxis) < NUM_AXES)
   {
      int16_t val = pad->axes[AXIS_NEG_GET(joyaxis)];
      /* Deal with analog triggers that report -32767 to 32767 */
      if (pad->neg_trigger[AXIS_NEG_GET(joyaxis)])
         val = (val + 0x7fff) / 2;
      if (val < 0)
         return val;
   }
   else if (AXIS_POS_GET(joyaxis) < NUM_AXES)
   {
      int16_t val = pad->axes[AXIS_POS_GET(joyaxis)];
      /* Deal with analog triggers that report -32767 to 32767 */
      if (pad->neg_trigger[AXIS_POS_GET(joyaxis)])
         val = (val + 0x7fff) / 2;
      if (val > 0)
         return val;
   }
   return 0;
}

/* Read in the poll alone: the state is the same until the next. */
static const input_pad_view_t *udev_joypad_view(unsigned port)
{
   return (port < MAX_USERS) ? &udev_pads[port].view : NULL;
}

static int16_t udev_joypad_axis(unsigned port, uint32_t joyaxis)
{
   const struct udev_joypad *pad = (const struct udev_joypad*)
      &udev_pads[port];
   return udev_joypad_axis_state(pad, port, joyaxis);
}

static int16_t udev_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const uint16_t *joykeys,
      const uint32_t *joyaxes,
      unsigned port)
{
   int16_t ret                          = 0;
   uint16_t port_idx                    = joypad_info->joy_idx;

   if (port_idx < MAX_USERS)
   {
      int i;
      const struct udev_joypad *pad     = (const struct udev_joypad*)
         &udev_pads[port_idx];
      for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
      {
         const uint64_t joykey  = joykeys[i];
         const uint32_t joyaxis = joyaxes[i];
         if (
               (uint16_t)joykey != NO_BTN
               && udev_joypad_button_state(pad, port_idx, (uint16_t)joykey)
            )
            ret |= ( 1 << i);
         else if (joyaxis != AXIS_NONE &&
               ((float)abs(udev_joypad_axis_state(pad, port_idx, joyaxis))
                / 0x8000) > joypad_info->axis_threshold)
            ret |= (1 << i);
      }
   }

   return ret;
}

static bool udev_joypad_query_pad(unsigned pad)
{
   return pad < MAX_USERS && udev_pads[pad].fd >= 0;
}

static const char *udev_joypad_name(unsigned pad)
{
   if (pad >= MAX_USERS || !*udev_pads[pad].ident)
      return NULL;
   return udev_pads[pad].ident;
}

static bool udev_joypad_set_sensor_state(unsigned port,
      enum retro_sensor_action action, unsigned rate)
{
   struct udev_joypad *pad;

   if (port >= MAX_USERS)
      return false;

   pad = &udev_pads[port];

   if (pad->sensor_fd < 0)
      return false;

   switch (action)
   {
      case RETRO_SENSOR_ACCELEROMETER_ENABLE:
         if (!pad->sensor_has_accel)
            return false;
         pad->sensor_accel_enabled = true;
         return true;
      case RETRO_SENSOR_ACCELEROMETER_DISABLE:
         pad->sensor_accel_enabled = false;
         return true;
      case RETRO_SENSOR_GYROSCOPE_ENABLE:
         if (!pad->sensor_has_gyro)
            return false;
         pad->sensor_gyro_enabled = true;
         return true;
      case RETRO_SENSOR_GYROSCOPE_DISABLE:
         pad->sensor_gyro_enabled = false;
         return true;
      default:
         break;
   }
   return false;
}

static bool udev_joypad_get_sensor_input(unsigned port,
      unsigned id, float *value)
{
   const struct udev_joypad *pad;

   if (port >= MAX_USERS)
      return false;

   pad = &udev_pads[port];

   if (pad->sensor_fd < 0)
      return false;

   if (id > RETRO_SENSOR_GYROSCOPE_Z)
      return false;

   /* Check if the requested sensor type is enabled */
   if (id <= RETRO_SENSOR_ACCELEROMETER_Z)
   {
      if (!pad->sensor_accel_enabled)
         return false;
      /* Accelerometer: resolution is units/g, so sensor_data is already in g */
      *value = pad->sensor_data[id];
   }
   else
   {
      if (!pad->sensor_gyro_enabled)
         return false;
      /* Gyroscope: resolution is units/(deg/s), convert to rad/s */
      *value = pad->sensor_data[id] * DEG_TO_RAD_F;
   }

   return true;
}

#if defined(__linux__)
/* A DualShock 3 on USB keeps the address it connects to over
 * Bluetooth in feature report 0xF5, from its third byte; it is reached
 * through the hidraw node its pressures come from. */
static bool udev_joypad_get_bt_host(unsigned port, uint8_t *addr)
{
   uint8_t buf[8];
   const struct udev_joypad *pad;

   if (port >= MAX_USERS)
      return false;
   pad = &udev_pads[port];
   if (!pad->pressure || !pad->usb)
      return false;
   memset(buf, 0, sizeof(buf));
   buf[0] = 0xf5;
   if (ioctl(pad->pressure_fd, HIDIOCGFEATURE(sizeof(buf)), buf)
         < (int)sizeof(buf))
      return false;
   memcpy(addr, buf + 2, 6);
   return true;
}

static bool udev_joypad_set_bt_host(unsigned port, const uint8_t *addr)
{
   uint8_t buf[8];
   const struct udev_joypad *pad;

   if (port >= MAX_USERS)
      return false;
   pad = &udev_pads[port];
   if (!pad->pressure || !pad->usb)
      return false;
   buf[0] = 0xf5;
   buf[1] = 0x00;
   memcpy(buf + 2, addr, 6);
   return ioctl(pad->pressure_fd, HIDIOCSFEATURE(sizeof(buf)), buf) >= 0;
}
#endif

input_device_driver_t udev_joypad = {
   udev_joypad_init,
   udev_joypad_query_pad,
   udev_joypad_destroy,
   udev_joypad_button,
   udev_joypad_state,
   udev_joypad_get_buttons,
   udev_joypad_axis,
   udev_joypad_poll,
   udev_set_rumble,
#ifndef HAVE_LAKKA_SWITCH
   udev_set_rumble_gain,
#else
   NULL, /* set_rumble_gain */
#endif
   udev_joypad_set_sensor_state,
   udev_joypad_get_sensor_input,
   udev_joypad_name,
   "udev",
#if defined(__linux__)
   udev_joypad_get_bt_host,
   udev_joypad_set_bt_host,
#else
   NULL, /* get_bt_host */
   NULL, /* set_bt_host */
#endif
   NULL, /* survives_video */
   udev_joypad_set_player,
   udev_joypad_view
};
