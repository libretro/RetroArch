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
#include "PadConst.hpp"

const u8 DEF_R_STICK =        220;
const u8 DEF_L_STICK =        221;

const u8 DEF_STICK_OFFSET_INVERT    =   CONTRPS_VPAD_BUTTON_L_STICK_X_INVERT   -    CONTRPS_VPAD_BUTTON_L_STICK_X;
const u8 DEF_STICK_OFFSET_DEADZONE  =   CONTRPS_VPAD_BUTTON_L_STICK_X_DEADZONE -    CONTRPS_VPAD_BUTTON_L_STICK_X;
const u8 DEF_STICK_OFFSET_MINMAX    =   CONTRPS_VPAD_BUTTON_L_STICK_X_MINMAX   -    CONTRPS_VPAD_BUTTON_L_STICK_X;

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Device names
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
const char *HID_GC_STRING         = "GameCube\nUSB-Adapter";
const char *HID_KEYBOARD_STRING   = "Keyboard";
const char *HID_MOUSE_STRING      = "Mouse";
const char *HID_DS3_STRING        = "DualShock 3\nController";
const char *HID_DS4_STRING        = "DualShock 4\nController";
const char *HID_NEW_DS4_STRING    = "DualShock 4\nController";
const char *HID_XINPUT_STRING     = "XInput\nController";
const char *HID_SWITCH_PRO_STRING = "Switch\nPro Controller";

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! GC-Adapter
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

const u8 HID_GC_BUTTON_A[]      = { 0x01,HID_GC_BUTTON_A_VALUE};
const u8 HID_GC_BUTTON_B[]      = { 0x01,HID_GC_BUTTON_B_VALUE};
const u8 HID_GC_BUTTON_X[]      = { 0x01,HID_GC_BUTTON_X_VALUE};
const u8 HID_GC_BUTTON_Y[]      = { 0x01,HID_GC_BUTTON_Y_VALUE};
const u8 HID_GC_BUTTON_LEFT[]   = { 0x01,HID_GC_BUTTON_LEFT_VALUE};
const u8 HID_GC_BUTTON_RIGHT[]  = { 0x01,HID_GC_BUTTON_RIGHT_VALUE};
const u8 HID_GC_BUTTON_DOWN[]   = { 0x01,HID_GC_BUTTON_DOWN_VALUE};
const u8 HID_GC_BUTTON_UP[]     = { 0x01,HID_GC_BUTTON_UP_VALUE};

const u8 HID_GC_BUTTON_START[]  = { 0x02,HID_GC_BUTTON_START_VALUE};
const u8 HID_GC_BUTTON_Z[]      = { 0x02,HID_GC_BUTTON_Z_VALUE};

const u8 HID_GC_BUTTON_L[]      = { 0x07,HID_GC_BUTTON_L_VALUE};
const u8 HID_GC_BUTTON_R[]      = { 0x08,HID_GC_BUTTON_R_VALUE};

const u8 HID_GC_BUTTON_DPAD_TYPE[]     = { CONTRPDM_Normal,0x00};

const u8 HID_GC_STICK_L_X[STICK_CONF_ENUM_MAXVALUE] =  {    STICK_CONF_MAGIC_VALUE, //STICK_CONF_MAGIC_VERSION
                                                            0x03, //STICK_CONF_BYTE,
                                                            0x80, //STICK_CONF_DEFAULT,
                                                            0x09, //STICK_CONF_DEADZONE,
                                                            0x00, //STICK_CONF_INVERT,
                                                            0x1A, //STICK_CONF_MIN,
                                                            0xE4};//STICK_CONF_MAX,

const u8 HID_GC_STICK_L_Y[STICK_CONF_ENUM_MAXVALUE] =  {    STICK_CONF_MAGIC_VALUE, //STICK_CONF_MAGIC_VERSION
                                                            0x04, //STICK_CONF_BYTE,
                                                            0x80, //STICK_CONF_DEFAULT,
                                                            0x09, //STICK_CONF_DEADZONE,
                                                            0x00, //STICK_CONF_INVERT,
                                                            0x11, //STICK_CONF_MIN,
                                                            0xE1};//STICK_CONF_MAX,

