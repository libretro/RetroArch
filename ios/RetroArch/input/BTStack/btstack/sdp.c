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
 * Implementation of the Service Discovery Protocol Server 
 */

#include "sdp.h"


#include <stdio.h>
#include <string.h>

#include <btstack/sdp_util.h>

#include "hci_dump.h"
#include "l2cap.h"

#include "debug.h"

// max reserved ServiceRecordHandle
#define maxReservedServiceRecordHandle 0xffff

// max SDP response
#define SDP_RESPONSE_BUFFER_SIZE (HCI_ACL_BUFFER_SIZE-HCI_ACL_HEADER_SIZE)

static void sdp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

// registered service records
static linked_list_t sdp_service_records = NULL;

// our handles start after the reserved range
static uint32_t sdp_next_service_record_handle = maxReservedServiceRecordHandle + 2;

static uint8_t sdp_response_buffer[SDP_RESPONSE_BUFFER_SIZE];

static void (*app_packet_handler)(void * connection, uint8_t packet_type,
                                  uint16_t channel, uint8_t *packet, uint16_t size) = NULL;

static uint16_t l2cap_cid = 0;
static uint16_t sdp_response_size = 0;

void sdp_init(){
    // register with l2cap psm sevices - max MTU
    l2cap_register_service_internal(NULL, sdp_packet_handler, PSM_SDP, 0xffff);
}

// register packet handler
void sdp_register_packet_handler(void (*handler)(void * connection, uint8_t packet_type,
                                                 uint16_t channel, uint8_t *packet, uint16_t size)){
	app_packet_handler = handler;
    l2cap_cid = 0;
}

uint32_t sdp_get_service_record_handle(uint8_t * record){
    uint8_t * serviceRecordHandleAttribute = sdp_get_attribute_value_for_attribute_id(record, SDP_ServiceRecordHandle);
    if (!serviceRecordHandleAttribute) return 0;
    if (de_get_element_type(serviceRecordHandleAttribute) != DE_UINT) return 0;
    if (de_get_size_type(serviceRecordHandleAttribute) != DE_SIZE_32) return 0;
    return READ_NET_32(serviceRecordHandleAttribute, 1); 
}

// data: event(8), len(8), status(8), service_record_handle(32)
static void sdp_emit_service_registered(void *connection, uint32_t handle, uint8_t status) {
    if (!app_packet_handler) return;
    uint8_t event[7];
    event[0] = SDP_SERVICE_REGISTERED;
    event[1] = sizeof(event) - 2;
    event[2] = status;
    bt_store_32(event, 3, handle);
    hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
	(*app_packet_handler)(connection, HCI_EVENT_PACKET, 0, (uint8_t *) event, sizeof(event));
}

service_record_item_t * sdp_get_record_for_handle(uint32_t handle){
    linked_item_t *it;
    for (it = (linked_item_t *) sdp_service_records; it ; it = it->next){
        service_record_item_t * item = (service_record_item_t *) it;
        if (item->service_record_handle == handle){
            return item;
        }
    }
    return NULL;
}

// get next free, unregistered service record handle
uint32_t sdp_create_service_record_handle(void){
    uint32_t handle = 0;
    do {
        handle = sdp_next_service_record_handle++;
        if (sdp_get_record_for_handle(handle)) handle = 0;
    } while (handle == 0);
    return handle;
}

#ifdef EMBEDDED

