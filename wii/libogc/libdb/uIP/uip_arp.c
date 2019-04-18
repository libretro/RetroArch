/**
 * \addtogroup uip
 * @{
 */

/**
 * \defgroup uiparp uIP Address Resolution Protocol
 * @{
 *
 * The Address Resolution Protocol ARP is used for mapping between IP
 * addresses and link level addresses such as the Ethernet MAC
 * addresses. ARP uses broadcast queries to ask for the link level
 * address of a known IP address and the host which is configured with
 * the IP address for which the query was meant, will respond with its
 * link level address.
 *
 * \note This ARP implementation only supports Ethernet.
 */

/**
 * \file
 * Implementation of the ARP Address Resolution Protocol.
 * \author Adam Dunkels <adam@dunkels.com>
 *
 */

/*
 * Copyright (c) 2001-2003, Adam Dunkels.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file is part of the uIP TCP/IP stack.
 *
 *
 */

#include "uip_pbuf.h"
#include "uip_netif.h"
#include "uip_arp.h"

#include <string.h>

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

#define ARP_TRY_HARD 0x01

#define ARP_MAXAGE 240
#define ARP_MAXPENDING 2

#define ARP_REQUEST 1
#define ARP_REPLY   2

#define ARP_HWTYPE_ETH 1

#define ARPH_HWLEN(hdr) (ntohs((hdr)->_hwlen_protolen) >> 8)
#define ARPH_PROTOLEN(hdr) (ntohs((hdr)->_hwlen_protolen) & 0xff)

#define ARPH_HWLEN_SET(hdr, len) (hdr)->_hwlen_protolen = htons(ARPH_PROTOLEN(hdr) | ((len) << 8))
#define ARPH_PROTOLEN_SET(hdr, len) (hdr)->_hwlen_protolen = htons((len) | (ARPH_HWLEN(hdr) << 8))

enum arp_state {
	ARP_STATE_EMPTY,
	ARP_STATE_PENDING,
	ARP_STATE_STABLE,
	ARP_STATE_EXPIRED
};

struct arp_entry {
  struct uip_ip_addr ipaddr;
  struct uip_eth_addr ethaddr;
  enum arp_state state;
  u8_t time;
};

static const struct uip_eth_addr ethbroadcast = {{0xff,0xff,0xff,0xff,0xff,0xff}};
static struct arp_entry arp_table[UIP_ARPTAB_SIZE];
/*-----------------------------------------------------------------------------------*/
/**
 * Initialize the ARP module.
 *
 */
/*-----------------------------------------------------------------------------------*/
void
uip_arp_init(void)
{
	s32_t i;
	for(i = 0; i < UIP_ARPTAB_SIZE; ++i) {
		arp_table[i].state = ARP_STATE_EMPTY;
		arp_table[i].time = 0;
	}
}
/*-----------------------------------------------------------------------------------*/
/**
 * Periodic ARP processing function.
 *
 * This function performs periodic timer processing in the ARP module
 * and should be called at regular intervals. The recommended interval
 * is 10 seconds between the calls.
 *
 */
/*-----------------------------------------------------------------------------------*/
void
uip_arp_timer(void)
{
	u8_t i;

	for(i=0;i<UIP_ARPTAB_SIZE;i++) {
		arp_table[i].time++;
		if(arp_table[i].state==ARP_STATE_STABLE && arp_table[i].time>=ARP_MAXAGE) {
			arp_table[i].state = ARP_STATE_EXPIRED;
		} else if(arp_table[i].state==ARP_STATE_PENDING) {
			if(arp_table[i].time>=ARP_MAXPENDING) arp_table[i].state = ARP_STATE_EXPIRED;
		}

		if(arp_table[i].state==ARP_STATE_EXPIRED) arp_table[i].state = ARP_STATE_EMPTY;
	}
}

