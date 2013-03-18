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
 *  hci.c
 *
 *  Created by Matthias Ringwald on 4/29/09.
 *
 */

#include "config.h"

#include "hci.h"

#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#ifndef EMBEDDED
#include <unistd.h> // gethostbyname
#include <btstack/version.h>
#endif

#include "btstack_memory.h"
#include "debug.h"
#include "hci_dump.h"

#include <btstack/hci_cmds.h>

#define HCI_CONNECTION_TIMEOUT_MS 10000

#ifdef USE_BLUETOOL
#include "bt_control_iphone.h"
#endif

static void hci_update_scan_enable(void);

// the STACK is here
static hci_stack_t       hci_stack;

/**
 * get connection for a given handle
 *
 * @return connection OR NULL, if not found
 */
hci_connection_t * connection_for_handle(hci_con_handle_t con_handle){
    linked_item_t *it;
    for (it = (linked_item_t *) hci_stack.connections; it ; it = it->next){
        if ( ((hci_connection_t *) it)->con_handle == con_handle){
            return (hci_connection_t *) it;
        }
    }
    return NULL;
}

static void hci_connection_timeout_handler(timer_source_t *timer){
    hci_connection_t * connection = (hci_connection_t *) linked_item_get_user(&timer->item);
#ifdef HAVE_TIME
    struct timeval tv;
    gettimeofday(&tv, NULL);
    if (tv.tv_sec >= connection->timestamp.tv_sec + HCI_CONNECTION_TIMEOUT_MS/1000) {
        // connections might be timed out
        hci_emit_l2cap_check_timeout(connection);
    }
#endif
#ifdef HAVE_TICK
    if (embedded_get_ticks() > connection->timestamp + embedded_ticks_for_ms(HCI_CONNECTION_TIMEOUT_MS)){
        // connections might be timed out
        hci_emit_l2cap_check_timeout(connection);
    }
#endif
    run_loop_set_timer(timer, HCI_CONNECTION_TIMEOUT_MS);
    run_loop_add_timer(timer);
}

static void hci_connection_timestamp(hci_connection_t *connection){
#ifdef HAVE_TIME
    gettimeofday(&connection->timestamp, NULL);
#endif
#ifdef HAVE_TICK
    connection->timestamp = embedded_get_ticks();
#endif
}

/**
 * create connection for given address
 *
 * @return connection OR NULL, if no memory left
 */
static hci_connection_t * create_connection_for_addr(bd_addr_t addr){
    hci_connection_t * conn = (hci_connection_t *) btstack_memory_hci_connection_get();
    if (!conn) return NULL;
    BD_ADDR_COPY(conn->address, addr);
    conn->con_handle = 0xffff;
    conn->authentication_flags = AUTH_FLAGS_NONE;
    linked_item_set_user(&conn->timeout.item, conn);
    conn->timeout.process = hci_connection_timeout_handler;
    hci_connection_timestamp(conn);
    conn->acl_recombination_length = 0;
    conn->acl_recombination_pos = 0;
    conn->num_acl_packets_sent = 0;
    linked_list_add(&hci_stack.connections, (linked_item_t *) conn);
    return conn;
}

/**
 * get connection for given address
 *
 * @return connection OR NULL, if not found
 */
static hci_connection_t * connection_for_address(bd_addr_t address){
    linked_item_t *it;
    for (it = (linked_item_t *) hci_stack.connections; it ; it = it->next){
        if ( ! BD_ADDR_CMP( ((hci_connection_t *) it)->address, address) ){
            return (hci_connection_t *) it;
        }
    }
    return NULL;
}

inline static void connectionSetAuthenticationFlags(hci_connection_t * conn, hci_authentication_flags_t flags){
    conn->authentication_flags = (hci_authentication_flags_t)(conn->authentication_flags | flags);
}

inline static void connectionClearAuthenticationFlags(hci_connection_t * conn, hci_authentication_flags_t flags){
    conn->authentication_flags = (hci_authentication_flags_t)(conn->authentication_flags & ~flags);
}


/**
 * add authentication flags and reset timer
 */
static void hci_add_connection_flags_for_flipped_bd_addr(uint8_t *bd_addr, hci_authentication_flags_t flags){
    bd_addr_t addr;
    bt_flip_addr(addr, *(bd_addr_t *) bd_addr);
    hci_connection_t * conn = connection_for_address(addr);
    if (conn) {
        connectionSetAuthenticationFlags(conn, flags);
        hci_connection_timestamp(conn);
    }
}

int  hci_authentication_active_for_handle(hci_con_handle_t handle){
    hci_connection_t * conn = connection_for_handle(handle);
    if (!conn) return 0;
    if (!conn->authentication_flags) return 0;
    if (conn->authentication_flags & SENT_LINK_KEY_REPLY) return 0;
    if (conn->authentication_flags & RECV_LINK_KEY_NOTIFICATION) return 0;
    return 1;
}

void hci_drop_link_key_for_bd_addr(bd_addr_t *addr){
    if (hci_stack.remote_device_db) {
        hci_stack.remote_device_db->delete_link_key(addr);
    }
}


/**
 * count connections
 */
static int nr_hci_connections(void){
    int count = 0;
    linked_item_t *it;
    for (it = (linked_item_t *) hci_stack.connections; it ; it = it->next, count++);
    return count;
}

/** 
 * Dummy handler called by HCI
 */
static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
}

uint8_t hci_number_outgoing_packets(hci_con_handle_t handle){
    hci_connection_t * connection = connection_for_handle(handle);
    if (!connection) {
        log_error("hci_number_outgoing_packets connectino for handle %u does not exist!\n", handle);
        return 0;
    }
    return connection->num_acl_packets_sent;
}

uint8_t hci_number_free_acl_slots(){
    uint8_t free_slots = hci_stack.total_num_acl_packets;
    linked_item_t *it;
    for (it = (linked_item_t *) hci_stack.connections; it ; it = it->next){
        hci_connection_t * connection = (hci_connection_t *) it;
        if (free_slots < connection->num_acl_packets_sent) {
            log_error("hci_number_free_acl_slots: sum of outgoing packets > total acl packets!\n");
            return 0;
        }
        free_slots -= connection->num_acl_packets_sent;
    }
    return free_slots;
}