// register service record internally - this special version doesn't copy the record, it should not be freeed
// pre: AttributeIDs are in ascending order
// pre: ServiceRecordHandle is first attribute and valid
// pre: record
// @returns ServiceRecordHandle or 0 if registration failed
uint32_t sdp_register_service_internal(void *connection, service_record_item_t * record_item){
    // get user record handle
    uint32_t record_handle = record_item->service_record_handle;
    // get actual record
    uint8_t *record = record_item->service_record;
    
    // check for ServiceRecordHandle attribute, returns pointer or null
    uint8_t * req_record_handle = sdp_get_attribute_value_for_attribute_id(record, SDP_ServiceRecordHandle);
    if (!req_record_handle) {
        log_error("SDP Error - record does not contain ServiceRecordHandle attribute\n");
        return 0;
    }
    
    // validate service record handle is not in reserved range
    if (record_handle <= maxReservedServiceRecordHandle) record_handle = 0;
    
    // check if already in use
    if (record_handle) {
        if (sdp_get_record_for_handle(record_handle)) {
            record_handle = 0;
        }
    }
    
    // create new handle if needed
    if (!record_handle){
        record_handle = sdp_create_service_record_handle();
        // Write the handle back into the record too
        record_item->service_record_handle = record_handle;
        sdp_set_attribute_value_for_attribute_id(record, SDP_ServiceRecordHandle, record_handle);
    }
    
    // add to linked list
    linked_list_add(&sdp_service_records, (linked_item_t *) record_item);
    
    sdp_emit_service_registered(connection, 0, record_item->service_record_handle);
    
    return record_handle;
}

#else

// AttributeIDList used to remove ServiceRecordHandle
static const uint8_t removeServiceRecordHandleAttributeIDList[] = { 0x36, 0x00, 0x05, 0x0A, 0x00, 0x01, 0xFF, 0xFF };

// register service record internally - the normal version creates a copy of the record
// pre: AttributeIDs are in ascending order => ServiceRecordHandle is first attribute if present
// @returns ServiceRecordHandle or 0 if registration failed
uint32_t sdp_register_service_internal(void *connection, uint8_t * record){

    // dump for now
    // printf("Register service record\n");
    // de_dump_data_element(record);
    
    // get user record handle
    uint32_t record_handle = sdp_get_service_record_handle(record);

    // validate service record handle is not in reserved range
    if (record_handle <= maxReservedServiceRecordHandle) record_handle = 0;
    
    // check if already in use
    if (record_handle) {
        if (sdp_get_record_for_handle(record_handle)) {
            record_handle = 0;
        }
    }
    
    // create new handle if needed
    if (!record_handle){
        record_handle = sdp_create_service_record_handle();
    }
    
    // calculate size of new service record: DES (2 byte len) 
    // + ServiceRecordHandle attribute (UINT16 UINT32) + size of existing attributes
    uint16_t recordSize =  3 + (3 + 5) + de_get_data_size(record);
        
    // alloc memory for new service_record_item
    service_record_item_t * newRecordItem = (service_record_item_t *) malloc(recordSize + sizeof(service_record_item_t));
    if (!newRecordItem) {
        sdp_emit_service_registered(connection, 0, BTSTACK_MEMORY_ALLOC_FAILED);
        return 0;
    }
    // link new service item to client connection
    newRecordItem->connection = connection;
    
    // set new handle
    newRecordItem->service_record_handle = record_handle;

    // create updated service record
    uint8_t * newRecord = (uint8_t *) &(newRecordItem->service_record);
    
    // create DES for new record
    de_create_sequence(newRecord);
    
    // set service record handle
    de_add_number(newRecord, DE_UINT, DE_SIZE_16, 0);
    de_add_number(newRecord, DE_UINT, DE_SIZE_32, record_handle);
    
    // add other attributes
    sdp_append_attributes_in_attributeIDList(record, (uint8_t *) removeServiceRecordHandleAttributeIDList, 0, recordSize, newRecord);
    
    // dump for now
    // de_dump_data_element(newRecord);
    // printf("reserved size %u, actual size %u\n", recordSize, de_get_len(newRecord));
    
    // add to linked list
    linked_list_add(&sdp_service_records, (linked_item_t *) newRecordItem);
    
    sdp_emit_service_registered(connection, 0, newRecordItem->service_record_handle);

    return record_handle;
}

#endif

