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
 *  RFCOMM.h
 */

#include <btstack/btstack.h>
#include <btstack/utils.h>

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif
    
void rfcomm_init(void);

// register packet handler
void rfcomm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
void rfcomm_register_packet_handler(void (*handler)(void * connection, uint8_t packet_type,
													uint16_t channel, uint8_t *packet, uint16_t size));

// BTstack Internal RFCOMM API
void rfcomm_create_channel_internal(void * connection, bd_addr_t *addr, uint8_t channel);
void rfcomm_create_channel_with_initial_credits_internal(void * connection, bd_addr_t *addr, uint8_t server_channel, uint8_t initial_credits);
void rfcomm_disconnect_internal(uint16_t rfcomm_cid);
void rfcomm_register_service_internal(void * connection, uint8_t channel, uint16_t max_frame_size);
void rfcomm_register_service_with_initial_credits_internal(void * connection, uint8_t channel, uint16_t max_frame_size, uint8_t initial_credits);
void rfcomm_unregister_service_internal(uint8_t service_channel);
void rfcomm_accept_connection_internal(uint16_t rfcomm_cid);
void rfcomm_decline_connection_internal(uint16_t rfcomm_cid);
void rfcomm_grant_credits(uint16_t rfcomm_cid, uint8_t credits);
int  rfcomm_send_internal(uint16_t rfcomm_cid, uint8_t *data, uint16_t len);
void rfcomm_close_connection(void *connection);

#define UNLIMITED_INCOMING_CREDITS 0xff

// private structs
typedef enum {
	RFCOMM_MULTIPLEXER_CLOSED = 1,
	RFCOMM_MULTIPLEXER_W4_CONNECT,  // outgoing
	RFCOMM_MULTIPLEXER_SEND_SABM_0,     //    "
	RFCOMM_MULTIPLEXER_W4_UA_0,     //    "
	RFCOMM_MULTIPLEXER_W4_SABM_0,   // incoming
    RFCOMM_MULTIPLEXER_SEND_UA_0,
	RFCOMM_MULTIPLEXER_OPEN,
    RFCOMM_MULTIPLEXER_SEND_UA_0_AND_DISC
} RFCOMM_MULTIPLEXER_STATE;

typedef enum {
    MULT_EV_READY_TO_SEND = 1,
    
} RFCOMM_MULTIPLEXER_EVENT;

typedef enum {
	RFCOMM_CHANNEL_CLOSED = 1,
	RFCOMM_CHANNEL_W4_MULTIPLEXER,
	RFCOMM_CHANNEL_SEND_UIH_PN,
    RFCOMM_CHANNEL_W4_PN_RSP,
	RFCOMM_CHANNEL_SEND_SABM_W4_UA,
	RFCOMM_CHANNEL_W4_UA,
    RFCOMM_CHANNEL_INCOMING_SETUP,
    RFCOMM_CHANNEL_DLC_SETUP,
	RFCOMM_CHANNEL_OPEN,
    RFCOMM_CHANNEL_SEND_UA_AFTER_DISC,
    RFCOMM_CHANNEL_SEND_DISC,
    RFCOMM_CHANNEL_SEND_DM,
    
} RFCOMM_CHANNEL_STATE;

typedef enum {
    RFCOMM_CHANNEL_STATE_VAR_NONE            = 0,
    RFCOMM_CHANNEL_STATE_VAR_CLIENT_ACCEPTED = 1 << 0,
    RFCOMM_CHANNEL_STATE_VAR_RCVD_PN         = 1 << 1,
    RFCOMM_CHANNEL_STATE_VAR_RCVD_RPN        = 1 << 2,
    RFCOMM_CHANNEL_STATE_VAR_RCVD_SABM       = 1 << 3,
    
    RFCOMM_CHANNEL_STATE_VAR_RCVD_MSC_CMD    = 1 << 4,
    RFCOMM_CHANNEL_STATE_VAR_RCVD_MSC_RSP    = 1 << 5,
    RFCOMM_CHANNEL_STATE_VAR_SEND_PN_RSP     = 1 << 6,
    RFCOMM_CHANNEL_STATE_VAR_SEND_RPN_INFO   = 1 << 7,
    
    RFCOMM_CHANNEL_STATE_VAR_SEND_RPN_RSP    = 1 << 8,
    RFCOMM_CHANNEL_STATE_VAR_SEND_UA         = 1 << 9,
    RFCOMM_CHANNEL_STATE_VAR_SEND_MSC_CMD    = 1 << 10,
    RFCOMM_CHANNEL_STATE_VAR_SEND_MSC_RSP    = 1 << 11,
    
    RFCOMM_CHANNEL_STATE_VAR_SEND_CREDITS    = 1 << 12,
    RFCOMM_CHANNEL_STATE_VAR_SENT_MSC_CMD    = 1 << 13,
    RFCOMM_CHANNEL_STATE_VAR_SENT_MSC_RSP    = 1 << 14,
    RFCOMM_CHANNEL_STATE_VAR_SENT_CREDITS    = 1 << 15,
} RFCOMM_CHANNEL_STATE_VAR;

