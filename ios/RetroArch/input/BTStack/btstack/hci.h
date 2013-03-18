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
 *  hci.h
 *
 *  Created by Matthias Ringwald on 4/29/09.
 *
 */

#pragma once

#include "config.h"

#include <btstack/hci_cmds.h>
#include <btstack/utils.h>
#include "hci_transport.h"
#include "bt_control.h"
#include "remote_device_db.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>

#if defined __cplusplus
extern "C" {
#endif
    
// packet header sizes
#define HCI_CMD_HEADER_SIZE          3
#define HCI_ACL_HEADER_SIZE   	     4
#define HCI_SCO_HEADER_SIZE  	     3
#define HCI_EVENT_HEADER_SIZE        2

// packet sizes (max payload)
#define HCI_ACL_DM1_SIZE            17
#define HCI_ACL_DH1_SIZE            27
#define HCI_ACL_2DH1_SIZE           54
#define HCI_ACL_3DH1_SIZE           83
#define HCI_ACL_DM3_SIZE           121
#define HCI_ACL_DH3_SIZE           183
#define HCI_ACL_DM5_SIZE           224
#define HCI_ACL_DH5_SIZE           339
#define HCI_ACL_2DH3_SIZE          367
#define HCI_ACL_3DH3_SIZE          552
#define HCI_ACL_2DH5_SIZE          679
#define HCI_ACL_3DH5_SIZE         1021
       
#define HCI_EVENT_PAYLOAD_SIZE     255
#define HCI_CMD_PAYLOAD_SIZE       255
    
// packet buffer sizes
// HCI_ACL_PAYLOAD_SIZE is configurable and defined in config.h
#define HCI_EVENT_BUFFER_SIZE      (HCI_EVENT_HEADER_SIZE + HCI_EVENT_PAYLOAD_SIZE)
#define HCI_CMD_BUFFER_SIZE        (HCI_CMD_HEADER_SIZE   + HCI_CMD_PAYLOAD_SIZE)
#define HCI_ACL_BUFFER_SIZE        (HCI_ACL_HEADER_SIZE   + HCI_ACL_PAYLOAD_SIZE)
    
// size of hci buffers, big enough for command, event, or acl packet without H4 packet type
// @note cmd buffer is bigger than event buffer
#if HCI_ACL_BUFFER_SIZE > HCI_CMD_BUFFER_SIZE
#define HCI_PACKET_BUFFER_SIZE HCI_ACL_BUFFER_SIZE
#else
#define HCI_PACKET_BUFFER_SIZE HCI_CMD_BUFFER_SIZE
#endif
    
// OGFs
#define OGF_LINK_CONTROL          0x01
#define OGF_LINK_POLICY           0x02
#define OGF_CONTROLLER_BASEBAND   0x03
#define OGF_INFORMATIONAL_PARAMETERS 0x04
#define OGF_LE_CONTROLLER 0x08
#define OGF_BTSTACK 0x3d
#define OGF_VENDOR  0x3f

// cmds for BTstack 
// get state: @returns HCI_STATE
#define BTSTACK_GET_STATE                                  0x01

// set power mode: @param HCI_POWER_MODE
#define BTSTACK_SET_POWER_MODE                             0x02

// set capture mode: @param on
#define BTSTACK_SET_ACL_CAPTURE_MODE                       0x03

// get BTstack version
#define BTSTACK_GET_VERSION                                0x04

// get system Bluetooth state
#define BTSTACK_GET_SYSTEM_BLUETOOTH_ENABLED               0x05

// set system Bluetooth state
#define BTSTACK_SET_SYSTEM_BLUETOOTH_ENABLED               0x06

// enable inquiry scan for this client
#define BTSTACK_SET_DISCOVERABLE                           0x07

// set global Bluetooth state
#define BTSTACK_SET_BLUETOOTH_ENABLED                      0x08

// create l2cap channel: @param bd_addr(48), psm (16)
#define L2CAP_CREATE_CHANNEL                               0x20

// disconnect l2cap disconnect, @param channel(16), reason(8)
#define L2CAP_DISCONNECT                                   0x21

// register l2cap service: @param psm(16), mtu (16)
#define L2CAP_REGISTER_SERVICE                             0x22

// unregister l2cap disconnect, @param psm(16)
#define L2CAP_UNREGISTER_SERVICE                           0x23

// accept connection @param bd_addr(48), dest cid (16)
#define L2CAP_ACCEPT_CONNECTION                            0x24

// decline l2cap disconnect,@param bd_addr(48), dest cid (16), reason(8)
#define L2CAP_DECLINE_CONNECTION                           0x25

// create l2cap channel: @param bd_addr(48), psm (16), mtu (16)
#define L2CAP_CREATE_CHANNEL_MTU                           0x26

// register SDP Service Record: service record (size)
#define SDP_REGISTER_SERVICE_RECORD                        0x30

// unregister SDP Service Record
#define SDP_UNREGISTER_SERVICE_RECORD                      0x31

// RFCOMM "HCI" Commands
#define RFCOMM_CREATE_CHANNEL       0x40
#define RFCOMM_DISCONNECT			0x41
#define RFCOMM_REGISTER_SERVICE     0x42
#define RFCOMM_UNREGISTER_SERVICE   0x43
#define RFCOMM_ACCEPT_CONNECTION    0x44
#define RFCOMM_DECLINE_CONNECTION   0x45
#define RFCOMM_PERSISTENT_CHANNEL   0x46
#define RFCOMM_CREATE_CHANNEL_WITH_CREDITS   0x47
#define RFCOMM_REGISTER_SERVICE_WITH_CREDITS 0x48
#define RFCOMM_GRANT_CREDITS                 0x49
    
// 
#define IS_COMMAND(packet, command) (READ_BT_16(packet,0) == command.opcode)

// data: event(8)
#define DAEMON_EVENT_CONNECTION_OPENED                     0x50

// data: event(8)
#define DAEMON_EVENT_CONNECTION_CLOSED                     0x51

// data: event(8), nr_connections(8)
#define DAEMON_NR_CONNECTIONS_CHANGED                      0x52

// data: event(8)
#define DAEMON_EVENT_NEW_RFCOMM_CREDITS                    0x53

// data: event()
#define DAEMON_EVENT_HCI_PACKET_SENT                       0x54
    
/**
 * Connection State 
 */
typedef enum {
    AUTH_FLAGS_NONE                = 0x00,
    RECV_LINK_KEY_REQUEST          = 0x01,
    HANDLE_LINK_KEY_REQUEST        = 0x02,
    SENT_LINK_KEY_REPLY            = 0x04,
    SENT_LINK_KEY_NEGATIVE_REQUEST = 0x08,
    RECV_LINK_KEY_NOTIFICATION     = 0x10,
    RECV_PIN_CODE_REQUEST          = 0x20,
    SENT_PIN_CODE_REPLY            = 0x40, 
    SENT_PIN_CODE_NEGATIVE_REPLY   = 0x80 
} hci_authentication_flags_t;

typedef enum {
    SENT_CREATE_CONNECTION = 1,
    RECEIVED_CONNECTION_REQUEST,
    ACCEPTED_CONNECTION_REQUEST,
    REJECTED_CONNECTION_REQUEST,
    OPEN,
    SENT_DISCONNECT
} CONNECTION_STATE;

typedef enum {
    BLUETOOTH_OFF = 1,
    BLUETOOTH_ON,
    BLUETOOTH_ACTIVE
} BLUETOOTH_STATE;

typedef struct {
    // linked list - assert: first field
    linked_item_t    item;
    
    // remote side
    bd_addr_t address;
    
    // module handle
    hci_con_handle_t con_handle;

    // state
    CONNECTION_STATE state;
    
    // errands
    hci_authentication_flags_t authentication_flags;

    timer_source_t timeout;
    
#ifdef HAVE_TIME
    // timer
    struct timeval timestamp;
#endif
#ifdef HAVE_TICK
    uint32_t timestamp; // timeout in system ticks
#endif
    
    // ACL packet recombination - ACL Header + ACL payload
    uint8_t  acl_recombination_buffer[4 + HCI_ACL_BUFFER_SIZE];
    uint16_t acl_recombination_pos;
    uint16_t acl_recombination_length;
    
    // number ACL packets sent to controller
    uint8_t num_acl_packets_sent;
    
} hci_connection_t;

/**
 * main data structure
 */
typedef struct {
    // transport component with configuration
    hci_transport_t  * hci_transport;
    void             * config;
    
    // hardware power controller
    bt_control_t     * control;
    
    // list of existing baseband connections
    linked_list_t     connections;

    // single buffer for HCI Command assembly
    uint8_t          hci_packet_buffer[HCI_PACKET_BUFFER_SIZE]; // opcode (16), len(8)
    
    /* host to controller flow control */
    uint8_t  num_cmd_packets;
    // uint8_t  total_num_cmd_packets;
    uint8_t  total_num_acl_packets;
    uint16_t acl_data_packet_length;

    // usable packet types given acl_data_packet_length and HCI_ACL_BUFFER_SIZE
    uint16_t packet_types;
    
    /* callback to L2CAP layer */
    void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);

    /* remote device db */
    remote_device_db_t const*remote_device_db;
    
    /* hci state machine */
    HCI_STATE state;
    uint8_t   substate;
    uint8_t   cmds_ready;
    
    uint8_t   discoverable;
    uint8_t   connectable;
    
    /* buffer for scan enable cmd - 0xff no change */
    uint8_t   new_scan_enable_value;
    
    // buffer for single connection decline
    uint8_t   decline_reason;
    bd_addr_t decline_addr;
    
} hci_stack_t;

