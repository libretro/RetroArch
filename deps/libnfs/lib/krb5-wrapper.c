/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2024 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

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

#ifdef HAVE_LIBKRB5

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

#include <krb5/krb5.h>
#if __APPLE__
#include <GSS/GSS.h>
#else
#include <gssapi/gssapi_krb5.h>
#include <gssapi/gssapi.h>
#endif
#include <stdio.h>

#include "slist.h"
#include "libnfs.h"
#include "libnfs-raw.h"
#include "libnfs-raw-mount.h"
#include "libnfs-raw-portmap.h"
#include "libnfs-private.h"

#include "krb5-wrapper.h"

#ifndef discard_const
#define discard_const(ptr) ((void *)((intptr_t)(ptr)))
#endif

void
krb5_free_auth_data(struct private_auth_data *auth)
{
        uint32_t maj, min;

        /* Delete context */
        if (auth->context) {
                maj = gss_delete_sec_context(&min, &auth->context,
                                             &auth->output_token);
                if (maj != GSS_S_COMPLETE) {
                        /* No logging, yet. Do we care? */
                }
        }

        gss_release_cred(&min, &auth->cred);
        gss_release_buffer(&min, &auth->output_token);

        if (auth->target_name) {
                gss_release_name(&min, &auth->target_name);
        }

        if (auth->user_name) {
                gss_release_name(&min, &auth->user_name);
        }

        free(auth->g_server);
        free(auth);
}

static char *
display_status(int type, uint32_t err)
{
        gss_buffer_desc text;
        uint32_t msg_ctx;
        char *msg, *tmp;
        uint32_t maj, min;

        msg = NULL;
        msg_ctx = 0;
        do {
                maj = gss_display_status(&min, err, type,
                                         GSS_C_NO_OID, &msg_ctx, &text);
                if (maj != GSS_S_COMPLETE) {
                        return msg;
                }

                tmp = NULL;
                if (msg) {
                        tmp = msg;
                        min = asprintf(&msg, "%s, %*s", msg,
                                       (int)text.length, (char *)text.value);
                } else {
                        min = asprintf(&msg, "%*s", (int)text.length,
                                       (char *)text.value);
                }
                if (min == -1) return tmp;
                free(tmp);
                gss_release_buffer(&min, &text);
        } while (msg_ctx != 0);

        return msg;
}

//qqq add back use_cached_credentials
struct private_auth_data *
krb5_auth_init(struct rpc_context *rpc,
               const char *server,
               const char *user_name,
               int wanted_sec)
{
        struct private_auth_data *auth_data;
        gss_buffer_desc target = GSS_C_EMPTY_BUFFER;
        uint32_t maj, min;
        gss_buffer_desc user;
        //char user_principal[2048];
        char *nc_password = NULL;
        //gss_buffer_desc passwd;
        gss_OID_set_desc mechOidSet;
        gss_OID_set_desc wantMech;

        auth_data = calloc(1, sizeof(struct private_auth_data));
        if (auth_data == NULL) {
                rpc_set_error(rpc, "Failed to allocate private_auth_data");
                return NULL;
        }
        auth_data->context = GSS_C_NO_CONTEXT;
        auth_data->wanted_sec = wanted_sec;

        if (asprintf(&auth_data->g_server, "nfs@%s", server) < 0) {
                krb5_free_auth_data(auth_data);
                rpc_set_error(rpc, "Failed to allocate server string");
                return NULL;
        }

        target.value = auth_data->g_server;
        target.length = strlen(auth_data->g_server);

        maj = gss_import_name(&min, &target, GSS_C_NT_HOSTBASED_SERVICE,
                              &auth_data->target_name);

        if (maj != GSS_S_COMPLETE) {
                krb5_free_auth_data(auth_data);
                krb5_set_gss_error(rpc, "gss_import_name", maj, min);
                return NULL;
        }

        user.value = discard_const(user_name);
        user.length = strlen(user_name);

        /* create a name for the user */
        maj = gss_import_name(&min, &user, GSS_C_NT_USER_NAME,
                              &auth_data->user_name);

        if (maj != GSS_S_COMPLETE) {
                krb5_free_auth_data(auth_data);
                krb5_set_gss_error(rpc, "gss_import_name", maj, min);
                return NULL;
        }

        /* TODO: the proper mechanism (SPNEGO vs NTLM vs KRB5) should be
         * selected based on the SMB negotiation flags */
        #ifdef __APPLE__
        auth_data->mech_type = GSS_SPNEGO_MECHANISM;
        #else
        auth_data->mech_type = &spnego_mech_krb5;
        #endif
        auth_data->cred = GSS_C_NO_CREDENTIAL;

        /* Create creds for the user */
        mechOidSet.count = 1;
        #ifdef __APPLE__
        mechOidSet.elements = discard_const(GSS_SPNEGO_MECHANISM);
        #else
        mechOidSet.elements = discard_const(&gss_mech_spnego);
        #endif
        
        maj = gss_acquire_cred(&min, auth_data->user_name, 0,
                               &mechOidSet, GSS_C_INITIATE,
                               &auth_data->cred, NULL, NULL);

        if (maj != GSS_S_COMPLETE) {
                krb5_free_auth_data(auth_data);
                krb5_set_gss_error(rpc, "gss_acquire_cred", maj, min);
                return NULL;
        }

        #ifndef __APPLE__ /* gss_set_neg_mechs is not defined on macOS/iOS. */
        if (rpc->sec != RPC_SEC_UNDEFINED) {
                wantMech.count = 1;
                
                switch (rpc->sec) {
                case RPC_SEC_KRB5:
                case RPC_SEC_KRB5I:
                case RPC_SEC_KRB5P:
                        wantMech.elements = discard_const(&spnego_mech_krb5);
                        break;
                case RPC_SEC_UNDEFINED:
                        ;
                }

                maj = gss_set_neg_mechs(&min, auth_data->cred, &wantMech);
                if (GSS_ERROR(maj)) {
                        krb5_free_auth_data(auth_data);
                        krb5_set_gss_error(rpc, "gss_set_neg_mechs", maj, min);
                        return NULL;
                }
        }
        #endif

        if (nc_password) {
                free(nc_password);
                nc_password = NULL;
        }
        return auth_data;
}

