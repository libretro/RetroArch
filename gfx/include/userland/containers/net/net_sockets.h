/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef VC_NET_SOCKETS_H
#define VC_NET_SOCKETS_H

/** \file net_sockets.h
 * Abstraction layer for socket-style network communication, to enable porting
 * between platforms.
 *
 * Does not support IPv6 multicast.
 */

#include "containers/containers_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Status codes that can occur in a socket instance. */
typedef enum {
   VC_CONTAINER_NET_SUCCESS = 0,                /**< No error */
   VC_CONTAINER_NET_ERROR_GENERAL,              /**< An unrecognised error has occurred */
   VC_CONTAINER_NET_ERROR_INVALID_SOCKET,       /**< Invalid socket passed to function */
   VC_CONTAINER_NET_ERROR_NOT_ALLOWED,          /**< The operation requested is not allowed */
   VC_CONTAINER_NET_ERROR_INVALID_PARAMETER,    /**< An invalid parameter was passed in */
   VC_CONTAINER_NET_ERROR_NO_MEMORY,            /**< Failure due to lack of memory */
   VC_CONTAINER_NET_ERROR_ACCESS_DENIED,        /**< Permission denied */
   VC_CONTAINER_NET_ERROR_TOO_BIG,              /**< Too many handles already open */
   VC_CONTAINER_NET_ERROR_WOULD_BLOCK,          /**< Asynchronous operation would block */
   VC_CONTAINER_NET_ERROR_IN_PROGRESS,          /**< An operation is already in progress on this socket */
   VC_CONTAINER_NET_ERROR_IN_USE,               /**< The address/port is already in use */
   VC_CONTAINER_NET_ERROR_NETWORK,              /**< Network is unavailable */
   VC_CONTAINER_NET_ERROR_CONNECTION_LOST,      /**< The connection has been lost, closed by network, etc. */
   VC_CONTAINER_NET_ERROR_NOT_CONNECTED,        /**< The socket is not connected */
   VC_CONTAINER_NET_ERROR_TIMED_OUT,            /**< Operation timed out */
   VC_CONTAINER_NET_ERROR_CONNECTION_REFUSED,   /**< Connection was refused by target */
   VC_CONTAINER_NET_ERROR_HOST_NOT_FOUND,       /**< Target address could not be resolved */
   VC_CONTAINER_NET_ERROR_TRY_AGAIN,            /**< A temporary failure occurred that may clear */
} vc_container_net_status_t;

/** Operations that can be applied to sockets */
typedef enum {
   /** Set the buffer size used on the socket
    * arg1: uint32_t - New buffer size in bytes */
   VC_CONTAINER_NET_CONTROL_SET_READ_BUFFER_SIZE = 1,
   /** Set the timeout to be used on read operations
    * arg1: uint32_t - New timeout in milliseconds, or INFINITE_TIMEOUT_MS */
   VC_CONTAINER_NET_CONTROL_SET_READ_TIMEOUT_MS,
} vc_container_net_control_t;

/** Container Input / Output Context.
 * This is an opaque structure that defines the context for a socket instance.
 * The details of the structure are contained within the platform implementation. */
typedef struct vc_container_net_tag VC_CONTAINER_NET_T;

/** \name Socket open flags
 * The following flags can be used when opening a network socket. */
/* @{ */
typedef uint32_t vc_container_net_open_flags_t;
/** Connected stream socket, rather than connectionless datagram socket */
#define VC_CONTAINER_NET_OPEN_FLAG_STREAM 1
/** Force use of IPv4 addressing */
#define VC_CONTAINER_NET_OPEN_FLAG_FORCE_IP4 2
/** Force use of IPv6 addressing */
#define VC_CONTAINER_NET_OPEN_FLAG_FORCE_IP6 6
/** Use IPv4 broadcast address for datagram delivery */
#define VC_CONTAINER_NET_OPEN_FLAG_IP4_BROADCAST 8
/* @} */

