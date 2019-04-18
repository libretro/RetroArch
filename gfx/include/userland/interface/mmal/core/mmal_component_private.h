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

#ifndef MMAL_COMPONENT_PRIVATE_H
#define MMAL_COMPONENT_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

#define MMAL_VIDEO_DECODE  "video_decode"
#define MMAL_VIDEO_ENCODE  "video_encode"
#define MMAL_VIDEO_RENDER  "video_render"
#define MMAL_AUDIO_DECODE  "audio_decode"
#define MMAL_AUDIO_ENCODE  "audio_encode"
#define MMAL_AUDIO_RENDER  "audio_render"
#define MMAL_CAMERA        "camera"

#if defined(__GNUC__) && (__GNUC__ > 2)
# define MMAL_CONSTRUCTOR(func) void __attribute__((constructor,used)) func(void)
# define MMAL_DESTRUCTOR(func) void __attribute__((destructor,used)) func(void)
#else
# define MMAL_CONSTRUCTOR(func) void func(void)
# define MMAL_DESTRUCTOR(func) void func(void)
#endif

#include "mmal.h"
#include "mmal_component.h"

/** Definition of a component. */
struct MMAL_COMPONENT_PRIVATE_T
{
   /** Pointer to the private data of the component module in use */
   struct MMAL_COMPONENT_MODULE_T *module;

   MMAL_STATUS_T (*pf_enable)(MMAL_COMPONENT_T *component);
   MMAL_STATUS_T (*pf_disable)(MMAL_COMPONENT_T *component);
   MMAL_STATUS_T (*pf_destroy)(MMAL_COMPONENT_T *component);

   /** Pool of event buffer headers, for sending events from component to client. */
   MMAL_POOL_T *event_pool;

   /** Reference counting of the component */
   int refcount;
   /** Reference counting of the ports. Component won't be destroyed until this
    * goes to 0 */
   int refcount_ports;

   /** Priority associated with the 'action thread' for this component, when
    * such action thread is applicable. */
   int priority;
};

/** Set a generic component control parameter.
  *
  * @param control_port control port of component on which to set the parameter.
  * @param param        parameter to be set.
  * @return MMAL_SUCCESS or another status on error.
  */
MMAL_STATUS_T mmal_component_parameter_set(MMAL_PORT_T *control_port,
                                           const MMAL_PARAMETER_HEADER_T *param);

/** Get a generic component control parameter.
  *
  * @param contorl_port control port of component from which to get the parameter.
  * @param param        parameter to be retrieved.
  * @return MMAL_SUCCESS or another status on error.
  */
MMAL_STATUS_T mmal_component_parameter_get(MMAL_PORT_T *control_port,
                                           MMAL_PARAMETER_HEADER_T *param);

/** Registers an action with the core.
  * The MMAL core allows components to register an action which will be run
  * from a separate thread context when the action is explicitly triggered by
  * the component.
  *
  * @param component    component registering the action.
  * @param action       action to register.
  * @return MMAL_SUCCESS or another status on error.
  */
MMAL_STATUS_T mmal_component_action_register(MMAL_COMPONENT_T *component,
                                             void (*pf_action)(MMAL_COMPONENT_T *));

/** De-registers the current action registered with the core.
  *
  * @param component    component de-registering the action.
  * @return MMAL_SUCCESS or another status on error.
  */
MMAL_STATUS_T mmal_component_action_deregister(MMAL_COMPONENT_T *component);

/** Triggers a registered action.
  * Explicitly triggers an action registered by a component.
  *
  * @param component    component on which to trigger the action.
  * @return MMAL_SUCCESS or another status on error.
  */
MMAL_STATUS_T mmal_component_action_trigger(MMAL_COMPONENT_T *component);

/** Lock an action to prevent it from running.
  * Allows a component to make sure no action is running while the lock is taken.
  *
  * @param component    component.
  * @return MMAL_SUCCESS or another status on error.
  */
MMAL_STATUS_T mmal_component_action_lock(MMAL_COMPONENT_T *component);

/** Unlock an action to allow it to run again.
  *
  * @param component    component.
  * @return MMAL_SUCCESS or another status on error.
  */
MMAL_STATUS_T mmal_component_action_unlock(MMAL_COMPONENT_T *component);

/** Prototype used by components to register themselves to the supplier. */
typedef MMAL_STATUS_T (*MMAL_COMPONENT_SUPPLIER_FUNCTION_T)(const char *name,
                                                            MMAL_COMPONENT_T *component);

/** Create an instance of a component given a constructor for the component.
 * This allows the creation of client-side components which haven't been registered with the core.
 * See \ref mmal_component_create for the public interface used to create components.
 *
 * @param name name assigned to the component by the client
 * @param constructor constructor function for the component
 * @param constructor_private private data for the constructor
 * @param component returned component
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_component_create_with_constructor(const char *name,
   MMAL_STATUS_T (*constructor)(const char *name, MMAL_COMPONENT_T *),
   struct MMAL_COMPONENT_MODULE_T *constructor_private,
   MMAL_COMPONENT_T **component);

/** Register a component with the mmal component supplier.
  *
  * @param prefix     prefix for this supplier, e.g. "VC"
  * @param create_fn  function which will instantiate a component given a name.
  */
void mmal_component_supplier_register(const char *prefix,
                                      MMAL_COMPONENT_SUPPLIER_FUNCTION_T create_fn);

#ifdef __cplusplus
}
#endif

#endif /* MMAL_COMPONENT_PRIVATE_H */
