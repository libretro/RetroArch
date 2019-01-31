/*
Copyright (c) 2016, Raspberry Pi (Trading) Ltd
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

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>

#include "vchost.h"

#include "interface/vcos/vcos.h"
#include "interface/vcos/vcos_logging.h"
#include "vcinclude/common.h"
#include "vc_vchi_gpuserv.h"
#include "vchiq.h"
#include "interface/vcos/vcos_stdbool.h"

// VCOS logging category
static VCOS_LOG_CAT_T vcos_log_category;
#define VCOS_LOG_CATEGORY  (&vcos_log_category)

static VCHIQ_INSTANCE_T gpuserv_client_vchiq_instance;
static VCOS_ONCE_T gpuserv_client_once = VCOS_ONCE_INIT;

/******************************************************************************
Local types and defines.
******************************************************************************/
#define GPUSERV_MAX_LENGTH 512
typedef struct {
   VCHIQ_SERVICE_HANDLE_T service;
   VCOS_MUTEX_T          lock;
   int                   initialised;
   VCOS_EVENT_T          message_available_event;
   int refcount;
} GPUSERV_SERVICE_T;

static GPUSERV_SERVICE_T gpuserv_client;


/******************************************************************************
Static function.
******************************************************************************/
static VCHIQ_STATUS_T gpuserv_callback( VCHIQ_REASON_T reason,
                                        VCHIQ_HEADER_T *header,
                                        VCHIQ_SERVICE_HANDLE_T vchiq_handle,
                                        void *bulk_userdata );

static void init_once(void)
{
   vcos_mutex_create(&gpuserv_client.lock, VCOS_FUNCTION);
}

/******************************************************************************
NAME

   vc_gpuserv_init

SYNOPSIS
   int32_t vc_gpuserv_init( void )

FUNCTION
   Initialise the gpu service for use. A negative return value
   indicates failure (which may mean it has not been started on VideoCore).

RETURNS
   zero on success
******************************************************************************/

int32_t vc_gpuserv_init( void )
{
   VCHIQ_SERVICE_PARAMS_T vchiq_params;
   VCHIQ_STATUS_T vchiq_status;

   vcos_once(&gpuserv_client_once, init_once);

   vcos_mutex_lock(&gpuserv_client.lock);

   if (gpuserv_client.refcount++ > 0)
   {
      /* Already initialised so nothing to do */
      vcos_mutex_unlock(&gpuserv_client.lock);
      return VCOS_SUCCESS;
   }

   vcos_log_set_level(VCOS_LOG_CATEGORY, VCOS_LOG_TRACE);
   vcos_log_register("gpuserv", VCOS_LOG_CATEGORY);

   vcos_log_trace("%s: starting initialisation", VCOS_FUNCTION);

   /* Initialise a VCHIQ instance */
   vchiq_status = vchiq_initialise(&gpuserv_client_vchiq_instance);
   if (vchiq_status != VCHIQ_SUCCESS)
   {
      vcos_log_error("%s: failed to initialise vchiq: %d", VCOS_FUNCTION, vchiq_status);
      goto error;
   }

   vchiq_status = vchiq_connect(gpuserv_client_vchiq_instance);
   if (vchiq_status != VCHIQ_SUCCESS)
   {
      vcos_log_error("%s: failed to connect to vchiq: %d", VCOS_FUNCTION, vchiq_status);
      goto error;
   }

   memset(&vchiq_params, 0, sizeof(vchiq_params));
   vchiq_params.fourcc = VCHIQ_MAKE_FOURCC('G','P','U','S');
   vchiq_params.callback = gpuserv_callback;
   vchiq_params.userdata = NULL;
   vchiq_params.version = 1;
   vchiq_params.version_min = 1;

   vchiq_status = vchiq_open_service(gpuserv_client_vchiq_instance, &vchiq_params, &gpuserv_client.service);
   if (vchiq_status != VCHIQ_SUCCESS)
   {
      vcos_log_error("%s: could not open vchiq service: %d", VCOS_FUNCTION, vchiq_status);
      goto error;
   }
   vcos_mutex_unlock(&gpuserv_client.lock);
   return 0;
error:
   vcos_mutex_unlock(&gpuserv_client.lock);
   return -1;
}

/******************************************************************************
NAME

   vc_gpuserv_deinit

SYNOPSIS
   void vc_gpuserv_init( void )

FUNCTION
   Deinitialise the gpu service. Should be called when gpu_service is no longer required

RETURNS
   zero on success
******************************************************************************/

void vc_gpuserv_deinit( void )
{
   vcos_mutex_lock(&gpuserv_client.lock);

   if (gpuserv_client.refcount > 0 && --gpuserv_client.refcount == 0)
   {
      vchi_service_close(gpuserv_client.service);
      gpuserv_client.service = 0;
   }
   vcos_mutex_unlock(&gpuserv_client.lock);
}

/******************************************************************************
NAME
   gpuserv_callback

SYNOPSIS
   void gpuserv_callback( VCHIQ_REASON_T reason,
                          VCHIQ_HEADER_T *header,
                          VCHIQ_SERVICE_HANDLE_T service,
                          void *bulk_userdata )
FUNCTION
   VCHIQ callback

RETURNS
   zero on success
******************************************************************************/
static VCHIQ_STATUS_T gpuserv_callback( VCHIQ_REASON_T reason,
                                        VCHIQ_HEADER_T *header,
                                        VCHIQ_SERVICE_HANDLE_T service,
                                        void *bulk_userdata )
{
   // reason is one of VCHIQ_MESSAGE_AVAILABLE, VCHIQ_BULK_TRANSMIT_DONE, VCHIQ_BULK_RECEIVE_DONE
   switch (reason)
   {
      case VCHIQ_MESSAGE_AVAILABLE:
      {
         struct gpu_callback_s *c = (struct gpu_callback_s *)header->data;
         if (c->func)
            c->func(c->cookie);
         vchiq_release_message(service, header);
         break;
      }
      default:
        ;
   }
   return 0; // Releases any command message (VCHIQ_MESSAGE_AVAILABLE), ignored otherwise
}

/******************************************************************************
NAME
   vc_gpuserv_execute_code

SYNOPSIS
   int32_t vc_gpuserv_execute_code(int num_jobs, struct gpu_job_s jobs[])

FUNCTION
   Submit a list of VPU/QPU jobs to be exeected by GPU

RETURNS
   zero on success
******************************************************************************/
#define MAX_JOBS 8
int32_t vc_gpuserv_execute_code(int num_jobs, struct gpu_job_s jobs[])
{
   VCHIQ_ELEMENT_T elements[MAX_JOBS];
   int i;

   // hack: temporarily allow calling this function without calling vc_gpuserv_init
   // will be removed later
   if (!gpuserv_client.service)
   {
      vc_gpuserv_init();
      vcos_log_error("%s: called without calling vc_gpuserv_init", VCOS_FUNCTION);
   }

   if (!gpuserv_client.service)
   {
      vcos_log_error("%s: vchiq service not initialised", VCOS_FUNCTION);
      return -1;
   }
   if (num_jobs > MAX_JOBS)
      return -1;

   for (i=0; i<num_jobs; i++)
   {
      elements[i].data = jobs + i;
      elements[i].size = sizeof *jobs;
   }
   if (vchiq_queue_message(gpuserv_client.service, elements, num_jobs) != VCHIQ_SUCCESS)
   {
      goto error_exit;
   }
   return 0;
   error_exit:
   return -1; 
}
