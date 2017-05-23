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
#ifndef _TCPSERVER_WINDOW_H_
#define _TCPSERVER_WINDOW_H_

#include "../ControllerPatcher.hpp"

#include "sys/socket.h"
#include "netinet/in.h"
#include "wiiu/os.h"

#define WIIU_CP_TCP_HANDSHAKE               WIIU_CP_TCP_HANDSHAKE_VERSION_3

#define WIIU_CP_TCP_HANDSHAKE_VERSION_MIN   WIIU_CP_TCP_HANDSHAKE_VERSION_1
#define WIIU_CP_TCP_HANDSHAKE_VERSION_MAX   WIIU_CP_TCP_HANDSHAKE_VERSION_3

#define WIIU_CP_TCP_HANDSHAKE_VERSION_1     0x12
#define WIIU_CP_TCP_HANDSHAKE_VERSION_2     0x13
#define WIIU_CP_TCP_HANDSHAKE_VERSION_3     0x14

#define  WIIU_CP_TCP_HANDSHAKE_ABORT        0x30

#define ATTACH 0x01
#define DETACH 0x00

#define WIIU_CP_TCP_ATTACH      0x01
#define WIIU_CP_TCP_DETACH      0x02
#define WIIU_CP_TCP_PING        0xF0
#define WIIU_CP_TCP_PONG        0xF1

#define WIIU_CP_TCP_ATTACH_CONFIG_FOUND         0xE0
#define WIIU_CP_TCP_ATTACH_CONFIG_NOT_FOUND     0xE1
#define WIIU_CP_TCP_ATTACH_USER_DATA_OKAY       0xE8
#define WIIU_CP_TCP_ATTACH_USER_DATA_BAD        0xE9

#define DEFAULT_TCP_PORT    8112

class TCPServer{
    friend class ControllerPatcher;

private:
     static TCPServer *getInstance() {
        if(!instance)
            instance = new TCPServer(DEFAULT_TCP_PORT);
        return instance;
    }

    static void destroyInstance() {
        if(instance){
            delete instance;
            instance = NULL;
        }
    }

    TCPServer(s32 port);
    ~TCPServer();

    void CloseSockets();
    void ErrorHandling();

    void StartTCPThread(TCPServer * server);
    static void DoTCPThread(ControllerPatcherThread *thread, void *arg);
    void DoTCPThreadInternal();
    static void DetachConnectedNetworkController();
    static void AttachDetach(s32 attach);
    void DetachAndDelete();
    static TCPServer *instance;

    s32 RunTCP();

    struct sockaddr_in sock_addr;
    volatile s32 sockfd = -1;
    volatile s32 clientfd = -1;


    volatile s32 exitThread = 0;
    static ControllerPatcherThread *pThread;
};

#endif //_TCPSERVER_WINDOW_H_
