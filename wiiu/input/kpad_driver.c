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
 * This sub-driver handles the wiimotes. The Wii U has 4 channels available.
 * This also handles wiimote attachments such as the nunchuk and classic/pro
 * controllers.
 */

#include <wiiu/pad_driver.h>

static bool kpad_init(void *data);
static bool kpad_query_pad(unsigned pad);
static void kpad_destroy(void);
static bool kpad_button(unsigned pad, uint16_t button);
static void kpad_get_buttons(unsigned pad, retro_bits_t *state);
static int16_t kpad_axis(unsigned pad, uint32_t axis);
static void kpad_poll(void);
static const char *kpad_name(unsigned pad);

typedef struct _wiimote_state wiimote_state;

struct _wiimote_state
{
   uint64_t button_state;
   int16_t  analog_state[3][2];
   uint8_t  type;
};

static bool ready = false;

wiimote_state wiimotes[WIIU_WIIMOTE_CHANNELS];

static unsigned to_wiimote_channel(unsigned pad)
{
   if (pad == PAD_GAMEPAD || pad > WIIU_WIIMOTE_CHANNELS)
      return 0xffffffff;

   return pad-1;
}

static unsigned to_retro_pad(unsigned channel)
{
   return channel+1;
}

static bool kpad_init(void *data)
{
   (void *)data;

   kpad_poll();
   ready = true;
}

static bool kpad_query_pad(unsigned pad)
{
   return ready && pad <= WIIU_WIIMOTE_CHANNELS && pad > PAD_GAMEPAD;
}

static void kpad_destroy(void)
{
   ready = false;
}

static bool kpad_button(unsigned pad, uint16_t button_bit)
{
   if (!kpad_query_pad(pad))
      return false;

   return wiimotes[to_wiimote_channel(pad)].button_state 
      & (UINT64_C(1) << button_bit);
}

static void kpad_get_buttons(unsigned pad, retro_bits_t *state)
{
   if (!kpad_query_pad(pad))
      BIT256_CLEAR_ALL_PTR(state);
   else
      BITS_COPY16_PTR(state, wiimotes[to_wiimote_channel(pad)].button_state);
}

static int16_t kpad_axis(unsigned pad, uint32_t axis)
{
   axis_data data;
   if (!kpad_query_pad(pad) || axis == AXIS_NONE)
      return 0;

   pad_functions.read_axis_data(axis, &data);
   return pad_functions.get_axis_value(data.axis,
         wiimotes[to_wiimote_channel(pad)].analog_state,
         data.is_negative);
}

static void kpad_register(unsigned channel, uint8_t device_type)
{
   if (wiimotes[channel].type != device_type)
   {
      wiimotes[channel].type = device_type;
      pad_functions.connect(to_retro_pad(channel), &kpad_driver);
   }
}

#define WIIU_PRO_BUTTON_MASK 0x3FC0000;
#define CLASSIC_BUTTON_MASK  0xFF0000;

static void kpad_poll_one_channel(unsigned channel, KPADData *kpad)
{
   kpad_register(channel, kpad->device_type);
   switch(kpad->device_type)
   {
      case WIIMOTE_TYPE_PRO:
         wiimotes[channel].button_state = kpad->classic.btns_h 
            & ~WIIU_PRO_BUTTON_MASK;
         pad_functions.set_axis_value(wiimotes[channel].analog_state,
               WIIU_READ_STICK(kpad->classic.lstick_x),
               WIIU_READ_STICK(kpad->classic.lstick_y),
               WIIU_READ_STICK(kpad->classic.rstick_x),
               WIIU_READ_STICK(kpad->classic.rstick_y), 0, 0);
         break;
      case WIIMOTE_TYPE_CLASSIC:
         wiimotes[channel].button_state = kpad->classic.btns_h 
            & ~CLASSIC_BUTTON_MASK;
         pad_functions.set_axis_value(wiimotes[channel].analog_state,
               WIIU_READ_STICK(kpad->classic.lstick_x),
               WIIU_READ_STICK(kpad->classic.lstick_y),
               WIIU_READ_STICK(kpad->classic.rstick_x),
               WIIU_READ_STICK(kpad->classic.rstick_y), 0, 0);
         break;
      case WIIMOTE_TYPE_NUNCHUK:
         wiimotes[channel].button_state = kpad->btns_h;
         pad_functions.set_axis_value(wiimotes[channel].analog_state,
               WIIU_READ_STICK(kpad->nunchuck.stick_x),
               WIIU_READ_STICK(kpad->nunchuck.stick_y), 0, 0, 0, 0);
         break;
      case WIIMOTE_TYPE_WIIPLUS:
         wiimotes[channel].button_state = kpad->btns_h;
         pad_functions.set_axis_value(wiimotes[channel].analog_state,
               0, 0, 0, 0, 0, 0);
         break;
   }
}

static void kpad_poll(void)
{
   unsigned channel;

   for (channel = 0; channel < WIIU_WIIMOTE_CHANNELS; channel++)
   {
      KPADData kpad;

      if (!KPADRead(channel, &kpad, 1))
         continue;

      kpad_poll_one_channel(channel, &kpad);
   }
}

static const char *kpad_name(unsigned pad)
{
   pad = to_wiimote_channel(pad);
   if (pad >= WIIU_WIIMOTE_CHANNELS)
      return "unknown";

   switch(wiimotes[pad].type)
   {
      case WIIMOTE_TYPE_PRO:
         return PAD_NAME_WIIU_PRO;
      case WIIMOTE_TYPE_CLASSIC:
         return PAD_NAME_CLASSIC;
      case WIIMOTE_TYPE_NUNCHUK:
         return PAD_NAME_NUNCHUK;
      case WIIMOTE_TYPE_WIIPLUS:
         return PAD_NAME_WIIMOTE;
      case WIIMOTE_TYPE_NONE:
      default:
         return "N/A";
   }
}

input_device_driver_t kpad_driver =
{
   kpad_init,
   kpad_query_pad,
   kpad_destroy,
   kpad_button,
   kpad_get_buttons,
   kpad_axis,
   kpad_poll,
   NULL,
   kpad_name,
   "wiimote",
};
