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

/**
 * @file ControllerPatcherDefs.h
 * @author Maschell
 * @date 30 Mar 2017
 * \brief This files contain all definitions for the ControllerPatcher engine
 *
 * @see https://github.com/Maschell/controller_patcher
 */

#ifndef _CONTROLLER_PATCHER_DEFS_H_
#define _CONTROLLER_PATCHER_DEFS_H_

#include <wiiu/types.h>

#define FIRST_INSTRUCTION_IN_SAMPLING_CALLBACK 0x9421FFB8

#define HID_INIT_NOT_DONE   0
#define HID_INIT_DONE       1
#define HID_SDCARD_READ     2

#define gHIDMaxDevices 32
#define HID_MAX_DATA_LENGTH_PER_PAD             16
#define HID_MAX_PADS_COUNT                      5
#define HID_MAX_DEVICES_PER_SLOT                2

#define NETWORK_CONTROLLER_VID      0
#define NETWORK_CONTROLLER_PID      1
#define NETWORK_CONTROLLER_ACTIVE   2
#define NETWORK_CONTROLLER_HANDLE   3


#define CONTROLLER_PATCHER_VALUE_SET            0x01
#define CONTROLLER_PATCHER_GC_DOUBLE_USE        0x01
#define CONTROLLER_PATCHER_INVALIDVALUE         0xFF

#define HID_INVALID_SLOT    0xFFFF
#define HID_INVALID_HIDMASK 0xFFFFFFFF

typedef int CONTROLLER_PATCHER_RESULT_OR_ERROR;

#define CONTROLLER_PATCHER_ERROR_NONE                       0
#define CONTROLLER_PATCHER_ERROR_INVALID_CHAN               -1
#define CONTROLLER_PATCHER_ERROR_UNKNOWN_VID_PID            -2
#define CONTROLLER_PATCHER_ERROR_FAILED_TO_GET_HIDDATA      -3
#define CONTROLLER_PATCHER_ERROR_MAPPING_DISABLED           -4
#define CONTROLLER_PATCHER_ERROR_INVALID_BUFFER             -5
#define CONTROLLER_PATCHER_ERROR_HID_NOT_CONNECTED          -6
#define CONTROLLER_PATCHER_ERROR_NO_PAD_CONNECTED           -7
#define CONTROLLER_PATCHER_ERROR_DEVICE_SLOT_NOT_FOUND      -8
#define CONTROLLER_PATCHER_ERROR_NULL_POINTER               -9
#define CONTROLLER_PATCHER_ERROR_CONFIG_NOT_DONE            -10
#define CONTROLLER_PATCHER_ERROR_NO_FREE_SLOT               -11
#define CONTROLLER_PATCHER_ERROR_UNKNOWN                    -50

#define PRO_CONTROLLER_MODE_KPADDATA        0
#define PRO_CONTROLLER_MODE_WPADReadData    1

#define STICK_VALUE_UP          1 << 1
#define STICK_VALUE_DOWN        1 << 2
#define STICK_VALUE_LEFT        1 << 3
#define STICK_VALUE_RIGHT       1 << 4

/**
 *  @brief The enumeration of Controller sticks defines
 */
enum Controller_Stick_Defines
{
    STICK_CONF_MAGIC_VERSION,   /**< Version of the stick configuration. Changes with every format*/
    STICK_CONF_BYTE,            /**< Byte where the stick-axis data is stored*/
    STICK_CONF_DEFAULT,         /**< Default value*/
    STICK_CONF_DEADZONE,        /**< Size of the deadzone */
    STICK_CONF_INVERT,          /**< Is 1 when the axis is inverted */
    STICK_CONF_MIN,             /**< Value that represent the minimum value (-1.0f)*/
    STICK_CONF_MAX,             /**< Value that represent the maximum value (1.0f) */
    STICK_CONF_ENUM_MAXVALUE    /**< Maxmimum enum value for iteration*/
};

#define STICK_CONF_MAGIC_VALUE 0xF0 // When you change the enum above, Dont forget to change the magic version!!!!

