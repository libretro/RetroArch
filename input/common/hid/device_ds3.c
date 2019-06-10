/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "hid_device_driver.h"

#define DS3_ACTIVATION_REPORT_ID 0xf4
#define DS3_RUMBLE_REPORT_ID     0x01

typedef struct ds3_instance {
   void *handle;
   joypad_connection_t *pad;
   int slot;
   bool led_set;
   uint32_t buttons;
   int16_t analog_state[3][2];
   uint16_t motors[2];
   uint8_t data[64];
} ds3_instance_t;

static uint8_t ds3_activation_packet[] = {
#if defined(IOS)
  0x53, 0xF4,
#elif defined(HAVE_WIIUSB_HID)
  0x02,
#endif
  0x42, 0x0c, 0x00, 0x00
};

#if defined(WIIU)
#define PACKET_OFFSET 2
#elif defined(HAVE_WIIUSB_HID)
#define PACKET_OFFSET 1
#else
#define PACKET_OFFSET 0
#endif

#define LED_OFFSET 11
#define MOTOR1_OFFSET 4
#define MOTOR2_OFFSET 6

static uint8_t ds3_control_packet[] = {
   0x52, 0x01,
   0x00, 0xff, 0x00, 0xff, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00,
   0xff, 0x27, 0x10, 0x00, 0x32,
   0xff, 0x27, 0x10, 0x00, 0x32,
   0xff, 0x27, 0x10, 0x00, 0x32,
   0xff, 0x27, 0x10, 0x00, 0x32,
   0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00
};

extern pad_connection_interface_t ds3_pad_connection;

static void ds3_update_pad_state(ds3_instance_t *instance);
static void ds3_update_analog_state(ds3_instance_t *instance);

static int32_t ds3_send_activation_packet(ds3_instance_t *instance)
{
   int32_t result;
#if defined(WIIU)
   result = HID_SET_REPORT(instance->handle,
                  HID_REPORT_FEATURE,
                  DS3_ACTIVATION_REPORT_ID,
                  ds3_activation_packet,
                  sizeof(ds3_activation_packet));
#else
   HID_SEND_CONTROL(instance->handle,
                    ds3_activation_packet, sizeof(ds3_activation_packet));
#endif

   return result;
}

static uint32_t set_protocol(ds3_instance_t *instance, int protocol)
{
   uint32_t result = 0;
#if defined(WIIU)
   result = HID_SET_PROTOCOL(instance->handle, 1);
#endif

   return result;
}

static int32_t ds3_send_control_packet(ds3_instance_t *instance)
{
   uint8_t packet_buffer[sizeof(ds3_control_packet)];
   int32_t result = 0;
   memcpy(packet_buffer, ds3_control_packet, sizeof(ds3_control_packet));

   packet_buffer[LED_OFFSET] = 0;
   if(instance->pad) {
      packet_buffer[LED_OFFSET] = 1 << ((instance->slot % 4) + 1);
   }
   packet_buffer[MOTOR1_OFFSET] = instance->motors[1] >> 8;
   packet_buffer[MOTOR2_OFFSET] = instance->motors[0] >> 8;

#if defined(HAVE_WIIUSB_HID)
   packet_buffer[1] = 0x03;
#endif

#if defined(WIIU)
   result = HID_SET_REPORT(instance->handle,
                  HID_REPORT_OUTPUT,
                  DS3_RUMBLE_REPORT_ID,
                  packet_buffer+PACKET_OFFSET,
                  sizeof(ds3_control_packet)-PACKET_OFFSET);
#else
   HID_SEND_CONTROL(instance->handle,
                    packet_buffer+PACKET_OFFSET,
                    sizeof(ds3_control_packet)-PACKET_OFFSET);
#endif /* WIIU */
   return result;
}

static void *ds3_init(void *handle)
{
   ds3_instance_t *instance;
   int errors = 0;
   RARCH_LOG("[ds3]: init\n");
   instance = (ds3_instance_t *)calloc(1, sizeof(ds3_instance_t));
   if(!instance)
     goto error;

   memset(instance, 0, sizeof(ds3_instance_t));
   instance->handle = handle;

   RARCH_LOG("[ds3]: setting protocol\n");

   /* this might fail, but we don't care. */
   set_protocol(instance, 1);

   RARCH_LOG("[ds3]: sending control packet\n");
   if(ds3_send_control_packet(instance) < 0)
      errors++;

   RARCH_LOG("[ds3]: sending activation packet\n");
   if(ds3_send_activation_packet(instance) < 0)
      errors++;

   if(errors)
      goto error;

   instance->pad = hid_pad_register(instance, &ds3_pad_connection);
   if(!instance->pad)
      goto error;

   RARCH_LOG("[ds3]: init complete.\n");
   return instance;

   error:
      RARCH_ERR("[ds3]: init failed.\n");
      if(instance)
         free(instance);
      return NULL;
}

static void ds3_free(void *data)
{
   ds3_instance_t *instance = (ds3_instance_t *)data;

   if(instance) {
      hid_pad_deregister(instance->pad);
      free(instance);
   }
}

static void ds3_handle_packet(void *data, uint8_t *packet, size_t size)
{
   ds3_instance_t *instance = (ds3_instance_t *)data;

   if(!instance || !instance->pad)
      return;

   instance->pad->iface->packet_handler(data, packet, size);
}

