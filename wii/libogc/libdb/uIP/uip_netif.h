#ifndef __UIP_NETIF_H__
#define __UIP_NETIF_H__

#include "uip.h"

#define UIP_NETIF_MAX_HWADDR_LEN 6U

/** TODO: define the use (where, when, whom) of netif flags */

/** whether the network interface is 'up'. this is
 * a software flag used to control whether this network
 * interface is enabled and processes traffic.
 */
#define UIP_NETIF_FLAG_UP 0x1U
/** if set, the netif has broadcast capability */
#define UIP_NETIF_FLAG_BROADCAST 0x2U
/** if set, the netif is one end of a point-to-point connection */
#define UIP_NETIF_FLAG_POINTTOPOINT 0x4U
/** if set, the interface is configured using DHCP */
#define UIP_NETIF_FLAG_DHCP 0x08U
/** if set, the interface has an active link
 *  (set by the network interface driver) */
#define UIP_NETIF_FLAG_LINK_UP 0x10U

struct uip_netif;
struct uip_pbuf;
struct uip_ip_addr;

struct uip_netif {
	struct uip_netif *next;

	struct uip_ip_addr ip_addr;
	struct uip_ip_addr netmask;
	struct uip_ip_addr gw;

	s8_t (*input)(struct uip_pbuf *p,struct uip_netif *inp);
	s8_t (*output)(struct uip_netif *netif,struct uip_pbuf *p,struct uip_ip_addr *ipaddr);
	s8_t (*linkoutput)(struct uip_netif *netif,struct uip_pbuf *p);

	void *state;

	u8_t hwaddr_len;
	u8_t hwaddr[UIP_NETIF_MAX_HWADDR_LEN];

	u16_t mtu;
	u8_t flags;

	s8_t name[2];
	u8_t num;
};

extern struct uip_netif *uip_netif_list;
extern struct uip_netif *uip_netif_default;

void uip_netif_init();
void uip_netif_setup(struct uip_netif *netif);
void uip_netif_setaddr(struct uip_netif *netif,struct uip_ip_addr *ipaddr,struct uip_ip_addr *netmask,struct uip_ip_addr *gw);
void uip_netif_setipaddr(struct uip_netif *netif,struct uip_ip_addr *ipaddr);
void uip_netif_setnetmask(struct uip_netif *netif,struct uip_ip_addr *netmask);
void uip_netif_setgw(struct uip_netif *netif,struct uip_ip_addr *gw);
void uip_netif_setdefault(struct uip_netif *netif);
u8_t uip_netif_isup(struct uip_netif *netif);
struct uip_netif* uip_netif_add(struct uip_netif *netif,struct uip_ip_addr *ipaddr,struct uip_ip_addr *netmask,struct uip_ip_addr *gw,void *state,s8_t (*init)(struct uip_netif *netif),s8_t (*input)(struct uip_pbuf *p,struct uip_netif *netif));

#endif