//! most data has the format: byte,value (byte starting at 0)
enum Controller_Patcher_Settings
{
    CONTRPS_VID,                          //! pid: 0x451d would be 0x45,0x1d
    CONTRPS_PID,                          //! vid: 0x488d would be 0x48,0x8d
    CONTRPS_BUF_SIZE,                     //! To set: CONTROLLER_PATCHER_VALUE_SET, BUF_SIZE (default is 64)
    CONTRPS_VPAD_BUTTON_A,
    CONTRPS_VPAD_BUTTON_B,
    CONTRPS_VPAD_BUTTON_X,
    CONTRPS_VPAD_BUTTON_Y,
    CONTRPS_DPAD_MODE,                     //! To set mode: CONTROLLER_PATCHER_VALUE_SET, Controller_Patcher_DPAD_MODE (default is normal mode)
    CONTRPS_DPAD_MASK,                     //! Mask needed for hat mode: CONTROLLER_PATCHER_VALUE_SET, mask
    /* Normal DPAD */
    CONTRPS_VPAD_BUTTON_LEFT,
    CONTRPS_VPAD_BUTTON_RIGHT,
    CONTRPS_VPAD_BUTTON_UP,
    CONTRPS_VPAD_BUTTON_DOWN,
    /* DPAD hat mode */
    CONTRPS_VPAD_BUTTON_DPAD_N,
    CONTRPS_VPAD_BUTTON_DPAD_NE,
    CONTRPS_VPAD_BUTTON_DPAD_E,
    CONTRPS_VPAD_BUTTON_DPAD_SE,
    CONTRPS_VPAD_BUTTON_DPAD_S,
    CONTRPS_VPAD_BUTTON_DPAD_SW,
    CONTRPS_VPAD_BUTTON_DPAD_W,
    CONTRPS_VPAD_BUTTON_DPAD_NW,
    CONTRPS_VPAD_BUTTON_DPAD_NEUTRAL,
    /* DPAD Absolute mode */
    CONTRPS_VPAD_BUTTON_DPAD_ABS_UP,
    CONTRPS_VPAD_BUTTON_DPAD_ABS_DOWN,
    CONTRPS_VPAD_BUTTON_DPAD_ABS_LEFT,
    CONTRPS_VPAD_BUTTON_DPAD_ABS_RIGHT,
    /* */
    CONTRPS_VPAD_BUTTON_ZL,
    CONTRPS_VPAD_BUTTON_ZR,
    CONTRPS_VPAD_BUTTON_L,
    CONTRPS_VPAD_BUTTON_R,
    CONTRPS_VPAD_BUTTON_PLUS,
    CONTRPS_VPAD_BUTTON_MINUS,
    CONTRPS_VPAD_BUTTON_HOME,
    CONTRPS_VPAD_BUTTON_SYNC,
    CONTRPS_VPAD_BUTTON_STICK_R,
    CONTRPS_VPAD_BUTTON_STICK_L,

    CONTRPS_VPAD_STICK_R_EMULATION_LEFT,
    CONTRPS_VPAD_STICK_R_EMULATION_RIGHT,
    CONTRPS_VPAD_STICK_R_EMULATION_UP,
    CONTRPS_VPAD_STICK_R_EMULATION_DOWN,
    CONTRPS_VPAD_STICK_L_EMULATION_LEFT,
    CONTRPS_VPAD_STICK_L_EMULATION_RIGHT,
    CONTRPS_VPAD_STICK_L_EMULATION_UP,
    CONTRPS_VPAD_STICK_L_EMULATION_DOWN,

    CONTRPS_VPAD_BUTTON_L_STICK_X,          //! byte, default value
    CONTRPS_VPAD_BUTTON_L_STICK_X_INVERT,   //! To invert: CONTROLLER_PATCHER_VALUE_SET, 0x01
    CONTRPS_VPAD_BUTTON_L_STICK_X_DEADZONE, //! Deadzone
    CONTRPS_VPAD_BUTTON_L_STICK_X_MINMAX,   //! min,max
    CONTRPS_VPAD_BUTTON_L_STICK_Y,          //! byte, default value
    CONTRPS_VPAD_BUTTON_L_STICK_Y_INVERT,   //! To invert: CONTROLLER_PATCHER_VALUE_SET, 0x01
    CONTRPS_VPAD_BUTTON_L_STICK_Y_DEADZONE, //! Deadzone
    CONTRPS_VPAD_BUTTON_L_STICK_Y_MINMAX,   //! min,max
    CONTRPS_VPAD_BUTTON_R_STICK_X,          //! byte, default value
    CONTRPS_VPAD_BUTTON_R_STICK_X_INVERT,   //! To invert: CONTROLLER_PATCHER_VALUE_SET, 0x01
    CONTRPS_VPAD_BUTTON_R_STICK_X_DEADZONE, //! Deadzone
    CONTRPS_VPAD_BUTTON_R_STICK_X_MINMAX,   //! min,max
    CONTRPS_VPAD_BUTTON_R_STICK_Y,          //! byte, default value
    CONTRPS_VPAD_BUTTON_R_STICK_Y_INVERT,   //! To invert: CONTROLLER_PATCHER_VALUE_SET, 0x01
    CONTRPS_VPAD_BUTTON_R_STICK_Y_DEADZONE, //! Deadzone
    CONTRPS_VPAD_BUTTON_R_STICK_Y_MINMAX,   //! min,max

