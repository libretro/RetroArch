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
 *  sdp_util.c
 */

#include <btstack/sdp_util.h>
#include <btstack/utils.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>   // PRIx32

// workaround for missing PRIx32 on mspgcc (16-bit MCU)
#ifndef PRIx32
#warning Using own: #define PRIx32 "lx"
#define PRIx32 "lx"
#endif

// date element type names
const char * const type_names[] = { "NIL", "UINT", "INT", "UUID", "STRING", "BOOL", "DES", "DEA", "URL"};

// Bluetooth Base UUID: 00000000-0000-1000-8000- 00805F9B34FB
const uint8_t sdp_bluetooth_base_uuid[] = { 0x00, 0x00, 0x00, 0x00, /* - */ 0x00, 0x00, /* - */ 0x10, 0x00, /* - */
    0x80, 0x00, /* - */ 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB };

void sdp_normalize_uuid(uint8_t *uuid, uint32_t shortUUID){
    memcpy(uuid, sdp_bluetooth_base_uuid, 16);
    net_store_32(uuid, 0, shortUUID);
}

// MARK: DataElement getter
de_size_t de_get_size_type(uint8_t *header){
    return (de_size_t) (header[0] & 7);
}

de_type_t de_get_element_type(uint8_t *header){
    return (de_type_t) (header[0] >> 3);
}

int de_get_header_size(uint8_t * header){
    de_size_t de_size = de_get_size_type(header);
    if (de_size <= DE_SIZE_128) {
        return 1;
    }
    return 1 + (1 << (de_size-DE_SIZE_VAR_8));
}

int de_get_data_size(uint8_t * header){
    uint32_t result = 0;
    de_type_t de_type = de_get_element_type(header);
    de_size_t de_size = de_get_size_type(header);
    switch (de_size){
        case DE_SIZE_VAR_8:
            result = header[1];
            break;
        case DE_SIZE_VAR_16:
            result = READ_NET_16(header,1);
            break;
        case DE_SIZE_VAR_32:
            result = READ_NET_32(header,1);
            break;
        default:
        // case DE_SIZE_8:
        // case DE_SIZE_16:
        // case DE_SIZE_32:
        // case DE_SIZE_64:
        // case DE_SIZE_128:
            if (de_type == DE_NIL) return 0;
            return 1 << de_size;
    }
    return result;    
}

int de_get_len(uint8_t *header){
    return de_get_header_size(header) + de_get_data_size(header); 
}

// @returns: element is valid UUID
int de_get_normalized_uuid(uint8_t *uuid128, uint8_t *element){
    de_type_t uuidType = de_get_element_type(element);
    de_size_t uuidSize = de_get_size_type(element);
    if (uuidType != DE_UUID) return 0;
    uint32_t shortUUID;
    switch (uuidSize){
        case DE_SIZE_16:
            shortUUID = READ_NET_16(element, 1);
            break;
        case DE_SIZE_32:
            shortUUID = READ_NET_32(element, 1);
            break;
        case DE_SIZE_128:
            memcpy(uuid128, element+1, 16);
            return 1;
        default:
            return 0;
    }
    sdp_normalize_uuid(uuid128, shortUUID);
    return 1;
}

// functions to create record
static void de_store_descriptor(uint8_t * header, de_type_t type, de_size_t size){
    header[0] = (type << 3) | size; 
}

void de_store_descriptor_with_len(uint8_t * header, de_type_t type, de_size_t size, uint32_t len){
    header[0] = (type << 3) | size; 
    switch (size){
        case DE_SIZE_VAR_8:
            header[1] = len;
            break;
        case DE_SIZE_VAR_16:
            net_store_16(header, 1, len);
            break;
        case DE_SIZE_VAR_32:
            net_store_32(header, 1, len);
            break;
        default:
            break;
    }
}

// MARK: DataElement creation

/* starts a new sequence in empty buffer - first call */
void de_create_sequence(uint8_t *header){
    de_store_descriptor_with_len( header, DE_DES, DE_SIZE_VAR_16, 0); // DES, 2 Byte Length
};

