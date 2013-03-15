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
 *  hci_cmds.h
 *
 *  Created by Matthias Ringwald on 7/23/09.
 */

#pragma once

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif
	
/**
 * packet types - used in BTstack and over the H4 UART interface
 */
#define HCI_COMMAND_DATA_PACKET	0x01
#define HCI_ACL_DATA_PACKET	    0x02
#define HCI_SCO_DATA_PACKET	    0x03
#define HCI_EVENT_PACKET	    0x04

// extension for client/server communication
#define DAEMON_EVENT_PACKET     0x05
    
// L2CAP data
#define L2CAP_DATA_PACKET       0x06

// RFCOMM data
#define RFCOMM_DATA_PACKET      0x07

// Attribute protocol data
#define ATT_DATA_PACKET         0x08

// Security Manager protocol data
#define SM_DATA_PACKET          0x09
    
// debug log messages
#define LOG_MESSAGE_PACKET      0xfc

    
// Fixed PSM numbers
#define PSM_SDP    0x01
#define PSM_RFCOMM 0x03
#define PSM_HID_CONTROL 0x11
#define PSM_HID_INTERRUPT 0x13

// Events from host controller to host
#define HCI_EVENT_INQUIRY_COMPLETE				           0x01
#define HCI_EVENT_INQUIRY_RESULT				           0x02
#define HCI_EVENT_CONNECTION_COMPLETE			           0x03
#define HCI_EVENT_CONNECTION_REQUEST			           0x04
#define HCI_EVENT_DISCONNECTION_COMPLETE		      	   0x05
#define HCI_EVENT_AUTHENTICATION_COMPLETE_EVENT            0x06
#define HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE	           0x07
#define HCI_EVENT_ENCRYPTION_CHANGE                        0x08
#define HCI_EVENT_CHANGE_CONNECTION_LINK_KEY_COMPLETE      0x09
#define HCI_EVENT_MASTER_LINK_KEY_COMPLETE                 0x0A
#define HCI_EVENT_READ_REMOTE_SUPPORTED_FEATURES_COMPLETE  0x0B
#define HCI_EVENT_READ_REMOTE_VERSION_INFORMATION_COMPLETE 0x0C
#define HCI_EVENT_QOS_SETUP_COMPLETE			           0x0D
#define HCI_EVENT_COMMAND_COMPLETE				           0x0E
#define HCI_EVENT_COMMAND_STATUS				           0x0F
#define HCI_EVENT_HARDWARE_ERROR                           0x10
#define HCI_EVENT_FLUSH_OCCURED                            0x11
#define HCI_EVENT_ROLE_CHANGE				               0x12
#define HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS	      	   0x13
#define HCI_EVENT_MODE_CHANGE_EVENT                        0x14
#define HCI_EVENT_RETURN_LINK_KEYS                         0x15
#define HCI_EVENT_PIN_CODE_REQUEST                         0x16
#define HCI_EVENT_LINK_KEY_REQUEST                         0x17
#define HCI_EVENT_LINK_KEY_NOTIFICATION                    0x18
#define HCI_EVENT_DATA_BUFFER_OVERFLOW                     0x1A
#define HCI_EVENT_MAX_SLOTS_CHANGED			               0x1B
#define HCI_EVENT_READ_CLOCK_OFFSET_COMPLETE               0x1C
#define HCI_EVENT_PACKET_TYPE_CHANGED                      0x1D
#define HCI_EVENT_INQUIRY_RESULT_WITH_RSSI		      	   0x22
#define HCI_EVENT_EXTENDED_INQUIRY_RESPONSE                0x2F
#define HCI_EVENT_LE_META                                  0x3E
#define HCI_EVENT_VENDOR_SPECIFIC				           0xFF

#define HCI_SUBEVENT_LE_CONNECTION_COMPLETE                0x01
#define HCI_SUBEVENT_LE_ADVERTISING_REPORT                 0x02
#define HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE         0x03
#define HCI_SUBEVENT_LE_READ_REMOTE_USED_FEATURES_COMPLETE 0x04
#define HCI_SUBEVENT_LE_LONG_TERM_KEY_REQUEST              0x05
    
// last used HCI_EVENT in 2.1 is 0x3d

// events 0x50-0x5f are used internally

// BTSTACK DAEMON EVENTS

// events from BTstack for application/client lib
#define BTSTACK_EVENT_STATE                                0x60

