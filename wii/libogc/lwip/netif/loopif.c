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

#include <time.h>
#include "lwp_watchdog.h"
#include "lwip/opt.h"

#if LWIP_HAVE_LOOPIF

#include "netif/loopif.h"
#include "lwip/mem.h"

#if defined(LWIP_DEBUG) && defined(LWIP_TCPDUMP)
#include "netif/tcpdump.h"
#endif /* LWIP_DEBUG && LWIP_TCPDUMP */

#include "lwip/tcp.h"
#include "lwip/ip.h"

#include "lwp_watchdog.h"

#define LOOP_TIMER_ID			0x00070045

static u64 loopif_ticks;
static wd_cntrl loopif_tmr_cntrl;

extern unsigned int timespec_to_interval(const struct timespec *);

static void
loopif_input( void * arg )
{
	struct netif *netif = (struct netif*)(((void**)arg)[0]);
	struct pbuf *r = (struct pbuf*)(((void**)arg)[1]);

	mem_free(arg);
	netif->input(r,netif);
}

static err_t
loopif_output(struct netif *netif, struct pbuf *p,
       struct ip_addr *ipaddr)
{
  struct pbuf *q, *r;
  char *ptr;
  void **arg;

#if defined(LWIP_DEBUG) && defined(LWIP_TCPDUMP)
  tcpdump(p);
#endif /* LWIP_DEBUG && LWIP_TCPDUMP */

  r = pbuf_alloc(PBUF_RAW, p->tot_len, PBUF_RAM);
  if (r != NULL) {
    ptr = r->payload;

    for(q = p; q != NULL; q = q->next) {
      memcpy(ptr, q->payload, q->len);
      ptr += q->len;
    }

    arg = mem_malloc( sizeof( void *[2]));
	if( NULL == arg ) {
		return ERR_MEM;
	}

	arg[0] = netif;
	arg[1] = r;
	/**
	 * workaround (patch #1779) to try to prevent bug #2595:
	 * When connecting to "localhost" with the loopif interface,
	 * tcp_output doesn't get the opportunity to finnish sending the
	 * segment before tcp_process gets it, resulting in tcp_process
	 * referencing pcb->unacked-> which still is NULL.
	 *
	 * TODO: Is there still a race condition here? Leon
	 */
	__lwp_wd_initialize(&loopif_tmr_cntrl,loopif_input,LOOP_TIMER_ID,arg);
	__lwp_wd_insert_ticks(&loopif_tmr_cntrl,loopif_ticks);

    return ERR_OK;
  }
  return ERR_MEM;
}

err_t
loopif_init(struct netif *netif)
{
  struct timespec tb;

  netif->name[0] = 'l';
  netif->name[1] = 'o';
#if 0 /** TODO: I think this should be enabled, or not? Leon */
  netif->input = loopif_input;
#endif
  netif->output = loopif_output;

 tb.tv_sec = 0;
 tb.tv_nsec = 10*TB_NSPERMS;
 loopif_ticks = __lwp_wd_calc_ticks(&tb);

 return ERR_OK;
}

#endif /* LWIP_HAVE_LOOPIF */
