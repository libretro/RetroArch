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

#ifndef _LIBSMB2_PRIVATE_H_
#define _LIBSMB2_PRIVATE_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_LIBKRB5
#include <krb5/krb5.h>

#if __APPLE__
#import <GSS/GSS.h>
#else
#include <gssapi/gssapi.h>
#include <gssapi/gssapi_ext.h>
#endif /* __APPLE__ */
#endif /* HAVE_LIBKRB5 */

#define MIN(a,b) (((a)<(b))?(a):(b))

#ifndef discard_const
#define discard_const(ptr) ((void *)((intptr_t)(ptr)))
#endif

#define MAX_ERROR_SIZE 256

#define PAD_TO_32BIT(len) ((len + 0x03) & 0xfffffffc)
#define PAD_TO_64BIT(len) ((len + 0x07) & 0xfffffff8)

#define SMB2_SPL_SIZE 4
#define SMB2_HEADER_SIZE 64

#define SMB2_SIGNATURE_SIZE 16
#define SMB2_KEY_SIZE 16

#define SMB2_MAX_VECTORS 256

struct smb2_io_vectors {
        size_t num_done;
        size_t total_size;
        int niov;
        struct smb2_iovec iov[SMB2_MAX_VECTORS];
};

struct smb2_async {
        uint64_t async_id;
};

struct smb2_sync {
        uint32_t process_id;
        uint32_t tree_id;
};

struct smb2_header {
        uint8_t protocol_id[4];
        uint16_t struct_size;
        uint16_t credit_charge;
        uint32_t status;
        uint16_t command;
        uint16_t credit_request_response;
        uint32_t flags;
        uint32_t next_command;
        uint64_t message_id;
        union {
                struct smb2_async async;
                struct smb2_sync sync;
        };
        uint64_t session_id;
        uint8_t signature[16];
};

/* States that we transition when we read data back from the server for
 * normal SMB2/3 :
 * 1: SMB2_RECV_SPL        SPL
 * 2: SMB2_RECV_HEADER     SMB2 Header
 * 3: SMB2_RECV_FIXED      The fixed part of the payload.
 * 4: SMB2_RECV_VARIABLE   Optional variable part of the payload.
 * 5: SMB2_RECV_PAD        Optional padding
 *
 * 2-5 will be repeated for compound commands.
 * 4-5 are optional and may or may not be present depending on the
 *     type of command.
 *
 * States for SMB3 encryption:
 * 1: SMB2_RECV_SPL        SPL
 * 2: SMB2_RECV_HEADER     SMB3 Transform Header
 * 3: SMB2_RECV_TRFM       encrypted payload
 *
 * States for cancelled PDUs
 * This is used when we receive a reply for a PDU not in our waitlist.
 * This can for example happen if we have cancelled a pdu when it was
 * in-flight.
 * 1: SMB2_RECV_SPL        SPL
 * 2: SMB2_RECV_HEADER     SMB3 Transform Header
 * 3: SMB2_RECV_UNKNOWN    data for a PDU we are not waiting for
 */
enum smb2_recv_state {
        SMB2_RECV_SPL = 0,
        SMB2_RECV_HEADER,
        SMB2_RECV_FIXED,
        SMB2_RECV_VARIABLE,
        SMB2_RECV_PAD,
        SMB2_RECV_TRFM,
        SMB2_RECV_UNKNOWN,
};

/* current tree id stack, note: index 0 in the stack is not used
*/
#define SMB2_MAX_TREE_NESTING 32
#define smb2_tree_id(smb2) (((smb2)->tree_id_cur >= 0)?smb2->tree_id[(smb2)->tree_id_cur]:0xdeadbeef)

#define MAX_CREDITS 1024
#define SMB2_SALT_SIZE 32

struct sync_cb_data {
	int is_finished;
	int status;
	void *ptr;
};

struct smb2_context {

        t_socket fd;

        struct smb2_server *owning_server;

        t_socket *connecting_fds;
        size_t connecting_fds_count;
        struct addrinfo *addrinfos;
        const struct addrinfo *next_addrinfo;

        int timeout;

        enum smb2_sec sec;

        uint16_t security_mode;
        uint32_t capabilities;
        int use_cached_creds;

        enum smb2_negotiate_version version;

        const char *server;
        const char *share;
        const char *user;