int hci_can_send_packet_now(uint8_t packet_type){

    // check for async hci transport implementations
    if (hci_stack.hci_transport->can_send_packet_now){
        if (!hci_stack.hci_transport->can_send_packet_now(packet_type)){
            return 0;
        }
    }
    
    // check regular Bluetooth flow control
    switch (packet_type) {
        case HCI_ACL_DATA_PACKET:
            return hci_number_free_acl_slots();
        case HCI_COMMAND_DATA_PACKET:
            return hci_stack.num_cmd_packets;
        default:
            return 0;
    }
}

int hci_send_acl_packet(uint8_t *packet, int size){

    // check for free places on BT module
    if (!hci_number_free_acl_slots()) return BTSTACK_ACL_BUFFERS_FULL;
    
    hci_con_handle_t con_handle = READ_ACL_CONNECTION_HANDLE(packet);
    hci_connection_t *connection = connection_for_handle( con_handle);
    if (!connection) return 0;
    hci_connection_timestamp(connection);
    
    // count packet
    connection->num_acl_packets_sent++;
    // log_info("hci_send_acl_packet - handle %u, sent %u\n", connection->con_handle, connection->num_acl_packets_sent);

    // send packet 
    int err = hci_stack.hci_transport->send_packet(HCI_ACL_DATA_PACKET, packet, size);
    
    return err;
}

static void acl_handler(uint8_t *packet, int size){

    // get info
    hci_con_handle_t con_handle = READ_ACL_CONNECTION_HANDLE(packet);
    hci_connection_t *conn      = connection_for_handle(con_handle);
    uint8_t  acl_flags          = READ_ACL_FLAGS(packet);
    uint16_t acl_length         = READ_ACL_LENGTH(packet);

    // ignore non-registered handle
    if (!conn){
        log_error( "hci.c: acl_handler called with non-registered handle %u!\n" , con_handle);
        return;
    }
    
    // update idle timestamp
    hci_connection_timestamp(conn);
    
    // handle different packet types
    switch (acl_flags & 0x03) {
            
        case 0x01: // continuation fragment
            
            // sanity check
            if (conn->acl_recombination_pos == 0) {
                log_error( "ACL Cont Fragment but no first fragment for handle 0x%02x\n", con_handle);
                return;
            }
            
            // append fragment payload (header already stored)
            memcpy(&conn->acl_recombination_buffer[conn->acl_recombination_pos], &packet[4], acl_length );
            conn->acl_recombination_pos += acl_length;
            
            // log_error( "ACL Cont Fragment: acl_len %u, combined_len %u, l2cap_len %u\n", acl_length,
            //        conn->acl_recombination_pos, conn->acl_recombination_length);  
            
            // forward complete L2CAP packet if complete. 
            if (conn->acl_recombination_pos >= conn->acl_recombination_length + 4 + 4){ // pos already incl. ACL header
                
                hci_stack.packet_handler(HCI_ACL_DATA_PACKET, conn->acl_recombination_buffer, conn->acl_recombination_pos);
                // reset recombination buffer
                conn->acl_recombination_length = 0;
                conn->acl_recombination_pos = 0;
            }
            break;
            
        case 0x02: { // first fragment
            
            // sanity check
            if (conn->acl_recombination_pos) {
                log_error( "ACL First Fragment but data in buffer for handle 0x%02x\n", con_handle);
                return;
            }

            // peek into L2CAP packet!
            uint16_t l2cap_length = READ_L2CAP_LENGTH( packet );

            // log_error( "ACL First Fragment: acl_len %u, l2cap_len %u\n", acl_length, l2cap_length);

            // compare fragment size to L2CAP packet size
            if (acl_length >= l2cap_length + 4){
                
                // forward fragment as L2CAP packet
                hci_stack.packet_handler(HCI_ACL_DATA_PACKET, packet, acl_length + 4);
            
            } else {
                // store first fragment and tweak acl length for complete package
                memcpy(conn->acl_recombination_buffer, packet, acl_length + 4);
                conn->acl_recombination_pos    = acl_length + 4;
                conn->acl_recombination_length = l2cap_length;
                bt_store_16(conn->acl_recombination_buffer, 2, l2cap_length +4);
            }
            break;
            
        } 
        default:
            log_error( "hci.c: acl_handler called with invalid packet boundary flags %u\n", acl_flags & 0x03);
            return;
    }
    
    // execute main loop
    hci_run();
}

static void hci_shutdown_connection(hci_connection_t *conn){
    log_info("Connection closed: handle %u, %s\n", conn->con_handle, bd_addr_to_str(conn->address));

    // cancel all l2cap connections
    hci_emit_disconnection_complete(conn->con_handle, 0x16);    // terminated by local host

    run_loop_remove_timer(&conn->timeout);
    
    linked_list_remove(&hci_stack.connections, (linked_item_t *) conn);
    btstack_memory_hci_connection_free( conn );
    
    // now it's gone
    hci_emit_nr_connections_changed();
}

static const uint16_t packet_type_sizes[] = {
    0, HCI_ACL_2DH1_SIZE, HCI_ACL_3DH1_SIZE, HCI_ACL_DM1_SIZE,
    HCI_ACL_DH1_SIZE, 0, 0, 0,
    HCI_ACL_2DH3_SIZE, HCI_ACL_3DH3_SIZE, HCI_ACL_DM3_SIZE, HCI_ACL_DH3_SIZE,
    HCI_ACL_2DH5_SIZE, HCI_ACL_3DH5_SIZE, HCI_ACL_DM5_SIZE, HCI_ACL_DH5_SIZE
};

static uint16_t hci_acl_packet_types_for_buffer_size(uint16_t buffer_size){
    uint16_t packet_types = 0;
    int i;
    for (i=0;i<16;i++){
        if (packet_type_sizes[i] == 0) continue;
        if (packet_type_sizes[i] <= buffer_size){
            packet_types |= 1 << i;
        }
    }
    // flip bits for "may not be used"
    packet_types ^= 0x3306;
    return packet_types;
}

uint16_t hci_usable_acl_packet_types(void){
    return hci_stack.packet_types;
}

