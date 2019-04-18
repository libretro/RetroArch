#ifndef __UIP_IP_H__
#define __UIP_IP_H__

#include "uip.h"

#define UIP_INADDR_NONE			((u32_t) 0xffffffff)  /* 255.255.255.255 */
#define UIP_INADDR_LOOPBACK		((u32_t) 0x7f000001)  /* 127.0.0.1 */

#define UIP_IPH_V(hdr)  (ntohs((hdr)->_v_hl_tos) >> 12)
#define UIP_IPH_HL(hdr) ((ntohs((hdr)->_v_hl_tos) >> 8) & 0x0f)
#define UIP_IPH_TOS(hdr) (ntohs((hdr)->_v_hl_tos) & 0xff)
#define UIP_IPH_LEN(hdr) ((hdr)->_len)
#define UIP_IPH_ID(hdr) ((hdr)->_id)
#define UIP_IPH_OFFSET(hdr) ((hdr)->_offset)
#define UIP_IPH_TTL(hdr) (ntohs((hdr)->_ttl_proto) >> 8)
#define UIP_IPH_PROTO(hdr) (ntohs((hdr)->_ttl_proto) & 0xff)
#define UIP_IPH_CHKSUM(hdr) ((hdr)->_chksum)

#define UIP_IPH_VHLTOS_SET(hdr, v, hl, tos) (hdr)->_v_hl_tos = (htons(((v) << 12) | ((hl) << 8) | (tos)))
#define UIP_IPH_LEN_SET(hdr, len) (hdr)->_len = (len)
#define UIP_IPH_ID_SET(hdr, id) (hdr)->_id = (id)
#define UIP_IPH_OFFSET_SET(hdr, off) (hdr)->_offset = (off)
#define UIP_IPH_TTL_SET(hdr, ttl) (hdr)->_ttl_proto = (htons(UIP_IPH_PROTO(hdr) | ((ttl) << 8)))
#define UIP_IPH_PROTO_SET(hdr, proto) (hdr)->_ttl_proto = (htons((proto) | (UIP_IPH_TTL(hdr) << 8)))
#define UIP_IPH_CHKSUM_SET(hdr, chksum) (hdr)->_chksum = (chksum)

/*
 * Option flags per-socket. These are the same like SO_XXX.
 */
#define	UIP_SOF_DEBUG	    (u16_t)0x0001U		/* turn on debugging info recording */
#define	UIP_SOF_ACCEPTCONN	(u16_t)0x0002U		/* socket has had listen() */
#define	UIP_SOF_REUSEADDR	(u16_t)0x0004U		/* allow local address reuse */
#define	UIP_SOF_KEEPALIVE	(u16_t)0x0008U		/* keep connections alive */
#define	UIP_SOF_DONTROUTE	(u16_t)0x0010U		/* just use interface addresses */
#define	UIP_SOF_BROADCAST	(u16_t)0x0020U		/* permit sending of broadcast msgs */
#define	UIP_SOF_USELOOPBACK	(u16_t)0x0040U		/* bypass hardware when possible */
#define	UIP_SOF_LINGER	    (u16_t)0x0080U		/* linger on close if data present */
#define	UIP_SOF_OOBINLINE	(u16_t)0x0100U		/* leave received OOB data in line */
#define	UIP_SOF_REUSEPORT	(u16_t)0x0200U		/* allow local address & port reuse */

#define IP4_ADDR(ipaddr, a,b,c,d) (ipaddr)->addr = htonl(((u32_t)(a & 0xff) << 24) | ((u32_t)(b & 0xff) << 16) | \
                                                         ((u32_t)(c & 0xff) << 8) | (u32_t)(d & 0xff))

#define ip_addr_set(dest, src) (dest)->addr = \
                               ((src) == NULL? 0:\
                               (src)->addr)

/**
 * Determine if two address are on the same network.
 *
 * @arg addr1 IP address 1
 * @arg addr2 IP address 2
 * @arg mask network identifier mask
 * @return !0 if the network identifiers of both address match
 */
#define ip_addr_netcmp(addr1, addr2, mask) (((addr1)->addr & \
                                              (mask)->addr) == \
                                             ((addr2)->addr & \
                                              (mask)->addr))
#define ip_addr_cmp(addr1, addr2) ((addr1)->addr == (addr2)->addr)

#define ip_addr_isany(addr1) ((addr1) == NULL || (addr1)->addr == 0)

#define ip_addr_isbroadcast			uip_ipaddr_isbroadcast

#define ip_addr_ismulticast(addr1) (((addr1)->addr & ntohl(0xf0000000)) == ntohl(0xe0000000))

#define ip4_addr1(ipaddr) ((u16_t)(ntohl((ipaddr)->addr) >> 24) & 0xff)
#define ip4_addr2(ipaddr) ((u16_t)(ntohl((ipaddr)->addr) >> 16) & 0xff)
#define ip4_addr3(ipaddr) ((u16_t)(ntohl((ipaddr)->addr) >> 8) & 0xff)
#define ip4_addr4(ipaddr) ((u16_t)(ntohl((ipaddr)->addr)) & 0xff)

#ifndef HAVE_IN_ADDR
#define HAVE_IN_ADDR
struct in_addr {
  u32 s_addr;
};
#endif

/* The IP Address */
PACK_STRUCT_BEGIN
struct uip_ip_addr {
	PACK_STRUCT_FIELD(u32_t addr);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END

PACK_STRUCT_BEGIN
struct uip_ip_addr2 {
	PACK_STRUCT_FIELD(u16_t addrw[2]);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END

/* The IP Header */
PACK_STRUCT_BEGIN
struct uip_ip_hdr {
#define UIP_IP_RF		0x8000
#define UIP_IP_DF		0x4000
#define UIP_IP_MF		0x2000
#define UIP_IP_OFFMASK	0x1fff
	PACK_STRUCT_FIELD(u16_t _v_hl_tos);
	PACK_STRUCT_FIELD(u16_t _len);
	PACK_STRUCT_FIELD(u16_t _id);
	PACK_STRUCT_FIELD(u16_t _offset);
	PACK_STRUCT_FIELD(u16_t _ttl_proto);
	PACK_STRUCT_FIELD(u16_t _chksum);

	PACK_STRUCT_FIELD(struct uip_ip_addr src);
	PACK_STRUCT_FIELD(struct uip_ip_addr dst);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END

#define UIP_IP_PCB					\
	struct uip_ip_addr local_ip;	\
	struct uip_ip_addr remote_ip;	\
	u16_t so_options;				\
	u8_t tos;						\
	u8_t ttl

struct uip_pbuf;
struct uip_netif;
struct ip_addr;

void uip_ipinit();

u32_t uip_ipaddr(const u8_t *cp);
s32_t uip_ipaton(const u8_t *cp,struct in_addr *addr);
s8_t uip_ipinput(struct uip_pbuf *p,struct uip_netif *inp);
s8_t uip_ipoutput(struct uip_pbuf *p,struct uip_ip_addr *src,struct uip_ip_addr *dst,u8_t ttl,u8_t tos,u8_t proto);
s8_t uip_ipoutput_if(struct uip_pbuf *p,struct uip_ip_addr *src,struct uip_ip_addr *dst,u8_t ttl,u8_t tos,u8_t proto,struct uip_netif *netif);
struct uip_netif* uip_iproute(struct uip_ip_addr *dst);
u8_t uip_ipaddr_isbroadcast(struct uip_ip_addr *addr,struct uip_netif *netif);

#endif
