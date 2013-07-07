/*
 * Copyright (C) 2010 by Matthias Ringwald
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
 *  sdp_util.h
 */

#pragma once

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif
	
typedef enum {
    DE_NIL = 0,
    DE_UINT,
    DE_INT,
    DE_UUID,
    DE_STRING,
    DE_BOOL,
    DE_DES,
    DE_DEA,
    DE_URL
} de_type_t;

typedef enum {
    DE_SIZE_8 = 0,
    DE_SIZE_16,
    DE_SIZE_32,
    DE_SIZE_64,
    DE_SIZE_128,
    DE_SIZE_VAR_8,
    DE_SIZE_VAR_16,
    DE_SIZE_VAR_32
} de_size_t;

// UNIVERSAL ATTRIBUTE DEFINITIONS
#define SDP_ServiceRecordHandle     0x0000
#define SDP_ServiceClassIDList      0x0001
#define SDP_ServiceRecordState      0x0002
#define SDP_ServiceID               0x0003
#define SDP_ProtocolDescriptorList  0x0004
#define SDP_BrowseGroupList		    0x0005
#define SDP_LanguageBaseAttributeIDList 0x0006
#define SDP_ServiceInfoTimeToLive	0x0007
#define SDP_ServiceAvailability		0x0008
#define SDP_BluetoothProfileDescriptorList 0x0009
#define SDP_DocumentationURL		0x000a
#define SDP_ClientExecutableURL     0x000b
#define SDP_IconURL                 0x000c
#define SDP_AdditionalProtocolDescriptorList 0x000d
#define SDP_SupportedFormatsList    0x0303

// SERVICE CLASSES
#define SDP_OBEXObjectPush    0x1105
#define SDP_OBEXFileTransfer  0x1106
#define SDP_PublicBrowseGroup 0x1002

// PROTOCOLS
#define SDP_SDPProtocol       0x0001
#define SDP_UDPProtocol       0x0002
#define SDP_RFCOMMProtocol    0x0003
#define SDP_OBEXProtocol      0x0008
#define SDP_L2CAPProtocol     0x0100

// OFFSETS FOR LOCALIZED ATTRIBUTES - SDP_LanguageBaseAttributeIDList
#define SDP_Offest_ServiceName      0x0000
#define SDP_Offest_ServiceDescription 0x0001
#define SDP_Offest_ProviderName     0x0002

// OBEX
#define SDP_vCard_2_1       0x01
#define SDP_vCard_3_0       0x02
#define SDP_vCal_1_0        0x03
#define SDP_iCal_2_0        0x04
#define SDP_vNote           0x05
#define SDP_vMessage        0x06
#define SDP_OBEXFileTypeAny 0xFF

// MARK: DateElement
void de_dump_data_element(uint8_t * record);
int de_get_len(uint8_t *header);
de_size_t de_get_size_type(uint8_t *header);
de_type_t de_get_element_type(uint8_t *header);
int de_get_header_size(uint8_t * header);
void de_create_sequence(uint8_t *header);
void de_store_descriptor_with_len(uint8_t * header, de_type_t type, de_size_t size, uint32_t len);
uint8_t * de_push_sequence(uint8_t *header);
void de_pop_sequence(uint8_t * parent, uint8_t * child);
void de_add_number(uint8_t *seq, de_type_t type, de_size_t size, uint32_t value);
void de_add_data( uint8_t *seq, de_type_t type, uint16_t size, uint8_t *data);

int de_get_data_size(uint8_t * header);
void de_add_uuid128(uint8_t * seq, uint8_t * uuid);

// MARK: SDP
uint16_t  sdp_append_attributes_in_attributeIDList(uint8_t *record, uint8_t *attributeIDList, uint16_t startOffset, uint16_t maxBytes, uint8_t *buffer);
uint8_t * sdp_get_attribute_value_for_attribute_id(uint8_t * record, uint16_t attributeID);
uint8_t   sdp_set_attribute_value_for_attribute_id(uint8_t * record, uint16_t attributeID, uint32_t value);
int       sdp_record_matches_service_search_pattern(uint8_t *record, uint8_t *serviceSearchPattern);
int       spd_get_filtered_size(uint8_t *record, uint8_t *attributeIDList);
int       sdp_filter_attributes_in_attributeIDList(uint8_t *record, uint8_t *attributeIDList, uint16_t startOffset, uint16_t maxBytes, uint16_t *usedBytes, uint8_t *buffer);  

void      sdp_create_spp_service(uint8_t *service, int service_id, const char *name);

#if defined __cplusplus
}
#endif
