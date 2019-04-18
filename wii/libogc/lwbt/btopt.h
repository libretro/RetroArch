/**
 * \defgroup uipopt Configuration options for uIP
 * @{
 *
 * uIP is configured using the per-project configuration file
 * "uipopt.h". This file contains all compile-time options for uIP and
 * should be tweaked to match each specific project. The uIP
 * distribution contains a documented example "uipopt.h" that can be
 * copied and modified for each project.
 */

/**
 * \file
 * Configuration options for uIP.
 * \author Adam Dunkels <adam@dunkels.com>
 *
 * This file is used for tweaking various configuration options for
 * uIP. You should make a copy of this file into one of your project's
 * directories instead of editing this example "uipopt.h" file that
 * comes with the uIP distribution.
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

#ifndef __BTOPT_H__
#define __BTOPT_H__

#include <gctypes.h>
#include <stdlib.h>
#include <string.h>

/*------------------------------------------------------------------------------*/
/**
 * \defgroup uipopttypedef uIP type definitions
 * @{
 */

/**
 * The 8-bit unsigned data type.
 *
 * This may have to be tweaked for your particular compiler. "unsigned
 * char" works for most compilers.
 */
typedef u8 u8_t;

/**
 * The 8-bit signed data type.
 *
 * This may have to be tweaked for your particular compiler. "unsigned
 * char" works for most compilers.
 */
typedef s8 s8_t;

/**
 * The 16-bit unsigned data type.
 *
 * This may have to be tweaked for your particular compiler. "unsigned
 * short" works for most compilers.
 */
typedef u16 u16_t;

/**
 * The 16-bit signed data type.
 *
 * This may have to be tweaked for your particular compiler. "unsigned
 * short" works for most compilers.
 */
typedef s16 s16_t;

/**
 * The 32-bit signed data type.
 *
 * This may have to be tweaked for your particular compiler. "unsigned
 * short" works for most compilers.
 */
typedef s32 s32_t;

/**
 * The 32-bit unsigned data type.
 *
 * This may have to be tweaked for your particular compiler. "unsigned
 * short" works for most compilers.
 */
typedef u32 u32_t;

/**
 * The 64-bit unsigned data type.
 *
 * This may have to be tweaked for your particular compiler. "unsigned
 * short" works for most compilers.
 */
typedef u64 u64_t;

/**
 * The 64-bit signed data type.
 *
 * This may have to be tweaked for your particular compiler. "unsigned
 * short" works for most compilers.
 */
typedef s64 s64_t;

/**
 * The statistics data type.
 *
 * This datatype determines how high the statistics counters are able
 * to count.
 */
typedef s8 err_t;

/*------------------------------------------------------------------------------*/

/**
 * \defgroup btopt general configuration options
 * @{
 */

/**
 * The size of the uIP packet buffer.
 *
 * The uIP packet buffer should not be smaller than 60 bytes, and does
 * not need to be larger than 1500 bytes. Lower size results in lower
 * TCP throughput, larger size results in higher TCP throughput.
 *
 * \hideinitializer
 */
#define MEM_SIZE				(64*1024)

#define PBUF_POOL_NUM			(HCI_HOST_MAX_NUM_ACL*MAX_NUM_CLIENTS)
#define PBUF_POOL_BUFSIZE		HCI_HOST_ACL_MAX_LEN

#define PBUF_ROM_NUM			45

/**
 * Determines if statistics support should be compiled in.
 *
 * The statistics is useful for debugging and to show the user.
 *
 * \hideinitializer
 */
#define STATISTICS  0

/**
 * Determines if logging of certain events should be compiled in.
 *
 * This is useful mostly for debugging. The function uip_log()
 * must be implemented to suit the architecture of the project, if
 * logging is turned on.
 *
 * \hideinitializer
 */
#define LOGGING     0
#define ERRORING	0

/**
 * Print out a uIP log message.
 *
 * This function must be implemented by the module that uses uIP, and
 * is called by uIP whenever a log message is generated.
 */
void bt_log(const char *filename,int line_nb,char *msg);

/**
 * The link level header length.
 *
 * This is the offset into the uip_buf where the IP header can be
 * found. For Ethernet, this should be set to 14. For SLIP, this
 * should be set to 0.
 *
 * \hideinitializer
 */
#define LL_HLEN     16

