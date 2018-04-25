/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>

#include <retro_endianness.h>
#include <retro_miscellaneous.h>
#include <retro_timers.h>

#include "joypad_connection.h"
#include "../input_defines.h"
#include "../common/hid/hid_device_driver.h"

/* wiimote state flags*/
#define WIIMOTE_STATE_DEV_FOUND              0x0001
#define WIIMOTE_STATE_HANDSHAKE              0x0002   /* Actual connection exists but no handshake yet */
#define WIIMOTE_STATE_HANDSHAKE_COMPLETE     0x0004
#define WIIMOTE_STATE_CONNECTED              0x0008
#define WIIMOTE_STATE_EXP                    0x0040

/* Communication channels */

#define WM_SET_REPORT                        0x50

/* Commands */
#define WM_CMD_LED                           0x11
#define WM_CMD_REPORT_TYPE                   0x12
#define WM_CMD_RUMBLE                        0x13
#define WM_CMD_IR                            0x13
#define WM_CMD_CTRL_STATUS                   0x15
#define WM_CMD_WRITE_DATA                    0x16
#define WM_CMD_READ_DATA                     0x17
#define WM_CMD_IR_2                          0x1A

/* Input report IDs */
#define WM_RPT_CTRL_STATUS                   0x20
#define WM_RPT_READ                          0x21
#define WM_RPT_WRITE                         0x22
#define WM_RPT_BTN                           0x30
#define WM_RPT_BTN_ACC                       0x31
#define WM_RPT_BTN_ACC_IR                    0x33
#define WM_RPT_BTN_EXP                       0x34
#define WM_RPT_BTN_ACC_EXP                   0x35
#define WM_RPT_BTN_IR_EXP                    0x36
#define WM_RPT_BTN_ACC_IR_EXP                0x37

#define WM_BT_INPUT                          0x01
#define WM_BT_OUTPUT                         0x02

/* controller status stuff */
#define WM_MAX_BATTERY_CODE                  0xC8

/* offsets in wiimote memory */
#define WM_MEM_OFFSET_CALIBRATION            0x16
#define WM_EXP_MEM_BASE                      0x04A40000
#define WM_EXP_MEM_ENABLE                    0x04A40040
#define WM_EXP_MEM_CALIBR                    0x04A40020

#define EXP_HANDSHAKE_LEN                    224

/* controller status flags for the first message byte */
/* bit 1 is unknown */
#define WM_CTRL_STATUS_BYTE1_ATTACHMENT      0x02
#define WM_CTRL_STATUS_BYTE1_SPEAKER_ENABLED 0x04
#define WM_CTRL_STATUS_BYTE1_IR_ENABLED      0x08
#define WM_CTRL_STATUS_BYTE1_LED_1           0x10
#define WM_CTRL_STATUS_BYTE1_LED_2           0x20
#define WM_CTRL_STATUS_BYTE1_LED_3           0x40
#define WM_CTRL_STATUS_BYTE1_LED_4           0x80

/* LED bit masks */
#define WIIMOTE_LED_NONE                     0x00
#define WIIMOTE_LED_1                        0x10
#define WIIMOTE_LED_2                        0x20
#define WIIMOTE_LED_3                        0x40
#define WIIMOTE_LED_4                        0x80

/* button masks */
#define WIIMOTE_BUTTON_ALL                   0x1F9F
#define CLASSIC_CTRL_BUTTON_ALL              0xFEFF

/* expansion codes */
#define EXP_NONE                             0
#define EXP_CLASSIC                          2

#define IDENT_NUNCHUK                        0xA4200000
#define IDENT_CC                             0xA4200101

typedef struct axis_t
{
   bool has_center;

   uint8_t min;
   uint8_t center;
   uint8_t max;
   uint8_t raw_value;
   float value;
} axis_t;

typedef struct connect_wii_joystick_t
{
   axis_t x;
   axis_t y;
} connect_wii_joystick_t;

typedef struct connect_wii_classic_ctrl_t
{
   int16_t btns;
   struct connect_wii_joystick_t ljs;
   struct connect_wii_joystick_t rjs;
} connect_wii_classic_ctrl_t;

/* Generic expansion device plugged into wiimote. */
typedef struct connect_wii_expansion_t
{
   /* Type of expansion attached. */
   int type;

   union
   {
      struct connect_wii_classic_ctrl_t classic;
   } cc;
} connect_wii_expansion_t;

