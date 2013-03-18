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
 *  bt_control.h
 *
 *  BT Control API -- allows BT Daemon to initialize and control differnt hardware
 *
 *  Created by Matthias Ringwald on 5/19/09.
 *
 */

#pragma once

#include <stdint.h>

typedef enum {
    POWER_WILL_SLEEP = 1,
    POWER_WILL_WAKE_UP
} POWER_NOTIFICATION_t;

typedef struct {
    int          (*on)   (void *config);  // <-- turn BT module on and configure
    int          (*off)  (void *config);  // <-- turn BT module off
    int          (*sleep)(void *config);  // <-- put BT module to sleep    - only to be called after ON
    int          (*wake) (void *config);  // <-- wake BT module from sleep - only to be called after SLEEP
    int          (*valid)(void *config);  // <-- test if hardware can be supported
    const char * (*name) (void *config);  // <-- return hardware name

    /** support for UART baud rate changes - cmd has to be stored in hci_cmd_buffer
     * @return have command
     */
    int          (*baudrate_cmd)(void * config, uint32_t baudrate, uint8_t *hci_cmd_buffer); 
    
    /** support custom init sequences after RESET command - cmd has to be stored in hci_cmd_buffer
      * @return have command
      */
    int          (*next_cmd)(void *config, uint8_t * hci_cmd_buffer); 

    void         (*register_for_power_notifications)(void (*cb)(POWER_NOTIFICATION_t event));

    void         (*hw_error)(void); 
} bt_control_t;
