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

/** \file
 * OpenMAX IL adaptation layer for MMAL - Commands related functions
 */

OMX_ERRORTYPE mmalomx_command_state_set(
   OMX_HANDLETYPE hComponent,
   OMX_STATETYPE state);

OMX_ERRORTYPE mmalomx_command_port_mark(
   OMX_HANDLETYPE hComponent,
   OMX_U32 nPortIndex,
   OMX_PTR *pCmdData);

OMX_ERRORTYPE mmalomx_command_port_flush(
   OMX_HANDLETYPE hComponent,
   OMX_U32 nPortIndex);

OMX_ERRORTYPE mmalomx_command_port_enable(
   OMX_HANDLETYPE hComponent,
   OMX_U32 nPortIndex);

OMX_ERRORTYPE mmalomx_command_port_disable(
   OMX_HANDLETYPE hComponent,
   OMX_U32 nPortIndex);

#define MMALOMX_ACTION_ENABLE                0x01
#define MMALOMX_ACTION_DISABLE               0x02
#define MMALOMX_ACTION_FLUSH                 0x04

#define MMALOMX_ACTION_PENDING_ENABLE        0x010
#define MMALOMX_ACTION_PENDING_DISABLE       0x020
#define MMALOMX_ACTION_PENDING_FLUSH         0x040

#define MMALOMX_ACTION_CHECK_ALLOCATED       0x0100
#define MMALOMX_ACTION_CHECK_DEALLOCATED     0x0200
#define MMALOMX_ACTION_CHECK_FLUSHED         0x0400

#define MMALOMX_ACTION_NOTIFY_DISABLE        0x1000
#define MMALOMX_ACTION_NOTIFY_ENABLE         0x2000
#define MMALOMX_ACTION_NOTIFY_FLUSH          0x4000
#define MMALOMX_ACTION_NOTIFY_STATE          0x8000

#define MMALOMX_COMMAND_EXIT            0
#define MMALOMX_COMMAND_STATE_SET       1
#define MMALOMX_COMMAND_PORT_MARK       2
#define MMALOMX_COMMAND_PORT_FLUSH      3
#define MMALOMX_COMMAND_PORT_ENABLE     4
#define MMALOMX_COMMAND_PORT_DISABLE    5

OMX_ERRORTYPE mmalomx_command_queue(
   MMALOMX_COMPONENT_T *component, OMX_U32 arg1, OMX_U32 arg2);
OMX_ERRORTYPE mmalomx_command_dequeue(
   MMALOMX_COMPONENT_T *component,  OMX_U32 *arg1, OMX_U32 *arg2);

void mmalomx_commands_actions_check(MMALOMX_COMPONENT_T *component);
void mmalomx_commands_actions_signal(MMALOMX_COMPONENT_T *component);
void mmalomx_commands_actions_next(MMALOMX_COMPONENT_T *component);
