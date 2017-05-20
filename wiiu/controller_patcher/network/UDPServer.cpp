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
#include "UDPServer.hpp"
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "sys/socket.h"

#define MAX_UDP_SIZE 0x578
#define wiiu_errno (*__gh_errno_ptr())

ControllerPatcherThread * UDPServer::pThread = NULL;
UDPServer * UDPServer::instance = NULL;

UDPServer::UDPServer(s32 port){
    s32 ret;
	struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = port;
    addr.sin_addr.s_addr = 0;

    this->sockfd = ret = socket(AF_INET, SOCK_DGRAM, 0);
    if(ret == -1) return;
    s32 enable = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    ret = bind(sockfd, (sockaddr *)&addr, 16);
    if(ret < 0) return;

    StartUDPThread(this);
}

UDPServer::~UDPServer(){
    ControllerPatcherThread * pThreadPointer = UDPServer::pThread;
    if(pThreadPointer != NULL){
        exitThread = 1;
        if(pThreadPointer != NULL){
            delete pThreadPointer;
            UDPServer::pThread = NULL;
            if (this->sockfd != -1){
                socketclose(sockfd);
            }
            this->sockfd = -1;
        }
    }
    if(HID_DEBUG){ printf("UDPServer::~UDPServer(line %d): Thread has been closed\n",__LINE__); }


}

void UDPServer::StartUDPThread(UDPServer * server){
    s32 priority = 28;
    if(OSGetTitleID() == 0x00050000101c9300 || //The Legend of Zelda Breath of the Wild JPN
       OSGetTitleID() == 0x00050000101c9400 || //The Legend of Zelda Breath of the Wild USA
       OSGetTitleID() == 0x00050000101c9500 || //The Legend of Zelda Breath of the Wild EUR
       OSGetTitleID() == 0x00050000101c9b00 || //The Binding of Isaac: Rebirth EUR
       OSGetTitleID() == 0x00050000101a3c00){  //The Binding of Isaac: Rebirth USA
        priority = 10;
        printf("UDPServer::StartUDPThread(line %d): This game needs higher thread priority. We set it to %d\n",__LINE__,priority);
    }
    UDPServer::pThread = ControllerPatcherThread::create(UDPServer::DoUDPThread, (void*)server, ControllerPatcherThread::eAttributeAffCore2,priority);
    UDPServer::pThread->resumeThread();
}

bool UDPServer::cpyIncrementBufferOffset(void * target, void * source, s32 * offset, s32 typesize, s32 maximum){
    if(((int)*offset + typesize) > maximum){
        printf("UDPServer::cpyIncrementBufferOffset(line %d): Transfer error. Excepted %04X bytes, but only got %04X\n",__LINE__,(*offset + typesize),maximum);
        return false;
    }
    memcpy(target,(void*)((u32)source+(*offset)),typesize);
    *offset += typesize;
    return true;
}

void UDPServer::DoUDPThread(ControllerPatcherThread *thread, void *arg){
    UDPServer * args = (UDPServer * )arg;
    args->DoUDPThreadInternal();
}

void UDPServer::DoUDPThreadInternal(){
    u8 buffer[MAX_UDP_SIZE];
    s32 n;

    my_cb_user  user;
    while(1){
        //s32 usingVar = exitThread;
        if(exitThread)break;
        memset(buffer,0,MAX_UDP_SIZE);
        n = recv(sockfd,buffer,MAX_UDP_SIZE,0);
        if (n < 0){
            s32 errno_ = wiiu_errno;
            wiiu_os_usleep(2000);
            if(errno_ != 11 && errno_ != 9){
                break;
            }
          continue;
        }
        s32 bufferoffset = 0;
        u8 type;
        memcpy((void *)&type,buffer,sizeof(type));
        bufferoffset += sizeof(type);
        switch (buffer[0]) {
            case WIIU_CP_UDP_CONTROLLER_READ_DATA: {
                if(gUsedProtocolVersion >= WIIU_CP_TCP_HANDSHAKE_VERSION_1){
                    u8 count_commands;
                    memcpy((void *)&count_commands,buffer+bufferoffset,sizeof(count_commands));
                    bufferoffset += sizeof(count_commands);
                    for(s32 i = 0;i<count_commands;i++){
                        s32 handle;
                        u16 deviceSlot;
                        u32 hid;
                        u8 padslot;
                        u8 datasize;

                        if(!cpyIncrementBufferOffset((void *)&handle,       (void *)buffer,&bufferoffset,sizeof(handle),    n))continue;
                        if(!cpyIncrementBufferOffset((void *)&deviceSlot,   (void *)buffer,&bufferoffset,sizeof(deviceSlot),n))continue;
                        hid = (1  << deviceSlot);
                        if(!cpyIncrementBufferOffset((void *)&padslot,      (void *)buffer,&bufferoffset,sizeof(padslot),   n))continue;
                        if(!cpyIncrementBufferOffset((void *)&datasize,     (void *)buffer,&bufferoffset,sizeof(datasize),  n))continue;
                        u8 * databuffer = (u8*) malloc(datasize * sizeof(u8));
                        if(!databuffer){
                            printf("UDPServer::DoUDPThreadInternal(line %d): Allocating memory failed\n",__LINE__);
                            continue;
                        }

                        if(!cpyIncrementBufferOffset((void *)databuffer,    (void *)buffer,&bufferoffset,datasize,          n))continue;
                        //printf("UDPServer::DoUDPThreadInternal(): Got handle: %d slot %04X hid %04X pad %02X datasize %02X\n",handle,deviceSlot,hid,padslot,datasize);

                        user.pad_slot = padslot;
                        user.slotdata.deviceslot =  deviceSlot;
                        user.slotdata.hidmask = hid;

                        if(gNetworkController[deviceSlot][padslot][0] == 0){
                            printf("UDPServer::DoUDPThreadInternal(line %d): Ehm. Pad is not connected. STOP SENDING DATA ;) \n",__LINE__);
                        }else{
                            ControllerPatcherHID::externHIDReadCallback(handle,databuffer,datasize,&user);
                        }
                        if(databuffer){
                            free(databuffer);
                            databuffer = NULL;
                        }
                    }
                    break;
                }
                break;
            }
            default:{
                break;
            }
        }
    }
    if(HID_DEBUG){ printf("UDPServer::DoUDPThreadInternal(line %d): UDPServer Thread ended\n",__LINE__); }
}
