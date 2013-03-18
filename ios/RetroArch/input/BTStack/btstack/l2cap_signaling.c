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

#include "l2cap_signaling.h"

#include <string.h>

static char *l2cap_signaling_commands_format[] = {
"D",    // 0x01 command reject: reason {cmd not understood (0), sig MTU exceeded (2:max sig MTU), invalid CID (4:req CID)}, data len, data
"22",   // 0x02 connection request: PSM, Source CID
"2222", // 0x03 connection response: Dest CID, Source CID, Result, Status
"22D",  // 0x04 config request: Dest CID, Flags, Configuration options
"222D", // 0x05 config response: Source CID, Flags, Result, Configuration options
"22",   // 0x06 disconection request: Dest CID, Source CID
"22",   // 0x07 disconection response: Dest CID, Source CID
"D",    // 0x08 echo request: Data
"D",    // 0x09 echo response: Data
"2",    // 0x0a information request: InfoType {1=Connectionless MTU, 2=Extended features supported}
"22D",  // 0x0b information response: InfoType, Result, Data
};

uint8_t   sig_seq_nr  = 0xff;
uint16_t  source_cid  = 0x40;

uint8_t l2cap_next_sig_id(void){
    if (sig_seq_nr == 0xff) {
        sig_seq_nr = 1;
    } else {
        sig_seq_nr++;
    }
    return sig_seq_nr;
}

uint16_t l2cap_next_local_cid(void){
    return source_cid++;
}

uint16_t l2cap_create_signaling_internal(uint8_t * acl_buffer, hci_con_handle_t handle, L2CAP_SIGNALING_COMMANDS cmd, uint8_t identifier, va_list argptr){
    
    // 0 - Connection handle : PB=10 : BC=00 
    bt_store_16(acl_buffer, 0, handle | (2 << 12) | (0 << 14));
    // 6 - L2CAP channel = 1
    bt_store_16(acl_buffer, 6, 1);
    // 8 - Code
    acl_buffer[8] = cmd;
    // 9 - id (!= 0 sequentially)
    acl_buffer[9] = identifier;
    
    // 12 - L2CAP signaling parameters
    uint16_t pos = 12;
    const char *format = l2cap_signaling_commands_format[cmd-1];
    uint16_t word;
    uint8_t * ptr;
    while (*format) {
        switch(*format) {
            case '1': //  8 bit value
            case '2': // 16 bit value
                word = va_arg(argptr, int);
                // minimal va_arg is int: 2 bytes on 8+16 bit CPUs
                acl_buffer[pos++] = word & 0xff;
                if (*format == '2') {
                    acl_buffer[pos++] = word >> 8;
                }
                break;
            case 'D': // variable data. passed: len, ptr
                word = va_arg(argptr, int);
                ptr  = va_arg(argptr, uint8_t *);
                memcpy(&acl_buffer[pos], ptr, word);
                pos += word;
                break;
            default:
                break;
        }
        format++;
    };
    va_end(argptr);
    
    // Fill in various length fields: it's the number of bytes following for ACL lenght and l2cap parameter length
    // - the l2cap payload length is counted after the following channel id (only payload) 
    
    // 2 - ACL length
    bt_store_16(acl_buffer, 2,  pos - 4);
    // 4 - L2CAP packet length
    bt_store_16(acl_buffer, 4,  pos - 6 - 2);
    // 10 - L2CAP signaling parameter length
    bt_store_16(acl_buffer, 10, pos - 12);
    
    return pos;
}
