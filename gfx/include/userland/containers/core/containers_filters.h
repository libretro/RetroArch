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
#ifndef VC_CONTAINERS_FILTERS_H
#define VC_CONTAINERS_FILTERS_H

/** \file containers_filters.h
 * Interface definition for the filter abstraction used by the container
 * common layer */
#include <stdarg.h>

#include "containers/containers.h"

/** \defgroup VcContainerFilterApi Container Filter API */
/* @{ */

/** Container Filter Context.
 * This structure defines the context for a container filter instance */
typedef struct VC_CONTAINER_FILTER_T
{   
   /** Pointer to container instance */
   struct VC_CONTAINER_T *container;
   /** Pointer to information private to the container filter instance */
   struct VC_CONTAINER_FILTER_PRIVATE_T *priv;
   /** Pointer to information private to the container filter module */
   struct VC_CONTAINER_FILTER_MODULE_T *module;

   /** \note the following list of function pointers should not be used directly.
    * They defines the interface for implementing container filter modules and are 
    * filled in by the container filter modules themselves. */

   /** \private
    * Function pointer to close and free all resources allocated by a
    * container filter module */
   VC_CONTAINER_STATUS_T (*pf_close)(struct VC_CONTAINER_FILTER_T *filter);

   /** \private
    * Function pointer to filter a data packet using a container filter module */
   VC_CONTAINER_STATUS_T (*pf_process)(struct VC_CONTAINER_FILTER_T *filter, VC_CONTAINER_PACKET_T *p_packet);

   /** \private
    * Function pointer to control container filter module */
   VC_CONTAINER_STATUS_T (*pf_control)( struct VC_CONTAINER_FILTER_T *filter, VC_CONTAINER_CONTROL_T operation, va_list args );

} VC_CONTAINER_FILTER_T;

/** Opens a container filter using a four character code describing the filter.
 * This will create an instance of the container filter.
 *
 * \param  filter       Four Character Code describing the filter
 * \param  type         Four Character Code describing the subtype - indicated whether filter is encrypt or decrypt
 * \param  container    Pointer to the container instance
 * \param  status       Returns the status of the operation
 * \return              If successful, this returns a pointer to the new instance
 *                      of the container filter. Returns NULL on failure.
 */
VC_CONTAINER_FILTER_T *vc_container_filter_open(VC_CONTAINER_FOURCC_T filter,
                                                VC_CONTAINER_FOURCC_T type, 
                                                VC_CONTAINER_T *container,
                                                VC_CONTAINER_STATUS_T *status );

/** Closes an instance of a container filter.
 * \param  context      Pointer to the VC_CONTAINER_FILTER_T context of the instance to close
 * \return              VC_CONTAINER_SUCCESS on success.
 */
VC_CONTAINER_STATUS_T vc_container_filter_close( VC_CONTAINER_FILTER_T *context );

/** Filter a data packet using a container filter module.
 * \param  context      Pointer to the VC_CONTAINER_FILTER_T instance to use
 * \param  packet       Pointer to the VC_CONTAINER_PACKET_T structure to process
 * \return              the status of the operation
 */
VC_CONTAINER_STATUS_T vc_container_filter_process(VC_CONTAINER_FILTER_T *context, VC_CONTAINER_PACKET_T *p_packet);
 
/** Extensible control function for container filter modules.
* This function takes a variable number of arguments which will depend on the specific operation.
*
* \param  context      Pointer to the VC_CONTAINER_FILTER_T instance to use
* \param  operation    The requested operation
* \param  args         Arguments for the operation
* \return              the status of the operation
*/ 
VC_CONTAINER_STATUS_T vc_container_filter_control(VC_CONTAINER_FILTER_T *context, VC_CONTAINER_CONTROL_T operation, ... );

/* @} */

#endif /* VC_CONTAINERS_FILTERS_H */