static s8_t uip_arp_findentry(struct uip_ip_addr *ipaddr,u8_t flags)
{
	s8_t old_pending = UIP_ARPTAB_SIZE, old_stable = UIP_ARPTAB_SIZE;
	s8_t empty = UIP_ARPTAB_SIZE;
	u8_t i = 0,age_pending = 0,age_stable = 0;

	/* Walk through the ARP mapping table and try to find an entry to
	   update. If none is found, the IP -> MAC address mapping is
	   inserted in the ARP table. */
	for(i = 0; i < UIP_ARPTAB_SIZE; ++i) {
		if(empty==UIP_ARPTAB_SIZE && arp_table[i].state==ARP_STATE_EMPTY) {
			empty = i;
		} else if(arp_table[i].state==ARP_STATE_PENDING) {
			if(ipaddr && ip_addr_cmp(ipaddr,&arp_table[i].ipaddr)) return i;
			else if(arp_table[i].time>=age_pending) {
				old_pending = i;
				age_pending = arp_table[i].time;
			}
		} else if(arp_table[i].state==ARP_STATE_STABLE) {
			if(ipaddr && ip_addr_cmp(ipaddr,&arp_table[i].ipaddr)) return i;
			else if(arp_table[i].time>=age_stable) {
				old_stable = i;
				age_stable = arp_table[i].time;
			}
		}
	}
	if(empty==UIP_ARPTAB_SIZE && !(flags&ARP_TRY_HARD)) return UIP_ERR_MEM;

	if(empty<UIP_ARPTAB_SIZE) i = empty;
	else if(old_stable<UIP_ARPTAB_SIZE) i = old_stable;
	else if(old_pending<UIP_ARPTAB_SIZE) i = old_pending;
	else return UIP_ERR_MEM;

	arp_table[i].time = 0;
	arp_table[i].state = ARP_STATE_EMPTY;
	if(ipaddr!=NULL) ip_addr_set(&arp_table[i].ipaddr,ipaddr);

	return (s8_t)i;
}

/*-----------------------------------------------------------------------------------*/
static s8_t uip_arp_update(struct uip_netif *netif,struct uip_ip_addr *ipaddr, struct uip_eth_addr *ethaddr,u8_t flags)
{
	s8_t i,k;

	if(ip_addr_isany(ipaddr) ||
		ip_addr_isbroadcast(ipaddr,netif) ||
		ip_addr_ismulticast(ipaddr)) return UIP_ERR_ARG;

	i = uip_arp_findentry(ipaddr,flags);
	if(i<0) return i;

	arp_table[i].time = 0;
	arp_table[i].state = ARP_STATE_STABLE;
	for(k=0;k<netif->hwaddr_len;k++) arp_table[i].ethaddr.addr[k] = ethaddr->addr[k];

	return UIP_ERR_OK;
}
/*-----------------------------------------------------------------------------------*/
/**
 * ARP processing for incoming IP packets
 *
 * This function should be called by the device driver when an IP
 * packet has been received. The function will check if the address is
 * in the ARP cache, and if so the ARP cache entry will be
 * refreshed. If no ARP cache entry was found, a new one is created.
 *
 * This function expects an IP packet with a prepended Ethernet header
 * in the uip_buf[] buffer, and the length of the packet in the global
 * variable uip_len.
 */
/*-----------------------------------------------------------------------------------*/
void
uip_arp_ipin(struct uip_netif *netif,struct uip_pbuf *p)
{
	struct uip_ethip_hdr *hdr;

	hdr = p->payload;
	if(!ip_addr_netcmp(&hdr->ip.src,&netif->ip_addr,&netif->netmask)) return;

	uip_arp_update(netif,&hdr->ip.src,&hdr->ethhdr.src,0);
 }

/*-----------------------------------------------------------------------------------*/
/**
 * ARP processing for incoming ARP packets.
 *
 * This function should be called by the device driver when an ARP
 * packet has been received. The function will act differently
 * depending on the ARP packet type: if it is a reply for a request
 * that we previously sent out, the ARP cache will be filled in with
 * the values from the ARP reply. If the incoming ARP packet is an ARP
 * request for our IP address, an ARP reply packet is created and put
 * into the uip_buf[] buffer.
 *
 * When the function returns, the value of the global variable uip_len
 * indicates whether the device driver should send out a packet or
 * not. If uip_len is zero, no packet should be sent. If uip_len is
 * non-zero, it contains the length of the outbound packet that is
 * present in the uip_buf[] buffer.
 *
 * This function expects an ARP packet with a prepended Ethernet
 * header in the uip_buf[] buffer, and the length of the packet in the
 * global variable uip_len.
 */
