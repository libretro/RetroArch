/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2016 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

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

#ifndef _SMB2_H_
#define _SMB2_H_

#include <smb2/smb2-errors.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct smb2_timeval {
        time_t tv_sec;
        long tv_usec;
};
#define SMB2_ERROR_REPLY_SIZE 9

struct smb2_error_reply {
        uint8_t error_context_count;
        uint32_t byte_count;
        uint8_t *error_data;
};


#define SMB2_FLAGS_SERVER_TO_REDIR    0x00000001
#define SMB2_FLAGS_ASYNC_COMMAND      0x00000002
#define SMB2_FLAGS_RELATED_OPERATIONS 0x00000004
#define SMB2_FLAGS_SIGNED             0x00000008
#define SMB2_FLAGS_PRIORITY_MASK      0x00000070
#define SMB2_FLAGS_DFS_OPERATIONS     0x10000000
#define SMB2_FLAGS_REPLAY_OPERATION   0x20000000

enum smb2_command {
        SMB2_NEGOTIATE       = 0,
        SMB2_SESSION_SETUP   = 1,
        SMB2_LOGOFF          = 2,
        SMB2_TREE_CONNECT    = 3,
        SMB2_TREE_DISCONNECT = 4,
        SMB2_CREATE          = 5,
        SMB2_CLOSE           = 6,
        SMB2_FLUSH           = 7,
        SMB2_READ            = 8,
        SMB2_WRITE           = 9,
        SMB2_LOCK            = 10,
        SMB2_IOCTL           = 11,
        SMB2_CANCEL          = 12,
        SMB2_ECHO            = 13,
        SMB2_QUERY_DIRECTORY = 14,
        SMB2_CHANGE_NOTIFY   = 15,
        SMB2_QUERY_INFO      = 16,
        SMB2_SET_INFO        = 17,
        SMB2_OPLOCK_BREAK    = 18,

        SMB1_NEGOTIATE       = 114,
};

/*
 * SMB2 NEGOTIATE
 */
#define SMB2_NEGOTIATE_SIGNING_ENABLED  0x0001
#define SMB2_NEGOTIATE_SIGNING_REQUIRED 0x0002

#define SMB2_GLOBAL_CAP_DFS                0x00000001
#define SMB2_GLOBAL_CAP_LEASING            0x00000002
#define SMB2_GLOBAL_CAP_LARGE_MTU          0x00000004
#define SMB2_GLOBAL_CAP_MULTI_CHANNEL      0x00000008
#define SMB2_GLOBAL_CAP_PERSISTENT_HANDLES 0x00000010
#define SMB2_GLOBAL_CAP_DIRECTORY_LEASING  0x00000020
#define SMB2_GLOBAL_CAP_ENCRYPTION         0x00000040

#define SMB2_PREAUTH_INTEGRITY_CAP         0x0001
#define SMB2_ENCRYPTION_CAP                0x0002
#define SMB2_COMPRESSION_CAP               0x0003
#define SMB2_NETNAME_NEGOTIATE_CONTEXT_ID  0x0005
#define SMB2_TRANSPORT_CAP                 0x0006
#define SMB2_RDMA_TRANSFORM_CAP            0x0007
#define SMB2_SIGNING_CAP                   0x0008
#define SMB2_CONTEXTTYPE_RESERVED          0x0100

#define SMB2_HASH_SHA_512                  0x0001
#define SMB2_PREAUTH_HASH_SIZE             64

#define SMB2_ENCRYPTION_AES_128_CCM        0x0001
#define SMB2_ENCRYPTION_AES_128_GCM        0x0002

#define SMB2_NEGOTIATE_MAX_DIALECTS 10

#define SMB2_NEGOTIATE_REQUEST_SIZE 36

#define SMB2_GUID_SIZE 16
typedef uint8_t smb2_guid[SMB2_GUID_SIZE];

struct smb2_negotiate_request {
        uint16_t dialect_count;
        uint16_t security_mode;
        uint32_t capabilities;
        smb2_guid client_guid;
        uint32_t negotiate_context_offset;
        uint16_t negotiate_context_count;
        uint16_t dialects[SMB2_NEGOTIATE_MAX_DIALECTS];
};

#define SMB2_NEGOTIATE_REPLY_SIZE 65

struct smb2_negotiate_reply {
        uint16_t security_mode;
        uint16_t dialect_revision;
        uint16_t cypher;
        smb2_guid server_guid;
        uint32_t capabilities;
        uint32_t max_transact_size;
        uint32_t max_read_size;
        uint32_t max_write_size;
        uint64_t system_time;
        uint64_t server_start_time;
        uint32_t negotiate_context_offset;
        uint16_t negotiate_context_count;
        uint16_t security_buffer_length;
        uint16_t security_buffer_offset;
        uint8_t *security_buffer;
};

/* session setup flags */
#define SMB2_SESSION_FLAG_BINDING 0x01

/* session setup capabilities */
#define SMB2_GLOBAL_CAP_DFS     0x00000001
#define SMB2_GLOBAL_CAP_UNUSED1 0x00000002
#define SMB2_GLOBAL_CAP_UNUSED2 0x00000004
#define SMB2_GLOBAL_CAP_UNUSED4 0x00000008

#define SMB2_SESSION_SETUP_REQUEST_SIZE 25

struct smb2_session_setup_request {
        uint8_t flags;
        uint8_t security_mode;
        uint32_t capabilities;
        uint32_t channel;
        uint64_t previous_session_id;
        uint16_t security_buffer_length;
        uint8_t *security_buffer;
};

#define SMB2_SESSION_FLAG_IS_GUEST        0x0001
#define SMB2_SESSION_FLAG_IS_NULL         0x0002
#define SMB2_SESSION_FLAG_IS_ENCRYPT_DATA 0x0004

#define SMB2_SESSION_SETUP_REPLY_SIZE 9

struct smb2_session_setup_reply {
        uint16_t session_flags;
        uint16_t security_buffer_length;
        uint16_t security_buffer_offset;
        uint8_t *security_buffer;
};

#define SMB2_TREE_CONNECT_REQUEST_SIZE 9

#define SMB2_SHAREFLAG_CLUSTER_RECONNECT 0x0001

