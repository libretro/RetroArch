/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
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
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 *
 * This file is built upon the file: src/arch/rtxc/netif/sioslip.c
 *
 * Author: Magnus Ivarsson <magnus.ivarsson(at)volvo.com> 
 */

/* 
 * This is an arch independent SLIP netif. The specific serial hooks must be provided 
 * by another file.They are sio_open, sio_recv and sio_send
 */ 

#include "netif/slipif.h"
#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include "lwip/stats.h"
#include "lwip/sio.h"

#define SLIP_END     0300
#define SLIP_ESC     0333
#define SLIP_ESC_END 0334
#define SLIP_ESC_ESC 0335

#define MAX_SIZE     1500

/**
 * Send a pbuf doing the necessary SLIP encapsulation
 *
 * Uses the serial layer's sio_send() 
 */ 
err_t
slipif_output(struct netif *netif, struct pbuf *p, struct ip_addr *ipaddr)
{
  struct pbuf *q;
  int i;
  u8_t c;

  /* Send pbuf out on the serial I/O device. */
  sio_send(SLIP_END, netif->state);

  for(q = p; q != NULL; q = q->next) {
    for(i = 0; i < q->len; i++) {
      c = ((u8_t *)q->payload)[i];
      switch (c) {
      case SLIP_END:
  sio_send(SLIP_ESC, netif->state);
  sio_send(SLIP_ESC_END, netif->state);
  break;
      case SLIP_ESC:
  sio_send(SLIP_ESC, netif->state);
  sio_send(SLIP_ESC_ESC, netif->state);
  break;
      default:
  sio_send(c, netif->state);
  break;
      }
    }
  }
  sio_send(SLIP_END, netif->state);
  return 0;
}

/**
 * Handle the incoming SLIP stream character by character
 *
 * Poll the serial layer by calling sio_recv()
 * 
 * @return The IP packet when SLIP_END is received 
 */ 
static struct pbuf *
slipif_input( struct netif * netif )
{
  u8_t c;
  struct pbuf *p, *q;
  int recved;
  int i;

  q = p = NULL;
  recved = i = 0;
  c = 0;

  while (1) {
    c = sio_recv(netif->state);
    switch (c) {
    case SLIP_END:
      if (recved > 0) {
  /* Received whole packet. */
  pbuf_realloc(q, recved);
  
  LINK_STATS_INC(link.recv);
  
  LWIP_DEBUGF(SLIP_DEBUG, ("slipif: Got packet\n"));
  return q;
      }
      break;

    case SLIP_ESC:
      c = sio_recv(netif->state);
      switch (c) {
      case SLIP_ESC_END:
  c = SLIP_END;
  break;
      case SLIP_ESC_ESC:
  c = SLIP_ESC;
  break;
      }
      /* FALLTHROUGH */
      
    default:
      if (p == NULL) {
  LWIP_DEBUGF(SLIP_DEBUG, ("slipif_input: alloc\n"));
  p = pbuf_alloc(PBUF_LINK, PBUF_POOL_BUFSIZE, PBUF_POOL);

  if (p == NULL) {
    LINK_STATS_INC(link.drop);
    LWIP_DEBUGF(SLIP_DEBUG, ("slipif_input: no new pbuf! (DROP)\n"));
  }
  
  if (q != NULL) {
    pbuf_cat(q, p);
  } else {
    q = p;
  }
      }
      if (p != NULL && recved < MAX_SIZE) {
  ((u8_t *)p->payload)[i] = c;
  recved++;
  i++;
  if (i >= p->len) {
    i = 0;
    p = NULL;
  }
      }
      break;
    }
    
  }
  return NULL;
}

/**
 * The SLIP input thread 
 *
 * Feed the IP layer with incoming packets
 */ 
static void
slipif_loop(void *nf)
{
  struct pbuf *p;
  struct netif *netif = (struct netif *)nf;

  while (1) {
    p = slipif_input(netif);
    netif->input(p, netif);
  }
}

/**
 * SLIP netif initialization
 *
 * Call the arch specific sio_open and remember
 * the opened device in the state field of the netif.
 */ 
err_t
slipif_init(struct netif *netif)
{
  
  LWIP_DEBUGF(SLIP_DEBUG, ("slipif_init: netif->num=%x\n", (int)netif->num));

  netif->name[0] = 's';
  netif->name[1] = 'l';
  netif->output = slipif_output;
  netif->mtu = 1500;  
  netif->flags = NETIF_FLAG_POINTTOPOINT;

  netif->state = sio_open(netif->num);
  if (!netif->state)
      return ERR_IF;

  sys_thread_new(slipif_loop, netif, SLIPIF_THREAD_PRIO);
  return ERR_OK;
}