/* starts a sub-sequence, @returns handle for sub-sequence */
uint8_t * de_push_sequence(uint8_t *header){
    int element_len = de_get_len(header);
    de_store_descriptor_with_len(header+element_len, DE_DES, DE_SIZE_VAR_16, 0); // DES, 2 Byte Length
    return header + element_len;
}

/* closes the current sequence and updates the parent sequence */
void de_pop_sequence(uint8_t * parent, uint8_t * child){
    int child_len = de_get_len(child);
    int data_size_parent = READ_NET_16(parent,1);
    net_store_16(parent, 1, data_size_parent + child_len);
}

/* adds a single number value and 16+32 bit UUID to the sequence */
void de_add_number(uint8_t *seq, de_type_t type, de_size_t size, uint32_t value){
    int data_size   = READ_NET_16(seq,1);
    int element_size = 1;   // e.g. for DE_TYPE_NIL
    de_store_descriptor(seq+3+data_size, type, size); 
    switch (size){
        case DE_SIZE_8:
            if (type != DE_NIL){
                seq[4+data_size] = value;
                element_size = 2;
            }
            break;
        case DE_SIZE_16:
            net_store_16(seq, 4+data_size, value);
            element_size = 3;
            break;
        case DE_SIZE_32:
            net_store_32(seq, 4+data_size, value);
            element_size = 5;
            break;
        default:
            break;
    }
    net_store_16(seq, 1, data_size+element_size);
}

/* add a single block of data, e.g. as DE_STRING, DE_URL */
void de_add_data( uint8_t *seq, de_type_t type, uint16_t size, uint8_t *data){
    int data_size   = READ_NET_16(seq,1);
    if (size > 0xff) {
        // use 16-bit lengh information (3 byte header)
        de_store_descriptor_with_len(seq+3+data_size, type, DE_SIZE_VAR_16, size); 
        data_size += 3;
    } else {
        // use 8-bit lengh information (2 byte header)
        de_store_descriptor_with_len(seq+3+data_size, type, DE_SIZE_VAR_8, size); 
        data_size += 2;
    }
    memcpy( seq + 3 + data_size, data, size);
    data_size += size;
    net_store_16(seq, 1, data_size);
}

void de_add_uuid128(uint8_t * seq, uint8_t * uuid){
    int data_size   = READ_NET_16(seq,1);
    de_store_descriptor(seq+3+data_size, DE_UUID, DE_SIZE_128); 
    memcpy( seq + 4 + data_size, uuid, 16);
    net_store_16(seq, 1, data_size+1+16);
}

void sdp_add_attribute(uint8_t *seq, uint16_t attributeID, uint8_t attributeValue){
}

// MARK: DataElementSequence traversal
typedef int (*de_traversal_callback_t)(uint8_t * element, de_type_t type, de_size_t size, void *context);
static void de_traverse_sequence(uint8_t * element, de_traversal_callback_t handler, void *context){
    de_type_t type = de_get_element_type(element);
    if (type != DE_DES) return;
    int pos = de_get_header_size(element);
    int end_pos = de_get_len(element);
    while (pos < end_pos){
        de_type_t elemType = de_get_element_type(element + pos);
        de_size_t elemSize = de_get_size_type(element + pos);
        uint8_t done = (*handler)(element + pos, elemType, elemSize, context); 
        if (done) break;
        pos += de_get_len(element + pos);
    }
}

// MARK: AttributeList traversal
typedef int (*sdp_attribute_list_traversal_callback_t)(uint16_t attributeID, uint8_t * attributeValue, de_type_t type, de_size_t size, void *context);
static void sdp_attribute_list_traverse_sequence(uint8_t * element, sdp_attribute_list_traversal_callback_t handler, void *context){
    de_type_t type = de_get_element_type(element);
    if (type != DE_DES) return;
    int pos = de_get_header_size(element);
    int end_pos = de_get_len(element);
    while (pos < end_pos){
        de_type_t idType = de_get_element_type(element + pos);
        de_size_t idSize = de_get_size_type(element + pos);
        if (idType != DE_UINT || idSize != DE_SIZE_16) break; // wrong type
        uint16_t attribute_id = READ_NET_16(element, pos + 1);
        pos += 3;
        if (pos >= end_pos) break; // array out of bounds
        de_type_t valueType = de_get_element_type(element + pos);
        de_size_t valueSize = de_get_size_type(element + pos);
        uint8_t done = (*handler)(attribute_id, element + pos, valueType, valueSize, context); 
        if (done) break;
        pos += de_get_len(element + pos);
    }
}

