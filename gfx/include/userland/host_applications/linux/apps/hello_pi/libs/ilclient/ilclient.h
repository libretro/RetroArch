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

/*
 * \file
 *
 * \brief This API defines helper functions for writing IL clients.
 *
 * This file defines an IL client side library.  This is useful when
 * writing IL clients, since there tends to be much repeated and
 * common code across both single and multiple clients.  This library
 * seeks to remove that common code and abstract some of the
 * interactions with components.  There is a wrapper around a
 * component and tunnel, and some operations can be done on lists of
 * these.  The callbacks from components are handled, and specific
 * events can be checked or waited for.
 */

#ifndef _IL_CLIENT_H
#define _IL_CLIENT_H

#include "IL/OMX_Broadcom.h"
#include "interface/vcos/vcos.h"

/**
 * The <DFN>ILCLIENT_T</DFN> structure encapsulates the state needed for the IL
 * Client API.  It contains a set of callback functions used to
 * communicate with the user.  It also includes a linked list of free
 * event structures.
 ***********************************************************/
typedef struct _ILCLIENT_T ILCLIENT_T;


/**
 * Each <DFN>ILEVENT_T</DFN> structure stores the result of an <DFN>EventHandler</DFN>
 * callback from a component, storing the event message type and any
 * parameters returned.
 ***********************************************************/
typedef struct _ILEVENT_T ILEVENT_T;



struct _COMPONENT_T;

/**
 * The <DFN>COMPONENT_T</DFN> structure represents an IL component,
 * together with the necessary extra information required by the IL
 * Client API.  This structure stores the handle to the OMX component,
 * as well as the event list containing all events sent by this
 * component.  The component state structure also holds a pair of
 * buffer queues, for input and output buffers returned to the client
 * by the <DFN>FillBufferDone</DFN> and <DFN>EmptyBufferDone</DFN>
 * callbacks.  As some operations result in error callbacks that can
 * be ignored, an error mask is maintained to allow some errors to be
 * ignored.  A pointer to the client state structure is also added.
 ***********************************************************/
typedef struct _COMPONENT_T COMPONENT_T;


/**
 * The generic callback function is used for communicating events from
 * a particular component to the user.
 *
 * @param userdata The data returned from when the callback was registered.
 *
 * @param comp The component structure representing the component that
 * originated this event.
 *
 * @param data The relevant data field from the event.
 *
 * @return Void.
 ***********************************************************/
typedef void (*ILCLIENT_CALLBACK_T)(void *userdata, COMPONENT_T *comp, OMX_U32 data);


/**
 * The buffer callback function is used for indicating that a
 * component has returned a buffer on a port using client buffer
 * communication.
 *
 * @param data The data returned from when the callback was registered.
 *
 * @param comp The component from which the buffer originated.
 *
 * @return Void.
 ***********************************************************/
typedef void (*ILCLIENT_BUFFER_CALLBACK_T)(void *data, COMPONENT_T *comp);


/**
 * The malloc function is passed into
 * <DFN>ilclient_enable_port_buffers()</DFN> and used for allocating the
 * buffer payload.
 *
 * @param userdata Private pointer passed into
 * <DFN>ilclient_enable_port_buffers()</DFN> call.
 *
 * @param size Size in bytes of the requested memory region.
 *
 * @param align Alignment requirement in bytes for the base memory address.
 *
 * @param description Text description of the memory being allocated.
 *
 * @return The memory address on success, <DFN>NULL</DFN> on failure.
 ***********************************************************/
typedef void *(*ILCLIENT_MALLOC_T)(void *userdata, VCOS_UNSIGNED size, VCOS_UNSIGNED align, const char *description);


/**
 * The free function is passed into
 * <DFN>ilclient_enable_port_buffers()</DFN> and
 * <DFN>ilclient_disable_port_buffers()</DFN> and used for freeing the
 * buffer payload.
 *
 * @param userdata Private pointer passed into
 * <DFN>ilclient_enable_port_buffers()</DFN> and
 * <DFN>ilclient_disable_port_buffers()</DFN>.
 *
 * @param pointer Memory address to free, that was previously returned
 * from <DFN>ILCLIENT_MALLOC_T</DFN> function.
 *
 * @return Void.
 ***********************************************************/
typedef void (*ILCLIENT_FREE_T)(void *userdata, void *pointer);


/**
 * The event mask enumeration describes the possible events that the
 * user can ask to wait for when waiting for a particular event.
 ***********************************************************/
