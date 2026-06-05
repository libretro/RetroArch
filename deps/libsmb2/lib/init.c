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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_UNISTD_H
#include <sys/unistd.h>
#endif

#include <stdarg.h>
#include <stdio.h>

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef HAVE_SYS_ERRNO_H
#include <sys/errno.h>
#endif

#include "compat.h"

#include "smb2.h"
#include "libsmb2.h"
#include "libsmb2-private.h"
#include "slist.h"

#define MAX_URL_SIZE 1024

/* This is a list of all allocated contexts so servers can iterate over each
 * it should probably be moved to the server context and a callback registered
 * here to tell the server when a context is destroyed, but this works
 */
static struct smb2_context *active_contexts;

static int
smb2_parse_args(struct smb2_context *smb2, const char *args)
{
        while (args && *args != 0) {
                char *next, *value;

                next = strchr(args, '&');
                if (next) {
                        *(next++) = '\0';
                }

                value = strchr(args, '=');
                if (value) {
                        *(value++) = '\0';
                }

                if (!strcmp(args, "seal")) {
                        smb2->seal = 1;
                } else if (!strcmp(args, "sign")) {
                        smb2->sign = 1;
                } else if (!strcmp(args, "ndr3264")) {
                        smb2->ndr = 0;
                } else if (!strcmp(args, "ndr32")) {
                        smb2->ndr = 1;
                } else if (!strcmp(args, "ndr64")) {
                        smb2->ndr = 2;
                } else if (!strcmp(args, "le")) {
                        smb2->endianness = 0;
                } else if (!strcmp(args, "be")) {
                        smb2->endianness = 1;
                } else if (!strcmp(args, "sec")) {
                        if(!strcmp(value, "krb5")) {
                                smb2->sec = SMB2_SEC_KRB5;
                        } else if(!strcmp(value, "krb5cc")) {
                                smb2->sec = SMB2_SEC_KRB5;
                                smb2->use_cached_creds = 1;
                        } else if (!strcmp(value, "ntlmssp")) {
                                smb2->sec = SMB2_SEC_NTLMSSP;
                        } else {
                                smb2_set_error(smb2, "Unknown sec= argument: "
                                               "%s", value);
                                return -1;
                        }
                } else if (!strcmp(args, "vers")) {
                        if(!strcmp(value, "2")) {
                                smb2->version = SMB2_VERSION_ANY2;
                        } else if(!strcmp(value, "3")) {
                                smb2->version = SMB2_VERSION_ANY3;
                        } else if(!strcmp(value, "2.02")) {
                                smb2->version = SMB2_VERSION_0202;
                        } else if(!strcmp(value, "2.10")) {
                                smb2->version = SMB2_VERSION_0210;
                        } else if(!strcmp(value, "3.0") ||
                                  !strcmp(value, "3.00")) {
                                smb2->version = SMB2_VERSION_0300;
                        } else if(!strcmp(value, "3.02")) {
                                smb2->version = SMB2_VERSION_0302;
                        } else if(!strcmp(value, "3.1.1")) {
                                smb2->version = SMB2_VERSION_0311;
                        } else {
                                smb2_set_error(smb2, "Unknown vers= argument: "
                                               "%s", value);
                                return -1;
                        }
                } else if (!strcmp(args, "timeout")) {
                        smb2->timeout = (int)strtol(value, NULL, 10);
                } else {
                        smb2_set_error(smb2, "Unknown argument: %s", args);
                        return -1;
                }
                args = next;
        }

        if (smb2->seal) {
                switch (smb2->version) {
                case SMB2_VERSION_ANY:
                        smb2->version = SMB2_VERSION_ANY3;
                        break;
                case SMB2_VERSION_ANY3:
                case SMB2_VERSION_0300:
                case SMB2_VERSION_0302:
                case SMB2_VERSION_0311:
                        break;
                default:
                        smb2_set_error(smb2, "Can only use seal with SMB3");
                        return -1;
                }
        }

        return 0;
}

struct smb2_url *smb2_parse_url(struct smb2_context *smb2, const char *url)
{
        struct smb2_url *u;
        char *ptr, *tmp, str[MAX_URL_SIZE];
        char *args;
        char *shared_folder;
        size_t len_shared_folder;

