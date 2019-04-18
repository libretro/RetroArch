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
 * This file is part of the lwBT stack.
 *
 *
 */

#ifndef __BT_ARCH_H__
#define __BT_ARCH_H__

#include "bt.h"
#include "asm.h"
#include "processor.h"

#define MEM_ALIGNMENT			4
#define MEM_ALIGN(mem)			((void*)(((u32_t)(mem)+MEM_ALIGNMENT-1)&~(u32_t)(MEM_ALIGNMENT-1)))
#define MEM_ALIGN_SIZE(size)	(((size)+MEM_ALIGNMENT-1)&~(u32_t)(MEM_ALIGNMENT-1))

#if BYTE_ORDER == BIG_ENDIAN
	#ifndef htole16
		#define htole16		bswap16
	#endif
	#ifndef htole32
		#define htole32		bswap32
	#endif
	#ifndef htole64
		#define htole64		bswap64
	#endif
	#ifndef le16toh
		#define le16toh		bswap16
	#endif
	#ifndef le32toh
		#define le32toh		bswap32
	#endif
	#ifndef le642toh
		#define le64toh		bswap64
	#endif
	#ifndef htons
		#define htons(x)	(x)
	#endif
	#ifndef htonl
		#define htonl(x)	(x)
	#endif
	#ifndef ntohl
		#define ntohl(x)	(x)
	#endif
	#ifndef ntohs
		#define ntohs(x)	(x)
	#endif
#else
	#ifndef htole16
		#define htole16
	#endif
	#ifndef htole32
		#define htole32
	#endif
	#ifndef le16toh
		#define le16toh
	#endif
	#ifndef le32toh
		#define le32toh
	#endif
#endif

#if LIBC_MEMFUNCREPLACE
static __inline__ void __memcpy(void *dest,const void *src,s32_t len)
{
	u8_t *dest0 = (u8_t*)dest;
	u8_t *src0 = (u8_t*)src;

	while(len--) {
		*dest0++ = *src0++;
	}
}

static __inline__ void __memset(void *dest,s32_t c,s32_t len)
{
	u8_t *dest0 = (u8_t*)dest;

	while(len--) {
		*dest0++ = (s8_t)c;
	}
}

#define MEMCPY				__memcpy
#define MEMSET				__memset
#else
#define MEMCPY				memcpy
#define MEMSET				memset
#endif

#if LOGGING == 1
#include <stdio.h>
#define LOG(fmt, ...) fprintf(stderr, "[BTLOG] " __FILE__ ":%i: " fmt "\n", __LINE__, ##__VA_ARGS__)
#else
#define LOG(fmt, ...)
#endif /* LOGGING == 1 */

#if ERRORING == 1
#include <stdio.h>
#define ERROR(fmt,...) fprintf(stderr, "[BTERR] " __FILE__ ":%i: " fmt "\n", __LINE__, ##__VA_ARGS__)
#else
#define ERROR(fmt, ...)
#endif /* ERRORING == 1 */

/** @} */

#endif /* __UIP_ARCH_H__ */
