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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef STDC_HEADERS
#include <stddef.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_UNISTD_H
#include <sys/unistd.h>
#endif

#include "portable-endian.h"
#include <errno.h>

#include "compat.h"

#include <stdio.h>
#include "smb2.h"
#include "libsmb2.h"
#include "libsmb2-dcerpc.h"
#include "libsmb2-raw.h"
#include "libsmb2-private.h"

#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)(void *)( (char *)__mptr - offsetof(type,member) );})

struct dcerpc_deferred_pointer {
        dcerpc_coder coder;
        void *ptr;
};

#define MAX_DEFERRED_PTR 1024

#define NDR32_UUID     0x8a885d04, 0x1ceb, 0x11c9, {0x9f, 0xe8, 0x08, 0x00, 0x2b, 0x10, 0x48, 0x60}
#define NDR64_UUID     0x71710533, 0xbeba, 0x4937, {0x83, 0x19, 0xb5, 0xdb, 0xef, 0x9c, 0xcc, 0x36}
/*
 * NDR64 is only supported for LITTLE_ENDIAN encodings:
 * https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-rpce/b1af93c7-f988-4a1a-ac74-063179942f32
 */

p_syntax_id_t ndr32_syntax = {
        {NDR32_UUID}, 2, 0
};

p_syntax_id_t ndr64_syntax = {
        {NDR64_UUID}, 1, 0
};

struct dcerpc_context {
        struct smb2_context *smb2;
        const char *path;
        p_syntax_id_t *syntax;
        smb2_file_id file_id;

        uint8_t tctx_id; /* 0:NDR32 1:NDR64 */
        uint8_t packed_drep[4];
        uint32_t call_id;
};

struct dcerpc_header {
        uint8_t  rpc_vers;
        uint8_t  rpc_vers_minor;
        uint8_t  PTYPE;
        uint8_t  pfc_flags;
        uint8_t  packed_drep[4];
        uint16_t frag_length;
        uint16_t auth_length;
        uint32_t call_id;
};

struct p_cont_elem_t {
          uint16_t          p_cont_id;
          uint8_t           n_transfer_syn;     /* number of items */
          uint8_t           reserved;           /* alignment pad, m.b.z. */
          p_syntax_id_t     *abstract_syntax;   /* transfer syntax list */
          p_syntax_id_t     **transfer_syntaxes;
};

struct dcerpc_bind_pdu {
        uint16_t max_xmit_frag;
        uint16_t max_recv_frag;
        uint32_t assoc_group_id;

        /* presentation context list */
        uint8_t          n_context_elem;      /* number of items */
        //u_int8          reserved;            /* alignment pad, m.b.z. */
        //u_short         reserved2;           /* alignment pad, m.b.z. */
        struct p_cont_elem_t *p_cont_elem;
        //p_cont_elem_t [size_is(n_cont_elem)] p_cont_elem[];
        //p_syntax_id_t *abstract_syntax;
};

#define ACK_RESULT_ACCEPTANCE         0
#define ACK_RESULT_USER_REJECTION     1
#define ACK_RESULT_PROVIDER_REJECTION 2

#define ACK_REASON_REASON_NOT_SPECIFIED                     0
#define ACK_REASON_ABSTRACT_SYNTAX_NOT_SUPPORTED            1
#define ACK_REASON_PROPOSED_TRANSFER_SYNTAXES_NOT_SUPPORTED 2
#define ACK_REASON_PROTOCOL_VERSION_NOT_SUPPORTED           4

struct dcerpc_bind_context_results {
        uint16_t ack_result;
        uint16_t ack_reason;
        dcerpc_uuid_t uuid;
        uint32_t syntax_version;
};

#define MAX_ACK_RESULTS 4
struct dcerpc_bind_ack_pdu {
        uint16_t max_xmit_frag;
        uint16_t max_recv_frag;
        uint32_t assoc_group_id;

        /* presentation context list */
        uint8_t num_results;
        struct dcerpc_bind_context_results results[MAX_ACK_RESULTS];
};

struct dcerpc_request_pdu {
        uint32_t alloc_hint;
        uint16_t context_id;
        uint16_t opnum;

      /* optional field for request, only present if the PFC_OBJECT_UUID
         * field is non-zero */
      /*  dcerpc_uuid_t  object;              24:16 object UID */

      /* stub data, 8-octet aligned 
                   .
                   .
                   .                 */
};

struct dcerpc_response_pdu {
        uint32_t alloc_hint;
        uint16_t context_id;
        uint8_t cancel_count;
        uint8_t reserved;
      /* stub data, 8-octet aligned 
                   .
                   .
                   .                 */
};

/* PDU Types */
#define PDU_TYPE_REQUEST             0
#define PDU_TYPE_PING                1
#define PDU_TYPE_RESPONSE            2
#define PDU_TYPE_FAULT               3
#define PDU_TYPE_WORKING             4
#define PDU_TYPE_NOCALL              5
#define PDU_TYPE_REJECT              6
#define PDU_TYPE_ACK                 7
#define PDU_TYPE_CL_CANCEL           8
#define PDU_TYPE_FACK                9
#define PDU_TYPE_CANCEL_ACK         10
#define PDU_TYPE_BIND               11
#define PDU_TYPE_BIND_ACK           12
#define PDU_TYPE_BIND_NAK           13
#define PDU_TYPE_ALTER_CONTEXT      14
#define PDU_TYPE_ALTER_CONTEXT_RESP 15
#define PDU_TYPE_SHUTDOWN           17
#define PDU_TYPE_CO_CANCEL          18
#define PDU_TYPE_ORPHANED           19


/* Flags */
#define PFC_FIRST_FRAG      0x01
#define PFC_LAST_FRAG       0x02
#define PFC_PENDING_CANCEL  0x04
#define PFC_RESERVED_1      0x08
#define PFC_CONC_MPX        0x10
#define PFC_DID_NOT_EXECUTE 0x20
#define PFC_MAYBE           0x40
#define PFC_OBJECT_UUID     0x80

#define NSE_BUF_SIZE 128*1024

struct dcerpc_cb_data {
        struct dcerpc_context *dce;
        dcerpc_cb cb;
        void *cb_data;
};

struct dcerpc_pdu {
        struct dcerpc_header hdr;

        union {
                struct dcerpc_bind_pdu bind;
                struct dcerpc_bind_ack_pdu bind_ack;
                struct dcerpc_request_pdu req;
                struct dcerpc_response_pdu rsp;
        };

        /* optional authentication verifier */
        /* following fields present iff auth_length != 0 */
#if 0
        auth_verifier_co_t   auth_verifier; 
#endif
        struct dcerpc_context *dce;
        dcerpc_cb cb;
        void *cb_data;

        dcerpc_coder coder;
        int decode_size;
        void *payload;

        int top_level;
        uint64_t ptr_id;

        int cur_ptr;
        int max_ptr;
        struct dcerpc_deferred_pointer ptrs[MAX_DEFERRED_PTR];
        int direction;

        /* All items are parsed twice, first to handle the conformance
         * fields and a second time to handle the data itself.
         * During the first run we also check what the maximum alignment
         * of the fields are.
         */
        int is_conformance_run;
        int max_alignment;
};

int
dcerpc_set_uint8(struct dcerpc_context *ctx, struct smb2_iovec *iov,
                 int *offset, uint8_t value)
{
        if (*offset + sizeof(uint8_t) > iov->len) {
                return -1;
        }
        *(uint8_t *)(iov->buf + *offset) = value;
        *offset += 1;
        return 0;
}

