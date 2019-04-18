#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <gctypes.h>
#include <sys/time.h>
#include <sys/types.h>

#define INVALID_SOCKET	(~0)
#define SOCKET_ERROR	(-1)

#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define SOCK_RAW        3

/*
 * Option flags per-socket.
 */
#define  SO_DEBUG			0x0001    /* turn on debugging info recording */
#define  SO_ACCEPTCONN		0x0002    /* socket has had listen() */
#define  SO_REUSEADDR		0x0004    /* allow local address reuse */
#define  SO_KEEPALIVE		0x0008    /* keep connections alive */
#define  SO_DONTROUTE		0x0010    /* just use interface addresses */
#define  SO_BROADCAST		0x0020    /* permit sending of broadcast msgs */
#define  SO_USELOOPBACK		0x0040    /* bypass hardware when possible */
#define  SO_LINGER			0x0080    /* linger on close if data present */
#define  SO_OOBINLINE		0x0100    /* leave received OOB data in line */
#define	 SO_REUSEPORT		0x0200		/* allow local address & port reuse */

#define SO_DONTLINGER		(int)(~SO_LINGER)

/*
 * Additional options, not kept in so_options.
 */
#define SO_SNDBUF			0x1001    /* send buffer size */
#define SO_RCVBUF			0x1002    /* receive buffer size */
#define SO_SNDLOWAT			0x1003    /* send low-water mark */
#define SO_RCVLOWAT			0x1004    /* receive low-water mark */
#define SO_SNDTIMEO			0x1005    /* send timeout */
#define SO_RCVTIMEO			0x1006    /* receive timeout */
#define  SO_ERROR			0x1007    /* get error status and clear */
#define  SO_TYPE			0x1008    /* get socket type */

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
#define  SOL_SOCKET			0xffff    /* options for socket level */

#define AF_UNSPEC			0
#define AF_INET				2
#define PF_INET				AF_INET
#define PF_UNSPEC			AF_UNSPEC

#define IPPROTO_IP			0
#define IPPROTO_TCP			6
#define IPPROTO_UDP			17

#define INADDR_ANY			0
#define INADDR_BROADCAST	0xffffffff

/* Flags we can use with send and recv. */
#define MSG_DONTWAIT		0x40            /* Nonblocking i/o for this operation only */

/*
 * Options for level IPPROTO_IP
 */
#define IP_TOS				1
#define IP_TTL				2

#define IPTOS_TOS_MASK      0x1E
#define IPTOS_TOS(tos)      ((tos) & IPTOS_TOS_MASK)
#define IPTOS_LOWDELAY      0x10
#define IPTOS_THROUGHPUT    0x08
#define IPTOS_RELIABILITY   0x04
#define IPTOS_LOWCOST       0x02
#define IPTOS_MINCOST       IPTOS_LOWCOST

/*
 * Definitions for IP precedence (also in ip_tos) (hopefully unused)
 */
#define IPTOS_PREC_MASK                 0xe0
#define IPTOS_PREC(tos)                 ((tos) & IPTOS_PREC_MASK)
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
#define O_NONBLOCK			04000U
#endif

#ifndef FD_SET
  #undef  FD_SETSIZE
  #define FD_SETSIZE		16
  #define FD_SET(n, p)		((p)->fd_bits[(n)/8] |=  (1 << ((n) & 7)))
  #define FD_CLR(n, p)		((p)->fd_bits[(n)/8] &= ~(1 << ((n) & 7)))
  #define FD_ISSET(n,p)		((p)->fd_bits[(n)/8] &   (1 << ((n) & 7)))
  #define FD_ZERO(p)		memset((void*)(p),0,sizeof(*(p)))

  typedef struct fd_set {
	u8 fd_bits [(FD_SETSIZE+7)/8];
  } fd_set;

#endif

#ifndef TCP_NODELAY
#define	TCP_NODELAY	   0x01	   /* don't delay send to coalesce packets */
#endif
#ifndef TCP_KEEPALIVE
#define TCP_KEEPALIVE  0x02    /* send KEEPALIVE probes when idle for pcb->keepalive miliseconds */
#endif

#ifndef socklen_t
#define socklen_t u32
#endif