// MARK: AttributeID in AttributeIDList 
// attribute ID in AttributeIDList
// context { result, attributeID }
struct sdp_context_attributeID_search {
    int result;
    uint16_t attributeID;
};
static int sdp_traversal_attributeID_search(uint8_t * element, de_type_t type, de_size_t size, void *my_context){
    struct sdp_context_attributeID_search * context = (struct sdp_context_attributeID_search *) my_context;
    if (type != DE_UINT) return 0;
    switch (size) {
        case DE_SIZE_16:
            if (READ_NET_16(element, 1) == context->attributeID) {
                context->result = 1;
                return 1;
            }
            break;
        case DE_SIZE_32:
            if (READ_NET_16(element, 1) <= context->attributeID
            &&  context->attributeID <= READ_NET_16(element, 3)) {
                context->result = 1;
                return 1;
            }
            break;
        default:
            break;
    }
    return 0;
}
int sdp_attribute_list_constains_id(uint8_t *attributeIDList, uint16_t attributeID){
    struct sdp_context_attributeID_search attributeID_search;
    attributeID_search.result = 0;
    attributeID_search.attributeID = attributeID;
    de_traverse_sequence(attributeIDList, sdp_traversal_attributeID_search, &attributeID_search);
    return attributeID_search.result;
}

// MARK: Append Attributes for AttributeIDList
// pre: buffer contains DES with 2 byte length field
struct sdp_context_append_attributes {
    uint8_t * buffer;
    uint16_t startOffset;     // offset of when to start copying
    uint16_t maxBytes;
    uint16_t usedBytes;
    uint8_t *attributeIDList;
};

static int sdp_traversal_append_attributes(uint16_t attributeID, uint8_t * attributeValue, de_type_t type, de_size_t size, void *my_context){
    struct sdp_context_append_attributes * context = (struct sdp_context_append_attributes *) my_context;
    if (sdp_attribute_list_constains_id(context->attributeIDList, attributeID)) {
        // DES_HEADER(3) + DES_DATA + (UINT16(3) + attribute)
        uint16_t data_size = READ_NET_16(context->buffer, 1);
        int attribute_len = de_get_len(attributeValue);
        if (3 + data_size + (3 + attribute_len) <= context->maxBytes) {
            // copy Attribute
            de_add_number(context->buffer, DE_UINT, DE_SIZE_16, attributeID);   
            data_size += 3; // 3 bytes
            memcpy(context->buffer + 3 + data_size, attributeValue, attribute_len);
            net_store_16(context->buffer,1,data_size+attribute_len);
        } else {
            // not enought space left -> continue with previous element
            return 1;
        }
    }
    return 0;
}

// maxBytes: maximal size of data element sequence
uint16_t sdp_append_attributes_in_attributeIDList(uint8_t *record, uint8_t *attributeIDList, uint16_t startOffset, uint16_t maxBytes, uint8_t *buffer){
    struct sdp_context_append_attributes context;
    context.buffer = buffer;
    context.maxBytes = maxBytes;
    context.usedBytes = 0;
    context.startOffset = startOffset;
    context.attributeIDList = attributeIDList;
    sdp_attribute_list_traverse_sequence(record, sdp_traversal_append_attributes, &context);
    return context.usedBytes;
}

// MARK: Filter attributes that match attribute list from startOffset and a max nr bytes
struct sdp_context_filter_attributes {
    uint8_t * buffer;
    uint16_t startOffset;     // offset of when to start copying
    uint16_t maxBytes;
    uint16_t usedBytes;
    uint8_t *attributeIDList;
    int      complete;
};

// copy data with given start offset and max bytes, returns OK if all data has been copied
static int spd_append_range(struct sdp_context_filter_attributes* context, uint16_t len, uint8_t *data){
    int ok = 1;
    uint16_t remainder_len = len - context->startOffset;
    if (context->maxBytes < remainder_len){
        remainder_len = context->maxBytes;
        ok = 0;
    }
    memcpy(context->buffer, &data[context->startOffset], remainder_len);
    context->usedBytes += remainder_len;
    context->buffer    += remainder_len;
    context->maxBytes  -= remainder_len;
    context->startOffset = 0;
    return ok;
}

