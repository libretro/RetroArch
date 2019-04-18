/*
 * Copyright (c) 2001, Adam Dunkels.
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "uip.h"
#include "uip_arch.h"
#include "uip_ip.h"
#include "uip_tcp.h"
#include "uip_pbuf.h"

/*-----------------------------------------------------------------------------------*/
u16_t uip_chksum(u16_t *sdata, u32_t len)
{
  u32_t acc;

  for(acc = 0;len > 1;len -= 2) {
    acc += *sdata++;
  }

  /* add up any odd byte */
  if(len==1) {
    acc += htons((u16_t)((((u8_t *)sdata)[0]&0xff)<<8));
  }
  while(acc>>16) acc = (acc&0xffffUL)+(acc>>16);

  return (u16_t)acc;
}

/*-----------------------------------------------------------------------------------*/
u16_t uip_chksum_pseudo(struct uip_pbuf *p,struct uip_ip_addr *src,struct uip_ip_addr *dst,u8_t proto,u16_t proto_len)
{
	u32_t acc,len,rem;
	struct uip_pbuf *q;

	acc = 0;

	rem = proto_len;
	for(q=p;q!=NULL && rem>0;q=q->next) {
		len = (rem>q->len)?q->len:rem;
		acc += uip_chksum(q->payload,len);
		rem -= len;
	}

	acc += (src->addr&0xffffUL);
	acc += ((src->addr>>16)&0xffffUL);
	acc += (dst->addr&0xffffUL);
	acc += ((dst->addr>>16)&0xffffUL);
	acc += (u32_t)htons(proto);
	acc += (u32_t)htons(proto_len);

	while(acc>>16) acc = (acc&0xffffUL)+(acc>>16);

	return (u16_t)~(acc&0xffffUL);
}
/*-----------------------------------------------------------------------------------*/
u16_t
uip_ipchksum(void *dataptr,u16_t len)
{
  return ~(uip_chksum(dataptr,len));
}

u16_t uip_ipchksum_pbuf(struct uip_pbuf *p)
{
  u32_t acc;
  struct uip_pbuf *q;

  acc = 0;
  for(q = p; q != NULL; q = q->next) {
	acc += uip_chksum(q->payload,q->len);
  }
  while(acc>>16) acc = (acc&0xffffUL)+(acc>>16);

  return (u16_t)~(acc & 0xffffUL);
}

void uip_log(const char *filename,int line_nb,char *msg)
{
	printf("%s(%d):\n%s\n",filename,line_nb,msg);
}

/*-----------------------------------------------------------------------------------*/