/* Wiimote structure. */
typedef struct connect_wii_wiimote_t
{
   /* User specified ID. */
   int unid;

   struct pad_connection* connection;
   hid_driver_t *driver;

   /* Various state flags. */
   uint32_t state;
   /* Currently lit LEDs. */
   uint8_t leds;
   /* Battery level. */
   float battery_level;
   /* The state of the connection handshake. */
   uint8_t handshake_state;
   /* Wiimote expansion device. */
   struct connect_wii_expansion_t exp;
   /* What buttons have just been pressed. */
   uint16_t btns;
} connect_wii_wiimote;

/* Macro to manage states */
#define WIIMOTE_IS_SET(wm, s)          ((wm->state & (s)) == (s))
#define WIIMOTE_ENABLE_STATE(wm, s)    (wm->state |= (s))
#define WIIMOTE_DISABLE_STATE(wm, s)   (wm->state &= ~(s))
#define WIIMOTE_TOGGLE_STATE(wm, s)    ((wm->state & (s)) ? WIIMOTE_DISABLE_STATE(wm, s) : WIIMOTE_ENABLE_STATE(wm, s))

static bool wiimote_is_connected(struct connect_wii_wiimote_t *wm)
{
   return WIIMOTE_IS_SET(wm, WIIMOTE_STATE_CONNECTED);
}

/*
 *	Send a packet to the wiimote.
 *
 *	This function should replace any write()s directly to the wiimote device.
 */
static int wiimote_send(struct connect_wii_wiimote_t* wm,
      uint8_t report_type, uint8_t* msg, int len)
{
   uint8_t buf[32] = {0};

   buf[0] = WM_SET_REPORT | WM_BT_OUTPUT;
   buf[1] = report_type;

   memcpy(buf+2, msg, len);

#ifdef WIIMOTE_DBG
   int x;
   printf("[DEBUG] (id %i) SEND: (%x) %.2x ", wm->unid, buf[0], buf[1]);
   for (x = 2; x < len+2; ++x)
      printf("%.2x ", buf[x]);
   printf("\n");
#endif

   wm->driver->send_control(wm->connection, buf, len + 2);
   return 1;
}

/*
 * Request the wiimote controller status.
 *
 * Controller status includes: battery level, LED status, expansions.
 */
static void wiimote_status(struct connect_wii_wiimote_t* wm)
{
   uint8_t buf = 0;

   if (!wm || !wiimote_is_connected(wm))
      return;

#ifdef WIIMOTE_DBG
   printf("Requested wiimote status.\n");
#endif

   wiimote_send(wm, WM_CMD_CTRL_STATUS, &buf, 1);
}

static void wiimote_data_report(struct connect_wii_wiimote_t* wm, uint8_t type)
{
   uint8_t buf[2] = {0x0,0x0};

   if (!wm  || !wiimote_is_connected(wm))
      return;

   buf[1] = type;

   /* CUIDADO es un &buf? */
   wiimote_send(wm, WM_CMD_REPORT_TYPE, buf, 2);
}

/*
 * Set the enabled LEDs.
 *
 * leds is a bitwise OR of:
 * - WIIMOTE_LED_1
 * - WIIMOTE_LED_2
 * - WIIMOTE_LED_3
 * - WIIMOTE_LED_4
 */

static void wiimote_set_leds(struct connect_wii_wiimote_t* wm, int leds)
{
   uint8_t buf = {0};

   if (!wm || !wiimote_is_connected(wm))
      return;

   /* Remove the lower 4 bits because they control rumble. */
   wm->leds = (leds & 0xF0);
   buf      = wm->leds;

   wiimote_send(wm, WM_CMD_LED, &buf, 1);
}

/* Find what buttons are pressed. */
static void wiimote_pressed_buttons(struct connect_wii_wiimote_t* wm, uint8_t* msg)
{
   /* Convert to big endian. */
   int16_t *val = (int16_t*)msg;
   int16_t now  = swap_if_little16(*val) & WIIMOTE_BUTTON_ALL;

   wm->btns     = now;
}

static int wiimote_classic_ctrl_handshake(struct connect_wii_wiimote_t* wm,
      struct connect_wii_classic_ctrl_t* cc, uint8_t* data, uint16_t len)
{
   memset(cc, 0, sizeof(*cc));
   wm->exp.type = EXP_CLASSIC;
   return 1;
}

static float wiimote_normalize_and_interpolate(float min, float max, float t)
{
   if (min == max)
      return 0.0f;
   return (t - min) / (max - min);
}

