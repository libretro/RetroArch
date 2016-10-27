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
#ifndef __SOCKET_FUNCTIONS_H_
#define __SOCKET_FUNCTIONS_H_

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned int nsysnet_handle;

#include <gctypes.h>

#define INADDR_ANY      0

#define AF_INET         2

#define SOCK_STREAM     1
#define SOCK_DGRAM      2

#define IPPROTO_IP      0
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17

#define TCP_NODELAY     0x2004

#define SOL_SOCKET      -1
#define SO_REUSEADDR    0x0004
#define SO_NONBLOCK     0x1016
#define SO_MYADDR       0x1013
#define SO_RCVTIMEO	0x1006

#define SOL_SOCKET      -1
#define MSG_DONTWAIT    32

#define htonl(x) x
#define htons(x) x
#define ntohl(x) x
#define ntohs(x) x


struct in_addr {
    unsigned int s_addr;
};
struct sockaddr_in {
    short sin_family;
    unsigned short sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};

struct sockaddr
{
   unsigned short sa_family;
   char sa_data[14];
};


void InitSocketFunctionPointers(void);
void InitAcquireSocket(void);

extern void (*socket_lib_init)(void);
extern int (*socket)(int domain, int type, int protocol);
extern int (*socketclose)(int s);
extern int (*connect)(int s, void *addr, int addrlen);
extern int (*bind)(s32 s,struct sockaddr *name,s32 namelen);
extern int (*listen)(s32 s,u32 backlog);
extern int (*accept)(s32 s,struct sockaddr *addr,s32 *addrlen);
extern int (*send)(int s, const void *buffer, int size, int flags);
extern int (*recv)(int s, void *buffer, int size, int flags);
extern int (*recvfrom)(int sockfd, void *buf, int len, int flags,struct sockaddr *src_addr, int *addrlen);

extern int (*sendto)(int s, const void *buffer, int size, int flags, const struct sockaddr *dest, int dest_len);
extern int (*setsockopt)(int s, int level, int optname, void *optval, int optlen);

extern int (* NSSLWrite)(int connection, const void* buf, int len,int * written);
extern int (* NSSLRead)(int connection, const void* buf, int len,int * read);
extern int (* NSSLCreateConnection)(int context, const char* host, int hotlen,int options,int sock,int block);

extern char * (*inet_ntoa)(struct in_addr in);
extern int (*inet_aton)(const char *cp, struct in_addr *inp);

#ifdef __cplusplus
}
#endif

#endif // __SOCKET_FUNCTIONS_H_
