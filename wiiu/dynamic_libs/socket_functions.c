/****************************************************************************
 * Copyright (C) 2015
 * by Dimok
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 ***************************************************************************/
#include "os_functions.h"
#include "socket_functions.h"

u32 hostIpAddress = 0;

unsigned int nsysnet_handle __attribute__((section(".data"))) = 0;

EXPORT_DECL(void, socket_lib_init, void);
EXPORT_DECL(int, socket, int domain, int type, int protocol);
EXPORT_DECL(int, socketclose, int s);
EXPORT_DECL(int, connect, int s, void *addr, int addrlen);
EXPORT_DECL(int, bind, s32 s,struct sockaddr *name,s32 namelen);
EXPORT_DECL(int, listen, s32 s,u32 backlog);
EXPORT_DECL(int, accept, s32 s,struct sockaddr *addr,s32 *addrlen);
EXPORT_DECL(int, send, int s, const void *buffer, int size, int flags);
EXPORT_DECL(int, recv, int s, void *buffer, int size, int flags);
EXPORT_DECL(int, recvfrom,int sockfd, void *buf, int len, int flags,struct sockaddr *src_addr, int *addrlen);
EXPORT_DECL(int, sendto, int s, const void *buffer, int size, int flags, const struct sockaddr *dest, int dest_len);
EXPORT_DECL(int, setsockopt, int s, int level, int optname, void *optval, int optlen);
EXPORT_DECL(char *, inet_ntoa, struct in_addr in);
EXPORT_DECL(int, inet_aton, const char *cp, struct in_addr *inp);

EXPORT_DECL(int, NSSLWrite, int connection, const void* buf, int len,int * written);
EXPORT_DECL(int, NSSLRead, int connection, const void* buf, int len,int * read);
EXPORT_DECL(int, NSSLCreateConnection, int context, const char* host, int hotlen,int options,int sock,int block);

void InitAcquireSocket(void)
{
    OSDynLoad_Acquire("nsysnet.rpl", &nsysnet_handle);
}

void InitSocketFunctionPointers(void)
{
    unsigned int *funcPointer = 0;

    InitAcquireSocket();

    OS_FIND_EXPORT(nsysnet_handle, socket_lib_init);
    OS_FIND_EXPORT(nsysnet_handle, socket);
    OS_FIND_EXPORT(nsysnet_handle, socketclose);
    OS_FIND_EXPORT(nsysnet_handle, connect);
    OS_FIND_EXPORT(nsysnet_handle, bind);
    OS_FIND_EXPORT(nsysnet_handle, listen);
    OS_FIND_EXPORT(nsysnet_handle, accept);
    OS_FIND_EXPORT(nsysnet_handle, send);
    OS_FIND_EXPORT(nsysnet_handle, recv);
    OS_FIND_EXPORT(nsysnet_handle, recvfrom);
    OS_FIND_EXPORT(nsysnet_handle, sendto);
    OS_FIND_EXPORT(nsysnet_handle, setsockopt);
    OS_FIND_EXPORT(nsysnet_handle, inet_ntoa);
    OS_FIND_EXPORT(nsysnet_handle, inet_aton);

    OS_FIND_EXPORT(nsysnet_handle, NSSLWrite);
    OS_FIND_EXPORT(nsysnet_handle, NSSLRead);
    OS_FIND_EXPORT(nsysnet_handle, NSSLCreateConnection);

    socket_lib_init();
}