        /* Only used with --without-libkrb5 */
        const char *password;
        const char *domain;
        const char *workstation;
        char client_challenge[8];

        void *opaque;

        smb2_error_cb error_cb;
        smb2_command_cb connect_cb;
        void *connect_data;
        struct sync_cb_data connect_cb_data;

        int credits;

        char client_guid[16];

        uint32_t tree_id[SMB2_MAX_TREE_NESTING];
        int  tree_id_top;
        int  tree_id_cur;
        uint64_t message_id;
        uint64_t session_id;
        uint64_t async_id;
        uint8_t *session_key;
        uint8_t session_key_size;

        uint8_t seal:1;
        uint8_t sign:1;
        uint8_t signing_key[SMB2_KEY_SIZE];
        uint8_t serverin_key[SMB2_KEY_SIZE];
        uint8_t serverout_key[SMB2_KEY_SIZE];
        uint8_t salt[SMB2_SALT_SIZE];
        uint16_t cypher;
        uint8_t preauthhash[SMB2_PREAUTH_HASH_SIZE];


#ifdef HAVE_LIBKRB5
        /* for delegation of client creds to proxy-client */
        gss_cred_id_t cred_handle;
#endif
        /*
         * For handling received smb3 encrypted blobs
         */
        unsigned char *enc;
        size_t enc_len;
        int enc_pos;

        /*
         * For sending PDUs
         */
        struct smb2_pdu *outqueue;
        struct smb2_pdu *waitqueue;

        /*
         * For receiving PDUs
         */
        struct smb2_io_vectors in;
        enum smb2_recv_state recv_state;
        /* SPL for the (compound) command we are currently reading */
        uint32_t spl;
        /* buffer to avoid having to malloc the header */
        uint8_t header[SMB2_HEADER_SIZE];
        struct smb2_header hdr;
        /* Offset into smb2->in where the payload for the current PDU starts */
        size_t payload_offset;

        /* Pointer to the current PDU that we are receiving the reply for.
         * Only valid once the full smb2 header has been received.
         */
        struct smb2_pdu *pdu;

        /* pointer to the pdu to read AFTER the current one is completed
         * (if this context is a server)
         */
        struct smb2_pdu *next_pdu;

        /* flag indicated command packers/unpackers can pass "extra"
         * content without trying to decode or encode it.  this is
         * useful for proxies and applies only to the commands with
         * complex data: query-info, query-directory, ioctl, and
         * create (contexts).  the command fixed part is always
         * de/en-coded regardless of this setting
         */
        int passthrough;

        /* for oplock/lease breaks, inform the app */
        smb2_oplock_or_lease_break_cb oplock_or_lease_break_cb;

        /* oplock state, needed to discriminate between notification or response */
        int oplock_break_count;

        /* last file_id in a create-reply, for "related requests" */
        smb2_file_id last_file_id;

        /* Server capabilities */
        uint8_t supports_multi_credit;

        uint32_t max_transact_size;
        uint32_t max_read_size;
        uint32_t max_write_size;
        uint16_t dialect;

        char error_string[MAX_ERROR_SIZE];
        int nterror;

        /* callbacks for the eventsystem */
        int events;
        smb2_change_fd_cb change_fd;
        smb2_change_events_cb change_events;

        /* dcerpc settings */
        uint8_t ndr;
        int endianness;

        /* to maintain lists of contexts for server used */
        struct smb2_context *next;
};

/*
 * Callback for freeing a payload.
 */
typedef void (*smb2_free_payload)(struct smb2_context *smb2, void *payload);


#define SMB2_MAX_PDU_SIZE 16*1024*1024

struct smb2_pdu {
        struct smb2_pdu *next;
        struct smb2_header header;

        struct smb2_pdu *next_compound;
        uint64_t prev_compound_mid;

        int caller_frees_pdu;
        smb2_command_cb cb;
        void *cb_data;
        void (*free_cb)(void *);

        /* buffer to avoid having to malloc the headers */
        uint8_t hdr[SMB2_HEADER_SIZE];

        /* pointer to the unmarshalled payload in a reply */
        void *payload;

        /* callback that frees the any additional memory allocated in the payload.
         * Or null if no additional memory needs to be freed.
         */
        smb2_free_payload free_payload;

