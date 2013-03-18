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
 *  l2cap.c
 *
 *  Logical Link Control and Adaption Protocl (L2CAP)
 *
 *  Created by Matthias Ringwald on 5/16/09.
 */

#include "l2cap.h"
#include "hci.h"
#include "hci_dump.h"
#include "debug.h"
#include "btstack_memory.h"

#include <stdarg.h>
#include <string.h>

#include <stdio.h>

// nr of buffered acl packets in outgoing queue to get max performance 
#define NR_BUFFERED_ACL_PACKETS 3

// used to cache l2cap rejects, echo, and informational requests
#define NR_PENDING_SIGNALING_RESPONSES 3

// offsets for L2CAP SIGNALING COMMANDS
#define L2CAP_SIGNALING_COMMAND_CODE_OFFSET   0
#define L2CAP_SIGNALING_COMMAND_SIGID_OFFSET  1
#define L2CAP_SIGNALING_COMMAND_LENGTH_OFFSET 2
#define L2CAP_SIGNALING_COMMAND_DATA_OFFSET   4

static void null_packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void l2cap_packet_handler(uint8_t packet_type, uint8_t *packet, uint16_t size);

// used to cache l2cap rejects, echo, and informational requests
static l2cap_signaling_response_t signaling_responses[NR_PENDING_SIGNALING_RESPONSES];
static int signaling_responses_pending;

static linked_list_t l2cap_channels = NULL;
static linked_list_t l2cap_services = NULL;
static void (*packet_handler) (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) = null_packet_handler;
static int new_credits_blocked = 0;

static btstack_packet_handler_t attribute_protocol_packet_handler = NULL;
static btstack_packet_handler_t security_protocol_packet_handler = NULL;

// prototypes
static void l2cap_finialize_channel_close(l2cap_channel_t *channel);
static l2cap_service_t * l2cap_get_service(uint16_t psm);
static void l2cap_emit_channel_opened(l2cap_channel_t *channel, uint8_t status);
static void l2cap_emit_channel_closed(l2cap_channel_t *channel);
static void l2cap_emit_connection_request(l2cap_channel_t *channel);
static int l2cap_channel_ready_for_open(l2cap_channel_t *channel);


void l2cap_init(){
    new_credits_blocked = 0;
    signaling_responses_pending = 0;
    
    l2cap_channels = NULL;
    l2cap_services = NULL;

    packet_handler = null_packet_handler;
    
    // 
    // register callback with HCI
    //
    hci_register_packet_handler(&l2cap_packet_handler);
    hci_connectable_control(0); // no services yet
}


/** Register L2CAP packet handlers */
static void null_packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
}
void l2cap_register_packet_handler(void (*handler)(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)){
    packet_handler = handler;
}

//  notify client/protocol handler
void l2cap_dispatch(l2cap_channel_t *channel, uint8_t type, uint8_t * data, uint16_t size){
    if (channel->packet_handler) {
        (* (channel->packet_handler))(type, channel->local_cid, data, size);
    } else {
        (*packet_handler)(channel->connection, type, channel->local_cid, data, size);
    }
}

void l2cap_emit_channel_opened(l2cap_channel_t *channel, uint8_t status) {
    log_info("L2CAP_EVENT_CHANNEL_OPENED status 0x%x addr %s handle 0x%x psm 0x%x local_cid 0x%x remote_cid 0x%x local_mtu %u, remote_mtu %u",
             status, bd_addr_to_str(channel->address), channel->handle, channel->psm,
             channel->local_cid, channel->remote_cid, channel->local_mtu, channel->remote_mtu);
    uint8_t event[21];
    event[0] = L2CAP_EVENT_CHANNEL_OPENED;
    event[1] = sizeof(event) - 2;
    event[2] = status;
    bt_flip_addr(&event[3], channel->address);
    bt_store_16(event,  9, channel->handle);
    bt_store_16(event, 11, channel->psm);
    bt_store_16(event, 13, channel->local_cid);
    bt_store_16(event, 15, channel->remote_cid);
    bt_store_16(event, 17, channel->local_mtu);
    bt_store_16(event, 19, channel->remote_mtu); 
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    l2cap_dispatch(channel, HCI_EVENT_PACKET, event, sizeof(event));
}

void l2cap_emit_channel_closed(l2cap_channel_t *channel) {
    log_info("L2CAP_EVENT_CHANNEL_CLOSED local_cid 0x%x", channel->local_cid);
    uint8_t event[4];
    event[0] = L2CAP_EVENT_CHANNEL_CLOSED;
    event[1] = sizeof(event) - 2;
    bt_store_16(event, 2, channel->local_cid);
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    l2cap_dispatch(channel, HCI_EVENT_PACKET, event, sizeof(event));
}

void l2cap_emit_connection_request(l2cap_channel_t *channel) {
    log_info("L2CAP_EVENT_INCOMING_CONNECTION addr %s handle 0x%x psm 0x%x local_cid 0x%x remote_cid 0x%x",
             bd_addr_to_str(channel->address), channel->handle,  channel->psm, channel->local_cid, channel->remote_cid);
    uint8_t event[16];
    event[0] = L2CAP_EVENT_INCOMING_CONNECTION;
    event[1] = sizeof(event) - 2;
    bt_flip_addr(&event[2], channel->address);
    bt_store_16(event,  8, channel->handle);
    bt_store_16(event, 10, channel->psm);
    bt_store_16(event, 12, channel->local_cid);
    bt_store_16(event, 14, channel->remote_cid);
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    l2cap_dispatch(channel, HCI_EVENT_PACKET, event, sizeof(event));
}

