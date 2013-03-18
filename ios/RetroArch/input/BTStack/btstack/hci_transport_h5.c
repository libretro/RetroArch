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
 *  hci_transport_h5.c
 *
 *  HCI Transport API implementation for basic H5 protocol
 *
 *  Created by Matthias Ringwald on 4/29/09.
 */

#include <termios.h>  /* POSIX terminal control definitions */
#include <fcntl.h>    /* File control definitions */
#include <unistd.h>   /* UNIX standard function definitions */
#include <stdio.h>
#include <string.h>

#include "hci.h"
#include "hci_transport.h"
#include "hci_dump.h"


typedef struct hci_transport_h5 {
    hci_transport_t transport;
    data_source_t *ds;
} hci_transport_h5_t;

// h5 slip state machine
typedef enum {
	unknown = 1,
	decoding,
	x_c0,
	x_db
} H5_STATE;

typedef struct h5_slip {
	state_t state;
	uint16_t length;
	uint8_t data[HCI_ACL_BUFFER_SIZE];
} h5_slip_t;

// Global State
static h5_slip_t read_sm;
static h5_slip_t write_sm;
static int bt_filedesc = 0;

// single instance
static hci_transport_h5_t * hci_transport_h5 = NULL;

static int  h5_process(struct data_source *ds);
static void dummy_handler(uint8_t packet_type, uint8_t *packet, int size); 
static      hci_uart_config_t *hci_uart_config;

static  void (*packet_handler)(uint8_t packet_type, uint8_t *packet, int size) = dummy_handler;

// prototypes
static int    h5_open(void *transport_config){
    hci_uart_config = (hci_uart_config_t*) transport_config;
    struct termios toptions;
    int fd = open(hci_uart_config->device_name, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1)  {
        perror("init_serialport: Unable to open port ");
        perror(hci_uart_config->device_name);
        return -1;
    }
    
    if (tcgetattr(fd, &toptions) < 0) {
        perror("init_serialport: Couldn't get term attributes");
        return -1;
    }
    speed_t brate = hci_uart_config->baudrate; // let you override switch below if needed
    switch(hci_uart_config->baudrate) {
        case 57600:  brate=B57600;  break;
        case 115200: brate=B115200; break;
#ifdef B230400
        case 230400: brate=B230400; break;
#endif
#ifdef B460800
        case 460800: brate=B460800; break;
#endif
#ifdef B921600
        case 921600: brate=B921600; break;
#endif
    }
    cfsetispeed(&toptions, brate);
    cfsetospeed(&toptions, brate);
    
    // 8N1
    toptions.c_cflag &= ~PARENB;
    toptions.c_cflag &= ~CSTOPB;
    toptions.c_cflag &= ~CSIZE;
    toptions.c_cflag |= CS8;

    if (hci_uart_config->flowcontrol) {
        // with flow control
        toptions.c_cflag |= CRTSCTS;
    } else {
        // no flow control
        toptions.c_cflag &= ~CRTSCTS;
    }
    
    toptions.c_cflag |= CREAD | CLOCAL;  // turn on READ & ignore ctrl lines
    toptions.c_iflag &= ~(IXON | IXOFF | IXANY); // turn off s/w flow ctrl
    
    toptions.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // make raw
    toptions.c_oflag &= ~OPOST; // make raw
    
    // see: http://unixwiz.net/techtips/termios-vmin-vtime.html
    toptions.c_cc[VMIN]  = 1;
    toptions.c_cc[VTIME] = 0;
    
    if( tcsetattr(fd, TCSANOW, &toptions) < 0) {
        perror("init_serialport: Couldn't set term attributes");
        return -1;
    }
    
    // set up data_source
    hci_transport_h5->ds = malloc(sizeof(data_source_t));
    if (!hci_transport_h5) return -1;
    hci_transport_h5->ds->fd = fd;
    hci_transport_h5->ds->process = h5_process;
    run_loop_add_data_source(hci_transport_h5->ds);
    
    // init state machine
	h5_slip_init( &read_sm);
	h5_slip_init( &write_sm);
    return 0;
}

