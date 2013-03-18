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
 *  hci_cmds.c
 *
 *  Created by Matthias Ringwald on 7/23/09.
 */

#include "hci_cmds.h"

#include <string.h>

#include "sdp_util.h"
#include "config.h"
#include "hci.h"

// calculate combined ogf/ocf value
#define OPCODE(ogf, ocf) (ocf | ogf << 10)

/**
 * construct HCI Command based on template
 *
 * Format:
 *   1,2,3,4: one to four byte value
 *   H: HCI connection handle
 *   B: Bluetooth Baseband Address (BD_ADDR)
 *   E: Extended Inquiry Result
 *   N: Name up to 248 chars, \0 terminated
 *   P: 16 byte Pairing code
 *   S: Service Record (Data Element Sequence)
 */
uint16_t hci_create_cmd_internal(uint8_t *hci_cmd_buffer, const hci_cmd_t *cmd, va_list argptr){
    
    hci_cmd_buffer[0] = cmd->opcode & 0xff;
    hci_cmd_buffer[1] = cmd->opcode >> 8;
    int pos = 3;
    
    const char *format = cmd->format;
    uint16_t word;
    uint32_t longword;
    uint8_t * ptr;
    while (*format) {
        switch(*format) {
            case '1': //  8 bit value
            case '2': // 16 bit value
            case 'H': // hci_handle
                word = va_arg(argptr, int);  // minimal va_arg is int: 2 bytes on 8+16 bit CPUs
                hci_cmd_buffer[pos++] = word & 0xff;
                if (*format == '2') {
                    hci_cmd_buffer[pos++] = word >> 8;
                } else if (*format == 'H') {
                    // TODO implement opaque client connection handles
                    //      pass module handle for now
                    hci_cmd_buffer[pos++] = word >> 8;
                } 
                break;
            case '3':
            case '4':
                longword = va_arg(argptr, uint32_t);
                // longword = va_arg(argptr, int);
                hci_cmd_buffer[pos++] = longword;
                hci_cmd_buffer[pos++] = longword >> 8;
                hci_cmd_buffer[pos++] = longword >> 16;
                if (*format == '4'){
                    hci_cmd_buffer[pos++] = longword >> 24;
                }
                break;
            case 'B': // bt-addr
                ptr = va_arg(argptr, uint8_t *);
                hci_cmd_buffer[pos++] = ptr[5];
                hci_cmd_buffer[pos++] = ptr[4];
                hci_cmd_buffer[pos++] = ptr[3];
                hci_cmd_buffer[pos++] = ptr[2];
                hci_cmd_buffer[pos++] = ptr[1];
                hci_cmd_buffer[pos++] = ptr[0];
                break;
            case 'E': // Extended Inquiry Information 240 octets
                ptr = va_arg(argptr, uint8_t *);
                memcpy(&hci_cmd_buffer[pos], ptr, 240);
                pos += 240;
                break;
            case 'N': { // UTF-8 string, null terminated
                ptr = va_arg(argptr, uint8_t *);
                uint16_t len = strlen((const char*) ptr);
                if (len > 248) {
                    len = 248;
                }
                memcpy(&hci_cmd_buffer[pos], ptr, len);
                if (len < 248) {
                    // fill remaining space with zeroes
                    memset(&hci_cmd_buffer[pos+len], 0, 248-len);
                }
                pos += 248;
                break;
            }
            case 'P': // 16 byte PIN code or link key
                ptr = va_arg(argptr, uint8_t *);
                memcpy(&hci_cmd_buffer[pos], ptr, 16);
                pos += 16;
                break;
#ifdef HAVE_BLE
            case 'A': // 31 bytes advertising data
                ptr = va_arg(argptr, uint8_t *);
                memcpy(&hci_cmd_buffer[pos], ptr, 31);
                pos += 31;
                break;
#endif
#ifdef HAVE_SDP
            case 'S': { // Service Record (Data Element Sequence)
                ptr = va_arg(argptr, uint8_t *);
                uint16_t len = de_get_len(ptr);
                memcpy(&hci_cmd_buffer[pos], ptr, len);
                pos += len;
                break;
            }
#endif
            default:
                break;
        }
        format++;
    };
    hci_cmd_buffer[2] = pos - 3;
    return pos;
}

/**
 * construct HCI Command based on template
 *
 * mainly calls hci_create_cmd_internal
 */