uint8_t* hci_get_outgoing_acl_packet_buffer(void){
    // hci packet buffer is >= acl data packet length
    return hci_stack.hci_packet_buffer;
}

uint16_t hci_max_acl_data_packet_length(){
    return hci_stack.acl_data_packet_length;
}

// avoid huge local variables
#ifndef EMBEDDED
static device_name_t device_name;
#endif
static void event_handler(uint8_t *packet, int size){
    bd_addr_t addr;
    uint8_t link_type;
    hci_con_handle_t handle;
    hci_connection_t * conn;
    int i;
        
    // printf("HCI:EVENT:%02x\n", packet[0]);
    
    switch (packet[0]) {
                        
        case HCI_EVENT_COMMAND_COMPLETE:
            // get num cmd packets
            // log_info("HCI_EVENT_COMMAND_COMPLETE cmds old %u - new %u\n", hci_stack.num_cmd_packets, packet[2]);
            hci_stack.num_cmd_packets = packet[2];
            
            if (COMMAND_COMPLETE_EVENT(packet, hci_read_buffer_size)){
                // from offset 5
                // status 
                // "The HC_ACL_Data_Packet_Length return parameter will be used to determine the size of the L2CAP segments contained in ACL Data Packets"
                hci_stack.acl_data_packet_length = READ_BT_16(packet, 6);
                // ignore: SCO data packet len (8)
                hci_stack.total_num_acl_packets  = packet[9];
                // ignore: total num SCO packets
                if (hci_stack.state == HCI_STATE_INITIALIZING){
                    // determine usable ACL payload size
                    if (HCI_ACL_PAYLOAD_SIZE < hci_stack.acl_data_packet_length){
                        hci_stack.acl_data_packet_length = HCI_ACL_PAYLOAD_SIZE;
                    }
                    // determine usable ACL packet types
                    hci_stack.packet_types = hci_acl_packet_types_for_buffer_size(hci_stack.acl_data_packet_length);
                    
                    log_info("hci_read_buffer_size: used size %u, count %u, packet types %04x\n",
                             hci_stack.acl_data_packet_length, hci_stack.total_num_acl_packets, hci_stack.packet_types); 
                }
            }
            // Dump local address
            if (COMMAND_COMPLETE_EVENT(packet, hci_read_bd_addr)) {
                bd_addr_t addr;
                bt_flip_addr(addr, &packet[OFFSET_OF_DATA_IN_COMMAND_COMPLETE + 1]);
                log_info("Local Address, Status: 0x%02x: Addr: %s\n",
                    packet[OFFSET_OF_DATA_IN_COMMAND_COMPLETE], bd_addr_to_str(addr));
            }
            if (COMMAND_COMPLETE_EVENT(packet, hci_write_scan_enable)){
                hci_emit_discoverable_enabled(hci_stack.discoverable);
            }
            break;
            
        case HCI_EVENT_COMMAND_STATUS:
            // get num cmd packets
            // log_info("HCI_EVENT_COMMAND_STATUS cmds - old %u - new %u\n", hci_stack.num_cmd_packets, packet[3]);
            hci_stack.num_cmd_packets = packet[3];
            break;
            
        case HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS:
            for (i=0; i<packet[2];i++){
                handle = READ_BT_16(packet, 3 + 2*i);
                uint16_t num_packets = READ_BT_16(packet, 3 + packet[2]*2 + 2*i);
                conn = connection_for_handle(handle);
                if (!conn){
                    log_error("hci_number_completed_packet lists unused con handle %u\n", handle);
                    continue;
                }
                conn->num_acl_packets_sent -= num_packets;
                // log_info("hci_number_completed_packet %u processed for handle %u, outstanding %u\n", num_packets, handle, conn->num_acl_packets_sent);
            }
            break;
            
        case HCI_EVENT_CONNECTION_REQUEST:
            bt_flip_addr(addr, &packet[2]);
            // TODO: eval COD 8-10
            link_type = packet[11];
            log_info("Connection_incoming: %s, type %u\n", bd_addr_to_str(addr), link_type);
            if (link_type == 1) { // ACL
                conn = connection_for_address(addr);
                if (!conn) {
                    conn = create_connection_for_addr(addr);
                }
                if (!conn) {
                    // CONNECTION REJECTED DUE TO LIMITED RESOURCES (0X0D)
                    hci_stack.decline_reason = 0x0d;
                    BD_ADDR_COPY(hci_stack.decline_addr, addr);
                    break;
                }
                conn->state = RECEIVED_CONNECTION_REQUEST;
                hci_run();
            } else {
                // SYNCHRONOUS CONNECTION LIMIT TO A DEVICE EXCEEDED (0X0A)
                hci_stack.decline_reason = 0x0a;
                BD_ADDR_COPY(hci_stack.decline_addr, addr);
            }
            break;
            
        case HCI_EVENT_CONNECTION_COMPLETE:
            // Connection management
            bt_flip_addr(addr, &packet[5]);
            log_info("Connection_complete (status=%u) %s\n", packet[2], bd_addr_to_str(addr));
            conn = connection_for_address(addr);
            if (conn) {
                if (!packet[2]){
                    conn->state = OPEN;
                    conn->con_handle = READ_BT_16(packet, 3);
                    
                    // restart timer
                    run_loop_set_timer(&conn->timeout, HCI_CONNECTION_TIMEOUT_MS);
                    run_loop_add_timer(&conn->timeout);
                    
                    log_info("New connection: handle %u, %s\n", conn->con_handle, bd_addr_to_str(conn->address));
                    
                    hci_emit_nr_connections_changed();
                } else {
                    // connection failed, remove entry
                    linked_list_remove(&hci_stack.connections, (linked_item_t *) conn);
                    btstack_memory_hci_connection_free( conn );
                    
                    // if authentication error, also delete link key
                    if (packet[2] == 0x05) {
                        hci_drop_link_key_for_bd_addr(&addr);
                    }
                }
            }
            break;

        case HCI_EVENT_LINK_KEY_REQUEST:
            log_info("HCI_EVENT_LINK_KEY_REQUEST\n");
            hci_add_connection_flags_for_flipped_bd_addr(&packet[2], RECV_LINK_KEY_REQUEST);
            if (!hci_stack.remote_device_db) break;
            hci_add_connection_flags_for_flipped_bd_addr(&packet[2], HANDLE_LINK_KEY_REQUEST);
            hci_run();
            // request handled by hci_run() as HANDLE_LINK_KEY_REQUEST gets set
            return;
            
        case HCI_EVENT_LINK_KEY_NOTIFICATION:
            hci_add_connection_flags_for_flipped_bd_addr(&packet[2], RECV_LINK_KEY_NOTIFICATION);
            if (!hci_stack.remote_device_db) break;
            bt_flip_addr(addr, &packet[2]);
            hci_stack.remote_device_db->put_link_key(&addr, (link_key_t *) &packet[8]);
            // still forward event to allow dismiss of pairing dialog
            break;
            
        case HCI_EVENT_PIN_CODE_REQUEST:
            hci_add_connection_flags_for_flipped_bd_addr(&packet[2], RECV_PIN_CODE_REQUEST);
            // PIN CODE REQUEST means the link key request didn't succee -> delete stored link key
            if (!hci_stack.remote_device_db) break;
            bt_flip_addr(addr, &packet[2]);
            hci_stack.remote_device_db->delete_link_key(&addr);
            break;
            
#ifndef EMBEDDED
        case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
            if (!hci_stack.remote_device_db) break;
            if (packet[2]) break; // status not ok
            bt_flip_addr(addr, &packet[3]);
            // fix for invalid remote names - terminate on 0xff
            for (i=0; i<248;i++){
                if (packet[9+i] == 0xff){
                    packet[9+i] = 0;
                    break;
                }
            }
            memset(&device_name, 0, sizeof(device_name_t));
            strncpy((char*) device_name, (char*) &packet[9], 248);
            hci_stack.remote_device_db->put_name(&addr, &device_name);
            break;
            
        case HCI_EVENT_INQUIRY_RESULT:
        case HCI_EVENT_INQUIRY_RESULT_WITH_RSSI:
            if (!hci_stack.remote_device_db) break;
            // first send inq result packet
            hci_stack.packet_handler(HCI_EVENT_PACKET, packet, size);
            // then send cached remote names
            for (i=0; i<packet[2];i++){
                bt_flip_addr(addr, &packet[3+i*6]);
                if (hci_stack.remote_device_db->get_name(&addr, &device_name)){
                    hci_emit_remote_name_cached(&addr, &device_name);
                }
            }
            return;
#endif
            
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            if (!packet[2]){
                handle = READ_BT_16(packet, 3);
                hci_connection_t * conn = connection_for_handle(handle);
                if (conn) {
                    hci_shutdown_connection(conn);
                }
            }
            break;
            
        case HCI_EVENT_HARDWARE_ERROR:
            if(hci_stack.control->hw_error){
                (*hci_stack.control->hw_error)();
            }
            break;

#ifdef HAVE_BLE
        case HCI_EVENT_LE_META:
            switch (packet[2]) {
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                    // Connection management
                    bt_flip_addr(addr, &packet[8]);
                    log_info("LE Connection_complete (status=%u) %s\n", packet[3], bd_addr_to_str(addr));
                    // LE connections are auto-accepted, so just create a connection if there isn't one already
                    conn = connection_for_address(addr);
                    if (packet[3]){
                        if (conn){
                            // outgoing connection failed, remove entry
                            linked_list_remove(&hci_stack.connections, (linked_item_t *) conn);
                            btstack_memory_hci_connection_free( conn );
                            
                        }
                        // if authentication error, also delete link key
                        if (packet[3] == 0x05) {
                            hci_drop_link_key_for_bd_addr(&addr);
                        }
                        break;
                    }
                    if (!conn){
                        conn = create_connection_for_addr(addr);
                    }
                    if (!conn){
                        // no memory
                        break;
                    }
                    
                    conn->state = OPEN;
                    conn->con_handle = READ_BT_16(packet, 4);
                    
                    // TODO: store - role, peer address type, conn_interval, conn_latency, supervision timeout, master clock

                    // restart timer
                    // run_loop_set_timer(&conn->timeout, HCI_CONNECTION_TIMEOUT_MS);
                    // run_loop_add_timer(&conn->timeout);
                    
                    log_info("New connection: handle %u, %s\n", conn->con_handle, bd_addr_to_str(conn->address));
                    
                    hci_emit_nr_connections_changed();
                    break;
                    
                default:
                    break;
            }
            break;
#endif            
            
        default:
            break;
    }

    // handle BT initialization
    if (hci_stack.state == HCI_STATE_INITIALIZING){
        // handle H4 synchronization loss on restart
        // if (hci_stack.substate == 1 && packet[0] == HCI_EVENT_HARDWARE_ERROR){
        //    hci_stack.substate = 0;
        // }
        // handle normal init sequence
        if (hci_stack.substate % 2){
            // odd: waiting for event
            if (packet[0] == HCI_EVENT_COMMAND_COMPLETE){
                hci_stack.substate++;
            }
        }
    }
    
    // help with BT sleep
    if (hci_stack.state == HCI_STATE_FALLING_ASLEEP
        && hci_stack.substate == 1
        && COMMAND_COMPLETE_EVENT(packet, hci_write_scan_enable)){
        hci_stack.substate++;
    }
    
    hci_stack.packet_handler(HCI_EVENT_PACKET, packet, size);
	
	// execute main loop
	hci_run();
}

