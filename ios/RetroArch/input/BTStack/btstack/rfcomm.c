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
 *  rfcomm.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h> // memcpy
#include <stdint.h>

#include <btstack/btstack.h>
#include <btstack/hci_cmds.h>
#include <btstack/utils.h>

#include <btstack/utils.h>
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"
#include "debug.h"
#include "rfcomm.h"

// workaround for missing PRIxPTR on mspgcc (16/20-bit MCU)
#ifndef PRIxPTR
#if defined(__MSP430X__)  &&  defined(__MSP430X_LARGE__)
#define PRIxPTR "lx"
#else
#define PRIxPTR "x"
#endif
#endif


// Control field values      bit no.       1 2 3 4 PF 6 7 8
#define BT_RFCOMM_SABM       0x3F       // 1 1 1 1  1 1 0 0
#define BT_RFCOMM_UA         0x73       // 1 1 0 0  1 1 1 0
#define BT_RFCOMM_DM         0x0F       // 1 1 1 1  0 0 0 0
#define BT_RFCOMM_DM_PF      0x1F		// 1 1 1 1  1 0 0 0
#define BT_RFCOMM_DISC       0x53       // 1 1 0 0  1 0 1 0
#define BT_RFCOMM_UIH        0xEF       // 1 1 1 1  0 1 1 1
#define BT_RFCOMM_UIH_PF     0xFF       // 1 1 1 1  0 1 1 1

// Multiplexer message types 
#define BT_RFCOMM_CLD_CMD    0xC3
#define BT_RFCOMM_FCON_CMD   0xA3
#define BT_RFCOMM_FCON_RSP   0xA1
#define BT_RFCOMM_FCOFF_CMD  0x63
#define BT_RFCOMM_FCOFF_RSP  0x61
#define BT_RFCOMM_MSC_CMD    0xE3
#define BT_RFCOMM_MSC_RSP    0xE1
#define BT_RFCOMM_NSC_RSP    0x11
#define BT_RFCOMM_PN_CMD     0x83
#define BT_RFCOMM_PN_RSP     0x81
#define BT_RFCOMM_RLS_CMD    0x53
#define BT_RFCOMM_RLS_RSP    0x51
#define BT_RFCOMM_RPN_CMD    0x93
#define BT_RFCOMM_RPN_RSP    0x91
#define BT_RFCOMM_TEST_CMD   0x23
#define BT_RFCOMM_TEST_RSP   0x21

#define RFCOMM_MULIPLEXER_TIMEOUT_MS 60000

// FCS calc 
#define BT_RFCOMM_CODE_WORD         0xE0 // pol = x8+x2+x1+1
#define BT_RFCOMM_CRC_CHECK_LEN     3
#define BT_RFCOMM_UIHCRC_CHECK_LEN  2

#include "l2cap.h"

// used for debugging
// #define RFCOMM_LOG_CREDITS

// global rfcomm data
static uint16_t      rfcomm_client_cid_generator;  // used for client channel IDs

// linked lists for all
static linked_list_t rfcomm_multiplexers = NULL;
static linked_list_t rfcomm_channels = NULL;
static linked_list_t rfcomm_services = NULL;

static void (*app_packet_handler)(void * connection, uint8_t packet_type,
                                  uint16_t channel, uint8_t *packet, uint16_t size);

static void rfcomm_run(void);
static void rfcomm_hand_out_credits(void);
static void rfcomm_channel_state_machine(rfcomm_channel_t *channel, rfcomm_channel_event_t *event);
static void rfcomm_channel_state_machine_2(rfcomm_multiplexer_t * multiplexer, uint8_t dlci, rfcomm_channel_event_t *event);
static int rfcomm_channel_ready_for_open(rfcomm_channel_t *channel);
static void rfcomm_multiplexer_state_machine(rfcomm_multiplexer_t * multiplexer, RFCOMM_MULTIPLEXER_EVENT event);


// MARK: RFCOMM CLIENT EVENTS

// data: event (8), len(8), address(48), channel (8), rfcomm_cid (16)
static void rfcomm_emit_connection_request(rfcomm_channel_t *channel) {
    log_info("RFCOMM_EVENT_INCOMING_CONNECTION addr %s channel #%u cid 0x%02x",
             bd_addr_to_str(channel->multiplexer->remote_addr), channel->dlci>>1, channel->rfcomm_cid);
    uint8_t event[11];
    event[0] = RFCOMM_EVENT_INCOMING_CONNECTION;
    event[1] = sizeof(event) - 2;
    bt_flip_addr(&event[2], channel->multiplexer->remote_addr);
    event[8] = channel->dlci >> 1;
    bt_store_16(event, 9, channel->rfcomm_cid);
    hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
	(*app_packet_handler)(channel->connection, HCI_EVENT_PACKET, 0, (uint8_t *) event, sizeof(event));
}

// API Change: BTstack-0.3.50x uses
// data: event(8), len(8), status (8), address (48), server channel(8), rfcomm_cid(16), max frame size(16)
// next Cydia release will use SVN version of this
// data: event(8), len(8), status (8), address (48), handle (16), server channel(8), rfcomm_cid(16), max frame size(16)
static void rfcomm_emit_channel_opened(rfcomm_channel_t *channel, uint8_t status) {
    log_info("RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE status 0x%x addr %s handle 0x%x channel #%u cid 0x%02x mtu %u",
             status, bd_addr_to_str(channel->multiplexer->remote_addr), channel->multiplexer->con_handle,
             channel->dlci>>1, channel->rfcomm_cid, channel->max_frame_size);
    uint8_t event[16];
    uint8_t pos = 0;
    event[pos++] = RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = status;
    bt_flip_addr(&event[pos], channel->multiplexer->remote_addr); pos += 6;
    bt_store_16(event,  pos, channel->multiplexer->con_handle);   pos += 2;
	event[pos++] = channel->dlci >> 1;
	bt_store_16(event, pos, channel->rfcomm_cid); pos += 2;       // channel ID
	bt_store_16(event, pos, channel->max_frame_size); pos += 2;   // max frame size
    hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
	(*app_packet_handler)(channel->connection, HCI_EVENT_PACKET, 0, (uint8_t *) event, pos);
}

static void rfcomm_emit_channel_open_failed_outgoing_memory(void * connection, bd_addr_t *addr, uint8_t server_channel){
    log_info("RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE BTSTACK_MEMORY_ALLOC_FAILED addr %s",
             bd_addr_to_str(*addr));
    uint8_t event[16];
    uint8_t pos = 0;
    event[pos++] = RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = BTSTACK_MEMORY_ALLOC_FAILED;
    bt_flip_addr(&event[pos], *addr); pos += 6;
    bt_store_16(event,  pos, 0);   pos += 2;
	event[pos++] = server_channel;
	bt_store_16(event, pos, 0); pos += 2;   // channel ID
	bt_store_16(event, pos, 0); pos += 2;   // max frame size
    hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
	(*app_packet_handler)(connection, HCI_EVENT_PACKET, 0, (uint8_t *) event, pos);
}

// data: event(8), len(8), creidts incoming(8), new credits incoming(8), credits outgoing(8)
static inline void rfcomm_emit_credit_status(rfcomm_channel_t * channel) {
#ifdef RFCOMM_LOG_CREDITS
    log_info("RFCOMM_LOG_CREDITS incoming %u new_incoming %u outgoing %u", channel->credits_incoming, channel->new_credits_incoming, channel->credits_outgoing);
    uint8_t event[5];
    event[0] = 0x88;
    event[1] = sizeof(event) - 2;
    event[2] = channel->credits_incoming;
    event[3] = channel->new_credits_incoming;
    event[4] = channel->credits_outgoing;
    hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
#endif
}

// data: event(8), len(8), rfcomm_cid(16)
static void rfcomm_emit_channel_closed(rfcomm_channel_t * channel) {
    log_info("RFCOMM_EVENT_CHANNEL_CLOSED cid 0x%02x", channel->rfcomm_cid);
    uint8_t event[4];
    event[0] = RFCOMM_EVENT_CHANNEL_CLOSED;
    event[1] = sizeof(event) - 2;
    bt_store_16(event, 2, channel->rfcomm_cid);
    hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
	(*app_packet_handler)(channel->connection, HCI_EVENT_PACKET, 0, (uint8_t *) event, sizeof(event));
}

static void rfcomm_emit_credits(rfcomm_channel_t * channel, uint8_t credits) {
    log_info("RFCOMM_EVENT_CREDITS cid 0x%02x credits %u", channel->rfcomm_cid, credits);
    uint8_t event[5];
    event[0] = RFCOMM_EVENT_CREDITS;
    event[1] = sizeof(event) - 2;
    bt_store_16(event, 2, channel->rfcomm_cid);
    event[4] = credits;
    hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
	(*app_packet_handler)(channel->connection, HCI_EVENT_PACKET, 0, (uint8_t *) event, sizeof(event));
}

static void rfcomm_emit_service_registered(void *connection, uint8_t status, uint8_t channel){
    log_info("RFCOMM_EVENT_SERVICE_REGISTERED status 0x%x channel #%u", status, channel);
    uint8_t event[4];
    event[0] = RFCOMM_EVENT_SERVICE_REGISTERED;
    event[1] = sizeof(event) - 2;
    event[2] = status;
    event[3] = channel;
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
	(*app_packet_handler)(connection, HCI_EVENT_PACKET, 0, (uint8_t *) event, sizeof(event));
}

// MARK: RFCOMM MULTIPLEXER HELPER

static uint16_t rfcomm_max_frame_size_for_l2cap_mtu(uint16_t l2cap_mtu){

    // Assume RFCOMM header with credits and single byte length field
    uint16_t max_frame_size = l2cap_mtu - 5;
    
    // single byte can denote len up to 127
    if (max_frame_size > 127) {
        max_frame_size--;
    }

    log_info("rfcomm_max_frame_size_for_l2cap_mtu:  %u -> %u\n", l2cap_mtu, max_frame_size);
    return max_frame_size;
}

static void rfcomm_multiplexer_initialize(rfcomm_multiplexer_t *multiplexer){

    memset(multiplexer, 0, sizeof(rfcomm_multiplexer_t));

    multiplexer->state = RFCOMM_MULTIPLEXER_CLOSED;
    multiplexer->l2cap_credits = 0;
    multiplexer->send_dm_for_dlci = 0;
    multiplexer->max_frame_size = rfcomm_max_frame_size_for_l2cap_mtu(l2cap_max_mtu());
}

