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

#ifndef MMAL_CONNECTION_H
#define MMAL_CONNECTION_H

/** \defgroup MmalConnectionUtility Port connection utility
 * \ingroup MmalUtilities
 * The port connection utility functions can be used in place of common sequences
 * of calls to the MMAL API in order to process buffers being passed between two
 * ports.
 *
 * \section ProcessingConnectionBufferHeaders Processing connection buffer headers
 * Either in response to the client callback function being called, or simply on a
 * timer, the client will need to process the buffer headers of the connection
 * (unless tunneling is used).
 *
 * Buffer headers that are in the pool queue will need to be sent to the output port,
 * while buffer headers in the connection queue are sent to the input port. The
 * buffer headers in the connection queue may contain pixel data (the cmd field is
 * zero) or an event (the cmd field is non-zero). In general, pixel data buffer
 * headers need to be passed on, while event buffer headers are released. In the
 * case of the format changed event, mmal_connection_event_format_changed() can be
 * called before the event is released.
 *
 * Other, specialized use cases may also be implemented, such as getting and
 * immediately releasing buffer headers from the connection queue in order to
 * prevent their propagation. This could be used to drop out video, for example.
 *
 * \section TunnellingConnections Tunnelling connections
 * If the \ref MMAL_CONNECTION_FLAG_TUNNELLING flag is set when the connection is
 * created, MMAL tunneling will be used. This automates the passing of the buffer
 * headers between the output port and input port, and back again. It will also do
 * this as efficiently as possible, avoiding trips between the ARM and the VideoCore
 * if both components are implemented on the VideoCore. The consequence of this is
 * that there is no client callback made as buffer headers get transferred.
 *
 * The client can still monitor the control port of a component (usually a sink
 * component, such as video_render) for the end of stream, in order to know when to
 * dismantle the connection.
 *
 * \section ConnectionClientCallback Client callback
 * When not using tunnelling, the client callback function is called each time a
 * buffer arrives from a port (either input or output).
 *
 * \note The callback is made on a different thread from the one used by the
 * client to set up the connection, so care must be taken with thread safety.
 * One option is to raise a signal to the main client thread that queue processing
 * needs to be done, another is for the callback to perform the queue processing
 * itself.
 *
 * The client can also store an opaque pointer in the connection object, which is
 * never used by the MMAL code and is only meaningful to the client.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** \name Connection flags
 * \anchor connectionflags
 * The following flags describe the properties of the connection. */
/* @{ */
/** The connection is tunnelled. Buffer headers do not transit via the client but
 * directly from the output port to the input port. */
#define MMAL_CONNECTION_FLAG_TUNNELLING 0x1
/** Force the pool of buffer headers used by the connection to be allocated on the input port. */
#define MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT 0x2
/** Force the pool of buffer headers used by the connection to be allocated on the output port. */
#define MMAL_CONNECTION_FLAG_ALLOCATION_ON_OUTPUT 0x4
/** Specify that the connection should not modify the buffer requirements. */
#define MMAL_CONNECTION_FLAG_KEEP_BUFFER_REQUIREMENTS 0x8
/** The connection is flagged as direct. This doesn't change the behaviour of
 * the connection itself but is used by the the graph utility to specify that
 * the buffer should be sent to the input port from with the port callback. */
#define MMAL_CONNECTION_FLAG_DIRECT 0x10
/** Specify that the connection should not modify the port formats. */
#define MMAL_CONNECTION_FLAG_KEEP_PORT_FORMATS 0x20
/* @} */

/** Forward type definition for a connection */
typedef struct MMAL_CONNECTION_T MMAL_CONNECTION_T;

/** Definition of the callback used by a connection to signal back to the client
 * that a buffer header is available either in the pool or in the output queue.
 *
 * @param connection Pointer to the connection
 */
typedef void (*MMAL_CONNECTION_CALLBACK_T)(MMAL_CONNECTION_T *connection);

/** Structure describing a connection between 2 ports (1 output and 1 input port) */
struct MMAL_CONNECTION_T {

   void *user_data;           /**< Field reserved for use by the client. */
   MMAL_CONNECTION_CALLBACK_T callback; /**< Callback set by the client. */

   uint32_t is_enabled;       /**< Specifies whether the connection is enabled or not (Read Only). */

