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

#include <errno.h>
#include <stdio.h>

#include "compat.h"

#include "smb2.h"
#include "libsmb2.h"
#include "libsmb2-dcerpc.h"
#include "libsmb2-dcerpc-srvsvc.h"
#include "libsmb2-raw.h"
#include "libsmb2-private.h"

#define SRVSVC_UUID    0x4b324fc8, 0x1670, 0x01d3, {0x12, 0x78, 0x5a, 0x47, 0xbf, 0x6e, 0xe1, 0x88}

int dcerpc_get_cr(struct dcerpc_pdu *pdu);

p_syntax_id_t srvsvc_interface = {
        {SRVSVC_UUID}, 3, 0
};

/*
 * SRVSVC BEGIN:  DEFINITIONS FROM SRVSVC.IDL
 * [MS-SRVS].pdf
 */

/*
 * typedef struct _SHARE_INFO_0 {
 *     [string] wchar_t * shi0_netname;
 * } SHARE_INFO_0, *PSHARE_INFO_0, *LPSHARE_INFO_0;
 */
int
srvsvc_SHARE_INFO_0_coder(struct dcerpc_context *ctx,
                           struct dcerpc_pdu *pdu,
                           struct smb2_iovec *iov, int *offset,
                           void *ptr)
{
        struct srvsvc_SHARE_INFO_0 *nsi1 = ptr;

        if (dcerpc_ptr_coder(ctx, pdu, iov, offset, &nsi1->netname,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        return 0;
}

/*
 *       [size_is(EntriesRead)] LPSHARE_INFO_0 Buffer;
 */
static int
srvsvc_SHARE_INFO_0_carray_coder(struct dcerpc_context *ctx,
                                 struct dcerpc_pdu *pdu,
                                 struct smb2_iovec *iov, int *offset,
                                 void *ptr)
{
        return dcerpc_carray_coder(ctx, pdu, iov, offset, ptr,
                                   sizeof(struct srvsvc_SHARE_INFO_0),
                                   srvsvc_SHARE_INFO_0_coder);
}

/*
 * typedef struct _SHARE_INFO_0_CONTAINER {
 *       DWORD EntriesRead;
 *       [size_is(EntriesRead)] LPSHARE_INFO_0 Buffer;
 * } SHARE_INFO_0_CONTAINER;
*/
int
srvsvc_SHARE_INFO_0_CONTAINER_coder(struct dcerpc_context *dce, struct dcerpc_pdu *pdu,
                          struct smb2_iovec *iov, int *offset,
                          void *ptr)
{
        struct srvsvc_SHARE_INFO_0_CONTAINER *ctr = ptr;

        if (dcerpc_uint32_coder(dce, pdu, iov, offset, &ctr->EntriesRead)) {
                return -1;
        }
        if (dcerpc_pdu_direction(pdu) == DCERPC_DECODE) {
                if (ctr->Buffer == NULL) {
                        ctr->Buffer = smb2_alloc_data(
                                  dcerpc_get_smb2_context(dce),
                                  dcerpc_get_pdu_payload(pdu),
                                  sizeof(struct srvsvc_SHARE_INFO_0_carray));
                }
        }
        if (dcerpc_ptr_coder(dce, pdu, iov, offset,
                             ctr->Buffer,
                             PTR_UNIQUE,
                             srvsvc_SHARE_INFO_0_carray_coder)) {
                return -1;
        }

        return 0;
}

/*
 * typedef struct _SHARE_INFO_1 {
 *       [string] wchar_t *netname;
 *       DWORD shi1_type;
 *       [string] wchar_t *remark;
 * } SHARE_INFO_1, *PSHARE_INFO_1, *LPSHARE_INFO_1;
 */
int
srvsvc_SHARE_INFO_1_coder(struct dcerpc_context *ctx,
                           struct dcerpc_pdu *pdu,
                           struct smb2_iovec *iov, int *offset,
                           void *ptr)
{
        struct srvsvc_SHARE_INFO_1 *nsi1 = ptr;

        if (dcerpc_ptr_coder(ctx, pdu, iov, offset, &nsi1->netname,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder(ctx, pdu, iov, offset, &nsi1->type)) {
                return -1;
        }
        if (dcerpc_ptr_coder(ctx, pdu, iov, offset, &nsi1->remark,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        return 0;
}

/*
 *       [size_is(EntriesRead)] LPSHARE_INFO_1 Buffer;
 */
static int
srvsvc_SHARE_INFO_1_carray_coder(struct dcerpc_context *ctx,
                                 struct dcerpc_pdu *pdu,
                                 struct smb2_iovec *iov, int *offset,
                                 void *ptr)
{
        return dcerpc_carray_coder(ctx, pdu, iov, offset, ptr,
                                   sizeof(struct srvsvc_SHARE_INFO_1),
                                   srvsvc_SHARE_INFO_1_coder);
}

/*
 * typedef struct _SHARE_INFO_1_CONTAINER {
 *       DWORD EntriesRead;
 *       [size_is(EntriesRead)] LPSHARE_INFO_1 Buffer;
 * } SHARE_INFO_1_CONTAINER;
*/
int
srvsvc_SHARE_INFO_1_CONTAINER_coder(struct dcerpc_context *dce, struct dcerpc_pdu *pdu,
                          struct smb2_iovec *iov, int *offset,
                          void *ptr)
{
        struct srvsvc_SHARE_INFO_1_CONTAINER *ctr = ptr;

        if (dcerpc_uint32_coder(dce, pdu, iov, offset, &ctr->EntriesRead)) {
                return -1;
        }
        if (dcerpc_pdu_direction(pdu) == DCERPC_DECODE) {
                if (ctr->Buffer == NULL) {
                        ctr->Buffer = smb2_alloc_data(
                                  dcerpc_get_smb2_context(dce),
                                  dcerpc_get_pdu_payload(pdu),
                                  sizeof(struct srvsvc_SHARE_INFO_1_carray));
                }
        }
        if (dcerpc_ptr_coder(dce, pdu, iov, offset,
                             ctr->Buffer,
                             PTR_UNIQUE,
                             srvsvc_SHARE_INFO_1_carray_coder)) {
                return -1;
        }

        return 0;
}

/*
 * typedef [switch_type(DWORD)] union _SHARE_ENUM_UNION {
 * [case(0)] SHARE_INFO_0_CONTAINER* Level0;
 * [case(1)] SHARE_INFO_1_CONTAINER* Level1;
 * [case(2)] SHARE_INFO_2_CONTAINER* Level2;
 * [case(501)] SHARE_INFO_501_CONTAINER* Level501;
 * [case(502)] SHARE_INFO_502_CONTAINER* Level502;
 * [case(503)] SHARE_INFO_503_CONTAINER* Level503;
 * } SHARE_ENUM_UNION;
 */
static int
srvsvc_SHARE_ENUM_UNION_coder(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                         struct smb2_iovec *iov, int *offset,
                         void *ptr)
{
        struct srvsvc_SHARE_ENUM_UNION *ctr = ptr;
        uint64_t p;

        p = ctr->Level;
        if (dcerpc_uint3264_coder(ctx, pdu, iov, offset, &p)) {
                return -1;
        }
        ctr->Level = (uint32_t)p;

        switch (ctr->Level) {
        case 0:
                if (dcerpc_ptr_coder(ctx, pdu, iov, offset, &ctr->Level0,
                                     PTR_UNIQUE,
                                     srvsvc_SHARE_INFO_0_CONTAINER_coder)) {
                        return -1;
                }
                break;
        case 1:
                if (dcerpc_ptr_coder(ctx, pdu, iov, offset, &ctr->Level1,
                                     PTR_UNIQUE,
                                     srvsvc_SHARE_INFO_1_CONTAINER_coder)) {
                        return -1;
                }
                break;
        };

        return 0;
}

/*
 * typedef struct _SHARE_ENUM_STRUCT {
 *       DWORD Level;
 *       [switch_is(Level)] SHARE_ENUM_UNION ShareInfo;
 * } SHARE_ENUM_STRUCT, *PSHARE_ENUM_STRUCT, *LPSHARE_ENUM_STRUCT;
 */
int
srvsvc_SHARE_ENUM_STRUCT_coder(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                         struct smb2_iovec *iov, int *offset,
                         void *ptr)
{
        struct srvsvc_SHARE_ENUM_STRUCT *ses = ptr;

        if (dcerpc_uint32_coder(ctx, pdu, iov, offset, &ses->Level)) {
                return -1;
        }
        if (srvsvc_SHARE_ENUM_UNION_coder(ctx, pdu, iov, offset, &ses->ShareInfo)) {
                return -1;
        }

        return 0;
}

/*****************
 * Function: 0x0f
 * NET_API_STATUS NetrShareEnum (
 *   [in,string,unique] SRVSVC_HANDLE ServerName,
 *   [in,out] LPSHARE_ENUM_STRUCT InfoStruct,
 *   [in] DWORD PreferedMaximumLength,
 *   [out] DWORD * TotalEntries,
 *   [in,out,unique] DWORD * ResumeHandle
 * );
 */
int
srvsvc_NetrShareEnum_req_coder(struct dcerpc_context *ctx,
                               struct dcerpc_pdu *pdu,
                               struct smb2_iovec *iov, int *offset,
                               void *ptr)
{
        struct srvsvc_NetrShareEnum_req *req = ptr;

        if (dcerpc_ptr_coder(ctx, pdu, iov, offset, &req->ServerName,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder(ctx, pdu, iov, offset, &req->ses,
                             PTR_REF, srvsvc_SHARE_ENUM_STRUCT_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder(ctx, pdu, iov, offset, &req->PreferedMaximumLength,
                             PTR_REF, dcerpc_uint32_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder(ctx, pdu, iov, offset, &req->ResumeHandle,
                             PTR_UNIQUE, dcerpc_uint32_coder)) {
                return -1;
        }

        return 0;
}

int
srvsvc_NetrShareEnum_rep_coder(struct dcerpc_context *dce,
                               struct dcerpc_pdu *pdu,
                               struct smb2_iovec *iov, int *offset,
                               void *ptr)
{
        struct srvsvc_NetrShareEnum_rep *rep = ptr;

        if (dcerpc_ptr_coder(dce, pdu, iov, offset, &rep->ses,
                             PTR_REF, srvsvc_SHARE_ENUM_STRUCT_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder(dce, pdu, iov, offset, &rep->total_entries,
                             PTR_REF, dcerpc_uint32_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder(dce, pdu, iov, offset, &rep->resume_handle,
                             PTR_UNIQUE, dcerpc_uint32_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder(dce, pdu, iov, offset, &rep->status)) {
                return -1;
        }

        return 0;
}

/*
 * typedef [switch_type(unsigned long)] union _SHARE_INFO {
 *   [case(0)] LPSHARE_INFO_0 ShareInfo0;
 *   [case(1)] LPSHARE_INFO_1 ShareInfo1;
 *   [case(2)] LPSHARE_INFO_2 ShareInfo2;
 *   [case(502)] LPSHARE_INFO_502_I ShareInfo502;
 *   [case(1004)] LPSHARE_INFO_1004 ShareInfo1004;
 *   [case(1006)] LPSHARE_INFO_1006 ShareInfo1006;
 *   [case(1501)] LPSHARE_INFO_1501_I ShareInfo1501;
 *   [default];
 *   [case(1005)] LPSHARE_INFO_1005 ShareInfo1005;
 *   [case(501)] LPSHARE_INFO_501 ShareInfo501;
 *   [case(503)] LPSHARE_INFO_503_I ShareInfo503;
 * } SHARE_INFO, *PSHARE_INFO, *LPSHARE_INFO;
 */
static int
srvsvc_SHARE_INFO_coder(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                           struct smb2_iovec *iov, int *offset,
                          void *ptr)
{
        struct srvsvc_SHARE_INFO *info = ptr;
        uint64_t p;

        p = info->level;
        if (dcerpc_uint3264_coder(ctx, pdu, iov, offset, &p)) {
                return -1;
        }
        info->level = (uint32_t)p;

        switch (info->level) {
        case 1:
                if (dcerpc_ptr_coder(ctx, pdu, iov, offset, &info->ShareInfo1,
                                     PTR_UNIQUE,
                                     srvsvc_SHARE_INFO_1_coder)) {
                        return -1;
                }
                break;
        };

        return 0;
}

/******************
 * Function: 0x10
 * NET_API_STATUS NetrShareGetInfo (
 *    [in,string,unique] SRVSVC_HANDLE ServerName,
 *    [in,string] WCHAR * NetName,
 *    [in] DWORD Level,
 *    [out, switch_is(Level)] LPSHARE_INFO InfoStruct
*/
int
srvsvc_NetrShareGetInfo_req_coder(struct dcerpc_context *dce,
                                  struct dcerpc_pdu *pdu,
                                  struct smb2_iovec *iov, int *offset,
                                  void *ptr)
{
        struct srvsvc_NetrShareGetInfo_req *req = ptr;

        if (dcerpc_ptr_coder(dce, pdu, iov, offset, &req->ServerName,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder(dce, pdu, iov, offset,
                             discard_const(&req->NetName),
                             PTR_REF, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder(dce, pdu, iov, offset, &req->Level)) {
                return -1;
        }

        return 0;
}

int
srvsvc_NetrShareGetInfo_rep_coder(struct dcerpc_context *dce,
                                  struct dcerpc_pdu *pdu,
                                  struct smb2_iovec *iov, int *offset,
                                  void *ptr)
{
        struct srvsvc_NetrShareGetInfo_rep *rep = ptr;

        if (dcerpc_ptr_coder(dce, pdu, iov, offset, &rep->InfoStruct,
                             PTR_REF, srvsvc_SHARE_INFO_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder(dce, pdu, iov, offset, &rep->status)) {
                return -1;
        }

        return 0;
}
