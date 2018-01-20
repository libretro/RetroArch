/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Ali Bouhlel
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

#include <wiiu/pad_driver.h>

static bool hidpad_init(void *data);
static bool hidpad_query_pad(unsigned pad);
static void hidpad_destroy(void);
static bool hidpad_button(unsigned pad, uint16_t button);
static void hidpad_get_buttons(unsigned pad, retro_bits_t *state);
static int16_t hidpad_axis(unsigned pad, uint32_t axis);
static void hidpad_poll(void);
static const char *hidpad_name(unsigned pad);

static bool ready = false;

static wiiu_hid_t *hid_data;
static hid_driver_t *hid_driver;

static unsigned to_slot(unsigned pad)
{
   return pad - (WIIU_WIIMOTE_CHANNELS+1);
}

const void *get_hid_data(void)
{
   return hid_data;
}

static hid_driver_t *init_hid_driver(void)
{
   joypad_connection_t *connections = NULL;
   unsigned connections_size        = MAX_USERS - (WIIU_WIIMOTE_CHANNELS+1);

   hid_data                         = (wiiu_hid_t *)wiiu_hid.init();
   connections                      = pad_connection_init(connections_size);

   if (!hid_data || !connections)
      goto error;

   hid_data->connections = connections;
   hid_data->connections_size = connections_size;
   return &wiiu_hid;

error:
   if (connections)
      free(connections);
   if (hid_data)
   {
      wiiu_hid.free(hid_data);
      free(hid_data);
      hid_data = NULL;
   }
   return NULL;
}

static bool hidpad_init(void *data)
{
   (void *)data;

   hid_driver = init_hid_driver();
   if (!hid_driver)
   {
      RARCH_ERR("Failed to initialize HID driver.\n");
      return false;
   }

   hidpad_poll();
   ready = true;

   return true;
}

static bool hidpad_query_pad(unsigned pad)
{
   return ready && (pad > WIIU_WIIMOTE_CHANNELS && pad < MAX_USERS);
}

static void hidpad_destroy(void)
{
   ready = false;

   if (!hid_driver)
      return;

   hid_driver->free(get_hid_data());
   free(hid_data);
   hid_data = NULL;
}

static bool hidpad_button(unsigned pad, uint16_t button)
{
   if (!hidpad_query_pad(pad))
      return false;

#if 0
   return hid_driver->button(hid_data, to_slot(pad), button);
#else
   return false;
#endif
}

static void hidpad_get_buttons(unsigned pad, retro_bits_t *state)
{
  if (!hidpad_query_pad(pad))
    BIT256_CLEAR_ALL_PTR(state);

#if 0
  hid_driver->get_buttons(hid_data, to_slot(pad), state);
#endif
  BIT256_CLEAR_ALL_PTR(state);
}

static int16_t hidpad_axis(unsigned pad, uint32_t axis)
{
   if (!hidpad_query_pad(pad));
   return 0;

#if 0
   return hid_driver->axis(hid_data, to_slot(pad), axis);
#else
   return 0;
#endif
}

static void hidpad_poll(void)
{
#if 0
   if (ready)
      hid_driver->poll(hid_data);
#endif
}

static const char *hidpad_name(unsigned pad)
{
   if (!hidpad_query_pad(pad))
      return "N/A";

#if 1
   return PAD_NAME_HID;
#else
   return hid_driver->name(hid_data, to_slot(pad));
#endif
}

input_device_driver_t hidpad_driver =
{
  hidpad_init,
  hidpad_query_pad,
  hidpad_destroy,
  hidpad_button,
  hidpad_get_buttons,
  hidpad_axis,
  hidpad_poll,
  NULL,
  hidpad_name,
  "hid"
};