const u8 HID_GC_STICK_R_X[STICK_CONF_ENUM_MAXVALUE] =  {    STICK_CONF_MAGIC_VALUE, //STICK_CONF_MAGIC_VERSION
                                                            0x05, //STICK_CONF_BYTE,
                                                            0x80, //STICK_CONF_DEFAULT,
                                                            0x09, //STICK_CONF_DEADZONE,
                                                            0x00, //STICK_CONF_INVERT,
                                                            0x2B, //STICK_CONF_MIN,
                                                            0xE2};//STICK_CONF_MAX,

const u8 HID_GC_STICK_R_Y[STICK_CONF_ENUM_MAXVALUE] =  {    STICK_CONF_MAGIC_VALUE, //STICK_CONF_MAGIC_VERSION
                                                            0x06, //STICK_CONF_BYTE,
                                                            0x80, //STICK_CONF_DEFAULT,
                                                            0x09, //STICK_CONF_DEADZONE,
                                                            0x00, //STICK_CONF_INVERT,
                                                            0x1D, //STICK_CONF_MIN,
                                                            0xDB};//STICK_CONF_MAX,

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! DS3
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

const u8 HID_DS3_BUTTON_CROSS[]     = { 0x03,HID_DS3_BUTTON_CROSS_VALUE};
const u8 HID_DS3_BUTTON_CIRCLE[]    = { 0x03,HID_DS3_BUTTON_CIRCLE_VALUE};
const u8 HID_DS3_BUTTON_SQUARE []   = { 0x03,HID_DS3_BUTTON_SQUARE_VALUE};
const u8 HID_DS3_BUTTON_TRIANGLE[]  = { 0x03,HID_DS3_BUTTON_TRIANGLE_VALUE};

const u8 HID_DS3_BUTTON_L1[]        = { 0x03,HID_DS3_BUTTON_L1_VALUE};
const u8 HID_DS3_BUTTON_L2[]        = { 0x03,HID_DS3_BUTTON_L2_VALUE};
const u8 HID_DS3_BUTTON_R1[]        = { 0x03,HID_DS3_BUTTON_R1_VALUE};
const u8 HID_DS3_BUTTON_R2[]        = { 0x03,HID_DS3_BUTTON_R2_VALUE};

const u8 HID_DS3_BUTTON_L3[]        = { 0x02,HID_DS3_BUTTON_L3_VALUE};
const u8 HID_DS3_BUTTON_R3[]        = { 0x02,HID_DS3_BUTTON_R3_VALUE};
const u8 HID_DS3_BUTTON_SELECT[]    = { 0x02,HID_DS3_BUTTON_SELECT_VALUE};
const u8 HID_DS3_BUTTON_START[]     = { 0x02,HID_DS3_BUTTON_START_VALUE};
const u8 HID_DS3_BUTTON_LEFT[]      = { 0x02,HID_DS3_BUTTON_LEFT_VALUE};
const u8 HID_DS3_BUTTON_RIGHT[]     = { 0x02,HID_DS3_BUTTON_RIGHT_VALUE};
const u8 HID_DS3_BUTTON_UP[]        = { 0x02,HID_DS3_BUTTON_UP_VALUE};
const u8 HID_DS3_BUTTON_DOWN[]      = { 0x02,HID_DS3_BUTTON_DOWN_VALUE};

const u8 HID_DS3_BUTTON_GUIDE[]      = { 0x04,HID_DS3_BUTTON_GUIDE_VALUE};

const u8 HID_DS3_BUTTON_DPAD_TYPE[]     = { CONTRPDM_Normal,0x00};

