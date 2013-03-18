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
 *  hci_dump.c
 *
 *  Dump HCI trace in various formats:
 *
 *  - BlueZ's hcidump format
 *  - Apple's PacketLogger
 *  - stdout hexdump
 *
 *  Created by Matthias Ringwald on 5/26/09.
 */

#include "config.h"

#include "hci_dump.h"
#include "hci.h"
#include "hci_transport.h"
#include <btstack/hci_cmds.h>

#ifndef EMBEDDED
#include <fcntl.h>        // open
#include <arpa/inet.h>    // hton..
#include <unistd.h>       // write 
#include <stdio.h>
#include <time.h>
#include <sys/time.h>     // for timestamps
#include <sys/stat.h>     // for mode flags
#include <stdarg.h>       // for va_list
#endif

// BLUEZ hcidump
typedef struct {
	uint16_t	len;
	uint8_t		in;
	uint8_t		pad;
	uint32_t	ts_sec;
	uint32_t	ts_usec;
    uint8_t     packet_type;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif 
hcidump_hdr;

// APPLE PacketLogger
typedef struct {
	uint32_t	len;
	uint32_t	ts_sec;
	uint32_t	ts_usec;
	uint8_t		type;   // 0xfc for note
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
pktlog_hdr;

#ifndef EMBEDDED
static int dump_file = -1;
static int dump_format;
static hcidump_hdr header_bluez;
static pktlog_hdr  header_packetlogger;
static char time_string[40];
static int  max_nr_packets = -1;
static int  nr_packets = 0;
static char log_message_buffer[256];
#endif

void hci_dump_open(char *filename, hci_dump_format_t format){
#ifndef EMBEDDED
    dump_format = format;
    if (dump_format == HCI_DUMP_STDOUT) {
        dump_file = fileno(stdout);
    } else {
        dump_file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    }
#endif
}

#ifndef EMBEDDED
void hci_dump_set_max_packets(int packets){
    max_nr_packets = packets;
}
#endif

void hci_dump_packet(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len) {
#ifndef EMBEDDED

    if (dump_file < 0) return; // not activated yet

    // don't grow bigger than max_nr_packets
    if (dump_format != HCI_DUMP_STDOUT && max_nr_packets > 0){
        if (nr_packets >= max_nr_packets){
            lseek(dump_file, 0, SEEK_SET);
            ftruncate(dump_file, 0);
            nr_packets = 0;
        }
        nr_packets++;
    }
    
    // get time
    struct timeval curr_time;
    struct tm* ptm;
    gettimeofday(&curr_time, NULL);
    
    switch (dump_format){
        case HCI_DUMP_STDOUT: {
            /* Obtain the time of day, and convert it to a tm struct. */
            ptm = localtime (&curr_time.tv_sec);
            /* Format the date and time, down to a single second. */
            strftime (time_string, sizeof (time_string), "[%Y-%m-%d %H:%M:%S", ptm);
            /* Compute milliseconds from microseconds. */
            uint16_t milliseconds = curr_time.tv_usec / 1000;
            /* Print the formatted time, in seconds, followed by a decimal point
             and the milliseconds. */
            printf ("%s.%03u] ", time_string, milliseconds);
            switch (packet_type){
                case HCI_COMMAND_DATA_PACKET:
                    printf("CMD => ");
                    break;
                case HCI_EVENT_PACKET:
                    printf("EVT <= ");
                    break;
                case HCI_ACL_DATA_PACKET:
                    if (in) {
                        printf("ACL <= ");
                    } else {
                        printf("ACL => ");
                    }
                    break;
                case LOG_MESSAGE_PACKET:
                    // assume buffer is big enough
                    packet[len] = 0;
                    printf("LOG -- %s\n", (char*) packet);
                    return;
                default:
                    return;
            }
            hexdump(packet, len);
            break;
        }
            
        case HCI_DUMP_BLUEZ:
            bt_store_16( (uint8_t *) &header_bluez.len, 0, 1 + len);
            header_bluez.in  = in;
            header_bluez.pad = 0;
            bt_store_32( (uint8_t *) &header_bluez.ts_sec,  0, curr_time.tv_sec);
            bt_store_32( (uint8_t *) &header_bluez.ts_usec, 0, curr_time.tv_usec);
            header_bluez.packet_type = packet_type;
            write (dump_file, &header_bluez, sizeof(hcidump_hdr) );
            write (dump_file, packet, len );
            break;
            
        case HCI_DUMP_PACKETLOGGER:
            header_packetlogger.len = htonl( sizeof(pktlog_hdr) - 4 + len);
            header_packetlogger.ts_sec =  htonl(curr_time.tv_sec);
            header_packetlogger.ts_usec = htonl(curr_time.tv_usec);
            switch (packet_type){
                case HCI_COMMAND_DATA_PACKET:
                    header_packetlogger.type = 0x00;
                    break;
                case HCI_ACL_DATA_PACKET:
                    if (in) {
                        header_packetlogger.type = 0x03;
                    } else {
                        header_packetlogger.type = 0x02;
                    }
                    break;
                case HCI_EVENT_PACKET:
                    header_packetlogger.type = 0x01;
                    break;
                case LOG_MESSAGE_PACKET:
                    header_packetlogger.type = 0xfc;
                    break;
                default:
                    return;
            }
            write (dump_file, &header_packetlogger, sizeof(pktlog_hdr) );
            write (dump_file, packet, len );
            break;
            
        default:
            break;
    }
#endif
}

void hci_dump_log(const char * format, ...){
#ifndef EMBEDDED
    va_list argptr;
    va_start(argptr, format);
    int len = vsnprintf(log_message_buffer, sizeof(log_message_buffer), format, argptr);
    hci_dump_packet(LOG_MESSAGE_PACKET, 0, (uint8_t*) log_message_buffer, len);
    va_end(argptr);
#endif    
}

void hci_dump_close(){
#ifndef EMBEDDED
    close(dump_file);
    dump_file = -1;
#endif
}