static void wiimote_process_axis(struct axis_t* axis, uint8_t raw)
{
   if (!axis->has_center)
   {
      axis->has_center = true;
      axis->min        = raw - 2;
      axis->center     = raw;
      axis->max        = raw + 2;
   }

   if (raw < axis->min)
      axis->min    = raw;
   if (raw > axis->max)
      axis->max    = raw;
   axis->raw_value = raw;

   if (raw < axis->center)
      axis->value  = -wiimote_normalize_and_interpolate(
            axis->center, axis->min, raw);
   else if (raw > axis->center)
      axis->value  = wiimote_normalize_and_interpolate(
            axis->center, axis->max, raw);
   else
      axis->value  = 0;
}

static void classic_ctrl_event(struct connect_wii_classic_ctrl_t* cc, uint8_t* msg)
{
   if (!cc)
      return;

   cc->btns = ~swap_if_little16(*(int16_t*)(msg + 4)) & CLASSIC_CTRL_BUTTON_ALL;

   wiimote_process_axis(&cc->ljs.x, (msg[0] & 0x3F));
   wiimote_process_axis(&cc->ljs.y, (msg[1] & 0x3F));
   wiimote_process_axis(&cc->rjs.x, ((msg[0] & 0xC0) >> 3) |
         ((msg[1] & 0xC0) >> 5) |   ((msg[2] & 0x80) >> 7));
   wiimote_process_axis(&cc->rjs.y, (msg[2] & 0x1F));
}

/*
 * Handle data from the expansion.
 */
static void wiimote_handle_expansion(struct connect_wii_wiimote_t* wm, uint8_t* msg)
{
   switch (wm->exp.type)
   {
      case EXP_CLASSIC:
         classic_ctrl_event(&wm->exp.cc.classic, msg);
         break;
      default:
         break;
   }
}

/*
 *	Write data to the wiimote.
 */
static int wiimote_write_data(struct connect_wii_wiimote_t* wm,
      uint32_t addr, uint8_t* data, uint8_t len)
{
   uint8_t buf[21] = {0};		/* the payload is always 23 */
   int32_t *buf32  = (int32_t*)buf;

   if (!wm || !wiimote_is_connected(wm))
      return 0;
   if (!data || !len)
      return 0;

#ifdef WIIMOTE_DBG
   int i           = 0;
   printf("Writing %i bytes to memory location 0x%x...\n", len, addr);
   printf("Write data is: ");
   for (; i < len; ++i)
      printf("%x ", data[i]);
   printf("\n");
#endif

   /* the offset is in big endian */
   *buf32 = swap_if_little32(addr);

   /* length */
   *(uint8_t*)(buf + 4) = len;

   /* data */
   memcpy(buf + 5, data, len);

   wiimote_send(wm, WM_CMD_WRITE_DATA, buf, 21);
   return 1;
}

/*
 *	Read data from the wiimote (event version).
 *
 *	The library can only handle one data read request at a time
 *	because it must keep track of the buffer and other
 *	events that are specific to that request.  So if a request
 *	has already been made, subsequent requests will be added
 *	to a pending list and be sent out when the previous
 *	finishes.
 */

static int wiimote_read_data(struct connect_wii_wiimote_t* wm, uint32_t addr,
      uint16_t len)
{
   uint8_t buf[6] = {0};
   int32_t *buf32 = (int32_t*)buf;
   int16_t *buf16 = (int16_t*)(buf + 4);

   /* No puden ser mas de 16 lo leido o vendra en trozos! */

   if (!wm || !wiimote_is_connected(wm) || !len)
      return 0;

   /* the offsets are in big endian */
   *buf32         = swap_if_little32(addr);
   *buf16         = swap_if_little16(len);

#ifdef WIIMOTE_DBG
   printf("Request read at address: 0x%x  length: %i", addr, len);
#endif
   wiimote_send(wm, WM_CMD_READ_DATA, buf, 6);

   return 1;
}

/*
 * Get initialization data from the Wiimote.
 *
 *	When first called for a connect_wii_wiimote_t structure, a request
 *	is sent to the wiimote for initialization information.
 *	This includes factory set accelerometer data.
 *	The handshake will be concluded when the wiimote responds
 *	with this data.
 */

