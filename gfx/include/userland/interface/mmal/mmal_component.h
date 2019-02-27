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

#ifndef MMAL_COMPONENT_H
#define MMAL_COMPONENT_H

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup MmalComponent Components
 * Definition of a MMAL component and its associated API. A component will always expose ports
 * which it uses to send and receive data in the form of buffer headers
 * (\ref MMAL_BUFFER_HEADER_T) */
/* @{ */

#include "mmal_types.h"
#include "mmal_port.h"

struct MMAL_COMPONENT_PRIVATE_T;
typedef struct MMAL_COMPONENT_PRIVATE_T MMAL_COMPONENT_PRIVATE_T;

/** Definition of a component. */
typedef struct MMAL_COMPONENT_T
{
   /** Pointer to the private data of the module in use */
   struct MMAL_COMPONENT_PRIVATE_T *priv;

   /** Pointer to private data of the client */
   struct MMAL_COMPONENT_USERDATA_T *userdata;

   /** Component name */
   const char *name;

   /** Specifies whether the component is enabled or not */
   uint32_t is_enabled;

   /** All components expose a control port.
    * The control port is used by clients to set / get parameters that are global to the
    * component. It is also used to receive events, which again are global to the component.
    * To be able to receive events, the client needs to enable and register a callback on the
    * control port. */
   MMAL_PORT_T *control;

   uint32_t    input_num;   /**< Number of input ports */
   MMAL_PORT_T **input;     /**< Array of input ports */

   uint32_t    output_num;  /**< Number of output ports */
   MMAL_PORT_T **output;    /**< Array of output ports */

   uint32_t    clock_num;   /**< Number of clock ports */
   MMAL_PORT_T **clock;     /**< Array of clock ports */

   uint32_t    port_num;    /**< Total number of ports */
   MMAL_PORT_T **port;      /**< Array of all the ports (control/input/output/clock) */

   /** Uniquely identifies the component's instance within the MMAL
    * context / process. For debugging. */
   uint32_t id;

} MMAL_COMPONENT_T;

/** Create an instance of a component.
 * The newly created component will expose ports to the client. All the exposed ports are
 * disabled by default.
 * Note that components are reference counted and creating a component automatically
 * acquires a reference to it (released when \ref mmal_component_destroy is called).
 *
 * @param name name of the component to create, e.g. "video_decode"
 * @param component returned component
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_component_create(const char *name,
                                    MMAL_COMPONENT_T **component);

/** Acquire a reference on a component.
 * Acquiring a reference on a component will prevent a component from being destroyed until
 * the acquired reference is released (by a call to \ref mmal_component_destroy).
 * References are internally counted so all acquired references need a matching call to
 * release them.
 *
 * @param component component to acquire
 */
void mmal_component_acquire(MMAL_COMPONENT_T *component);

/** Release a reference on a component
 * Release an acquired reference on a component. Triggers the destruction of the component when
 * the last reference is being released.
 * \note This is in fact an alias of \ref mmal_component_destroy which is added to make client
 * code clearer.
 *
 * @param component component to release
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_component_release(MMAL_COMPONENT_T *component);

/** Destroy a previously created component
 * Release an acquired reference on a component. Only actually destroys the component when
 * the last reference is being released.
 *
 * @param component component to destroy
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_component_destroy(MMAL_COMPONENT_T *component);

/** Enable processing on a component
 * @param component component to enable
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_component_enable(MMAL_COMPONENT_T *component);

/** Disable processing on a component
 * @param component component to disable
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_component_disable(MMAL_COMPONENT_T *component);

/* @} */

#ifdef __cplusplus
}
#endif

#endif /* MMAL_COMPONENT_H */
