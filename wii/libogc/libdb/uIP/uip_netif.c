#include <stdlib.h>
#include <string.h>

#include "uip_ip.h"
#include "uip_tcp.h"
#include "uip_pbuf.h"
#include "uip_netif.h"

#if UIP_LOGGING == 1
#include <stdio.h>
#define UIP_LOG(m) uip_log(__FILE__,__LINE__,m)
#else
#define UIP_LOG(m)
#endif /* UIP_LOGGING == 1 */

#if UIP_ERRORING == 1
#include <stdio.h>
#define UIP_ERROR(m) uip_log(__FILE__,__LINE__,m)
#else
#define UIP_ERROR(m)
#endif /* UIP_ERRORING == 1 */

#if UIP_STATISTICS == 1
struct uip_stats uip_stat;
#define UIP_STAT(s) s
#else
#define UIP_STAT(s)
#endif /* UIP_STATISTICS == 1 */

struct uip_netif *uip_netif_list;
struct uip_netif *uip_netif_default;

void uip_netif_init()
{
	uip_netif_list = uip_netif_default = NULL;
}

void uip_netif_setup(struct uip_netif *netif)
{
	netif->flags |= UIP_NETIF_FLAG_UP;
}

u8_t uip_netif_isup(struct uip_netif *netif)
{
	return (netif->flags&UIP_NETIF_FLAG_UP);
}

struct uip_netif* uip_netif_add(struct uip_netif *netif,struct uip_ip_addr *ipaddr,struct uip_ip_addr *netmask,struct uip_ip_addr *gw,void *state,s8_t (*init)(struct uip_netif *netif),s8_t (*input)(struct uip_pbuf *p,struct uip_netif *netif))
{
	static int netif_num = 0;

	netif->state = state;
	netif->num = netif_num++;
	netif->input = input;

	uip_netif_setaddr(netif,ipaddr,netmask,gw);

	if(init(netif)!=0) return NULL;

	UIP_LOG("uip_netif_add: netif is up.\n");

	netif->next = uip_netif_list;
	uip_netif_list = netif;

	return netif;
}

void uip_netif_setaddr(struct uip_netif *netif,struct uip_ip_addr *ipaddr,struct uip_ip_addr *netmask,struct uip_ip_addr *gw)
{
	uip_netif_setipaddr(netif,ipaddr);
	uip_netif_setnetmask(netif,netmask);
	uip_netif_setgw(netif,gw);
}

void uip_netif_setipaddr(struct uip_netif *netif,struct uip_ip_addr *ipaddr)
{
#if UIP_TCP
	struct uip_tcp_pcb *pcb;
	struct uip_tcp_pcb_listen *lpcb;

	if((ip_addr_cmp(ipaddr,&netif->ip_addr))==0) {
		pcb = uip_tcp_active_pcbs;
		while(pcb!=NULL) {
			if(ip_addr_cmp(&pcb->local_ip,&netif->ip_addr)) {
				struct uip_tcp_pcb *next = pcb->next;
				pcb = next;
			} else {
				pcb = pcb->next;
			}
		}
		for(lpcb=uip_tcp_listen_pcbs.listen_pcbs;lpcb!=NULL;lpcb=lpcb->next) {
			if(ip_addr_cmp(&lpcb->local_ip,&netif->ip_addr)) {
				ip_addr_set(&lpcb->local_ip,ipaddr);
			}
		}
	}
#endif
	ip_addr_set(&netif->ip_addr,ipaddr);
}

void uip_netif_setnetmask(struct uip_netif *netif,struct uip_ip_addr *netmask)
{
	ip_addr_set(&netif->netmask,netmask);
}

void uip_netif_setgw(struct uip_netif *netif,struct uip_ip_addr *gw)
{
	ip_addr_set(&netif->gw,gw);
}

void uip_netif_setdefault(struct uip_netif *netif)
{
	uip_netif_default = netif;
}

struct uip_netif* uip_netif_find(const char *name)
{
	u8_t num;
	struct uip_netif *netif;

	if(name==NULL) return NULL;

	num = name[2] - '0';

	for(netif=uip_netif_list;netif!=NULL;netif=netif->next) {
		if(netif->num==num &&
			netif->name[0]==name[0] &&
			netif->name[1]==name[1]) return netif;
	}

	return NULL;
}
