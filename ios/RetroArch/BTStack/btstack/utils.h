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
 *  utils.h
 *
 *  General utility functions
 *
 *  Created by Matthias Ringwald on 7/23/09.
 */

#pragma once


#if defined __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief hci connection handle type
 */
typedef uint16_t hci_con_handle_t;

/**
 * @brief Length of a bluetooth device address.
 */
#define BD_ADDR_LEN 6
typedef uint8_t bd_addr_t[BD_ADDR_LEN];

/**
 * @brief The link key type
 */
#define LINK_KEY_LEN 16
typedef uint8_t link_key_t[LINK_KEY_LEN]; 

/**
 * @brief The device name type
 */
#define DEVICE_NAME_LEN 248
typedef uint8_t device_name_t[DEVICE_NAME_LEN+1]; 
	
	
// helper for BT little endian format
#define READ_BT_16( buffer, pos) ( ((uint16_t) buffer[pos]) | (((uint16_t)buffer[pos+1]) << 8))
#define READ_BT_24( buffer, pos) ( ((uint32_t) buffer[pos]) | (((uint32_t)buffer[pos+1]) << 8) | (((uint32_t)buffer[pos+2]) << 16))
#define READ_BT_32( buffer, pos) ( ((uint32_t) buffer[pos]) | (((uint32_t)buffer[pos+1]) << 8) | (((uint32_t)buffer[pos+2]) << 16) | (((uint32_t) buffer[pos+3])) << 24)

// helper for SDP big endian format
#define READ_NET_16( buffer, pos) ( ((uint16_t) buffer[pos+1]) | (((uint16_t)buffer[pos  ]) << 8))
#define READ_NET_32( buffer, pos) ( ((uint32_t) buffer[pos+3]) | (((uint32_t)buffer[pos+2]) << 8) | (((uint32_t)buffer[pos+1]) << 16) | (((uint32_t) buffer[pos])) << 24)

// HCI CMD OGF/OCF
#define READ_CMD_OGF(buffer) (buffer[1] >> 2)
#define READ_CMD_OCF(buffer) ((buffer[1] & 0x03) << 8 | buffer[0])

// check if command complete event for given command
#define COMMAND_COMPLETE_EVENT(event,cmd) ( event[0] == HCI_EVENT_COMMAND_COMPLETE && READ_BT_16(event,3) == cmd.opcode)
#define COMMAND_STATUS_EVENT(event,cmd) ( event[0] == HCI_EVENT_COMMAND_STATUS && READ_BT_16(event,4) == cmd.opcode)

// Code+Len=2, Pkts+Opcode=3; total=5
#define OFFSET_OF_DATA_IN_COMMAND_COMPLETE 5

// ACL Packet
#define READ_ACL_CONNECTION_HANDLE( buffer ) ( READ_BT_16(buffer,0) & 0x0fff)
#define READ_ACL_FLAGS( buffer )      ( buffer[1] >> 4 )
#define READ_ACL_LENGTH( buffer )     (READ_BT_16(buffer, 2))

// L2CAP Packet
#define READ_L2CAP_LENGTH(buffer)     ( READ_BT_16(buffer, 4))
#define READ_L2CAP_CHANNEL_ID(buffer) ( READ_BT_16(buffer, 6))

void bt_store_16(uint8_t *buffer, uint16_t pos, uint16_t value);
void bt_store_32(uint8_t *buffer, uint16_t pos, uint32_t value);
void bt_flip_addr(bd_addr_t dest, bd_addr_t src);

void net_store_16(uint8_t *buffer, uint16_t pos, uint16_t value);
void net_store_32(uint8_t *buffer, uint16_t pos, uint32_t value);

void hexdump(void *data, int size);
void printUUID(uint8_t *uuid);

// @deprecated please use more convenient bd_addr_to_str
void print_bd_addr( bd_addr_t addr);
char * bd_addr_to_str(bd_addr_t addr);

int sscan_bd_addr(uint8_t * addr_string, bd_addr_t addr);
    
uint8_t crc8_check(uint8_t *data, uint16_t len, uint8_t check_sum);
uint8_t crc8_calc(uint8_t *data, uint16_t len);

#define BD_ADDR_CMP(a,b) memcmp(a,b, BD_ADDR_LEN)
#define BD_ADDR_COPY(dest,src) memcpy(dest,src,BD_ADDR_LEN)

#if defined __cplusplus
}
#endif
		
