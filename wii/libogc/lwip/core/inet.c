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

/* inet.c
 *
 * Functions common to all TCP/IP modules, such as the Internet checksum and the
 * byte order functions.
 *
 */

#include "lwip/opt.h"

#include "lwip/arch.h"

#include "lwip/def.h"
#include "lwip/inet.h"

#include "lwip/sys.h"

/* This is a reference implementation of the checksum algorithm, with the
 * aim of being simple, correct and fully portable. Checksumming is the
 * first thing you would want to optimize for your platform. You will
 * need to port it to your architecture and in your sys_arch.h:
 *
 * #define LWIP_CHKSUM <your_checksum_routine>
*/
#ifndef LWIP_CHKSUM
#define LWIP_CHKSUM lwip_standard_chksum

/**
 * lwip checksum
 *
 * @param dataptr points to start of data to be summed at any boundary
 * @param len length of data to be summed
 * @return host order (!) lwip checksum (non-inverted Internet sum)
 *
 * @note accumulator size limits summable lenght to 64k
 * @note host endianess is irrelevant (p3 RFC1071)
 */
static u16_t
lwip_standard_chksum(void *dataptr, u16_t len)
{
  u32_t acc;
  u16_t src;
  u8_t *octetptr;

  acc = 0;
  /* dataptr may be at odd or even addresses */
  octetptr = (u8_t*)dataptr;
  while (len > 1)
  {
    /* declare first octet as most significant
       thus assume network order, ignoring host order */
    src = (*octetptr) << 8;
    octetptr++;
    /* declare second octet as least significant */
    src |= (*octetptr);
    octetptr++;
    acc += src;
    len -= 2;
  }
  if (len > 0)
  {
    /* accumulate remaining octet */
    src = (*octetptr) << 8;
    acc += src;
  }
  /* add deferred carry bits */
  acc = (acc >> 16) + (acc & 0x0000ffffUL);
  if ((acc & 0xffff0000) != 0) {
    acc = (acc >> 16) + (acc & 0x0000ffffUL);
  }
  /* This maybe a little confusing: reorder sum using htons()
     instead of ntohs() since it has a little less call overhead.
     The caller must invert bits for Internet sum ! */
  return htons((u16_t)acc);
}

#endif

#if 0
/*
 * Curt McDowell
 * Broadcom Corp.
 * csm@broadcom.com
 *
 * IP checksum two bytes at a time with support for
 * unaligned buffer.
 * Works for len up to and including 0x20000.
 * by Curt McDowell, Broadcom Corp. 12/08/2005
 */

static u16_t
lwip_standard_chksum2(void *dataptr, int len)
{
  u8_t *pb = dataptr;
  u16_t *ps, t = 0;
  u32_t sum = 0;
  int odd = ((u32_t)pb & 1);

  /* Get aligned to u16_t */
  if (odd && len > 0) {
    ((u8_t *)&t)[1] = *pb++;
    len--;
  }

  /* Add the bulk of the data */
  ps = (u16_t *)pb;
  while (len > 1) {
    sum += *ps++;
    len -= 2;
  }

  /* Consume left-over byte, if any */
  if (len > 0)
    ((u8_t *)&t)[0] = *(u8_t *)ps;;

  /* Add end bytes */
  sum += t;

  /*  Fold 32-bit sum to 16 bits */
  while (sum >> 16)
    sum = (sum & 0xffff) + (sum >> 16);

  /* Swap if alignment was odd */
  if (odd)
    sum = ((sum & 0xff) << 8) | ((sum & 0xff00) >> 8);

  return sum;
}

/**
 * An optimized checksum routine. Basically, it uses loop-unrolling on
 * the checksum loop, treating the head and tail bytes specially, whereas
 * the inner loop acts on 8 bytes at a time.
 *
 * @arg start of buffer to be checksummed. May be an odd byte address.
 * @len number of bytes in the buffer to be checksummed.
 *
 * @todo First argument type conflicts with generic checksum routine.
 *
 * by Curt McDowell, Broadcom Corp. December 8th, 2005
 */