static int
dcerpc_set_uint16(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                  struct smb2_iovec *iov, int *offset, uint16_t value)
{
        *offset = (*offset + 1) & ~1;
        
        if (*offset + sizeof(uint16_t) > iov->len) {
                return -1;
        }
        if (!(pdu->hdr.packed_drep[0] & DCERPC_DR_LITTLE_ENDIAN)) {
                *(uint16_t *)(void *)(iov->buf + *offset) = htobe16(value);
        } else {
                *(uint16_t *)(void *)(iov->buf + *offset) = htole16(value);
        }
        *offset += 2;

        return 0;
}

static int
dcerpc_set_uint32(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                  struct smb2_iovec *iov, int *offset, uint32_t value)
{
        *offset = (*offset + 3) & ~3;
        
        if (*offset + sizeof(uint32_t) > iov->len) {
                return -1;
        }
        if (!(pdu->hdr.packed_drep[0] & DCERPC_DR_LITTLE_ENDIAN)) {
                *(uint32_t *)(void *)(iov->buf + *offset) = htobe32(value);
        } else {
                *(uint32_t *)(void *)(iov->buf + *offset) = htole32(value);
        }
        *offset += 4;
        return 0;
}

static int
dcerpc_set_uint64(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                  struct smb2_iovec *iov, int *offset, uint64_t value)
{
        *offset = (*offset + 7) & ~7;

        if (*offset + sizeof(uint64_t) > iov->len) {
                return -1;
        }
        if (!(pdu->hdr.packed_drep[0] & DCERPC_DR_LITTLE_ENDIAN)) {
                *(uint64_t *)(void *)(iov->buf + *offset) = htobe64(value);
        } else {
                *(uint64_t *)(void *)(iov->buf + *offset) = htole64(value);
        }
        *offset += 8;
        return 0;
}

static int
dcerpc_get_uint8(struct dcerpc_context *ctx, struct smb2_iovec *iov,
                  int *offset, uint8_t *value)
{
        if (*offset + sizeof(uint8_t) > iov->len) {
                return -1;
        }
        *value = iov->buf[*offset];
        *offset += 1;
        return 0;
}

static int
dcerpc_get_uint16(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                  struct smb2_iovec *iov, int *offset, uint16_t *value)
{
        uint16_t val;

        *offset = (*offset + 1) & ~1;
        
        if (*offset + sizeof(uint16_t) > iov->len) {
                return -1;
        }
        if (!(pdu->hdr.packed_drep[0] & DCERPC_DR_LITTLE_ENDIAN)) {
                val = be16toh(*(uint16_t *)(void *)(iov->buf + *offset));
        } else {
                val = le16toh(*(uint16_t *)(void *)(iov->buf + *offset));
        }
        *value = val;
        *offset += 2;
        return 0;
}

static int
dcerpc_get_uint32(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                  struct smb2_iovec *iov, int *offset, uint32_t *value)
{
        uint32_t val;
        
        *offset = (*offset + 3) & ~3;
        
        if (*offset + sizeof(uint32_t) > iov->len) {
                return -1;
        }
        
        if (!(pdu->hdr.packed_drep[0] & DCERPC_DR_LITTLE_ENDIAN)) {
                val = be32toh(*(uint32_t *)(void *)(iov->buf + *offset));
        } else {
                val = le32toh(*(uint32_t *)(void *)(iov->buf + *offset));
        }
        *value = val;
        *offset += 4;
        return 0;
}

static int
dcerpc_get_uint64(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                  struct smb2_iovec *iov, int *offset, uint64_t *value)
{
        uint64_t val;

        *offset = (*offset + 7) & ~7;

        if (*offset + sizeof(uint64_t) > iov->len) {
                return -1;
        }
        if (!(pdu->hdr.packed_drep[0] & DCERPC_DR_LITTLE_ENDIAN)) {
                val = be64toh(*(uint64_t *)(void *)(iov->buf + *offset));
        } else {
                val = le64toh(*(uint64_t *)(void *)(iov->buf + *offset));
        }
        *value = val;
        *offset += 8;
        return 0;
}

int
dcerpc_uint64_coder(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                    struct smb2_iovec *iov, int *offset, void *ptr)
{
        if (pdu->is_conformance_run) {
                if (pdu->max_alignment < 8) {
                        pdu->max_alignment = 8;
                }
                return 0;
        }

        if (pdu->direction == DCERPC_DECODE) {
                return dcerpc_get_uint64(ctx, pdu, iov, offset, ptr);
        } else {
                return dcerpc_set_uint64(ctx, pdu, iov, offset, *(uint32_t *)ptr);
        }
}

struct smb2_context *
dcerpc_get_smb2_context(struct dcerpc_context *dce)
{
        return dce->smb2;
}

void *
dcerpc_get_pdu_payload(struct dcerpc_pdu *pdu)
{
        return pdu->payload;
}

struct dcerpc_context *
dcerpc_create_context(struct smb2_context *smb2)
{
        struct dcerpc_context *ctx;

        ctx = calloc(1, sizeof(struct dcerpc_context));
        if (ctx == NULL) {
                smb2_set_error(smb2, "Failed to allocate dcercp context.");
                return NULL;
        }

        ctx->smb2 = smb2;
        ctx->packed_drep[0] |= DCERPC_DR_LITTLE_ENDIAN;
        return ctx;
}

int
dcerpc_connect_context_async(struct dcerpc_context *dce, const char *path,
                             p_syntax_id_t *syntax,
                             dcerpc_cb cb, void *cb_data)
{
        dce->call_id = 2;
        dce->path = strdup(path);
        if (dce->path == NULL) {
                smb2_set_error(dce->smb2, "Failed to allocate path for "
                               "dcercp context.");
                return -ENOMEM;
        }
        dce->syntax = syntax;
        dce->packed_drep[0] = DCERPC_DR_ASCII;
        if (!dce->smb2->endianness) {
                dce->packed_drep[0] |= DCERPC_DR_LITTLE_ENDIAN;
        }

        if (dcerpc_open_async(dce, cb, cb_data) != 0) {
                return -1;
        }

        return 0;
}

void
dcerpc_destroy_context(struct dcerpc_context *dce)
{
        if (dce == NULL) {
                return;
        }
        free(discard_const(dce->path));
        free(dce);
}

void
dcerpc_free_pdu(struct dcerpc_context *dce, struct dcerpc_pdu *pdu)
{
        if (pdu == NULL) {
                return;
        }

        if (pdu->payload) {
                smb2_free_data(dce->smb2, pdu->payload);
        }
        free(pdu);
}

struct dcerpc_pdu *
dcerpc_allocate_pdu(struct dcerpc_context *dce, int direction, int payload_size)
{
        struct dcerpc_pdu *pdu;

        pdu = calloc(1, sizeof(struct dcerpc_pdu));
        if (pdu == NULL) {
                smb2_set_error(dce->smb2, "Failed to allocate DCERPC PDU");
                return NULL;
        }

        pdu->dce = dce;
        pdu->hdr.call_id = dce->call_id++;
        pdu->direction = direction;
        pdu->top_level = 1;
        pdu->payload = smb2_alloc_init(dce->smb2, payload_size);
        if (pdu->payload == NULL) {
                smb2_set_error(dce->smb2, "Failed to allocate PDU Payload");
                dcerpc_free_pdu(dce, pdu);
                return NULL;
        }

        return pdu;
}

static void
dcerpc_add_deferred_pointer(struct dcerpc_context *ctx,
                            struct dcerpc_pdu *pdu,
                            dcerpc_coder coder, void *ptr)
{
        pdu->ptrs[pdu->max_ptr].coder = coder;
        pdu->ptrs[pdu->max_ptr].ptr = ptr;
        pdu->max_ptr++;
}