        /* For sending/receiving
         * out contains at least two vectors:
         * [0]  64 bytes for the smb header
         * [1+] command and and extra parameters
         *
         * in contains at least one vector:
         * [0+] command and and extra parameters
         */
        struct smb2_io_vectors out;
        struct smb2_io_vectors in;

        /* Data we need to retain between request/reply for QUERY INFO */
        uint8_t info_type;
        uint8_t file_info_class;

        /* For encrypted PDUs */
        uint8_t seal:1;
        uint32_t crypt_len;
        unsigned char *crypt;
        time_t timeout;
};

struct smb2_dirent_internal {
        struct smb2_dirent_internal *next;
        struct smb2dirent dirent;
};

struct smb2dir {
        smb2_command_cb cb;
        void (*free_cb_data)(void *);
        void *cb_data;
        smb2_file_id file_id;

        struct smb2_dirent_internal *entries;
        struct smb2_dirent_internal *current_entry;
        int index;
};


#define smb2_is_server(ctx) ((ctx)->owning_server != NULL)

void smb2_set_nterror(struct smb2_context *smb2, int nterror,
                    const char *error_string, ...);

void smb2_close_connecting_fds(struct smb2_context *smb2);

void *smb2_alloc_init(struct smb2_context *smb2, size_t size);
void *smb2_alloc_data(struct smb2_context *smb2, void *memctx, size_t size);

struct smb2_iovec *smb2_add_iovector(struct smb2_context *smb2,
                                     struct smb2_io_vectors *v,
                                     uint8_t *buf, size_t len,
                                     void (*free)(void *));

int smb2_pad_to_64bit(struct smb2_context *smb2, struct smb2_io_vectors *v);

int smb2_connect_tree_id(struct smb2_context *smb2, uint32_t tree_id);
int smb2_disconnect_tree_id(struct smb2_context *smb2, uint32_t tree_id);

struct smb2_pdu *smb2_allocate_pdu(struct smb2_context *smb2,
                                   enum smb2_command command,
                                   smb2_command_cb cb, void *cb_data);
int smb2_process_payload_fixed(struct smb2_context *smb2,
                               struct smb2_pdu *pdu);
int smb2_process_payload_variable(struct smb2_context *smb2,
                                  struct smb2_pdu *pdu);
int smb2_get_fixed_size(struct smb2_context *smb2, struct smb2_pdu *pdu);

struct smb2_pdu *smb2_find_pdu(struct smb2_context *smb2, uint64_t message_id);
void smb2_free_iovector(struct smb2_context *smb2, struct smb2_io_vectors *v);

void smb2_oplock_break_notify(struct smb2_context *smb2, int status, void *command_data, void *cb_data);

int smb2_decode_header(struct smb2_context *smb2, struct smb2_iovec *iov,
                       struct smb2_header *hdr);
int smb2_calc_signature(struct smb2_context *smb2, uint8_t *signature,
                        struct smb2_iovec *iov, size_t niov);

int smb2_set_uint8(struct smb2_iovec *iov, int offset, uint8_t value);
int smb2_set_uint16(struct smb2_iovec *iov, int offset, uint16_t value);
int smb2_set_uint32(struct smb2_iovec *iov, int offset, uint32_t value);
int smb2_set_uint64(struct smb2_iovec *iov, int offset, uint64_t value);

int smb2_get_uint8(struct smb2_iovec *iov, int offset, uint8_t *value);
int smb2_get_uint16(struct smb2_iovec *iov, int offset, uint16_t *value);
int smb2_get_uint32(struct smb2_iovec *iov, int offset, uint32_t *value);
int smb2_get_uint64(struct smb2_iovec *iov, int offset, uint64_t *value);

int smb2_process_error_fixed(struct smb2_context *smb2,
                             struct smb2_pdu *pdu);
int smb2_process_error_variable(struct smb2_context *smb2,
                                struct smb2_pdu *pdu);
int smb2_process_negotiate_fixed(struct smb2_context *smb2,
                                 struct smb2_pdu *pdu);
int smb2_process_negotiate_variable(struct smb2_context *smb2,
                                    struct smb2_pdu *pdu);
int smb2_process_negotiate_request_fixed(struct smb2_context *smb2,
                                     struct smb2_pdu *pdu);
int smb2_process_negotiate_request_variable(struct smb2_context *smb2,
                                     struct smb2_pdu *pdu);