#define TCPIP_HLEN	40
/** @} */
/*------------------------------------------------------------------------------*/
/**
 * \defgroup uipoptcpu CPU architecture configuration
 * @{
 *
 * The CPU architecture configuration is where the endianess of the
 * CPU on which uIP is to be run is specified. Most CPUs today are
 * little endian, and the most notable exception are the Motorolas
 * which are big endian. The BYTE_ORDER macro should be changed to
 * reflect the CPU architecture on which uIP is to be run.
 */
#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN  3412
#endif /* LITTLE_ENDIAN */
#ifndef BIG_ENDIAN
#define BIG_ENDIAN     1234
#endif /* BIGE_ENDIAN */

/**
 * The byte order of the CPU architecture on which uIP is to be run.
 *
 * This option can be either BIG_ENDIAN (Motorola byte order) or
 * LITTLE_ENDIAN (Intel byte order).
 *
 * \hideinitializer
 */
#ifndef BYTE_ORDER
#define BYTE_ORDER     BIG_ENDIAN
#endif /* BYTE_ORDER */

/** @} */
/*------------------------------------------------------------------------------*/

#define LIBC_MEMFUNCREPLACE				0

/* ---------- Memory options ---------- */
#define MAX_NUM_CLIENTS					6 /* Maximum number of connected Bluetooth clients. No more than 6 */
#define MAX_NUM_OPT_CLIENTS             10 /* Maximum number of possible Bluetooth clients we might listen to */

#define MEMB_NUM_HCI_PCB				1 /* Always set to one */
#define MEMB_NUM_HCI_LINK				MAX_NUM_CLIENTS /* One for DT + One per ACL connection */
#define MEMB_NUM_HCI_INQ				256 /* One per max number of returned results from an inquiry */
#define MEMB_NUM_HCI_LINK_KEY			256 /* One per max number of returned results from an read stored link key */

/* MEMP_NUM_L2CAP_PCB: the number of simulatenously active L2CAP
   connections. */
#define MEMB_NUM_L2CAP_PCB				(2 + 2 * MAX_NUM_CLIENTS) /* One for a closing connection + one for DT + one per number of connected Bluetooth clients */
/* MEMP_NUM_L2CAP_PCB_LISTEN: the number of listening L2CAP
   connections. */
#define MEMB_NUM_L2CAP_PCB_LISTEN		(2 * MAX_NUM_OPT_CLIENTS) /* One per listening PSM */
/* MEMP_NUM_L2CAP_SIG: the number of simultaneously unresponded
   L2CAP signals */
#define MEMB_NUM_L2CAP_SIG				(2 * MAX_NUM_CLIENTS)/* Two per number of connected Bluetooth clients but min 2 */
#define MEMB_NUM_L2CAP_SEG				(2 + 2 * MAX_NUM_CLIENTS) /* One per number of L2CAP connections */

#define MEMB_NUM_SDP_PCB				MAX_NUM_CLIENTS /* One per number of connected Bluetooth clients */
#define MEMB_NUM_SDP_RECORD				1 /* One per registered service record */

#define MEMP_NUM_RFCOMM_PCB				(2 + 2 * MAX_NUM_CLIENTS) /* Two for DT + Two per number of connected Bluetooth clients */
#define MEMP_NUM_RFCOMM_PCB_LISTEN		(2 * MAX_NUM_CLIENTS) /* Two per number of connected Bluetooth clients */

#define MEMP_NUM_HIDP_PCB				(2 + 2 * MAX_NUM_CLIENTS) /* Two for DT + Two per number of connected Bluetooth clients */
#define MEMP_NUM_HIDP_PCB_LISTEN		(2 * MAX_NUM_CLIENTS) /* Two per number of connected Bluetooth clients */

#define MEMP_NUM_PPP_PCB				(1 + MAX_NUM_CLIENTS) /* One for DT + One per number of connected Bluetooth clients */
#define MEMP_NUM_PPP_REQ				MAX_NUM_CLIENTS /* One per number of connected Bluetooth clients but min 1 */

#define MEMP_NUM_BTE_PCB				(2 + 2 * MAX_NUM_CLIENTS) /* Two for DT + Two per number of connected Bluetooth clients */
#define MEMP_NUM_BTE_PCB_LISTEN			(2 * MAX_NUM_CLIENTS) /* Two per number of connected Bluetooth clients */

#define MEMP_NUM_BTE_CTRLS				256

/* ---------- HCI options ---------- */
/* HCI: Defines if we have lower layers of the Bluetooth stack running on a separate host
   controller */
#define HCI 1