static void l2cap_emit_service_registered(void *connection, uint8_t status, uint16_t psm){
    log_info("L2CAP_EVENT_SERVICE_REGISTERED status 0x%x psm 0x%x", status, psm);
    uint8_t event[5];
    event[0] = L2CAP_EVENT_SERVICE_REGISTERED;
    event[1] = sizeof(event) - 2;
    event[2] = status;
    bt_store_16(event, 3, psm);
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    (*packet_handler)(connection, HCI_EVENT_PACKET, 0, event, sizeof(event));
}

void l2cap_emit_credits(l2cap_channel_t *channel, uint8_t credits) {
    
    log_info("L2CAP_EVENT_CREDITS local_cid 0x%x credits %u", channel->local_cid, credits);
    // track credits
    channel->packets_granted += credits;
    
    uint8_t event[5];
    event[0] = L2CAP_EVENT_CREDITS;
    event[1] = sizeof(event) - 2;
    bt_store_16(event, 2, channel->local_cid);
    event[4] = credits;
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    l2cap_dispatch(channel, HCI_EVENT_PACKET, event, sizeof(event));
}

void l2cap_block_new_credits(uint8_t blocked){
    new_credits_blocked = blocked;
}

void l2cap_hand_out_credits(void){

    if (new_credits_blocked) return;    // we're told not to. used by daemon
    
    linked_item_t *it;
    for (it = (linked_item_t *) l2cap_channels; it ; it = it->next){
        if (!hci_number_free_acl_slots()) return;
        l2cap_channel_t * channel = (l2cap_channel_t *) it;
        if (channel->state != L2CAP_STATE_OPEN) continue;
        if (hci_number_outgoing_packets(channel->handle) < NR_BUFFERED_ACL_PACKETS && channel->packets_granted == 0) {
            l2cap_emit_credits(channel, 1);
        }
    }
}

l2cap_channel_t * l2cap_get_channel_for_local_cid(uint16_t local_cid){
    linked_item_t *it;
    for (it = (linked_item_t *) l2cap_channels; it ; it = it->next){
        l2cap_channel_t * channel = (l2cap_channel_t *) it;
        if ( channel->local_cid == local_cid) {
            return channel;
        }
    }
    return NULL;
}

int  l2cap_can_send_packet_now(uint16_t local_cid){
    l2cap_channel_t *channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) return 0;
    if (!channel->packets_granted) return 0;
    return hci_can_send_packet_now(HCI_ACL_DATA_PACKET);
}

uint16_t l2cap_get_remote_mtu_for_local_cid(uint16_t local_cid){
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (channel) {
        return channel->remote_mtu;
    } 
    return 0;
}

int l2cap_send_signaling_packet(hci_con_handle_t handle, L2CAP_SIGNALING_COMMANDS cmd, uint8_t identifier, ...){

    if (!hci_can_send_packet_now(HCI_ACL_DATA_PACKET)){
        log_info("l2cap_send_signaling_packet, cannot send\n");
        return BTSTACK_ACL_BUFFERS_FULL;
    }
    
    // log_info("l2cap_send_signaling_packet type %u\n", cmd);
    uint8_t *acl_buffer = hci_get_outgoing_acl_packet_buffer();
    va_list argptr;
    va_start(argptr, identifier);
    uint16_t len = l2cap_create_signaling_internal(acl_buffer, handle, cmd, identifier, argptr);
    va_end(argptr);
    // log_info("l2cap_send_signaling_packet con %u!\n", handle);
    return hci_send_acl_packet(acl_buffer, len);
}

uint8_t *l2cap_get_outgoing_buffer(void){
    return hci_get_outgoing_acl_packet_buffer() + COMPLETE_L2CAP_HEADER; // 8 bytes
}

int l2cap_send_prepared(uint16_t local_cid, uint16_t len){
    
    if (!hci_can_send_packet_now(HCI_ACL_DATA_PACKET)){
        log_info("l2cap_send_internal cid 0x%02x, cannot send\n", local_cid);
        return BTSTACK_ACL_BUFFERS_FULL;
    }
    
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) {
        log_error("l2cap_send_internal no channel for cid 0x%02x\n", local_cid);
        return -1;   // TODO: define error
    }

    if (channel->packets_granted == 0){
        log_error("l2cap_send_internal cid 0x%02x, no credits!\n", local_cid);
        return -1;  // TODO: define error
    }
    
    --channel->packets_granted;

    log_debug("l2cap_send_internal cid 0x%02x, handle %u, 1 credit used, credits left %u;\n",
                  local_cid, channel->handle, channel->packets_granted);
    
    uint8_t *acl_buffer = hci_get_outgoing_acl_packet_buffer();

    // 0 - Connection handle : PB=10 : BC=00 
    bt_store_16(acl_buffer, 0, channel->handle | (2 << 12) | (0 << 14));
    // 2 - ACL length
    bt_store_16(acl_buffer, 2,  len + 4);
    // 4 - L2CAP packet length
    bt_store_16(acl_buffer, 4,  len + 0);
    // 6 - L2CAP channel DEST
    bt_store_16(acl_buffer, 6, channel->remote_cid);    
    // send
    int err = hci_send_acl_packet(acl_buffer, len+8);
    
    l2cap_hand_out_credits();
    
    return err;
}