struct smb2_tree_connect_request {
        uint16_t flags;
        uint16_t path_offset;
        uint16_t path_length;
        uint16_t *path;
};

#define SMB2_SHARE_TYPE_DISK  0x01
#define SMB2_SHARE_TYPE_PIPE  0x02
#define SMB2_SHARE_TYPE_PRINT 0x03

#define SMB2_SHAREFLAG_MANUAL_CACHING              0x00000000
#define SMB2_SHAREFLAG_DFS                         0x00000001
#define SMB2_SHAREFLAG_DFS_ROOT                    0x00000002
#define SMB2_SHAREFLAG_AUTO_CACHING                0x00000010
#define SMB2_SHAREFLAG_VDO_CACHING                 0x00000020
#define SMB2_SHAREFLAG_NO_CACHING                  0x00000030
#define SMB2_SHAREFLAG_RESTRICT_EXCLUSIVE_OPENS    0x00000100
#define SMB2_SHAREFLAG_FORCE_SHARED_DELETE         0x00000200
#define SMB2_SHAREFLAG_ALLOW_NAMESPACE_CACHING     0x00000400
#define SMB2_SHAREFLAG_ACCESS_BASED_DIRECTORY_ENUM 0x00000800
#define SMB2_SHAREFLAG_FORCE_LEVELII_OPLOCK        0x00001000
#define SMB2_SHAREFLAG_ENABLE_HASH_V1              0x00002000
#define SMB2_SHAREFLAG_ENABLE_HASH_V2              0x00004000
#define SMB2_SHAREFLAG_ENCRYPT_DATA                0x00008000

#define SMB2_SHARE_CAP_DFS                         0x00000008
#define SMB2_SHARE_CAP_CONTINUOUS_AVAILABILITY     0x00000010
#define SMB2_SHARE_CAP_SCALEOUT                    0x00000020
#define SMB2_SHARE_CAP_CLUSTER                     0x00000040
#define SMB2_SHARE_CAP_ASYMMETRIC                  0x00000080

#define SMB2_TREE_CONNECT_REPLY_SIZE 16

struct smb2_tree_connect_reply {
        uint8_t share_type;
        uint32_t share_flags;
        uint32_t capabilities;
        uint32_t maximal_access;
};

#define SMB2_CREATE_REQUEST_SIZE 57

#define SMB2_OPLOCK_LEVEL_NONE      0x00
#define SMB2_OPLOCK_LEVEL_II        0x01
#define SMB2_OPLOCK_LEVEL_EXCLUSIVE 0x08
#define SMB2_OPLOCK_LEVEL_BATCH     0x09
#define SMB2_OPLOCK_LEVEL_LEASE     0xff

#define SMB2_CREATE_REQUEST_LEASE_SIZE  32

#define SMB2_IMPERSONATION_ANONYMOUS      0x00000000
#define SMB2_IMPERSONATION_IDENTIFICATION 0x00000001
#define SMB2_IMPERSONATION_IMPERSONATION  0x00000002
#define SMB2_IMPERSONATION_DELEGATE       0x00000003

/* Access mask common to all objects */
#define SMB2_FILE_READ_EA           0x00000008
#define SMB2_FILE_WRITE_EA          0x00000010
#define SMB2_FILE_DELETE_CHILD      0x00000040
#define SMB2_FILE_READ_ATTRIBUTES   0x00000080
#define SMB2_FILE_WRITE_ATTRIBUTES  0x00000100
#define SMB2_DELETE                 0x00010000
#define SMB2_READ_CONTROL           0x00020000
#define SMB2_WRITE_DACL             0x00040000
#define SMB2_WRITE_OWNER            0x00080000
#define SMB2_SYNCHRONIZE            0x00100000
#define SMB2_ACCESS_SYSTEM_SECURITY 0x01000000
#define SMB2_MAXIMUM_ALLOWED        0x02000000
#define SMB2_GENERIC_ALL            0x10000000
#define SMB2_GENERIC_EXECUTE        0x20000000
#define SMB2_GENERIC_WRITE          0x40000000
#define SMB2_GENERIC_READ           0x80000000

/* Access mask unique for file/pipe/printer */
#define SMB2_FILE_READ_DATA         0x00000001
#define SMB2_FILE_WRITE_DATA        0x00000002
#define SMB2_FILE_APPEND_DATA       0x00000004
#define SMB2_FILE_EXECUTE           0x00000020

/* Access mask unique for directories */
#define SMB2_FILE_LIST_DIRECTORY    0x00000001
#define SMB2_FILE_ADD_FILE          0x00000002
#define SMB2_FILE_ADD_SUBDIRECTORY  0x00000004
#define SMB2_FILE_TRAVERSE          0x00000020

/* File attributes */
#define SMB2_FILE_ATTRIBUTE_READONLY            0x00000001
#define SMB2_FILE_ATTRIBUTE_HIDDEN              0x00000002
#define SMB2_FILE_ATTRIBUTE_SYSTEM              0x00000004
#define SMB2_FILE_ATTRIBUTE_DIRECTORY           0x00000010
#define SMB2_FILE_ATTRIBUTE_ARCHIVE             0x00000020
#define SMB2_FILE_ATTRIBUTE_NORMAL              0x00000080
#define SMB2_FILE_ATTRIBUTE_TEMPORARY           0x00000100
#define SMB2_FILE_ATTRIBUTE_SPARSE_FILE         0x00000200
#define SMB2_FILE_ATTRIBUTE_REPARSE_POINT       0x00000400
#define SMB2_FILE_ATTRIBUTE_COMPRESSED          0x00000800
#define SMB2_FILE_ATTRIBUTE_OFFLINE             0x00001000
#define SMB2_FILE_ATTRIBUTE_NOT_CONTENT_INDEXED 0x00002000
#define SMB2_FILE_ATTRIBUTE_ENCRYPTED           0x00004000
#define SMB2_FILE_ATTRIBUTE_INTEGRITY_STREAM    0x00008000
#define SMB2_FILE_ATTRIBUTE_NO_SCRUB_DATA       0x00020000

/* Share access */
#define SMB2_FILE_SHARE_READ 0x00000001
#define SMB2_FILE_SHARE_WRITE 0x00000002
#define SMB2_FILE_SHARE_DELETE 0x00000004