void packet_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            event_handler(packet, size);
            break;
        case HCI_ACL_DATA_PACKET:
            acl_handler(packet, size);
            break;
        default:
            break;
    }
}

/** Register HCI packet handlers */
void hci_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    hci_stack.packet_handler = handler;
}

void hci_init(hci_transport_t *transport, void *config, bt_control_t *control, remote_device_db_t const* remote_device_db){
    
    // reference to use transport layer implementation
    hci_stack.hci_transport = transport;
    
    // references to used control implementation
    hci_stack.control = control;
    
    // reference to used config
    hci_stack.config = config;
    
    // no connections yet
    hci_stack.connections = NULL;
    hci_stack.discoverable = 0;
    hci_stack.connectable = 0;
    
    // no pending cmds
    hci_stack.decline_reason = 0;
    hci_stack.new_scan_enable_value = 0xff;
    
    // higher level handler
    hci_stack.packet_handler = dummy_handler;

    // store and open remote device db
    hci_stack.remote_device_db = remote_device_db;
    if (hci_stack.remote_device_db) {
        hci_stack.remote_device_db->open();
    }
    
    // max acl payload size defined in config.h
    hci_stack.acl_data_packet_length = HCI_ACL_PAYLOAD_SIZE;
    
    // register packet handlers with transport
    transport->register_packet_handler(&packet_handler);

    hci_stack.state = HCI_STATE_OFF;
}