static int sdp_traversal_filter_attributes(uint16_t attributeID, uint8_t * attributeValue, de_type_t type, de_size_t size, void *my_context){
    struct sdp_context_filter_attributes * context = (struct sdp_context_filter_attributes *) my_context;

    if (!sdp_attribute_list_constains_id(context->attributeIDList, attributeID)) return 0;

    // { Attribute ID (Descriptor, big endian 16-bit ID), AttributeValue (data)}

    // handle Attribute ID
    if (context->startOffset >= 3){
        context->startOffset -= 3;
    } else {
        uint8_t idBuffer[3];
        de_store_descriptor(idBuffer, DE_UINT,  DE_SIZE_16);
        net_store_16(idBuffer,1,attributeID);
        
        int ok = spd_append_range(context, 3, idBuffer);
        if (!ok) {
            context->complete = 0;
            return 1;
        }
    }
    
    // handle Attribute Value
    int attribute_len = de_get_len(attributeValue);
    if (context->startOffset >= attribute_len) {
        context->startOffset -= attribute_len;
        return 0;
    }
    
    int ok = spd_append_range(context, attribute_len, attributeValue);
    if (!ok) {
        context->complete = 0;
        return 1;
    }
    return 0;
}

int sdp_filter_attributes_in_attributeIDList(uint8_t *record, uint8_t *attributeIDList, uint16_t startOffset, uint16_t maxBytes, uint16_t *usedBytes, uint8_t *buffer){

    struct sdp_context_filter_attributes context;
    context.buffer = buffer;
    context.maxBytes = maxBytes;
    context.usedBytes = 0;
    context.startOffset = startOffset;
    context.attributeIDList = attributeIDList;
    context.complete = 1;

    sdp_attribute_list_traverse_sequence(record, sdp_traversal_filter_attributes, &context);

    *usedBytes = context.usedBytes;
    return context.complete;
}

// MARK: Get sum of attributes matching attribute list
struct sdp_context_get_filtered_size {
    uint8_t *attributeIDList;
    uint16_t size;
};

static int sdp_traversal_get_filtered_size(uint16_t attributeID, uint8_t * attributeValue, de_type_t type, de_size_t size, void *my_context){
    struct sdp_context_get_filtered_size * context = (struct sdp_context_get_filtered_size *) my_context;
    if (sdp_attribute_list_constains_id(context->attributeIDList, attributeID)) {
        context->size += 3 + de_get_len(attributeValue);
    }
    return 0;
}

int spd_get_filtered_size(uint8_t *record, uint8_t *attributeIDList){
    struct sdp_context_get_filtered_size context;
    context.size = 0;
    context.attributeIDList = attributeIDList;
    sdp_attribute_list_traverse_sequence(record, sdp_traversal_get_filtered_size, &context);
    return context.size;
}

// MARK: Get AttributeValue for AttributeID
// find attribute (ELEMENT) by ID
struct sdp_context_attribute_by_id {
    uint16_t  attributeID;
    uint8_t * attributeValue;
};
static int sdp_traversal_attribute_by_id(uint16_t attributeID, uint8_t * attributeValue, de_type_t attributeType, de_size_t size, void *my_context){
    struct sdp_context_attribute_by_id * context = (struct sdp_context_attribute_by_id *) my_context;
    if (attributeID == context->attributeID) {
        context->attributeValue = attributeValue;
        return 1;
    }
    return 0;
}

uint8_t * sdp_get_attribute_value_for_attribute_id(uint8_t * record, uint16_t attributeID){
    struct sdp_context_attribute_by_id context;
    context.attributeValue = NULL;
    context.attributeID = attributeID;
    sdp_attribute_list_traverse_sequence(record, sdp_traversal_attribute_by_id, &context);
    return context.attributeValue;
}

