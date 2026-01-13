/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2016 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   Portions of this code are copyright 2017 to Primary Data Inc.

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

#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif

#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_SYS_FCNTL_H
#include <sys/fcntl.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#if defined(_WIN32) || defined(_XBOX) || defined(__AROS__)
#include "asprintf.h"
#endif

#include "compat.h"

#include "sha.h"
#include "sha-private.h"

#include "slist.h"
#include "smb2.h"
#include "libsmb2.h"
#include "libsmb2-raw.h"
#include "libsmb2-private.h"
#include "smb2-signing.h"
#include "portable-endian.h"
#include "ntlmssp.h"

#ifdef HAVE_LIBKRB5
#include "krb5-wrapper.h"
#endif
#include "spnego-wrapper.h"

#if defined(ESP_PLATFORM)
#define DEFAULT_OUTPUT_BUFFER_LENGTH 512
#elif defined(__PS2__)
#define DEFAULT_OUTPUT_BUFFER_LENGTH 4096
#else
#define DEFAULT_OUTPUT_BUFFER_LENGTH 0xffff
#endif

/* strings used to derive SMB signing and encryption keys */
static const char SMBSigningKey[] = "SMBSigningKey";
static const char SMBC2SCipherKey[] = "SMBC2SCipherKey";
static const char SMBS2CCipherKey[] = "SMBS2CCipherKey";
static const char SMB2AESCMAC[] = "SMB2AESCMAC";
static const char SmbSign[] = "SmbSign";
static const char SMB2AESCCM[] = "SMB2AESCCM";
static const char ServerOut[] = "ServerOut";
static const char ServerIn[] = "ServerIn ";
/* The following strings will be used for deriving other keys */
#if 0
static const char SMB2APP[] = "SMB2APP";
static const char SmbRpc[] = "SmbRpc";
static const char SMBAppKey[] = "SMBAppKey";
#endif

const smb2_file_id compound_file_id = {
        0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff
};

struct connect_data {
        smb2_command_cb cb;
        void *cb_data;

        const char *server;
        const char *share;
        const char *user;

        /* UNC for the share in utf8 as well as utf16 formats */
        char *utf8_unc;
        struct smb2_utf16 *utf16_unc;

        void *auth_data;

        /* if context is being served by our server */
        struct smb2_server *server_context;
};

struct smb2fh {
        smb2_command_cb cb;
        void *cb_data;

        smb2_file_id file_id;
        int64_t offset;
        int64_t end_of_file;
};

void
smb2_close_context(struct smb2_context *smb2)
{
        if (smb2 == NULL) {
                return;
        }

        if (SMB2_VALID_SOCKET(smb2->fd)) {
                if (smb2->change_fd) {
                        smb2->change_fd(smb2, smb2->fd, SMB2_DEL_FD);
                }
                close(smb2->fd);
                smb2->fd = SMB2_INVALID_SOCKET;
        }

        smb2->message_id = 0;
        smb2->session_id = 0;
        smb2->tree_id_top = 0;
        smb2->tree_id_cur = 0;
        smb2->tree_id[0] = 0xdeadbeef;
        memset(smb2->signing_key, 0, SMB2_KEY_SIZE);
        if (smb2->session_key) {
                free(smb2->session_key);
                smb2->session_key = NULL;
        }
        smb2->session_key_size = 0;
}

static int
send_session_setup_request(struct smb2_context *smb2,
                           struct connect_data *c_data,
                           unsigned char *buf, int len);

static void
free_smb2dir(struct smb2_context *smb2, struct smb2dir *dir)
{
        while (dir->entries) {
                struct smb2_dirent_internal *e = dir->entries->next;

                free(discard_const(dir->entries->dirent.name));
                free(dir->entries);
                dir->entries = e;
        }
        if (dir->free_cb_data) {
                dir->free_cb_data(dir->cb_data);
        }
        free(dir);
}

void
smb2_seekdir(struct smb2_context *smb2, struct smb2dir *dir,
                  long loc)
{
        if (dir == NULL){
                return;
        }
        dir->current_entry = dir->entries;
        dir->index = 0;

        while (dir->current_entry && loc--) {
                dir->current_entry = dir->current_entry->next;
                dir->index++;
        }
}

long
smb2_telldir(struct smb2_context *smb2, struct smb2dir *dir)
{
        if (dir == NULL) {
                return -EINVAL;
        }
        return dir->index;
}

void
smb2_rewinddir(struct smb2_context *smb2,
                    struct smb2dir *dir)
{
        if (dir == NULL) {
                return;
        }
        dir->current_entry = dir->entries;
        dir->index = 0;
}

struct smb2dirent *
smb2_readdir(struct smb2_context *smb2,
             struct smb2dir *dir)
{
        struct smb2dirent *ent;
        if ((dir == NULL) || (dir->current_entry == NULL)) {
                return NULL;
        }

        ent = &dir->current_entry->dirent;
        dir->current_entry = dir->current_entry->next;
        dir->index++;

        return ent;
}

void
smb2_closedir(struct smb2_context *smb2, struct smb2dir *dir)
{
        if ((smb2 == NULL) || (dir == NULL)) {
                return;
        }
        free_smb2dir(smb2, dir);
}

static int
decode_dirents(struct smb2_context *smb2, struct smb2dir *dir,
               struct smb2_iovec *vec)
{
        struct smb2_dirent_internal *ent;
        struct smb2_fileidfulldirectoryinformation fs;
        uint32_t offset = 0;

        do {
                struct smb2_iovec tmp_vec _U_;

                /* Make sure we do not go beyond end of vector */
                if (offset >= vec->len) {
                        smb2_set_error(smb2, "Malformed query reply.");
                        return -1;
                }

                ent = calloc(1, sizeof(struct smb2_dirent_internal));
                if (ent == NULL) {
                        smb2_set_error(smb2, "Failed to allocate "
                                       "dirent_internal");
                        return -1;
                }
                SMB2_LIST_ADD(&dir->entries, ent);


                tmp_vec.buf = &vec->buf[offset];
                tmp_vec.len = vec->len - offset;

                smb2_decode_fileidfulldirectoryinformation(smb2, &fs,
                                                           &tmp_vec);
                /* steal the name */
                ent->dirent.name = fs.name;
                ent->dirent.st.smb2_type = SMB2_TYPE_FILE;
                if (fs.file_attributes & SMB2_FILE_ATTRIBUTE_DIRECTORY) {
                        ent->dirent.st.smb2_type = SMB2_TYPE_DIRECTORY;
                }
                if (fs.file_attributes & SMB2_FILE_ATTRIBUTE_REPARSE_POINT) {
                        ent->dirent.st.smb2_type = SMB2_TYPE_LINK;
                }
                ent->dirent.st.smb2_nlink = 0;
                ent->dirent.st.smb2_ino = fs.file_id;
                ent->dirent.st.smb2_size = fs.end_of_file;
                ent->dirent.st.smb2_atime = fs.last_access_time.tv_sec;
                ent->dirent.st.smb2_atime_nsec = fs.last_access_time.tv_usec * 1000;
                ent->dirent.st.smb2_mtime = fs.last_write_time.tv_sec;
                ent->dirent.st.smb2_mtime_nsec = fs.last_write_time.tv_usec * 1000;
                ent->dirent.st.smb2_ctime = fs.change_time.tv_sec;
                ent->dirent.st.smb2_ctime_nsec = fs.change_time.tv_usec * 1000;
                ent->dirent.st.smb2_btime = fs.creation_time.tv_sec;
                ent->dirent.st.smb2_btime_nsec = fs.creation_time.tv_usec * 1000;

                offset += fs.next_entry_offset;
        } while (fs.next_entry_offset);

        return 0;
}

static void
od_close_cb(struct smb2_context *smb2, int status,
         void *command_data, void *private_data)
{
        struct smb2dir *dir = private_data;

        if (status != SMB2_STATUS_SUCCESS) {
                dir->cb(smb2, -nterror_to_errno(status),
                        NULL, dir->cb_data);
                free_smb2dir(smb2, dir);
                return;
        }

        dir->current_entry = dir->entries;
        dir->index = 0;

        /* dir will be freed in smb2_closedir() */
        dir->cb(smb2, 0, dir, dir->cb_data);
}

static void
query_cb(struct smb2_context *smb2, int status,
         void *command_data, void *private_data)
{
        struct smb2dir *dir = private_data;
        struct smb2_query_directory_reply *rep = command_data;

        if (status == SMB2_STATUS_SUCCESS) {
                struct smb2_iovec vec _U_;
                struct smb2_query_directory_request req;
                struct smb2_pdu *pdu;

                vec.buf = rep->output_buffer;
                vec.len = rep->output_buffer_length;

                if (decode_dirents(smb2, dir, &vec) < 0) {
                        dir->cb(smb2, -ENOMEM, NULL, dir->cb_data);
                        free_smb2dir(smb2, dir);
                        return;
                }

                /* We need to get more data */
                memset(&req, 0, sizeof(struct smb2_query_directory_request));
                req.file_information_class = SMB2_FILE_ID_FULL_DIRECTORY_INFORMATION;
                req.flags = 0;
                memcpy(req.file_id, dir->file_id, SMB2_FD_SIZE);
                req.output_buffer_length = DEFAULT_OUTPUT_BUFFER_LENGTH;
                req.name = "*";

                pdu = smb2_cmd_query_directory_async(smb2, &req, query_cb, dir);
                if (pdu == NULL) {
                        dir->cb(smb2, -ENOMEM, NULL, dir->cb_data);
                        free_smb2dir(smb2, dir);
                        return;
                }
                smb2_queue_pdu(smb2, pdu);

                return;
        }

        if (status == SMB2_STATUS_NO_MORE_FILES) {
                struct smb2_close_request req;
                struct smb2_pdu *pdu;

                /* We have all the data */
                memset(&req, 0, sizeof(struct smb2_close_request));
                req.flags = SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB;
                memcpy(req.file_id, dir->file_id, SMB2_FD_SIZE);

                pdu = smb2_cmd_close_async(smb2, &req, od_close_cb, dir);
                if (pdu == NULL) {
                        dir->cb(smb2, -ENOMEM, NULL, dir->cb_data);
                        free_smb2dir(smb2, dir);
                        return;
                }
                smb2_queue_pdu(smb2, pdu);

                return;
        }

        smb2_set_nterror(smb2, status, "Query directory failed with (0x%08x) %s. %s",
                       status, nterror_to_str(status),
                       smb2_get_error(smb2));
        dir->cb(smb2, -nterror_to_errno(status), NULL, dir->cb_data);
        free_smb2dir(smb2, dir);
}

static void
opendir_cb(struct smb2_context *smb2, int status,
           void *command_data, void *private_data)
{
        struct smb2dir *dir = private_data;
        struct smb2_create_reply *rep = command_data;
        struct smb2_query_directory_request req;
        struct smb2_pdu *pdu;

        if (status != SMB2_STATUS_SUCCESS) {
                smb2_set_nterror(smb2, status, "Opendir failed with (0x%08x) %s.",
                               status, nterror_to_str(status));
                dir->cb(smb2, -nterror_to_errno(status), NULL, dir->cb_data);
                free_smb2dir(smb2, dir);
                return;
        }

        memcpy(dir->file_id, rep->file_id, SMB2_FD_SIZE);

        memset(&req, 0, sizeof(struct smb2_query_directory_request));
        req.file_information_class = SMB2_FILE_ID_FULL_DIRECTORY_INFORMATION;
        req.flags = 0;
        memcpy(req.file_id, dir->file_id, SMB2_FD_SIZE);
        req.output_buffer_length = DEFAULT_OUTPUT_BUFFER_LENGTH;
        req.name = "*";

        pdu = smb2_cmd_query_directory_async(smb2, &req, query_cb, dir);
        if (pdu == NULL) {
                smb2_set_error(smb2, "Failed to create query command.");
                dir->cb(smb2, -ENOMEM, NULL, dir->cb_data);
                free_smb2dir(smb2, dir);
                return;
        }
        smb2_queue_pdu(smb2, pdu);
}

static struct smb2_pdu *
_smb2_opendir_async(struct smb2_context *smb2, const char *path,
                    smb2_command_cb cb, void *cb_data, void (*free_cb)(void *),
                    int caller_frees_pdu)
{
        struct smb2_create_request req;
        struct smb2dir *dir;
        struct smb2_pdu *pdu;

        if (smb2 == NULL) {
                return NULL;
        }

        if (path == NULL) {
                path = "";
        }

        dir = calloc(1, sizeof(struct smb2dir));
        if (dir == NULL) {
                smb2_set_error(smb2, "Failed to allocate smb2dir.");
                return NULL;
        }
        dir->cb = cb;
        dir->cb_data = cb_data;

        memset(&req, 0, sizeof(struct smb2_create_request));
        req.requested_oplock_level = SMB2_OPLOCK_LEVEL_NONE;
        req.impersonation_level = SMB2_IMPERSONATION_IMPERSONATION;
        req.desired_access = SMB2_FILE_LIST_DIRECTORY | SMB2_FILE_READ_ATTRIBUTES;
        req.file_attributes = SMB2_FILE_ATTRIBUTE_DIRECTORY;
        req.share_access = SMB2_FILE_SHARE_READ | SMB2_FILE_SHARE_WRITE;
        req.create_disposition = SMB2_FILE_OPEN;
        req.create_options = SMB2_FILE_DIRECTORY_FILE;
        req.name = path;

        pdu = smb2_cmd_create_async(smb2, &req, opendir_cb, dir);
        if (pdu == NULL) {
                free_smb2dir(smb2, dir);
                smb2_set_error(smb2, "Failed to create opendir command.");
                return NULL;
        }
        pdu->free_cb = free_cb;
        pdu->caller_frees_pdu = caller_frees_pdu;
        smb2_queue_pdu(smb2, pdu);

        return pdu;
}

struct smb2_pdu *
smb2_opendir_async_pdu(struct smb2_context *smb2, const char *path,
                       smb2_command_cb cb, void *cb_data, void (*free_cb)(void *))
{
        struct smb2_pdu *pdu;

        pdu = _smb2_opendir_async(smb2, path, cb, cb_data, free_cb, 1);
        return pdu;
}

int
smb2_opendir_async(struct smb2_context *smb2, const char *path,
                   smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;

        pdu = _smb2_opendir_async(smb2, path, cb, cb_data, NULL, 0);
        return pdu ? 0 : -1;
}

extern void
free_c_data(struct smb2_context *smb2, struct connect_data *c_data)
{
        if (c_data->auth_data) {
                if (smb2->sec == SMB2_SEC_NTLMSSP) {
                        ntlmssp_destroy_context(c_data->auth_data);
                }
#ifdef HAVE_LIBKRB5
                else {
                        krb5_free_auth_data(c_data->auth_data);
                }
#endif
        }

        if (smb2->connect_data == c_data) {
            smb2->connect_data = NULL;  /* to prevent double-free in smb2_destroy_context */
        }
        free(c_data->utf8_unc);
        free(c_data->utf16_unc);
        free(discard_const(c_data->server));
        free(discard_const(c_data->share));
        free(discard_const(c_data->user));
        free(c_data);
}

static void
tree_connect_cb(struct smb2_context *smb2, int status,
                void *command_data, void *private_data)
{
        struct connect_data *c_data = private_data;

        if (status != SMB2_STATUS_SUCCESS) {
                smb2_close_context(smb2);
                smb2_set_nterror(smb2, status, "Tree Connect failed with (0x%08x) %s. %s",
                               status, nterror_to_str(status),
                               smb2_get_error(smb2));
                c_data->cb(smb2, -nterror_to_errno(status), NULL, c_data->cb_data);
                free_c_data(smb2, c_data);
                return;
        }

        c_data->cb(smb2, 0, NULL, c_data->cb_data);
        free_c_data(smb2, c_data);
}

void smb2_derive_key(
    uint8_t     *derivation_key,
    uint32_t    derivation_key_len,
    const char  *label,
    uint32_t    label_len,
    const char  *context,
    uint32_t    context_len,
    uint8_t     derived_key[SMB2_KEY_SIZE]
    )
{
        unsigned char nul = 0;
        const uint32_t counter = htobe32(1);
        const uint32_t keylen = htobe32(SMB2_KEY_SIZE * 8);
        uint8_t input_key[SMB2_KEY_SIZE] = {0};
        HMACContext ctx;
        uint8_t digest[USHAMaxHashSize];

        memcpy(input_key, derivation_key, MIN(sizeof(input_key),
                                              derivation_key_len));
        hmacReset(&ctx, SHA256, input_key, sizeof(input_key));
        hmacInput(&ctx, (unsigned char *)&counter, sizeof(counter));
        hmacInput(&ctx, (unsigned char *)label, label_len);
        hmacInput(&ctx, &nul, 1);
        hmacInput(&ctx, (unsigned char *)context, context_len);
        hmacInput(&ctx, (unsigned char *)&keylen, sizeof(keylen));
        hmacResult(&ctx, digest);
        memcpy(derived_key, digest, SMB2_KEY_SIZE);
}

/* MS-SMB2 3.2.5.2 */
static void
smb3_init_preauth_hash(struct smb2_context *smb2)
{
        memset(&smb2->preauthhash[0], 0, SMB2_PREAUTH_HASH_SIZE);
}

/* MS-SMB2 3.2.5.2 */
static int
smb3_update_preauth_hash(struct smb2_context *smb2, int niov,
                         struct smb2_iovec *iov)
{
        int i;
        USHAContext tctx;

        USHAReset(&tctx, SHA512);
        USHAInput(&tctx, smb2->preauthhash, SMB2_PREAUTH_HASH_SIZE);
        for (i = 0; i < niov; i++) {
                USHAInput(&tctx, iov[i].buf, iov[i].len);
        }
        USHAResult(&tctx, smb2->preauthhash);

        return 0;
}

