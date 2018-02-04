#include <stdlib.h>
#include <string.h>

#include "uip_ip.h"
#include "uip_pbuf.h"
#include "uip_netif.h"
#include "uip_icmp.h"

#if UIP_LOGGING == 1
#include <stdio.h>
#define UIP_LOG(m) uip_log(__FILE__,__LINE__,m)
#else
#define UIP_LOG(m)
#endif /* UIP_LOGGING == 1 */

#if UIP_STATISTICS == 1
struct uip_stats uip_stat;
#define UIP_STAT(s) s
#else
#define UIP_STAT(s)
#endif /* UIP_STATISTICS == 1 */

void uip_icmpinput(struct uip_pbuf *p,struct uip_netif *inp)
{
	u8_t type;
	u16_t hlen;
	struct uip_ip_addr tmpaddr;
	struct uip_ip_hdr *iphdr;
	struct uip_icmp_echo_hdr *iecho;

	iphdr = p->payload;
	hlen = UIP_IPH_HL(iphdr)*4;
	if(uip_pbuf_header(p,-((s16_t)hlen)) || p->tot_len<sizeof(u16_t)*2) {
		UIP_LOG("uip_icmpinput: short ICMP received.\n");
		uip_pbuf_free(p);
		return;
	}

	type = *((u8_t*)p->payload);
	//code = *((u8_t*)p->payload+1);
	switch(type) {
		case UIP_ICMP_ECHO:
			if(ip_addr_isbroadcast(&iphdr->dst,inp) || ip_addr_ismulticast(&iphdr->dst)) {
				UIP_LOG("uip_icmpinput: Not echoing to broadcast pings.\n");
				uip_pbuf_free(p);
				return;
			}

			if(p->tot_len<sizeof(struct uip_icmp_echo_hdr)) {
				UIP_LOG("uip_icmpinput: bad ICMP echo received.\n");
				uip_pbuf_free(p);
				return;
			}

			iecho = p->payload;
			if(uip_ipchksum_pbuf(p)!=0) {
				UIP_LOG("uip_icmpinput: checksum failed for received ICMP echo.\n");
				uip_pbuf_free(p);
				return;
			}

			tmpaddr.addr = iphdr->src.addr;
			iphdr->src.addr = iphdr->dst.addr;
			iphdr->dst.addr = tmpaddr.addr;
			UIP_ICMPH_TYPE_SET(iecho,UIP_ICMP_ER);

			if(iecho->chksum>=htons(0xffff-(UIP_ICMP_ECHO<<8))) iecho->chksum += htons(UIP_ICMP_ECHO<<8)+1;
			else iecho->chksum += htons(UIP_ICMP_ECHO<<8);

			uip_pbuf_header(p,hlen);
			uip_ipoutput_if(p,&iphdr->src,NULL,UIP_IPH_TTL(iphdr),0,UIP_PROTO_ICMP,inp);
			break;
		default:
			UIP_LOG("uip_icmpinput: ICMP type/code not supported.\n");
			break;
	}
	uip_pbuf_free(p);
}

void uip_icmp_destunreach(struct uip_pbuf *p,enum uip_icmp_dur_type t)
{
	struct uip_pbuf *q;
	struct uip_ip_hdr *iphdr;
	struct uip_icmp_dur_hdr *idur;

	q = uip_pbuf_alloc(UIP_PBUF_IP,sizeof(struct uip_icmp_dur_hdr)+UIP_IP_HLEN+8,UIP_PBUF_RAM);

	iphdr = p->payload;
	idur = q->payload;

	UIP_ICMPH_TYPE_SET(idur,UIP_ICMP_DUR);
	UIP_ICMPH_CODE_SET(idur,t);

	UIP_MEMCPY((u8_t*)q->payload+sizeof(struct uip_icmp_dur_hdr),p->payload,UIP_IP_HLEN+8);

	idur->chksum = 0;
	idur->chksum = uip_ipchksum(idur,q->len);

	uip_ipoutput(q,NULL,&iphdr->src,UIP_ICMP_TTL,0,UIP_PROTO_ICMP);
	uip_pbuf_free(q);
}