void hci_close(){
    // close remote device db
    if (hci_stack.remote_device_db) {
        hci_stack.remote_device_db->close();
    }
    while (hci_stack.connections) {
        hci_shutdown_connection((hci_connection_t *) hci_stack.connections);
}
    hci_power_control(HCI_POWER_OFF);
}

// State-Module-Driver overview
// state                    module  low-level 
// HCI_STATE_OFF             off      close
// HCI_STATE_INITIALIZING,   on       open
// HCI_STATE_WORKING,        on       open
// HCI_STATE_HALTING,        on       open
// HCI_STATE_SLEEPING,    off/sleep   close
// HCI_STATE_FALLING_ASLEEP  on       open

static int hci_power_control_on(void){
    
    // power on
    int err = 0;
    if (hci_stack.control && hci_stack.control->on){
        err = (*hci_stack.control->on)(hci_stack.config);
    }
    if (err){
        log_error( "POWER_ON failed\n");
        hci_emit_hci_open_failed();
        return err;
    }
    
    // open low-level device
    err = hci_stack.hci_transport->open(hci_stack.config);
    if (err){
        log_error( "HCI_INIT failed, turning Bluetooth off again\n");
        if (hci_stack.control && hci_stack.control->off){
            (*hci_stack.control->off)(hci_stack.config);
        }
        hci_emit_hci_open_failed();
        return err;
    }
    return 0;
}

static void hci_power_control_off(void){
    
    log_info("hci_power_control_off\n");

    // close low-level device
    hci_stack.hci_transport->close(hci_stack.config);

    log_info("hci_power_control_off - hci_transport closed\n");
    
    // power off
    if (hci_stack.control && hci_stack.control->off){
        (*hci_stack.control->off)(hci_stack.config);
    }
    
    log_info("hci_power_control_off - control closed\n");

    hci_stack.state = HCI_STATE_OFF;
}

static void hci_power_control_sleep(void){
    
    log_info("hci_power_control_sleep\n");
    
#if 0
    // don't close serial port during sleep
    
    // close low-level device
    hci_stack.hci_transport->close(hci_stack.config);
#endif
    
    // sleep mode
    if (hci_stack.control && hci_stack.control->sleep){
        (*hci_stack.control->sleep)(hci_stack.config);
    }
    
    hci_stack.state = HCI_STATE_SLEEPING;
}

static int hci_power_control_wake(void){
    
    log_info("hci_power_control_wake\n");

    // wake on
    if (hci_stack.control && hci_stack.control->wake){
        (*hci_stack.control->wake)(hci_stack.config);
    }
    
#if 0
    // open low-level device
    int err = hci_stack.hci_transport->open(hci_stack.config);
    if (err){
        log_error( "HCI_INIT failed, turning Bluetooth off again\n");
        if (hci_stack.control && hci_stack.control->off){
            (*hci_stack.control->off)(hci_stack.config);
        }
        hci_emit_hci_open_failed();
        return err;
    }
#endif
    
    return 0;
}


int hci_power_control(HCI_POWER_MODE power_mode){
    
    log_info("hci_power_control: %u, current mode %u\n", power_mode, hci_stack.state);
    
    int err = 0;
    switch (hci_stack.state){
            
        case HCI_STATE_OFF:
            switch (power_mode){
                case HCI_POWER_ON:
                    err = hci_power_control_on();
                    if (err) return err;
                    // set up state machine
                    hci_stack.num_cmd_packets = 1; // assume that one cmd can be sent
                    hci_stack.state = HCI_STATE_INITIALIZING;
                    hci_stack.substate = 0;
                    break;
                case HCI_POWER_OFF:
                    // do nothing
                    break;  
                case HCI_POWER_SLEEP:
                    // do nothing (with SLEEP == OFF)
                    break;
            }
            break;
            
        case HCI_STATE_INITIALIZING:
            switch (power_mode){
                case HCI_POWER_ON:
                    // do nothing
                    break;
                case HCI_POWER_OFF:
                    // no connections yet, just turn it off
                    hci_power_control_off();
                    break;  
                case HCI_POWER_SLEEP:
                    // no connections yet, just turn it off
                    hci_power_control_sleep();
                    break;
            }
            break;
            
        case HCI_STATE_WORKING:
            switch (power_mode){
                case HCI_POWER_ON:
                    // do nothing
                    break;
                case HCI_POWER_OFF:
                    // see hci_run
                    hci_stack.state = HCI_STATE_HALTING;
                    break;  
                case HCI_POWER_SLEEP:
                    // see hci_run
                    hci_stack.state = HCI_STATE_FALLING_ASLEEP;
                    hci_stack.substate = 0;
                    break;
            }
            break;
            
        case HCI_STATE_HALTING:
            switch (power_mode){
                case HCI_POWER_ON:
                    // set up state machine
                    hci_stack.state = HCI_STATE_INITIALIZING;
                    hci_stack.substate = 0;
                    break;
                case HCI_POWER_OFF:
                    // do nothing
                    break;  
                case HCI_POWER_SLEEP:
                    // see hci_run
                    hci_stack.state = HCI_STATE_FALLING_ASLEEP;
                    hci_stack.substate = 0;
                    break;
            }
            break;
            
        case HCI_STATE_FALLING_ASLEEP:
            switch (power_mode){
                case HCI_POWER_ON:

#if defined(USE_POWERMANAGEMENT) && defined(USE_BLUETOOL)
                    // nothing to do, if H4 supports power management
                    if (bt_control_iphone_power_management_enabled()){
                        hci_stack.state = HCI_STATE_INITIALIZING;
                        hci_stack.substate = 6;
                        break;
                    }
#endif
                    // set up state machine
                    hci_stack.num_cmd_packets = 1; // assume that one cmd can be sent
                    hci_stack.state = HCI_STATE_INITIALIZING;
                    hci_stack.substate = 0;
                    break;
                case HCI_POWER_OFF:
                    // see hci_run
                    hci_stack.state = HCI_STATE_HALTING;
                    break;  
                case HCI_POWER_SLEEP:
                    // do nothing
                    break;
            }
            break;
            
        case HCI_STATE_SLEEPING:
            switch (power_mode){
                case HCI_POWER_ON:
                    
#if defined(USE_POWERMANAGEMENT) && defined(USE_BLUETOOL)
                    // nothing to do, if H4 supports power management
                    if (bt_control_iphone_power_management_enabled()){
                        hci_stack.state = HCI_STATE_INITIALIZING;
                        hci_stack.substate = 6;
                        hci_update_scan_enable();
                        break;
                    }
#endif
                    err = hci_power_control_wake();
                    if (err) return err;
                    // set up state machine
                    hci_stack.num_cmd_packets = 1; // assume that one cmd can be sent
                    hci_stack.state = HCI_STATE_INITIALIZING;
                    hci_stack.substate = 0;
                    break;
                case HCI_POWER_OFF:
                    hci_stack.state = HCI_STATE_HALTING;
                    break;  
                case HCI_POWER_SLEEP:
                    // do nothing
                    break;
            }
            break;
    }

    // create internal event
	hci_emit_state();
    
	// trigger next/first action
	hci_run();
	
    return 0;
}

