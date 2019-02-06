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

#include "interface/vmcs_host/khronos/IL/OMX_Component.h"

#include "interface/vmcs_host/vcilcs.h"
#include "interface/vmcs_host/vcilcs_common.h"

static IL_FN_T vcilcs_fns[] = {NULL, // response
                               NULL, // create component

                               vcil_in_get_component_version,
                               NULL, // send command
                               vcil_in_get_parameter,
                               vcil_in_set_parameter,
                               vcil_in_get_config,
                               vcil_in_set_config,
                               vcil_in_get_extension_index,
                               vcil_in_get_state,
                               NULL, // tunnel request
                               vcil_in_use_buffer,
                               NULL, // use egl image
                               NULL, // allocate buffer
                               vcil_in_free_buffer,
                               vcil_in_empty_this_buffer,
                               vcil_in_fill_this_buffer,
                               NULL, // set callbacks
                               NULL, // component role enum

                               NULL, // deinit

                               vcil_out_event_handler,
                               vcil_out_empty_buffer_done,
                               vcil_out_fill_buffer_done,

                               NULL, // component name enum
                               NULL, // get debug information

                               NULL
};

static ILCS_COMMON_T *vcilcs_common_init(ILCS_SERVICE_T *ilcs)
{
   ILCS_COMMON_T *st;

   st = vcos_malloc(sizeof(ILCS_COMMON_T), "ILCS_HOST_COMMON");
   if(!st)
      return NULL;

   if(vcos_semaphore_create(&st->component_lock, "ILCS", 1) != VCOS_SUCCESS)
   {
      vcos_free(st);
      return NULL;
   }

   st->ilcs = ilcs;
   st->component_list = NULL;
   return st;
}

static void vcilcs_common_deinit(ILCS_COMMON_T *st)
{
   vcos_semaphore_delete(&st->component_lock);

   while(st->component_list)
   {
      VC_PRIVATE_COMPONENT_T *comp = st->component_list;
      st->component_list = comp->next;
      vcos_free(comp);
   }

   vcos_free(st);
}

static void vcilcs_thread_init(ILCS_COMMON_T *st)
{
}

static unsigned char *vcilcs_mem_lock(OMX_BUFFERHEADERTYPE *buffer)
{
   return buffer->pBuffer;
}

static void vcilcs_mem_unlock(OMX_BUFFERHEADERTYPE *buffer)
{
}

void vcilcs_config(ILCS_CONFIG_T *config)
{
   config->fns = vcilcs_fns;
   config->ilcs_common_init = vcilcs_common_init;
   config->ilcs_common_deinit = vcilcs_common_deinit;
   config->ilcs_thread_init = vcilcs_thread_init;
   config->ilcs_mem_lock = vcilcs_mem_lock;
   config->ilcs_mem_unlock = vcilcs_mem_unlock;
}