    CONTRPS_VPAD_BUTTON_L_STICK_UP,
    CONTRPS_VPAD_BUTTON_L_STICK_DOWN,
    CONTRPS_VPAD_BUTTON_L_STICK_LEFT,
    CONTRPS_VPAD_BUTTON_L_STICK_RIGHT,
    CONTRPS_VPAD_BUTTON_R_STICK_UP,
    CONTRPS_VPAD_BUTTON_R_STICK_DOWN,
    CONTRPS_VPAD_BUTTON_R_STICK_LEFT,
    CONTRPS_VPAD_BUTTON_R_STICK_RIGHT,

    CONTRPS_VPAD_BUTTON_TV,
    CONTRPS_DOUBLE_USE,                     //!When used: e.g. CONTROLLER_PATCHER_VALUE_SET, CONTROLLER_PATCHER_GC_DOUBLE_USE
    CONTRPS_DOUBLE_USE_BUTTON_ACTIVATOR,
    CONTRPS_DOUBLE_USE_BUTTON_1_PRESSED,
    CONTRPS_DOUBLE_USE_BUTTON_2_PRESSED,
    CONTRPS_DOUBLE_USE_BUTTON_3_PRESSED,
    CONTRPS_DOUBLE_USE_BUTTON_4_PRESSED,
    CONTRPS_DOUBLE_USE_BUTTON_5_PRESSED,
    CONTRPS_DOUBLE_USE_BUTTON_1_RELEASED,
    CONTRPS_DOUBLE_USE_BUTTON_2_RELEASED,
    CONTRPS_DOUBLE_USE_BUTTON_3_RELEASED,
    CONTRPS_DOUBLE_USE_BUTTON_4_RELEASED,
    CONTRPS_DOUBLE_USE_BUTTON_5_RELEASED,
    CONTRPS_PAD_COUNT,                      //!
    CONTRPS_CONNECTED_PADS,                 //!
    CONTRPS_INPUT_FILTER,                   //!
    CONTRPS_PAD1_FILTER,                   //!
    CONTRPS_PAD2_FILTER,                   //!
    CONTRPS_PAD3_FILTER,                   //!
    CONTRPS_PAD4_FILTER,                   //!
    CONTRPS_PAD5_FILTER,                   //!
    CONTRPS_MOUSE_STICK,
    CONTRPS_MAX_VALUE
};
/**
 *  @brief The enumeration of different DPAD-Modes
 */
enum Controller_Patcher_DPAD_MODE
{
    CONTRPDM_Normal,            /**< Normal mode */
    CONTRPDM_Hat,               /**< Hat mode */
    CONTRPDM_Absolute_2Values,  /**< DPAD Value stored in 2 values (one for each axis), acting like a stick */
};
/**
 *  @brief The enumeration of DPAD Settings. Needed for saving both in the PADConst.
 */
enum Controller_Patcher_DPAD_Settings
{
    CONTRDPAD_MODE = 0, /**< Byte where the DPAD Mode is stored */
    CONTRDPAD_MASK = 1, /**< Byte where the DPAD Mask is stored */
};

/**
 *  @brief Stores data if the Slot the device is using in gHID_Devices
 */
typedef struct _HIDSlotData{
	u16 deviceslot;     /**< deviceslot number */
	u32 hidmask;        /**< Used HID-Mask */
}HIDSlotData;

/**
 *  @brief Stores a VID and PID
 */
typedef struct _DeviceVIDPIDInfo{
	u16 vid; /**< Vendor ID of this device */
	u16 pid; /**< Product ID of this device */
}DeviceVIDPIDInfo;

/**
 *  @brief Struct where the data for the callback funtion is stored
 */
typedef struct _my_cb_user{
	u8 *buf;  /**< pointer the buffer that is used */
	u32 transfersize; /**< number of transfered data */
	u32 handle; /**< HID handle */
    HIDSlotData slotdata; /**< Information about the deviceslot and hidmask */
	u32 pads_per_device; /**< Number of maximum pads of this device */
	u8 pad_slot; /**< number of the pad that will be used */
	u8 rumblestatus[HID_MAX_PADS_COUNT]; /**< Current status of the device rumble */
	u8 forceRumbleInTicks[HID_MAX_PADS_COUNT];
	DeviceVIDPIDInfo vidpid; /**< The VID/PID of the device */
}my_cb_user;

/**
 *  @brief Stores data for the mouse
 */
typedef struct _HID_Mouse_Data {
    u8 left_click; /**< Is 1 when the left mouse button is pressed */
    u8 right_click; /**< Is 1 when the right mouse button is pressed */
    s16 X; /**< X position of the cursor */
    s16 Y; /**< Y position of the cursor */
    s16 deltaX; /**< difference of the X value since the last call */
    s16 deltaY; /**< difference of the Y value since the last call */
    u8 valuedChanged; /**< Is 1 when the value has changed */
} HID_Mouse_Data;

