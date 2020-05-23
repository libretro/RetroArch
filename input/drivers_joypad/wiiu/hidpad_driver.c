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

#include "../../include/wiiu/input.h"

static bool hidpad_init(void *data);
static bool hidpad_query_pad(unsigned pad);
static void hidpad_destroy(void);
static bool hidpad_button(unsigned pad, uint16_t button);
static void hidpad_get_buttons(unsigned pad, input_bits_t *state);
static int16_t hidpad_axis(unsigned pad, uint32_t axis);
static void hidpad_poll(void);
static const char *hidpad_name(unsigned pad);

static bool hidpad_ready = false;

static bool init_hid_driver(void)
{
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

   hidpad_poll();
   hidpad_ready = true;

   return true;
}

static bool hidpad_query_pad(unsigned pad)
{
   return hidpad_ready && pad < MAX_USERS;
}

static void hidpad_destroy(void)
{
   hidpad_ready = false;

   hid_deinit(&hid_instance);
}

static bool hidpad_button(unsigned pad, uint16_t button)
{
   if (!hidpad_query_pad(pad))
      return false;

   return HID_BUTTON(pad, button);
}

static void hidpad_get_buttons(unsigned pad, input_bits_t *state)
{
  if (!hidpad_query_pad(pad))
    BIT256_CLEAR_ALL_PTR(state);

  HID_GET_BUTTONS(pad, state);
}

static int16_t hidpad_axis(unsigned pad, uint32_t axis)
{
   if (!hidpad_query_pad(pad))
      return 0;

   return HID_AXIS(pad, axis);
}

static void hidpad_poll(void)
{
   if (hidpad_ready)
      HID_POLL();
}

static const char *hidpad_name(unsigned pad)
{
   if (!hidpad_query_pad(pad))
      return "N/A";

   return HID_PAD_NAME(pad);
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