#if HCI
/* HCI_HOST_MAX_NUM_ACL: The maximum number of ACL packets that the host can buffer */
#define HCI_HOST_MAX_NUM_ACL			20 //TODO: Should be equal to PBUF_POOL_SIZE/2??? */
/* HCI_HOST_ACL_MAX_LEN: The maximum size of an ACL packet that the host can buffer */
#define HCI_HOST_ACL_MAX_LEN			1691 /* Default: RFCOMM MFS + ACL header size, L2CAP header size,
                                                RFCOMM header size and RFCOMM FCS size */
/* HCI_PACKET_TYPE: The set of packet types which may be used on the connection. In order to
   maximize packet throughput, it is recommended that RFCOMM should make use of the 3 and 5
   slot baseband packets.*/
#define HCI_PACKET_TYPE					0xCC18 /* Default DM1, DH1, DM3, DH3, DM5, DH5 */
/* HCI_ALLOW_ROLE_SWITCH: Tells the host controller whether to accept a Master/Slave switch
   during establishment of a connection */
#define HCI_ALLOW_ROLE_SWITCH			1 /* Default 1 */
/* HCI_FLOW_QUEUEING: Control if a packet should be queued if the host controller is out of
   bufferspace for outgoing packets. Only the first packet sent when out of credits will be
   queued */
#define HCI_FLOW_QUEUEING 0 /* Default: 0 */

#endif /* HCI */

/* ---------- L2CAP options ---------- */
/* L2CAP_HCI: Option for including HCI to access the Bluetooth baseband capabilities */
#define L2CAP_HCI						1 //TODO: NEEDED?
/* L2CAP_CFG_QOS: Control if a flow specification similar to RFC 1363 should be used */
#define L2CAP_CFG_QOS					0
/* L2CAP_MTU: Maximum transmission unit for L2CAP packet payload (min 48) */
#define L2CAP_MTU						(HIDD_N + 1)/* Default for this implementation is RFCOMM MFS + RFCOMM header size and
														 RFCOMM FCS size while the L2CAP default is 672 */
/* L2CAP_OUT_FLUSHTO: For some networking protocols, such as many real-time protocols, guaranteed delivery
   is undesirable. The flush time-out value SHALL be set to its default value 0xffff for a reliable L2CAP
   channel, and MAY be set to other values if guaranteed delivery is not desired. (min 1) */
#define L2CAP_OUT_FLUSHTO				0xFFFF /* Default: 0xFFFF. Infinite number of retransmissions (reliable channel)
												  The value of 1 implies no retransmissions at the Baseband level
												  should be performed since the minimum polling interval is 1.25 ms.*/
/* L2CAP_RTX: The Responsive Timeout eXpired timer is used to terminate
   the channel when the remote endpoint is unresponsive to signalling
   requests (min 1s, max 60s) */
#define L2CAP_RTX						60
/* L2CAP_ERTX: The Extended Response Timeout eXpired timer is used in
   place of the RTC timer when a L2CAP_ConnectRspPnd event is received
   (min 60s, max 300s) */
#define L2CAP_ERTX						300
/* L2CAP_MAXRTX: Maximum number of Request retransmissions before
   terminating the channel identified by the request. The decision
   should be based on the flush timeout of the signalling link. If the
   flush timeout is infinite, no retransmissions should be performed */
#define L2CAP_MAXRTX					0
/* L2CAP_CFG_TO: Amount of time spent arbitrating the channel parameters
   before terminating the connection (max 120s) */
#define L2CAP_CFG_TO					30

/* ---------- BTE options ---------- */

/* ---------- HIDD options ---------- */
/* RFCOMM_N: Maximum frame size for RFCOMM segments (min 23, max 32767)*/
#define HIDD_N							672			 /* Default: Worst case byte stuffed PPP packet size +
																 non-compressed PPP header size and FCS size */
/* RFCOMM_K: Initial amount of credits issued to the peer (min 0, max 7) */
#define RFCOMM_K						0
/* RFCOMM_TO: Acknowledgement timer (T1) and response timer for multiplexer control channel (T2).
   T1 is the timeout for frames sent with the P/F bit set to 1 (SABM and DISC) and T2 is the timeout
   for commands sent in UIH frames on DLCI 0 (min 10s, max 60s) */
#define RFCOMM_TO						20
/* RFCOMM_FLOW_QUEUEING: Control if a packet should be queued if a channel is out of credits for
   outgoing packets. Only the first packet sent when out of credits will be queued */
#define RFCOMM_FLOW_QUEUEING			0 /* Default: 0 */

#endif /* __BTOPT_H__ */