uint16_t hci_create_cmd(uint8_t *hci_cmd_buffer, hci_cmd_t *cmd, ...){
    va_list argptr;
    va_start(argptr, cmd);
    uint16_t len = hci_create_cmd_internal(hci_cmd_buffer, cmd, argptr);
    va_end(argptr);
    return len;
}


/**
 *  Link Control Commands 
 */
const hci_cmd_t hci_inquiry = {
OPCODE(OGF_LINK_CONTROL, 0x01), "311"
// LAP, Inquiry length, Num_responses
};
const hci_cmd_t hci_inquiry_cancel = {
OPCODE(OGF_LINK_CONTROL, 0x02), ""
// no params
};
const hci_cmd_t hci_create_connection = {
OPCODE(OGF_LINK_CONTROL, 0x05), "B21121"
// BD_ADDR, Packet_Type, Page_Scan_Repetition_Mode, Reserved, Clock_Offset, Allow_Role_Switch
};
const hci_cmd_t hci_disconnect = {
OPCODE(OGF_LINK_CONTROL, 0x06), "H1"
// Handle, Reason: 0x05, 0x13-0x15, 0x1a, 0x29
// see Errors Codes in BT Spec Part D
};
const hci_cmd_t hci_create_connection_cancel = {
OPCODE(OGF_LINK_CONTROL, 0x08), "B"
// BD_ADDR
};
const hci_cmd_t hci_accept_connection_request = {
OPCODE(OGF_LINK_CONTROL, 0x09), "B1"
// BD_ADDR, Role: become master, stay slave
};
const hci_cmd_t hci_reject_connection_request = {
OPCODE(OGF_LINK_CONTROL, 0x0a), "B1"
// BD_ADDR, reason e.g. CONNECTION REJECTED DUE TO LIMITED RESOURCES (0x0d)
};
const hci_cmd_t hci_link_key_request_reply = {
OPCODE(OGF_LINK_CONTROL, 0x0b), "BP"
// BD_ADDR, LINK_KEY
};
const hci_cmd_t hci_link_key_request_negative_reply = {
OPCODE(OGF_LINK_CONTROL, 0x0c), "B"
// BD_ADDR
};
const hci_cmd_t hci_pin_code_request_reply = {
OPCODE(OGF_LINK_CONTROL, 0x0d), "B1P"
// BD_ADDR, pin length, PIN: c-string
};
const hci_cmd_t hci_pin_code_request_negative_reply = {
OPCODE(OGF_LINK_CONTROL, 0x0e), "B"
// BD_ADDR
};
const hci_cmd_t hci_authentication_requested = {
OPCODE(OGF_LINK_CONTROL, 0x11), "H"
// Handle
};
const hci_cmd_t hci_set_connection_encryption = {
OPCODE(OGF_LINK_CONTROL, 0x13), "H1"
// Handle, Encryption_Enable
};
const hci_cmd_t hci_change_connection_link_key = {
OPCODE(OGF_LINK_CONTROL, 0x15), "H"
// Handle
};
const hci_cmd_t hci_remote_name_request = {
OPCODE(OGF_LINK_CONTROL, 0x19), "B112"
// BD_ADDR, Page_Scan_Repetition_Mode, Reserved, Clock_Offset
};
const hci_cmd_t hci_remote_name_request_cancel = {
OPCODE(OGF_LINK_CONTROL, 0x1A), "B"
// BD_ADDR
};

/**
 *  Link Policy Commands 
 */
const hci_cmd_t hci_sniff_mode = {
OPCODE(OGF_LINK_POLICY, 0x03), "H2222"
// handle, Sniff_Max_Interval, Sniff_Min_Interval, Sniff_Attempt, Sniff_Timeout:
};
const hci_cmd_t hci_qos_setup = {
OPCODE(OGF_LINK_POLICY, 0x07), "H114444"
// handle, flags, service_type, token rate (bytes/s), peak bandwith (bytes/s),
// latency (us), delay_variation (us)
};
const hci_cmd_t hci_role_discovery = {
OPCODE(OGF_LINK_POLICY, 0x09), "H"
// handle
};
const hci_cmd_t hci_switch_role_command= {
OPCODE(OGF_LINK_POLICY, 0x0b), "B1"
// BD_ADDR, role: {0=master,1=slave}
};
const hci_cmd_t hci_read_link_policy_settings = {
OPCODE(OGF_LINK_POLICY, 0x0c), "H"
// handle 
};
const hci_cmd_t hci_write_link_policy_settings = {
OPCODE(OGF_LINK_POLICY, 0x0d), "H2"
// handle, settings
};

