/* IPv4 addresses (big-endian, like the processor). */

#ifndef GEKKO_NETINET_IN_H
#define GEKKO_NETINET_IN_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>

#ifndef _IN_ADDR_T_DECLARED
typedef uint32_t in_addr_t;
#define _IN_ADDR_T_DECLARED
#endif
#ifndef _IN_PORT_T_DECLARED
typedef uint16_t in_port_t;
#define _IN_PORT_T_DECLARED
#endif

struct in_addr
{
   in_addr_t s_addr;
};

struct sockaddr_in
{
   uint8_t        sin_len;
   sa_family_t    sin_family;
   in_port_t      sin_port;
   struct in_addr sin_addr;
   char           sin_zero[8];
};

#define IPPROTO_IP        0
#define IPPROTO_ICMP      1
#define IPPROTO_TCP       6
#define IPPROTO_UDP       17

#define INADDR_ANY        ((in_addr_t)0x00000000)
#define INADDR_LOOPBACK   ((in_addr_t)0x7f000001)
#define INADDR_BROADCAST  ((in_addr_t)0xffffffff)
#define INADDR_NONE       ((in_addr_t)0xffffffff)

#define INET_ADDRSTRLEN   16

#define htonl(x) ((uint32_t)(x))
#define ntohl(x) ((uint32_t)(x))
#define htons(x) ((uint16_t)(x))
#define ntohs(x) ((uint16_t)(x))

#endif
