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

#include "mmalomx.h"
#include "mmalomx_commands.h"
#include "mmalomx_buffer.h"
#include "mmalomx_logging.h"

typedef struct {
   OMX_STATETYPE state;
   OMX_STATETYPE request;
   uint32_t actions;
} MMALOMX_STATE_TRANSITION_T;

MMALOMX_STATE_TRANSITION_T state_transition_table[] =
{
  {OMX_StateInvalid, OMX_StateInvalid, 0},
  {OMX_StateLoaded, OMX_StateIdle, MMALOMX_ACTION_CHECK_ALLOCATED|MMALOMX_ACTION_ENABLE},
  {OMX_StateLoaded, OMX_StateWaitForResources, 0},
  {OMX_StateWaitForResources, OMX_StateLoaded, 0},
  {OMX_StateWaitForResources, OMX_StateIdle, MMALOMX_ACTION_CHECK_ALLOCATED|MMALOMX_ACTION_ENABLE},
  {OMX_StateIdle, OMX_StateLoaded, MMALOMX_ACTION_CHECK_DEALLOCATED|MMALOMX_ACTION_DISABLE},
  {OMX_StateIdle, OMX_StateExecuting, 0},
  {OMX_StateIdle, OMX_StatePause, 0},
  {OMX_StateExecuting, OMX_StateIdle, MMALOMX_ACTION_FLUSH|MMALOMX_ACTION_CHECK_FLUSHED},
  {OMX_StateExecuting, OMX_StatePause, 0},
  {OMX_StatePause, OMX_StateIdle, 0},
  {OMX_StatePause, OMX_StateExecuting, 0},
  {OMX_StateMax, OMX_StateMax, 0}
};

/*****************************************************************************/
static unsigned int mmalomx_state_transition_get(OMX_STATETYPE state, OMX_STATETYPE request)
{
   unsigned int i;

   for (i = 0; state_transition_table[i].state != OMX_StateMax; i++)
      if (state_transition_table[i].state == state &&
          state_transition_table[i].request == request)
         break;

   return state_transition_table[i].state != OMX_StateMax ? i : 0;
}

/*****************************************************************************/
static void mmalomx_buffer_cb_io(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   mmalomx_buffer_return((MMALOMX_PORT_T *)port->userdata, buffer);
}

