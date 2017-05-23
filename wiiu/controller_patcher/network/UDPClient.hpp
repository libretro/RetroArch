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
#ifndef _UDPCLIENT_WINDOW_H_
#define _UDPCLIENT_WINDOW_H_

#include "../ControllerPatcher.hpp"

#include "sys/socket.h"
#include "netinet/in.h"

#define DEFAULT_UDP_CLIENT_PORT    8114

class UDPClient{
    friend class ControllerPatcher;
    friend class ControllerPatcherHID;
    friend class TCPServer;
public:

private:
    static UDPClient *getInstance() {
        if(instance == NULL){
            createInstance();
        }
        return instance;
    }


    static UDPClient *createInstance() {
        if(instance != NULL){
            destroyInstance();
        }
        instance = new UDPClient(gUDPClientip,DEFAULT_UDP_CLIENT_PORT);

        return  getInstance();
    }

    static void destroyInstance() {
        if(instance != NULL){
            delete instance;
            instance = NULL;
        }
    }

    UDPClient(u32 ip,s32 port);
    ~UDPClient();
    bool sendData(char * data, s32 length);

    volatile s32 sockfd = -1;
    struct sockaddr_in addr;
    static UDPClient *instance;
};

#endif //_UDPClient_WINDOW_H_