/**
 *  Controller & Baseband Commands 
 */
const hci_cmd_t hci_set_event_mask = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x01), "44"
// event_mask lower 4 octets, higher 4 bytes
};
const hci_cmd_t hci_reset = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x03), ""
// no params
};
const hci_cmd_t hci_delete_stored_link_key = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x12), "B1"
// BD_ADDR, Delete_All_Flag
};
const hci_cmd_t hci_write_local_name = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x13), "N"
// Local name (UTF-8, Null Terminated, max 248 octets)
};
const hci_cmd_t hci_write_page_timeout = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x18), "2"
// Page_Timeout * 0.625 ms
};
const hci_cmd_t hci_write_scan_enable = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x1A), "1"
// Scan_enable: no, inq, page, inq+page
};
const hci_cmd_t hci_write_authentication_enable = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x20), "1"
// Authentication_Enable
};
const hci_cmd_t hci_write_class_of_device = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x24), "3"
// Class of Device
};
const hci_cmd_t hci_read_num_broadcast_retransmissions = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x29), ""
};
const hci_cmd_t hci_write_num_broadcast_retransmissions = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x2a), "1"
// Num broadcast retransmissions (e.g. 0 for a single broadcast)
};
const hci_cmd_t hci_host_buffer_size = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x33), "2122"
// Host_ACL_Data_Packet_Length:, Host_Synchronous_Data_Packet_Length:, Host_Total_Num_ACL_Data_Packets:, Host_Total_Num_Synchronous_Data_Packets:
};
const hci_cmd_t hci_read_link_supervision_timeout = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x36), "H"
// handle
};
const hci_cmd_t hci_write_link_supervision_timeout = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x37), "H2"
// handle, Range for N: 0x0001 Ð 0xFFFF Time (Range: 0.625ms Ð 40.9 sec)
};
const hci_cmd_t hci_write_inquiry_mode = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x45), "1"
// Inquiry mode: 0x00 = standard, 0x01 = with RSSI, 0x02 = extended
};
const hci_cmd_t hci_write_extended_inquiry_response = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x52), "1E"
// FEC_Required, Exstended Inquiry Response
};
const hci_cmd_t hci_write_simple_pairing_mode = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x56), "1"
// mode: 0 = off, 1 = on
};
const hci_cmd_t hci_read_le_host_supported = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x6c), ""
// params: none
// return: status, le supported host, simultaneous le host
};
const hci_cmd_t hci_write_le_host_supported = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x6d), "11"
// param: le supported host, simultaneous le host
// return: status
};

/**
 * Informational Parameters
 */
const hci_cmd_t hci_read_local_supported_features = {
OPCODE(OGF_INFORMATIONAL_PARAMETERS, 0x03), ""
// no params
};
const hci_cmd_t hci_read_buffer_size = {
OPCODE(OGF_INFORMATIONAL_PARAMETERS, 0x05), ""
// no params
};
const hci_cmd_t hci_read_bd_addr = {
OPCODE(OGF_INFORMATIONAL_PARAMETERS, 0x09), ""
// no params
};

#ifdef HAVE_BLE
/**
 * Low Energy Commands
 */