/*****************************************************************************/
static void mmalomx_commands_check_port_actions(MMALOMX_COMPONENT_T *component,
   MMALOMX_PORT_T *port)
{
   uint32_t exec_actions = 0;
   MMAL_STATUS_T status;

   MMALOMX_LOCK_PORT(component, port);
   if (!port->actions)
   {
      MMALOMX_UNLOCK_PORT(component, port);
      return;
   }

   if (port->actions & MMALOMX_ACTION_FLUSH)
   {
      port->actions &= ~MMALOMX_ACTION_FLUSH;
      port->actions |= MMALOMX_ACTION_PENDING_FLUSH;
      exec_actions |= MMALOMX_ACTION_PENDING_FLUSH;
   }
   if ((port->actions & MMALOMX_ACTION_DISABLE) &&
       (!port->buffers_in_transit ||
        !(port->actions & MMALOMX_ACTION_CHECK_FLUSHED)))
   {
      port->actions &= ~MMALOMX_ACTION_DISABLE;
      port->actions |= MMALOMX_ACTION_PENDING_DISABLE;
      exec_actions |= MMALOMX_ACTION_PENDING_DISABLE;
   }
   if ((port->actions & MMALOMX_ACTION_ENABLE) &&
       port->buffers)
   {
      /* We defer enabling the mmal port until the first buffer allocation
       * has been done. Only at that point do we know for sure whether we
       * are going to use shared memory or not.
       * We might want to delay it to just before sending the event to the client ???
       */
      port->actions &= ~MMALOMX_ACTION_ENABLE;
      port->actions |= MMALOMX_ACTION_PENDING_ENABLE;
      exec_actions |= MMALOMX_ACTION_PENDING_ENABLE;
   }
   MMALOMX_UNLOCK_PORT(component, port);

   if (exec_actions & MMALOMX_ACTION_PENDING_FLUSH)
      mmal_port_flush(port->mmal);

   if (exec_actions & MMALOMX_ACTION_PENDING_DISABLE)
   {
      mmal_port_disable(port->mmal);

      /* If there was a port format changed event, we need to make sure
       * the new format has been committed */
      if (port->format_changed)
      {
         status = mmal_port_format_commit(port->mmal);
         if (status != MMAL_SUCCESS)
            LOG_WARN("could not commit new format (%i)", status);
         port->format_changed = MMAL_FALSE;
      }
   }

   if (exec_actions & MMALOMX_ACTION_PENDING_ENABLE)
   {
      status = mmal_port_enable(port->mmal, mmalomx_buffer_cb_io);
      if (status == MMAL_SUCCESS)
         status = mmal_pool_resize(port->pool, port->mmal->buffer_num, 0);
      if (status != MMAL_SUCCESS)
         mmalomx_callback_event_handler(component, OMX_EventError, mmalil_error_to_omx(status), 0, NULL);
      /* FIXME: we're still going to generate a cmd complete. Not sure if that's an issue. */
   }

   MMALOMX_LOCK_PORT(component, port);

   port->actions &= ~exec_actions;
   if ((port->actions & MMALOMX_ACTION_CHECK_ALLOCATED) && port->populated)
      port->actions &= ~MMALOMX_ACTION_CHECK_ALLOCATED;
   if ((port->actions & MMALOMX_ACTION_CHECK_DEALLOCATED) && !port->buffers)
      port->actions &= ~MMALOMX_ACTION_CHECK_DEALLOCATED;
   if ((port->actions & MMALOMX_ACTION_CHECK_FLUSHED) && !port->buffers_in_transit)
      port->actions &= ~MMALOMX_ACTION_CHECK_FLUSHED;
   exec_actions = port->actions;

   if (port->actions == MMALOMX_ACTION_NOTIFY_FLUSH ||
       port->actions == MMALOMX_ACTION_NOTIFY_ENABLE ||
       port->actions == MMALOMX_ACTION_NOTIFY_DISABLE)
      port->actions = 0;  /* We're done */

   MMALOMX_UNLOCK_PORT(component, port);

   if (exec_actions == MMALOMX_ACTION_NOTIFY_FLUSH)
      mmalomx_callback_event_handler(component, OMX_EventCmdComplete,
         OMX_CommandFlush, port->index, NULL);
   else if (exec_actions == MMALOMX_ACTION_NOTIFY_ENABLE)
      mmalomx_callback_event_handler(component, OMX_EventCmdComplete,
         OMX_CommandPortEnable, port->index, NULL);
   else if (exec_actions == MMALOMX_ACTION_NOTIFY_DISABLE)
      mmalomx_callback_event_handler(component, OMX_EventCmdComplete,
         OMX_CommandPortDisable, port->index, NULL);
}

/*****************************************************************************/
void mmalomx_commands_actions_check(MMALOMX_COMPONENT_T *component)
{
   uint32_t actions_left = 0;
   unsigned int i;

   for (i = 0; i < component->ports_num; i++)
      mmalomx_commands_check_port_actions(component, &component->ports[i]);

   MMALOMX_LOCK(component);
   for (i = 0; i < component->ports_num; i++)
      actions_left |= component->ports[i].actions;

   if (!actions_left && component->state_transition)
   {
      component->state = state_transition_table[component->state_transition].request;
      component->state_transition = 0;
      actions_left = MMALOMX_ACTION_NOTIFY_STATE;
   }
   MMALOMX_UNLOCK(component);

   if (actions_left == MMALOMX_ACTION_NOTIFY_STATE)
   {
      mmalomx_callback_event_handler(component, OMX_EventCmdComplete,
         OMX_CommandStateSet, component->state, NULL);
      actions_left = 0;
   }

   /* If we're not currently processing a command, we can start processing
    * the next one. */
   if (!actions_left)
      mmalomx_commands_actions_next(component);
}

/*****************************************************************************/
void mmalomx_commands_actions_signal(MMALOMX_COMPONENT_T *component)
{
   if (component->cmd_thread_used)
      vcos_semaphore_post(&component->cmd_sema);
   else
      mmalomx_commands_actions_check(component);
}

