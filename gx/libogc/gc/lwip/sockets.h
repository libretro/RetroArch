/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */


#ifndef __LWIP_SOCKETS_H__
#define __LWIP_SOCKETS_H__
#include "lwip/ip_addr.h"

struct sockaddr_in {
  u8_t sin_len;
  u8_t sin_family;
  u16_t sin_port;
  struct in_addr sin_addr;
  char sin_zero[8];
};

struct sockaddr {
  u8_t sa_len;
  u8_t sa_family;
  char sa_data[14];
};

#ifndef socklen_t
#  define socklen_t int
#endif


#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define SOCK_RAW        3

/*
 * Option flags per-socket.
 */
#define  SO_DEBUG  0x0001    /* turn on debugging info recording */
#define  SO_ACCEPTCONN  0x0002    /* socket has had listen() */
#define  SO_REUSEADDR  0x0004    /* allow local address reuse */
#define  SO_KEEPALIVE  0x0008    /* keep connections alive */
#define  SO_DONTROUTE  0x0010    /* just use interface addresses */
#define  SO_BROADCAST  0x0020    /* permit sending of broadcast msgs */
#define  SO_USELOOPBACK  0x0040    /* bypass hardware when possible */
#define  SO_LINGER  0x0080    /* linger on close if data present */
#define  SO_OOBINLINE  0x0100    /* leave received OOB data in line */
#define	 SO_REUSEPORT	0x0200		/* allow local address & port reuse */

#define SO_DONTLINGER   (int)(~SO_LINGER)

/*
 * Additional options, not kept in so_options.
 */
#define SO_SNDBUF  0x1001    /* send buffer size */
#define SO_RCVBUF  0x1002    /* receive buffer size */
#define SO_SNDLOWAT  0x1003    /* send low-water mark */
#define SO_RCVLOWAT  0x1004    /* receive low-water mark */
#define SO_SNDTIMEO  0x1005    /* send timeout */
#define SO_RCVTIMEO  0x1006    /* receive timeout */
#define  SO_ERROR  0x1007    /* get error status and clear */
#define  SO_TYPE    0x1008    /* get socket type */



/*
 * Structure used for manipulating linger option.
 */
struct linger {
       int l_onoff;                /* option on/off */
       int l_linger;               /* linger time */
};

/*
 * Level number for (get/set)sockopt() to apply to socket itself.
 */
#define  SOL_SOCKET  0xfff    /* options for socket level */


#define AF_UNSPEC       0
#define AF_INET         2
#define PF_INET         AF_INET
#define PF_UNSPEC       AF_UNSPEC

#define IPPROTO_IP      0
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17

#define INADDR_ANY      0
#define INADDR_BROADCAST 0xffffffff

/* Flags we can use with send and recv. */
#define MSG_DONTWAIT    0x40            /* Nonblocking i/o for this operation only */


/*
 * Options for level IPPROTO_IP
 */
#define IP_TOS       1
#define IP_TTL       2


#define IPTOS_TOS_MASK          0x1E
#define IPTOS_TOS(tos)          ((tos) & IPTOS_TOS_MASK)
#define IPTOS_LOWDELAY          0x10
#define IPTOS_THROUGHPUT        0x08
#define IPTOS_RELIABILITY       0x04
#define IPTOS_LOWCOST           0x02
#define IPTOS_MINCOST           IPTOS_LOWCOST

/*
 * Definitions for IP precedence (also in ip_tos) (hopefully unused)
 */
#define IPTOS_PREC_MASK                 0xe0
#define IPTOS_PREC(tos)                ((tos) & IPTOS_PREC_MASK)
#define IPTOS_PREC_NETCONTROL           0xe0
#define IPTOS_PREC_INTERNETCONTROL      0xc0
#define IPTOS_PREC_CRITIC_ECP           0xa0
#define IPTOS_PREC_FLASHOVERRIDE        0x80
#define IPTOS_PREC_FLASH                0x60
#define IPTOS_PREC_IMMEDIATE            0x40
#define IPTOS_PREC_PRIORITY             0x20
#define IPTOS_PREC_ROUTINE              0x00


/*
 * Commands for ioctlsocket(),  taken from the BSD file fcntl.h.
 *
 *
 * Ioctl's have the command encoded in the lower word,
 * and the size of any in or out parameters in the upper
 * word.  The high 2 bits of the upper word are used
 * to encode the in/out status of the parameter; for now
 * we restrict parameters to at most 128 bytes.
 */
#if !defined(FIONREAD) || !defined(FIONBIO)
#define IOCPARM_MASK    0x7f            /* parameters must be < 128 bytes */
#define IOC_VOID        0x20000000      /* no parameters */
#define IOC_OUT         0x40000000      /* copy out parameters */
#define IOC_IN          0x80000000      /* copy in parameters */
#define IOC_INOUT       (IOC_IN|IOC_OUT)
                                        /* 0x20000000 distinguishes new &
                                           old ioctl's */
#define _IO(x,y)        (IOC_VOID|((x)<<8)|(y))

#define _IOR(x,y,t)     (IOC_OUT|(((long)sizeof(t)&IOCPARM_MASK)<<16)|((x)<<8)|(y))