/**
 *  @brief The enumeration of device types
 */
typedef enum DEVICE_TYPE_
{
    DEVICE_TYPE_CONTROLLER  = 0, /**< Normal Controller */
    DEVICE_TYPE_MOUSE = 1,       /**< Mouse */
}DEVICE_TYPE;

/**
 *  @brief Stores all data of the HID Device for accessing
 */
typedef struct _HID_Data {
    u32 handle;         /**< The HID-handle this device is using */
    u8 rumbleActive;    /**< 1 when rumble is active */
    u32 last_buttons;   /**< The last pressed buttons, based on VPAD_BUTTON_XXX data */
    union{
        struct{
            u8 cur_hid_data[HID_MAX_DATA_LENGTH_PER_PAD];   /**< Array where the current controller data is stored */
            u8 last_hid_data[HID_MAX_DATA_LENGTH_PER_PAD];  /**< Array where the last  controller data is stored */
        } controller; /**< Used when the device in a controller. Using u8 array where the raw data of the controller is placed. */
        struct{
            HID_Mouse_Data cur_mouse_data;  /**< Struct where the current mouse data is stored */
            HID_Mouse_Data last_mouse_data; /**< Struct where the last mouse data is stored */
        } mouse; /**< Used when the device in a mouse. Using a new struct to store the data. */
    }data_union; /**< The data union where the current and last data is stored.*/
    DEVICE_TYPE type;  /**< The device type*/
    HIDSlotData slotdata;  /**< Information about the deviceslot and his mask*/
    my_cb_user * user_data; /**< Pointer to the user data the read callback is using*/
} HID_Data;


/**
 *  @brief Struct where current hid data of one device type is stored
 */
typedef struct _HID_DEVICE_DATA {
    HID_Data pad_data[HID_MAX_PADS_COUNT];
} HID_DEVICE_DATA;

/**
 *  @brief Infos of the device
 */
typedef struct _DeviceInfo{
	HIDSlotData slotdata; /**< The slot used by this device */
	DeviceVIDPIDInfo vidpid; /**< The VID/PID of the device */
	u8  pad_count; /**< Number of maximum pads this device can have*/
}DeviceInfo;

/**
 *  @brief The enumeration of Controller-Mapping types
 */
typedef enum ControllerMapping_Type_Defines_{
    CM_Type_Controller = 0, /**< Device with single input */
    CM_Type_RealController = 1, /**< Real Pro Controller */
    CM_Type_Mouse = 2, /**< Mouse */
    CM_Type_Keyboard = 3, /**< Keyboard */
} ControllerMapping_Type_Defines;

/**
 *  @brief Infos of a mapped controller
 */
typedef struct _ControllerMappingPADInfo{
    u8 active; /**< Set to one if mapped */
    ControllerMapping_Type_Defines type; /**< Type of the controller mapping */
    DeviceVIDPIDInfo vidpid; /**< The VID/PID of the device */
    u8  pad; /**< Stores which pad it mapped */
}ControllerMappingPADInfo;

/**
 *  @brief Infos of a mapped controller
 */
typedef struct _ControllerMappingPAD{
    ControllerMappingPADInfo pad_infos[HID_MAX_DEVICES_PER_SLOT]; //lets limit this to HID_MAX_DEVICES_PER_SLOT.
    u8 useAll;
    u8 rumble; /**< Set when the controller should rumble */
}ControllerMappingPAD;

/**
 *  @brief Stores informations about all mapped controller
 */
typedef struct _ControllerMapping{
    ControllerMappingPAD gamepad; /**< Information about the gamepad mapping */
    ControllerMappingPAD proController[4]; /**< Information about the Pro Controller mapping */
}ControllerMapping;

/**
 *  @brief Pressed/Released/Down Button data.
 */
typedef struct _InputButtonData{
    u32 hold; /**< Buttons beeing hold */
    u32 trigger; /**< Buttons that started pressing */
    u32 release; /**< Buttons that were button released */
}InputButtonData;

typedef struct _InputStickData{
    f32 leftStickX;
    f32 leftStickY;
    f32 rightStickX;
    f32 rightStickY;
}InputStickData;

/**
 *  @brief Struct where the inputdata of a device for all HID_MAX_PADS_COUNT pads can be stored
 */
typedef struct _InputData{
    DeviceInfo device_info; /**< Infos about the device where the data is coming from */
    u8 status;
    InputButtonData button_data;
    InputStickData stick_data;
}InputData;

/**
 *  @brief The enumeration of WiiU Controller types
 */
enum UController_Type{
    UController_Type_Gamepad,
    UController_Type_Pro1,
    UController_Type_Pro2,
    UController_Type_Pro3,
    UController_Type_Pro4,
};

