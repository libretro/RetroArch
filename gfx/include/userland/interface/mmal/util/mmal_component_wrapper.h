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

#ifndef MMAL_WRAPPER_H
#define MMAL_WRAPPER_H

/** \defgroup MmalComponentWrapper utility
 * \ingroup MmalUtilities
 * The component wrapper utility functions can be used in place of common sequences
 * of calls to the MMAL API in order to control a standalone component. It hides some
 * of the complexity in using standalone components behind a fully synchronous
 * interface.
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Forward type definition for a wrapper */
typedef struct MMAL_WRAPPER_T MMAL_WRAPPER_T;

/** Definition of the callback used by a wrapper to signal back to the client
 * that a buffer header is available either in the pool or in the output queue.
 *
 * @param wrapper Pointer to the wrapper
 */
typedef void (*MMAL_WRAPPER_CALLBACK_T)(MMAL_WRAPPER_T *wrapper);

/** Structure describing a wrapper around a component */
struct MMAL_WRAPPER_T {

   void *user_data;           /**< Field reserved for use by the client. */
   MMAL_WRAPPER_CALLBACK_T callback; /**< Callback set by the client. */
   MMAL_COMPONENT_T *component;
   MMAL_STATUS_T status;

   MMAL_PORT_T *control;        /**< Control port (Read Only). */

   uint32_t    input_num;       /**< Number of input ports (Read Only). */
   MMAL_PORT_T **input;         /**< Array of input ports (Read Only). */
   MMAL_POOL_T **input_pool;    /**< Array of input pools (Read Only). */

   uint32_t    output_num;      /**< Number of output ports (Read Only). */
   MMAL_PORT_T **output;        /**< Array of output ports (Read Only). */
   MMAL_POOL_T **output_pool;   /**< Array of output pools (Read Only). */
   MMAL_QUEUE_T **output_queue; /**< Array of output queues (Read Only). */

   /* Used for debug / statistics */
   int64_t time_setup;          /**< Time in microseconds taken to setup the connection. */
   int64_t time_enable;         /**< Time in microseconds taken to enable the connection. */
   int64_t time_disable;        /**< Time in microseconds taken to disable the connection. */

};

/** Create a wrapper around a component.
 * The wrapper shall include a pool of buffer headers for each port. The pools will be suitable
 * for the current format of its associated port.
 *
 * @param wrapper    The address of a wrapper pointer that will be set to point to the created
 *                   wrapper.
 * @param name       The name of the component to create.
 * @return MMAL_SUCCESS on success.
 */
MMAL_STATUS_T mmal_wrapper_create(MMAL_WRAPPER_T **wrapper, const char *name);

/** \name MMAL wrapper flags
 * \anchor wrapperflags
 */
/* @{ */
/** The operation should be blocking */
#define MMAL_WRAPPER_FLAG_WAIT 1
/** The pool for the port should allocate memory for the payloads */
#define MMAL_WRAPPER_FLAG_PAYLOAD_ALLOCATE 2
/** The port will use shared memory payloads */
#define MMAL_WRAPPER_FLAG_PAYLOAD_USE_SHARED_MEMORY 4
/* @} */

/** Enable a port on a component wrapper.
 *
 * @param port port to enable
 * @param flags used to specify payload allocation flags for the pool
 * @return MMAL_SUCCESS on success.
 */
MMAL_STATUS_T mmal_wrapper_port_enable(MMAL_PORT_T *port, uint32_t flags);

/** Disable a port on a component wrapper.
 *
 * @param port port to disable
 * @return MMAL_SUCCESS on success.
 */
MMAL_STATUS_T mmal_wrapper_port_disable(MMAL_PORT_T *port);

/** Wait for an empty buffer to be available on a port.
 *
 * @param port port to get an empty buffer from
 * @param buffer points to the retreived buffer on return
 * @param flags specify MMAL_WRAPPER_FLAG_WAIT for a blocking operation
 * @return MMAL_SUCCESS on success.
 */
MMAL_STATUS_T mmal_wrapper_buffer_get_empty(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T **buffer, uint32_t flags);

/** Wait for a full buffer to be available on a port.
 *
 * @param port port to get a full buffer from
 * @param buffer points to the retreived buffer on return
 * @param flags specify MMAL_WRAPPER_FLAG_WAIT for a blocking operation
 * @return MMAL_SUCCESS on success.
 */
MMAL_STATUS_T mmal_wrapper_buffer_get_full(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T **buffer, uint32_t flags);

/** Cancel any ongoing blocking operation on a component wrapper.
 *
 * @param wrapper The wrapper on which to cancel operations.
 * @return MMAL_SUCCESS on success.
 */
MMAL_STATUS_T mmal_wrapper_cancel(MMAL_WRAPPER_T *wrapper);

/** Destroy a wrapper.
 * Destroys a component wrapper and any resources it owns.
 *
 * @param wrapper The wrapper to be destroyed.
 * @return MMAL_SUCCESS on success.
 */
MMAL_STATUS_T mmal_wrapper_destroy(MMAL_WRAPPER_T *wrapper);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* MMAL_WRAPPER_H */