#define _IOW(x,y,t)     (IOC_IN|(((long)sizeof(t)&IOCPARM_MASK)<<16)|((x)<<8)|(y))
#endif

#ifndef FIONREAD
#define FIONREAD    _IOR('f', 127, unsigned long) /* get # bytes to read */
#endif
#ifndef FIONBIO
#define FIONBIO     _IOW('f', 126, unsigned long) /* set/clear non-blocking i/o */
#endif

/* Socket I/O Controls */
#ifndef SIOCSHIWAT
#define SIOCSHIWAT  _IOW('s',  0, unsigned long)  /* set high watermark */
#define SIOCGHIWAT  _IOR('s',  1, unsigned long)  /* get high watermark */
#define SIOCSLOWAT  _IOW('s',  2, unsigned long)  /* set low watermark */
#define SIOCGLOWAT  _IOR('s',  3, unsigned long)  /* get low watermark */
#define SIOCATMARK  _IOR('s',  7, unsigned long)  /* at oob mark? */
#endif

#ifndef O_NONBLOCK
#define O_NONBLOCK    04000U
#endif

#ifndef FD_SET
  #undef  FD_SETSIZE
  #define FD_SETSIZE    16
  #define FD_SET(n, p)  ((p)->fd_bits[(n)/8] |=  (1 << ((n) & 7)))
  #define FD_CLR(n, p)  ((p)->fd_bits[(n)/8] &= ~(1 << ((n) & 7)))
  #define FD_ISSET(n,p) ((p)->fd_bits[(n)/8] &   (1 << ((n) & 7)))
  #define FD_ZERO(p)    memset((void*)(p),0,sizeof(*(p)))

  typedef struct fd_set {
          unsigned char fd_bits [(FD_SETSIZE+7)/8];
        } fd_set;

/* 
 * only define this in sockets.c so it does not interfere
 * with other projects namespaces where timeval is present
 */ 
#ifndef LWIP_TIMEVAL_PRIVATE
#define LWIP_TIMEVAL_PRIVATE 1
#endif

#if LWIP_TIMEVAL_PRIVATE
  struct timeval {
    long    tv_sec;         /* seconds */
    long    tv_usec;        /* and microseconds */
  };
#endif

#endif

int lwip_accept(int s, struct sockaddr *addr, socklen_t *addrlen);
int lwip_bind(int s, struct sockaddr *name, socklen_t namelen);
int lwip_shutdown(int s, int how);
int lwip_getpeername (int s, struct sockaddr *name, socklen_t *namelen);
int lwip_getsockname (int s, struct sockaddr *name, socklen_t *namelen);
int lwip_getsockopt (int s, int level, int optname, void *optval, socklen_t *optlen);
int lwip_setsockopt (int s, int level, int optname, const void *optval, socklen_t optlen);
int lwip_close(int s);
int lwip_connect(int s, struct sockaddr *name, socklen_t namelen);
int lwip_listen(int s, int backlog);
int lwip_recv(int s, void *mem, int len, unsigned int flags);
int lwip_read(int s, void *mem, int len);
int lwip_recvfrom(int s, void *mem, int len, unsigned int flags,
      struct sockaddr *from, socklen_t *fromlen);
int lwip_send(int s, void *dataptr, int size, unsigned int flags);
int lwip_sendto(int s, void *dataptr, int size, unsigned int flags,
    struct sockaddr *to, socklen_t tolen);
int lwip_socket(int domain, int type, int protocol);
int lwip_write(int s, void *dataptr, int size);
int lwip_select(int maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset,
                struct timeval *timeout);
int lwip_ioctl(int s, long cmd, void *argp);

#if LWIP_COMPAT_SOCKETS
#define accept(a,b,c)         lwip_accept(a,b,c)
#define bind(a,b,c)           lwip_bind(a,b,c)
#define shutdown(a,b)         lwip_shutdown(a,b)
#define close(s)              lwip_close(s)
#define connect(a,b,c)        lwip_connect(a,b,c)
#define getsockname(a,b,c)    lwip_getsockname(a,b,c)
#define getpeername(a,b,c)    lwip_getpeername(a,b,c)
#define setsockopt(a,b,c,d,e) lwip_setsockopt(a,b,c,d,e)
#define getsockopt(a,b,c,d,e) lwip_getsockopt(a,b,c,d,e)
#define listen(a,b)           lwip_listen(a,b)
#define recv(a,b,c,d)         lwip_recv(a,b,c,d)
#define read(a,b,c)           lwip_read(a,b,c)
#define recvfrom(a,b,c,d,e,f) lwip_recvfrom(a,b,c,d,e,f)
#define send(a,b,c,d)         lwip_send(a,b,c,d)
#define sendto(a,b,c,d,e,f)   lwip_sendto(a,b,c,d,e,f)
#define socket(a,b,c)         lwip_socket(a,b,c)
#define write(a,b,c)          lwip_write(a,b,c)
#define select(a,b,c,d,e)     lwip_select(a,b,c,d,e)
#define ioctlsocket(a,b,c)    lwip_ioctl(a,b,c)
#endif /* LWIP_COMPAT_SOCKETS */

#endif /* __LWIP_SOCKETS_H__ */

