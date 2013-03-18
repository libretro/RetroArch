/*
 * Copyright (C) 2009-2012 by Matthias Ringwald
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
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
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
 * Please inquire about commercial licensing options at btstack@ringwald.ch
 *
 */

/*
 *  btstack.c
 *
 *  Created by Matthias Ringwald on 7/1/09.
 *
 *  BTstack client API
 */

#include <btstack/btstack.h>

#include "l2cap.h"
#include "socket_connection.h"
#include <btstack/run_loop.h>

#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>

// static uint8_t hci_cmd_buffer[3+255]; // HCI Command Header + max payload
static uint8_t hci_cmd_buffer[HCI_ACL_BUFFER_SIZE]; // BTstack command packets are not size restricted

static connection_t *btstack_connection = NULL;

/** prototypes & dummy functions */
static void dummy_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){};
static int btstack_packet_handler(connection_t *connection, uint16_t packet_type, uint16_t channel, uint8_t *data, uint16_t size);

/** local globals :) */
static void (*client_packet_handler)(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) = dummy_handler;

static const char * daemon_tcp_address = NULL;
static uint16_t     daemon_tcp_port    = BTSTACK_PORT;

// optional: if called before bt_open, TCP socket is used instead of local unix socket
//           note: address is not copied and must be valid during bt_open
void bt_use_tcp(const char * address, uint16_t port){
    daemon_tcp_address = address;
    daemon_tcp_port    = port;
}

// init BTstack library
int bt_open(){

    socket_connection_register_packet_callback(btstack_packet_handler);

    // BTdaemon
    if (daemon_tcp_address) {
        btstack_connection = socket_connection_open_tcp(daemon_tcp_address,daemon_tcp_port);
    } else {
        btstack_connection = socket_connection_open_unix();
    }
    if (!btstack_connection) return -1;

    return 0;
}

// stop using BTstack library
int bt_close(){
    return socket_connection_close_tcp(btstack_connection);
}

// send hci cmd packet
int bt_send_cmd(const hci_cmd_t *cmd, ...){
    va_list argptr;
    va_start(argptr, cmd);
    uint16_t len = hci_create_cmd_internal(hci_cmd_buffer, cmd, argptr);
    va_end(argptr);
    socket_connection_send_packet(btstack_connection, HCI_COMMAND_DATA_PACKET, 0, hci_cmd_buffer, len);
    return 0;
}

int btstack_packet_handler(connection_t *connection, uint16_t packet_type, uint16_t channel, uint8_t *data, uint16_t size){
    // printf("BTstack client handler: packet type %u, data[0] %x\n", packet_type, data[0]);
    (*client_packet_handler)(packet_type, channel, data, size);
    return 0;
}

// register packet handler
btstack_packet_handler_t bt_register_packet_handler(btstack_packet_handler_t handler){
    btstack_packet_handler_t old_handler = client_packet_handler;
    client_packet_handler = handler;
    return old_handler;
}

void bt_send_l2cap(uint16_t source_cid, uint8_t *data, uint16_t len){
    // send
    socket_connection_send_packet(btstack_connection, L2CAP_DATA_PACKET, source_cid, data, len);
}

void bt_send_rfcomm(uint16_t rfcomm_cid, uint8_t *data, uint16_t len){
    // send
    socket_connection_send_packet(btstack_connection, RFCOMM_DATA_PACKET, rfcomm_cid, data, len);
}

void bt_send_acl(uint8_t * data, uint16_t len){
    // send
    socket_connection_send_packet(btstack_connection, HCI_ACL_DATA_PACKET, 0, data, len);
}
