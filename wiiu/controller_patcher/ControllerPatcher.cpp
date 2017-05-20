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
#include "ControllerPatcher.hpp"
#include <malloc.h>
#include <stdlib.h>

#include <string.h>
#include <stdio.h>
#include <vector>

#include "wiiu/sysapp.h"
#include "wiiu/syshid.h"
#include "sys/socket.h"
#include "wiiu/kpad.h"

#include "utils/CPRetainVars.hpp"

// This stores the holded buttons for the gamepad after the button remapping.
static u32 buttonRemapping_lastButtonsHold = 0;

/* To set the controls of an Pro Controler, we first get the result for an Gamepad and convert them later. This way both can share the same functions.*/

// This arrays stores the last hold buttons of the Pro Controllers. One u32 for each channel of the controllers
static u32 last_button_hold[4] = {0,0,0,0};
// This arrays stores the VPADStatus that will be used to get the HID Data for the Pro Controllers. One for each channel.
static VPADStatus myVPADBuffer[4];

void ControllerPatcher::InitButtonMapping(){
    if(HID_DEBUG){ printf("ControllerPatcher::InitButtonMapping(line %d): Init called \n",__LINE__); }
    if(!gButtonRemappingConfigDone){
        if(HID_DEBUG){ printf("ControllerPatcher::InitButtonMapping(line %d): Remapping is running! \n",__LINE__); }
        gButtonRemappingConfigDone = 1;
        memset(gGamePadValues,0,sizeof(gGamePadValues)); // Init / Invalid everything

        gGamePadValues[CONTRPS_VPAD_BUTTON_A] =                 VPAD_BUTTON_A;
        gGamePadValues[CONTRPS_VPAD_BUTTON_B] =                 VPAD_BUTTON_B;
        gGamePadValues[CONTRPS_VPAD_BUTTON_X] =                 VPAD_BUTTON_X;
        gGamePadValues[CONTRPS_VPAD_BUTTON_Y] =                 VPAD_BUTTON_Y;
        gGamePadValues[CONTRPS_VPAD_BUTTON_LEFT] =              VPAD_BUTTON_LEFT;
        gGamePadValues[CONTRPS_VPAD_BUTTON_RIGHT] =             VPAD_BUTTON_RIGHT;
        gGamePadValues[CONTRPS_VPAD_BUTTON_UP] =                VPAD_BUTTON_UP;
        gGamePadValues[CONTRPS_VPAD_BUTTON_DOWN] =              VPAD_BUTTON_DOWN;
        gGamePadValues[CONTRPS_VPAD_BUTTON_ZL] =                VPAD_BUTTON_ZL;
        gGamePadValues[CONTRPS_VPAD_BUTTON_ZR] =                VPAD_BUTTON_ZR;
        gGamePadValues[CONTRPS_VPAD_BUTTON_L] =                 VPAD_BUTTON_L;
        gGamePadValues[CONTRPS_VPAD_BUTTON_R] =                 VPAD_BUTTON_R;
        gGamePadValues[CONTRPS_VPAD_BUTTON_PLUS] =              VPAD_BUTTON_PLUS;
        gGamePadValues[CONTRPS_VPAD_BUTTON_MINUS] =             VPAD_BUTTON_MINUS;
        gGamePadValues[CONTRPS_VPAD_BUTTON_HOME] =              VPAD_BUTTON_HOME;
        gGamePadValues[CONTRPS_VPAD_BUTTON_SYNC] =              VPAD_BUTTON_SYNC;
        gGamePadValues[CONTRPS_VPAD_BUTTON_STICK_R] =           VPAD_BUTTON_STICK_R;
        gGamePadValues[CONTRPS_VPAD_BUTTON_STICK_L] =           VPAD_BUTTON_STICK_L;
        gGamePadValues[CONTRPS_VPAD_BUTTON_TV] =                VPAD_BUTTON_TV;

        gGamePadValues[CONTRPS_VPAD_STICK_R_EMULATION_LEFT] =   VPAD_STICK_R_EMULATION_LEFT;
        gGamePadValues[CONTRPS_VPAD_STICK_R_EMULATION_RIGHT] =  VPAD_STICK_R_EMULATION_RIGHT;
        gGamePadValues[CONTRPS_VPAD_STICK_R_EMULATION_UP] =     VPAD_STICK_R_EMULATION_UP;
        gGamePadValues[CONTRPS_VPAD_STICK_R_EMULATION_DOWN] =   VPAD_STICK_R_EMULATION_DOWN;
        gGamePadValues[CONTRPS_VPAD_STICK_L_EMULATION_LEFT] =   VPAD_STICK_L_EMULATION_LEFT;
        gGamePadValues[CONTRPS_VPAD_STICK_L_EMULATION_RIGHT] =  VPAD_STICK_L_EMULATION_RIGHT;
        gGamePadValues[CONTRPS_VPAD_STICK_L_EMULATION_UP] =     VPAD_STICK_L_EMULATION_UP;
        gGamePadValues[CONTRPS_VPAD_STICK_L_EMULATION_DOWN] =   VPAD_STICK_L_EMULATION_DOWN;
    }
}