static void smb2_create_signing_key(struct smb2_context *smb2)
{
        /* Derive the signing key from session key
         * This is based on negotiated protocol
         */
        if (smb2->dialect == SMB2_VERSION_0202 ||
            smb2->dialect == SMB2_VERSION_0210) {
                /* For SMB2 session key is the signing key */
                memcpy(smb2->signing_key,
                       smb2->session_key,
                       MIN(smb2->session_key_size, SMB2_KEY_SIZE));
        } else if (smb2->dialect <= SMB2_VERSION_0302) {
                smb2_derive_key(smb2->session_key,
                                smb2->session_key_size,
                                SMB2AESCMAC,
                                sizeof(SMB2AESCMAC),
                                SmbSign,
                                sizeof(SmbSign),
                                smb2->signing_key);
                smb2_derive_key(smb2->session_key,
                                smb2->session_key_size,
                                SMB2AESCCM,
                                sizeof(SMB2AESCCM),
                                ServerIn,
                                sizeof(ServerIn),
                                smb2->serverin_key);
                smb2_derive_key(smb2->session_key,
                                smb2->session_key_size,
                                SMB2AESCCM,
                                sizeof(SMB2AESCCM),
                                ServerOut,
                                sizeof(ServerOut),
                                smb2->serverout_key);
        } else if (smb2->dialect > SMB2_VERSION_0302) {
                smb2_derive_key(smb2->session_key,
                                smb2->session_key_size,
                                SMBSigningKey,
                                sizeof(SMBSigningKey),
                                (char *)smb2->preauthhash,
                                SMB2_PREAUTH_HASH_SIZE,
                                smb2->signing_key);
                smb2_derive_key(smb2->session_key,
                                smb2->session_key_size,
                                SMBC2SCipherKey,
                                sizeof(SMBC2SCipherKey),
                                (char *)smb2->preauthhash,
                                SMB2_PREAUTH_HASH_SIZE,
                                smb2->serverin_key);
                smb2_derive_key(smb2->session_key,
                                smb2->session_key_size,
                                SMBS2CCipherKey,
                                sizeof(SMBS2CCipherKey),
                                (char *)smb2->preauthhash,
                                SMB2_PREAUTH_HASH_SIZE,
                                smb2->serverout_key);
        }
}

static void
session_setup_cb(struct smb2_context *smb2, int status,
                 void *command_data, void *private_data)
{
        struct connect_data *c_data = private_data;
        struct smb2_session_setup_reply *rep = command_data;
        struct smb2_tree_connect_request req;
        struct smb2_pdu *pdu;
        int ret;

        if (status == SMB2_STATUS_MORE_PROCESSING_REQUIRED &&
            rep->security_buffer) {
                smb3_update_preauth_hash(smb2, smb2->in.niov - 1, &smb2->in.iov[1]);
                if ((ret = send_session_setup_request(
                                smb2, c_data, rep->security_buffer,
                                rep->security_buffer_length)) < 0) {
                        smb2_close_context(smb2);
                        c_data->cb(smb2, ret, NULL, c_data->cb_data);
                        free_c_data(smb2, c_data);
                        return;
                }
                return;
        } else if (status != SMB2_STATUS_SUCCESS) {
                smb2_close_context(smb2);
                smb2_set_nterror(smb2, status, "Session setup failed with (0x%08x) %s",
                                status, nterror_to_str(status));
                c_data->cb(smb2, -nterror_to_errno(status), NULL,
                                c_data->cb_data);
                free_c_data(smb2, c_data);
                return;
        }

        if (rep->session_flags & SMB2_SESSION_FLAG_IS_ENCRYPT_DATA) {
                smb2->seal = 1;
                smb2->sign = 0;
        }

#ifdef HAVE_LIBKRB5
       if (smb2->sec == SMB2_SEC_KRB5) {
                /* For NTLM the status will be
                 * SMB2_STATUS_MORE_PROCESSING_REQUIRED and a second call to
                 * gss_init_sec_context will complete the gss session.
                 * But for krb5 a second call to gss_init_sec_context is
                 * required if GSS_C_MUTUAL_FLAG is set.
                 *
                 * At this point SMB2 layer reported success already, so we
                 * ignore krb5 errors.
                 */
                krb5_session_request(smb2, c_data->auth_data,
                                         rep->security_buffer,
                                         rep->security_buffer_length);
       }
#endif


        if (smb2->sign || smb2->seal || smb2->dialect == SMB2_VERSION_0311) {
                uint8_t zero_key[SMB2_KEY_SIZE] = {0};
                int have_valid_session_key = 1;

                if (smb2->sec == SMB2_SEC_NTLMSSP) {
                        if (ntlmssp_get_session_key(c_data->auth_data,
                                                    &smb2->session_key,
                                                    &smb2->session_key_size) < 0) {
                                have_valid_session_key = 0;
                        }
                }
#ifdef HAVE_LIBKRB5
                else {
                        if (krb5_session_get_session_key(smb2, c_data->auth_data) < 0) {
                                have_valid_session_key = 0;
                        }
                }
#endif
                /* check if the session key is proper */
                if (smb2->session_key == NULL || memcmp(smb2->session_key, zero_key, SMB2_KEY_SIZE) == 0) {
                        have_valid_session_key = 0;
                }
                if (smb2->sign && have_valid_session_key == 0) {
                        smb2_close_context(smb2);
                        smb2_set_error(smb2, "Signing required by server. Session "
                                       "Key is not available %s",
                                       smb2_get_error(smb2));
                        c_data->cb(smb2, -EACCES, NULL, c_data->cb_data);
                        free_c_data(smb2, c_data);
                        return;
                }

                smb2_create_signing_key(smb2);

                if (smb2->hdr.flags & SMB2_FLAGS_SIGNED) {
                        uint8_t signature[16] _U_;

                        memcpy(&signature[0], &smb2->in.iov[1].buf[48], 16);
                        if (smb2_calc_signature(smb2, &smb2->in.iov[1].buf[48],
                                                &smb2->in.iov[1],
                                                smb2->in.niov - 1) < 0) {
                                c_data->cb(smb2, -EINVAL, NULL, c_data->cb_data);
                                free_c_data(smb2, c_data);
                                return;
                        }
                        if (memcmp(&signature[0], &smb2->in.iov[1].buf[48], 16)) {
                                smb2_set_error(smb2, "Wrong signature in received "
                                               "PDU");
                                c_data->cb(smb2, -EINVAL, NULL, c_data->cb_data);
                                free_c_data(smb2, c_data);
                                return;
                        }
                }
        }

        memset(&req, 0, sizeof(struct smb2_tree_connect_request));
        req.flags       = 0;
        req.path_length = 2 * c_data->utf16_unc->len;
        req.path        = c_data->utf16_unc->val;

        if (!smb2->passthrough) {
                pdu = smb2_cmd_tree_connect_async(smb2, &req, tree_connect_cb, c_data);
                if (pdu == NULL) {
                        smb2_close_context(smb2);
                        c_data->cb(smb2, -ENOMEM, NULL, c_data->cb_data);
                        free_c_data(smb2, c_data);
                        return;
                }
                smb2_queue_pdu(smb2, pdu);
        }
        else {
                /* if user wants raw data she probably doesnt want us to
                 * do an implicit tree-connect, so just end here
                 */
                c_data->cb(smb2, 0, NULL, c_data->cb_data);
                free_c_data(smb2, c_data);
        }
}

/* Returns 0 for success and -errno for failure */
static int
send_session_setup_request(struct smb2_context *smb2,
                           struct connect_data *c_data,
                           unsigned char *buf, int len)
{
        struct smb2_pdu *pdu;
        struct smb2_session_setup_request req;

        /* Session setup request. */
        memset(&req, 0, sizeof(struct smb2_session_setup_request));
        req.security_mode = (uint8_t)smb2->security_mode;

        if (smb2->sec == SMB2_SEC_NTLMSSP) {
                if (ntlmssp_generate_blob(NULL, smb2, time(NULL), c_data->auth_data,
                                          buf, len,
                                          &req.security_buffer,
                                          &req.security_buffer_length) < 0) {
                        smb2_close_context(smb2);
                        return -1;
                }
        }
#ifdef HAVE_LIBKRB5
        else {
                if (krb5_session_request(smb2, c_data->auth_data,
                                         buf, len) < 0) {
                        smb2_close_context(smb2);
                        return -1;
                }
                req.security_buffer_length =
                        krb5_get_output_token_length(c_data->auth_data);
                req.security_buffer =
                        krb5_get_output_token_buffer(c_data->auth_data);
        }
#endif

        pdu = smb2_cmd_session_setup_async(smb2, &req,
                                           session_setup_cb, c_data);
        if (pdu == NULL) {
                smb2_close_context(smb2);
                return -ENOMEM;
        }
        smb2_queue_pdu(smb2, pdu);
        smb3_update_preauth_hash(smb2, pdu->out.niov, &pdu->out.iov[0]);

        return 0;
}

static void
negotiate_cb(struct smb2_context *smb2, int status,
             void *command_data, void *private_data)
{
        struct connect_data *c_data = private_data;
        struct smb2_negotiate_reply *rep = command_data;
        uint32_t spnego_mechs;
        int ret;

        smb3_update_preauth_hash(smb2, smb2->in.niov - 1, &smb2->in.iov[1]);

        if (status != SMB2_STATUS_SUCCESS) {
                smb2_close_context(smb2);
                smb2_set_nterror(smb2, status, "Negotiate failed with (0x%08x) %s. %s",
                               status, nterror_to_str(status),
                               smb2_get_error(smb2));
                /* calls connect_cb */
                c_data->cb(smb2, -nterror_to_errno(status), NULL,
                           c_data->cb_data);
                free_c_data(smb2, c_data);
                return;
        }

        /* update the context with the server capabilities */
        if (rep->dialect_revision > SMB2_VERSION_0202) {
                if (rep->capabilities & SMB2_GLOBAL_CAP_LARGE_MTU) {
                        smb2->supports_multi_credit = 1;
                }
        }

        smb2->max_transact_size = rep->max_transact_size;
        smb2->max_read_size     = rep->max_read_size;
        smb2->max_write_size    = rep->max_write_size;
        smb2->dialect           = rep->dialect_revision;
        smb2->cypher            = rep->cypher;

        if (smb2->seal && (smb2->dialect == SMB2_VERSION_0300 ||
                           smb2->dialect == SMB2_VERSION_0302)) {
                if(!(rep->capabilities & SMB2_GLOBAL_CAP_ENCRYPTION)) {
                        smb2_set_error(smb2, "Encryption requested but server "
                                       "does not support encryption.");
                        smb2_close_context(smb2);
                        c_data->cb(smb2, -ENOMEM, NULL, c_data->cb_data);
                        free_c_data(smb2, c_data);
                        return;
                }
        }

        if (smb2->sign &&
            !(rep->security_mode & SMB2_NEGOTIATE_SIGNING_ENABLED)) {
                smb2_set_error(smb2, "Signing requested but server "
                               "does not support signing.");
                smb2_close_context(smb2);
                c_data->cb(smb2, -ENOMEM, NULL, c_data->cb_data);
                free_c_data(smb2, c_data);
                return;
        }

        if (rep->security_mode & SMB2_NEGOTIATE_SIGNING_REQUIRED) {
                smb2->sign = 1;
        }

        if (smb2->seal) {
                smb2->sign = 0;
        }

        /* if there is a gssapi blob in the reply, parse it to determine which
         * mechanisms are supported if caller hasn't explicitly set security
         */
        if (smb2->sec == SMB2_SEC_UNDEFINED &&
                        rep->security_buffer && rep->security_buffer_length) {
                spnego_mechs = 0;
                smb2_spnego_unwrap_gssapi(smb2, rep->security_buffer,
                        rep->security_buffer_length, 1,
                        NULL, &spnego_mechs);
#ifdef HAVE_LIBKRB5
                if (spnego_mechs & SPNEGO_MECHANISM_KRB5) {
                        smb2->sec = SMB2_SEC_KRB5;
                }
#endif
                if (smb2->sec == SMB2_SEC_UNDEFINED &&
                                (spnego_mechs & SPNEGO_MECHANISM_NTLMSSP)) {
                        smb2->sec = SMB2_SEC_NTLMSSP;
                }
        }
        if (smb2->sec == SMB2_SEC_UNDEFINED) {
#ifdef HAVE_LIBKRB5
                smb2->sec = SMB2_SEC_KRB5;
#else
                smb2->sec = SMB2_SEC_NTLMSSP;
#endif
        }

        if (smb2->sec == SMB2_SEC_NTLMSSP) {
                c_data->auth_data = ntlmssp_init_context(smb2->user,
                                                         smb2->password,
                                                         smb2->domain,
                                                         smb2->workstation,
                                                         smb2->client_challenge);
        }
#ifdef HAVE_LIBKRB5
        else {
                c_data->auth_data = krb5_negotiate_reply(smb2,
                                                         c_data->server,
                                                         smb2->domain,
                                                         c_data->user,
                                                         smb2->password);
        }
#endif
        if (c_data->auth_data == NULL) {
                smb2_close_context(smb2);
                c_data->cb(smb2, -ENOMEM, NULL, c_data->cb_data);
                free_c_data(smb2, c_data);
                return;
        }

        if ((ret = send_session_setup_request(smb2, c_data, NULL, 0)) < 0) {
                smb2_close_context(smb2);
                c_data->cb(smb2, ret, NULL, c_data->cb_data);
                free_c_data(smb2, c_data);
                return;
        }
}

static void
connect_cb(struct smb2_context *smb2, int status,
           void *command_data _U_, void *private_data)
{
        struct connect_data *c_data = private_data;
        struct smb2_negotiate_request req;
        struct smb2_pdu *pdu;

        if (status != 0) {
                smb2_set_error(smb2, "Socket connect failed with %d",
                               status);
                c_data->cb(smb2, -status, NULL, c_data->cb_data);
                free_c_data(smb2, c_data);
                return;
        }

        memset(&req, 0, sizeof(struct smb2_negotiate_request));
        req.capabilities = SMB2_GLOBAL_CAP_LARGE_MTU;
        if (smb2->version == SMB2_VERSION_ANY  ||
            smb2->version == SMB2_VERSION_ANY3 ||
            smb2->version == SMB2_VERSION_0300 ||
            smb2->version == SMB2_VERSION_0302 ||
            smb2->version == SMB2_VERSION_0311) {
                req.capabilities |= SMB2_GLOBAL_CAP_ENCRYPTION;
        }
        req.security_mode = smb2->security_mode;
        switch (smb2->version) {
        case SMB2_VERSION_ANY:
                req.dialect_count = 5;
                req.dialects[0] = SMB2_VERSION_0202;
                req.dialects[1] = SMB2_VERSION_0210;
                req.dialects[2] = SMB2_VERSION_0300;
                req.dialects[3] = SMB2_VERSION_0302;
                req.dialects[4] = SMB2_VERSION_0311;
                break;
        case SMB2_VERSION_ANY2:
                req.dialect_count = 2;
                req.dialects[0] = SMB2_VERSION_0202;
                req.dialects[1] = SMB2_VERSION_0210;
                break;
        case SMB2_VERSION_ANY3:
                req.dialect_count = 3;
                req.dialects[0] = SMB2_VERSION_0300;
                req.dialects[1] = SMB2_VERSION_0302;
                req.dialects[2] = SMB2_VERSION_0311;
                break;
        case SMB2_VERSION_0202:
        case SMB2_VERSION_0210:
        case SMB2_VERSION_0300:
        case SMB2_VERSION_0302:
        case SMB2_VERSION_0311:
                req.dialect_count = 1;
                req.dialects[0] = smb2->version;
                break;
        }

        memcpy(req.client_guid, smb2_get_client_guid(smb2), SMB2_GUID_SIZE);

        smb3_init_preauth_hash(smb2);
        pdu = smb2_cmd_negotiate_async(smb2, &req, negotiate_cb, c_data);
        if (pdu == NULL) {
                c_data->cb(smb2, -ENOMEM, NULL, c_data->cb_data);
                free_c_data(smb2, c_data);
                return;
        }
        smb2_queue_pdu(smb2, pdu);
        smb3_update_preauth_hash(smb2, pdu->out.niov, &pdu->out.iov[0]);
}

int
smb2_connect_share_async(struct smb2_context *smb2,
                         const char *server,
                         const char *share, const char *user,
                         smb2_command_cb cb, void *cb_data)
{
        struct connect_data *c_data;
        int err;

        if (smb2 == NULL) {
                return -EINVAL;
        }

        if (smb2->server != NULL) {
                free(discard_const(smb2->server));
                smb2->server = NULL;
        }
        if (server == NULL) {
                smb2_set_error(smb2, "No server name provided");
                return -EINVAL;
        }
        if (share == NULL) {
                smb2_set_error(smb2, "No share name provided");
                return -EINVAL;
        }
        smb2->server = strdup(server);
        if (smb2->server == NULL) {
                return -ENOMEM;
        }

        if (smb2->share) {
                free(discard_const(smb2->share));
        }
        smb2->share = strdup(share);
        if (smb2->share == NULL) {
                return -ENOMEM;
        }

        if (user) {
                smb2_set_user(smb2, user);
        }
        c_data = calloc(1, sizeof(struct connect_data));
        if (c_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate connect_data");
                return -ENOMEM;
        }
        c_data->server = strdup(smb2->server);
        if (c_data->server == NULL) {
                free_c_data(smb2, c_data);
                smb2_set_error(smb2, "Failed to strdup(server)");
                return -ENOMEM;
        }
        c_data->share = strdup(smb2->share);
        if (c_data->share == NULL) {
                free_c_data(smb2, c_data);
                smb2_set_error(smb2, "Failed to strdup(share)");
                return -ENOMEM;
        }
        if (smb2->user == NULL) {
                smb2_set_error(smb2, "smb2->user is NULL");
                return -ENOMEM;
        }
        c_data->user = strdup(smb2->user);
        if (c_data->user == NULL) {
                free_c_data(smb2, c_data);
                smb2_set_error(smb2, "Failed to strdup(user)");
                return -ENOMEM;
        }
        if (asprintf(&c_data->utf8_unc, "\\\\%s\\%s", c_data->server,
                     c_data->share) < 0) {
                free_c_data(smb2, c_data);
                smb2_set_error(smb2, "Failed to allocate unc string.");
                return -ENOMEM;
        }

        c_data->utf16_unc = smb2_utf8_to_utf16(c_data->utf8_unc);
        if (c_data->utf16_unc == NULL) {
                smb2_set_error(smb2, "Count not convert UNC:[%s] into UTF-16",
                               c_data->utf8_unc);
                free_c_data(smb2, c_data);
                return -ENOMEM;
        }

        c_data->cb = cb;
        c_data->cb_data = cb_data;

        err = smb2_connect_async(smb2, server, connect_cb, c_data);
        if (err != 0) {
                free_c_data(smb2, c_data);
                return err;
        }

        return 0;
}

static void
free_smb2fh(struct smb2_context *smb2, struct smb2fh *fh)
{
        free(fh);
}

