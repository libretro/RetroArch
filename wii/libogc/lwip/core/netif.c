/**
 * @file
 *
 * lwIP network interface abstraction
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

#include "lwip/opt.h"

#include "lwip/def.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/tcp.h"

struct netif *netif_list = NULL;
struct netif *netif_default = NULL;

/**
 * Add a network interface to the list of lwIP netifs.
 *
 * @param netif a pre-allocated netif structure
 * @param ipaddr IP address for the new netif
 * @param netmask network mask for the new netif
 * @param gw default gateway IP address for the new netif
 * @param state opaque data passed to the new netif
 * @param init callback function that initializes the interface
 * @param input callback function that is called to pass
 * ingress packets up in the protocol layer stack.
 *
 * @return netif, or NULL if failed.
 */
struct netif *
netif_add(struct netif *netif, struct ip_addr *ipaddr, struct ip_addr *netmask,
  struct ip_addr *gw,
  void *state,
  err_t (* init)(struct netif *netif),
  err_t (* input)(struct pbuf *p, struct netif *netif))
{
  static s16_t netifnum = 0;

#if LWIP_DHCP
  /* netif not under DHCP control by default */
  netif->dhcp = NULL;
#endif
  /* remember netif specific state information data */
  netif->state = state;
  netif->num = netifnum++;
  netif->input = input;

  netif_set_addr(netif, ipaddr, netmask, gw);

  /* call user specified initialization function for netif */
  if (init(netif) != ERR_OK) {
    return NULL;
  }

  /* add this netif to the list */
  netif->next = netif_list;
  netif_list = netif;
  LWIP_DEBUGF(NETIF_DEBUG, ("netif: added interface %c%c IP addr ",
    netif->name[0], netif->name[1]));
  ip_addr_debug_print(NETIF_DEBUG, ipaddr);
  LWIP_DEBUGF(NETIF_DEBUG, (" netmask "));
  ip_addr_debug_print(NETIF_DEBUG, netmask);
  LWIP_DEBUGF(NETIF_DEBUG, (" gw "));
  ip_addr_debug_print(NETIF_DEBUG, gw);
  LWIP_DEBUGF(NETIF_DEBUG, ("\n"));
  return netif;
}

void
netif_set_addr(struct netif *netif,struct ip_addr *ipaddr, struct ip_addr *netmask,
    struct ip_addr *gw)
{
  netif_set_ipaddr(netif, ipaddr);
  netif_set_netmask(netif, netmask);
  netif_set_gw(netif, gw);
}

void netif_remove(struct netif * netif)
{
  if ( netif == NULL ) return;

  /*  is it the first netif? */
  if (netif_list == netif) {
    netif_list = netif->next;
  }
  else {
    /*  look for netif further down the list */
    struct netif * tmpNetif;
    for (tmpNetif = netif_list; tmpNetif != NULL; tmpNetif = tmpNetif->next) {
      if (tmpNetif->next == netif) {
        tmpNetif->next = netif->next;
        break;
        }
    }
    if (tmpNetif == NULL)
      return; /*  we didn't find any netif today */
  }
  /* this netif is default? */
  if (netif_default == netif)
    /* reset default netif */
    netif_default = NULL;
  LWIP_DEBUGF( NETIF_DEBUG, ("netif_remove: removed netif\n") );
}

struct netif *
netif_find(char *name)
{
  struct netif *netif;
  u8_t num;

  if (name == NULL) {
    return NULL;
  }

  num = name[2] - '0';

  for(netif = netif_list; netif != NULL; netif = netif->next) {
    if (num == netif->num &&
       name[0] == netif->name[0] &&
       name[1] == netif->name[1]) {
      LWIP_DEBUGF(NETIF_DEBUG, ("netif_find: found %c%c\n", name[0], name[1]));
      return netif;
    }
  }
  LWIP_DEBUGF(NETIF_DEBUG, ("netif_find: didn't find %c%c\n", name[0], name[1]));
  return NULL;
}