typedef enum {
   ILCLIENT_EMPTY_BUFFER_DONE  = 0x1,   /**< Set when a buffer is
                                           returned from an input
                                           port */

   ILCLIENT_FILL_BUFFER_DONE   = 0x2,   /**< Set when a buffer is
                                           returned from an output
                                           port */

   ILCLIENT_PORT_DISABLED      = 0x4,   /**< Set when a port indicates
                                           it has completed a disable
                                           command. */

   ILCLIENT_PORT_ENABLED       = 0x8,   /**< Set when a port indicates
                                           is has completed an enable
                                           command. */

   ILCLIENT_STATE_CHANGED      = 0x10,  /**< Set when a component
                                           indicates it has completed
                                           a state change command. */

   ILCLIENT_BUFFER_FLAG_EOS    = 0x20,  /**< Set when a port signals
                                           an EOS event. */

   ILCLIENT_PARAMETER_CHANGED  = 0x40,  /**< Set when a port signals a
                                           port settings changed
                                           event. */

   ILCLIENT_EVENT_ERROR        = 0x80,  /**< Set when a component
                                           indicates an error. */

   ILCLIENT_PORT_FLUSH         = 0x100, /**< Set when a port indicates
                                           is has completed a flush
                                           command. */

   ILCLIENT_MARKED_BUFFER      = 0x200, /**< Set when a port indicates
                                           it has marked a buffer. */

   ILCLIENT_BUFFER_MARK        = 0x400, /**< Set when a port indicates
                                           it has received a buffer
                                           mark. */

   ILCLIENT_CONFIG_CHANGED     = 0x800  /**< Set when a config parameter
                                           changed. */
} ILEVENT_MASK_T;


/**
 * On component creation the user can set flags to control the 
 * creation of that component.
 ***********************************************************/
typedef enum {
   ILCLIENT_FLAGS_NONE            = 0x0, /**< Used if no flags are
                                            set. */

   ILCLIENT_ENABLE_INPUT_BUFFERS  = 0x1, /**< If set we allow the
                                            client to communicate with
                                            input ports via buffer
                                            communication, rather than
                                            tunneling with another
                                            component. */

   ILCLIENT_ENABLE_OUTPUT_BUFFERS = 0x2, /**< If set we allow the
                                            client to communicate with
                                            output ports via buffer
                                            communication, rather than
                                            tunneling with another
                                            component. */

   ILCLIENT_DISABLE_ALL_PORTS     = 0x4, /**< If set we disable all
                                            ports on creation. */

   ILCLIENT_HOST_COMPONENT        = 0x8, /**< Create a host component.
                                            The default host ilcore
                                            only can create host components
                                            by being locally hosted
                                            so should only be used for testing
                                            purposes. */

   ILCLIENT_OUTPUT_ZERO_BUFFERS   = 0x10 /**< All output ports will have
                                            nBufferCountActual set to zero,
                                            if supported by the component. */                                            
} ILCLIENT_CREATE_FLAGS_T;
  

/**
 * \brief This structure represents a tunnel in the OpenMAX IL API.
 *
 * Some operations in this API act on a tunnel, so the tunnel state
 * structure (<DFN>TUNNEL_T</DFN>) is a convenient store of the source and sink
 * of the tunnel.  For each, a pointer to the relevant component state
 * structure and the port index is stored.
 ***********************************************************/
typedef struct {
   COMPONENT_T *source;  /**< The source component */
   int source_port;      /**< The output port index on the source component */
   COMPONENT_T *sink;    /**< The sink component */
   int sink_port;        /**< The input port index on the sink component */
} TUNNEL_T;


/**
 * The <DFN>set_tunnel</DFN> macro is a useful function that initialises a
 * <DFN>TUNNEL_T</DFN> structure.
 ***********************************************************/
#define set_tunnel(t,a,b,c,d)  do {TUNNEL_T *_ilct = (t); \
  _ilct->source = (a); _ilct->source_port = (b); \
  _ilct->sink = (c); _ilct->sink_port = (d);} while(0)

/**
 * For calling OpenMAX IL methods directory, we need to access the
 * <DFN>OMX_HANDLETYPE</DFN> corresponding to the <DFN>COMPONENT_T</DFN> structure.  This
 * macro enables this while keeping the <DFN>COMPONENT_T</DFN> structure opaque.
 * The parameter <DFN>x</DFN> should be of the type <DFN>*COMPONENT_T</DFN>.
 ***********************************************************/
#define ILC_GET_HANDLE(x) ilclient_get_handle(x)

/**
 * An IL Client structure is created by the <DFN>ilclient_init()</DFN>
 * method.  This structure is used when creating components, but
 * otherwise is not needed in other API functions as a pointer to this
 * structure is maintained in the <DFN>COMPONENT_T</DFN> structure.
 *
 * @return pointer to client structure
 ***********************************************************/
VCHPRE_ ILCLIENT_T VCHPOST_ *ilclient_init(void);

/**
 * When all components have been deleted, the IL Client structure can
 * be destroyed by calling the <DFN>ilclient_destroy()</DFN> function.
 *
 * @param handle The client handle.  After calling this function, this
 * handle should not be used.
 *
 * @return void
 ***********************************************************/
VCHPRE_ void VCHPOST_ ilclient_destroy(ILCLIENT_T *handle);