static int    h5_close(){
    // first remove run loop handler
	run_loop_remove_data_source(hci_transport_h5->ds);
    
    // close device 
    close(hci_transport_h5->ds->fd);
    free(hci_transport_h5->ds);
    
    // free struct
    hci_transport_h5->ds = NULL;
    return 0;
}

static int    h5_send_packet(uint8_t packet_type, uint8_t *packet, int size){
    if (hci_transport_h5->ds == NULL) return -1;
    if (hci_transport_h5->ds->fd == 0) return -1;
    char *data = (char*) packet;

    hci_dump_packet( (uint8_t) packet_type, 0, packet, size);
    
    write(hci_transport_h5->ds->fd, &packet_type, 1);
    while (size > 0) {
        int bytes_written = write(hci_transport_h5->ds->fd, data, size);
        if (bytes_written < 0) {
            usleep(5000);
            continue;
        }
        data += bytes_written;
        size -= bytes_written;
    }
    return 0;
}

static void   h5_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, int size)){
    packet_handler = handler;
}

static void h5_slip_init( h5_slip_t * sm){
	sm->state = unknown;
}

static void h5_slip_process( h5_slip_t * sm, uint8_t input, uint8_t in){
	uint8_t type;
	switch (sm->state) {
		case unknown:
			if (input == 0xc0){
				sm->length = 0;
				sm->state  = x_c0;
			}
			break;
		case x_c0:
			switch (input ){
				case 0xc0:
					break;
				case 0xdb:
					sm->state = x_db;
					break;
				default:
					sm->data[sm->length] = input;
					++sm->length;
					sm->state   = decoding;
					break;
			}
			break;
		case decoding:
			switch (input ){
				case 0xc0:
					sm->state = unknown;
					// packet done - check if valid HCI packet
					if (sm->length < 6) break;
					type = sm->data[1] & 0x0f;
					if (type < 1 || type > 4) break;
					hci_dump_packet( type, in, &sm->data[4], sm->length-4-2); // -4 header, -2 crc for reliable
					switch (type) {
						case HCI_ACL_DATA_PACKET:
							acl_packet_handler( &sm->data[4], sm->length-4-2);
							break;
						case HCI_EVENT_PACKET:
							event_packet_handler( &sm->data[4], sm->length-4-2);
							break;
						default:
							break;
					}
					break;
				case 0xdb:
					sm->state = x_db;
					break;
				default:
					sm->data[sm->length] = input;
					++sm->length;
					break;
			}
			break;
		case x_db:
			switch (input) {
				case 0xdc:
					sm->data[sm->length] = 0xc0;
					++sm->length;
					sm->state = decoding;
					break;
				case 0xdd:
					sm->data[sm->length] = 0xdb;
					++sm->length;
					sm->state = decoding;
					break;
				default:
					sm->state = unknown;
					break;
			}
			break;
		default:
			break;
	}
}


static int    h5_process(struct data_source *ds) {
    if (hci_transport_h5->ds->fd == 0) return -1;

    // read up to bytes_to_read data in
	uint8_t data;
	while (1) {
		int bytes_read = read(hci_transport_h5->ds->fd, &data, 1);
		if (bytes_read < 1) break;
		h5_slip_process( &read_sm, data, 1);
	};
    return 0;
}

static const char * h5_get_transport_name(){
    return "H5";
}

static void dummy_handler(uint8_t *packet, int size){
}

// get h5 singleton
hci_transport_t * hci_transport_h5_instance() {
    if (hci_transport_h5 == NULL) {
        hci_transport_h5 = malloc( sizeof(hci_transport_h5_t));
        hci_transport_h5->ds                                      = NULL;
        hci_transport_h5->transport.open                          = h5_open;
        hci_transport_h5->transport.close                         = h5_close;
        hci_transport_h5->transport.send_packet                   = h5_send_packet;
        hci_transport_h5->transport.register_packet_handler       = h5_register_event_packet_handler;
        hci_transport_h5->transport.get_transport_name            = h5_get_transport_name;
        hci_transport_h5->transport.set_baudrate                  = NULL;
        hci_transport_h5->transport.can_send_packet_now           = NULL;
    }
    return (hci_transport_t *) hci_transport_h5;
}
