#ifndef _CONTROLLERPATCHERNET_H_
#define _CONTROLLERPATCHERNET_H_

#include "wiiu/types.h"

class ControllerPatcherNet{
    friend class TCPServer;
    friend class UDPServer;
    private:
        static s32 recvwait(s32 sock, void *buffer, s32 len);
        static u8 recvbyte(s32 sock);
        static s32 checkbyte(s32 sock);
        static s32 sendwait(s32 sock, const void *buffer, s32 len);
        static s32 sendbyte(s32 sock, unsigned char byte);
};

#endif
