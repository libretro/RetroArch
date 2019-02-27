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
#ifndef __LWIP_OPT_H__
#define __LWIP_OPT_H__

/* Include user defined options first */
#include "lwipopts.h"
#include "lwip/debug.h"

/* Define default values for unconfigured parameters. */

/* Platform specific locking */

/*
 * enable SYS_LIGHTWEIGHT_PROT in lwipopts.h if you want inter-task protection
 * for certain critical regions during buffer allocation, deallocation and memory
 * allocation and deallocation.
 */
#ifndef SYS_LIGHTWEIGHT_PROT
#define SYS_LIGHTWEIGHT_PROT            0
#endif

#ifndef NO_SYS
#define NO_SYS                          0
#endif
/* ---------- Memory options ---------- */
/* MEM_ALIGNMENT: should be set to the alignment of the CPU for which
   lwIP is compiled. 4 byte alignment -> define MEM_ALIGNMENT to 4, 2
   byte alignment -> define MEM_ALIGNMENT to 2. */

#ifndef MEM_ALIGNMENT
#define MEM_ALIGNMENT                   1
#endif

/* MEM_SIZE: the size of the heap memory. If the application will send
a lot of data that needs to be copied, this should be set high. */
#ifndef MEM_SIZE
#define MEM_SIZE                        1600
#endif

#ifndef MEMP_SANITY_CHECK
#define MEMP_SANITY_CHECK		0
#endif

/* MEMP_NUM_PBUF: the number of memp struct pbufs. If the application
   sends a lot of data out of ROM (or other static memory), this
   should be set high. */
#ifndef MEMP_NUM_PBUF
#define MEMP_NUM_PBUF                   16
#endif

/* Number of raw connection PCBs */
#ifndef MEMP_NUM_RAW_PCB
#define MEMP_NUM_RAW_PCB                4
#endif

/* MEMP_NUM_UDP_PCB: the number of UDP protocol control blocks. One
   per active UDP "connection". */
#ifndef MEMP_NUM_UDP_PCB
#define MEMP_NUM_UDP_PCB                4
#endif
/* MEMP_NUM_TCP_PCB: the number of simulatenously active TCP
   connections. */
#ifndef MEMP_NUM_TCP_PCB
#define MEMP_NUM_TCP_PCB                5
#endif
/* MEMP_NUM_TCP_PCB_LISTEN: the number of listening TCP
   connections. */
#ifndef MEMP_NUM_TCP_PCB_LISTEN
#define MEMP_NUM_TCP_PCB_LISTEN         8
#endif
/* MEMP_NUM_TCP_SEG: the number of simultaneously queued TCP
   segments. */
#ifndef MEMP_NUM_TCP_SEG
#define MEMP_NUM_TCP_SEG                16
#endif
/* MEMP_NUM_SYS_TIMEOUT: the number of simulateously active
   timeouts. */
#ifndef MEMP_NUM_SYS_TIMEOUT
#define MEMP_NUM_SYS_TIMEOUT            3
#endif

/* The following four are used only with the sequential API and can be
   set to 0 if the application only will use the raw API. */
/* MEMP_NUM_NETBUF: the number of struct netbufs. */
#ifndef MEMP_NUM_NETBUF
#define MEMP_NUM_NETBUF                 2
#endif
/* MEMP_NUM_NETCONN: the number of struct netconns. */
#ifndef MEMP_NUM_NETCONN
#define MEMP_NUM_NETCONN                4
#endif
/* MEMP_NUM_APIMSG: the number of struct api_msg, used for
   communication between the TCP/IP stack and the sequential
   programs. */
#ifndef MEMP_NUM_API_MSG
#define MEMP_NUM_API_MSG                8
#endif
/* MEMP_NUM_TCPIPMSG: the number of struct tcpip_msg, which is used
   for sequential API communication and incoming packets. Used in
   src/api/tcpip.c. */
