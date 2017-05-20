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
#include "ControllerPatcherUtils.hpp"
#include <math.h>
#include <string.h>

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcherUtils::getDataByHandle(s32 handle, my_cb_user ** data){
    for(s32 i = 0;i< gHIDMaxDevices;i++){
        for(s32 j = 0;j<4;j++){
            //printf("%d %d %d %d\n",i,j,gHID_Devices[i].pad_data[j].handle,(u32)handle);
            if(gHID_Devices[i].pad_data[j].handle == (u32)handle){
                *data = gHID_Devices[i].pad_data[j].user_data;
                return CONTROLLER_PATCHER_ERROR_NONE;
            }
        }
    }
    return CONTROLLER_PATCHER_ERROR_UNKNOWN;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 * Analyse inputs
 *---------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcherUtils::getButtonPressed(HID_Data * data, s32 * buttons_hold, s32 VPADButton){
    if(data == NULL || buttons_hold == NULL) return CONTROLLER_PATCHER_ERROR_NULL_POINTER;

    s32 deviceslot = data->slotdata.deviceslot;

    s32 result = -1;

    do{
        if(data->type == DEVICE_TYPE_MOUSE){
            HID_Mouse_Data *  ms_data = &data->data_union.mouse.cur_mouse_data;
            if(ms_data == NULL) return CONTROLLER_PATCHER_ERROR_NULL_POINTER;
            if(gHID_Mouse_Mode == HID_MOUSE_MODE_TOUCH){
                if(VPADButton == VPAD_BUTTON_TOUCH){
                    if(ms_data->left_click & 0x01){
                            result = 1; break;
                    }
                }
            }else if(gHID_Mouse_Mode == HID_MOUSE_MODE_AIM){
                if(config_controller[deviceslot][CONTRPS_VPAD_BUTTON_LEFT][0] == CONTROLLER_PATCHER_VALUE_SET){
                    if(VPADButton == (int)gGamePadValues[config_controller[deviceslot][CONTRPS_VPAD_BUTTON_LEFT][1]]){
                        if(ms_data->left_click & 0x01){
                            result = 1; break;
                        }
                    }
                }
                if(config_controller[deviceslot][CONTRPS_VPAD_BUTTON_RIGHT][0] == CONTROLLER_PATCHER_VALUE_SET){
                    if(VPADButton == (int)gGamePadValues[config_controller[deviceslot][CONTRPS_VPAD_BUTTON_RIGHT][1]]){
                        if(ms_data->right_click & 0x01){
                            result = 1; break;
                        }
                    }
                }
            }
            result = 0; break;
        }
        u8 * cur_data = &data->data_union.controller.cur_hid_data[0];
        if(cur_data == NULL) return CONTROLLER_PATCHER_ERROR_NULL_POINTER;

        s32 cur_config = 0;

        if(VPADButton == VPAD_BUTTON_A){
            cur_config = CONTRPS_VPAD_BUTTON_A;
        }else if(VPADButton == VPAD_BUTTON_B){
            cur_config = CONTRPS_VPAD_BUTTON_B;
        }else if(VPADButton == VPAD_BUTTON_X){
            cur_config = CONTRPS_VPAD_BUTTON_X;
        }else if(VPADButton == VPAD_BUTTON_Y){
            cur_config = CONTRPS_VPAD_BUTTON_Y;
        }else if(VPADButton == VPAD_BUTTON_L){
            cur_config = CONTRPS_VPAD_BUTTON_L;
        }else if(VPADButton == VPAD_BUTTON_R){
            cur_config = CONTRPS_VPAD_BUTTON_R;
        }else if(VPADButton == VPAD_BUTTON_ZL){
            cur_config = CONTRPS_VPAD_BUTTON_ZL;
        }else if(VPADButton == VPAD_BUTTON_ZR){
            cur_config = CONTRPS_VPAD_BUTTON_ZR;
        }else if(VPADButton == VPAD_BUTTON_STICK_L){
            cur_config = CONTRPS_VPAD_BUTTON_STICK_L;
        }else if(VPADButton == VPAD_BUTTON_STICK_R){
            cur_config = CONTRPS_VPAD_BUTTON_STICK_R;
        }else if(VPADButton == VPAD_BUTTON_PLUS){
            cur_config = CONTRPS_VPAD_BUTTON_PLUS;
        }else if(VPADButton == VPAD_BUTTON_MINUS){
            cur_config = CONTRPS_VPAD_BUTTON_MINUS;
        }else if(VPADButton == VPAD_BUTTON_HOME){
            cur_config = CONTRPS_VPAD_BUTTON_HOME;
        }

        //! Special DPAD treatment.
        if(config_controller[deviceslot][CONTRPS_DPAD_MODE][0] == CONTROLLER_PATCHER_VALUE_SET){
            if(config_controller[deviceslot][CONTRPS_DPAD_MODE][1] == CONTRPDM_Hat){
                u8 mask = 0x0F;
                if(config_controller[deviceslot][CONTRPS_DPAD_MASK][0] == CONTROLLER_PATCHER_VALUE_SET){
                   mask = config_controller[deviceslot][CONTRPS_DPAD_MASK][1];
                }
                if(cur_data[config_controller[deviceslot][CONTRPS_VPAD_BUTTON_DPAD_NEUTRAL][0]] != config_controller[deviceslot][CONTRPS_VPAD_BUTTON_DPAD_NEUTRAL][1]){ // Not neutral
                    u8 dir1_0 = 0,dir1_1 = 0;
                    u8 dir2_0 = 0,dir2_1 = 0;
                    u8 dir3_0 = 0,dir3_1 = 0;
                    u8 direction = 0;
                    if(VPADButton == VPAD_BUTTON_LEFT){
                        dir1_0 = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_DPAD_W][0];
                        dir2_0 = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_DPAD_NW][0];
                        dir3_0 = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_DPAD_SW][0];
                        dir1_1 = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_DPAD_W][1];
                        dir2_1 = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_DPAD_NW][1];
                        dir3_1 = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_DPAD_SW][1];
                        direction = 1;
                    }else if(VPADButton == VPAD_BUTTON_RIGHT){
                        dir1_0 = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_DPAD_E][0];
                        dir2_0 = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_DPAD_SE][0];
                        dir3_0 = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_DPAD_NE][0];
                        dir1_1 = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_DPAD_E][1];
                        dir2_1 = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_DPAD_SE][1];
                        dir3_1 = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_DPAD_NE][1];
                        direction = 1;
                    }else if(VPADButton == VPAD_BUTTON_DOWN){
                        dir1_0 = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_DPAD_S][0];
                        dir2_0 = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_DPAD_SE][0];
                        dir3_0 = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_DPAD_SW][0];
                        dir1_1 = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_DPAD_S][1];
                        dir2_1 = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_DPAD_SE][1];
                        dir3_1 = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_DPAD_SW][1];
                        direction = 1;
                    }else if(VPADButton == VPAD_BUTTON_UP){
                        dir1_0 = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_DPAD_N][0];
                        dir2_0 = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_DPAD_NW][0];
                        dir3_0 = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_DPAD_NE][0];
                        dir1_1 = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_DPAD_N][1];
                        dir2_1 = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_DPAD_NW][1];
                        dir3_1 = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_DPAD_NE][1];
                        direction = 1;
                    }
                    if(direction && (((cur_data[dir1_0] & mask) == dir1_1) ||
                            ((cur_data[dir2_0] & mask) == dir2_1) ||
                            ((cur_data[dir3_0] & mask) == dir3_1))) {result = 1; break;}
                }

            }else  if(config_controller[deviceslot][CONTRPS_DPAD_MODE][1] == CONTRPDM_Absolute_2Values){
                s32 contrps_value = 0;
                if(VPADButton == VPAD_BUTTON_LEFT){
                    contrps_value = CONTRPS_VPAD_BUTTON_DPAD_ABS_LEFT;
                }else if(VPADButton == VPAD_BUTTON_RIGHT){
                    contrps_value = CONTRPS_VPAD_BUTTON_DPAD_ABS_RIGHT;
                }else if(VPADButton == VPAD_BUTTON_UP){
                    contrps_value = CONTRPS_VPAD_BUTTON_DPAD_ABS_UP;
                }else if(VPADButton == VPAD_BUTTON_DOWN){
                    contrps_value = CONTRPS_VPAD_BUTTON_DPAD_ABS_DOWN;
                }

                if(contrps_value != 0){
                    s32 value_byte = CONTROLLER_PATCHER_INVALIDVALUE;
                    if((value_byte = config_controller[deviceslot][contrps_value][0]) != CONTROLLER_PATCHER_INVALIDVALUE){
                        if(cur_data[config_controller[deviceslot][contrps_value][0]] == config_controller[deviceslot][contrps_value][1]){
                            result = 1;
                            break;
                        }
                    }
                }
            }
        }

        //! Normal DPAD treatment.
        if(VPADButton == VPAD_BUTTON_LEFT){
            cur_config = CONTRPS_VPAD_BUTTON_LEFT;
        }else if(VPADButton == VPAD_BUTTON_RIGHT){
            cur_config = CONTRPS_VPAD_BUTTON_RIGHT;
        }else if(VPADButton == VPAD_BUTTON_DOWN){
            cur_config = CONTRPS_VPAD_BUTTON_DOWN;
        }else if(VPADButton == VPAD_BUTTON_UP){
            cur_config = CONTRPS_VPAD_BUTTON_UP;
        }
        if(result && config_controller[deviceslot][CONTRPS_DOUBLE_USE][0] == CONTROLLER_PATCHER_VALUE_SET){
            if(config_controller[deviceslot][CONTRPS_DOUBLE_USE][1] == CONTROLLER_PATCHER_GC_DOUBLE_USE){
                if(cur_data[config_controller[deviceslot][CONTRPS_DOUBLE_USE_BUTTON_ACTIVATOR][0]] & config_controller[deviceslot][CONTRPS_DOUBLE_USE_BUTTON_ACTIVATOR][1]){
                    if(checkValueinConfigController(deviceslot,CONTRPS_DOUBLE_USE_BUTTON_1_RELEASED,cur_config)){result = 0; break;}
                    if(checkValueinConfigController(deviceslot,CONTRPS_DOUBLE_USE_BUTTON_2_RELEASED,cur_config)){result = 0; break;}
                    if(checkValueinConfigController(deviceslot,CONTRPS_DOUBLE_USE_BUTTON_3_RELEASED,cur_config)){result = 0; break;}
                    if(checkValueinConfigController(deviceslot,CONTRPS_DOUBLE_USE_BUTTON_4_RELEASED,cur_config)){result = 0; break;}
                    if(checkValueinConfigController(deviceslot,CONTRPS_DOUBLE_USE_BUTTON_5_RELEASED,cur_config)){result = 0; break;}
                }else{
                    if(checkValueinConfigController(deviceslot,CONTRPS_DOUBLE_USE_BUTTON_1_PRESSED,cur_config)){result = 0; break;}
                    if(checkValueinConfigController(deviceslot,CONTRPS_DOUBLE_USE_BUTTON_2_PRESSED,cur_config)){result = 0; break;}
                    if(checkValueinConfigController(deviceslot,CONTRPS_DOUBLE_USE_BUTTON_3_PRESSED,cur_config)){result = 0; break;}
                    if(checkValueinConfigController(deviceslot,CONTRPS_DOUBLE_USE_BUTTON_4_PRESSED,cur_config)){result = 0; break;}
                    if(checkValueinConfigController(deviceslot,CONTRPS_DOUBLE_USE_BUTTON_5_PRESSED,cur_config)){result = 0; break;}
                }
            }
        }

        if(isValueSet(data,cur_config) == 1){
            result = 1; break;
        }else{
            //printf("Invalid data! deviceslot(slot): %d config: %d\n",deviceslot,cur_config);
        }
    }while(0); //The break will become handy ;)


    if(result == 1){
        *buttons_hold |= VPADButton; // -1 would be also true.
        return 1;
    }
    return CONTROLLER_PATCHER_ERROR_NONE;
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcherUtils::isValueSet(HID_Data * data,s32 cur_config){
    if(data == NULL) return CONTROLLER_PATCHER_ERROR_NULL_POINTER;

    u8 * cur_data = &data->data_union.controller.cur_hid_data[0];
    if(cur_data == NULL) return CONTROLLER_PATCHER_ERROR_NULL_POINTER;

    u32 hidmask = data->slotdata.hidmask;
    s32 deviceslot = data->slotdata.deviceslot;

    s32 result = CONTROLLER_PATCHER_ERROR_NONE;
    if(config_controller[deviceslot][cur_config][0] != CONTROLLER_PATCHER_INVALIDVALUE){ //Invalid data
        if(hidmask & gHID_LIST_KEYBOARD){
            if(isInKeyboardData(cur_data,config_controller[deviceslot][cur_config][1]) > 0) {
                result = 1;
            }
        }else{
            if((cur_data[config_controller[deviceslot][cur_config][0]] & config_controller[deviceslot][cur_config][1]) == config_controller[deviceslot][cur_config][1]){
                result = 1;
            }
        }
    }
    return result;
 }
CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcherUtils::isInKeyboardData(unsigned char * keyboardData,s32 key){
    if(keyboardData == NULL) return CONTROLLER_PATCHER_ERROR_NULL_POINTER;
    for(s32 i = 0;i<HID_KEYBOARD_DATA_LENGTH;i++){
        if(keyboardData[i] == 0 && i > 1){
             break;
        }else if (keyboardData[i] == key){
            return 1;
        }
    }
    return 0;
}

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 * Utils for setting the Button data
 *---------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcherUtils::setButtonRemappingData(VPADStatus * old_buffer, VPADStatus * new_buffer,u32 VPADButton, s32 CONTRPS_SLOT){
    if(old_buffer == NULL || new_buffer == NULL) return CONTROLLER_PATCHER_ERROR_NULL_POINTER;
    u32 new_value = VPADButton;

    if(config_controller[gGamePadSlot][CONTRPS_SLOT][0] != CONTROLLER_PATCHER_INVALIDVALUE){ //using new value!
       new_value = gGamePadValues[config_controller[gGamePadSlot][CONTRPS_SLOT][1]];
    }

    setButtonData(old_buffer,new_buffer,VPADButton,new_value);
    return CONTROLLER_PATCHER_ERROR_NONE;
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcherUtils::setButtonData(VPADStatus * old_buffer, VPADStatus * new_buffer,u32 oldVPADButton,u32 newVPADButton){
    if(old_buffer == NULL || new_buffer == NULL) return CONTROLLER_PATCHER_ERROR_NULL_POINTER;
    if((old_buffer->hold & oldVPADButton) == oldVPADButton){
        new_buffer->hold |= newVPADButton;
    }
    if((old_buffer->release & oldVPADButton) == oldVPADButton){
        new_buffer->release |= newVPADButton;
    }
    if((old_buffer->trigger & oldVPADButton) == oldVPADButton){
        new_buffer->trigger |= newVPADButton;
    }

    return CONTROLLER_PATCHER_ERROR_NONE;
}

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 * Pad Status functions
 *---------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcherUtils::checkActivePad(u32 hidmask,s32 pad){
    if(hidmask & gHID_LIST_GC && pad >= 0 && pad <= 3){
        if (!(((gHID_Devices[gHID_SLOT_GC].pad_data[pad].data_union.controller.cur_hid_data[0] & 0x10) == 0) && ((gHID_Devices[gHID_SLOT_GC].pad_data[pad].data_union.controller.cur_hid_data[0] & 0x22) != 0x22))) return 1;
        return CONTROLLER_PATCHER_ERROR_NO_PAD_CONNECTED;
    }else{
        s32 deviceslot = getDeviceSlot(hidmask);
        if(deviceslot < 0 ) return CONTROLLER_PATCHER_ERROR_DEVICE_SLOT_NOT_FOUND;
        s32 connected_pads = config_controller[deviceslot][CONTRPS_CONNECTED_PADS][1];

        if((connected_pads & (1 << pad)) > 0){
            return 1;
        }
    }
    return CONTROLLER_PATCHER_ERROR_NO_PAD_CONNECTED;
}

/*
CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcherUtils::getActivePad(u32 hidmask){
     if(hidmask & gHID_LIST_GC){
        if (!(((gHID_Devices[gHID_SLOT_GC].pad_data[0].data_union.controller.cur_hid_data[0] & 0x10) == 0) && ((gHID_Devices[gHID_SLOT_GC].pad_data[0].data_union.controller.cur_hid_data[0] & 0x22) != 0x22))) return 0;
        if (!(((gHID_Devices[gHID_SLOT_GC].pad_data[1].data_union.controller.cur_hid_data[0] & 0x10) == 0) && ((gHID_Devices[gHID_SLOT_GC].pad_data[1].data_union.controller.cur_hid_data[0] & 0x22) != 0x22))) return 1;
        if (!(((gHID_Devices[gHID_SLOT_GC].pad_data[2].data_union.controller.cur_hid_data[0] & 0x10) == 0) && ((gHID_Devices[gHID_SLOT_GC].pad_data[2].data_union.controller.cur_hid_data[0] & 0x22) != 0x22))) return 2;
        if (!(((gHID_Devices[gHID_SLOT_GC].pad_data[3].data_union.controller.cur_hid_data[0] & 0x10) == 0) && ((gHID_Devices[gHID_SLOT_GC].pad_data[3].data_union.controller.cur_hid_data[0] & 0x22) != 0x22))) return 3;

        return CONTROLLER_PATCHER_ERROR_NO_PAD_CONNECTED;
     }
     return 0;
}*/

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 * Stick functions
 *---------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcherUtils::normalizeStickValues(VPADVec2D * stick){
    if(stick == NULL) return CONTROLLER_PATCHER_ERROR_NULL_POINTER;

    f32 max_val = 0.0f;
    f32 mul_val = 0.0f;

    if((max_val = (fabs(stick->x)) + fabs(stick->y)) > 1.414f){
        mul_val = 1.414f / max_val;
        stick->x *= mul_val;
        stick->y *= mul_val;
    }

    if(stick->x > 1.0f){ stick->x = 1.0f; }
    if(stick->y > 1.0f){ stick->y = 1.0f; }
    if(stick->x < -1.0f){ stick->x = -1.0f; }
    if(stick->y < -1.0f){ stick->y = -1.0f; }

    return CONTROLLER_PATCHER_ERROR_NONE;
}

f32 ControllerPatcherUtils::convertAnalogValue(u8 value, u8 default_val, u8 min, u8 max, u8 invert,u8 deadzone){
    s8 new_value = (s8)(value - default_val);
    u8 range = 0;
    if(value >= max){
        if(invert == 0x01) return -1.0f;
        return 1.0f;
    }else if(value <= min){
        if(invert == 0x01) return 1.0f;
        return -1.0f;
    }
    if((value-deadzone) > default_val){
        new_value -= deadzone;
        range = (max - (default_val + deadzone));
    }else if((value+deadzone) < default_val){
        new_value += deadzone;
        range = ((default_val - deadzone) - min);
    }else{
        return 0.0f;
    }
    if(invert != 0x01){
        return (new_value / (1.0f*range));
    }else{
        return -1.0f*(new_value / (1.0f*range));
    }
}

VPADVec2D ControllerPatcherUtils::getAnalogValueByButtons(u8 stick_values){
    VPADVec2D stick;
    stick.x = 0.0f;
    stick.y = 0.0f;

    u8 up =     ((stick_values & STICK_VALUE_UP) == STICK_VALUE_UP);
    u8 down =   ((stick_values & STICK_VALUE_DOWN) == STICK_VALUE_DOWN);
    u8 left =   ((stick_values & STICK_VALUE_LEFT) == STICK_VALUE_LEFT);
    u8 right =  ((stick_values & STICK_VALUE_RIGHT) == STICK_VALUE_RIGHT);

    if(up){
        if(!down){
            stick.y = 1.0f;
        }
        if(left || right){
            stick.y = 0.707f;
            if(left) stick.x = -0.707f;
            if(right) stick.x = 0.707f;
        }
    }else if(down){
        if(!up){
            stick.y = -1.0f;
        }
        if(left || right){
            stick.y = -0.707f;
            if(left) stick.x = -0.707f;
            if(right) stick.x = 0.707f;
        }
    }else{
        if(left){
             if(!right){
                stick.x = -1.0f;
             }

        }else if(right){
            if(!down){
                stick.x = 1.0f;
             }
        }
    }
    return stick;
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcherUtils::convertAnalogSticks(HID_Data * data, VPADStatus * buffer){
    if(buffer == NULL || data == NULL) return CONTROLLER_PATCHER_ERROR_NULL_POINTER;

    s32 deviceslot = data->slotdata.deviceslot;
    if (data->type == DEVICE_TYPE_MOUSE){

        if(gHID_Mouse_Mode == HID_MOUSE_MODE_AIM){ // TODO: tweak values
            HID_Mouse_Data * ms_data = &data->data_union.mouse.cur_mouse_data;
            if(ms_data == NULL) return CONTROLLER_PATCHER_ERROR_NULL_POINTER;

            f32 x_value = ms_data->deltaX/10.0f;
            f32 y_value = -1.0f*(ms_data->deltaY/10.0f);

            if(config_controller[deviceslot][CONTRPS_MOUSE_STICK][0] != CONTROLLER_PATCHER_INVALIDVALUE){
                if(config_controller[deviceslot][CONTRPS_MOUSE_STICK][1] == DEF_L_STICK){
                    buffer->leftStick.x += x_value;
                    buffer->leftStick.y += y_value;
                    return CONTROLLER_PATCHER_ERROR_NONE;
                }
            }

            buffer->rightStick.x += x_value;
            buffer->rightStick.y += y_value;
        }
    }else{
        u8 * cur_data = &data->data_union.controller.cur_hid_data[0];
        if(cur_data == NULL) return CONTROLLER_PATCHER_ERROR_NULL_POINTER;

        s32 deadzone = 0;

        if( config_controller[deviceslot][CONTRPS_VPAD_BUTTON_L_STICK_X][0] != CONTROLLER_PATCHER_INVALIDVALUE){
            if(config_controller[deviceslot][CONTRPS_VPAD_BUTTON_L_STICK_X_DEADZONE][0] == CONTROLLER_PATCHER_VALUE_SET){
                 deadzone = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_L_STICK_X_DEADZONE][1];
            }

            buffer->leftStick.x += convertAnalogValue(cur_data[config_controller[deviceslot][CONTRPS_VPAD_BUTTON_L_STICK_X][0]],
                                                   config_controller[deviceslot][CONTRPS_VPAD_BUTTON_L_STICK_X][1],
                                                   config_controller[deviceslot][CONTRPS_VPAD_BUTTON_L_STICK_X_MINMAX][0],
                                                   config_controller[deviceslot][CONTRPS_VPAD_BUTTON_L_STICK_X_MINMAX][1],
                                                   config_controller[deviceslot][CONTRPS_VPAD_BUTTON_L_STICK_X_INVERT][1],
                                                   deadzone);
        }

        if( config_controller[deviceslot][CONTRPS_VPAD_BUTTON_L_STICK_Y][0] != CONTROLLER_PATCHER_INVALIDVALUE){
            deadzone = 0;
            if(config_controller[deviceslot][CONTRPS_VPAD_BUTTON_L_STICK_Y_DEADZONE][0] == CONTROLLER_PATCHER_VALUE_SET){
                 deadzone = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_L_STICK_Y_DEADZONE][1];
            }
            buffer->leftStick.y += convertAnalogValue(cur_data[config_controller[deviceslot][CONTRPS_VPAD_BUTTON_L_STICK_Y][0]],
                                                   config_controller[deviceslot][CONTRPS_VPAD_BUTTON_L_STICK_Y][1],
                                                   config_controller[deviceslot][CONTRPS_VPAD_BUTTON_L_STICK_Y_MINMAX][0],
                                                   config_controller[deviceslot][CONTRPS_VPAD_BUTTON_L_STICK_Y_MINMAX][1],
                                                   config_controller[deviceslot][CONTRPS_VPAD_BUTTON_L_STICK_Y_INVERT][1],
                                                   deadzone);
        }

        if( config_controller[deviceslot][CONTRPS_VPAD_BUTTON_R_STICK_X][0] != CONTROLLER_PATCHER_INVALIDVALUE){
            deadzone = 0;
            if(config_controller[deviceslot][CONTRPS_VPAD_BUTTON_R_STICK_X_DEADZONE][0] == CONTROLLER_PATCHER_VALUE_SET){
                 deadzone = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_R_STICK_X_DEADZONE][1];
            }

            buffer->rightStick.x += convertAnalogValue(cur_data[config_controller[deviceslot][CONTRPS_VPAD_BUTTON_R_STICK_X][0]],
                                                   config_controller[deviceslot][CONTRPS_VPAD_BUTTON_R_STICK_X][1],
                                                   config_controller[deviceslot][CONTRPS_VPAD_BUTTON_R_STICK_X_MINMAX][0],
                                                   config_controller[deviceslot][CONTRPS_VPAD_BUTTON_R_STICK_X_MINMAX][1],
                                                   config_controller[deviceslot][CONTRPS_VPAD_BUTTON_R_STICK_X_INVERT][1],
                                                   deadzone);
        }

        if( config_controller[deviceslot][CONTRPS_VPAD_BUTTON_R_STICK_Y][0] != CONTROLLER_PATCHER_INVALIDVALUE){
            deadzone = 0;
            if(config_controller[deviceslot][CONTRPS_VPAD_BUTTON_R_STICK_Y_DEADZONE][0] == CONTROLLER_PATCHER_VALUE_SET){
                 deadzone = config_controller[deviceslot][CONTRPS_VPAD_BUTTON_R_STICK_Y_DEADZONE][1];
            }

            buffer->rightStick.y += convertAnalogValue(cur_data[config_controller[deviceslot][CONTRPS_VPAD_BUTTON_R_STICK_Y][0]],
                                                   config_controller[deviceslot][CONTRPS_VPAD_BUTTON_R_STICK_Y][1],
                                                   config_controller[deviceslot][CONTRPS_VPAD_BUTTON_R_STICK_Y_MINMAX][0],
                                                   config_controller[deviceslot][CONTRPS_VPAD_BUTTON_R_STICK_Y_MINMAX][1],
                                                   config_controller[deviceslot][CONTRPS_VPAD_BUTTON_R_STICK_Y_INVERT][1],
                                                   deadzone);
        }

        u8 stick_values = 0;

        if(isValueSet(data,CONTRPS_VPAD_BUTTON_L_STICK_UP)){    stick_values |= STICK_VALUE_UP; }
        if(isValueSet(data,CONTRPS_VPAD_BUTTON_L_STICK_DOWN)){  stick_values |= STICK_VALUE_DOWN; }
        if(isValueSet(data,CONTRPS_VPAD_BUTTON_L_STICK_LEFT)){  stick_values |= STICK_VALUE_LEFT; }
        if(isValueSet(data,CONTRPS_VPAD_BUTTON_L_STICK_RIGHT)){ stick_values |= STICK_VALUE_RIGHT; }

        if(stick_values > 0 ){
            VPADVec2D stick = getAnalogValueByButtons(stick_values);
            buffer->leftStick.x += stick.x;
            buffer->leftStick.y += stick.y;
        }

        stick_values = 0;
        if(isValueSet(data,CONTRPS_VPAD_BUTTON_R_STICK_UP)){    stick_values |= STICK_VALUE_UP; }
        if(isValueSet(data,CONTRPS_VPAD_BUTTON_R_STICK_DOWN)){  stick_values |= STICK_VALUE_DOWN; }
        if(isValueSet(data,CONTRPS_VPAD_BUTTON_R_STICK_LEFT)){  stick_values |= STICK_VALUE_LEFT; }
        if(isValueSet(data,CONTRPS_VPAD_BUTTON_R_STICK_RIGHT)){ stick_values |= STICK_VALUE_RIGHT; }

        if(stick_values > 0 ){
            VPADVec2D stick = getAnalogValueByButtons(stick_values);
            buffer->rightStick.x += stick.x;
            buffer->rightStick.y += stick.y;
        }

        /*printf("LX %f(%02X) LY %f(%02X) RX %f(%02X) RY %f(%02X)\n",buffer->leftStick.x,cur_data[config_controller[deviceslot][CONTRPS_VPAD_BUTTON_L_STICK_X][0]],
                                                               buffer->leftStick.y,cur_data[config_controller[deviceslot][CONTRPS_VPAD_BUTTON_L_STICK_Y][0]],
                                                               buffer->rightStick.x,cur_data[config_controller[deviceslot][CONTRPS_VPAD_BUTTON_R_STICK_X][0]],
                                                               buffer->rightStick.y,cur_data[config_controller[deviceslot][CONTRPS_VPAD_BUTTON_R_STICK_Y][0]]);*/

    }
    return CONTROLLER_PATCHER_ERROR_NONE;
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcherUtils::setEmulatedSticks(VPADStatus * buffer, u32 * last_emulatedSticks){
    if(buffer == NULL || last_emulatedSticks == NULL) return CONTROLLER_PATCHER_ERROR_NULL_POINTER;

    u32 emulatedSticks = 0;

    s32 l_x_full = (buffer->leftStick.x > 0.5f || buffer->leftStick.x < -0.5f)? 1:0;
    s32 l_y_full = (buffer->leftStick.y > 0.5f || buffer->leftStick.y < -0.5f)? 1:0;
    s32 r_x_full = (buffer->rightStick.x > 0.5f || buffer->rightStick.x < -0.5f)? 1:0;
    s32 r_y_full = (buffer->rightStick.y > 0.5f || buffer->rightStick.y < -0.5f)? 1:0;

    if((buffer->leftStick.x > 0.5f) || (buffer->leftStick.x > 0.1f && !l_y_full)){
        emulatedSticks |= VPAD_STICK_L_EMULATION_RIGHT;
    }
    if((buffer->leftStick.x < -0.5f) || (buffer->leftStick.x < -0.1f && !l_y_full)){
        emulatedSticks |= VPAD_STICK_L_EMULATION_LEFT;
    }
    if((buffer->leftStick.y > 0.5f) || (buffer->leftStick.y > 0.1f && !l_x_full)){
        emulatedSticks |= VPAD_STICK_L_EMULATION_UP;
    }
    if((buffer->leftStick.y < -0.5f) || (buffer->leftStick.y < -0.1f && !l_x_full)){
        emulatedSticks |= VPAD_STICK_L_EMULATION_DOWN;
    }

    if((buffer->rightStick.x > 0.5f) || (buffer->rightStick.x > 0.1f && !r_y_full)){
        emulatedSticks |= VPAD_STICK_R_EMULATION_RIGHT;
    }
    if((buffer->rightStick.x < -0.5f) || (buffer->rightStick.x < -0.1f && !r_y_full)){
        emulatedSticks |= VPAD_STICK_R_EMULATION_LEFT;
    }
    if((buffer->rightStick.y > 0.5f) || (buffer->rightStick.y > 0.1f && !r_x_full)){
        emulatedSticks |= VPAD_STICK_R_EMULATION_UP;
    }
    if((buffer->rightStick.y < -0.5f) || (buffer->rightStick.y < -0.1f && !r_x_full)){
        emulatedSticks |= VPAD_STICK_R_EMULATION_DOWN;
    }

    //Setting the emulated sticks
    buffer->hold |= emulatedSticks;
    buffer->trigger |= (emulatedSticks & (~*last_emulatedSticks));
    buffer->release |= (*last_emulatedSticks & (~emulatedSticks));

    *last_emulatedSticks = emulatedSticks;

    return CONTROLLER_PATCHER_ERROR_NONE;
}

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 * Touch functions
 *---------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcherUtils::setTouch(HID_Data * data,VPADStatus * buffer){
    if(buffer == NULL || data == NULL) return CONTROLLER_PATCHER_ERROR_NULL_POINTER;
    if(data->type == DEVICE_TYPE_MOUSE && gHID_Mouse_Mode == HID_MOUSE_MODE_TOUCH){
        s32 buttons_hold;
        if(getButtonPressed(data,&buttons_hold,VPAD_BUTTON_TOUCH)){
           HID_Mouse_Data *  ms_data = &data->data_union.mouse.cur_mouse_data;
           if(ms_data == NULL) return CONTROLLER_PATCHER_ERROR_NULL_POINTER;
           s32 x_mouse = 80 + ((int)(((ms_data->X)*1.0f/1280.0)*3890.0f));
           s32 y_mouse = 3910 - ((int)(((ms_data->Y)*1.0f/720.0)*3760.0f));
           buffer->tpNormal.x = x_mouse;
           buffer->tpNormal.y = y_mouse;
           buffer->tpNormal.touched = 1;
           buffer->tpNormal.validity = 0;
           buffer->tpFiltered1.x = x_mouse;
           buffer->tpFiltered1.y = y_mouse;
           buffer->tpFiltered1.touched = 1;
           buffer->tpFiltered1.validity = 0;
           buffer->tpFiltered2.x = x_mouse;
           buffer->tpFiltered2.y = y_mouse;
           buffer->tpFiltered2.touched = 1;
           buffer->tpFiltered2.validity = 0;
        }
    }
    return CONTROLLER_PATCHER_ERROR_NONE;
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcherUtils::checkAndSetMouseMode(HID_Data * data){
    u32 hidmask = data->slotdata.hidmask;

    if(hidmask & gHID_LIST_KEYBOARD){
        u8 * cur_data = &data->data_union.controller.cur_hid_data[0];
        u8 * last_data = &data->data_union.controller.last_hid_data[0];
        if((isInKeyboardData(cur_data,HID_KEYBOARD_BUTTON_F1) > 0) && ((isInKeyboardData(cur_data,HID_KEYBOARD_BUTTON_F1) > 0) != (isInKeyboardData(last_data,HID_KEYBOARD_BUTTON_F1) > 0))){
            if(gHID_Mouse_Mode == HID_MOUSE_MODE_AIM){
                gHID_Mouse_Mode = HID_MOUSE_MODE_TOUCH;
                if(HID_DEBUG){ printf("ControllerPatcherUtils::checkAndSetMouseMode(line %d): Mouse mode changed! to touch \n",__LINE__); }
            }else if(gHID_Mouse_Mode == HID_MOUSE_MODE_TOUCH){
                if(HID_DEBUG){ printf("ControllerPatcherUtils::checkAndSetMouseMode(line %d): Mouse mode changed! to aim \n",__LINE__); }
                gHID_Mouse_Mode = HID_MOUSE_MODE_AIM;
            }
        }
    }
    return CONTROLLER_PATCHER_ERROR_NONE;
}

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 * Other functions
 *---------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcherUtils::translateToPro(VPADStatus * vpad_buffer,KPADData * pro_buffer,u32 * lastButtonsPressesPRO){
    if(vpad_buffer == NULL || pro_buffer == NULL || lastButtonsPressesPRO == NULL) return CONTROLLER_PATCHER_ERROR_NULL_POINTER;

    s32 buttons_hold = 0;

    if(vpad_buffer->hold & VPAD_BUTTON_A)                 buttons_hold |= WPAD_PRO_BUTTON_A;
    if(vpad_buffer->hold & VPAD_BUTTON_B)                 buttons_hold |= WPAD_PRO_BUTTON_B;
    if(vpad_buffer->hold & VPAD_BUTTON_X)                 buttons_hold |= WPAD_PRO_BUTTON_X;
    if(vpad_buffer->hold & VPAD_BUTTON_Y)                 buttons_hold |= WPAD_PRO_BUTTON_Y;

    if(vpad_buffer->hold & VPAD_BUTTON_PLUS)              buttons_hold |= WPAD_PRO_BUTTON_PLUS;
    if(vpad_buffer->hold & VPAD_BUTTON_MINUS)             buttons_hold |= WPAD_PRO_BUTTON_MINUS;
    if(vpad_buffer->hold & VPAD_BUTTON_HOME)              buttons_hold |= WPAD_PRO_BUTTON_HOME;

    if(vpad_buffer->hold & VPAD_BUTTON_LEFT)              buttons_hold |= WPAD_PRO_BUTTON_LEFT;
    if(vpad_buffer->hold & VPAD_BUTTON_RIGHT)             buttons_hold |= WPAD_PRO_BUTTON_RIGHT;
    if(vpad_buffer->hold & VPAD_BUTTON_UP)                buttons_hold |= WPAD_PRO_BUTTON_UP;
    if(vpad_buffer->hold & VPAD_BUTTON_DOWN)              buttons_hold |= WPAD_PRO_BUTTON_DOWN;

    if(vpad_buffer->hold & VPAD_BUTTON_L)                 buttons_hold |= WPAD_PRO_TRIGGER_L;
    if(vpad_buffer->hold & VPAD_BUTTON_ZL)                buttons_hold |= WPAD_PRO_TRIGGER_ZL;

    if(vpad_buffer->hold & VPAD_BUTTON_R)                 buttons_hold |= WPAD_PRO_TRIGGER_R;
    if(vpad_buffer->hold & VPAD_BUTTON_ZR)                buttons_hold |= WPAD_PRO_TRIGGER_ZR;

    if(vpad_buffer->hold & VPAD_BUTTON_STICK_L)           buttons_hold |= WPAD_PRO_BUTTON_STICK_L;
    if(vpad_buffer->hold & VPAD_BUTTON_STICK_R)           buttons_hold |= WPAD_PRO_BUTTON_STICK_R;

    if(vpad_buffer->hold & VPAD_STICK_L_EMULATION_LEFT)   buttons_hold |= WPAD_PRO_STICK_L_EMULATION_LEFT;
    if(vpad_buffer->hold & VPAD_STICK_L_EMULATION_RIGHT)  buttons_hold |= WPAD_PRO_STICK_L_EMULATION_RIGHT;
    if(vpad_buffer->hold & VPAD_STICK_L_EMULATION_UP)     buttons_hold |= WPAD_PRO_STICK_L_EMULATION_UP;
    if(vpad_buffer->hold & VPAD_STICK_L_EMULATION_DOWN)   buttons_hold |= WPAD_PRO_STICK_L_EMULATION_DOWN;

    if(vpad_buffer->hold & VPAD_STICK_R_EMULATION_LEFT)   buttons_hold |= WPAD_PRO_STICK_R_EMULATION_LEFT;
    if(vpad_buffer->hold & VPAD_STICK_R_EMULATION_RIGHT)  buttons_hold |= WPAD_PRO_STICK_R_EMULATION_RIGHT;
    if(vpad_buffer->hold & VPAD_STICK_R_EMULATION_UP)     buttons_hold |= WPAD_PRO_STICK_R_EMULATION_UP;
    if(vpad_buffer->hold & VPAD_STICK_R_EMULATION_DOWN)   buttons_hold |= WPAD_PRO_STICK_R_EMULATION_DOWN;

    pro_buffer->pro.lstick_x = vpad_buffer->leftStick.x;
    pro_buffer->pro.lstick_x = vpad_buffer->leftStick.y;
    pro_buffer->pro.rstick_x = vpad_buffer->rightStick.x;
    pro_buffer->pro.rstick_x = vpad_buffer->rightStick.y;

    /*
    pro_buffer->unused_1[1] = 0xBF800000;
    pro_buffer->unused_1[3] = 0x3F800000;
    pro_buffer->angle_x = 0x3F800000;
    pro_buffer->unused_3[4] = 0x3F800000;
    pro_buffer->unused_3[7] = 0x3F800000;
    pro_buffer->unused_6[17] = 0x3F800000;
    pro_buffer->unused_7[1] = 0x3F800000;
    pro_buffer->unused_7[5] = 0x3F800000;*/

    pro_buffer->pro.hold = buttons_hold;
    pro_buffer->pro.trigger = (buttons_hold & (~*lastButtonsPressesPRO));
    pro_buffer->pro.release = (*lastButtonsPressesPRO & (~buttons_hold));

    *lastButtonsPressesPRO = buttons_hold;

    pro_buffer->format = WPAD_FMT_PRO_CONTROLLER;
    pro_buffer->wpad_error = 0x00;
    pro_buffer->device_type = WPAD_EXT_PRO_CONTROLLER;
    pro_buffer->pro.wired = 1;
    pro_buffer->pro.charging = 1;

    return CONTROLLER_PATCHER_ERROR_NONE;
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcherUtils::translateToProWPADRead(VPADStatus * vpad_buffer,WPADReadData * pro_buffer){
    if(vpad_buffer == NULL || pro_buffer == NULL) return CONTROLLER_PATCHER_ERROR_NULL_POINTER;

    s32 buttons_hold = 0;

    if(vpad_buffer->hold & VPAD_BUTTON_A)                 buttons_hold |= WPAD_PRO_BUTTON_A;
    if(vpad_buffer->hold & VPAD_BUTTON_B)                 buttons_hold |= WPAD_PRO_BUTTON_B;
    if(vpad_buffer->hold & VPAD_BUTTON_X)                 buttons_hold |= WPAD_PRO_BUTTON_X;
    if(vpad_buffer->hold & VPAD_BUTTON_Y)                 buttons_hold |= WPAD_PRO_BUTTON_Y;

    if(vpad_buffer->hold & VPAD_BUTTON_PLUS)              buttons_hold |= WPAD_PRO_BUTTON_PLUS;
    if(vpad_buffer->hold & VPAD_BUTTON_MINUS)             buttons_hold |= WPAD_PRO_BUTTON_MINUS;
    if(vpad_buffer->hold & VPAD_BUTTON_HOME)              buttons_hold |= WPAD_PRO_BUTTON_HOME;

    if(vpad_buffer->hold & VPAD_BUTTON_LEFT)              buttons_hold |= WPAD_PRO_BUTTON_LEFT;
    if(vpad_buffer->hold & VPAD_BUTTON_RIGHT)             buttons_hold |= WPAD_PRO_BUTTON_RIGHT;
    if(vpad_buffer->hold & VPAD_BUTTON_UP)                buttons_hold |= WPAD_PRO_BUTTON_UP;
    if(vpad_buffer->hold & VPAD_BUTTON_DOWN)              buttons_hold |= WPAD_PRO_BUTTON_DOWN;

    if(vpad_buffer->hold & VPAD_BUTTON_L)                 buttons_hold |= WPAD_PRO_TRIGGER_L;
    if(vpad_buffer->hold & VPAD_BUTTON_ZL)                buttons_hold |= WPAD_PRO_TRIGGER_ZL;

    if(vpad_buffer->hold & VPAD_BUTTON_R)                 buttons_hold |= WPAD_PRO_TRIGGER_R;
    if(vpad_buffer->hold & VPAD_BUTTON_ZR)                buttons_hold |= WPAD_PRO_TRIGGER_ZR;

    if(vpad_buffer->hold & VPAD_BUTTON_STICK_L)           buttons_hold |= WPAD_PRO_BUTTON_STICK_L;
    if(vpad_buffer->hold & VPAD_BUTTON_STICK_R)           buttons_hold |= WPAD_PRO_BUTTON_STICK_R;

    if(vpad_buffer->hold & VPAD_STICK_L_EMULATION_LEFT)   buttons_hold |= WPAD_PRO_STICK_L_EMULATION_LEFT;
    if(vpad_buffer->hold & VPAD_STICK_L_EMULATION_RIGHT)  buttons_hold |= WPAD_PRO_STICK_L_EMULATION_RIGHT;
    if(vpad_buffer->hold & VPAD_STICK_L_EMULATION_UP)     buttons_hold |= WPAD_PRO_STICK_L_EMULATION_UP;
    if(vpad_buffer->hold & VPAD_STICK_L_EMULATION_DOWN)   buttons_hold |= WPAD_PRO_STICK_L_EMULATION_DOWN;

    if(vpad_buffer->hold & VPAD_STICK_R_EMULATION_LEFT)   buttons_hold |= WPAD_PRO_STICK_R_EMULATION_LEFT;
    if(vpad_buffer->hold & VPAD_STICK_R_EMULATION_RIGHT)  buttons_hold |= WPAD_PRO_STICK_R_EMULATION_RIGHT;
    if(vpad_buffer->hold & VPAD_STICK_R_EMULATION_UP)     buttons_hold |= WPAD_PRO_STICK_R_EMULATION_UP;
    if(vpad_buffer->hold & VPAD_STICK_R_EMULATION_DOWN)   buttons_hold |= WPAD_PRO_STICK_R_EMULATION_DOWN;

    pro_buffer->l_stick_x = (s16) (vpad_buffer->leftStick.x * 950.0f);
    pro_buffer->l_stick_y = (s16) (vpad_buffer->leftStick.y * 950.0f);
    pro_buffer->r_stick_x = (s16) (vpad_buffer->rightStick.x * 950.0f);
    pro_buffer->r_stick_y = (s16) (vpad_buffer->rightStick.y * 950.0f);

    pro_buffer->buttons = buttons_hold;

    pro_buffer->fmt = WPAD_FMT_PRO_CONTROLLER;
    pro_buffer->err = 0x00;
    pro_buffer->dev = WPAD_EXT_PRO_CONTROLLER;

    return CONTROLLER_PATCHER_ERROR_NONE;
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcherUtils::translateToVPAD(VPADStatus * vpad_buffer,KPADData * pro_buffer,u32 * lastButtonsPressesVPAD){
    if(vpad_buffer == NULL || pro_buffer == NULL || lastButtonsPressesVPAD == NULL) return CONTROLLER_PATCHER_ERROR_NULL_POINTER;

    s32 buttons_hold = 0;

    if(pro_buffer->pro.hold & WPAD_PRO_BUTTON_A)                 buttons_hold |= VPAD_BUTTON_A;

    if(pro_buffer->pro.hold & WPAD_PRO_BUTTON_B)                 buttons_hold |= VPAD_BUTTON_B;
    if(pro_buffer->pro.hold & WPAD_PRO_BUTTON_X)                 buttons_hold |= VPAD_BUTTON_X;
    if(pro_buffer->pro.hold & WPAD_PRO_BUTTON_Y)                 buttons_hold |= VPAD_BUTTON_Y;

    if(pro_buffer->pro.hold & WPAD_PRO_BUTTON_PLUS)              buttons_hold |= VPAD_BUTTON_PLUS;
    if(pro_buffer->pro.hold & WPAD_PRO_BUTTON_MINUS)             buttons_hold |= VPAD_BUTTON_MINUS;

    if(pro_buffer->pro.hold & WPAD_PRO_BUTTON_HOME)              buttons_hold |= VPAD_BUTTON_HOME;

    if(pro_buffer->pro.hold & WPAD_PRO_BUTTON_LEFT)              buttons_hold |= VPAD_BUTTON_LEFT;
    if(pro_buffer->pro.hold & WPAD_PRO_BUTTON_RIGHT)             buttons_hold |= VPAD_BUTTON_RIGHT;
    if(pro_buffer->pro.hold & WPAD_PRO_BUTTON_UP)                buttons_hold |= VPAD_BUTTON_UP;
    if(pro_buffer->pro.hold & WPAD_PRO_BUTTON_DOWN)              buttons_hold |= VPAD_BUTTON_DOWN;

    if(pro_buffer->pro.hold & WPAD_PRO_TRIGGER_L)                buttons_hold |= VPAD_BUTTON_L;
    if(pro_buffer->pro.hold & WPAD_PRO_TRIGGER_ZL)               buttons_hold |= VPAD_BUTTON_ZL;

    if(pro_buffer->pro.hold & WPAD_PRO_TRIGGER_R)                buttons_hold |= VPAD_BUTTON_R;
    if(pro_buffer->pro.hold & WPAD_PRO_TRIGGER_ZR)               buttons_hold |= VPAD_BUTTON_ZR;

    if(pro_buffer->pro.hold & WPAD_PRO_BUTTON_STICK_L)           buttons_hold |= VPAD_BUTTON_STICK_L;
    if(pro_buffer->pro.hold & WPAD_PRO_BUTTON_STICK_R)           buttons_hold |= VPAD_BUTTON_STICK_R;

    if(pro_buffer->pro.hold & WPAD_PRO_STICK_L_EMULATION_LEFT)   buttons_hold |= VPAD_STICK_L_EMULATION_LEFT;
    if(pro_buffer->pro.hold & WPAD_PRO_STICK_L_EMULATION_RIGHT)  buttons_hold |= VPAD_STICK_L_EMULATION_RIGHT;
    if(pro_buffer->pro.hold & WPAD_PRO_STICK_L_EMULATION_UP)     buttons_hold |= VPAD_STICK_L_EMULATION_UP;
    if(pro_buffer->pro.hold & WPAD_PRO_STICK_L_EMULATION_DOWN)   buttons_hold |= VPAD_STICK_L_EMULATION_DOWN;

    if(pro_buffer->pro.hold & WPAD_PRO_STICK_R_EMULATION_LEFT)   buttons_hold |= VPAD_STICK_R_EMULATION_LEFT;
    if(pro_buffer->pro.hold & WPAD_PRO_STICK_R_EMULATION_RIGHT)  buttons_hold |= VPAD_STICK_R_EMULATION_RIGHT;
    if(pro_buffer->pro.hold & WPAD_PRO_STICK_R_EMULATION_UP)     buttons_hold |= VPAD_STICK_R_EMULATION_UP;
    if(pro_buffer->pro.hold & WPAD_PRO_STICK_R_EMULATION_DOWN)   buttons_hold |= VPAD_STICK_R_EMULATION_DOWN;

    vpad_buffer->leftStick.x = pro_buffer->pro.lstick_x;
    vpad_buffer->leftStick.y = pro_buffer->pro.lstick_x;
    vpad_buffer->rightStick.x = pro_buffer->pro.rstick_x;
    vpad_buffer->rightStick.y = pro_buffer->pro.rstick_x;

    vpad_buffer->hold |= buttons_hold;
    vpad_buffer->trigger |= (buttons_hold & (~*lastButtonsPressesVPAD));
    vpad_buffer->release |= (*lastButtonsPressesVPAD & (~buttons_hold));

    *lastButtonsPressesVPAD = buttons_hold;

    return CONTROLLER_PATCHER_ERROR_NONE;
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcherUtils::checkValueinConfigController(s32 deviceslot,s32 CONTRPS_slot,s32 expectedValue){
    if(config_controller[deviceslot][CONTRPS_slot][0] != CONTROLLER_PATCHER_INVALIDVALUE){
        if(expectedValue == config_controller[deviceslot][CONTRPS_slot][1]) return 1;
    }
    return 0;
}

void ControllerPatcherUtils::setConfigValue(u8 * dest, u8 first, u8 second){
    dest[0] = first;
    dest[1] = second;
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcherUtils::getDeviceSlot(u32 hidmask){
    for(s32 i = 0;i < gHIDMaxDevices;i++){
        if(hidmask & config_controller_hidmask[i]){
              return i;
        }
    }
    return CONTROLLER_PATCHER_ERROR_DEVICE_SLOT_NOT_FOUND;
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcherUtils::getDeviceInfoFromVidPid(DeviceInfo * info){
    if(info != NULL){
        for(s32 i = 0;i< gHIDMaxDevices;i++){
            u16 my_vid = config_controller[i][CONTRPS_VID][0] * 0x100 + config_controller[i][CONTRPS_VID][1];
            u16 my_pid = config_controller[i][CONTRPS_PID][0] * 0x100 + config_controller[i][CONTRPS_PID][1];
            //printf("info->vidpid.vid (%04X) == my_vid (%04X) && info->vidpid.pid (%04X) == my_pid (%04X)\n",info->vidpid.vid,my_vid,info->vidpid.pid,my_pid);
            if(info->vidpid.vid == my_vid && info->vidpid.pid == my_pid){
                info->slotdata.hidmask = config_controller_hidmask[i];
                info->slotdata.deviceslot = i;
                info->pad_count = 1;
                if(config_controller[i][CONTRPS_PAD_COUNT][0] != CONTROLLER_PATCHER_INVALIDVALUE){
                   info->pad_count = config_controller[i][CONTRPS_PAD_COUNT][1];
                   if(info->pad_count > HID_MAX_PADS_COUNT){
                        info->pad_count = HID_MAX_PADS_COUNT;
                   }
                }

                return CONTROLLER_PATCHER_ERROR_NONE;
                //printf("Found device: device: %s slot: %d\n",byte_to_binary(device),deviceSlot);
                break;
            }
        }
        return CONTROLLER_PATCHER_ERROR_UNKNOWN_VID_PID;
    }
    return CONTROLLER_PATCHER_ERROR_INVALID_BUFFER;
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcherUtils::getNextSlotData(HIDSlotData * slotdata){
    if(slotdata == NULL) return CONTROLLER_PATCHER_ERROR_NULL_POINTER;
    if(gHIDRegisteredDevices >= gHIDMaxDevices) return CONTROLLER_PATCHER_ERROR_NO_FREE_SLOT;
    slotdata->deviceslot = gHIDRegisteredDevices;
    slotdata->hidmask = (1 << (gHIDRegisteredDevices));
    gHIDRegisteredDevices++;

    return CONTROLLER_PATCHER_ERROR_NONE;
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcherUtils::getVIDPIDbyDeviceSlot(s32 deviceslot, DeviceVIDPIDInfo * vidpid){
    if(vidpid == NULL) return CONTROLLER_PATCHER_ERROR_NULL_POINTER;
    if(deviceslot >= gHIDMaxDevices || deviceslot < 0) return CONTROLLER_PATCHER_ERROR_DEVICE_SLOT_NOT_FOUND;
    vidpid->vid = config_controller[deviceslot][CONTRPS_VID][0] * 0x100 + config_controller[deviceslot][CONTRPS_VID][1];
    vidpid->pid = config_controller[deviceslot][CONTRPS_PID][0] * 0x100 + config_controller[deviceslot][CONTRPS_PID][1];
    if(vidpid->vid == 0x0000) return CONTROLLER_PATCHER_ERROR_FAILED_TO_GET_HIDDATA;
    return CONTROLLER_PATCHER_ERROR_NONE;
}

s32 ControllerPatcherUtils::getPadSlotInAdapter(s32 deviceslot, u8 * input_data){
    s32 slot_incr = 0;
    if(config_controller[deviceslot][CONTRPS_PAD_COUNT][0] != CONTROLLER_PATCHER_INVALIDVALUE){
        s32 pad_count = config_controller[deviceslot][CONTRPS_PAD_COUNT][1];
        if(pad_count > HID_MAX_PADS_COUNT){
            pad_count = HID_MAX_PADS_COUNT;
        }
        for(s32 i= 0;i<pad_count;i++){
             if(        config_controller[deviceslot][CONTRPS_PAD1_FILTER + i][0] != CONTROLLER_PATCHER_INVALIDVALUE){
                 if(input_data[config_controller[deviceslot][CONTRPS_PAD1_FILTER + i][0]] == config_controller[deviceslot][CONTRPS_PAD1_FILTER + i][1]){
                    slot_incr = i;
                    break;
                }
            }
        }
    }
    return slot_incr;
}

ControllerMappingPAD * ControllerPatcherUtils::getControllerMappingByType(UController_Type type){
    ControllerMappingPAD * cm_map_pad = NULL;

    if(type == UController_Type_Gamepad){
        cm_map_pad = &(gControllerMapping.gamepad);
    }else if(type == UController_Type_Pro1){
        cm_map_pad = &(gControllerMapping.proController[0]);
    }else if(type == UController_Type_Pro2){
        cm_map_pad = &(gControllerMapping.proController[1]);
    }else if(type == UController_Type_Pro3){
        cm_map_pad = &(gControllerMapping.proController[2]);
    }else if(type == UController_Type_Pro4){
        cm_map_pad = &(gControllerMapping.proController[3]);
    }
    return cm_map_pad;
}

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcherUtils::doSampling(u16 deviceslot,u8 padslot = 0,bool ignorePadSlot = false){
    if(gSamplingCallback != NULL){
        for(int i=0;i<4;i++){
            ControllerMappingPADInfo * padinfo = &gControllerMapping.proController[i].pad_infos[0];
            if(padinfo->active){
                DeviceInfo device_info;

                memset(&device_info,0,sizeof(device_info));
                device_info.vidpid = padinfo->vidpid;

                s32 res = -1;
                if((res = ControllerPatcherUtils::getDeviceInfoFromVidPid(&device_info)) >= 0){
                    if(!ignorePadSlot){
                        s32 real_pad = (padinfo->pad/(device_info.pad_count))*device_info.pad_count;
                        if(real_pad == padslot && device_info.slotdata.deviceslot == deviceslot){
                            if(ControllerPatcherUtils::checkActivePad(device_info.slotdata.hidmask,padinfo->pad)){
                                gSamplingCallback(i);
                            }
                        }
                    }else{
                        gSamplingCallback(i);
                    }
                }
            }
        }
    }
    return CONTROLLER_PATCHER_ERROR_NONE;
}