// unregister service record internally
// 
// makes sure one client cannot remove service records of other clients
//
void sdp_unregister_service_internal(void *connection, uint32_t service_record_handle){
    service_record_item_t * record_item = sdp_get_record_for_handle(service_record_handle);
    if (record_item && record_item->connection == connection) {
        linked_list_remove(&sdp_service_records, (linked_item_t *) record_item);
    }
}

// remove all service record for a client connection
void sdp_unregister_services_for_connection(void *connection){
    linked_item_t *it = (linked_item_t *) &sdp_service_records;
    while (it->next){
        service_record_item_t *record_item = (service_record_item_t *) it->next;
        if (record_item->connection == connection){
            it->next = it->next->next;
#ifndef EMBEDDED
            free(record_item);
#endif
        } else {
            it = it->next;
        }
    }
}

// PDU
// PDU ID (1), Transaction ID (2), Param Length (2), Param 1, Param 2, ..

int sdp_create_error_response(uint16_t transaction_id, uint16_t error_code){
    sdp_response_buffer[0] = SDP_ErrorResponse;
    net_store_16(sdp_response_buffer, 1, transaction_id);
    net_store_16(sdp_response_buffer, 3, 2);
    net_store_16(sdp_response_buffer, 5, error_code); // invalid syntax
    return 7;
}

int sdp_handle_service_search_request(uint8_t * packet, uint16_t remote_mtu){
    
    // get request details
    uint16_t  transaction_id = READ_NET_16(packet, 1);
    // not used yet - uint16_t  param_len = READ_NET_16(packet, 3);
    uint8_t * serviceSearchPattern = &packet[5];
    uint16_t  serviceSearchPatternLen = de_get_len(serviceSearchPattern);
    uint16_t  maximumServiceRecordCount = READ_NET_16(packet, 5 + serviceSearchPatternLen);
    uint8_t * continuationState = &packet[5+serviceSearchPatternLen+2];
    
    // calc maxumumServiceRecordCount based on remote MTU
    uint16_t maxNrServiceRecordsPerResponse = (remote_mtu - (9+3))/4;
    
    // continuation state contains index of next service record to examine
    int      continuation = 0;
    uint16_t continuation_index = 0;
    if (continuationState[0] == 2){
        continuation_index = READ_NET_16(continuationState, 1);
    }
    
    // get and limit total count
    linked_item_t *it;
    uint16_t total_service_count   = 0;
    for (it = (linked_item_t *) sdp_service_records; it ; it = it->next){
        service_record_item_t * item = (service_record_item_t *) it;
        if (!sdp_record_matches_service_search_pattern(item->service_record, serviceSearchPattern)) continue;
        total_service_count++;
    }
    if (total_service_count > maximumServiceRecordCount){
        total_service_count = maximumServiceRecordCount;
    }
    
    // ServiceRecordHandleList at 9
    uint16_t pos = 9;
    uint16_t current_service_count  = 0;
    uint16_t current_service_index  = 0;
    uint16_t matching_service_count = 0;
    for (it = (linked_item_t *) sdp_service_records; it ; it = it->next, ++current_service_index){
        service_record_item_t * item = (service_record_item_t *) it;

        if (!sdp_record_matches_service_search_pattern(item->service_record, serviceSearchPattern)) continue;
        matching_service_count++;
        
        if (current_service_index < continuation_index) continue;

        net_store_32(sdp_response_buffer, pos, item->service_record_handle);
        pos += 4;
        current_service_count++;
        
        if (matching_service_count >= total_service_count) break;

        if (current_service_count >= maxNrServiceRecordsPerResponse){
            continuation = 1;
            continuation_index = current_service_index + 1;
            break;
        }
    }
    
    // Store continuation state
    if (continuation) {
        sdp_response_buffer[pos++] = 2;
        net_store_16(sdp_response_buffer, pos, continuation_index);
        pos += 2;
    } else {
        sdp_response_buffer[pos++] = 0;
    }

    // header
    sdp_response_buffer[0] = SDP_ServiceSearchResponse;
    net_store_16(sdp_response_buffer, 1, transaction_id);
    net_store_16(sdp_response_buffer, 3, pos - 5); // size of variable payload
    net_store_16(sdp_response_buffer, 5, total_service_count);
    net_store_16(sdp_response_buffer, 7, current_service_count);
    
    return pos;
}