const u8 HID_DS3_STICK_L_X[STICK_CONF_ENUM_MAXVALUE] =  {   STICK_CONF_MAGIC_VALUE, //STICK_CONF_MAGIC_VERSION
                                                            0x06, //STICK_CONF_BYTE,
                                                            0x80, //STICK_CONF_DEFAULT,
                                                            0x06, //STICK_CONF_DEADZONE,
                                                            0x00, //STICK_CONF_INVERT,
                                                            0x00, //STICK_CONF_MIN,
                                                            0xFF};//STICK_CONF_MAX,

const u8 HID_DS3_STICK_L_Y[STICK_CONF_ENUM_MAXVALUE] =  {   STICK_CONF_MAGIC_VALUE, //STICK_CONF_MAGIC_VERSION
                                                            0x07, //STICK_CONF_BYTE,
                                                            0x80, //STICK_CONF_DEFAULT,
                                                            0x06, //STICK_CONF_DEADZONE,
                                                            0x01, //STICK_CONF_INVERT,
                                                            0x00, //STICK_CONF_MIN,
                                                            0xFF};//STICK_CONF_MAX,

const u8 HID_DS3_STICK_R_X[STICK_CONF_ENUM_MAXVALUE] =  {   STICK_CONF_MAGIC_VALUE, //STICK_CONF_MAGIC_VERSION
                                                            0x08, //STICK_CONF_BYTE,
                                                            0x80, //STICK_CONF_DEFAULT,
                                                            0x06, //STICK_CONF_DEADZONE,
                                                            0x00, //STICK_CONF_INVERT,
                                                            0x00, //STICK_CONF_MIN,
                                                            0xFF};//STICK_CONF_MAX,

const u8 HID_DS3_STICK_R_Y[STICK_CONF_ENUM_MAXVALUE] =  {   STICK_CONF_MAGIC_VALUE, //STICK_CONF_MAGIC_VERSION
                                                            0x09, //STICK_CONF_BYTE,
                                                            0x80, //STICK_CONF_DEFAULT,
                                                            0x06, //STICK_CONF_DEADZONE,
                                                            0x01, //STICK_CONF_INVERT,
                                                            0x00, //STICK_CONF_MIN,
                                                            0xFF};//STICK_CONF_MAX,

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! DS4
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

const u8 HID_DS4_BUTTON_CROSS[]     = { 0x05,HID_DS4_BUTTON_CROSS_VALUE};
const u8 HID_DS4_BUTTON_CIRCLE[]    = { 0x05,HID_DS4_BUTTON_CIRCLE_VALUE};
const u8 HID_DS4_BUTTON_SQUARE []   = { 0x05,HID_DS4_BUTTON_SQUARE_VALUE};
const u8 HID_DS4_BUTTON_TRIANGLE[]  = { 0x05,HID_DS4_BUTTON_TRIANGLE_VALUE};

const u8 HID_DS4_BUTTON_L1[]        = { 0x06,HID_DS4_BUTTON_L1_VALUE};
const u8 HID_DS4_BUTTON_L2[]        = { 0x06,HID_DS4_BUTTON_L2_VALUE};
const u8 HID_DS4_BUTTON_L3[]        = { 0x06,HID_DS4_BUTTON_L3_VALUE};

const u8 HID_DS4_BUTTON_R1[]        = { 0x06,HID_DS4_BUTTON_R1_VALUE};
const u8 HID_DS4_BUTTON_R2[]        = { 0x06,HID_DS4_BUTTON_R2_VALUE};
const u8 HID_DS4_BUTTON_R3[]        = { 0x06,HID_DS4_BUTTON_R3_VALUE};

const u8 HID_DS4_BUTTON_SHARE[]     = { 0x06,HID_DS4_BUTTON_SHARE_VALUE};
const u8 HID_DS4_BUTTON_OPTIONS[]   = { 0x06,HID_DS4_BUTTON_OPTIONS_VALUE};


