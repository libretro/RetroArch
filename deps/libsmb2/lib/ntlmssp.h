/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
#ifndef _GSSAPI_WRAPPER_H_
#define _GSSAPI_WRAPPER_H_

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

#ifdef __cplusplus
extern "C" {
#endif

#define NEGOTIATE_MESSAGE      0x00000001
#define CHALLENGE_MESSAGE      0x00000002
#define AUTHENTICATION_MESSAGE 0x00000003

struct auth_data;

struct auth_data *
ntlmssp_init_context(const char *user,
                     const char *password,
                     const char *domain,
                     const char *workstation,
                     const char *client_challenge);

void
ntlmssp_destroy_context(struct auth_data *auth);

void
ntlmssp_set_spnego_wrapping(struct auth_data *auth, int wrap);

int
ntlmssp_get_spnego_wrapping(struct auth_data *auth);

int
ntlmssp_get_message_type(struct smb2_context *smb2,
                      uint8_t *ntlmssp_buffer, int len,
                      int suppress_errors,
                      uint32_t *message_type,
                      uint8_t **ntlmssp_ptr, int *ntlmssp_len,
                      int *is_wrapped);

int
ntlmssp_generate_blob(struct smb2_server *server, struct smb2_context *smb2, time_t t,
                      struct auth_data *auth_data,
                      unsigned char *input_buf, int input_len,
                      unsigned char **output_buf, uint16_t *output_len);

int
ntlmssp_authenticate_blob(struct smb2_server *server, struct smb2_context *smb2,
                      struct auth_data *auth_data,
                      unsigned char *input_buf, int input_len);

int
ntlmssp_get_authenticated(struct auth_data *auth);

int
ntlmssp_get_session_key(struct auth_data *auth, uint8_t **key, uint8_t *key_size);

#ifdef __cplusplus
}
#endif

#endif /* _GSSAPI_WRAPPER_H_ */