/**
 * The <DFN>ilclient_set_port_settings_callback()</DFN> function registers a
 * callback to be used when the <DFN>OMX_EventPortSettingsChanged</DFN> event is
 * received.  When the event is received, a pointer to the component
 * structure and port index is returned by the callback.
 *
 * @param handle The client handle
 *
 * @param func The callback function to use.  Calling this function
 * with a <DFN>NULL</DFN> function pointer will deregister any existing
 * registered callback.
 *
 * @param userdata Data to be passed back when calling the callback
 * function.
 *
 * @return void
 ***********************************************************/
VCHPRE_ void VCHPOST_ ilclient_set_port_settings_callback(ILCLIENT_T *handle,
                                                          ILCLIENT_CALLBACK_T func,
                                                          void *userdata);

/**
 * The <DFN>ilclient_set_eos_callback()</DFN> function registers a callback to be
 * used when the <DFN>OMX_EventBufferFlag</DFN> is received with the
 * <DFN>OMX_BUFFERFLAG_EOS</DFN> flag set. When the event is received, a pointer
 * to the component structure and port index is returned by the
 * callback.
 *
 * @param handle The client handle
 *
 * @param func The callback function to use.  Calling this function
 * with a <DFN>NULL</DFN> function pointer will deregister any existing
 * registered callback.
 *
 * @param userdata Data to be passed back when calling the callback
 * function.
 *
 * @return void
 ***********************************************************/
VCHPRE_ void VCHPOST_ ilclient_set_eos_callback(ILCLIENT_T *handle,
                                                ILCLIENT_CALLBACK_T func,
                                                void *userdata);

/**
 * The <DFN>ilclient_set_error_callback()</DFN> function registers a callback to be
 * used when the <DFN>OMX_EventError</DFN> is received from a component.  When
 * the event is received, a pointer to the component structure and the
 * error code are reported by the callback.
 *
 * @param handle The client handle
 *
 * @param func The callback function to use.  Calling this function
 * with a <DFN>NULL</DFN> function pointer will deregister any existing
 * registered callback.
 *
 * @param userdata Data to be passed back when calling the callback
 * function.
 *
 * @return void
 ***********************************************************/
VCHPRE_ void VCHPOST_ ilclient_set_error_callback(ILCLIENT_T *handle,
                                                  ILCLIENT_CALLBACK_T func,
                                                  void *userdata);

/**
 * The <DFN>ilclient_set_configchanged_callback()</DFN> function
 * registers a callback to be used when an
 * <DFN>OMX_EventParamOrConfigChanged</DFN> event occurs.  When the
 * event is received, a pointer to the component structure and the
 * index value that has changed are reported by the callback.  The
 * user may then use an <DFN>OMX_GetConfig</DFN> call with the index
 * as specified to retrieve the updated information.
 *
 * @param handle The client handle
 *
 * @param func The callback function to use.  Calling this function
 * with a <DFN>NULL</DFN> function pointer will deregister any existing
 * registered callback.
 *
 * @param userdata Data to be passed back when calling the callback
 * function.
 *
 * @return void
 ***********************************************************/
VCHPRE_ void VCHPOST_ ilclient_set_configchanged_callback(ILCLIENT_T *handle, 
                                                          ILCLIENT_CALLBACK_T func, 
                                                          void *userdata);


/**
 * The <DFN>ilclient_set_fill_buffer_done_callback()</DFN> function registers a
 * callback to be used when a buffer passed to an output port using the
 * <DFN>OMX_FillBuffer</DFN> call is returned with the <DFN>OMX_FillBufferDone</DFN>
 * callback.  When the event is received, a pointer to the component
 * structure is returned by the callback.  The user may then use the
 * <DFN>ilclient_get_output_buffer()</DFN> function to retrieve the buffer.
 *
 * @param handle The client handle
 *
 * @param func The callback function to use.  Calling this function
 * with a <DFN>NULL</DFN> function pointer will deregister any existing
 * registered callback.
 *
 * @param userdata Data to be passed back when calling the callback
 * function.
 *
 * @return void
 ***********************************************************/
VCHPRE_ void VCHPOST_ ilclient_set_fill_buffer_done_callback(ILCLIENT_T *handle,
                                                             ILCLIENT_BUFFER_CALLBACK_T func,
                                                             void *userdata);

/**
 * The <DFN>ilclient_set_empty_buffer_done_callback()</DFN> function registers a
 * callback to be used when a buffer passed to an input port using the
 * <DFN>OMX_EmptyBuffer</DFN> call is returned with the <DFN>OMX_EmptyBufferDone</DFN>
 * callback.  When the event is received, a pointer to the component
 * structure is returned by the callback.  The user may then use the
 * <DFN>ilclient_get_input_buffer()</DFN> function to retrieve the buffer.
 *
 * @param handle The client handle
 *
 * @param func The callback function to use.  Calling this function
 * with a <DFN>NULL</DFN> function pointer will deregister any existing
 * registered callback.
 *
 * @param userdata Data to be passed back when calling the callback
 * function.
 *
 * @return void
 ***********************************************************/