#define UController_Type_Gamepad_Name gettext("GamePad")
#define UController_Type_Pro1_Name gettext("Pro Controller 1")
#define UController_Type_Pro2_Name gettext("Pro Controller 2")
#define UController_Type_Pro3_Name gettext("Pro Controller 3")
#define UController_Type_Pro4_Name gettext("Pro Controller 4")

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 * VID/PID values
 *---------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/


#define HID_GC_VID                            0x057e
#define HID_GC_PID                            0x0337

#define HID_KEYBOARD_VID                      0xAFFE
#define HID_KEYBOARD_PID                      0XAAAC

#define HID_MOUSE_VID                         0xAFFE
#define HID_MOUSE_PID                         0XAAAB

#define HID_DS3_VID                           0x054c
#define HID_DS3_PID                           0x0268

#define HID_DS4_VID                           0x054c
#define HID_DS4_PID                           0x05c4

#define HID_NEW_DS4_VID                       0x054c
#define HID_NEW_DS4_PID                       0x09CC

#define HID_XINPUT_VID                        0x7331
#define HID_XINPUT_PID                        0x1337

#define HID_SWITCH_PRO_VID                    0x057e
#define HID_SWITCH_PRO_PID                    0x2009

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 * GC Adapter
 *---------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

#define HID_GC_BUTTON_A_VALUE                 0x01
#define HID_GC_BUTTON_B_VALUE                 0x02
#define HID_GC_BUTTON_X_VALUE                 0x04
#define HID_GC_BUTTON_Y_VALUE                 0x08
#define HID_GC_BUTTON_LEFT_VALUE              0x10
#define HID_GC_BUTTON_RIGHT_VALUE             0x20
#define HID_GC_BUTTON_DOWN_VALUE              0x40
#define HID_GC_BUTTON_UP_VALUE                0x80

#define HID_GC_BUTTON_START_VALUE             0x01
#define HID_GC_BUTTON_L_VALUE                 0x80
#define HID_GC_BUTTON_R_VALUE                 0x80
#define HID_GC_BUTTON_Z_VALUE                 0x02

#define HID_GC_PAD_COUNT                      4

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 * DS3
 *---------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

#define PS3_F4_REPORT_LEN                     4
#define PS3_F5_REPORT_LEN                     8
#define PS3_01_REPORT_LEN                     48
#define HID_REPORT_FEATURE                    3
#define HID_REPORT_OUTPUT                     2
#define PS3_F4_REPORT_ID                      0xF4
#define PS3_01_REPORT_ID                      0x01
#define PS3_F5_REPORT_ID                      0xF5

#define HID_DS3_BUTTON_CROSS_VALUE            0x40 // 3
#define HID_DS3_BUTTON_CIRCLE_VALUE           0x20 // 3
#define HID_DS3_BUTTON_SQUARE_VALUE           0x80 // 3
#define HID_DS3_BUTTON_TRIANGLE_VALUE         0x10 // 3
#define HID_DS3_BUTTON_L1_VALUE               0x04 // 3
#define HID_DS3_BUTTON_L2_VALUE               0x01 // 3
#define HID_DS3_BUTTON_R1_VALUE               0x08 // 3
#define HID_DS3_BUTTON_R2_VALUE               0x02 // 3

#define HID_DS3_BUTTON_L3_VALUE               0x02 // 2
#define HID_DS3_BUTTON_R3_VALUE               0x04 // 2
#define HID_DS3_BUTTON_SELECT_VALUE           0x01 // 2
#define HID_DS3_BUTTON_START_VALUE            0x08 // 2
#define HID_DS3_BUTTON_LEFT_VALUE             0x80 // 2
#define HID_DS3_BUTTON_RIGHT_VALUE            0x20 // 2
#define HID_DS3_BUTTON_UP_VALUE               0x10 // 2
#define HID_DS3_BUTTON_DOWN_VALUE             0x40 // 2
#define HID_DS3_BUTTON_GUIDE_VALUE            0x01 // 4

#define HID_DS3_PAD_COUNT                     1

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 * DS4
 *---------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

#define HID_DS4_BUTTON_CROSS_VALUE            0x20 // 5
#define HID_DS4_BUTTON_SQUARE_VALUE           0x10 // 5
#define HID_DS4_BUTTON_CIRCLE_VALUE           0x40 // 5
#define HID_DS4_BUTTON_TRIANGLE_VALUE         0x80 // 5
#define HID_DS4_BUTTON_L1_VALUE               0x01 // 6
#define HID_DS4_BUTTON_L2_VALUE               0x04 // 6
#define HID_DS4_BUTTON_L3_VALUE               0x40 // 6
#define HID_DS4_BUTTON_R1_VALUE               0x02 // 6
#define HID_DS4_BUTTON_R2_VALUE               0x08 // 6
#define HID_DS4_BUTTON_R3_VALUE               0x80 // 6
#define HID_DS4_BUTTON_SHARE_VALUE            0x10 // 6
#define HID_DS4_BUTTON_OPTIONS_VALUE          0x20 // 6