static void hci_update_scan_enable(void){
    // 2 = page scan, 1 = inq scan
    hci_stack.new_scan_enable_value  = hci_stack.connectable << 1 | hci_stack.discoverable;
    hci_run();
}

void hci_discoverable_control(uint8_t enable){
    if (enable) enable = 1; // normalize argument
    
    if (hci_stack.discoverable == enable){
        hci_emit_discoverable_enabled(hci_stack.discoverable);
        return;
    }

    hci_stack.discoverable = enable;
    hci_update_scan_enable();
}

void hci_connectable_control(uint8_t enable){
    if (enable) enable = 1; // normalize argument
    
    // don't emit event
    if (hci_stack.connectable == enable) return;

    hci_stack.connectable = enable;
    hci_update_scan_enable();
}

void hci_run(){
        
    hci_connection_t * connection;
    linked_item_t * it;
    
    if (!hci_can_send_packet_now(HCI_COMMAND_DATA_PACKET)) return;

    // global/non-connection oriented commands
    
    // decline incoming connections
    if (hci_stack.decline_reason){
        uint8_t reason = hci_stack.decline_reason;
        hci_stack.decline_reason = 0;
        hci_send_cmd(&hci_reject_connection_request, hci_stack.decline_addr, reason);
    }

    if (!hci_can_send_packet_now(HCI_COMMAND_DATA_PACKET)) return;

    // send scan enable
    if (hci_stack.new_scan_enable_value != 0xff){
        hci_send_cmd(&hci_write_scan_enable, hci_stack.new_scan_enable_value);
        hci_stack.new_scan_enable_value = 0xff;
    }
    
    // send pending HCI commands
    for (it = (linked_item_t *) hci_stack.connections; it ; it = it->next){

        if (!hci_can_send_packet_now(HCI_COMMAND_DATA_PACKET)) return;

        connection = (hci_connection_t *) it;
        
        if (connection->state == RECEIVED_CONNECTION_REQUEST){
            log_info("sending hci_accept_connection_request\n");
            hci_send_cmd(&hci_accept_connection_request, connection->address, 1);
            connection->state = ACCEPTED_CONNECTION_REQUEST;
        }

        if (!hci_can_send_packet_now(HCI_COMMAND_DATA_PACKET)) return;
        
        if (connection->authentication_flags & HANDLE_LINK_KEY_REQUEST){
            link_key_t link_key;
            log_info("responding to link key request\n");
            if ( hci_stack.remote_device_db->get_link_key( &connection->address, &link_key)){
               hci_send_cmd(&hci_link_key_request_reply, connection->address, &link_key);
            } else {
               hci_send_cmd(&hci_link_key_request_negative_reply, connection->address);
            }
            connectionClearAuthenticationFlags(connection, HANDLE_LINK_KEY_REQUEST);
        }
    }

    if (!hci_can_send_packet_now(HCI_COMMAND_DATA_PACKET)) return;
        
    switch (hci_stack.state){
        case HCI_STATE_INITIALIZING:
            // log_info("hci_init: substate %u\n", hci_stack.substate);
            if (hci_stack.substate % 2) {
                // odd: waiting for command completion
                return;
            }
            switch (hci_stack.substate >> 1){
                case 0: // RESET
                    hci_send_cmd(&hci_reset);
                    if (hci_stack.config == 0 || ((hci_uart_config_t *)hci_stack.config)->baudrate_main == 0){
                        // skip baud change
                        hci_stack.substate = 4; // >> 1 = 2
                    }
                    break;
                case 1: // SEND BAUD CHANGE
                    hci_stack.control->baudrate_cmd(hci_stack.config, ((hci_uart_config_t *)hci_stack.config)->baudrate_main, hci_stack.hci_packet_buffer);
                    hci_send_cmd_packet(hci_stack.hci_packet_buffer, 3 + hci_stack.hci_packet_buffer[2]);
                    break;
                case 2: // LOCAL BAUD CHANGE
                    hci_stack.hci_transport->set_baudrate(((hci_uart_config_t *)hci_stack.config)->baudrate_main);
                    hci_stack.substate += 2;
                    // break missing here for fall through
                    
                case 3:
                    // custom initialization
                    if (hci_stack.control && hci_stack.control->next_cmd){
                        int valid_cmd = (*hci_stack.control->next_cmd)(hci_stack.config, hci_stack.hci_packet_buffer);
                        if (valid_cmd){
                            int size = 3 + hci_stack.hci_packet_buffer[2];
                            hci_stack.hci_transport->send_packet(HCI_COMMAND_DATA_PACKET, hci_stack.hci_packet_buffer, size);
                            hci_stack.substate = 4; // more init commands
                            break;
                        }
                        log_info("hci_run: init script done\n\r");
                    }
                    // otherwise continue
					hci_send_cmd(&hci_read_bd_addr);
					break;
				case 4:
					hci_send_cmd(&hci_read_buffer_size);
					break;
                case 5:
                    // ca. 15 sec
                    hci_send_cmd(&hci_write_page_timeout, 0x6000);
                    break;
				case 6:
					hci_send_cmd(&hci_write_scan_enable, (hci_stack.connectable << 1) | hci_stack.discoverable); // page scan
					break;
                case 7:
#ifndef EMBEDDED
                {
                    char hostname[30];
                    gethostname(hostname, 30);
                    hostname[29] = '\0';
                    hci_send_cmd(&hci_write_local_name, hostname);
                    break;
                }
                case 8:
#ifdef USE_BLUETOOL
                    hci_send_cmd(&hci_write_class_of_device, 0x007a020c); // Smartphone
                    break;
                    
                case 9:
#endif
#endif
                    // done.
                    hci_stack.state = HCI_STATE_WORKING;
                    hci_emit_state();
                    break;
                default:
                    break;
            }
            hci_stack.substate++;
            break;
            
        case HCI_STATE_HALTING:

            log_info("HCI_STATE_HALTING\n");
            // close all open connections
            connection =  (hci_connection_t *) hci_stack.connections;
            if (connection){
                
                // send disconnect
                if (!hci_can_send_packet_now(HCI_COMMAND_DATA_PACKET)) return;
                
                log_info("HCI_STATE_HALTING, connection %p, handle %u\n", connection, (uint16_t)connection->con_handle);
                hci_send_cmd(&hci_disconnect, connection->con_handle, 0x13);  // remote closed connection

                // send disconnected event right away - causes higher layer connections to get closed, too.
                hci_shutdown_connection(connection);
                return;
            }
            log_info("HCI_STATE_HALTING, calling off\n");
            
            // switch mode
            hci_power_control_off();
            
            log_info("HCI_STATE_HALTING, emitting state\n");
            hci_emit_state();
            log_info("HCI_STATE_HALTING, done\n");
            break;
            
        case HCI_STATE_FALLING_ASLEEP:
            switch(hci_stack.substate) {
                case 0:
                    log_info("HCI_STATE_FALLING_ASLEEP\n");
                    // close all open connections
                    connection =  (hci_connection_t *) hci_stack.connections;

#if defined(USE_POWERMANAGEMENT) && defined(USE_BLUETOOL)
                    // don't close connections, if H4 supports power management
                    if (bt_control_iphone_power_management_enabled()){
                        connection = NULL;
                    }
#endif
                    if (connection){
                        
                        // send disconnect
                        if (!hci_can_send_packet_now(HCI_COMMAND_DATA_PACKET)) return;

                        log_info("HCI_STATE_FALLING_ASLEEP, connection %p, handle %u\n", connection, (uint16_t)connection->con_handle);
                        hci_send_cmd(&hci_disconnect, connection->con_handle, 0x13);  // remote closed connection
                        
                        // send disconnected event right away - causes higher layer connections to get closed, too.
                        hci_shutdown_connection(connection);
                        return;
                    }
                    
                    // disable page and inquiry scan
                    if (!hci_can_send_packet_now(HCI_COMMAND_DATA_PACKET)) return;
                    
                    log_info("HCI_STATE_HALTING, disabling inq cans\n");
                    hci_send_cmd(&hci_write_scan_enable, hci_stack.connectable << 1); // drop inquiry scan but keep page scan
                    
                    // continue in next sub state
                    hci_stack.substate++;
                    break;
                case 1:
                    // wait for command complete "hci_write_scan_enable" in event_handler();
                    break;
                case 2:
                    log_info("HCI_STATE_HALTING, calling sleep\n");
#if defined(USE_POWERMANAGEMENT) && defined(USE_BLUETOOL)
                    // don't actually go to sleep, if H4 supports power management
                    if (bt_control_iphone_power_management_enabled()){
                        // SLEEP MODE reached
                        hci_stack.state = HCI_STATE_SLEEPING; 
                        hci_emit_state();
                        break;
                    }
#endif
                    // switch mode
                    hci_power_control_sleep();  // changes hci_stack.state to SLEEP
                    hci_emit_state();
                    break;
                    
                default:
                    break;
            }
            break;
            
        default:
            break;
    }
}