static void
open_cb(struct smb2_context *smb2, int status,
        void *command_data, void *private_data)
{
        struct smb2fh *fh = private_data;
        struct smb2_create_reply *rep = command_data;

        if (status != SMB2_STATUS_SUCCESS) {
                smb2_set_nterror(smb2, status, "Open failed with (0x%08x) %s.",
                               status, nterror_to_str(status));
                fh->cb(smb2, -nterror_to_errno(status), NULL, fh->cb_data);
                free_smb2fh(smb2, fh);
                return;
        }

        memcpy(fh->file_id, rep->file_id, SMB2_FD_SIZE);
        fh->end_of_file = rep->end_of_file;
        fh->cb(smb2, 0, fh, fh->cb_data);
}

static struct smb2_pdu *
_smb2_open_async_with_oplock_or_lease(struct smb2_context *smb2, const char *path, int flags,
                uint8_t oplock_level, uint32_t lease_state, smb2_lease_key lease_key,
                smb2_command_cb cb, void *cb_data, void (*free_cb)(void *),
                int caller_frees_pdu)
{
        struct smb2fh *fh;
        struct smb2_create_request req;
        struct smb2_pdu *pdu;
        struct smb2_iovec iov;
        uint32_t desired_access = 0;
        uint32_t create_disposition = 0;
        uint32_t create_options = 0;
        uint32_t file_attributes = 0;

        if (smb2 == NULL) {
                return NULL;
        }

        fh = calloc(1, sizeof(struct smb2fh));
        if (fh == NULL) {
                smb2_set_error(smb2, "Failed to allocate smbfh");
                return NULL;
        }

        fh->cb = cb;
        fh->cb_data = cb_data;

        /* Create disposition */
        if (flags & O_CREAT) {
                if (flags & O_EXCL) {
                        create_disposition = SMB2_FILE_CREATE;
                } else if(flags & O_TRUNC) {
                        create_disposition = SMB2_FILE_OVERWRITE_IF;
                } else {
                        create_disposition = SMB2_FILE_OPEN_IF;
                }
        } else {
                if (flags & O_TRUNC) {
                        create_disposition = SMB2_FILE_OVERWRITE;
                } else {
                        create_disposition = SMB2_FILE_OPEN;
                }
        }

        /* desired access */
        switch (flags & O_ACCMODE) {
                case O_RDWR:
                case O_WRONLY:
                        desired_access |= SMB2_FILE_WRITE_DATA |
                                SMB2_FILE_WRITE_EA |
                                SMB2_FILE_WRITE_ATTRIBUTES;
                        if ((flags & O_ACCMODE) == O_WRONLY)
                                break;
                case O_RDONLY:
                        desired_access |= SMB2_FILE_READ_DATA |
                                SMB2_FILE_READ_EA |
                                SMB2_FILE_READ_ATTRIBUTES;
                        break;
        }

#ifdef O_DIRECTORY
        if (flags & O_DIRECTORY) {
                /* must be directory */
                create_options |= SMB2_FILE_DIRECTORY_FILE;
        } else {
                /* must not be directory */
                create_options |= SMB2_FILE_NON_DIRECTORY_FILE;
        }
#else
        create_options |= SMB2_FILE_NON_DIRECTORY_FILE;
#endif

        if (flags & O_SYNC) {
                desired_access |= SMB2_SYNCHRONIZE;
                create_options |= SMB2_FILE_NO_INTERMEDIATE_BUFFERING;
        }

        memset(&req, 0, sizeof(struct smb2_create_request));
        req.requested_oplock_level = oplock_level;
        req.impersonation_level = SMB2_IMPERSONATION_IMPERSONATION;
        req.desired_access = desired_access;
        req.file_attributes = file_attributes;
        req.share_access = SMB2_FILE_SHARE_READ | SMB2_FILE_SHARE_WRITE;
        req.create_disposition = create_disposition;
        req.create_options = create_options;
        req.name = path;

        if (lease_state && lease_key) {
                req.create_context_length = SMB2_CREATE_REQUEST_LEASE_SIZE + 24;
                req.create_context = calloc(1, SMB2_CREATE_REQUEST_LEASE_SIZE + 24);
                iov.buf = req.create_context;
                iov.len = req.create_context_length;
                smb2_set_uint32(&iov, 0, 0);    /* chain offset */
                smb2_set_uint16(&iov, 4, 16);   /* tag offset */
                smb2_set_uint16(&iov, 6, 4);    /* tag length lo */
                smb2_set_uint16(&iov, 8, 0);    /* tag length up */
                smb2_set_uint16(&iov, 10, 24);  /* data offset */
                smb2_set_uint16(&iov, 12, SMB2_CREATE_REQUEST_LEASE_SIZE);
                smb2_set_uint32(&iov, 16, htobe32(0x52714c73));
                memcpy(iov.buf + 24, lease_key, SMB2_LEASE_KEY_SIZE);
                smb2_set_uint32(&iov, 40, lease_state);
        }

        pdu = smb2_cmd_create_async(smb2, &req, open_cb, fh);
        if (pdu == NULL) {
                smb2_set_error(smb2, "Failed to create create command");
                free_smb2fh(smb2, fh);
                return NULL;
        }
        if (req.create_context && req.create_context_length) {
                free(req.create_context);
        }

        pdu->caller_frees_pdu = caller_frees_pdu;
        smb2_queue_pdu(smb2, pdu);

        return pdu;
}

struct smb2_pdu *
smb2_open_async_pdu(struct smb2_context *smb2, const char *path, int flags,
                    smb2_command_cb cb, void *cb_data, void (*free_cb)(void *))
{
        return _smb2_open_async_with_oplock_or_lease(smb2, path, flags,
                SMB2_OPLOCK_LEVEL_NONE, 0, NULL,
                cb, cb_data, free_cb, 1);
}
        
int
smb2_open_async_with_oplock_or_lease(struct smb2_context *smb2, const char *path, int flags,
                uint8_t oplock_level, uint32_t lease_state, smb2_lease_key lease_key,
                smb2_command_cb cb, void *cb_data)
        
{
        struct smb2_pdu *pdu;
        
        pdu = _smb2_open_async_with_oplock_or_lease(smb2, path, flags,
                oplock_level, lease_state, lease_key,
                cb, cb_data, NULL, 0);
        return pdu ? 0 : -1;
}

int
smb2_open_async(struct smb2_context *smb2, const char *path, int flags,
                smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;
        
        pdu = _smb2_open_async_with_oplock_or_lease(smb2, path, flags,
                SMB2_OPLOCK_LEVEL_NONE, 0, NULL,
                cb, cb_data, NULL, 0);
        return pdu ? 0 : -1;
}

static void
close_cb(struct smb2_context *smb2, int status,
         void *command_data, void *private_data)
{
        struct smb2fh *fh = private_data;

        if (status != SMB2_STATUS_SUCCESS) {
                smb2_set_nterror(smb2, status, "Close failed with (0x%08x) %s",
                               status, nterror_to_str(status));
                fh->cb(smb2, -nterror_to_errno(status), NULL, fh->cb_data);
                free_smb2fh(smb2, fh);
                return;
        }

        fh->cb(smb2, 0, NULL, fh->cb_data);
        free_smb2fh(smb2, fh);
}

int
smb2_close_async(struct smb2_context *smb2, struct smb2fh *fh,
                 smb2_command_cb cb, void *cb_data)
{
        struct smb2_close_request req;
        struct smb2_pdu *pdu;

        if (smb2 == NULL) {
            return -EINVAL;
        }
        if (fh == NULL) {
            smb2_set_error(smb2, "File handle was NULL");
            return -EINVAL;
        }

        fh->cb = cb;
        fh->cb_data = cb_data;

        memset(&req, 0, sizeof(struct smb2_close_request));
        req.flags = SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB;
        memcpy(req.file_id, fh->file_id, SMB2_FD_SIZE);

        pdu = smb2_cmd_close_async(smb2, &req, close_cb, fh);
        if (pdu == NULL) {
                smb2_set_error(smb2, "Failed to create close command");
                return -ENOMEM;
        }
        smb2_queue_pdu(smb2, pdu);

        return 0;
}

static void
fsync_cb(struct smb2_context *smb2, int status,
         void *command_data, void *private_data)
{
        struct smb2fh *fh = private_data;

        if (status != SMB2_STATUS_SUCCESS) {
                smb2_set_nterror(smb2, status, "Flush failed with (0x%08x) %s",
                               status, nterror_to_str(status));
                fh->cb(smb2, -nterror_to_errno(status), NULL, fh->cb_data);
                return;
        }

        fh->cb(smb2, 0, NULL, fh->cb_data);
}

int
smb2_fsync_async(struct smb2_context *smb2, struct smb2fh *fh,
                 smb2_command_cb cb, void *cb_data)
{
        struct smb2_flush_request req;
        struct smb2_pdu *pdu;

        if (smb2 == NULL) {
            return -EINVAL;
        }
        if (fh == NULL) {
            smb2_set_error(smb2, "File handle was NULL");
            return -EINVAL;
        }

        fh->cb = cb;
        fh->cb_data = cb_data;

        memset(&req, 0, sizeof(struct smb2_flush_request));
        memcpy(req.file_id, fh->file_id, SMB2_FD_SIZE);

        pdu = smb2_cmd_flush_async(smb2, &req, fsync_cb, fh);
        if (pdu == NULL) {
                smb2_set_error(smb2, "Failed to create flush command");
                return -ENOMEM;
        }
        smb2_queue_pdu(smb2, pdu);

        return 0;
}

struct read_data {
        smb2_command_cb cb;
        void *cb_data;

        struct smb2_read_cb_data read_cb_data;
};

static void
read_cb(struct smb2_context *smb2, int status,
      void *command_data, void *private_data)
{
        struct read_data *rd = private_data;
        struct smb2_read_reply *rep = command_data;

        if (status && status != SMB2_STATUS_END_OF_FILE) {
                smb2_set_nterror(smb2, status, "Read/Write failed with (0x%08x) %s",
                               status, nterror_to_str(status));
                rd->cb(smb2, -nterror_to_errno(status), &rd->read_cb_data, rd->cb_data);
                free(rd);
                return;
        }

        if (status == SMB2_STATUS_SUCCESS) {
                rd->read_cb_data.fh->offset = rd->read_cb_data.offset + rep->data_length;
        }

        rd->cb(smb2, rep->data_length, &rd->read_cb_data, rd->cb_data);
        free(rd);
}

int
smb2_pread_async(struct smb2_context *smb2, struct smb2fh *fh,
                 uint8_t *buf, uint32_t count, uint64_t offset,
                 smb2_command_cb cb, void *cb_data)
{
        struct smb2_read_request req;
        struct read_data *rd;
        struct smb2_pdu *pdu;
        int needed_credits;

        if (smb2 == NULL) {
                return -EINVAL;
        }
        if (fh == NULL) {
                smb2_set_error(smb2, "File handle was NULL");
                return -EINVAL;
        }

        rd = calloc(1, sizeof(struct read_data));
        if (rd == NULL) {
                smb2_set_error(smb2, "Failed to allocate read_data");
                return -ENOMEM;
        }

        rd->cb = cb;
        rd->cb_data = cb_data;
        rd->read_cb_data.fh = fh;
        rd->read_cb_data.buf = buf;
        rd->read_cb_data.count = count;
        rd->read_cb_data.offset = offset;

        if (count > smb2->max_read_size) {
                count = smb2->max_read_size;
        }
        needed_credits = (count - 1) / 65536 + 1;

        if (smb2->dialect > SMB2_VERSION_0202) {
                if (needed_credits > MAX_CREDITS - 16) {
                        count =  (MAX_CREDITS - 16) * 65536;
                }
                needed_credits = (count - 1) / 65536 + 1;
                if (needed_credits > smb2->credits) {
                        count = smb2->credits * 65536;
                }
        } else {
                if (count > 65536) {
                        count = 65536;
                }
        }
        needed_credits = (count - 1) / 65536 + 1;

        memset(&req, 0, sizeof(struct smb2_read_request));
        req.flags = 0;
        req.length = count;
        req.offset = offset;
        req.buf = buf;
        memcpy(req.file_id, fh->file_id, SMB2_FD_SIZE);
        req.minimum_count = 0;
        req.channel = SMB2_CHANNEL_NONE;
        req.remaining_bytes = 0;

        pdu = smb2_cmd_read_async(smb2, &req, read_cb, rd);
        if (pdu == NULL) {
                smb2_set_error(smb2, "Failed to create read command");
                return -EINVAL;
        }

        smb2_queue_pdu(smb2, pdu);

        return 0;
}

int
smb2_read_async(struct smb2_context *smb2, struct smb2fh *fh,
                uint8_t *buf, uint32_t count,
                smb2_command_cb cb, void *cb_data)
{
        if (smb2 == NULL) {
                return -EINVAL;
        }
        if (fh == NULL) {
                smb2_set_error(smb2, "File handle was NULL");
                return -EINVAL;
        }

        return smb2_pread_async(smb2, fh, buf, count, fh->offset,
                                cb, cb_data);
}

struct write_data {
        smb2_command_cb cb;
        void *cb_data;

        struct smb2_write_cb_data write_cb_data;
};

static void
write_cb(struct smb2_context *smb2, int status,
      void *command_data, void *private_data)
{
        struct write_data *wd = private_data;
        struct smb2_write_reply *rep = command_data;

        if (status && status != SMB2_STATUS_END_OF_FILE) {
                smb2_set_nterror(smb2, status, "Read/Write failed with (0x%08x) %s",
                               status, nterror_to_str(status));
                wd->cb(smb2, -nterror_to_errno(status), &wd->write_cb_data, wd->cb_data);
                free(wd);
                return;
        }

        if (status == SMB2_STATUS_SUCCESS) {
                wd->write_cb_data.fh->offset = wd->write_cb_data.offset + rep->count;
        }

        wd->cb(smb2, rep->count, &wd->write_cb_data, wd->cb_data);
        free(wd);
}

int
smb2_pwrite_async(struct smb2_context *smb2, struct smb2fh *fh,
                  const uint8_t *buf, uint32_t count, uint64_t offset,
                  smb2_command_cb cb, void *cb_data)
{
        struct smb2_write_request req;
        struct write_data *wr;
        struct smb2_pdu *pdu;
        int needed_credits;

        if (smb2 == NULL) {
                return -EINVAL;
        }
        if (fh == NULL) {
                smb2_set_error(smb2, "File handle was NULL");
                return -EINVAL;
        }

        wr = calloc(1, sizeof(struct write_data));
        if (wr == NULL) {
                smb2_set_error(smb2, "Failed to allocate write_data");
                return -ENOMEM;
        }

        wr->cb = cb;
        wr->cb_data = cb_data;
        wr->write_cb_data.fh = fh;
        wr->write_cb_data.buf = buf;
        wr->write_cb_data.count = count;
        wr->write_cb_data.offset = offset;

        if (count > smb2->max_write_size) {
                count = smb2->max_write_size;
        }
        needed_credits = (count - 1) / 65536 + 1;

        if (smb2->dialect > SMB2_VERSION_0202) {
                if (needed_credits > MAX_CREDITS - 16) {
                        count =  (MAX_CREDITS - 16) * 65536;
                }
                needed_credits = (count - 1) / 65536 + 1;
                if (needed_credits > smb2->credits) {
                        count = smb2->credits * 65536;
                }
        } else {
                if (count > 65536) {
                        count = 65536;
                }
        }
        needed_credits = (count - 1) / 65536 + 1;

        memset(&req, 0, sizeof(struct smb2_write_request));
        req.length = count;
        req.offset = offset;
        req.buf = buf;
        memcpy(req.file_id, fh->file_id, SMB2_FD_SIZE);
        req.channel = SMB2_CHANNEL_NONE;
        req.remaining_bytes = 0;
        req.flags = 0;

        pdu = smb2_cmd_write_async(smb2, &req, 0, write_cb, wr);
        if (pdu == NULL) {
                smb2_set_error(smb2, "Failed to create write command");
                return -EINVAL;
        }
        smb2_queue_pdu(smb2, pdu);

        return 0;
}

int
smb2_write_async(struct smb2_context *smb2, struct smb2fh *fh,
                 const uint8_t *buf, uint32_t count,
                 smb2_command_cb cb, void *cb_data)
{
        if (smb2 == NULL) {
                return -EINVAL;
        }
        if (fh == NULL) {
                smb2_set_error(smb2, "File handle was NULL");
                return -EINVAL;
        }
        return smb2_pwrite_async(smb2, fh, buf, count, fh->offset,
                                 cb, cb_data);
}

int64_t
smb2_lseek(struct smb2_context *smb2, struct smb2fh *fh,
           int64_t offset, int whence, uint64_t *current_offset)
{
        if (smb2 == NULL) {
                return -EINVAL;
        }
        if (fh == NULL) {
                smb2_set_error(smb2, "File handle was NULL");
                return -EINVAL;
        }

        switch(whence) {
        case SEEK_SET:
                if (offset < 0) {
                        smb2_set_error(smb2, "Lseek() offset would become"
                                        "negative");
                        return -EINVAL;
                }
                fh->offset = offset;
                if (current_offset) {
                        *current_offset = fh->offset;
                }
                return fh->offset;
        case SEEK_CUR:
                if (fh->offset + offset < 0) {
                        smb2_set_error(smb2, "Lseek() offset would become"
                                        "negative");
                        return -EINVAL;
                }
                fh->offset += offset;
                if (current_offset) {
                        *current_offset = fh->offset;
                }
                return fh->offset;
        case SEEK_END:
                fh->offset = fh->end_of_file;
                if (fh->offset + offset < 0) {
                        smb2_set_error(smb2, "Lseek() offset would become"
                                        "negative");
                        return -EINVAL;
                }
                fh->offset += offset;
                if (current_offset) {
                        *current_offset = fh->offset;
                }
                return fh->offset;
        default:
                smb2_set_error(smb2, "Invalid whence(%d) for lseek",
                                    whence);
                return -EINVAL;
        }
}

struct create_cb_data {
        smb2_command_cb cb;
        void *cb_data;
};

static void
create_cb_2(struct smb2_context *smb2, int status,
            void *command_data, void *private_data)
{
        struct create_cb_data *create_data = private_data;

        if (status != SMB2_STATUS_SUCCESS) {
                status = -nterror_to_errno(status);
        }

        create_data->cb(smb2, status, NULL, create_data->cb_data);
        free(create_data);
}