        if (strncmp(url, "smb://", 6)) {
                smb2_set_error(smb2, "URL does not start with 'smb://'");
                return NULL;
        }
        if (strlen(url + 6) >= MAX_URL_SIZE) {
                smb2_set_error(smb2, "URL is too long");
                return NULL;
        }

        strncpy(str, url + 6, MAX_URL_SIZE);
        args = strchr(str, '?');
        if (args) {
                *(args++) = '\0';
                if (smb2_parse_args(smb2, args) != 0) {
                        return NULL;
                }
        }

        u = calloc(1, sizeof(struct smb2_url));
        if (u == NULL) {
                smb2_set_error(smb2, "Failed to allocate smb2_url");
                return NULL;
        }

        ptr = str;

        shared_folder = strchr(ptr, '/');
        if (!shared_folder) {
                smb2_set_error(smb2, "Wrong URL format");
                return NULL;
        }
        len_shared_folder = strlen(shared_folder);

        /* domain */
        if ((tmp = strchr(ptr, ';')) != NULL && strlen(tmp) > len_shared_folder) {
                *(tmp++) = '\0';
                u->domain = strdup(ptr);
                ptr = tmp;
        }
        /* user */
        if ((tmp = strchr(ptr, '@')) != NULL && strlen(tmp) > len_shared_folder) {
                *(tmp++) = '\0';
                u->user = strdup(ptr);
                ptr = tmp;
        }
        /* server */
        if ((tmp = strchr(ptr, '/')) != NULL) {
                *(tmp++) = '\0';
                u->server = strdup(ptr);
                ptr = tmp;
        }

        /* Do we just have a share or do we have both a share and an object */
        tmp = strchr(ptr, '/');

        /* We only have a share */
        if (tmp == NULL) {
                u->share = strdup(ptr);
                return u;
        }

        /* we have both share and object path */
        *(tmp++) = '\0';
        u->share = strdup(ptr);
        u->path = strdup(tmp);

        return u;
}

void smb2_destroy_url(struct smb2_url *url)
{
        if (url == NULL) {
                return;
        }
        free(discard_const(url->domain));
        free(discard_const(url->user));
        free(discard_const(url->server));
        free(discard_const(url->share));
        free(discard_const(url->path));
        free(url);
}


struct smb2_context *smb2_init_context(void)
{
        struct smb2_context *smb2;
        char buf[1024] _U_;
        int i, ret;
        static int ctr;

        srandom((unsigned)time(NULL) ^ getpid() ^ ctr++);

        smb2 = calloc(1, sizeof(struct smb2_context));
        if (smb2 == NULL) {
                return NULL;
        }

        ret = getlogin_r(buf, sizeof(buf));
        smb2_set_user(smb2, ret == 0 ? buf : "Guest");
        smb2->fd = SMB2_INVALID_SOCKET;
        smb2->connecting_fds = NULL;
        smb2->connecting_fds_count = 0;
        smb2->addrinfos = NULL;
        smb2->next_addrinfo = NULL;
        smb2->sec = SMB2_SEC_UNDEFINED;
        smb2->version = SMB2_VERSION_ANY;
        smb2->ndr = 1;

        for (i = 0; i < 8; i++) {
                smb2->client_challenge[i] = random() & 0xff;
        }
        for (i = 0; i < SMB2_SALT_SIZE; i++) {
                smb2->salt[i] = random() & 0xff;
        }

        snprintf(smb2->client_guid, 16, "libsmb2-%d", (int)random());

        smb2->session_key = NULL;

        SMB2_LIST_ADD(&active_contexts, smb2);

        return smb2;
}