#define HID_DS4_BUTTON_DPAD_MASK_VALUE        0x0F

#define HID_DS4_BUTTON_DPAD_N_VALUE           0x00 // 5
#define HID_DS4_BUTTON_DPAD_NE_VALUE          0x01 // 5
#define HID_DS4_BUTTON_DPAD_E_VALUE           0x02 // 5
#define HID_DS4_BUTTON_DPAD_SE_VALUE          0x03 // 5
#define HID_DS4_BUTTON_DPAD_S_VALUE           0x04 // 5
#define HID_DS4_BUTTON_DPAD_SW_VALUE          0x05 // 5
#define HID_DS4_BUTTON_DPAD_W_VALUE           0x06 // 5
#define HID_DS4_BUTTON_DPAD_NW_VALUE          0x07 // 5
#define HID_DS4_BUTTON_DPAD_NEUTRAL_VALUE     0x08 // 5

#define HID_DS4_BUTTON_GUIDE_VALUE            0x01 // 7
#define HID_DS4_BUTTON_T_PAD_CLICK_VALUE      0x02 // 7

#define HID_DS4_PAD_COUNT                     1

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 * XInput
 *---------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

#define HID_XINPUT_BUTTON_A_VALUE             0x01 // 7
#define HID_XINPUT_BUTTON_B_VALUE             0x02 // 7
#define HID_XINPUT_BUTTON_X_VALUE             0x04 // 7
#define HID_XINPUT_BUTTON_Y_VALUE             0x08 // 7

#define HID_XINPUT_BUTTON_START_VALUE         0x02 // 6
#define HID_XINPUT_BUTTON_BACK_VALUE          0x01 // 6
#define HID_XINPUT_BUTTON_GUIDE_VALUE         0x80 // 6

#define HID_XINPUT_BUTTON_LB_VALUE            0x04 // 6
#define HID_XINPUT_BUTTON_RB_VALUE            0x08 // 6

#define HID_XINPUT_BUTTON_L3_VALUE            0x10 // 6
#define HID_XINPUT_BUTTON_R3_VALUE            0x20 // 6

#define HID_XINPUT_BUTTON_LT_VALUE            0x80 // 4
#define HID_XINPUT_BUTTON_RT_VALUE            0x80 // 5

#define HID_XINPUT_BUTTON_DPAD_MASK_VALUE     0xF0
#define HID_XINPUT_BUTTON_LEFT_VALUE          0x10 // 7
#define HID_XINPUT_BUTTON_RIGHT_VALUE         0x40 // 7
#define HID_XINPUT_BUTTON_DOWN_VALUE          0x80 // 7
#define HID_XINPUT_BUTTON_UP_VALUE            0x20 // 7

#define HID_XINPUT_PAD_COUNT               1

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 * Switch Pro Controller
 *---------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

#define HID_SWITCH_PRO_USB_BUTTON_A_VALUE           0x08000000
#define HID_SWITCH_PRO_USB_BUTTON_B_VALUE           0x04000000
#define HID_SWITCH_PRO_USB_BUTTON_X_VALUE           0x02000000
#define HID_SWITCH_PRO_USB_BUTTON_Y_VALUE           0x01000000
#define HID_SWITCH_PRO_USB_BUTTON_PLUS_VALUE        0x00020000
#define HID_SWITCH_PRO_USB_BUTTON_MINUS_VALUE       0x00010000
#define HID_SWITCH_PRO_USB_BUTTON_HOME_VALUE        0x00100000
#define HID_SWITCH_PRO_USB_BUTTON_SCREENSHOT_VALUE  0x00200000
#define HID_SWITCH_PRO_USB_BUTTON_R_VALUE           0x40000000
#define HID_SWITCH_PRO_USB_BUTTON_ZR_VALUE          0x80000000
#define HID_SWITCH_PRO_USB_BUTTON_STICK_R_VALUE     0x00040000
#define HID_SWITCH_PRO_USB_BUTTON_L_VALUE           0x00004000
#define HID_SWITCH_PRO_USB_BUTTON_ZL_VALUE          0x00008000
#define HID_SWITCH_PRO_USB_BUTTON_STICK_L_VALUE     0x00080000

#define HID_SWITCH_PRO_USB_BUTTON_DPAD_MASK_VALUE   0x0F
#define HID_SWITCH_PRO_USB_BUTTON_LEFT_VALUE        0x08 // 2
#define HID_SWITCH_PRO_USB_BUTTON_RIGHT_VALUE       0x04 // 2
#define HID_SWITCH_PRO_USB_BUTTON_DOWN_VALUE        0x01 // 2
#define HID_SWITCH_PRO_USB_BUTTON_UP_VALUE          0x02 // 2

