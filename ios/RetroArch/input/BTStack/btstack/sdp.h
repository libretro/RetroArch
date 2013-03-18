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
#pragma once

#include <stdint.h>
#include <btstack/linked_list.h>

#include "config.h"

typedef enum {
	SDP_ErrorResponse = 1,
	SDP_ServiceSearchRequest,
	SDP_ServiceSearchResponse,
	SDP_ServiceAttributeRequest,
	SDP_ServiceAttributeResponse,
	SDP_ServiceSearchAttributeRequest,
	SDP_ServiceSearchAttributeResponse
} SDP_PDU_ID_t;

// service record
// -- uses user_data field for actual
typedef struct {
    // linked list - assert: first field
    linked_item_t   item;
    
    // client connection
    void *  connection;
    
    // data is contained in same memory
    uint32_t        service_record_handle;
    uint8_t         service_record[0];
} service_record_item_t;


void sdp_init(void);

void sdp_register_packet_handler(void (*handler)(void * connection, uint8_t packet_type,
                                                 uint16_t channel, uint8_t *packet, uint16_t size));

#ifdef EMBEDDED
// register service record internally - the normal version creates a copy of the record
// pre: AttributeIDs are in ascending order => ServiceRecordHandle is first attribute if present
// @returns ServiceRecordHandle or 0 if registration failed
uint32_t sdp_register_service_internal(void *connection, service_record_item_t * record_item);
#else
// register service record internally - this special version doesn't copy the record, it cannot be freeed
// pre: AttributeIDs are in ascending order
// pre: ServiceRecordHandle is first attribute and valid
// pre: record
// @returns ServiceRecordHandle or 0 if registration failed
uint32_t sdp_register_service_internal(void *connection, uint8_t * service_record);
#endif

// unregister service record internally
void sdp_unregister_service_internal(void *connection, uint32_t service_record_handle);

//
void sdp_unregister_services_for_connection(void *connection);

//
int sdp_handle_service_search_request(uint8_t * packet, uint16_t remote_mtu);
int sdp_handle_service_attribute_request(uint8_t * packet, uint16_t remote_mtu);
int sdp_handle_service_search_attribute_request(uint8_t * packet, uint16_t remote_mtu);
