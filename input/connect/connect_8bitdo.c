/*  RetroArch - A frontend for libretro.
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

/* 8BitDo's pads in their D-input mode, on USB and Bluetooth, as SDL's
 * 8BitDo driver reads them: the SN30 Pro and SF30 Pro (B + Start), the
 * Pro 2 and Pro 3, and the Ultimate 2 Wireless, 2C Wireless and 3.
 *
 * Their buttons keep the numbers their report gives them - A 0, B 1,
 * X 3, Y 4, L 6, R 7, L2 8, R2 9, Select 10, Start 11, Home 12, L3 13,
 * R3 14, and on the newer pads the back paddles 2 and 5, Share 15 and
 * L4 and R4 16 and 17 - and the D-pad is hat 0, as the hid profiles
 * for the SN30 Pro and SF30 Pro have them; Home also opens the menu.
 * The sticks are axes 0, 1, 4 and 5, as those profiles have them too,
 * and L2 and R2's travel axes 2 and 3.
 *
 * Firmware from 2.05 on the SN30 Pro and SF30 Pro, and the Pro 2 and 3,
 * have a mode their feature report 0x06 starts; the Ultimate 3 says in
 * its feature report 0x30 what it has; the Ultimate 2 Wireless has it
 * where its report is 34 bytes. That mode has rumble - output report
 * 0x05, the large motor then the small - and the motion sensors from
 * byte 15. The SN30 Pro's older firmware sends a 9-byte report with no
 * report ID, and neither. */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <boolean.h>

#include "joypad_connection.h"
#include "../input_defines.h"
#include "../common/pad_motion.h"

#define SN30_OLD_REPORT_LEN 9   /* firmware before 2.05: no report ID */
#define SN30_REPORT_LEN     10  /* report 0x01, 0x03 or 0x04 */
#define ULTIMATE2_FULL_LEN  34  /* the Ultimate 2 Wireless with rumble */
#define MOTION_AT           15  /* accelerometer, then gyroscope */

enum hidpad_8bitdo_model
{
   EIGHTBITDO_SN30_PRO = 0,
   EIGHTBITDO_SF30_PRO,
   EIGHTBITDO_PRO_2,
   EIGHTBITDO_PRO_3,
   EIGHTBITDO_ULTIMATE_2,
   EIGHTBITDO_ULTIMATE_2C,
   EIGHTBITDO_ULTIMATE_3
};

/* the names the hid profiles go by, and SDL's for the rest */
static const char *const hidpad_8bitdo_names[] = {
   "8Bitdo SN30 Pro", "8Bitdo SF30 Pro", "8BitDo Pro 2", "8BitDo Pro 3",
   "8BitDo Ultimate 2 Wireless", "8BitDo Ultimate 2C Wireless",
   "8BitDo Ultimate 3"
};

struct hidpad_8bitdo_data
{
   struct pad_connection *connection;
   hid_driver_t *driver;
   input_pad_view_t view;
   int16_t imu[6];      /* accelerometer X Y Z, gyroscope X Y Z */
   uint8_t motors[2];   /* strong, weak: what the core asked last */
   uint8_t model;
   uint8_t motion_want; /* pad_motion.h's bits */
   bool rumble;         /* the mode with rumble and motion is on */
   bool behind_stack;   /* a Bluetooth stack's header: nothing is sent */
   bool motors_due;
   bool imu_seen;
};

static void *hidpad_8bitdo_start(void *data, hid_driver_t *driver,
      unsigned model)
{
   struct pad_connection *connection = (struct pad_connection*)data;
   struct hidpad_8bitdo_data *device;
   uint8_t feature[64];

   if (!connection)
      return NULL;
   if (!(device = (struct hidpad_8bitdo_data*)
            calloc(1, sizeof(struct hidpad_8bitdo_data))))
      return NULL;

   device->connection = connection;
   device->driver     = driver;
   device->model      = (uint8_t)model;
   memset(feature, 0, sizeof(feature));
   switch (model)
   {
      case EIGHTBITDO_ULTIMATE_2:   /* by its report's length */
      case EIGHTBITDO_ULTIMATE_2C:  /* none */
         break;
      case EIGHTBITDO_ULTIMATE_3:
         /* rumble unless byte 3 says none; the sensors are read where
          * the report has them */
         device->rumble = !(driver && driver->get_report
               && driver->get_report(connection, HID_REPORT_FEATURE,
                  0x30, feature, sizeof(feature)) > 4 && !feature[3]);
         break;
      default:
         device->rumble = driver && driver->get_report
            && driver->get_report(connection, HID_REPORT_FEATURE,
                  0x06, feature, sizeof(feature)) > 0;
         break;
   }
   return device;
}

