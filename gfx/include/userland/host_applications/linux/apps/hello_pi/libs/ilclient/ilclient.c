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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "interface/vcos/vcos.h"
#include "interface/vcos/vcos_logging.h"
#include "interface/vmcs_host/vchost.h"

#include "IL/OMX_Broadcom.h"
#include "ilclient.h"

#define VCOS_LOG_CATEGORY (&ilclient_log_category)

#ifndef ILCLIENT_THREAD_DEFAULT_STACK_SIZE
#define ILCLIENT_THREAD_DEFAULT_STACK_SIZE   (6<<10)
#endif

static VCOS_LOG_CAT_T ilclient_log_category;

/******************************************************************************
Static data and types used only in this file.
******************************************************************************/

struct _ILEVENT_T {
   OMX_EVENTTYPE eEvent;
   OMX_U32 nData1;
   OMX_U32 nData2;
   OMX_PTR pEventData;
   struct _ILEVENT_T *next;
};

#define NUM_EVENTS 100
struct _ILCLIENT_T {
   ILEVENT_T *event_list;
   VCOS_SEMAPHORE_T event_sema;
   ILEVENT_T event_rep[NUM_EVENTS];

   ILCLIENT_CALLBACK_T port_settings_callback;
   void *port_settings_callback_data;
   ILCLIENT_CALLBACK_T eos_callback;
   void *eos_callback_data;
   ILCLIENT_CALLBACK_T error_callback;
   void *error_callback_data;
   ILCLIENT_BUFFER_CALLBACK_T fill_buffer_done_callback;
   void *fill_buffer_done_callback_data;
   ILCLIENT_BUFFER_CALLBACK_T empty_buffer_done_callback;
   void *empty_buffer_done_callback_data;
   ILCLIENT_CALLBACK_T configchanged_callback;
   void *configchanged_callback_data;
};

struct _COMPONENT_T {
   OMX_HANDLETYPE comp;
   ILCLIENT_CREATE_FLAGS_T flags;
   VCOS_SEMAPHORE_T sema;
   VCOS_EVENT_FLAGS_T event;
   struct _COMPONENT_T *related;
   OMX_BUFFERHEADERTYPE *out_list;
   OMX_BUFFERHEADERTYPE *in_list;
   char name[32];
   char bufname[32];
   unsigned int error_mask;
   unsigned int private;
   ILEVENT_T *list;
   ILCLIENT_T *client;
};

#define random_wait()
static char *states[] = {"Invalid", "Loaded", "Idle", "Executing", "Pause", "WaitingForResources"};

typedef enum {
   ILCLIENT_ERROR_UNPOPULATED  = 0x1,
   ILCLIENT_ERROR_SAMESTATE    = 0x2,
   ILCLIENT_ERROR_BADPARAMETER = 0x4
} ILERROR_MASK_T;

/******************************************************************************
Static functions.
******************************************************************************/

static OMX_ERRORTYPE ilclient_empty_buffer_done(OMX_IN OMX_HANDLETYPE hComponent,
      OMX_IN OMX_PTR pAppData,
      OMX_IN OMX_BUFFERHEADERTYPE* pBuffer);
static OMX_ERRORTYPE ilclient_empty_buffer_done_error(OMX_IN OMX_HANDLETYPE hComponent,
      OMX_IN OMX_PTR pAppData,
      OMX_IN OMX_BUFFERHEADERTYPE* pBuffer);
static OMX_ERRORTYPE ilclient_fill_buffer_done(OMX_OUT OMX_HANDLETYPE hComponent,
      OMX_OUT OMX_PTR pAppData,
      OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);
static OMX_ERRORTYPE ilclient_fill_buffer_done_error(OMX_OUT OMX_HANDLETYPE hComponent,
      OMX_OUT OMX_PTR pAppData,
      OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);
static OMX_ERRORTYPE ilclient_event_handler(OMX_IN OMX_HANDLETYPE hComponent,
      OMX_IN OMX_PTR pAppData,
      OMX_IN OMX_EVENTTYPE eEvent,
      OMX_IN OMX_U32 nData1,
      OMX_IN OMX_U32 nData2,
      OMX_IN OMX_PTR pEventData);
static void ilclient_lock_events(ILCLIENT_T *st);
static void ilclient_unlock_events(ILCLIENT_T *st);

/******************************************************************************
Global functions
******************************************************************************/

/***********************************************************
 * Name: ilclient_init
 *
 * Description: Creates ilclient pointer
 *
 * Returns: pointer to client structure
 ***********************************************************/
ILCLIENT_T *ilclient_init()
{
   ILCLIENT_T *st = vcos_malloc(sizeof(ILCLIENT_T), "ilclient");
   int i;
   
   if (!st)
      return NULL;
   
   vcos_log_set_level(VCOS_LOG_CATEGORY, VCOS_LOG_WARN);
   vcos_log_register("ilclient", VCOS_LOG_CATEGORY);

   memset(st, 0, sizeof(ILCLIENT_T));

   i = vcos_semaphore_create(&st->event_sema, "il:event", 1);
   vc_assert(i == VCOS_SUCCESS);

   ilclient_lock_events(st);
   st->event_list = NULL;
   for (i=0; i<NUM_EVENTS; i++)
   {
      st->event_rep[i].eEvent = -1; // mark as unused
      st->event_rep[i].next = st->event_list;
      st->event_list = st->event_rep+i;
   }
   ilclient_unlock_events(st);
   return st;
}

/***********************************************************
 * Name: ilclient_destroy
 *
 * Description: frees client state
 *
 * Returns: void
 ***********************************************************/
void ilclient_destroy(ILCLIENT_T *st)
{
   vcos_semaphore_delete(&st->event_sema);
   vcos_free(st);
   vcos_log_unregister(VCOS_LOG_CATEGORY);
}

/***********************************************************
 * Name: ilclient_set_port_settings_callback
 *
 * Description: sets the callback used when receiving port settings
 * changed messages.  The data field in the callback function will be
 * the port index reporting the message.
 *
 * Returns: void
 ***********************************************************/
void ilclient_set_port_settings_callback(ILCLIENT_T *st, ILCLIENT_CALLBACK_T func, void *userdata)
{
   st->port_settings_callback = func;
   st->port_settings_callback_data = userdata;
}

/***********************************************************
 * Name: ilclient_set_eos_callback
 *
 * Description: sets the callback used when receiving eos flags.  The
 * data parameter in the callback function will be the port index
 * reporting an eos flag.
 *
 * Returns: void
 ***********************************************************/
void ilclient_set_eos_callback(ILCLIENT_T *st, ILCLIENT_CALLBACK_T func, void *userdata)
{
   st->eos_callback = func;
   st->eos_callback_data = userdata;
}

/***********************************************************
 * Name: ilclient_set_error_callback
 *
 * Description: sets the callback used when receiving error events.
 * The data parameter in the callback function will be the error code
 * being reported.
 *
 * Returns: void
 ***********************************************************/
void ilclient_set_error_callback(ILCLIENT_T *st, ILCLIENT_CALLBACK_T func, void *userdata)
{
   st->error_callback = func;
   st->error_callback_data = userdata;
}

/***********************************************************
 * Name: ilclient_set_fill_buffer_done_callback
 *
 * Description: sets the callback used when receiving
 * fill_buffer_done event
 *
 * Returns: void
 ***********************************************************/
void ilclient_set_fill_buffer_done_callback(ILCLIENT_T *st, ILCLIENT_BUFFER_CALLBACK_T func, void *userdata)
{
   st->fill_buffer_done_callback = func;
   st->fill_buffer_done_callback_data = userdata;
}

/***********************************************************
 * Name: ilclient_set_empty_buffer_done_callback
 *
 * Description: sets the callback used when receiving
 * empty_buffer_done event
 *
 * Returns: void
 ***********************************************************/
void ilclient_set_empty_buffer_done_callback(ILCLIENT_T *st, ILCLIENT_BUFFER_CALLBACK_T func, void *userdata)
{
   st->empty_buffer_done_callback = func;
   st->empty_buffer_done_callback_data = userdata;
}