const hci_cmd_t hci_le_set_event_mask = {
OPCODE(OGF_LE_CONTROLLER, 0x01), "44"
// params: event_mask lower 4 octets, higher 4 bytes
// return: status
};
const hci_cmd_t hci_le_read_buffer_size = {
OPCODE(OGF_LE_CONTROLLER, 0x02), ""
// params: none
// return: status, le acl data packet len (16), total num le acl data packets(8)
};
const hci_cmd_t hci_le_read_supported_features = {
OPCODE(OGF_LE_CONTROLLER, 0x03), ""
// params: none
// return: LE_Features See [Vol 6] Part B, Section 4.6
};
const hci_cmd_t hci_le_set_random_address = {
OPCODE(OGF_LE_CONTROLLER, 0x05), "B"
// params: random device address
// return: status
};
const hci_cmd_t hci_le_set_advertising_parameters = {
OPCODE(OGF_LE_CONTROLLER, 0x06), "22111B11"
// param: min advertising interval, [0x0020,0x4000], default: 0x0800, unit: 0.625 msec
// param: max advertising interval, [0x0020,0x4000], default: 0x0800, unit: 0.625 msec
// param: advertising type (enum from 0): ADV_IND, ADC_DIRECT_IND, ADV_SCAN_IND, ADV_NONCONN_IND
// param: own address type (enum from 0): public device address, random device address
// param: direct address type (enum from 0): public device address, random device address
// param: direct address - public or random address of device to be connecteed
// param: advertising channel map (flags): chan_37(1), chan_38(2), chan_39(4)
// param: advertising filter policy (enum from 0): scan any conn any, scan whitelist, con any, scan any conn whitelist, scan whitelist, con whitelist
// return: status
};
const hci_cmd_t hci_le_read_advertising_channel_tx_power = {
OPCODE(OGF_LE_CONTROLLER, 0x07), ""
// params: none
// return: status, level [-20,10] signed int (8), units dBm
};
const hci_cmd_t hci_le_set_advertising_data= {
OPCODE(OGF_LE_CONTROLLER, 0x08), "1A"
// param: advertising data len
// param: advertising data (31 bytes)
// return: status
};
const hci_cmd_t hci_le_set_scan_response_data= {
OPCODE(OGF_LE_CONTROLLER, 0x09), "1A"
// param: scan response data len
// param: scan response data (31 bytes)
// return: status
};
const hci_cmd_t hci_le_set_advertise_enable = {
OPCODE(OGF_LE_CONTROLLER, 0x0a), "1"
// params: avertise enable: off (0), on (1)
// return: status
};
const hci_cmd_t hci_le_set_scan_parameters = {
OPCODE(OGF_LE_CONTROLLER, 0x0b), "12211"
// param: le scan type: passive (0), active (1)
// param: le scan interval [0x0004,0x4000], unit: 0.625 msec
// param: le scan window   [0x0004,0x4000], unit: 0.625 msec
// param: own address type: public (0), random (1)
// param: scanning filter policy: any (0), only whitelist (1)
// return: status
};
const hci_cmd_t hci_le_set_scan_enable = {
OPCODE(OGF_LE_CONTROLLER, 0x0c), "11"
// param: le scan enable:  disabled (0), enabled (1)
// param: filter duplices: disabled (0), enabled (1)
// return: status
};
const hci_cmd_t hci_le_create_connection= {
OPCODE(OGF_LE_CONTROLLER, 0x0d), "2211B1222222"
// param: le scan interval, [0x0004, 0x4000], unit: 0.625 msec
// param: le scan window, [0x0004, 0x4000], unit: 0.625 msec
// param: initiator filter policy: peer address type + peer address (0), whitelist (1)
// param: peer address type: public (0), random (1)
// param: peer address
// param: own address type: public (0), random (1)
// param: conn interval min, [0x0006, 0x0c80], unit: 1.25 msec
// param: conn interval max, [0x0006, 0x0c80], unit: 1.25 msec
// param: conn latency, number of connection events [0x0000, 0x01f4]
// param: supervision timeout, [0x000a, 0x0c80], unit: 10 msec
// param: minimum CE length, [0x0000, 0xffff], unit: 0.625 msec
// return: none -> le create connection complete event
};
const hci_cmd_t hci_le_create_connection_cancel = {
OPCODE(OGF_LE_CONTROLLER, 0x0e), ""
// params: none
// return: status
};
const hci_cmd_t hci_le_read_white_list_size = {
OPCODE(OGF_LE_CONTROLLER, 0x0f), ""
// params: none
// return: status, number of entries in controller whitelist
};
const hci_cmd_t hci_le_clear_white_list = {
OPCODE(OGF_LE_CONTROLLER, 0x10), ""
// params: none
// return: status
};
const hci_cmd_t hci_le_add_device_to_whitelist = {
OPCODE(OGF_LE_CONTROLLER, 0x11), "1B"
// param: address type: public (0), random (1)
// param: address
// return: status
};
const hci_cmd_t hci_le_remove_device_from_whitelist = {
OPCODE(OGF_LE_CONTROLLER, 0x12), "1B"
// param: address type: public (0), random (1)
// param: address
// return: status
};
const hci_cmd_t hci_le_connection_update = {
OPCODE(OGF_LE_CONTROLLER, 0x13), "H222222"
// param: conn handle
// param: conn interval min, [0x0006,0x0c80], unit: 1.25 msec
// param: conn interval max, [0x0006,0x0c80], unit: 1.25 msec
// param: conn latency, [0x0000,0x03e8], number of connection events
// param: supervision timeout, [0x000a,0x0c80], unit: 10 msec
// param: minimum CE length, [0x0000,0xffff], unit: 0.625 msec
// param: maximum CE length, [0x0000,0xffff], unit: 0.625 msec
// return: none -> le connection update complete event
};
const hci_cmd_t hci_le_set_host_channel_classification = {
OPCODE(OGF_LE_CONTROLLER, 0x14), "41"
// param: channel map 37 bit, split into first 32 and higher 5 bits
// return: status
};
const hci_cmd_t hci_le_read_channel_map = {
OPCODE(OGF_LE_CONTROLLER, 0x15), "H"
// params: connection handle
// return: status, connection handle, channel map (5 bytes, 37 used)
};
const hci_cmd_t hci_le_read_remote_used_features = {
OPCODE(OGF_LE_CONTROLLER, 0x16), "H"
// params: connection handle
// return: none -> le read remote used features complete event
};
const hci_cmd_t hci_le_encrypt = {
OPCODE(OGF_LE_CONTROLLER, 0x17), "PP"
// param: key (128) for AES-128
// param: plain text (128) 
// return: status, encrypted data (128)
};
const hci_cmd_t hci_le_rand = {
OPCODE(OGF_LE_CONTROLLER, 0x18), ""
// params: none
// return: status, random number (64)
};
const hci_cmd_t hci_le_start_encryption = {
OPCODE(OGF_LE_CONTROLLER, 0x19), "H442P"
// param: connection handle
// param: 64 bit random number lower  32 bit
// param: 64 bit random number higher 32 bit
// param: encryption diversifier (16)
// param: long term key (128)
// return: none -> encryption changed or encryption key refresh complete event
};
const hci_cmd_t hci_le_long_term_key_request_reply = {
OPCODE(OGF_LE_CONTROLLER, 0x1a), "HP"
// param: connection handle
// param: long term key (128)
// return: status, connection handle
};
const hci_cmd_t hci_le_long_term_key_negative_reply = {
OPCODE(OGF_LE_CONTROLLER, 0x1b), "H"
// param: connection handle
// return: status, connection handle
};
const hci_cmd_t hci_le_read_supported_states = {
OPCODE(OGF_LE_CONTROLLER, 0x1c), "H"
// param: none
// return: status, LE states (64)
};
const hci_cmd_t hci_le_receiver_test = {
OPCODE(OGF_LE_CONTROLLER, 0x1d), "1"
// param: rx frequency, [0x00 0x27], frequency (MHz): 2420 + N*2
// return: status
};
const hci_cmd_t hci_le_transmitter_test = {
    OPCODE(OGF_LE_CONTROLLER, 0x1e), "111"
    // param: tx frequency, [0x00 0x27], frequency (MHz): 2420 + N*2
    // param: lengh of test payload [0x00,0x25]
    // param: packet payload [0,7] different patterns
    // return: status
};
const hci_cmd_t hci_le_test_end = {
    OPCODE(OGF_LE_CONTROLLER, 0x1f), "1"
    // params: none
    // return: status, number of packets (8)
};
#endif

