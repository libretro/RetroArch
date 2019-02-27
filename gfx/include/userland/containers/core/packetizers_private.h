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
#ifndef VC_PACKETIZERS_PRIVATE_H
#define VC_PACKETIZERS_PRIVATE_H

/** \file
 * Private interface for packetizers
 */

#include <stdarg.h>
#include "containers/packetizers.h"
#include "containers/core/containers_common.h"
#include "containers/core/containers_bytestream.h"
#include "containers/core/containers_time.h"

/** \defgroup VcPacketizerModuleApi Packetizer Module API
 * Private interface for modules implementing packetizers */
/* @{ */

/** Context private to the packetizer instance. This private context is used to
 * store data which shouldn't be exported by the public API. */
typedef struct VC_PACKETIZER_PRIVATE_T
{
   /** Pointer to the private data of the packetizer module in use */
   struct VC_PACKETIZER_MODULE_T *module;

   /** Bytestream abstraction layer */
   struct VC_CONTAINER_BYTESTREAM_T stream;

   /** Current stream time */
   VC_CONTAINER_TIME_T time;

   /** Packetize the bytestream.
    *
    * \param  context   Pointer to the context of the instance of the packetizer
    * \param  out       Pointer to the output packet structure which needs to be filled
    * \param  flags     Miscellaneous flags controlling the packetizing
    * \return           the status of the operation
    */
   VC_CONTAINER_STATUS_T (*pf_packetize)( VC_PACKETIZER_T *context,
      VC_CONTAINER_PACKET_T *out, VC_PACKETIZER_FLAGS_T flags );

   /** Reset packetizer state.
    *
    * \param  context   Pointer to the context of the instance of the packetizer
    * \return           the status of the operation
    */
   VC_CONTAINER_STATUS_T (*pf_reset)( VC_PACKETIZER_T *context );

   /** Closes a packetizer module.
    *
    * \param  context   Pointer to the context of the instance to close
    * \return           the status of the operation
    */
   VC_CONTAINER_STATUS_T (*pf_close)( struct VC_PACKETIZER_T *context );

   /** Pointer to the packetizer module code and symbols*/
   void *module_handle;

   /** Temporary packet structure used when the caller does not provide one */
   VC_CONTAINER_PACKET_T packet;

} VC_PACKETIZER_PRIVATE_T;

/** Structure used by packetizers to register themselves with the core. */
typedef struct VC_PACKETIZER_REGISTRY_ENTRY_T
{
   struct VC_PACKETIZER_REGISTRY_ENTRY_T *next; /**< To link entries together */
   const char *name; /**< Name of the packetizer */
   VC_CONTAINER_STATUS_T (*open)( VC_PACKETIZER_T * ); /**< Called to open packetizer */
} VC_PACKETIZER_REGISTRY_ENTRY_T;

/** Register a packetizer with the core.
 *
 * \param entry Entry to register with the core
 */
void vc_packetizer_register(VC_PACKETIZER_REGISTRY_ENTRY_T *entry);

/** Utility macro used to register a packetizer with the core */
#define VC_PACKETIZER_REGISTER(func, name) \
VC_CONTAINER_CONSTRUCTOR(func##_register); \
static VC_PACKETIZER_REGISTRY_ENTRY_T registry_entry = {0, name, func}; \
void func##_register(void) { vc_packetizer_register(&registry_entry); }

/* @} */

#endif /* VC_PACKETIZERS_PRIVATE_H */
