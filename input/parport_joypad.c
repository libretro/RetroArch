/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2014 - Mike Robinson
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

#include "input_common.h"
#include "../general.h"
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <linux/parport.h>
#include <linux/ppdev.h>
#include <sys/ioctl.h>
#include <fcntl.h>

/* Linux parport driver does not support reading the control register
   Other platforms may support up to 17 buttons */
#define NUM_BUTTONS 13

struct parport_joypad
{
   int fd;
   bool buttons[NUM_BUTTONS];
   bool button_enable[NUM_BUTTONS];
   char saved_data;
   char saved_control;
   char *ident;
};

static struct parport_joypad g_pads[MAX_PLAYERS];

static void poll_pad(struct parport_joypad *pad)
{
   /* RetroArch uses an extended version of the Linux
   * Multisystem 2-button joystick protocol for parallel port
   * joypads:
   *
   * | Function    | Pin | Register | Bit | Active |
   * |-------------|-----|----------|-----|--------|
   * | Up          | 2   | Data     | 0   | Low    |
   * | Down        | 3   | Data     | 1   | Low    |
   * | Left        | 4   | Data     | 2   | Low    |
   * | Right       | 5   | Data     | 3   | Low    |
   * | A           | 6   | Data     | 4   | Low    |
   * | B           | 7   | Data     | 5   | Low    |
   * | Start       | 8   | Data     | 6   | Low    |
   * | Select      | 9   | Data     | 7   | Low    |
   * | Menu toggle | 10  | Status   | 6   | Low    |
   * | X           | 11  | Status   | 7   | Low*   |
   * | Y           | 12  | Status   | 5   | Low    |
   * | L1          | 13  | Status   | 4   | Low    |
   * | R1          | 15  | Status   | 3   | Low    |
   *
   * (*) Pin is hardware inverted, but RetroArch inverts it
   *     back again so the same pullup scheme may be used for
   *     all pins.
   *
   * Pin 1 is set high so it can be used for pullups.
   *
   * RetroArch does not perform debouncing, and so long as
   * the button settling time is less than the frame time
   * no bouncing will be observed. This replicates the latching
   * behavior common in old games consoles. For optimum latency
   * and jitter a high performance debouncing routine should be
   * implemented in the controller hardware.
   */

   int i;
   char data;
   char status;

   ioctl(pad->fd, PPRDATA, &data);
   ioctl(pad->fd, PPRSTATUS, &status);

   for (i = 0; i < 8; i++)
   {
      pad->buttons[i] = !(data & UINT8_C(1 << i)) && pad->button_enable[i];
   }
   for (i = 3; i < 8; i++)
   {
      pad->buttons[i + 5] = !(status & UINT8_C(1 << i)) && pad->button_enable[i];
   }
   pad->buttons[12] = pad->buttons[12] ? false : true && pad->button_enable[12];
}

static bool parport_joypad_init_pad(const char *path, struct parport_joypad *pad)
{
   int datadir = 1; /* read */
   char data;
   struct ppdev_frob_struct frob;
   bool set_control = false;
   int i;

   int mode = IEEE1284_MODE_BYTE;

   if (pad->fd >= 0)
      return false;

   if (access(path, R_OK | W_OK) < 0)
      return false;

   pad->fd = open(path, O_RDWR);

   *pad->ident = '\0';
   if (pad->fd >= 0)
   {
      RARCH_LOG("[Joypad]: Found parallel port: %s\n", path);

      /* Parport driver does not log failures with RARCH_ERR because they could be
       * a normal result of connected non-joypad devices. */
      if (ioctl(pad->fd, PPCLAIM) < 0)
      {
         RARCH_WARN("[Joypad]: Failed to claim %s\n", path);
         goto error;
      }
      if (ioctl(pad->fd, PPSETMODE, &mode) < 0)
      {
         RARCH_WARN("[Joypad]: Failed to set byte mode on %s\n", path);
         goto error;
      }
      if (ioctl(pad->fd, PPDATADIR, &datadir) < 0)
      {
         RARCH_WARN("[Joypad]: Failed to set data direction to input on %s\n", path);
         goto error;
      }

      if (ioctl(pad->fd, PPRDATA, &data) < 0)
      {
         RARCH_WARN("[Joypad]: Failed to save original data register on %s\n", path);
         goto error;
      }
      pad->saved_data = data;

      if (ioctl(pad->fd, PPRCONTROL, &data) == 0)
      {
         pad->saved_control = data;
         /* Clear strobe bit to set strobe high for pullup +V */
         /* Clear control bit 4 to disable interrupts */
         frob.mask = PARPORT_CONTROL_STROBE | (UINT8_C(1 << 4));
         frob.val = 0;
         if (ioctl(pad->fd, PPFCONTROL, &frob) == 0)
            set_control = true;
      }
      else
      {
         data = pad->saved_data;
         if (ioctl(pad->fd, PPWDATA, &data) < 0)
            RARCH_WARN("[Joypad]: Failed to restore original data register on %s\n", path);
         RARCH_WARN("[Joypad]: Failed to save original control register on %s\n", path);
         goto error;
      }

      /* Failure to enable strobe can break Linux Multisystem style controllers.
       * Controllers using an alternative power source will still work.
       * Failure to disable interrupts slightly increases CPU usage. */
      if (!set_control)
         RARCH_WARN("[Joypad]: Failed to clear nStrobe and nIRQ bits on %s\n", path);

      strlcpy(pad->ident, path, sizeof(g_settings.input.device_names[0]));

      for (i = 0; i < NUM_BUTTONS; i++)
         pad->button_enable[i] = true;

      return true;
error:
      close(pad->fd);
      pad->fd = 0;
      return false;
   }

   RARCH_WARN("[Joypad]: Failed to open parallel port %s (error: %s).\n", path, strerror(errno));
   return false;
}