// data: event(8), len(8), nr hci connections
#define BTSTACK_EVENT_NR_CONNECTIONS_CHANGED               0x61

// data: none
#define BTSTACK_EVENT_POWERON_FAILED                       0x62

// data: majot (8), minor (8), revision(16)
#define BTSTACK_EVENT_VERSION	        				   0x63

// data: system bluetooth on/off (bool)
#define BTSTACK_EVENT_SYSTEM_BLUETOOTH_ENABLED			   0x64

// data: event (8), len(8), status (8) == 0, address (48), name (1984 bits = 248 bytes)
#define BTSTACK_EVENT_REMOTE_NAME_CACHED	     		   0x65

// data: discoverable enabled (bool)
#define BTSTACK_EVENT_DISCOVERABLE_ENABLED			       0x66

// L2CAP EVENTS
	
// data: event (8), len(8), status (8), address(48), handle (16), psm (16), local_cid(16), remote_cid (16), local_mtu(16), remote_mtu(16) 
#define L2CAP_EVENT_CHANNEL_OPENED                         0x70

// data: event (8), len(8), channel (16)
#define L2CAP_EVENT_CHANNEL_CLOSED                         0x71

// data: event (8), len(8), address(48), handle (16), psm (16), local_cid(16), remote_cid (16) 
#define L2CAP_EVENT_INCOMING_CONNECTION					   0x72

// data: event(8), len(8), handle(16)
#define L2CAP_EVENT_TIMEOUT_CHECK                          0x73

// data: event(8), len(8), local_cid(16), credits(8)
#define L2CAP_EVENT_CREDITS								   0x74

// data: event(8), len(8), status (8), psm (16)
#define L2CAP_EVENT_SERVICE_REGISTERED                     0x75


// RFCOMM EVENTS
	
// data: event(8), len(8), status (8), address (48), handle (16), server channel(8), rfcomm_cid(16), max frame size(16)
#define RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE                 0x80
	
// data: event(8), len(8), rfcomm_cid(16)
#define RFCOMM_EVENT_CHANNEL_CLOSED                        0x81
	
// data: event (8), len(8), address(48), channel (8), rfcomm_cid (16)
#define RFCOMM_EVENT_INCOMING_CONNECTION                   0x82
	
// data: event (8), len(8), rfcommid (16), ...
#define RFCOMM_EVENT_REMOTE_LINE_STATUS                    0x83
	
// data: event(8), len(8), rfcomm_cid(16), credits(8)
#define RFCOMM_EVENT_CREDITS			                   0x84
	
// data: event(8), len(8), status (8), rfcomm server channel id (8) 
#define RFCOMM_EVENT_SERVICE_REGISTERED                    0x85
    
// data: event(8), len(8), status (8), rfcomm server channel id (8) 
#define RFCOMM_EVENT_PERSISTENT_CHANNEL                    0x86
    
    
// data: event(8), len(8), status(8), service_record_handle(32)
#define SDP_SERVICE_REGISTERED                             0x90

	
// last error code in 2.1 is 0x38 - we start with 0x50 for BTstack errors

#define BTSTACK_CONNECTION_TO_BTDAEMON_FAILED              0x50
#define BTSTACK_ACTIVATION_FAILED_SYSTEM_BLUETOOTH		   0x51
#define BTSTACK_ACTIVATION_POWERON_FAILED       		   0x52
#define BTSTACK_ACTIVATION_FAILED_UNKNOWN       		   0x53
#define BTSTACK_NOT_ACTIVATED							   0x54
#define BTSTACK_BUSY									   0x55
#define BTSTACK_MEMORY_ALLOC_FAILED                        0x56
#define BTSTACK_ACL_BUFFERS_FULL                           0x57

// l2cap errors - enumeration by the command that created them
#define L2CAP_COMMAND_REJECT_REASON_COMMAND_NOT_UNDERSTOOD 0x60
#define L2CAP_COMMAND_REJECT_REASON_SIGNALING_MTU_EXCEEDED 0x61
#define L2CAP_COMMAND_REJECT_REASON_INVALID_CID_IN_REQUEST 0x62

#define L2CAP_CONNECTION_RESPONSE_RESULT_SUCCESSFUL        0x63
#define L2CAP_CONNECTION_RESPONSE_RESULT_PENDING           0x64
#define L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_PSM       0x65
#define L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_SECURITY  0x66
#define L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_RESOURCES 0x65