// MARK: Set AttributeValue for AttributeID
struct sdp_context_set_attribute_for_id {
    uint16_t  attributeID;
    uint32_t  attributeValue;
    uint8_t   attributeFound;
};
static int sdp_traversal_set_attribute_for_id(uint16_t attributeID, uint8_t * attributeValue, de_type_t attributeType, de_size_t size, void *my_context){
    struct sdp_context_set_attribute_for_id * context = (struct sdp_context_set_attribute_for_id *) my_context;
    if (attributeID == context->attributeID) {
        context->attributeFound = 1;
        switch (size){
            case DE_SIZE_8:
                if (attributeType != DE_NIL){
                    attributeValue[1] = context->attributeValue;
                }
                break;
            case DE_SIZE_16:
                net_store_16(attributeValue, 1, context->attributeValue);
                break;
            case DE_SIZE_32:
                net_store_32(attributeValue, 1, context->attributeValue);
                break;
                // Might want to support STRINGS to, copy upto original length
            default:
                break;
        }        
        return 1;
    }
    return 0;
}
uint8_t sdp_set_attribute_value_for_attribute_id(uint8_t * record, uint16_t attributeID, uint32_t value){
    struct sdp_context_set_attribute_for_id context;
    context.attributeID = attributeID;
    context.attributeValue = value;
    context.attributeFound = 0;
    sdp_attribute_list_traverse_sequence(record, sdp_traversal_set_attribute_for_id, &context);
    return context.attributeFound;
}

// MARK: ServiceRecord contains UUID
// service record contains UUID
// context { normalizedUUID }
struct sdp_context_contains_uuid128 {
    uint8_t * uuid128;
    int result;
};
int sdp_record_contains_UUID128(uint8_t *record, uint8_t *uuid128);
static int sdp_traversal_contains_UUID128(uint8_t * element, de_type_t type, de_size_t size, void *my_context){
    struct sdp_context_contains_uuid128 * context = (struct sdp_context_contains_uuid128 *) my_context;
    uint8_t normalizedUUID[16];
    if (type == DE_UUID){
        uint8_t uuidOK = de_get_normalized_uuid(normalizedUUID, element);
        context->result = uuidOK && memcmp(context->uuid128, normalizedUUID, 16) == 0;
    }
    if (type == DE_DES){
        context->result = sdp_record_contains_UUID128(element, context->uuid128);
    }
    return context->result;
}
int sdp_record_contains_UUID128(uint8_t *record, uint8_t *uuid128){
    struct sdp_context_contains_uuid128 context;
    context.uuid128 = uuid128;
    context.result = 0;
    de_traverse_sequence(record, sdp_traversal_contains_UUID128, &context);
    return context.result;
}
    
// MARK: ServiceRecord matches SearchServicePattern
// if UUID in searchServicePattern is not found in record => false
// context { result, record }
struct sdp_context_match_pattern {
    uint8_t * record;
    int result;
};
int sdp_traversal_match_pattern(uint8_t * element, de_type_t attributeType, de_size_t size, void *my_context){
    struct sdp_context_match_pattern * context = (struct sdp_context_match_pattern *) my_context;
    uint8_t normalizedUUID[16];
    uint8_t uuidOK = de_get_normalized_uuid(normalizedUUID, element);
    if (!uuidOK || !sdp_record_contains_UUID128(context->record, normalizedUUID)){
        context->result = 0;
        return 1;
    }
    return 0;
}
int sdp_record_matches_service_search_pattern(uint8_t *record, uint8_t *serviceSearchPattern){
    struct sdp_context_match_pattern context;
    context.record = record;
    context.result = 1;
    de_traverse_sequence(serviceSearchPattern, sdp_traversal_match_pattern, &context);
    return context.result;
}

