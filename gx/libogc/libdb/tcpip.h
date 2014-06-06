#ifndef __TCPIP_H__
#define __TCPIP_H__

#include "uIP/uip.h"
#include <sys/time.h>
#include <sys/types.h>

#define AF_UNSPEC			0
#define AF_INET				2
#define PF_INET				AF_INET
#define PF_UNSPEC			AF_UNSPEC

#define INADDR_ANY			0
#define INADDR_BROADCAST	0xffffffff

#ifndef socklen_t
#define socklen_t u32_t
#endif

#ifndef HAVE_IN_ADDR
#define HAVE_IN_ADDR
struct in_addr {
  u32 s_addr;
};
#endif

struct sockaddr_in {
  u8 sin_len;
  u8 sin_family;
  u16 sin_port;
  struct in_addr sin_addr;
  s8 sin_zero[8];
};

struct sockaddr {
  u8 sa_len;
  u8 sa_family;
  s8 sa_data[14];
};

struct dbginterface* tcpip_init(struct uip_ip_addr *localip,struct uip_ip_addr *netmask,struct uip_ip_addr *gateway,u16 port);

void tcpip_close(s32_t s);
void tcpip_starttimer(s32_t s);
void tcpip_stoptimer(s32_t s);
s32_t tcpip_socket();
s32_t tcpip_listen(s32_t s,u32_t backlog);
s32_t tcpip_bind(s32_t s,struct sockaddr *name,socklen_t *namelen);
s32_t tcpip_accept(s32_t s);
s32_t tcpip_read(s32_t s,void *buffer,u32_t len);
s32_t tcpip_write(s32_t s,const void *buffer,u32_t len);

#endif
