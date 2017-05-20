/****************************************************************************
 * Copyright (C) 2016,2017 Maschell
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/

#ifndef _PAD_CONST_H_
#define _PAD_CONST_H_

#include <string>
#include "../patcher/ControllerPatcherDefs.h"

extern const u8 DEF_R_STICK;
extern const u8 DEF_L_STICK;

extern const u8 DEF_STICK_OFFSET_INVERT;
extern const u8 DEF_STICK_OFFSET_DEADZONE;
extern const u8 DEF_STICK_OFFSET_MINMAX;

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Device names
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern const char *HID_GC_STRING;
extern const char *HID_KEYBOARD_STRING;
extern const char *HID_MOUSE_STRING;
extern const char *HID_DS3_STRING;
extern const char *HID_DS4_STRING;
extern const char *HID_NEW_DS4_STRING;
extern const char *HID_XINPUT_STRING;
extern const char *HID_SWITCH_PRO_STRING;

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! GC_Adapter
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

extern const u8 HID_GC_BUTTON_A[];
extern const u8 HID_GC_BUTTON_B[];
extern const u8 HID_GC_BUTTON_X[];
extern const u8 HID_GC_BUTTON_Y[];
extern const u8 HID_GC_BUTTON_LEFT[];
extern const u8 HID_GC_BUTTON_RIGHT[];
extern const u8 HID_GC_BUTTON_DOWN[];
extern const u8 HID_GC_BUTTON_UP[];

extern const u8 HID_GC_BUTTON_START[];
extern const u8 HID_GC_BUTTON_Z[];

extern const u8 HID_GC_BUTTON_L[];
extern const u8 HID_GC_BUTTON_R[];

extern const u8 HID_GC_BUTTON_DPAD_TYPE[];

extern const u8 HID_GC_STICK_L_X[STICK_CONF_ENUM_MAXVALUE];
extern const u8 HID_GC_STICK_L_Y[STICK_CONF_ENUM_MAXVALUE];
extern const u8 HID_GC_STICK_R_X[STICK_CONF_ENUM_MAXVALUE];
extern const u8 HID_GC_STICK_R_Y[STICK_CONF_ENUM_MAXVALUE];


//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! DS3
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

extern const u8 HID_DS3_BUTTON_CROSS[];
extern const u8 HID_DS3_BUTTON_CIRCLE[];
extern const u8 HID_DS3_BUTTON_SQUARE [];
extern const u8 HID_DS3_BUTTON_TRIANGLE[];

extern const u8 HID_DS3_BUTTON_L1[];
extern const u8 HID_DS3_BUTTON_L2[];
extern const u8 HID_DS3_BUTTON_R1[];
extern const u8 HID_DS3_BUTTON_R2[];

extern const u8 HID_DS3_BUTTON_L3[];
extern const u8 HID_DS3_BUTTON_R3[];
extern const u8 HID_DS3_BUTTON_SELECT[];
extern const u8 HID_DS3_BUTTON_START[];
extern const u8 HID_DS3_BUTTON_LEFT[];
extern const u8 HID_DS3_BUTTON_RIGHT[];
extern const u8 HID_DS3_BUTTON_UP[];
extern const u8 HID_DS3_BUTTON_DOWN[];

extern const u8 HID_DS3_BUTTON_GUIDE[];

extern const u8 HID_DS3_BUTTON_DPAD_TYPE[];

extern const u8 HID_DS3_STICK_L_X[STICK_CONF_ENUM_MAXVALUE];
extern const u8 HID_DS3_STICK_L_Y[STICK_CONF_ENUM_MAXVALUE];
extern const u8 HID_DS3_STICK_R_X[STICK_CONF_ENUM_MAXVALUE];
extern const u8 HID_DS3_STICK_R_Y[STICK_CONF_ENUM_MAXVALUE];

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! DS4
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

extern const u8 HID_DS4_BUTTON_CROSS[];
extern const u8 HID_DS4_BUTTON_CIRCLE[];
extern const u8 HID_DS4_BUTTON_SQUARE [];
extern const u8 HID_DS4_BUTTON_TRIANGLE[];

extern const u8 HID_DS4_BUTTON_L1[];
extern const u8 HID_DS4_BUTTON_L2[];
extern const u8 HID_DS4_BUTTON_L3[];
extern const u8 HID_DS4_BUTTON_R1[];
extern const u8 HID_DS4_BUTTON_R2[];
extern const u8 HID_DS4_BUTTON_R3[];

extern const u8 HID_DS4_BUTTON_SHARE[];
extern const u8 HID_DS4_BUTTON_OPTIONS[];

extern const u8 HID_DS4_BUTTON_DPAD_TYPE[];
extern const u8 HID_DS4_BUTTON_DPAD_N[];
extern const u8 HID_DS4_BUTTON_DPAD_NE[];
extern const u8 HID_DS4_BUTTON_DPAD_E[];
extern const u8 HID_DS4_BUTTON_DPAD_SE[];
extern const u8 HID_DS4_BUTTON_DPAD_S[];
extern const u8 HID_DS4_BUTTON_DPAD_SW[];
extern const u8 HID_DS4_BUTTON_DPAD_W[];
extern const u8 HID_DS4_BUTTON_DPAD_NW[];
extern const u8 HID_DS4_BUTTON_DPAD_NEUTRAL[];

extern const u8 HID_DS4_BUTTON_GUIDE[];
extern const u8 HID_DS4_BUTTON_T_PAD_CLICK[];

extern const u8 HID_DS4_STICK_L_X[STICK_CONF_ENUM_MAXVALUE];
extern const u8 HID_DS4_STICK_L_Y[STICK_CONF_ENUM_MAXVALUE];
extern const u8 HID_DS4_STICK_R_X[STICK_CONF_ENUM_MAXVALUE];
extern const u8 HID_DS4_STICK_R_Y[STICK_CONF_ENUM_MAXVALUE];


//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! XInput
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

extern const u8 HID_XINPUT_BUTTON_A[];
extern const u8 HID_XINPUT_BUTTON_B[];
extern const u8 HID_XINPUT_BUTTON_X[];
extern const u8 HID_XINPUT_BUTTON_Y[];

extern const u8 HID_XINPUT_BUTTON_LB[];
extern const u8 HID_XINPUT_BUTTON_LT[];
extern const u8 HID_XINPUT_BUTTON_L3[];

extern const u8 HID_XINPUT_BUTTON_RB[];
extern const u8 HID_XINPUT_BUTTON_RT[];
extern const u8 HID_XINPUT_BUTTON_R3[];

extern const u8 HID_XINPUT_BUTTON_START[];
extern const u8 HID_XINPUT_BUTTON_BACK[];
extern const u8 HID_XINPUT_BUTTON_GUIDE[];

extern const u8 HID_XINPUT_BUTTON_DPAD_TYPE[];
extern const u8 HID_XINPUT_BUTTON_LEFT[];
extern const u8 HID_XINPUT_BUTTON_RIGHT[];
extern const u8 HID_XINPUT_BUTTON_DOWN[];
extern const u8 HID_XINPUT_BUTTON_UP[];

extern const u8 HID_XINPUT_STICK_L_X[STICK_CONF_ENUM_MAXVALUE];
extern const u8 HID_XINPUT_STICK_L_Y[STICK_CONF_ENUM_MAXVALUE];
extern const u8 HID_XINPUT_STICK_R_X[STICK_CONF_ENUM_MAXVALUE];
extern const u8 HID_XINPUT_STICK_R_Y[STICK_CONF_ENUM_MAXVALUE];

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Switch Pro Controller
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

extern const u8 HID_SWITCH_PRO_BT_BUTTON_A[];
extern const u8 HID_SWITCH_PRO_BT_BUTTON_B[];
extern const u8 HID_SWITCH_PRO_BT_BUTTON_X[];
extern const u8 HID_SWITCH_PRO_BT_BUTTON_Y[];

extern const u8 HID_SWITCH_PRO_BT_BUTTON_L[];
extern const u8 HID_SWITCH_PRO_BT_BUTTON_ZL[];
extern const u8 HID_SWITCH_PRO_BT_BUTTON_STICK_L[];

extern const u8 HID_SWITCH_PRO_BT_BUTTON_R[];
extern const u8 HID_SWITCH_PRO_BT_BUTTON_ZR[];
extern const u8 HID_SWITCH_PRO_BT_BUTTON_STICK_R[];

extern const u8 HID_SWITCH_PRO_BT_BUTTON_PLUS[];
extern const u8 HID_SWITCH_PRO_BT_BUTTON_MINUS[];
extern const u8 HID_SWITCH_PRO_BT_BUTTON_HOME[];

extern const u8 HID_SWITCH_PRO_BT_BUTTON_DPAD_TYPE[];
extern const u8 HID_SWITCH_PRO_BT_BUTTON_DPAD_N[];
extern const u8 HID_SWITCH_PRO_BT_BUTTON_DPAD_NE[];
extern const u8 HID_SWITCH_PRO_BT_BUTTON_DPAD_E[];
extern const u8 HID_SWITCH_PRO_BT_BUTTON_DPAD_SE[];
extern const u8 HID_SWITCH_PRO_BT_BUTTON_DPAD_S[];
extern const u8 HID_SWITCH_PRO_BT_BUTTON_DPAD_SW[];
extern const u8 HID_SWITCH_PRO_BT_BUTTON_DPAD_W[];
extern const u8 HID_SWITCH_PRO_BT_BUTTON_DPAD_NW[];
extern const u8 HID_SWITCH_PRO_BT_BUTTON_DPAD_NEUTRAL[];


extern const u8 HID_SWITCH_PRO_BT_STICK_L_X[STICK_CONF_ENUM_MAXVALUE];
extern const u8 HID_SWITCH_PRO_BT_STICK_L_Y[STICK_CONF_ENUM_MAXVALUE];
extern const u8 HID_SWITCH_PRO_BT_STICK_R_X[STICK_CONF_ENUM_MAXVALUE];
extern const u8 HID_SWITCH_PRO_BT_STICK_R_Y[STICK_CONF_ENUM_MAXVALUE];

#endif /* _PAD_CONST_H_ */