int sdp_handle_service_attribute_request(uint8_t * packet, uint16_t remote_mtu){
    
    // get request details
    uint16_t  transaction_id = READ_NET_16(packet, 1);
    // not used yet - uint16_t  param_len = READ_NET_16(packet, 3);
    uint32_t  serviceRecordHandle = READ_NET_32(packet, 5);
    uint16_t  maximumAttributeByteCount = READ_NET_16(packet, 9);
    uint8_t * attributeIDList = &packet[11];
    uint16_t  attributeIDListLen = de_get_len(attributeIDList);
    uint8_t * continuationState = &packet[11+attributeIDListLen];
    
    // calc maximumAttributeByteCount based on remote MTU
    uint16_t maximumAttributeByteCount2 = remote_mtu - (7+3);
    if (maximumAttributeByteCount2 < maximumAttributeByteCount) {
        maximumAttributeByteCount = maximumAttributeByteCount2;
    }
    
    // continuation state contains the offset into the complete response
    uint16_t continuation_offset = 0;
    if (continuationState[0] == 2){
        continuation_offset = READ_NET_16(continuationState, 1);
    }
    
    // get service record
    service_record_item_t * item = sdp_get_record_for_handle(serviceRecordHandle);
    if (!item){
        // service record handle doesn't exist
        return sdp_create_error_response(transaction_id, 0x0002); /// invalid Service Record Handle
    }
    
    
    // AttributeList - starts at offset 7
    uint16_t pos = 7;
    
    if (continuation_offset == 0){
        
        // get size of this record
        uint16_t filtered_attributes_size = spd_get_filtered_size(item->service_record, attributeIDList);
        
        // store DES
        de_store_descriptor_with_len(&sdp_response_buffer[pos], DE_DES, DE_SIZE_VAR_16, filtered_attributes_size);
        maximumAttributeByteCount -= 3;
        pos += 3;
    }

    // copy maximumAttributeByteCount from record
    uint16_t bytes_used;
    int complete = sdp_filter_attributes_in_attributeIDList(item->service_record, attributeIDList, continuation_offset, maximumAttributeByteCount, &bytes_used, &sdp_response_buffer[pos]);
    pos += bytes_used;
    
    uint16_t attributeListByteCount = pos - 7;

    if (complete) {
        sdp_response_buffer[pos++] = 0;
    } else {
        continuation_offset += bytes_used;
        sdp_response_buffer[pos++] = 2;
        net_store_16(sdp_response_buffer, pos, continuation_offset);
        pos += 2;
    }

    // header
    sdp_response_buffer[0] = SDP_ServiceAttributeResponse;
    net_store_16(sdp_response_buffer, 1, transaction_id);
    net_store_16(sdp_response_buffer, 3, pos - 5);  // size of variable payload
    net_store_16(sdp_response_buffer, 5, attributeListByteCount); 
    
    return pos;
}

static uint16_t sdp_get_size_for_service_search_attribute_response(uint8_t * serviceSearchPattern, uint8_t * attributeIDList){
    uint16_t total_response_size = 0;
    linked_item_t *it;
    for (it = (linked_item_t *) sdp_service_records; it ; it = it->next){
        service_record_item_t * item = (service_record_item_t *) it;
        
        if (!sdp_record_matches_service_search_pattern(item->service_record, serviceSearchPattern)) continue;
        
        // for all service records that match
        total_response_size += 3 + spd_get_filtered_size(item->service_record, attributeIDList);
    }
    return total_response_size;
}