void smb2_destroy_context(struct smb2_context *smb2)
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
        else {
                smb2_close_connecting_fds(smb2);
        }

        while (smb2->outqueue) {
                struct smb2_pdu *pdu = smb2->outqueue;

                smb2->outqueue = pdu->next;
                if (pdu->cb) {
                        pdu->cb(smb2, SMB2_STATUS_SHUTDOWN, NULL, pdu->cb_data);
                }
                smb2_free_pdu(smb2, pdu);
        }
        if (smb2->pdu) {
                struct smb2_pdu *pdu = smb2->pdu;

                if (pdu->cb) {
                        pdu->cb(smb2, SMB2_STATUS_SHUTDOWN, NULL, pdu->cb_data);
                }
                smb2_free_pdu(smb2, smb2->pdu);
        }
        while (smb2->waitqueue) {
                struct smb2_pdu *pdu = smb2->waitqueue;

                smb2->waitqueue = pdu->next;
                if (pdu->cb) {
                        pdu->cb(smb2, SMB2_STATUS_SHUTDOWN, NULL, pdu->cb_data);
                }
                if (pdu == smb2->pdu) {
                        smb2->pdu = NULL;
                }
                smb2_free_pdu(smb2, pdu);
        }
        smb2_free_iovector(smb2, &smb2->in);

        if (smb2->connect_cb) {
           smb2->connect_cb(smb2, SMB2_STATUS_CANCELLED,
                         NULL, smb2->connect_data);
           smb2->connect_cb = NULL;
        }
        free(smb2->session_key);
        smb2->session_key = NULL;

        free(discard_const(smb2->user));
        free(discard_const(smb2->server));
        free(discard_const(smb2->share));
        free(discard_const(smb2->password));
        free(discard_const(smb2->domain));
        free(discard_const(smb2->workstation));
        free(smb2->enc);

#ifdef HAVE_LIBKRB5
        if (smb2->cred_handle) {
                OM_uint32 min;
                (void) gss_release_cred(&min, &smb2->cred_handle);
                smb2->cred_handle = NULL;
        }
#endif
        if (smb2->connect_data) {
            free_c_data(smb2, smb2->connect_data);  /* sets smb2->connect_data to NULL */
        }

        SMB2_LIST_REMOVE(&active_contexts, smb2);
        free(smb2);
}

struct smb2_context *smb2_active_contexts(void)
{
        return active_contexts;
}

int smb2_context_active(struct smb2_context *smb2)
{
        struct smb2_context *context = active_contexts;

        while (context) {
                if (smb2 == context) {
                        return 1;
                }
                context = context->next;
        }
        return 0;
}

void smb2_free_iovector(struct smb2_context *smb2, struct smb2_io_vectors *v)
{
        int i;

        for (i = 0; i < v->niov; i++) {
                if (v->iov[i].free) {
                        v->iov[i].free(v->iov[i].buf);
                }
        }
        v->niov = 0;
        v->total_size = 0;
        v->num_done = 0;
}

struct smb2_iovec *smb2_add_iovector(struct smb2_context *smb2,
                                    struct smb2_io_vectors *v,
                                    uint8_t *buf, size_t len,
                                    void (*free_cb)(void *))
{
                struct smb2_iovec *iov;
                /* Bounds checking */
                if (v->niov >= SMB2_MAX_VECTORS) {
                        smb2_set_error(smb2, "Too many I/O vectors");
                        /* Avoid leaks for caller-provided buffers */
                        if (free_cb && buf) {
                                free_cb(buf);
                        }
                        return NULL;
                }

                iov = &v->iov[v->niov];
                v->iov[v->niov].buf = buf;
                v->iov[v->niov].len = len;
                v->iov[v->niov].free = free_cb;
        v->total_size += len;
        v->niov++;

        return iov;
}