int l2cap_send_prepared_connectionless(uint16_t handle, uint16_t cid, uint16_t len){
    
    if (!hci_can_send_packet_now(HCI_ACL_DATA_PACKET)){
        log_info("l2cap_send_prepared_to_handle cid 0x%02x, cannot send\n", cid);
        return BTSTACK_ACL_BUFFERS_FULL;
    }
    
    log_debug("l2cap_send_prepared_to_handle cid 0x%02x, handle %u\n", cid, handle);
    
    uint8_t *acl_buffer = hci_get_outgoing_acl_packet_buffer();
    
    // 0 - Connection handle : PB=10 : BC=00 
    bt_store_16(acl_buffer, 0, handle | (2 << 12) | (0 << 14));
    // 2 - ACL length
    bt_store_16(acl_buffer, 2,  len + 4);
    // 4 - L2CAP packet length
    bt_store_16(acl_buffer, 4,  len + 0);
    // 6 - L2CAP channel DEST
    bt_store_16(acl_buffer, 6, cid);    
    // send
    int err = hci_send_acl_packet(acl_buffer, len+8);
    
    l2cap_hand_out_credits();

    return err;
}

int l2cap_send_internal(uint16_t local_cid, uint8_t *data, uint16_t len){

    if (!hci_can_send_packet_now(HCI_ACL_DATA_PACKET)){
        log_info("l2cap_send_internal cid 0x%02x, cannot send\n", local_cid);
        return BTSTACK_ACL_BUFFERS_FULL;
    }

    uint8_t *acl_buffer = hci_get_outgoing_acl_packet_buffer();

    memcpy(&acl_buffer[8], data, len);

    return l2cap_send_prepared(local_cid, len);
}

int l2cap_send_connectionless(uint16_t handle, uint16_t cid, uint8_t *data, uint16_t len){
    
    if (!hci_can_send_packet_now(HCI_ACL_DATA_PACKET)){
        log_info("l2cap_send_internal cid 0x%02x, cannot send\n", cid);
        return BTSTACK_ACL_BUFFERS_FULL;
    }
    
    uint8_t *acl_buffer = hci_get_outgoing_acl_packet_buffer();
    
    memcpy(&acl_buffer[8], data, len);
    
    return l2cap_send_prepared_connectionless(handle, cid, len);
}

static inline void channelStateVarSetFlag(l2cap_channel_t *channel, L2CAP_CHANNEL_STATE_VAR flag){
    channel->state_var = (L2CAP_CHANNEL_STATE_VAR) (channel->state_var | flag);
}

static inline void channelStateVarClearFlag(l2cap_channel_t *channel, L2CAP_CHANNEL_STATE_VAR flag){
    channel->state_var = (L2CAP_CHANNEL_STATE_VAR) (channel->state_var & ~flag);
}