int smb2_process_session_setup_fixed(struct smb2_context *smb2,
                                     struct smb2_pdu *pdu);
int smb2_process_session_setup_variable(struct smb2_context *smb2,
                                        struct smb2_pdu *pdu);
int smb2_process_session_setup_request_fixed(struct smb2_context *smb2,
                                        struct smb2_pdu *pdu);
int smb2_process_session_setup_request_variable(struct smb2_context *smb2,
                                        struct smb2_pdu *pdu);
int smb2_process_tree_connect_fixed(struct smb2_context *smb2,
                                        struct smb2_pdu *pdu);
int smb2_process_tree_connect_request_fixed(struct smb2_context *smb2,
                                        struct smb2_pdu *pdu);
int smb2_process_tree_connect_request_variable(struct smb2_context *smb2,
                                        struct smb2_pdu *pdu);
int smb2_process_create_fixed(struct smb2_context *smb2,
                              struct smb2_pdu *pdu);
int smb2_process_create_variable(struct smb2_context *smb2,
                                 struct smb2_pdu *pdu);
int smb2_process_create_request_fixed(struct smb2_context *smb2,
                              struct smb2_pdu *pdu);
int smb2_process_create_request_variable(struct smb2_context *smb2,
                                 struct smb2_pdu *pdu);
int smb2_process_query_directory_fixed(struct smb2_context *smb2,
                                       struct smb2_pdu *pdu);
int smb2_process_query_directory_variable(struct smb2_context *smb2,
                                          struct smb2_pdu *pdu);
int smb2_process_query_directory_request_fixed(struct smb2_context *smb2,
                                       struct smb2_pdu *pdu);
int smb2_process_query_directory_request_variable(struct smb2_context *smb2,
                                          struct smb2_pdu *pdu);
int smb2_process_change_notify_fixed(struct smb2_context *smb2,
                             struct smb2_pdu *pdu);
int smb2_process_change_notify_variable(struct smb2_context *smb2,
                             struct smb2_pdu *pdu);
int smb2_process_change_notify_request_fixed(struct smb2_context *smb2,
                             struct smb2_pdu *pdu);
int smb2_process_query_info_fixed(struct smb2_context *smb2,
                                  struct smb2_pdu *pdu);
int smb2_process_query_info_variable(struct smb2_context *smb2,
                                     struct smb2_pdu *pdu);
int smb2_process_query_info_request_fixed(struct smb2_context *smb2,
                                  struct smb2_pdu *pdu);
int smb2_process_query_info_request_variable(struct smb2_context *smb2,
                                     struct smb2_pdu *pdu);
int smb2_process_close_fixed(struct smb2_context *smb2,
                             struct smb2_pdu *pdu);
int smb2_process_close_request_fixed(struct smb2_context *smb2,
                             struct smb2_pdu *pdu);
int smb2_process_set_info_fixed(struct smb2_context *smb2,
                                struct smb2_pdu *pdu);
int smb2_process_set_info_request_fixed(struct smb2_context *smb2,
                                struct smb2_pdu *pdu);
int smb2_process_set_info_request_variable(struct smb2_context *smb2,
                                struct smb2_pdu *pdu);
int smb2_process_tree_disconnect_fixed(struct smb2_context *smb2,
                                       struct smb2_pdu *pdu);
int smb2_process_tree_disconnect_request_fixed(struct smb2_context *smb2,
                                       struct smb2_pdu *pdu);
int smb2_process_logoff_fixed(struct smb2_context *smb2,
                              struct smb2_pdu *pdu);
int smb2_process_logoff_request_fixed(struct smb2_context *smb2,
                              struct smb2_pdu *pdu);
int smb2_process_lock_fixed(struct smb2_context *smb2,
                            struct smb2_pdu *pdu);
int smb2_process_lock_request_fixed(struct smb2_context *smb2,
                            struct smb2_pdu *pdu);
int smb2_process_lock_request_variable(struct smb2_context *smb2,
                            struct smb2_pdu *pdu);
int smb2_process_oplock_break_fixed(struct smb2_context *smb2,
                             struct smb2_pdu *pdu);
int smb2_process_oplock_break_variable(struct smb2_context *smb2,
                             struct smb2_pdu *pdu);
int smb2_process_oplock_break_request_fixed(struct smb2_context *smb2,
                             struct smb2_pdu *pdu);