const u8 HID_DS4_BUTTON_DPAD_TYPE[]     = { CONTRPDM_Hat,HID_DS4_BUTTON_DPAD_MASK_VALUE};
const u8 HID_DS4_BUTTON_DPAD_N[]        = { 0x05,HID_DS4_BUTTON_DPAD_N_VALUE};
const u8 HID_DS4_BUTTON_DPAD_NE[]       = { 0x05,HID_DS4_BUTTON_DPAD_NE_VALUE};
const u8 HID_DS4_BUTTON_DPAD_E[]        = { 0x05,HID_DS4_BUTTON_DPAD_E_VALUE};
const u8 HID_DS4_BUTTON_DPAD_SE[]       = { 0x05,HID_DS4_BUTTON_DPAD_SE_VALUE};
const u8 HID_DS4_BUTTON_DPAD_S[]        = { 0x05,HID_DS4_BUTTON_DPAD_S_VALUE};
const u8 HID_DS4_BUTTON_DPAD_SW[]       = { 0x05,HID_DS4_BUTTON_DPAD_SW_VALUE};
const u8 HID_DS4_BUTTON_DPAD_W[]        = { 0x05,HID_DS4_BUTTON_DPAD_W_VALUE};
const u8 HID_DS4_BUTTON_DPAD_NW[]       = { 0x05,HID_DS4_BUTTON_DPAD_NW_VALUE};
const u8 HID_DS4_BUTTON_DPAD_NEUTRAL[]  = { 0x05,HID_DS4_BUTTON_DPAD_NEUTRAL_VALUE};

const u8 HID_DS4_BUTTON_GUIDE[]          = { 0x07,HID_DS4_BUTTON_GUIDE_VALUE};
const u8 HID_DS4_BUTTON_T_PAD_CLICK[]    = { 0x07,HID_DS4_BUTTON_T_PAD_CLICK_VALUE};

const u8 HID_DS4_STICK_L_X[STICK_CONF_ENUM_MAXVALUE] =  {   STICK_CONF_MAGIC_VALUE, //STICK_CONF_MAGIC_VERSION
                                                            0x01, //STICK_CONF_BYTE,
                                                            0x80, //STICK_CONF_DEFAULT,
                                                            0x06, //STICK_CONF_DEADZONE,
                                                            0x00, //STICK_CONF_INVERT,
                                                            0x00, //STICK_CONF_MIN,
                                                            0xFF};//STICK_CONF_MAX,

const u8 HID_DS4_STICK_L_Y[STICK_CONF_ENUM_MAXVALUE] =  {   STICK_CONF_MAGIC_VALUE, //STICK_CONF_MAGIC_VERSION
                                                            0x02, //STICK_CONF_BYTE,
                                                            0x80, //STICK_CONF_DEFAULT,
                                                            0x05, //STICK_CONF_DEADZONE,
                                                            0x01, //STICK_CONF_INVERT,
                                                            0x00, //STICK_CONF_MIN,
                                                            0xFF};//STICK_CONF_MAX,

const u8 HID_DS4_STICK_R_X[STICK_CONF_ENUM_MAXVALUE] =  {   STICK_CONF_MAGIC_VALUE, //STICK_CONF_MAGIC_VERSION
                                                            0x03, //STICK_CONF_BYTE,
                                                            0x80, //STICK_CONF_DEFAULT,
                                                            0x07, //STICK_CONF_DEADZONE,
                                                            0x00, //STICK_CONF_INVERT,
                                                            0x00, //STICK_CONF_MIN,
                                                            0xFF};//STICK_CONF_MAX,

const u8 HID_DS4_STICK_R_Y[STICK_CONF_ENUM_MAXVALUE] =  {   STICK_CONF_MAGIC_VALUE, //STICK_CONF_MAGIC_VERSION
                                                            0x04, //STICK_CONF_BYTE,
                                                            0x80, //STICK_CONF_DEFAULT,
                                                            0x09, //STICK_CONF_DEADZONE,
                                                            0x01, //STICK_CONF_INVERT,
                                                            0x00, //STICK_CONF_MIN,
                                                            0xFF};//STICK_CONF_MAX,

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! XInput
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