int
dcerpc_do_coder(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                struct smb2_iovec *iov,
                int *offset, void *ptr,
                dcerpc_coder coder)
{
        pdu->max_alignment = 1;
        pdu->is_conformance_run = 1;
        if (coder(ctx, pdu, iov, offset, ptr)) {
                return -1;
        }
        *offset = (*offset + (pdu->max_alignment - 1)) & ~(pdu->max_alignment - 1);
        pdu->is_conformance_run = 0;
        if (coder(ctx, pdu, iov, offset, ptr)) {
                return -1;
        }
        return 0;
}

static int
dcerpc_process_deferred_pointers(struct dcerpc_context *ctx,
                                 struct dcerpc_pdu *pdu,
                                 struct smb2_iovec *iov,
                                 int *offset)
{
        struct dcerpc_deferred_pointer *dp;
        int idx;

        while (pdu->cur_ptr != pdu->max_ptr) {
                idx = pdu->cur_ptr++;
                dp = &pdu->ptrs[idx];
                if (dcerpc_do_coder(ctx, pdu, iov, offset, dp->ptr, dp->coder)) {
                        return -1;
                }
        }
        return 0;
}

int
dcerpc_uint32_coder(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                    struct smb2_iovec *iov, int *offset, void *ptr)
{
        if (pdu->is_conformance_run) {
                if (pdu->max_alignment < 4) {
                        pdu->max_alignment = 4;
                }
                return 0;
        }
        if (pdu->direction == DCERPC_DECODE) {
                return dcerpc_get_uint32(ctx, pdu, iov, offset, ptr);
        } else {
                return dcerpc_set_uint32(ctx, pdu, iov, offset, *(uint32_t *)ptr);
        }
}

int
dcerpc_uint16_coder(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                    struct smb2_iovec *iov, int *offset, void *ptr)
{
        if (pdu->is_conformance_run) {
                if (pdu->max_alignment < 2) {
                        pdu->max_alignment = 2;
                }
                return 0;
        }
        
        if (pdu->direction == DCERPC_DECODE) {
                return dcerpc_get_uint16(ctx, pdu, iov, offset, ptr);
        } else {
                return dcerpc_set_uint16(ctx, pdu, iov, offset, *(uint16_t *)ptr);
        }
}

int
dcerpc_uint8_coder(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                   struct smb2_iovec *iov, int *offset, void *ptr)
{
        if (pdu->is_conformance_run) {
                if (pdu->max_alignment < 1) {
                        pdu->max_alignment = 1;
                }
                return 0;
        }
        if (pdu->direction == DCERPC_DECODE) {
                return dcerpc_get_uint8(ctx, iov, offset, ptr);
        } else {
                return dcerpc_set_uint8(ctx, iov, offset, *(uint8_t *)ptr);
        }

        return 0;
}

/* Encode words that vary in size depending on the transport syntax */
int
dcerpc_uint3264_coder(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                      struct smb2_iovec *iov, int *offset, void *ptr)
{
        uint32_t u32 = 0;
        uint64_t val = *(uint64_t *)ptr;

        if (pdu->is_conformance_run) {
                if (ctx->tctx_id) {
                        if (pdu->max_alignment < 8) {
                                pdu->max_alignment = 8;
                        }
                } else {
                        if (pdu->max_alignment < 4) {
                                pdu->max_alignment = 4;
                        }
                }
                return 0;
        }
        if (pdu->direction == DCERPC_DECODE) {
                if (ctx->tctx_id) {
                        if (dcerpc_get_uint64(ctx, pdu, iov, offset, ptr)) {
                                return -1;
                        }
                } else {
                        if (dcerpc_get_uint32(ctx, pdu, iov, offset, &u32)) {
                                return -1;
                        }
                        *(uint64_t *)ptr = u32;
                }
        } else {
                if (ctx->tctx_id) {
                        if (dcerpc_set_uint64(ctx, pdu, iov, offset, val)) {
                                return -1;
                        }
                } else {
                        if (dcerpc_set_uint32(ctx, pdu, iov, offset, (uint32_t)val)) {
                                return -1;
                        }
                }
        }
        return 0;
}

int
dcerpc_conformance_coder(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                         struct smb2_iovec *iov, int *offset, void *ptr)
{
        uint32_t u32 = 0;
        uint64_t val = *(uint64_t *)ptr;

        if (!pdu->is_conformance_run) {
                return 0;
        }

        if (pdu->direction == DCERPC_DECODE) {
                if (ctx->tctx_id) {
                        if (dcerpc_get_uint64(ctx, pdu, iov, offset, ptr)) {
                                return -1;
                        }
                } else {
                        if (dcerpc_get_uint32(ctx, pdu, iov, offset, &u32)) {
                                return -1;
                        }
                        *(uint64_t *)ptr = u32;
                }
        } else {
                if (ctx->tctx_id) {
                        if (dcerpc_set_uint64(ctx, pdu, iov, offset, val)) {
                                return -1;
                        }
                } else {
                        if (dcerpc_set_uint32(ctx, pdu, iov, offset, val)) {
                                return -1;
                        }
                }
        }
        return 0;
}

#define RPTR 0x5270747272747052
#define UPTR 0x5570747272747055
static int
dcerpc_encode_ptr(struct dcerpc_context *dce, struct dcerpc_pdu *pdu,
                  struct smb2_iovec *iov,
                  int *offset, void *ptr,
                  enum ptr_type type, dcerpc_coder coder)
{
        int top_level = pdu->top_level;
        uint64_t val;

        if (pdu->is_conformance_run) {
                if (dce->tctx_id) {
                        if (pdu->max_alignment < 8) {
                                pdu->max_alignment = 8;
                        }
                } else {
                        if (pdu->max_alignment < 4) {
                                pdu->max_alignment = 4;
                        }
                }
                return 0;
        }

        switch (type) {
        case PTR_REF:
                if (pdu->top_level) {
                        pdu->top_level = 0;
                        if (dcerpc_do_coder(dce, pdu, iov, offset, ptr, coder)) {
                                return -1;
                        }
                        pdu->top_level = top_level;
                        goto out;
                }

                val = RPTR;
                if (dcerpc_uint3264_coder(dce, pdu, iov, offset, &val)) {
                        return -1;
                }
                dcerpc_add_deferred_pointer(dce, pdu, (dcerpc_coder)coder, ptr);
                break;
        case PTR_FULL:
                if (ptr == NULL) {
                        val = 0;
                        if (dcerpc_uint3264_coder(dce, pdu, iov, offset, &val)) {
                                return -1;
                        }
                        goto out;
                }
                
                pdu->ptr_id++;
                val = pdu->ptr_id;
                if (dcerpc_uint3264_coder(dce, pdu, iov, offset, &val)) {
                        return -1;
                }
                if (pdu->top_level) {
                        pdu->top_level = 0;
                        if (dcerpc_do_coder(dce, pdu, iov, offset, ptr, coder)) {
                                return -1;
                        }
                        pdu->top_level = top_level;
                } else {
                        dcerpc_add_deferred_pointer(dce, pdu, (dcerpc_coder)coder, ptr);
                }
                break;
        case PTR_UNIQUE:
                if (ptr == NULL) {
                        val = 0;
                        if (dcerpc_uint3264_coder(dce, pdu, iov, offset, &val)) {
                                return -1;
                        }
                        goto out;
                }

                val = UPTR;
                if (dcerpc_uint3264_coder(dce, pdu, iov, offset, &val)) {
                        return -1;
                }
                if (pdu->top_level) {
                        pdu->top_level = 0;
                        if (dcerpc_do_coder(dce, pdu, iov, offset, ptr, coder)) {
                                return -1;
                        }
                        pdu->top_level = top_level;
                } else {
                        dcerpc_add_deferred_pointer(dce, pdu, (dcerpc_coder)coder, ptr);
                }
                break;
        }