/***********************************************************
 * Name: ilclient_set_configchanged_callback
 *
 * Description: sets the callback used when a config changed
 * event is received
 *
 * Returns: void
 ***********************************************************/
void ilclient_set_configchanged_callback(ILCLIENT_T *st, ILCLIENT_CALLBACK_T func, void *userdata)
{
   st->configchanged_callback = func;
   st->configchanged_callback_data = userdata;
}

/***********************************************************
 * Name: ilclient_create_component
 *
 * Description: initialises a component state structure and creates
 * the IL component.
 *
 * Returns: 0 on success, -1 on failure
 ***********************************************************/
int ilclient_create_component(ILCLIENT_T *client, COMPONENT_T **comp, char *name,
                              ILCLIENT_CREATE_FLAGS_T flags)
{
   OMX_CALLBACKTYPE callbacks;
   OMX_ERRORTYPE error;
   char component_name[128];
   int32_t status;

   *comp = vcos_malloc(sizeof(COMPONENT_T), "il:comp");
   if(!*comp)
      return -1;

   memset(*comp, 0, sizeof(COMPONENT_T));

#define COMP_PREFIX "OMX.broadcom."

   status = vcos_event_flags_create(&(*comp)->event,"il:comp");
   vc_assert(status == VCOS_SUCCESS);
   status = vcos_semaphore_create(&(*comp)->sema, "il:comp", 1);
   vc_assert(status == VCOS_SUCCESS);
   (*comp)->client = client;

   vcos_snprintf((*comp)->name, sizeof((*comp)->name), "cl:%s", name);
   vcos_snprintf((*comp)->bufname, sizeof((*comp)->bufname), "cl:%s buffer", name);
   vcos_snprintf(component_name, sizeof(component_name), "%s%s", COMP_PREFIX, name);

   (*comp)->flags = flags;

   callbacks.EventHandler = ilclient_event_handler;
   callbacks.EmptyBufferDone = flags & ILCLIENT_ENABLE_INPUT_BUFFERS ? ilclient_empty_buffer_done : ilclient_empty_buffer_done_error;
   callbacks.FillBufferDone = flags & ILCLIENT_ENABLE_OUTPUT_BUFFERS ? ilclient_fill_buffer_done : ilclient_fill_buffer_done_error;

   error = OMX_GetHandle(&(*comp)->comp, component_name, *comp, &callbacks);

   if (error == OMX_ErrorNone)
   {
      OMX_UUIDTYPE uid;
      char name[128];
      OMX_VERSIONTYPE compVersion, specVersion;

      if(OMX_GetComponentVersion((*comp)->comp, name, &compVersion, &specVersion, &uid) == OMX_ErrorNone)
      {
         char *p = (char *) uid + strlen(COMP_PREFIX);

         vcos_snprintf((*comp)->name, sizeof((*comp)->name), "cl:%s", p);
         (*comp)->name[sizeof((*comp)->name)-1] = 0;
         vcos_snprintf((*comp)->bufname, sizeof((*comp)->bufname), "cl:%s buffer", p);
         (*comp)->bufname[sizeof((*comp)->bufname)-1] = 0;
      }

      if(flags & (ILCLIENT_DISABLE_ALL_PORTS | ILCLIENT_OUTPUT_ZERO_BUFFERS))
      {
         OMX_PORT_PARAM_TYPE ports;
         OMX_INDEXTYPE types[] = {OMX_IndexParamAudioInit, OMX_IndexParamVideoInit, OMX_IndexParamImageInit, OMX_IndexParamOtherInit};
         int i;

         ports.nSize = sizeof(OMX_PORT_PARAM_TYPE);
         ports.nVersion.nVersion = OMX_VERSION;

         for(i=0; i<4; i++)
         {
            OMX_ERRORTYPE error = OMX_GetParameter((*comp)->comp, types[i], &ports);
            if(error == OMX_ErrorNone)
            {
               uint32_t j;
               for(j=0; j<ports.nPorts; j++)
               {
                  if(flags & ILCLIENT_DISABLE_ALL_PORTS)
                     ilclient_disable_port(*comp, ports.nStartPortNumber+j);
                  
                  if(flags & ILCLIENT_OUTPUT_ZERO_BUFFERS)
                  {
                     OMX_PARAM_PORTDEFINITIONTYPE portdef;
                     portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
                     portdef.nVersion.nVersion = OMX_VERSION;
                     portdef.nPortIndex = ports.nStartPortNumber+j;
                     if(OMX_GetParameter((*comp)->comp, OMX_IndexParamPortDefinition, &portdef) == OMX_ErrorNone)
                     {
                        if(portdef.eDir == OMX_DirOutput && portdef.nBufferCountActual > 0)
                        {
                           portdef.nBufferCountActual = 0;
                           OMX_SetParameter((*comp)->comp, OMX_IndexParamPortDefinition, &portdef);
                        }
                     }
                  }
               }
            }
         }
      }
      return 0;
   }
   else
   {
      vcos_event_flags_delete(&(*comp)->event);
      vcos_semaphore_delete(&(*comp)->sema);
      vcos_free(*comp);
      *comp = NULL;
      return -1;
   }
}

/***********************************************************
 * Name: ilclient_remove_event
 *
 * Description: Removes an event from a component event list.  ignore1
 * and ignore2 are flags indicating whether to not match on nData1 and
 * nData2 respectively.
 *
 * Returns: 0 if the event was removed.  -1 if no matching event was
 * found.
 ***********************************************************/
int ilclient_remove_event(COMPONENT_T *st, OMX_EVENTTYPE eEvent,
                          OMX_U32 nData1, int ignore1, OMX_IN OMX_U32 nData2, int ignore2)
{
   ILEVENT_T *cur, *prev;
   uint32_t set;
   ilclient_lock_events(st->client);

   cur = st->list;
   prev = NULL;

   while (cur && !(cur->eEvent == eEvent && (ignore1 || cur->nData1 == nData1) && (ignore2 || cur->nData2 == nData2)))
   {
      prev = cur;
      cur = cur->next;
   }

   if (cur == NULL)
   {
      ilclient_unlock_events(st->client);
      return -1;
   }

   if (prev == NULL)
      st->list = cur->next;
   else
      prev->next = cur->next;

   // add back into spare list
   cur->next = st->client->event_list;
   st->client->event_list = cur;
   cur->eEvent = -1; // mark as unused

   // if we're removing an OMX_EventError or OMX_EventParamOrConfigChanged event, then clear the error bit from the eventgroup,
   // since the user might have been notified through the error callback, and then 
   // can't clear the event bit - this will then cause problems the next time they
   // wait for an error.
   if(eEvent == OMX_EventError)
      vcos_event_flags_get(&st->event, ILCLIENT_EVENT_ERROR, VCOS_OR_CONSUME, 0, &set);
   else if(eEvent == OMX_EventParamOrConfigChanged)
      vcos_event_flags_get(&st->event, ILCLIENT_CONFIG_CHANGED, VCOS_OR_CONSUME, 0, &set);

   ilclient_unlock_events(st->client);
   return 0;
}

/***********************************************************
 * Name: ilclient_state_transition
 *
 * Description: Transitions a null terminated list of IL components to
 * a given state.  All components are told to transition in a random
 * order before any are checked for transition completion.
 *
 * Returns: void
 ***********************************************************/