static void
create_cb_1(struct smb2_context *smb2, int status,
            void *command_data, void *private_data)
{
        if (status != SMB2_STATUS_SUCCESS) {
                smb2_set_error(smb2, "Create failed with status %s.", nterror_to_str(status));
                return;
        }
}

static int
smb2_unlink_internal(struct smb2_context *smb2, const char *path,
                     int is_dir,
                     smb2_command_cb cb, void *cb_data)
{
        struct create_cb_data *create_data;
        struct smb2_create_request cr_req;
        struct smb2_close_request cl_req;
        struct smb2_pdu *pdu, *next_pdu;

        if (smb2 == NULL) {
                return -EINVAL;
        }

        create_data = calloc(1, sizeof(struct create_cb_data));
        if (create_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate create_data");
                return -ENOMEM;
        }

        create_data->cb = cb;
        create_data->cb_data = cb_data;


        memset(&cr_req, 0, sizeof(struct smb2_create_request));
        cr_req.requested_oplock_level = SMB2_OPLOCK_LEVEL_NONE;
        cr_req.impersonation_level = SMB2_IMPERSONATION_IMPERSONATION;
        cr_req.desired_access = SMB2_DELETE;
        if (is_dir) {
                cr_req.file_attributes = SMB2_FILE_ATTRIBUTE_DIRECTORY;
        } else {
                cr_req.file_attributes = SMB2_FILE_ATTRIBUTE_NORMAL;
        }
        cr_req.share_access = SMB2_FILE_SHARE_READ | SMB2_FILE_SHARE_WRITE |
                SMB2_FILE_SHARE_DELETE;
        cr_req.create_disposition = SMB2_FILE_OPEN;
        cr_req.create_options = SMB2_FILE_DELETE_ON_CLOSE;
        cr_req.name = path;

        pdu = smb2_cmd_create_async(smb2, &cr_req, create_cb_1, create_data);
        if (pdu == NULL) {
                smb2_set_error(smb2, "Failed to create create command");
                return -ENOMEM;
        }

        memset(&cl_req, 0, sizeof(struct smb2_close_request));
        cl_req.flags = SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB;
        memcpy(cl_req.file_id, compound_file_id, SMB2_FD_SIZE);

        next_pdu = smb2_cmd_close_async(smb2, &cl_req, create_cb_2, create_data);
        if (next_pdu == NULL) {
                smb2_set_error(smb2, "Failed to create close command");
                smb2_free_pdu(smb2, pdu);
                free(create_data);
                return -ENOMEM;
        }
        smb2_add_compound_pdu(smb2, pdu, next_pdu);

        smb2_queue_pdu(smb2, pdu);

        return 0;
}

int
smb2_unlink_async(struct smb2_context *smb2, const char *path,
                  smb2_command_cb cb, void *cb_data)
{
        return smb2_unlink_internal(smb2, path, 0, cb, cb_data);
}

int
smb2_rmdir_async(struct smb2_context *smb2, const char *path,
                 smb2_command_cb cb, void *cb_data)
{
        return smb2_unlink_internal(smb2, path, 1, cb, cb_data);
}

int
smb2_mkdir_async(struct smb2_context *smb2, const char *path,
                 smb2_command_cb cb, void *cb_data)
{
        struct create_cb_data *create_data;
        struct smb2_create_request cr_req;
        struct smb2_close_request cl_req;
        struct smb2_pdu *pdu, *next_pdu;

        if (smb2 == NULL) {
                return -EINVAL;
        }

        create_data = calloc(1, sizeof(struct create_cb_data));
        if (create_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate create_data");
                return -ENOMEM;
        }

        create_data->cb = cb;
        create_data->cb_data = cb_data;

        memset(&cr_req, 0, sizeof(struct smb2_create_request));
        cr_req.requested_oplock_level = SMB2_OPLOCK_LEVEL_NONE;
        cr_req.impersonation_level = SMB2_IMPERSONATION_IMPERSONATION;
        cr_req.desired_access = SMB2_FILE_READ_ATTRIBUTES;
        cr_req.file_attributes = SMB2_FILE_ATTRIBUTE_DIRECTORY;
        cr_req.share_access = SMB2_FILE_SHARE_READ | SMB2_FILE_SHARE_WRITE;
        cr_req.create_disposition = SMB2_FILE_CREATE;
        cr_req.create_options = SMB2_FILE_DIRECTORY_FILE;
        cr_req.name = path;

        pdu = smb2_cmd_create_async(smb2, &cr_req, create_cb_1, create_data);
        if (pdu == NULL) {
                smb2_set_error(smb2, "Failed to create create command");
                return -ENOMEM;
        }

        memset(&cl_req, 0, sizeof(struct smb2_close_request));
        cl_req.flags = SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB;
        memcpy(cl_req.file_id, compound_file_id, SMB2_FD_SIZE);

        next_pdu = smb2_cmd_close_async(smb2, &cl_req, create_cb_2, create_data);
        if (next_pdu == NULL) {
                smb2_set_error(smb2, "Failed to create close command");
                smb2_free_pdu(smb2, pdu);
                free(create_data);
                return -ENOMEM;
        }
        smb2_add_compound_pdu(smb2, pdu, next_pdu);

        smb2_queue_pdu(smb2, pdu);

        return 0;
}

struct stat_cb_data {
        smb2_command_cb cb;
        void *cb_data;

        uint32_t status;
        uint8_t info_type;
        uint8_t file_info_class;
        void *st;
};

static void
fstat_cb_1(struct smb2_context *smb2, int status,
           void *command_data, void *private_data)
{
        struct stat_cb_data *stat_data = private_data;
        struct smb2_query_info_reply *rep = command_data;
        struct smb2_file_all_info *fs = rep->output_buffer;
        struct smb2_stat_64 *st = stat_data->st;

        if (status != SMB2_STATUS_SUCCESS) {
                stat_data->cb(smb2, -nterror_to_errno(status),
                       NULL, stat_data->cb_data);
                free(stat_data);
                return;
        }

        st->smb2_type = SMB2_TYPE_FILE;
        if (fs->basic.file_attributes & SMB2_FILE_ATTRIBUTE_DIRECTORY) {
                st->smb2_type = SMB2_TYPE_DIRECTORY;
        }
        if (fs->basic.file_attributes & SMB2_FILE_ATTRIBUTE_REPARSE_POINT) {
                st->smb2_type = SMB2_TYPE_LINK;
        }
        st->smb2_nlink      = fs->standard.number_of_links;
        st->smb2_ino        = fs->index_number;
        st->smb2_size       = fs->standard.end_of_file;
        st->smb2_atime      = fs->basic.last_access_time.tv_sec;
        st->smb2_atime_nsec = fs->basic.last_access_time.tv_usec *
                1000;
        st->smb2_mtime      = fs->basic.last_write_time.tv_sec;
        st->smb2_mtime_nsec = fs->basic.last_write_time.tv_usec *
                1000;
        st->smb2_ctime      = fs->basic.change_time.tv_sec;
        st->smb2_ctime_nsec = fs->basic.change_time.tv_usec *
                1000;
        st->smb2_btime      = fs->basic.creation_time.tv_sec;
        st->smb2_btime_nsec = fs->basic.creation_time.tv_usec *
                1000;

        smb2_free_data(smb2, fs);

        stat_data->cb(smb2, 0, st, stat_data->cb_data);
        free(stat_data);
}

int
smb2_fstat_async(struct smb2_context *smb2, struct smb2fh *fh,
                 struct smb2_stat_64 *st,
                 smb2_command_cb cb, void *cb_data)
{
        struct stat_cb_data *stat_data;
        struct smb2_query_info_request req;
        struct smb2_pdu *pdu;

        if (smb2 == NULL) {
                return -EINVAL;
        }
        if (fh == NULL) {
                smb2_set_error(smb2, "File handle was NULL");
                return -EINVAL;
        }

        stat_data = calloc(1, sizeof(struct stat_cb_data));
        if (stat_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate stat_data");
                return -ENOMEM;
        }

        stat_data->cb = cb;
        stat_data->cb_data = cb_data;
        stat_data->st = st;

        memset(&req, 0, sizeof(struct smb2_query_info_request));
        req.info_type = SMB2_0_INFO_FILE;
        req.file_info_class = SMB2_FILE_ALL_INFORMATION;
        req.output_buffer_length = DEFAULT_OUTPUT_BUFFER_LENGTH;
        req.additional_information = 0;
        req.flags = 0;
        memcpy(req.file_id, fh->file_id, SMB2_FD_SIZE);

        pdu = smb2_cmd_query_info_async(smb2, &req, fstat_cb_1, stat_data);
        if (pdu == NULL) {
                smb2_set_error(smb2, "Failed to create query command");
                free(stat_data);
                return -ENOMEM;
        }
        smb2_queue_pdu(smb2, pdu);

        return 0;
}

static void
getinfo_cb_3(struct smb2_context *smb2, int status,
             void *command_data _U_, void *private_data)
{
        struct stat_cb_data *stat_data = private_data;

        if (stat_data->status == SMB2_STATUS_SUCCESS) {
                stat_data->status = status;
        }

        stat_data->cb(smb2, -nterror_to_errno(stat_data->status),
                      stat_data->st, stat_data->cb_data);
        free(stat_data);
}

static void
getinfo_cb_2(struct smb2_context *smb2, int status,
             void *command_data, void *private_data)
{
        struct stat_cb_data *stat_data = private_data;
        struct smb2_query_info_reply *rep = command_data;

        if (stat_data->status == SMB2_STATUS_SUCCESS) {
                stat_data->status = status;
        }
        if (stat_data->status != SMB2_STATUS_SUCCESS) {
                return;
        }

        if (stat_data->info_type == SMB2_0_INFO_FILE &&
            stat_data->file_info_class == SMB2_FILE_ALL_INFORMATION) {
                struct smb2_stat_64 *st = stat_data->st;
                struct smb2_file_all_info *fs = rep->output_buffer;

                st->smb2_type = SMB2_TYPE_FILE;
                if (fs->basic.file_attributes & SMB2_FILE_ATTRIBUTE_DIRECTORY) {
                        st->smb2_type = SMB2_TYPE_DIRECTORY;
                }
                if (fs->basic.file_attributes & SMB2_FILE_ATTRIBUTE_REPARSE_POINT) {
                        st->smb2_type = SMB2_TYPE_LINK;
                }
                st->smb2_nlink      = fs->standard.number_of_links;
                st->smb2_ino        = fs->index_number;
                st->smb2_size       = fs->standard.end_of_file;
                st->smb2_atime      = fs->basic.last_access_time.tv_sec;
                st->smb2_atime_nsec = fs->basic.last_access_time.tv_usec *
                        1000;
                st->smb2_mtime      = fs->basic.last_write_time.tv_sec;
                st->smb2_mtime_nsec = fs->basic.last_write_time.tv_usec *
                        1000;
                st->smb2_ctime      = fs->basic.change_time.tv_sec;
                st->smb2_ctime_nsec = fs->basic.change_time.tv_usec *
                        1000;
                st->smb2_btime      = fs->basic.creation_time.tv_sec;
                st->smb2_btime_nsec = fs->basic.creation_time.tv_usec *
                        1000;
        } else if (stat_data->info_type == SMB2_0_INFO_FILESYSTEM &&
                   stat_data->file_info_class == SMB2_FILE_FS_FULL_SIZE_INFORMATION) {
                struct smb2_statvfs *statvfs = stat_data->st;
                struct smb2_file_fs_full_size_info *vfs = rep->output_buffer;

                memset(statvfs, 0, sizeof(struct smb2_statvfs));
                statvfs->f_bsize = statvfs->f_frsize =
                        vfs->bytes_per_sector *
                        vfs->sectors_per_allocation_unit;
                statvfs->f_blocks = vfs->total_allocation_units;
                statvfs->f_bfree = statvfs->f_bavail =
                        vfs->caller_available_allocation_units;
        }
        smb2_free_data(smb2, rep->output_buffer);
}

static void
getinfo_cb_1(struct smb2_context *smb2, int status,
             void *command_data _U_, void *private_data)
{
        struct stat_cb_data *stat_data = private_data;

        if (stat_data->status == SMB2_STATUS_SUCCESS) {
                stat_data->status = status;
        }
}

static int
smb2_getinfo_async(struct smb2_context *smb2, const char *path,
                   uint8_t info_type, uint8_t file_info_class,
                   void *st,
                   smb2_command_cb cb, void *cb_data)
{
        struct stat_cb_data *stat_data;
        struct smb2_create_request cr_req;
        struct smb2_query_info_request qi_req;
        struct smb2_close_request cl_req;
        struct smb2_pdu *pdu, *next_pdu;

        if (smb2 == NULL) {
                return -EINVAL;
        }

        stat_data = calloc(1, sizeof(struct stat_cb_data));
        if (stat_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate create_data");
                return -1;
        }

        stat_data->cb = cb;
        stat_data->cb_data = cb_data;
        stat_data->info_type = info_type;
        stat_data->file_info_class = file_info_class;
        stat_data->st = st;

        /* CREATE command */
        memset(&cr_req, 0, sizeof(struct smb2_create_request));
        cr_req.requested_oplock_level = SMB2_OPLOCK_LEVEL_NONE;
        cr_req.impersonation_level = SMB2_IMPERSONATION_IMPERSONATION;
        cr_req.desired_access = SMB2_FILE_READ_ATTRIBUTES | SMB2_FILE_READ_EA;
        cr_req.file_attributes = 0;
        cr_req.share_access = SMB2_FILE_SHARE_READ | SMB2_FILE_SHARE_WRITE;
        cr_req.create_disposition = SMB2_FILE_OPEN;
        cr_req.create_options = 0;
        cr_req.name = path;

        pdu = smb2_cmd_create_async(smb2, &cr_req, getinfo_cb_1, stat_data);
        if (pdu == NULL) {
                smb2_set_error(smb2, "Failed to create create command");
                free(stat_data);
                return -1;
        }

        /* QUERY INFO command */
        memset(&qi_req, 0, sizeof(struct smb2_query_info_request));
        qi_req.info_type = info_type;
        qi_req.file_info_class = file_info_class;
        qi_req.output_buffer_length = DEFAULT_OUTPUT_BUFFER_LENGTH;
        qi_req.additional_information = 0;
        qi_req.flags = 0;
        memcpy(qi_req.file_id, compound_file_id, SMB2_FD_SIZE);

        next_pdu = smb2_cmd_query_info_async(smb2, &qi_req,
                                             getinfo_cb_2, stat_data);
        if (next_pdu == NULL) {
                smb2_set_error(smb2, "Failed to create query command");
                free(stat_data);
                smb2_free_pdu(smb2, pdu);
                return -1;
        }
        smb2_add_compound_pdu(smb2, pdu, next_pdu);

        /* CLOSE command */
        memset(&cl_req, 0, sizeof(struct smb2_close_request));
        cl_req.flags = SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB;
        memcpy(cl_req.file_id, compound_file_id, SMB2_FD_SIZE);

        next_pdu = smb2_cmd_close_async(smb2, &cl_req, getinfo_cb_3, stat_data);
        if (next_pdu == NULL) {
                stat_data->cb(smb2, -ENOMEM, NULL, stat_data->cb_data);
                free(stat_data);
                smb2_free_pdu(smb2, pdu);
                return -1;
        }
        smb2_add_compound_pdu(smb2, pdu, next_pdu);

        smb2_queue_pdu(smb2, pdu);

        return 0;
}

int
smb2_stat_async(struct smb2_context *smb2, const char *path,
                struct smb2_stat_64 *st,
                smb2_command_cb cb, void *cb_data)
{
        return smb2_getinfo_async(smb2, path,
                                  SMB2_0_INFO_FILE,
                                  SMB2_FILE_ALL_INFORMATION,
                                  st, cb, cb_data);
}

int
smb2_statvfs_async(struct smb2_context *smb2, const char *path,
                   struct smb2_statvfs *statvfs,
                   smb2_command_cb cb, void *cb_data)
{
        return smb2_getinfo_async(smb2, path,
                                  SMB2_0_INFO_FILESYSTEM,
                                  SMB2_FILE_FS_FULL_SIZE_INFORMATION,
                                  statvfs, cb, cb_data);
}

struct trunc_cb_data {
        smb2_command_cb cb;
        void *cb_data;

        uint32_t status;
        uint64_t length;
};

static void
trunc_cb_3(struct smb2_context *smb2, int status,
           void *command_data _U_, void *private_data)
{
        struct trunc_cb_data *trunc_data = private_data;

        if (trunc_data->status == SMB2_STATUS_SUCCESS) {
                trunc_data->status = status;
        }

        trunc_data->cb(smb2, -nterror_to_errno(trunc_data->status),
                       NULL, trunc_data->cb_data);
        free(trunc_data);
}

static void
trunc_cb_2(struct smb2_context *smb2, int status,
           void *command_data, void *private_data)
{
        struct trunc_cb_data *trunc_data = private_data;

        if (trunc_data->status == SMB2_STATUS_SUCCESS) {
                trunc_data->status = status;
        }
}

static void
trunc_cb_1(struct smb2_context *smb2, int status,
           void *command_data _U_, void *private_data)
{
        struct trunc_cb_data *trunc_data = private_data;

        if (trunc_data->status == SMB2_STATUS_SUCCESS) {
                trunc_data->status = status;
        }
}

