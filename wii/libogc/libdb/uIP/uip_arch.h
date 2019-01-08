/**
 * \defgroup uiparch Architecture specific uIP functions
 * @{
 *
 * The functions in the architecture specific module implement the IP
 * check sum and 32-bit additions.
 *
 * The IP checksum calculation is the most computationally expensive
 * operation in the TCP/IP stack and it therefore pays off to
 * implement this in efficient assembler. The purpose of the uip-arch
 * module is to let the checksum functions to be implemented in
 * architecture specific assembler.
 *
 */

/**
 * \file
 * Declarations of architecture specific functions.
 * \author Adam Dunkels <adam@dunkels.com>
 */

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

#ifndef __UIP_ARCH_H__
#define __UIP_ARCH_H__

#include "uip.h"

#define UIP_MIN(x,y)			(x)<(y)?(x):(y)

#define MEM_ALIGNMENT			4
#define MEM_ALIGN(mem)			((void*)(((u32_t)(mem)+MEM_ALIGNMENT-1)&~(u32_t)(MEM_ALIGNMENT-1)))
#define MEM_ALIGN_SIZE(size)	(((size)+MEM_ALIGNMENT-1)&~(u32_t)(MEM_ALIGNMENT-1))

#define PACK_STRUCT_STRUCT		__attribute__((packed))
#define PACK_STRUCT_FIELD(x)	x
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

#ifndef htons
#define htons(x) (x)
#endif
#ifndef ntohs
#define ntohs(x) (x)
#endif
#ifndef htonl
#define htonl(x) (x)
#endif
#ifndef ntohl
#define ntohl(x) (x)
#endif

struct uip_pbuf;
struct uip_ip_addr;

/**
 * Calculate the Internet checksum over a buffer.
 *
 * The Internet checksum is the one's complement of the one's
 * complement sum of all 16-bit words in the buffer.
 *
 * See RFC1071.
 *
 * \note This function is not called in the current version of uIP,
 * but future versions might make use of it.
 *
 * \param buf A pointer to the buffer over which the checksum is to be
 * computed.
 *
 * \param len The length of the buffer over which the checksum is to
 * be computed.
 *
 * \return The Internet checksum of the buffer.
 */
u16_t uip_chksum(u16_t *buf, u32_t len);

/**
 * Calculate the IP header checksum of the packet header in uip_buf.
 *
 * The IP header checksum is the Internet checksum of the 20 bytes of
 * the IP header.
 *
 * \return The IP header checksum of the IP header in the uip_buf
 * buffer.
 */
u16_t uip_ipchksum(void *dataptr,u16_t len);

u16_t uip_ipchksum_pbuf(struct uip_pbuf *p);

/**
 * Calculate the TCP checksum of the packet in uip_buf and uip_appdata.
 *
 * The TCP checksum is the Internet checksum of data contents of the
 * TCP segment, and a pseudo-header as defined in RFC793.
 *
 * \note The uip_appdata pointer that points to the packet data may
 * point anywhere in memory, so it is not possible to simply calculate
 * the Internet checksum of the contents of the uip_buf buffer.
 *
 * \return The TCP checksum of the TCP segment in uip_buf and pointed
 * to by uip_appdata.
 */
u16_t uip_chksum_pseudo(struct uip_pbuf *p,struct uip_ip_addr *src,struct uip_ip_addr *dst,u8_t proto,u16_t proto_len);

extern void tcpip_tmr_needed();
#define tcp_tmr_needed		tcpip_tmr_needed

#if UIP_LIBC_MEMFUNCREPLACE
static __inline__ void uip_memcpy(void *dest,const void *src,s32_t len)
{
	u8_t *dest0 = (u8_t*)dest;
	u8_t *src0 = (u8_t*)src;

	while(len--) {
		*dest0++ = *src0++;
	}
}

static __inline__ void uip_memset(void *dest,s32_t c,s32_t len)
{
	u8_t *dest0 = (u8_t*)dest;

	while(len--) {
		*dest0++ = (s8_t)c;
	}
}

#define UIP_MEMCPY				uip_memcpy
#define UIP_MEMSET				uip_memset
#else
#define UIP_MEMCPY				memcpy
#define UIP_MEMSET				memset
#endif

/** @} */

#endif /* __UIP_ARCH_H__ */