VCHPRE_ void VCHPOST_ ilclient_set_empty_buffer_done_callback(ILCLIENT_T *handle,
                                                              ILCLIENT_BUFFER_CALLBACK_T func,
                                                              void *userdata);


/**
 * Components are created using the <DFN>ilclient_create_component()</DFN>
 * function.  
 *
 * @param handle The client handle
 *
 * @param comp On successful creation, the component structure pointer
 * will be written back into <DFN>comp</DFN>.
 *
 * @param name The name of the component to be created.  Component
 * names as provided are automatically prefixed with
 * <DFN>"OMX.broadcom."</DFN> before passing to the IL core.  The name
 * provided will also be used in debugging messages added about this
 * component.
 *
 * @param flags The client can specify some creation behaviour by using
 * the <DFN>flags</DFN> field.  The meaning of each flag is defined 
 * by the <DFN>ILCLIENT_CREATE_FLAGS_T</DFN> type.
 *
 * @return 0 on success, -1 on failure
 ***********************************************************/
VCHPRE_ int VCHPOST_ ilclient_create_component(ILCLIENT_T *handle,
                                               COMPONENT_T **comp,
                                               char *name,
                                               ILCLIENT_CREATE_FLAGS_T flags);

/**
 * The <DFN>ilclient_cleanup_components()</DFN> function deallocates all
 * state associated with components and frees the OpenMAX component
 * handles. All tunnels connecting components should have been torn
 * down explicitly, and all components must be in loaded state.
 *
 * @param list A null-terminated list of component pointers to be
 * deallocated.
 * 
 * @return void
 ***********************************************************/
VCHPRE_ void VCHPOST_ ilclient_cleanup_components(COMPONENT_T *list[]);


/**
 * The <DFN>ilclient_change_component_state()</DFN> function changes the
 * state of an individual component.  This will trigger the state
 * change, and also wait for that state change to be completed.  It
 * should not be called if this state change has dependencies on other
 * components also changing states.  Trying to change a component to
 * its current state is treated as success.
 *
 * @param comp The component to change.
 *
 * @param state The new state to transition to.
 *
 * @return 0 on success, -1 on failure.
 ***********************************************************/
VCHPRE_ int VCHPOST_ ilclient_change_component_state(COMPONENT_T *comp,
                                                     OMX_STATETYPE state);


/**
 * The <DFN>ilclient_state_transition()</DFN> function transitions a set of
 * components that need to perform a simultaneous state transition; 
 * for example, when two components are tunnelled and the buffer
 * supplier port needs to allocate and pass buffers to a non-supplier
 * port.  All components are sent a command to change state, then the
 * function will wait for all components to signal that they have
 * changed state.
 *
 * @param list A null-terminated list of component pointers.
 *
 * @param state The new state to which to transition all components.
 *
 * @return void
 ***********************************************************/
VCHPRE_ void VCHPOST_  ilclient_state_transition(COMPONENT_T *list[],
                                                 OMX_STATETYPE state);


/**
 * The <DFN>ilclient_disable_port()</DFN> function disables a port on a
 * given component.  This function sends the disable port message to
 * the component and waits for the component to signal that this has
 * taken place.  If the port is already disabled, this is treated as a
 * success.
 *
 * @param comp The component containing the port to disable.
 *
 * @param portIndex The port index of the port to disable.  This must
 * be a named port index, rather than a <DFN>OMX_ALL</DFN> value.
 *
 * @return void
 ***********************************************************/
VCHPRE_ void VCHPOST_ ilclient_disable_port(COMPONENT_T *comp,
                                            int portIndex);


/**
 * The <DFN>ilclient_enable_port()</DFN> function enables a port on a
 * given component.  This function sends the enable port message to
 * the component and waits for the component to signal that this has
 * taken place.  If the port is already disabled, this is treated as a
 * success.
 *
 * @param comp The component containing the port to enable.
 *
 * @param portIndex The port index of the port to enable.  This must
 * be a named port index, rather than a <DFN>OMX_ALL</DFN> value.
 *
 * @return void
 ***********************************************************/
VCHPRE_ void VCHPOST_ ilclient_enable_port(COMPONENT_T *comp,
                                           int portIndex);



/**
 * The <DFN>ilclient_enable_port_buffers()</DFN> function enables a port
 * in base profile mode on a given component.  The port is not
 * tunneled, so requires buffers to be allocated.
 *
 * @param comp The component containing the port to enable.
 *
 * @param portIndex The port index of the port to enable.  This must
 * be a named port index, rather than a <DFN>OMX_ALL</DFN> value.
 *
 * @param ilclient_malloc This function will be used to allocate
 * buffer payloads.  If <DFN>NULL</DFN> then
 * <DFN>vcos_malloc_aligned</DFN> will be used.
 *
 * @param ilclient_free If an error occurs, this function is used to
 * free previously allocated payloads.  If <DFN>NULL</DFN> then
 * <DFN>vcos_free</DFN> will be used.
 *
 * @param userdata The first argument to calls to
 * <DFN>ilclient_malloc</DFN> and <DFN>ilclient_free</DFN>, if these
 * arguments are not <DFN>NULL</DFN>.
 *
 * @return 0 indicates success. -1 indicates failure.
 ***********************************************************/