void ilclient_state_transition(COMPONENT_T *list[], OMX_STATETYPE state)
{
   OMX_ERRORTYPE error;
   int i, num;
   uint32_t set;

   num=0;
   while (list[num])
      num++;

   // if we transition the supplier port first, it will call freebuffer on the non
   // supplier, which will correctly signal a port unpopulated error.  We want to
   // ignore these errors.
   if (state == OMX_StateLoaded)
      for (i=0; i<num; i++)
         list[i]->error_mask |= ILCLIENT_ERROR_UNPOPULATED;
   for (i=0; i<num; i++)
      list[i]->private = ((rand() >> 13) & 0xff)+1;

   for (i=0; i<num; i++)
   {
      // transition the components in a random order
      int j, min = -1;
      for (j=0; j<num; j++)
         if (list[j]->private && (min == -1 || list[min]->private > list[j]->private))
            min = j;

      list[min]->private = 0;

      random_wait();
      //Clear error event for this component
      vcos_event_flags_get(&list[min]->event, ILCLIENT_EVENT_ERROR, VCOS_OR_CONSUME, 0, &set);

      error = OMX_SendCommand(list[min]->comp, OMX_CommandStateSet, state, NULL);
      vc_assert(error == OMX_ErrorNone);
   }

   random_wait();

   for (i=0; i<num; i++)
      if(ilclient_wait_for_command_complete(list[i], OMX_CommandStateSet, state) < 0)
         vc_assert(0);

   if (state == OMX_StateLoaded)
      for (i=0; i<num; i++)
         list[i]->error_mask &= ~ILCLIENT_ERROR_UNPOPULATED;
}

/***********************************************************
 * Name: ilclient_teardown_tunnels
 *
 * Description: tears down a null terminated list of tunnels.
 *
 * Returns: void
 ***********************************************************/
void ilclient_teardown_tunnels(TUNNEL_T *tunnel)
{
   int i;
   OMX_ERRORTYPE error;

   i=0;;
   while (tunnel[i].source)
   {
      error = OMX_SetupTunnel(tunnel[i].source->comp, tunnel[i].source_port, NULL, 0);
      vc_assert(error == OMX_ErrorNone);

      error = OMX_SetupTunnel(tunnel[i].sink->comp, tunnel[i].sink_port, NULL, 0);
      vc_assert(error == OMX_ErrorNone);
      i++;
   }
}

/***********************************************************
 * Name: ilclient_disable_tunnel
 *
 * Description: disables a tunnel by disabling the ports.  Allows
 * ports to signal same state error if they were already disabled.
 *
 * Returns: void
 ***********************************************************/
void ilclient_disable_tunnel(TUNNEL_T *tunnel)
{
   OMX_ERRORTYPE error;
   
   if(tunnel->source == 0 || tunnel->sink == 0)
      return;

   tunnel->source->error_mask |= ILCLIENT_ERROR_UNPOPULATED;
   tunnel->sink->error_mask |= ILCLIENT_ERROR_UNPOPULATED;

   error = OMX_SendCommand(tunnel->source->comp, OMX_CommandPortDisable, tunnel->source_port, NULL);
   vc_assert(error == OMX_ErrorNone);

   error = OMX_SendCommand(tunnel->sink->comp, OMX_CommandPortDisable, tunnel->sink_port, NULL);
   vc_assert(error == OMX_ErrorNone);

   if(ilclient_wait_for_command_complete(tunnel->source, OMX_CommandPortDisable, tunnel->source_port) < 0)
      vc_assert(0);

   if(ilclient_wait_for_command_complete(tunnel->sink, OMX_CommandPortDisable, tunnel->sink_port) < 0)
      vc_assert(0);

   tunnel->source->error_mask &= ~ILCLIENT_ERROR_UNPOPULATED;
   tunnel->sink->error_mask &= ~ILCLIENT_ERROR_UNPOPULATED;
}

/***********************************************************
 * Name: ilclient_enable_tunnel
 *
 * Description: enables a tunnel by enabling the ports
 *
 * Returns: 0 on success, -1 on failure
 ***********************************************************/
int ilclient_enable_tunnel(TUNNEL_T *tunnel)
{
   OMX_STATETYPE state;
   OMX_ERRORTYPE error;

   ilclient_debug_output("ilclient: enable tunnel from %x/%d to %x/%d",
                         tunnel->source, tunnel->source_port,
                         tunnel->sink, tunnel->sink_port);

   error = OMX_SendCommand(tunnel->source->comp, OMX_CommandPortEnable, tunnel->source_port, NULL);
   vc_assert(error == OMX_ErrorNone);

   error = OMX_SendCommand(tunnel->sink->comp, OMX_CommandPortEnable, tunnel->sink_port, NULL);
   vc_assert(error == OMX_ErrorNone);

   // to complete, the sink component can't be in loaded state
   error = OMX_GetState(tunnel->sink->comp, &state);
   vc_assert(error == OMX_ErrorNone);
   if (state == OMX_StateLoaded)
   {
      int ret = 0;

      if(ilclient_wait_for_command_complete(tunnel->sink, OMX_CommandPortEnable, tunnel->sink_port) != 0 ||
         OMX_SendCommand(tunnel->sink->comp, OMX_CommandStateSet, OMX_StateIdle, NULL) != OMX_ErrorNone ||
         (ret = ilclient_wait_for_command_complete_dual(tunnel->sink, OMX_CommandStateSet, OMX_StateIdle, tunnel->source)) < 0)
      {
         if(ret == -2)
         {
            // the error was reported fom the source component: clear this error and disable the sink component
            ilclient_wait_for_command_complete(tunnel->source, OMX_CommandPortEnable, tunnel->source_port);
            ilclient_disable_port(tunnel->sink, tunnel->sink_port);
         }

         ilclient_debug_output("ilclient: could not change component state to IDLE");
         ilclient_disable_port(tunnel->source, tunnel->source_port);
         return -1;
      }
   }
   else
   {
      if (ilclient_wait_for_command_complete(tunnel->sink, OMX_CommandPortEnable, tunnel->sink_port) != 0)
      {
         ilclient_debug_output("ilclient: could not change sink port %d to enabled", tunnel->sink_port);

         //Oops failed to enable the sink port
         ilclient_disable_port(tunnel->source, tunnel->source_port);
         //Clean up the port enable event from the source port.
         ilclient_wait_for_event(tunnel->source, OMX_EventCmdComplete,
                                 OMX_CommandPortEnable, 0, tunnel->source_port, 0,
                                 ILCLIENT_PORT_ENABLED | ILCLIENT_EVENT_ERROR, VCOS_EVENT_FLAGS_SUSPEND);
         return -1;
      }
   }

   if(ilclient_wait_for_command_complete(tunnel->source, OMX_CommandPortEnable, tunnel->source_port) != 0)
   {
      ilclient_debug_output("ilclient: could not change source port %d to enabled", tunnel->source_port);

      //Failed to enable the source port
      ilclient_disable_port(tunnel->sink, tunnel->sink_port);
      return -1;
   }

   return 0;
}


/***********************************************************
 * Name: ilclient_flush_tunnels
 *
 * Description: flushes all ports used in a null terminated list of
 * tunnels.  max specifies the maximum number of tunnels to flush from
 * the list, where max=0 means all tunnels.
 *
 * Returns: void
 ***********************************************************/
void ilclient_flush_tunnels(TUNNEL_T *tunnel, int max)
{
   OMX_ERRORTYPE error;
   int i;

   i=0;
   while (tunnel[i].source && (max == 0 || i < max))
   {
      error = OMX_SendCommand(tunnel[i].source->comp, OMX_CommandFlush, tunnel[i].source_port, NULL);
      vc_assert(error == OMX_ErrorNone);

      error = OMX_SendCommand(tunnel[i].sink->comp, OMX_CommandFlush, tunnel[i].sink_port, NULL);
      vc_assert(error == OMX_ErrorNone);

      ilclient_wait_for_event(tunnel[i].source, OMX_EventCmdComplete,
                              OMX_CommandFlush, 0, tunnel[i].source_port, 0,
                              ILCLIENT_PORT_FLUSH, VCOS_EVENT_FLAGS_SUSPEND);
      ilclient_wait_for_event(tunnel[i].sink, OMX_EventCmdComplete,
                              OMX_CommandFlush, 0, tunnel[i].sink_port, 0,
                              ILCLIENT_PORT_FLUSH, VCOS_EVENT_FLAGS_SUSPEND);
      i++;
   }
}


/***********************************************************
 * Name: ilclient_return_events
 *
 * Description: Returns all events from a component event list to the
 * list of unused event structures.
 *
 * Returns: void
 ***********************************************************/
