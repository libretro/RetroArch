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

#include "../../include/wiiu/input.h"

#define WIIU_PRO_BUTTON_MASK 0x3FC0000;
#define CLASSIC_BUTTON_MASK  0xFF0000;

/* Forward declarations */
static void kpad_poll(void);
static void kpad_deregister(unsigned channel);

typedef struct _wiimote_state wiimote_state;

struct _wiimote_state
{
   uint64_t button_state;
   int16_t  analog_state[3][2];
   uint8_t  type;
};

/* static global variables */
static bool kpad_ready                          = false;
static int channel_slot_map[]                   = { -1, -1, -1, -1 };
static int poll_failures[WIIU_WIIMOTE_CHANNELS] = { 0, 0, 0, 0 };
static wiimote_state 
wiimotes[WIIU_WIIMOTE_CHANNELS]                 = {
  { 0, {{0,0},{0,0},{0,0}}, WIIMOTE_TYPE_NONE },
  { 0, {{0,0},{0,0},{0,0}}, WIIMOTE_TYPE_NONE },
  { 0, {{0,0},{0,0},{0,0}}, WIIMOTE_TYPE_NONE },
  { 0, {{0,0},{0,0},{0,0}}, WIIMOTE_TYPE_NONE },
};


static int to_wiimote_channel(unsigned pad)
{
   unsigned i;

   for (i = 0; i < WIIU_WIIMOTE_CHANNELS; i++)
      if (channel_slot_map[i] == pad)
         return i;

   return -1;
}

static int get_slot_for_channel(unsigned channel)
{
   int slot = pad_connection_find_vacant_pad(hid_instance.pad_list);
   if (slot >= 0)
   {
      channel_slot_map[channel]             = slot;
      hid_instance.pad_list[slot].connected = true;
   }

   return slot;
}

static void *kpad_init(void *data)
{
   kpad_poll();
   kpad_ready = true;

   return (void*)-1;
}

static bool kpad_query_pad(unsigned pad)
{
   return kpad_ready && pad < MAX_USERS;
}

static void kpad_destroy(void)
{
   kpad_ready = false;
}

static int32_t kpad_button(unsigned port, uint16_t joykey)
{
   int channel;
   if (!kpad_query_pad(port))
      return 0;

   channel = to_wiimote_channel(port);
   if (channel < 0)
      return 0;
   return (wiimotes[channel].button_state
         & (UINT64_C(1) << joykey));
}

static void kpad_get_buttons(unsigned port, input_bits_t *state)
{
   int channel = to_wiimote_channel(port);

   if (!kpad_query_pad(port) || channel < 0)
      BIT256_CLEAR_ALL_PTR(state);
   else
      BITS_COPY16_PTR(state, wiimotes[channel].button_state);
}

static int16_t kpad_axis(unsigned port, uint32_t axis)
{
   axis_data data;
   int channel = to_wiimote_channel(port);

   if (!kpad_query_pad(port) || channel < 0)
      return 0;

   pad_functions.read_axis_data(axis, &data);
   return pad_functions.get_axis_value(data.axis,
         wiimotes[channel].analog_state,
         data.is_negative);
}

static int16_t kpad_state(
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
            && kpad_button(port_idx, (uint16_t)joykey))
         ret |= ( 1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(kpad_axis(port_idx, joyaxis)) 
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static void kpad_register(unsigned channel, uint8_t device_type)
{
   if (wiimotes[channel].type != device_type)
   {
      int slot;

      kpad_deregister(channel);
      slot = get_slot_for_channel(channel);

      if (slot < 0)
      {
         RARCH_ERR("Couldn't get a slot for this remote.\n");
         return;
      }

      wiimotes[channel].type = device_type;
      input_pad_connect(slot, &kpad_driver);
   }
}

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

static void kpad_deregister(unsigned channel)
{
   int slot = channel_slot_map[channel];

   if (slot >= 0)
   {
      input_autoconfigure_disconnect(slot, kpad_driver.name(slot));
      wiimotes[channel].type = WIIMOTE_TYPE_NONE;
      hid_instance.pad_list[slot].connected = false;
      channel_slot_map[channel] = -1;
   }
}

static void kpad_poll(void)
{
   unsigned channel;
   KPADData kpad;
   int32_t result = 0;

   for (channel = 0; channel < WIIU_WIIMOTE_CHANNELS; channel++)
   {
      memset(&kpad, 0, sizeof(kpad));

      result = KPADRead(channel, &kpad, 1);
      /* this is a hack to prevent spurious disconnects */
      /* TODO: use KPADSetConnectCallback and use callbacks to detect */
      /*       pad disconnects properly. */
      if (result == 0)
      {
         poll_failures[channel]++;
         if (poll_failures[channel] > 5)
            kpad_deregister(channel);
         continue;
      }
      poll_failures[channel] = 0;

      /* Several reads when a device is connected or an attachment added give */
      /* bogus results, try to weed them out */
      if (kpad.wpad_error || kpad.device_type == 255) {
         continue;
      }

      kpad_poll_one_channel(channel, &kpad);
   }
}

static const char *kpad_name(unsigned pad)
{
   int channel = to_wiimote_channel(pad);
   if (channel < 0)
      return "unknown";

   switch(wiimotes[channel].type)
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
#ifdef DEBUG
         RARCH_LOG("[kpad]: Unknown pad type %d\n", wiimotes[pad].type);
#endif
         break;
   }

   return "N/A";
}

input_device_driver_t kpad_driver =
{
   kpad_init,
   kpad_query_pad,
   kpad_destroy,
   kpad_button,
   kpad_state,
   kpad_get_buttons,
   kpad_axis,
   kpad_poll,
   NULL,
   NULL,
   kpad_name,
   "wiimote",
};