int hci_send_cmd_packet(uint8_t *packet, int size){
    bd_addr_t addr;
    hci_connection_t * conn;
    // house-keeping
    
    // create_connection?
    if (IS_COMMAND(packet, hci_create_connection)){
        bt_flip_addr(addr, &packet[3]);
        log_info("Create_connection to %s\n", bd_addr_to_str(addr));
        conn = connection_for_address(addr);
        if (conn) {
            // if connection exists
            if (conn->state == OPEN) {
                // and OPEN, emit connection complete command
                hci_emit_connection_complete(conn, 0);
            }
            //    otherwise, just ignore as it is already in the open process
            return 0; // don't sent packet to controller
            
        }
        // create connection struct and register, state = SENT_CREATE_CONNECTION
        conn = create_connection_for_addr(addr);
        if (!conn){
            // notify client that alloc failed
            hci_emit_connection_complete(conn, BTSTACK_MEMORY_ALLOC_FAILED);
            return 0; // don't sent packet to controller
        }
        conn->state = SENT_CREATE_CONNECTION;
    }
    
    if (IS_COMMAND(packet, hci_link_key_request_reply)){
        hci_add_connection_flags_for_flipped_bd_addr(&packet[3], SENT_LINK_KEY_REPLY);
    }
    if (IS_COMMAND(packet, hci_link_key_request_negative_reply)){
        hci_add_connection_flags_for_flipped_bd_addr(&packet[3], SENT_LINK_KEY_NEGATIVE_REQUEST);
    }
    if (IS_COMMAND(packet, hci_pin_code_request_reply)){
        hci_add_connection_flags_for_flipped_bd_addr(&packet[3], SENT_PIN_CODE_REPLY);
    }
    if (IS_COMMAND(packet, hci_pin_code_request_negative_reply)){
        hci_add_connection_flags_for_flipped_bd_addr(&packet[3], SENT_PIN_CODE_NEGATIVE_REPLY);
    }
    
    if (IS_COMMAND(packet, hci_delete_stored_link_key)){
        if (hci_stack.remote_device_db){
            bt_flip_addr(addr, &packet[3]);
            hci_stack.remote_device_db->delete_link_key(&addr);
        }
    }
    
    hci_stack.num_cmd_packets--;
    return hci_stack.hci_transport->send_packet(HCI_COMMAND_DATA_PACKET, packet, size);
}