void ilclient_return_events(COMPONENT_T *comp)
{
   ilclient_lock_events(comp->client);
   while (comp->list)
   {
      ILEVENT_T *next = comp->list->next;
      comp->list->next = comp->client->event_list;
      comp->client->event_list = comp->list;
      comp->list = next;
   }
   ilclient_unlock_events(comp->client);
}

/***********************************************************
 * Name: ilclient_cleanup_components
 *
 * Description: frees all components from a null terminated list and
 * deletes resources used in component state structure.
 *
 * Returns: void
 ***********************************************************/
void ilclient_cleanup_components(COMPONENT_T *list[])
{
   int i;
   OMX_ERRORTYPE error;

   i=0;
   while (list[i])
   {
      ilclient_return_events(list[i]);
      if (list[i]->comp)
      {
         error = OMX_FreeHandle(list[i]->comp);

         vc_assert(error == OMX_ErrorNone);
      }
      i++;
   }

   i=0;
   while (list[i])
   {
      vcos_event_flags_delete(&list[i]->event);
      vcos_semaphore_delete(&list[i]->sema);
      vcos_free(list[i]);
      list[i] = NULL;
      i++;
   }
}

/***********************************************************
 * Name: ilclient_change_component_state
 *
 * Description: changes the state of a single component.  Note: this
 * may not be suitable if the component is tunnelled and requires
 * connected components to also change state.
 *
 * Returns: 0 on success, -1 on failure (note - trying to change to
 * the same state which causes a OMX_ErrorSameState is treated as
 * success)
 ***********************************************************/
int ilclient_change_component_state(COMPONENT_T *comp, OMX_STATETYPE state)
{
   OMX_ERRORTYPE error;
   error = OMX_SendCommand(comp->comp, OMX_CommandStateSet, state, NULL);
   vc_assert(error == OMX_ErrorNone);
   if(ilclient_wait_for_command_complete(comp, OMX_CommandStateSet, state) < 0)
   {
      ilclient_debug_output("ilclient: could not change component state to %d", state);
      ilclient_remove_event(comp, OMX_EventError, 0, 1, 0, 1);
      return -1;
   }
   return 0;
}

/***********************************************************
 * Name: ilclient_disable_port
 *
 * Description: disables a port on a given component.
 *
 * Returns: void
 ***********************************************************/
void ilclient_disable_port(COMPONENT_T *comp, int portIndex)
{
   OMX_ERRORTYPE error;
   error = OMX_SendCommand(comp->comp, OMX_CommandPortDisable, portIndex, NULL);
   vc_assert(error == OMX_ErrorNone);
   if(ilclient_wait_for_command_complete(comp, OMX_CommandPortDisable, portIndex) < 0)
      vc_assert(0);
}

/***********************************************************
 * Name: ilclient_enabled_port
 *
 * Description: enables a port on a given component.
 *
 * Returns: void
 ***********************************************************/
void ilclient_enable_port(COMPONENT_T *comp, int portIndex)
{
   OMX_ERRORTYPE error;
   error = OMX_SendCommand(comp->comp, OMX_CommandPortEnable, portIndex, NULL);
   vc_assert(error == OMX_ErrorNone);
   if(ilclient_wait_for_command_complete(comp, OMX_CommandPortEnable, portIndex) < 0)
      vc_assert(0);
}


/***********************************************************
 * Name: ilclient_enable_port_buffers
 *
 * Description: enables a port on a given component which requires
 * buffers to be supplied by the client.
 *
 * Returns: void
 ***********************************************************/