// MARK: L2CAP_RUN
// process outstanding signaling tasks
void l2cap_run(void){
    
    // check pending signaling responses
    while (signaling_responses_pending){
        
        if (!hci_can_send_packet_now(HCI_ACL_DATA_PACKET)) break;
        
        hci_con_handle_t handle = signaling_responses[0].handle;
        uint8_t sig_id = signaling_responses[0].sig_id;
        uint16_t infoType = signaling_responses[0].data;    // INFORMATION_REQUEST
        uint16_t result   = signaling_responses[0].data;    // CONNECTION_REQUEST
        
        switch (signaling_responses[0].code){
            case CONNECTION_REQUEST:
                l2cap_send_signaling_packet(handle, CONNECTION_RESPONSE, sig_id, 0, 0, result, 0);
                break;
            case ECHO_REQUEST:
                l2cap_send_signaling_packet(handle, ECHO_RESPONSE, sig_id, 0, NULL);
                break;
            case INFORMATION_REQUEST:
                if (infoType == 2) {
                    uint32_t features = 0;
                    // extended features request supported, however no features present
                    l2cap_send_signaling_packet(handle, INFORMATION_RESPONSE, sig_id, infoType, 0, 4, &features);
                } else {
                    // all other types are not supported
                    l2cap_send_signaling_packet(handle, INFORMATION_RESPONSE, sig_id, infoType, 1, 0, NULL);
                }
                break;
            default:
                // should not happen
                break;
        }
        
        // remove first item
        signaling_responses_pending--;
        int i;
        for (i=0; i < signaling_responses_pending; i++){
            memcpy(&signaling_responses[i], &signaling_responses[i+1], sizeof(l2cap_signaling_response_t));
        }
    }
    
    uint8_t  config_options[4];
    linked_item_t *it;
    linked_item_t *next;
    for (it = (linked_item_t *) l2cap_channels; it ; it = next){
        next = it->next;    // cache next item as current item might get freed

        if (!hci_can_send_packet_now(HCI_COMMAND_DATA_PACKET)) break;
        if (!hci_can_send_packet_now(HCI_ACL_DATA_PACKET)) break;
        
        l2cap_channel_t * channel = (l2cap_channel_t *) it;
        
        // log_info("l2cap_run: state %u, var 0x%02x\n", channel->state, channel->state_var);
        
        
        switch (channel->state){

            case L2CAP_STATE_WILL_SEND_CREATE_CONNECTION:
                // send connection request - set state first
                channel->state = L2CAP_STATE_WAIT_CONNECTION_COMPLETE;
                // BD_ADDR, Packet_Type, Page_Scan_Repetition_Mode, Reserved, Clock_Offset, Allow_Role_Switch
                hci_send_cmd(&hci_create_connection, channel->address, hci_usable_acl_packet_types(), 0, 0, 0, 1); 
                break;
                
            case L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_DECLINE:
                l2cap_send_signaling_packet(channel->handle, CONNECTION_RESPONSE, channel->remote_sig_id, 0, 0, channel->reason, 0);
                // discard channel - l2cap_finialize_channel_close without sending l2cap close event
                linked_list_remove(&l2cap_channels, (linked_item_t *) channel); // -- remove from list
                btstack_memory_l2cap_channel_free(channel); 
                break;
                
            case L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_ACCEPT:
                channel->state = L2CAP_STATE_CONFIG;
                channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_REQ);
                l2cap_send_signaling_packet(channel->handle, CONNECTION_RESPONSE, channel->remote_sig_id, channel->local_cid, channel->remote_cid, 0, 0);
                break;
                
            case L2CAP_STATE_WILL_SEND_CONNECTION_REQUEST:
                // success, start l2cap handshake
                channel->local_sig_id = l2cap_next_sig_id();
                channel->state = L2CAP_STATE_WAIT_CONNECT_RSP;
                l2cap_send_signaling_packet( channel->handle, CONNECTION_REQUEST, channel->local_sig_id, channel->psm, channel->local_cid);                   
                break;
            
            case L2CAP_STATE_CONFIG:
                if (channel->state_var & L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP){
                    channelStateVarClearFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP);
                    channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SENT_CONF_RSP);
                    l2cap_send_signaling_packet(channel->handle, CONFIGURE_RESPONSE, channel->remote_sig_id, channel->remote_cid, 0, 0, 0, NULL);
                }
                else if (channel->state_var & L2CAP_CHANNEL_STATE_VAR_SEND_CONF_REQ){
                    channelStateVarClearFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_REQ);
                    channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SENT_CONF_REQ);
                    channel->local_sig_id = l2cap_next_sig_id();
                    config_options[0] = 1; // MTU
                    config_options[1] = 2; // len param
                    bt_store_16( (uint8_t*)&config_options, 2, channel->local_mtu);
                    l2cap_send_signaling_packet(channel->handle, CONFIGURE_REQUEST, channel->local_sig_id, channel->remote_cid, 0, 4, &config_options);
                }
                if (l2cap_channel_ready_for_open(channel)){
                    channel->state = L2CAP_STATE_OPEN;
                    l2cap_emit_channel_opened(channel, 0);  // success
                    l2cap_emit_credits(channel, 1);
                }
                break;

            case L2CAP_STATE_WILL_SEND_DISCONNECT_RESPONSE:
                l2cap_send_signaling_packet( channel->handle, DISCONNECTION_RESPONSE, channel->remote_sig_id, channel->local_cid, channel->remote_cid);   
                l2cap_finialize_channel_close(channel);  // -- remove from list
                break;
                
            case L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST:
                channel->local_sig_id = l2cap_next_sig_id();
                channel->state = L2CAP_STATE_WAIT_DISCONNECT;
                l2cap_send_signaling_packet( channel->handle, DISCONNECTION_REQUEST, channel->local_sig_id, channel->remote_cid, channel->local_cid);   
                break;
            default:
                break;
        }
    }
}

uint16_t l2cap_max_mtu(void){
    return hci_max_acl_data_packet_length() - L2CAP_HEADER_SIZE;
}

// open outgoing L2CAP channel
void l2cap_create_channel_internal(void * connection, btstack_packet_handler_t packet_handler,
                                   bd_addr_t address, uint16_t psm, uint16_t mtu){
    
    log_info("L2CAP_CREATE_CHANNEL_MTU addr %s psm 0x%x mtu %u", bd_addr_to_str(address), psm, mtu);
    
    // alloc structure
    l2cap_channel_t * chan = (l2cap_channel_t*) btstack_memory_l2cap_channel_get();
    if (!chan) {
        // emit error event
        l2cap_channel_t dummy_channel;
        BD_ADDR_COPY(dummy_channel.address, address);
        dummy_channel.psm = psm;
        l2cap_emit_channel_opened(&dummy_channel, BTSTACK_MEMORY_ALLOC_FAILED);
        return;
    }
    // limit local mtu to max acl packet length
    if (mtu > l2cap_max_mtu()) {
        mtu = l2cap_max_mtu();
    }
        
    // fill in 
    BD_ADDR_COPY(chan->address, address);
    chan->psm = psm;
    chan->handle = 0;
    chan->connection = connection;
    chan->packet_handler = packet_handler;
    chan->remote_mtu = L2CAP_MINIMAL_MTU;
    chan->local_mtu = mtu;
    chan->packets_granted = 0;
    
    // set initial state
    chan->state = L2CAP_STATE_WILL_SEND_CREATE_CONNECTION;
    chan->state_var = L2CAP_CHANNEL_STATE_VAR_NONE;
    chan->remote_sig_id = L2CAP_SIG_ID_INVALID;
    chan->local_sig_id = L2CAP_SIG_ID_INVALID;
    
    // add to connections list
    linked_list_add(&l2cap_channels, (linked_item_t *) chan);
    
    l2cap_run();
}

void l2cap_disconnect_internal(uint16_t local_cid, uint8_t reason){
    log_info("L2CAP_DISCONNECT local_cid 0x%x reason 0x%x", local_cid, reason);
    // find channel for local_cid
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (channel) {
        channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST;
    }
    // process
    l2cap_run();
}

