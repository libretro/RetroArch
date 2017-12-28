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

// if the GameCube adapter is attached, this will be the offset
// of the first pad.
static unsigned gca_pad = 0;
static joypad_connection_t *hid_pads;

static bool hidpad_init(void *data)
{
  (void *)data;
  hid_pads = pad_connection_init(MAX_USERS-(WIIU_WIIMOTE_CHANNELS+1));

  hidpad_poll();
  ready = true;

  return true;
}

static bool hidpad_query_pad(unsigned pad)
{
  return (pad > WIIU_WIIMOTE_CHANNELS && pad < MAX_USERS);
}

static void hidpad_destroy(void)
{
  ready = false;
  if(hid_pads) {
    pad_connection_destroy(hid_pads);
    hid_pads = NULL;
  }
}

static bool hidpad_button(unsigned pad, uint16_t button)
{
  return false;
}

static void hidpad_get_buttons(unsigned pad, retro_bits_t *state)
{
  BIT256_CLEAR_ALL_PTR(state);
}

static int16_t hidpad_axis(unsigned pad, uint32_t axis)
{
  return 0;
}

static void hidpad_poll(void)
{
}

static const char *hidpad_name(unsigned pad)
{
  if(!hidpad_query_pad(pad))
    return "n/a";

  return PAD_NAME_HID;
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