/** Mask of bits used in forcing address type */
#define VC_CONTAINER_NET_OPEN_FLAG_FORCE_MASK 6

/** Blocks until data is available, or an error occurs.
 * Used with the VC_CONTAINER_NET_CONTROL_SET_READ_TIMEOUT_MS control operation. */
#define INFINITE_TIMEOUT_MS   0xFFFFFFFFUL

/** Opens a network socket instance.
 * The network address can be a host name, dotted IP4, hex IP6 address or NULL. Passing NULL
 * signifies the socket is either to be used as a datagram receiver or a stream server,
 * depending on the flags.
 * \ref VC_CONTAINER_NET_OPEN_FLAG_STREAM will open the socket for connected streaming. The default
 * is to use connectionless datagrams.
 * \ref VC_CONTAINER_NET_OPEN_FLAG_FORCE_IP4 will force the use of IPv4 addressing or fail to open
 * the socket. The default is to pick the first available.
 * \ref VC_CONTAINER_NET_OPEN_FLAG_FORCE_IP6 will force the use of IPv6 addressing or fail to open
 * the socket. The default is to pick the first available.
 * \ref VC_CONTAINER_NET_OPEN_FLAG_IP4_BROADCAST will use IPv4 broadcast addressing for a datagram
 * sender. Use with an IPv6 address, stream socket or datagram receiver will raise an error.
 * If the p_status parameter is not NULL, the status code will be written to it to indicate the
 * reason for failure, or VC_CONTAINER_NET_SUCCESS on success.
 * Sockets shall be bound and connected as necessary. Stream server sockets shall further need
 * to have vc_container_net_listen and vc_container_net_accept called on them before data can be transferred.
 *
 * \param address Network address or NULL.
 * \param port Network port or well-known name. This is the local port for receivers/servers.
 * \param flags Flags controlling socket type.
 * \param p_status Optional pointer to variable to receive status of operation.
 * \return The socket instance or NULL on error. */
VC_CONTAINER_NET_T *vc_container_net_open( const char *address, const char *port,
      vc_container_net_open_flags_t flags, vc_container_net_status_t *p_status );

/** Closes a network socket instance.
 * The p_ctx pointer must not be used after it has been closed.
 *
 * \param p_ctx The socket instance to close.
 * \return The status code for closing the socket. */
vc_container_net_status_t vc_container_net_close( VC_CONTAINER_NET_T *p_ctx );

/** Query the latest status of the socket.
 *
 * \param p_ctx The socket instance.
 * \return The status of the socket. */
vc_container_net_status_t vc_container_net_status( VC_CONTAINER_NET_T *p_ctx );

/** Read data from the socket.
 * The function will read up to the requested number of bytes into the buffer.
 * If there is no data immediately available to read, the function will block
 * until data arrives, an error occurs or the timeout is reached (if set).
 * When the function returns zero, the socket may have been closed, an error
 * may have occurred, a zero length datagram received, or the timeout reached.
 * Check vc_container_net_status() to differentiate.
 * Attempting to read on a datagram sender socket will trigger an error.
 *
 * \param p_ctx The socket instance.
 * \param buffer The buffer into which bytes will be read.
 * \param size The maximum number of bytes to read.
 * \return The number of bytes actually read. */
size_t vc_container_net_read( VC_CONTAINER_NET_T *p_ctx, void *buffer, size_t size );

/** Write data to the socket.
 * If the socket cannot send the requested number of bytes in one go, the function
 * will return a value smaller than size.
 * Attempting to write on a datagram receiver socket will trigger an error.
 *
 * \param p_ctx The socket instance.
 * \param buffer The buffer from which bytes will be written.
 * \param size The maximum number of bytes to write.
 * \return The number of bytes actually written. */
size_t vc_container_net_write( VC_CONTAINER_NET_T *p_ctx, const void *buffer, size_t size );

