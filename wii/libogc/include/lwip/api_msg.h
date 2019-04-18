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
#ifndef __LWIP_API_MSG_H__
#define __LWIP_API_MSG_H__

#include "lwip/opt.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"

#include "lwip/ip.h"

#include "lwip/udp.h"
#include "lwip/tcp.h"

#include "lwip/api.h"

enum apimsg_type {
	APIMSG_NEWCONN,
	APIMSG_DELCONN,
	APIMSG_BIND,
	APIMSG_CONNECT,
	APIMSG_DISCONNECT,
	APIMSG_LISTEN,
	APIMSG_ACCEPT,
	APIMSG_SEND,
	APIMSG_RECV,
	APIMSG_WRITE,
	APIMSG_CLOSE,
	APIMSG_MAX
};

struct apimsg_msg {
	struct netconn *conn;
	enum netconn_type type;
	union {
		struct pbuf *p;
		struct {
			struct ip_addr *ipaddr;
			u16 port;
		} bc;
		struct {
			void *dataptr;
			u32 len;
			u8 copy;
		} w;
		sys_mbox mbox;
		u16 len;
	} msg;
};

struct api_msg {
	enum apimsg_type type;
	struct apimsg_msg msg;
};

#endif /* __LWIP_API_MSG_H__ */