static rfcomm_multiplexer_t * rfcomm_multiplexer_create_for_addr(bd_addr_t *addr){
    
    // alloc structure 
    rfcomm_multiplexer_t * multiplexer = btstack_memory_rfcomm_multiplexer_get();
    if (!multiplexer) return NULL;
    
    // fill in 
    rfcomm_multiplexer_initialize(multiplexer);
    BD_ADDR_COPY(&multiplexer->remote_addr, addr);

    // add to services list
    linked_list_add(&rfcomm_multiplexers, (linked_item_t *) multiplexer);
    
    return multiplexer;
}

static rfcomm_multiplexer_t * rfcomm_multiplexer_for_addr(bd_addr_t *addr){
    linked_item_t *it;
    for (it = (linked_item_t *) rfcomm_multiplexers; it ; it = it->next){
        rfcomm_multiplexer_t * multiplexer = ((rfcomm_multiplexer_t *) it);
        if (BD_ADDR_CMP(addr, multiplexer->remote_addr) == 0) {
            return multiplexer;
        };
    }
    return NULL;
}

static rfcomm_multiplexer_t * rfcomm_multiplexer_for_l2cap_cid(uint16_t l2cap_cid) {
    linked_item_t *it;
    for (it = (linked_item_t *) rfcomm_multiplexers; it ; it = it->next){
        rfcomm_multiplexer_t * multiplexer = ((rfcomm_multiplexer_t *) it);
        if (multiplexer->l2cap_cid == l2cap_cid) {
            return multiplexer;
        };
    }
    return NULL;
}

static int rfcomm_multiplexer_has_channels(rfcomm_multiplexer_t * multiplexer){
    linked_item_t *it;
    for (it = (linked_item_t *) rfcomm_channels; it ; it = it->next){
        rfcomm_channel_t * channel = ((rfcomm_channel_t *) it);
        if (channel->multiplexer == multiplexer) {
            return 1;
        }
    }
    return 0;
}

// MARK: RFCOMM CHANNEL HELPER

static void rfcomm_dump_channels(void){
#ifndef EMBEDDED
    linked_item_t * it;
    int channels = 0;
    for (it = (linked_item_t *) rfcomm_channels; it ; it = it->next){
        rfcomm_channel_t * channel = (rfcomm_channel_t *) it;
        log_info("Channel #%u: addr %p, state %u\n", channels, channel, channel->state);
        channels++;
    }
#endif
}

static void rfcomm_channel_initialize(rfcomm_channel_t *channel, rfcomm_multiplexer_t *multiplexer, 
                               rfcomm_service_t *service, uint8_t server_channel){
    
    // don't use 0 as channel id
    if (rfcomm_client_cid_generator == 0) ++rfcomm_client_cid_generator;
    
    // setup channel
    memset(channel, 0, sizeof(rfcomm_channel_t));
    
    channel->state             = RFCOMM_CHANNEL_CLOSED;
    channel->state_var         = RFCOMM_CHANNEL_STATE_VAR_NONE;
    
    channel->multiplexer      = multiplexer;
    channel->service          = service;
    channel->rfcomm_cid       = rfcomm_client_cid_generator++;
    channel->max_frame_size   = multiplexer->max_frame_size;

    channel->credits_incoming = 0;
    channel->credits_outgoing = 0;
    channel->packets_granted  = 0;

    // incoming flow control not active
    channel->new_credits_incoming  = 0x30;
    channel->incoming_flow_control = 0;
    
	if (service) {
		// incoming connection
		channel->outgoing = 0;
		channel->dlci = (server_channel << 1) |  multiplexer->outgoing;
        if (channel->max_frame_size > service->max_frame_size) {
            channel->max_frame_size = service->max_frame_size;
        }
        channel->incoming_flow_control = service->incoming_flow_control;
        channel->new_credits_incoming  = service->incoming_initial_credits;
	} else {
		// outgoing connection
		channel->outgoing = 1;
		channel->dlci = (server_channel << 1) | (multiplexer->outgoing ^ 1);
	}
}

// service == NULL -> outgoing channel
static rfcomm_channel_t * rfcomm_channel_create(rfcomm_multiplexer_t * multiplexer,
                                                rfcomm_service_t * service, uint8_t server_channel){

    log_info("rfcomm_channel_create for service %p, channel %u --- list of channels:\n", service, server_channel);
    rfcomm_dump_channels();

    // alloc structure 
    rfcomm_channel_t * channel = btstack_memory_rfcomm_channel_get();
    if (!channel) return NULL;
    
    // fill in 
    rfcomm_channel_initialize(channel, multiplexer, service, server_channel);
    
    // add to services list
    linked_list_add(&rfcomm_channels, (linked_item_t *) channel);
    
    return channel;
}

static rfcomm_channel_t * rfcomm_channel_for_rfcomm_cid(uint16_t rfcomm_cid){
    linked_item_t *it;
    for (it = (linked_item_t *) rfcomm_channels; it ; it = it->next){
        rfcomm_channel_t * channel = ((rfcomm_channel_t *) it);
        if (channel->rfcomm_cid == rfcomm_cid) {
            return channel;
        };
    }
    return NULL;
}

static rfcomm_channel_t * rfcomm_channel_for_multiplexer_and_dlci(rfcomm_multiplexer_t * multiplexer, uint8_t dlci){
    linked_item_t *it;
    for (it = (linked_item_t *) rfcomm_channels; it ; it = it->next){
        rfcomm_channel_t * channel = ((rfcomm_channel_t *) it);
        if (channel->dlci == dlci && channel->multiplexer == multiplexer) {
            return channel;
        };
    }
    return NULL;
}

static rfcomm_service_t * rfcomm_service_for_channel(uint8_t server_channel){
    linked_item_t *it;
    for (it = (linked_item_t *) rfcomm_services; it ; it = it->next){
        rfcomm_service_t * service = ((rfcomm_service_t *) it);
        if ( service->server_channel == server_channel){
            return service;
        };
    }
    return NULL;
}

// MARK: RFCOMM SEND

/**
 * @param credits - only used for RFCOMM flow control in UIH wiht P/F = 1
 */
static int rfcomm_send_packet_for_multiplexer(rfcomm_multiplexer_t *multiplexer, uint8_t address, uint8_t control, uint8_t credits, uint8_t *data, uint16_t len){

    if (!l2cap_can_send_packet_now(multiplexer->l2cap_cid)) return BTSTACK_ACL_BUFFERS_FULL;
    
    uint8_t * rfcomm_out_buffer = l2cap_get_outgoing_buffer();
    
	uint16_t pos = 0;
	uint8_t crc_fields = 3;
	
	rfcomm_out_buffer[pos++] = address;
	rfcomm_out_buffer[pos++] = control;
	
	// length field can be 1 or 2 octets
	if (len < 128){
		rfcomm_out_buffer[pos++] = (len << 1)| 1;     // bits 0-6
	} else {
		rfcomm_out_buffer[pos++] = (len & 0x7f) << 1; // bits 0-6
		rfcomm_out_buffer[pos++] = len >> 7;          // bits 7-14
		crc_fields++;
	}

	// add credits for UIH frames when PF bit is set
	if (control == BT_RFCOMM_UIH_PF){
		rfcomm_out_buffer[pos++] = credits;
	}
	
	// copy actual data
	if (len) {
		memcpy(&rfcomm_out_buffer[pos], data, len);
		pos += len;
	}
	
	// UIH frames only calc FCS over address + control (5.1.1)
	if ((control & 0xef) == BT_RFCOMM_UIH){
		crc_fields = 2;
	}
	rfcomm_out_buffer[pos++] =  crc8_calc(rfcomm_out_buffer, crc_fields); // calc fcs

    int credits_taken = 0;
    if (multiplexer->l2cap_credits){
        credits_taken++;
        multiplexer->l2cap_credits--;
    } else {
        log_info( "rfcomm_send_packet addr %02x, ctrl %02x size %u without l2cap credits\n", address, control, pos);
    }
    
    int err = l2cap_send_prepared(multiplexer->l2cap_cid, pos);
    
    if (err) {
        // undo credit counting
        multiplexer->l2cap_credits += credits_taken;
    }
    return err;
}

// C/R Flag in Address
// - terms: initiator = station that creates multiplexer with SABM
// - terms: responder = station that responds to multiplexer setup with UA
// "For SABM, UA, DM and DISC frames C/R bit is set according to Table 1 in GSM 07.10, section 5.2.1.2"
//    - command initiator = 1 /response responder = 1
//    - command responder = 0 /response initiator = 0
// "For UIH frames, the C/R bit is always set according to section 5.4.3.1 in GSM 07.10. 
//  This applies independently of what is contained wthin the UIH frames, either data or control messages."
//    - c/r = 1 for frames by initiating station, 0 = for frames by responding station

// C/R Flag in Message
// "In the message level, the C/R bit in the command type field is set as stated in section 5.4.6.2 in GSM 07.10." 
//   - If the C/R bit is set to 1 the message is a command
//   - if it is set to 0 the message is a response.

// temp/old messge construction

// new object oriented version
static int rfcomm_send_sabm(rfcomm_multiplexer_t *multiplexer, uint8_t dlci){
	uint8_t address = (1 << 0) | (multiplexer->outgoing << 1) | (dlci << 2);   // command
    return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_SABM, 0, NULL, 0);
}

static int rfcomm_send_disc(rfcomm_multiplexer_t *multiplexer, uint8_t dlci){
	uint8_t address = (1 << 0) | (multiplexer->outgoing << 1) | (dlci << 2);  // command
    return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_DISC, 0, NULL, 0);
}

static int rfcomm_send_ua(rfcomm_multiplexer_t *multiplexer, uint8_t dlci){
	uint8_t address = (1 << 0) | ((multiplexer->outgoing ^ 1) << 1) | (dlci << 2); // response
    return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UA, 0, NULL, 0);
}

static int rfcomm_send_dm_pf(rfcomm_multiplexer_t *multiplexer, uint8_t dlci){
	uint8_t address = (1 << 0) | ((multiplexer->outgoing ^ 1) << 1) | (dlci << 2); // response
    return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_DM_PF, 0, NULL, 0);
}