static void l2cap_handle_connection_failed_for_addr(bd_addr_t address, uint8_t status){
    linked_item_t *it = (linked_item_t *) &l2cap_channels;
    while (it->next){
        l2cap_channel_t * channel = (l2cap_channel_t *) it->next;
        if ( ! BD_ADDR_CMP( channel->address, address) ){
            if (channel->state == L2CAP_STATE_WAIT_CONNECTION_COMPLETE || channel->state == L2CAP_STATE_WILL_SEND_CREATE_CONNECTION) {
                // failure, forward error code
                l2cap_emit_channel_opened(channel, status);
                // discard channel
                it->next = it->next->next;
                btstack_memory_l2cap_channel_free(channel);
            }
        } else {
            it = it->next;
        }
    }
}

static void l2cap_handle_connection_success_for_addr(bd_addr_t address, hci_con_handle_t handle){
    linked_item_t *it;
    for (it = (linked_item_t *) l2cap_channels; it ; it = it->next){
        l2cap_channel_t * channel = (l2cap_channel_t *) it;
        if ( ! BD_ADDR_CMP( channel->address, address) ){
            if (channel->state == L2CAP_STATE_WAIT_CONNECTION_COMPLETE || channel->state == L2CAP_STATE_WILL_SEND_CREATE_CONNECTION) {
                // success, start l2cap handshake
                channel->state = L2CAP_STATE_WILL_SEND_CONNECTION_REQUEST;
                channel->handle = handle;
                channel->local_cid = l2cap_next_local_cid();
            }
        }
    }
    // process
    l2cap_run();
}

void l2cap_event_handler( uint8_t *packet, uint16_t size ){
    
    bd_addr_t address;
    hci_con_handle_t handle;
    l2cap_channel_t * channel;
    linked_item_t *it;
    int hci_con_used;
    
    switch(packet[0]){
            
        // handle connection complete events
        case HCI_EVENT_CONNECTION_COMPLETE:
            bt_flip_addr(address, &packet[5]);
            if (packet[2] == 0){
                handle = READ_BT_16(packet, 3);
                l2cap_handle_connection_success_for_addr(address, handle);
            } else {
                l2cap_handle_connection_failed_for_addr(address, packet[2]);
            }
            break;
            
        // handle successful create connection cancel command
        case HCI_EVENT_COMMAND_COMPLETE:
            if ( COMMAND_COMPLETE_EVENT(packet, hci_create_connection_cancel) ) {
                if (packet[5] == 0){
                    bt_flip_addr(address, &packet[6]);
                    // CONNECTION TERMINATED BY LOCAL HOST (0X16)
                    l2cap_handle_connection_failed_for_addr(address, 0x16);
                }
            }
            l2cap_run();    // try sending signaling packets first
            break;
            
        case HCI_EVENT_COMMAND_STATUS:
            l2cap_run();    // try sending signaling packets first
            break;
            
        // handle disconnection complete events
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            // send l2cap disconnect events for all channels on this handle
            handle = READ_BT_16(packet, 3);
            it = (linked_item_t *) &l2cap_channels;
            while (it->next){
                l2cap_channel_t * channel = (l2cap_channel_t *) it->next;
                if ( channel->handle == handle ){
                    // update prev item before free'ing next element - don't call l2cap_finalize_channel_close
                    it->next = it->next->next;
                    l2cap_emit_channel_closed(channel);
                    btstack_memory_l2cap_channel_free(channel);
                } else {
                    it = it->next;
                }
            }
            break;
            
        case HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS:
            l2cap_run();    // try sending signaling packets first
            l2cap_hand_out_credits();
            break;
            
        // HCI Connection Timeouts
        case L2CAP_EVENT_TIMEOUT_CHECK:
            handle = READ_BT_16(packet, 2);
            if (hci_authentication_active_for_handle(handle)) break;
            hci_con_used = 0;
            for (it = (linked_item_t *) l2cap_channels; it ; it = it->next){
                channel = (l2cap_channel_t *) it;
                if (channel->handle == handle) {
                    hci_con_used = 1;
                }
            }
            if (hci_con_used) break;
            if (!hci_can_send_packet_now(HCI_COMMAND_DATA_PACKET)) break;
            hci_send_cmd(&hci_disconnect, handle, 0x13); // remote closed connection             
            break;

        case DAEMON_EVENT_HCI_PACKET_SENT:
            for (it = (linked_item_t *) l2cap_channels; it ; it = it->next){
                channel = (l2cap_channel_t *) it;
                if (channel->packet_handler) {
                    (* (channel->packet_handler))(HCI_EVENT_PACKET, channel->local_cid, packet, size);
                } 
            }
            if (attribute_protocol_packet_handler) {
                (*attribute_protocol_packet_handler)(HCI_EVENT_PACKET, 0, packet, size);
            }
            if (security_protocol_packet_handler) {
                (*security_protocol_packet_handler)(HCI_EVENT_PACKET, 0, packet, size);
            }
            break;
            
        default:
            break;
    }
    
    // pass on
    (*packet_handler)(NULL, HCI_EVENT_PACKET, 0, packet, size);
}

static void l2cap_handle_disconnect_request(l2cap_channel_t *channel, uint16_t identifier){
    channel->remote_sig_id = identifier;
    channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_RESPONSE;
    l2cap_run();
}

static void l2cap_register_signaling_response(hci_con_handle_t handle, uint8_t code, uint8_t sig_id, uint16_t data){
    // Vol 3, Part A, 4.3: "The DCID and SCID fields shall be ignored when the result field indi- cates the connection was refused."
    if (signaling_responses_pending < NR_PENDING_SIGNALING_RESPONSES) {
        signaling_responses[signaling_responses_pending].handle = handle;
        signaling_responses[signaling_responses_pending].code = code;
        signaling_responses[signaling_responses_pending].sig_id = sig_id;
        signaling_responses[signaling_responses_pending].data = data;
        signaling_responses_pending++;
        l2cap_run();
    }
}