int ilclient_enable_port_buffers(COMPONENT_T *comp, int portIndex,
                                 ILCLIENT_MALLOC_T ilclient_malloc,
                                 ILCLIENT_FREE_T ilclient_free,
                                 void *private)
{
   OMX_ERRORTYPE error;
   OMX_PARAM_PORTDEFINITIONTYPE portdef;
   OMX_BUFFERHEADERTYPE *list = NULL, **end = &list;
   OMX_STATETYPE state;
   int i;

   memset(&portdef, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
   portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
   portdef.nVersion.nVersion = OMX_VERSION;
   portdef.nPortIndex = portIndex;
   
   // work out buffer requirements, check port is in the right state
   error = OMX_GetParameter(comp->comp, OMX_IndexParamPortDefinition, &portdef);
   if(error != OMX_ErrorNone || portdef.bEnabled != OMX_FALSE || portdef.nBufferCountActual == 0 || portdef.nBufferSize == 0)
      return -1;

   // check component is in the right state to accept buffers
   error = OMX_GetState(comp->comp, &state);
   if (error != OMX_ErrorNone || !(state == OMX_StateIdle || state == OMX_StateExecuting || state == OMX_StatePause))
      return -1;

   // send the command
   error = OMX_SendCommand(comp->comp, OMX_CommandPortEnable, portIndex, NULL);
   vc_assert(error == OMX_ErrorNone);

   for (i=0; i != portdef.nBufferCountActual; i++)
   {
      unsigned char *buf;
      if(ilclient_malloc)
         buf = ilclient_malloc(private, portdef.nBufferSize, portdef.nBufferAlignment, comp->bufname);
      else
         buf = vcos_malloc_aligned(portdef.nBufferSize, portdef.nBufferAlignment, comp->bufname);

      if(!buf)
         break;

      error = OMX_UseBuffer(comp->comp, end, portIndex, NULL, portdef.nBufferSize, buf);
      if(error != OMX_ErrorNone)
      {
         if(ilclient_free)
            ilclient_free(private, buf);
         else
            vcos_free(buf);

         break;
      }
      end = (OMX_BUFFERHEADERTYPE **) &((*end)->pAppPrivate);
   }

   // queue these buffers
   vcos_semaphore_wait(&comp->sema);

   if(portdef.eDir == OMX_DirInput)
   {
      *end = comp->in_list;
      comp->in_list = list;
   }
   else
   {
      *end = comp->out_list;
      comp->out_list = list;
   }

   vcos_semaphore_post(&comp->sema);

   if(i != portdef.nBufferCountActual ||
      ilclient_wait_for_command_complete(comp, OMX_CommandPortEnable, portIndex) < 0)
   {
      ilclient_disable_port_buffers(comp, portIndex, NULL, ilclient_free, private);

      // at this point the first command might have terminated with an error, which means that
      // the port is disabled before the disable_port_buffers function is called, so we're left
      // with the error bit set and an error event in the queue.  Clear these now if they exist.
      ilclient_remove_event(comp, OMX_EventError, 0, 1, 1, 0);

      return -1;
   }

   // success
   return 0;
}


/***********************************************************
 * Name: ilclient_disable_port_buffers
 *
 * Description: disables a port on a given component which has
 * buffers supplied by the client.
 *
 * Returns: void
 ***********************************************************/
void ilclient_disable_port_buffers(COMPONENT_T *comp, int portIndex,
                                   OMX_BUFFERHEADERTYPE *bufferList,
                                   ILCLIENT_FREE_T ilclient_free,
                                   void *private)
{
   OMX_ERRORTYPE error;
   OMX_BUFFERHEADERTYPE *list = bufferList;
   OMX_BUFFERHEADERTYPE **head, *clist, *prev;
   OMX_PARAM_PORTDEFINITIONTYPE portdef;
   int num;

   // get the buffers off the relevant queue
   memset(&portdef, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
   portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
   portdef.nVersion.nVersion = OMX_VERSION;
   portdef.nPortIndex = portIndex;
   
   // work out buffer requirements, check port is in the right state
   error = OMX_GetParameter(comp->comp, OMX_IndexParamPortDefinition, &portdef);
   if(error != OMX_ErrorNone || portdef.bEnabled != OMX_TRUE || portdef.nBufferCountActual == 0 || portdef.nBufferSize == 0)
      return;
   
   num = portdef.nBufferCountActual;
   
   error = OMX_SendCommand(comp->comp, OMX_CommandPortDisable, portIndex, NULL);
   vc_assert(error == OMX_ErrorNone);
      
   while(num > 0)
   {
      VCOS_UNSIGNED set;

      if(list == NULL)
      {
         vcos_semaphore_wait(&comp->sema);
         
         // take buffers for this port off the relevant queue
         head = portdef.eDir == OMX_DirInput ? &comp->in_list : &comp->out_list;
         clist = *head;
         prev = NULL;
         
         while(clist)
         {
            if((portdef.eDir == OMX_DirInput ? clist->nInputPortIndex : clist->nOutputPortIndex) == portIndex)
            {
               OMX_BUFFERHEADERTYPE *pBuffer = clist;
               
               if(!prev)
                  clist = *head = (OMX_BUFFERHEADERTYPE *) pBuffer->pAppPrivate;
               else
                  clist = prev->pAppPrivate = (OMX_BUFFERHEADERTYPE *) pBuffer->pAppPrivate;
               
               pBuffer->pAppPrivate = list;
               list = pBuffer;
            }
            else
            {
               prev = clist;
               clist = (OMX_BUFFERHEADERTYPE *) &(clist->pAppPrivate);
            }
         }
         
         vcos_semaphore_post(&comp->sema);
      }

      while(list)
      {
         void *buf = list->pBuffer;
         OMX_BUFFERHEADERTYPE *next = list->pAppPrivate;
         
         error = OMX_FreeBuffer(comp->comp, portIndex, list);
         vc_assert(error == OMX_ErrorNone);
         
         if(ilclient_free)
            ilclient_free(private, buf);
         else
            vcos_free(buf);
         
         num--;
         list = next;
      }

      if(num)
      {
         OMX_U32 mask = ILCLIENT_PORT_DISABLED | ILCLIENT_EVENT_ERROR;
         mask |= (portdef.eDir == OMX_DirInput ? ILCLIENT_EMPTY_BUFFER_DONE : ILCLIENT_FILL_BUFFER_DONE);

         // also wait for command complete/error in case we didn't have all the buffers allocated
         vcos_event_flags_get(&comp->event, mask, VCOS_OR_CONSUME, -1, &set);

         if((set & ILCLIENT_EVENT_ERROR) && ilclient_remove_event(comp, OMX_EventError, 0, 1, 1, 0) >= 0)
            return;

         if((set & ILCLIENT_PORT_DISABLED) && ilclient_remove_event(comp, OMX_EventCmdComplete, OMX_CommandPortDisable, 0, portIndex, 0) >= 0)
            return;
      }            
   }
  
   if(ilclient_wait_for_command_complete(comp, OMX_CommandPortDisable, portIndex) < 0)
      vc_assert(0);
}


/***********************************************************
 * Name: ilclient_setup_tunnel
 *
 * Description: creates a tunnel between components that require that
 * ports be inititially disabled, then enabled after tunnel setup.  If
 * timeout is non-zero, it will initially wait until a port settings
 * changes message has been received by the output port.  If port
 * streams are supported by the output port, the requested port stream
 * will be selected.
 *
 * Returns: 0 indicates success, negative indicates failure.
 * -1: a timeout waiting for the parameter changed
 * -2: an error was returned instead of parameter changed
 * -3: no streams are available from this port
 * -4: requested stream is not available from this port
 * -5: the data format was not acceptable to the sink
 ***********************************************************/
int ilclient_setup_tunnel(TUNNEL_T *tunnel, unsigned int portStream, int timeout)
{
   OMX_ERRORTYPE error;
   OMX_PARAM_U32TYPE param;
   OMX_STATETYPE state;
   int32_t status;
   int enable_error;

   // source component must at least be idle, not loaded
   error = OMX_GetState(tunnel->source->comp, &state);
   vc_assert(error == OMX_ErrorNone);
   if (state == OMX_StateLoaded && ilclient_change_component_state(tunnel->source, OMX_StateIdle) < 0)
      return -2;

   // wait for the port parameter changed from the source port
   if(timeout)
   {
      status = ilclient_wait_for_event(tunnel->source, OMX_EventPortSettingsChanged,
                                       tunnel->source_port, 0, -1, 1,
                                       ILCLIENT_PARAMETER_CHANGED | ILCLIENT_EVENT_ERROR, timeout);
      
      if (status < 0)
      {
         ilclient_debug_output(
            "ilclient: timed out waiting for port settings changed on port %d", tunnel->source_port);
         return status;
      }
   }

   // disable ports
   ilclient_disable_tunnel(tunnel);

   // if this source port uses port streams, we need to select one of them before proceeding
   // if getparameter causes an error that's fine, nothing needs selecting
   param.nSize = sizeof(OMX_PARAM_U32TYPE);
   param.nVersion.nVersion = OMX_VERSION;
   param.nPortIndex = tunnel->source_port;
   if (OMX_GetParameter(tunnel->source->comp, OMX_IndexParamNumAvailableStreams, &param) == OMX_ErrorNone)
   {
      if (param.nU32 == 0)
      {
         // no streams available
         // leave the source port disabled, and return a failure
         return -3;
      }
      if (param.nU32 <= portStream)
      {
         // requested stream not available
         // no streams available
         // leave the source port disabled, and return a failure
         return -4;
      }

      param.nU32 = portStream;
      error = OMX_SetParameter(tunnel->source->comp, OMX_IndexParamActiveStream, &param);
      vc_assert(error == OMX_ErrorNone);
   }

   // now create the tunnel
   error = OMX_SetupTunnel(tunnel->source->comp, tunnel->source_port, tunnel->sink->comp, tunnel->sink_port);

   enable_error = 0;

   if (error != OMX_ErrorNone || (enable_error=ilclient_enable_tunnel(tunnel)) < 0)
   {
      // probably format not compatible
      error = OMX_SetupTunnel(tunnel->source->comp, tunnel->source_port, NULL, 0);
      vc_assert(error == OMX_ErrorNone);
      error = OMX_SetupTunnel(tunnel->sink->comp, tunnel->sink_port, NULL, 0);
      vc_assert(error == OMX_ErrorNone);
      
      if(enable_error)
      {
         //Clean up the errors. This does risk removing an error that was nothing to do with this tunnel :-/
         ilclient_remove_event(tunnel->sink, OMX_EventError, 0, 1, 0, 1);
         ilclient_remove_event(tunnel->source, OMX_EventError, 0, 1, 0, 1);
      }

      ilclient_debug_output("ilclient: could not setup/enable tunnel (setup=0x%x,enable=%d)",
                             error, enable_error);
      return -5;
   }

   return 0;
}

/***********************************************************
 * Name: ilclient_wait_for_event
 *
 * Description: waits for a given event to appear on a component event
 * list.  If not immediately present, will wait on that components
 * event group for the given event flag.
 *
 * Returns: 0 indicates success, negative indicates failure.
 * -1: a timeout was received.
 * -2: an error event was received.
 * -3: a config change event was received.
 ***********************************************************/
int ilclient_wait_for_event(COMPONENT_T *comp, OMX_EVENTTYPE event,
                            OMX_U32 nData1, int ignore1, OMX_IN OMX_U32 nData2, int ignore2,
                            int event_flag, int suspend)
{
   int32_t status;
   uint32_t set;

   while (ilclient_remove_event(comp, event, nData1, ignore1, nData2, ignore2) < 0)
   {
      // if we want to be notified of errors, check the list for an error now
      // before blocking, the event flag may have been cleared already.
      if(event_flag & ILCLIENT_EVENT_ERROR)
      {
         ILEVENT_T *cur;
         ilclient_lock_events(comp->client);
         cur = comp->list;
         while(cur && cur->eEvent != OMX_EventError)            
            cur = cur->next;
         
         if(cur)
         {
            // clear error flag
            vcos_event_flags_get(&comp->event, ILCLIENT_EVENT_ERROR, VCOS_OR_CONSUME, 0, &set);
            ilclient_unlock_events(comp->client);
            return -2;
         }

         ilclient_unlock_events(comp->client);
      }
      // check for config change event if we are asked to be notified of that
      if(event_flag & ILCLIENT_CONFIG_CHANGED)
      {
         ILEVENT_T *cur;
         ilclient_lock_events(comp->client);
         cur = comp->list;
         while(cur && cur->eEvent != OMX_EventParamOrConfigChanged)
            cur = cur->next;
         
         ilclient_unlock_events(comp->client);

         if(cur)
            return ilclient_remove_event(comp, event, nData1, ignore1, nData2, ignore2) == 0 ? 0 : -3;
      }

      status = vcos_event_flags_get(&comp->event, event_flag, VCOS_OR_CONSUME, 
                                    suspend, &set);
      if (status != 0)
         return -1;
      if (set & ILCLIENT_EVENT_ERROR)
         return -2;
      if (set & ILCLIENT_CONFIG_CHANGED)
         return ilclient_remove_event(comp, event, nData1, ignore1, nData2, ignore2) == 0 ? 0 : -3;
   }

   return 0;
}



/***********************************************************
 * Name: ilclient_wait_for_command_complete_dual
 *
 * Description: Waits for an event signalling command completion.  In
 * this version we may also return failure if there is an error event
 * that has terminated a command on a second component.
 *
 * Returns: 0 on success, -1 on failure of comp, -2 on failure of other
 ***********************************************************/
int ilclient_wait_for_command_complete_dual(COMPONENT_T *comp, OMX_COMMANDTYPE command, OMX_U32 nData2, COMPONENT_T *other)
{
   OMX_U32 mask = ILCLIENT_EVENT_ERROR;
   int ret = 0;

   switch(command) {
   case OMX_CommandStateSet:    mask |= ILCLIENT_STATE_CHANGED; break;
   case OMX_CommandPortDisable: mask |= ILCLIENT_PORT_DISABLED; break;
   case OMX_CommandPortEnable:  mask |= ILCLIENT_PORT_ENABLED;  break;
   default: return -1;
   }

   if(other)
      other->related = comp;

   while(1)
   {
      ILEVENT_T *cur, *prev = NULL;
      VCOS_UNSIGNED set;

      ilclient_lock_events(comp->client);

      cur = comp->list;
      while(cur &&
            !(cur->eEvent == OMX_EventCmdComplete && cur->nData1 == command && cur->nData2 == nData2) &&
            !(cur->eEvent == OMX_EventError && cur->nData2 == 1))
      {
         prev = cur;
         cur = cur->next;
      }

      if(cur)
      {
         if(prev == NULL)
            comp->list = cur->next;
         else
            prev->next = cur->next;

         // work out whether this was a success or a fail event
         ret = cur->eEvent == OMX_EventCmdComplete || cur->nData1 == OMX_ErrorSameState ? 0 : -1;

         if(cur->eEvent == OMX_EventError)
            vcos_event_flags_get(&comp->event, ILCLIENT_EVENT_ERROR, VCOS_OR_CONSUME, 0, &set);

         // add back into spare list
         cur->next = comp->client->event_list;
         comp->client->event_list = cur;
         cur->eEvent = -1; // mark as unused
         
         ilclient_unlock_events(comp->client);
         break;
      }
      else if(other != NULL)
      {
         // check the other component for an error event that terminates a command
         cur = other->list;
         while(cur && !(cur->eEvent == OMX_EventError && cur->nData2 == 1))
            cur = cur->next;

         if(cur)
         {
            // we don't remove the event in this case, since the user
            // can confirm that this event errored by calling wait_for_command on the
            // other component

            ret = -2;
            ilclient_unlock_events(comp->client);
            break;
         }
      }

      ilclient_unlock_events(comp->client);

      vcos_event_flags_get(&comp->event, mask, VCOS_OR_CONSUME, VCOS_SUSPEND, &set);
   }

   if(other)
      other->related = NULL;

   return ret;
}


/***********************************************************
 * Name: ilclient_wait_for_command_complete
 *
 * Description: Waits for an event signalling command completion.
 *
 * Returns: 0 on success, -1 on failure.
 ***********************************************************/
int ilclient_wait_for_command_complete(COMPONENT_T *comp, OMX_COMMANDTYPE command, OMX_U32 nData2)
{
   return ilclient_wait_for_command_complete_dual(comp, command, nData2, NULL);
}

/***********************************************************
 * Name: ilclient_get_output_buffer
 *
 * Description: Returns an output buffer returned from a component
 * using the OMX_FillBufferDone callback from the output list for the
 * given component and port index.
 *
 * Returns: pointer to buffer if available, otherwise NULL
 ***********************************************************/
OMX_BUFFERHEADERTYPE *ilclient_get_output_buffer(COMPONENT_T *comp, int portIndex, int block)
{
   OMX_BUFFERHEADERTYPE *ret = NULL, *prev = NULL;
   VCOS_UNSIGNED set;

   do {
      vcos_semaphore_wait(&comp->sema);
      ret = comp->out_list;
      while(ret != NULL && ret->nOutputPortIndex != portIndex)
      {
         prev = ret;
         ret = ret->pAppPrivate;
      }
      
      if(ret)
      {
         if(prev == NULL)
            comp->out_list = ret->pAppPrivate;
         else
            prev->pAppPrivate = ret->pAppPrivate;
         
         ret->pAppPrivate = NULL;
      }
      vcos_semaphore_post(&comp->sema);

      if(block && !ret)
         vcos_event_flags_get(&comp->event, ILCLIENT_FILL_BUFFER_DONE, VCOS_OR_CONSUME, -1, &set);

   } while(block && !ret);

   return ret;
}

/***********************************************************
 * Name: ilclient_get_input_buffer
 *
 * Description: Returns an input buffer return from a component using
 * the OMX_EmptyBufferDone callback from the output list for the given
 * component and port index.
 *
 * Returns: pointer to buffer if available, otherwise NULL
 ***********************************************************/
OMX_BUFFERHEADERTYPE *ilclient_get_input_buffer(COMPONENT_T *comp, int portIndex, int block)
{
   OMX_BUFFERHEADERTYPE *ret = NULL, *prev = NULL;

   do {
      VCOS_UNSIGNED set;

      vcos_semaphore_wait(&comp->sema);
      ret = comp->in_list;
      while(ret != NULL && ret->nInputPortIndex != portIndex)
      {
         prev = ret;
         ret = ret->pAppPrivate;
      }
      
      if(ret)
      {
         if(prev == NULL)
            comp->in_list = ret->pAppPrivate;
         else
            prev->pAppPrivate = ret->pAppPrivate;
         
         ret->pAppPrivate = NULL;
      }
      vcos_semaphore_post(&comp->sema);

      if(block && !ret)
         vcos_event_flags_get(&comp->event, ILCLIENT_EMPTY_BUFFER_DONE, VCOS_OR_CONSUME, -1, &set);

   } while(block && !ret);

   return ret;
}

/***********************************************************
 * Name: ilclient_debug_output
 *
 * Description: prints a varg message to the log or the debug screen
 * under win32
 *
 * Returns: void
 ***********************************************************/
void ilclient_debug_output(char *format, ...)
{
   va_list args;

   va_start(args, format);
   vcos_vlog_info(format, args);
   va_end(args);
}

/******************************************************************************
Static functions
******************************************************************************/

/***********************************************************
 * Name: ilclient_lock_events
 *
 * Description: locks the client event structure
 *
 * Returns: void
 ***********************************************************/
static void ilclient_lock_events(ILCLIENT_T *st)
{
   vcos_semaphore_wait(&st->event_sema);
}

/***********************************************************
 * Name: ilclient_unlock_events
 *
 * Description: unlocks the client event structure
 *
 * Returns: void
 ***********************************************************/
static void ilclient_unlock_events(ILCLIENT_T *st)
{
   vcos_semaphore_post(&st->event_sema);
}

/***********************************************************
 * Name: ilclient_event_handler
 *
 * Description: event handler passed to core to use as component
 * callback
 *
 * Returns: success
 ***********************************************************/
static OMX_ERRORTYPE ilclient_event_handler(OMX_IN OMX_HANDLETYPE hComponent,
                                            OMX_IN OMX_PTR pAppData,
                                            OMX_IN OMX_EVENTTYPE eEvent,
                                            OMX_IN OMX_U32 nData1,
                                            OMX_IN OMX_U32 nData2,
                                            OMX_IN OMX_PTR pEventData)
{
   COMPONENT_T *st = (COMPONENT_T *) pAppData;
   ILEVENT_T *event;
   OMX_ERRORTYPE error = OMX_ErrorNone;

   ilclient_lock_events(st->client);

   // go through the events on this component and remove any duplicates in the
   // existing list, since the client probably doesn't need them.  it's better
   // than asserting when we run out.
   event = st->list;
   while(event != NULL)
   {
      ILEVENT_T **list = &(event->next);
      while(*list != NULL)
      {
         if((*list)->eEvent == event->eEvent &&
            (*list)->nData1 == event->nData1 &&
            (*list)->nData2 == event->nData2)
         {
            // remove this duplicate
            ILEVENT_T *rem = *list;
            ilclient_debug_output("%s: removing %d/%d/%d", st->name, event->eEvent, event->nData1, event->nData2);            
            *list = rem->next;
            rem->eEvent = -1;
            rem->next = st->client->event_list;
            st->client->event_list = rem;
         }
         else
            list = &((*list)->next);
      }

      event = event->next;
   }

   vc_assert(st->client->event_list);
   event = st->client->event_list;

   switch (eEvent) {
   case OMX_EventCmdComplete:
      switch (nData1) {
      case OMX_CommandStateSet:
         ilclient_debug_output("%s: callback state changed (%s)", st->name, states[nData2]);
         vcos_event_flags_set(&st->event, ILCLIENT_STATE_CHANGED, VCOS_OR);
         break;
      case OMX_CommandPortDisable:
         ilclient_debug_output("%s: callback port disable %d", st->name, nData2);
         vcos_event_flags_set(&st->event, ILCLIENT_PORT_DISABLED, VCOS_OR);
         break;
      case OMX_CommandPortEnable:
         ilclient_debug_output("%s: callback port enable %d", st->name, nData2);
         vcos_event_flags_set(&st->event, ILCLIENT_PORT_ENABLED, VCOS_OR);
         break;
      case OMX_CommandFlush:
         ilclient_debug_output("%s: callback port flush %d", st->name, nData2);
         vcos_event_flags_set(&st->event, ILCLIENT_PORT_FLUSH, VCOS_OR);
         break;
      case OMX_CommandMarkBuffer:
         ilclient_debug_output("%s: callback mark buffer %d", st->name, nData2);
         vcos_event_flags_set(&st->event, ILCLIENT_MARKED_BUFFER, VCOS_OR);
         break;
      default:
         vc_assert(0);
      }
      break;
   case OMX_EventError:
      {
         // check if this component failed a command, and we have to notify another command
         // of this failure
         if(nData2 == 1 && st->related != NULL)
            vcos_event_flags_set(&st->related->event, ILCLIENT_EVENT_ERROR, VCOS_OR);

         error = nData1;
         switch (error) {
         case OMX_ErrorPortUnpopulated:
            if (st->error_mask & ILCLIENT_ERROR_UNPOPULATED)
            {
               ilclient_debug_output("%s: ignore error: port unpopulated (%d)", st->name, nData2);
               event = NULL;
               break;
            }
            ilclient_debug_output("%s: port unpopulated %x (%d)", st->name, error, nData2);
            vcos_event_flags_set(&st->event, ILCLIENT_EVENT_ERROR, VCOS_OR);
            break;
         case OMX_ErrorSameState:
            if (st->error_mask & ILCLIENT_ERROR_SAMESTATE)
            {
               ilclient_debug_output("%s: ignore error: same state (%d)", st->name, nData2);
               event = NULL;
               break;
            }
            ilclient_debug_output("%s: same state %x (%d)", st->name, error, nData2);
            vcos_event_flags_set(&st->event, ILCLIENT_EVENT_ERROR, VCOS_OR);
            break;
         case OMX_ErrorBadParameter:
            if (st->error_mask & ILCLIENT_ERROR_BADPARAMETER)
            {
               ilclient_debug_output("%s: ignore error: bad parameter (%d)", st->name, nData2);
               event = NULL;
               break;
            }
            ilclient_debug_output("%s: bad parameter %x (%d)", st->name, error, nData2);
            vcos_event_flags_set(&st->event, ILCLIENT_EVENT_ERROR, VCOS_OR);
            break;
         case OMX_ErrorIncorrectStateTransition:
            ilclient_debug_output("%s: incorrect state transition %x (%d)", st->name, error, nData2);
            vcos_event_flags_set(&st->event, ILCLIENT_EVENT_ERROR, VCOS_OR);
            break;
         case OMX_ErrorBadPortIndex:
            ilclient_debug_output("%s: bad port index %x (%d)", st->name, error, nData2);
            vcos_event_flags_set(&st->event, ILCLIENT_EVENT_ERROR, VCOS_OR);
            break;
         case OMX_ErrorStreamCorrupt:
            ilclient_debug_output("%s: stream corrupt %x (%d)", st->name, error, nData2);
            vcos_event_flags_set(&st->event, ILCLIENT_EVENT_ERROR, VCOS_OR);
            break;
         case OMX_ErrorInsufficientResources:
            ilclient_debug_output("%s: insufficient resources %x (%d)", st->name, error, nData2);
            vcos_event_flags_set(&st->event, ILCLIENT_EVENT_ERROR, VCOS_OR);
            break;
         case OMX_ErrorUnsupportedSetting:
            ilclient_debug_output("%s: unsupported setting %x (%d)", st->name, error, nData2);
            vcos_event_flags_set(&st->event, ILCLIENT_EVENT_ERROR, VCOS_OR);
            break;
         case OMX_ErrorOverflow:
            ilclient_debug_output("%s: overflow %x (%d)", st->name, error, nData2);
            vcos_event_flags_set(&st->event, ILCLIENT_EVENT_ERROR, VCOS_OR);
            break;
         case OMX_ErrorDiskFull:
            ilclient_debug_output("%s: disk full %x (%d)", st->name, error, nData2);
            //we do not set the error
            break;
         case OMX_ErrorMaxFileSize:
            ilclient_debug_output("%s: max file size %x (%d)", st->name, error, nData2);
            //we do not set the error
            break;
         case OMX_ErrorDrmUnauthorised:
            ilclient_debug_output("%s: drm file is unauthorised %x (%d)", st->name, error, nData2);
            vcos_event_flags_set(&st->event, ILCLIENT_EVENT_ERROR, VCOS_OR);
            break;
         case OMX_ErrorDrmExpired:
            ilclient_debug_output("%s: drm file has expired %x (%d)", st->name, error, nData2);
            vcos_event_flags_set(&st->event, ILCLIENT_EVENT_ERROR, VCOS_OR);
            break;
         case OMX_ErrorDrmGeneral:
            ilclient_debug_output("%s: drm library error %x (%d)", st->name, error, nData2);
            vcos_event_flags_set(&st->event, ILCLIENT_EVENT_ERROR, VCOS_OR);
            break;
         default:
            vc_assert(0);
            ilclient_debug_output("%s: unexpected error %x (%d)", st->name, error, nData2);
            vcos_event_flags_set(&st->event, ILCLIENT_EVENT_ERROR, VCOS_OR);
            break;
         }
      }
      break;
   case OMX_EventBufferFlag:
      ilclient_debug_output("%s: buffer flag %d/%x", st->name, nData1, nData2);
      if (nData2 & OMX_BUFFERFLAG_EOS)
      {
         vcos_event_flags_set(&st->event, ILCLIENT_BUFFER_FLAG_EOS, VCOS_OR);
         nData2 = OMX_BUFFERFLAG_EOS;
      }
      else
         vc_assert(0);
      break;
   case OMX_EventPortSettingsChanged:
      ilclient_debug_output("%s: port settings changed %d", st->name, nData1);
      vcos_event_flags_set(&st->event, ILCLIENT_PARAMETER_CHANGED, VCOS_OR);
      break;
   case OMX_EventMark:
      ilclient_debug_output("%s: buffer mark %p", st->name, pEventData);
      vcos_event_flags_set(&st->event, ILCLIENT_BUFFER_MARK, VCOS_OR);
      break;
   case OMX_EventParamOrConfigChanged:
      ilclient_debug_output("%s: param/config 0x%X on port %d changed", st->name, nData2, nData1);
      vcos_event_flags_set(&st->event, ILCLIENT_CONFIG_CHANGED, VCOS_OR);
      break;
   default:
      vc_assert(0);
      break;
   }

   if (event)
   {
      // fill in details
      event->eEvent = eEvent;
      event->nData1 = nData1;
      event->nData2 = nData2;
      event->pEventData = pEventData;

      // remove from top of spare list
      st->client->event_list = st->client->event_list->next;

      // put at head of component event queue
      event->next = st->list;
      st->list = event;
   }
   ilclient_unlock_events(st->client);

   // now call any callbacks without the event lock so the client can 
   // remove the event in context
   switch(eEvent) {
   case OMX_EventError:
      if(event && st->client->error_callback)
         st->client->error_callback(st->client->error_callback_data, st, error);
      break;
   case OMX_EventBufferFlag:
      if ((nData2 & OMX_BUFFERFLAG_EOS) && st->client->eos_callback)
         st->client->eos_callback(st->client->eos_callback_data, st, nData1);
      break;
   case OMX_EventPortSettingsChanged:
      if (st->client->port_settings_callback)
         st->client->port_settings_callback(st->client->port_settings_callback_data, st, nData1);
      break;
   case OMX_EventParamOrConfigChanged:
      if (st->client->configchanged_callback)
         st->client->configchanged_callback(st->client->configchanged_callback_data, st, nData2);
      break;
   default:
      // ignore
      break;
   }

   return OMX_ErrorNone;
}

/***********************************************************
 * Name: ilclient_empty_buffer_done
 *
 * Description: passed to core to use as component callback, puts
 * buffer on list
 *
 * Returns:
 ***********************************************************/
static OMX_ERRORTYPE ilclient_empty_buffer_done(OMX_IN OMX_HANDLETYPE hComponent,
      OMX_IN OMX_PTR pAppData,
      OMX_IN OMX_BUFFERHEADERTYPE* pBuffer)
{
   COMPONENT_T *st = (COMPONENT_T *) pAppData;
   OMX_BUFFERHEADERTYPE *list;

   ilclient_debug_output("%s: empty buffer done %p", st->name, pBuffer);

   vcos_semaphore_wait(&st->sema);
   // insert at end of the list, so we process buffers in
   // the same order
   list = st->in_list;
   while(list && list->pAppPrivate)
      list = list->pAppPrivate;

   if(!list)
      st->in_list = pBuffer;
   else
      list->pAppPrivate = pBuffer;

   pBuffer->pAppPrivate = NULL;
   vcos_semaphore_post(&st->sema);

   vcos_event_flags_set(&st->event, ILCLIENT_EMPTY_BUFFER_DONE, VCOS_OR);

   if (st->client->empty_buffer_done_callback)
      st->client->empty_buffer_done_callback(st->client->empty_buffer_done_callback_data, st);

   return OMX_ErrorNone;
}

/***********************************************************
 * Name: ilclient_empty_buffer_done_error
 *
 * Description: passed to core to use as component callback, asserts
 * on use as client not expecting component to use this callback.
 *
 * Returns:
 ***********************************************************/
static OMX_ERRORTYPE ilclient_empty_buffer_done_error(OMX_IN OMX_HANDLETYPE hComponent,
      OMX_IN OMX_PTR pAppData,
      OMX_IN OMX_BUFFERHEADERTYPE* pBuffer)
{
   vc_assert(0);
   return OMX_ErrorNone;
}

/***********************************************************
 * Name: ilclient_fill_buffer_done
 *
 * Description: passed to core to use as component callback, puts
 * buffer on list
 *
 * Returns:
 ***********************************************************/
static OMX_ERRORTYPE ilclient_fill_buffer_done(OMX_OUT OMX_HANDLETYPE hComponent,
      OMX_OUT OMX_PTR pAppData,
      OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
   COMPONENT_T *st = (COMPONENT_T *) pAppData;
   OMX_BUFFERHEADERTYPE *list;

   ilclient_debug_output("%s: fill buffer done %p", st->name, pBuffer);

   vcos_semaphore_wait(&st->sema);
   // insert at end of the list, so we process buffers in
   // the correct order
   list = st->out_list;
   while(list && list->pAppPrivate)
      list = list->pAppPrivate;

   if(!list)
      st->out_list = pBuffer;
   else
      list->pAppPrivate = pBuffer;
      
   pBuffer->pAppPrivate = NULL;
   vcos_semaphore_post(&st->sema);

   vcos_event_flags_set(&st->event, ILCLIENT_FILL_BUFFER_DONE, VCOS_OR);

   if (st->client->fill_buffer_done_callback)
      st->client->fill_buffer_done_callback(st->client->fill_buffer_done_callback_data, st);

   return OMX_ErrorNone;
}

/***********************************************************
 * Name: ilclient_fill_buffer_done_error
 *
 * Description: passed to core to use as component callback, asserts
 * on use as client not expecting component to use this callback.
 *
 * Returns:
 ***********************************************************/
static OMX_ERRORTYPE ilclient_fill_buffer_done_error(OMX_OUT OMX_HANDLETYPE hComponent,
      OMX_OUT OMX_PTR pAppData,
      OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
   vc_assert(0);
   return OMX_ErrorNone;
}



OMX_HANDLETYPE ilclient_get_handle(COMPONENT_T *comp)
{
   vcos_assert(comp);
   return comp->comp;
}


static struct {
   OMX_PORTDOMAINTYPE dom;
   int param;
} port_types[] = {
   { OMX_PortDomainVideo, OMX_IndexParamVideoInit },
   { OMX_PortDomainAudio, OMX_IndexParamAudioInit },
   { OMX_PortDomainImage, OMX_IndexParamImageInit },
   { OMX_PortDomainOther, OMX_IndexParamOtherInit },
};

int ilclient_get_port_index(COMPONENT_T *comp, OMX_DIRTYPE dir, OMX_PORTDOMAINTYPE type, int index)
{
   uint32_t i;
   // for each possible port type...
   for (i=0; i<sizeof(port_types)/sizeof(port_types[0]); i++)
   {
      if ((port_types[i].dom == type) || (type == (OMX_PORTDOMAINTYPE) -1))
      {
         OMX_PORT_PARAM_TYPE param;
         OMX_ERRORTYPE error;
         uint32_t j;

         param.nSize = sizeof(param);
         param.nVersion.nVersion = OMX_VERSION;
         error = OMX_GetParameter(ILC_GET_HANDLE(comp), port_types[i].param, &param);
         assert(error == OMX_ErrorNone);

         // for each port of this type...
         for (j=0; j<param.nPorts; j++)
         {
            int port = param.nStartPortNumber+j;

            OMX_PARAM_PORTDEFINITIONTYPE portdef;
            portdef.nSize = sizeof(portdef);
            portdef.nVersion.nVersion = OMX_VERSION;
            portdef.nPortIndex = port;

            error = OMX_GetParameter(ILC_GET_HANDLE(comp), OMX_IndexParamPortDefinition, &portdef);
            assert(error == OMX_ErrorNone);

            if (portdef.eDir == dir)
            {
               if (index-- == 0)
                  return port;
            }
         }
      }
   }
   return -1;
}

int ilclient_suggest_bufsize(COMPONENT_T *comp, OMX_U32 nBufSizeHint)
{
   OMX_PARAM_BRCMOUTPUTBUFFERSIZETYPE param;
   OMX_ERRORTYPE error;

   param.nSize = sizeof(param);
   param.nVersion.nVersion = OMX_VERSION;
   param.nBufferSize = nBufSizeHint;
   error = OMX_SetParameter(ILC_GET_HANDLE(comp), OMX_IndexParamBrcmOutputBufferSize,
                            &param);
   assert(error == OMX_ErrorNone);

   return (error == OMX_ErrorNone) ? 0 : -1;
}

unsigned int ilclient_stack_size(void)
{
   return ILCLIENT_THREAD_DEFAULT_STACK_SIZE;
}

