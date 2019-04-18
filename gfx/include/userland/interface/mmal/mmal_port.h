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

#ifndef MMAL_PORT_H
#define MMAL_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup MmalPort Ports
 * Definition of a MMAL port and its associated API */
/* @{ */

#include "mmal_types.h"
#include "mmal_format.h"
#include "mmal_buffer.h"
#include "mmal_parameters.h"

/** List of port types */
typedef enum
{
   MMAL_PORT_TYPE_UNKNOWN = 0,          /**< Unknown port type */
   MMAL_PORT_TYPE_CONTROL,              /**< Control port */
   MMAL_PORT_TYPE_INPUT,                /**< Input port */
   MMAL_PORT_TYPE_OUTPUT,               /**< Output port */
   MMAL_PORT_TYPE_CLOCK,                /**< Clock port */
   MMAL_PORT_TYPE_INVALID = 0xffffffff  /**< Dummy value to force 32bit enum */

} MMAL_PORT_TYPE_T;

/** \name Port capabilities
 * \anchor portcapabilities
 * The following flags describe the capabilities advertised by a port */
/* @{ */
/** The port is pass-through and doesn't need buffer headers allocated */
#define MMAL_PORT_CAPABILITY_PASSTHROUGH                       0x01
/** The port wants to allocate the buffer payloads. This signals a preference that
 * payload allocation should be done on this port for efficiency reasons. */
#define MMAL_PORT_CAPABILITY_ALLOCATION                        0x02
/** The port supports format change events. This applies to input ports and is used
 * to let the client know whether the port supports being reconfigured via a format
 * change event (i.e. without having to disable the port). */
#define MMAL_PORT_CAPABILITY_SUPPORTS_EVENT_FORMAT_CHANGE      0x04
/* @} */

/** Definition of a port.
 * A port is the entity that is exposed by components to receive or transmit
 * buffer headers (\ref MMAL_BUFFER_HEADER_T). A port is defined by its
 * \ref MMAL_ES_FORMAT_T.
 *
 * It may be possible to override the buffer requirements of a port by using
 * the MMAL_PARAMETER_BUFFER_REQUIREMENTS parameter.
 */
typedef struct MMAL_PORT_T
{
   struct MMAL_PORT_PRIVATE_T *priv; /**< Private member used by the framework */
   const char *name;                 /**< Port name. Used for debugging purposes (Read Only) */

   MMAL_PORT_TYPE_T type;            /**< Type of the port (Read Only) */
   uint16_t index;                   /**< Index of the port in its type list (Read Only) */
   uint16_t index_all;               /**< Index of the port in the list of all ports (Read Only) */

   uint32_t is_enabled;              /**< Indicates whether the port is enabled or not (Read Only) */
   MMAL_ES_FORMAT_T *format;         /**< Format of the elementary stream */

   uint32_t buffer_num_min;          /**< Minimum number of buffers the port requires (Read Only).
                                          This is set by the component. */
   uint32_t buffer_size_min;         /**< Minimum size of buffers the port requires (Read Only).
                                          This is set by the component. */
   uint32_t buffer_alignment_min;    /**< Minimum alignment requirement for the buffers (Read Only).
                                          A value of zero means no special alignment requirements.
                                          This is set by the component. */
   uint32_t buffer_num_recommended;  /**< Number of buffers the port recommends for optimal performance (Read Only).
                                          A value of zero means no special recommendation.
                                          This is set by the component. */
   uint32_t buffer_size_recommended; /**< Size of buffers the port recommends for optimal performance (Read Only).
                                          A value of zero means no special recommendation.
                                          This is set by the component. */
   uint32_t buffer_num;              /**< Actual number of buffers the port will use.
                                          This is set by the client. */
   uint32_t buffer_size;             /**< Actual maximum size of the buffers that will be sent
                                          to the port. This is set by the client. */

   struct MMAL_COMPONENT_T *component;    /**< Component this port belongs to (Read Only) */
   struct MMAL_PORT_USERDATA_T *userdata; /**< Field reserved for use by the client */

   uint32_t capabilities;            /**< Flags describing the capabilities of a port (Read Only).
                                       * Bitwise combination of \ref portcapabilities "Port capabilities"
                                       * values.
                                       */

} MMAL_PORT_T;

/** Commit format changes on a port.
 *
 * @param port The port for which format changes are to be committed.
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_port_format_commit(MMAL_PORT_T *port);

/** Definition of the callback used by a port to send a \ref MMAL_BUFFER_HEADER_T
 * back to the user.
 *
 * @param port The port sending the buffer header.
 * @param buffer The buffer header being sent.
 */
typedef void (*MMAL_PORT_BH_CB_T)(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);