/* Create disposition */
#define SMB2_FILE_SUPERSEDE    0x00000000
#define SMB2_FILE_OPEN         0x00000001
#define SMB2_FILE_CREATE       0x00000002
#define SMB2_FILE_OPEN_IF      0x00000003
#define SMB2_FILE_OVERWRITE    0x00000004
#define SMB2_FILE_OVERWRITE_IF 0x00000005

/* Create options */
#define SMB2_FILE_DIRECTORY_FILE            0x00000001
#define SMB2_FILE_WRITE_THROUGH             0x00000002
#define SMB2_FILE_SEQUENTIAL_ONLY           0x00000004
#define SMB2_FILE_NO_INTERMEDIATE_BUFFERING 0x00000008
#define SMB2_FILE_SYNCHRONOUS_IO_ALERT      0x00000010
#define SMB2_FILE_SYNCHRONOUS_IO_NONALERT   0x00000020
#define SMB2_FILE_NON_DIRECTORY_FILE        0x00000040
#define SMB2_FILE_COMPLETE_IF_OPLOCKED      0x00000100
#define SMB2_FILE_NO_EA_KNOWLEDGE           0x00000200
#define SMB2_FILE_RANDOM_ACCESS             0x00000800
#define SMB2_FILE_DELETE_ON_CLOSE           0x00001000
#define SMB2_FILE_OPEN_BY_FILE_ID           0x00002000
#define SMB2_FILE_OPEN_FOR_BACKUP_INTENT    0x00004000
#define SMB2_FILE_NO_COMPRESSION            0x00008000
#define SMB2_FILE_OPEN_REMOTE_INSTANCE      0x00000400
#define SMB2_FILE_OPEN_REQUIRING_OPLOCK     0x00010000
#define SMB2_FILE_DISALLOW_EXCLUSIVE        0x00020000
#define SMB2_FILE_RESERVE_OPFILTER          0x00100000
#define SMB2_FILE_OPEN_REPARSE_POINT        0x00200000
#define SMB2_FILE_OPEN_NO_RECALL            0x00400000
#define SMB2_FILE_OPEN_FOR_FREE_SPACE_QUERY 0x00800000

struct smb2_create_request {
        uint8_t security_flags;
        uint8_t requested_oplock_level;
        uint32_t impersonation_level;
        uint64_t smb_create_flags;
        uint32_t desired_access;
        uint32_t file_attributes;
        uint32_t share_access;
        uint32_t create_disposition;
        uint32_t create_options;
        uint16_t name_offset;
        uint16_t name_length;
        const char *name;       /* name in UTF8 */
        uint32_t create_context_offset;
        uint32_t create_context_length;
        uint8_t *create_context;
};

#define SMB2_CREATE_REPLY_SIZE 89

#define SMB2_FD_SIZE 16
typedef uint8_t smb2_file_id[SMB2_FD_SIZE];

#define SMB2_LEASE_KEY_SIZE 16
typedef uint8_t smb2_lease_key[SMB2_LEASE_KEY_SIZE];

struct smb2fh;
smb2_file_id *smb2_get_file_id(struct smb2fh *fh);

/*
 * This creates a new smb2fh based on fileid.
 * Free it with smb2_close_async()
 */
struct smb2_context;
struct smb2fh *smb2_fh_from_file_id(struct smb2_context *smb2,
                                    smb2_file_id *fileid);

struct smb2_create_reply {
        uint8_t oplock_level;
        uint8_t flags;
        uint32_t create_action;
        uint64_t creation_time;
        uint64_t last_access_time;
        uint64_t last_write_time;
        uint64_t change_time;
        uint64_t allocation_size;
        uint64_t end_of_file;
        uint32_t file_attributes;
        smb2_file_id file_id;
        uint32_t create_context_length;
        uint32_t create_context_offset;
        uint8_t *create_context;
};

#define SMB2_CLOSE_REQUEST_SIZE 24

#define SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB 0x0001

struct smb2_close_request {
        uint16_t flags;
        smb2_file_id file_id;
};

#define SMB2_CLOSE_REPLY_SIZE 60

struct smb2_close_reply {
        uint16_t flags;
        uint64_t creation_time;
        uint64_t last_access_time;
        uint64_t last_write_time;
        uint64_t change_time;
        uint64_t allocation_size;
        uint64_t end_of_file;
        uint32_t file_attributes;
};

#define SMB2_FLUSH_REQUEST_SIZE 24

struct smb2_flush_request {
        smb2_file_id file_id;
};

#define SMB2_LOGFF_REQUEST_SIZE 4

struct smb2_logoff_request {
        uint16_t reserved;
};

#define SMB2_ECHO_REQUEST_SIZE 4

struct smb2_echo_request {
        uint16_t reserved;
};

#define SMB2_FLUSH_REPLY_SIZE 4

#define SMB2_QUERY_DIRECTORY_REQUEST_SIZE 33

/* File information class */
#define SMB2_FILE_DIRECTORY_INFORMATION         0x01
#define SMB2_FILE_FULL_DIRECTORY_INFORMATION    0x02
#define SMB2_FILE_BOTH_DIRECTORY_INFORMATION    0x03
#define SMB2_FILE_ID_BOTH_DIRECTORY_INFORMATION 0x25
#define SMB2_FILE_ID_FULL_DIRECTORY_INFORMATION 0x26

/* query flags */
#define SMB2_RESTART_SCANS       0x01
#define SMB2_RETURN_SINGLE_ENTRY 0x02
#define SMB2_INDEX_SPECIFIED     0x04
#define SMB2_REOPEN              0x10

#define SMB2_FILEID_FULL_DIRECTORY_INFORMATION_SIZE  80

/* Structure for SMB2_FILE_ID_FULL_DIRECTORY_INFORMATION.
 * This is also used as the dirent content.
 */
struct smb2_fileidfulldirectoryinformation {
        uint32_t next_entry_offset;
        uint32_t file_index;
        struct smb2_timeval creation_time;
        struct smb2_timeval last_access_time;
        struct smb2_timeval last_write_time;
        struct smb2_timeval change_time;
        uint64_t end_of_file;
        uint64_t allocation_size;
        uint32_t file_attributes;
        uint32_t file_name_length;
        uint32_t ea_size;
        uint64_t file_id;
        const char *name; /* or "reserved" for replys */
};