#ifndef _IOP
static void smb2_set_error_string(struct smb2_context *smb2, const char * error_string, va_list args)
{
        char errstr[MAX_ERROR_SIZE] = {0};
#ifdef _XBOX
        if (_vsnprintf(errstr, MAX_ERROR_SIZE, error_string, args) < 0) {
#else
        if (vsnprintf(errstr, MAX_ERROR_SIZE, error_string, args) < 0) {
#endif
                strncpy(errstr, "could not format error string!",
                        MAX_ERROR_SIZE);
        }
        strncpy(smb2->error_string, errstr, MAX_ERROR_SIZE);
}
#endif /* _IOP */


void smb2_set_error(struct smb2_context *smb2, const char *error_string, ...)
{
#ifndef _IOP
        va_list ap;

        if (!smb2)
                return;
        if (!error_string || !*error_string)
                smb2->nterror = 0;
        va_start(ap, error_string);
        smb2_set_error_string(smb2, error_string, ap);
        va_end(ap);
        if(smb2->error_cb) {
                smb2->error_cb(smb2, smb2_get_error(smb2));
        }
#endif
}

void smb2_register_error_callback(struct smb2_context *smb2,
                    smb2_error_cb error_cb)
{
        smb2->error_cb = error_cb;
}

void smb2_set_nterror(struct smb2_context *smb2, int nterror, const char *error_string, ...)
{
        if (!smb2)
                return;
#ifndef _IOP
        {
                va_list ap;

                va_start(ap, error_string);
                smb2_set_error_string(smb2, error_string, ap);
                va_end(ap);
        }
#endif
        smb2->nterror = nterror;
}

const char *smb2_get_error(struct smb2_context *smb2)
{
        return smb2 ? smb2->error_string : "";
}

int smb2_get_nterror(struct smb2_context *smb2)
{
        return smb2 ? smb2->nterror : 0;
}

void smb2_set_client_guid(struct smb2_context *smb2, const uint8_t guid[SMB2_GUID_SIZE])
{
        memcpy(smb2->client_guid, guid, SMB2_GUID_SIZE);
}

const char *smb2_get_client_guid(struct smb2_context *smb2)
{
        return smb2->client_guid;
}

uint16_t smb2_get_dialect(struct smb2_context *smb2)
{
        return smb2->dialect;
}

void smb2_set_security_mode(struct smb2_context *smb2, uint16_t security_mode)
{
        smb2->security_mode = security_mode;
}

void smb2_set_password_from_file(struct smb2_context *smb2)
{
#if !defined(_XBOX) && !defined(_IOP) && !defined(__amigaos4__) && !defined(__AMIGA__) && !defined(__AROS__)
        char *name = NULL;
        FILE *fh;
        char buf[256];
        char *domain, *user, *password;
        int finished;

#ifdef _MSC_UWP
/* GetEnvironmentVariable is not available for UWP up to 10.0.16299 SDK */
#if defined(NTDDI_WIN10_RS3) && (NTDDI_VERSION >= NTDDI_WIN10_RS3)
        uint32_t name_len = GetEnvironmentVariableA("NTLM_USER_FILE", NULL, 0);
        if (name_len > 0) {
                name = (char*)malloc(name_len + 1);
                if (name == NULL) {
                        return;
                }
                GetEnvironmentVariableA("NTLM_USER_FILE", name, name_len);
        }
#endif
#else
        name = getenv("NTLM_USER_FILE");
#endif
        if (name == NULL || smb2->user == NULL) {
#ifdef _MSC_UWP
                free(name);
#endif
                return;
        }
        fh = fopen(name, "r");
#ifdef _MSC_UWP
        free(name);
#endif
        if (!fh) {
            return;
        }

        smb2_set_password(smb2, NULL);

        /* Match on domain if available, else match on server name */
        while (!feof(fh)) {
                if (fgets(buf, 256, fh) == NULL) {
                        break;
                }
                buf[255] = 0;
                finished = 0;
                while (!finished) {
                        switch (buf[strlen(buf) - 1]) {
                        case '\n':
                                buf[strlen(buf) - 1] = 0;
                        default:
                                finished = 1;
                        }
                        if (strlen(buf) == 0) {
                                break;
                        }
                }
                if (buf[0] == 0) {
                        break;
                }
                domain = buf;
                user = strchr(domain, ':');
                if (user == NULL) {
                        continue;
                }
                *user++ = 0;
                password = strchr(user, ':');                
                if (password == NULL) {
                        continue;
                }
                *password++ = 0;

                if (domain[0] && smb2->domain && !strcmp(smb2->domain, domain)) {
                        smb2_set_password(smb2, password);
                        fclose(fh);
                        return;
                }
                if (domain[0] && smb2->server && !strcmp(smb2->server, domain)) {
                        smb2_set_password(smb2, password);
                        fclose(fh);
                        return;
                }
                if (!domain[0]) {
                        /* wildcard. grab this as default but continue looking for a better match */
                        smb2_set_password(smb2, password);
                }
        }
        fclose(fh);
#endif /* !defined(_XBOX) && !defined(_IOP) &&  ... */
}

void smb2_set_user(struct smb2_context *smb2, const char *user)
{
        if (smb2->user) {
                free(discard_const(smb2->user));
                smb2->user = NULL;
        }
        if (user == NULL) {
                return;
        }
        smb2->user = strdup(user);
        smb2_set_password_from_file(smb2);
}

const char *smb2_get_user(struct smb2_context *smb2)
{
        if (smb2 && smb2->user) {
                return smb2->user;
        }
        return NULL;
}

const char *smb2_get_workstation(struct smb2_context *smb2)
{
        if (smb2 && smb2->workstation) {
                return smb2->workstation;
        }
        return NULL;
}

void smb2_set_password(struct smb2_context *smb2, const char *password)
{
        if (smb2->password) {
                free(discard_const(smb2->password));
                smb2->password = NULL;
        }
        if (password == NULL) {
                return;
        }
        smb2->password = strdup(password);
}

void smb2_set_domain(struct smb2_context *smb2, const char *domain)
{
        if (smb2->domain) {
                free(discard_const(smb2->domain));
                smb2->domain = NULL;
        }
        if (domain == NULL) {
                return;
        }
        smb2->domain = strdup(domain);
        smb2_set_password_from_file(smb2);
}

const char *smb2_get_domain(struct smb2_context *smb2)
{
        if (smb2 && smb2->domain) {
                return smb2->domain;
        }
        return NULL;
}

void smb2_set_workstation(struct smb2_context *smb2, const char *workstation)
{
        if (smb2->workstation) {
                free(discard_const(smb2->workstation));
                smb2->workstation = NULL;
        }
        if (workstation == NULL) {
                return;
        }
        smb2->workstation = strdup(workstation);
}

void smb2_set_opaque(struct smb2_context *smb2, void *opaque)
{
        smb2->opaque = opaque;
}

void *smb2_get_opaque(struct smb2_context *smb2)
{
        return smb2->opaque;
}

void smb2_set_seal(struct smb2_context *smb2, int val)
{
        smb2->seal = val;
}

void smb2_set_sign(struct smb2_context *smb2, int val)
{
        smb2->sign = val;
}

void smb2_set_authentication(struct smb2_context *smb2, int val)
{
        smb2->sec = (enum smb2_sec)val;
}

void smb2_set_timeout(struct smb2_context *smb2, int seconds)
{
        smb2->timeout = seconds;
}

void smb2_set_version(struct smb2_context *smb2,
                      enum smb2_negotiate_version version)
{
        smb2->version = version;
}

void smb2_get_libsmb2Version(struct smb2_libversion *smb2_ver)
{
        smb2_ver->major_version = LIBSMB2_MAJOR_VERSION;
        smb2_ver->minor_version = LIBSMB2_MINOR_VERSION;
        smb2_ver->patch_version = LIBSMB2_MAJOR_VERSION;
}

void smb2_set_passthrough(struct smb2_context *smb2,
                      int passthrough)
{
        smb2->passthrough = passthrough;
}

void smb2_get_passthrough(struct smb2_context *smb2,
                      int *passthrough)
{
        *passthrough = smb2->passthrough;
}

void smb2_set_oplock_or_lease_break_callback(struct smb2_context *smb2,
                    smb2_oplock_or_lease_break_cb cb)
{
        smb2->oplock_or_lease_break_cb = cb;
}

#ifdef HAVE_LIBKRB5
int smb2_delegate_credentials(struct smb2_context *in, struct smb2_context *out)
{
        if (in && out && in->cred_handle) {
                out->cred_handle = in->cred_handle;
                in->cred_handle = NULL;
                return 0;
        } else {
                return -1;
        }
}
#else
int smb2_delegate_credentials(struct smb2_context *in, struct smb2_context *out)
{
        return -1;
}
#endif

