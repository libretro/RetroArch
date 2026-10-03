/* Stub for vitasdk's <psp2/net/net.h>: the names and signatures
 * libretro-common/include/net/net_compat.h maps the socket API onto,
 * for the host-compiled Vita lanes in the compile matrix. Values are
 * placeholders; the Vita toolchain build is the authority on them. */
#ifndef _PSP2_NET_NET_H_
#define _PSP2_NET_NET_H_

#define SCE_NET_AF_INET          2
#define SCE_NET_SOCK_STREAM      1
#define SCE_NET_SOCK_DGRAM       2
#define SCE_NET_INADDR_ANY       0x00000000
#define SCE_NET_SOL_SOCKET       0xffff
#define SCE_NET_SO_REUSEADDR     0x00000004
#define SCE_NET_SO_KEEPALIVE     0x00000008
#define SCE_NET_SO_BROADCAST     0x00000020
#define SCE_NET_SO_SNDBUF        0x00001001
#define SCE_NET_SO_RCVBUF        0x00001002
#define SCE_NET_SO_SNDTIMEO      0x00001005
#define SCE_NET_SO_RCVTIMEO      0x00001006
#define SCE_NET_SO_ERROR         0x00001007
#define SCE_NET_SO_NBIO          0x00001100
#define SCE_NET_IPPROTO_IP       0
#define SCE_NET_IP_MULTICAST_TTL 10
#define SCE_NET_IPPROTO_TCP      6
#define SCE_NET_TCP_NODELAY      1
#define SCE_NET_IPPROTO_UDP      17
#define SCE_NET_MSG_DONTWAIT     0x00000080
#define SCE_NET_EPOLLIN          0x00000001
#define SCE_NET_EPOLLOUT         0x00000004
#define SCE_NET_EPOLLERR         0x00000008
#define SCE_NET_EPOLLHUP         0x00000010
#define SCE_NET_ERROR_EAGAIN      0x80410123
#define SCE_NET_ERROR_EWOULDBLOCK 0x80410123
#define SCE_NET_ERROR_EINPROGRESS 0x80410124

typedef struct SceNetInAddr
{
   unsigned int s_addr;
} SceNetInAddr;

typedef struct SceNetSockaddr
{
   unsigned char sa_len;
   unsigned char sa_family;
   char          sa_data[14];
} SceNetSockaddr;

typedef struct SceNetSockaddrIn
{
   unsigned char  sin_len;
   unsigned char  sin_family;
   unsigned short sin_port;
   SceNetInAddr   sin_addr;
   unsigned short sin_vport;
   char           sin_zero[6];
} SceNetSockaddrIn;

int sceNetSocket(const char *name, int domain, int type, int protocol);
int sceNetSocketClose(int s);
int sceNetBind(int s, const SceNetSockaddr *addr, unsigned int addrlen);
int sceNetListen(int s, int backlog);
int sceNetAccept(int s, SceNetSockaddr *addr, unsigned int *addrlen);
int sceNetConnect(int s, const SceNetSockaddr *name, unsigned int namelen);
int sceNetSend(int s, const void *msg, unsigned int len, int flags);
int sceNetSendto(int s, const void *msg, unsigned int len, int flags,
      const SceNetSockaddr *to, unsigned int tolen);
int sceNetRecv(int s, void *buf, unsigned int len, int flags);
int sceNetRecvfrom(int s, void *buf, unsigned int len, int flags,
      SceNetSockaddr *from, unsigned int *fromlen);
int sceNetSetsockopt(int s, int level, int optname, const void *optval,
      unsigned int optlen);
int sceNetGetsockopt(int s, int level, int optname, void *optval,
      unsigned int *optlen);
int sceNetGetsockname(int s, SceNetSockaddr *name, unsigned int *namelen);
unsigned int   sceNetHtonl(unsigned int host32);
unsigned int   sceNetNtohl(unsigned int net32);
unsigned short sceNetHtons(unsigned short host16);
unsigned short sceNetNtohs(unsigned short net16);
const char *sceNetInetNtop(int af, const void *src, char *dst, unsigned int size);
int sceNetInetPton(int af, const char *src, void *dst);

#endif