/*-----------------------------------------------------------------------------------*/
void
uip_arp_arpin(struct uip_netif *netif,struct uip_eth_addr *ethaddr,struct uip_pbuf *p)
{
	u8_t i,for_us;
	struct uip_ip_addr sipaddr,dipaddr;
	struct uip_arp_hdr *hdr;

	if(p->tot_len<sizeof(struct uip_arp_hdr)) {
		uip_pbuf_free(p);
		return;
	}

	hdr = p->payload;

	*(struct uip_ip_addr2*)((void*)&sipaddr) = hdr->sipaddr;
	*(struct uip_ip_addr2*)((void*)&dipaddr) = hdr->dipaddr;

	if(netif->ip_addr.addr==0) for_us = 0;
	else for_us = ip_addr_cmp(&dipaddr,&netif->ip_addr);

	if(for_us) uip_arp_update(netif,&sipaddr,&hdr->shwaddr,ARP_TRY_HARD);
	else uip_arp_update(netif,&sipaddr,&hdr->shwaddr,0);

	switch(htons(hdr->opcode)) {
		case ARP_REQUEST:
			if(for_us) {
				hdr->opcode = htons(ARP_REPLY);
				hdr->dipaddr = hdr->sipaddr;
				hdr->sipaddr = *(struct uip_ip_addr2*)((void*)&netif->ip_addr);

				for(i=0;i<netif->hwaddr_len;i++) {
					hdr->dhwaddr.addr[i] = hdr->shwaddr.addr[i];
					hdr->shwaddr.addr[i] = ethaddr->addr[i];
					hdr->ethhdr.dest.addr[i] = hdr->dhwaddr.addr[i];
					hdr->ethhdr.src.addr[i] = ethaddr->addr[i];
				}

				hdr->hwtype = htons(ARP_HWTYPE_ETH);
				ARPH_HWLEN_SET(hdr,netif->hwaddr_len);

				hdr->protocol = htons(UIP_ETHTYPE_IP);
				ARPH_PROTOLEN_SET(hdr,sizeof(struct uip_ip_addr));

				netif->linkoutput(netif,p);
			} else {
				UIP_LOG("uip_arp_arpin: ip packet not for us.\n");
			}
			break;
		case ARP_REPLY:
			break;
		default:
			UIP_LOG("uip_arp_arpin: ARP unknown opcode type.\n");
			break;
	}
	uip_pbuf_free(p);
}
/*-----------------------------------------------------------------------------------*/
/**
 * Prepend Ethernet header to an outbound IP packet and see if we need
 * to send out an ARP request.
 *
 * This function should be called before sending out an IP packet. The
 * function checks the destination IP address of the IP packet to see
 * what Ethernet MAC address that should be used as a destination MAC
 * address on the Ethernet.
 *
 * If the destination IP address is in the local network (determined
 * by logical ANDing of netmask and our IP address), the function
 * checks the ARP cache to see if an entry for the destination IP
 * address is found. If so, an Ethernet header is prepended and the
 * function returns. If no ARP cache entry is found for the
 * destination IP address, the packet in the uip_buf[] is replaced by
 * an ARP request packet for the IP address. The IP packet is dropped
 * and it is assumed that they higher level protocols (e.g., TCP)
 * eventually will retransmit the dropped packet.
 *
 * If the destination IP address is not on the local network, the IP
 * address of the default router is used instead.
 *
 * When the function returns, a packet is present in the uip_buf[]
 * buffer, and the length of the packet is in the global variable
 * uip_len.
 */
/*-----------------------------------------------------------------------------------*/
s8_t uip_arp_out(struct uip_netif *netif,struct uip_ip_addr *ipaddr,struct uip_pbuf *q)
{
	u8_t i;
	struct uip_eth_addr *dest,*srcaddr,mcastaddr;
	struct uip_eth_hdr *ethhdr;

	if(uip_pbuf_header(q,sizeof(struct uip_eth_hdr))!=0) {
		UIP_LOG("uip_arp_out: could not allocate room for header.\n");
		return UIP_ERR_BUF;
	}

	dest = NULL;
	if(ip_addr_isbroadcast(ipaddr,netif)) {
		dest = (struct uip_eth_addr*)&ethbroadcast;
	} else if(ip_addr_ismulticast(ipaddr)) {
		/* Hash IP multicast address to MAC address.*/
		mcastaddr.addr[0] = 0x01;
		mcastaddr.addr[1] = 0x00;
		mcastaddr.addr[2] = 0x5e;
		mcastaddr.addr[3] = ip4_addr2(ipaddr) & 0x7f;
		mcastaddr.addr[4] = ip4_addr3(ipaddr);
		mcastaddr.addr[5] = ip4_addr4(ipaddr);
		/* destination Ethernet address is multicast */
	   dest = &mcastaddr;
	} else {
		if(!ip_addr_netcmp(ipaddr,&netif->ip_addr,&netif->netmask)) {
			if(netif->gw.addr!=0) ipaddr = &netif->gw;
			else return UIP_ERR_RTE;
		}
		return uip_arp_arpquery(netif,ipaddr,q);
	}

	srcaddr = (struct uip_eth_addr*)netif->hwaddr;
	ethhdr = q->payload;
	for(i=0;i<netif->hwaddr_len;i++) {
		ethhdr->dest.addr[i] = dest->addr[i];
		ethhdr->src.addr[i] = srcaddr->addr[i];
	}

	ethhdr->type = htons(UIP_ETHTYPE_IP);
	return netif->linkoutput(netif,q);
}
/*-----------------------------------------------------------------------------------*/