VCHPRE_ int VCHPOST_ ilclient_enable_port_buffers(COMPONENT_T *comp,
                                                  int portIndex,
                                                  ILCLIENT_MALLOC_T ilclient_malloc,
                                                  ILCLIENT_FREE_T ilclient_free,
                                                  void *userdata);


/**
 * The <DFN>ilclient_disable_port_buffers()</DFN> function disables a
 * port in base profile mode on a given component.  The port is not
 * tunneled, and has been supplied with buffers by the client.
 *
 * @param comp The component containing the port to disable.
 *
 * @param portIndex The port index of the port to disable.  This must
 * be a named port index, rather than a <DFN>OMX_ALL</DFN> value.
 *
 * @param bufferList A list of buffers, using <DFN>pAppPrivate</DFN>
 * as the next pointer that were allocated on this port, and currently
 * held by the application.  If buffers on this port are held by IL
 * client or the component then these are automatically freed.
 *
 * @param ilclient_free This function is used to free the buffer payloads.
 * If <DFN>NULL</DFN> then <DFN>vcos_free</DFN> will be used.
 *
 * @param userdata The first argument to calls to
 * <DFN>ilclient_free</DFN>.
 *
 * @return void
 */
VCHPRE_ void VCHPOST_ ilclient_disable_port_buffers(COMPONENT_T *comp,
                                                    int portIndex,
                                                    OMX_BUFFERHEADERTYPE *bufferList,
                                                    ILCLIENT_FREE_T ilclient_free,
                                                    void *userdata);


/**
 * With a populated tunnel structure, the
 * <DFN>ilclient_setup_tunnel()</DFN> function connects the tunnel.  It
 * first transitions the source component to idle if currently in
 * loaded state, and then optionally checks the source event list for
 * a port settings changed event from the source port.  If this event
 * is not in the event queue then this function optionally waits for
 * it to arrive.  To disable this check for the port settings changed
 * event, set <DFN>timeout</DFN> to zero.
 *
 * Both ports are then disabled, and the source port is inspected for
 * a port streams parameter.  If this is supported, then the
 * <DFN>portStream</DFN> argument is used to select which port stream
 * to use.  The two ports are then tunnelled using the
 * <DFN>OMX_SetupTunnel</DFN> function.  If this is successful, then
 * both ports are enabled.  Note that for disabling and enabling the
 * tunnelled ports, the functions <DFN>ilclient_disable_tunnel()</DFN>
 * and <DFN>ilclient_enable_tunnel()</DFN> are used, so the relevant
 * documentation for those functions applies here.
 *
 * @param tunnel The tunnel structure representing the tunnel to
 * set up.
 *
 * @param portStream If port streams are supported on the output port
 * of the tunnel, then this parameter indicates the port stream to
 * select on this port.
 *
 * @param timeout The time duration in milliseconds to wait for the
 * output port to signal a port settings changed event before
 * returning a timeout failure.  If this is 0, then we do not check
 * for a port settings changed before setting up the tunnel.
 *
 * @return 0 indicates success, negative indicates failure:
 *  - -1: a timeout waiting for the parameter changed
 *  - -2: an error was returned instead of parameter changed
 *  - -3: no streams are available from this port
 *  - -4: requested stream is not available from this port
 *  - -5: the data format was not acceptable to the sink
 ***********************************************************/
VCHPRE_ int VCHPOST_ ilclient_setup_tunnel(TUNNEL_T *tunnel,
                                           unsigned int portStream,
                                           int timeout);


/**
 * The <DFN>ilclient_disable_tunnel()</DFN> function disables both ports listed in
 * the tunnel structure.  It will send a port disable command to each
 * port, then waits for both to indicate they have completed the
 * transition.  The errors <DFN>OMX_ErrorPortUnpopulated</DFN> and
 * <DFN>OMX_ErrorSameState</DFN> are both ignored by this function; the former
 * since the first port to disable may deallocate buffers before the
 * second port has been disabled, leading to the second port reporting
 * the unpopulated error.
 *
 * @param tunnel The tunnel to disable.
 *
 * @return void
 ***********************************************************/
VCHPRE_ void VCHPOST_ ilclient_disable_tunnel(TUNNEL_T *tunnel);