#ifndef MEMP_NUM_TCPIP_MSG
#define MEMP_NUM_TCPIP_MSG              8
#endif

/* ---------- Pbuf options ---------- */
/* PBUF_POOL_SIZE: the number of buffers in the pbuf pool. */

#ifndef PBUF_POOL_SIZE
#define PBUF_POOL_SIZE                  16
#endif

/* PBUF_POOL_BUFSIZE: the size of each pbuf in the pbuf pool. */

#ifndef PBUF_POOL_BUFSIZE
#define PBUF_POOL_BUFSIZE               128
#endif

/* PBUF_LINK_HLEN: the number of bytes that should be allocated for a
   link level header. Defaults to 14 for Ethernet. */

#ifndef PBUF_LINK_HLEN
#define PBUF_LINK_HLEN                  14
#endif

/* ---------- ARP options ---------- */

/** Number of active hardware address, IP address pairs cached */
#ifndef ARP_TABLE_SIZE
#define ARP_TABLE_SIZE                  10
#endif

/**
 * If enabled, outgoing packets are queued during hardware address
 * resolution.
 *
 * This feature has not stabilized yet. Single-packet queueing is
 * believed to be stable, multi-packet queueing is believed to
 * clash with the TCP segment queueing.
 *
 * As multi-packet-queueing is currently disabled, enabling this
 * _should_ work, but we need your testing feedback on lwip-users.
 *
 */
#ifndef ARP_QUEUEING
#define ARP_QUEUEING                    1
#endif

/* This option is deprecated */
#ifdef ETHARP_QUEUE_FIRST
#error ETHARP_QUEUE_FIRST option is deprecated. Remove it from your lwipopts.h.
#endif

/* This option is removed to comply with the ARP standard */
#ifdef ETHARP_ALWAYS_INSERT
#error ETHARP_ALWAYS_INSERT option is deprecated. Remove it from your lwipopts.h.
#endif

/* ---------- IP options ---------- */
/* Define IP_FORWARD to 1 if you wish to have the ability to forward
   IP packets across network interfaces. If you are going to run lwIP
   on a device with only one network interface, define this to 0. */
#ifndef IP_FORWARD
#define IP_FORWARD                      0
#endif

/* If defined to 1, IP options are allowed (but not parsed). If
   defined to 0, all packets with IP options are dropped. */
#ifndef IP_OPTIONS
#define IP_OPTIONS                      1
#endif

/** IP reassembly and segmentation. Even if they both deal with IP
 *  fragments, note that these are orthogonal, one dealing with incoming
 *  packets, the other with outgoing packets
 */

/** Reassemble incoming fragmented IP packets */
#ifndef IP_REASSEMBLY
#define IP_REASSEMBLY                   1
#endif

/** Fragment outgoing IP packets if their size exceeds MTU */
#ifndef IP_FRAG
#define IP_FRAG                         1
#endif

/* ---------- ICMP options ---------- */

#ifndef ICMP_TTL
#define ICMP_TTL                        255
#endif

/* ---------- RAW options ---------- */

#ifndef LWIP_RAW
#define LWIP_RAW                        1
#endif

#ifndef RAW_TTL
#define RAW_TTL                        255
#endif

/* ---------- DHCP options ---------- */

#ifndef LWIP_DHCP
#define LWIP_DHCP                       0
#endif

/* 1 if you want to do an ARP check on the offered address
   (recommended). */
#ifndef DHCP_DOES_ARP_CHECK
#define DHCP_DOES_ARP_CHECK             1
#endif

/* ---------- UDP options ---------- */
#ifndef LWIP_UDP
#define LWIP_UDP                        1
#endif

#ifndef UDP_TTL
#define UDP_TTL                         255
#endif

/* ---------- TCP options ---------- */
#ifndef LWIP_TCP
#define LWIP_TCP                        1
#endif

#ifndef TCP_TTL
#define TCP_TTL                         255
#endif

#ifndef TCP_WND
#define TCP_WND                         2048
#endif