#define HID_SWITCH_PRO_BT_BUTTON_A_VALUE            0x02000000
#define HID_SWITCH_PRO_BT_BUTTON_B_VALUE            0x01000000
#define HID_SWITCH_PRO_BT_BUTTON_X_VALUE            0x08000000
#define HID_SWITCH_PRO_BT_BUTTON_Y_VALUE            0x04000000
#define HID_SWITCH_PRO_BT_BUTTON_PLUS_VALUE         0x00020000
#define HID_SWITCH_PRO_BT_BUTTON_MINUS_VALUE        0x00010000
#define HID_SWITCH_PRO_BT_BUTTON_HOME_VALUE         0x00100000

#define HID_SWITCH_PRO_BT_BUTTON_R_VALUE            0x20000000
#define HID_SWITCH_PRO_BT_BUTTON_ZR_VALUE           0x80000000
#define HID_SWITCH_PRO_BT_BUTTON_STICK_R_VALUE      0x00080000

#define HID_SWITCH_PRO_BT_BUTTON_L_VALUE            0x10000000
#define HID_SWITCH_PRO_BT_BUTTON_ZL_VALUE           0x40000000
#define HID_SWITCH_PRO_BT_BUTTON_STICK_L_VALUE      0x00040000


#define HID_SWITCH_PRO_BT_BUTTON_DPAD_MASK_VALUE    0x0F
#define HID_SWITCH_PRO_BT_BUTTON_DPAD_N_VALUE       0x00 // 2
#define HID_SWITCH_PRO_BT_BUTTON_DPAD_NE_VALUE      0x01 // 2
#define HID_SWITCH_PRO_BT_BUTTON_DPAD_E_VALUE       0x02 // 2
#define HID_SWITCH_PRO_BT_BUTTON_DPAD_SE_VALUE      0x03 // 2
#define HID_SWITCH_PRO_BT_BUTTON_DPAD_S_VALUE       0x04 // 2
#define HID_SWITCH_PRO_BT_BUTTON_DPAD_SW_VALUE      0x05 // 2
#define HID_SWITCH_PRO_BT_BUTTON_DPAD_W_VALUE       0x06 // 2
#define HID_SWITCH_PRO_BT_BUTTON_DPAD_NW_VALUE      0x07 // 2
#define HID_SWITCH_PRO_BT_BUTTON_DPAD_NEUTRAL_VALUE 0x08 // 2