void
krb5_set_gss_error(struct rpc_context *rpc, char *func,
                   uint32_t maj, uint32_t min)
{
        char *err_maj = display_status(GSS_C_GSS_CODE, maj);
        char *err_min = display_status(GSS_C_MECH_CODE, min);
        rpc_set_error(rpc, "%s: (%s, %s)", func, err_maj, err_min);
        free(err_min);
        free(err_maj);
}

int
krb5_auth_request(struct rpc_context *rpc,
                     struct private_auth_data *auth_data,
                     unsigned char *buf, int len)
{
        uint32_t maj, min;
        gss_buffer_desc *input_token = NULL;
        gss_buffer_desc token = GSS_C_EMPTY_BUFFER;

        if (buf) {
                /* release the previous token */
                gss_release_buffer(&min, &auth_data->output_token);
                auth_data->output_token.length = 0;
                auth_data->output_token.value = NULL;

                token.value = buf;
                token.length = len;
                input_token = &token;
        }

        /* TODO return -errno instead of just -1 */
        /* NOTE: this call is not async, a helper thread should be used if that
         * is an issue */
        auth_data->req_flags = GSS_C_MUTUAL_FLAG;
        if (auth_data->wanted_sec == RPC_SEC_KRB5I) {
                auth_data->req_flags |= GSS_C_INTEG_FLAG;
        }
        if (auth_data->wanted_sec == RPC_SEC_KRB5P) {
                auth_data->req_flags |= GSS_C_CONF_FLAG;
        }
        if (auth_data->cred == GSS_C_NO_CREDENTIAL) {
                input_token=GSS_C_NO_BUFFER;
        }
        maj = gss_init_sec_context(&min, auth_data->cred,
                                   &auth_data->context,
                                   auth_data->target_name,
                                   discard_const(auth_data->mech_type),
                                   auth_data->req_flags,
                                   GSS_C_INDEFINITE,
                                   GSS_C_NO_CHANNEL_BINDINGS,
                                   input_token,
                                   NULL,
                                   &auth_data->output_token,
                                   NULL,
                                   NULL);
        rpc->gss_context = auth_data->context;

        /* GSS_C_MUTUAL_FLAG expects the acceptor to send a token so
         * a second call to gss_init_sec_context is required to complete the session.
         * A second call is required even if the first call returns GSS_S_COMPLETE
         */
        if (maj & GSS_S_CONTINUE_NEEDED) {
            return 0;
        }
        if (GSS_ERROR(maj)) {
                krb5_set_gss_error(rpc, "gss_init_sec_context", maj, min);
                return -1;
        }

        return 0;
}

int
krb5_get_output_token_length(struct private_auth_data *auth_data)
{
        return auth_data->output_token.length;
}

unsigned char *
krb5_get_output_token_buffer(struct private_auth_data *auth_data)
{
        return auth_data->output_token.value;
}

#endif /* HAVE_LIBKRB5 */
