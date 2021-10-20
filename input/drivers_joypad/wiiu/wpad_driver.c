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

/**
 * This driver handles the Wii U Gamepad.
 *
 * - For wiimote and wiimote attachments, see kpad_driver.c
 * - For HID controllers, see hid_driver.c
 */

#include "../../include/wiiu/input.h"

#define PANIC_BUTTON_MASK (VPAD_BUTTON_R | VPAD_BUTTON_L | VPAD_BUTTON_STICK_R | VPAD_BUTTON_STICK_L)

#define WPAD_INVALID_CHANNEL -1

static VPADChan to_gamepad_channel(unsigned pad)
{
   unsigned i;

   for (i = 0; i < WIIU_GAMEPAD_CHANNELS; i++)
   {
      if (joypad_state.wpad.channel_slot_map[i] == pad)
         return i;
   }

   return WPAD_INVALID_CHANNEL;
}

static void wpad_deregister(unsigned channel)
{
   unsigned slot;

   if (channel >= WIIU_GAMEPAD_CHANNELS)
      return;

   /* See if Gamepad is already disconnected */
   if (joypad_state.wpad.channel_slot_map[channel] == WPAD_INVALID_CHANNEL)
      return;

   /* Sanity check, about to use as unsigned */
   if (joypad_state.wpad.channel_slot_map[channel] < 0)
   {
      joypad_state.wpad.channel_slot_map[channel] = WPAD_INVALID_CHANNEL;
      return;
   }

   slot = (unsigned)joypad_state.wpad.channel_slot_map[channel];
   if (slot >= MAX_USERS)
      return;

   input_autoconfigure_disconnect(slot, wpad_driver.ident);
   joypad_state.pads[slot].connected = false;
   joypad_state.wpad.channel_slot_map[channel] = WPAD_INVALID_CHANNEL;
}

static void wpad_register(unsigned channel)
{
   int slot;

   if (channel >= WIIU_GAMEPAD_CHANNELS)
      return;

   /* Check if gamepad is already handled
      Other checks not needed here - about to overwrite 
      joypad_state.wpad.channel_slot_map entry*/
   if (joypad_state.wpad.channel_slot_map[channel] != WPAD_INVALID_CHANNEL)
      return;

   slot = pad_connection_find_vacant_pad(joypad_state.pads);
   if(slot < 0)
      return;

   joypad_state.pads[slot].connected = true;
   joypad_state.pads[slot].input_driver = &wpad_driver;
   input_pad_connect(slot, &wpad_driver);
   joypad_state.wpad.channel_slot_map[channel] = slot;
}

static void update_button_state(uint64_t *state, uint32_t held_buttons)
{
   *state = held_buttons & VPAD_MASK_BUTTONS;
}

static void update_analog_state(int16_t state[3][2], VPADStatus *vpad)
{
   state[RETRO_DEVICE_INDEX_ANALOG_LEFT] [RETRO_DEVICE_ID_ANALOG_X] = WIIU_READ_STICK(vpad->leftStick.x);
   state[RETRO_DEVICE_INDEX_ANALOG_LEFT] [RETRO_DEVICE_ID_ANALOG_Y] = WIIU_READ_STICK(vpad->leftStick.y);
   state[RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_X] = WIIU_READ_STICK(vpad->rightStick.x);
   state[RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_Y] = WIIU_READ_STICK(vpad->rightStick.y);
}

static int16_t scale_touchpad(int16_t from_min, int16_t from_max,
      int16_t to_min,   int16_t to_max, int16_t value )
{
   int32_t from_range = from_max - from_min;
   int32_t to_range   = to_max - to_min;

   return (((value - from_min) * to_range) / from_range) + to_min;
}

static void get_calibrated_point(VPADTouchData *point,
      struct video_viewport *viewport, VPADStatus *vpad, unsigned channel)
{
   VPADTouchData calibrated720p = {0};

   VPADGetTPCalibratedPoint(channel, &calibrated720p, &(vpad->tpNormal));
   point->x = scale_touchpad(12, 1268, 0, viewport->full_width, calibrated720p.x);
   point->y = scale_touchpad(12, 708, 0, viewport->full_height, calibrated720p.y);
}

static void apply_clamping(VPADTouchData *point, struct video_viewport *viewport, bool *clamped)
{
   /* clamp the x domain to the viewport */
   if (point->x < viewport->x)
   {
      point->x = viewport->x;
      *clamped = true;
   }
   else if (point->x > (viewport->x + viewport->width))
   {
      point->x = viewport->x + viewport->width;
      *clamped = true;
   }

   /* clamp the y domain to the viewport */
   if (point->y < viewport->y)
   {
      point->y = viewport->y;
      *clamped = true;
   }
   else if (point->y > (viewport->y + viewport->height))
   {
      point->y =  viewport->y + viewport->height;
      *clamped = true;
   }
}

static void get_touch_coordinates(VPADTouchData *point, VPADStatus *vpad,
      VPADChan channel, struct video_viewport *viewport, bool *clamped)
{
   get_calibrated_point(point, viewport, vpad, channel);
   apply_clamping(point, viewport, clamped);
}

#if 0
/**
 * Get absolute value of a signed integer using bit manipulation.
 */
static int16_t bitwise_abs(int16_t value)
{
   bool is_negative = value & 0x8000;
   if (!is_negative)
      return value;

   value = value &~ 0x8000;
   return (~value & 0x7fff)+1;
}

/**
 * printf doesn't have a concept of a signed hex digit, so we fake it.
 */
