/*
   Copyright (C) 2020 by Damian Parrino <www.bucanero.com.ar>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#ifndef PS3_COMPAT_H
#define PS3_COMPAT_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netdb.h>
#include <net/socket.h>
#include <net/poll.h>
#include <netinet/in.h>

#define getnameinfo  nfs_getnameinfo
#define getaddrinfo  nfs_getaddrinfo
#define freeaddrinfo nfs_freeaddrinfo

int nfs_getnameinfo(const struct sockaddr *sa, socklen_t salen,
char *host, size_t hostlen,
char *serv, size_t servlen, int flags);
int nfs_getaddrinfo(const char *node, const char*service,
                const struct addrinfo *hints,
                struct addrinfo **res);
void nfs_freeaddrinfo(struct addrinfo *res);

#define IFNAMSIZ 16

/* just pretend they are the same so we compile */
#define sockaddr_in6 sockaddr_in

#define IPPORT_RESERVED   1024
#define MSG_NOSIGNAL      0x20000
#define O_NOFOLLOW        0400000
#define MINORBITS         20
#define MINORMASK         ((1U << MINORBITS) - 1)

#define major(dev)        ((unsigned int) ((dev) >> MINORBITS))
#define minor(dev)        ((unsigned int) ((dev) & MINORMASK))

#define IFF_UP            0x1     /* interface is up          */
#define IFF_BROADCAST     0x2     /* broadcast address valid  */
#define IFF_DEBUG         0x4     /* turn on debugging        */
#define IFF_LOOPBACK      0x8     /* is a loopback net        */


struct ifmap {
       unsigned long mem_start;
       unsigned long mem_end;
       unsigned short base_addr; 
       unsigned char irq;
       unsigned char dma;
       unsigned char port;
       /* 3 bytes spare */
};
  
struct ifreq {
    char ifr_name[IFNAMSIZ]; /* Interface name */
    union {
        struct sockaddr ifr_addr;
        struct sockaddr ifr_dstaddr;
        struct sockaddr ifr_broadaddr;
        struct sockaddr ifr_netmask;
        struct sockaddr ifr_hwaddr;
        short           ifr_flags;
        int             ifr_ifindex;
        int             ifr_metric;
        int             ifr_mtu;
        struct ifmap    ifr_map;
        char            ifr_slave[IFNAMSIZ];
        char            ifr_newname[IFNAMSIZ];
        char           *ifr_data;
    };
};

struct ifconf {
    int                 ifc_len; /* size of buffer */
    union {
        char           *ifc_buf; /* buffer address */
        struct ifreq   *ifc_req; /* array of structures */
    };
};

typedef uint32_t fsblkcnt_t;
typedef uint32_t fsfilcnt_t;

struct statvfs {
     unsigned long f_bsize;   
     unsigned long f_frsize;  
     fsblkcnt_t f_blocks;     
     fsblkcnt_t f_bfree;      
     fsblkcnt_t f_bavail;     
     fsfilcnt_t f_files;      
     fsfilcnt_t f_ffree;      
     fsfilcnt_t f_favail;     
     unsigned long f_fsid;    
     unsigned long f_flag;    
     unsigned long f_namemax; 
};

#endif