 out:
        if (pdu->top_level) {
                pdu->top_level = 0;
                if (dcerpc_process_deferred_pointers(dce, pdu, iov, offset)) {
                        return -1;
                }
                pdu->top_level = top_level;
        }
        return 0;
}

/* TODO conformance split
 * during the conformance run we need to do the alignment in all the
  coders, even for the coders that do  not have any conformance data.
*/
static int
dcerpc_decode_ptr(struct dcerpc_context *dce, struct dcerpc_pdu *pdu,
                  struct smb2_iovec *iov, int *offset, void *ptr,
                  enum ptr_type type, dcerpc_coder coder)
{
        int top_level = pdu->top_level;
        uint64_t p;

        if (pdu->is_conformance_run) {
                if (!(type==PTR_REF && pdu->top_level)) {
                        if (dce->tctx_id) {
                                if (pdu->max_alignment < 8) {
                                        pdu->max_alignment = 8;
                                }
                        } else {
                                if (pdu->max_alignment < 4) {
                                        pdu->max_alignment = 4;
                                }
                        }
                }
                return 0;
        }
        
        switch (type) {
        case PTR_REF:
                if (pdu->top_level) {
                        pdu->top_level = 0;
                        if (dcerpc_do_coder(dce, pdu, iov, offset, ptr, coder)) {
                                return -1;
                        }
                        pdu->top_level = top_level;
                        goto out;
                }

                if (dcerpc_uint3264_coder(dce, pdu, iov, offset, &p)) {
                        return -1;
                }
                dcerpc_add_deferred_pointer(dce, pdu, (dcerpc_coder)coder, ptr);
                break;
        case PTR_UNIQUE:
                if (dcerpc_uint3264_coder(dce, pdu, iov, offset, &p)) {
                        return -1;
                }
                if (p == 0 || ptr == NULL) {
                        return 0;
                }
                
                if (pdu->top_level) {
                        pdu->top_level = 0;
                        if (dcerpc_do_coder(dce, pdu, iov, offset, ptr, coder)) {
                                return -1;
                        }
                        pdu->top_level = top_level;
                } else {
                        dcerpc_add_deferred_pointer(dce, pdu, (dcerpc_coder)coder, ptr);
                }
                break;
        case PTR_FULL:
                /* not implemented yet */
                break;
        }

 out:
        if (pdu->top_level) {
                pdu->top_level = 0;
                if (dcerpc_process_deferred_pointers(dce, pdu, iov, offset)) {
                        return -1;
                }
                pdu->top_level = top_level;
        }
        return 0;
}

int
dcerpc_carray_coder(struct dcerpc_context *ctx,
                    struct dcerpc_pdu *pdu,
                    struct smb2_iovec *iov, int *offset,
                    void *ptr, int elem_size, dcerpc_coder coder)
{
        struct dcerpc_carray *carray = ptr;
        int i;
        uint64_t p;

        /* Conformance */
        p = carray->max_count;
        if (dcerpc_conformance_coder(ctx, pdu, iov, offset, &p)) {
                return -1;
        }
        carray->max_count = p;

        if (dcerpc_pdu_direction(pdu) == DCERPC_DECODE) {
                if (carray->max_count && carray->data == NULL) {
                        carray->data = smb2_alloc_data(
                                dcerpc_get_smb2_context(ctx),
                                dcerpc_get_pdu_payload(pdu),
                                carray->max_count * elem_size);
                        if (carray->data == NULL) {
                                return -1;
                        }
                }
        }
        
        /* Data */
        for (i = 0; i < p; i++) {
                if (coder(ctx, pdu, iov, offset, &carray->data[i * elem_size])) {
                        return -1;
                }
        }

        return 0;
}

int
dcerpc_ptr_coder(struct dcerpc_context *dce, struct dcerpc_pdu *pdu,
                 struct smb2_iovec *iov, int *offset, void *ptr,
                 enum ptr_type type, dcerpc_coder coder)
{
        if (pdu->direction == DCERPC_DECODE) {
                return dcerpc_decode_ptr(dce, pdu, iov, offset, ptr,
                                         type, coder);
        } else {
                return dcerpc_encode_ptr(dce, pdu, iov, offset, ptr,
                                         type, coder);
        }
}

static int
dcerpc_encode_utf16(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                    struct smb2_iovec *iov, int *offset,
                    void *ptr, int nult)
{
        struct dcerpc_utf16 *s = ptr;
        int i;
        uint64_t val;
        uint16_t zero = 0;

        /* Conformance part */
        if (pdu->is_conformance_run) {
                if (s->utf8 == NULL) {
                        s->utf8 = "";
                }
                s->utf16 = smb2_utf8_to_utf16(s->utf8);
                if (s->utf16 == NULL) {
                        return -1;
                }

                if (nult) {
                        val = s->utf16->len + 1;
                } else {
                        val = s->utf16->len;
                }
                s->actual_count = (uint32_t)val;
                if (!nult) {
                        if (val & 0x01) val++;
                }
                s->max_count = (uint32_t)val;
                s->offset    = 0;

                val = s->max_count;
                if (dcerpc_conformance_coder(ctx, pdu, iov, offset, &val)) {
                        return -1;
                }
                val = s->offset;
                if (dcerpc_conformance_coder(ctx, pdu, iov, offset, &val)) {
                        return -1;
                }
                val = s->actual_count;
                if (dcerpc_conformance_coder(ctx, pdu, iov, offset, &val)) {
                        return -1;
                }
                if (pdu->max_alignment < 2) {
                        pdu->max_alignment = 2;
                }
                return 0;
        }

        /* Data part */
        for (i = 0; i < s->utf16->len; i++) {
                if (dcerpc_uint16_coder(ctx, pdu, iov, offset, &s->utf16->val[i])) {
                        return -1;
                }
        }
        if (nult) {
                if (dcerpc_uint16_coder(ctx, pdu, iov, offset, &zero)) {
                        return -1;
                }
        }
        free(s->utf16);
        return 0;
}

static int
dcerpc_decode_utf16(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                   struct smb2_iovec *iov, int *offset,
                   void *ptr, int nult)
{
        struct dcerpc_utf16 *s = ptr;
        uint64_t val; /* Any fundament of this? */
        char *str;
        const char *tmp;

        /* Conformance part */
        if (pdu->is_conformance_run) {
                if (dcerpc_conformance_coder(ctx, pdu, iov, offset, &val)) {
                        return -1;
                }
                s->max_count = (uint32_t)val;
                if (dcerpc_conformance_coder(ctx, pdu, iov, offset, &val)) {
                        return -1;
                }
                s->offset = (uint32_t)val;
                if (dcerpc_conformance_coder(ctx, pdu, iov, offset, &val)) {
                        return -1;
                }
                s->actual_count = (uint32_t)val;
                if (pdu->max_alignment < 2) {
                        pdu->max_alignment = 2;
                }
                return 0;
        }
        
        /* Data part */
        if (*offset + s->actual_count * 2 > iov->len) {
                return -1;
        }
        if (!(pdu->hdr.packed_drep[0] & DCERPC_DR_LITTLE_ENDIAN)) {
                int i, o;
                uint16_t v;
                for (i = 0; i < s->actual_count; i++) {
                        o = *offset + i *2;
                        if (dcerpc_get_uint16(ctx, pdu, iov, &o, &v)) {
                                return -1;
                        }
                        *(uint16_t *)(void *)&iov->buf[*offset + i * 2] = v;
                }
        }
        tmp = smb2_utf16_to_utf8((uint16_t *)(void *)(&iov->buf[*offset]), (size_t)s->actual_count);
        *offset += (int)s->actual_count * 2;

        str = smb2_alloc_data(ctx->smb2, pdu->payload, strlen(tmp) + 1);
        if (str == NULL) {
                free(discard_const(tmp));
                return -1;
        }
        strcat(str, tmp);
        free(discard_const(tmp));

        s->utf8 = str;

        return 0;
}