static void l2cap_handle_connection_request(hci_con_handle_t handle, uint8_t sig_id, uint16_t psm, uint16_t source_cid){
    
    // log_info("l2cap_handle_connection_request for handle %u, psm %u cid 0x%02x\n", handle, psm, source_cid);
    l2cap_service_t *service = l2cap_get_service(psm);
    if (!service) {
        // 0x0002 PSM not supported
        l2cap_register_signaling_response(handle, CONNECTION_REQUEST, sig_id, 0x0002);
        return;
    }
    
    hci_connection_t * hci_connection = connection_for_handle( handle );
    if (!hci_connection) {
        // 
        log_error("no hci_connection for handle %u\n", handle);
        return;
    }
    // alloc structure
    // log_info("l2cap_handle_connection_request register channel\n");
    l2cap_channel_t * channel = (l2cap_channel_t*) btstack_memory_l2cap_channel_get();
    if (!channel){
        // 0x0004 No resources available
        l2cap_register_signaling_response(handle, CONNECTION_REQUEST, sig_id, 0x0004);
        return;
    }
    
    // fill in 
    BD_ADDR_COPY(channel->address, hci_connection->address);
    channel->psm = psm;
    channel->handle = handle;
    channel->connection = service->connection;
    channel->packet_handler = service->packet_handler;
    channel->local_cid  = l2cap_next_local_cid();
    channel->remote_cid = source_cid;
    channel->local_mtu  = service->mtu;
    channel->remote_mtu = L2CAP_DEFAULT_MTU;
    channel->packets_granted = 0;
    channel->remote_sig_id = sig_id; 

    // limit local mtu to max acl packet length
    if (channel->local_mtu > l2cap_max_mtu()) {
        channel->local_mtu = l2cap_max_mtu();
    }
    
    // set initial state
    channel->state = L2CAP_STATE_WAIT_CLIENT_ACCEPT_OR_REJECT;
    channel->state_var = L2CAP_CHANNEL_STATE_VAR_NONE;
    
    // add to connections list
    linked_list_add(&l2cap_channels, (linked_item_t *) channel);
    
    // emit incoming connection request
    l2cap_emit_connection_request(channel);
}

void l2cap_accept_connection_internal(uint16_t local_cid){
    log_info("L2CAP_ACCEPT_CONNECTION local_cid 0x%x", local_cid);
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(local_cid);
    if (!channel) {
        log_error("l2cap_accept_connection_internal called but local_cid 0x%x not found", local_cid);
        return;
    }

    channel->state = L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_ACCEPT;

    // process
    l2cap_run();
}

void l2cap_decline_connection_internal(uint16_t local_cid, uint8_t reason){
    log_info("L2CAP_DECLINE_CONNECTION local_cid 0x%x, reason %x", local_cid, reason);
    l2cap_channel_t * channel = l2cap_get_channel_for_local_cid( local_cid);
    if (!channel) {
        log_error( "l2cap_decline_connection_internal called but local_cid 0x%x not found", local_cid);
        return;
    }
    channel->state  = L2CAP_STATE_WILL_SEND_CONNECTION_RESPONSE_DECLINE;
    channel->reason = reason;
    l2cap_run();
}

void l2cap_signaling_handle_configure_request(l2cap_channel_t *channel, uint8_t *command){

    channel->remote_sig_id = command[L2CAP_SIGNALING_COMMAND_SIGID_OFFSET];

    // accept the other's configuration options
    uint16_t end_pos = 4 + READ_BT_16(command, L2CAP_SIGNALING_COMMAND_LENGTH_OFFSET);
    uint16_t pos     = 8;
    while (pos < end_pos){
        uint8_t type   = command[pos++];
        uint8_t length = command[pos++];
        // MTU { type(8): 1, len(8):2, MTU(16) }
        if ((type & 0x7f) == 1 && length == 2){
            channel->remote_mtu = READ_BT_16(command, pos);
            // log_info("l2cap cid 0x%02x, remote mtu %u\n", channel->local_cid, channel->remote_mtu);
        }
        pos += length;
    }
}

static int l2cap_channel_ready_for_open(l2cap_channel_t *channel){
    // log_info("l2cap_channel_ready_for_open 0x%02x\n", channel->state_var);
    if ((channel->state_var & L2CAP_CHANNEL_STATE_VAR_RCVD_CONF_RSP) == 0) return 0;
    if ((channel->state_var & L2CAP_CHANNEL_STATE_VAR_SENT_CONF_RSP) == 0) return 0;
    return 1;
}