static int wiimote_handshake(struct connect_wii_wiimote_t* wm,
      uint8_t event, uint8_t* data, uint16_t len)
{
   if (!wm)
      return 0;

   do
   {
      switch (wm->handshake_state)
      {
         case 0:
            /* no ha habido nunca handshake, debemos forzar un
             * mensaje de staus para ver que pasa. */

            WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE);
            wiimote_set_leds(wm, WIIMOTE_LED_NONE);

            /* Request the status of the Wiimote to
             * see if there is an expansion */
            wiimote_status(wm);

            wm->handshake_state=1;
            return 0;
         case 1:
            {
               /* estamos haciendo handshake o bien se necesita iniciar un
                * nuevo handshake ya que se inserta(quita una expansion. */
               int attachment = 0;

               if(event != WM_RPT_CTRL_STATUS)
                  return 0;

               /* Is an attachment connected to
                * the expansion port? */
               if ((data[2] & WM_CTRL_STATUS_BYTE1_ATTACHMENT) ==
                     WM_CTRL_STATUS_BYTE1_ATTACHMENT)
                  attachment = 1;

#ifdef WIIMOTE_DBG
               printf("attachment %d %d\n",attachment,
                     WIIMOTE_IS_SET(wm, WIIMOTE_STATE_EXP));
#endif

               if (attachment && !WIIMOTE_IS_SET(wm, WIIMOTE_STATE_EXP))
               {
                  uint8_t buf = 0;

                  /* Expansion port */

                  WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_EXP);

                  /* Send the initialization code for the attachment */

                  if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_HANDSHAKE_COMPLETE))
                  {
                     /* Rehandshake. */

                     WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE_COMPLETE);
                     /* forzamos un handshake por si venimos
                      * de un hanshake completo. */
                     WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE);
                  }

                  /*Old way. initialize the extension was by writing the
                   * single encryption byte 0x00 to 0x(4)A40040. */
#if 0
                  buf = 0x00;
                  wiimote_write_data(wm, WM_EXP_MEM_ENABLE, &buf, 1);
#endif

                  /* NEW WAY 0x55 to 0x(4)A400F0, then writing
                   * 0x00 to 0x(4)A400FB. (support clones) */
                  buf = 0x55;
                  wiimote_write_data(wm, 0x04A400F0, &buf, 1);
                  retro_sleep(100);
                  buf = 0x00;
                  wiimote_write_data(wm, 0x04A400FB, &buf, 1);

                  /* check extension type! */
                  retro_sleep(100);
                  wiimote_read_data(wm, WM_EXP_MEM_CALIBR + 220, 4);
#if 0
                  wiimote_read_data(wm, WM_EXP_MEM_CALIBR, EXP_HANDSHAKE_LEN);
#endif

                  wm->handshake_state = 4;
                  return 0;
               }
               else if (!attachment && WIIMOTE_IS_SET(wm, WIIMOTE_STATE_EXP))
               {
                  /* Attachment removed */
                  WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_EXP);
                  wm->exp.type = EXP_NONE;

                  if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_HANDSHAKE_COMPLETE))
                  {
#ifdef WIIMOTE_DBG
                     printf("rehandshake\n");
#endif
                     WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE_COMPLETE);
                     /* forzamos un handshake por si venimos
                      * de un hanshake completo. */
                     WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE);
                  }
               }

               if(!attachment &&  WIIMOTE_IS_SET(wm,WIIMOTE_STATE_HANDSHAKE))
               {
                  wm->handshake_state = 2;
                  continue;
               }

               return 0;
            }
         case 2:
            /* Find handshake no expansion. */
#ifdef WIIMOTE_DBG
            printf("Finalizado HANDSHAKE SIN EXPANSION\n");
#endif
            wiimote_data_report(wm,WM_RPT_BTN);
            wm->handshake_state = 6;
            continue;
         case 3:
            /* Find handshake expansion. */
#ifdef WIIMOTE_DBG
            printf("Finalizado HANDSHAKE CON EXPANSION\n");