const u8 HID_XINPUT_BUTTON_A[]          = { 0x07,HID_XINPUT_BUTTON_A_VALUE};
const u8 HID_XINPUT_BUTTON_B[]          = { 0x07,HID_XINPUT_BUTTON_B_VALUE};
const u8 HID_XINPUT_BUTTON_X[]          = { 0x07,HID_XINPUT_BUTTON_X_VALUE};
const u8 HID_XINPUT_BUTTON_Y[]          = { 0x07,HID_XINPUT_BUTTON_Y_VALUE};

const u8 HID_XINPUT_BUTTON_LB[]         = { 0x06,HID_XINPUT_BUTTON_LB_VALUE};
const u8 HID_XINPUT_BUTTON_LT[]         = { 0x04,HID_XINPUT_BUTTON_LT_VALUE};
const u8 HID_XINPUT_BUTTON_L3[]         = { 0x06,HID_XINPUT_BUTTON_L3_VALUE};

const u8 HID_XINPUT_BUTTON_RB[]         = { 0x06,HID_XINPUT_BUTTON_RB_VALUE};
const u8 HID_XINPUT_BUTTON_RT[]         = { 0x05,HID_XINPUT_BUTTON_RT_VALUE};
const u8 HID_XINPUT_BUTTON_R3[]         = { 0x06,HID_XINPUT_BUTTON_R3_VALUE};

const u8 HID_XINPUT_BUTTON_START[]      = { 0x06,HID_XINPUT_BUTTON_START_VALUE};
const u8 HID_XINPUT_BUTTON_BACK[]       = { 0x06,HID_XINPUT_BUTTON_BACK_VALUE};
const u8 HID_XINPUT_BUTTON_GUIDE[]      = { 0x06,HID_XINPUT_BUTTON_GUIDE_VALUE};

const u8 HID_XINPUT_BUTTON_DPAD_TYPE[]  = { CONTRPDM_Normal,HID_XINPUT_BUTTON_DPAD_MASK_VALUE};
const u8 HID_XINPUT_BUTTON_LEFT[]       = { 0x07,HID_XINPUT_BUTTON_LEFT_VALUE};
const u8 HID_XINPUT_BUTTON_RIGHT[]      = { 0x07,HID_XINPUT_BUTTON_RIGHT_VALUE};
const u8 HID_XINPUT_BUTTON_DOWN[]       = { 0x07,HID_XINPUT_BUTTON_DOWN_VALUE};
const u8 HID_XINPUT_BUTTON_UP[]         = { 0x07,HID_XINPUT_BUTTON_UP_VALUE};

const u8 HID_XINPUT_STICK_L_X[STICK_CONF_ENUM_MAXVALUE] =  {   STICK_CONF_MAGIC_VALUE, //STICK_CONF_MAGIC_VERSION
                                                            0x00, //STICK_CONF_BYTE,
                                                            0x80, //STICK_CONF_DEFAULT,
                                                            0x10, //STICK_CONF_DEADZONE,
                                                            0x00, //STICK_CONF_INVERT,
                                                            0x00, //STICK_CONF_MIN,
                                                            0xFF};//STICK_CONF_MAX,

const u8 HID_XINPUT_STICK_L_Y[STICK_CONF_ENUM_MAXVALUE] =  {   STICK_CONF_MAGIC_VALUE, //STICK_CONF_MAGIC_VERSION
                                                            0x01, //STICK_CONF_BYTE,
                                                            0x80, //STICK_CONF_DEFAULT,
                                                            0x10, //STICK_CONF_DEADZONE,
                                                            0x00, //STICK_CONF_INVERT,
                                                            0x00, //STICK_CONF_MIN,
                                                            0xFF};//STICK_CONF_MAX,

const u8 HID_XINPUT_STICK_R_X[STICK_CONF_ENUM_MAXVALUE] =  {   STICK_CONF_MAGIC_VALUE, //STICK_CONF_MAGIC_VERSION
                                                            0x02, //STICK_CONF_BYTE,
                                                            0x80, //STICK_CONF_DEFAULT,
                                                            0x10, //STICK_CONF_DEADZONE,
                                                            0x00, //STICK_CONF_INVERT,
                                                            0x00, //STICK_CONF_MIN,
                                                            0xFF};//STICK_CONF_MAX,