// MARK: Dump DataElement
// context { indent }
static int de_traversal_dump_data(uint8_t * element, de_type_t de_type, de_size_t de_size, void *my_context){
    int indent = *(int*) my_context;
    int i;
    for (i=0; i<indent;i++) printf("    ");
    int pos     = de_get_header_size(element);
    int end_pos = de_get_len(element);
    printf("type %5s (%u), element len %2u ", type_names[de_type], de_type, end_pos);
    if (de_type == DE_DES) {
		printf("\n");
        indent++;
        de_traverse_sequence(element, de_traversal_dump_data, (void *)&indent);
    } else if (de_type == DE_UUID && de_size == DE_SIZE_128) {
        printf(", value: ");
        printUUID(element+1);
        printf("\n");
    } else if (de_type == DE_STRING) {
        int len = 0;
        switch (de_size){
            case DE_SIZE_VAR_8:
                len = element[1];
                break;
            case DE_SIZE_VAR_16:
                len = READ_NET_16(element, 1);
                break;
            default:
                break;
        }
        printf("len %u (0x%02x)\n", len, len);
        hexdump(&element[pos], len);
    } else {
        uint32_t value = 0;
        switch (de_size) {
            case DE_SIZE_8:
                if (de_type != DE_NIL){
                    value = element[pos];
                }
                break;
            case DE_SIZE_16:
				value = READ_NET_16(element,pos);
                break;
            case DE_SIZE_32:
				value = READ_NET_32(element,pos);
                break;
            default:
                break;
        }
        printf(", value: 0x%08" PRIx32 "\n", value);
    }
    return 0;
}

void de_dump_data_element(uint8_t * record){
    int indent = 0;
    // hack to get root DES, too.
    de_type_t type = de_get_element_type(record);
    de_size_t size = de_get_size_type(record);
    de_traversal_dump_data(record, type, size, (void*) &indent);
}

void sdp_create_spp_service(uint8_t *service, int service_id, const char *name){
	
	uint8_t* attribute;
	de_create_sequence(service);
    
    // 0x0000 "Service Record Handle"
	de_add_number(service, DE_UINT, DE_SIZE_16, SDP_ServiceRecordHandle);
	de_add_number(service, DE_UINT, DE_SIZE_32, 0x10001);
    
	// 0x0001 "Service Class ID List"
	de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_ServiceClassIDList);
	attribute = de_push_sequence(service);
	{
		de_add_number(attribute,  DE_UUID, DE_SIZE_16, 0x1101 );
	}
	de_pop_sequence(service, attribute);
	
	// 0x0004 "Protocol Descriptor List"
	de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_ProtocolDescriptorList);
	attribute = de_push_sequence(service);
	{
		uint8_t* l2cpProtocol = de_push_sequence(attribute);
		{
			de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, 0x0100);
		}
		de_pop_sequence(attribute, l2cpProtocol);
		
		uint8_t* rfcomm = de_push_sequence(attribute);
		{
			de_add_number(rfcomm,  DE_UUID, DE_SIZE_16, 0x0003);  // rfcomm_service
			de_add_number(rfcomm,  DE_UINT, DE_SIZE_8,  service_id);  // rfcomm channel
		}
		de_pop_sequence(attribute, rfcomm);
	}
	de_pop_sequence(service, attribute);
	
	// 0x0005 "Public Browse Group"
	de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_BrowseGroupList); // public browse group
	attribute = de_push_sequence(service);
	{
		de_add_number(attribute,  DE_UUID, DE_SIZE_16, 0x1002 );
	}
	de_pop_sequence(service, attribute);
	
	// 0x0006
	de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_LanguageBaseAttributeIDList);
	attribute = de_push_sequence(service);
	{
		de_add_number(attribute, DE_UINT, DE_SIZE_16, 0x656e);
		de_add_number(attribute, DE_UINT, DE_SIZE_16, 0x006a);
		de_add_number(attribute, DE_UINT, DE_SIZE_16, 0x0100);
	}
	de_pop_sequence(service, attribute);
	
	// 0x0009 "Bluetooth Profile Descriptor List"
	de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_BluetoothProfileDescriptorList);
	attribute = de_push_sequence(service);
	{
		uint8_t *sppProfile = de_push_sequence(attribute);
		{
			de_add_number(sppProfile,  DE_UUID, DE_SIZE_16, 0x1101);
			de_add_number(sppProfile,  DE_UINT, DE_SIZE_16, 0x0100);
		}
		de_pop_sequence(attribute, sppProfile);
	}
	de_pop_sequence(service, attribute);
	
	// 0x0100 "ServiceName"
	de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0100);
	de_add_data(service,  DE_STRING, strlen(name), (uint8_t *) name);
}
