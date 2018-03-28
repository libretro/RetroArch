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
#include "../../input/include/hid_driver.h"
#include "../../input/common/hid/hid_device_driver.h"

static bool hidpad_init(void *data);
static bool hidpad_query_pad(unsigned pad);
static void hidpad_destroy(void);
static bool hidpad_button(unsigned pad, uint16_t button);
static void hidpad_get_buttons(unsigned pad, retro_bits_t *state);
static int16_t hidpad_axis(unsigned pad, uint32_t axis);
static void hidpad_poll(void);
static const char *hidpad_name(unsigned pad);

static bool ready = false;

static bool init_hid_driver(void)
{
   memset(&hid_instance, 0, sizeof(hid_instance));

   return hid_init(&hid_instance, &wiiu_hid, &hidpad_driver, MAX_USERS);
}

static bool hidpad_init(void *data)
{
   (void *)data;
   int i;

   if(!init_hid_driver())
   {
      RARCH_ERR("Failed to initialize HID driver.\n");
      return false;
   }

/*   hid_instance.pad_list[0].connected = true; */

   for(i = 0; i < (WIIU_WIIMOTE_CHANNELS+1); i++)
   {
      hid_instance.pad_list[i].connected = true;
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

   hid_deinit(&hid_instance);
}

static bool hidpad_button(unsigned pad, uint16_t button)
{
   if (!hidpad_query_pad(pad))
      return false;

#if 0
   return hid_driver->button(hid_data, pad, button);
#else
   return false;
#endif
}

static void hidpad_get_buttons(unsigned pad, retro_bits_t *state)
{
  if (!hidpad_query_pad(pad))
    BIT256_CLEAR_ALL_PTR(state);

#if 0
  hid_driver->get_buttons(hid_data, pad, state);
#endif
  BIT256_CLEAR_ALL_PTR(state);
}

static int16_t hidpad_axis(unsigned pad, uint32_t axis)
{
   if (!hidpad_query_pad(pad));
   return 0;

#if 0
   return hid_driver->axis(hid_data, pad, axis);
#else
   return 0;
#endif
}

static void hidpad_poll(void)
{
   if (ready)
      hid_instance.os_driver->poll(hid_instance.os_driver_data);
}

static const char *hidpad_name(unsigned pad)
{
   if (!hidpad_query_pad(pad))
      return "N/A";

#if 1
   return PAD_NAME_HID;
#else
   return hid_driver->name(hid_data, pad);
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