/**
 * pre: numcmds >= 0 - it's allowed to send a command to the controller
 */
int hci_send_cmd(const hci_cmd_t *cmd, ...){
    va_list argptr;
    va_start(argptr, cmd);
    uint16_t size = hci_create_cmd_internal(hci_stack.hci_packet_buffer, cmd, argptr);
    va_end(argptr);
    return hci_send_cmd_packet(hci_stack.hci_packet_buffer, size);
}

// Create various non-HCI events. 
// TODO: generalize, use table similar to hci_create_command

void hci_emit_state(){
    log_info("BTSTACK_EVENT_STATE %u", hci_stack.state);
    uint8_t event[3];
    event[0] = BTSTACK_EVENT_STATE;
    event[1] = sizeof(event) - 2;
    event[2] = hci_stack.state;
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    hci_stack.packet_handler(HCI_EVENT_PACKET, event, sizeof(event));
}

void hci_emit_connection_complete(hci_connection_t *conn, uint8_t status){
    uint8_t event[13];
    event[0] = HCI_EVENT_CONNECTION_COMPLETE;
    event[1] = sizeof(event) - 2;
    event[2] = status;
    bt_store_16(event, 3, conn->con_handle);
    bt_flip_addr(&event[5], conn->address);
    event[11] = 1; // ACL connection
    event[12] = 0; // encryption disabled
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    hci_stack.packet_handler(HCI_EVENT_PACKET, event, sizeof(event));
}

void hci_emit_disconnection_complete(uint16_t handle, uint8_t reason){
    uint8_t event[6];
    event[0] = HCI_EVENT_DISCONNECTION_COMPLETE;
    event[1] = sizeof(event) - 2;
    event[2] = 0; // status = OK
    bt_store_16(event, 3, handle);
    event[5] = reason;
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    hci_stack.packet_handler(HCI_EVENT_PACKET, event, sizeof(event));
}

void hci_emit_l2cap_check_timeout(hci_connection_t *conn){
    log_info("L2CAP_EVENT_TIMEOUT_CHECK");
    uint8_t event[4];
    event[0] = L2CAP_EVENT_TIMEOUT_CHECK;
    event[1] = sizeof(event) - 2;
    bt_store_16(event, 2, conn->con_handle);
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    hci_stack.packet_handler(HCI_EVENT_PACKET, event, sizeof(event));
}

void hci_emit_nr_connections_changed(){
    log_info("BTSTACK_EVENT_NR_CONNECTIONS_CHANGED %u", nr_hci_connections());
    uint8_t event[3];
    event[0] = BTSTACK_EVENT_NR_CONNECTIONS_CHANGED;
    event[1] = sizeof(event) - 2;
    event[2] = nr_hci_connections();
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    hci_stack.packet_handler(HCI_EVENT_PACKET, event, sizeof(event));
}

void hci_emit_hci_open_failed(){
    log_info("BTSTACK_EVENT_POWERON_FAILED");
    uint8_t event[2];
    event[0] = BTSTACK_EVENT_POWERON_FAILED;
    event[1] = sizeof(event) - 2;
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    hci_stack.packet_handler(HCI_EVENT_PACKET, event, sizeof(event));
}

#ifndef EMBEDDED
void hci_emit_btstack_version() {
    log_info("BTSTACK_EVENT_VERSION %u.%u", BTSTACK_MAJOR, BTSTACK_MINOR);
    uint8_t event[6];
    event[0] = BTSTACK_EVENT_VERSION;
    event[1] = sizeof(event) - 2;
    event[2] = BTSTACK_MAJOR;
    event[3] = BTSTACK_MINOR;
    bt_store_16(event, 4, BTSTACK_REVISION);
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    hci_stack.packet_handler(HCI_EVENT_PACKET, event, sizeof(event));
}
#endif

void hci_emit_system_bluetooth_enabled(uint8_t enabled){
    log_info("BTSTACK_EVENT_SYSTEM_BLUETOOTH_ENABLED %u", enabled);
    uint8_t event[3];
    event[0] = BTSTACK_EVENT_SYSTEM_BLUETOOTH_ENABLED;
    event[1] = sizeof(event) - 2;
    event[2] = enabled;
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    hci_stack.packet_handler(HCI_EVENT_PACKET, event, sizeof(event));
}

void hci_emit_remote_name_cached(bd_addr_t *addr, device_name_t *name){
    uint8_t event[2+1+6+248+1]; // +1 for \0 in log_info
    event[0] = BTSTACK_EVENT_REMOTE_NAME_CACHED;
    event[1] = sizeof(event) - 2 - 1;
    event[2] = 0;   // just to be compatible with HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE
    bt_flip_addr(&event[3], *addr);
    memcpy(&event[9], name, 248);
    
    event[9+248] = 0;   // assert \0 for log_info
    log_info("BTSTACK_EVENT_REMOTE_NAME_CACHED %s = '%s'", bd_addr_to_str(*addr), &event[9]);

    hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event)-1);
    hci_stack.packet_handler(HCI_EVENT_PACKET, event, sizeof(event)-1);
}

void hci_emit_discoverable_enabled(uint8_t enabled){
    log_info("BTSTACK_EVENT_DISCOVERABLE_ENABLED %u", enabled);
    uint8_t event[3];
    event[0] = BTSTACK_EVENT_DISCOVERABLE_ENABLED;
    event[1] = sizeof(event) - 2;
    event[2] = enabled;
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    hci_stack.packet_handler(HCI_EVENT_PACKET, event, sizeof(event));
}