/**
 * The <DFN>ilclient_enable_tunnel()</DFN> function enables both ports listed in
 * the tunnel structure.  It will first send a port enable command to
 * each port.  It then checks whether the sink component is not in
 * loaded state - if so, the function waits for both ports to complete
 * the requested port enable.  If the sink component was in loaded
 * state, then this component is transitioned to idle to allow the
 * ports to exchange buffers and enable the ports.  This is since
 * typically this function is used when creating a tunnel between two
 * components, where the source component is processing data to enable
 * it to report the port settings changed event, and the sink port has
 * yet to be used.  Before transitioning the sink component to idle,
 * this function waits for the sink port to be enabled - since the
 * component is in loaded state, this will happen quickly.  If the
 * transition to idle fails, the sink component is transitioned back
 * to loaded and the source port disabled.  If the transition
 * succeeds, the function then waits for the source port to complete
 * the requested port enable.
 *
 * @param tunnel The tunnel to enable.
 *
 * @return 0 on success, -1 on failure.
 ***********************************************************/
VCHPRE_ int VCHPOST_ ilclient_enable_tunnel(TUNNEL_T *tunnel);


/**
 * The <DFN>ilclient_flush_tunnels()</DFN> function will flush a number of tunnels
 * from the list of tunnels presented.  For each tunnel that is to be
 * flushed, both source and sink ports are sent a flush command.  The
 * function then waits for both ports to report they have completed
 * the flush operation.
 *
 * @param tunnel List of tunnels.  The list must be terminated with a
 * tunnel structure with <DFN>NULL</DFN> component entries.
 *
 * @param max The maximum number of tunnels to flush from the list.
 * A value of 0 indicates that all tunnels in the list are flushed.
 *
 * @return void
 ***********************************************************/
VCHPRE_ void VCHPOST_ ilclient_flush_tunnels(TUNNEL_T *tunnel,
                                             int max);


/**
 * The <DFN>ilclient_teardown_tunnels()</DFN> function tears down all tunnels in
 * the list of tunnels presented.  For each tunnel in the list, the
 * <DFN>OMX_SetupTunnel</DFN> is called on the source port and on the sink port,
 * where for both calls the destination component is <DFN>NULL</DFN> and the
 * destination port is zero.  The VMCSX IL implementation requires
 * that all tunnels are torn down in this manner before components are
 * freed. 
 *
 * @param tunnels List of tunnels to teardown.  The list must be
 * terminated with a tunnel structure with <DFN>NULL</DFN> component entries.
 *
 * @return void
 ***********************************************************/
VCHPRE_ void VCHPOST_ ilclient_teardown_tunnels(TUNNEL_T *tunnels);


/**
 * The <DFN>ilclient_get_output_buffer()</DFN> function returns a buffer
 * that was sent to an output port and that has been returned from a
 * component using the <DFN>OMX_FillBufferDone</DFN> callback.
 *
 * @param comp The component that returned the buffer.
 *
 * @param portIndex The port index on the component that the buffer
 * was returned from.
 *
 * @param block If non-zero, the function will block until a buffer is available.
 *
 * @return Pointer to buffer if available, otherwise <DFN>NULL</DFN>.
 ***********************************************************/
VCHPRE_ OMX_BUFFERHEADERTYPE* VCHPOST_ ilclient_get_output_buffer(COMPONENT_T *comp,
                                                                  int portIndex,
                                                                  int block);


/**
 * The <DFN>ilclient_get_input_buffer()</DFN> function returns a buffer
 * that was sent to an input port and that has been returned from a
 * component using the <DFN>OMX_EmptyBufferDone</DFN> callback.
 *
 * @param comp The component that returned the buffer.
 *
 * @param portIndex The port index on the component from which the buffer
 * was returned.
 *
 * @param block If non-zero, the function will block until a buffer is available.
 *
 * @return pointer to buffer if available, otherwise <DFN>NULL</DFN>
 ***********************************************************/
VCHPRE_ OMX_BUFFERHEADERTYPE* VCHPOST_ ilclient_get_input_buffer(COMPONENT_T *comp,
                                                                 int portIndex,
                                                                 int block);


/**
 * The <DFN>ilclient_remove_event()</DFN> function queries the event list for the
 * given component, matching against the given criteria.  If a matching
 * event is found, it is removed and added to the free event list.
 *
 * @param comp The component that returned the matching event.
 *
 * @param event The event type of the matching event.
 *
 * @param nData1 The <DFN>nData1</DFN> field of the matching event.
 *
 * @param ignore1 Whether to ignore the <DFN>nData1</DFN> field when finding a
 * matching event.  A value of 0 indicates that <DFN>nData1</DFN> must match, a
 * value of 1 indicates that <DFN>nData1</DFN> does not have to match.
 *
 * @param nData2 The <DFN>nData2</DFN> field of the matching event.
 *
 * @param ignore2 Whether to ignore the <DFN>nData2</DFN> field when finding a
 * matching event.  A value of 0 indicates that <DFN>nData2</DFN> must match, a
 * value of 1 indicates that <DFN>nData2</DFN> does not have to match.
 *
 * @return 0 if the event was removed.  -1 if no matching event was
 * found.
 ***********************************************************/
