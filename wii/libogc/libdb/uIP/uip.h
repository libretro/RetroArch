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

#ifndef __UIP_H__
#define __UIP_H__

#include "uipopt.h"
#include "uip_arch.h"
#include "uip_ip.h"

#define UIP_ERR_OK			0
#define UIP_ERR_MEM			-1
#define UIP_ERR_BUF			-2
#define UIP_ERR_ABRT		-3
#define UIP_ERR_RST			-4
#define UIP_ERR_CLSD		-5
#define UIP_ERR_CONN		-6
#define UIP_ERR_VAL			-7
#define UIP_ERR_ARG			-8
#define UIP_ERR_RTE			-9
#define UIP_ERR_USE			-10
#define UIP_ERR_IF			-11
#define UIP_ERR_PKTSIZE		-17

#define UIP_PROTO_ICMP		1
#define UIP_PROTO_TCP		6
#define UIP_PROTO_UDP		17

/* Header sizes. */
#define UIP_IP_HLEN			20    /* Size of IP header */
#define UIP_TRANSPORT_HLEN	20

#define UIP_UDP_HLEN		8    /* Size of UDP header */
#define UIP_TCP_HLEN		20    /* Size of TCP header */
#define UIP_IPUDP_HLEN		28    /* Size of IP + UDP header */
#define UIP_IPTCP_HLEN		40    /* Size of IP + TCP header */

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
 * The structure holding the TCP/IP statistics that are gathered if
 * UIP_STATISTICS is set to 1.
 *
 */
struct uip_stats {
  struct {
    uip_stats_t drop;     /**< Number of dropped packets at the IP
			     layer. */
    uip_stats_t recv;     /**< Number of received packets at the IP
			     layer. */
    uip_stats_t sent;     /**< Number of sent packets at the IP
			     layer. */
    uip_stats_t vhlerr;   /**< Number of packets dropped due to wrong
			     IP version or header length. */
    uip_stats_t hblenerr; /**< Number of packets dropped due to wrong
			     IP length, high byte. */
    uip_stats_t lblenerr; /**< Number of packets dropped due to wrong
			     IP length, low byte. */
    uip_stats_t fragerr;  /**< Number of packets dropped since they
			     were IP fragments. */
    uip_stats_t chkerr;   /**< Number of packets dropped due to IP
			     checksum errors. */
    uip_stats_t protoerr; /**< Number of packets dropped since they
			     were neither ICMP, UDP nor TCP. */
  } ip;                   /**< IP statistics. */
  struct {
    uip_stats_t drop;     /**< Number of dropped ICMP packets. */
    uip_stats_t recv;     /**< Number of received ICMP packets. */
    uip_stats_t sent;     /**< Number of sent ICMP packets. */
    uip_stats_t typeerr;  /**< Number of ICMP packets with a wrong
			     type. */
  } icmp;                 /**< ICMP statistics. */
  struct {
    uip_stats_t drop;     /**< Number of dropped TCP segments. */
    uip_stats_t recv;     /**< Number of recived TCP segments. */
    uip_stats_t sent;     /**< Number of sent TCP segments. */
    uip_stats_t chkerr;   /**< Number of TCP segments with a bad
			     checksum. */
    uip_stats_t ackerr;   /**< Number of TCP segments with a bad ACK
			     number. */
    uip_stats_t rst;      /**< Number of recevied TCP RST (reset) segments. */
    uip_stats_t rexmit;   /**< Number of retransmitted TCP segments. */
    uip_stats_t syndrop;  /**< Number of dropped SYNs due to too few
			     connections was avaliable. */
    uip_stats_t synrst;   /**< Number of SYNs for closed ports,
			     triggering a RST. */
  } tcp;                  /**< TCP statistics. */
};

/**
 * The uIP TCP/IP statistics.
 *
 * This is the variable in which the uIP TCP/IP statistics are gathered.
 */
extern struct uip_stats uip_stat;

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