int
smb2_truncate_async(struct smb2_context *smb2, const char *path,
                    uint64_t length, smb2_command_cb cb, void *cb_data)
{
        struct trunc_cb_data *trunc_data;
        struct smb2_create_request cr_req;
        struct smb2_set_info_request si_req;
        struct smb2_close_request cl_req;
        struct smb2_pdu *pdu, *next_pdu;
        struct smb2_file_end_of_file_info eofi _U_;

        if (smb2 == NULL) {
                return -EINVAL;
        }

        trunc_data = calloc(1, sizeof(struct trunc_cb_data));
        if (trunc_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate trunc_data");
                return -ENOMEM;
        }

        trunc_data->cb = cb;
        trunc_data->cb_data = cb_data;
        trunc_data->length = length;

        /* CREATE command */
        memset(&cr_req, 0, sizeof(struct smb2_create_request));
        cr_req.requested_oplock_level = SMB2_OPLOCK_LEVEL_NONE;
        cr_req.impersonation_level = SMB2_IMPERSONATION_IMPERSONATION;
        cr_req.desired_access = SMB2_GENERIC_WRITE;
        cr_req.file_attributes = 0;
        cr_req.share_access = SMB2_FILE_SHARE_READ | SMB2_FILE_SHARE_WRITE;
        cr_req.create_disposition = SMB2_FILE_OPEN;
        cr_req.create_options = 0;
        cr_req.name = path;

        pdu = smb2_cmd_create_async(smb2, &cr_req, trunc_cb_1, trunc_data);
        if (pdu == NULL) {
                smb2_set_error(smb2, "Failed to create create command");
                free(trunc_data);
                return -EINVAL;
        }

        /* SET INFO command */
        eofi.end_of_file = length;

        memset(&si_req, 0, sizeof(struct smb2_set_info_request));
        si_req.info_type = SMB2_0_INFO_FILE;
        si_req.file_info_class = SMB2_FILE_END_OF_FILE_INFORMATION;
        si_req.additional_information = 0;
        memcpy(si_req.file_id, compound_file_id, SMB2_FD_SIZE);
        si_req.input_data = &eofi;

        next_pdu = smb2_cmd_set_info_async(smb2, &si_req,
                                           trunc_cb_2, trunc_data);
        if (next_pdu == NULL) {
                smb2_set_error(smb2, "Failed to create set command. %s",
                               smb2_get_error(smb2));
                free(trunc_data);
                smb2_free_pdu(smb2, pdu);
                return -EINVAL;
        }
        smb2_add_compound_pdu(smb2, pdu, next_pdu);

        /* CLOSE command */
        memset(&cl_req, 0, sizeof(struct smb2_close_request));
        cl_req.flags = SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB;
        memcpy(cl_req.file_id, compound_file_id, SMB2_FD_SIZE);

        next_pdu = smb2_cmd_close_async(smb2, &cl_req, trunc_cb_3, trunc_data);
        if (next_pdu == NULL) {
                trunc_data->cb(smb2, -ENOMEM, NULL, trunc_data->cb_data);
                free(trunc_data);
                smb2_free_pdu(smb2, pdu);
                return -EINVAL;
        }
        smb2_add_compound_pdu(smb2, pdu, next_pdu);

        smb2_queue_pdu(smb2, pdu);

        return 0;
}

struct rename_cb_data {
        uint8_t *newpath;
        smb2_command_cb cb;
        void *cb_data;
        uint32_t status;
};

static void free_rename_data(struct rename_cb_data *rename_data)
{
        free(rename_data->newpath);
        free(rename_data);
}

static void
rename_cb_3(struct smb2_context *smb2, int status,
           void *command_data _U_, void *private_data)
{
        struct rename_cb_data *rename_data = private_data;

        if (rename_data->status == SMB2_STATUS_SUCCESS) {
                rename_data->status = status;
        }

        rename_data->cb(smb2, -nterror_to_errno(rename_data->status),
                        NULL, rename_data->cb_data);
        free_rename_data(rename_data);
}

static void
rename_cb_2(struct smb2_context *smb2, int status,
           void *command_data _U_, void *private_data)
{
        struct rename_cb_data *rename_data = private_data;

        if (rename_data->status == SMB2_STATUS_SUCCESS) {
                rename_data->status = status;
        }
}

static void
rename_cb_1(struct smb2_context *smb2, int status,
           void *command_data _U_, void *private_data)
{
        struct rename_cb_data *rename_data = private_data;

        if (rename_data->status == SMB2_STATUS_SUCCESS) {
                rename_data->status = status;
        }
}

int
smb2_rename_async(struct smb2_context *smb2, const char *oldpath,
                  const char *newpath, smb2_command_cb cb, void *cb_data)
{
        struct rename_cb_data *rename_data;
        struct smb2_create_request cr_req;
        struct smb2_set_info_request si_req;
        struct smb2_close_request cl_req;
        struct smb2_pdu *pdu, *next_pdu;
        struct smb2_file_rename_info rn_info _U_;
        uint8_t *ptr;

        if (smb2 == NULL) {
                return -EINVAL;
        }

        rename_data = calloc(1, sizeof(struct rename_cb_data));
        if (rename_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate rename_data");
                return -ENOMEM;
        }

        rename_data->cb = cb;
        rename_data->cb_data = cb_data;
        rename_data->newpath = (uint8_t *)strdup(newpath);
        if (rename_data->newpath == NULL) {
                free_rename_data(rename_data);
                smb2_set_error(smb2, "Failed to allocate rename_data->newpath");
                return -ENOMEM;
        }
        for (ptr = rename_data->newpath; *ptr; ptr++) {
                if (*ptr == '/') {
                        *ptr = '\\';
                }
        }

        /* CREATE command */
        memset(&cr_req, 0, sizeof(struct smb2_create_request));
        cr_req.requested_oplock_level = SMB2_OPLOCK_LEVEL_NONE;
        cr_req.impersonation_level = SMB2_IMPERSONATION_IMPERSONATION;
        cr_req.desired_access = SMB2_GENERIC_READ  | SMB2_FILE_READ_ATTRIBUTES | SMB2_DELETE;
        cr_req.file_attributes = 0;
        cr_req.share_access = SMB2_FILE_SHARE_READ | SMB2_FILE_SHARE_WRITE | SMB2_FILE_SHARE_DELETE;
        cr_req.create_disposition = SMB2_FILE_OPEN;
        cr_req.create_options = 0;
        cr_req.name = oldpath;

        pdu = smb2_cmd_create_async(smb2, &cr_req, rename_cb_1, rename_data);
        if (pdu == NULL) {
                smb2_set_error(smb2, "Failed to create create command");
                free_rename_data(rename_data);
                return -EINVAL;
        }

        /* SET INFO command */
        rn_info.replace_if_exist = 0;
        rn_info.file_name = rename_data->newpath;

        memset(&si_req, 0, sizeof(struct smb2_set_info_request));
        si_req.info_type = SMB2_0_INFO_FILE;
        si_req.file_info_class = SMB2_FILE_RENAME_INFORMATION;
        si_req.additional_information = 0;
        memcpy(si_req.file_id, compound_file_id, SMB2_FD_SIZE);
        si_req.input_data = &rn_info;

        next_pdu = smb2_cmd_set_info_async(smb2, &si_req,
                                           rename_cb_2, rename_data);
        if (next_pdu == NULL) {
                smb2_set_error(smb2, "Failed to create set command. %s",
                               smb2_get_error(smb2));
                free_rename_data(rename_data);
                smb2_free_pdu(smb2, pdu);
                return -EINVAL;
        }
        smb2_add_compound_pdu(smb2, pdu, next_pdu);

        /* CLOSE command */
        memset(&cl_req, 0, sizeof(struct smb2_close_request));
        cl_req.flags = SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB;
        memcpy(cl_req.file_id, compound_file_id, SMB2_FD_SIZE);

        next_pdu = smb2_cmd_close_async(smb2, &cl_req, rename_cb_3, rename_data);
        if (next_pdu == NULL) {
                rename_data->cb(smb2, -ENOMEM, NULL, rename_data->cb_data);
                free_rename_data(rename_data);
                smb2_free_pdu(smb2, pdu);
                return -EINVAL;
        }
        smb2_add_compound_pdu(smb2, pdu, next_pdu);

        smb2_queue_pdu(smb2, pdu);

        return 0;
}

static void
ftrunc_cb_1(struct smb2_context *smb2, int status,
            void *command_data _U_, void *private_data)
{
        struct create_cb_data *cb_data = private_data;

        cb_data->cb(smb2, -nterror_to_errno(status),
                    NULL, cb_data->cb_data);
        free(cb_data);
}

int
smb2_ftruncate_async(struct smb2_context *smb2, struct smb2fh *fh,
                     uint64_t length, smb2_command_cb cb, void *cb_data)
{
        struct create_cb_data *create_data;
        struct smb2_set_info_request req;
        struct smb2_file_end_of_file_info eofi _U_;
        struct smb2_pdu *pdu;

        if (smb2 == NULL) {
                return -EINVAL;
        }
        if (fh == NULL) {
                smb2_set_error(smb2, "File handle was NULL");
                return -EINVAL;
        }

        create_data = calloc(1, sizeof(struct create_cb_data));
        if (create_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate create_data");
                return -ENOMEM;
        }

        create_data->cb = cb;
        create_data->cb_data = cb_data;

        eofi.end_of_file = length;

        memset(&req, 0, sizeof(struct smb2_set_info_request));
        req.info_type = SMB2_0_INFO_FILE;
        req.file_info_class = SMB2_FILE_END_OF_FILE_INFORMATION;
        req.additional_information = 0;
        memcpy(req.file_id, fh->file_id, SMB2_FD_SIZE);
        req.input_data = &eofi;

        pdu = smb2_cmd_set_info_async(smb2, &req, ftrunc_cb_1, create_data);
        if (pdu == NULL) {
                smb2_set_error(smb2, "Failed to create set info command");
                return -ENOMEM;
        }
        smb2_queue_pdu(smb2, pdu);

        return 0;
}

struct readlink_cb_data {
        smb2_command_cb cb;
        void *cb_data;

        uint32_t status;
        struct smb2_reparse_data_buffer *reparse;
};

static void
readlink_cb_3(struct smb2_context *smb2, int status,
            void *command_data _U_, void *private_data)
{
        struct readlink_cb_data *cb_data = private_data;
        struct smb2_reparse_data_buffer *rp = cb_data->reparse;
        char *target = (char*)"<unknown reparse point type>";

        if (rp) {
                switch (rp->reparse_tag) {
                case SMB2_REPARSE_TAG_SYMLINK:
                        target = rp->symlink.subname;
                }
        }
        cb_data->cb(smb2, -nterror_to_errno(cb_data->status),
                    target, cb_data->cb_data);
        smb2_free_data(smb2, rp);
        free(cb_data);
}

static void
readlink_cb_2(struct smb2_context *smb2, int status,
            void *command_data, void *private_data)
{
        struct readlink_cb_data *cb_data = private_data;
        struct smb2_ioctl_reply *rep = command_data;

        if (cb_data->status == SMB2_STATUS_SUCCESS) {
                cb_data->status = status;
        }
        if (status == SMB2_STATUS_NOT_A_REPARSE_POINT) {
                smb2_set_error(smb2, "Not a reparse point");
        }
        if (status == SMB2_STATUS_SUCCESS) {
                cb_data->reparse = rep->output;
        }
}

static void
readlink_cb_1(struct smb2_context *smb2, int status,
            void *command_data _U_, void *private_data)
{
        struct readlink_cb_data *cb_data = private_data;

        if (status != SMB2_STATUS_SUCCESS) {
                smb2_set_nterror(smb2, status, "%s", nterror_to_str(status));
        }
        cb_data->status = status;
}

int
smb2_readlink_async(struct smb2_context *smb2, const char *path,
                    smb2_command_cb cb, void *cb_data)
{
        struct readlink_cb_data *readlink_data;
        struct smb2_create_request cr_req;
        struct smb2_ioctl_request io_req;
        struct smb2_close_request cl_req;
        struct smb2_pdu *pdu, *next_pdu;

        if (smb2 == NULL) {
                return -EINVAL;
        }

        readlink_data = calloc(1, sizeof(struct readlink_cb_data));
        if (readlink_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate readlink_data");
                return -ENOMEM;
        }

        readlink_data->cb = cb;
        readlink_data->cb_data = cb_data;

        /* CREATE command */
        memset(&cr_req, 0, sizeof(struct smb2_create_request));
        cr_req.requested_oplock_level = SMB2_OPLOCK_LEVEL_NONE;
        cr_req.impersonation_level = SMB2_IMPERSONATION_IMPERSONATION;
        cr_req.desired_access = SMB2_FILE_READ_ATTRIBUTES;
        cr_req.file_attributes = 0;
        cr_req.share_access = SMB2_FILE_SHARE_READ | SMB2_FILE_SHARE_WRITE |
                SMB2_FILE_SHARE_DELETE;
        cr_req.create_disposition = SMB2_FILE_OPEN;
        cr_req.create_options = SMB2_FILE_OPEN_REPARSE_POINT;
        cr_req.name = path;

        pdu = smb2_cmd_create_async(smb2, &cr_req, readlink_cb_1, readlink_data);
        if (pdu == NULL) {
                smb2_set_error(smb2, "Failed to create create command");
                free(readlink_data);
                return -EINVAL;
        }

        /* IOCTL command */
        memset(&io_req, 0, sizeof(struct smb2_ioctl_request));
        io_req.ctl_code = SMB2_FSCTL_GET_REPARSE_POINT;
        memcpy(io_req.file_id, compound_file_id, SMB2_FD_SIZE);
        io_req.input_count = 0;
        io_req.input = NULL;
        io_req.flags = SMB2_0_IOCTL_IS_FSCTL;

        next_pdu = smb2_cmd_ioctl_async(smb2, &io_req, readlink_cb_2,
                                        readlink_data);
        if (next_pdu == NULL) {
                free(readlink_data);
                smb2_free_pdu(smb2, pdu);
                return -EINVAL;
        }
        smb2_add_compound_pdu(smb2, pdu, next_pdu);

        /* CLOSE command */
        memset(&cl_req, 0, sizeof(struct smb2_close_request));
        cl_req.flags = SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB;
        memcpy(cl_req.file_id, compound_file_id, SMB2_FD_SIZE);

        next_pdu = smb2_cmd_close_async(smb2, &cl_req, readlink_cb_3,
                                        readlink_data);
        if (next_pdu == NULL) {
                free(readlink_data);
                smb2_free_pdu(smb2, pdu);
                return -EINVAL;
        }
        smb2_add_compound_pdu(smb2, pdu, next_pdu);

        smb2_queue_pdu(smb2, pdu);

        return 0;
}

struct disconnect_data {
        smb2_command_cb cb;
        void *cb_data;
};

static void
disconnect_cb_2(struct smb2_context *smb2, int status,
           void *command_data _U_, void *private_data)
{
        struct disconnect_data *dc_data = private_data;

        dc_data->cb(smb2, 0, NULL, dc_data->cb_data);
        free(dc_data);
        if (smb2->change_fd) {
                smb2->change_fd(smb2, smb2->fd, SMB2_DEL_FD);
        }
        close(smb2->fd);
        smb2->fd = SMB2_INVALID_SOCKET;
}

static void
disconnect_cb_1(struct smb2_context *smb2, int status,
           void *command_data _U_, void *private_data)
{
        struct disconnect_data *dc_data = private_data;
        struct smb2_pdu *pdu;

        if (status != SMB2_STATUS_SUCCESS) {
                smb2_set_nterror(smb2, status, "%s", nterror_to_str(status));
                dc_data->cb(smb2, -ENOMEM, NULL, dc_data->cb_data);
                free(dc_data);
                return;
        }
        pdu = smb2_cmd_logoff_async(smb2, disconnect_cb_2, dc_data);
        if (pdu == NULL) {
                dc_data->cb(smb2, -ENOMEM, NULL, dc_data->cb_data);
                free(dc_data);
                return;
        }
        smb2_queue_pdu(smb2, pdu);
}

int
smb2_disconnect_share_async(struct smb2_context *smb2,
                            smb2_command_cb cb, void *cb_data)
{
        struct disconnect_data *dc_data;
        struct smb2_pdu *pdu;

        if (smb2 == NULL) {
                return -EINVAL;
        }

        if (!SMB2_VALID_SOCKET(smb2->fd)) {
                smb2_set_error(smb2, "connection is alreeady disconnected or was never connected");
                return -EINVAL;
        }

        dc_data = calloc(1, sizeof(struct disconnect_data));
        if (dc_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate disconnect_data");
                return -ENOMEM;
        }

        dc_data->cb = cb;
        dc_data->cb_data = cb_data;

        pdu = smb2_cmd_tree_disconnect_async(smb2, disconnect_cb_1, dc_data);
        if (pdu == NULL) {
                free(dc_data);
                return -ENOMEM;
        }
        smb2_queue_pdu(smb2, pdu);

        return 0;
}

struct echo_data {
        smb2_command_cb cb;
        void *cb_data;
};

static void
echo_cb(struct smb2_context *smb2, int status,
           void *command_data _U_, void *private_data)
{
        struct echo_data *cb_data = private_data;

        cb_data->cb(smb2, -nterror_to_errno(status),
                    NULL, cb_data->cb_data);
        free(cb_data);
}

int
smb2_echo_async(struct smb2_context *smb2,
                smb2_command_cb cb, void *cb_data)
{
        struct echo_data *echo_data;
        struct smb2_pdu *pdu;

        if (smb2 == NULL) {
                return -EINVAL;
        }

        echo_data = calloc(1, sizeof(struct echo_data));
        if (echo_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate echo_data");
                return -ENOMEM;
        }

        echo_data->cb = cb;
        echo_data->cb_data = cb_data;

        pdu = smb2_cmd_echo_async(smb2, echo_cb, echo_data);
        if (pdu == NULL) {
                free(echo_data);
                return -ENOMEM;
        }
        smb2_queue_pdu(smb2, pdu);

        return 0;
}

uint32_t
smb2_get_max_read_size(struct smb2_context *smb2)
{
        return smb2->max_read_size;
}

uint32_t
smb2_get_max_write_size(struct smb2_context *smb2)
{
        return smb2->max_write_size;
}

smb2_file_id *
smb2_get_file_id(struct smb2fh *fh)
{
        return &fh->file_id;
}

struct smb2fh *
smb2_fh_from_file_id(struct smb2_context *smb2, smb2_file_id *fileid)
{
        struct smb2fh *fh;

        fh = calloc(1, sizeof(struct smb2fh));
        if (fh == NULL) {
                return NULL;
        }
        memcpy(fh->file_id, fileid, SMB2_FD_SIZE);

        return fh;
}

void
smb2_fd_event_callbacks(struct smb2_context *smb2,
                        smb2_change_fd_cb change_fd,
                        smb2_change_events_cb change_events)
{
        smb2->change_fd = change_fd;
        smb2->change_events = change_events;
}