/** Start a stream server socket listening for connections from clients.
 * Attempting to use this on anything other than a stream server socket shall
 * trigger an error.
 *
 * \param p_ctx The socket instance.
 * \param maximum_connections The maximum number of queued connections to allow.
 * \return The status of the socket. */
vc_container_net_status_t vc_container_net_listen( VC_CONTAINER_NET_T *p_ctx, uint32_t maximum_connections );

/** Accept a client connection on a listening stream server socket.
 * Attempting to use this on anything other than a listening stream server socket
 * shall trigger an error.
 * When a client connection is made, the new instance representing it is returned
 * via pp_client_ctx.
 *
 * \param p_server ctx The server socket instance.
 * \param pp_client_ctx The address where the pointer to the new client's socket
 * instance is written.
 * \return The status of the socket. */
vc_container_net_status_t vc_container_net_accept( VC_CONTAINER_NET_T *p_server_ctx, VC_CONTAINER_NET_T **pp_client_ctx );

/** Non-blocking check for data being available to read.
 * If an error occurs, the function will return false and the error can be
 * obtained using socket_status().
 *
 * \param p_ctx The socket instance.
 * \return True if there is data available to read immediately. */
bool vc_container_net_is_data_available( VC_CONTAINER_NET_T *p_ctx );

/** Returns the maximum size of a datagram in bytes, for sending or receiving.
 * The limit for reading from or writing to stream sockets will generally be
 * greater than this value, although the call can also be made on such sockets.
 *
 * \param p_ctx The socket instance.
 * \return The maximum size of a datagram in bytes. */
size_t vc_container_net_maximum_datagram_size( VC_CONTAINER_NET_T *p_ctx );

/** Get the DNS name or IP address of a stream server client, if connected.
 * The length of the name will be limited by name_len, taking into account a
 * terminating NUL character.
 * Calling this function on a non-stream server instance, or one that is not
 * connected to a client, will result in an error status.
 *
 * \param p_ctx The socket instance.
 * \param name Pointer where the name should be written.
 * \param name_len Maximum number of characters to write to name.
 * \return The status of the socket. */
vc_container_net_status_t vc_container_net_get_client_name( VC_CONTAINER_NET_T *p_ctx, char *name, size_t name_len );

/** Get the port of a stream server client, if connected.
 * The port is written to the address in host order.
 * Calling this function on a non-stream server instance, or one that is not
 * connected to a client, will result in an error status.
 *
 * \param p_ctx The socket instance.
 * \param port Pointer where the port should be written.
 * \return The status of the socket. */
vc_container_net_status_t vc_container_net_get_client_port( VC_CONTAINER_NET_T *p_ctx , unsigned short *port );

/** Perform a control operation on the socket.
 * See vc_container_net_control_t for more details.
 *
 * \param p_ctx The socket instance.
 * \param operation The control operation to perform.
 * \param args Variable list of additional arguments to the operation.
 * \return The status of the socket. */
vc_container_net_status_t vc_container_net_control( VC_CONTAINER_NET_T *p_ctx, vc_container_net_control_t operation, va_list args);

/** Convert a 32-bit unsigned value from network order (big endian) to host order.
 *
 * \param value The value to be converted.
 * \return The converted value. */
uint32_t vc_container_net_to_host( uint32_t value );

/** Convert a 32-bit unsigned value from host order to network order (big endian).
 *
 * \param value The value to be converted.
 * \return The converted value. */
uint32_t vc_container_net_from_host( uint32_t value );

/** Convert a 16-bit unsigned value from network order (big endian) to host order.
 *
 * \param value The value to be converted.
 * \return The converted value. */
uint16_t vc_container_net_to_host_16( uint16_t value );

/** Convert a 16-bit unsigned value from host order to network order (big endian).
 *
 * \param value The value to be converted.
 * \return The converted value. */
uint16_t vc_container_net_from_host_16( uint16_t value );

#ifdef __cplusplus
}
#endif

#endif /* VC_NET_SOCKETS_H */