/** Enable processing on a port
 *
 * If this port is connected to another, the given callback must be NULL, while for a
 * disconnected port, the callback must be non-NULL.
 *
 * If this is a connected output port and is successfully enabled:
 * <ul>
 * <li>The port shall be populated with a pool of buffers, allocated as required, according
 * to the buffer_num and buffer_size values.
 * <li>The input port to which it is connected shall be set to the same buffer
 * configuration and then be enabled. Should that fail, the original port shall be
 * disabled.
 * </ul>
 *
 * @param port port to enable
 * @param cb callback use by the port to send a \ref MMAL_BUFFER_HEADER_T back
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_port_enable(MMAL_PORT_T *port, MMAL_PORT_BH_CB_T cb);

/** Disable processing on a port
 *
 * Disabling a port will stop all processing on this port and return all (non-processed)
 * buffer headers to the client.
 *
 * If this is a connected output port, the input port to which it is connected shall
 * also be disabled. Any buffer pool shall be released.
 *
 * @param port port to disable
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_port_disable(MMAL_PORT_T *port);

/** Ask a port to release all the buffer headers it currently has.
 *
 * Flushing a port will ask the port to send all the buffer headers it currently has
 * to the client. Flushing is an asynchronous request and the flush call will
 * return before all the buffer headers are returned to the client.
 * It is up to the client to keep a count on the buffer headers to know when the
 * flush operation has completed.
 * It is also important to note that flushing will also reset the state of the port
 * and any processing which was buffered by the port will be lost.
 *
 * \attention Flushing a connected port behaviour TBD.
 *
 * @param port The port to flush.
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_port_flush(MMAL_PORT_T *port);

/** Set a parameter on a port.
 *
 * @param port The port to which the request is sent.
 * @param param The pointer to the header of the parameter to set.
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_port_parameter_set(MMAL_PORT_T *port,
   const MMAL_PARAMETER_HEADER_T *param);

/** Get a parameter from a port.
 * The size field must be set on input to the maximum size of the parameter
 * (including the header) and will be set on output to the actual size of the
 * parameter retrieved.
 *
 * \note If MMAL_ENOSPC is returned, the parameter is larger than the size
 * given. The given parameter will have been filled up to its size and then
 * the size field set to the full parameter's size. This can be used to
 * resize the parameter buffer so that a second call should succeed.
 *
 * @param port The port to which the request is sent.
 * @param param The pointer to the header of the parameter to get.
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_port_parameter_get(MMAL_PORT_T *port,
   MMAL_PARAMETER_HEADER_T *param);

/** Send a buffer header to a port.
 *
 * @param port The port to which the buffer header is to be sent.
 * @param buffer The buffer header to send.
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_port_send_buffer(MMAL_PORT_T *port,
   MMAL_BUFFER_HEADER_T *buffer);

/** Connect an output port to an input port.
 *
 * When connected and enabled, buffers will automatically progress from the
 * output port to the input port when they become available, and released back
 * to the output port when no longer required by the input port.
 *
 * Ports can be given either way around, but one must be an output port and
 * the other must be an input port. Neither can be connected or enabled
 * already. The format of the output port will be applied to the input port
 * on connection.
 *
 * @param port One of the ports to connect.
 * @param other_port The other port to connect.
 * @return MMAL_SUCCESS on success.
 */
MMAL_STATUS_T mmal_port_connect(MMAL_PORT_T *port, MMAL_PORT_T *other_port);

/** Disconnect a connected port.
 *
 * If the port is not connected, an error will be returned. Otherwise, if the
 * ports are enabled, they will be disabled and any buffer pool created will be
 * freed.
 *
 * @param port The ports to disconnect.
 * @return MMAL_SUCCESS on success.
 */
MMAL_STATUS_T mmal_port_disconnect(MMAL_PORT_T *port);

/** Allocate a payload buffer.
 * This allows a client to allocate memory for a payload buffer based on the preferences
 * of a port. This for instance will allow the port to allocate memory which can be shared
 * between the host processor and videocore.
 *
 * See \ref mmal_pool_create_with_allocator().
 *
 * @param port         Port responsible for allocating the memory.
 * @param payload_size Size of the payload buffer which will be allocated.
 *
 * @return Pointer to the allocated memory.
 */
uint8_t *mmal_port_payload_alloc(MMAL_PORT_T *port, uint32_t payload_size);

/** Free a payload buffer.
 * This allows a client to free memory allocated by a previous call to \ref mmal_port_payload_alloc.
 *
 * See \ref mmal_pool_create_with_allocator().
 *
 * @param port         Port responsible for allocating the memory.
 * @param payload      Pointer to the memory to free.
 */
void mmal_port_payload_free(MMAL_PORT_T *port, uint8_t *payload);

/** Get an empty event buffer header from a port
 *
 * @param port The port from which to get the event buffer header.
 * @param buffer The address of a buffer header pointer, which will be set on return.
 * @param event The specific event FourCC required. See the \ref MmalEvents "pre-defined events".
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_port_event_get(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T **buffer, uint32_t event);

/* @} */

#ifdef __cplusplus
}
#endif

#endif /* MMAL_PORT_H */