static void log_coords(int16_t x, int16_t y)
{
   bool x_negative = x & 0x8000;
   bool y_negative = y & 0x8000;

   int16_t x_digit = bitwise_abs(x);
   int16_t y_digit = bitwise_abs(y);

   RARCH_LOG("[wpad]: calibrated point: %s%04x, %s%04x\n",
         x_negative ? "-" : "",
         x_digit,
         y_negative ? "-" : "",
         y_digit);
}
#endif

static void update_touch_state(int16_t state[3][2],
      uint64_t *buttons, VPADStatus *vpad, VPADChan channel)
{
   VPADTouchData point            = {0};
   struct video_viewport viewport = {0};
   bool touch_clamped             = false;

   if (!vpad->tpNormal.touched || vpad->tpNormal.validity != VPAD_VALID)
   {
      *buttons &= ~VPAD_BUTTON_TOUCH;
      return;
   }

   video_driver_get_viewport_info(&viewport);
   get_touch_coordinates(&point, vpad, channel, &viewport, &touch_clamped);

   state[WIIU_DEVICE_INDEX_TOUCHPAD][RETRO_DEVICE_ID_ANALOG_X] = scale_touchpad(
         viewport.x, viewport.x + viewport.width, -0x7fff, 0x7fff, point.x);
   state[WIIU_DEVICE_INDEX_TOUCHPAD][RETRO_DEVICE_ID_ANALOG_Y] = scale_touchpad(
         viewport.y, viewport.y + viewport.height, -0x7fff, 0x7fff, point.y);

#if 0
   log_coords(state[WIIU_DEVICE_INDEX_TOUCHPAD][RETRO_DEVICE_ID_ANALOG_X],
         state[WIIU_DEVICE_INDEX_TOUCHPAD][RETRO_DEVICE_ID_ANALOG_Y]);
#endif

   if (!touch_clamped)
      *buttons |= VPAD_BUTTON_TOUCH;
   else
      *buttons &= ~VPAD_BUTTON_TOUCH;
}

static void check_panic_button(uint32_t held_buttons)
{
   if ((held_buttons & PANIC_BUTTON_MASK) == PANIC_BUTTON_MASK)
      command_event(CMD_EVENT_QUIT, NULL);
}

static void wpad_poll(void)
{
   VPADStatus vpad;
   VPADReadError error;
   VPADChan channel;

   for (channel = VPAD_CHAN_0; channel < WIIU_GAMEPAD_CHANNELS; channel++)
   {
      VPADRead(channel, &vpad, 1, &error);

      /* Gamepad is connected! */
      if (error == VPAD_READ_SUCCESS || error == VPAD_READ_NO_SAMPLES)
         wpad_register(channel);
      else if (error == VPAD_READ_INVALID_CONTROLLER)
         wpad_deregister(channel);

      if (error == VPAD_READ_SUCCESS)
      {
         update_button_state(&joypad_state.wpad.pads[channel].button_state, vpad.hold);
         update_analog_state(joypad_state.wpad.pads[channel].analog_state, &vpad);
         update_touch_state(joypad_state.wpad.pads[channel].analog_state, &joypad_state.wpad.pads[channel].button_state, &vpad, channel);
         check_panic_button(vpad.hold);
      }
   }
}

static void *wpad_init(void *data)
{
   memset(&joypad_state.wpad, 0, sizeof(joypad_state.wpad));
   for(int i = 0; i < WIIU_GAMEPAD_CHANNELS; i++) {
      joypad_state.wpad.channel_slot_map[i] = WPAD_INVALID_CHANNEL;
   }
   wpad_poll();
   return (void*)-1;
}

static bool wpad_query_pad(unsigned port)
{
   return port < MAX_USERS && 
      (to_gamepad_channel(port) != WPAD_INVALID_CHANNEL);
}

static void wpad_destroy(void) { }

static int32_t wpad_button(unsigned port, uint16_t joykey)
{
   VPADChan channel;
   if (!wpad_query_pad(port))
      return 0;
   channel = to_gamepad_channel(port);
   if (channel < 0)
      return 0;
   return (joypad_state.wpad.pads[channel].button_state & (UINT64_C(1) << joykey));
}

static void wpad_get_buttons(unsigned port, input_bits_t *state)
{
   VPADChan channel;

   if (!wpad_query_pad(port))
   {
      BIT256_CLEAR_ALL_PTR(state);
      return;
   }

   channel = to_gamepad_channel(port);
   if (channel < 0)
   {
      BIT256_CLEAR_ALL_PTR(state);
      return;
   }

   BITS_COPY32_PTR(state, joypad_state.wpad.pads[channel].button_state);
}

static int16_t wpad_axis(unsigned port, uint32_t axis)
{
   axis_data data;
   VPADChan channel;

   if (!wpad_query_pad(port))
      return 0;

   channel = to_gamepad_channel(port);
   if (channel < 0)
      return 0;

   pad_functions.read_axis_data(axis, &data);
   return pad_functions.get_axis_value(data.axis,
         joypad_state.wpad.pads[channel].analog_state, data.is_negative);
}

static int16_t wpad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   unsigned i;
   int16_t ret                          = 0;
   uint16_t port_idx                    = joypad_info->joy_idx;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;
      if (
               (uint16_t)joykey != NO_BTN 
            && wpad_button(port_idx, (uint16_t)joykey))
         ret |= ( 1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(wpad_axis(port_idx, joyaxis)) 
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static const char *wpad_name(unsigned port)
{
   return PAD_NAME_WIIU_GAMEPAD;
}

input_device_driver_t wpad_driver =
{
  wpad_init,
  wpad_query_pad,
  wpad_destroy,
  wpad_button,
  wpad_state,
  wpad_get_buttons,
  wpad_axis,
  wpad_poll,
  NULL,
  NULL,
  wpad_name,
  "gamepad",
};