void ControllerPatcher::ResetConfig(){
    memset(connectionOrderHelper,0,sizeof(connectionOrderHelper));
    memset(&gControllerMapping,0,sizeof(gControllerMapping)); // Init / Invalid everything
    memset(config_controller,CONTROLLER_PATCHER_INVALIDVALUE,sizeof(config_controller)); // Init / Invalid everything
    memset(config_controller_hidmask,0,sizeof(config_controller_hidmask)); // Init / Invalid everything
    memset(gNetworkController,0,sizeof(gNetworkController)); // Init / Invalid everything
    memset(gHID_Devices,0,sizeof(gHID_Devices)); // Init / Invalid everything

    memset(gWPADConnectCallback,0,sizeof(gWPADConnectCallback));
    memset(gKPADConnectCallback,0,sizeof(gKPADConnectCallback));
    memset(gExtensionCallback,0,sizeof(gExtensionCallback));
    gCallbackCooldown = 0;

    gHID_Mouse_Mode = HID_MOUSE_MODE_AIM;
    gHID_LIST_GC = 0;
    gHID_LIST_DS3 = 0;
    gHID_LIST_DS4 = 0;
    gHID_LIST_KEYBOARD = 0;
    gHID_LIST_MOUSE = 0;
    gGamePadSlot = 0;
    gHID_SLOT_GC = 0;
    gHID_SLOT_KEYBOARD = 0;
    gMouseSlot = 0;

    HIDSlotData slotdata;
    gHIDRegisteredDevices = 0;
    ControllerPatcherUtils::getNextSlotData(&slotdata);
    gGamePadSlot = slotdata.deviceslot;
    if(HID_DEBUG){ printf("ControllerPatcher::ResetConfig(line %d): Register Gamepad-Config. HID-Mask %s Device-Slot: %d\n",__LINE__,CPStringTools::byte_to_binary(slotdata.hidmask),gGamePadSlot); }

    ControllerPatcherUtils::getNextSlotData(&slotdata);
    gMouseSlot = slotdata.deviceslot;
    gHID_LIST_MOUSE = slotdata.hidmask;
    if(HID_DEBUG){ printf("ControllerPatcher::ResetConfig(line %d): Register Mouse-Config. HID-Mask %s Device-Slot: %d\n",__LINE__,CPStringTools::byte_to_binary(gHID_LIST_MOUSE),gMouseSlot); }

    ControllerPatcherUtils::getNextSlotData(&slotdata);
    u32 keyboard_slot = slotdata.deviceslot;
    u32 keyboard_hid = slotdata.hidmask;
    gHID_LIST_KEYBOARD = keyboard_hid;
    gHID_SLOT_KEYBOARD = keyboard_slot;

    if(HID_DEBUG){ printf("ControllerPatcher::ResetConfig(line %d): Register Keyboard-Config. HID-Mask %s Device-Slot: %d\n",__LINE__,CPStringTools::byte_to_binary(gHID_LIST_KEYBOARD),gHID_SLOT_KEYBOARD); }

    ControllerPatcherUtils::getNextSlotData(&slotdata);
    u32 gc_slot = slotdata.deviceslot;
    u32 gc_hid = slotdata.hidmask;
    gHID_LIST_GC = gc_hid;
    gHID_SLOT_GC = gc_slot;
    if(HID_DEBUG){ printf("ControllerPatcher::ResetConfig(line %d): Register GC-Adapter-Config. HID-Mask %s Device-Slot: %d\n",__LINE__,CPStringTools::byte_to_binary(gHID_LIST_GC),gHID_SLOT_GC); }

    ControllerPatcherUtils::getNextSlotData(&slotdata);
    u32 ds3_slot = slotdata.deviceslot;
    u32 ds3_hid = slotdata.hidmask;
    gHID_LIST_DS3 = ds3_hid;
    if(HID_DEBUG){ printf("ControllerPatcher::ResetConfig(line %d): Register DS3-Config. HID-Mask %s Device-Slot: %d\n",__LINE__,CPStringTools::byte_to_binary(gHID_LIST_DS3),ds3_slot); }

    ControllerPatcherUtils::getNextSlotData(&slotdata);
    u32 ds4_slot = slotdata.deviceslot;
    u32 ds4_hid = slotdata.hidmask;
    gHID_LIST_DS4 = ds4_hid;
    if(HID_DEBUG){ printf("ControllerPatcher::ResetConfig(line %d): Register DS4-Config. HID-Mask %s Device-Slot: %d\n",__LINE__,CPStringTools::byte_to_binary(ds4_hid),ds4_slot); }

    ControllerPatcherUtils::getNextSlotData(&slotdata);
    u32 xinput_slot = slotdata.deviceslot;
    u32 xinput_hid = slotdata.hidmask;
    if(HID_DEBUG){ printf("ControllerPatcher::ResetConfig(line %d): Register XInput-Config. HID-Mask %s Device-Slot: %d\n",__LINE__,CPStringTools::byte_to_binary(xinput_hid),xinput_slot); }

    ControllerPatcherUtils::getNextSlotData(&slotdata);
    u32 switch_pro_slot = slotdata.deviceslot;
    gHID_LIST_SWITCH_PRO = slotdata.hidmask;
    if(HID_DEBUG){ printf("ControllerPatcher::ResetConfig(line %d): Register Switch-Pro-Config. HID-Mask %s Device-Slot: %d\n",__LINE__,CPStringTools::byte_to_binary(gHID_LIST_SWITCH_PRO),switch_pro_slot); }


    config_controller_hidmask[gc_slot] =                                                            gHID_LIST_GC;
    config_controller_hidmask[ds3_slot] =                                                           gHID_LIST_DS3;
    config_controller_hidmask[keyboard_slot] =                                                      gHID_LIST_KEYBOARD;
    config_controller_hidmask[gMouseSlot] =                                                         gHID_LIST_MOUSE;
    config_controller_hidmask[ds4_slot] =                                                           gHID_LIST_DS4;
    config_controller_hidmask[xinput_slot] =                                                        xinput_hid;
    config_controller_hidmask[switch_pro_slot] =                                                    gHID_LIST_SWITCH_PRO;

    /* We need to give the GamePad, Mouse and Keyboard a unique VID/PID to find the right slots.*/
    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //! GamePad
    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gGamePadSlot][CONTRPS_VID],                            0xAF,0xFE);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gGamePadSlot][CONTRPS_PID],                            0xAA,0xAA);

    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //! Switch Pro Controller
    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VID],                           (HID_SWITCH_PRO_VID>>8)&0xFF,                   HID_SWITCH_PRO_VID&0xFF);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_PID],                           (HID_SWITCH_PRO_PID>>8)&0xFF,                   HID_SWITCH_PRO_PID&0xFF);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_A],                 HID_SWITCH_PRO_BT_BUTTON_A[0],                  HID_SWITCH_PRO_BT_BUTTON_A[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_B],                 HID_SWITCH_PRO_BT_BUTTON_B[0],                  HID_SWITCH_PRO_BT_BUTTON_B[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_X],                 HID_SWITCH_PRO_BT_BUTTON_X[0],                  HID_SWITCH_PRO_BT_BUTTON_X[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_Y],                 HID_SWITCH_PRO_BT_BUTTON_Y[0],                  HID_SWITCH_PRO_BT_BUTTON_Y[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_DPAD_MODE],                     CONTROLLER_PATCHER_VALUE_SET,                   HID_SWITCH_PRO_BT_BUTTON_DPAD_TYPE[CONTRDPAD_MODE]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_DPAD_MASK],                     CONTROLLER_PATCHER_VALUE_SET,                   HID_SWITCH_PRO_BT_BUTTON_DPAD_TYPE[CONTRDPAD_MASK]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_DPAD_N],            HID_SWITCH_PRO_BT_BUTTON_DPAD_N[0],             HID_SWITCH_PRO_BT_BUTTON_DPAD_N[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_DPAD_NE],           HID_SWITCH_PRO_BT_BUTTON_DPAD_NE[0],            HID_SWITCH_PRO_BT_BUTTON_DPAD_NE[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_DPAD_E],            HID_SWITCH_PRO_BT_BUTTON_DPAD_E[0],             HID_SWITCH_PRO_BT_BUTTON_DPAD_E[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_DPAD_SE],           HID_SWITCH_PRO_BT_BUTTON_DPAD_SE[0],            HID_SWITCH_PRO_BT_BUTTON_DPAD_SE[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_DPAD_S],            HID_SWITCH_PRO_BT_BUTTON_DPAD_S[0],             HID_SWITCH_PRO_BT_BUTTON_DPAD_S[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_DPAD_SW],           HID_SWITCH_PRO_BT_BUTTON_DPAD_SW[0],            HID_SWITCH_PRO_BT_BUTTON_DPAD_SW[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_DPAD_W],            HID_SWITCH_PRO_BT_BUTTON_DPAD_W[0],             HID_SWITCH_PRO_BT_BUTTON_DPAD_W[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_DPAD_NW],           HID_SWITCH_PRO_BT_BUTTON_DPAD_NW[0],            HID_SWITCH_PRO_BT_BUTTON_DPAD_NW[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_DPAD_NEUTRAL],      HID_SWITCH_PRO_BT_BUTTON_DPAD_NEUTRAL[0],       HID_SWITCH_PRO_BT_BUTTON_DPAD_NEUTRAL[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_PLUS],              HID_SWITCH_PRO_BT_BUTTON_PLUS[0],               HID_SWITCH_PRO_BT_BUTTON_PLUS[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_MINUS],             HID_SWITCH_PRO_BT_BUTTON_MINUS[0],              HID_SWITCH_PRO_BT_BUTTON_MINUS[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_L],                 HID_SWITCH_PRO_BT_BUTTON_L[0],                  HID_SWITCH_PRO_BT_BUTTON_L[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_R],                 HID_SWITCH_PRO_BT_BUTTON_R[0],                  HID_SWITCH_PRO_BT_BUTTON_R[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_ZL],                HID_SWITCH_PRO_BT_BUTTON_ZL[0],                 HID_SWITCH_PRO_BT_BUTTON_ZL[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_ZR],                HID_SWITCH_PRO_BT_BUTTON_ZR[0],                 HID_SWITCH_PRO_BT_BUTTON_ZR[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_STICK_L],           HID_SWITCH_PRO_BT_BUTTON_STICK_L[0],            HID_SWITCH_PRO_BT_BUTTON_STICK_L[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_STICK_R],           HID_SWITCH_PRO_BT_BUTTON_STICK_R[0],            HID_SWITCH_PRO_BT_BUTTON_STICK_R[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_HOME],              HID_SWITCH_PRO_BT_BUTTON_HOME[0],               HID_SWITCH_PRO_BT_BUTTON_HOME[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_PAD_COUNT],                     CONTROLLER_PATCHER_VALUE_SET,                   HID_SWITCH_PRO_BT_PAD_COUNT);

    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_L_STICK_X],          HID_SWITCH_PRO_BT_STICK_L_X[STICK_CONF_BYTE],  HID_SWITCH_PRO_BT_STICK_L_X[STICK_CONF_DEFAULT]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_L_STICK_X_DEADZONE], CONTROLLER_PATCHER_VALUE_SET,                  HID_SWITCH_PRO_BT_STICK_L_X[STICK_CONF_DEADZONE]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_L_STICK_X_MINMAX],   HID_SWITCH_PRO_BT_STICK_L_X[STICK_CONF_MIN],   HID_SWITCH_PRO_BT_STICK_L_X[STICK_CONF_MAX]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_L_STICK_X_INVERT],   CONTROLLER_PATCHER_VALUE_SET,                  HID_SWITCH_PRO_BT_STICK_L_X[STICK_CONF_INVERT]);

    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_L_STICK_Y],          HID_SWITCH_PRO_BT_STICK_L_Y[STICK_CONF_BYTE],  HID_SWITCH_PRO_BT_STICK_L_Y[STICK_CONF_DEFAULT]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_L_STICK_Y_DEADZONE], CONTROLLER_PATCHER_VALUE_SET,                  HID_SWITCH_PRO_BT_STICK_L_Y[STICK_CONF_DEADZONE]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_L_STICK_Y_MINMAX],   HID_SWITCH_PRO_BT_STICK_L_Y[STICK_CONF_MIN],   HID_SWITCH_PRO_BT_STICK_L_Y[STICK_CONF_MAX]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_L_STICK_Y_INVERT],   CONTROLLER_PATCHER_VALUE_SET,                  HID_SWITCH_PRO_BT_STICK_L_Y[STICK_CONF_INVERT]);

    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_R_STICK_X],          HID_SWITCH_PRO_BT_STICK_R_X[STICK_CONF_BYTE],  HID_SWITCH_PRO_BT_STICK_R_X[STICK_CONF_DEFAULT]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_R_STICK_X_DEADZONE], CONTROLLER_PATCHER_VALUE_SET,                  HID_SWITCH_PRO_BT_STICK_R_X[STICK_CONF_DEADZONE]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_R_STICK_X_MINMAX],   HID_SWITCH_PRO_BT_STICK_R_X[STICK_CONF_MIN],   HID_SWITCH_PRO_BT_STICK_R_X[STICK_CONF_MAX]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_R_STICK_X_INVERT],   CONTROLLER_PATCHER_VALUE_SET,                  HID_SWITCH_PRO_BT_STICK_R_X[STICK_CONF_INVERT]);

    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_R_STICK_Y],          HID_SWITCH_PRO_BT_STICK_R_Y[STICK_CONF_BYTE],  HID_SWITCH_PRO_BT_STICK_R_Y[STICK_CONF_DEFAULT]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_R_STICK_Y_DEADZONE], CONTROLLER_PATCHER_VALUE_SET,                  HID_SWITCH_PRO_BT_STICK_R_Y[STICK_CONF_DEADZONE]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_R_STICK_Y_MINMAX],   HID_SWITCH_PRO_BT_STICK_R_Y[STICK_CONF_MIN],   HID_SWITCH_PRO_BT_STICK_R_Y[STICK_CONF_MAX]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[switch_pro_slot][CONTRPS_VPAD_BUTTON_R_STICK_Y_INVERT],   CONTROLLER_PATCHER_VALUE_SET,                  HID_SWITCH_PRO_BT_STICK_R_Y[STICK_CONF_INVERT]);

    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //! Mouse
    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gMouseSlot][CONTRPS_VID],                              (HID_MOUSE_VID>>8)&0xFF,      HID_MOUSE_VID&0xFF);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gMouseSlot][CONTRPS_PID],                              (HID_MOUSE_PID>>8)&0xFF,      HID_MOUSE_PID&0xFF);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gMouseSlot][CONTRPS_VPAD_BUTTON_LEFT],                 CONTROLLER_PATCHER_VALUE_SET, CONTRPS_VPAD_BUTTON_ZR);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gMouseSlot][CONTRPS_VPAD_BUTTON_RIGHT],                CONTROLLER_PATCHER_VALUE_SET, CONTRPS_VPAD_BUTTON_R);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gMouseSlot][CONTRPS_PAD_COUNT],                        CONTROLLER_PATCHER_VALUE_SET, HID_MOUSE_PAD_COUNT);

    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //! Keyboard
    //!---------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_VID],                           (HID_KEYBOARD_VID>>8)&0xFF,  HID_KEYBOARD_VID&0xFF);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_PID],                           (HID_KEYBOARD_PID>>8)&0xFF,  HID_KEYBOARD_PID&0xFF);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_VPAD_BUTTON_A],                 0x00,                        HID_KEYBOARD_BUTTON_E);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_VPAD_BUTTON_B],                 0x00,                        HID_KEYBOARD_BUTTON_Q);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_VPAD_BUTTON_X],                 0x00,                        HID_KEYBOARD_BUTTON_SPACEBAR);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_VPAD_BUTTON_Y],                 0x00,                        HID_KEYBOARD_BUTTON_R);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_DPAD_MODE],                     CONTROLLER_PATCHER_VALUE_SET,CONTRPDM_Normal);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_VPAD_BUTTON_LEFT],              0x00,                        HID_KEYBOARD_BUTTON_LEFT);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_VPAD_BUTTON_RIGHT],             0x00,                        HID_KEYBOARD_BUTTON_RIGHT);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_VPAD_BUTTON_DOWN],              0x00,                        HID_KEYBOARD_BUTTON_DOWN);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_VPAD_BUTTON_UP],                0x00,                        HID_KEYBOARD_BUTTON_UP);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_VPAD_BUTTON_PLUS],              0x00,                        HID_KEYBOARD_BUTTON_RETURN);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_VPAD_BUTTON_MINUS],             0x00,                        HID_KEYBOARD_KEYPAD_BUTTON_MINUS);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_VPAD_BUTTON_L],                 0x00,                        HID_KEYBOARD_BUTTON_V);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_VPAD_BUTTON_R],                 0x00,                        HID_KEYBOARD_BUTTON_B);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_VPAD_BUTTON_ZL],                0x00,                        HID_KEYBOARD_BUTTON_SHIFT);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_VPAD_BUTTON_ZR],                0x00,                        HID_KEYBOARD_BUTTON_N);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_VPAD_BUTTON_STICK_L],           0x00,                        HID_KEYBOARD_BUTTON_F);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_VPAD_BUTTON_STICK_R],           0x00,                        HID_KEYBOARD_BUTTON_TAB);

    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_VPAD_BUTTON_L_STICK_UP],        0x00,                        HID_KEYBOARD_BUTTON_W);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_VPAD_BUTTON_L_STICK_DOWN],      0x00,                        HID_KEYBOARD_BUTTON_S);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_VPAD_BUTTON_L_STICK_LEFT],      0x00,                        HID_KEYBOARD_BUTTON_A);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_VPAD_BUTTON_L_STICK_RIGHT],     0x00,                        HID_KEYBOARD_BUTTON_D);

    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_VPAD_BUTTON_R_STICK_UP],        0x00,                        HID_KEYBOARD_KEYPAD_BUTTON_8);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_VPAD_BUTTON_R_STICK_DOWN],      0x00,                        HID_KEYBOARD_KEYPAD_BUTTON_2);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_VPAD_BUTTON_R_STICK_LEFT],      0x00,                        HID_KEYBOARD_KEYPAD_BUTTON_4);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_VPAD_BUTTON_R_STICK_RIGHT],     0x00,                        HID_KEYBOARD_KEYPAD_BUTTON_6);

    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_KEYBOARD][CONTRPS_PAD_COUNT],                     CONTROLLER_PATCHER_VALUE_SET,HID_KEYBOARD_PAD_COUNT);

    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //! GC-Adapter
    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VID],                            (HID_GC_VID>>8)&0xFF,             HID_GC_VID&0xFF);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_PID],                            (HID_GC_PID>>8)&0xFF,             HID_GC_PID&0xFF);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_A],                  HID_GC_BUTTON_A[0],               HID_GC_BUTTON_A[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_B],                  HID_GC_BUTTON_B[0],               HID_GC_BUTTON_B[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_X],                  HID_GC_BUTTON_X[0],               HID_GC_BUTTON_X[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_Y],                  HID_GC_BUTTON_Y[0],               HID_GC_BUTTON_Y[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_DPAD_MODE],                      CONTROLLER_PATCHER_VALUE_SET,     HID_GC_BUTTON_DPAD_TYPE[CONTRDPAD_MODE]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_DPAD_MASK],                      CONTROLLER_PATCHER_VALUE_SET,     HID_GC_BUTTON_DPAD_TYPE[CONTRDPAD_MASK]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_LEFT],               HID_GC_BUTTON_LEFT[0],            HID_GC_BUTTON_LEFT[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_RIGHT],              HID_GC_BUTTON_RIGHT[0],           HID_GC_BUTTON_RIGHT[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_DOWN],               HID_GC_BUTTON_DOWN[0],            HID_GC_BUTTON_DOWN[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_UP],                 HID_GC_BUTTON_UP[0],              HID_GC_BUTTON_UP[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_MINUS],              HID_GC_BUTTON_START[0],           HID_GC_BUTTON_START[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_L],                  HID_GC_BUTTON_L[0],               HID_GC_BUTTON_L[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_R],                  HID_GC_BUTTON_R[0],               HID_GC_BUTTON_R[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_PLUS],               HID_GC_BUTTON_START[0],           HID_GC_BUTTON_START[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_ZL],                 HID_GC_BUTTON_L[0],               HID_GC_BUTTON_L[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_ZR],                 HID_GC_BUTTON_R[0],               HID_GC_BUTTON_R[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_STICK_L],            HID_GC_BUTTON_A[0],               HID_GC_BUTTON_A[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_STICK_R],            HID_GC_BUTTON_B[0],               HID_GC_BUTTON_B[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_DOUBLE_USE],                     CONTROLLER_PATCHER_VALUE_SET,     CONTROLLER_PATCHER_GC_DOUBLE_USE);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_DOUBLE_USE_BUTTON_ACTIVATOR],    HID_GC_BUTTON_Z[0],               HID_GC_BUTTON_Z[1]);

    //Buttons that will be ignored when the CONTRPS_DOUBLE_USE_BUTTON_ACTIVATOR is pressed
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_DOUBLE_USE_BUTTON_1_PRESSED],    CONTROLLER_PATCHER_VALUE_SET,     CONTRPS_VPAD_BUTTON_MINUS);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_DOUBLE_USE_BUTTON_2_PRESSED],    CONTROLLER_PATCHER_VALUE_SET,     CONTRPS_VPAD_BUTTON_L);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_DOUBLE_USE_BUTTON_3_PRESSED],    CONTROLLER_PATCHER_VALUE_SET,     CONTRPS_VPAD_BUTTON_R);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_DOUBLE_USE_BUTTON_4_PRESSED],    CONTROLLER_PATCHER_VALUE_SET,     CONTRPS_VPAD_BUTTON_STICK_L);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_DOUBLE_USE_BUTTON_5_PRESSED],    CONTROLLER_PATCHER_VALUE_SET,     CONTRPS_VPAD_BUTTON_STICK_R);

     //Buttons that will be ignored when the CONTRPS_DOUBLE_USE_BUTTON_ACTIVATOR is released
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_DOUBLE_USE_BUTTON_1_RELEASED],   CONTROLLER_PATCHER_VALUE_SET,     CONTRPS_VPAD_BUTTON_PLUS);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_DOUBLE_USE_BUTTON_2_RELEASED],   CONTROLLER_PATCHER_VALUE_SET,     CONTRPS_VPAD_BUTTON_ZL);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_DOUBLE_USE_BUTTON_3_RELEASED],   CONTROLLER_PATCHER_VALUE_SET,     CONTRPS_VPAD_BUTTON_ZR);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_DOUBLE_USE_BUTTON_4_RELEASED],   CONTROLLER_PATCHER_VALUE_SET,     CONTRPS_VPAD_BUTTON_A);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_DOUBLE_USE_BUTTON_5_RELEASED],   CONTROLLER_PATCHER_VALUE_SET,     CONTRPS_VPAD_BUTTON_B);

    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_PAD_COUNT],                      CONTROLLER_PATCHER_VALUE_SET,     HID_GC_PAD_COUNT);

    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_L_STICK_X],          HID_GC_STICK_L_X[STICK_CONF_BYTE],HID_GC_STICK_L_X[STICK_CONF_DEFAULT]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_L_STICK_X_DEADZONE], CONTROLLER_PATCHER_VALUE_SET,     HID_GC_STICK_L_X[STICK_CONF_DEADZONE]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_L_STICK_X_MINMAX],   HID_GC_STICK_L_X[STICK_CONF_MIN], HID_GC_STICK_L_X[STICK_CONF_MAX]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_L_STICK_X_INVERT],   CONTROLLER_PATCHER_VALUE_SET,     HID_GC_STICK_L_X[STICK_CONF_INVERT]);

    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_L_STICK_Y],          HID_GC_STICK_L_Y[STICK_CONF_BYTE],HID_GC_STICK_L_Y[STICK_CONF_DEFAULT]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_L_STICK_Y_DEADZONE], CONTROLLER_PATCHER_VALUE_SET,     HID_GC_STICK_L_Y[STICK_CONF_DEADZONE]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_L_STICK_Y_MINMAX],   HID_GC_STICK_L_Y[STICK_CONF_MIN], HID_GC_STICK_L_Y[STICK_CONF_MAX]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_L_STICK_Y_INVERT],   CONTROLLER_PATCHER_VALUE_SET,     HID_GC_STICK_L_Y[STICK_CONF_INVERT]);

    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_R_STICK_X],          HID_GC_STICK_R_X[STICK_CONF_BYTE],HID_GC_STICK_R_X[STICK_CONF_DEFAULT]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_R_STICK_X_DEADZONE], CONTROLLER_PATCHER_VALUE_SET,     HID_GC_STICK_R_X[STICK_CONF_DEADZONE]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_R_STICK_X_MINMAX],   HID_GC_STICK_R_X[STICK_CONF_MIN], HID_GC_STICK_R_X[STICK_CONF_MAX]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_R_STICK_X_INVERT],   CONTROLLER_PATCHER_VALUE_SET,     HID_GC_STICK_R_X[STICK_CONF_INVERT]);

    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_R_STICK_Y],          HID_GC_STICK_R_Y[STICK_CONF_BYTE],HID_GC_STICK_R_Y[STICK_CONF_DEFAULT]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_R_STICK_Y_DEADZONE], CONTROLLER_PATCHER_VALUE_SET,     HID_GC_STICK_R_Y[STICK_CONF_DEADZONE]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_R_STICK_Y_MINMAX],   HID_GC_STICK_R_Y[STICK_CONF_MIN], HID_GC_STICK_R_Y[STICK_CONF_MAX]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[gHID_SLOT_GC][CONTRPS_VPAD_BUTTON_R_STICK_Y_INVERT],   CONTROLLER_PATCHER_VALUE_SET,     HID_GC_STICK_R_Y[STICK_CONF_INVERT]);

    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //! DS3
    //!---------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VID],                           (HID_DS3_VID>>8)&0xFF,                 HID_DS3_VID&0xFF);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_PID],                           (HID_DS3_PID>>8)&0xFF,                 HID_DS3_PID&0xFF);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_BUF_SIZE],                      CONTROLLER_PATCHER_VALUE_SET,          128);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_A],                 HID_DS3_BUTTON_CIRCLE[0],              HID_DS3_BUTTON_CIRCLE[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_B],                 HID_DS3_BUTTON_CROSS[0],               HID_DS3_BUTTON_CROSS[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_X],                 HID_DS3_BUTTON_TRIANGLE[0],            HID_DS3_BUTTON_TRIANGLE[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_Y],                 HID_DS3_BUTTON_SQUARE[0],              HID_DS3_BUTTON_SQUARE[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_DPAD_MODE],                     CONTROLLER_PATCHER_VALUE_SET,          HID_DS3_BUTTON_DPAD_TYPE[CONTRDPAD_MODE]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_DPAD_MASK],                     CONTROLLER_PATCHER_VALUE_SET,          HID_DS3_BUTTON_DPAD_TYPE[CONTRDPAD_MASK]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_LEFT],              HID_DS3_BUTTON_LEFT[0],                HID_DS3_BUTTON_LEFT[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_RIGHT],             HID_DS3_BUTTON_RIGHT[0],               HID_DS3_BUTTON_RIGHT[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_DOWN],              HID_DS3_BUTTON_DOWN[0],                HID_DS3_BUTTON_DOWN[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_UP],                HID_DS3_BUTTON_UP[0],                  HID_DS3_BUTTON_UP[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_PLUS],              HID_DS3_BUTTON_START[0],               HID_DS3_BUTTON_START[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_MINUS],             HID_DS3_BUTTON_SELECT[0],              HID_DS3_BUTTON_SELECT[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_L],                 HID_DS3_BUTTON_L1[0],                  HID_DS3_BUTTON_L1[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_R],                 HID_DS3_BUTTON_R1[0],                  HID_DS3_BUTTON_R1[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_ZL],                HID_DS3_BUTTON_L2[0],                  HID_DS3_BUTTON_L2[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_ZR],                HID_DS3_BUTTON_R2[0],                  HID_DS3_BUTTON_R2[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_STICK_L],           HID_DS3_BUTTON_L3[0],                  HID_DS3_BUTTON_L3[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_STICK_R],           HID_DS3_BUTTON_R3[0],                  HID_DS3_BUTTON_R3[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_HOME],              HID_DS3_BUTTON_GUIDE[0],               HID_DS3_BUTTON_GUIDE[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_PAD_COUNT],                     CONTROLLER_PATCHER_VALUE_SET,          HID_DS3_PAD_COUNT);

    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_L_STICK_X],          HID_DS3_STICK_L_X[STICK_CONF_BYTE],   HID_DS3_STICK_L_X[STICK_CONF_DEFAULT]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_L_STICK_X_DEADZONE], CONTROLLER_PATCHER_VALUE_SET,         HID_DS3_STICK_L_X[STICK_CONF_DEADZONE]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_L_STICK_X_MINMAX],   HID_DS3_STICK_L_X[STICK_CONF_MIN],    HID_DS3_STICK_L_X[STICK_CONF_MAX]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_L_STICK_X_INVERT],   CONTROLLER_PATCHER_VALUE_SET,         HID_DS3_STICK_L_X[STICK_CONF_INVERT]);

    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_L_STICK_Y],          HID_DS3_STICK_L_Y[STICK_CONF_BYTE],   HID_DS3_STICK_L_Y[STICK_CONF_DEFAULT]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_L_STICK_Y_DEADZONE], CONTROLLER_PATCHER_VALUE_SET,         HID_DS3_STICK_L_Y[STICK_CONF_DEADZONE]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_L_STICK_Y_MINMAX],   HID_DS3_STICK_L_Y[STICK_CONF_MIN],    HID_DS3_STICK_L_Y[STICK_CONF_MAX]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_L_STICK_Y_INVERT],   CONTROLLER_PATCHER_VALUE_SET,         HID_DS3_STICK_L_Y[STICK_CONF_INVERT]);

    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_R_STICK_X],          HID_DS3_STICK_R_X[STICK_CONF_BYTE],   HID_DS3_STICK_R_X[STICK_CONF_DEFAULT]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_R_STICK_X_DEADZONE], CONTROLLER_PATCHER_VALUE_SET,         HID_DS3_STICK_R_X[STICK_CONF_DEADZONE]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_R_STICK_X_MINMAX],   HID_DS3_STICK_R_X[STICK_CONF_MIN],    HID_DS3_STICK_R_X[STICK_CONF_MAX]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_R_STICK_X_INVERT],   CONTROLLER_PATCHER_VALUE_SET,         HID_DS3_STICK_R_X[STICK_CONF_INVERT]);

    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_R_STICK_Y],          HID_DS3_STICK_R_Y[STICK_CONF_BYTE],   HID_DS3_STICK_R_Y[STICK_CONF_DEFAULT]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_R_STICK_Y_DEADZONE], CONTROLLER_PATCHER_VALUE_SET,         HID_DS3_STICK_R_Y[STICK_CONF_DEADZONE]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_R_STICK_Y_MINMAX],   HID_DS3_STICK_R_Y[STICK_CONF_MIN],    HID_DS3_STICK_R_Y[STICK_CONF_MAX]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds3_slot][CONTRPS_VPAD_BUTTON_R_STICK_Y_INVERT],   CONTROLLER_PATCHER_VALUE_SET,         HID_DS3_STICK_R_Y[STICK_CONF_INVERT]);

    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //! DS4
    //!---------------------------------------------------------------------------------------------------------------------------------------------------------------------------

    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VID],                           (HID_DS4_VID>>8)&0xFF,                 HID_DS4_VID&0xFF);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_PID],                           (HID_DS4_PID>>8)&0xFF,                 HID_DS4_PID&0xFF);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_BUF_SIZE],                      CONTROLLER_PATCHER_VALUE_SET,          128);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_A],                 HID_DS4_BUTTON_CIRCLE[0],              HID_DS4_BUTTON_CIRCLE[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_B],                 HID_DS4_BUTTON_CROSS[0],               HID_DS4_BUTTON_CROSS[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_X],                 HID_DS4_BUTTON_TRIANGLE[0],            HID_DS4_BUTTON_TRIANGLE[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_Y],                 HID_DS4_BUTTON_SQUARE[0],              HID_DS4_BUTTON_SQUARE[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_DPAD_MODE],                     CONTROLLER_PATCHER_VALUE_SET,          HID_DS4_BUTTON_DPAD_TYPE[CONTRDPAD_MODE]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_DPAD_MASK],                     CONTROLLER_PATCHER_VALUE_SET,          HID_DS4_BUTTON_DPAD_TYPE[CONTRDPAD_MASK]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_DPAD_N],            HID_DS4_BUTTON_DPAD_N[0],              HID_DS4_BUTTON_DPAD_N[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_DPAD_NE],           HID_DS4_BUTTON_DPAD_NE[0],             HID_DS4_BUTTON_DPAD_NE[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_DPAD_E],            HID_DS4_BUTTON_DPAD_E[0],              HID_DS4_BUTTON_DPAD_E[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_DPAD_SE],           HID_DS4_BUTTON_DPAD_SE[0],             HID_DS4_BUTTON_DPAD_SE[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_DPAD_S],            HID_DS4_BUTTON_DPAD_S[0],              HID_DS4_BUTTON_DPAD_S[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_DPAD_SW],           HID_DS4_BUTTON_DPAD_SW[0],             HID_DS4_BUTTON_DPAD_SW[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_DPAD_W],            HID_DS4_BUTTON_DPAD_W[0],              HID_DS4_BUTTON_DPAD_W[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_DPAD_NW],           HID_DS4_BUTTON_DPAD_NW[0],             HID_DS4_BUTTON_DPAD_NW[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_DPAD_NEUTRAL],      HID_DS4_BUTTON_DPAD_NEUTRAL[0],        HID_DS4_BUTTON_DPAD_NEUTRAL[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_PLUS],              HID_DS4_BUTTON_OPTIONS[0],             HID_DS4_BUTTON_OPTIONS[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_MINUS],             HID_DS4_BUTTON_SHARE[0],               HID_DS4_BUTTON_SHARE[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_L],                 HID_DS4_BUTTON_L1[0],                  HID_DS4_BUTTON_L1[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_R],                 HID_DS4_BUTTON_R1[0],                  HID_DS4_BUTTON_R1[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_ZL],                HID_DS4_BUTTON_L2[0],                  HID_DS4_BUTTON_L2[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_ZR],                HID_DS4_BUTTON_R2[0],                  HID_DS4_BUTTON_R2[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_STICK_L],           HID_DS4_BUTTON_L3[0],                  HID_DS4_BUTTON_L3[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_STICK_R],           HID_DS4_BUTTON_R3[0],                  HID_DS4_BUTTON_R3[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_HOME],              HID_DS4_BUTTON_GUIDE[0],               HID_DS4_BUTTON_GUIDE[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_PAD_COUNT],                     CONTROLLER_PATCHER_VALUE_SET,          HID_DS4_PAD_COUNT);

    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_L_STICK_X],          HID_DS4_STICK_L_X[STICK_CONF_BYTE],   HID_DS4_STICK_L_X[STICK_CONF_DEFAULT]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_L_STICK_X_DEADZONE], CONTROLLER_PATCHER_VALUE_SET,         HID_DS4_STICK_L_X[STICK_CONF_DEADZONE]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_L_STICK_X_MINMAX],   HID_DS4_STICK_L_X[STICK_CONF_MIN],    HID_DS4_STICK_L_X[STICK_CONF_MAX]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_L_STICK_X_INVERT],   CONTROLLER_PATCHER_VALUE_SET,         HID_DS4_STICK_L_X[STICK_CONF_INVERT]);

    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_L_STICK_Y],          HID_DS4_STICK_L_Y[STICK_CONF_BYTE],   HID_DS4_STICK_L_Y[STICK_CONF_DEFAULT]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_L_STICK_Y_DEADZONE], CONTROLLER_PATCHER_VALUE_SET,         HID_DS4_STICK_L_Y[STICK_CONF_DEADZONE]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_L_STICK_Y_MINMAX],   HID_DS4_STICK_L_Y[STICK_CONF_MIN],    HID_DS4_STICK_L_Y[STICK_CONF_MAX]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_L_STICK_Y_INVERT],   CONTROLLER_PATCHER_VALUE_SET,         HID_DS4_STICK_L_Y[STICK_CONF_INVERT]);

    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_R_STICK_X],          HID_DS4_STICK_R_X[STICK_CONF_BYTE],   HID_DS4_STICK_R_X[STICK_CONF_DEFAULT]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_R_STICK_X_DEADZONE], CONTROLLER_PATCHER_VALUE_SET,         HID_DS4_STICK_R_X[STICK_CONF_DEADZONE]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_R_STICK_X_MINMAX],   HID_DS4_STICK_R_X[STICK_CONF_MIN],    HID_DS4_STICK_R_X[STICK_CONF_MAX]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_R_STICK_X_INVERT],   CONTROLLER_PATCHER_VALUE_SET,         HID_DS4_STICK_R_X[STICK_CONF_INVERT]);

    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_R_STICK_Y],          HID_DS4_STICK_R_Y[STICK_CONF_BYTE],   HID_DS4_STICK_R_Y[STICK_CONF_DEFAULT]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_R_STICK_Y_DEADZONE], CONTROLLER_PATCHER_VALUE_SET,         HID_DS4_STICK_R_Y[STICK_CONF_DEADZONE]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_R_STICK_Y_MINMAX],   HID_DS4_STICK_R_Y[STICK_CONF_MIN],    HID_DS4_STICK_R_Y[STICK_CONF_MAX]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[ds4_slot][CONTRPS_VPAD_BUTTON_R_STICK_Y_INVERT],   CONTROLLER_PATCHER_VALUE_SET,         HID_DS4_STICK_R_Y[STICK_CONF_INVERT]);

    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //! XInput
    //!---------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VID],                           (HID_XINPUT_VID>>8)&0xFF,               HID_XINPUT_VID&0xFF);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_PID],                           (HID_XINPUT_PID>>8)&0xFF,               HID_XINPUT_PID&0xFF);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_BUF_SIZE],                      CONTROLLER_PATCHER_VALUE_SET,           128);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_A],                 HID_XINPUT_BUTTON_B[0],                 HID_XINPUT_BUTTON_B[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_B],                 HID_XINPUT_BUTTON_A[0],                 HID_XINPUT_BUTTON_A[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_X],                 HID_XINPUT_BUTTON_Y[0],                 HID_XINPUT_BUTTON_Y[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_Y],                 HID_XINPUT_BUTTON_X[0],                 HID_XINPUT_BUTTON_X[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_DPAD_MODE],                     CONTROLLER_PATCHER_VALUE_SET,           HID_XINPUT_BUTTON_DPAD_TYPE[CONTRDPAD_MODE]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_DPAD_MASK],                     CONTROLLER_PATCHER_VALUE_SET,           HID_XINPUT_BUTTON_DPAD_TYPE[CONTRDPAD_MASK]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_UP],                HID_XINPUT_BUTTON_UP[0],                HID_XINPUT_BUTTON_UP[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_DOWN],              HID_XINPUT_BUTTON_DOWN[0],              HID_XINPUT_BUTTON_DOWN[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_LEFT],              HID_XINPUT_BUTTON_LEFT[0],              HID_XINPUT_BUTTON_LEFT[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_RIGHT],             HID_XINPUT_BUTTON_RIGHT[0],             HID_XINPUT_BUTTON_RIGHT[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_PLUS],              HID_XINPUT_BUTTON_START[0],             HID_XINPUT_BUTTON_START[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_MINUS],             HID_XINPUT_BUTTON_BACK[0],              HID_XINPUT_BUTTON_BACK[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_L],                 HID_XINPUT_BUTTON_LB[0],                HID_XINPUT_BUTTON_LB[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_R],                 HID_XINPUT_BUTTON_RB[0],                HID_XINPUT_BUTTON_RB[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_ZL],                HID_XINPUT_BUTTON_LT[0],                HID_XINPUT_BUTTON_LT[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_ZR],                HID_XINPUT_BUTTON_RT[0],                HID_XINPUT_BUTTON_RT[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_STICK_L],           HID_XINPUT_BUTTON_L3[0],                HID_XINPUT_BUTTON_L3[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_STICK_R],           HID_XINPUT_BUTTON_R3[0],                HID_XINPUT_BUTTON_R3[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_HOME],              HID_XINPUT_BUTTON_GUIDE[0],             HID_XINPUT_BUTTON_GUIDE[1]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_PAD_COUNT],                     CONTROLLER_PATCHER_VALUE_SET,           HID_XINPUT_PAD_COUNT);

    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_L_STICK_X],          HID_XINPUT_STICK_L_X[STICK_CONF_BYTE], HID_XINPUT_STICK_L_X[STICK_CONF_DEFAULT]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_L_STICK_X_DEADZONE], CONTROLLER_PATCHER_VALUE_SET,          HID_XINPUT_STICK_L_X[STICK_CONF_DEADZONE]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_L_STICK_X_MINMAX],   HID_XINPUT_STICK_L_X[STICK_CONF_MIN],  HID_XINPUT_STICK_L_X[STICK_CONF_MAX]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_L_STICK_X_INVERT],   CONTROLLER_PATCHER_VALUE_SET,          HID_XINPUT_STICK_L_X[STICK_CONF_INVERT]);

    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_L_STICK_Y],          HID_XINPUT_STICK_L_Y[STICK_CONF_BYTE], HID_XINPUT_STICK_L_Y[STICK_CONF_DEFAULT]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_L_STICK_Y_DEADZONE], CONTROLLER_PATCHER_VALUE_SET,          HID_XINPUT_STICK_L_Y[STICK_CONF_DEADZONE]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_L_STICK_Y_MINMAX],   HID_XINPUT_STICK_L_Y[STICK_CONF_MIN],  HID_XINPUT_STICK_L_Y[STICK_CONF_MAX]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_L_STICK_Y_INVERT],   CONTROLLER_PATCHER_VALUE_SET,          HID_XINPUT_STICK_L_Y[STICK_CONF_INVERT]);

    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_R_STICK_X],          HID_XINPUT_STICK_R_X[STICK_CONF_BYTE], HID_XINPUT_STICK_R_X[STICK_CONF_DEFAULT]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_R_STICK_X_DEADZONE], CONTROLLER_PATCHER_VALUE_SET,          HID_XINPUT_STICK_R_X[STICK_CONF_DEADZONE]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_R_STICK_X_MINMAX],   HID_XINPUT_STICK_R_X[STICK_CONF_MIN],  HID_XINPUT_STICK_R_X[STICK_CONF_MAX]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_R_STICK_X_INVERT],   CONTROLLER_PATCHER_VALUE_SET,          HID_XINPUT_STICK_R_X[STICK_CONF_INVERT]);

    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_R_STICK_Y],          HID_XINPUT_STICK_R_Y[STICK_CONF_BYTE], HID_XINPUT_STICK_R_Y[STICK_CONF_DEFAULT]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_R_STICK_Y_DEADZONE], CONTROLLER_PATCHER_VALUE_SET,          HID_XINPUT_STICK_R_Y[STICK_CONF_DEADZONE]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_R_STICK_Y_MINMAX],   HID_XINPUT_STICK_R_Y[STICK_CONF_MIN],  HID_XINPUT_STICK_R_Y[STICK_CONF_MAX]);
    ControllerPatcherUtils::setConfigValue((u8*)&config_controller[xinput_slot][CONTRPS_VPAD_BUTTON_R_STICK_Y_INVERT],   CONTROLLER_PATCHER_VALUE_SET,          HID_XINPUT_STICK_R_Y[STICK_CONF_INVERT]);
}