int smb2_process_oplock_break_request_variable(struct smb2_context *smb2,
                             struct smb2_pdu *pdu);
int smb2_process_echo_fixed(struct smb2_context *smb2,
                            struct smb2_pdu *pdu);
int smb2_process_echo_request_fixed(struct smb2_context *smb2,
                            struct smb2_pdu *pdu);
int smb2_process_flush_fixed(struct smb2_context *smb2,
                             struct smb2_pdu *pdu);
int smb2_process_flush_request_fixed(struct smb2_context *smb2,
                             struct smb2_pdu *pdu);
int smb2_process_read_fixed(struct smb2_context *smb2,
                            struct smb2_pdu *pdu);
int smb2_process_read_variable(struct smb2_context *smb2,
                            struct smb2_pdu *pdu);
int smb2_process_read_request_fixed(struct smb2_context *smb2,
                            struct smb2_pdu *pdu);
int smb2_process_read_request_variable(struct smb2_context *smb2,
                            struct smb2_pdu *pdu);
int smb2_process_write_fixed(struct smb2_context *smb2,
                             struct smb2_pdu *pdu);
int smb2_process_write_request_fixed(struct smb2_context *smb2,
                             struct smb2_pdu *pdu);
int smb2_process_write_request_variable(struct smb2_context *smb2,
                             struct smb2_pdu *pdu);
int smb2_process_ioctl_fixed(struct smb2_context *smb2,
                             struct smb2_pdu *pdu);
int smb2_process_ioctl_variable(struct smb2_context *smb2,
                                struct smb2_pdu *pdu);
int smb2_process_ioctl_request_fixed(struct smb2_context *smb2,
                             struct smb2_pdu *pdu);
int smb2_process_ioctl_request_variable(struct smb2_context *smb2,
                                struct smb2_pdu *pdu);

int smb2_decode_file_basic_info(struct smb2_context *smb2,
                                void *memctx,
                                struct smb2_file_basic_info *fs,
                                struct smb2_iovec *vec);
int smb2_encode_file_basic_info(struct smb2_context *smb2,
                                struct smb2_file_basic_info *fs,
                                struct smb2_iovec *vec);

int smb2_decode_file_standard_info(struct smb2_context *smb2,
                                   void *memctx,
                                   struct smb2_file_standard_info *fs,
                                   struct smb2_iovec *vec);

int smb2_encode_file_standard_info(struct smb2_context *smb2,
                                   struct smb2_file_standard_info *fs,
                                   struct smb2_iovec *vec);

int smb2_decode_file_stream_info(struct smb2_context *smb2,
                                   void *memctx,
                                   struct smb2_file_stream_info *fs,
                                   struct smb2_iovec *vec);

int smb2_encode_file_stream_info(struct smb2_context *smb2,
                                   struct smb2_file_stream_info *fs,
                                   struct smb2_iovec *vec);

int smb2_decode_file_position_info(struct smb2_context *smb2,
                                   void *memctx,
                                   struct smb2_file_position_info *fs,
                                   struct smb2_iovec *vec);

int smb2_encode_file_position_info(struct smb2_context *smb2,
                                   struct smb2_file_position_info *fs,
                                   struct smb2_iovec *vec);

int smb2_decode_file_all_info(struct smb2_context *smb2,
                              void *memctx,
                              struct smb2_file_all_info *fs,
                              struct smb2_iovec *vec);

int smb2_encode_file_all_info(struct smb2_context *smb2,
                              struct smb2_file_all_info *fs,
                              struct smb2_iovec *vec);

int smb2_decode_file_network_open_info(struct smb2_context *smb2,
                                       void *memctx,
                                       struct smb2_file_network_open_info *fs,
                                       struct smb2_iovec *vec);

int smb2_encode_file_network_open_info(struct smb2_context *smb2,
                                       struct smb2_file_network_open_info *fs,
                                       struct smb2_iovec *vec);

int smb2_decode_file_normalized_name_info(struct smb2_context *smb2,
                                          void *memctx,
                                          struct smb2_file_name_info *fs,
                                          struct smb2_iovec *vec);
int smb2_encode_file_normalized_name_info(struct smb2_context *smb2,
                                          struct smb2_file_name_info *fs,
                                          struct smb2_iovec *vec);