static int rfcomm_send_uih_msc_cmd(rfcomm_multiplexer_t *multiplexer, uint8_t dlci, uint8_t signals) {
	uint8_t address = (1 << 0) | (multiplexer->outgoing << 1);
	uint8_t payload[4]; 
	uint8_t pos = 0;
	payload[pos++] = BT_RFCOMM_MSC_CMD;
	payload[pos++] = 2 << 1 | 1;  // len
	payload[pos++] = (1 << 0) | (1 << 1) | (dlci << 2); // CMD => C/R = 1
	payload[pos++] = signals;
	return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UIH, 0, (uint8_t *) payload, pos);
}

static int rfcomm_send_uih_msc_rsp(rfcomm_multiplexer_t *multiplexer, uint8_t dlci, uint8_t signals) {
	uint8_t address = (1 << 0) | (multiplexer->outgoing<< 1);
	uint8_t payload[4]; 
	uint8_t pos = 0;
	payload[pos++] = BT_RFCOMM_MSC_RSP;
	payload[pos++] = 2 << 1 | 1;  // len
	payload[pos++] = (1 << 0) | (1 << 1) | (dlci << 2); // CMD => C/R = 1
	payload[pos++] = signals;
	return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UIH, 0, (uint8_t *) payload, pos);
}

static int rfcomm_send_uih_pn_command(rfcomm_multiplexer_t *multiplexer, uint8_t dlci, uint16_t max_frame_size){
	uint8_t payload[10];
	uint8_t address = (1 << 0) | (multiplexer->outgoing << 1); 
	uint8_t pos = 0;
	payload[pos++] = BT_RFCOMM_PN_CMD;
	payload[pos++] = 8 << 1 | 1;  // len
	payload[pos++] = dlci;
	payload[pos++] = 0xf0; // pre-defined for Bluetooth, see 5.5.3 of TS 07.10 Adaption for RFCOMM
	payload[pos++] = 0; // priority
	payload[pos++] = 0; // max 60 seconds ack
	payload[pos++] = max_frame_size & 0xff; // max framesize low
	payload[pos++] = max_frame_size >> 8;   // max framesize high
	payload[pos++] = 0x00; // number of retransmissions
	payload[pos++] = 0x00; // (unused error recovery window) initial number of credits
	return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UIH, 0, (uint8_t *) payload, pos);
}

// "The response may not change the DLCI, the priority, the convergence layer, or the timer value." RFCOMM-tutorial.pdf
static int rfcomm_send_uih_pn_response(rfcomm_multiplexer_t *multiplexer, uint8_t dlci,
                                       uint8_t priority, uint16_t max_frame_size){
	uint8_t payload[10];
	uint8_t address = (1 << 0) | (multiplexer->outgoing << 1); 
	uint8_t pos = 0;
	payload[pos++] = BT_RFCOMM_PN_RSP;
	payload[pos++] = 8 << 1 | 1;  // len
	payload[pos++] = dlci;
	payload[pos++] = 0xe0; // pre defined for Bluetooth, see 5.5.3 of TS 07.10 Adaption for RFCOMM
	payload[pos++] = priority; // priority
	payload[pos++] = 0; // max 60 seconds ack
	payload[pos++] = max_frame_size & 0xff; // max framesize low
	payload[pos++] = max_frame_size >> 8;   // max framesize high
	payload[pos++] = 0x00; // number of retransmissions
	payload[pos++] = 0x00; // (unused error recovery window) initial number of credits
	return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UIH, 0, (uint8_t *) payload, pos);
}

static int rfcomm_send_uih_rpn_rsp(rfcomm_multiplexer_t *multiplexer, uint8_t dlci, rfcomm_rpn_data_t *rpn_data) {
	uint8_t payload[10];
	uint8_t address = (1 << 0) | (multiplexer->outgoing << 1); 
	uint8_t pos = 0;
	payload[pos++] = BT_RFCOMM_RPN_RSP;
	payload[pos++] = 8 << 1 | 1;  // len
	payload[pos++] = (1 << 0) | (1 << 1) | (dlci << 2); // CMD => C/R = 1
	payload[pos++] = rpn_data->baud_rate;
	payload[pos++] = rpn_data->flags;
	payload[pos++] = rpn_data->flow_control;
	payload[pos++] = rpn_data->xon;
	payload[pos++] = rpn_data->xoff;
	payload[pos++] = rpn_data->parameter_mask_0;
	payload[pos++] = rpn_data->parameter_mask_1;
	return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UIH, 0, (uint8_t *) payload, pos);
}

static int rfcomm_send_uih_data(rfcomm_multiplexer_t *multiplexer, uint8_t dlci, uint8_t *data, uint16_t len){
	uint8_t address = (1 << 0) | (multiplexer->outgoing << 1) | (dlci << 2); 
    return rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UIH, 0, data, len);
}

static void rfcomm_send_uih_credits(rfcomm_multiplexer_t *multiplexer, uint8_t dlci,  uint8_t credits){
    uint8_t address = (1 << 0) | (multiplexer->outgoing << 1) |  (dlci << 2); 
    rfcomm_send_packet_for_multiplexer(multiplexer, address, BT_RFCOMM_UIH_PF, credits, NULL, 0);
}

// MARK: RFCOMM MULTIPLEXER
static void rfcomm_multiplexer_stop_timer(rfcomm_multiplexer_t * multiplexer){
    if (multiplexer->timer_active) {
        run_loop_remove_timer(&multiplexer->timer);
        multiplexer->timer_active = 0;
    }
}
static void rfcomm_multiplexer_free(rfcomm_multiplexer_t * multiplexer){
    linked_list_remove( &rfcomm_multiplexers, (linked_item_t *) multiplexer);
    btstack_memory_rfcomm_multiplexer_free(multiplexer);
}

static void rfcomm_multiplexer_finalize(rfcomm_multiplexer_t * multiplexer){
    
    // remove (potential) timer
    rfcomm_multiplexer_stop_timer(multiplexer);
    
    // close and remove all channels
    linked_item_t *it = (linked_item_t *) &rfcomm_channels;
    while (it->next){
        rfcomm_channel_t * channel = (rfcomm_channel_t *) it->next;
        if (channel->multiplexer == multiplexer) {
            // emit appropriate events
            if (channel->state == RFCOMM_CHANNEL_OPEN) {
                rfcomm_emit_channel_closed(channel);
            } else {
                rfcomm_emit_channel_opened(channel, RFCOMM_MULTIPLEXER_STOPPED); 
            }
            // remove from list
            it->next = it->next->next;
            // free channel struct
            btstack_memory_rfcomm_channel_free(channel);
        } else {
            it = it->next;
        }
    }
    
    // keep reference to l2cap channel
    uint16_t l2cap_cid = multiplexer->l2cap_cid;
    
    // remove mutliplexer
    rfcomm_multiplexer_free(multiplexer);
    
    // close l2cap multiplexer channel, too
    l2cap_disconnect_internal(l2cap_cid, 0x13);
}

static void rfcomm_multiplexer_timer_handler(timer_source_t *timer){
    rfcomm_multiplexer_t * multiplexer = (rfcomm_multiplexer_t *) linked_item_get_user( (linked_item_t *) timer);
    if (!rfcomm_multiplexer_has_channels(multiplexer)){
        log_info( "rfcomm_multiplexer_timer_handler timeout: shutting down multiplexer!\n");
        rfcomm_multiplexer_finalize(multiplexer);
    }
}

static void rfcomm_multiplexer_prepare_idle_timer(rfcomm_multiplexer_t * multiplexer){
    if (multiplexer->timer_active) {
        run_loop_remove_timer(&multiplexer->timer);
        multiplexer->timer_active = 0;
    }
    if (!rfcomm_multiplexer_has_channels(multiplexer)){
        // start timer for multiplexer timeout check
        run_loop_set_timer(&multiplexer->timer, RFCOMM_MULIPLEXER_TIMEOUT_MS);
        multiplexer->timer.process = rfcomm_multiplexer_timer_handler;
        linked_item_set_user((linked_item_t*) &multiplexer->timer, multiplexer);
        run_loop_add_timer(&multiplexer->timer);
        multiplexer->timer_active = 1;
    }
}

static void rfcomm_multiplexer_opened(rfcomm_multiplexer_t *multiplexer){
    log_info("Multiplexer up and running\n");
    multiplexer->state = RFCOMM_MULTIPLEXER_OPEN;
    
    rfcomm_channel_event_t event = { CH_EVT_MULTIPLEXER_READY };
    
    // transition of channels that wait for multiplexer 
    linked_item_t *it;
    for (it = (linked_item_t *) rfcomm_channels; it ; it = it->next){
        rfcomm_channel_t * channel = ((rfcomm_channel_t *) it);
        if (channel->multiplexer != multiplexer) continue;
        rfcomm_channel_state_machine(channel, &event);
    }        
    
    rfcomm_run();
    rfcomm_multiplexer_prepare_idle_timer(multiplexer);
}


/**
 * @return handled packet
 */