static u16_t
lwip_standard_chksum4(u8_t *pb, int len)
{
  u16_t *ps, t = 0;
  u32_t *pl;
  u32_t sum = 0, tmp;
  /* starts at odd byte address? */
  int odd = ((u32_t)pb & 1);

  if (odd && len > 0) {
    ((u8_t *)&t)[1] = *pb++;
    len--;
  }

  ps = (u16_t *)pb;

  if (((u32_t)ps & 3) && len > 1) {
    sum += *ps++;
    len -= 2;
  }

  pl = (u32_t *)ps;

  while (len > 7)  {
    tmp = sum + *pl++;          /* ping */
    if (tmp < sum)
      tmp++;                    /* add back carry */

    sum = tmp + *pl++;          /* pong */
    if (sum < tmp)
      sum++;                    /* add back carry */

    len -= 8;
  }

  /* make room in upper bits */
  sum = (sum >> 16) + (sum & 0xffff);

  ps = (u16_t *)pl;

  /* 16-bit aligned word remaining? */
  while (len > 1) {
    sum += *ps++;
    len -= 2;
  }

  /* dangling tail byte remaining? */
  if (len > 0)                  /* include odd byte */
    ((u8_t *)&t)[0] = *(u8_t *)ps;

  sum += t;                     /* add end bytes */

  while (sum >> 16)             /* combine halves */
    sum = (sum >> 16) + (sum & 0xffff);

  if (odd)
    sum = ((sum & 0xff) << 8) | ((sum & 0xff00) >> 8);

  return sum;
}
#endif

/* inet_chksum_pseudo:
 *
 * Calculates the pseudo Internet checksum used by TCP and UDP for a pbuf chain.
 */

u16_t
inet_chksum_pseudo(struct pbuf *p,
       struct ip_addr *src, struct ip_addr *dest,
       u8_t proto, u16_t proto_len)
{
  u32_t acc;
  struct pbuf *q;
  u8_t swapped;

  acc = 0;
  swapped = 0;
  /* iterate through all pbuf in chain */
  for(q = p; q != NULL; q = q->next) {
    LWIP_DEBUGF(INET_DEBUG, ("inet_chksum_pseudo(): checksumming pbuf %p (has next %p) \n",
      (void *)q, (void *)q->next));
    acc += LWIP_CHKSUM(q->payload, q->len);
    /*LWIP_DEBUGF(INET_DEBUG, ("inet_chksum_pseudo(): unwrapped lwip_chksum()=%"X32_F" \n", acc));*/
    while (acc >> 16) {
      acc = (acc & 0xffffUL) + (acc >> 16);
    }
    if (q->len % 2 != 0) {
      swapped = 1 - swapped;
      acc = ((acc & 0xff) << 8) | ((acc & 0xff00UL) >> 8);
    }
    /*LWIP_DEBUGF(INET_DEBUG, ("inet_chksum_pseudo(): wrapped lwip_chksum()=%"X32_F" \n", acc));*/
  }

  if (swapped) {
    acc = ((acc & 0xff) << 8) | ((acc & 0xff00UL) >> 8);
  }
  acc += (src->addr & 0xffffUL);
  acc += ((src->addr >> 16) & 0xffffUL);
  acc += (dest->addr & 0xffffUL);
  acc += ((dest->addr >> 16) & 0xffffUL);
  acc += (u32_t)htons((u16_t)proto);
  acc += (u32_t)htons(proto_len);

  while (acc >> 16) {
    acc = (acc & 0xffffUL) + (acc >> 16);
  }
  LWIP_DEBUGF(INET_DEBUG, ("inet_chksum_pseudo(): pbuf chain lwip_chksum()=%"X32_F"\n", acc));
  return (u16_t)~(acc & 0xffffUL);
}

/* inet_chksum:
 *
 * Calculates the Internet checksum over a portion of memory. Used primarely for IP
 * and ICMP.
 */

u16_t
inet_chksum(void *dataptr, u16_t len)
{
  u32_t acc;

  acc = LWIP_CHKSUM(dataptr, len);
  while (acc >> 16) {
    acc = (acc & 0xffff) + (acc >> 16);
  }
  return (u16_t)~(acc & 0xffff);
}

u16_t
inet_chksum_pbuf(struct pbuf *p)
{
  u32_t acc;
  struct pbuf *q;
  u8_t swapped;

  acc = 0;
  swapped = 0;
  for(q = p; q != NULL; q = q->next) {
    acc += LWIP_CHKSUM(q->payload, q->len);
    while (acc >> 16) {
      acc = (acc & 0xffffUL) + (acc >> 16);
    }
    if (q->len % 2 != 0) {
      swapped = 1 - swapped;
      acc = (acc & 0x00ffUL << 8) | (acc & 0xff00UL >> 8);
    }
  }

  if (swapped) {
    acc = ((acc & 0x00ffUL) << 8) | ((acc & 0xff00UL) >> 8);
  }
  return (u16_t)~(acc & 0xffffUL);
}