int sdp_handle_service_search_attribute_request(uint8_t * packet, uint16_t remote_mtu){
    
    // SDP header before attribute sevice list: 7
    // Continuation, worst case: 5
    
    // get request details
    uint16_t  transaction_id = READ_NET_16(packet, 1);
    // not used yet - uint16_t  param_len = READ_NET_16(packet, 3);
    uint8_t * serviceSearchPattern = &packet[5];
    uint16_t  serviceSearchPatternLen = de_get_len(serviceSearchPattern);
    uint16_t  maximumAttributeByteCount = READ_NET_16(packet, 5 + serviceSearchPatternLen);
    uint8_t * attributeIDList = &packet[5+serviceSearchPatternLen+2];
    uint16_t  attributeIDListLen = de_get_len(attributeIDList);
    uint8_t * continuationState = &packet[5+serviceSearchPatternLen+2+attributeIDListLen];
    
    // calc maximumAttributeByteCount based on remote MTU, SDP header and reserved Continuation block
    uint16_t maximumAttributeByteCount2 = remote_mtu - 12;
    if (maximumAttributeByteCount2 < maximumAttributeByteCount) {
        maximumAttributeByteCount = maximumAttributeByteCount2;
    }
    
    // continuation state contains: index of next service record to examine
    // continuation state contains: byte offset into this service record
    uint16_t continuation_service_index = 0;
    uint16_t continuation_offset = 0;
    if (continuationState[0] == 4){
        continuation_service_index = READ_NET_16(continuationState, 1);
        continuation_offset = READ_NET_16(continuationState, 3);
    }

    // printf("--> sdp_handle_service_search_attribute_request, cont %u/%u, max %u\n", continuation_service_index, continuation_offset, maximumAttributeByteCount);
    
    // AttributeLists - starts at offset 7
    uint16_t pos = 7;
    
    // add DES with total size for first request
    if (continuation_service_index == 0 && continuation_offset == 0){
        uint16_t total_response_size = sdp_get_size_for_service_search_attribute_response(serviceSearchPattern, attributeIDList);
        de_store_descriptor_with_len(&sdp_response_buffer[pos], DE_DES, DE_SIZE_VAR_16, total_response_size);
        // log_info("total response size %u\n", total_response_size);
        pos += 3;
        maximumAttributeByteCount -= 3;
    }
    
    // create attribute list
    int      first_answer = 1;
    int      continuation = 0;
    uint16_t current_service_index = 0;
    linked_item_t *it = (linked_item_t *) sdp_service_records;
    for ( ; it ; it = it->next, ++current_service_index){
        service_record_item_t * item = (service_record_item_t *) it;
        
        if (current_service_index < continuation_service_index ) continue;
        if (!sdp_record_matches_service_search_pattern(item->service_record, serviceSearchPattern)) continue;

        if (continuation_offset == 0){
            
            // get size of this record
            uint16_t filtered_attributes_size = spd_get_filtered_size(item->service_record, attributeIDList);
            
            // stop if complete record doesn't fits into response but we already have a partial response
            if ((filtered_attributes_size + 3 > maximumAttributeByteCount) && !first_answer) {
                continuation = 1;
                break;
            }
            
            // store DES
            de_store_descriptor_with_len(&sdp_response_buffer[pos], DE_DES, DE_SIZE_VAR_16, filtered_attributes_size);
            pos += 3;
            maximumAttributeByteCount -= 3;
        }
        
        first_answer = 0;
    
        // copy maximumAttributeByteCount from record
        uint16_t bytes_used;
        int complete = sdp_filter_attributes_in_attributeIDList(item->service_record, attributeIDList, continuation_offset, maximumAttributeByteCount, &bytes_used, &sdp_response_buffer[pos]);
        pos += bytes_used;
        maximumAttributeByteCount -= bytes_used;
        
        if (complete) {
            continuation_offset = 0;
            continue;
        }
        
        continuation = 1;
        continuation_offset += bytes_used;
        break;
    }
    
    uint16_t attributeListsByteCount = pos - 7;
    
    // Continuation State
    if (continuation){
        sdp_response_buffer[pos++] = 4;
        net_store_16(sdp_response_buffer, pos, (uint16_t) current_service_index);
        pos += 2;
        net_store_16(sdp_response_buffer, pos, continuation_offset);
        pos += 2;
    } else {
        // complete
        sdp_response_buffer[pos++] = 0;
    }
        
    // create SDP header
    sdp_response_buffer[0] = SDP_ServiceSearchAttributeResponse;
    net_store_16(sdp_response_buffer, 1, transaction_id);
    net_store_16(sdp_response_buffer, 3, pos - 5);  // size of variable payload
    net_store_16(sdp_response_buffer, 5, attributeListsByteCount);
    
    return pos;
}