#define HIDPAD_8BITDO_INIT(fn, model) \
static void *fn(void *data, uint32_t slot, hid_driver_t *driver) \
{ \
   return hidpad_8bitdo_start(data, driver, model); \
}
HIDPAD_8BITDO_INIT(hidpad_8bitdo_sn30_init,   EIGHTBITDO_SN30_PRO)
HIDPAD_8BITDO_INIT(hidpad_8bitdo_sf30_init,   EIGHTBITDO_SF30_PRO)
HIDPAD_8BITDO_INIT(hidpad_8bitdo_pro2_init,   EIGHTBITDO_PRO_2)
HIDPAD_8BITDO_INIT(hidpad_8bitdo_pro3_init,   EIGHTBITDO_PRO_3)
HIDPAD_8BITDO_INIT(hidpad_8bitdo_ult2_init,   EIGHTBITDO_ULTIMATE_2)
HIDPAD_8BITDO_INIT(hidpad_8bitdo_ult2c_init,  EIGHTBITDO_ULTIMATE_2C)
HIDPAD_8BITDO_INIT(hidpad_8bitdo_ult3_init,   EIGHTBITDO_ULTIMATE_3)

static void hidpad_8bitdo_deinit(void *data)
{
   struct hidpad_8bitdo_data *device = (struct hidpad_8bitdo_data*)data;
   if (device)
      free(device);
}

/* The hat's eight ways from up, clockwise: up 1, down 2, left 4,
 * right 8. Anything else is its rest. */
static const uint8_t hidpad_8bitdo_hat[8] = {
   1, 1 | 8, 8, 8 | 2, 2, 2 | 4, 4, 4 | 1
};

/* 0x7f is the rest; 0 and 0xff the ends */
static int16_t hidpad_8bitdo_stick(uint8_t b)
{
   int v = (int)b - 0x7f;
   return (int16_t)(v < 0 ? v * 32768 / 127 : v * 32767 / 128);
}

static int16_t hidpad_8bitdo_trigger(uint8_t t)
{
   return (int16_t)((t << 7) | (t >> 1));
}

static void hidpad_8bitdo_send(struct hidpad_8bitdo_data *device)
{
   uint8_t r[5];
   r[0] = 0x05;
   r[1] = device->motors[0];
   r[2] = device->motors[1];
   r[3] = 0;
   r[4] = 0;
   if (device->driver && device->driver->send_control)
      device->driver->send_control(device->connection, r, sizeof(r));
   device->motors_due = false;
}

static void hidpad_8bitdo_packet_handler(void *data,
      uint8_t *packet, uint16_t size)
{
   struct hidpad_8bitdo_data *device = (struct hidpad_8bitdo_data*)data;
   const uint8_t *r                  = packet;
   unsigned len                      = size;
   const uint8_t *buttons;
   uint8_t hat;

   if (!device || !r)
      return;

   /* a Bluetooth stack's DATA | INPUT header; never the old report,
    * whose first byte is buttons */
   if (     len > SN30_REPORT_LEN && r[0] == 0xa1
         && (r[1] == 0x01 || r[1] == 0x03 || r[1] == 0x04))
   {
      r++;
      len--;
      device->behind_stack = true;
   }

   if (len == SN30_OLD_REPORT_LEN)
   {
      buttons              = r;
      hat                  = r[2];
      device->view.axes[0] = hidpad_8bitdo_stick(r[3]);
      device->view.axes[1] = hidpad_8bitdo_stick(r[4]);
      device->view.axes[4] = hidpad_8bitdo_stick(r[5]);
      device->view.axes[5] = hidpad_8bitdo_stick(r[6]);
      device->view.axes[2] = (r[1] & 0x01) ? 0x7fff : 0;
      device->view.axes[3] = (r[1] & 0x02) ? 0x7fff : 0;
   }
   else if (len >= SN30_REPORT_LEN
         && (r[0] == 0x01 || r[0] == 0x03 || r[0] == 0x04))
   {
      buttons              = r + 8;
      hat                  = r[1];
      device->view.axes[0] = hidpad_8bitdo_stick(r[2]);
      device->view.axes[1] = hidpad_8bitdo_stick(r[3]);
      device->view.axes[4] = hidpad_8bitdo_stick(r[4]);
      device->view.axes[5] = hidpad_8bitdo_stick(r[5]);
      device->view.axes[2] = hidpad_8bitdo_trigger(r[7]);
      device->view.axes[3] = hidpad_8bitdo_trigger(r[6]);
   }
   else
      return;

   device->view.hats = hat < 8 ? hidpad_8bitdo_hat[hat] : 0;
   {
      uint32_t b = (uint32_t)buttons[0] | ((uint32_t)buttons[1] << 8);
      /* L4 and R4, on the pads that have them */
      if (     buttons == r + 8 && len > 10
            && device->model >= EIGHTBITDO_PRO_2)
         b |= (uint32_t)(r[10] & 0x03) << 16;
      pad_view_set_buttons(&device->view, b, (buttons[1] & 0x10) != 0);
   }
   /* the Ultimate 2 Wireless: its long report is the mode with rumble */
   if (     device->model == EIGHTBITDO_ULTIMATE_2 && !device->rumble
         && len >= ULTIMATE2_FULL_LEN)
      device->rumble = true;
   if (     device->motion_want && device->rumble && buttons == r + 8
         && len >= MOTION_AT + 12)
   {
      unsigned i;
      for (i = 0; i < 6; i++)
         device->imu[i] = (int16_t)(uint16_t)(r[MOTION_AT + 2 * i]
               | (r[MOTION_AT + 2 * i + 1] << 8));
      device->imu_seen = true;
   }

   if (device->motors_due && device->rumble && !device->behind_stack)
      hidpad_8bitdo_send(device);
}