void
smb2_oplock_break_notify(struct smb2_context *smb2, int status, void *command_data, void *cb_data)
{
        struct smb2_oplock_or_lease_break_reply *rep;
        struct smb2_oplock_break_reply rep_oplock;
        struct smb2_lease_break_reply rep_lease;
        struct smb2_pdu *pdu = NULL;
        uint8_t new_oplock_level;
        uint32_t new_lease_state;

        rep= command_data;


        if (smb2->oplock_or_lease_break_cb) {
                smb2->oplock_or_lease_break_cb(smb2,
                               status, rep, &new_oplock_level, &new_lease_state);
        }
        /* for passthrough case assume the app callback will do everything needed
         */
        if (!smb2->passthrough) {
                if (status) {
                        return;
                } else switch (rep->break_type) {
                        case SMB2_BREAK_TYPE_OPLOCK_NOTIFICATION:
                                memset(&rep_oplock, 0, sizeof(rep_oplock));
                                rep_oplock.oplock_level = new_oplock_level;
                                memcpy(rep_oplock.file_id, rep->lock.oplock.file_id, SMB2_FD_SIZE);
                                pdu = smb2_cmd_oplock_break_reply_async(smb2, &rep_oplock, NULL, cb_data);
                                break;
                        case SMB2_BREAK_TYPE_OPLOCK_RESPONSE:
                                break;
                        case SMB2_BREAK_TYPE_LEASE_NOTIFICATION:
                                memset(&rep_lease, 0, sizeof(rep_oplock));
                                rep_lease.flags = rep->lock.lease.flags;
                                rep_lease.lease_state = new_lease_state;
                                memcpy(rep_lease.lease_key, rep->lock.lease.lease_key, SMB2_LEASE_KEY_SIZE);
                                pdu = smb2_cmd_lease_break_reply_async(smb2, &rep_lease, NULL, cb_data);
                                break;
                        case SMB2_BREAK_TYPE_LEASE_RESPONSE:
                                break;
                        default:
                                smb2_set_error(smb2, "Bad oplock/lease break request %s",
                                                smb2_get_error(smb2));
                                return;
                }
                if (pdu != NULL) {
                        smb2_queue_pdu(smb2, pdu);
                }
        }
}

int
smb2_decode_filenotifychangeinformation(
    struct smb2_context *smb2,
    struct smb2_file_notify_change_information *fnc,
    struct smb2_iovec *vec,
    uint32_t next_entry_offset)
{
        uint32_t name_len, tmp;

        if (next_entry_offset + 12 > vec->len) {
                return 0;
        }
        smb2_get_uint32(vec, next_entry_offset+4, &fnc->action);
        smb2_get_uint32(vec, next_entry_offset+8, &name_len);
        fnc->name = smb2_utf16_to_utf8((uint16_t *)(void *)&vec->buf[next_entry_offset+12], name_len / 2);

        smb2_get_uint32(vec, next_entry_offset, &tmp);
        next_entry_offset += tmp;
        if (tmp != 0) {
                struct smb2_file_notify_change_information *next_fnc = calloc(1, sizeof(struct smb2_file_notify_change_information));
                fnc->next = next_fnc;
                smb2_decode_filenotifychangeinformation(smb2, next_fnc, vec, next_entry_offset);
        }
        return 0;
}

void
free_smb2_file_notify_change_information(struct smb2_context *smb2, struct smb2_file_notify_change_information *fnc)
{
        if (fnc->next) {
                free_smb2_file_notify_change_information(smb2, fnc->next);
        }
        free(discard_const(fnc->name));
        free(fnc);
}

struct notify_change_cb_data {
        smb2_command_cb cb;
        void *cb_data;
        // smb2fh file handle of the directory to get notified
        struct smb2fh *fh;
        // filter of SMB2_CHANGE_NOTIFY_FILE_NOTIFY_CHANGE_* flags
        uint16_t filter;
        // flags such as SMB2_CHANGE_NOTIFY_WATCH_TREE
        uint32_t flags;
        // do a new notify_change request after each response if 1
        uint32_t loop;
        uint32_t status;
};

static void
notify_change_cb(struct smb2_context *smb2, int status,
          void *command_data _U_, void *private_data)
{
        struct notify_change_cb_data *notify_change_data = private_data;

        struct smb2_change_notify_reply *rep = command_data;
        struct smb2_iovec vec;
        struct smb2_file_notify_change_information *fnc = calloc(1, sizeof(struct smb2_file_notify_change_information));

        if (status) {
                smb2_set_error(smb2, "notify_change_cb failed (%s) %s\n",
                               strerror(-status), smb2_get_error(smb2));
        }

        vec.buf = rep->output;
        vec.len = rep->output_buffer_length;

        if (smb2_decode_filenotifychangeinformation(smb2, fnc, &vec, 0)) {
                smb2_set_error(smb2, "Failed to decode file notify change information\n");
        }

        if (notify_change_data->cb) {
                notify_change_data->cb(
                        smb2,
                        -nterror_to_errno(notify_change_data->status),
                        fnc,
                        notify_change_data->cb_data
                );
        }
        if (notify_change_data->loop) {
                smb2_notify_change_filehandle_async(smb2, notify_change_data->fh, notify_change_data->flags, notify_change_data->filter,
                        notify_change_data->loop, notify_change_data->cb, notify_change_data->cb_data);
        } else {
                smb2_close(smb2, notify_change_data->fh);
        }
        free(notify_change_data);
}

int smb2_notify_change_filehandle_async(struct smb2_context *smb2, struct smb2fh *smb2_dir_fh, uint16_t flags, uint32_t filter, int loop,
                       smb2_command_cb cb, void *cb_data)
{
        struct notify_change_cb_data *notify_change_cb_data;
        struct smb2_change_notify_request ch_req;
        struct smb2_pdu *pdu;

        notify_change_cb_data = calloc(1, sizeof(struct notify_change_cb_data));
        if (notify_change_cb_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate notify_change_data");
                return -1;
        }
        memset(notify_change_cb_data, 0, sizeof(struct notify_change_cb_data));
        notify_change_cb_data->cb = cb;
        notify_change_cb_data->cb_data = cb_data;
        notify_change_cb_data->fh = smb2_dir_fh;

        notify_change_cb_data->flags = flags;
        notify_change_cb_data->filter = filter;
        notify_change_cb_data->loop = loop;

        /* CHANGE NOTIFY command */
        memset(&ch_req, 0, sizeof(struct smb2_change_notify_request));
        ch_req.flags = flags;
        ch_req.output_buffer_length = DEFAULT_OUTPUT_BUFFER_LENGTH;
        const smb2_file_id *file_id = smb2_get_file_id(smb2_dir_fh);
        memcpy(ch_req.file_id, file_id, SMB2_FD_SIZE);
        ch_req.completion_filter = filter;

        pdu = smb2_cmd_change_notify_async(smb2, &ch_req,
                                             notify_change_cb, notify_change_cb_data);
        if (pdu == NULL) {
                smb2_set_error(smb2, "Failed to create change_notify command\n");
                free(notify_change_cb_data);
                return -1;
        }
        smb2_queue_pdu(smb2, pdu);

        return 0;
}

int smb2_notify_change_async(struct smb2_context *smb2, const char *path, uint16_t flags, uint32_t filter, int loop,
                       smb2_command_cb cb, void *cb_data)
{
        struct smb2fh *fh;
#ifdef O_DIRECTORY
        fh = smb2_open(smb2, path, O_DIRECTORY);
#else
        fh = smb2_open(smb2, path, 0);
#endif
        if (fh == NULL) {
                smb2_set_error(smb2, "smb2_open failed. %s\n", smb2_get_error(smb2));
                return -1;
        }
        return smb2_notify_change_filehandle_async(smb2, fh, flags, filter, loop, cb, cb_data);

}

/*************************** server handlers *************************************************************/
static void
smb2_logoff_request_cb(struct smb2_server *server, struct smb2_context *smb2, void *command_data, void *cb_data)
{
        struct smb2_pdu *pdu = NULL;
        struct smb2_error_reply err;
        int ret = -EINVAL;

        if (server->handlers && server->handlers->logoff_cmd) {
                ret = server->handlers->logoff_cmd(server, smb2);
        }
        if (!ret) {
                pdu = smb2_cmd_logoff_reply_async(smb2, NULL, cb_data);
        }
        else if (ret < 0) {
                memset(&err, 0, sizeof(err));
                pdu = smb2_cmd_error_reply_async(smb2,
                                &err, SMB2_LOGOFF, SMB2_STATUS_NOT_IMPLEMENTED, NULL, cb_data);
        }
        if (pdu != NULL) {
                smb2_set_pdu_message_id(smb2, pdu, smb2->message_id);
                smb2_queue_pdu(smb2, pdu);
        }
}

static void
smb2_tree_connect_request_cb(struct smb2_server *server, struct smb2_context *smb2, void *command_data, void *cb_data)
{
        struct smb2_tree_connect_request *req = command_data;
        struct smb2_tree_connect_reply rep;
        struct smb2_error_reply err;
        struct smb2_pdu *pdu = NULL;
        int ret = -1;

        memset(&rep, 0, sizeof(rep));
        if (server->handlers && server->handlers->tree_connect_cmd) {
                ret = server->handlers->tree_connect_cmd(server, smb2, req, &rep);
        }
        if (!ret) {
                pdu = smb2_cmd_tree_connect_reply_async(smb2, &rep, 0, NULL, cb_data);
        }
        else if (ret < 0) {
                memset(&err, 0, sizeof(err));
                pdu = smb2_cmd_error_reply_async(smb2,
                                &err, SMB2_TREE_CONNECT, SMB2_STATUS_NOT_IMPLEMENTED, NULL, cb_data);
        }
        if (pdu != NULL) {
                smb2_set_pdu_message_id(smb2, pdu, smb2->message_id);
                smb2_queue_pdu(smb2, pdu);
        }
}

static void
smb2_tree_disconnect_request_cb(struct smb2_server *server, struct smb2_context *smb2, void *command_data, void *cb_data)
{
        struct smb2_pdu *pdu = NULL;
        struct smb2_error_reply err;
        uint32_t tree_id = smb2->hdr.sync.tree_id;
        int ret = -1;

        if (server->handlers && server->handlers->tree_disconnect_cmd) {
                ret = server->handlers->tree_disconnect_cmd(server, smb2, smb2_tree_id(smb2));
        }
        if (!ret) {
                pdu = smb2_cmd_tree_disconnect_reply_async(smb2, NULL, cb_data);
        }
        else if (ret < 0) {
                memset(&err, 0, sizeof(err));
                pdu = smb2_cmd_error_reply_async(smb2,
                                &err, SMB2_TREE_DISCONNECT, SMB2_STATUS_NOT_IMPLEMENTED, NULL, cb_data);
        }
        if (pdu != NULL) {
                smb2_set_pdu_message_id(smb2, pdu, smb2->message_id);
                smb2_queue_pdu(smb2, pdu);
        }

        smb2_disconnect_tree_id(smb2, tree_id);
}

static void
smb2_create_request_cb(struct smb2_server *server, struct smb2_context *smb2, void *command_data, void *cb_data)
{
        struct smb2_create_request *req = command_data;
        struct smb2_create_reply rep;
        struct smb2_error_reply err;
        struct smb2_pdu *pdu = NULL;
        int ret = -1;

        memset(&rep, 0, sizeof(rep));
        if (server->handlers && server->handlers->create_cmd) {
                ret = server->handlers->create_cmd(server, smb2, req, &rep);
        }
        if (!ret) {
                pdu = smb2_cmd_create_reply_async(smb2, &rep, NULL, cb_data);
        }
        else if (ret < 0) {
                memset(&err, 0, sizeof(err));
                pdu = smb2_cmd_error_reply_async(smb2,
                                &err, SMB2_CREATE, SMB2_STATUS_NOT_IMPLEMENTED, NULL, cb_data);
        }
        if (pdu) {
                if (req->name) {
                        smb2_free_data(smb2, discard_const(req->name));
                }
                smb2_set_pdu_message_id(smb2, pdu, smb2->message_id);
                smb2_queue_pdu(smb2, pdu);
        }
}

static void
smb2_close_request_cb(struct smb2_server *server, struct smb2_context *smb2, void *command_data, void *cb_data)
{
        struct smb2_close_request *req = command_data;
        struct smb2_close_reply rep;
        struct smb2_error_reply err;
        struct smb2_pdu *pdu = NULL;
        int ret = -1;

        memset(&rep, 0, sizeof(rep));
        if (server->handlers && server->handlers->close_cmd) {
                ret = server->handlers->close_cmd(server, smb2, req, &rep);
        }
        if (!ret) {
                pdu = smb2_cmd_close_reply_async(smb2, &rep, NULL, cb_data);
        }
        else if (ret < 0) {
                memset(&err, 0, sizeof(err));
                pdu = smb2_cmd_error_reply_async(smb2,
                                &err, SMB2_CLOSE, SMB2_STATUS_NOT_IMPLEMENTED, NULL, cb_data);
        }
        if (pdu != NULL) {
                smb2_set_pdu_message_id(smb2, pdu, smb2->message_id);
                smb2_queue_pdu(smb2, pdu);
        }
}

static void
smb2_flush_request_cb(struct smb2_server *server, struct smb2_context *smb2, void *command_data, void *cb_data)
{
        struct smb2_flush_request *req = command_data;
        struct smb2_error_reply err;
        struct smb2_pdu *pdu = NULL;
        int ret = -1;

        if (server->handlers && server->handlers->flush_cmd) {
                ret = server->handlers->flush_cmd(server, smb2, req);
        }
        if (!ret) {
                pdu = smb2_cmd_flush_reply_async(smb2, NULL, cb_data);
        }
        else if (ret < 0) {
                memset(&err, 0, sizeof(err));
                pdu = smb2_cmd_error_reply_async(smb2,
                                &err, SMB2_FLUSH, SMB2_STATUS_NOT_IMPLEMENTED, NULL, cb_data);
        }
        if (pdu != NULL) {
                smb2_set_pdu_message_id(smb2, pdu, smb2->message_id);
                smb2_queue_pdu(smb2, pdu);
        }
}

static void
smb2_read_request_cb(struct smb2_server *server, struct smb2_context *smb2, void *command_data, void *cb_data)
{
        struct smb2_read_request *req = command_data;
        struct smb2_read_reply rep;
        struct smb2_error_reply err;
        struct smb2_pdu *pdu = NULL;
        int ret = -1;

        memset(&rep, 0, sizeof(rep));
        if (server->handlers && server->handlers->read_cmd) {
                ret = server->handlers->read_cmd(server, smb2, req, &rep);
        }
        if (!ret) {
                pdu = smb2_cmd_read_reply_async(smb2, &rep, NULL, cb_data);
        }
        else if (ret < 0) {
                memset(&err, 0, sizeof(err));
                pdu = smb2_cmd_error_reply_async(smb2,
                                &err, SMB2_READ, SMB2_STATUS_NOT_IMPLEMENTED, NULL, cb_data);
        }
        if (pdu != NULL) {
                smb2_set_pdu_message_id(smb2, pdu, smb2->message_id);
                smb2_queue_pdu(smb2, pdu);
        }
}

static void
smb2_write_request_cb(struct smb2_server *server, struct smb2_context *smb2, void *command_data, void *cb_data)
{
        struct smb2_write_request *req = command_data;
        struct smb2_write_reply rep;
        struct smb2_error_reply err;
        struct smb2_pdu *pdu = NULL;
        int ret = -1;

        memset(&rep, 0, sizeof(rep));
        if (server->handlers && server->handlers->write_cmd) {
                ret = server->handlers->write_cmd(server, smb2, req, &rep);
        }
        if (!ret) {
                pdu = smb2_cmd_write_reply_async(smb2, &rep, NULL, cb_data);
        }
        else if (ret < 0) {
                memset(&err, 0, sizeof(err));
                pdu = smb2_cmd_error_reply_async(smb2,
                                &err, SMB2_WRITE, SMB2_STATUS_NOT_IMPLEMENTED, NULL, cb_data);
        }
        if (pdu != NULL) {
                smb2_set_pdu_message_id(smb2, pdu, smb2->message_id);
                smb2_queue_pdu(smb2, pdu);
        }
}

static void
smb2_oplock_break_request_cb(struct smb2_server *server, struct smb2_context *smb2, void *command_data, void *cb_data)
{
        struct smb2_oplock_or_lease_break_request *req = command_data;
        struct smb2_oplock_break_reply rep_oplock;
        struct smb2_lease_break_reply rep_lease;
        struct smb2_error_reply err;
        struct smb2_pdu *pdu = NULL;
        int ret = -1;

        if (req->struct_size == SMB2_OPLOCK_BREAK_NOTIFICATION_SIZE) {
                if (server->handlers && server->handlers->oplock_break_cmd) {
                        ret = server->handlers->oplock_break_cmd(server, smb2,
                                       &req->lock.oplock);
                        if (!ret) {
                                memset(&rep_oplock, 0, sizeof(rep_oplock));
                                pdu = smb2_cmd_oplock_break_reply_async(smb2,
                                        &rep_oplock, NULL, cb_data);
                        }
                }
        }
        else if ((req->struct_size == SMB2_LEASE_BREAK_NOTIFICATION_SIZE) |
                        (req->struct_size == SMB2_LEASE_BREAK_REPLY_SIZE)) {
                if (server->handlers && server->handlers->lease_break_cmd) {
                        ret = server->handlers->lease_break_cmd(server, smb2,
                                       &req->lock.lease);
                        if (!ret) {
                                memset(&rep_lease, 0, sizeof(rep_lease));
                                pdu = smb2_cmd_lease_break_reply_async(smb2,
                                        &rep_lease, NULL, cb_data);
                        }
                }
        }
        if(ret < 0) {
                memset(&err, 0, sizeof(err));
                pdu = smb2_cmd_error_reply_async(smb2,
                                &err, SMB2_LOCK, SMB2_STATUS_NOT_IMPLEMENTED, NULL, cb_data);
        }
        if (pdu != NULL) {
                smb2_set_pdu_message_id(smb2, pdu, smb2->message_id);
                smb2_queue_pdu(smb2, pdu);
        }
}

static void
smb2_lock_request_cb(struct smb2_server *server, struct smb2_context *smb2, void *command_data, void *cb_data)
{
        struct smb2_lock_request *req = command_data;
        struct smb2_error_reply err;
        struct smb2_pdu *pdu = NULL;
        int ret = -1;

        if (server->handlers && server->handlers->lock_cmd) {
                ret = server->handlers->lock_cmd(server, smb2, req);
        }
        if (!ret) {
                pdu = smb2_cmd_lock_reply_async(smb2, NULL, cb_data);
        }
        else if(ret < 0) {
                memset(&err, 0, sizeof(err));
                pdu = smb2_cmd_error_reply_async(smb2,
                                &err, SMB2_LOCK, SMB2_STATUS_NOT_IMPLEMENTED, NULL, cb_data);
        }
        if (pdu != NULL) {
                smb2_set_pdu_message_id(smb2, pdu, smb2->message_id);
                smb2_queue_pdu(smb2, pdu);
        }
}