void l2cap_signaling_handler_channel(l2cap_channel_t *channel, uint8_t *command){

    uint8_t  code       = command[L2CAP_SIGNALING_COMMAND_CODE_OFFSET];
    uint8_t  identifier = command[L2CAP_SIGNALING_COMMAND_SIGID_OFFSET];
    uint16_t result = 0;
    
    log_info("L2CAP signaling handler code %u, state %u\n", code, channel->state);
    
    // handle DISCONNECT REQUESTS seperately
    if (code == DISCONNECTION_REQUEST){
        switch (channel->state){
            case L2CAP_STATE_CONFIG:
            case L2CAP_STATE_OPEN:
            case L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST:
            case L2CAP_STATE_WAIT_DISCONNECT:
                l2cap_handle_disconnect_request(channel, identifier);
                break;

            default:
                // ignore in other states
                break;
        }
        return;
    }
    
    // @STATEMACHINE(l2cap)
    switch (channel->state) {
            
        case L2CAP_STATE_WAIT_CONNECT_RSP:
            switch (code){
                case CONNECTION_RESPONSE:
                    result = READ_BT_16 (command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET+4);
                    switch (result) {
                        case 0:
                            // successful connection
                            channel->remote_cid = READ_BT_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET);
                            channel->state = L2CAP_STATE_CONFIG;
                            channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_REQ);
                            break;
                        case 1:
                            // connection pending. get some coffee
                            break;
                        default:
                            // channel closed
                            channel->state = L2CAP_STATE_CLOSED;

                            // map l2cap connection response result to BTstack status enumeration
                            l2cap_emit_channel_opened(channel, L2CAP_CONNECTION_RESPONSE_RESULT_SUCCESSFUL + result);
                            
                            // drop link key if security block
                            if (L2CAP_CONNECTION_RESPONSE_RESULT_SUCCESSFUL + result == L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_SECURITY){
                                hci_drop_link_key_for_bd_addr(&channel->address);
                            }
                            
                            // discard channel
                            linked_list_remove(&l2cap_channels, (linked_item_t *) channel);
                            btstack_memory_l2cap_channel_free(channel);
                            break;
                    }
                    break;
                    
                default:
                    //@TODO: implement other signaling packets
                    break;
            }
            break;

        case L2CAP_STATE_CONFIG:
            switch (code) {
                case CONFIGURE_REQUEST:
                    channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_RCVD_CONF_REQ);
                    channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_SEND_CONF_RSP);
                    l2cap_signaling_handle_configure_request(channel, command);
                    break;
                case CONFIGURE_RESPONSE:
                    channelStateVarSetFlag(channel, L2CAP_CHANNEL_STATE_VAR_RCVD_CONF_RSP);
                    break;
                default:
                    break;
            }
            if (l2cap_channel_ready_for_open(channel)){
                // for open:
                channel->state = L2CAP_STATE_OPEN;
                l2cap_emit_channel_opened(channel, 0);
                l2cap_emit_credits(channel, 1);
            }
            break;
            
        case L2CAP_STATE_WAIT_DISCONNECT:
            switch (code) {
                case DISCONNECTION_RESPONSE:
                    l2cap_finialize_channel_close(channel);
                    break;
                default:
                    //@TODO: implement other signaling packets
                    break;
            }
            break;
            
        case L2CAP_STATE_CLOSED:
            // @TODO handle incoming requests
            break;
            
        case L2CAP_STATE_OPEN:
            //@TODO: implement other signaling packets, e.g. re-configure
            break;
        default:
            break;
    }
    // log_info("new state %u\n", channel->state);
}


void l2cap_signaling_handler_dispatch( hci_con_handle_t handle, uint8_t * command){
    
    // get code, signalind identifier and command len
    uint8_t code   = command[L2CAP_SIGNALING_COMMAND_CODE_OFFSET];
    uint8_t sig_id = command[L2CAP_SIGNALING_COMMAND_SIGID_OFFSET];
    
    // not for a particular channel, and not CONNECTION_REQUEST, ECHO_[REQUEST|RESPONSE], INFORMATION_REQUEST 
    if (code < 1 || code == ECHO_RESPONSE || code > INFORMATION_REQUEST){
        return;
    }

    // general commands without an assigned channel
    switch(code) {
            
        case CONNECTION_REQUEST: {
            uint16_t psm =        READ_BT_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET);
            uint16_t source_cid = READ_BT_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET+2);
            l2cap_handle_connection_request(handle, sig_id, psm, source_cid);
            return;
        }
            
        case ECHO_REQUEST:
            l2cap_register_signaling_response(handle, code, sig_id, 0);
            return;
            
        case INFORMATION_REQUEST: {
            uint16_t infoType = READ_BT_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET);
            l2cap_register_signaling_response(handle, code, sig_id, infoType);
            return;
        }
            
        default:
            break;
    }
    
    
    // Get potential destination CID
    uint16_t dest_cid = READ_BT_16(command, L2CAP_SIGNALING_COMMAND_DATA_OFFSET);
    
    // Find channel for this sig_id and connection handle
    linked_item_t *it;
    for (it = (linked_item_t *) l2cap_channels; it ; it = it->next){
        l2cap_channel_t * channel = (l2cap_channel_t *) it;
        if (channel->handle == handle) {
            if (code & 1) {
                // match odd commands (responses) by previous signaling identifier 
                if (channel->local_sig_id == sig_id) {
                    l2cap_signaling_handler_channel(channel, command);
                    break;
                }
            } else {
                // match even commands (requests) by local channel id
                if (channel->local_cid == dest_cid) {
                    l2cap_signaling_handler_channel(channel, command);
                    break;
                }
            }
        }
    }
}