#ifndef TCP_MAXRTX
#define TCP_MAXRTX                      12
#endif

#ifndef TCP_SYNMAXRTX
#define TCP_SYNMAXRTX                   6
#endif

/* Controls if TCP should queue segments that arrive out of
   order. Define to 0 if your device is low on memory. */
#ifndef TCP_QUEUE_OOSEQ
#define TCP_QUEUE_OOSEQ                 1
#endif

/* TCP Maximum segment size. */
#ifndef TCP_MSS
#define TCP_MSS                         128 /* A *very* conservative default. */
#endif

/* TCP sender buffer space (bytes). */
#ifndef TCP_SND_BUF
#define TCP_SND_BUF                     256
#endif

/* TCP sender buffer space (pbufs). This must be at least = 2 *
   TCP_SND_BUF/TCP_MSS for things to work. */
#ifndef TCP_SND_QUEUELEN
#define TCP_SND_QUEUELEN                4 * TCP_SND_BUF/TCP_MSS
#endif

/* Maximum number of retransmissions of data segments. */

/* Maximum number of retransmissions of SYN segments. */

/* TCP writable space (bytes). This must be less than or equal
   to TCP_SND_BUF. It is the amount of space which must be
   available in the tcp snd_buf for select to return writable */
#ifndef TCP_SNDLOWAT
#define TCP_SNDLOWAT                    TCP_SND_BUF/2
#endif

/* Support loop interface (127.0.0.1) */
#ifndef LWIP_HAVE_LOOPIF
#define LWIP_HAVE_LOOPIF		1
#endif

#ifndef LWIP_EVENT_API
#define LWIP_EVENT_API                  0
#define LWIP_CALLBACK_API               1
#else
#define LWIP_EVENT_API                  1
#define LWIP_CALLBACK_API               0
#endif

#ifndef LWIP_COMPAT_SOCKETS
#define LWIP_COMPAT_SOCKETS             1
#endif

#ifndef TCPIP_THREAD_PRIO
#define TCPIP_THREAD_PRIO               1
#endif

#ifndef SLIPIF_THREAD_PRIO
#define SLIPIF_THREAD_PRIO              1
#endif

#ifndef PPP_THREAD_PRIO
#define PPP_THREAD_PRIO                 1
#endif

#ifndef DEFAULT_THREAD_PRIO
#define DEFAULT_THREAD_PRIO             1
#endif

/* ---------- Socket Options ---------- */
/* Enable SO_REUSEADDR and SO_REUSEPORT options */
#ifndef SO_REUSE
# define SO_REUSE 0
#endif

/* ---------- Statistics options ---------- */
#ifndef LWIP_STATS
#define LWIP_STATS                      1
#endif

#if LWIP_STATS

#ifndef LWIP_STATS_DISPLAY
#define LWIP_STATS_DISPLAY 0
#endif

#ifndef LINK_STATS
#define LINK_STATS	1
#endif

#ifndef IP_STATS
#define IP_STATS	1
#endif

#ifndef IPFRAG_STATS
#define IPFRAG_STATS	1
#endif

#ifndef ICMP_STATS
#define ICMP_STATS	1
#endif

#ifndef UDP_STATS
#define UDP_STATS	1
#endif

#ifndef TCP_STATS
#define TCP_STATS	1
#endif

#ifndef MEM_STATS
#define MEM_STATS	1
#endif

#ifndef MEMP_STATS
#define MEMP_STATS	1
#endif

#ifndef PBUF_STATS
#define PBUF_STATS	1
#endif

#ifndef SYS_STATS
#define SYS_STATS	1
#endif

#ifndef RAW_STATS
#define RAW_STATS	0
#endif

#else

#define LINK_STATS	0
#define IP_STATS	0
#define IPFRAG_STATS	0
#define ICMP_STATS	0
#define UDP_STATS	0
#define TCP_STATS	0
#define MEM_STATS	0
#define MEMP_STATS	0
#define PBUF_STATS	0
#define SYS_STATS	0
#define RAW_STATS	0
#define LWIP_STATS_DISPLAY	0