/* Handle \0 terminated utf16 strings */
/* ptr is char ** */
int
dcerpc_utf16z_coder(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                    struct smb2_iovec *iov, int *offset,
                    void *ptr)
{
        if (pdu->direction == DCERPC_DECODE) {
                return dcerpc_decode_utf16(ctx, pdu, iov, offset, ptr, 1);
        } else {
                return dcerpc_encode_utf16(ctx, pdu, iov, offset, ptr, 1);
        }
}

/* Handle utf16 strings that are NOT \0 terminated */
int
dcerpc_utf16_coder(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                   struct smb2_iovec *iov, int *offset,
                   void *ptr)
{
        if (pdu->direction == DCERPC_DECODE) {
                return dcerpc_decode_utf16(ctx, pdu, iov, offset, ptr, 0);
        } else {
                return dcerpc_encode_utf16(ctx, pdu, iov, offset, ptr, 0);
        }
}

int
dcerpc_header_coder(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                    struct smb2_iovec *iov, int *offset,
                    struct dcerpc_header *hdr)
{
        /* Major Version */
        if (dcerpc_uint8_coder(ctx, pdu, iov, offset, &hdr->rpc_vers)) {
                return -1;
        }
        /* Minor Version */
        if (dcerpc_uint8_coder(ctx, pdu, iov, offset, &hdr->rpc_vers_minor)) {
                return -1;
        }
        /* Packet Type */
        if (dcerpc_uint8_coder(ctx, pdu, iov, offset, &hdr->PTYPE)) {
                return -1;
        }
        /* Flags */
        if (dcerpc_uint8_coder(ctx, pdu, iov, offset, &hdr->pfc_flags)) {
                return -1;
        }

        /* Data Representation */
        if (dcerpc_uint8_coder(ctx, pdu, iov, offset, &hdr->packed_drep[0])) {
                return -1;
        }
        if (dcerpc_uint8_coder(ctx, pdu, iov, offset, &hdr->packed_drep[1])) {
                return -1;
        }
        if (dcerpc_uint8_coder(ctx, pdu, iov, offset, &hdr->packed_drep[2])) {
                return -1;
        }
        if (dcerpc_uint8_coder(ctx, pdu, iov, offset, &hdr->packed_drep[3])) {
                return -1;
        }

        /* Fragment len */
        if (dcerpc_uint16_coder(ctx, pdu, iov, offset, &hdr->frag_length)) {
                return -1;
        }

        /* Auth len */
        if (dcerpc_uint16_coder(ctx, pdu, iov, offset, &hdr->auth_length)) {
                return -1;
        }

        /* Call id */
        if (dcerpc_uint32_coder(ctx, pdu, iov, offset, &hdr->call_id)) {
                return -1;
        }

        return 0;
}

int
dcerpc_uuid_coder(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                  struct smb2_iovec *iov, int *offset,
                  dcerpc_uuid_t *uuid)
{
        int i;
        
        if (dcerpc_uint32_coder(ctx, pdu, iov, offset, &uuid->v1)) {
                return -1;
        }
        if (dcerpc_uint16_coder(ctx, pdu, iov, offset, &uuid->v2)) {
                return -1;
        }
        if (dcerpc_uint16_coder(ctx, pdu, iov, offset, &uuid->v3)) {
                return -1;
        }
        for (i = 0; i < 8; i++) {
                if (dcerpc_uint8_coder(ctx, pdu, iov, offset, &uuid->v4[i])) {
                        return -1;
                }
        }

        return 0;
}        

/**********************
 * typedef struct ndr_context_handle {
 *    unsigned32 context_handle_attributes;
 *    dcerpc_uuid_t context_handle_uuid;
 * } ndr_context_handle;
 **********************/
int
dcerpc_context_handle_coder(struct dcerpc_context *dce,
                            struct dcerpc_pdu *pdu,
                            struct smb2_iovec *iov, int *offset,
                            void *ptr)
{
        struct ndr_context_handle *handle = ptr;

        if (dcerpc_uint32_coder(dce, pdu, iov, offset, &handle->context_handle_attributes)) {
                return -1;
        }
        if (dcerpc_uuid_coder(dce, pdu, iov, offset,
                              &handle->context_handle_uuid)) {
                return -1;
        }

        return 0;
}

static int
dcerpc_bind_coder(struct dcerpc_context *ctx,
                  struct dcerpc_pdu *pdu,
                  struct dcerpc_bind_pdu *bind,
                  struct smb2_iovec *iov, int *offset)
{
        int oo, i, j;
        uint16_t v;

        /* Max Xmit Frag */
        if (dcerpc_uint16_coder(ctx, pdu, iov, offset, &bind->max_xmit_frag)) {
                return -1;
        }

        /* Max Recv Frag */
        if (dcerpc_uint16_coder(ctx, pdu, iov, offset, &bind->max_recv_frag)) {
                return -1;
        }

        /* Association Group */
        if (dcerpc_uint32_coder(ctx, pdu, iov, offset, &bind->assoc_group_id)) {
                return -1;
        }

        /* Number Of Context Items */
        if (dcerpc_uint8_coder(ctx, pdu, iov, offset, &bind->n_context_elem)) {
                return -1;
        }
        *offset += 3;

        //qqq TODO allocate p_cont_elem on decode
        for (i = 0; i < bind->n_context_elem; i++) {
                if (dcerpc_uint16_coder(ctx, pdu, iov, offset, &pdu->bind.p_cont_elem[i].p_cont_id)) {
                        return -1;
                }
                if (dcerpc_uint8_coder(ctx, pdu, iov, offset, &pdu->bind.p_cont_elem[i].n_transfer_syn)) {
                        return -1;
                }
                *offset += 1;
                /* Abstract Syntax */
                //qqq TODO allocate abstract_syntax on decode
                if (dcerpc_uuid_coder(ctx, pdu, iov, offset, &pdu->bind.p_cont_elem[i].abstract_syntax->uuid)) {
                        return -1;
                }
                if (dcerpc_uint16_coder(ctx, pdu, iov, offset, &pdu->bind.p_cont_elem[i].abstract_syntax->vers)) {
                        return -1;
                }
                if (dcerpc_uint16_coder(ctx, pdu, iov, offset, &pdu->bind.p_cont_elem[i].abstract_syntax->vers_minor)) {
                        return -1;
                }
                //qqq TODO allocate transfer_syntaxes on decode
                for (j = 0; j < pdu->bind.p_cont_elem[i].n_transfer_syn; j++) {
                        if (dcerpc_uuid_coder(ctx, pdu, iov, offset, &pdu->bind.p_cont_elem[i].transfer_syntaxes[j]->uuid)) {
                                return -1;
                        }
                        if (dcerpc_uint16_coder(ctx, pdu, iov, offset, &pdu->bind.p_cont_elem[i].transfer_syntaxes[j]->vers)) {
                                return -1;
                        }
                        if (dcerpc_uint16_coder(ctx, pdu, iov, offset, &pdu->bind.p_cont_elem[i].transfer_syntaxes[j]->vers_minor)) {
                                return -1;
                        }
                }

        }
        /* Fixup fragment length */
        oo = 8;
        v = *offset;
        if (dcerpc_uint16_coder(ctx, pdu, iov, &oo, &v)) {
                return -1;
        }
        
        return 0;
}

static int
dcerpc_request_coder(struct dcerpc_context *ctx,
                     struct dcerpc_pdu *pdu,
                     struct dcerpc_request_pdu *req,
                     struct smb2_iovec *iov, int *offset)
{
        /* Alloc Hint */
        if (dcerpc_uint32_coder(ctx, pdu, iov, offset, &req->alloc_hint)) {
                return -1;
        }

