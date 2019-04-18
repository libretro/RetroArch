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

#ifndef __UIPOPT_H__
#define __UIPOPT_H__

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
 * The statistics data type.
 *
 * This datatype determines how high the statistics counters are able
 * to count.
 */
typedef u16 uip_stats_t;

/** @} */
/*------------------------------------------------------------------------------*/
/**
 * \defgroup uipoptip IP configuration options
 * @{
 *
 */
/**
 * The IP TTL (time to live) of IP packets sent by uIP.
 *
 * This should normally not be changed.
 */
#define UIP_TCP_TTL         255

#define UIP_TCP				1

/**
 * Turn on support for IP packet reassembly.
 *
 * uIP supports reassembly of fragmented IP packets. This features
 * requires an additonal amount of RAM to hold the reassembly buffer
 * and the reassembly code size is approximately 700 bytes.  The
 * reassembly buffer is of the same size as the uip_buf buffer
 * (configured by UIP_BUFSIZE).
 *
 * \note IP packet reassembly is not heavily tested.
 *
 * \hideinitializer
 */
#define UIP_IP_REASSEMBLY 0

#define UIP_IP_FRAG 0

/**
 * The maximum time an IP fragment should wait in the reassembly
 * buffer before it is dropped.
 *
 */
#define UIP_REASS_MAXAGE 30

/** @} */

/*------------------------------------------------------------------------------*/
/**
 * \defgroup uipoptudp UDP configuration options
 * @{
 *
 * \note The UDP support in uIP is still not entirely complete; there
 * is no support for sending or receiving broadcast or multicast
 * packets, but it works well enough to support a number of vital
 * applications such as DNS queries, though
 */

/**
 * Toggles wether UDP support should be compiled in or not.
 *
 * \hideinitializer
 */
#define UIP_UDP           0

/**
 * Toggles if UDP checksums should be used or not.
 *
 * \note Support for UDP checksums is currently not included in uIP,
 * so this option has no function.
 *
 * \hideinitializer
 */
#define UIP_UDP_CHECKSUMS 0

/**
 * The maximum amount of concurrent UDP connections.
 *
 * \hideinitializer
 */
#define UIP_UDP_CONNS    10

/**
 * The name of the function that should be called when UDP datagrams arrive.
 *
 * \hideinitializer
 */
//#define UIP_UDP_APPCALL		((void*0)

/** @} */
/*------------------------------------------------------------------------------*/
/**
 * \defgroup uipopttcp TCP configuration options
 * @{
 */

/**
 * Determines if support for opening connections from uIP should be
 * compiled in.
 *
 * If the applications that are running on top of uIP for this project
 * do not need to open outgoing TCP connections, this configration
 * option can be turned off to reduce the code size of uIP.
 *
 * \hideinitializer
 */
#define UIP_ACTIVE_OPEN				1

/**
 * The maximum number of simultaneously open TCP connections.
 *
 * Since the TCP connections are statically allocated, turning this
 * configuration knob down results in less RAM used. Each TCP
 * connection requires approximatly 30 bytes of memory.
 *
 * \hideinitializer
 */
#define UIP_TCP_PCBS				4

/**
 * The maximum number of simultaneously listening TCP ports.
 *
 * Each listening TCP port requires 2 bytes of memory.
 *
 * \hideinitializer
 */
#define UIP_LISTEN_TCP_PCBS			2

/**
 * The size of the advertised receiver's window.
 *
 * Should be set low (i.e., to the size of the uip_buf buffer) is the
 * application is slow to process incoming data, or high (32768 bytes)
 * if the application processes data quickly.
 *
 * \hideinitializer
 */
#define UIP_TCPIP_SOCKS				32

#define UIP_TCP_SEGS				32

/**
 * Determines if support for TCP urgent data notification should be
 * compiled in.
 *
 * Urgent data (out-of-band data) is a rarely used TCP feature that
 * very seldom would be required.
 *
 * \hideinitializer
 */
#define UIP_URGDATA				1

/**
 * The initial retransmission timeout counted in timer pulses.
 *
 * This should not be changed.
 */
#define UIP_RTO					3

/**
 * The maximum number of times a segment should be retransmitted
 * before the connection should be aborted.
 *
 * This should not be changed.
 */
#define UIP_MAXRTX				12