// BTstack commands
const hci_cmd_t btstack_get_state = {
OPCODE(OGF_BTSTACK, BTSTACK_GET_STATE), ""
// no params -> 
};

const hci_cmd_t btstack_set_power_mode = {
OPCODE(OGF_BTSTACK, BTSTACK_SET_POWER_MODE), "1"
// mode: 0 = off, 1 = on
};

const hci_cmd_t btstack_set_acl_capture_mode = {
OPCODE(OGF_BTSTACK, BTSTACK_SET_ACL_CAPTURE_MODE), "1"
// mode: 0 = off, 1 = on
};

const hci_cmd_t btstack_get_version = {
OPCODE(OGF_BTSTACK, BTSTACK_GET_VERSION), ""
};

const hci_cmd_t btstack_get_system_bluetooth_enabled = {
OPCODE(OGF_BTSTACK, BTSTACK_GET_SYSTEM_BLUETOOTH_ENABLED), ""
};

const hci_cmd_t btstack_set_system_bluetooth_enabled = {
OPCODE(OGF_BTSTACK, BTSTACK_SET_SYSTEM_BLUETOOTH_ENABLED), "1"
};

const hci_cmd_t btstack_set_discoverable = {
OPCODE(OGF_BTSTACK, BTSTACK_SET_DISCOVERABLE), "1"
};