        /* Context ID */
        if (dcerpc_uint16_coder(ctx, pdu, iov, offset, &req->context_id)) {
                return -1;
        }
        
        /* Opnum */
        if (dcerpc_uint16_coder(ctx, pdu, iov, offset, &req->opnum)) {
                return -1;
        }

        return 0;
}

static int
dcerpc_bind_ack_coder(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                      struct dcerpc_bind_ack_pdu *bind_ack,
                      struct smb2_iovec *iov, int *offset)
{
        int i;
        uint16_t sec_addr_len;

        /* Max Xmit Frag */
        if (dcerpc_uint16_coder(ctx, pdu, iov, offset, &bind_ack->max_xmit_frag)) {
                return -1;
        }

        /* Max Recv Frag */
        if (dcerpc_uint16_coder(ctx, pdu, iov, offset, &bind_ack->max_recv_frag)) {
                return -1;
        }

        /* Association Group */
        if (dcerpc_uint32_coder(ctx, pdu, iov, offset, &bind_ack->assoc_group_id)) {
                return -1;
        }

        /* Secondary Address Length */
        if (dcerpc_uint16_coder(ctx, pdu, iov, offset, &sec_addr_len)) {
                return -1;
        }

        /* Skip the secondary address and realign to 32bit */
        /* TODO: we need to handle the encode case.
        *        it is something like "\\PIPE\\srvsvc"
        */
        *offset += sec_addr_len;
        *offset = (*offset + 3) & ~3;

        /* Number Of Results */
        if (dcerpc_uint8_coder(ctx, pdu, iov, offset, &bind_ack->num_results)) {
                return -1;
        }
        *offset += 3;

        for (i = 0; i < bind_ack->num_results; i++) {
                if (dcerpc_uint16_coder(ctx, pdu, iov, offset, &bind_ack->results[i].ack_result)) {
                        return -1;
                }

                if (dcerpc_uint16_coder(ctx, pdu, iov, offset, &bind_ack->results[i].ack_reason)) {
                        return -1;
                }

                if (dcerpc_uuid_coder(ctx, pdu, iov, offset,
                                      &bind_ack->results[i].uuid)) {
                        return -1;
                }

                if (dcerpc_uint32_coder(ctx, pdu, iov, offset, &bind_ack->results[i].syntax_version)) {
                        return -1;
                }
        }

        return 0;
}

static int
dcerpc_response_coder(struct dcerpc_context *ctx,
                      struct dcerpc_response_pdu *rsp,
                      struct smb2_iovec *iov, int *offset)
{
#ifndef _MSC_VER
        struct dcerpc_pdu *pdu = container_of(rsp, struct dcerpc_pdu, rsp);
#else
        const char* __mptr = (const char*)rsp;
        struct dcerpc_pdu *pdu = (struct dcerpc_pdu*)((char *)__mptr - offsetof(struct dcerpc_pdu, rsp));
#endif /* !_MSC_VER */
    
        if (*offset < 0) {
                return -1;
        }

        /* Alloc Hint */
        if (dcerpc_uint32_coder(ctx, pdu, iov, offset, &rsp->alloc_hint)) {
                return -1;
        }

        if (rsp->alloc_hint > 16*1024*1024) {
                smb2_set_error(ctx->smb2, "DCERPC RESPONSE alloc_hint out "
                               "of range.");
                return -1;
        }

        /* Context Id */
        if (dcerpc_uint16_coder(ctx, pdu, iov, offset, &rsp->context_id)) {
                return -1;
        }
        
        /* Cancel Count */
        if (dcerpc_uint8_coder(ctx, pdu, iov, offset, &rsp->cancel_count)) {
                return -1;
        }
        *offset += 1;


        /* decode the blob */
        pdu->top_level = 1;
        if (pdu->coder(ctx, pdu, iov, offset, pdu->payload) < 0) {
                return -1;
        }

        *offset += rsp->alloc_hint;

        return 0;
}

static int
dcerpc_pdu_coder(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                 struct smb2_iovec *iov, int *offset)
{
        if (dcerpc_header_coder(ctx, pdu, iov, offset, &pdu->hdr)) {
                return -1;
        }

        switch (pdu->hdr.PTYPE) {
        case PDU_TYPE_BIND:
                if (dcerpc_bind_coder(ctx, pdu, &pdu->bind, iov, offset)) {
                        return -1;
                }
                break;
        case PDU_TYPE_BIND_ACK:
                if (dcerpc_bind_ack_coder(ctx, pdu, &pdu->bind_ack, iov, offset)) {
                        return -1;
                }
                break;
        case PDU_TYPE_REQUEST:
                if (dcerpc_request_coder(ctx, pdu, &pdu->req, iov, offset)) {
                        return -1;
                }
                break;
        case PDU_TYPE_RESPONSE:
                if (dcerpc_response_coder(ctx, &pdu->rsp, iov, offset)) {
                        return -1;
                }
                break;
        default:
                smb2_set_error(ctx->smb2, "DCERPC No decoder for PDU type %d",
                               pdu->hdr.PTYPE);
                return -1;
        }

        return 0;
}

static void
dce_unfragment_ioctl(struct dcerpc_context *dce,  struct dcerpc_pdu *pdu,
                     struct smb2_iovec *iov)
{
        int offset = 0;
        int unfragment_len;
        struct dcerpc_header hdr, next_hdr;
        struct smb2_iovec tmpiov _U_;
        int o;

        o = 0;
        if (dcerpc_header_coder(dce, pdu, iov, &o, &hdr)) {
                return;
        }
        if (hdr.rpc_vers != 5 || hdr.rpc_vers_minor != 0 ||
            hdr.PTYPE != PDU_TYPE_RESPONSE) {
                return;
        }

        if (hdr.pfc_flags & PFC_LAST_FRAG) {
                return;
        }

        offset += hdr.frag_length;
        unfragment_len = hdr.frag_length;
        do {
                /* We must have at least a DCERPC header plus a
                 * RESPONSE header
                 */
                if (iov->len - offset < 24) {
                        return;
                }

                tmpiov.buf = iov->buf + offset;
                tmpiov.len = iov->len - offset;
                o = 0;
                if (dcerpc_header_coder(dce, pdu, &tmpiov, &o, &next_hdr)) {
                        return;
                }

                memmove(iov->buf + unfragment_len, iov->buf + offset + 24,
                        next_hdr.frag_length - 24);
                unfragment_len += next_hdr.frag_length - 24;
                offset += next_hdr.frag_length;

                hdr.frag_length += next_hdr.frag_length;
                if (next_hdr.pfc_flags & PFC_LAST_FRAG) {
                        hdr.pfc_flags |= PFC_LAST_FRAG;
                }
                o = 0;
                if (dcerpc_header_coder(dce, pdu, iov, &o, &hdr)) {
                        return;
                }
        } while (!(next_hdr.pfc_flags & PFC_LAST_FRAG));
        iov->len = unfragment_len;
}

static void
dcerpc_send_pdu_cb_and_free(struct dcerpc_context *dce, struct dcerpc_pdu *pdu,
                            int status, void *command_data)
{
        dcerpc_cb pdu_cb = pdu->cb;
        void *pdu_cb_data = pdu->cb_data;

        dcerpc_free_pdu(dce, pdu);
        pdu_cb(dce, status, command_data, pdu_cb_data);
}