void l2cap_acl_handler( uint8_t *packet, uint16_t size ){
        
    // Get Channel ID
    uint16_t channel_id = READ_L2CAP_CHANNEL_ID(packet); 
    hci_con_handle_t handle = READ_ACL_CONNECTION_HANDLE(packet);
    
    switch (channel_id) {
            
        case L2CAP_CID_SIGNALING: {
            
            uint16_t command_offset = 8;
            while (command_offset < size) {                
                
                // handle signaling commands
                l2cap_signaling_handler_dispatch(handle, &packet[command_offset]);
                
                // increment command_offset
                command_offset += L2CAP_SIGNALING_COMMAND_DATA_OFFSET + READ_BT_16(packet, command_offset + L2CAP_SIGNALING_COMMAND_LENGTH_OFFSET);
            }
            break;
        }
            
        case L2CAP_CID_ATTRIBUTE_PROTOCOL:
            if (attribute_protocol_packet_handler) {
                (*attribute_protocol_packet_handler)(ATT_DATA_PACKET, handle, &packet[COMPLETE_L2CAP_HEADER], size-COMPLETE_L2CAP_HEADER);
            }
            break;

        case L2CAP_CID_SECURITY_MANAGER_PROTOCOL:
            if (security_protocol_packet_handler) {
                (*security_protocol_packet_handler)(SM_DATA_PACKET, handle, &packet[COMPLETE_L2CAP_HEADER], size-COMPLETE_L2CAP_HEADER);
            }
            break;
            
        default: {
            // Find channel for this channel_id and connection handle
            l2cap_channel_t * channel = l2cap_get_channel_for_local_cid(channel_id);
            if (channel) {
                l2cap_dispatch(channel, L2CAP_DATA_PACKET, &packet[COMPLETE_L2CAP_HEADER], size-COMPLETE_L2CAP_HEADER);
            }
            break;
        }
    }
    
    l2cap_run();
}

static void l2cap_packet_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            l2cap_event_handler(packet, size);
            break;
        case HCI_ACL_DATA_PACKET:
            l2cap_acl_handler(packet, size);
            break;
        default:
            break;
    }
}

// finalize closed channel - l2cap_handle_disconnect_request & DISCONNECTION_RESPONSE
void l2cap_finialize_channel_close(l2cap_channel_t *channel){
    channel->state = L2CAP_STATE_CLOSED;
    l2cap_emit_channel_closed(channel);
    // discard channel
    linked_list_remove(&l2cap_channels, (linked_item_t *) channel);
    btstack_memory_l2cap_channel_free(channel);
}

l2cap_service_t * l2cap_get_service(uint16_t psm){
    linked_item_t *it;
    
    // close open channels
    for (it = (linked_item_t *) l2cap_services; it ; it = it->next){
        l2cap_service_t * service = ((l2cap_service_t *) it);
        if ( service->psm == psm){
            return service;
        };
    }
    return NULL;
}

void l2cap_register_service_internal(void *connection, btstack_packet_handler_t packet_handler, uint16_t psm, uint16_t mtu){
    
    log_info("L2CAP_REGISTER_SERVICE psm 0x%x mtu %u", psm, mtu);
    
    // check for alread registered psm 
    // TODO: emit error event
    l2cap_service_t *service = l2cap_get_service(psm);
    if (service) {
        log_error("l2cap_register_service_internal: PSM %u already registered\n", psm);
        l2cap_emit_service_registered(connection, L2CAP_SERVICE_ALREADY_REGISTERED, psm);
        return;
    }
    
    // alloc structure
    // TODO: emit error event
    service = (l2cap_service_t *) btstack_memory_l2cap_service_get();
    if (!service) {
        log_error("l2cap_register_service_internal: no memory for l2cap_service_t\n");
        l2cap_emit_service_registered(connection, BTSTACK_MEMORY_ALLOC_FAILED, psm);
        return;
    }
    
    // fill in 
    service->psm = psm;
    service->mtu = mtu;
    service->connection = connection;
    service->packet_handler = packet_handler;

    // add to services list
    linked_list_add(&l2cap_services, (linked_item_t *) service);
    
    // enable page scan
    hci_connectable_control(1);

    // done
    l2cap_emit_service_registered(connection, 0, psm);
}

void l2cap_unregister_service_internal(void *connection, uint16_t psm){
    
    log_info("L2CAP_UNREGISTER_SERVICE psm 0x%x", psm);

    l2cap_service_t *service = l2cap_get_service(psm);
    if (!service) return;
    linked_list_remove(&l2cap_services, (linked_item_t *) service);
    btstack_memory_l2cap_service_free(service);
    
    // disable page scan when no services registered
    if (!linked_list_empty(&l2cap_services)) return;
    hci_connectable_control(0);
}

//
void l2cap_close_connection(void *connection){
    linked_item_t *it;
    
    // close open channels - note to myself: no channel is freed, so no new for fancy iterator tricks
    l2cap_channel_t * channel;
    for (it = (linked_item_t *) l2cap_channels; it ; it = it->next){
        channel = (l2cap_channel_t *) it;
        if (channel->connection == connection) {
            channel->state = L2CAP_STATE_WILL_SEND_DISCONNECT_REQUEST;
        }
    }   
    
    // unregister services
    it = (linked_item_t *) &l2cap_services;
    while (it->next) {
        l2cap_service_t * service = (l2cap_service_t *) it->next;
        if (service->connection == connection){
            it->next = it->next->next;
            btstack_memory_l2cap_service_free(service);
        } else {
            it = it->next;
        }
    }
    
    // process
    l2cap_run();
}

// Bluetooth 4.0 - allows to register handler for Attribute Protocol and Security Manager Protocol
void l2cap_register_fixed_channel(btstack_packet_handler_t packet_handler, uint16_t channel_id) {
    switch(channel_id){
        case L2CAP_CID_ATTRIBUTE_PROTOCOL:
            attribute_protocol_packet_handler = packet_handler;
            break;
        case L2CAP_CID_SECURITY_MANAGER_PROTOCOL:
            security_protocol_packet_handler = packet_handler;
            break;
    }
}

