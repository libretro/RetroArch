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

#ifndef MMAL_GRAPH_H
#define MMAL_GRAPH_H

#include "util/mmal_connection.h"

/** \defgroup MmalGraphUtility Graph Utility
 * \ingroup MmalUtilities
 * The graph utility functions allows one to easily create graphs of MMAL components.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** List of topology types */
typedef enum
{
   MMAL_GRAPH_TOPOLOGY_ALL = 0,    /**< All input ports and output ports are linked */
   MMAL_GRAPH_TOPOLOGY_STRAIGHT,   /**< Input ports and output ports of the same index are linked */
   MMAL_GRAPH_TOPOLOGY_CUSTOM,     /**< Custom defined topology */
   MMAL_GRAPH_TOPOLOGY_MAX

} MMAL_GRAPH_TOPOLOGY_T;

/** Structure describing a graph */
typedef struct MMAL_GRAPH_T
{
   /** Pointer to private data of the client */
   struct MMAL_GRAPH_USERDATA_T *userdata;

   /** Optional callback that the client can set to get notified when the graph is going to be destroyed */
   void (*pf_destroy)(struct MMAL_GRAPH_T *);

   /** Optional callback that the client can set to intercept parameter requests on ports exposed by the graph */
   MMAL_STATUS_T (*pf_parameter_set)(struct MMAL_GRAPH_T *, MMAL_PORT_T *port, const MMAL_PARAMETER_HEADER_T *param);
   /** Optional callback that the client can set to intercept parameter requests on ports exposed by the graph */
   MMAL_STATUS_T (*pf_parameter_get)(struct MMAL_GRAPH_T *, MMAL_PORT_T *port, MMAL_PARAMETER_HEADER_T *param);
   /** Optional callback that the client can set to intercept format commit calls on ports exposed by the graph */
   MMAL_STATUS_T (*pf_format_commit)(struct MMAL_GRAPH_T *, MMAL_PORT_T *port);
   /** Optional callback that the client can set to intercept send buffer calls on ports exposed by the graph */
   MMAL_STATUS_T (*pf_send_buffer)(struct MMAL_GRAPH_T *, MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
   /** Optional callback that the client can set to intercept buffer callbacks on ports exposed by the graph */
   MMAL_STATUS_T (*pf_return_buffer)(struct MMAL_GRAPH_T *, MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
   /** Optional callback that the client can set to intercept payload alloc calls on ports exposed by the graph */
   MMAL_STATUS_T (*pf_payload_alloc)(struct MMAL_GRAPH_T *, MMAL_PORT_T *port, uint32_t payload_size, uint8_t **);
   /** Optional callback that the client can set to intercept payload free calls on ports exposed by the graph */
   MMAL_STATUS_T (*pf_payload_free)(struct MMAL_GRAPH_T *, MMAL_PORT_T *port, uint8_t *payload);
   /** Optional callback that the client can set to intercept flush calls on ports exposed by the graph */
   MMAL_STATUS_T (*pf_flush)(struct MMAL_GRAPH_T *, MMAL_PORT_T *port);
   /** Optional callback that the client can set to control callbacks from the internal components of the graph */
   /** Optional callback that the client can set to intercept enable calls on ports exposed by the graph */
   MMAL_STATUS_T (*pf_enable)(struct MMAL_GRAPH_T *, MMAL_PORT_T *port);
   /** Optional callback that the client can set to intercept disable calls on ports exposed by the graph */
   MMAL_STATUS_T (*pf_disable)(struct MMAL_GRAPH_T *, MMAL_PORT_T *port);
   /** Optional callback that the client can set to control callbacks from the internal components of the graph */
   MMAL_STATUS_T (*pf_control_callback)(struct MMAL_GRAPH_T *, MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
   /** Optional callback that the client can set to intercept component_enable/disable calls made to the graph */
   MMAL_STATUS_T (*pf_graph_enable)(struct MMAL_GRAPH_T *, MMAL_BOOL_T enable);
   /** Optional callback that the client can set to intercept buffers going through internal connections.
    * This will only be triggered if the connection is not tunnelled */
   MMAL_STATUS_T (*pf_connection_buffer)(struct MMAL_GRAPH_T *, MMAL_CONNECTION_T *connection, MMAL_BUFFER_HEADER_T *buffer);

} MMAL_GRAPH_T;

/** Create an instance of a graph.
 * The newly created graph will need to be populated by the client.
 *
 * @param graph returned graph
 * @param userdata_size size to be allocated for the userdata field
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_graph_create(MMAL_GRAPH_T **graph, unsigned int userdata_size);

/** Add a component to a graph.
 * Allows the client to add a component to the graph.
 *
 * @param graph instance of the graph
 * @param component component to add to a graph
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_graph_add_component(MMAL_GRAPH_T *graph, MMAL_COMPONENT_T *component);

/** Describe the topology of the ports of a component.
 * Allows the client to describe the topology of a component. This information
 * is used by the graph to choose which action to perform when
 * enabling / disabling / committing / flushing a port exposed by the graph.
 * Note that by default the topology of a component is set to MMAL_GRAPH_TOPOLOGY_ALL.
 *
 * @param graph instance of the graph
 * @param component component to describe
 * @param topology type of topology used by this component
 * @param input output index (or -1 if sink) linked to each input port
 * @param input_num number of indexes in the input list
 * @param output input index (or -1 if source) linked to each output port
 * @param output_num number of indexes in the output list
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_graph_component_topology(MMAL_GRAPH_T *graph, MMAL_COMPONENT_T *component,
    MMAL_GRAPH_TOPOLOGY_T topology, int8_t *input, unsigned int input_num,
    int8_t *output, unsigned int output_num);

/** Add a port to a graph.
 * Allows the client to add an input or output port to a graph. The given port
 * will effectively become an end point for the graph.
 *
 * @param graph instance of the graph
 * @param port port to add to the graph
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_graph_add_port(MMAL_GRAPH_T *graph, MMAL_PORT_T *port);

/** Add a connection to a graph.
 * Allows the client to add an internal connection to a graph.
 *
 * @param graph instance of the graph
 * @param connection connection to add to the graph
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_graph_add_connection(MMAL_GRAPH_T *graph, MMAL_CONNECTION_T *connection);

/** Create a new component and add it to a graph.
 * Allows the client to create and add a component to the graph.
 *
 * @param graph instance of the graph
 * @param name name of the component to create
 * @param component if not NULL, will contain a pointer to the created component
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_graph_new_component(MMAL_GRAPH_T *graph, const char *name,
   MMAL_COMPONENT_T **component);

/** Create and add a connection to a graph.
 * Allows the client to create and add an internal connection to a graph.
 *
 * @param graph      instance of the graph
 * @param out        the output port to use for the connection
 * @param in         the input port to use for the connection
 * @param flags      the flags specifying which type of connection should be created
 * @param connection if not NULL, will contain a pointer to the created connection
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_graph_new_connection(MMAL_GRAPH_T *graph, MMAL_PORT_T *out, MMAL_PORT_T *in,
   uint32_t flags, MMAL_CONNECTION_T **connection);

/** Definition of the callback used by a graph to send events to the client.
 *
 * @param graph   the graph sending the event
 * @param port    the port which generated the event
 * @param buffer  the buffer header containing the event data
 * @param cb_data data passed back to the client when the callback is invoked
 */
typedef void (*MMAL_GRAPH_EVENT_CB)(MMAL_GRAPH_T *graph, MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer,
   void *cb_data);

/** Enable the graph and start processing.
 *
 * @param graph   the graph to enable
 * @param cb      the callback to invoke when an event occurs on any of the internal control ports
 * @param cb_data data passed back to the client when the callback is invoked
 *
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_graph_enable(MMAL_GRAPH_T *graph, MMAL_GRAPH_EVENT_CB cb, void *cb_data);

MMAL_STATUS_T mmal_graph_disable(MMAL_GRAPH_T *graph);

/** Find a port in the graph.
 *
 * @param graph graph instance
 * @param name  name of the component of interest
 * @param type  type of port (in/out)
 * @param index which port index within the component
 *
 * @return port, or NULL if not found
 */
MMAL_PORT_T *mmal_graph_find_port(MMAL_GRAPH_T *graph,
                                  const char *name,
                                  MMAL_PORT_TYPE_T type,
                                  unsigned index);

/** Create an instance of a component from a graph.
 * The newly created component will expose input and output ports to the client.
 * Not that all the exposed ports will be in a disabled state by default.
 *
 * @param graph graph to create the component from
 * @param name name of the component to create
 * @param component returned component
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_graph_build(MMAL_GRAPH_T *ctx,
   const char *name, MMAL_COMPONENT_T **component);

/** Component constructor for a graph.
 * FIXME: private function
 *
 * @param name name of the component to create
 * @param component component
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_graph_component_constructor(const char *name, MMAL_COMPONENT_T *component);

/** Destroy a previously created graph
 * @param graph graph to destroy
 * @return MMAL_SUCCESS on success
 */
MMAL_STATUS_T mmal_graph_destroy(MMAL_GRAPH_T *ctx);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* MMAL_GRAPH_H */