s8_t uip_arp_arpquery(struct uip_netif *netif,struct uip_ip_addr *ipaddr,struct uip_pbuf *q)
{
	s8_t i,k;
	s8_t err = UIP_ERR_MEM;
	struct uip_eth_addr *srcaddr = (struct uip_eth_addr*)netif->hwaddr;

	if(ip_addr_isbroadcast(ipaddr,netif) ||
		ip_addr_ismulticast(ipaddr) ||
		ip_addr_isany(ipaddr)) return UIP_ERR_ARG;

	i = uip_arp_findentry(ipaddr,ARP_TRY_HARD);
	if(i<0) return i;

	if(arp_table[i].state==ARP_STATE_EMPTY) arp_table[i].state = ARP_STATE_PENDING;
	if(arp_table[i].state==ARP_STATE_PENDING || q==NULL) err = uip_arp_arprequest(netif,ipaddr);

	if(q!=NULL) {
		if(arp_table[i].state==ARP_STATE_STABLE) {

			struct uip_eth_hdr *hdr = q->payload;
			for(k=0;k<netif->hwaddr_len;k++) {
				hdr->dest.addr[k] = arp_table[i].ethaddr.addr[k];
				hdr->src.addr[k] = srcaddr->addr[k];
			}

			hdr->type = htons(UIP_ETHTYPE_IP);
			err = netif->linkoutput(netif,q);
		} else if(arp_table[i].state==ARP_STATE_PENDING) {
			UIP_LOG("uip_arp_query: Ethernet destination address unknown, queueing disabled, packet dropped.\n");
		}
	}
	return err;
}

s8_t uip_arp_arprequest(struct uip_netif *netif,struct uip_ip_addr *ipaddr)
{
	s8_t k;
	s8_t err = UIP_ERR_MEM;
	struct uip_arp_hdr *hdr;
	struct uip_pbuf *p;
	struct uip_eth_addr *srcaddr = (struct uip_eth_addr*)netif->hwaddr;

	p = uip_pbuf_alloc(UIP_PBUF_LINK,sizeof(struct uip_arp_hdr),UIP_PBUF_RAM);
	if(p==NULL) return err;

	hdr = p->payload;
	hdr->opcode = htons(ARP_REQUEST);

	for(k=0;k<netif->hwaddr_len;k++) {
		hdr->shwaddr.addr[k] = srcaddr->addr[k];
		hdr->dhwaddr.addr[k] = 0;
	}

	hdr->dipaddr = *(struct uip_ip_addr2*)((void*)ipaddr);
	hdr->sipaddr = *(struct uip_ip_addr2*)((void*)&netif->ip_addr);

	hdr->hwtype = htons(ARP_HWTYPE_ETH);
	ARPH_HWLEN_SET(hdr,netif->hwaddr_len);

	hdr->protocol = htons(UIP_ETHTYPE_IP);
	ARPH_PROTOLEN_SET(hdr,sizeof(struct uip_ip_addr));
	for(k=0;k<netif->hwaddr_len;k++) {
		hdr->ethhdr.dest.addr[k] = 0xff;
		hdr->ethhdr.src.addr[k] = srcaddr->addr[k];
	}
	hdr->ethhdr.type = htons(UIP_ETHTYPE_ARP);

	err = netif->linkoutput(netif,p);
	uip_pbuf_free(p);

	return err;
}

/** @} */
/** @} */
