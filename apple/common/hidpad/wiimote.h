/*
 * This file is part of iMAME4all.
 *
 * Copyright (C) 2010 David Valdeita (Seleuco)
 *
 * based on:
 *
 *  wiiuse
 *
 * Written By:
 *    Michael Laforest  < para >
 *    Email: < thepara (--AT--) g m a i l [--DOT--] com >
 *
 * Copyright 2006-2007
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * In addition, as a special exception, Seleuco
 * gives permission to link the code of this program with
 * the MAME library (or with modified versions of MAME that use the
 * same license as MAME), and distribute linked combinations including
 * the two.  You must obey the GNU General Public License in all
 * respects for all of the code used other than MAME.  If you modify
 * this file, you may extend this exception to your version of the
 * file, but you are not obligated to do so.  If you do not wish to
 * do so, delete this exception statement from your version.
 */

#ifndef __WIIMOTE_H__
#define __WIIMOTE_H__

#if defined(__cplusplus)
extern "C" {
#endif

   typedef unsigned char byte;
   typedef char sbyte;

   /* Convert to big endian */
   #define BIG_ENDIAN_LONG(i)                   (htonl(i))
   #define BIG_ENDIAN_SHORT(i)                  (htons(i))

   #define absf(x)                              ((x >= 0) ? (x) : (x * -1.0f))
   #define diff_f(x, y)                         ((x >= y) ? (absf(x - y)) : (absf(y - x)))

   /* wiimote state flags*/
   #define WIIMOTE_STATE_DEV_FOUND              0x0001
   #define WIIMOTE_STATE_HANDSHAKE              0x0002   /* actual connection exists but no handshake yet */
   #define WIIMOTE_STATE_HANDSHAKE_COMPLETE     0x0004
   #define WIIMOTE_STATE_CONNECTED              0x0008
    #define WIIMOTE_STATE_EXP                   0x0040

   /* Communication channels */

   #define WM_SET_REPORT                        0x50

   /* commands */
   #define WM_CMD_LED                           0x11
   #define WM_CMD_REPORT_TYPE                   0x12
   #define WM_CMD_RUMBLE                        0x13
   #define WM_CMD_IR                            0x13
   #define WM_CMD_CTRL_STATUS                   0x15
   #define WM_CMD_WRITE_DATA                    0x16
   #define WM_CMD_READ_DATA                     0x17
   #define WM_CMD_IR_2                          0x1A

   /* input report ids */
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

    #define WM_BT_INPUT                         0x01
    #define WM_BT_OUTPUT                        0x02

   /* controller status stuff */
   #define WM_MAX_BATTERY_CODE                  0xC8

    #define EXP_ID_CODE_CLASSIC_CONTROLLER      0x9A1EFDFD

   /* offsets in wiimote memory */
   #define WM_MEM_OFFSET_CALIBRATION            0x16
   #define WM_EXP_MEM_BASE                      0x04A40000
   #define WM_EXP_MEM_ENABLE                    0x04A40040
   #define WM_EXP_MEM_CALIBR                    0x04A40020

    #define EXP_HANDSHAKE_LEN                   224

   /* controller status flags for the first message byte */
   /* bit 1 is unknown */
   #define WM_CTRL_STATUS_BYTE1_ATTACHMENT      0x02
   #define WM_CTRL_STATUS_BYTE1_SPEAKER_ENABLED 0x04
   #define WM_CTRL_STATUS_BYTE1_IR_ENABLED      0x08
   #define WM_CTRL_STATUS_BYTE1_LED_1           0x10
   #define WM_CTRL_STATUS_BYTE1_LED_2           0x20
   #define WM_CTRL_STATUS_BYTE1_LED_3           0x40
   #define WM_CTRL_STATUS_BYTE1_LED_4           0x80

   /* led bit masks */
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

   typedef struct axis_t {
      bool has_center;

      byte min;
      byte center;
      byte max;
      byte raw_value;
      float value;
   } axis_t;

   typedef struct joystick_t {
      axis_t x;
      axis_t y;
   } joystick_t;

   typedef struct classic_ctrl_t {
      short btns;
      struct joystick_t ljs;
      struct joystick_t rjs;
   } classic_ctrl_t;

   /**
    * @struct expansion_t
    * @brief Generic expansion device plugged into wiimote.
    */
   typedef struct expansion_t {
      int type;                  /**< type of expansion attached           */

      union {
         struct classic_ctrl_t classic;
      };
   } expansion_t;

   /**
    * @struct wiimote_t
    * @brief Wiimote structure.
    */
   typedef struct wiimote_t {
      int unid;                  /**< user specified id                 */

      struct apple_pad_connection* connection;
          int state;                /**< various state flags               */
      byte leds;                 /**< currently lit leds                */
      float battery_level;          /**< battery level                     */

      byte handshake_state;         /**< the state of the connection handshake   */

      struct expansion_t exp;       /**< wiimote expansion device          */

      unsigned short btns;          /**< what buttons have just been pressed  */
   } wiimote;

   /* macro to manage states */
   #define WIIMOTE_IS_SET(wm, s)          ((wm->state & (s)) == (s))
   #define WIIMOTE_ENABLE_STATE(wm, s)    (wm->state |= (s))
   #define WIIMOTE_DISABLE_STATE(wm, s)   (wm->state &= ~(s))
   #define WIIMOTE_TOGGLE_STATE(wm, s)    ((wm->state & (s)) ? WIIMOTE_DISABLE_STATE(wm, s) : WIIMOTE_ENABLE_STATE(wm, s))
   #define WIIMOTE_IS_CONNECTED(wm)       (WIIMOTE_IS_SET(wm, WIIMOTE_STATE_CONNECTED))

int  wiimote_handshake(struct wiimote_t* wm,  byte event, byte* data, unsigned short len);
void wiimote_status(struct wiimote_t* wm);
void wiimote_data_report(struct wiimote_t* wm, byte type);
void wiimote_pressed_buttons(struct wiimote_t* wm, byte* msg);
void wiimote_handle_expansion(struct wiimote_t* wm, byte* msg);


#if defined(__cplusplus)
}
#endif

#endif
