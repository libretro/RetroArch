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
#include "ControllerPatcherHID.hpp"

#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "wiiu/os.h"

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 * public implementation for the network controller
 *---------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ControllerPatcherHID::externAttachDetachCallback(HIDDevice *p_device, u32 attach){
    HIDClient client;
    memset(&client,0,sizeof(client));
    return AttachDetachCallback(&client,p_device,attach);
}

void ControllerPatcherHID::externHIDReadCallback(u32 handle, unsigned char *buf, u32 bytes_transfered, my_cb_user * usr){
    HIDReadCallback(handle,buf,bytes_transfered,usr);
}

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 * private implementation for the HID Api.
 *---------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ControllerPatcherHID::myAttachDetachCallback(HIDClient *p_client, HIDDevice *p_device, u32 attach){
    return AttachDetachCallback(p_client,p_device,attach);
}

void ControllerPatcherHID::myHIDMouseReadCallback(u32 handle, s32 error, unsigned char *buf, u32 bytes_transfered, void *p_user){
	if(error == 0){
        my_cb_user *usr = (my_cb_user*)p_user;

        u32 slot = 0;
        if(usr->pad_slot < HID_MAX_PADS_COUNT){
            slot = usr->pad_slot;
        }

        HID_Data * data_ptr = &(gHID_Devices[usr->slotdata.deviceslot].pad_data[slot]);
        HID_Mouse_Data * cur_mouse_data = &data_ptr->data_union.mouse.cur_mouse_data;

        data_ptr->type = DEVICE_TYPE_MOUSE;
        //printf("%02X %02X %02X %02X %02X bytes_transfered: %d\n",buf[0],buf[1],buf[2],buf[3],buf[4],bytes_transfered);

        if(buf[0] == 2 && bytes_transfered > 3){ // using the other mouse mode
            buf +=1;
        }

        s8 x_value = 0;
        s8 y_value = 0;

        x_value = buf[1];
        y_value = buf[2];

        cur_mouse_data->X += x_value;
        cur_mouse_data->deltaX = x_value;

        cur_mouse_data->Y += y_value;
        cur_mouse_data->deltaY = y_value;

        cur_mouse_data->left_click = buf[0];
	    cur_mouse_data->right_click = buf[0]>>1;

        if(cur_mouse_data->X < 0) cur_mouse_data->X = 0;
        if(cur_mouse_data->X > 1280) cur_mouse_data->X = 1280;

        if(cur_mouse_data->Y < 0) cur_mouse_data->Y = 0;
        if(cur_mouse_data->Y > 720) cur_mouse_data->Y = 720;

        cur_mouse_data->valuedChanged = 1;

        //printf("%02X %02X %02X %02X %02X %02X %02X %02X %d = X: %d Y: %d \n",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],bytes_transfered,x_value,y_value);

        HIDRead(handle, usr->buf, bytes_transfered, myHIDMouseReadCallback, usr);
	}
}

void ControllerPatcherHID::myHIDReadCallback(u32 handle, s32 error, unsigned char *buf, u32 bytes_transfered, void *p_user){
    if(error == 0 && p_user != NULL && gHIDAttached){
	    my_cb_user *usr = (my_cb_user*)p_user;

	    HIDReadCallback(handle,buf,bytes_transfered,usr);

        if(usr->slotdata.hidmask == gHID_LIST_DS4){
	        wiiu_os_usleep(1000*2); //DS4 is way tooo fast. sleeping to reduce lag. (need to check the other pads)
	    }
        HIDRead(handle, usr->buf, bytes_transfered, myHIDReadCallback, usr);
	}
}

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 * Intern Callback actions
 *---------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