#endif /* LWIP_STATS */

/* ---------- PPP options ---------- */

#ifndef PPP_SUPPORT
#define PPP_SUPPORT                     0      /* Set for PPP */
#endif

#if PPP_SUPPORT

#define NUM_PPP                         1      /* Max PPP sessions. */

#ifndef PAP_SUPPORT
#define PAP_SUPPORT                     0      /* Set for PAP. */
#endif

#ifndef CHAP_SUPPORT
#define CHAP_SUPPORT                    0      /* Set for CHAP. */
#endif

#define MSCHAP_SUPPORT                  0      /* Set for MSCHAP (NOT FUNCTIONAL!) */
#define CBCP_SUPPORT                    0      /* Set for CBCP (NOT FUNCTIONAL!) */
#define CCP_SUPPORT                     0      /* Set for CCP (NOT FUNCTIONAL!) */

#ifndef VJ_SUPPORT
#define VJ_SUPPORT                      0      /* Set for VJ header compression. */
#endif

#ifndef MD5_SUPPORT
#define MD5_SUPPORT                     0      /* Set for MD5 (see also CHAP) */
#endif

/*
 * Timeouts.
 */
#define FSM_DEFTIMEOUT                  6       /* Timeout time in seconds */
#define FSM_DEFMAXTERMREQS              2       /* Maximum Terminate-Request transmissions */
#define FSM_DEFMAXCONFREQS              10      /* Maximum Configure-Request transmissions */
#define FSM_DEFMAXNAKLOOPS              5       /* Maximum number of nak loops */

#define UPAP_DEFTIMEOUT                 6       /* Timeout (seconds) for retransmitting req */
#define UPAP_DEFREQTIME                 30      /* Time to wait for auth-req from peer */

#define CHAP_DEFTIMEOUT                 6       /* Timeout time in seconds */
#define CHAP_DEFTRANSMITS               10      /* max # times to send challenge */

/* Interval in seconds between keepalive echo requests, 0 to disable. */
#if 1
#define LCP_ECHOINTERVAL                0
#else
#define LCP_ECHOINTERVAL                10
#endif

/* Number of unanswered echo requests before failure. */
#define LCP_MAXECHOFAILS                3

/* Max Xmit idle time (in jiffies) before resend flag char. */
#define PPP_MAXIDLEFLAG                 100

/*
 * Packet sizes
 *
 * Note - lcp shouldn't be allowed to negotiate stuff outside these
 *    limits.  See lcp.h in the pppd directory.
 * (XXX - these constants should simply be shared by lcp.c instead
 *    of living in lcp.h)
 */
#define PPP_MTU                         1500     /* Default MTU (size of Info field) */
#if 0
#define PPP_MAXMTU  65535 - (PPP_HDRLEN + PPP_FCSLEN)
#else
#define PPP_MAXMTU                      1500 /* Largest MTU we allow */
#endif
#define PPP_MINMTU                      64
#define PPP_MRU                         1500     /* default MRU = max length of info field */
#define PPP_MAXMRU                      1500     /* Largest MRU we allow */
#define PPP_DEFMRU                      296             /* Try for this */
#define PPP_MINMRU                      128             /* No MRUs below this */

#define MAXNAMELEN                      256     /* max length of hostname or name for auth */
#define MAXSECRETLEN                    256     /* max length of password or secret */

#endif /* PPP_SUPPORT */

/* checksum options - set to zero for hardware checksum support */

#ifndef CHECKSUM_GEN_IP
#define CHECKSUM_GEN_IP                 1
#endif

#ifndef CHECKSUM_GEN_UDP
#define CHECKSUM_GEN_UDP                1
#endif

#ifndef CHECKSUM_GEN_TCP
#define CHECKSUM_GEN_TCP                1
#endif

