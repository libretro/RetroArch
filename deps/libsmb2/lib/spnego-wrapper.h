/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
#ifndef SPEGNO_WRAPPER_H
#define SPEGNO_WRAPPER_H 1

/*
   Copyright (C) 2024 by Brian Dodge <bdodge09@gmail.com>
   Copyright (C) 2024 by André Guilherme <andregui17@outlook.com>

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

#define SPNEGO_MECHANISM_KRB5       (0x0001)
#define SPNEGO_MECHANISM_NTLMSSP    (0x0002)

int smb2_spnego_create_negotiate_reply_blob(struct smb2_context *smb2,
                int allow_ntlmssp,
                void **neg_init_token);

int smb2_spnego_wrap_gssapi(struct smb2_context *smb2,
                const uint8_t *ntlmssp_token,
                const int token_len,
                void **blob);

int smb2_spnego_wrap_ntlmssp_challenge(struct smb2_context *smb2,
                const uint8_t *ntlmssp_token,
                const int token_len, void **neg_targ_token);

int smb2_spnego_wrap_ntlmssp_auth(struct smb2_context *smb2,
                const uint8_t *ntlmssp_token,
                const int token_len, void **neg_targ_token);

int smb2_spnego_wrap_authenticate_result(struct smb2_context *smb2,
                const int authorized_ok, void **blob);

int smb2_spnego_unwrap_gssapi(struct smb2_context *smb2,
                const uint8_t *spnego, const int spnego_len,
                const int suppress_errors,
                uint8_t **token, uint32_t *mechanisms);

int smb2_spnego_unwrap_blob(struct smb2_context *smb2,
                const uint8_t *spnego,
                const int spnego_len,
                const int suppress_errors,
                uint8_t **response_token,
                uint32_t *mechanisms);

#ifdef __cplusplus
}
#endif

#endif /* SPEGNO_WRAPPER_H */