#define SMB2_FILEID_BOTH_DIRECTORY_INFORMATION_SIZE  104

/* Structure for SMB2_FILE_ID_BOTH_DIRECTORY_INFORMATION.
 */
struct smb2_fileidbothdirectoryinformation {
        uint32_t next_entry_offset;
        uint32_t file_index;
        struct smb2_timeval creation_time;
        struct smb2_timeval last_access_time;
        struct smb2_timeval last_write_time;
        struct smb2_timeval change_time;
        uint64_t end_of_file;
        uint64_t allocation_size;
        uint32_t file_attributes;
        uint32_t file_name_length;
        uint32_t ea_size;
        uint8_t short_name_length;
        uint8_t short_name[24];
        uint64_t file_id;
        const char *name; /* or "reserved" for replys */
};

struct smb2_iovec;
int smb2_decode_fileidfulldirectoryinformation(
        struct smb2_context *smb2,
        struct smb2_fileidfulldirectoryinformation *fs,
        struct smb2_iovec *vec);

struct smb2_query_directory_request {
        uint8_t file_information_class;
        uint8_t flags;
        uint32_t file_index;
        smb2_file_id file_id;
        uint32_t output_buffer_length;
        uint16_t file_name_offset;
        uint16_t file_name_length;
        const char *name;       /* name in UTF8 */
};

#define SMB2_QUERY_DIRECTORY_REPLY_SIZE 9

struct smb2_query_directory_reply {
        uint16_t output_buffer_offset;
        uint32_t output_buffer_length;
        uint8_t *output_buffer;
};

#define SMB2_READ_REQUEST_SIZE 49

#define SMB2_READFLAG_READ_UNBUFFERED 0x01

#define SMB2_CHANNEL_NONE               0x00000000
#define SMB2_CHANNEL_RDMA_V1            0x00000001
#define SMB2_CHANNEL_RDMA_V1_INVALIDATE 0x00000002

struct smb2_read_request {
        uint8_t flags;
        uint32_t length;
        uint64_t offset;
        uint8_t *buf;
        smb2_file_id file_id;
        uint32_t minimum_count;
        uint32_t channel;
        uint32_t remaining_bytes;
        uint16_t read_channel_info_offset;
        uint16_t read_channel_info_length;
        uint8_t *read_channel_info;
};

#define SMB2_READ_REPLY_SIZE 17

struct smb2_read_reply {
        uint8_t data_offset;
        uint32_t data_length;
        uint32_t data_remaining;
        uint8_t *data;
};

#define SMB2_QUERY_INFO_REQUEST_SIZE 41

/* info type */
#define SMB2_0_INFO_FILE       0x01
#define SMB2_0_INFO_FILESYSTEM 0x02
#define SMB2_0_INFO_SECURITY   0x03
#define SMB2_0_INFO_QUOTA      0x04

/* File information class : for SMB2_0_INFO_FILE */
#define SMB2_FILE_DIRECTORY_INFORMATION         0x01
#define SMB2_FILE_FULL_DIRECTORY_INFORMATION    0x02
#define SMB2_FILE_BOTH_DIRECTORY_INFORMATION    0x03
#define SMB2_FILE_BASIC_INFORMATION             0x04
#define SMB2_FILE_STANDARD_INFORMATION          0x05
#define SMB2_FILE_INTERNAL_INFORMATION          0x06
#define SMB2_FILE_EA_INFORMATION                0x07
#define SMB2_FILE_ACCESS_INFORMATION            0x08
#define SMB2_FILE_NAME_INFORMATION              0x09
#define SMB2_FILE_RENAME_INFORMATION            0x0A
#define SMB2_FILE_LINK_INFORMATION              0x0B
#define SMB2_FILE_NAMES_INFORMATION             0x0C
#define SMB2_FILE_DISPOSITION_INFORMATION       0x0D
#define SMB2_FILE_POSITION_INFORMATION          0x0E
#define SMB2_FILE_FULL_EA_INFORMATION           0x0F
#define SMB2_FILE_MODE_INFORMATION              0x10
#define SMB2_FILE_ALIGNMENT_INFORMATION         0x11
#define SMB2_FILE_ALL_INFORMATION               0x12
#define SMB2_FILE_ALLOCATION_INFORMATION        0x13
#define SMB2_FILE_END_OF_FILE_INFORMATION       0x14
#define SMB2_FILE_ALTERNATE_NAME_INFORMATION    0x15
#define SMB2_FILE_OBJECT_ID_INFORMATION         0x1D
#define SMB2_FILE_ATTRIBUTE_TAG_INFORMATION     0x23
#define SMB2_FILE_VALID_DATA_LENGTH_INFORMATION 0x27
#define SMB2_FILE_NORMALIZED_NAME_INFORMATION   0x30
#define SMB2_FILE_ID_INFORMATION                0x3B

#define SMB2_FILE_STREAM_INFORMATION            0x16
#define SMB2_FILE_PIPE_INFORMATION              0x17
#define SMB2_FILE_PIPE_LOCAL_INFORMATION        0x18
#define SMB2_FILE_PIPE_REMOTE_INFORMATION       0x19
#define SMB2_FILE_MAILSLOT_QUERY_INFORMATION    0x1A
#define SMB2_FILE_MAILSLOT_SET_INFORMATION      0x1B
#define SMB2_FILE_COMPRESSION_INFORMATION       0x1C
#define SMB2_FILE_OBJECT_ID_INFORMATION         0x1D
#define SMB2_FILE_QUOTA_INFORMATION             0x20
#define SMB2_FILE_REPARSE_POINT_INFORMATION     0x21
#define SMB2_FILE_NETWORK_OPEN_INFORMATION      0x22

/* Filesystem information class : for SMB2_0_INFO_FILESYSTEM */
#define SMB2_FILE_FS_VOLUME_INFORMATION            1
#define SMB2_FILE_FS_SIZE_INFORMATION              3
#define SMB2_FILE_FS_DEVICE_INFORMATION            4
#define SMB2_FILE_FS_ATTRIBUTE_INFORMATION         5
#define SMB2_FILE_FS_CONTROL_INFORMATION           6
#define SMB2_FILE_FS_FULL_SIZE_INFORMATION         7
#define SMB2_FILE_FS_OBJECT_ID_INFORMATION         8
#define SMB2_FILE_FS_SECTOR_SIZE_INFORMATION      11

