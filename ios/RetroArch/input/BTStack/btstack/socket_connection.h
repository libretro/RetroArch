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
 *  socket_connection.h
 *
 *  Created by Matthias Ringwald on 6/6/09.
 */

#pragma once

#include <btstack/run_loop.h>

#include <stdint.h>


/** opaque connection type */
typedef struct connection connection_t;

/** 
 * create socket data_source for socket specified by launchd configuration
 */
int socket_connection_create_launchd(void);

/** 
 * create socket for incoming tcp connections
 */
int socket_connection_create_tcp(int port);

/** 
 * create socket for incoming unix domain connections
 */
int socket_connection_create_unix(char *path);

/**
 * close socket connection to BTdaemon 
 */
int socket_connection_close_tcp(connection_t *connection);

/**
 * create TCP socket connection to BTdaemon 
 */
connection_t * socket_connection_open_tcp(const char *address, uint16_t port);

/**
 * close TCP socket connection to BTdaemon 
 */
int socket_connection_close_tcp(connection_t *connection);

/**
 * create unix socket connection to BTdaemon 
 */
connection_t * socket_connection_open_unix(void);

/**
 * close unix connection to BTdaemon 
 */
int socket_connection_close_unix(connection_t *connection);

/**
 * set packet handler for all auto-accepted connections 
 * -- packet_callback @return: 0 == OK/NO ERROR
 */
void socket_connection_register_packet_callback( int (*packet_callback)(connection_t *connection, uint16_t packet_type, uint16_t channel, uint8_t *data, uint16_t length) );

/**
 * send HCI packet to single connection
 */
void socket_connection_send_packet(connection_t *connection, uint16_t packet_type, uint16_t channel, uint8_t *data, uint16_t size);

/**
 * send event data to all clients
 */
void socket_connection_send_packet_all(uint16_t type, uint16_t channel, uint8_t *packet, uint16_t size);

/**
 * try to dispatch packet for all "parked" connections.
 * if dispatch is successful, a connection is added again to run loop
 */
void socket_connection_retry_parked(void);


/**
 * query if at least one connection had to be parked
 */
int  socket_connection_has_parked_connections(void);