static void parport_joypad_poll(void)
{
   int i;

   for (i = 0; i < MAX_PLAYERS; i++)
   {
      if (g_pads[i].fd >= 0)
      {
         poll_pad(&g_pads[i]);
      }
   }
}

static void destroy_pad(struct parport_joypad *pad)
{
   char data;
   data = pad->saved_data;
   if (ioctl(pad->fd, PPWDATA, &data) < 0)
      RARCH_ERR("[Joypad]: Failed to restore original data register on %s\n", pad->ident);

   data = pad->saved_control;
   if (ioctl(pad->fd, PPWDATA, &data) < 0)
      RARCH_ERR("[Joypad]: Failed to restore original control register on %s\n", pad->ident);

   if (ioctl(pad->fd, PPRELEASE) < 0)
      RARCH_ERR("[Joypad]: Failed to release parallel port %s\n", pad->ident);

   close(pad->fd);
   pad->fd = -1;
}

static bool parport_joypad_init(void)
{
   unsigned i, j;
   bool found_enabled_button;
   bool found_disabled_button;
   char buf[NUM_BUTTONS * 3 + 1];
   char pin[3 + 1];

   for (i = 0; i < MAX_PLAYERS; i++)
   {
      struct parport_joypad *pad = &g_pads[i];
      pad->fd = -1;
      pad->ident = g_settings.input.device_names[i];

      char path[PATH_MAX];
      snprintf(path, sizeof(path), "/dev/parport%u", i);

      if (parport_joypad_init_pad(path, pad))
      {
         /* If a pin is low on initialization it can either mean
          * a button is pressed or that nothing is connected.
          * Polling non-connected pins can render the menu unusable
          * so assume the user is not holding any button on startup
          * and disable any low pins.
          */
         poll_pad(pad);
         found_enabled_button = false;
         found_disabled_button = false;

         for (j = 0; j < NUM_BUTTONS; j++)
         {
            if (!pad->buttons[j])
            {
               pad->button_enable[j] = true;
               found_enabled_button = true;
            }
            else
            {
               pad->button_enable[j] = false;
               found_disabled_button = true;
            }
         }

         if (found_enabled_button)
         {
            if (found_disabled_button)
            {
               buf[0] = '\0';
               for (j = 0; j < NUM_BUTTONS; j++)
               {
                  if (!pad->button_enable[j])
                  {
                     snprintf(pin, sizeof(pin), "%d ", j);
                     strlcat(buf, pin, sizeof(buf));
                  }
               }
               RARCH_WARN("[Joypad]: Pin(s) %son %s were low on init, assuming not connected\n", \
                                       buf, path);
            }
            input_config_autoconfigure_joypad(i, pad->ident, 0, 0, "parport");
         }
         else
         {
            RARCH_WARN("[Joypad]: All pins low on %s, assuming nothing connected\n", path);
            destroy_pad(pad);
            input_config_autoconfigure_joypad(i, NULL, 0, 0, NULL);
         }
      }
      else
         input_config_autoconfigure_joypad(i, NULL, 0, 0, NULL);
      }

   return true;
}

static void parport_joypad_destroy(void)
{
   unsigned i;
   struct parport_joypad *pad;

   for (i = 0; i < MAX_PLAYERS; i++)
   {
      pad = &g_pads[i];
      if (pad->fd >= 0)
      {
         destroy_pad(pad);
      }
   }
   memset(g_pads, 0, sizeof(g_pads));
   for (i = 0; i < MAX_PLAYERS; i++)
      g_pads[i].fd = -1;
}

static bool parport_joypad_button(unsigned port, uint16_t joykey)
{
   const struct parport_joypad *pad = &g_pads[port];

   return joykey < NUM_BUTTONS && pad->buttons[joykey];
}

static int16_t parport_joypad_axis(unsigned port, uint32_t joyaxis)
{
   /* Parport does not support analog sticks */
   return 0;
}

static bool parport_joypad_query_pad(unsigned pad)
{
   return pad < MAX_PLAYERS && g_pads[pad].fd >= 0;
}

static const char *parport_joypad_name(unsigned pad)
{
   if (pad >= MAX_PLAYERS)
      return NULL;

   return *g_pads[pad].ident ? g_pads[pad].ident : NULL;
}

rarch_joypad_driver_t parport_joypad = {
   parport_joypad_init,
   parport_joypad_query_pad,
   parport_joypad_destroy,
   parport_joypad_button,
   parport_joypad_axis,
   parport_joypad_poll,
   NULL,
   parport_joypad_name,
   "parport",
};