#define SMB2_FILE_INFO_CLASS_RESERVED           0x40

/* additional info */
#define SMB2_OWNER_SECURITY_INFORMATION     0x00000001
#define SMB2_GROUP_SECURITY_INFORMATION     0x00000002
#define SMB2_DACL_SECURITY_INFORMATION      0x00000004
#define SMB2_SACL_SECURITY_INFORMATION      0x00000008
#define SMB2_LABEL_SECURITY_INFORMATION     0x00000010
#define SMB2_ATTRIBUTE_SECURITY_INFORMATION 0x00000020
#define SMB2_SCOPE_SECURITY_INFORMATION     0x00000040
#define SMB2_BACKUP_SECURITY_INFORMATION    0x00010000

/* flags */
#define SL_RESTART_SCAN        0x00000001
#define SL_RETURN_SINGLE_ENTRY 0x00000002
#define SL_INDEX_SPECIFIED     0x00000004

/*
 * FILE_BASIC_INFORMATION
 */
struct smb2_file_basic_info {
        struct smb2_timeval creation_time;
        struct smb2_timeval last_access_time;
        struct smb2_timeval last_write_time;
        struct smb2_timeval change_time;
        uint32_t file_attributes;
};

/*
 * FILE_STANDARD_INFORMATION
 */
struct smb2_file_standard_info {
        uint64_t allocation_size;
        uint64_t end_of_file;
        uint32_t number_of_links;
        uint8_t delete_pending;
        uint8_t directory;
};

/*
 * FILE_STREAM_INFORMATION
 */
struct smb2_file_stream_info {
        uint32_t next_entry_offset;
        uint32_t stream_name_length;
        uint64_t stream_size;
        uint64_t stream_allocation_size;
        const char *stream_name;
};

/*
 * FILE_POSITION_INFORMATION
 */
struct smb2_file_position_info {
        uint64_t current_byte_offset;
};

/*
 * FILE_NAME_INFORMATION
 */
struct smb2_file_name_info {
        uint32_t file_name_length;
        const uint8_t *name;
};

/*
 * FILE_ALL_INFORMATION.
 */
struct smb2_file_all_info {
        struct smb2_file_basic_info basic;
        struct smb2_file_standard_info standard;
        uint64_t index_number;
        uint32_t ea_size;
        uint32_t access_flags;
        uint64_t current_byte_offset;
        uint32_t mode;
        uint32_t alignment_requirement;
        const uint8_t *name;
};

struct smb2_query_info_request {
        uint8_t info_type;
        uint8_t file_info_class;
        uint32_t output_buffer_length;
        uint16_t input_buffer_offset;
        uint32_t input_buffer_length;
        uint8_t *input_buffer;
        uint32_t additional_information;
        uint32_t flags;
        smb2_file_id file_id;
        const uint8_t *input;
};

/*
 * FILE_END_OF_FILE_INFORMATION.
 */
struct smb2_file_end_of_file_info {
        uint64_t end_of_file;
};

/*
 * FILE_DISPOSITION_INFORMATION.
 */
struct smb2_file_disposition_info {
        uint8_t delete_pending;
};

/*
 * SMB2_FILE_RENAME_INFORMATION.
 */
struct smb2_file_rename_info {
        uint8_t replace_if_exist;
        const uint8_t* file_name;
};

/*
 * FILE_NETWORK_OPEN_INFORMATION
 */
struct smb2_file_network_open_info {
        struct smb2_timeval creation_time;
        struct smb2_timeval last_access_time;
        struct smb2_timeval last_write_time;
        struct smb2_timeval change_time;
        uint64_t allocation_size;
        uint64_t end_of_file;
        uint32_t file_attributes;
};

#define SMB2_SET_INFO_REQUEST_SIZE 33

struct smb2_set_info_request {
        uint8_t info_type;
        uint8_t file_info_class;
        uint32_t buffer_length;
        uint16_t buffer_offset;
        uint32_t additional_information;
        smb2_file_id file_id;
        void *input_data;
};

#define SMB2_SET_INFO_REPLY_SIZE 2

/*
 * SID
 */
#define SID_ID_AUTH_LEN 6

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning( disable : 4200 ) /* Silence c4200 warning. */
#endif

struct smb2_sid {
        uint8_t revision;
        uint8_t sub_auth_count;
        uint8_t id_auth[SID_ID_AUTH_LEN];
        uint32_t sub_auth[0];
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

/*
 * ACE
 */
/* ace type */
#define SMB2_ACCESS_ALLOWED_ACE_TYPE                 0x00
#define SMB2_ACCESS_DENIED_ACE_TYPE                  0x01
#define SMB2_SYSTEM_AUDIT_ACE_TYPE                   0x02
/*
 * Reserved for future use
 * #define SMB2_SYSTEM_ALARM_ACE_TYPE                   0x03
 */
#define SMB2_ACCESS_ALLOWED_OBJECT_ACE_TYPE          0x05
#define SMB2_ACCESS_DENIED_OBJECT_ACE_TYPE           0x06
#define SMB2_SYSTEM_AUDIT_OBJECT_ACE_TYPE            0x07
/*
 * Reserved for future use
 * #define SMB2_SYSTEM_ALARM_OBJECT_ACE_TYPE            0x08
 */
#define SMB2_ACCESS_ALLOWED_CALLBACK_ACE_TYPE        0x09
#define SMB2_ACCESS_DENIED_CALLBACK_ACE_TYPE         0x10
#define SMB2_SYSTEM_MANDATORY_LABEL_ACE_TYPE         0x11
#define SMB2_SYSTEM_RESOURCE_ATTRIBUTE_ACE_TYPE      0x12
#define SMB2_SYSTEM_SCOPED_POLICY_ID_ACE_TYPE        0x13

/* ace flags */
#define SMB2_OBJECT_INHERIT_ACE         0x01
#define SMB2_CONTAINER_INHERIT_ACE      0x02
#define SMB2_NO_PROPAGATE_INHERIT_ACE   0x04
#define SMB2_INHERIT_ONLY_ACE           0x08
#define SMB2_INHERITED_ACE              0x10
#define SMB2_SUCCESSFUL_ACCESS_ACE_FLAG 0x40
#define SMB2_FAILED_ACCESS_ACE_FLAG     0x80

#define SMB2_OBJECT_TYPE_SIZE 16

struct smb2_ace {
        struct smb2_ace *next;