/*****************************************************************************/
OMX_ERRORTYPE mmalomx_command_state_set(
   OMX_HANDLETYPE hComponent,
   OMX_STATETYPE state)
{
   MMALOMX_COMPONENT_T *component = (MMALOMX_COMPONENT_T *)hComponent;
   unsigned int i, transition;

   if (component->state == state)
   {
      mmalomx_callback_event_handler(component, OMX_EventError, OMX_ErrorSameState, 0, NULL);
      return OMX_ErrorNone;
   }

   /* We're asked to transition to StateInvalid */
   if (state == OMX_StateInvalid)
   {
      component->state = state;
      mmalomx_callback_event_handler(component, OMX_EventError, OMX_ErrorInvalidState, 0, NULL);
      return OMX_ErrorNone;
   }

   /* Commands are being queued so we should never get into that state */
   vcos_assert(!component->state_transition);

   /* Check the transition is valid */
   transition = mmalomx_state_transition_get(component->state, state);
   if (!transition)
   {
      mmalomx_callback_event_handler(component, OMX_EventError, OMX_ErrorIncorrectStateTransition, 0, NULL);
      return OMX_ErrorNone;
   }

   /* Special case for transition in and out of Executing */
   if (state == OMX_StateExecuting || component->state == OMX_StateExecuting)
   {
      MMAL_STATUS_T status;

      if (state == OMX_StateExecuting)
         status = mmal_component_enable(component->mmal);
      else
         status = mmal_component_disable(component->mmal);

      if (status != MMAL_SUCCESS)
      {
         LOG_ERROR("could not %s %s", state == OMX_StateExecuting ? "enable" : "disable", component->name);
         mmalomx_callback_event_handler(component, OMX_EventError, mmalil_error_to_omx(status), 0, NULL);
         return OMX_ErrorNone;
      }
   }

   MMALOMX_LOCK(component);
   component->state_transition = transition;

   for (i = 0; i < component->ports_num; i++)
   {
      if (!component->ports[i].enabled)
         continue;

      MMALOMX_LOCK_PORT(component, component->ports + i);
      component->ports[i].actions = state_transition_table[transition].actions;

      /* If we're transitionning from Idle to Loaded we'd rather do a flush first
       * to avoid the cmd thread to block for too long (mmal_disable is a
       * blocking call). */
      if (state_transition_table[transition].state == OMX_StateIdle &&
          state_transition_table[transition].request == OMX_StateLoaded &&
          component->cmd_thread_used)
         component->ports[i].actions |= MMALOMX_ACTION_FLUSH|MMALOMX_ACTION_CHECK_FLUSHED;
      MMALOMX_UNLOCK_PORT(component, component->ports + i);
   }
   MMALOMX_UNLOCK(component);

   mmalomx_commands_actions_check(component);
   return OMX_ErrorNone;
}

/*****************************************************************************/
OMX_ERRORTYPE mmalomx_command_port_mark(
   OMX_HANDLETYPE hComponent,
   OMX_U32 nPortIndex,
   OMX_PTR *pCmdData)
{
   MMALOMX_COMPONENT_T *component = (MMALOMX_COMPONENT_T *)hComponent;
   OMX_MARKTYPE *mark = (OMX_MARKTYPE *)pCmdData;
   MMALOMX_PORT_T *port;

   if (nPortIndex >= component->ports_num)
      return OMX_ErrorBadPortIndex;
   port = &component->ports[nPortIndex];

   if (port->marks_num == MAX_MARKS_NUM)
      return OMX_ErrorInsufficientResources;

   port->marks[(port->marks_first + port->marks_num) % MAX_MARKS_NUM] = *mark;
   port->marks_num++;

   return OMX_ErrorNone;
}

/*****************************************************************************/
OMX_ERRORTYPE mmalomx_command_port_flush(
   OMX_HANDLETYPE hComponent,
   OMX_U32 nPortIndex)
{
   MMALOMX_COMPONENT_T *component = (MMALOMX_COMPONENT_T *)hComponent;

   MMALOMX_LOCK_PORT(component, &component->ports[nPortIndex]);
   component->ports[nPortIndex].actions =
      MMALOMX_ACTION_FLUSH|MMALOMX_ACTION_CHECK_FLUSHED|MMALOMX_ACTION_NOTIFY_FLUSH;
   MMALOMX_UNLOCK_PORT(component, &component->ports[nPortIndex]);

   mmalomx_commands_actions_check(component);

   return OMX_ErrorNone;
}

/*****************************************************************************/
OMX_ERRORTYPE mmalomx_command_port_enable(
   OMX_HANDLETYPE hComponent,
   OMX_U32 nPortIndex)
{
   MMALOMX_COMPONENT_T *component = (MMALOMX_COMPONENT_T *)hComponent;
   component->ports[nPortIndex].enabled = MMAL_TRUE;

   if (component->state == OMX_StateLoaded ||
       component->state == OMX_StateWaitForResources)
   {
      mmalomx_callback_event_handler(component, OMX_EventCmdComplete, OMX_CommandPortEnable, nPortIndex, NULL);
      return OMX_ErrorNone;
   }

   MMALOMX_LOCK_PORT(component, &component->ports[nPortIndex]);
   component->ports[nPortIndex].actions =
      MMALOMX_ACTION_CHECK_ALLOCATED|MMALOMX_ACTION_ENABLE|MMALOMX_ACTION_NOTIFY_ENABLE;
   MMALOMX_UNLOCK_PORT(component, &component->ports[nPortIndex]);

   mmalomx_commands_actions_check(component);

   return OMX_ErrorNone;
}

