/*
 * Copyright (C) 2009-2012 by Matthias Ringwald
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at btstack@ringwald.ch
 *
 */

/*
 *  l2cap_signaling.h
 *
 *  Created by Matthias Ringwald on 7/23/09.
 */

#pragma once

#include <stdint.h>
#include <stdarg.h>
#include <btstack/utils.h>
#include <btstack/hci_cmds.h>

typedef enum {
    COMMAND_REJECT = 1,
    CONNECTION_REQUEST,
    CONNECTION_RESPONSE,
    CONFIGURE_REQUEST,
    CONFIGURE_RESPONSE,
    DISCONNECTION_REQUEST,
    DISCONNECTION_RESPONSE,
    ECHO_REQUEST,
    ECHO_RESPONSE,
    INFORMATION_REQUEST,
    INFORMATION_RESPONSE
} L2CAP_SIGNALING_COMMANDS;

uint16_t l2cap_create_signaling_internal(uint8_t * acl_buffer,hci_con_handle_t handle, L2CAP_SIGNALING_COMMANDS cmd, uint8_t identifier, va_list argptr);
uint8_t  l2cap_next_sig_id(void);
uint16_t l2cap_next_local_cid(void);

