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
#include "../ControllerPatcher.hpp"

ControllerMapping gControllerMapping __attribute__((section(".data")));

u8 gConfig_done __attribute__((section(".data"))) = 0;
u8 gButtonRemappingConfigDone __attribute__((section(".data"))) = 0;

u32 gHIDAttached __attribute__((section(".data"))) = 0;
u32 gHIDCurrentDevice __attribute__((section(".data"))) = 0;

u16 gHIDRegisteredDevices __attribute__((section(".data"))) = 0;

HIDClient gHIDClient __attribute__((section(".data")));

HID_DEVICE_DATA gHID_Devices[gHIDMaxDevices] __attribute__((section(".data")));

u8 gHID_Mouse_Mode __attribute__((section(".data"))) = HID_MOUSE_MODE_TOUCH;

u32 gGamePadValues[CONTRPS_MAX_VALUE] __attribute__((section(".data")));

u8  config_controller[gHIDMaxDevices][CONTRPS_MAX_VALUE][2] __attribute__((section(".data")));
u32 config_controller_hidmask[gHIDMaxDevices] __attribute__((section(".data")));

u32 gHID_LIST_GC __attribute__((section(".data"))) = 0;
u32 gHID_LIST_DS3 __attribute__((section(".data"))) = 0;
u32 gHID_LIST_DS4 __attribute__((section(".data"))) = 0;
u32 gHID_LIST_KEYBOARD __attribute__((section(".data"))) = 0;
u32 gHID_LIST_SWITCH_PRO __attribute__((section(".data"))) = 0;
u32 gHID_LIST_MOUSE __attribute__((section(".data"))) = 0;

u16 gGamePadSlot __attribute__((section(".data"))) = 0;
u16 gHID_SLOT_GC __attribute__((section(".data"))) = 0;
u16 gHID_SLOT_KEYBOARD __attribute__((section(".data"))) = 0;
u16 gMouseSlot __attribute__((section(".data"))) = 0;

u8 gOriginalDimState __attribute__((section(".data"))) = 0;
u8 gOriginalAPDState __attribute__((section(".data"))) = 0;

u16 gNetworkController[gHIDMaxDevices][HID_MAX_PADS_COUNT][4] __attribute__((section(".data")));
s32 gHIDNetworkClientID __attribute__((section(".data"))) = 0;
u8 gUsedProtocolVersion  __attribute__((section(".data"))) = WIIU_CP_TCP_HANDSHAKE;

wpad_connect_callback_t gWPADConnectCallback[4] __attribute__((section(".data")));
wpad_connect_callback_t gKPADConnectCallback[4] __attribute__((section(".data")));
wpad_extension_callback_t gExtensionCallback[4] __attribute__((section(".data")));
wpad_sampling_callback_t gSamplingCallback __attribute__((section(".data"))) = 0;
u8 gCallbackCooldown __attribute__((section(".data"))) = 0;

u8 gGlobalRumbleActivated __attribute__((section(".data"))) = 0;

my_cb_user * connectionOrderHelper[gHIDMaxDevices]  __attribute__((section(".data")));

u32 gUDPClientip __attribute__((section(".data"))) = 0;
ControllerMappingPADInfo* gProPadInfo[4] __attribute__((section(".data"))) = {&gControllerMapping.proController[0].pad_infos[0],
                                                                               &gControllerMapping.proController[1].pad_infos[0],
                                                                               &gControllerMapping.proController[2].pad_infos[0],
                                                                               &gControllerMapping.proController[3].pad_infos[0]} ;