const u8 HID_XINPUT_STICK_R_Y[STICK_CONF_ENUM_MAXVALUE] =  {   STICK_CONF_MAGIC_VALUE, //STICK_CONF_MAGIC_VERSION
                                                            0x03, //STICK_CONF_BYTE,
                                                            0x80, //STICK_CONF_DEFAULT,
                                                            0x10, //STICK_CONF_DEADZONE,
                                                            0x00, //STICK_CONF_INVERT,
                                                            0x00, //STICK_CONF_MIN,
                                                            0xFF};//STICK_CONF_MAX,



//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Switch Pro Controller
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

const u8 HID_SWITCH_PRO_BT_BUTTON_A[]             = { 0x00,(u8)((HID_SWITCH_PRO_BT_BUTTON_A_VALUE >> 24) & 0xFF)};
const u8 HID_SWITCH_PRO_BT_BUTTON_B[]             = { 0x00,(u8)((HID_SWITCH_PRO_BT_BUTTON_B_VALUE >> 24) & 0xFF)};
const u8 HID_SWITCH_PRO_BT_BUTTON_X[]             = { 0x00,(u8)((HID_SWITCH_PRO_BT_BUTTON_X_VALUE >> 24) & 0xFF)};
const u8 HID_SWITCH_PRO_BT_BUTTON_Y[]             = { 0x00,(u8)((HID_SWITCH_PRO_BT_BUTTON_Y_VALUE >> 24) & 0xFF)};

const u8 HID_SWITCH_PRO_BT_BUTTON_L[]             = { 0x00,(u8)((HID_SWITCH_PRO_BT_BUTTON_L_VALUE >> 24) & 0xFF)};
const u8 HID_SWITCH_PRO_BT_BUTTON_ZL[]            = { 0x00,(u8)((HID_SWITCH_PRO_BT_BUTTON_ZL_VALUE >> 24) & 0xFF)};
const u8 HID_SWITCH_PRO_BT_BUTTON_STICK_L[]       = { 0x01,(u8)((HID_SWITCH_PRO_BT_BUTTON_STICK_L_VALUE >> 16) & 0xFF)};

const u8 HID_SWITCH_PRO_BT_BUTTON_R[]             = { 0x00,(u8)((HID_SWITCH_PRO_BT_BUTTON_R_VALUE >> 24) & 0xFF)};
const u8 HID_SWITCH_PRO_BT_BUTTON_ZR[]            = { 0x00,(u8)((HID_SWITCH_PRO_BT_BUTTON_ZR_VALUE >> 24) & 0xFF)};
const u8 HID_SWITCH_PRO_BT_BUTTON_STICK_R[]       = { 0x01,(u8)((HID_SWITCH_PRO_BT_BUTTON_STICK_R_VALUE >> 16) & 0xFF)};

const u8 HID_SWITCH_PRO_BT_BUTTON_PLUS[]          = { 0x01,(u8)((HID_SWITCH_PRO_BT_BUTTON_PLUS_VALUE >> 16) & 0xFF)};
const u8 HID_SWITCH_PRO_BT_BUTTON_MINUS[]         = { 0x01,(u8)((HID_SWITCH_PRO_BT_BUTTON_MINUS_VALUE >> 16) & 0xFF)};
const u8 HID_SWITCH_PRO_BT_BUTTON_HOME[]          = { 0x01,(u8)((HID_SWITCH_PRO_BT_BUTTON_HOME_VALUE >> 16) & 0xFF)};