typedef enum {
    CH_EVT_RCVD_SABM = 1,
    CH_EVT_RCVD_UA,
    CH_EVT_RCVD_PN,
    CH_EVT_RCVD_PN_RSP,
    CH_EVT_RCVD_DISC,
    CH_EVT_RCVD_DM,
    CH_EVT_RCVD_MSC_CMD,
    CH_EVT_RCVD_MSC_RSP,
    CH_EVT_RCVD_RPN_CMD,
    CH_EVT_RCVD_RPN_REQ,
    CH_EVT_RCVD_CREDITS,
    CH_EVT_MULTIPLEXER_READY,
    CH_EVT_READY_TO_SEND,
} RFCOMM_CHANNEL_EVENT;

typedef struct rfcomm_channel_event {
    RFCOMM_CHANNEL_EVENT type;
} rfcomm_channel_event_t;

typedef struct rfcomm_channel_event_pn {
    rfcomm_channel_event_t super;
    uint16_t max_frame_size;
    uint8_t  priority;
    uint8_t  credits_outgoing;
} rfcomm_channel_event_pn_t;

typedef struct rfcomm_rpn_data {
    uint8_t baud_rate;
    uint8_t flags;
    uint8_t flow_control;
    uint8_t xon;
    uint8_t xoff;
    uint8_t parameter_mask_0;   // first byte
    uint8_t parameter_mask_1;   // second byte
} rfcomm_rpn_data_t;

typedef struct rfcomm_channel_event_rpn {
    rfcomm_channel_event_t super;
    rfcomm_rpn_data_t data;
} rfcomm_channel_event_rpn_t;

// info regarding potential connections
typedef struct {
    // linked list - assert: first field
    linked_item_t    item;
	
    // server channel
    uint8_t server_channel;
    
    // incoming max frame size
    uint16_t max_frame_size;

    // use incoming flow control
    uint8_t incoming_flow_control;
    
    // initial incoming credits
    uint8_t incoming_initial_credits;
    
    // client connection
    void *connection;    
    
    // internal connection
    btstack_packet_handler_t packet_handler;
    
} rfcomm_service_t;

// info regarding multiplexer
// note: spec mandates single multplexer per device combination
typedef struct {
    // linked list - assert: first field
    linked_item_t    item;
    
    timer_source_t   timer;
    int              timer_active;
    
	RFCOMM_MULTIPLEXER_STATE state;	
    
    uint16_t  l2cap_cid;
    uint8_t   l2cap_credits;
    
	bd_addr_t remote_addr;
    hci_con_handle_t con_handle;
    
	uint8_t   outgoing;
    
    // hack to deal with authentication failure only observed by remote side
    uint8_t   at_least_one_connection;
    
    uint16_t max_frame_size;
    
    // send DM for DLCI != 0
    uint8_t send_dm_for_dlci;
    
} rfcomm_multiplexer_t;

// info regarding an actual coneection
typedef struct {
    // linked list - assert: first field
    linked_item_t    item;
	
	rfcomm_multiplexer_t *multiplexer;
	uint16_t rfcomm_cid;
    uint8_t  outgoing;
    uint8_t  dlci; 
    
    // number of packets granted to client
    uint8_t packets_granted;

    // credits for outgoing traffic
    uint8_t credits_outgoing;
    
    // number of packets remote will be granted
    uint8_t new_credits_incoming;

    // credits for incoming traffic
    uint8_t credits_incoming;
    
    // use incoming flow control
    uint8_t incoming_flow_control;
    
    // channel state
    RFCOMM_CHANNEL_STATE state;
    
    // state variables used in RFCOMM_CHANNEL_INCOMING
    RFCOMM_CHANNEL_STATE_VAR state_var;
    
    // priority set by incoming side in PN
    uint8_t pn_priority;
    
	// negotiated frame size
    uint16_t max_frame_size;
	
    // rpn data
    rfcomm_rpn_data_t rpn_data;
    
	// server channel (see rfcomm_service_t) - NULL => outgoing channel
	rfcomm_service_t * service;
    
    // internal connection
    btstack_packet_handler_t packet_handler;
    
    // client connection
    void * connection;
    
} rfcomm_channel_t;


#if defined __cplusplus
}
#endif