/*****************************************************************************/
OMX_ERRORTYPE mmalomx_command_port_disable(
   OMX_HANDLETYPE hComponent,
   OMX_U32 nPortIndex)
{
   MMALOMX_COMPONENT_T *component = (MMALOMX_COMPONENT_T *)hComponent;
   component->ports[nPortIndex].enabled = MMAL_FALSE;

   if (component->state == OMX_StateLoaded ||
       component->state == OMX_StateWaitForResources)
   {
      mmalomx_callback_event_handler(component, OMX_EventCmdComplete, OMX_CommandPortDisable, nPortIndex, NULL);
      return OMX_ErrorNone;
   }

   MMALOMX_LOCK_PORT(component, &component->ports[nPortIndex]);
   component->ports[nPortIndex].actions =
      MMALOMX_ACTION_DISABLE|MMALOMX_ACTION_CHECK_DEALLOCATED|MMALOMX_ACTION_NOTIFY_DISABLE;
   if (component->cmd_thread_used)
      component->ports[nPortIndex].actions |=
         MMALOMX_ACTION_FLUSH|MMALOMX_ACTION_CHECK_FLUSHED;
   MMALOMX_UNLOCK_PORT(component, &component->ports[nPortIndex]);

   mmalomx_commands_actions_check(component);

   return OMX_ErrorNone;
}

/*****************************************************************************/
OMX_ERRORTYPE mmalomx_command_queue(
   MMALOMX_COMPONENT_T *component, 
   OMX_U32 arg1, OMX_U32 arg2)
{
   MMAL_BUFFER_HEADER_T *cmd = mmal_queue_get(component->cmd_pool->queue);

   if (!vcos_verify(cmd))
   {
      LOG_ERROR("command queue too small");
      return OMX_ErrorInsufficientResources;
   }

   cmd->cmd = arg1;
   cmd->offset = arg2;
   mmal_queue_put(component->cmd_queue, cmd);

   mmalomx_commands_actions_signal(component);

   return OMX_ErrorNone;
}

/*****************************************************************************/
OMX_ERRORTYPE mmalomx_command_dequeue(
   MMALOMX_COMPONENT_T *component,
   OMX_U32 *arg1, OMX_U32 *arg2)
{
   MMAL_BUFFER_HEADER_T *cmd = mmal_queue_get(component->cmd_queue);
   if (!cmd)
      return OMX_ErrorNoMore;

   *arg1 = cmd->cmd;
   *arg2 = cmd->offset;
   mmal_buffer_header_release(cmd);
   return OMX_ErrorNone;
}

/*****************************************************************************/
void mmalomx_commands_actions_next(MMALOMX_COMPONENT_T *component)
{
   OMX_ERRORTYPE status = OMX_ErrorNone;
   OMX_COMMANDTYPE cmd;
   OMX_U32 arg1, arg2, nParam1;
   unsigned int i;

   status = mmalomx_command_dequeue(component, &arg1, &arg2);
   if (status != OMX_ErrorNone)
      return;

   cmd = (OMX_COMMANDTYPE)arg1;
   nParam1 = arg2;

   if (cmd == OMX_CommandStateSet)
   {
      mmalomx_command_state_set((OMX_HANDLETYPE)&component->omx, nParam1);
   }
   else if (cmd == OMX_CommandFlush)
   {
      for (i = 0; i < component->ports_num; i++)
         if (i == nParam1 || nParam1 == OMX_ALL)
            mmalomx_command_port_flush((OMX_HANDLETYPE)&component->omx, i);
   }
   else if (cmd == OMX_CommandPortEnable)
   {
      for (i = 0; i < component->ports_num; i++)
         if (i == nParam1 || nParam1 == OMX_ALL)
            mmalomx_command_port_enable((OMX_HANDLETYPE)&component->omx, i);
   }
   else if (cmd == OMX_CommandPortDisable)
   {
      for (i = 0; i < component->ports_num; i++)
         if (i == nParam1 || nParam1 == OMX_ALL)
            mmalomx_command_port_disable((OMX_HANDLETYPE)&component->omx, i);
   }
}