        uint8_t ace_type;
        uint8_t ace_flags;
        uint16_t ace_size;

        /* Which fields are valid depends on the ace type */
        uint32_t mask;
        uint32_t flags;
        struct smb2_sid *sid;
        uint8_t object_type[SMB2_OBJECT_TYPE_SIZE];
        uint8_t inherited_object_type[SMB2_OBJECT_TYPE_SIZE];

        /* ApplicationData/AttributeData. Used by
         * SMB2_ACCESS_ALLOWED_CALLBACK_ACE_TYPE,
         * SMB2_DENIED_ALLOWED_CALLBACK_ACE_TYPE,
         * SMB2_SYSTEM_RESOURCE_ATTRIBUTE_ACE_TYPE
         */
        size_t ad_len;
        char *ad_data;

        /* raw blob, used for unknown ACE types */
        size_t   raw_len;
        char *raw_data;
};

/*
 * ACL
 */
#define SMB2_ACL_REVISION    0x02
#define SMB2_ACL_REVISION_DS 0x04

struct smb2_acl {
        uint8_t revision;
        uint16_t ace_count;
        struct smb2_ace *aces;
};

/*
 * SECURITY_DESCRIPTOR
 */
/* Security descriptor control flags */
#define SMB2_SD_CONTROL_OD 0x0001
#define SMB2_SD_CONTROL_GD 0x0002
#define SMB2_SD_CONTROL_DP 0x0004
#define SMB2_SD_CONTROL_DD 0x0008
#define SMB2_SD_CONTROL_SP 0x0010
#define SMB2_SD_CONTROL_SD 0x0020
#define SMB2_SD_CONTROL_SS 0x0040
#define SMB2_SD_CONTROL_DT 0x0080
#define SMB2_SD_CONTROL_DC 0x0100
#define SMB2_SD_CONTROL_SC 0x0200
#define SMB2_SD_CONTROL_DI 0x0400
#define SMB2_SD_CONTROL_SI 0x0800
#define SMB2_SD_CONTROL_PD 0x1000
#define SMB2_SD_CONTROL_PS 0x2000
#define SMB2_SD_CONTROL_RM 0x4000
#define SMB2_SD_CONTROL_SR 0x8000

struct smb2_security_descriptor {
        uint8_t revision;
        uint16_t control;
        struct smb2_sid *owner;
        struct smb2_sid *group;
        struct smb2_acl *dacl;
};

struct smb2_file_fs_volume_info {
        struct smb2_timeval creation_time;
        uint32_t volume_serial_number;
        uint32_t volume_label_length;
        uint8_t supports_objects;
        uint8_t reserved;
        const uint8_t *volume_label;
};

struct smb2_file_fs_size_info {
        uint64_t total_allocation_units;
        uint64_t available_allocation_units;
        uint32_t sectors_per_allocation_unit;
        uint32_t bytes_per_sector;
};

struct smb2_file_fs_attribute_info {
        uint32_t filesystem_attributes;
        uint32_t maximum_component_name_length;
        uint32_t filesystem_name_length;
        const uint8_t *filesystem_name;
};

/* Device type */
#define FILE_DEVICE_CD_ROM 0x00000002
#define FILE_DEVICE_DISK   0x00000007

/* Characteristics */
#define FILE_REMOVABLE_MEDIA                     0x00000001
#define FILE_READ_ONLY_DEVICE                    0x00000002
#define FILE_FLOPPY_DISKETTE                     0x00000004
#define FILE_WRITE_ONCE_MEDIA                    0x00000008
#define FILE_REMOTE_DEVICE                       0x00000010
#define FILE_DEVICE_IS_MOUNTED                   0x00000020
#define FILE_VIRTUAL_VOLUME                      0x00000040
#define FILE_DEVICE_SECURE_OPEN                  0x00000100
#define FILE_CHARACTERISTIC_TS_DEVICE            0x00001000
#define FILE_CHARACTERISTIC_WEBDAV_DEVICE        0x00002000
#define FILE_DEVICE_ALLOW_APPCONTAINER_TRAVERSAL 0x00020000
#define FILE_PORTABLE_DEVICE                     0x00040000

struct smb2_file_fs_device_info {
        uint32_t device_type;
        uint32_t characteristics;
};

/* File System Control Flags */
#define FILE_VC_QUOTA_TRACK            0x00000001
#define FILE_VC_QUOTA_ENFORCE          0x00000002
#define FILE_VC_CONTENT_INDEX_DISABLED 0x00000008
#define FILE_VC_LOG_QUOTA_THRESHOLD    0x00000010
#define FILE_VC_LOG_QUOTA_LIMIT        0x00000020
#define FILE_VC_LOG_VOLUME_THRESHOLD   0x00000040
#define FILE_VC_LOG_VOLUME_LIMIT       0x00000080
#define FILE_VC_QUOTAS_INCOMPLETE      0x00000100
#define FILE_VC_QUOTAS_REBUILDING      0x00000200

struct smb2_file_fs_control_info {
        uint64_t free_space_start_filtering;
        uint64_t free_space_threshold;
        uint64_t free_space_stop_filtering;
        uint64_t default_quota_threshold;
        uint64_t default_quota_limit;
        uint32_t file_system_control_flags;
};

struct smb2_file_fs_full_size_info {
        uint64_t total_allocation_units;
        uint64_t caller_available_allocation_units;
        uint64_t actual_available_allocation_units;
        uint32_t sectors_per_allocation_unit;
        uint32_t bytes_per_sector;
};

struct smb2_file_fs_object_id_info {
        smb2_guid object_id;
        uint8_t extended_info[48];
};

/* Flags */
#define SSINFO_FLAGS_ALIGNED_DEVICE              0x00000001
#define SSINFO_FLAGS_PARTITION_ALIGNED_ON_DEVICE 0x00000002
#define SSINFO_FLAGS_NO_SEEK_PENALTY             0x00000004
#define SSINFO_FLAGS_TRIM_ENABLED                0x00000008

struct smb2_file_fs_sector_size_info {
        uint32_t logical_bytes_per_sector;
        uint32_t physical_bytes_per_sector_for_atomicity;
        uint32_t physical_bytes_per_sector_for_performance;
        uint32_t file_system_effective_physical_bytes_per_sector_for_atomicity;
        uint32_t flags;
        uint32_t byte_offset_for_sector_alignment;
        uint32_t byte_offset_for_partition_alignment;
};

#define SMB2_QUERY_INFO_REPLY_SIZE 9

struct smb2_query_info_reply {
        uint16_t output_buffer_offset;
        uint32_t output_buffer_length;
        void *output_buffer;
};

#define SMB2_IOCTL_REQUEST_SIZE 57

/* CtlCode */
#define SMB2_FSCTL_DFS_GET_REFERRALS            0x00060194
#define SMB2_FSCTL_PIPE_PEEK                    0x0011400C
#define SMB2_FSCTL_PIPE_WAIT                    0x00110018
#define SMB2_FSCTL_PIPE_TRANSCEIVE              0x0011C017
#define SMB2_FSCTL_SRV_COPYCHUNK                0x001440F2
#define SMB2_FSCTL_SRV_ENUMERATE_SNAPSHOTS      0x00144064
#define SMB2_FSCTL_SRV_REQUEST_RESUME_KEY       0x00140078
#define SMB2_FSCTL_SRV_READ_HASH                0x001441bb
#define SMB2_FSCTL_SRV_COPYCHUNK_WRITE          0x001480F2
#define SMB2_FSCTL_LMR_REQUEST_RESILIENCY       0x001401D4
#define SMB2_FSCTL_QUERY_NETWORK_INTERFACE_INFO 0x001401FC
#define SMB2_FSCTL_SET_REPARSE_POINT            0x000900A4
#define SMB2_FSCTL_GET_REPARSE_POINT            0X000900A8
#define SMB2_FSCTL_DFS_GET_REFERRALS_EX         0x000601B0
#define SMB2_FSCTL_FILE_LEVEL_TRIM              0x00098208
#define SMB2_FSCTL_VALIDATE_NEGOTIATE_INFO      0x00140204

/* Flags */
#define SMB2_0_IOCTL_IS_FSCTL                   0x00000001

#define SMB2_SYMLINK_FLAG_RELATIVE 0x00000001
struct smb2_symlink_reparse_buffer {
        uint32_t flags;
        char *subname;
        char *printname;
};

#define SMB2_REPARSE_TAG_SYMLINK                0xa000000c
/*
 * Reparse_data_buffer
 */
struct smb2_reparse_data_buffer {
        uint32_t reparse_tag;
        uint16_t reparse_data_length;
        union {
                struct smb2_symlink_reparse_buffer symlink;
        };
};

struct smb2_ioctl_request {
        uint32_t ctl_code;
        smb2_file_id file_id;
        uint32_t input_offset;
        uint32_t input_count;
        uint32_t max_input_response;
        uint32_t output_offset;
        uint32_t output_count;
        uint32_t max_output_response;
        uint32_t flags;
        void *input;
};

#define SMB2_IOCTL_REPLY_SIZE 49

struct smb2_ioctl_reply {
        uint32_t ctl_code;
        smb2_file_id file_id;
        uint32_t input_offset;
        uint32_t input_count;
        uint32_t output_offset;
        uint32_t output_count;
        uint32_t flags;
        void *output;
};

#define SMB2_IOCTL_VALIDIATE_NEGOTIATE_INFO_SIZE 24

struct  smb2_ioctl_validate_negotiate_info {
        uint32_t capabilities;
        uint8_t  guid[16];
        uint16_t security_mode;
        uint16_t dialect;
};

#define SMB2_CHANGE_NOTIFY_FILE_NOTIFY_CHANGE_FILE_NAME    0x00000001
#define SMB2_CHANGE_NOTIFY_FILE_NOTIFY_CHANGE_DIR_NAME     0x00000002
#define SMB2_CHANGE_NOTIFY_FILE_NOTIFY_CHANGE_ATTRIBUTES   0x00000004
#define SMB2_CHANGE_NOTIFY_FILE_NOTIFY_CHANGE_SIZE         0x00000008
#define SMB2_CHANGE_NOTIFY_FILE_NOTIFY_CHANGE_LAST_WRITE   0x00000010
#define SMB2_CHANGE_NOTIFY_FILE_NOTIFY_CHANGE_LAST_ACCESS  0x00000020
#define SMB2_CHANGE_NOTIFY_FILE_NOTIFY_CHANGE_CREATION     0x00000040
#define SMB2_CHANGE_NOTIFY_FILE_NOTIFY_CHANGE_EA           0x00000080
#define SMB2_CHANGE_NOTIFY_FILE_NOTIFY_CHANGE_SECURITY     0x00000100
#define SMB2_CHANGE_NOTIFY_FILE_NOTIFY_CHANGE_STREAM_NAME  0x00000200
#define SMB2_CHANGE_NOTIFY_FILE_NOTIFY_CHANGE_STREAM_SIZE  0x00000400
#define SMB2_CHANGE_NOTIFY_FILE_NOTIFY_CHANGE_STREAM_WRITE 0x00000800

#define SMB2_CHANGE_NOTIFY_WATCH_TREE    0x0001

#define SMB2_CHANGE_NOTIFY_REQUEST_SIZE 32

#define SMB2_NOTIFY_CHANGE_FILE_ACTION_ADDED                0x0001
#define SMB2_NOTIFY_CHANGE_FILE_ACTION_REMOVED              0x0002
#define SMB2_NOTIFY_CHANGE_FILE_ACTION_MODIFIED             0x0003
#define SMB2_NOTIFY_CHANGE_FILE_ACTION_RENAMED_OLD_NAME     0x0004
#define SMB2_NOTIFY_CHANGE_FILE_ACTION_RENAMED_NEW_NAME     0x0005
#define SMB2_NOTIFY_CHANGE_FILE_ACTION_ADDED_STREAM         0x0006
#define SMB2_NOTIFY_CHANGE_FILE_ACTION_REMOVED_STREAM       0x0007
#define SMB2_NOTIFY_CHANGE_FILE_ACTION_MODIFIED_STREAM      0x0008

struct smb2_change_notify_request {
        uint16_t flags;
        uint32_t output_buffer_length;
        smb2_file_id file_id;
        uint32_t completion_filter;
};

#define SMB2_CHANGE_NOTIFY_REPLY_SIZE 9

struct smb2_change_notify_reply {
        uint16_t output_buffer_offset;
        uint32_t output_buffer_length;
        uint8_t *output;
};

struct smb2_file_notify_change_information {
        uint32_t action;
        const char *name;
        struct smb2_file_notify_change_information *next;
};

int
smb2_decode_filenotifychangeinformation(
    struct smb2_context *smb2,
    struct smb2_file_notify_change_information *fnc,
    struct smb2_iovec *vec,
    uint32_t next_entry_offset);

#define SMB2_OPLOCK_LEVEL_NONE        0x00
#define SMB2_OPLOCK_LEVEL_II          0x01
#define SMB2_OPLOCK_LEVEL_EXCLUSIVE   0x08

#define SMB2_OPLOCK_BREAK_NOTIFICATION_SIZE 24

struct smb2_oplock_break_notification {
        uint8_t oplock_level;
        smb2_file_id file_id;
};

#define SMB2_OPLOCK_BREAK_ACKNOWLEDGE_SIZE 24

struct smb2_oplock_break_acknowledgement {
        uint8_t oplock_level;
        smb2_file_id file_id;
};

#define SMB2_OPLOCK_BREAK_REPLY_SIZE 24

struct smb2_oplock_break_reply {
        uint8_t oplock_level;
        smb2_file_id file_id;
};

#define SMB2_LEASE_NONE                 0x00
#define SMB2_LEASE_READ_CACHING         0x01
#define SMB2_LEASE_HANDLE_CACHING       0x02
#define SMB2_LEASE_WRITE_CACHING        0x04

#define SMB2_BREAK_TYPE_OPLOCK_NOTIFICATION     0x01
#define SMB2_BREAK_TYPE_OPLOCK_RESPONSE         0x02
#define SMB2_BREAK_TYPE_OPLOCK_ACKNOWLEDGE      0x03

#define SMB2_BREAK_TYPE_LEASE_NOTIFICATION      0x04
#define SMB2_BREAK_TYPE_LEASE_RESPONSE          0x05
#define SMB2_BREAK_TYPE_LEASE_ACKNOWLEDGE       0x06

#define SMB2_LEASE_BREAK_NOTIFICATION_SIZE 44

struct smb2_lease_break_notification {
        uint16_t new_epoch;
        uint32_t flags;
        smb2_lease_key lease_key;
        uint32_t current_lease_state;
        uint32_t new_lease_state;
        uint32_t break_reason;
        uint32_t access_mask_hint;
        uint32_t share_mask_hint;
};

#define SMB2_LEASE_BREAK_ACKNOWLEDGE_SIZE 36

struct smb2_lease_break_acknowledgement {
        uint32_t flags;
        smb2_lease_key lease_key;
        uint32_t lease_state;
        uint64_t lease_duration;
};

#define SMB2_LEASE_BREAK_REPLY_SIZE 36

struct smb2_lease_break_reply {
        uint32_t flags;
        smb2_lease_key lease_key;
        uint32_t lease_state;
        uint64_t lease_duration;
};

/* note that for oplocks, notifications (request) and responses (reply)
 * come from the server, while acknowledgements come from the client
 */
struct smb2_oplock_or_lease_break_reply {
        uint16_t struct_size;
        int break_type;
        union {
                struct smb2_oplock_break_notification oplock;
                struct smb2_oplock_break_reply oplockrep;
                struct smb2_lease_break_notification lease;
                struct smb2_lease_break_reply leaserep;
        }
        lock;
};

struct smb2_oplock_or_lease_break_request {
        uint16_t struct_size;
        int break_type;
        union {
                struct smb2_oplock_break_acknowledgement oplock;
                struct smb2_lease_break_acknowledgement lease;
        }
        lock;
};

#define SMB2_WRITE_REQUEST_SIZE 49

#define SMB2_WRITEFLAG_WRITE_THROUGH    0x00000001
#define SMB2_WRITEFLAG_WRITE_UNBUFFERED 0x00000002

struct smb2_write_request {
        uint16_t data_offset;
        uint32_t length;
        uint64_t offset;
        const uint8_t* buf;
        smb2_file_id file_id;
        uint32_t channel;
        uint32_t remaining_bytes;
        uint16_t write_channel_info_offset;
        uint16_t write_channel_info_length;
        uint8_t *write_channel_info;
        uint32_t flags;
};

#define SMB2_WRITE_REPLY_SIZE 17

struct smb2_write_reply {
        uint32_t count;
        uint32_t remaining;
};

#define SMB2_LOCK_ELEMENT_SIZE 24

struct smb2_lock_element {
        uint64_t offset;
        uint64_t length;
        uint32_t flags;
        uint32_t reserved;
};

/* Note that this size includes 1 lock element */
#define SMB2_LOCK_REQUEST_SIZE 48

struct smb2_lock_request {
        uint16_t lock_count;
        uint8_t  lock_sequence_number;
        uint32_t lock_sequence_index;
        smb2_file_id file_id;
        struct smb2_lock_element *locks;
};

#define SMB2_LOCK_REPLY_SIZE 4


#define SMB2_ECHO_REQUEST_SIZE 4
#define SMB2_ECHO_REPLY_SIZE 4
#define SMB2_CANCEL_REQUEST_SIZE 4

#define SMB2_LOGOFF_REQUEST_SIZE 4
#define SMB2_LOGOFF_REPLY_SIZE 4

#define SMB2_TREE_DISCONNECT_REQUEST_SIZE 4
#define SMB2_TREE_DISCONNECT_REPLY_SIZE 4

#define SMB_ENCRYPTION_AES128_CCM     0x0001

#ifdef __cplusplus
}
#endif

#endif /* !_SMB2_H_ */