/**
 * The maximum number of times a SYN segment should be retransmitted
 * before a connection request should be deemed to have been
 * unsuccessful.
 *
 * This should not need to be changed.
 */
#define UIP_MAXSYNRTX			4

/**
 * The TCP maximum segment size.
 *
 * This is should not be to set to more than UIP_BUFSIZE - UIP_LLH_LEN - 40.
 */
#define UIP_TCP_MSS				(1460)

#define UIP_TCP_SND_BUF			(4*UIP_TCP_MSS)

#define UIP_TCP_SND_QUEUELEN	(4*UIP_TCP_SND_BUF/UIP_TCP_MSS)

#define UIP_TCP_WND				(4*UIP_TCP_MSS)

/**
 * How long a connection should stay in the TIME_WAIT state.
 *
 * This configiration option has no real implication, and it should be
 * left untouched.
 */
#define UIP_TIME_WAIT_TIMEOUT	120

/** @} */
/*------------------------------------------------------------------------------*/
/**
 * \defgroup uipoptarp ARP configuration options
 * @{
 */

/**
 * The size of the ARP table.
 *
 * This option should be set to a larger value if this uIP node will
 * have many connections from the local network.
 *
 * \hideinitializer
 */
#define UIP_ARPTAB_SIZE 8

/**
 * The maxium age of ARP table entries measured in 10ths of seconds.
 *
 * An UIP_ARP_MAXAGE of 120 corresponds to 20 minutes (BSD
 * default).
 */
#define UIP_ARP_MAXAGE 120

/** @} */

/*------------------------------------------------------------------------------*/

/**
 * \defgroup uipoptgeneral General configuration options
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
#define UIP_MEM_SIZE				(28*1024)

#define UIP_PBUF_POOL_NUM			16
#define UIP_PBUF_POOL_BUFSIZE		1600

#define UIP_PBUF_ROM_NUM			128

/**
 * Determines if statistics support should be compiled in.
 *
 * The statistics is useful for debugging and to show the user.
 *
 * \hideinitializer
 */
#define UIP_STATISTICS  0

/**
 * Determines if logging of certain events should be compiled in.
 *
 * This is useful mostly for debugging. The function uip_log()
 * must be implemented to suit the architecture of the project, if
 * logging is turned on.
 *
 * \hideinitializer
 */
#define UIP_LOGGING     0
#define UIP_ERRORING	0

/**
 * Print out a uIP log message.
 *
 * This function must be implemented by the module that uses uIP, and
 * is called by uIP whenever a log message is generated.
 */
void uip_log(const char *filename,int line_nb,char *msg);

/**
 * The link level header length.
 *
 * This is the offset into the uip_buf where the IP header can be
 * found. For Ethernet, this should be set to 14. For SLIP, this
 * should be set to 0.
 *
 * \hideinitializer
 */
#define UIP_LL_HLEN     16

#define UIP_TCPIP_HLEN	40
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

/**
 * \defgroup uipoptapp Appication specific configurations
 * @{
 *
 * An uIP application is implemented using a single application
 * function that is called by uIP whenever a TCP/IP event occurs. The
 * name of this function must be registered with uIP at compile time
 * using the UIP_APPCALL definition.
 *
 * uIP applications can store the application state within the
 * uip_conn structure by specifying the size of the application
 * structure with the UIP_APPSTATE_SIZE macro.
 *
 * The file containing the definitions must be included in the
 * uipopt.h file.
 *
 * The following example illustrates how this can look.
 \code

void httpd_appcall(void);
#define UIP_APPCALL     httpd_appcall

struct httpd_state {
  u8_t state;
  u16_t count;
  char *dataptr;
  char *script;
};
#define UIP_APPSTATE_SIZE (sizeof(struct httpd_state))
 \endcode
 */

/**
 * \var #define UIP_APPCALL
 *
 * The name of the application function that uIP should call in
 * response to TCP/IP events.
 *
 */

/**
 * \var #define UIP_APPSTATE_SIZE
 *
 * The size of the application state that is to be stored in the
 * uip_conn structure.
 */
/** @} */

/* Include the header file for the application program that should be
   used. If you don't use the example web server, you should change
   this. */

#define UIP_LIBC_MEMFUNCREPLACE				1

#endif /* __UIPOPT_H__ */
