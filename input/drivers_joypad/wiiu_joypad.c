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

#include "wiiu_dbg.h"

static input_device_driver_t *pad_drivers[MAX_USERS];
static bool ready = false;

static bool wiiu_joypad_init(void *data);
static bool wiiu_joypad_query_pad(unsigned pad);
static void wiiu_joypad_destroy(void);
static bool wiiu_joypad_button(unsigned pad, uint16_t button);
static void wiiu_joypad_get_buttons(unsigned pad, retro_bits_t *state);
static int16_t wiiu_joypad_axis(unsigned pad, uint32_t axis);
static void wiiu_joypad_poll(void);
static const char *wiiu_joypad_name(unsigned pad);

/**
 * Translates a pad to its appropriate driver.
 * Note that this is a helper for build_pad_map and shouldn't be
 * used directly.
 */
static input_device_driver_t *get_driver_for_pad(unsigned pad)
{
  if(wpad_driver.query_pad(pad))
    return &wpad_driver;
  if(kpad_driver.query_pad(pad))
    return &kpad_driver;

  return &hidpad_driver;
}

/**
 * Populates the pad_driver array. We do this once at init time so
 * that lookups at runtime are constant time.
 */
static void build_pad_map(void)
{
  unsigned i;

  for(i = 0; i < MAX_USERS; i++)
  {
    pad_drivers[i] = get_driver_for_pad(i);
  }
}

static bool wiiu_joypad_init(void* data)
{
  // the sub-drivers have to init first, otherwise
  // build_pad_map will fail (because all lookups will return false).
  wpad_driver.init(data);
  kpad_driver.init(data);
  hidpad_driver.init(data);

  build_pad_map();

  ready = true;
  (void)data;

  return true;
}

static bool wiiu_joypad_query_pad(unsigned pad)
{
  return ready && pad < MAX_USERS;
}

static void wiiu_joypad_destroy(void)
{
  ready = false;

  wpad_driver.destroy();
  kpad_driver.destroy();
  hidpad_driver.destroy();
}

static bool wiiu_joypad_button(unsigned pad, uint16_t key)
{
  if(!wiiu_joypad_query_pad(pad))
    return false;

  return pad_drivers[pad]->button(pad, key);
}

static void wiiu_joypad_get_buttons(unsigned pad, retro_bits_t *state)
{
  if(!wiiu_joypad_query_pad(pad))
    return;

  pad_drivers[pad]->get_buttons(pad, state);
}

static int16_t wiiu_joypad_axis(unsigned pad, uint32_t joyaxis)
{
  if(!wiiu_joypad_query_pad(pad))
    return 0;

  return pad_drivers[pad]->axis(pad, joyaxis);
}

static void wiiu_joypad_poll(void)
{
  wpad_driver.poll();
  kpad_driver.poll();
  hidpad_driver.poll();
}

static const char* wiiu_joypad_name(unsigned pad)
{
  if(!wiiu_joypad_query_pad(pad))
    return "N/A";

  return pad_drivers[pad]->name(pad);
}

input_device_driver_t wiiu_joypad =
{
  wiiu_joypad_init,
  wiiu_joypad_query_pad,
  wiiu_joypad_destroy,
  wiiu_joypad_button,
  wiiu_joypad_get_buttons,
  wiiu_joypad_axis,
  wiiu_joypad_poll,
  NULL,
  wiiu_joypad_name,
  "wiiu",
};