static void
dcerpc_call_cb(struct smb2_context *smb2, int status,
               void *command_data, void *private_data)
{
        struct dcerpc_pdu *pdu = private_data;
        struct dcerpc_context *dce = pdu->dce;
        struct smb2_iovec iov _U_;
        struct smb2_ioctl_reply *rep = command_data;
        void *payload;
        int offset = 0;

        pdu->direction = DCERPC_DECODE;

        if (status != SMB2_STATUS_SUCCESS) {
                dcerpc_send_pdu_cb_and_free(dce, pdu, -nterror_to_errno(status), NULL);
                return;
        }

        smb2_free_data(dce->smb2, pdu->payload);
        pdu->payload = NULL;

        pdu->payload = smb2_alloc_init(dce->smb2, pdu->decode_size);
        if (pdu->payload == NULL) {
                dcerpc_send_pdu_cb_and_free(dce, pdu, -ENOMEM, NULL);
                return;
        }

        iov.buf = rep->output;
        iov.len = rep->output_count;
        iov.free = NULL;

        dce_unfragment_ioctl(dce, pdu, &iov);

        if (dcerpc_pdu_coder(dce, pdu, &iov, &offset)) {
                smb2_free_data(dce->smb2, rep->output);
                dcerpc_send_pdu_cb_and_free(dce, pdu, -EINVAL, NULL);
                return;
        }
        smb2_free_data(dce->smb2, rep->output);

        if (pdu->hdr.PTYPE != PDU_TYPE_RESPONSE) {
                smb2_set_error(dce->smb2, "DCERPC response was not a RESPONSE");
                dcerpc_send_pdu_cb_and_free(dce, pdu, -EINVAL, NULL);
                return;
        }

        payload = pdu->payload;
        pdu->payload = NULL;
        dcerpc_send_pdu_cb_and_free(dce, pdu, 0, payload);
}

int
dcerpc_call_async(struct dcerpc_context *dce,
                  int opnum,
                  dcerpc_coder req_coder, void *req,
                  dcerpc_coder rep_coder, int decode_size,
                  dcerpc_cb cb, void *cb_data)
{
        struct dcerpc_pdu *pdu;
        struct smb2_pdu *smb2_pdu;
        struct smb2_ioctl_request smb2_req;
        struct smb2_iovec iov;
        int offset = 0, o;
        uint32_t v;

        pdu = dcerpc_allocate_pdu(dce, DCERPC_ENCODE, NSE_BUF_SIZE);
        if (pdu == NULL) {
                return -ENOMEM;
        }

        pdu->hdr.rpc_vers = 5;
        pdu->hdr.rpc_vers_minor = 0;
        pdu->hdr.PTYPE = PDU_TYPE_REQUEST;
        pdu->hdr.pfc_flags = PFC_FIRST_FRAG | PFC_LAST_FRAG;
        pdu->hdr.packed_drep[0] = dce->packed_drep[0];
        pdu->hdr.frag_length = 0;
        pdu->hdr.auth_length = 0;
        pdu->req.alloc_hint = 0;
        pdu->req.context_id = dce->tctx_id;
        pdu->req.opnum = opnum;

        pdu->coder = rep_coder;
        pdu->decode_size = decode_size;
        pdu->cb = cb;
        pdu->cb_data = cb_data;

        iov.buf = pdu->payload;
        iov.len = NSE_BUF_SIZE;
        iov.free = NULL;
        if (dcerpc_pdu_coder(dce, pdu, &iov, &offset)) {
                dcerpc_free_pdu(dce, pdu);
                return -ENOMEM;
        }

        /* encode the blob */
        pdu->top_level = 1;
        if (req_coder(dce, pdu, &iov, &offset, req)) {
                return -1;
        }

        iov.len = offset;

        /* Fixup frag_length and alloc_hint */
        o = 8;
        if (dcerpc_set_uint16(dce, pdu, &iov,  &o, offset)) {
                return -1;
        }
        o = 16;
        v = offset - 24;
        if (dcerpc_uint32_coder(dce, pdu, &iov, &o, &v)) {
                return -1;
        }

        memset(&smb2_req, 0, sizeof(struct smb2_ioctl_request));
        smb2_req.ctl_code = SMB2_FSCTL_PIPE_TRANSCEIVE;
        memcpy(smb2_req.file_id, dce->file_id, SMB2_FD_SIZE);
        smb2_req.input_count = (uint32_t)iov.len;
        smb2_req.input = iov.buf;
        smb2_req.flags = SMB2_0_IOCTL_IS_FSCTL;

        smb2_pdu = smb2_cmd_ioctl_async(dce->smb2, &smb2_req, dcerpc_call_cb, pdu);
        if (smb2_pdu == NULL) {
                dcerpc_free_pdu(dce, pdu);
                return -ENOMEM;
        }
        smb2_queue_pdu(dce->smb2, smb2_pdu);
 
        return 0;
}

static void
dcerpc_bind_cb(struct dcerpc_context *dce, int status,
               void *command_data, void *cb_data)
{
        struct dcerpc_cb_data *data = cb_data;

        if (status != SMB2_STATUS_SUCCESS) {
                data->cb(dce, status, NULL, data->cb_data);
                free(data);
                return;
        }

        data->cb(dce, 0, NULL, data->cb_data);
        free(data);
}

static void
smb2_bind_cb(struct smb2_context *smb2, int status,
             void *command_data, void *private_data)
{
        struct dcerpc_pdu *pdu = private_data;
        struct dcerpc_context *dce = pdu->dce;
        struct smb2_iovec iov _U_;
        struct smb2_ioctl_reply *rep = command_data;
        int i;
        int offset = 0;
        
        pdu->direction = DCERPC_DECODE;

        if (status != SMB2_STATUS_SUCCESS) {
                dcerpc_send_pdu_cb_and_free(dce, pdu, -nterror_to_errno(status), NULL);
                return;
        }

        iov.buf = rep->output;
        iov.len = rep->output_count;
        iov.free = NULL;
        if (dcerpc_pdu_coder(dce, pdu, &iov, &offset)) {
                smb2_free_data(dce->smb2, rep->output);
                dcerpc_send_pdu_cb_and_free(dce, pdu, -EINVAL, NULL);
                return;
        }
        smb2_free_data(dce->smb2, rep->output);

        if (pdu->hdr.PTYPE != PDU_TYPE_BIND_ACK) {
                smb2_set_error(dce->smb2, "DCERPC response was not a BIND_ACK");
                dcerpc_send_pdu_cb_and_free(dce, pdu, -EINVAL, NULL);
                return;
        }

        if (pdu->bind_ack.num_results < 1) {
                smb2_set_error(smb2, "No results in BIND ACK");
                dcerpc_send_pdu_cb_and_free(dce, pdu, -EINVAL, NULL);
                return;
        }
        for (i = 0; i < pdu->bind_ack.num_results; i++) {
                if (pdu->bind_ack.results[i].ack_result !=
                    ACK_RESULT_ACCEPTANCE) {
                        continue;
                }

                switch (smb2->ndr) {
                case 0:
                        dce->tctx_id = i;
                        break;
                case 1:
                        dce->tctx_id = 0;
                        break;
                case 2:
                        dce->tctx_id = 1;
                        break;
                }
                break;
        }
        if (i == pdu->bind_ack.num_results) {
                smb2_set_error(smb2, "Bind rejected all contexts");
                dcerpc_send_pdu_cb_and_free(dce, pdu, -EINVAL, NULL);
                return;
        }

        dcerpc_send_pdu_cb_and_free(dce, pdu, 0, NULL);
}

