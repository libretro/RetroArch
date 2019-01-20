/**
 * \addtogroup uip
 * @{
 */

/**
 * \file
 * Header file for the uIP TCP/IP stack.
 * \author Adam Dunkels <adam@dunkels.com>
 *
 * The uIP TCP/IP stack header file contains definitions for a number
 * of C macros that are used by uIP programs as well as internal uIP
 * structures, TCP/IP header structures and function declarations.
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

#ifndef __BT_H__
#define __BT_H__

#include "btopt.h"
#include "btarch.h"

#define ERR_OK			0
#define ERR_MEM			-1
#define ERR_BUF			-2
#define ERR_ABRT		-3
#define ERR_RST			-4
#define ERR_CLSD		-5
#define ERR_CONN		-6
#define ERR_VAL			-7
#define ERR_ARG			-8
#define ERR_RTE			-9
#define ERR_USE			-10
#define ERR_IF			-11
#define ERR_PKTSIZE		-17

#define PROTO_ICMP		1
#define PROTO_TCP		6
#define PROTO_UDP		17

/* Headezes. */
#define IP_HLEN			20    /* Size of IP header */
#define TRANSPORT_HLEN	20

#define UDP_HLEN		8    /* Size of UDP header */
#define TCP_HLEN		20    /* Size of TCP header */
#define IPUDP_HLEN		28    /* Size of IP + UDP header */
#define IPTCP_HLEN		40    /* Size of IP + TCP header */

/**
 * Convert 16-bit quantity from host byte order to network byte order.
 *
 * This macro is primarily used for converting constants from host
 * byte order to network byte order. For converting variables to
 * network byte order, use the htons() function instead.
 *
 * \hideinitializer
 */
#ifndef HTONS
#   if BYTE_ORDER == BIG_ENDIAN
#      define HTONS(n) (n)
#   else /* BYTE_ORDER == BIG_ENDIAN */
#      define HTONS(n) ((((u16_t)((n) & 0xff)) << 8) | (((n) & 0xff00) >> 8))
#   endif /* BYTE_ORDER == BIG_ENDIAN */
#endif /* HTONS */

/**
 * Convert 16-bit quantity from host byte order to network byte order.
 *
 * This function is primarily used for converting variables from host
 * byte order to network byte order. For converting constants to
 * network byte order, use the HTONS() macro instead.
 */
#ifndef htons
u16_t htons(u16_t val);
#endif /* htons */

/** @} */

#endif /* __UIP_H__ */

/** @} */