#ifndef CHECKSUM_CHECK_IP
#define CHECKSUM_CHECK_IP               1
#endif

#ifndef CHECKSUM_CHECK_UDP
#define CHECKSUM_CHECK_UDP              1
#endif

#ifndef CHECKSUM_CHECK_TCP
#define CHECKSUM_CHECK_TCP              1
#endif

/* Debugging options all default to off */

#ifndef DBG_TYPES_ON
#define DBG_TYPES_ON                    0
#endif

#ifndef ETHARP_DEBUG
#define ETHARP_DEBUG                    DBG_OFF
#endif

#ifndef NETIF_DEBUG
#define NETIF_DEBUG                     DBG_OFF
#endif

#ifndef PBUF_DEBUG
#define PBUF_DEBUG                      DBG_OFF
#endif

#ifndef API_LIB_DEBUG
#define API_LIB_DEBUG                   DBG_OFF
#endif

#ifndef API_MSG_DEBUG
#define API_MSG_DEBUG                   DBG_OFF
#endif

#ifndef SOCKETS_DEBUG
#define SOCKETS_DEBUG                   DBG_OFF
#endif

#ifndef ICMP_DEBUG
#define ICMP_DEBUG                      DBG_OFF
#endif

#ifndef INET_DEBUG
#define INET_DEBUG                      DBG_OFF
#endif

#ifndef IP_DEBUG
#define IP_DEBUG                        DBG_OFF
#endif

#ifndef IP_REASS_DEBUG
#define IP_REASS_DEBUG                  DBG_OFF
#endif

#ifndef RAW_DEBUG
#define RAW_DEBUG                       DBG_OFF
#endif

#ifndef MEM_DEBUG
#define MEM_DEBUG                       DBG_OFF
#endif

#ifndef MEMP_DEBUG
#define MEMP_DEBUG                      DBG_OFF
#endif

#ifndef SYS_DEBUG
#define SYS_DEBUG                       DBG_OFF
#endif

#ifndef TCP_DEBUG
#define TCP_DEBUG                       DBG_OFF
#endif

#ifndef TCP_INPUT_DEBUG
#define TCP_INPUT_DEBUG                 DBG_OFF
#endif

#ifndef TCP_FR_DEBUG
#define TCP_FR_DEBUG                    DBG_OFF
#endif

#ifndef TCP_RTO_DEBUG
#define TCP_RTO_DEBUG                   DBG_OFF
#endif

#ifndef TCP_REXMIT_DEBUG
#define TCP_REXMIT_DEBUG                DBG_OFF
#endif

#ifndef TCP_CWND_DEBUG
#define TCP_CWND_DEBUG                  DBG_OFF
#endif

#ifndef TCP_WND_DEBUG
#define TCP_WND_DEBUG                   DBG_OFF
#endif

#ifndef TCP_OUTPUT_DEBUG
#define TCP_OUTPUT_DEBUG                DBG_OFF
#endif

#ifndef TCP_RST_DEBUG
#define TCP_RST_DEBUG                   DBG_OFF
#endif

#ifndef TCP_QLEN_DEBUG
#define TCP_QLEN_DEBUG                  DBG_OFF
#endif

#ifndef UDP_DEBUG
#define UDP_DEBUG                       DBG_OFF
#endif

#ifndef TCPIP_DEBUG
#define TCPIP_DEBUG                     DBG_OFF
#endif

#ifndef PPP_DEBUG
#define PPP_DEBUG                       DBG_OFF
#endif

#ifndef SLIP_DEBUG
#define SLIP_DEBUG                      DBG_OFF
#endif

#ifndef DHCP_DEBUG
#define DHCP_DEBUG                      DBG_OFF
#endif

#ifndef DBG_MIN_LEVEL
#define DBG_MIN_LEVEL                   DBG_LEVEL_OFF
#endif

#endif /* __LWIP_OPT_H__ */
