#ifndef	_NETINET_IN_H
#define	_NETINET_IN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct in_addr
{
   unsigned int s_addr;
};

#define IPPROTO_IP      0
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17

struct sockaddr_in
{
   short sin_family;
   unsigned short sin_port;
   struct in_addr sin_addr;
   char sin_zero[8];
};

uint32_t ntohl (uint32_t netlong);
uint16_t ntohs (uint16_t netshort);
uint32_t htonl (uint32_t hostlong);
uint16_t htons (uint16_t hostshort);

#ifdef __cplusplus
}
#endif

#endif	/* _NETINET_IN_H */
