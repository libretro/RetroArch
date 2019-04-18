#ifndef __UIP_ICMP_H__
#define __UIP_ICMP_H__

#include "uip.h"
#include "uip_arch.h"

#define UIP_ICMP_TTL		255

#define UIP_ICMP_ER 0      /* echo reply */
#define UIP_ICMP_DUR 3     /* destination unreachable */
#define UIP_ICMP_SQ 4      /* source quench */
#define UIP_ICMP_RD 5      /* redirect */
#define UIP_ICMP_ECHO 8    /* echo */
#define UIP_ICMP_TE 11     /* time exceeded */
#define UIP_ICMP_PP 12     /* parameter problem */
#define UIP_ICMP_TS 13     /* timestamp */
#define UIP_ICMP_TSR 14    /* timestamp reply */
#define UIP_ICMP_IRQ 15    /* information request */
#define UIP_ICMP_IR 16     /* information reply */

#define UIP_ICMPH_TYPE(hdr) (ntohs((hdr)->_type_code) >> 8)
#define UIP_ICMPH_CODE(hdr) (ntohs((hdr)->_type_code) & 0xff)

#define UIP_ICMPH_TYPE_SET(hdr, type) ((hdr)->_type_code = htons(UIP_ICMPH_CODE(hdr) | ((type) << 8)))
#define UIP_ICMPH_CODE_SET(hdr, code) ((hdr)->_type_code = htons((code) | (UIP_ICMPH_TYPE(hdr) << 8)))

enum uip_icmp_dur_type {
  UIP_ICMP_DUR_NET = 0,    /* net unreachable */
  UIP_ICMP_DUR_HOST = 1,   /* host unreachable */
  UIP_ICMP_DUR_PROTO = 2,  /* protocol unreachable */
  UIP_ICMP_DUR_PORT = 3,   /* port unreachable */
  UIP_ICMP_DUR_FRAG = 4,   /* fragmentation needed and DF set */
  UIP_ICMP_DUR_SR = 5      /* source route failed */
};

PACK_STRUCT_BEGIN
struct uip_icmp_echo_hdr {
	PACK_STRUCT_FIELD(u16_t _type_code);
	PACK_STRUCT_FIELD(u16_t chksum);
	PACK_STRUCT_FIELD(u16_t id);
	PACK_STRUCT_FIELD(u16_t seqno);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END

PACK_STRUCT_BEGIN
struct uip_icmp_dur_hdr {
	PACK_STRUCT_FIELD(u16_t _type_code);
	PACK_STRUCT_FIELD(u16_t chksum);
	PACK_STRUCT_FIELD(u32_t unused);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END

struct uip_pbuf;
struct uip_netif;

void uip_icmpinput(struct uip_pbuf *p,struct uip_netif *inp);
void uip_icmp_destunreach(struct uip_pbuf *p,enum uip_icmp_dur_type t);

#endif