#define HID_SWITCH_PRO_BT_PAD_COUNT                 1

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 * Keyboard (Full list is on: http://www.freebsddiary.org/APC/usb_hid_usages.php)
 *---------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

#define HID_KEYBOARD_BUTTON_SHIFT             0x02

#define HID_KEYBOARD_BUTTON_A                 0x04
#define HID_KEYBOARD_BUTTON_B                 0x05
#define HID_KEYBOARD_BUTTON_C                 0x06
#define HID_KEYBOARD_BUTTON_D                 0x07
#define HID_KEYBOARD_BUTTON_E                 0x08
#define HID_KEYBOARD_BUTTON_F                 0x09
#define HID_KEYBOARD_BUTTON_G                 0x0A
#define HID_KEYBOARD_BUTTON_H                 0x0B
#define HID_KEYBOARD_BUTTON_I                 0x0C
#define HID_KEYBOARD_BUTTON_J                 0x0D
#define HID_KEYBOARD_BUTTON_K                 0x0E
#define HID_KEYBOARD_BUTTON_L                 0x0F
#define HID_KEYBOARD_BUTTON_M                 0x10
#define HID_KEYBOARD_BUTTON_N                 0x11
#define HID_KEYBOARD_BUTTON_O                 0x12
#define HID_KEYBOARD_BUTTON_P                 0x13
#define HID_KEYBOARD_BUTTON_Q                 0x14
#define HID_KEYBOARD_BUTTON_R                 0x15
#define HID_KEYBOARD_BUTTON_S                 0x16
#define HID_KEYBOARD_BUTTON_T                 0x17
#define HID_KEYBOARD_BUTTON_U                 0x18
#define HID_KEYBOARD_BUTTON_V                 0x19
#define HID_KEYBOARD_BUTTON_W                 0x1A
#define HID_KEYBOARD_BUTTON_X                 0x1B
#define HID_KEYBOARD_BUTTON_Y                 0x1C
#define HID_KEYBOARD_BUTTON_Z                 0x1D
#define HID_KEYBOARD_BUTTON_F1                0x3A
#define HID_KEYBOARD_BUTTON_F2                0x3B
#define HID_KEYBOARD_BUTTON_F3                0x3C
#define HID_KEYBOARD_BUTTON_F4                0x3D
#define HID_KEYBOARD_BUTTON_F5                0x3E
#define HID_KEYBOARD_BUTTON_F6                0x3F
#define HID_KEYBOARD_BUTTON_F7                0x40
#define HID_KEYBOARD_BUTTON_F8                0x41
#define HID_KEYBOARD_BUTTON_F9                0x42
#define HID_KEYBOARD_BUTTON_F10               0x43
#define HID_KEYBOARD_BUTTON_F11               0x44
#define HID_KEYBOARD_BUTTON_F12               0x45
#define HID_KEYBOARD_BUTTON_1                 0x1E
#define HID_KEYBOARD_BUTTON_2                 0x1F
#define HID_KEYBOARD_BUTTON_3                 0x20
#define HID_KEYBOARD_BUTTON_4                 0x21
#define HID_KEYBOARD_BUTTON_5                 0x22
#define HID_KEYBOARD_BUTTON_6                 0x23
#define HID_KEYBOARD_BUTTON_7                 0x24
#define HID_KEYBOARD_BUTTON_8                 0x25
#define HID_KEYBOARD_BUTTON_9                 0x26
#define HID_KEYBOARD_BUTTON_0                 0x27

#define HID_KEYBOARD_BUTTON_RETURN            0x28
#define HID_KEYBOARD_BUTTON_ESCAPE            0x29
#define HID_KEYBOARD_BUTTON_DELETE            0x2A
#define HID_KEYBOARD_BUTTON_TAB               0x2B
#define HID_KEYBOARD_BUTTON_SPACEBAR          0x2C
#define HID_KEYBOARD_BUTTON_CAPSLOCK          0x39
#define HID_KEYBOARD_BUTTON_PRINTSCREEN       0x46
#define HID_KEYBOARD_BUTTON_SCROLLLOCK        0x47
#define HID_KEYBOARD_BUTTON_PAUSE             0x48
#define HID_KEYBOARD_BUTTON_INSERT            0x49
#define HID_KEYBOARD_BUTTON_HOME              0x4A
#define HID_KEYBOARD_BUTTON_PAGEUP            0x4B
#define HID_KEYBOARD_BUTTON_PAGEDOWN          0x4E
#define HID_KEYBOARD_BUTTON_DELETEFORWARD     0x4C
#define HID_KEYBOARD_BUTTON_END               0x4D
#define HID_KEYBOARD_BUTTON_LEFT_CONTROL      0xE0
#define HID_KEYBOARD_BUTTON_LEFT_ALT          0xE2
#define HID_KEYBOARD_BUTTON_RIGHT_CONTROL     0xE4
#define HID_KEYBOARD_BUTTON_RIGHT_SHIFT       0xE5
#define HID_KEYBOARD_BUTTON_RIGHT_ALT         0xE6

#define HID_KEYBOARD_BUTTON_LEFT              0x50
#define HID_KEYBOARD_BUTTON_RIGHT             0x4f
#define HID_KEYBOARD_BUTTON_DOWN              0x51
#define HID_KEYBOARD_BUTTON_UP                0x52

#define HID_KEYBOARD_KEYPAD_BUTTON_1          0x59
#define HID_KEYBOARD_KEYPAD_BUTTON_2          0x5A
#define HID_KEYBOARD_KEYPAD_BUTTON_3          0x5B
#define HID_KEYBOARD_KEYPAD_BUTTON_4          0x5C
#define HID_KEYBOARD_KEYPAD_BUTTON_5          0x5D
#define HID_KEYBOARD_KEYPAD_BUTTON_6          0x5E
#define HID_KEYBOARD_KEYPAD_BUTTON_7          0x5F
#define HID_KEYBOARD_KEYPAD_BUTTON_8          0x60
#define HID_KEYBOARD_KEYPAD_BUTTON_9          0x61
#define HID_KEYBOARD_KEYPAD_BUTTON_0          0x62
#define HID_KEYBOARD_KEYPAD_BUTTON_NUMLOCK    0x53
#define HID_KEYBOARD_KEYPAD_BUTTON_MINUS      0x56
#define HID_KEYBOARD_KEYPAD_BUTTON_PLUS       0x57

#define HID_KEYBOARD_PAD_COUNT             1
#define HID_KEYBOARD_DATA_LENGTH           8

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 * Mouse
 *---------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

#define HID_MOUSE_BUTTON_LEFTCLICK                 0x04
#define HID_MOUSE_BUTTON_RIGHTCLICK                0x05

#define HID_MOUSE_PAD_COUNT               1

#define HID_MOUSE_MODE_AIM                0x01
#define HID_MOUSE_MODE_TOUCH              0x02

#endif /* _CONTROLLER_PATCHER_DEFS_H_ */