VCHPRE_ int VCHPOST_ ilclient_remove_event(COMPONENT_T *comp,
                                           OMX_EVENTTYPE event,
                                           OMX_U32 nData1,
                                           int ignore1,
                                           OMX_U32 nData2,
                                           int ignore2);


/**
 * The <DFN>ilclient_return_events()</DFN> function removes all events
 * from a component event list and adds them to the IL client free
 * event list.  This function is automatically called when components
 * are freed.
 *
 * @param comp The component from which all events should be moved to
 * the free list.
 *
 * @return void
 ***********************************************************/
VCHPRE_ void VCHPOST_ ilclient_return_events(COMPONENT_T *comp);


/**
 * The <DFN>ilclient_wait_for_event()</DFN> function is similar to
 * <DFN>ilclient_remove_event()</DFN>, but allows the caller to block until that
 * event arrives.
 *
 * @param comp The component that returned the matching event.
 *
 * @param event The event type of the matching event.
 *
 * @param nData1 The <DFN>nData1</DFN> field of the matching event.
 *
 * @param ignore1 Whether to ignore the <DFN>nData1</DFN> field when finding a
 * matching event.  A value of 0 indicates that <DFN>nData1</DFN> must match, a
 * value of 1 indicates that <DFN>nData1</DFN> does not have to match.
 *
 * @param nData2 The <DFN>nData2</DFN> field of the matching event.
 *
 * @param ignore2 Whether to ignore the <DFN>nData2</DFN> field when finding a
 * matching event.  A value of 0 indicates that <DFN>nData2</DFN> must match, a
 * value of 1 indicates that <DFN>nData2</DFN> does not have to match.
 *
 * @param event_flag Specifies a bitfield of IL client events to wait
 * for, given in <DFN>ILEVENT_MASK_T</DFN>.  If any of these events
 * are signalled by the component, the event list is then re-checked
 * for a matching event.  If the <DFN>ILCLIENT_EVENT_ERROR</DFN> bit
 * is included, and an error is signalled by the component, then the
 * function will return an error code.  If the
 * <DFN>ILCLIENT_CONFIG_CHANGED</DFN> bit is included, and this bit is
 * signalled by the component, then the function will return an error
 * code.
 *
 * @param timeout Specifies how long to block for in milliseconds
 * before returning a failure.
 *
 * @return 0 indicates success, a matching event has been removed from
 * the component's event queue.  A negative return indicates failure,
 * in this case no events have been removed from the component's event
 * queue.
 *  - -1: a timeout was received.
 *  - -2: an error event was received.
 *  - -3: a config changed event was received.
 ***********************************************************/
VCHPRE_ int VCHPOST_ ilclient_wait_for_event(COMPONENT_T *comp,
                                             OMX_EVENTTYPE event,
                                             OMX_U32 nData1,
                                             int ignore1,
                                             OMX_U32 nData2,
                                             int ignore2,
                                             int event_flag,
                                             int timeout);


/**
 * The <DFN>ilclient_wait_for_command_complete()</DFN> function waits
 * for a message from a component that indicates that the command
 * has completed.  This is either a command success message, or an 
 * error message that signals the completion of an event.
 * 
 * @param comp The component currently processing a command.
 *
 * @param command The command being processed.  This must be either
 * <DFN>OMX_CommandStateSet</DFN>, <DFN>OMX_CommandPortDisable</DFN>
 * or <DFN>OMX_CommandPortEnable</DFN>.
 *
 * @param nData2 The expected value of <DFN>nData2</DFN> in the
 * command complete message.
 *
 * @return 0 indicates success, either the command successfully completed
 * or the <DFN>OMX_ErrorSameState</DFN> was returned.  -1 indicates
 * that the command terminated with a different error message.
 ***********************************************************/
VCHPRE_ int VCHPOST_ ilclient_wait_for_command_complete(COMPONENT_T *comp,
                                                        OMX_COMMANDTYPE command,
                                                        OMX_U32 nData2);


/**
 * The <DFN>ilclient_wait_for_command_complete_dual()</DFN> function
 * is similar to <DFN>ilclient_wait_for_command_complete()</DFN>.  The
 * difference is that while waiting for the component to complete the
 * event or raise an error, we can also report if another reports an
 * error that terminates a command.  This is useful if the two
 * components are tunneled, and we need to wait for one component to
 * enable a port, or change state to <DFN>OMX_StateIdle</DFN>.  If the
 * other component is the buffer supplier and reports an error, then
 * it will not allocate buffers, so our first component may stall.
 * 
 * @param comp The component currently processing a command.
 *
 * @param command The command being processed.  This must be either
 * <DFN>OMX_CommandStateSet</DFN>, <DFN>OMX_CommandPortDisable</DFN>
 * or <DFN>OMX_CommandPortEnable</DFN>.
 *
 * @param nData2 The expected value of <DFN>nData2</DFN> in the
 * command complete message.
 *
 * @param related Another component, where we will return if this
 * component raises an error that terminates a command.
 *
 * @return 0 indicates success, either the command successfully
 * completed or the <DFN>OMX_ErrorSameState</DFN> was returned.  -1
 * indicates that the command terminated with a different error
 * message. -2 indicates that the related component raised an error.
 * In this case, the error is not cleared from the related
 * component's event list.
 ***********************************************************/