s32 ControllerPatcherHID::AttachDetachCallback(HIDClient *p_client, HIDDevice *p_device, u32 attach){
    if(attach){
        printf("ControllerPatcherHID::AttachDetachCallback(line %d): vid %04x pid %04x connected\n",__LINE__, SWAP16(p_device->vid),SWAP16(p_device->pid));
        if(HID_DEBUG){  printf("interface index  %02x\n", p_device->interface_index);
                        printf("sub class        %02x\n", p_device->sub_class);
                        printf("protocol         %02x\n", p_device->protocol);
                        printf("max packet in    %02x\n", p_device->max_packet_size_rx);
                        printf("max packet out   %02x\n", p_device->max_packet_size_tx); }
    }
    if(!attach){
        printf("ControllerPatcherHID::AttachDetachCallback(line %d): vid %04x pid %04x disconnected\n",__LINE__, SWAP16(p_device->vid),SWAP16(p_device->pid));
    }
    DeviceInfo device_info;
    memset(&device_info,0,sizeof(DeviceInfo));
    device_info.slotdata.deviceslot = -1;
    device_info.vidpid.vid = SWAP16(p_device->vid);
    device_info.vidpid.pid = SWAP16(p_device->pid);

    HIDSlotData * slotdata = &(device_info.slotdata);

    if ((p_device->sub_class == 1) && (p_device->protocol == 1)) { //Keyboard
        slotdata->hidmask = gHID_LIST_KEYBOARD;
        slotdata->deviceslot = gHID_SLOT_KEYBOARD;
        //printf("Found Keyboard: device: %s slot: %d\n",byte_to_binary(device_info.slotdata.hidmask),device_info.slotdata.deviceslot);
    }else if ((p_device->sub_class == 1) && (p_device->protocol == 2)){ // MOUSE
        slotdata->hidmask = gHID_LIST_MOUSE;
        slotdata->deviceslot = gMouseSlot;
        //printf("Found Mouse: device: %s slot: %d\n",byte_to_binary(device_info.hid),device_info.slot);
    }else{
        s32 ret;
        if((ret = ControllerPatcherUtils::getDeviceInfoFromVidPid(&device_info)) < 0){
            printf("ControllerPatcherHID::AttachDetachCallback(line %d): ControllerPatcherUtils::getDeviceInfoFromVidPid(&device_info) failed %d \n",__LINE__,ret);
            return HID_DEVICE_DETACH;
        }else{
            //printf("ControllerPatcherHID::AttachDetachCallback(line %d): ControllerPatcherUtils::getDeviceInfoFromVidPid(&device_info) success %d \n",__LINE__,ret);
        }
    }

    if(slotdata->hidmask){
        if(attach){
            s32 bufSize = 64;
            if(slotdata->hidmask != gHID_LIST_MOUSE && config_controller[slotdata->deviceslot][CONTRPS_BUF_SIZE][0] == CONTROLLER_PATCHER_VALUE_SET){
                bufSize = config_controller[slotdata->deviceslot][CONTRPS_BUF_SIZE][1];
            }
            unsigned char *buf = (unsigned char *) memalign(64,bufSize);
            memset(buf,0,bufSize);
            my_cb_user *usr = (my_cb_user *) memalign(64,sizeof(my_cb_user));
            usr->buf = buf;
            usr->slotdata = device_info.slotdata;
            usr->transfersize = p_device->max_packet_size_rx;
            usr->handle = p_device->handle;
            usr->vidpid = device_info.vidpid;
            gHIDAttached |= slotdata->hidmask;
            gHIDCurrentDevice |= slotdata->hidmask;
            s32 pads_per_device = 1;
            if(config_controller[slotdata->deviceslot][CONTRPS_PAD_COUNT][0] != CONTROLLER_PATCHER_INVALIDVALUE){
                pads_per_device = config_controller[slotdata->deviceslot][CONTRPS_PAD_COUNT][1];
                if(pads_per_device > HID_MAX_PADS_COUNT){//maximum of HID_MAX_PADS_COUNT
                    pads_per_device = HID_MAX_PADS_COUNT;
                }
            }

            s32 pad_count = config_controller[slotdata->deviceslot][CONTRPS_CONNECTED_PADS][1];
            if(pad_count > 0x0F) pad_count = 0; //???

            s32 pad_slot = 0;

            s32 failed = 1;

            for(s32 i = 0;i<HID_MAX_PADS_COUNT;i += pads_per_device){
                if(!(pad_count & (1 << i))){
                    failed = 0;
                    pad_count |= (1 << i);
                    pad_slot = i;
                    break;
                }
            }

            if(failed){
                printf("ControllerPatcherHID::AttachDetachCallback(line %d) error: I can only handle %d devices of the same type. Sorry \n",__LINE__,HID_MAX_PADS_COUNT);
                if(buf){
                    free(buf);
                    buf = NULL;
                }
                if(usr){
                    free(usr);
                    usr = NULL;
                }
                return 0;
            }

            config_controller[slotdata->deviceslot][CONTRPS_CONNECTED_PADS][1] = pad_count;

            DCFlushRange(&config_controller[slotdata->deviceslot][CONTRPS_CONNECTED_PADS][1],sizeof(config_controller[slotdata->deviceslot][CONTRPS_CONNECTED_PADS][1]));
            DCInvalidateRange(&config_controller[slotdata->deviceslot][CONTRPS_CONNECTED_PADS][1],sizeof(config_controller[slotdata->deviceslot][CONTRPS_CONNECTED_PADS][1]));

            usr->pads_per_device = pads_per_device;
            usr->pad_slot = pad_slot;

            for(s32 i = 0;i<pads_per_device;i++){
                memset(&gHID_Devices[slotdata->deviceslot].pad_data[pad_slot+i],0,sizeof(HID_Data));

                gHID_Devices[slotdata->deviceslot].pad_data[pad_slot+i].handle = p_device->handle;
                //printf("ControllerPatcherHID::AttachDetachCallback(line %d): saved handle %d to slot %d and pad %d\n",__LINE__,p_device->handle,slotdata->deviceslot,pad_slot);

                gHID_Devices[slotdata->deviceslot].pad_data[pad_slot+i].user_data = usr;
                gHID_Devices[slotdata->deviceslot].pad_data[pad_slot+i].slotdata = device_info.slotdata;

                DCFlushRange(&gHID_Devices[slotdata->deviceslot].pad_data[pad_slot+i],sizeof(HID_Data));
                DCInvalidateRange(&gHID_Devices[slotdata->deviceslot].pad_data[pad_slot+i],sizeof(HID_Data));
            }

            for(s32 j = 0;j < pads_per_device; j++){
                for(s32 i = 0;i < gHIDMaxDevices; i++){
                    if(connectionOrderHelper[i] == NULL){
                        connectionOrderHelper[i] = usr;
                        break;
                    }
                }
            }

            if(HID_DEBUG){ printf("ControllerPatcherHID::AttachDetachCallback(line %d): Device successfully attached\n",__LINE__); }

            if(slotdata->hidmask == gHID_LIST_GC){ // GC PAD
                //The GC Adapter has all ports in one device. Set them all.
                gHID_Devices[slotdata->deviceslot].pad_data[0].slotdata = device_info.slotdata;
                gHID_Devices[slotdata->deviceslot].pad_data[1].slotdata = device_info.slotdata;
                gHID_Devices[slotdata->deviceslot].pad_data[2].slotdata = device_info.slotdata;
                gHID_Devices[slotdata->deviceslot].pad_data[3].slotdata = device_info.slotdata;

                buf[0] = 0x13;
                HIDWrite(p_device->handle, usr->buf, 1, NULL,NULL);
                HIDRead(p_device->handle, usr->buf, usr->transfersize, myHIDReadCallback, usr);
            }else if (slotdata->hidmask == gHID_LIST_MOUSE){
                HIDSetProtocol(p_device->handle, p_device->interface_index, 0, 0, 0);
                //HIDGetDescriptor(p_device->handle,0x22,0x00,0,my_buf,512,my_foo_cb,NULL);
                HIDSetIdle(p_device->handle,p_device->interface_index,1,NULL,NULL);
                gHID_Mouse_Mode = HID_MOUSE_MODE_AIM;
                HIDRead(p_device->handle, buf, p_device->max_packet_size_rx, myHIDMouseReadCallback, usr);
            }else if (slotdata->hidmask == gHID_LIST_SWITCH_PRO){
                s32 read_result = HIDRead(p_device->handle, usr->buf, usr->transfersize, NULL, NULL);
                if(read_result == 64){
                    if(usr->buf[01] == 0x01){ //We need to do the handshake
                        printf("ControllerPatcherHID::AttachDetachCallback(line %d): Switch Pro Controller handshake needed\n",__LINE__);
                        /**
                            Thanks to ShinyQuagsire23 for the values (https://github.com/shinyquagsire23/HID-Joy-Con-Whispering)
                        **/
                        //Get MAC
                        buf[0] = 0x80;
                        buf[1] = 0x01;
                        HIDWrite(p_device->handle, usr->buf, 2, NULL,NULL);
                        HIDRead(p_device->handle, usr->buf, usr->transfersize, NULL, NULL);
                        //Do handshake
                        buf[0] = 0x80;
                        buf[1] = 0x02;
                        HIDWrite(p_device->handle, usr->buf, 2, NULL,NULL);
                        HIDRead(p_device->handle, usr->buf, usr->transfersize, NULL, NULL);
                        //Talk over HID only.
                        buf[0] = 0x80;
                        buf[1] = 0x04;
                        HIDWrite(p_device->handle, usr->buf, 2, NULL,NULL);
                        HIDRead(p_device->handle, usr->buf, usr->transfersize, NULL, NULL);
                    }else{
                        printf("ControllerPatcherHID::AttachDetachCallback(line %d): Switch Pro Controller handshake already done\n",__LINE__);
                    }
                    HIDRead(p_device->handle, usr->buf, usr->transfersize, myHIDReadCallback, usr);
                }
            }else if (slotdata->hidmask == gHID_LIST_KEYBOARD){
                HIDSetProtocol(p_device->handle, p_device->interface_index, 1, 0, 0);
                HIDSetIdle(p_device->handle, p_device->interface_index, 0, 0, 0);
                HIDRead(p_device->handle, buf, p_device->max_packet_size_rx, myHIDReadCallback, usr);
            }else if (slotdata->hidmask == gHID_LIST_DS3){
                HIDSetProtocol(p_device->handle, p_device->interface_index, 1, 0, 0);
                HIDDS3Rumble(p_device->handle,usr,0);
                buf[0] = 0x42; buf[1] = 0x0c; buf[2] = 0x00; buf[3] = 0x00;
                HIDSetReport(p_device->handle, HID_REPORT_FEATURE, PS3_F4_REPORT_ID, buf, PS3_F4_REPORT_LEN, NULL, NULL);
                HIDRead(p_device->handle, usr->buf, p_device->max_packet_size_rx, myHIDReadCallback, usr);
            }else{
                HIDRead(p_device->handle, usr->buf, p_device->max_packet_size_rx, myHIDReadCallback, usr);
            }
            return HID_DEVICE_ATTACH;

        }else{
            my_cb_user * user_data = NULL;
            s32 founddata = 0;
            for(s32 i = 0;i<HID_MAX_PADS_COUNT;i++){
                if(gHID_Devices[slotdata->deviceslot].pad_data[i].handle == p_device->handle){
                    gHID_Devices[slotdata->deviceslot].pad_data[i].handle = 0;

                    DCFlushRange(&gHID_Devices[slotdata->deviceslot].pad_data[i].handle,sizeof(gHID_Devices[slotdata->deviceslot].pad_data[i].handle));
                    DCInvalidateRange(&gHID_Devices[slotdata->deviceslot].pad_data[i].handle,sizeof(gHID_Devices[slotdata->deviceslot].pad_data[i].handle));

                    user_data = (my_cb_user *) gHID_Devices[slotdata->deviceslot].pad_data[i].user_data;

                    founddata = 1;
                    break;
                }
            }

            if(user_data){
                for(s32 j = 0;j < user_data->pads_per_device; j++){
                    for(s32 i = 0;i < gHIDMaxDevices; i++){
                        if(connectionOrderHelper[i] == user_data){
                            connectionOrderHelper[i] = NULL;
                            break;
                        }
                    }
                }

                config_controller[slotdata->deviceslot][CONTRPS_CONNECTED_PADS][1] &= ~ (1 << user_data->pad_slot);
                DCFlushRange(&config_controller[slotdata->deviceslot][CONTRPS_CONNECTED_PADS][1],sizeof(config_controller[slotdata->deviceslot][CONTRPS_CONNECTED_PADS][1]));
                DCInvalidateRange(&config_controller[slotdata->deviceslot][CONTRPS_CONNECTED_PADS][1],sizeof(config_controller[slotdata->deviceslot][CONTRPS_CONNECTED_PADS][1]));
                if(user_data->buf){
                    free(user_data->buf);
                    user_data->buf = NULL;
                }
                free(user_data);
                user_data = NULL;
            }else{
                if(founddata){ printf("ControllerPatcherHID::AttachDetachCallback(line %d): user_data null. You may have a memory leak.\n",__LINE__); }
                return HID_DEVICE_DETACH;
            }
            if(config_controller[slotdata->deviceslot][CONTRPS_CONNECTED_PADS][1] == 0){
                gHIDAttached &= ~slotdata->hidmask;
                gHIDCurrentDevice &= ~slotdata->hidmask;

                DCFlushRange(&gHIDAttached,sizeof(gHIDAttached));
                DCInvalidateRange(&gHIDAttached,sizeof(gHIDAttached));
                DCFlushRange(&gHIDCurrentDevice,sizeof(gHIDCurrentDevice));
                DCInvalidateRange(&gHIDCurrentDevice,sizeof(gHIDCurrentDevice));

                if (slotdata->hidmask == gHID_LIST_MOUSE){
                    gHID_Mouse_Mode = HID_MOUSE_MODE_AIM;
                }
            }else{
                if(HID_DEBUG){printf("ControllerPatcherHID::AttachDetachCallback(line %d): We still have pad for deviceslot %d connected.\n",__LINE__,slotdata->deviceslot); }
            }
            if(HID_DEBUG){printf("ControllerPatcherHID::AttachDetachCallback(line %d): Device successfully detached\n",__LINE__); }
        }
    }else{
        printf("ControllerPatcherHID::AttachDetachCallback(line %d): HID-Device currently not supported! You can add support through config files\n",__LINE__);
	}
	return HID_DEVICE_DETACH;
}

