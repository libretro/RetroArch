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
#include "UDPClient.hpp"
#include <stdio.h>
#include <string.h>

#define MAX_UDP_SIZE 0x578

UDPClient * UDPClient::instance = NULL;

UDPClient::UDPClient(u32 ip, s32 port){
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sockfd < 0)
		return;

	struct sockaddr_in connect_addr;
	memset(&connect_addr, 0, sizeof(connect_addr));
	connect_addr.sin_family = AF_INET;
	connect_addr.sin_port = port;
	connect_addr.sin_addr.s_addr = ip;

	if(connect(sockfd, (struct sockaddr*)&connect_addr, sizeof(connect_addr)) < 0)
	{
	    socketclose(sockfd);
	    sockfd = -1;
	}
}

UDPClient::~UDPClient(){
    if (this->sockfd != -1){
        socketclose(sockfd);
    }
    if(HID_DEBUG){ printf("UDPClient::~UDPClient(line %d): Thread has been closed\n",__LINE__); }
}

bool UDPClient::sendData(char * data,s32 length){
    if(sockfd < 0 || data == 0 || length < 0 || gUsedProtocolVersion < WIIU_CP_TCP_HANDSHAKE_VERSION_3){
        return false;
    }
    if(length > 1400) length = 1400;

    s32 ret = send(sockfd, data, length, 0);
    return (ret >= 0);
}
