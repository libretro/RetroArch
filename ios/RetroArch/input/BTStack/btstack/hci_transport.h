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
 *  hci_transport.h
 *
 *  HCI Transport API -- allows BT Daemon to use different transport protcols 
 *
 *  Created by Matthias Ringwald on 4/29/09.
 *
 */
#pragma once

#include <stdint.h>
#include <btstack/run_loop.h>

#if defined __cplusplus
extern "C" {
#endif
    
/* HCI packet types */
typedef struct {
    int    (*open)(void *transport_config);
    int    (*close)(void *transport_config);
    int    (*send_packet)(uint8_t packet_type, uint8_t *packet, int size);
    void   (*register_packet_handler)(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size));
    const char * (*get_transport_name)(void);
    // custom extension for UART transport implementations
    int    (*set_baudrate)(uint32_t baudrate);
    // support async transport layers, e.g. IRQ driven without buffers
    int    (*can_send_packet_now)(uint8_t packet_type);
} hci_transport_t;

typedef struct {
    const char *device_name;
    uint32_t   baudrate_init; // initial baud rate
    uint32_t   baudrate_main; // = 0: same as initial baudrate
    int   flowcontrol; // 
} hci_uart_config_t;


// inline various hci_transport_X.h files
extern hci_transport_t * hci_transport_h4_instance(void);
extern hci_transport_t * hci_transport_h4_dma_instance(void);
extern hci_transport_t * hci_transport_h4_iphone_instance(void);
extern hci_transport_t * hci_transport_h5_instance(void);
extern hci_transport_t * hci_transport_usb_instance(void);

// support for "enforece wake device" in h4 - used by iOS power management
extern void hci_transport_h4_iphone_set_enforce_wake_device(char *path);
    
#if defined __cplusplus
}
#endif