bool ControllerPatcher::Init(){
    /*
    InitOSFunctionPointers();
    InitSocketFunctionPointers();
    InitSysHIDFunctionPointers();
    InitVPadFunctionPointers();
    InitPadScoreFunctionPointers();*/

    OSDynLoadModule padscore_handle = 0;
    OSDynLoad_Acquire("padscore.rpl", &padscore_handle);
    void(*KPADRead_test)();
    OSDynLoad_FindExport(padscore_handle, 0, "KPADRead", (void**)&KPADRead_test);
    printf("ControllerPatcher::Init(line %d): Found the KPADRead at %08X \n",__LINE__,KPADRead_test);

    gSamplingCallback = (wpad_sampling_callback_t)((u32)KPADRead_test + 0x1F0);
    if(*(u32*)gSamplingCallback != FIRST_INSTRUCTION_IN_SAMPLING_CALLBACK){
        gSamplingCallback = NULL;
        //In Firmware <= 5.1.2 the offset changes
        gSamplingCallback = (wpad_sampling_callback_t)((u32)KPADRead_test + 0x1F8);
        if(*(u32*)gSamplingCallback != FIRST_INSTRUCTION_IN_SAMPLING_CALLBACK){
            //Should never happen. I looked into the padscore.rpl of ALL firmwares.
            gSamplingCallback = NULL;
        }
    }
    printf("ControllerPatcher::Init(line %d): Found the gSamplingCallback at %08X \n",__LINE__,gSamplingCallback);

    if(HID_DEBUG){ printf("ControllerPatcher::Init(line %d): Init called! \n",__LINE__); }

    /* if(syshid_handle == 0){
         printf("ControllerPatcher::Init(line %d): Failed to load the HID API \n",__LINE__);
         return false;
    }*/

    if(gConfig_done == HID_INIT_NOT_DONE){
        if(HID_DEBUG){ printf("ControllerPatcher::Init(line %d): First time calling the Init\n",__LINE__); }
        gConfig_done = HID_INIT_DONE;
        ControllerPatcher::ResetConfig();
    }else{
        if(HID_DEBUG){ printf("ControllerPatcher::Init(line %d): ControllerPatcher::Init(): Config already done!\n",__LINE__); }
    }

    //Broken
    if(gConfig_done != HID_SDCARD_READ){
        printf("ControllerPatcher::Init(line %d): Reading config files from SD Card\n",__LINE__);
        ConfigReader* reader = ConfigReader::getInstance();
        s32 status = 0;
        if((status = reader->InitSDCard()) == 0){
            if(HID_DEBUG){ printf("ControllerPatcher::Init(line %d):  SD Card mounted for controller config!\n",__LINE__); }
             reader->ReadAllConfigs();
            printf("ControllerPatcher::Init(line %d): Done with reading config files from SD Card\n",__LINE__);
            gConfig_done = HID_SDCARD_READ;
        }else{
            printf("ControllerPatcher::Init(line %d): SD mounting failed! %d\n",__LINE__,status);
        }
        ConfigReader::destroyInstance();
    }

    printf("ControllerPatcher::Init(line %d): Initializing the data for button remapping\n",__LINE__);
    InitButtonMapping();

    if(!gHIDAttached){
        HIDAddClient(&gHIDClient, ControllerPatcherHID::myAttachDetachCallback);
    }

    return true;
}