#endif
            wiimote_data_report(wm,WM_RPT_BTN_EXP);
            wm->handshake_state = 6;
            continue;
         case 4:
            {
               uint32_t id;
               int32_t *ptr = (int32_t*)data;

               if (event != WM_RPT_READ)
                  return 0;

               id = swap_if_little32(*ptr);

               switch (id)
               {
                  case IDENT_CC:
                     retro_sleep(100);
                     /* pedimos datos de calibracion del JOY! */
                     wiimote_read_data(wm, WM_EXP_MEM_CALIBR, 16);
                     wm->handshake_state = 5;
                     break;
                  default:
                     wm->handshake_state = 2;
#if 0
                     WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_EXP);
#endif
                     continue;
               }
            }
            return 0;
         case 5:
            if(event !=  WM_RPT_READ)
               return 0;

            wiimote_classic_ctrl_handshake(wm, &wm->exp.cc.classic, data,len);
            wm->handshake_state = 3;
            continue;
         case 6:
            WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE);
            WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_HANDSHAKE_COMPLETE);
            wm->handshake_state = 1;

            switch (wm->unid)
            {
               case 0:
                  wiimote_set_leds(wm, WIIMOTE_LED_1);
                  break;
               case 1:
                  wiimote_set_leds(wm, WIIMOTE_LED_2);
                  break;
               case 2:
                  wiimote_set_leds(wm, WIIMOTE_LED_3);
                  break;
               case 3:
                  wiimote_set_leds(wm, WIIMOTE_LED_4);
                  break;
            }
            return 1;
         default:
            break;
      }
   }while(1);
}

static void hidpad_wii_deinit(void *data)
{
   struct connect_wii_wiimote_t* device = (struct connect_wii_wiimote_t*)data;

   if (device)
      free(device);
}

static void* hidpad_wii_init(void *data, uint32_t slot,
      hid_driver_t *driver)
{
   struct pad_connection *connection = (struct pad_connection*)data;
   struct connect_wii_wiimote_t *device = (struct connect_wii_wiimote_t*)
      calloc(1, sizeof(struct connect_wii_wiimote_t));

   if (!device)
      goto error;

   if (!connection)
      goto error;

   device->connection = connection;
   device->unid       = slot;
   device->state      = WIIMOTE_STATE_CONNECTED;
   device->exp.type   = EXP_NONE;
   device->driver     = driver;

   wiimote_handshake(device, -1, NULL, -1);

   return device;

error:
   hidpad_wii_deinit(device);
   return NULL;
}

static int16_t hidpad_wii_get_axis(void *data, unsigned axis)
{
   struct connect_wii_wiimote_t* device = (struct connect_wii_wiimote_t*)data;

   if (!device)
      return 0;

   switch (device->exp.type)
   {
      case EXP_CLASSIC:
         switch (axis)
         {
            case 0:
               return device->exp.cc.classic.ljs.x.value * 0x7FFF;
            case 1:
               return device->exp.cc.classic.ljs.y.value * 0x7FFF;
            case 2:
               return device->exp.cc.classic.rjs.x.value * 0x7FFF;
            case 3:
               return device->exp.cc.classic.rjs.y.value * 0x7FFF;
         }
         break;
      default:
         break;
   }

   return 0;
}

static void hidpad_wii_get_buttons(void *data, input_bits_t *state)
{
	struct connect_wii_wiimote_t* device = (struct connect_wii_wiimote_t*)data;
	if ( device )
	{
		/* TODO/FIXME - Broken? this doesn't match retropad! */
		uint32_t b = device->btns | (device->exp.cc.classic.btns << 16);
		BITS_COPY32_PTR(state, b);
	}
}

static void hidpad_wii_packet_handler(void *data,
      uint8_t *packet, uint16_t size)
{
   struct connect_wii_wiimote_t* device = (struct connect_wii_wiimote_t*)data;
   uint8_t             *msg = packet + 2;

   if (!device)
      return;

   switch (packet[1])
   {
      case WM_RPT_BTN:
         wiimote_pressed_buttons(device, msg);
         break;
      case WM_RPT_READ:
         wiimote_pressed_buttons(device, msg);
         wiimote_handshake(device, WM_RPT_READ, msg + 5,
               ((msg[2] & 0xF0) >> 4) + 1);
         break;
      case WM_RPT_CTRL_STATUS:
         wiimote_pressed_buttons(device, msg);
         wiimote_handshake(device,WM_RPT_CTRL_STATUS,msg,-1);
         break;
      case WM_RPT_BTN_EXP:
         wiimote_pressed_buttons(device, msg);
         wiimote_handle_expansion(device, msg+2);
         break;
   }
}

static void hidpad_wii_set_rumble(void *data,
      enum retro_rumble_effect effect, uint16_t strength)
{
   /* TODO */
   (void)data;
   (void)effect;
   (void)strength;
}

pad_connection_interface_t pad_connection_wii = {
   hidpad_wii_init,
   hidpad_wii_deinit,
   hidpad_wii_packet_handler,
   hidpad_wii_set_rumble,
   hidpad_wii_get_buttons,
   hidpad_wii_get_axis,
   NULL,
};