static bool ds3_detect(uint16_t vendor_id, uint16_t product_id)
{
   return vendor_id == VID_SONY && product_id == PID_SONY_DS3;
}

hid_device_t ds3_hid_device = {
   ds3_init,
   ds3_free,
   ds3_handle_packet,
   ds3_detect,
   "Sony DualShock 3"
};

/**
 * pad interface implementation
 */

static void *ds3_pad_init(void *data, uint32_t slot, hid_driver_t *driver)
{
   ds3_instance_t *pad = (ds3_instance_t *)data;
   pad->slot = slot;

   return data;
}

static void ds3_pad_deinit(void *data)
{
   ds3_instance_t *pad = (ds3_instance_t *)data;
   if(pad) {
      input_autoconfigure_disconnect(pad->slot, ds3_pad_connection.get_name(pad));
   }
}

static void ds3_get_buttons(void *data, input_bits_t *state)
{
   ds3_instance_t *pad = (ds3_instance_t *)data;

   if(pad)
   {
      BITS_COPY16_PTR(state, pad->buttons);

      if(pad->buttons & 0x10000)
         BIT256_SET_PTR(state, RARCH_MENU_TOGGLE);
   } else {
      BIT256_CLEAR_ALL_PTR(state);
   }
}

static void ds3_packet_handler(void *data, uint8_t *packet, uint16_t size)
{
   ds3_instance_t *instance = (ds3_instance_t *)data;

   if(instance->pad && !instance->led_set)
   {
      ds3_send_control_packet(instance);
      instance->led_set = true;
   }

   if(size > sizeof(ds3_control_packet))
   {
      RARCH_ERR("[ds3]: Expecting packet to be %d but was %d\n",
         sizeof(ds3_control_packet), size);
      return;
   }

   memcpy(instance->data, packet, size);
   ds3_update_pad_state(instance);
   ds3_update_analog_state(instance);
}

static void ds3_update_analog_state(ds3_instance_t *instance)
{
   int pad_axis;
   int16_t interpolated;
   unsigned stick, axis;

   for(pad_axis = 0; pad_axis < 4; pad_axis++)
   {
      axis = pad_axis % 2 ? 0 : 1;
      stick = pad_axis / 2;
      interpolated = instance->data[6+pad_axis];
      instance->analog_state[stick][axis] = (interpolated - 128) * 256;
   }
}

static void ds3_update_pad_state(ds3_instance_t *instance)
{
   uint32_t i, pressed_keys;

   static const uint32_t button_mapping[17] =
   {
      RETRO_DEVICE_ID_JOYPAD_SELECT,
      RETRO_DEVICE_ID_JOYPAD_L3,
      RETRO_DEVICE_ID_JOYPAD_R3,
      RETRO_DEVICE_ID_JOYPAD_START,
      RETRO_DEVICE_ID_JOYPAD_UP,
      RETRO_DEVICE_ID_JOYPAD_RIGHT,
      RETRO_DEVICE_ID_JOYPAD_DOWN,
      RETRO_DEVICE_ID_JOYPAD_LEFT,
      RETRO_DEVICE_ID_JOYPAD_L2,
      RETRO_DEVICE_ID_JOYPAD_R2,
      RETRO_DEVICE_ID_JOYPAD_L,
      RETRO_DEVICE_ID_JOYPAD_R,
      RETRO_DEVICE_ID_JOYPAD_X,
      RETRO_DEVICE_ID_JOYPAD_A,
      RETRO_DEVICE_ID_JOYPAD_B,
      RETRO_DEVICE_ID_JOYPAD_Y,
      16 /* PS button */
   };

   instance->buttons = 0;

   pressed_keys = instance->data[2]|(instance->data[3] << 8)|((instance->data[4] & 0x01) << 16);

   for(i = 0; i < 17; i++)
     instance->buttons |= (pressed_keys & (1 << i)) ?
        (1 << button_mapping[i]) : 0;
}

static void ds3_set_rumble(void *data, enum retro_rumble_effect effect, uint16_t strength)
{
   ds3_instance_t *pad = (ds3_instance_t *)data;
}

static int16_t ds3_get_axis(void *data, unsigned axis)
{
   axis_data axis_data;
   ds3_instance_t *pad = (ds3_instance_t *)data;

   gamepad_read_axis_data(axis, &axis_data);

   if(!pad || axis_data.axis >= 4)
      return 0;

   return gamepad_get_axis_value(pad->analog_state, &axis_data);
}

static const char *ds3_get_name(void *data)
{
   ds3_instance_t *pad = (ds3_instance_t *)data;
   return "Sony DualShock 3";
}

static bool ds3_button(void *data, uint16_t joykey)
{
   ds3_instance_t *pad = (ds3_instance_t *)data;
   if(!pad || joykey > 31)
      return false;

   return pad->buttons & (1 << joykey);
}

pad_connection_interface_t ds3_pad_connection = {
   ds3_pad_init,
   ds3_pad_deinit,
   ds3_packet_handler,
   ds3_set_rumble,
   ds3_get_buttons,
   ds3_get_axis,
   ds3_get_name,
   ds3_button
};