static int rfcomm_multiplexer_hci_event_handler(uint8_t *packet, uint16_t size){
    bd_addr_t event_addr;
    uint16_t  psm;
    uint16_t l2cap_cid;
    hci_con_handle_t con_handle;
    rfcomm_multiplexer_t *multiplexer = NULL;
    uint8_t status;
    
    switch (packet[0]) {
            
        // accept incoming PSM_RFCOMM connection if no multiplexer exists yet
        case L2CAP_EVENT_INCOMING_CONNECTION:
            // data: event(8), len(8), address(48), handle (16),  psm (16), source cid(16) dest cid(16)
            bt_flip_addr(event_addr, &packet[2]);
            con_handle = READ_BT_16(packet,  8);
            psm        = READ_BT_16(packet, 10); 
            l2cap_cid  = READ_BT_16(packet, 12); 

            if (psm != PSM_RFCOMM) break;

            multiplexer = rfcomm_multiplexer_for_addr(&event_addr);
            
            if (multiplexer) {
                log_info("INCOMING_CONNECTION (l2cap_cid 0x%02x) for PSM_RFCOMM => decline - multiplexer already exists", l2cap_cid);
                l2cap_decline_connection_internal(l2cap_cid,  0x04);    // no resources available
                return 1;
            }
            
            // create and inititialize new multiplexer instance (incoming)
            multiplexer = rfcomm_multiplexer_create_for_addr(&event_addr);
            if (!multiplexer){
                log_info("INCOMING_CONNECTION (l2cap_cid 0x%02x) for PSM_RFCOMM => decline - no memory left", l2cap_cid);
                l2cap_decline_connection_internal(l2cap_cid,  0x04);    // no resources available
                return 1;
            }
            
            multiplexer->con_handle = con_handle;
            multiplexer->l2cap_cid = l2cap_cid;
            multiplexer->state = RFCOMM_MULTIPLEXER_W4_SABM_0;
            
            log_info("L2CAP_EVENT_INCOMING_CONNECTION (l2cap_cid 0x%02x) for PSM_RFCOMM => accept", l2cap_cid);
            l2cap_accept_connection_internal(l2cap_cid);
            return 1;
            
        // l2cap connection opened -> store l2cap_cid, remote_addr
        case L2CAP_EVENT_CHANNEL_OPENED: 

            if (READ_BT_16(packet, 11) != PSM_RFCOMM) break;
            
            status = packet[2];
            log_info("L2CAP_EVENT_CHANNEL_OPENED for PSM_RFCOMM, status %u\n", status);
            
            // get multiplexer for remote addr
            con_handle = READ_BT_16(packet, 9);
            l2cap_cid = READ_BT_16(packet, 13);
            bt_flip_addr(event_addr, &packet[3]);
            multiplexer = rfcomm_multiplexer_for_addr(&event_addr);
            if (!multiplexer) {
                log_error("L2CAP_EVENT_CHANNEL_OPENED but no multiplexer prepared\n");
                return 1;
            }
            
            // on l2cap open error discard everything
            if (status){

                // remove (potential) timer
                rfcomm_multiplexer_stop_timer(multiplexer);

                // emit rfcomm_channel_opened with status and free channel
                linked_item_t * it = (linked_item_t *) &rfcomm_channels;
                while (it->next) {
                    rfcomm_channel_t * channel = (rfcomm_channel_t *) it->next;
                    if (channel->multiplexer == multiplexer){
                        rfcomm_emit_channel_opened(channel, status);
                        it->next = it->next->next;
                        btstack_memory_rfcomm_channel_free(channel);
                    } else {
                        it = it->next;
                    }
                }

                // free multiplexer
                rfcomm_multiplexer_free(multiplexer);
                return 1;
            }
            
            if (multiplexer->state == RFCOMM_MULTIPLEXER_W4_CONNECT) {
                log_info("L2CAP_EVENT_CHANNEL_OPENED: outgoing connection\n");
                // wrong remote addr
                if (BD_ADDR_CMP(event_addr, multiplexer->remote_addr)) break;
                multiplexer->l2cap_cid = l2cap_cid;
                multiplexer->con_handle = con_handle;
                // send SABM #0
                multiplexer->state = RFCOMM_MULTIPLEXER_SEND_SABM_0;
            } else { // multiplexer->state == RFCOMM_MULTIPLEXER_W4_SABM_0
                
                // set max frame size based on l2cap MTU
                multiplexer->max_frame_size = rfcomm_max_frame_size_for_l2cap_mtu(READ_BT_16(packet, 17));
            }
            return 1;
            
            // l2cap disconnect -> state = RFCOMM_MULTIPLEXER_CLOSED;
            
        case L2CAP_EVENT_CREDITS:
            // data: event(8), len(8), local_cid(16), credits(8)
            l2cap_cid = READ_BT_16(packet, 2);
            multiplexer = rfcomm_multiplexer_for_l2cap_cid(l2cap_cid);
            if (!multiplexer) break;
            multiplexer->l2cap_credits += packet[4];
            
            // log_info("L2CAP_EVENT_CREDITS: %u (now %u)\n", packet[4], multiplexer->l2cap_credits);

            // new credits, continue with signaling
            rfcomm_run();
            
            if (multiplexer->state != RFCOMM_MULTIPLEXER_OPEN) break;
            rfcomm_hand_out_credits();
            return 1;
        
        case DAEMON_EVENT_HCI_PACKET_SENT:
            // testing DMA done code
            rfcomm_run();
            break;
            
        case L2CAP_EVENT_CHANNEL_CLOSED:
            // data: event (8), len(8), channel (16)
            l2cap_cid = READ_BT_16(packet, 2);
            multiplexer = rfcomm_multiplexer_for_l2cap_cid(l2cap_cid);
            if (!multiplexer) break;
            switch (multiplexer->state) {
                case RFCOMM_MULTIPLEXER_W4_SABM_0:
                case RFCOMM_MULTIPLEXER_W4_UA_0:
                case RFCOMM_MULTIPLEXER_OPEN:
                    rfcomm_multiplexer_finalize(multiplexer);
                    return 1;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return 0;
}

static int rfcomm_multiplexer_l2cap_packet_handler(uint16_t channel, uint8_t *packet, uint16_t size){
    
    // get or create a multiplexer for a certain device
    rfcomm_multiplexer_t *multiplexer = rfcomm_multiplexer_for_l2cap_cid(channel);
    if (!multiplexer) return 0;
    
	// but only care for multiplexer control channel
    uint8_t frame_dlci = packet[0] >> 2;
    if (frame_dlci) return 0;
    const uint8_t length_offset = (packet[2] & 1) ^ 1;  // to be used for pos >= 3
    const uint8_t credit_offset = ((packet[1] & BT_RFCOMM_UIH_PF) == BT_RFCOMM_UIH_PF) ? 1 : 0;   // credits for uih_pf frames
    const uint8_t payload_offset = 3 + length_offset + credit_offset;
    switch (packet[1]){
            
        case BT_RFCOMM_SABM:
            if (multiplexer->state == RFCOMM_MULTIPLEXER_W4_SABM_0){
                log_info("Received SABM #0\n");
                multiplexer->outgoing = 0;
                multiplexer->state = RFCOMM_MULTIPLEXER_SEND_UA_0;
                return 1;
            }
            break;
            
        case BT_RFCOMM_UA:
            if (multiplexer->state == RFCOMM_MULTIPLEXER_W4_UA_0) {
                // UA #0 -> send UA #0, state = RFCOMM_MULTIPLEXER_OPEN
                log_info("Received UA #0 \n");
                rfcomm_multiplexer_opened(multiplexer);
                return 1;
            }
            break;
            
        case BT_RFCOMM_DISC:
            // DISC #0 -> send UA #0, close multiplexer
            log_info("Received DISC #0, (ougoing = %u)\n", multiplexer->outgoing);
            multiplexer->state = RFCOMM_MULTIPLEXER_SEND_UA_0_AND_DISC;
            return 1;
            
        case BT_RFCOMM_DM:
            // DM #0 - we shouldn't get this, just give up
            log_info("Received DM #0\n");
            log_info("-> Closing down multiplexer\n");
            rfcomm_multiplexer_finalize(multiplexer);
            return 1;
            
        case BT_RFCOMM_UIH:
            if (packet[payload_offset] == BT_RFCOMM_CLD_CMD){
                // Multiplexer close down (CLD) -> close mutliplexer
                log_info("Received Multiplexer close down command\n");
                log_info("-> Closing down multiplexer\n");
                rfcomm_multiplexer_finalize(multiplexer);
                return 1;
            }
            break;
            
        default:
            break;
            
    }
    return 0;
}

static void rfcomm_multiplexer_state_machine(rfcomm_multiplexer_t * multiplexer, RFCOMM_MULTIPLEXER_EVENT event){
    
    // process stored DM responses
    if (multiplexer->send_dm_for_dlci){
        rfcomm_send_dm_pf(multiplexer, multiplexer->send_dm_for_dlci);
        multiplexer->send_dm_for_dlci = 0;
    }

    switch (multiplexer->state) {
        case RFCOMM_MULTIPLEXER_SEND_SABM_0:
            switch (event) {
                case MULT_EV_READY_TO_SEND:
                    log_info("Sending SABM #0 - (multi 0x%p)\n", multiplexer);
                    multiplexer->state = RFCOMM_MULTIPLEXER_W4_UA_0;
                    rfcomm_send_sabm(multiplexer, 0);
                    break;
                default:
                    break;
            }
            break;
        case RFCOMM_MULTIPLEXER_SEND_UA_0:
            switch (event) {
                case MULT_EV_READY_TO_SEND:
                    log_info("Sending UA #0\n");
                    multiplexer->state = RFCOMM_MULTIPLEXER_OPEN;
                    rfcomm_send_ua(multiplexer, 0);
                    rfcomm_multiplexer_opened(multiplexer);
                    break;
                default:
                    break;
            }
            break;
        case RFCOMM_MULTIPLEXER_SEND_UA_0_AND_DISC:
            switch (event) {
                case MULT_EV_READY_TO_SEND:
                    log_info("Sending UA #0\n");
                    log_info("Closing down multiplexer\n");
                    multiplexer->state = RFCOMM_MULTIPLEXER_CLOSED;
                    rfcomm_send_ua(multiplexer, 0);
                    rfcomm_multiplexer_finalize(multiplexer);
                    // try to detect authentication errors: drop link key if multiplexer closed before first channel got opened
                    if (!multiplexer->at_least_one_connection){
                        log_info("TODO: no connections established - delete link key prophylactically\n");
                        // hci_send_cmd(&hci_delete_stored_link_key, multiplexer->remote_addr);
                    }
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

// MARK: RFCOMM CHANNEL

static void rfcomm_hand_out_credits(void){
    linked_item_t * it;
    for (it = (linked_item_t *) rfcomm_channels; it ; it = it->next){
        rfcomm_channel_t * channel = (rfcomm_channel_t *) it;
        if (channel->state != RFCOMM_CHANNEL_OPEN) {
            // log_info("RFCOMM_EVENT_CREDITS: multiplexer not open\n");
            continue;
        }
        if (channel->packets_granted) {
            // log_info("RFCOMM_EVENT_CREDITS: already packets granted\n");
            continue;
        }
        if (!channel->credits_outgoing) {
            // log_info("RFCOMM_EVENT_CREDITS: no outgoing credits\n");
            continue;
        }
        if (!channel->multiplexer->l2cap_credits){
            // log_info("RFCOMM_EVENT_CREDITS: no l2cap credits\n");
            continue;
        }
        // channel open, multiplexer has l2cap credits and we didn't hand out credit before -> go!
        // log_info("RFCOMM_EVENT_CREDITS: 1\n");
        channel->packets_granted += 1;
        rfcomm_emit_credits(channel, 1);
    }        
}

static void rfcomm_channel_send_credits(rfcomm_channel_t *channel, uint8_t credits){
    rfcomm_send_uih_credits(channel->multiplexer, channel->dlci, credits);
    channel->credits_incoming += credits;
    
    rfcomm_emit_credit_status(channel);
}

static void rfcomm_channel_opened(rfcomm_channel_t *rfChannel){
    
    log_info("rfcomm_channel_opened!\n");
    
    rfChannel->state = RFCOMM_CHANNEL_OPEN;
    rfcomm_emit_channel_opened(rfChannel, 0);
    rfcomm_hand_out_credits();

    // remove (potential) timer
    rfcomm_multiplexer_t *multiplexer = rfChannel->multiplexer;
    if (multiplexer->timer_active) {
        run_loop_remove_timer(&multiplexer->timer);
        multiplexer->timer_active = 0;
    }
    // hack for problem detecting authentication failure
    multiplexer->at_least_one_connection = 1;
    
    // start next connection request if pending
    rfcomm_run();
}

static void rfcomm_channel_packet_handler_uih(rfcomm_multiplexer_t *multiplexer, uint8_t * packet, uint16_t size){
    const uint8_t frame_dlci = packet[0] >> 2;
    const uint8_t length_offset = (packet[2] & 1) ^ 1;  // to be used for pos >= 3
    const uint8_t credit_offset = ((packet[1] & BT_RFCOMM_UIH_PF) == BT_RFCOMM_UIH_PF) ? 1 : 0;   // credits for uih_pf frames
    const uint8_t payload_offset = 3 + length_offset + credit_offset;

    rfcomm_channel_t * channel = rfcomm_channel_for_multiplexer_and_dlci(multiplexer, frame_dlci);
    if (!channel) return;
    
    // handle new outgoing credits
    if (packet[1] == BT_RFCOMM_UIH_PF) {
        
        // add them
        uint16_t new_credits = packet[3+length_offset];
        channel->credits_outgoing += new_credits;
        log_info( "RFCOMM data UIH_PF, new credits: %u, now %u\n", new_credits, channel->credits_outgoing);

        // notify channel statemachine 
        rfcomm_channel_event_t channel_event = { CH_EVT_RCVD_CREDITS };
        rfcomm_channel_state_machine(channel, &channel_event);
    }
    
    // contains payload?
    if (size - 1 > payload_offset){

        // log_info( "RFCOMM data UIH_PF, size %u, channel %p\n", size-payload_offset-1, rfChannel->connection);

        // decrease incoming credit counter
        if (channel->credits_incoming > 0){
            channel->credits_incoming--;
        }
        
        // deliver payload
        (*app_packet_handler)(channel->connection, RFCOMM_DATA_PACKET, channel->rfcomm_cid,
                              &packet[payload_offset], size-payload_offset-1);
    }
    
    // automatically provide new credits to remote device, if no incoming flow control
    if (!channel->incoming_flow_control && channel->credits_incoming < 5){
        channel->new_credits_incoming = 0x30;
    }    
    
    rfcomm_emit_credit_status(channel);
    
    // we received new RFCOMM credits, hand them out if possible
    rfcomm_hand_out_credits();
}

static void rfcomm_channel_accept_pn(rfcomm_channel_t *channel, rfcomm_channel_event_pn_t *event){
    // priority of client request
    channel->pn_priority = event->priority;
    
    // new credits
    channel->credits_outgoing = event->credits_outgoing;
    
    // negotiate max frame size
    if (channel->max_frame_size > channel->multiplexer->max_frame_size) {
        channel->max_frame_size = channel->multiplexer->max_frame_size;
    }
    if (channel->max_frame_size > event->max_frame_size) {
        channel->max_frame_size = event->max_frame_size;
    }
    
}

static void rfcomm_channel_finalize(rfcomm_channel_t *channel){

    rfcomm_multiplexer_t *multiplexer = channel->multiplexer;

    // remove from list
    linked_list_remove( &rfcomm_channels, (linked_item_t *) channel);

    // free channel
    btstack_memory_rfcomm_channel_free(channel);
    
    // update multiplexer timeout after channel was removed from list
    rfcomm_multiplexer_prepare_idle_timer(multiplexer);
}

static void rfcomm_channel_state_machine_2(rfcomm_multiplexer_t * multiplexer, uint8_t dlci, rfcomm_channel_event_t *event){

    // TODO: if client max frame size is smaller than RFCOMM_DEFAULT_SIZE, send PN

    
    // lookup existing channel
    rfcomm_channel_t * channel = rfcomm_channel_for_multiplexer_and_dlci(multiplexer, dlci);

    // log_info("rfcomm_channel_state_machine_2 lookup dlci #%u = 0x%08x - event %u\n", dlci, (int) channel, event->type);

    if (channel) {
        rfcomm_channel_state_machine(channel, event);
        return;
    }
    
    // service registered?
    rfcomm_service_t * service = rfcomm_service_for_channel(dlci >> 1);
    // log_info("rfcomm_channel_state_machine_2 service dlci #%u = 0x%08x\n", dlci, (int) service);
    if (!service) {
        // discard request by sending disconnected mode
        multiplexer->send_dm_for_dlci = dlci;
        return;
    }

    // create channel for some events
    switch (event->type) {
        case CH_EVT_RCVD_SABM:
        case CH_EVT_RCVD_PN:
        case CH_EVT_RCVD_RPN_REQ:
        case CH_EVT_RCVD_RPN_CMD:
            // setup incoming channel
            channel = rfcomm_channel_create(multiplexer, service, dlci >> 1);
            if (!channel){
                // discard request by sending disconnected mode
                multiplexer->send_dm_for_dlci = dlci;
            }
            break;
        default:
            break;
    }

    if (!channel) {
        // discard request by sending disconnected mode
        multiplexer->send_dm_for_dlci = dlci;
        return;
    }
    channel->connection = service->connection;
    rfcomm_channel_state_machine(channel, event);
}

void rfcomm_channel_packet_handler(rfcomm_multiplexer_t * multiplexer,  uint8_t *packet, uint16_t size){
    
    // rfcomm: (0) addr [76543 server channel] [2 direction: initiator uses 1] [1 C/R: CMD by initiator = 1] [0 EA=1]
    const uint8_t frame_dlci = packet[0] >> 2;
    uint8_t message_dlci; // used by commands in UIH(_PF) packets 
	uint8_t message_len;  //   "
    
    // rfcomm: (1) command/control
    // -- credits_offset = 1 if command == BT_RFCOMM_UIH_PF
    const uint8_t credit_offset = ((packet[1] & BT_RFCOMM_UIH_PF) == BT_RFCOMM_UIH_PF) ? 1 : 0;   // credits for uih_pf frames
    // rfcomm: (2) length. if bit 0 is cleared, 2 byte length is used. (little endian)
    const uint8_t length_offset = (packet[2] & 1) ^ 1;  // to be used for pos >= 3
    // rfcomm: (3+length_offset) credits if credits_offset == 1
    // rfcomm: (3+length_offest+credits_offset)
    const uint8_t payload_offset = 3 + length_offset + credit_offset;
    
    rfcomm_channel_event_t event;
    rfcomm_channel_event_pn_t event_pn;
    rfcomm_channel_event_rpn_t event_rpn;

    // switch by rfcomm message type
    switch(packet[1]) {
            
        case BT_RFCOMM_SABM:
            event.type = CH_EVT_RCVD_SABM;
            log_info("Received SABM #%u\n", frame_dlci);
            rfcomm_channel_state_machine_2(multiplexer, frame_dlci, &event);
            break;
            
        case BT_RFCOMM_UA:
            event.type = CH_EVT_RCVD_UA;
            log_info("Received UA #%u - channel opened\n",frame_dlci);
            rfcomm_channel_state_machine_2(multiplexer, frame_dlci, &event);
            break;
            
        case BT_RFCOMM_DISC:
            event.type = CH_EVT_RCVD_DISC;
            rfcomm_channel_state_machine_2(multiplexer, frame_dlci, &event);
            break;
            
        case BT_RFCOMM_DM:
        case BT_RFCOMM_DM_PF:
            event.type = CH_EVT_RCVD_DM;
            rfcomm_channel_state_machine_2(multiplexer, frame_dlci, &event);
            break;
            
        case BT_RFCOMM_UIH_PF:
        case BT_RFCOMM_UIH:

            message_len  = packet[payload_offset+1] >> 1;

            switch (packet[payload_offset]) {
                case BT_RFCOMM_PN_CMD:
                    message_dlci = packet[payload_offset+2];
                    event_pn.super.type = CH_EVT_RCVD_PN;
                    event_pn.priority = packet[payload_offset+4];
                    event_pn.max_frame_size = READ_BT_16(packet, payload_offset+6);
                    event_pn.credits_outgoing = packet[payload_offset+9];
                    log_info("Received UIH Parameter Negotiation Command for #%u\n", message_dlci);
                    rfcomm_channel_state_machine_2(multiplexer, message_dlci, (rfcomm_channel_event_t*) &event_pn);
                    break;
                    
                case BT_RFCOMM_PN_RSP:
                    message_dlci = packet[payload_offset+2];
                    event_pn.super.type = CH_EVT_RCVD_PN_RSP;
                    event_pn.priority = packet[payload_offset+4];
                    event_pn.max_frame_size = READ_BT_16(packet, payload_offset+6);
                    event_pn.credits_outgoing = packet[payload_offset+9];
                    log_info("UIH Parameter Negotiation Response max frame %u, credits %u\n",
                            event_pn.max_frame_size, event_pn.credits_outgoing);
                    rfcomm_channel_state_machine_2(multiplexer, message_dlci, (rfcomm_channel_event_t*) &event_pn);
                    break;
                    
                case BT_RFCOMM_MSC_CMD: 
                    message_dlci = packet[payload_offset+2] >> 2;
                    event.type = CH_EVT_RCVD_MSC_CMD;
                    log_info("Received MSC CMD for #%u, \n", message_dlci);
                    rfcomm_channel_state_machine_2(multiplexer, message_dlci, &event);
                    break;
                    
                case BT_RFCOMM_MSC_RSP:
                    message_dlci = packet[payload_offset+2] >> 2;
                    event.type = CH_EVT_RCVD_MSC_RSP;
                    log_info("Received MSC RSP for #%u\n", message_dlci);
                    rfcomm_channel_state_machine_2(multiplexer, message_dlci, &event);
                    break;
                    
                case BT_RFCOMM_RPN_CMD:
                    message_dlci = packet[payload_offset+2] >> 2;
                    switch (message_len){
                        case 1:
                            log_info("Received Remote Port Negotiation for #%u\n", message_dlci);
                            event.type = CH_EVT_RCVD_RPN_REQ;
                            rfcomm_channel_state_machine_2(multiplexer, message_dlci, &event);
                            break;
                        case 8:
                            log_info("Received Remote Port Negotiation (Info) for #%u\n", message_dlci);
                            event_rpn.super.type = CH_EVT_RCVD_RPN_CMD;
                            event_rpn.data.baud_rate = packet[payload_offset+3];
                            event_rpn.data.flags = packet[payload_offset+4];
                            event_rpn.data.flow_control = packet[payload_offset+5];
                            event_rpn.data.xon  = packet[payload_offset+6];
                            event_rpn.data.xoff = packet[payload_offset+7];
                            event_rpn.data.parameter_mask_0 = packet[payload_offset+8];
                            event_rpn.data.parameter_mask_1 = packet[payload_offset+9];
                            rfcomm_channel_state_machine_2(multiplexer, message_dlci, (rfcomm_channel_event_t*) &event_rpn);
                            break;
                        default:
                            break;
                    }
                    break;
                    
                default:
                    log_error("Received unknown UIH packet - 0x%02x\n", packet[payload_offset]); 
                    break;
            }
            break;
            
        default:
            log_error("Received unknown RFCOMM message type %x\n", packet[1]);
            break;
    }
    
    // trigger next action - example W4_PN_RSP: transition to SEND_SABM which only depends on "can send"
    rfcomm_run();
}

void rfcomm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    
    // multiplexer handler
    int handled = 0;
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            handled = rfcomm_multiplexer_hci_event_handler(packet, size);
            break;
        case L2CAP_DATA_PACKET:
            handled = rfcomm_multiplexer_l2cap_packet_handler(channel, packet, size);
            break;
        default:
            break;
    }
    
    if (handled) {
        rfcomm_run();
        return;
    }
    
    // we only handle l2cap packet over open multiplexer channel now
    if (packet_type != L2CAP_DATA_PACKET) {
        (*app_packet_handler)(NULL, packet_type, channel, packet, size);
        return;
    }
    rfcomm_multiplexer_t * multiplexer = rfcomm_multiplexer_for_l2cap_cid(channel);
    if (!multiplexer || multiplexer->state != RFCOMM_MULTIPLEXER_OPEN) {
        (*app_packet_handler)(NULL, packet_type, channel, packet, size);
        return;
    }
    
    // channel data ?
    // rfcomm: (0) addr [76543 server channel] [2 direction: initiator uses 1] [1 C/R: CMD by initiator = 1] [0 EA=1]
    const uint8_t frame_dlci = packet[0] >> 2;
	
    if (frame_dlci && (packet[1] == BT_RFCOMM_UIH || packet[1] == BT_RFCOMM_UIH_PF)) {
        rfcomm_channel_packet_handler_uih(multiplexer, packet, size);
        rfcomm_run();
        return;
    }
     
    rfcomm_channel_packet_handler(multiplexer, packet, size);
}

static int rfcomm_channel_ready_for_open(rfcomm_channel_t *channel){
    // log_info("rfcomm_channel_ready_for_open state %u, flags needed %04x, current %04x, rf credits %u, l2cap credits %u \n", channel->state, RFCOMM_CHANNEL_STATE_VAR_RCVD_MSC_RSP|RFCOMM_CHANNEL_STATE_VAR_SENT_MSC_RSP|RFCOMM_CHANNEL_STATE_VAR_SENT_CREDITS, channel->state_var, channel->credits_outgoing, channel->multiplexer->l2cap_credits);
    if ((channel->state_var & RFCOMM_CHANNEL_STATE_VAR_RCVD_MSC_RSP) == 0) return 0;
    if ((channel->state_var & RFCOMM_CHANNEL_STATE_VAR_SENT_MSC_RSP) == 0) return 0;
    if ((channel->state_var & RFCOMM_CHANNEL_STATE_VAR_SENT_CREDITS) == 0) return 0;
    if (channel->credits_outgoing == 0) return 0;
    
    return 1;
}

static void rfcomm_channel_state_machine(rfcomm_channel_t *channel, rfcomm_channel_event_t *event){
    
    // log_info("rfcomm_channel_state_machine: state %u, state_var %04x, event %u\n", channel->state, channel->state_var ,event->type);
    
    rfcomm_multiplexer_t *multiplexer = channel->multiplexer;
    
    // TODO: integrate in common switch
    if (event->type == CH_EVT_RCVD_DISC){
        rfcomm_emit_channel_closed(channel);
        channel->state = RFCOMM_CHANNEL_SEND_UA_AFTER_DISC;
        return;
    }
    
    // TODO: integrate in common switch
    if (event->type == CH_EVT_RCVD_DM){
        log_info("Received DM message for #%u\n", channel->dlci);
        log_info("-> Closing channel locally for #%u\n", channel->dlci);
        rfcomm_emit_channel_closed(channel);
        rfcomm_channel_finalize(channel);
        return;
    }
    
    // remote port negotiation command - just accept everything for now
    //
    // "The RPN command can be used before a new DLC is opened and should be used whenever the port settings change."
    // "The RPN command is specified as optional in TS 07.10, but it is mandatory to recognize and respond to it in RFCOMM. 
    //   (Although the handling of individual settings are implementation-dependent.)"
    //
    
    // TODO: integrate in common switch
    if (event->type == CH_EVT_RCVD_RPN_CMD){
        // control port parameters
        rfcomm_channel_event_rpn_t *event_rpn = (rfcomm_channel_event_rpn_t*) event;
        memcpy(&channel->rpn_data, &event_rpn->data, sizeof(rfcomm_rpn_data_t));
        channel->state_var |= RFCOMM_CHANNEL_STATE_VAR_SEND_RPN_RSP;
        return;
    }
    
    // TODO: integrate in common switch
    if (event->type == CH_EVT_RCVD_RPN_REQ){
        // default rpn rsp
        rfcomm_rpn_data_t rpn_data;
        rpn_data.baud_rate = 0xa0;        /* 9600 bps */
        rpn_data.flags = 0x03;            /* 8-n-1 */
        rpn_data.flow_control = 0;        /* no flow control */
        rpn_data.xon  = 0xd1;             /* XON */
        rpn_data.xoff = 0xd3;             /* XOFF */
        rpn_data.parameter_mask_0 = 0x7f; /* parameter mask, all values set */
        rpn_data.parameter_mask_1 = 0x3f; /* parameter mask, all values set */
        memcpy(&channel->rpn_data, &rpn_data, sizeof(rfcomm_rpn_data_t));
        channel->state_var |= RFCOMM_CHANNEL_STATE_VAR_SEND_RPN_RSP;
        return;
    }
    
    // TODO: integrate in common swich
    if (event->type == CH_EVT_READY_TO_SEND){
        if (channel->state_var & RFCOMM_CHANNEL_STATE_VAR_SEND_RPN_RSP){
            log_info("Sending Remote Port Negotiation RSP for #%u\n", channel->dlci);
            channel->state_var &= ~RFCOMM_CHANNEL_STATE_VAR_SEND_RPN_RSP;
            rfcomm_send_uih_rpn_rsp(multiplexer, channel->dlci, &channel->rpn_data);
            return;
        }
    }
    
    rfcomm_channel_event_pn_t * event_pn = (rfcomm_channel_event_pn_t*) event;
    
    switch (channel->state) {
        case RFCOMM_CHANNEL_CLOSED:
            switch (event->type){
                case CH_EVT_RCVD_SABM:
                    log_info("-> Inform app\n");
                    channel->state_var |= RFCOMM_CHANNEL_STATE_VAR_RCVD_SABM;
                    channel->state = RFCOMM_CHANNEL_INCOMING_SETUP;
                    rfcomm_emit_connection_request(channel);
                    break;
                case CH_EVT_RCVD_PN:
                    rfcomm_channel_accept_pn(channel, event_pn);
                    log_info("-> Inform app\n");
                    channel->state_var |= RFCOMM_CHANNEL_STATE_VAR_RCVD_PN;
                    channel->state = RFCOMM_CHANNEL_INCOMING_SETUP;
                    rfcomm_emit_connection_request(channel);
                    break;
                default:
                    break;
            }
            break;
            
        case RFCOMM_CHANNEL_INCOMING_SETUP:
            switch (event->type){
                case CH_EVT_RCVD_SABM:
                    channel->state_var |= RFCOMM_CHANNEL_STATE_VAR_RCVD_SABM;
                    if (channel->state_var & RFCOMM_CHANNEL_STATE_VAR_CLIENT_ACCEPTED) {
                        channel->state_var |= RFCOMM_CHANNEL_STATE_VAR_SEND_UA;
                    }
                    break;
                case CH_EVT_RCVD_PN:
                    rfcomm_channel_accept_pn(channel, event_pn);
                    channel->state_var |= RFCOMM_CHANNEL_STATE_VAR_RCVD_PN;
                    if (channel->state_var & RFCOMM_CHANNEL_STATE_VAR_CLIENT_ACCEPTED) {
                        channel->state_var |= RFCOMM_CHANNEL_STATE_VAR_SEND_PN_RSP;
                    }
                    break;
                case CH_EVT_READY_TO_SEND:
                    if (channel->state_var & RFCOMM_CHANNEL_STATE_VAR_SEND_PN_RSP){
                        log_info("Sending UIH Parameter Negotiation Respond for #%u\n", channel->dlci);
                        channel->state_var &= ~RFCOMM_CHANNEL_STATE_VAR_SEND_PN_RSP;
                        rfcomm_send_uih_pn_response(multiplexer, channel->dlci, channel->pn_priority, channel->max_frame_size);
                    }
                    else if (channel->state_var & RFCOMM_CHANNEL_STATE_VAR_SEND_UA){
                        log_info("Sending UA #%u\n", channel->dlci);
                        channel->state_var &= ~RFCOMM_CHANNEL_STATE_VAR_SEND_UA;
                        rfcomm_send_ua(multiplexer, channel->dlci);
                    }
                    if ((channel->state_var & RFCOMM_CHANNEL_STATE_VAR_CLIENT_ACCEPTED) && (channel->state_var & RFCOMM_CHANNEL_STATE_VAR_RCVD_SABM)) {
                        channel->state_var |= RFCOMM_CHANNEL_STATE_VAR_SEND_MSC_CMD;
                        channel->state_var |= RFCOMM_CHANNEL_STATE_VAR_SEND_CREDITS;
                        channel->state = RFCOMM_CHANNEL_DLC_SETUP;
                    } 
                    break;
                    
                default:
                    break;
            }
            break;
            
        case RFCOMM_CHANNEL_W4_MULTIPLEXER:
            switch (event->type) {
                case CH_EVT_MULTIPLEXER_READY:
                    log_info("Muliplexer opened, sending UIH PN next\n");
                    channel->state = RFCOMM_CHANNEL_SEND_UIH_PN;
                    break;
                default:
                    break;
            }
            break;
            
        case RFCOMM_CHANNEL_SEND_UIH_PN:
            switch (event->type) {
                case CH_EVT_READY_TO_SEND:
                    log_info("Sending UIH Parameter Negotiation Command for #%u (channel 0x%p)\n", channel->dlci, channel );
                    channel->state = RFCOMM_CHANNEL_W4_PN_RSP;
                    rfcomm_send_uih_pn_command(multiplexer, channel->dlci, channel->max_frame_size);
                    break;
                default:
                    break;
            }
            break;
            
        case RFCOMM_CHANNEL_W4_PN_RSP:
            switch (event->type){
                case CH_EVT_RCVD_PN_RSP:
                    // update max frame size
                    if (channel->max_frame_size > event_pn->max_frame_size) {
                        channel->max_frame_size = event_pn->max_frame_size;
                    }
                    // new credits
                    channel->credits_outgoing = event_pn->credits_outgoing;
                    channel->state = RFCOMM_CHANNEL_SEND_SABM_W4_UA;
                    break;
                default:
                    break;
            }
            break;

        case RFCOMM_CHANNEL_SEND_SABM_W4_UA:
            switch (event->type) {
                case CH_EVT_READY_TO_SEND:
                    log_info("Sending SABM #%u\n", channel->dlci);
                    channel->state = RFCOMM_CHANNEL_W4_UA;
                    rfcomm_send_sabm(multiplexer, channel->dlci);
                    break;
                default:
                    break;
            }
            break;
            
        case RFCOMM_CHANNEL_W4_UA:
            switch (event->type){
                case CH_EVT_RCVD_UA:
                    channel->state = RFCOMM_CHANNEL_DLC_SETUP;
                    channel->state_var |= RFCOMM_CHANNEL_STATE_VAR_SEND_MSC_CMD;
                    channel->state_var |= RFCOMM_CHANNEL_STATE_VAR_SEND_CREDITS;
                    break;
                default:
                    break;
            }
            break;
            
        case RFCOMM_CHANNEL_DLC_SETUP:
            switch (event->type){
                case CH_EVT_RCVD_MSC_CMD:
                    channel->state_var |= RFCOMM_CHANNEL_STATE_VAR_RCVD_MSC_CMD;
                    channel->state_var |= RFCOMM_CHANNEL_STATE_VAR_SEND_MSC_RSP;
                    break;
                case CH_EVT_RCVD_MSC_RSP:
                    channel->state_var |= RFCOMM_CHANNEL_STATE_VAR_RCVD_MSC_RSP;
                    break;
                    
                case CH_EVT_READY_TO_SEND:
                    if (channel->state_var & RFCOMM_CHANNEL_STATE_VAR_SEND_MSC_CMD){
                        log_info("Sending MSC CMD for #%u\n", channel->dlci);
                        channel->state_var &= ~RFCOMM_CHANNEL_STATE_VAR_SEND_MSC_CMD;
                        channel->state_var |= RFCOMM_CHANNEL_STATE_VAR_SENT_MSC_CMD;
                        rfcomm_send_uih_msc_cmd(multiplexer, channel->dlci , 0x8d);  // ea=1,fc=0,rtc=1,rtr=1,ic=0,dv=1
                        break;
                    }
                    if (channel->state_var & RFCOMM_CHANNEL_STATE_VAR_SEND_MSC_RSP){
                        log_info("Sending MSC RSP for #%u\n", channel->dlci);
                        channel->state_var &= ~RFCOMM_CHANNEL_STATE_VAR_SEND_MSC_RSP;
                        channel->state_var |= RFCOMM_CHANNEL_STATE_VAR_SENT_MSC_RSP;
                        rfcomm_send_uih_msc_rsp(multiplexer, channel->dlci, 0x8d);  // ea=1,fc=0,rtc=1,rtr=1,ic=0,dv=1
                        break;
                    }
                    if (channel->state_var & RFCOMM_CHANNEL_STATE_VAR_SEND_CREDITS){
                        log_info("Providing credits for #%u\n", channel->dlci);
                        channel->state_var &= ~RFCOMM_CHANNEL_STATE_VAR_SEND_CREDITS;
                        channel->state_var |= RFCOMM_CHANNEL_STATE_VAR_SENT_CREDITS;
                        if (channel->new_credits_incoming) {
                            uint8_t new_credits = channel->new_credits_incoming;
                            channel->new_credits_incoming = 0;
                            rfcomm_channel_send_credits(channel, new_credits);
                        }
                        break;

                    }
                    break;
                default:
                    break;
            }
            // finally done?
            if (rfcomm_channel_ready_for_open(channel)){
                channel->state = RFCOMM_CHANNEL_OPEN;
                rfcomm_channel_opened(channel);
            }
            break;
        
        case RFCOMM_CHANNEL_OPEN:
            switch (event->type){
                case CH_EVT_RCVD_MSC_CMD:
                    channel->state_var |= RFCOMM_CHANNEL_STATE_VAR_SEND_MSC_RSP;
                    break;
                case CH_EVT_READY_TO_SEND:
                    if (channel->state_var & RFCOMM_CHANNEL_STATE_VAR_SEND_MSC_RSP){
                        log_info("Sending MSC RSP for #%u\n", channel->dlci);
                        channel->state_var &= ~RFCOMM_CHANNEL_STATE_VAR_SEND_MSC_RSP;
                        rfcomm_send_uih_msc_rsp(multiplexer, channel->dlci, 0x8d);  // ea=1,fc=0,rtc=1,rtr=1,ic=0,dv=1
                        break;
                    }
                    if (channel->new_credits_incoming) {
                        uint8_t new_credits = channel->new_credits_incoming;
                        channel->new_credits_incoming = 0;
                        rfcomm_channel_send_credits(channel, new_credits);
                        break;
                    }
                    break;
                case CH_EVT_RCVD_CREDITS: {
                    // notify daemon -> might trigger re-try of parked connections
                    uint8_t event[1] = { DAEMON_EVENT_NEW_RFCOMM_CREDITS };
                    (*app_packet_handler)(channel->connection, DAEMON_EVENT_PACKET, channel->rfcomm_cid, event, sizeof(event));
                    break;
                }  
                default:
                    break;
            }
            break;
            
        case RFCOMM_CHANNEL_SEND_DM:
            switch (event->type) {
                case CH_EVT_READY_TO_SEND:
                    log_info("Sending DM_PF for #%u\n", channel->dlci);
                    // don't emit channel closed - channel was never open
                    channel->state = RFCOMM_CHANNEL_CLOSED;
                    rfcomm_send_dm_pf(multiplexer, channel->dlci);
                    rfcomm_channel_finalize(channel);
                    break;
                default:
                    break;
            }
            break;
            
        case RFCOMM_CHANNEL_SEND_DISC:
            switch (event->type) {
                case CH_EVT_READY_TO_SEND:
                    channel->state = RFCOMM_CHANNEL_CLOSED;
                    rfcomm_send_disc(multiplexer, channel->dlci);
                    rfcomm_emit_channel_closed(channel);
                    rfcomm_channel_finalize(channel);
                    break;
                default:
                    break;
            }
            break;
            
        case RFCOMM_CHANNEL_SEND_UA_AFTER_DISC:
            switch (event->type) {
                case CH_EVT_READY_TO_SEND:
                    log_info("Sending UA after DISC for #%u\n", channel->dlci);
                    channel->state = RFCOMM_CHANNEL_CLOSED;
                    rfcomm_send_ua(multiplexer, channel->dlci);
                    rfcomm_channel_finalize(channel);
                    break;
                default:
                    break;
            }
            break;
            
        default:
            break;
    }
}


// MARK: RFCOMM RUN
// process outstanding signaling tasks
static void rfcomm_run(void){
    
    linked_item_t *it;
    linked_item_t *next;
    
    for (it = (linked_item_t *) rfcomm_multiplexers; it ; it = next){

        next = it->next;    // be prepared for removal of channel in state machine
        
        rfcomm_multiplexer_t * multiplexer = ((rfcomm_multiplexer_t *) it);
        
        if (!l2cap_can_send_packet_now(multiplexer->l2cap_cid)) {
            // log_info("rfcomm_run cannot send l2cap packet for #%u, credits %u\n", multiplexer->l2cap_cid, multiplexer->l2cap_credits);
            continue;
        }
        // log_info("rfcomm_run: multi 0x%08x, state %u\n", (int) multiplexer, multiplexer->state);

        rfcomm_multiplexer_state_machine(multiplexer, MULT_EV_READY_TO_SEND);
    }

    for (it = (linked_item_t *) rfcomm_channels; it ; it = next){

        next = it->next;    // be prepared for removal of channel in state machine

        rfcomm_channel_t * channel = ((rfcomm_channel_t *) it);
        rfcomm_multiplexer_t * multiplexer = channel->multiplexer;
        
        if (!l2cap_can_send_packet_now(multiplexer->l2cap_cid)) continue;
     
        rfcomm_channel_event_t event = { CH_EVT_READY_TO_SEND };
        rfcomm_channel_state_machine(channel, &event);
    }
}

// MARK: RFCOMM BTstack API

void rfcomm_init(void){
    rfcomm_client_cid_generator = 0;
    rfcomm_multiplexers = NULL;
    rfcomm_services     = NULL;
    rfcomm_channels     = NULL;
}

// register packet handler
void rfcomm_register_packet_handler(void (*handler)(void * connection, uint8_t packet_type,
                                                    uint16_t channel, uint8_t *packet, uint16_t size)){
	app_packet_handler = handler;
}

// send packet over specific channel
int rfcomm_send_internal(uint16_t rfcomm_cid, uint8_t *data, uint16_t len){

    rfcomm_channel_t * channel = rfcomm_channel_for_rfcomm_cid(rfcomm_cid);
    if (!channel){
        log_error("rfcomm_send_internal cid 0x%02x doesn't exist!\n", rfcomm_cid);
        return 0;
    }
    
    if (!channel->credits_outgoing){
        log_info("rfcomm_send_internal cid 0x%02x, no rfcomm outgoing credits!\n", rfcomm_cid);
        return RFCOMM_NO_OUTGOING_CREDITS;
    }

    if (!channel->packets_granted){
        log_info("rfcomm_send_internal cid 0x%02x, no rfcomm credits granted!\n", rfcomm_cid);
        return RFCOMM_NO_OUTGOING_CREDITS;
    }
    
    // log_info("rfcomm_send_internal: len %u... outgoing credits %u, l2cap credit %us, granted %u\n",
    //        len, channel->credits_outgoing, channel->multiplexer->l2cap_credits, channel->packets_granted);
    

    // send might cause l2cap to emit new credits, update counters first
    channel->credits_outgoing--;
    int packets_granted_decreased = 0;
    if (channel->packets_granted) {
        channel->packets_granted--;
        packets_granted_decreased++;
    }
    
    int result = rfcomm_send_uih_data(channel->multiplexer, channel->dlci, data, len);
    
    if (result != 0) {
        channel->credits_outgoing++;
        channel->packets_granted += packets_granted_decreased;
        log_info("rfcomm_send_internal: error %d\n", result);
        return result;
    }
    
    // log_info("rfcomm_send_internal: now outgoing credits %u, l2cap credit %us, granted %u\n",
    //        channel->credits_outgoing, channel->multiplexer->l2cap_credits, channel->packets_granted);

    rfcomm_hand_out_credits();
    
    return result;
}

void rfcomm_create_channel2(void * connection, bd_addr_t *addr, uint8_t server_channel, uint8_t incoming_flow_control, uint8_t initial_credits){
    log_info("RFCOMM_CREATE_CHANNEL addr %s channel #%u flow control %u init credits %u\n",  bd_addr_to_str(*addr), server_channel,
             incoming_flow_control, initial_credits);
    
    // create new multiplexer if necessary
    rfcomm_multiplexer_t * multiplexer = rfcomm_multiplexer_for_addr(addr);
    if (!multiplexer) {
        multiplexer = rfcomm_multiplexer_create_for_addr(addr);
        if (!multiplexer){
            rfcomm_emit_channel_open_failed_outgoing_memory(connection, addr, server_channel);
            return;
        }
        multiplexer->outgoing = 1;
        multiplexer->state = RFCOMM_MULTIPLEXER_W4_CONNECT;
    }
    
    // prepare channel
    rfcomm_channel_t * channel = rfcomm_channel_create(multiplexer, NULL, server_channel);
    if (!channel){
        rfcomm_emit_channel_open_failed_outgoing_memory(connection, addr, server_channel);
        return;
    }
    channel->connection = connection;
    channel->incoming_flow_control = incoming_flow_control;
    channel->new_credits_incoming  = initial_credits;
    
    // start multiplexer setup
    if (multiplexer->state != RFCOMM_MULTIPLEXER_OPEN) {
        
        channel->state = RFCOMM_CHANNEL_W4_MULTIPLEXER;
        
        l2cap_create_channel_internal(connection, rfcomm_packet_handler, *addr, PSM_RFCOMM, l2cap_max_mtu());
        
        return;
    }
    
    channel->state = RFCOMM_CHANNEL_SEND_UIH_PN;
    
    // start connecting, if multiplexer is already up and running
    rfcomm_run();
}

void rfcomm_create_channel_with_initial_credits_internal(void * connection, bd_addr_t *addr, uint8_t server_channel, uint8_t initial_credits){
    rfcomm_create_channel2(connection, addr, server_channel, 1, initial_credits);
}

void rfcomm_create_channel_internal(void * connection, bd_addr_t *addr, uint8_t server_channel){
    rfcomm_create_channel2(connection, addr, server_channel, 0, 0x30);
}

void rfcomm_disconnect_internal(uint16_t rfcomm_cid){
    log_info("RFCOMM_DISCONNECT cid 0x%02x", rfcomm_cid);
    rfcomm_channel_t * channel = rfcomm_channel_for_rfcomm_cid(rfcomm_cid);
    if (channel) {
        channel->state = RFCOMM_CHANNEL_SEND_DISC;
    }
    
    // process
    rfcomm_run();
}


void rfcomm_register_service2(void * connection, uint8_t channel, uint16_t max_frame_size, uint8_t incoming_flow_control, uint8_t initial_credits){
    log_info("RFCOMM_REGISTER_SERVICE channel #%u mtu %u flow_control %u credits %u",
             channel, max_frame_size, incoming_flow_control, initial_credits);
    // check if already registered
    rfcomm_service_t * service = rfcomm_service_for_channel(channel);
    if (service){
        rfcomm_emit_service_registered(connection, RFCOMM_CHANNEL_ALREADY_REGISTERED, channel);
        return;
    }
    
    // alloc structure
    service = btstack_memory_rfcomm_service_get();
    if (!service) {
        rfcomm_emit_service_registered(connection, BTSTACK_MEMORY_ALLOC_FAILED, channel);
        return;
    }
    
    // register with l2cap if not registered before, max MTU
    if (linked_list_empty(&rfcomm_services)){
        l2cap_register_service_internal(NULL, rfcomm_packet_handler, PSM_RFCOMM, 0xffff);
    }
    
    // fill in 
    service->connection     = connection;
    service->server_channel = channel;
    service->max_frame_size = max_frame_size;
    service->incoming_flow_control = incoming_flow_control;
    service->incoming_initial_credits = initial_credits;
    
    // add to services list
    linked_list_add(&rfcomm_services, (linked_item_t *) service);
    
    // done
    rfcomm_emit_service_registered(connection, 0, channel);
}

void rfcomm_register_service_with_initial_credits_internal(void * connection, uint8_t channel, uint16_t max_frame_size, uint8_t initial_credits){
    rfcomm_register_service2(connection, channel, max_frame_size, 1, initial_credits);
}

void rfcomm_register_service_internal(void * connection, uint8_t channel, uint16_t max_frame_size){
    rfcomm_register_service2(connection, channel, max_frame_size, 0, 0x30);
}

void rfcomm_unregister_service_internal(uint8_t service_channel){
    log_info("RFCOMM_UNREGISTER_SERVICE #%u", service_channel);
    rfcomm_service_t *service = rfcomm_service_for_channel(service_channel);
    if (!service) return;
    linked_list_remove(&rfcomm_services, (linked_item_t *) service);
    btstack_memory_rfcomm_service_free(service);
    
    // unregister if no services active
    if (linked_list_empty(&rfcomm_services)){
        // bt_send_cmd(&l2cap_unregister_service, PSM_RFCOMM);
        l2cap_unregister_service_internal(NULL, PSM_RFCOMM);
    }
}

void rfcomm_accept_connection_internal(uint16_t rfcomm_cid){
    log_info("RFCOMM_ACCEPT_CONNECTION cid 0x%02x", rfcomm_cid);
    rfcomm_channel_t * channel = rfcomm_channel_for_rfcomm_cid(rfcomm_cid);
    if (!channel) return;
    switch (channel->state) {
        case RFCOMM_CHANNEL_INCOMING_SETUP:
            channel->state_var |= RFCOMM_CHANNEL_STATE_VAR_CLIENT_ACCEPTED;
            if (channel->state_var & RFCOMM_CHANNEL_STATE_VAR_RCVD_PN){
                channel->state_var |= RFCOMM_CHANNEL_STATE_VAR_SEND_PN_RSP;
            }
            if (channel->state_var & RFCOMM_CHANNEL_STATE_VAR_RCVD_SABM){
                channel->state_var |= RFCOMM_CHANNEL_STATE_VAR_SEND_UA;
            }
            break;
        default:
            break;
    }

    // process
    rfcomm_run();
}

void rfcomm_decline_connection_internal(uint16_t rfcomm_cid){
    log_info("RFCOMM_DECLINE_CONNECTION cid 0x%02x", rfcomm_cid);
    rfcomm_channel_t * channel = rfcomm_channel_for_rfcomm_cid(rfcomm_cid);
    if (!channel) return;
    switch (channel->state) {
        case RFCOMM_CHANNEL_INCOMING_SETUP:
            channel->state = RFCOMM_CHANNEL_SEND_DM;
            break;
        default:
            break;
    }

    // process
    rfcomm_run();
}

void rfcomm_grant_credits(uint16_t rfcomm_cid, uint8_t credits){
    log_info("RFCOMM_GRANT_CREDITS cid 0x%02x credits %u", rfcomm_cid, credits);
    rfcomm_channel_t * channel = rfcomm_channel_for_rfcomm_cid(rfcomm_cid);
    if (!channel) return;
    if (!channel->incoming_flow_control) return;
    channel->new_credits_incoming += credits;

    // process
    rfcomm_run();
}

//
void rfcomm_close_connection(void *connection){
    linked_item_t *it;
    
    // close open channels
    for (it = (linked_item_t *) rfcomm_channels; it ; it = it->next){
        rfcomm_channel_t * channel = (rfcomm_channel_t *)it;
        if (channel->connection != connection) continue;
        channel->state = RFCOMM_CHANNEL_SEND_DISC;
    }
    
    // unregister services
    it = (linked_item_t *) &rfcomm_services;
    while (it->next) {
        rfcomm_service_t * service = (rfcomm_service_t *) it->next;
        if (service->connection == connection){
            it->next = it->next->next;
            btstack_memory_rfcomm_service_free(service);
        } else {
            it = it->next;
        }
    }

    // process
    rfcomm_run();
}