int smb2_decode_security_descriptor(struct smb2_context *smb2,
                                    void *memctx,
                                    struct smb2_security_descriptor *sd,
                                    struct smb2_iovec *vec);

int smb2_decode_file_fs_volume_info(struct smb2_context *smb2,
                                    void *memctx,
                                    struct smb2_file_fs_volume_info *fs,
                                    struct smb2_iovec *vec);
int smb2_encode_file_fs_volume_info(struct smb2_context *smb2,
                                    struct smb2_file_fs_volume_info *fs,
                                    struct smb2_iovec *vec);
int smb2_decode_file_fs_size_info(struct smb2_context *smb2,
                                  void *memctx,
                                  struct smb2_file_fs_size_info *fs,
                                  struct smb2_iovec *vec);
int smb2_encode_file_fs_size_info(struct smb2_context *smb2,
                                  struct smb2_file_fs_size_info *fs,
                                  struct smb2_iovec *vec);
int smb2_decode_file_fs_device_info(struct smb2_context *smb2,
                                    void *memctx,
                                    struct smb2_file_fs_device_info *fs,
                                    struct smb2_iovec *vec);
int smb2_encode_file_fs_device_info(struct smb2_context *smb2,
                                    struct smb2_file_fs_device_info *fs,
                                    struct smb2_iovec *vec);
int smb2_decode_file_fs_attribute_info(struct smb2_context *smb2,
                                  void *memctx,
                                  struct smb2_file_fs_attribute_info *fs,
                                  struct smb2_iovec *vec);
int smb2_encode_file_fs_attribute_info(struct smb2_context *smb2,
                                  struct smb2_file_fs_attribute_info *fs,
                                  struct smb2_iovec *vec);
int smb2_decode_file_fs_control_info(struct smb2_context *smb2,
                                     void *memctx,
                                     struct smb2_file_fs_control_info *fs,
                                     struct smb2_iovec *vec);
int smb2_encode_file_fs_control_info(struct smb2_context *smb2,
                                     struct smb2_file_fs_control_info *fs,
                                     struct smb2_iovec *vec);
int smb2_decode_file_fs_full_size_info(struct smb2_context *smb2,
                                       void *memctx,
                                       struct smb2_file_fs_full_size_info *fs,
                                       struct smb2_iovec *vec);
int smb2_encode_file_fs_full_size_info(struct smb2_context *smb2,
                                       struct smb2_file_fs_full_size_info *fs,
                                       struct smb2_iovec *vec);
int smb2_decode_file_fs_object_id_info(struct smb2_context *smb2,
                                     void *memctx,
                                     struct smb2_file_fs_object_id_info *fs,
                                     struct smb2_iovec *vec);
int smb2_encode_file_fs_object_id_info(struct smb2_context *smb2,
                                     struct smb2_file_fs_object_id_info *fs,
                                     struct smb2_iovec *vec);
int smb2_decode_file_fs_sector_size_info(struct smb2_context *smb2,
                                     void *memctx,
                                     struct smb2_file_fs_sector_size_info *fs,
                                     struct smb2_iovec *vec);
int smb2_encode_file_fs_sector_size_info(struct smb2_context *smb2,
                                     struct smb2_file_fs_sector_size_info *fs,
                                     struct smb2_iovec *vec);
int smb2_decode_reparse_data_buffer(struct smb2_context *smb2,
                                    void *memctx,
                                    struct smb2_reparse_data_buffer *rp,
                                    struct smb2_iovec *vec);

int smb2_read_from_buf(struct smb2_context *smb2);
void smb2_change_events(struct smb2_context *smb2, t_socket fd, int events);
void smb2_timeout_pdus(struct smb2_context *smb2);

struct dcerpc_context;
int dcerpc_set_uint8(struct dcerpc_context *ctx, struct smb2_iovec *iov,
                     int *offset, uint8_t value);

struct dcerpc_pdu;
int dcerpc_pdu_direction(struct dcerpc_pdu *pdu);

int dcerpc_align_3264(struct dcerpc_context *ctx, int offset);

struct connect_data;                                           /* defined in libsmb2.c */
void free_c_data(struct smb2_context*, struct connect_data*);  /* defined in libsmb2.c */

int smb2_write_to_socket(struct smb2_context *smb2);
        
#ifdef __cplusplus
}
#endif

#endif /* !_LIBSMB2_PRIVATE_H_ */
