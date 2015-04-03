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

#ifndef _BTSTACK_HEADER_H
#define _BTSTACK_HEADER_H

#include <stdint.h>
#include <boolean.h>
#include <sys/time.h>

#if defined __cplusplus
extern "C" {
#endif
    
/* Length of a Bluetooth device address. */
#define BD_ADDR_LEN        6
/* The link key type. */
#define LINK_KEY_LEN       16
/* The device name type. */
#define DEVICE_NAME_LEN    248
    
/* Type definitions. */
typedef uint16_t hci_con_handle_t;
typedef uint8_t bd_addr_t[BD_ADDR_LEN];
typedef uint8_t link_key_t[LINK_KEY_LEN];
typedef uint8_t device_name_t[DEVICE_NAME_LEN+1];
    
/* Packet handler. */
typedef void (*btstack_packet_handler_t) (uint8_t packet_type,
    uint16_t channel, uint8_t *packet, uint16_t size);

/* Hardware state of Bluetooth controller  */
typedef enum
{
   HCI_POWER_OFF = 0,
   HCI_POWER_ON,
   HCI_POWER_SLEEP
} HCI_POWER_MODE;
    
/* State of BTstack */
typedef enum
{
   HCI_STATE_OFF = 0,
   HCI_STATE_INITIALIZING,
   HCI_STATE_WORKING,
   HCI_STATE_HALTING,
   HCI_STATE_SLEEPING,
   HCI_STATE_FALLING_ASLEEP
} HCI_STATE;
    
typedef enum
{
   RUN_LOOP_POSIX = 1,
   RUN_LOOP_COCOA,
   RUN_LOOP_EMBEDDED
} RUN_LOOP_TYPE;
    
/* compact HCI Command packet description */
typedef struct
{
   uint16_t    opcode;
   const char *format;
} hci_cmd_t;

typedef struct linked_item
{
   struct linked_item *next; /* <-- next element in list, or NULL */
   void *user_data;          /* <-- pointer to struct base */
} linked_item_t;
    
typedef linked_item_t *linked_list_t;

typedef struct data_source
{
   linked_item_t item;

   /* File descriptor to watch or 0. */
   int  fd;

   int  (*process)(struct data_source *ds);
} data_source_t;

typedef struct timer
{
   linked_item_t item; 
   /* Next timeout. */
   struct timeval timeout;
#ifdef HAVE_TICK
   /* Timeout in system ticks. */
   uint32_t timeout;
#endif
   void  (*process)(struct timer *ts);
} timer_source_t;

/* btdynamic.h */

bool btstack_try_load(void);

void btstack_set_poweron(bool on);

#ifndef BUILDING_BTDYNAMIC
#define BTDIMPORT extern
#else
#define BTDIMPORT
#endif

BTDIMPORT int (*bt_open_ptr)(void);
BTDIMPORT void (*bt_close_ptr)(void);
BTDIMPORT void (*bt_flip_addr_ptr)(bd_addr_t dest, bd_addr_t src);
BTDIMPORT char* (*bd_addr_to_str_ptr)(bd_addr_t addr);
BTDIMPORT btstack_packet_handler_t (*bt_register_packet_handler_ptr)
   (btstack_packet_handler_t handler);
BTDIMPORT int (*bt_send_cmd_ptr)(const hci_cmd_t *cmd, ...);
BTDIMPORT void (*bt_send_l2cap_ptr)(uint16_t local_cid, uint8_t *data, uint16_t len);
BTDIMPORT void (*run_loop_init_ptr)(RUN_LOOP_TYPE type);
BTDIMPORT void (*run_loop_execute_ptr)(void);

BTDIMPORT const hci_cmd_t* btstack_set_power_mode_ptr;
BTDIMPORT const hci_cmd_t* hci_delete_stored_link_key_ptr;
BTDIMPORT const hci_cmd_t* hci_disconnect_ptr;
BTDIMPORT const hci_cmd_t* hci_read_bd_addr_ptr;
BTDIMPORT const hci_cmd_t* hci_inquiry_ptr;
BTDIMPORT const hci_cmd_t* hci_inquiry_cancel_ptr;
BTDIMPORT const hci_cmd_t* hci_pin_code_request_reply_ptr;
BTDIMPORT const hci_cmd_t* hci_pin_code_request_negative_reply_ptr;
BTDIMPORT const hci_cmd_t* hci_remote_name_request_ptr;
BTDIMPORT const hci_cmd_t* hci_remote_name_request_cancel_ptr;
BTDIMPORT const hci_cmd_t* hci_write_authentication_enable_ptr;
BTDIMPORT const hci_cmd_t* hci_write_inquiry_mode_ptr;
BTDIMPORT const hci_cmd_t* l2cap_create_channel_ptr;
BTDIMPORT const hci_cmd_t* l2cap_register_service_ptr;
BTDIMPORT const hci_cmd_t* l2cap_accept_connection_ptr;
BTDIMPORT const hci_cmd_t* l2cap_decline_connection_ptr;

/* hci_cmds.h */

/**
 * packet types - used in BTstack and over the H4 UART interface
 */
#define HCI_COMMAND_DATA_PACKET                             0x01
#define HCI_ACL_DATA_PACKET                                 0x02
#define HCI_SCO_DATA_PACKET                                 0x03
#define HCI_EVENT_PACKET                                    0x04

/* extension for client/server communication */
#define DAEMON_EVENT_PACKET                                 0x05
    
/* L2CAP data */
#define L2CAP_DATA_PACKET                                   0x06

/* RFCOMM data */
#define RFCOMM_DATA_PACKET                                  0x07

/* Attribute protocol data */
#define ATT_DATA_PACKET                                     0x08

/* Security Manager protocol data */
#define SM_DATA_PACKET                                      0x09
    
/* debug log messages */
#define LOG_MESSAGE_PACKET                                  0xFC

/* Fixed PSM numbers */
#define PSM_SDP                                             0x01
#define PSM_RFCOMM                                          0x03
#define PSM_HID_CONTROL                                     0x11
#define PSM_HID_INTERRUPT                                   0x13

/* Events from host controller to host */
#define HCI_EVENT_INQUIRY_COMPLETE                          0x01
#define HCI_EVENT_INQUIRY_RESULT                            0x02
#define HCI_EVENT_CONNECTION_COMPLETE                       0x03
#define HCI_EVENT_CONNECTION_REQUEST                        0x04
#define HCI_EVENT_DISCONNECTION_COMPLETE                    0x05
#define HCI_EVENT_AUTHENTICATION_COMPLETE_EVENT             0x06
#define HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE              0x07
#define HCI_EVENT_ENCRYPTION_CHANGE                         0x08
#define HCI_EVENT_CHANGE_CONNECTION_LINK_KEY_COMPLETE       0x09
#define HCI_EVENT_MASTER_LINK_KEY_COMPLETE                  0x0A
#define HCI_EVENT_READ_REMOTE_SUPPORTED_FEATURES_COMPLETE   0x0B
#define HCI_EVENT_READ_REMOTE_VERSION_INFORMATION_COMPLETE  0x0C
#define HCI_EVENT_QOS_SETUP_COMPLETE                        0x0D
#define HCI_EVENT_COMMAND_COMPLETE                          0x0E
#define HCI_EVENT_COMMAND_STATUS                            0x0F
#define HCI_EVENT_HARDWARE_ERROR                            0x10
#define HCI_EVENT_FLUSH_OCCURED                             0x11
#define HCI_EVENT_ROLE_CHANGE                               0x12
#define HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS               0x13
#define HCI_EVENT_MODE_CHANGE_EVENT                         0x14
#define HCI_EVENT_RETURN_LINK_KEYS                          0x15
#define HCI_EVENT_PIN_CODE_REQUEST                          0x16
#define HCI_EVENT_LINK_KEY_REQUEST                          0x17
#define HCI_EVENT_LINK_KEY_NOTIFICATION                     0x18
#define HCI_EVENT_DATA_BUFFER_OVERFLOW                      0x1A
#define HCI_EVENT_MAX_SLOTS_CHANGED                         0x1B
#define HCI_EVENT_READ_CLOCK_OFFSET_COMPLETE                0x1C
#define HCI_EVENT_PACKET_TYPE_CHANGED                       0x1D
#define HCI_EVENT_INQUIRY_RESULT_WITH_RSSI                  0x22
#define HCI_EVENT_EXTENDED_INQUIRY_RESPONSE                 0x2F
#define HCI_EVENT_LE_META                                   0x3E
#define HCI_EVENT_VENDOR_SPECIFIC                           0xFF

#define HCI_SUBEVENT_LE_CONNECTION_COMPLETE                 0x01
#define HCI_SUBEVENT_LE_ADVERTISING_REPORT                  0x02
#define HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE          0x03
#define HCI_SUBEVENT_LE_READ_REMOTE_USED_FEATURES_COMPLETE  0x04
#define HCI_SUBEVENT_LE_LONG_TERM_KEY_REQUEST               0x05
    
/* last used HCI_EVENT in 2.1 is 0x3d */

/* events 0x50-0x5f are used internally */

/* BTSTACK DAEMON EVENTS */

/* events from BTstack for application/client lib */
#define BTSTACK_EVENT_STATE                                 0x60

/* data: event(8), len(8), nr hci connections */
#define BTSTACK_EVENT_NR_CONNECTIONS_CHANGED                0x61

/* data: none */
#define BTSTACK_EVENT_POWERON_FAILED                        0x62

/* data: major (8), minor (8), revision(16) */
#define BTSTACK_EVENT_VERSION                               0x63

/* data: system bluetooth on/off (bool) */
#define BTSTACK_EVENT_SYSTEM_BLUETOOTH_ENABLED              0x64

/* data: event (8), len(8), status (8) == 0, address (48), name (1984 bits = 248 bytes) */
#define BTSTACK_EVENT_REMOTE_NAME_CACHED                    0x65

/* data: discoverable enabled (bool) */
#define BTSTACK_EVENT_DISCOVERABLE_ENABLED                  0x66

/* L2CAP EVENTS */
	
/* data: event (8), len(8), status (8), address(48), handle (16), psm (16), local_cid(16), remote_cid (16), local_mtu(16), remote_mtu(16)  */
#define L2CAP_EVENT_CHANNEL_OPENED                          0x70

/* data: event (8), len(8), channel (16) */
#define L2CAP_EVENT_CHANNEL_CLOSED                          0x71

/* data: event (8), len(8), address(48), handle (16), psm (16), local_cid(16), remote_cid (16)  */
#define L2CAP_EVENT_INCOMING_CONNECTION                     0x72

/* data: event(8), len(8), handle(16) */
#define L2CAP_EVENT_TIMEOUT_CHECK                           0x73

/* data: event(8), len(8), local_cid(16), credits(8) */
#define L2CAP_EVENT_CREDITS                                 0x74

/* data: event(8), len(8), status (8), psm (16) */
#define L2CAP_EVENT_SERVICE_REGISTERED                      0x75

/* RFCOMM EVENTS */
	
// data: event(8), len(8), status (8), address (48), handle (16), server channel(8), rfcomm_cid(16), max frame size(16)
#define RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE                  0x80
	
// data: event(8), len(8), rfcomm_cid(16)
#define RFCOMM_EVENT_CHANNEL_CLOSED                         0x81
	
// data: event (8), len(8), address(48), channel (8), rfcomm_cid (16)
#define RFCOMM_EVENT_INCOMING_CONNECTION                    0x82
	
// data: event (8), len(8), rfcommid (16), ...
#define RFCOMM_EVENT_REMOTE_LINE_STATUS                     0x83
	
/* data: event(8), len(8), rfcomm_cid(16), credits(8) */
#define RFCOMM_EVENT_CREDITS                                0x84
	
/* data: event(8), len(8), status (8), rfcomm server channel id (8)  */
#define RFCOMM_EVENT_SERVICE_REGISTERED                     0x85
    
/* data: event(8), len(8), status (8), rfcomm server channel id (8)  */
#define RFCOMM_EVENT_PERSISTENT_CHANNEL                     0x86
    
/* data: event(8), len(8), status(8), service_record_handle(32) */
#define SDP_SERVICE_REGISTERED                              0x90
	
/* last error code in 2.1 is 0x38 - we start with 0x50 for BTstack errors */

#define BTSTACK_CONNECTION_TO_BTDAEMON_FAILED               0x50
#define BTSTACK_ACTIVATION_FAILED_SYSTEM_BLUETOOTH          0x51
#define BTSTACK_ACTIVATION_POWERON_FAILED                   0x52
#define BTSTACK_ACTIVATION_FAILED_UNKNOWN                   0x53
#define BTSTACK_NOT_ACTIVATED                               0x54
#define BTSTACK_BUSY                                        0x55
#define BTSTACK_MEMORY_ALLOC_FAILED                         0x56
#define BTSTACK_ACL_BUFFERS_FULL                            0x57

/* L2CAP errors - enumeration by the command that created them */
#define L2CAP_COMMAND_REJECT_REASON_COMMAND_NOT_UNDERSTOOD  0x60
#define L2CAP_COMMAND_REJECT_REASON_SIGNALING_MTU_EXCEEDED  0x61
#define L2CAP_COMMAND_REJECT_REASON_INVALID_CID_IN_REQUEST  0x62

#define L2CAP_CONNECTION_RESPONSE_RESULT_SUCCESSFUL         0x63
#define L2CAP_CONNECTION_RESPONSE_RESULT_PENDING            0x64
#define L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_PSM        0x65
#define L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_SECURITY   0x66
#define L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_RESOURCES  0x65

#define L2CAP_CONFIG_RESPONSE_RESULT_SUCCESSFUL             0x66
#define L2CAP_CONFIG_RESPONSE_RESULT_UNACCEPTABLE_PARAMS    0x67
#define L2CAP_CONFIG_RESPONSE_RESULT_REJECTED               0x68
#define L2CAP_CONFIG_RESPONSE_RESULT_UNKNOWN_OPTIONS        0x69
#define L2CAP_SERVICE_ALREADY_REGISTERED                    0x6a
    
#define RFCOMM_MULTIPLEXER_STOPPED                          0x70
#define RFCOMM_CHANNEL_ALREADY_REGISTERED                   0x71
#define RFCOMM_NO_OUTGOING_CREDITS                          0x72

#define SDP_HANDLE_ALREADY_REGISTERED                       0x80
 
/* Default INQ Mode 
 * 0x9E8B33: General/Unlimited Inquiry Access Code (GIAC)
 **/
#define HCI_INQUIRY_LAP                                     0x9E8B33L

/* HCI Commands - see hci_cmds.c for info on parameters */
extern const hci_cmd_t btstack_get_state;
extern const hci_cmd_t btstack_set_power_mode;
extern const hci_cmd_t btstack_set_acl_capture_mode;
extern const hci_cmd_t btstack_get_version;
extern const hci_cmd_t btstack_get_system_bluetooth_enabled;
extern const hci_cmd_t btstack_set_system_bluetooth_enabled;
extern const hci_cmd_t btstack_set_discoverable;
extern const hci_cmd_t btstack_set_bluetooth_enabled;    /* only used by btstack config */
	
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

/* accept connection @param bd_addr(48), rfcomm_cid (16) */
extern const hci_cmd_t rfcomm_accept_connection;
/* create rfcomm channel: @param bd_addr(48), channel (8) */
extern const hci_cmd_t rfcomm_create_channel;
/* create rfcomm channel: @param bd_addr(48), channel (8), mtu (16), credits (8) */
extern const hci_cmd_t rfcomm_create_channel_with_initial_credits;
/* decline rfcomm disconnect,@param bd_addr(48), rfcomm cid (16), reason(8) */
extern const hci_cmd_t rfcomm_decline_connection;
/* disconnect rfcomm disconnect, @param rfcomm_cid(8), reason(8) */
extern const hci_cmd_t rfcomm_disconnect;
/* register rfcomm service: @param channel(8), mtu (16) */
extern const hci_cmd_t rfcomm_register_service;
/* register rfcomm service: @param channel(8), mtu (16), initial credits (8) */
extern const hci_cmd_t rfcomm_register_service_with_initial_credits;
/* unregister rfcomm service, @param service_channel(16) */
extern const hci_cmd_t rfcomm_unregister_service;
/* request persisten rfcomm channel for service name: serive name (char*)  */
extern const hci_cmd_t rfcomm_persistent_channel_for_service;

/* linked_list.h */


void linked_item_set_user(linked_item_t *item, void *user_data);

void * linked_item_get_user(linked_item_t *item);

int  linked_list_empty(linked_list_t * list);

void linked_list_add(linked_list_t * list, linked_item_t *item);

void linked_list_add_tail(linked_list_t * list, linked_item_t *item);

int  linked_list_remove(linked_list_t * list, linked_item_t *item);

linked_item_t * linked_list_get_last_item(linked_list_t * list);

void test_linked_list(void);

/* run_loop.h */



/* Set timer based on current time in milliseconds. */
void run_loop_set_timer(timer_source_t *a, uint32_t timeout_in_ms);

/* Set callback that will be executed when timer expires. */
void run_loop_set_timer_handler(timer_source_t *ts,
      void (*process)(timer_source_t *_ts));

/* Add timer source. */
void run_loop_add_timer(timer_source_t *timer); 

/* Remove timer source. */
int  run_loop_remove_timer(timer_source_t *timer);

/* Init must be called before any other run_loop call. 
 * Use RUN_LOOP_EMBEDDED for embedded devices.
 */
void run_loop_init(RUN_LOOP_TYPE type);

/* Set data source callback. */
void run_loop_set_data_source_handler(data_source_t *ds,
      int (*process)(data_source_t *_ds));

/* Add data source. */
void run_loop_add_data_source(data_source_t *dataSource);

/* Remove data source. */
int  run_loop_remove_data_source(data_source_t *dataSource);

/* Execute configured run loop. 
 * This function does not return. */
void run_loop_execute(void);

/* Hack to fix HCI timer handling. */
#ifdef HAVE_TICK
/* Sets how many milliseconds has one tick. */
uint32_t embedded_ticks_for_ms(uint32_t time_in_ms);

/* Queries the current time in ticks. */
uint32_t embedded_get_ticks(void);

#endif

/* utils.h */

/* Connection handle type. */

	
/* helper for BT little endian format. */
#define READ_BT_16( buffer, pos) ( ((uint16_t) buffer[pos]) | (((uint16_t)buffer[pos+1]) << 8))
#define READ_BT_24( buffer, pos) ( ((uint32_t) buffer[pos]) | (((uint32_t)buffer[pos+1]) << 8) | (((uint32_t)buffer[pos+2]) << 16))
#define READ_BT_32( buffer, pos) ( ((uint32_t) buffer[pos]) | (((uint32_t)buffer[pos+1]) << 8) | (((uint32_t)buffer[pos+2]) << 16) | (((uint32_t) buffer[pos+3])) << 24)

/* helper for SDP big endian format. */
#define READ_NET_16( buffer, pos) ( ((uint16_t) buffer[pos+1]) | (((uint16_t)buffer[pos  ]) << 8))
#define READ_NET_32( buffer, pos) ( ((uint32_t) buffer[pos+3]) | (((uint32_t)buffer[pos+2]) << 8) | (((uint32_t)buffer[pos+1]) << 16) | (((uint32_t) buffer[pos])) << 24)

/* HCI CMD OGF/OCF. */
#define READ_CMD_OGF(buffer) (buffer[1] >> 2)
#define READ_CMD_OCF(buffer) ((buffer[1] & 0x03) << 8 | buffer[0])

/* Check if command complete event for given command. */
#define COMMAND_COMPLETE_EVENT(event,cmd) ( event[0] == HCI_EVENT_COMMAND_COMPLETE && READ_BT_16(event,3) == cmd.opcode)
#define COMMAND_STATUS_EVENT(event,cmd) ( event[0] == HCI_EVENT_COMMAND_STATUS && READ_BT_16(event,4) == cmd.opcode)

/* Code+Len=2, Pkts+Opcode=3; total=5 */
#define OFFSET_OF_DATA_IN_COMMAND_COMPLETE 5

/* ACL Packet. */
#define READ_ACL_CONNECTION_HANDLE( buffer ) ( READ_BT_16(buffer,0) & 0x0fff)
#define READ_ACL_FLAGS( buffer )      ( buffer[1] >> 4 )
#define READ_ACL_LENGTH( buffer )     (READ_BT_16(buffer, 2))

/* L2CAP Packet. */
#define READ_L2CAP_LENGTH(buffer)     ( READ_BT_16(buffer, 4))
#define READ_L2CAP_CHANNEL_ID(buffer) ( READ_BT_16(buffer, 6))

#define BD_ADDR_CMP(a,b) memcmp(a,b, BD_ADDR_LEN)
#define BD_ADDR_COPY(dest,src) memcpy(dest,src,BD_ADDR_LEN)

void bt_store_16(uint8_t *buffer, uint16_t pos, uint16_t value);

void bt_store_32(uint8_t *buffer, uint16_t pos, uint32_t value);

void bt_flip_addr(bd_addr_t dest, bd_addr_t src);

void net_store_16(uint8_t *buffer, uint16_t pos, uint16_t value);

void net_store_32(uint8_t *buffer, uint16_t pos, uint32_t value);

void hexdump(void *data, int size);

void printUUID(uint8_t *uuid);

/* Deprecated - please use more convenient bd_addr_to_str. */
void print_bd_addr( bd_addr_t addr);

char * bd_addr_to_str(bd_addr_t addr);

int sscan_bd_addr(uint8_t * addr_string, bd_addr_t addr);
    
uint8_t crc8_check(uint8_t *data, uint16_t len, uint8_t check_sum);

uint8_t crc8_calc(uint8_t *data, uint16_t len);

/* btstack.h */
	
/* Default TCP port for BTstack daemon. */
#define BTSTACK_PORT            13333

/* UNIX domain socket for BTstack. */
#define BTSTACK_UNIX            "/tmp/BTstack"


/* Optional
 *
 * If called before bt_open, TCP socket is used 
 * instead of local UNIX socket.
 *
 * note: Address is not copied and must be 
 * valid during bt_open.
 */
void bt_use_tcp(const char * address, uint16_t port); 

/* Init BTstack library. */
int bt_open(void);

/* Stop using BTstack library. */
int bt_close(void);

/* Send HCI cmd packet. */
int bt_send_cmd(const hci_cmd_t *cmd, ...);

/* Register packet handler -- channel only valid 
 for L2CAP and RFCOMM packets.
 */
btstack_packet_handler_t bt_register_packet_handler(
      btstack_packet_handler_t handler);

void bt_send_acl(uint8_t * data, uint16_t len);

void bt_send_l2cap(uint16_t local_cid, uint8_t *data, uint16_t len);

void bt_send_rfcomm(uint16_t rfcom_cid, uint8_t *data, uint16_t len);

/* custom functions */

void btpad_set_inquiry_state(bool on);

#if defined __cplusplus
}
#endif

#endif