#ifndef htons
#define htons(x) (x)
#endif
#ifndef ntohs
#define ntohs(x) (x)
#endif
#ifndef htonl
#define htonl(x) (x)
#endif
#ifndef ntohl
#define ntohl(x) (x)
#endif

#ifndef h_addr
#define h_addr h_addr_list[0]
#endif

#ifndef IP4_ADDR
#define IP4_ADDR(ipaddr, a,b,c,d) (ipaddr)->s_addr = htonl(((u32)(a&0xff)<<24)|((u32)(b&0xff)<<16)|((u32)(c&0xff)<<8)|(u32)(d&0xff))
#define ip4_addr1(ipaddr) ((u32)(ntohl((ipaddr)->s_addr) >> 24) & 0xff)
#define ip4_addr2(ipaddr) ((u32)(ntohl((ipaddr)->s_addr) >> 16) & 0xff)
#define ip4_addr3(ipaddr) ((u32)(ntohl((ipaddr)->s_addr) >> 8) & 0xff)
#define ip4_addr4(ipaddr) ((u32)(ntohl((ipaddr)->s_addr)) & 0xff)
#endif

#define POLLIN				0x0001
#define POLLPRI				0x0002
#define POLLOUT				0x0004
#define POLLERR				0x0008
#define POLLHUP				0x0010
#define POLLNVAL			0x0020

#ifdef __cplusplus
extern "C" {
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

struct hostent {
  char    *h_name;        /* official name of host */
  char    **h_aliases;    /* alias list */
  u16     h_addrtype;     /* host address type */
  u16     h_length;       /* length of address */
  char    **h_addr_list;  /* list of addresses from name server */
};

struct pollsd {
	s32 socket;
	u32 events;
	u32 revents;
};

u32 inet_addr(const char *cp);
s8 inet_aton(const char *cp, struct in_addr *addr);
char *inet_ntoa(struct in_addr addr); /* returns ptr to static buffer; not reentrant! */

s32 if_config( char *local_ip, char *netmask, char *gateway,bool use_dhcp, int max_retries);
s32 if_configex(struct in_addr *local_ip, struct in_addr *netmask, struct in_addr *gateway, bool use_dhcp, int max_retries);

s32 net_init();
#ifdef HW_RVL
typedef s32 (*netcallback)(s32 result, void *usrdata);
s32 net_init_async(netcallback cb, void *usrdata);
s32 net_get_status(void);
void net_wc24cleanup();
s32 net_get_mac_address(void *mac_buf);
#endif
void net_deinit();

u32 net_gethostip();
s32 net_socket(u32 domain,u32 type,u32 protocol);
s32 net_bind(s32 s,struct sockaddr *name,socklen_t namelen);
s32 net_listen(s32 s,u32 backlog);
s32 net_accept(s32 s,struct sockaddr *addr,socklen_t *addrlen);
s32 net_connect(s32 s,struct sockaddr *,socklen_t);
s32 net_write(s32 s,const void *data,s32 size);
s32 net_send(s32 s,const void *data,s32 size,u32 flags);
s32 net_sendto(s32 s,const void *data,s32 len,u32 flags,struct sockaddr *to,socklen_t tolen);
s32 net_recv(s32 s,void *mem,s32 len,u32 flags);
s32 net_recvfrom(s32 s,void *mem,s32 len,u32 flags,struct sockaddr *from,socklen_t *fromlen);
s32 net_read(s32 s,void *mem,s32 len);
s32 net_close(s32 s);
s32 net_select(s32 maxfdp1,fd_set *readset,fd_set *writeset,fd_set *exceptset,struct timeval *timeout);
s32 net_setsockopt(s32 s,u32 level,u32 optname,const void *optval,socklen_t optlen);
s32 net_ioctl(s32 s, u32 cmd, void *argp);
s32 net_fcntl(s32 s, u32 cmd, u32 flags);
s32 net_poll(struct pollsd *sds,s32 nsds,s32 timeout);
s32 net_shutdown(s32 s, u32 how);

struct hostent * net_gethostbyname(const char *addrString);

#ifdef __cplusplus
	}
#endif

#endif
