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

/* Some ICMP messages should be passed to the transport protocols. This
   is not implemented. */

#include <string.h>

#include "lwip/opt.h"
#include "lwip/icmp.h"
#include "lwip/inet.h"
#include "lwip/ip.h"
#include "lwip/def.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"

void
icmp_input(struct pbuf *p, struct netif *inp)
{
  u8_t type;
  u8_t code;
  struct icmp_echo_hdr *iecho;
  struct ip_hdr *iphdr;
  struct ip_addr tmpaddr;
  u16_t hlen;

  ICMP_STATS_INC(icmp.recv);
  snmp_inc_icmpinmsgs();

  iphdr = p->payload;
  hlen = IPH_HL(iphdr) * 4;
  if (pbuf_header(p, -((s16_t)hlen)) || (p->tot_len < sizeof(u16_t)*2)) {
    LWIP_DEBUGF(ICMP_DEBUG, ("icmp_input: short ICMP (%"U16_F" bytes) received\n", p->tot_len));
    pbuf_free(p);
    ICMP_STATS_INC(icmp.lenerr);
    snmp_inc_icmpinerrors();
    return;
  }

  type = *((u8_t *)p->payload);
  code = *(((u8_t *)p->payload)+1);
  (void)code;
  switch (type) {
  case ICMP_ECHO:
    /* broadcast or multicast destination address? */
    if (ip_addr_isbroadcast(&iphdr->dest, inp) || ip_addr_ismulticast(&iphdr->dest)) {
      LWIP_DEBUGF(ICMP_DEBUG, ("icmp_input: Not echoing to multicast or broadcast pings\n"));
      ICMP_STATS_INC(icmp.err);
      pbuf_free(p);
      return;
    }
    LWIP_DEBUGF(ICMP_DEBUG, ("icmp_input: ping\n"));
    if (p->tot_len < sizeof(struct icmp_echo_hdr)) {
      LWIP_DEBUGF(ICMP_DEBUG, ("icmp_input: bad ICMP echo received\n"));
      pbuf_free(p);
      ICMP_STATS_INC(icmp.lenerr);
      snmp_inc_icmpinerrors();

      return;
    }
    iecho = p->payload;
    if (inet_chksum_pbuf(p) != 0) {
      LWIP_DEBUGF(ICMP_DEBUG, ("icmp_input: checksum failed for received ICMP echo\n"));
      pbuf_free(p);
      ICMP_STATS_INC(icmp.chkerr);
      snmp_inc_icmpinerrors();
      return;
    }
    tmpaddr.addr = iphdr->src.addr;
    iphdr->src.addr = iphdr->dest.addr;
    iphdr->dest.addr = tmpaddr.addr;
    ICMPH_TYPE_SET(iecho, ICMP_ER);
    /* adjust the checksum */
    if (iecho->chksum >= htons(0xffff - (ICMP_ECHO << 8))) {
      iecho->chksum += htons(ICMP_ECHO << 8) + 1;
    } else {
      iecho->chksum += htons(ICMP_ECHO << 8);
    }
    ICMP_STATS_INC(icmp.xmit);
    /* increase number of messages attempted to send */
    snmp_inc_icmpoutmsgs();
    /* increase number of echo replies attempted to send */
    snmp_inc_icmpoutechoreps();

    pbuf_header(p, hlen);
    ip_output_if(p, &(iphdr->src), IP_HDRINCL,
		 IPH_TTL(iphdr), 0, IP_PROTO_ICMP, inp);
    break;
  default:
  LWIP_DEBUGF(ICMP_DEBUG, ("icmp_input: ICMP type %"S16_F" code %"S16_F" not supported.\n", (s16_t)type, (s16_t)code));
    ICMP_STATS_INC(icmp.proterr);
    ICMP_STATS_INC(icmp.drop);
  }
  pbuf_free(p);
}

void
icmp_dest_unreach(struct pbuf *p, enum icmp_dur_type t)
{
  struct pbuf *q;
  struct ip_hdr *iphdr;
  struct icmp_dur_hdr *idur;

  q = pbuf_alloc(PBUF_IP, 8 + IP_HLEN + 8, PBUF_RAM);
  /* ICMP header + IP header + 8 bytes of data */

  iphdr = p->payload;

  idur = q->payload;
  ICMPH_TYPE_SET(idur, ICMP_DUR);
  ICMPH_CODE_SET(idur, t);

  memcpy((u8_t *)q->payload + 8, p->payload, IP_HLEN + 8);

  /* calculate checksum */
  idur->chksum = 0;
  idur->chksum = inet_chksum(idur, q->len);
  ICMP_STATS_INC(icmp.xmit);
  /* increase number of messages attempted to send */
  snmp_inc_icmpoutmsgs();
  /* increase number of destination unreachable messages attempted to send */
  snmp_inc_icmpoutdestunreachs();

  ip_output(q, NULL, &(iphdr->src),
	    ICMP_TTL, 0, IP_PROTO_ICMP);
  pbuf_free(q);
}

#if IP_FORWARD
void
icmp_time_exceeded(struct pbuf *p, enum icmp_te_type t)
{
  struct pbuf *q;
  struct ip_hdr *iphdr;
  struct icmp_te_hdr *tehdr;

  q = pbuf_alloc(PBUF_IP, 8 + IP_HLEN + 8, PBUF_RAM);

  iphdr = p->payload;
  LWIP_DEBUGF(ICMP_DEBUG, ("icmp_time_exceeded from "));
  ip_addr_debug_print(ICMP_DEBUG, &(iphdr->src));
  LWIP_DEBUGF(ICMP_DEBUG, (" to "));
  ip_addr_debug_print(ICMP_DEBUG, &(iphdr->dest));
  LWIP_DEBUGF(ICMP_DEBUG, ("\n"));

  tehdr = q->payload;
  ICMPH_TYPE_SET(tehdr, ICMP_TE);
  ICMPH_CODE_SET(tehdr, t);

  /* copy fields from original packet */
  memcpy((u8_t *)q->payload + 8, (u8_t *)p->payload, IP_HLEN + 8);

  /* calculate checksum */
  tehdr->chksum = 0;
  tehdr->chksum = inet_chksum(tehdr, q->len);
  ICMP_STATS_INC(icmp.xmit);
  /* increase number of messages attempted to send */
  snmp_inc_icmpoutmsgs();
  /* increase number of destination unreachable messages attempted to send */
  snmp_inc_icmpouttimeexcds();
  ip_output(q, NULL, &(iphdr->src),
	    ICMP_TTL, 0, IP_PROTO_ICMP);
  pbuf_free(q);
}

#endif /* IP_FORWARD */