// create and send hci command packets based on a template and a list of parameters
uint16_t hci_create_cmd(uint8_t *hci_cmd_buffer, hci_cmd_t *cmd, ...);
uint16_t hci_create_cmd_internal(uint8_t *hci_cmd_buffer, const hci_cmd_t *cmd, va_list argptr);

// set up HCI
void hci_init(hci_transport_t *transport, void *config, bt_control_t *control, remote_device_db_t const* remote_device_db);
void hci_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size));
void hci_close(void);

// power and inquriy scan control
int  hci_power_control(HCI_POWER_MODE mode);
void hci_discoverable_control(uint8_t enable);
void hci_connectable_control(uint8_t enable);

/**
 * run the hci control loop once
 */
void hci_run(void);

// create and send hci command packets based on a template and a list of parameters
int hci_send_cmd(const hci_cmd_t *cmd, ...);

// send complete CMD packet
int hci_send_cmd_packet(uint8_t *packet, int size);

// send ACL packet
int hci_send_acl_packet(uint8_t *packet, int size);

// non-blocking UART driver needs
int hci_can_send_packet_now(uint8_t packet_type);
    
hci_connection_t * connection_for_handle(hci_con_handle_t con_handle);
uint8_t  hci_number_outgoing_packets(hci_con_handle_t handle);
uint8_t  hci_number_free_acl_slots(void);
int      hci_authentication_active_for_handle(hci_con_handle_t handle);
void     hci_drop_link_key_for_bd_addr(bd_addr_t *addr);
uint16_t hci_max_acl_data_packet_length(void);
uint16_t hci_usable_acl_packet_types(void);
uint8_t* hci_get_outgoing_acl_packet_buffer(void);

// 
void hci_emit_state(void);
void hci_emit_connection_complete(hci_connection_t *conn, uint8_t status);
void hci_emit_l2cap_check_timeout(hci_connection_t *conn);
void hci_emit_disconnection_complete(uint16_t handle, uint8_t reason);
void hci_emit_nr_connections_changed(void);
void hci_emit_hci_open_failed(void);
void hci_emit_btstack_version(void);
void hci_emit_system_bluetooth_enabled(uint8_t enabled);
void hci_emit_remote_name_cached(bd_addr_t *addr, device_name_t *name);
void hci_emit_discoverable_enabled(uint8_t enabled);

#if defined __cplusplus
}
#endif