static int
dcerpc_bind_async(struct dcerpc_context *dce, dcerpc_cb cb,
                  void *cb_data)
{
        struct dcerpc_pdu *pdu;
        struct smb2_pdu *smb2_pdu;
        struct smb2_ioctl_request req;
        struct smb2_iovec iov _U_;
        int offset = 0;
        struct p_cont_elem_t *pce;

        pdu = dcerpc_allocate_pdu(dce, DCERPC_ENCODE, NSE_BUF_SIZE);
        if (pdu == NULL) {
                return -ENOMEM;
        }

        pdu->hdr.rpc_vers = 5;
        pdu->hdr.rpc_vers_minor = 0;
        pdu->hdr.PTYPE = PDU_TYPE_BIND;
        pdu->hdr.pfc_flags = PFC_FIRST_FRAG | PFC_LAST_FRAG;
        pdu->hdr.packed_drep[0] = dce->packed_drep[0];
        pdu->hdr.frag_length = 0;
        pdu->hdr.auth_length = 0;
        pdu->bind.max_xmit_frag = 32768;
        pdu->bind.max_recv_frag = 32768;
        pdu->bind.assoc_group_id = 0;
        pdu->bind.n_context_elem = dce->smb2->ndr ? 1 : 2;
        pdu->bind.p_cont_elem = smb2_alloc_data(dce->smb2, pdu->payload,
                     pdu->bind.n_context_elem * sizeof(struct p_cont_elem_t));
        if (pdu->bind.p_cont_elem == NULL) {
                smb2_set_error(dce->smb2, "Failed to allocate p_cont_elem");
                dcerpc_free_pdu(dce, pdu);
                return -ENOMEM;
        }
        pce = pdu->bind.p_cont_elem;
        if (dce->smb2->ndr == 0 || dce->smb2->ndr == 1) {
                pce->p_cont_id = 0;
                pce->n_transfer_syn = 1;
                pce->abstract_syntax = dce->syntax;
                pce->transfer_syntaxes = smb2_alloc_data(
                     dce->smb2, pdu->payload,
                     pce->n_transfer_syn * sizeof(struct p_cont_elem_t *));
                if (pce->transfer_syntaxes == NULL) {
                        smb2_set_error(dce->smb2, "Failed to allocate transfer_syntaxes");
                        dcerpc_free_pdu(dce, pdu);
                        return -ENOMEM;
                }
                pce->transfer_syntaxes[0] = &ndr32_syntax;
                pce++;
        }
        if (dce->smb2->ndr == 0 || dce->smb2->ndr == 2) {
                pce->p_cont_id = 1;
                pce->n_transfer_syn = 1;
                pce->abstract_syntax = dce->syntax;
                pce->transfer_syntaxes = smb2_alloc_data(
                     dce->smb2, pdu->payload,
                     pce->n_transfer_syn * sizeof(struct p_cont_elem_t *));
                if (pce->transfer_syntaxes == NULL) {
                        smb2_set_error(dce->smb2, "Failed to allocate transfer_syntaxes");
                        dcerpc_free_pdu(dce, pdu);
                        return -ENOMEM;
                }
                pce->transfer_syntaxes[0] = &ndr64_syntax;
        }

        pdu->cb = cb;
        pdu->cb_data = cb_data;

        iov.buf = pdu->payload;
        iov.len = NSE_BUF_SIZE;
        iov.free = NULL;
        if (dcerpc_pdu_coder(dce, pdu, &iov, &offset)) {
                dcerpc_free_pdu(dce, pdu);
                return -ENOMEM;
        }
        iov.len = offset;

        memset(&req, 0, sizeof(struct smb2_ioctl_request));
        req.ctl_code = SMB2_FSCTL_PIPE_TRANSCEIVE;
        memcpy(req.file_id, dce->file_id, SMB2_FD_SIZE);
        req.input_count = (uint32_t)iov.len;
        req.input = iov.buf;
        req.flags = SMB2_0_IOCTL_IS_FSCTL;

        smb2_pdu = smb2_cmd_ioctl_async(dce->smb2, &req, smb2_bind_cb, pdu);
        if (smb2_pdu == NULL) {
                dcerpc_free_pdu(dce, pdu);
                return -ENOMEM;
        }
        smb2_queue_pdu(dce->smb2, smb2_pdu);
 
        return 0;
}

static void
smb2_open_cb(struct smb2_context *smb2, int status,
             void *command_data, void *private_data)
{
        struct dcerpc_cb_data *data = private_data;
        struct smb2_create_reply *rep = command_data;
        struct dcerpc_context *dce = data->dce;

        if (status != SMB2_STATUS_SUCCESS) {
                data->cb(dce, -nterror_to_errno(status),
                         NULL, data->cb_data);
                free(data);
                return;
        }
        
        memcpy(dce->file_id, rep->file_id, SMB2_FD_SIZE);

        status = dcerpc_bind_async(dce, dcerpc_bind_cb, data);
        if (status) {
                data->cb(dce, status, NULL, data->cb_data);
                free(data);
                return;
        }

        return;
}

int
dcerpc_open_async(struct dcerpc_context *dce, dcerpc_cb cb,
                  void *cb_data)
{
        struct smb2_create_request req;
        struct smb2_pdu *pdu;
        struct dcerpc_cb_data *data;

        data = calloc(1, sizeof(struct dcerpc_cb_data));
        if (data == NULL) {
                smb2_set_error(dce->smb2, "Failed to allocate dcerpc callback "
                               "data");
                return -ENOMEM;
        }
        data->dce = dce;
        data->cb = cb;
        data->cb_data = cb_data;

        memset(&req, 0, sizeof(struct smb2_create_request));
        req.requested_oplock_level = SMB2_OPLOCK_LEVEL_NONE;
        req.impersonation_level = SMB2_IMPERSONATION_IMPERSONATION;
        req.desired_access = SMB2_FILE_READ_DATA |
                SMB2_FILE_WRITE_DATA |
                SMB2_FILE_APPEND_DATA |
                SMB2_FILE_READ_EA |
                SMB2_FILE_READ_ATTRIBUTES |
                SMB2_FILE_WRITE_EA |
                SMB2_FILE_WRITE_ATTRIBUTES |
                SMB2_READ_CONTROL |
                SMB2_SYNCHRONIZE;
        req.file_attributes = 0;
        req.share_access = SMB2_FILE_SHARE_READ |
                SMB2_FILE_SHARE_WRITE |
                SMB2_FILE_SHARE_DELETE;
        req.create_disposition = SMB2_FILE_OPEN;
        req.create_options = 0;
        req.name = dce->path;

        pdu = smb2_cmd_create_async(dce->smb2, &req, smb2_open_cb, data);
        if (pdu == NULL) {
                free(data);
                return -ENOMEM;
        }
        smb2_queue_pdu(dce->smb2, pdu);

        return 0;
}

const char *
dcerpc_get_error(struct dcerpc_context *dce)
{
        return smb2_get_error(dcerpc_get_smb2_context(dce));
}

void
dcerpc_free_data(struct dcerpc_context *dce, void *data)
{
        smb2_free_data(dcerpc_get_smb2_context(dce), data);
}

int
dcerpc_pdu_direction(struct dcerpc_pdu *pdu)
{
        return pdu->direction;
}

int
dcerpc_align_3264(struct dcerpc_context *ctx, int offset)
{
        if (offset < 0) {
                return offset;
        }

        if (ctx->tctx_id) {
                offset = (offset + 7) & ~7;
        } else {
                offset = (offset + 3) & ~3;
        }
        return offset;
}

/* Used for testing. Override/force the transfer syntax. */
void dcerpc_set_tctx(struct dcerpc_context *ctx, int tctx)
{
        ctx->tctx_id = tctx;
}

/* Used for testing. Override/force the transfer syntax. */
void dcerpc_set_endian(struct dcerpc_pdu *pdu, int little_endian)
{
        if (little_endian) {
                pdu->hdr.packed_drep[0] |= DCERPC_DR_LITTLE_ENDIAN;
        } else {
                pdu->hdr.packed_drep[0] &= ~DCERPC_DR_LITTLE_ENDIAN;
        }
}
int dcerpc_get_cr(struct dcerpc_pdu *pdu)
{
        return pdu->is_conformance_run;
}

                                                 