void ControllerPatcherHID::HIDReadCallback(u32 handle, unsigned char *buf, u32 bytes_transfered, my_cb_user * usr){
    ControllerPatcherUtils::doSampling(usr->slotdata.deviceslot,usr->pad_slot,false);

    //printf("my_read_cbInternal: %d %08X %d\n",bytes_transfered,usr->slotdata.hidmask,usr->slotdata.deviceslot);
    if(usr->slotdata.hidmask == gHID_LIST_GC){

        HID_Data * data_ptr = NULL;
        //Copy the data for all 4 pads
        for(s32 i = 0;i<4;i++){
            data_ptr = &(gHID_Devices[gHID_SLOT_GC].pad_data[i]);
            memcpy(&(data_ptr->data_union.controller.last_hid_data[0]),&(data_ptr->data_union.controller.cur_hid_data[0]),10); //save last data.
            memcpy(&(data_ptr->data_union.controller.cur_hid_data[0]),&buf[(i*9)+1],9);                  //save new data.

        }

        /*
        s32 i = 0;
        printf("GC1 %08X: %02X %02X %02X %02X %02X %02X %02X %02X %02X ",       buf[i*9+0],buf[i*9+1],buf[i*9+2],buf[i*9+3],buf[i*9+4],buf[i*9+5],buf[i*9+6],buf[i*9+7],buf[i*9+8]);i++;
        printf("GC2 %08X: %02X %02X %02X %02X %02X %02X %02X %02X %02X ",       buf[i*9+0],buf[i*9+1],buf[i*9+2],buf[i*9+3],buf[i*9+4],buf[i*9+5],buf[i*9+6],buf[i*9+7],buf[i*9+8]);i++;
        printf("GC3 %08X: %02X %02X %02X %02X %02X %02X %02X %02X %02X ",       buf[i*9+0],buf[i*9+1],buf[i*9+2],buf[i*9+3],buf[i*9+4],buf[i*9+5],buf[i*9+6],buf[i*9+7],buf[i*9+8]);i++;
        printf("GC4 %08X: %02X %02X %02X %02X %02X %02X %02X %02X %02X \n",     buf[i*9+0],buf[i*9+1],buf[i*9+2],buf[i*9+3],buf[i*9+4],buf[i*9+5],buf[i*9+6],buf[i*9+7],buf[i*9+8]);*/
        HIDGCRumble(handle,usr);
    }else if(usr->slotdata.hidmask != 0){
        //Depending on how the switch pro controller is connected, it has a different data format. At first we had the Bluetooth version, so we need to convert
        //the USB one into it now. (When it's connected via USB). The network client always sends the BT version, even if connected via USB to the PC.
        if(usr->slotdata.hidmask == gHID_LIST_SWITCH_PRO && buf != NULL && bytes_transfered >= 0x20){
            u8 buffer[0x13];
            memcpy(buffer,buf+0x0D,0x013);

            /**
                Thanks to ShinyQuagsire23 for the values (https://github.com/shinyquagsire23/HID-Joy-Con-Whispering)
            **/
            buf[0] = 0x80;
            buf[1] = 0x92;
            buf[2] = 0x00;
            buf[3] = 0x01;
            buf[4] = 0x00;
            buf[5] = 0x00;
            buf[6] = 0x00;
            buf[7] = 0x00;
            buf[8] = 0x1F;
            //We want to get the next input!
            s32 res = HIDWrite(handle, buf, 9, NULL,NULL);

            if(res == 9){ //Check if it's the USB data format.
                if(buffer[1]  == 0) return;
                //Converting the buttons
                u32 buttons = (((u32*)(buffer))[0]) & 0xFFFFFF00;
                u32 newButtons = 0;
                if((buttons & HID_SWITCH_PRO_USB_BUTTON_A_VALUE)          == HID_SWITCH_PRO_USB_BUTTON_A_VALUE)         newButtons |= HID_SWITCH_PRO_BT_BUTTON_A_VALUE;
                if((buttons & HID_SWITCH_PRO_USB_BUTTON_B_VALUE)          == HID_SWITCH_PRO_USB_BUTTON_B_VALUE)         newButtons |= HID_SWITCH_PRO_BT_BUTTON_B_VALUE;
                if((buttons & HID_SWITCH_PRO_USB_BUTTON_X_VALUE)          == HID_SWITCH_PRO_USB_BUTTON_X_VALUE)         newButtons |= HID_SWITCH_PRO_BT_BUTTON_X_VALUE;
                if((buttons & HID_SWITCH_PRO_USB_BUTTON_Y_VALUE)          == HID_SWITCH_PRO_USB_BUTTON_Y_VALUE)         newButtons |= HID_SWITCH_PRO_BT_BUTTON_Y_VALUE;

                if((buttons & HID_SWITCH_PRO_USB_BUTTON_PLUS_VALUE)       == HID_SWITCH_PRO_USB_BUTTON_PLUS_VALUE)      newButtons |= HID_SWITCH_PRO_BT_BUTTON_PLUS_VALUE;
                if((buttons & HID_SWITCH_PRO_USB_BUTTON_MINUS_VALUE)      == HID_SWITCH_PRO_USB_BUTTON_MINUS_VALUE)     newButtons |= HID_SWITCH_PRO_BT_BUTTON_MINUS_VALUE;
                if((buttons & HID_SWITCH_PRO_USB_BUTTON_HOME_VALUE)       == HID_SWITCH_PRO_USB_BUTTON_HOME_VALUE)      newButtons |= HID_SWITCH_PRO_BT_BUTTON_HOME_VALUE;
                //if((buttons & SWITCH_PRO_USB_BUTTON_SCREENSHOT) == HID_SWITCH_PRO_USB_BUTTON_SCREENSHOT) newButtons |= HID_SWITCH_PRO_BT_BUTTON_SCREENSHOT;

                if((buttons & HID_SWITCH_PRO_USB_BUTTON_R_VALUE)          == HID_SWITCH_PRO_USB_BUTTON_R_VALUE)         newButtons |= HID_SWITCH_PRO_BT_BUTTON_R_VALUE;
                if((buttons & HID_SWITCH_PRO_USB_BUTTON_ZR_VALUE)         == HID_SWITCH_PRO_USB_BUTTON_ZR_VALUE)        newButtons |= HID_SWITCH_PRO_BT_BUTTON_ZR_VALUE;
                if((buttons & HID_SWITCH_PRO_USB_BUTTON_STICK_R_VALUE)    == HID_SWITCH_PRO_USB_BUTTON_STICK_R_VALUE)   newButtons |= HID_SWITCH_PRO_BT_BUTTON_STICK_R_VALUE;

                if((buttons & HID_SWITCH_PRO_USB_BUTTON_L_VALUE)          == HID_SWITCH_PRO_USB_BUTTON_L_VALUE)         newButtons |= HID_SWITCH_PRO_BT_BUTTON_L_VALUE;
                if((buttons & HID_SWITCH_PRO_USB_BUTTON_ZL_VALUE)         == HID_SWITCH_PRO_USB_BUTTON_ZL_VALUE)        newButtons |= HID_SWITCH_PRO_BT_BUTTON_ZL_VALUE;
                if((buttons & HID_SWITCH_PRO_USB_BUTTON_STICK_L_VALUE)    == HID_SWITCH_PRO_USB_BUTTON_STICK_L_VALUE)   newButtons |= HID_SWITCH_PRO_BT_BUTTON_STICK_L_VALUE;

                u8 dpad = buffer[2];
                u8 dpadResult = HID_SWITCH_PRO_BT_BUTTON_DPAD_NEUTRAL_VALUE;

                //Converting the DPAD
                if(((dpad & HID_SWITCH_PRO_USB_BUTTON_UP_VALUE)           == HID_SWITCH_PRO_USB_BUTTON_UP_VALUE) &&
                         ((dpad & HID_SWITCH_PRO_USB_BUTTON_RIGHT_VALUE)  == HID_SWITCH_PRO_USB_BUTTON_RIGHT_VALUE)){
                    dpadResult = HID_SWITCH_PRO_BT_BUTTON_DPAD_NE_VALUE;
                }else if(((dpad & HID_SWITCH_PRO_USB_BUTTON_DOWN_VALUE)   == HID_SWITCH_PRO_USB_BUTTON_DOWN_VALUE) &&
                         ((dpad & HID_SWITCH_PRO_USB_BUTTON_RIGHT_VALUE)  == HID_SWITCH_PRO_USB_BUTTON_RIGHT_VALUE)){
                    dpadResult = HID_SWITCH_PRO_BT_BUTTON_DPAD_SE_VALUE;
                }else if(((dpad & HID_SWITCH_PRO_USB_BUTTON_DOWN_VALUE)   == HID_SWITCH_PRO_USB_BUTTON_DOWN_VALUE) &&
                         ((dpad & HID_SWITCH_PRO_USB_BUTTON_LEFT_VALUE)   == HID_SWITCH_PRO_USB_BUTTON_LEFT_VALUE)){
                    dpadResult = HID_SWITCH_PRO_BT_BUTTON_DPAD_SW_VALUE;
                }else if(((dpad & HID_SWITCH_PRO_USB_BUTTON_UP_VALUE)     == HID_SWITCH_PRO_USB_BUTTON_UP_VALUE) &&
                         ((dpad & HID_SWITCH_PRO_USB_BUTTON_LEFT_VALUE)   == HID_SWITCH_PRO_USB_BUTTON_LEFT_VALUE)){
                    dpadResult = HID_SWITCH_PRO_BT_BUTTON_DPAD_NW_VALUE;
                }else if((dpad & HID_SWITCH_PRO_USB_BUTTON_UP_VALUE)      == HID_SWITCH_PRO_USB_BUTTON_UP_VALUE){
                    dpadResult = HID_SWITCH_PRO_BT_BUTTON_DPAD_N_VALUE;
                }else if((dpad &  HID_SWITCH_PRO_USB_BUTTON_RIGHT_VALUE)  == HID_SWITCH_PRO_USB_BUTTON_RIGHT_VALUE){
                    dpadResult = HID_SWITCH_PRO_BT_BUTTON_DPAD_E_VALUE;
                }else if((dpad &  HID_SWITCH_PRO_USB_BUTTON_DOWN_VALUE)   == HID_SWITCH_PRO_USB_BUTTON_DOWN_VALUE){
                    dpadResult = HID_SWITCH_PRO_BT_BUTTON_DPAD_S_VALUE;
                }else if((dpad &  HID_SWITCH_PRO_USB_BUTTON_LEFT_VALUE)   == HID_SWITCH_PRO_USB_BUTTON_LEFT_VALUE){
                    dpadResult = HID_SWITCH_PRO_BT_BUTTON_DPAD_W_VALUE;
                }

                //Converting the stick data
                u8 LX = (u8) ((u16) ((buffer[0x04] << 8 &0xFF00) | (((u16)buffer[0x03])&0xFF)) >> 0x04);
                u8 LY = (u8)((buffer[0x05] *-1));
                u8 RX = (u8) ((u16) ((buffer[0x07] << 8 &0xFF00) | (((u16)buffer[0x06])&0xFF)) >> 0x04);
                u8 RY = (u8)((buffer[0x08] *-1));

                buf[0]  = (newButtons >> 24) & 0xFF;
                buf[1]  = (newButtons >> 16) & 0xFF;
                buf[2] |= dpadResult;
                buf[4]  = LX;
                buf[6]  = LY;
                buf[8]  = RX;
                buf[10] = RY;
            }
        }

        s32 dsize = (HID_MAX_DATA_LENGTH_PER_PAD > bytes_transfered)? bytes_transfered : HID_MAX_DATA_LENGTH_PER_PAD;
        s32 skip = 0;

        //Input filter
        if(        config_controller[usr->slotdata.deviceslot][CONTRPS_INPUT_FILTER][0] != CONTROLLER_PATCHER_INVALIDVALUE){
            if(buf[config_controller[usr->slotdata.deviceslot][CONTRPS_INPUT_FILTER][0]] != config_controller[usr->slotdata.deviceslot][CONTRPS_INPUT_FILTER][1]){
                skip = 1;
            }
        }

       if(!skip){
            u32 slot = 0;
            if(usr->pad_slot < HID_MAX_PADS_COUNT){
                slot = usr->pad_slot;
            }
            slot += ControllerPatcherUtils::getPadSlotInAdapter(usr->slotdata.deviceslot,buf); // If the controller has multiple slots, we need to use the right one.

            HID_Data * data_ptr = &(gHID_Devices[usr->slotdata.deviceslot].pad_data[slot]);

            memcpy(&(data_ptr->data_union.controller.last_hid_data[0]),&(data_ptr->data_union.controller.cur_hid_data[0]),dsize);    // save the last data.
            memcpy(&(data_ptr->data_union.controller.cur_hid_data[0]),&buf[0],dsize);                                                // save the new data.

            DCFlushRange(&gHID_Devices[usr->slotdata.deviceslot].pad_data[slot],sizeof(HID_Data));

            data_ptr = &(gHID_Devices[usr->slotdata.deviceslot].pad_data[slot]);

            HIDRumble(handle,usr,slot);
        }
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 * Other functions
 *---------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcherHID::setVPADControllerData(VPADStatus * buffer,std::vector<HID_Data *>& data){
    if(buffer == NULL) return CONTROLLER_PATCHER_ERROR_NULL_POINTER;
    HID_Data * data_cur;

    s32 buttons_hold;
    for(u32 i = 0;i<data.size();i++){
        data_cur = data[i];

        if(data_cur->slotdata.hidmask & gHID_LIST_MOUSE){  //Reset the input when we have no new inputs
            HID_Mouse_Data * mouse_data = &data_cur->data_union.mouse.cur_mouse_data;
            if(mouse_data->valuedChanged == 1){ //Fix for the mouse cursor
                mouse_data->valuedChanged = 0;
            }else{
                mouse_data->deltaX = 0;
                mouse_data->deltaY = 0;
            }
        }

        buttons_hold = 0;
        ControllerPatcherUtils::getButtonPressed(data_cur,&buttons_hold,VPAD_BUTTON_A);
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

        u32 last_emulate_stick =  (data_cur->last_buttons) & VPAD_MASK_EMULATED_STICKS; // We should only need the emulated stick data.
        s32 last_realbuttons = (data_cur->last_buttons) & VPAD_MASK_BUTTONS;

        buffer->hold |= buttons_hold;
        buffer->trigger |= (buttons_hold & (~last_realbuttons));
        buffer->release |= (last_realbuttons & (~buttons_hold));

        ControllerPatcherUtils::convertAnalogSticks(data_cur,buffer);

        ControllerPatcherUtils::setEmulatedSticks(buffer,&last_emulate_stick);

        ControllerPatcherUtils::checkAndSetMouseMode(data_cur);

        ControllerPatcherUtils::setTouch(data_cur,buffer);

        data_cur->last_buttons = buttons_hold & VPAD_MASK_BUTTONS;
        data_cur->last_buttons |= last_emulate_stick;
    }

    // Caculates a valid stick position
    if(data.size() > 0){
        ControllerPatcherUtils::normalizeStickValues(&buffer->leftStick);
        ControllerPatcherUtils::normalizeStickValues(&buffer->rightStick);
    }

    return CONTROLLER_PATCHER_ERROR_NONE;
}

std::vector<HID_Data *> ControllerPatcherHID::getHIDDataAll(){
    u32 hid = gHIDCurrentDevice;

    std::vector<HID_Data *> data_list;
    for(s32 i = 0;i < gHIDMaxDevices;i++){
        if((hid & (1 << i)) != 0){
            u32 cur_hidmask = config_controller_hidmask[i];
            for(s32 pad = 0; pad < HID_MAX_PADS_COUNT; pad++){
                s32 res;
                HID_Data * new_data = NULL;
                if((res = ControllerPatcherHID::getHIDData(cur_hidmask,pad,&new_data)) < 0){ // Checks if the pad is invalid.
                    //printf("ControllerPatcherHID::getHIDDataAll(line %d): error: Error getting the HID data from HID(%s) CHAN(). Error %d\n",__LINE__,CPStringTools::byte_to_binary(cur_hidmask),pad,res);
                    continue;
                }
                if(new_data != NULL) data_list.push_back(new_data);
            }
        }
    }
    return data_list;
}

/*
The slotdata in the HID_Data pointer is empty. We need to provide the hidmask via the parameter
*/
CONTROLLER_PATCHER_RESULT_OR_ERROR ControllerPatcherHID::getHIDData(u32 hidmask, s32 pad, HID_Data ** data){
    if(data == NULL) return CONTROLLER_PATCHER_ERROR_INVALID_BUFFER;
    if(!(hidmask & gHIDCurrentDevice)) return CONTROLLER_PATCHER_ERROR_HID_NOT_CONNECTED;
    if(pad < 0 && pad > 3) return CONTROLLER_PATCHER_ERROR_INVALID_CHAN;

    s32 device_slot = ControllerPatcherUtils::getDeviceSlot(hidmask);
    if(device_slot < 0){
        return CONTROLLER_PATCHER_ERROR_DEVICE_SLOT_NOT_FOUND;
    }

    s32 real_pad = pad;
    if((device_slot !=  gHID_SLOT_GC) && config_controller[device_slot][CONTRPS_PAD_COUNT][0] != CONTROLLER_PATCHER_INVALIDVALUE){
        s32 pad_count = config_controller[device_slot][CONTRPS_PAD_COUNT][1];
        if(pad_count > HID_MAX_PADS_COUNT) pad_count = HID_MAX_PADS_COUNT;
        pad = (pad/(pad_count))*pad_count;
    }

    s32 result = ControllerPatcherUtils::checkActivePad(hidmask,pad);

    if(result < 0){ //Not pad connected to adapter
        return CONTROLLER_PATCHER_ERROR_NO_PAD_CONNECTED;
    }

    *data = &gHID_Devices[device_slot].pad_data[real_pad];

    return CONTROLLER_PATCHER_ERROR_NONE;
}


void ControllerPatcherHID::HIDGCRumble(u32 handle,my_cb_user *usr){
    if(usr == NULL) return;
    if(!ControllerPatcher::isRumbleActivated()) return;

    s32 rumblechanged = 0;

    for(s32 i = 0;i<HID_GC_PAD_COUNT;i++){
        HID_Data * data_ptr = &(gHID_Devices[usr->slotdata.deviceslot].pad_data[i]);
        if(data_ptr->rumbleActive != usr->rumblestatus[i]){
            rumblechanged = 1;
        }
        usr->rumblestatus[i] = data_ptr->rumbleActive;
        usr->buf[i+1] = usr->rumblestatus[i];
    }
    usr->forceRumbleInTicks[0]--;
    if(rumblechanged || usr->forceRumbleInTicks[0] <= 0){
        usr->buf[0] = 0x11;
        HIDWrite(handle, usr->buf, 5, NULL, NULL);
        usr->forceRumbleInTicks[0] = 10;
    }
}

void ControllerPatcherHID::HIDRumble(u32 handle,my_cb_user *usr,u32 pad){
    if(usr == NULL || pad > HID_MAX_PADS_COUNT) return;
    if(!ControllerPatcher::isRumbleActivated()) return;

    s32 rumblechanged = 0;
    HID_Data * data_ptr = &(gHID_Devices[usr->slotdata.deviceslot].pad_data[pad]);
    if(data_ptr->rumbleActive != usr->rumblestatus[pad]){
        usr->rumblestatus[pad] = data_ptr->rumbleActive;
        rumblechanged = 1;
    }
    usr->forceRumbleInTicks[pad]--;
    if(rumblechanged || usr->forceRumbleInTicks[pad] <= 0){
        //printf("Rumble: %d %d\n",usr->rumblestatus[pad],usr->rumbleForce[pad]);
        //Seding to the network client!
        char bytes[6];

        s32 i = 0;
        bytes[i++] = 0x01;
        bytes[i++] = (handle >> 24) & 0xFF;
        bytes[i++] = (handle >> 16) & 0xFF;
        bytes[i++] = (handle >> 8) & 0xFF;
        bytes[i++] = handle & 0xFF;
        bytes[i++] = usr->rumblestatus[pad];

        UDPClient * instance = UDPClient::getInstance();
        if(instance != NULL){
            instance->sendData(bytes,6);
        }


        if(usr->slotdata.hidmask == gHID_LIST_DS3){
            HIDDS3Rumble(handle,usr,usr->rumblestatus[pad]);
        }else{
            // Not implemented for other devices =(
        }
        usr->forceRumbleInTicks[pad] = 10;
    }
}

static u8 ds3_rumble_Report[48] =
{
    0x00, 0xFF, 0x00, 0xFF, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0x27, 0x10, 0x00, 0x32,
    0xFF, 0x27, 0x10, 0x00, 0x32,
    0xFF, 0x27, 0x10, 0x00, 0x32,
    0xFF, 0x27, 0x10, 0x00, 0x32,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
};

void ControllerPatcherHID::HIDDS3Rumble(u32 handle,my_cb_user *usr,s32 rumble){
    memcpy(usr->buf, ds3_rumble_Report, 48);

    if (rumble) {
        usr->buf[2] = 0x01;
        usr->buf[4] = 0xff;
    }

    HIDSetReport(handle, HID_REPORT_OUTPUT, PS3_01_REPORT_ID, usr->buf, 48, NULL, NULL);
}
