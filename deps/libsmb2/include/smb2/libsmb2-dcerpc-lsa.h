/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2020 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

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

#ifndef _LIBSMB2_DCERPC_LSA_H_
#define _LIBSMB2_DCERPC_LSA_H_

#ifdef __cplusplus
extern "C" {
#endif

#define LSA_CLOSE          0x00
#define LSA_OPENPOLICY2    0x2c
#define LSA_LOOKUPSIDS2    0x39

/* Access Mask. LSA specific flags. */
#define POLICY_VIEW_LOCAL_INFORMATION    0x00000001
#define POLICY_VIEW_AUDIT_INFORMATION    0x00000002
#define POLICY_GET_PRIVATE_INFORMATION   0x00000004
#define POLICY_TRUST_ADMIN               0x00000008
#define POLICY_CREATE_ACCOUNT            0x00000010
#define POLICY_CREATE_SECRET             0x00000020
#define POLICY_CREATE_PRIVILEGE          0x00000040
#define POLICY_SET_DEFAULT_QUOTA_LIMITS  0x00000080
#define POLICY_SET_AUDIT_REQUIREMENTS    0x00000100
#define POLICY_AUDIT_LOG_ADMIN           0x00000200
#define POLICY_SERVER_ADMIN              0x00000400
#define POLICY_LOOKUP_NAMES              0x00000800
#define POLICY_NOTIFICATION              0x00001000

extern unsigned char NT_SID_AUTHORITY[6];

typedef struct RPC_SID {
        uint8_t Revision;
        uint8_t SubAuthorityCount;
        uint8_t IdentifierAuthority[6];
        uint32_t *SubAuthority;
} RPC_SID, *PRPC_SID;

typedef struct _LSAPR_TRANSLATED_NAME_EX {
        uint32_t Use;
        char *Name;
        uint32_t DomainIndex;
        uint32_t Flags;
} LSAPR_TRANSLATED_NAME_EX, *PLSAPR_TRANSLATED_NAME_EX;

typedef struct _LSAPR_TRANSLATED_NAMES_EX {
        uint32_t Entries;
        LSAPR_TRANSLATED_NAME_EX  *Names;
} LSAPR_TRANSLATED_NAMES_EX, *PLSAPR_TRANSLATED_NAMES_EX;

typedef struct _SID_ENUM_BUFFER {
        uint32_t Entries;
        PRPC_SID *SidInfo;
} LSAPR_SID_ENUM_BUFFER, *PLSAPR_SID_ENUM_BUFFER;

typedef enum _LSAP_LOOKUP_LEVEL {
        LsapLookupWksta = 1,
        LsapLookupPDC,
        LsapLookupTDL,
        LsapLookupGC,
        LsapLookupXForestReferral,
        LsapLookupXForestResolve,
        LsapLookupRODCReferralToFullDC
} LSAP_LOOKUP_LEVEL, *PLSAP_LOOKUP_LEVEL;

typedef struct _LSAPR_TRUST_INFORMATION {
        char *Name;
        RPC_SID Sid;
} LSAPR_TRUST_INFORMATION, *PLSAPR_TRUST_INFORMATION;

typedef struct _LSAPR_REFERENCED_DOMAIN_LIST {
        uint32_t Entries;
        LSAPR_TRUST_INFORMATION *Domains;
        uint32_t MaxEntries; /* must be ignored */
} LSAPR_REFERENCED_DOMAIN_LIST, *PLSAPR_REFERENCED_DOMAIN_LIST;

/* For OPENPOLICY2: RootDirectory MUST be zero. Everything else is ignored. */
typedef struct _LSAPR_OBJECT_ATTRIBUTES {
        uint32_t Length;
        unsigned char *RootDirectory;
        void *ObjectName;
        uint32_t Attributes;
        void *SecurityDescriptor;
        void *SecurityQualityOfService;
} LSAPR_OBJECT_ATTRIBUTES, *PLSAPR_OBJECT_ATTRIBUTES;

struct lsa_close_req {
        struct ndr_context_handle PolicyHandle;
};

struct lsa_close_rep {
        uint32_t status;

        struct ndr_context_handle PolicyHandle;
};

struct lsa_openpolicy2_req {
        char *SystemName;
        LSAPR_OBJECT_ATTRIBUTES ObjectAttributes;
        uint32_t DesiredAccess;
};

struct lsa_openpolicy2_rep {
        uint32_t status;

        struct ndr_context_handle PolicyHandle;
};

struct lsa_lookupsids2_req {
        struct ndr_context_handle PolicyHandle;
        LSAPR_SID_ENUM_BUFFER SidEnumBuffer;
        LSAPR_TRANSLATED_NAMES_EX TranslatedNames;
        LSAP_LOOKUP_LEVEL LookupLevel;
};

struct lsa_lookupsids2_rep {
        uint32_t status;

        LSAPR_REFERENCED_DOMAIN_LIST ReferencedDomains;
        LSAPR_TRANSLATED_NAMES_EX TranslatedNames;
        uint32_t MappedCount;
};

int lsa_Close_rep_coder(struct dcerpc_context *dce,
                        struct dcerpc_pdu *pdu,
                        struct smb2_iovec *iov, int *offset,
                        void *ptr);
int lsa_Close_req_coder(struct dcerpc_context *dce,
                        struct dcerpc_pdu *pdu,
                        struct smb2_iovec *iov, int *offset,
                        void *ptr);
int lsa_LookupSids2_rep_coder(struct dcerpc_context *dce,
                              struct dcerpc_pdu *pdu,
                              struct smb2_iovec *iov, int *offset,
                              void *ptr);
int lsa_LookupSids2_req_coder(struct dcerpc_context *dce,
                              struct dcerpc_pdu *pdu,
                              struct smb2_iovec *iov, int *offset,
                              void *ptr);
int lsa_OpenPolicy2_rep_coder(struct dcerpc_context *dce,
                              struct dcerpc_pdu *pdu,
                              struct smb2_iovec *iov, int *offset,
                              void *ptr);
int lsa_OpenPolicy2_req_coder(struct dcerpc_context *dce,
                              struct dcerpc_pdu *pdu,
                              struct smb2_iovec *iov, int *offset,
                              void *ptr);
int lsa_RPC_SID_coder(struct dcerpc_context *dce,
                      struct dcerpc_pdu *pdu,
                      struct smb2_iovec *iov, int *offset,
                      void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* !_LIBSMB2_DCERPC_LSA_H_ */