#define L2CAP_CONFIG_RESPONSE_RESULT_SUCCESSFUL            0x66
#define L2CAP_CONFIG_RESPONSE_RESULT_UNACCEPTABLE_PARAMS   0x67
#define L2CAP_CONFIG_RESPONSE_RESULT_REJECTED              0x68
#define L2CAP_CONFIG_RESPONSE_RESULT_UNKNOWN_OPTIONS       0x69
#define L2CAP_SERVICE_ALREADY_REGISTERED                   0x6a
    
#define RFCOMM_MULTIPLEXER_STOPPED                         0x70
#define RFCOMM_CHANNEL_ALREADY_REGISTERED                  0x71
#define RFCOMM_NO_OUTGOING_CREDITS                         0x72

#define SDP_HANDLE_ALREADY_REGISTERED                      0x80
 
/**
 * Default INQ Mode
 */
#define HCI_INQUIRY_LAP 0x9E8B33L  // 0x9E8B33: General/Unlimited Inquiry Access Code (GIAC)
/**
 *  Hardware state of Bluetooth controller 
 */
typedef enum {
    HCI_POWER_OFF = 0,
    HCI_POWER_ON,
	HCI_POWER_SLEEP
} HCI_POWER_MODE;

/**
 * State of BTstack 
 */
typedef enum {
    HCI_STATE_OFF = 0,
    HCI_STATE_INITIALIZING,
    HCI_STATE_WORKING,
    HCI_STATE_HALTING,
	HCI_STATE_SLEEPING,
	HCI_STATE_FALLING_ASLEEP
} HCI_STATE;

/** 
 * compact HCI Command packet description
 */
 typedef struct {
    uint16_t    opcode;
    const char *format;
} hci_cmd_t;


// HCI Commands - see hci_cmds.c for info on parameters
extern const hci_cmd_t btstack_get_state;
extern const hci_cmd_t btstack_set_power_mode;
extern const hci_cmd_t btstack_set_acl_capture_mode;
extern const hci_cmd_t btstack_get_version;
extern const hci_cmd_t btstack_get_system_bluetooth_enabled;
extern const hci_cmd_t btstack_set_system_bluetooth_enabled;
extern const hci_cmd_t btstack_set_discoverable;
extern const hci_cmd_t btstack_set_bluetooth_enabled;    // only used by btstack config
	
extern const hci_cmd_t hci_accept_connection_request;
extern const hci_cmd_t hci_authentication_requested;
extern const hci_cmd_t hci_change_connection_link_key;
extern const hci_cmd_t hci_create_connection;
extern const hci_cmd_t hci_create_connection_cancel;
extern const hci_cmd_t hci_delete_stored_link_key;
extern const hci_cmd_t hci_disconnect;
extern const hci_cmd_t hci_host_buffer_size;
extern const hci_cmd_t hci_inquiry;
extern const hci_cmd_t hci_inquiry_cancel;
extern const hci_cmd_t hci_link_key_request_negative_reply;
extern const hci_cmd_t hci_link_key_request_reply;
extern const hci_cmd_t hci_pin_code_request_reply;
extern const hci_cmd_t hci_pin_code_request_negative_reply;
extern const hci_cmd_t hci_qos_setup;
extern const hci_cmd_t hci_read_bd_addr;
extern const hci_cmd_t hci_read_buffer_size;
extern const hci_cmd_t hci_read_le_host_supported;
extern const hci_cmd_t hci_read_link_policy_settings;
extern const hci_cmd_t hci_read_link_supervision_timeout;
extern const hci_cmd_t hci_read_local_supported_features;
extern const hci_cmd_t hci_read_num_broadcast_retransmissions;
extern const hci_cmd_t hci_reject_connection_request;
extern const hci_cmd_t hci_remote_name_request;
extern const hci_cmd_t hci_remote_name_request_cancel;
extern const hci_cmd_t hci_reset;
extern const hci_cmd_t hci_role_discovery;
extern const hci_cmd_t hci_set_event_mask;
extern const hci_cmd_t hci_set_connection_encryption;
extern const hci_cmd_t hci_sniff_mode;
extern const hci_cmd_t hci_switch_role_command;
extern const hci_cmd_t hci_write_authentication_enable;
extern const hci_cmd_t hci_write_class_of_device;
extern const hci_cmd_t hci_write_extended_inquiry_response;
extern const hci_cmd_t hci_write_inquiry_mode;
extern const hci_cmd_t hci_write_le_host_supported;
extern const hci_cmd_t hci_write_link_policy_settings;
extern const hci_cmd_t hci_write_link_supervision_timeout;
extern const hci_cmd_t hci_write_local_name;
extern const hci_cmd_t hci_write_num_broadcast_retransmissions;
extern const hci_cmd_t hci_write_page_timeout;
extern const hci_cmd_t hci_write_scan_enable;
extern const hci_cmd_t hci_write_simple_pairing_mode;