   uint32_t flags;            /**< Flags passed during the create call (Read Only). A bitwise
                               * combination of \ref connectionflags "Connection flags" values.
                               */
   MMAL_PORT_T *in;           /**< Input port used for the connection (Read Only). */
   MMAL_PORT_T *out;          /**< Output port used for the connection (Read Only). */

   MMAL_POOL_T *pool;         /**< Pool of buffer headers used by the output port (Read Only). */
   MMAL_QUEUE_T *queue;       /**< Queue for the buffer headers produced by the output port (Read Only). */

   const char *name;          /**< Connection name (Read Only). Used for debugging purposes. */

   /* Used for debug / statistics */
   int64_t time_setup;        /**< Time in microseconds taken to setup the connection. */
   int64_t time_enable;       /**< Time in microseconds taken to enable the connection. */
   int64_t time_disable;      /**< Time in microseconds taken to disable the connection. */
};

/** Create a connection between two ports.
 * The connection shall include a pool of buffer headers suitable for the current format of
 * the output port. The format of the input port shall have been set to the same as that of
 * the input port.
 * Note that connections are reference counted and creating a connection automatically
 * acquires a reference to it (released when \ref mmal_connection_destroy is called).
 *
 * @param connection The address of a connection pointer that will be set to point to the created
 * connection.
 * @param out        The output port to use for the connection.
 * @param in         The input port to use for the connection.
 * @param flags      The flags specifying which type of connection should be created.
 *    A bitwise combination of \ref connectionflags "Connection flags" values.
 * @return MMAL_SUCCESS on success.
 */
MMAL_STATUS_T mmal_connection_create(MMAL_CONNECTION_T **connection,
   MMAL_PORT_T *out, MMAL_PORT_T *in, uint32_t flags);

/** Acquire a reference on a connection.
 * Acquiring a reference on a connection will prevent a connection from being destroyed until
 * the acquired reference is released (by a call to \ref mmal_connection_destroy).
 * References are internally counted so all acquired references need a matching call to
 * release them.
 *
 * @param connection connection to acquire
 */
void mmal_connection_acquire(MMAL_CONNECTION_T *connection);

/** Release a reference on a connection
 * Release an acquired reference on a connection. Triggers the destruction of the connection when
 * the last reference is being released.
 * \note This is in fact an alias of \ref mmal_connection_destroy which is added to make client
 * code clearer.
 *
 * @param connection connection to release
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_connection_release(MMAL_CONNECTION_T *connection);

/** Destroy a connection.
 * Release an acquired reference on a connection. Only actually destroys the connection when
 * the last reference is being released.
 * The actual destruction of the connection will start by disabling it, if necessary.
 * Any pool, queue, and so on owned by the connection shall then be destroyed.
 *
 * @param connection The connection to be destroyed.
 * @return MMAL_SUCCESS on success.
 */
MMAL_STATUS_T mmal_connection_destroy(MMAL_CONNECTION_T *connection);

/** Enable a connection.
 * The format of the two ports must have been committed before calling this function,
 * although note that on creation, the connection automatically copies and commits the
 * output port's format to the input port.
 *
 * The MMAL_CONNECTION_T::callback field must have been set if the \ref MMAL_CONNECTION_FLAG_TUNNELLING
 * flag was not specified on creation. The client may also set the MMAL_CONNECTION_T::user_data
 * in order to get a pointer passed, via the connection, to the callback.
 *
 * @param connection The connection to be enabled.
 * @return MMAL_SUCCESS on success.
 */
MMAL_STATUS_T mmal_connection_enable(MMAL_CONNECTION_T *connection);

/** Disable a connection.
 *
 * @param connection The connection to be disabled.
 * @return MMAL_SUCCESS on success.
 */
MMAL_STATUS_T mmal_connection_disable(MMAL_CONNECTION_T *connection);

/** Apply a format changed event to the connection.
 * This function can be used when the client is processing buffer headers and receives
 * a format changed event (\ref MMAL_EVENT_FORMAT_CHANGED). The connection is
 * reconfigured, changing the format of the ports, the number of buffer headers and
 * the size of the payload buffers as necessary.
 *
 * @param connection The connection to which the event shall be applied.
 * @param buffer The buffer containing a format changed event.
 * @return MMAL_SUCCESS on success.
 */
MMAL_STATUS_T mmal_connection_event_format_changed(MMAL_CONNECTION_T *connection,
   MMAL_BUFFER_HEADER_T *buffer);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* MMAL_CONNECTION_H */