const hci_cmd_t btstack_set_bluetooth_enabled = {
// only used by btstack config
OPCODE(OGF_BTSTACK, BTSTACK_SET_BLUETOOTH_ENABLED), "1"
};

const hci_cmd_t l2cap_create_channel = {
OPCODE(OGF_BTSTACK, L2CAP_CREATE_CHANNEL), "B2"
// @param bd_addr(48), psm (16)
};
const hci_cmd_t l2cap_create_channel_mtu = {
OPCODE(OGF_BTSTACK, L2CAP_CREATE_CHANNEL_MTU), "B22"
// @param bd_addr(48), psm (16), mtu (16)
};
const hci_cmd_t l2cap_disconnect = {
OPCODE(OGF_BTSTACK, L2CAP_DISCONNECT), "21"
// @param channel(16), reason(8)
};
const hci_cmd_t l2cap_register_service = {
OPCODE(OGF_BTSTACK, L2CAP_REGISTER_SERVICE), "22"
// @param psm (16), mtu (16)
};
const hci_cmd_t l2cap_unregister_service = {
OPCODE(OGF_BTSTACK, L2CAP_UNREGISTER_SERVICE), "2"
// @param psm (16)
};
const hci_cmd_t l2cap_accept_connection = {
OPCODE(OGF_BTSTACK, L2CAP_ACCEPT_CONNECTION), "2"
// @param source cid (16)
};
const hci_cmd_t l2cap_decline_connection = {
OPCODE(OGF_BTSTACK, L2CAP_DECLINE_CONNECTION), "21"
// @param source cid (16), reason(8)
};
const hci_cmd_t sdp_register_service_record = {
OPCODE(OGF_BTSTACK, SDP_REGISTER_SERVICE_RECORD), "S"
// @param service record handle (DES)
};
const hci_cmd_t sdp_unregister_service_record = {
OPCODE(OGF_BTSTACK, SDP_UNREGISTER_SERVICE_RECORD), "4"
// @param service record handle (32)
};

// create rfcomm channel: @param bd_addr(48), channel (8)
const hci_cmd_t rfcomm_create_channel = {
	OPCODE(OGF_BTSTACK, RFCOMM_CREATE_CHANNEL), "B1"
};
// create rfcomm channel: @param bd_addr(48), channel (8), mtu (16), credits (8)
const hci_cmd_t rfcomm_create_channel_with_initial_credits = {
	OPCODE(OGF_BTSTACK, RFCOMM_CREATE_CHANNEL_WITH_CREDITS), "B121"
};
// grant credits: @param rfcomm_cid(16), credits (8)
const hci_cmd_t rfcomm_grants_credits= {
	OPCODE(OGF_BTSTACK, RFCOMM_GRANT_CREDITS), "21"
};
// disconnect rfcomm disconnect, @param rfcomm_cid(16), reason(8)
const  hci_cmd_t rfcomm_disconnect = {
	OPCODE(OGF_BTSTACK, RFCOMM_DISCONNECT), "21"
};

// register rfcomm service: @param channel(8), mtu (16)
const hci_cmd_t rfcomm_register_service = {
    OPCODE(OGF_BTSTACK, RFCOMM_REGISTER_SERVICE), "12"
};
// register rfcomm service: @param channel(8), mtu (16), initial credits (8)
const hci_cmd_t rfcomm_register_service_with_initial_credits = {
    OPCODE(OGF_BTSTACK, RFCOMM_REGISTER_SERVICE_WITH_CREDITS), "121"
};

// unregister rfcomm service, @param service_channel(16)
const hci_cmd_t rfcomm_unregister_service = {
    OPCODE(OGF_BTSTACK, RFCOMM_UNREGISTER_SERVICE), "2"
};
// accept connection @param source cid (16)
const hci_cmd_t rfcomm_accept_connection = {
    OPCODE(OGF_BTSTACK, RFCOMM_ACCEPT_CONNECTION), "2"
};
// decline connection @param source cid (16)
const hci_cmd_t rfcomm_decline_connection = {
    OPCODE(OGF_BTSTACK, RFCOMM_DECLINE_CONNECTION), "21"
};
// request persisten rfcomm channel number for named service
const hci_cmd_t rfcomm_persistent_channel_for_service = {
    OPCODE(OGF_BTSTACK, RFCOMM_PERSISTENT_CHANNEL), "N"
};

// register rfcomm service: @param channel(8), mtu (16), initial credits (8)
extern const hci_cmd_t rfcomm_register_service_with_initial_credits;
