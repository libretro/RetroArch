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

static VPADChan wpad_to_gamepad_channel(unsigned pad)
{
   int i;
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
   joypad_state.pads[slot].connected           = false;
   pad_connection_release_slot(&joypad_state.pads[slot]);
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

   if ((slot = pad_connection_find_vacant_pad(joypad_state.pads)) < 0)
      return;

   joypad_state.pads[slot].connected = true;
   joypad_state.pads[slot].input_driver = &wpad_driver;
   input_pad_connect(slot, &wpad_driver);
   joypad_state.wpad.channel_slot_map[channel] = slot;
}

static void wpad_update_button_state(input_pad_view_t *v, uint32_t held_buttons)
{
   v->buttons.data[0] = held_buttons & VPAD_MASK_BUTTONS;
}

static void wpad_update_analog_state(input_pad_view_t *v, VPADStatus *vpad)
{
   v->axes[0] = WIIU_READ_STICK(vpad->leftStick.x);
   v->axes[1] = WIIU_READ_STICK(vpad->leftStick.y);
   v->axes[2] = WIIU_READ_STICK(vpad->rightStick.x);
   v->axes[3] = WIIU_READ_STICK(vpad->rightStick.y);
}

static int16_t wpad_scale_touchpad(int16_t from_min, int16_t from_max,
      int16_t to_min,   int16_t to_max, int16_t value )
{
   int32_t from_range = from_max - from_min;
   int32_t to_range   = to_max - to_min;

   return (((value - from_min) * to_range) / from_range) + to_min;
}

static void wpad_get_calibrated_point(VPADTouchData *point,
      struct video_viewport *viewport, VPADStatus *vpad, unsigned channel)
{
   VPADTouchData calibrated720p = {0};

   VPADGetTPCalibratedPoint(channel, &calibrated720p, &(vpad->tpNormal));
   point->x = wpad_scale_touchpad(12, 1268, 0, VIDEO_SCALE_W(viewport->full_dims), calibrated720p.x);
   point->y = wpad_scale_touchpad(12, 708, 0, VIDEO_SCALE_H(viewport->full_dims), calibrated720p.y);
}

static void wpad_apply_clamping(VPADTouchData *point, struct video_viewport *viewport, bool *clamped)
{
   /* clamp the x domain to the viewport */
   if (point->x < VIDEO_POS_X(viewport->pos))
   {
      point->x = VIDEO_POS_X(viewport->pos);
      *clamped = true;
   }
   else if (point->x > (VIDEO_POS_X(viewport->pos) + VIDEO_SCALE_W(viewport->dims)))
   {
      point->x = VIDEO_POS_X(viewport->pos) + VIDEO_SCALE_W(viewport->dims);
      *clamped = true;
   }

   /* clamp the y domain to the viewport */
   if (point->y < VIDEO_POS_Y(viewport->pos))
   {
      point->y = VIDEO_POS_Y(viewport->pos);
      *clamped = true;
   }
   else if (point->y > (VIDEO_POS_Y(viewport->pos) + VIDEO_SCALE_H(viewport->dims)))
   {
      point->y =  VIDEO_POS_Y(viewport->pos) + VIDEO_SCALE_H(viewport->dims);
      *clamped = true;
   }
}

static void wpad_get_touch_coordinates(VPADTouchData *point, VPADStatus *vpad,
      VPADChan channel, struct video_viewport *viewport, bool *clamped)
{
   wpad_get_calibrated_point(point, viewport, vpad, channel);
   wpad_apply_clamping(point, viewport, clamped);
}