static void sdp_try_respond(void){
    if (!sdp_response_size ) return;
    if (!l2cap_cid) return;
    if (!l2cap_can_send_packet_now(l2cap_cid)) return;
    
    // update state before sending packet (avoid getting called when new l2cap credit gets emitted)
    uint16_t size = sdp_response_size;
    sdp_response_size = 0;
    l2cap_send_internal(l2cap_cid, sdp_response_buffer, size);
}

// we assume that we don't get two requests in a row
static void sdp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	uint16_t transaction_id;
    SDP_PDU_ID_t pdu_id;
    uint16_t remote_mtu;
    // uint16_t param_len;
    
	switch (packet_type) {
			
		case L2CAP_DATA_PACKET:
            pdu_id = (SDP_PDU_ID_t) packet[0];
            transaction_id = READ_NET_16(packet, 1);
            // param_len = READ_NET_16(packet, 3);
            remote_mtu = l2cap_get_remote_mtu_for_local_cid(channel);
            // account for our buffer
            if (remote_mtu > SDP_RESPONSE_BUFFER_SIZE){
                remote_mtu = SDP_RESPONSE_BUFFER_SIZE;
            }
            
            // printf("SDP Request: type %u, transaction id %u, len %u, mtu %u\n", pdu_id, transaction_id, param_len, remote_mtu);
            switch (pdu_id){
                    
                case SDP_ServiceSearchRequest:
                    sdp_response_size = sdp_handle_service_search_request(packet, remote_mtu);
                    break;
                                        
                case SDP_ServiceAttributeRequest:
                    sdp_response_size = sdp_handle_service_attribute_request(packet, remote_mtu);
                    break;
                    
                case SDP_ServiceSearchAttributeRequest:
                    sdp_response_size = sdp_handle_service_search_attribute_request(packet, remote_mtu);
                    break;
                    
                default:
                    sdp_response_size = sdp_create_error_response(transaction_id, 0x0003); // invalid syntax
                    break;
            }
            
            sdp_try_respond();
            
			break;
			
		case HCI_EVENT_PACKET:
			
			switch (packet[0]) {

				case L2CAP_EVENT_INCOMING_CONNECTION:
                    if (l2cap_cid) {
                        // CONNECTION REJECTED DUE TO LIMITED RESOURCES 
                        l2cap_decline_connection_internal(channel, 0x0d);
                        break;
                    }
                    // accept
                    l2cap_cid = channel;
                    sdp_response_size = 0;
                    l2cap_accept_connection_internal(channel);
					break;
                    
                case L2CAP_EVENT_CHANNEL_OPENED:
                    if (packet[2]) {
                        // open failed -> reset
                        l2cap_cid = 0;
                    }
                    break;

                case L2CAP_EVENT_CREDITS:
                case DAEMON_EVENT_HCI_PACKET_SENT:
                    sdp_try_respond();
                    break;
                
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    if (channel == l2cap_cid){
                        // reset
                        l2cap_cid = 0;
                    }
                    break;
					                    
				default:
					// other event
					break;
			}
			break;
			
		default:
			// other packet type
			break;
	}
}