static void
smb2_ioctl_request_cb(struct smb2_server *server, struct smb2_context *smb2, void *command_data, void *cb_data)
{
        struct smb2_ioctl_request *req = command_data;
        struct smb2_ioctl_reply rep;
        struct smb2_error_reply err;
        struct smb2_pdu *pdu = NULL;
        struct smb2_ioctl_validate_negotiate_info out_info;
        int ret = -1;

        memset(&rep, 0, sizeof(rep));
        rep.ctl_code = req->ctl_code;
        memcpy(rep.file_id, req->file_id, SMB2_FD_SIZE);

        if (req->ctl_code == SMB2_FSCTL_VALIDATE_NEGOTIATE_INFO) {
                /* this one only needs local handling ever */
                /* in_info = (struct smb2_ioctl_validate_negotiate_info *)req->input; */
                out_info.capabilities = smb2->capabilities;
                out_info.security_mode = smb2->security_mode;
                memcpy(out_info.guid, server->guid, 16);
                out_info.dialect = smb2->dialect;
                rep.output = (uint8_t*)&out_info;
                rep.output_count = sizeof(out_info);
                pdu = smb2_cmd_ioctl_reply_async(smb2, &rep, NULL, cb_data);
                if (req->input) {
                        smb2_free_data(smb2, discard_const(req->input));
                }
        }
        else {
                if (server->handlers && server->handlers->ioctl_cmd) {
                        ret = server->handlers->ioctl_cmd(server, smb2, req, &rep);
                }
                if (!ret) {
                        pdu = smb2_cmd_ioctl_reply_async(smb2, &rep, NULL, cb_data);
                }
                else if (ret < 0) {
                        memset(&err, 0, sizeof(err));
                        pdu = smb2_cmd_error_reply_async(smb2,
                                        &err, SMB2_IOCTL, SMB2_STATUS_NOT_IMPLEMENTED, NULL, cb_data);
                }
        }
        if (pdu != NULL) {
                smb2_set_pdu_message_id(smb2, pdu, smb2->message_id);
                smb2_queue_pdu(smb2, pdu);
        }
}

static void
smb2_cancel_request_cb(struct smb2_server *server, struct smb2_context *smb2, void *command_data, void *cb_data)
{
        struct smb2_error_reply err;
        struct smb2_pdu *pdu = NULL;
        int ret = -1;

        if (server->handlers && server->handlers->cancel_cmd) {
                ret = server->handlers->cancel_cmd(server, smb2);
        }
        if (ret < 0) {
                memset(&err, 0, sizeof(err));
                pdu = smb2_cmd_error_reply_async(smb2,
                                &err, SMB2_CANCEL, SMB2_STATUS_NOT_IMPLEMENTED, NULL, cb_data);
        }
        if (pdu != NULL) {
                smb2_set_pdu_message_id(smb2, pdu, smb2->message_id);
                smb2_queue_pdu(smb2, pdu);
        }
}

static void
smb2_echo_request_cb(struct smb2_server *server, struct smb2_context *smb2, void *command_data, void *cb_data)
{
        struct smb2_error_reply err;
        struct smb2_pdu *pdu = NULL;
        int ret = -1;

        if (server->handlers && server->handlers->echo_cmd) {
                ret = server->handlers->echo_cmd(server, smb2);
        }
        if (!ret) {
                pdu = smb2_cmd_echo_reply_async(smb2, NULL, cb_data);
        }
        else if (ret < 0) {
                memset(&err, 0, sizeof(err));
                pdu = smb2_cmd_error_reply_async(smb2,
                                &err, SMB2_ECHO, SMB2_STATUS_NOT_IMPLEMENTED, NULL, cb_data);
        }
        if (pdu != NULL) {
                smb2_set_pdu_message_id(smb2, pdu, smb2->message_id);
                smb2_queue_pdu(smb2, pdu);
        }
}

static void
smb2_query_directory_request_cb(struct smb2_server *server, struct smb2_context *smb2, void *command_data, void *cb_data)
{
        struct smb2_query_directory_request *req = command_data;
        struct smb2_query_directory_reply rep;
        struct smb2_error_reply err;
        struct smb2_pdu *pdu = NULL;
        int ret = -1;

        memset(&rep, 0, sizeof(rep));
        memset(&err, 0, sizeof(err));

        if (server->handlers && server->handlers->query_directory_cmd) {
                ret = server->handlers->query_directory_cmd(server, smb2, req, &rep);
        }
        if (ret < 0) {
                pdu = smb2_cmd_error_reply_async(smb2,
                                &err, SMB2_QUERY_DIRECTORY, SMB2_STATUS_NOT_IMPLEMENTED, NULL, cb_data);
        }
        else if (!ret) {
                if (rep.output_buffer_length == 0) {
                        pdu = smb2_cmd_error_reply_async(smb2,
                                        &err, SMB2_QUERY_DIRECTORY, SMB2_STATUS_NO_MORE_FILES, NULL, cb_data);
                }
                else if (rep.output_buffer_length < 0) {
                        pdu = smb2_cmd_error_reply_async(smb2,
                                        &err, SMB2_QUERY_DIRECTORY, SMB2_STATUS_NOT_SUPPORTED, NULL, cb_data);
                }
                else {
                        pdu = smb2_cmd_query_directory_reply_async(smb2, req, &rep, NULL, cb_data);
                }
        }
        if (req->name) {
                smb2_free_data(smb2, discard_const(req->name));
        }
        if (pdu != NULL) {
                smb2_set_pdu_message_id(smb2, pdu, smb2->message_id);
                smb2_queue_pdu(smb2, pdu);
        }
}

static void
smb2_change_notify_request_cb(struct smb2_server *server, struct smb2_context *smb2, void *command_data, void *cb_data)
{
        struct smb2_change_notify_request *req = command_data;
        struct smb2_change_notify_reply rep;
        struct smb2_error_reply err;
        struct smb2_pdu *pdu = NULL;
        int ret = -1;

        memset(&rep, 0, sizeof(rep));
        memset(&err, 0, sizeof(err));

        if (server->handlers && server->handlers->change_notify_cmd) {
                ret = server->handlers->change_notify_cmd(server, smb2, req, &rep);
        }
        if (ret < 0) {
                pdu = smb2_cmd_error_reply_async(smb2,
                                &err, SMB2_CHANGE_NOTIFY, SMB2_STATUS_NOT_IMPLEMENTED, NULL, cb_data);
        }
        else if (!ret) {
                pdu = smb2_cmd_change_notify_reply_async(smb2, &rep, NULL, cb_data);
        }
        if (pdu != NULL) {
                smb2_set_pdu_message_id(smb2, pdu, smb2->message_id);
                smb2_queue_pdu(smb2, pdu);
        }
}

static void
smb2_query_info_request_cb(struct smb2_server *server, struct smb2_context *smb2, void *command_data, void *cb_data)
{
        struct smb2_query_info_request *req = command_data;
        struct smb2_query_info_reply rep;
        struct smb2_error_reply err;
        struct smb2_pdu *pdu = NULL;
        int ret = -1;

        memset(&rep, 0, sizeof(rep));
        memset(&err, 0, sizeof(err));

        if (server->handlers && server->handlers->query_info_cmd) {
                ret = server->handlers->query_info_cmd(server, smb2, req, &rep);
        }
        if (ret < 0) {
                pdu = smb2_cmd_error_reply_async(smb2,
                                &err, SMB2_QUERY_INFO, SMB2_STATUS_NOT_IMPLEMENTED, NULL, cb_data);
        }
        else if (!ret) {
                if (rep.output_buffer_length == 0) {
                        pdu = smb2_cmd_error_reply_async(smb2,
                                        &err, SMB2_QUERY_INFO, SMB2_STATUS_NOT_SUPPORTED, NULL, cb_data);
                }
                else if (rep.output_buffer_length < 0) {
                        pdu = smb2_cmd_error_reply_async(smb2,
                                        &err, SMB2_QUERY_INFO, SMB2_STATUS_INVALID_INFO_CLASS, NULL, cb_data);
                }
                else {
                        pdu = smb2_cmd_query_info_reply_async(smb2, req, &rep, NULL, cb_data);
                }
        }
        if (pdu != NULL) {
                smb2_set_pdu_message_id(smb2, pdu, smb2->message_id);
                smb2_queue_pdu(smb2, pdu);
        }
}

static void
smb2_set_info_request_cb(struct smb2_server *server, struct smb2_context *smb2, void *command_data, void *cb_data)
{
        struct smb2_set_info_request *req = command_data;
        struct smb2_error_reply err;
        struct smb2_pdu *pdu = NULL;
        int ret = -1;

        memset(&err, 0, sizeof(err));

        if (server->handlers && server->handlers->set_info_cmd) {
                ret = server->handlers->set_info_cmd(server, smb2, req);
        }
        if (ret < 0) {
                pdu = smb2_cmd_error_reply_async(smb2,
                                &err, SMB2_SET_INFO, SMB2_STATUS_NOT_IMPLEMENTED, NULL, cb_data);
        }
        else if (!ret) {
                pdu = smb2_cmd_set_info_reply_async(smb2, req, NULL, cb_data);
        }
        if (pdu != NULL) {
                smb2_set_pdu_message_id(smb2, pdu, smb2->message_id);
                smb2_queue_pdu(smb2, pdu);
        }
}

static void
smb2_session_setup_request_cb(struct smb2_context *smb2, int status, void *command_data, void *cb_data);

static void
smb2_general_client_request_cb(struct smb2_context *smb2, int status, void *command_data, void *cb_data)
{
        struct connect_data *c_data = cb_data;
        struct smb2_server *server = c_data->server_context;
        enum smb2_command next_cmd = SMB2_TREE_CONNECT;
        smb2_command_cb next_cb = smb2_general_client_request_cb;

        if (!smb2->pdu) {
                smb2_set_error(smb2, "No pdu for general client request");
                smb2_close_context(smb2);
                return;
        }
        if (status == SMB2_STATUS_CANCELLED || status == SMB2_STATUS_SHUTDOWN) {
                return;
        }

        switch (smb2->pdu->header.command) {
        case SMB2_SESSION_SETUP:
                smb2_session_setup_request_cb(smb2, status, command_data, cb_data);
                /* session setup cb allocs next_pdu itself */
                next_cb = NULL;
                break;
        case SMB2_LOGOFF:
                smb2_logoff_request_cb(server, smb2, command_data, cb_data);
                /* prep for a new session setup req */
                next_cmd = SMB2_SESSION_SETUP;
                next_cb = smb2_session_setup_request_cb;
                break;
        case SMB2_TREE_CONNECT:
                smb2_tree_connect_request_cb(server, smb2, command_data, cb_data);
                break;
        case SMB2_TREE_DISCONNECT:
                smb2_tree_disconnect_request_cb(server, smb2, command_data, cb_data);
                break;
        case SMB2_CREATE:
                smb2_create_request_cb(server, smb2, command_data, cb_data);
                break;
        case SMB2_CLOSE:
                smb2_close_request_cb(server, smb2, command_data, cb_data);
                break;
        case SMB2_FLUSH:
                smb2_flush_request_cb(server, smb2, command_data, cb_data);
                break;
        case SMB2_READ:
                smb2_read_request_cb(server, smb2, command_data, cb_data);
                break;
        case SMB2_WRITE:
                smb2_write_request_cb(server, smb2, command_data, cb_data);
                break;
        case SMB2_OPLOCK_BREAK:
                smb2_oplock_break_request_cb(server, smb2, command_data, cb_data);
                break;
        case SMB2_LOCK:
                smb2_lock_request_cb(server, smb2, command_data, cb_data);
                break;
        case SMB2_IOCTL:
                smb2_ioctl_request_cb(server, smb2, command_data, cb_data);
                break;
        case SMB2_CANCEL:
                smb2_cancel_request_cb(server, smb2, command_data, cb_data);
                break;
        case SMB2_ECHO:
                smb2_echo_request_cb(server, smb2, command_data, cb_data);
                break;
        case SMB2_QUERY_DIRECTORY:
                smb2_query_directory_request_cb(server, smb2, command_data, cb_data);
                break;
        case SMB2_CHANGE_NOTIFY:
                smb2_change_notify_request_cb(server, smb2, command_data, cb_data);
                break;
        case SMB2_QUERY_INFO:
                smb2_query_info_request_cb(server, smb2, command_data, cb_data);
                break;
        case SMB2_SET_INFO:
                smb2_set_info_request_cb(server, smb2, command_data, cb_data);
                break;
        default:
                smb2_set_error(smb2, "Client request %d not implemented  %s",
                               smb2->pdu->header.command, smb2_get_error(smb2));
                break;
        }

        if (next_cb) {
                /* alloc a pdu for next request. note that we dont really expect a tree connect, its just to
                 * allow pdu reading to know to allow for any command above negotiate and session-setup
                 */
                smb2->next_pdu = smb2_allocate_pdu(smb2, next_cmd, next_cb, cb_data);
                if (!smb2->next_pdu) {
                        smb2_set_error(smb2, "can not alloc pdu for authorization session setup request");
                        smb2_close_context(smb2);
                }
        }
}

static void
smb2_session_setup_request_cb(struct smb2_context *smb2, int status, void *command_data, void *cb_data)
{
        struct connect_data *c_data = cb_data;
        struct smb2_server *server = c_data->server_context;
        struct smb2_session_setup_request *req = command_data;
        struct smb2_session_setup_reply rep;
        struct smb2_pdu *pdu;
        struct smb2_error_reply err;
        uint32_t message_type;
        int more_processing_needed = 0;
        uint8_t *response_token;
        int response_length;
        int is_spnego_wrapped;
        int have_valid_session_key = 1;
        int ret;

        if (status) {
                return;
        }

        rep.security_buffer_length = 0;
        rep.security_buffer_offset = 0;

        rep.session_flags = 0; /* req->flags; */

        smb3_update_preauth_hash(smb2, smb2->in.niov - 1, &smb2->in.iov[1]);
        memset(&err, 0, sizeof(err));

        pdu = NULL;

        if (smb2->sec == SMB2_SEC_UNDEFINED || smb2->sec == SMB2_SEC_NTLMSSP) {
                /* if we haven't set sec type yet, and the blob contains
                 * valid ntlmssp, use our ntlmssp implementation, but supress
                 * error-setting while sniffing. if we are expecting ntlmssp
                 * insist on a valid message
                 * TODO - perhaps use krb5+ntlmssp if configured?
                 */
                if (ntlmssp_get_message_type(smb2,
                                req->security_buffer, req->security_buffer_length,
                                smb2->sec == SMB2_SEC_UNDEFINED,
                                &message_type,
                                &response_token, &response_length,
                                &is_spnego_wrapped) >= 0) {
                        smb2->sec = SMB2_SEC_NTLMSSP;
                } else {

#ifdef HAVE_LIBKRB5
                        smb2->sec = SMB2_SEC_KRB5;
#else
                        smb2_set_error(smb2, "No message type in NTLMSSP %s",
                                        smb2_get_error(smb2));
                        smb2_close_context(smb2);
                        return;
#endif
                }
        }

        if (smb2->sec == SMB2_SEC_NTLMSSP) {
                /* set error code in header - more processing required if negotiate req not auth req */
                if (message_type == NEGOTIATE_MESSAGE) {
                        if (c_data->auth_data) {
                                ntlmssp_destroy_context(c_data->auth_data);
                        }
                        c_data->auth_data = ntlmssp_init_context(
                                        "",
                                        "",
                                        "",
                                        server->hostname,
                                        smb2->client_challenge
                                        );
                        if (!c_data->auth_data) {
                                smb2_set_error(smb2, "can not init auth data %s", smb2_get_error(smb2));
                                smb2_close_context(smb2);
                                return;
                        }
                        smb2->connect_data = c_data;

                        /* alloc a pdu for next request */
                        smb2->next_pdu = smb2_allocate_pdu(smb2, SMB2_SESSION_SETUP,
                                       smb2_session_setup_request_cb, cb_data);
                        more_processing_needed = 1;
                        smb2->session_id = server->session_counter++;
                }
                else if (message_type == AUTHENTICATION_MESSAGE) {
                        /* alloc a pdu for next request (not really required to get tree connect) */
                        smb2->next_pdu = smb2_allocate_pdu(smb2, SMB2_TREE_CONNECT,
                                       smb2_general_client_request_cb, cb_data);
                }
                else {
                        smb2_set_error(smb2, "Unexpected ntlmssp msg code %08X", message_type);
                        smb2_close_context(smb2);
                        return;
                }
                if (ntlmssp_generate_blob(server, smb2, 0, c_data->auth_data,
                                          req->security_buffer, req->security_buffer_length,
                                          &rep.security_buffer,
                                          &rep.security_buffer_length) < 0) {
                        smb2_close_context(smb2);
                        return;
                }
                if (message_type == AUTHENTICATION_MESSAGE) {
                        if (!ntlmssp_get_authenticated(c_data->auth_data)) {
                                smb2_set_error(smb2, "Authentication failed: %s", smb2_get_error(smb2));
                                #if 0
                                smb2_close_context(smb2);
                                return;
                                #else
                                pdu = smb2_cmd_error_reply_async(smb2,
                                                &err, SMB2_SESSION_SETUP,
                                                SMB2_STATUS_LOGON_FAILURE, NULL, cb_data);
                                smb2_free_pdu(smb2, smb2->next_pdu);
                                smb2->next_pdu = smb2_allocate_pdu(smb2, SMB2_SESSION_SETUP,
                                               smb2_session_setup_request_cb, cb_data);
                                more_processing_needed = 0;
                                #endif
                        }
                        if (ntlmssp_get_session_key(c_data->auth_data,
                                                    &smb2->session_key,
                                                    &smb2->session_key_size) < 0) {
                                have_valid_session_key = 0;
                        }
                }
        }
#ifdef HAVE_LIBKRB5
        else {
                if (!c_data->auth_data) {
                        c_data->auth_data = krb5_init_server_client_cred(server, smb2, NULL);
                        if (!c_data->auth_data) {
                                smb2_set_error(smb2, "can not init auth data %s", smb2_get_error(smb2));
                                smb2_close_context(smb2);
                                return;
                        }
                        smb2->connect_data = c_data;
                        if  (!smb2->session_id) {
                                smb2->session_id = server->session_counter++;
                        }
                }

                if (krb5_session_reply(smb2, c_data->auth_data,
                                         req->security_buffer,
                                         req->security_buffer_length,
                                         &more_processing_needed)) {
                        smb2_close_context(smb2);
                        return;
                }

                /* alloc a pdu for next request */
                if (more_processing_needed) {
                        smb2->next_pdu = smb2_allocate_pdu(smb2, SMB2_SESSION_SETUP,
                                                smb2_session_setup_request_cb, cb_data);
                        smb2->session_id = server->session_counter++;
                } else {
                        smb2->next_pdu = smb2_allocate_pdu(smb2, SMB2_TREE_CONNECT,
                                                smb2_general_client_request_cb, cb_data);
                }
                rep.security_buffer_length =
                        krb5_get_output_token_length(c_data->auth_data);
                rep.security_buffer =
                        krb5_get_output_token_buffer(c_data->auth_data);

                if (!krb5_session_get_session_key(smb2, c_data->auth_data)) {
                        have_valid_session_key = 1;
                }
        }
#endif
        if (smb2->sign && have_valid_session_key == 0) {
                smb2_close_context(smb2);
                smb2_set_error(smb2, "Signing required by server. Session "
                               "Key is not available %s",
                               smb2_get_error(smb2));
                smb2_close_context(smb2);
                return;
        }

        if (smb2->sign)  {
                /* Derive the signing key from session key
                * This is based on negotiated protocol
                */
                smb2_create_signing_key(smb2);
        }

        if (server->allow_anonymous &&
                         ((smb2->user == NULL || smb2->user[0] == '\0')||
                         (smb2->password == NULL || smb2->password[0] == '\0'))) {
                rep.session_flags |= SMB2_SESSION_FLAG_IS_GUEST;
        }

        if (!pdu) {
                pdu = smb2_cmd_session_setup_reply_async(smb2, &rep, NULL, cb_data);
                if (pdu == NULL) {
                        smb2_set_error(smb2, "can not alloc pdu for session setup reply");
                        smb2_close_context(smb2);
                        return;
                }
                if (more_processing_needed) {
                        pdu->header.status = SMB2_STATUS_MORE_PROCESSING_REQUIRED;
                }
                else {
                        if (server->handlers && server->handlers->session_established) {
                                ret = server->handlers->session_established(server, smb2);
                                if (ret) {
                                        smb2_set_error(smb2, "server session start handler failed");
                                        smb2_close_context(smb2);
                                        return;
                                }
                        }
                        else {
                                pdu = smb2_cmd_error_reply_async(smb2,
                                                &err, SMB2_SESSION_SETUP,
                                                SMB2_STATUS_NOT_IMPLEMENTED, NULL, cb_data);
                        }
                }
        }
        if (!smb2->next_pdu) {
                smb2_set_error(smb2, "can not alloc pdu for authorization session setup request");
                smb2_close_context(smb2);
                return;
        }

        smb2_set_pdu_message_id(smb2, pdu, smb2->message_id);
        smb2_queue_pdu(smb2, pdu);
        smb3_update_preauth_hash(smb2, pdu->out.niov, &pdu->out.iov[0]);
}

