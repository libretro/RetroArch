/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2018 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _LIBSMB2_DCERPC_H_
#define _LIBSMB2_DCERPC_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Data representation */
/* Integer */
#define DCERPC_DR_BIG_ENDIAN                    0x00
#define DCERPC_DR_LITTLE_ENDIAN                 0x10
/* Character */
#define DCERPC_DR_ASCII                         0x00
#define DCERPC_DR_EBCDIC                        0x01

struct dcerpc_context;
struct dcerpc_pdu;

/* Encoder/Decoder for a DCERPC object */
typedef int (*dcerpc_coder)(struct dcerpc_context *dce, struct dcerpc_pdu *pdu,
                            struct smb2_iovec *iov, int *offset,
                            void *ptr);

enum ptr_type {
        PTR_REF    = 0,
        PTR_UNIQUE = 1,
        PTR_FULL   = 2
};

typedef struct dcerpc_uuid {
        uint32_t v1;
        uint16_t v2;
        uint16_t v3;
        uint8_t v4[8];
} dcerpc_uuid_t;

typedef struct p_syntax_id {
        dcerpc_uuid_t uuid;
        uint16_t vers;
        uint16_t vers_minor;
} p_syntax_id_t;

struct ndr_transfer_syntax {
        dcerpc_uuid_t uuid;
        uint16_t vers;
};

struct ndr_context_handle {
        uint32_t context_handle_attributes;
        dcerpc_uuid_t context_handle_uuid;
};

struct dcerpc_utf16 {
        uint32_t max_count;       /* internal use only */
        uint32_t offset;          /* internal use only */
        uint32_t actual_count;    /* internal use only */

        struct smb2_utf16 *utf16; /* internal use only */
        
        const char *utf8;
};

struct dcerpc_carray {
        uint32_t max_count;
        uint8_t *data;
};
        
extern p_syntax_id_t lsa_interface;
extern p_syntax_id_t srvsvc_interface;
        
typedef void (*dcerpc_cb)(struct dcerpc_context *dce, int status,
                          void *command_data, void *cb_data);

struct dcerpc_context *dcerpc_create_context(struct smb2_context *smb2);
void dcerpc_free_data(struct dcerpc_context *dce, void *data);
const char *dcerpc_get_error(struct dcerpc_context *dce);
int dcerpc_connect_context_async(struct dcerpc_context *dce,
                                 const char *path, p_syntax_id_t *syntax,
                                 dcerpc_cb cb, void *cb_data);
void dcerpc_destroy_context(struct dcerpc_context *dce);

struct smb2_context *dcerpc_get_smb2_context(struct dcerpc_context *dce);
void *dcerpc_get_pdu_payload(struct dcerpc_pdu *pdu);

int dcerpc_open_async(struct dcerpc_context *dce, dcerpc_cb cb, void *cb_data);
int dcerpc_call_async(struct dcerpc_context *dce,
                      int opnum,
                      dcerpc_coder req_coder, void *req,
                      dcerpc_coder rep_coder, int decode_size,
                      dcerpc_cb cb, void *cb_data);

int dcerpc_do_coder(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                    struct smb2_iovec *iov,
                    int *offset, void *ptr,
                    dcerpc_coder coder);
int dcerpc_ptr_coder(struct dcerpc_context *dce, struct dcerpc_pdu *pdu,
                     struct smb2_iovec *iov, int *offset, void *ptr,
                     enum ptr_type type, dcerpc_coder coder);
int dcerpc_carray_coder(struct dcerpc_context *ctx,
                        struct dcerpc_pdu *pdu,
                        struct smb2_iovec *iov, int *offset,
                        void *ptr, int elem_size, dcerpc_coder coder);
int dcerpc_uint8_coder(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                    struct smb2_iovec *iov, int *offset, void *ptr);
int dcerpc_uint16_coder(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                     struct smb2_iovec *iov, int *offset, void *ptr);
int dcerpc_uint32_coder(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                     struct smb2_iovec *iov, int *offset, void *ptr);
int dcerpc_uint3264_coder(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                       struct smb2_iovec *iov, int *offset, void *ptr);
int dcerpc_conformance_coder(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                       struct smb2_iovec *iov, int *offset, void *ptr);
int dcerpc_utf16_coder(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                      struct smb2_iovec *iov, int *offset, void *ptr);
int dcerpc_utf16z_coder(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                        struct smb2_iovec *iov, int *offset, void *ptr);
int dcerpc_context_handle_coder(struct dcerpc_context *dce,
                                struct dcerpc_pdu *pdu,
                                struct smb2_iovec *iov, int *offset,
                                void *ptr);
int dcerpc_uuid_coder(struct dcerpc_context *dce,
                      struct dcerpc_pdu *pdu,
                      struct smb2_iovec *iov, int *offset,
                      dcerpc_uuid_t *uuid);
int dcerpc_uint8_coder(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                    struct smb2_iovec *iov, int *offset, void *ptr);
#define DCERPC_DECODE 0
#define DCERPC_ENCODE 1
struct dcerpc_pdu *dcerpc_allocate_pdu(struct dcerpc_context *dce,
                                       int direction, int payload_size);
void dcerpc_free_pdu(struct dcerpc_context *dce, struct dcerpc_pdu *pdu);

#ifdef __cplusplus
}
#endif

#endif /* !_LIBSMB2_DCERPC_H_ */