const u8 HID_SWITCH_PRO_BT_BUTTON_DPAD_TYPE[]     = { CONTRPDM_Hat,HID_SWITCH_PRO_BT_BUTTON_DPAD_MASK_VALUE};
const u8 HID_SWITCH_PRO_BT_BUTTON_DPAD_N[]        = { 0x02,HID_SWITCH_PRO_BT_BUTTON_DPAD_N_VALUE};
const u8 HID_SWITCH_PRO_BT_BUTTON_DPAD_NE[]       = { 0x02,HID_SWITCH_PRO_BT_BUTTON_DPAD_NE_VALUE};
const u8 HID_SWITCH_PRO_BT_BUTTON_DPAD_E[]        = { 0x02,HID_SWITCH_PRO_BT_BUTTON_DPAD_E_VALUE};
const u8 HID_SWITCH_PRO_BT_BUTTON_DPAD_SE[]       = { 0x02,HID_SWITCH_PRO_BT_BUTTON_DPAD_SE_VALUE};
const u8 HID_SWITCH_PRO_BT_BUTTON_DPAD_S[]        = { 0x02,HID_SWITCH_PRO_BT_BUTTON_DPAD_S_VALUE};
const u8 HID_SWITCH_PRO_BT_BUTTON_DPAD_SW[]       = { 0x02,HID_SWITCH_PRO_BT_BUTTON_DPAD_SW_VALUE};
const u8 HID_SWITCH_PRO_BT_BUTTON_DPAD_W[]        = { 0x02,HID_SWITCH_PRO_BT_BUTTON_DPAD_W_VALUE};
const u8 HID_SWITCH_PRO_BT_BUTTON_DPAD_NW[]       = { 0x02,HID_SWITCH_PRO_BT_BUTTON_DPAD_NW_VALUE};
const u8 HID_SWITCH_PRO_BT_BUTTON_DPAD_NEUTRAL[]  = { 0x02,HID_SWITCH_PRO_BT_BUTTON_DPAD_NEUTRAL_VALUE};


const u8 HID_SWITCH_PRO_BT_STICK_L_X[STICK_CONF_ENUM_MAXVALUE] =  {   STICK_CONF_MAGIC_VALUE, //STICK_CONF_MAGIC_VERSION
                                                            0x04, //STICK_CONF_BYTE,
                                                            0x80, //STICK_CONF_DEFAULT,
                                                            0x01, //STICK_CONF_DEADZONE,
                                                            0x00, //STICK_CONF_INVERT,
                                                            0x28, //STICK_CONF_MIN,
                                                            0xDF};//STICK_CONF_MAX,

const u8 HID_SWITCH_PRO_BT_STICK_L_Y[STICK_CONF_ENUM_MAXVALUE] =  {   STICK_CONF_MAGIC_VALUE, //STICK_CONF_MAGIC_VERSION
                                                            0x06, //STICK_CONF_BYTE,
                                                            0x80, //STICK_CONF_DEFAULT,
                                                            0x06, //STICK_CONF_DEADZONE,
                                                            0x01, //STICK_CONF_INVERT,
                                                            0x16, //STICK_CONF_MIN,
                                                            0xD7};//STICK_CONF_MAX,

const u8 HID_SWITCH_PRO_BT_STICK_R_X[STICK_CONF_ENUM_MAXVALUE] =  {   STICK_CONF_MAGIC_VALUE, //STICK_CONF_MAGIC_VERSION
                                                            0x08, //STICK_CONF_BYTE,
                                                            0x80, //STICK_CONF_DEFAULT,
                                                            0x04, //STICK_CONF_DEADZONE,
                                                            0x00, //STICK_CONF_INVERT,
                                                            0x29, //STICK_CONF_MIN,
                                                            0xE2};//STICK_CONF_MAX,

const u8 HID_SWITCH_PRO_BT_STICK_R_Y[STICK_CONF_ENUM_MAXVALUE] =  {   STICK_CONF_MAGIC_VALUE, //STICK_CONF_MAGIC_VERSION
                                                            0x0A, //STICK_CONF_BYTE,
                                                            0x80, //STICK_CONF_DEFAULT,
                                                            0x08, //STICK_CONF_DEADZONE,
                                                            0x01, //STICK_CONF_INVERT,
                                                            0x22, //STICK_CONF_MIN,
                                                            0xE4};//STICK_CONF_MAX,