VCHPRE_ int VCHPOST_ ilclient_wait_for_command_complete_dual(COMPONENT_T *comp,
                                                             OMX_COMMANDTYPE command,
                                                             OMX_U32 nData2,
                                                             COMPONENT_T *related);
                                                             

/**
 * The <DFN>ilclient_debug_output()</DFN> function adds a message to a 
 * host-specific debug display.  For a local VideoCore host the message is
 * added to the internal message log.  For a Win32 host the message is
 * printed to the debug display.  This function should be customised
 * when IL client is ported to another platform.
 * 
 * @param format A message to add, together with the variable
 * argument list similar to <DFN>printf</DFN> and other standard C functions.
 *
 * @return void
 ***********************************************************/
VCHPRE_ void VCHPOST_ ilclient_debug_output(char *format, ...);

/**
 * The <DFN>ilclient_get_handle()</DFN> function returns the
 * underlying OMX component held by an IL component handle.  This is
 * needed when calling methods such as <DFN>OMX_SetParameter</DFN> on
 * a component created using the IL client library.
 *
 * @param comp  IL component handle
 *
 * @return The <DFN>OMX_HANDLETYPE</DFN> value for the component.
 ***********************************************************/
VCHPRE_ OMX_HANDLETYPE VCHPOST_ ilclient_get_handle(COMPONENT_T *comp);


#ifndef OMX_SKIP64BIT

/**
 * Macro to return <DFN>OMX_TICKS</DFN> from a signed 64 bit value.
 * This is the version where <DFN>OMX_TICKS</DFN> is a signed 64 bit
 * value, an alternative definition is used when <DFN>OMX_TICKS</DFN>
 * is a structure.
 */
#define ilclient_ticks_from_s64(s) (s)

/**
 * Macro to return signed 64 bit value from <DFN>OMX_TICKS</DFN>.
 * This is the version where <DFN>OMX_TICKS</DFN> is a signed 64 bit
 * value, an alternative definition is used when <DFN>OMX_TICKS</DFN>
 * is a structure.
 */
#define ilclient_ticks_to_s64(t)   (t)

#else

/**
 * Inline function to return <DFN>OMX_TICKS</DFN> from a signed 64 bit
 * value.  This is the version where <DFN>OMX_TICKS</DFN> is a
 * structure, an alternative definition is used when
 * <DFN>OMX_TICKS</DFN> is a signed 64 bit value.
 */
static inline OMX_TICKS ilclient_ticks_from_s64(int64_t s) {
   OMX_TICKS ret;
   ret.nLowPart = s;
   ret.nHighPart = s>>32;
   return ret;
}

/**
 * Inline function to return signed 64 bit value from
 * <DFN>OMX_TICKS</DFN>.  This is the version where
 * <DFN>OMX_TICKS</DFN> is a structure, an alternative definition is
 * used when <DFN>OMX_TICKS</DFN> is a signed 64 bit value.
 */
static inline int64_t ilclient_ticks_to_s64(OMX_TICKS t) {
   uint64_t u = t.nLowPart | ((uint64_t)t.nHighPart << 32);
   return u;
}


#endif /* OMX_SKIP64BIT */

/**
 * The <DFN>ilclient_get_port_index()</DFN> function returns the n'th
 * port index of the specified type and direction for the given
 * component.
 *
 * @param comp    The component of interest
 * @param dir     The direction
 * @param type    The type, or -1 for any type.
 * @param index   Which port (counting from 0).
 *
 * @return        The port index, or -1 if not found.
 ***********************************************************/
VCHPRE_ int VCHPOST_ ilclient_get_port_index(COMPONENT_T *comp,
                                             OMX_DIRTYPE dir,
                                             OMX_PORTDOMAINTYPE type,
                                             int index);


/**
 * The <DFN>ilclient_suggest_bufsize()</DFN> function gives a
 * component a hint about the size of buffer it should use.  This size
 * is set on the component by setting the
 * <DFN>OMX_IndexParamBrcmOutputBufferSize</DFN> index on the given
 * component.
 *
 * @param comp         IL component handle
 * @param nBufSizeHint Size of buffer in bytes
 *
 * @return             0 indicates success, -1 indicates failure.
 ***********************************************************/
VCHPRE_ int VCHPOST_ ilclient_suggest_bufsize(COMPONENT_T *comp,
                                              OMX_U32 nBufSizeHint);


/**
 * The <DFN>ilclient_stack_size()</DFN> function suggests a minimum
 * stack size that tasks calling into with API will require.
 *
 * @return    Suggested stack size in bytes.
 ***********************************************************/
VCHPRE_ unsigned int VCHPOST_ ilclient_stack_size(void);

#endif /* ILCLIENT_H */