static void hidpad_8bitdo_set_rumble(void *data,
      enum retro_rumble_effect effect, uint16_t strength)
{
   struct hidpad_8bitdo_data *device = (struct hidpad_8bitdo_data*)data;
   unsigned idx = (effect == RETRO_RUMBLE_STRONG) ? 0 : 1;
   uint8_t motor = (uint8_t)(strength >> 8);

   if (device && device->motors[idx] != motor)
   {
      device->motors[idx] = motor;
      device->motors_due  = true;
   }
}

static void hidpad_8bitdo_get_buttons(void *data, input_bits_t *state)
{
   struct hidpad_8bitdo_data *device = (struct hidpad_8bitdo_data*)data;
   if (device)
      *state = device->view.buttons;
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t hidpad_8bitdo_get_axis(void *data, unsigned axis)
{
   struct hidpad_8bitdo_data *device = (struct hidpad_8bitdo_data*)data;
   if (!device || axis >= INPUT_PAD_VIEW_AXES)
      return 0;
   return device->view.axes[axis];
}

static const char *hidpad_8bitdo_get_name(void *data)
{
   struct hidpad_8bitdo_data *device = (struct hidpad_8bitdo_data*)data;
   return hidpad_8bitdo_names[device ? device->model : 0];
}

static bool hidpad_8bitdo_set_sensor(void *data,
      enum retro_sensor_action action)
{
   struct hidpad_8bitdo_data *device = (struct hidpad_8bitdo_data*)data;
   if (!device || !pad_motion_want(&device->motion_want, action))
      return false;
   if (!device->motion_want)
      device->imu_seen = false;
   return true;
}

/* As SDL reads them: 4096 counts a g, 2000 degrees a second at the
 * gyroscope's end, turned onto SDL's axes, then libretro's. */
static bool hidpad_8bitdo_sensor(void *data, unsigned id, float *value)
{
   struct hidpad_8bitdo_data *device = (struct hidpad_8bitdo_data*)data;
   const float g_per = 1.0f / 4096.0f;
   const float r_per = 2000.0f * 3.14159265358979323846f / 180.0f / 32767.0f;
   float accel[3], gyro[3];

   if (!device || !device->imu_seen)
      return false;
   accel[0] = -device->imu[1] * g_per;
   accel[1] =  device->imu[2] * g_per;
   accel[2] = -device->imu[0] * g_per;
   gyro[0]  = -device->imu[4] * r_per;
   gyro[1]  =  device->imu[5] * r_per;
   gyro[2]  = -device->imu[3] * r_per;
   return pad_motion_value(accel, gyro, device->motion_want, id, value);
}

/* The hat too, for a driver that asks the handler for it */
static int32_t hidpad_8bitdo_button(void *data, uint16_t joykey)
{
   struct hidpad_8bitdo_data *device = (struct hidpad_8bitdo_data*)data;
   unsigned hat_dir;

   if (!device)
      return 0;
   if ((hat_dir = GET_HAT_DIR(joykey)))
   {
      if (GET_HAT(joykey) != 0)
         return 0;
      switch (hat_dir)
      {
         case HAT_UP_MASK:    return (device->view.hats & 1) != 0;
         case HAT_DOWN_MASK:  return (device->view.hats & 2) != 0;
         case HAT_LEFT_MASK:  return (device->view.hats & 4) != 0;
         case HAT_RIGHT_MASK: return (device->view.hats & 8) != 0;
      }
      return 0;
   }
   if (joykey > 31)
      return 0;
   return BIT256_GET(device->view.buttons, joykey) ? 1 : 0;
}

static const input_pad_view_t *hidpad_8bitdo_view(void *data)
{
   return &((struct hidpad_8bitdo_data*)data)->view;
}


pad_connection_interface_t pad_connection_8bitdo_sn30_pro = {
   hidpad_8bitdo_sn30_init,
   hidpad_8bitdo_deinit,
   hidpad_8bitdo_packet_handler,
   hidpad_8bitdo_set_rumble,
   hidpad_8bitdo_get_buttons,
   hidpad_8bitdo_get_axis,
   hidpad_8bitdo_get_name,
   hidpad_8bitdo_button,
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   NULL,  /* set_player */
   hidpad_8bitdo_view,
   NULL,  /* idle */
   hidpad_8bitdo_set_sensor,
   hidpad_8bitdo_sensor
};

pad_connection_interface_t pad_connection_8bitdo_sf30_pro = {
   hidpad_8bitdo_sf30_init,
   hidpad_8bitdo_deinit,
   hidpad_8bitdo_packet_handler,
   hidpad_8bitdo_set_rumble,
   hidpad_8bitdo_get_buttons,
   hidpad_8bitdo_get_axis,
   hidpad_8bitdo_get_name,
   hidpad_8bitdo_button,
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   NULL,  /* set_player */
   hidpad_8bitdo_view,
   NULL,  /* idle */
   hidpad_8bitdo_set_sensor,
   hidpad_8bitdo_sensor
};

pad_connection_interface_t pad_connection_8bitdo_pro_2 = {
   hidpad_8bitdo_pro2_init,
   hidpad_8bitdo_deinit,
   hidpad_8bitdo_packet_handler,
   hidpad_8bitdo_set_rumble,
   hidpad_8bitdo_get_buttons,
   hidpad_8bitdo_get_axis,
   hidpad_8bitdo_get_name,
   hidpad_8bitdo_button,
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   NULL,  /* set_player */
   hidpad_8bitdo_view,
   NULL,  /* idle */
   hidpad_8bitdo_set_sensor,
   hidpad_8bitdo_sensor
};

pad_connection_interface_t pad_connection_8bitdo_pro_3 = {
   hidpad_8bitdo_pro3_init,
   hidpad_8bitdo_deinit,
   hidpad_8bitdo_packet_handler,
   hidpad_8bitdo_set_rumble,
   hidpad_8bitdo_get_buttons,
   hidpad_8bitdo_get_axis,
   hidpad_8bitdo_get_name,
   hidpad_8bitdo_button,
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   NULL,  /* set_player */
   hidpad_8bitdo_view,
   NULL,  /* idle */
   hidpad_8bitdo_set_sensor,
   hidpad_8bitdo_sensor
};

pad_connection_interface_t pad_connection_8bitdo_ultimate_2 = {
   hidpad_8bitdo_ult2_init,
   hidpad_8bitdo_deinit,
   hidpad_8bitdo_packet_handler,
   hidpad_8bitdo_set_rumble,
   hidpad_8bitdo_get_buttons,
   hidpad_8bitdo_get_axis,
   hidpad_8bitdo_get_name,
   hidpad_8bitdo_button,
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   NULL,  /* set_player */
   hidpad_8bitdo_view,
   NULL,  /* idle */
   hidpad_8bitdo_set_sensor,
   hidpad_8bitdo_sensor
};

pad_connection_interface_t pad_connection_8bitdo_ultimate_2c = {
   hidpad_8bitdo_ult2c_init,
   hidpad_8bitdo_deinit,
   hidpad_8bitdo_packet_handler,
   hidpad_8bitdo_set_rumble,
   hidpad_8bitdo_get_buttons,
   hidpad_8bitdo_get_axis,
   hidpad_8bitdo_get_name,
   hidpad_8bitdo_button,
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   NULL,  /* set_player */
   hidpad_8bitdo_view,
   NULL,  /* idle */
   hidpad_8bitdo_set_sensor,
   hidpad_8bitdo_sensor
};

pad_connection_interface_t pad_connection_8bitdo_ultimate_3 = {
   hidpad_8bitdo_ult3_init,
   hidpad_8bitdo_deinit,
   hidpad_8bitdo_packet_handler,
   hidpad_8bitdo_set_rumble,
   hidpad_8bitdo_get_buttons,
   hidpad_8bitdo_get_axis,
   hidpad_8bitdo_get_name,
   hidpad_8bitdo_button,
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   NULL,  /* set_player */
   hidpad_8bitdo_view,
   NULL,  /* idle */
   hidpad_8bitdo_set_sensor,
   hidpad_8bitdo_sensor
};
