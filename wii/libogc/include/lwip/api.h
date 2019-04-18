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
#ifndef __LWIP_API_H__
#define __LWIP_API_H__

#include "lwip/opt.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"

#include "lwip/ip.h"

#include "lwip/raw.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"

#include "lwip/err.h"

#define NETCONN_NOCOPY 0x00
#define NETCONN_COPY   0x01

enum netconn_type {
  NETCONN_TCP,
  NETCONN_UDP,
  NETCONN_UDPLITE,
  NETCONN_UDPNOCHKSUM,
  NETCONN_RAW
};

enum netconn_state {
  NETCONN_NONE,
  NETCONN_WRITE,
  NETCONN_ACCEPT,
  NETCONN_RECV,
  NETCONN_CONNECT,
  NETCONN_CLOSE
};

enum netconn_evt {
  NETCONN_EVTRCVPLUS,
  NETCONN_EVTRCVMINUS,
  NETCONN_EVTSENDPLUS,
  NETCONN_EVTSENDMINUS
};

struct netbuf {
  struct pbuf *p, *ptr;
  struct ip_addr *fromaddr;
  u16_t fromport;
  err_t err;
};

struct netconn {
	enum netconn_type type;
	enum netconn_state state;
	union {
		struct tcp_pcb *tcp;
		struct udp_pcb *udp;
		struct raw_pcb *raw;
	} pcb;
	err_t err;
	sys_sem sem;
	sys_mbox mbox;
	sys_mbox recvmbox;
	sys_mbox acceptmbox;
	u16 recvavail;
	s32 socket;
	void (*callback)(struct netconn *,enum netconn_evt,u32);
};

#endif /* __LWIP_API_H__ */
