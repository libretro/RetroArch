/*
 * Copyright (C) 2009 by Matthias Ringwald
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 *  btstack.h
 *
 *  Created by Matthias Ringwald on 7/1/09.
 *
 *  BTstack client API
 *  
 */

#pragma once

#include <btstack/hci_cmds.h>
#include <btstack/run_loop.h>
#include <btstack/utils.h>

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif
	
// Default TCP port for BTstack daemon
#define BTSTACK_PORT            13333

// UNIX domain socket for BTstack */
#define BTSTACK_UNIX            "/tmp/BTstack"

// packet handler
typedef void (*btstack_packet_handler_t) (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

// optional: if called before bt_open, TCP socket is used instead of local unix socket
//           note: address is not copied and must be valid during bt_open
void bt_use_tcp(const char * address, uint16_t port); 

// init BTstack library
int bt_open(void);

// stop using BTstack library
int bt_close(void);

// send hci cmd packet
int bt_send_cmd(const hci_cmd_t *cmd, ...);

// register packet handler -- channel only valid for l2cap and rfcomm packets
// @returns old packet handler
btstack_packet_handler_t bt_register_packet_handler(btstack_packet_handler_t handler);

void bt_send_acl(uint8_t * data, uint16_t len);

void bt_send_l2cap(uint16_t local_cid, uint8_t *data, uint16_t len);
void bt_send_rfcomm(uint16_t rfcom_cid, uint8_t *data, uint16_t len);

#if defined __cplusplus
}
#endif