void ControllerPatcher::startNetworkServer(){
    UDPServer::getInstance();
    TCPServer::getInstance();
}

void ControllerPatcher::stopNetworkServer(){
    UDPServer::destroyInstance();
    UDPClient::destroyInstance();
    TCPServer::destroyInstance();
}

void ControllerPatcher::DeInit(){
    if(HID_DEBUG){ printf("ControllerPatcher::DeInit(line %d) called! \n",__LINE__); }

    if(gHIDAttached) HIDDelClient(&gHIDClient);

    //Resetting the state of the last pressed data.
    buttonRemapping_lastButtonsHold = 0;
    memset(last_button_hold,0,sizeof(last_button_hold));
    memset(myVPADBuffer,0,sizeof(myVPADBuffer));
    memset(&gControllerMapping,0,sizeof(gControllerMapping));
    memset(&gHIDClient,0,sizeof(gHIDClient));
    memset(gHID_Devices,0,sizeof(gHID_Devices));
    memset(gGamePadValues,0,sizeof(gGamePadValues));
    memset(config_controller,0,sizeof(config_controller));
    memset(config_controller_hidmask,0,sizeof(config_controller_hidmask));
    memset(gNetworkController,0,sizeof(gNetworkController));
    memset(connectionOrderHelper,0,sizeof(connectionOrderHelper));

    memset(gWPADConnectCallback,0,sizeof(gWPADConnectCallback));
    memset(gKPADConnectCallback,0,sizeof(gKPADConnectCallback));
    memset(gExtensionCallback,0,sizeof(gExtensionCallback));
    gCallbackCooldown = 0;

    gConfig_done = HID_INIT_NOT_DONE;
    gButtonRemappingConfigDone = 0;
    gHIDAttached = 0;
    gHIDCurrentDevice  = 0;
    gHIDRegisteredDevices  = 0;
    gHID_Mouse_Mode = HID_MOUSE_MODE_TOUCH;
    gHID_LIST_GC = 0;
    gHID_LIST_DS3 = 0;
    gHID_LIST_DS4 = 0;
    gHID_LIST_KEYBOARD = 0;
    gHID_LIST_MOUSE = 0;
    gHID_LIST_SWITCH_PRO = 0;

    gGamePadSlot = 0;
    gHID_SLOT_GC = 0;
    gHID_SLOT_KEYBOARD = 0;
    gMouseSlot = 0;

    gOriginalDimState = 0;
    gOriginalAPDState = 0;

    gHIDNetworkClientID = 0;

    destroyConfigHelper();
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcher::enableControllerMapping(){
    gControllerMapping.gamepad.useAll = 0;
    return CONTROLLER_PATCHER_ERROR_NONE;
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcher::disableControllerMapping(){
    gControllerMapping.gamepad.useAll = 1;
    return CONTROLLER_PATCHER_ERROR_NONE;
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcher::disableWiiUEnergySetting(){
    s32 res;
    if(IMIsDimEnabled(&res) == 0){
        if(res == 1){
            if(HID_DEBUG){ printf("ControllerPatcher::disableWiiUEnergySetting(): Dim was orignally enabled!\n"); }
            gOriginalDimState = 1;
        }
    }

    if(IMIsAPDEnabled(&res) == 0){
        if(res == 1){
            if(HID_DEBUG){ printf("ControllerPatcher::disableWiiUEnergySetting(): Auto power down was orignally enabled!\n"); }
            gOriginalAPDState = 1;
        }
    }

    IMDisableDim();
    IMDisableAPD();
    printf("ControllerPatcher::disableWiiUEnergySetting(): Disable Energy savers\n");
    return CONTROLLER_PATCHER_ERROR_NONE;
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcher::restoreWiiUEnergySetting(){

    //Check if we need to enable Auto Power down again on exiting
    if(gOriginalAPDState == 1){
        printf("ControllerPatcher::restoreWiiUEnergySetting(): Auto shutdown was on before using HID to VPAD. Setting it to on again.\n");
        IMEnableAPD();
    }
    if(gOriginalDimState == 1){
        printf("ControllerPatcher::restoreWiiUEnergySetting(): Burn-in reduction was on before using HID to VPAD. Setting it to on again.\n");
        IMEnableDim();
    }
    return CONTROLLER_PATCHER_ERROR_NONE;
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcher::resetControllerMapping(UController_Type type){
    ControllerMappingPAD * cm_map_pad = ControllerPatcherUtils::getControllerMappingByType(type);

    if(cm_map_pad == NULL){return CONTROLLER_PATCHER_ERROR_NULL_POINTER;}
    memset(cm_map_pad,0,sizeof(*cm_map_pad));

    return CONTROLLER_PATCHER_ERROR_NONE;
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcher::addControllerMapping(UController_Type type,ControllerMappingPADInfo config){
    ControllerMappingPAD * cm_map_pad = ControllerPatcherUtils::getControllerMappingByType(type);

    s32 result = 0;

    for(s32 i=0;i<HID_MAX_DEVICES_PER_SLOT;i++){
        ControllerMappingPADInfo * info = &(cm_map_pad->pad_infos[i]);
        if(info != NULL && !info->active){
            info->active = 1;
            info->pad = config.pad;
            info->type = config.type;
            info->vidpid.vid =  config.vidpid.vid;
            info->vidpid.pid =  config.vidpid.pid;

            result = 1;
            break;
        }
    }
    if(result == 0){
        //No free slot.
        return -1;
    }

    return CONTROLLER_PATCHER_ERROR_NONE;
}

s32 ControllerPatcher::getActiveMappingSlot(UController_Type type){
    ControllerMappingPAD * cm_map_pad = ControllerPatcherUtils::getControllerMappingByType(type);

    if(cm_map_pad == NULL){return -1;}

    s32 connected = -1;
    for(s32 i =0;i<HID_MAX_DEVICES_PER_SLOT;i++){
        if(cm_map_pad->pad_infos[i].active || cm_map_pad->pad_infos[i].type == CM_Type_RealController){
            connected = i;
            break;
        }
    }

    return connected;
}

bool ControllerPatcher::isControllerConnectedAndActive(UController_Type type,s32 mapping_slot){
    ControllerMappingPADInfo * padinfo = getControllerMappingInfo(type,mapping_slot);
    if(!padinfo){
        return false;
    }
    if(padinfo->active){
        DeviceInfo device_info;

        memset(&device_info,0,sizeof(device_info));

        device_info.vidpid = padinfo->vidpid;

        s32 res;
        if((res = ControllerPatcherUtils::getDeviceInfoFromVidPid(&device_info)) < 0){
            return false;
        }

        u32 hidmask = device_info.slotdata.hidmask;
        u32 pad = padinfo->pad;

        HID_Data * data_cur;

        if((res = ControllerPatcherHID::getHIDData(hidmask,pad,&data_cur)) < 0) {
           return false;
        }

        return true;
    }
    return false;
}

ControllerMappingPADInfo * ControllerPatcher::getControllerMappingInfo(UController_Type type,s32 mapping_slot){
    ControllerMappingPAD * cm_map_pad = ControllerPatcherUtils::getControllerMappingByType(type);

    if(cm_map_pad == NULL){return NULL;}

    if(mapping_slot < 0 || mapping_slot > HID_MAX_DEVICES_PER_SLOT-1){ return NULL;}

    return &(cm_map_pad->pad_infos[mapping_slot]);
}

HID_Mouse_Data * ControllerPatcher::getMouseData(){
    ControllerMappingPAD * CMPAD = ControllerPatcherUtils::getControllerMappingByType(UController_Type_Gamepad);

    if(CMPAD == NULL){
        return NULL;
    }

    HID_Mouse_Data * result = NULL;

    for(s32 i;i<HID_MAX_DEVICES_PER_SLOT;i++){
        ControllerMappingPADInfo * padinfo = &(CMPAD->pad_infos[i]);
        if(!padinfo->active){
            break;
        }
        if(padinfo->type == CM_Type_Mouse){
            result = &(gHID_Devices[gMouseSlot].pad_data[padinfo->pad].data_union.mouse.cur_mouse_data);
        }
    }
    return result;
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcher::setRumble(UController_Type type,u32 status){
    ControllerMappingPAD * cm_map_pad = ControllerPatcherUtils::getControllerMappingByType(type);
    if(cm_map_pad == NULL){return -1;}
    cm_map_pad->rumble = !!status; //to make sure it's only 0 or 1.
    return CONTROLLER_PATCHER_ERROR_NONE;
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcher::gettingInputAllDevices(InputData * output,s32 array_size){
    HID_Data * data_cur;
    VPADStatus pad_buffer;
    VPADStatus * buffer = &pad_buffer;
    s32 result = CONTROLLER_PATCHER_ERROR_NONE;
    std::map<my_cb_user *,s32> pad_count;
    //printf("fill in data\n");
    for(s32 i = 0;i < gHIDMaxDevices;i++){
        u8 * status = &output[result].status;
        *status = 0;
        if(connectionOrderHelper[i] != NULL){
            *status = 1;

            my_cb_user *  usr = connectionOrderHelper[i];
            pad_count[usr] = pad_count[usr] +1;
            s32 hid = usr->slotdata.hidmask;
            //printf("result[%d] usr: %08X\n",result,usr);

            s32 realpad = pad_count[usr] - 1;

            output[result].device_info.pad_count = usr->pads_per_device;
            output[result].device_info.slotdata = usr->slotdata;
            output[result].device_info.vidpid = usr->vidpid;

            InputButtonData * buttondata = &output[result].button_data;
            InputStickData * stickdata = &output[result].stick_data;

            s32 buttons_hold = 0;
            buttondata->hold = 0;
            buttondata->trigger = 0;
            buttondata->release = 0;

            s32 res;
            if((res = ControllerPatcherHID::getHIDData(hid,realpad,&data_cur)) == 0){
                res = ControllerPatcherUtils::getButtonPressed(data_cur,&buttons_hold,VPAD_BUTTON_A);
                ControllerPatcherUtils::getButtonPressed(data_cur,&buttons_hold,VPAD_BUTTON_B);
                ControllerPatcherUtils::getButtonPressed(data_cur,&buttons_hold,VPAD_BUTTON_X);
                ControllerPatcherUtils::getButtonPressed(data_cur,&buttons_hold,VPAD_BUTTON_Y);

                ControllerPatcherUtils::getButtonPressed(data_cur,&buttons_hold,VPAD_BUTTON_LEFT);
                ControllerPatcherUtils::getButtonPressed(data_cur,&buttons_hold,VPAD_BUTTON_RIGHT);
                ControllerPatcherUtils::getButtonPressed(data_cur,&buttons_hold,VPAD_BUTTON_DOWN);
                ControllerPatcherUtils::getButtonPressed(data_cur,&buttons_hold,VPAD_BUTTON_UP);

                ControllerPatcherUtils::getButtonPressed(data_cur,&buttons_hold,VPAD_BUTTON_MINUS);
                ControllerPatcherUtils::getButtonPressed(data_cur,&buttons_hold,VPAD_BUTTON_L);
                ControllerPatcherUtils::getButtonPressed(data_cur,&buttons_hold,VPAD_BUTTON_R);

                ControllerPatcherUtils::getButtonPressed(data_cur,&buttons_hold,VPAD_BUTTON_PLUS);
                ControllerPatcherUtils::getButtonPressed(data_cur,&buttons_hold,VPAD_BUTTON_ZL);
                ControllerPatcherUtils::getButtonPressed(data_cur,&buttons_hold,VPAD_BUTTON_ZR);

                ControllerPatcherUtils::getButtonPressed(data_cur,&buttons_hold,VPAD_BUTTON_HOME);
                ControllerPatcherUtils::getButtonPressed(data_cur,&buttons_hold,VPAD_BUTTON_STICK_L);
                ControllerPatcherUtils::getButtonPressed(data_cur,&buttons_hold,VPAD_BUTTON_STICK_R);

                buttondata->hold |= buttons_hold;
                buttondata->trigger |= (buttons_hold & (~(data_cur->last_buttons)));
                buttondata->release |= ((data_cur->last_buttons) & (~buttons_hold));

                buffer->leftStick.x = 0.0f;
                buffer->leftStick.y = 0.0f;
                buffer->rightStick.x = 0.0f;
                buffer->rightStick.y = 0.0f;

                ControllerPatcherUtils::convertAnalogSticks(data_cur,buffer);
                ControllerPatcherUtils::normalizeStickValues(&(buffer->leftStick));
                ControllerPatcherUtils::normalizeStickValues(&(buffer->rightStick));

                stickdata->leftStickX = buffer->leftStick.x;
                stickdata->leftStickY = buffer->leftStick.y;
                stickdata->rightStickX = buffer->rightStick.x;
                stickdata->rightStickY = buffer->rightStick.y;

                //printf("result[%d] buttons %08X\n",result,buttons_hold);

                data_cur->last_buttons = buttons_hold;
            }
        }

        result++;
        if(result >= array_size){
            break;
        }
    }

    return result;
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcher::setProControllerDataFromHID(void * data,s32 chan, s32 mode){
    if(data == NULL) return CONTROLLER_PATCHER_ERROR_NULL_POINTER;
    if(chan < 0 || chan > 3) return CONTROLLER_PATCHER_ERROR_INVALID_CHAN;
    //if(gControllerMapping.proController[chan].enabled == 0) return CONTROLLER_PATCHER_ERROR_MAPPING_DISABLED;

    VPADStatus * vpad_buffer = &myVPADBuffer[chan];
    memset(vpad_buffer,0,sizeof(*vpad_buffer));

    std::vector<HID_Data *> data_list;

    for(s32 i = 0;i<HID_MAX_DEVICES_PER_SLOT;i++){
        ControllerMappingPADInfo cm_map_pad_info = gControllerMapping.proController[chan].pad_infos[i];
        if(!cm_map_pad_info.active){
            continue;
        }
        DeviceInfo device_info;
        memset(&device_info,0,sizeof(device_info));

        device_info.vidpid = cm_map_pad_info.vidpid;

        s32 res;
        if((res = ControllerPatcherUtils::getDeviceInfoFromVidPid(&device_info)) < 0){
            printf("ControllerPatcherUtils::getDeviceInfoFromVidPid(&device_info) = %d\n",res);
            continue;
        }

        s32 hidmask = device_info.slotdata.hidmask;
        s32 pad = cm_map_pad_info.pad;

        HID_Data * data_cur;

        if((res = ControllerPatcherHID::getHIDData(hidmask,pad,&data_cur)) < 0) {
            //printf("ControllerPatcherHID::getHIDData(hidmask,pad,&data_cur)) = %d\n",res);
            continue;
        }
        data_list.push_back(data_cur);
    }

    if(data_list.empty()){
        return CONTROLLER_PATCHER_ERROR_FAILED_TO_GET_HIDDATA;
    }
    s32 res = 0;
    if((res = ControllerPatcherHID::setVPADControllerData(vpad_buffer,data_list)) < 0) return res;
    //make it configurable?
    //ControllerPatcher::printVPADButtons(vpad_buffer); //Leads to random crashes on transitions.

    //a bit hacky?
    if(mode == PRO_CONTROLLER_MODE_KPADDATA){
        KPADData *  pro_buffer = (KPADData *) data;
        if((res = ControllerPatcherUtils::translateToPro(vpad_buffer,pro_buffer,&last_button_hold[chan])) < 0 ) return res;
    }else if(mode == PRO_CONTROLLER_MODE_WPADReadData){
        WPADReadData * pro_buffer = (WPADReadData *) data;
        if((res = ControllerPatcherUtils::translateToProWPADRead(vpad_buffer,pro_buffer)) < 0 ) return res;
    }
    for(std::vector<HID_Data *>::iterator it = data_list.begin(); it != data_list.end(); ++it) {
        HID_Data * cur_ptr = *it;
        cur_ptr->rumbleActive = !!gControllerMapping.proController[chan].rumble;
    }

    return CONTROLLER_PATCHER_ERROR_NONE;
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcher::setControllerDataFromHID(VPADStatus * buffer){
    if(buffer == NULL) return CONTROLLER_PATCHER_ERROR_NULL_POINTER;
    //if(gControllerMapping.gamepad.enabled == 0) return CONTROLLER_PATCHER_ERROR_MAPPING_DISABLED;

    s32 hidmask = 0;
    s32 pad = 0;

    ControllerMappingPAD cm_map_pad = gControllerMapping.gamepad;
    std::vector<HID_Data *> data_list;

    if (cm_map_pad.useAll == 1) {
        data_list = ControllerPatcherHID::getHIDDataAll();
    }else{
        for(s32 i = 0;i<HID_MAX_DEVICES_PER_SLOT;i++){
            ControllerMappingPADInfo cm_map_pad_info = cm_map_pad.pad_infos[i];
            if(cm_map_pad_info.active){
                DeviceInfo device_info;
                memset(&device_info,0,sizeof(device_info));
                device_info.vidpid = cm_map_pad_info.vidpid;

                if(ControllerPatcherUtils::getDeviceInfoFromVidPid(&device_info) < 0){
                    continue;
                    //return CONTROLLER_PATCHER_ERROR_UNKNOWN_VID_PID;
                }

                hidmask = device_info.slotdata.hidmask;

                HID_Data * data;

                pad = cm_map_pad_info.pad;
                s32 res = ControllerPatcherHID::getHIDData(hidmask,pad,&data);
                if(res < 0){
                    continue;
                    //return CONTROLLER_PATCHER_ERROR_FAILED_TO_GET_HIDDATA;
                }
                if(data != NULL) data_list.push_back(data);
            }
        }
    }
    if(data_list.empty()){
        return CONTROLLER_PATCHER_ERROR_FAILED_TO_GET_HIDDATA;
    }

    ControllerPatcherHID::setVPADControllerData(buffer,data_list);

    for(u32 i = 0; i < data_list.size();i++){
        data_list[i]->rumbleActive = !!gControllerMapping.gamepad.rumble;
    }

    return CONTROLLER_PATCHER_ERROR_NONE;
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcher::printVPADButtons(VPADStatus * buffer){
    return CONTROLLER_PATCHER_ERROR_NONE;
    /* BROKEN on transitions.*/
    if(buffer == NULL) return CONTROLLER_PATCHER_ERROR_NULL_POINTER;
    if(buffer->trigger != 0x00000000){
        char output[250];

        output[0] = 0; //null terminate it. just in case.

        if((buffer->trigger & VPAD_BUTTON_A)                 ==  VPAD_BUTTON_A)                  strcat(output,"A        ");
        if((buffer->trigger & VPAD_BUTTON_B)                 ==  VPAD_BUTTON_B)                  strcat(output,"B        ");
        if((buffer->trigger & VPAD_BUTTON_X)                 ==  VPAD_BUTTON_X)                  strcat(output,"X        ");
        if((buffer->trigger & VPAD_BUTTON_Y)                 ==  VPAD_BUTTON_Y)                  strcat(output,"Y        ");
        if((buffer->trigger & VPAD_BUTTON_L)                 ==  VPAD_BUTTON_L)                  strcat(output,"L        ");
        if((buffer->trigger & VPAD_BUTTON_R)                 ==  VPAD_BUTTON_R)                  strcat(output,"R        ");
        if((buffer->trigger & VPAD_BUTTON_ZR)                ==  VPAD_BUTTON_ZR)                 strcat(output,"ZR       ");
        if((buffer->trigger & VPAD_BUTTON_ZL)                ==  VPAD_BUTTON_ZL)                 strcat(output,"ZL       ");
        if((buffer->trigger & VPAD_BUTTON_LEFT)              ==  VPAD_BUTTON_LEFT)               strcat(output,"Left     ");
        if((buffer->trigger & VPAD_BUTTON_RIGHT)             ==  VPAD_BUTTON_RIGHT)              strcat(output,"Right    ");
        if((buffer->trigger & VPAD_BUTTON_UP)                ==  VPAD_BUTTON_UP)                 strcat(output,"Up       ");
        if((buffer->trigger & VPAD_BUTTON_DOWN)              ==  VPAD_BUTTON_DOWN)               strcat(output,"Down     ");
        if((buffer->trigger & VPAD_BUTTON_PLUS)              ==  VPAD_BUTTON_PLUS)               strcat(output,"+        ");
        if((buffer->trigger & VPAD_BUTTON_MINUS)             ==  VPAD_BUTTON_MINUS)              strcat(output,"-        ");
        if((buffer->trigger & VPAD_BUTTON_TV)                ==  VPAD_BUTTON_TV)                 strcat(output,"TV       ");
        if((buffer->trigger & VPAD_BUTTON_HOME)              ==  VPAD_BUTTON_HOME)               strcat(output,"HOME     ");
        if((buffer->trigger & VPAD_BUTTON_STICK_L)           ==  VPAD_BUTTON_STICK_L)            strcat(output,"SL       ");
        if((buffer->trigger & VPAD_BUTTON_STICK_R)           ==  VPAD_BUTTON_STICK_R)            strcat(output,"SR       ");
        if((buffer->trigger & VPAD_STICK_R_EMULATION_LEFT)   ==  VPAD_STICK_R_EMULATION_LEFT)    strcat(output,"RE_Left  ");
        if((buffer->trigger & VPAD_STICK_R_EMULATION_RIGHT)  ==  VPAD_STICK_R_EMULATION_RIGHT)   strcat(output,"RE_Right ");
        if((buffer->trigger & VPAD_STICK_R_EMULATION_UP)     ==  VPAD_STICK_R_EMULATION_UP)      strcat(output,"RE_Up    ");
        if((buffer->trigger & VPAD_STICK_R_EMULATION_DOWN)   ==  VPAD_STICK_R_EMULATION_DOWN)    strcat(output,"RE_Down  ");
        if((buffer->trigger & VPAD_STICK_L_EMULATION_LEFT)   ==  VPAD_STICK_L_EMULATION_LEFT)    strcat(output,"LE_Left  ");
        if((buffer->trigger & VPAD_STICK_L_EMULATION_RIGHT)  ==  VPAD_STICK_L_EMULATION_RIGHT)   strcat(output,"LE_Right ");
        if((buffer->trigger & VPAD_STICK_L_EMULATION_UP)     ==  VPAD_STICK_L_EMULATION_UP)      strcat(output,"LE_Up    ");
        if((buffer->trigger & VPAD_STICK_L_EMULATION_DOWN)   ==  VPAD_STICK_L_EMULATION_DOWN)    strcat(output,"LE_Down  ");

        printf("%spressed Sticks: LX %f LY %f RX %f RY %f\n",output,buffer->leftStick.x,buffer->leftStick.y,buffer->rightStick.x,buffer->rightStick.y);
    }
    return CONTROLLER_PATCHER_ERROR_NONE;
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcher::buttonRemapping(VPADStatus * buffer,s32 buffer_count){
    if(!gButtonRemappingConfigDone) return CONTROLLER_PATCHER_ERROR_CONFIG_NOT_DONE;
    if(buffer == NULL) return CONTROLLER_PATCHER_ERROR_NULL_POINTER;
    for(s32 i = 0;i < buffer_count;i++){
        VPADStatus new_data;
        memset(&new_data,0,sizeof(new_data));

        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_BUTTON_A,                  CONTRPS_VPAD_BUTTON_A);
        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_BUTTON_B,                  CONTRPS_VPAD_BUTTON_B);
        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_BUTTON_X,                  CONTRPS_VPAD_BUTTON_X);
        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_BUTTON_Y,                  CONTRPS_VPAD_BUTTON_Y);
        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_BUTTON_LEFT,               CONTRPS_VPAD_BUTTON_LEFT);
        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_BUTTON_RIGHT,              CONTRPS_VPAD_BUTTON_RIGHT);
        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_BUTTON_UP,                 CONTRPS_VPAD_BUTTON_UP);
        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_BUTTON_DOWN,               CONTRPS_VPAD_BUTTON_DOWN);
        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_BUTTON_ZL,                 CONTRPS_VPAD_BUTTON_ZL);
        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_BUTTON_ZR,                 CONTRPS_VPAD_BUTTON_ZR);
        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_BUTTON_L,                  CONTRPS_VPAD_BUTTON_L);
        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_BUTTON_R,                  CONTRPS_VPAD_BUTTON_R);
        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_BUTTON_PLUS,               CONTRPS_VPAD_BUTTON_PLUS);
        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_BUTTON_MINUS,              CONTRPS_VPAD_BUTTON_MINUS);
        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_BUTTON_HOME,               CONTRPS_VPAD_BUTTON_HOME);
        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_BUTTON_SYNC,               CONTRPS_VPAD_BUTTON_SYNC);
        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_BUTTON_STICK_R,            CONTRPS_VPAD_BUTTON_STICK_R);
        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_BUTTON_STICK_L,            CONTRPS_VPAD_BUTTON_STICK_L);
        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_BUTTON_TV,                 CONTRPS_VPAD_BUTTON_TV);

        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_STICK_R_EMULATION_LEFT,    CONTRPS_VPAD_STICK_R_EMULATION_LEFT);
        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_STICK_R_EMULATION_RIGHT,   CONTRPS_VPAD_STICK_R_EMULATION_RIGHT);
        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_STICK_R_EMULATION_UP,      CONTRPS_VPAD_STICK_R_EMULATION_UP);
        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_STICK_R_EMULATION_DOWN,    CONTRPS_VPAD_STICK_R_EMULATION_DOWN);

        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_STICK_L_EMULATION_LEFT,    CONTRPS_VPAD_STICK_L_EMULATION_LEFT);
        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_STICK_L_EMULATION_RIGHT,   CONTRPS_VPAD_STICK_L_EMULATION_RIGHT);
        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_STICK_L_EMULATION_UP,      CONTRPS_VPAD_STICK_L_EMULATION_UP);
        ControllerPatcherUtils::setButtonRemappingData(&buffer[i],&new_data,VPAD_STICK_L_EMULATION_DOWN,    CONTRPS_VPAD_STICK_L_EMULATION_DOWN);

        //Even when you remap any Stick Emulation to a button, we still want to keep the emulated stick data. Some games like New Super Mario Bros. only work with the emulated stick data.
        ControllerPatcherUtils::setButtonData(&buffer[i],&new_data,VPAD_STICK_L_EMULATION_LEFT,             VPAD_STICK_L_EMULATION_LEFT);
        ControllerPatcherUtils::setButtonData(&buffer[i],&new_data,VPAD_STICK_L_EMULATION_RIGHT,            VPAD_STICK_L_EMULATION_RIGHT);
        ControllerPatcherUtils::setButtonData(&buffer[i],&new_data,VPAD_STICK_L_EMULATION_UP,               VPAD_STICK_L_EMULATION_UP);
        ControllerPatcherUtils::setButtonData(&buffer[i],&new_data,VPAD_STICK_L_EMULATION_DOWN,             VPAD_STICK_L_EMULATION_DOWN);

        ControllerPatcherUtils::setButtonData(&buffer[i],&new_data,VPAD_STICK_R_EMULATION_LEFT,             VPAD_STICK_R_EMULATION_LEFT);
        ControllerPatcherUtils::setButtonData(&buffer[i],&new_data,VPAD_STICK_R_EMULATION_RIGHT,            VPAD_STICK_R_EMULATION_RIGHT);
        ControllerPatcherUtils::setButtonData(&buffer[i],&new_data,VPAD_STICK_R_EMULATION_UP,               VPAD_STICK_R_EMULATION_UP);
        ControllerPatcherUtils::setButtonData(&buffer[i],&new_data,VPAD_STICK_R_EMULATION_DOWN,             VPAD_STICK_R_EMULATION_DOWN);

        buffer[i].hold = new_data.hold;
        buffer[i].trigger = new_data.trigger;
        buffer[i].release = new_data.release;
    }
    return CONTROLLER_PATCHER_ERROR_NONE;
}

std::string ControllerPatcher::getIdentifierByVIDPID(u16 vid,u16 pid){
    return ConfigValues::getStringByVIDPID(vid,pid);
}

void ControllerPatcher::destroyConfigHelper(){
    ConfigReader::destroyInstance();
    ConfigValues::destroyInstance();
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcher::doSamplingForDeviceSlot(u16 device_slot){
    return ControllerPatcherUtils::doSampling(device_slot,0,true);
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcher::setRumbleActivated(bool value){
    gGlobalRumbleActivated = value;
    return CONTROLLER_PATCHER_ERROR_NONE;
}

bool ControllerPatcher::isRumbleActivated(){
    return gGlobalRumbleActivated;
}