static void wpad_update_touch_state(input_pad_view_t *v,
      VPADStatus *vpad, VPADChan channel)
{
   VPADTouchData point            = {0};
   struct video_viewport viewport = {0};
   bool touch_clamped             = false;

   if (!vpad->tpNormal.touched || vpad->tpNormal.validity != VPAD_VALID)
   {
      v->buttons.data[0] &= ~(uint32_t)VPAD_BUTTON_TOUCH;
      return;
   }

   video_driver_get_viewport_info(&viewport);
   wpad_get_touch_coordinates(&point, vpad, channel, &viewport, &touch_clamped);

   v->axes[WIIU_AXIS_TOUCH_X] = wpad_scale_touchpad(
         VIDEO_POS_X(viewport.pos), VIDEO_POS_X(viewport.pos) + VIDEO_SCALE_W(viewport.dims), -0x7fff, 0x7fff, point.x);
   v->axes[WIIU_AXIS_TOUCH_Y] = wpad_scale_touchpad(
         VIDEO_POS_Y(viewport.pos), VIDEO_POS_Y(viewport.pos) + VIDEO_SCALE_H(viewport.dims), -0x7fff, 0x7fff, point.y);

   if (!touch_clamped)
      v->buttons.data[0] |= (uint32_t)VPAD_BUTTON_TOUCH;
   else
      v->buttons.data[0] &= ~(uint32_t)VPAD_BUTTON_TOUCH;
}

static void wpad_check_panic_button(uint32_t held_buttons)
{
   if ((held_buttons & PANIC_BUTTON_MASK) == PANIC_BUTTON_MASK)
      input_driver_platform_request(INPUT_PLATFORM_QUIT);
}

static void wpad_poll(void)
{
   VPADStatus vpad;
   VPADChan channel;
   VPADReadError err;

   for (channel = VPAD_CHAN_0; channel < WIIU_GAMEPAD_CHANNELS; channel++)
   {
      VPADRead(channel, &vpad, 1, &err);

      /* Gamepad is connected! */
      if (err == VPAD_READ_SUCCESS || err == VPAD_READ_NO_SAMPLES)
         wpad_register(channel);
      else if (err == VPAD_READ_INVALID_CONTROLLER)
         wpad_deregister(channel);

      if (err == VPAD_READ_SUCCESS)
      {
         input_pad_view_t *v = &joypad_state.wpad.pads[channel].view;
         wpad_update_button_state(v, vpad.hold);
         wpad_update_analog_state(v, &vpad);
         wpad_update_touch_state(v, &vpad, channel);
         wpad_check_panic_button(vpad.hold);
      }
   }
}

static void *wpad_init(void *data)
{
   int i;
   memset(&joypad_state.wpad, 0, sizeof(joypad_state.wpad));
   for (i = 0; i < WIIU_GAMEPAD_CHANNELS; i++)
      joypad_state.wpad.channel_slot_map[i] = WPAD_INVALID_CHANNEL;
   wpad_poll();
   return (void*)-1;
}

static bool wpad_query_pad(unsigned port)
{
   return port < MAX_USERS &&
      (wpad_to_gamepad_channel(port) != WPAD_INVALID_CHANNEL);
}

static void wpad_destroy(void) { }

/* The view of the GamePad on @port; NULL for none */
static const input_pad_view_t *wpad_view(unsigned port)
{
   VPADChan channel;
   if (!wpad_query_pad(port))
      return NULL;
   channel = wpad_to_gamepad_channel(port);
   return (channel >= 0) ? &joypad_state.wpad.pads[channel].view : NULL;
}

static int32_t wpad_button(unsigned port, uint16_t joykey)
{
   const input_pad_view_t *v = wpad_view(port);
   return v ? input_pad_view_button(v, joykey) : 0;
}

static void wpad_get_buttons(unsigned port, input_bits_t *state)
{
   const input_pad_view_t *v = wpad_view(port);
   if (v)
      *state = v->buttons;
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t wpad_axis(unsigned port, uint32_t axis)
{
   const input_pad_view_t *v = wpad_view(port);
   return v ? input_pad_view_axis(v, axis) : 0;
}

static int16_t wpad_state(
      rarch_joypad_info_t *joypad_info,
      const uint16_t *joykeys,
      const uint32_t *joyaxes,
      unsigned port)
{
   const input_pad_view_t *v = wpad_view(joypad_info->joy_idx);
   return v ? input_pad_view_state(v, joypad_info, joykeys, joyaxes) : 0;
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
  NULL, /* set_rumble */
  NULL, /* set_rumble_gain */
  NULL, /* set_sensor_state */
  NULL, /* get_sensor_input */
  wpad_name,
  "gamepad",
  NULL, /* get_bt_host */
  NULL, /* set_bt_host */
  NULL, /* survives_video */
  NULL, /* set_player */
  wpad_view
};