static void
smb2_negotiate_request_cb(struct smb2_context *smb2, int status, void *command_data, void *cb_data)
{
        struct connect_data *c_data = cb_data;
        struct smb2_server *server = c_data->server_context;
        struct smb2_negotiate_request *req = command_data;
        struct smb2_negotiate_reply rep;
        struct smb2_error_reply err;
        struct smb2_pdu *pdu;
        uint16_t dialects[SMB2_NEGOTIATE_MAX_DIALECTS];
        int dialect_count;
        int d;
        int dialect_index;
        struct smb2_timeval now;
        int will_sign = 0;

        memset(&rep, 0, sizeof(rep));
        memset(&err, 0, sizeof(err));
        smb2_set_error(smb2, "");

        if (status != SMB2_STATUS_SUCCESS) {
                /* context is being destroyed */
                return;
        }

        /* assume we can always reply */
        smb2->credits = 128;

        /* negotiate highest version in request dialects */
        switch (smb2->version) {
        case SMB2_VERSION_ANY:
                dialect_count = 5;
                dialects[0] = SMB2_VERSION_0202;
                dialects[1] = SMB2_VERSION_0210;
                dialects[2] = SMB2_VERSION_0300;
                dialects[3] = SMB2_VERSION_0302;
                dialects[4] = SMB2_VERSION_0311;
                break;
        case SMB2_VERSION_ANY2:
                dialect_count = 2;
                dialects[0] = SMB2_VERSION_0202;
                dialects[1] = SMB2_VERSION_0210;
                break;
        case SMB2_VERSION_ANY3:
                dialect_count = 3;
                dialects[0] = SMB2_VERSION_0300;
                dialects[1] = SMB2_VERSION_0302;
                dialects[2] = SMB2_VERSION_0311;
                break;
        case SMB2_VERSION_0202:
        case SMB2_VERSION_0210:
        case SMB2_VERSION_0300:
        case SMB2_VERSION_0302:
        case SMB2_VERSION_0311:
        default:
                dialect_count = 1;
                dialects[0] = smb2->version;
                break;
        }

        if (req && smb2->pdu->header.command != SMB1_NEGOTIATE) {
                if (req->dialect_count == 0) {
                        /* windows does this crap */
                        /* alloc a pdu for another negotiate  request */
                        smb2->next_pdu = smb2_allocate_pdu(smb2, SMB2_NEGOTIATE, smb2_negotiate_request_cb, cb_data);
                        if (!smb2->next_pdu) {
                                smb2_set_error(smb2, "can not alloc pdu for second negotiate request");
                                smb2_close_context(smb2);
                        }
                        pdu = smb2_cmd_error_reply_async(smb2,
                                        &err, SMB2_NEGOTIATE, SMB2_STATUS_INVALID_PARAMETER, NULL, cb_data);
                        if (pdu == NULL) {
                                return;
                        }
                        smb2_set_pdu_message_id(smb2, pdu, smb2->message_id);
                        smb2_queue_pdu(smb2, pdu);
                        return;
                }
                smb2->dialect = 0;
                for (dialect_index = req->dialect_count - 1;
                               dialect_index >= 0; dialect_index--) {
                        for (d = dialect_count - 1; d >= 0; d--) {
                                if (dialects[d] == req->dialects[dialect_index]) {
                                        smb2->dialect = dialects[d];
                                        break;
                                }
                        }
                        if (smb2->dialect != 0) {
                                break;
                        }
                }

                if (dialect_index < 0) {
                        smb2_set_error(smb2, "No common dialects for protocol");
                        smb2_close_context(smb2);
                        return;
                }

                smb2_set_client_guid(smb2, req->client_guid);
        }
        else {
                /* an smb1-negotiate, list all dialects */
                smb2->dialect = SMB2_VERSION_WILDCARD;
        }

        smb3_init_preauth_hash(smb2);
        smb3_update_preauth_hash(smb2, smb2->in.niov - 1, &smb2->in.iov[1]);

        if (req) {
                rep.capabilities = SMB2_GLOBAL_CAP_LARGE_MTU;
                if (smb2->version == SMB2_VERSION_ANY  ||
                    smb2->version == SMB2_VERSION_ANY3 ||
                    smb2->version == SMB2_VERSION_0300 ||
                    smb2->version == SMB2_VERSION_0302 ||
                    smb2->version == SMB2_VERSION_0311) {
                        rep.capabilities |= SMB2_GLOBAL_CAP_ENCRYPTION;
                }

                /* update the context with the client capabilities */
                if (smb2->dialect > SMB2_VERSION_0202) {
                        if (req->capabilities & SMB2_GLOBAL_CAP_LARGE_MTU) {
                                smb2->supports_multi_credit = 1;
                        }
                }

                if (smb2->seal && (smb2->dialect == SMB2_VERSION_0300 ||
                                   smb2->dialect == SMB2_VERSION_0302)) {
                        if(!(req->capabilities & SMB2_GLOBAL_CAP_ENCRYPTION)) {
                                smb2_set_error(smb2, "Encryption requested but client "
                                               "does not support encryption.");
                                smb2_close_context(smb2);
                                return;
                        }
                }

                if (req->security_mode & SMB2_NEGOTIATE_SIGNING_REQUIRED) {
                        will_sign = 1;
                }

                if (!server->allow_anonymous ||
                                (smb2->password && smb2->password[0])) {
                        if (smb2->dialect == SMB2_VERSION_0210) {
                                /* smb2.1 requires signing if enabled on both sides
                                 * regardless of what the flags say */
                                will_sign = 1;
                        }
                        if (smb2->dialect >= SMB2_VERSION_0311) {
                                /* smb3.1.1 requires signing if enabled on both sides
                                 * regardless of what the flags say */
                                will_sign = 1;
                        }
                }

                if (smb2->seal) {
                        smb2->sign = 0;
                } else if (will_sign) {
                        if (server->signing_enabled) {
                                smb2->sign = 1;
                        } else {
                                smb2_set_error(smb2, "Signing required but server "
                                               "does not have signing enabled.");
                                smb2_close_context(smb2);
                                return;
                        }
                }
        }

        rep.security_mode = (server->signing_enabled ? SMB2_NEGOTIATE_SIGNING_ENABLED : 0)|
                             (smb2->sign ? SMB2_NEGOTIATE_SIGNING_REQUIRED : 0);
        memcpy(rep.server_guid, server->guid, 16); /* TODO */
        rep.max_transact_size  = smb2->max_transact_size;;
        rep.max_read_size      = smb2->max_read_size;
        rep.max_write_size     = smb2->max_write_size;
        rep.dialect_revision   = smb2->dialect;
        rep.cypher             = smb2->cypher;

        /* remember negotiated capabilites and security mode */
        smb2->capabilities = rep.capabilities;
        smb2->security_mode = rep.security_mode;

        now.tv_sec = time(NULL);
        now.tv_usec = 0;

        rep.system_time = smb2_timeval_to_win(&now);
        now.tv_sec = 0;
        rep.server_start_time = smb2_timeval_to_win(&now);

        rep.security_buffer_length = smb2_spnego_create_negotiate_reply_blob(
                                        smb2,
                                        (smb2->sec == SMB2_SEC_UNDEFINED || smb2->sec == SMB2_SEC_NTLMSSP),
                                        (void*)&rep.security_buffer);

        pdu = smb2_cmd_negotiate_reply_async(smb2, &rep, NULL, cb_data);
        if (rep.security_buffer) {
                free(rep.security_buffer);
        }
        if (pdu == NULL) {
                return;
        }

        smb2_set_pdu_message_id(smb2, pdu, smb2->message_id);
        smb2_queue_pdu(smb2, pdu);
        smb3_update_preauth_hash(smb2, pdu->out.niov, &pdu->out.iov[0]);

        if (req) {
                /* alloc a pdu for session request */
                smb2->next_pdu = smb2_allocate_pdu(smb2, SMB2_SESSION_SETUP, smb2_session_setup_request_cb, cb_data);
                if (!smb2->next_pdu) {
                        smb2_set_error(smb2, "can not alloc pdu for session setup request");
                        smb2_close_context(smb2);
                }
        }
        else {
                /* alloc a pdu for another negotiate  request */
                smb2->next_pdu = smb2_allocate_pdu(smb2, SMB2_NEGOTIATE, smb2_negotiate_request_cb, cb_data);
                if (!smb2->next_pdu) {
                        smb2_set_error(smb2, "can not alloc pdu for second negotiate request");
                        smb2_close_context(smb2);
                }
        }
}

static int
accept_cb(const int fd, void *cb_data)
{
        int err = -1;
        struct smb2_context **psmb2 = (struct smb2_context**)cb_data;
        struct smb2_context *smb2;

        if (!psmb2) {
                return -EINVAL;
        }

        *psmb2 = NULL;

        smb2 = smb2_init_context();
        if (smb2 == NULL) {
                err = -ENOMEM;
        }
        else {
                *psmb2 = smb2;
                /* put client fd into connecting fd array (todo? for now just set fd) */
                smb2->fd = fd;
                err = 0;
        }

        return err;
}

int smb2_serve_port_async(const int fd, const int to_msecs, struct smb2_context **smb2)
{
        int err = -1;

        err = smb2_accept_connection_async(fd, to_msecs, accept_cb, smb2);
        return err;
}

int smb2_serve_port(struct smb2_server *server, const int max_connections, smb2_client_connection cb, void *cb_data)
{
        struct smb2_context *smb2;
        struct connect_data *c_data = cb_data;
        fd_set rfds, wfds;
        int maxfd;
        int ready;
        short events;
        struct timeval timeout;
        int err = -1;
        static const char *default_domain = "WORKGROUP";
        time_t now;
#ifdef HAVE_LIBKRB5
        static time_t credential_renewal_time = 0;
#endif

        if (!server->max_transact_size) {
                server->max_transact_size = 0x100000;
                server->max_read_size = 0x100000;
                server->max_write_size = 0x100000;
        }
        if (!server->guid[0]) {
                memcpy(server->guid, "libsmb2-srvrguid", 16);
        }
        if (!server->hostname[0]) {
                gethostname(server->hostname, sizeof(server->hostname));
        }
        if (!server->domain[0]) {
                strncpy(server->domain, default_domain,
                               MIN(sizeof(server->domain),strlen(default_domain) + 1));
        }

#ifdef HAVE_LIBKRB5
        err = krb5_init_server_credentials(server, server->keytab_path);
        if (err) {
                return err;
        }
#endif
        err = smb2_bind_and_listen(server->port, max_connections, &server->fd);
        if (err != 0) {
                return err;
        }
        server->session_counter = 0x1234;

        do {
                /* select on the file descriptors of all active client connections and our server socket
                   for the first readable event
                */
                FD_ZERO(&rfds);
                FD_ZERO(&wfds);
                FD_SET(server->fd, &rfds);
                maxfd = server->fd;

                for (smb2 = smb2_active_contexts(); smb2; smb2 = smb2->next) {
                        if (SMB2_VALID_SOCKET(smb2_get_fd(smb2))) {
                                events = smb2_which_events(smb2);
                                if (events) {
                                        if (events & POLLIN) {
                                                FD_SET(smb2_get_fd(smb2), &rfds);
                                        }
                                        if (events & POLLOUT) {
                                                FD_SET(smb2_get_fd(smb2), &wfds);
                                        }
                                        if (smb2_get_fd(smb2) > (t_socket)maxfd) {
                                                maxfd = smb2_get_fd(smb2);
                                        }
                                }
                        }
                }

                /* 100ms select timeout to allow periodic pdu timeouts */
                timeout.tv_sec = 0;
                timeout.tv_usec = 100000;

                ready = select(
                            maxfd + 1,
                            &rfds,
                            &wfds,
                            NULL,
                            (timeout.tv_sec > 0 || timeout.tv_usec >= 0) ? &timeout : NULL
                           );

                if (ready > 0) {
                        now = time(NULL);

                        /* for each client context ready to read, process that context */
                        for (smb2 = smb2_active_contexts(); smb2; smb2 = smb2->next) {
                                if (SMB2_VALID_SOCKET(smb2_get_fd(smb2)) && FD_ISSET(smb2_get_fd(smb2), &rfds)) {
                                        if (smb2_service(smb2, POLLIN) < 0) {
                                                smb2_set_error(smb2, "smb2_service (in) failed with : "
                                                                "%s", smb2_get_error(smb2));
                                                smb2_close_context(smb2);
                                        }
                                        err = 0;
                                }
                                if (SMB2_VALID_SOCKET(smb2_get_fd(smb2)) && FD_ISSET(smb2_get_fd(smb2), &wfds)) {
                                        if (smb2_service(smb2, POLLOUT) < 0) {
                                                smb2_set_error(smb2, "smb2_service (out) failed with : "
                                                                "%s", smb2_get_error(smb2));
                                                smb2_close_context(smb2);
                                        }
                                }
                                if (!SMB2_VALID_SOCKET(smb2->fd) && ((time(NULL) - now) > (smb2->timeout)))
                                {
                                        smb2_set_error(smb2, "Timeout expired and no connection exists");
                                        smb2_close_context(smb2);
                                }
                                if (smb2->timeout) {
                                        smb2_timeout_pdus(smb2);
                                }
                        }

                        if (FD_ISSET(server->fd, &rfds)) {
                                smb2 = NULL;
                                err = smb2_serve_port_async(server->fd, 10, &smb2);
                                if (!err && smb2) {
                                        c_data = calloc(1, sizeof(struct connect_data));
                                        if (c_data == NULL) {
                                                smb2_set_error(smb2, "Failed to allocate connect_data");
                                                smb2_close_context(smb2);
                                        }
                                        c_data->server_context = server;
                                        smb2->connect_data = c_data;

                                        /* alloc a pdu for first server request */
                                        smb2->pdu = smb2_allocate_pdu(smb2, SMB2_NEGOTIATE, smb2_negotiate_request_cb, c_data);
                                        if (!smb2->pdu) {
                                                smb2_set_error(smb2, "can not alloc pdu for request");
                                                smb2_close_context(smb2);
                                        }
                                        /* got a new smb2 context with a connection, enlist it and tell user */
                                        smb2->owning_server = server;
                                        smb2->max_transact_size = server->max_transact_size;
                                        smb2->max_read_size     = server->max_read_size;
                                        smb2->max_write_size    = server->max_write_size;

                                        if (cb) {
                                                cb(smb2, cb_data);
                                        }
                                }
                                else if (err) {
                                        break;
                                }
                        }

                        /* cull connection-less servers here (servers who's client has disconnected)
                         * do only one per iteration since active list changes on destroy
                         */
                        for (smb2 = smb2_active_contexts(); smb2; smb2 = smb2->next) {
                                if (smb2_is_server(smb2)) {
                                        if (!SMB2_VALID_SOCKET(smb2_get_fd(smb2))) {
                                                if (server->handlers && server->handlers->destruction_event) {
                                                        server->handlers->destruction_event(server, smb2);
                                                }
                                                smb2_destroy_context(smb2);
                                                break;
                                        }
                                }
                                /* client connections are destroyed when they timeout or get disconnected */
                        }
                }
#ifdef HAVE_LIBKRB5
                /* renew kerberos credentials daily */
                time(&now);

                if (credential_renewal_time < now) {
                        credential_renewal_time = now + 60*60*24;

                        err = krb5_renew_server_credentials(server);
                }
#endif
        }
        while (err == 0);

        close(server->fd);
        server->fd = -1;

        while (smb2_active_contexts()) {
                smb2 = smb2_active_contexts();
                smb2_destroy_context(smb2);
        }
#ifdef HAVE_LIBKRB5
        krb5_free_server_credentials(server);
#endif
        return err;
}