void
netif_set_ipaddr(struct netif *netif, struct ip_addr *ipaddr)
{
  /* TODO: Handling of obsolete pcbs */
  /* See:  http://mail.gnu.org/archive/html/lwip-users/2003-03/msg00118.html */
#if LWIP_TCP
  struct tcp_pcb *pcb;
  struct tcp_pcb_listen *lpcb;

  /* address is actually being changed? */
  if ((ip_addr_cmp(ipaddr, &(netif->ip_addr))) == 0)
  {
    /* extern struct tcp_pcb *tcp_active_pcbs; defined by tcp.h */
    LWIP_DEBUGF(NETIF_DEBUG | 1, ("netif_set_ipaddr: netif address being changed\n"));
    pcb = tcp_active_pcbs;
    while (pcb != NULL) {
      /* PCB bound to current local interface address? */
      if (ip_addr_cmp(&(pcb->local_ip), &(netif->ip_addr))) {
        /* this connection must be aborted */
        struct tcp_pcb *next = pcb->next;
        LWIP_DEBUGF(NETIF_DEBUG | 1, ("netif_set_ipaddr: aborting TCP pcb %p\n", (void *)pcb));
        tcp_abort(pcb);
        pcb = next;
      } else {
        pcb = pcb->next;
      }
    }
    for (lpcb = tcp_listen_pcbs.listen_pcbs; lpcb != NULL; lpcb = lpcb->next) {
      /* PCB bound to current local interface address? */
      if (ip_addr_cmp(&(lpcb->local_ip), &(netif->ip_addr))) {
        /* The PCB is listening to the old ipaddr and
         * is set to listen to the new one instead */
        ip_addr_set(&(lpcb->local_ip), ipaddr);
      }
    }
  }
#endif
  ip_addr_set(&(netif->ip_addr), ipaddr);
#if 0 /* only allowed for Ethernet interfaces TODO: how can we check? */
  /** For Ethernet network interfaces, we would like to send a
   *  "gratuitous ARP"; this is an ARP packet sent by a node in order
   *  to spontaneously cause other nodes to update an entry in their
   *  ARP cache. From RFC 3220 "IP Mobility Support for IPv4" section 4.6.
   */
  etharp_query(netif, ipaddr, NULL);
#endif
  LWIP_DEBUGF(NETIF_DEBUG | DBG_TRACE | DBG_STATE | 3, ("netif: IP address of interface %c%c set to %"U16_F".%"U16_F".%"U16_F".%"U16_F"\n",
    netif->name[0], netif->name[1],
    ip4_addr1(&netif->ip_addr),
    ip4_addr2(&netif->ip_addr),
    ip4_addr3(&netif->ip_addr),
    ip4_addr4(&netif->ip_addr)));
}

void
netif_set_gw(struct netif *netif, struct ip_addr *gw)
{
  ip_addr_set(&(netif->gw), gw);
  LWIP_DEBUGF(NETIF_DEBUG | DBG_TRACE | DBG_STATE | 3, ("netif: GW address of interface %c%c set to %"U16_F".%"U16_F".%"U16_F".%"U16_F"\n",
    netif->name[0], netif->name[1],
    ip4_addr1(&netif->gw),
    ip4_addr2(&netif->gw),
    ip4_addr3(&netif->gw),
    ip4_addr4(&netif->gw)));
}

void
netif_set_netmask(struct netif *netif, struct ip_addr *netmask)
{
  ip_addr_set(&(netif->netmask), netmask);
  LWIP_DEBUGF(NETIF_DEBUG | DBG_TRACE | DBG_STATE | 3, ("netif: netmask of interface %c%c set to %"U16_F".%"U16_F".%"U16_F".%"U16_F"\n",
    netif->name[0], netif->name[1],
    ip4_addr1(&netif->netmask),
    ip4_addr2(&netif->netmask),
    ip4_addr3(&netif->netmask),
    ip4_addr4(&netif->netmask)));
}

void
netif_set_default(struct netif *netif)
{
  netif_default = netif;
  LWIP_DEBUGF(NETIF_DEBUG, ("netif: setting default interface %c%c\n",
           netif ? netif->name[0] : '\'', netif ? netif->name[1] : '\''));
}

/**
 * Bring an interface up, available for processing
 * traffic.
 *
 * @note: Enabling DHCP on a down interface will make it come
 * up once configured.
 *
 * @see dhcp_start()
 */
void netif_set_up(struct netif *netif)
{
  netif->flags |= NETIF_FLAG_UP;
}

/**
 * Ask if an interface is up
 */
u8_t netif_is_up(struct netif *netif)
{
  return (netif->flags & NETIF_FLAG_UP)?1:0;
}

/**
 * Bring an interface down, disabling any traffic processing.
 *
 * @note: Enabling DHCP on a down interface will make it come
 * up once configured.
 *
 * @see dhcp_start()
 */
void netif_set_down(struct netif *netif)
{
  netif->flags &= ~NETIF_FLAG_UP;
}

void
netif_init(void)
{
  netif_list = netif_default = NULL;
}