extern const hci_cmd_t hci_le_add_device_to_whitelist;
extern const hci_cmd_t hci_le_clear_white_list;
extern const hci_cmd_t hci_le_connection_update;
extern const hci_cmd_t hci_le_create_connection;
extern const hci_cmd_t hci_le_create_connection_cancel;
extern const hci_cmd_t hci_le_encrypt;
extern const hci_cmd_t hci_le_long_term_key_negative_reply;
extern const hci_cmd_t hci_le_long_term_key_request_reply;
extern const hci_cmd_t hci_le_rand;
extern const hci_cmd_t hci_le_read_advertising_channel_tx_power;
extern const hci_cmd_t hci_le_read_buffer_size ;
extern const hci_cmd_t hci_le_read_channel_map;
extern const hci_cmd_t hci_le_read_remote_used_features;
extern const hci_cmd_t hci_le_read_supported_features;
extern const hci_cmd_t hci_le_read_supported_states;
extern const hci_cmd_t hci_le_read_white_list_size;
extern const hci_cmd_t hci_le_receiver_test;
extern const hci_cmd_t hci_le_remove_device_from_whitelist;
extern const hci_cmd_t hci_le_set_advertise_enable;
extern const hci_cmd_t hci_le_set_advertising_data;
extern const hci_cmd_t hci_le_set_advertising_parameters;
extern const hci_cmd_t hci_le_set_event_mask;
extern const hci_cmd_t hci_le_set_host_channel_classification;
extern const hci_cmd_t hci_le_set_random_address;
extern const hci_cmd_t hci_le_set_scan_enable;
extern const hci_cmd_t hci_le_set_scan_parameters;
extern const hci_cmd_t hci_le_set_scan_response_data;
extern const hci_cmd_t hci_le_start_encryption;
extern const hci_cmd_t hci_le_test_end;
extern const hci_cmd_t hci_le_transmitter_test;
    
extern const hci_cmd_t l2cap_accept_connection;
extern const hci_cmd_t l2cap_create_channel;
extern const hci_cmd_t l2cap_create_channel_mtu;
extern const hci_cmd_t l2cap_decline_connection;
extern const hci_cmd_t l2cap_disconnect;
extern const hci_cmd_t l2cap_register_service;
extern const hci_cmd_t l2cap_unregister_service;

extern const hci_cmd_t sdp_register_service_record;
extern const hci_cmd_t sdp_unregister_service_record;

// accept connection @param bd_addr(48), rfcomm_cid (16)
extern const hci_cmd_t rfcomm_accept_connection;
// create rfcomm channel: @param bd_addr(48), channel (8)
extern const hci_cmd_t rfcomm_create_channel;
// create rfcomm channel: @param bd_addr(48), channel (8), mtu (16), credits (8)
extern const hci_cmd_t rfcomm_create_channel_with_initial_credits;
// decline rfcomm disconnect,@param bd_addr(48), rfcomm cid (16), reason(8)
extern const hci_cmd_t rfcomm_decline_connection;
// disconnect rfcomm disconnect, @param rfcomm_cid(8), reason(8)
extern const hci_cmd_t rfcomm_disconnect;
// register rfcomm service: @param channel(8), mtu (16)
extern const hci_cmd_t rfcomm_register_service;
// register rfcomm service: @param channel(8), mtu (16), initial credits (8)
extern const hci_cmd_t rfcomm_register_service_with_initial_credits;
// unregister rfcomm service, @param service_channel(16)
extern const hci_cmd_t rfcomm_unregister_service;
// request persisten rfcomm channel for service name: serive name (char*) 
extern const hci_cmd_t rfcomm_persistent_channel_for_service;
    
#if defined __cplusplus
}
#endif
