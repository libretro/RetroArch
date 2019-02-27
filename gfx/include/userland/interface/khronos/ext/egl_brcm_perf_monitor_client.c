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

#define EGL_EGLEXT_PROTOTYPES /* we want the prototypes so the compiler will check that the signatures match */

#include "interface/khronos/common/khrn_int_common.h"

#include "interface/khronos/common/khrn_client.h"
#include "interface/khronos/common/khrn_client_rpc.h"

#include "interface/khronos/egl/egl_client_config.h"

#include "interface/khronos/ext/egl_brcm_perf_monitor_client.h"

#include "interface/khronos/include/EGL/egl.h"
#include "interface/khronos/include/EGL/eglext.h"

#if EGL_BRCM_perf_monitor

EGLAPI EGLBoolean EGLAPIENTRY eglInitPerfMonitorBRCM(EGLDisplay dpy)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   EGLBoolean result;

   CLIENT_LOCK();

   {
      CLIENT_PROCESS_STATE_T *process = client_egl_get_process_state(thread, dpy, EGL_TRUE);

      if (process) {
         if (!process->perf_monitor_inited)
            process->perf_monitor_inited = RPC_BOOLEAN_RES(RPC_CALL0_RES(eglInitPerfMonitorBRCM_impl,
                                                         EGLINITPERFMONITORBRCM_ID));

         if (process->perf_monitor_inited) {
            thread->error = EGL_SUCCESS;
            result = EGL_TRUE;
         } else {
            thread->error = EGL_BAD_ALLOC;
            result = EGL_FALSE;
         }
      } else
         result = EGL_FALSE;
   }

   CLIENT_UNLOCK();

   return result;
}

void egl_perf_monitor_term(CLIENT_PROCESS_STATE_T *process)
{
   if (process->perf_monitor_inited) {
      RPC_CALL0(eglTermPerfMonitorBRCM_impl,
                EGLTERMPERFMONITORBRCM_ID);

      process->perf_monitor_inited = false;
   }
}

EGLAPI EGLBoolean EGLAPIENTRY eglTermPerfMonitorBRCM(EGLDisplay dpy)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   EGLBoolean result;

   CLIENT_LOCK();

   {
      CLIENT_PROCESS_STATE_T *process = client_egl_get_process_state(thread, dpy, EGL_TRUE);

      if (process) {
         egl_perf_monitor_term(process);

         thread->error = EGL_SUCCESS;
         result = EGL_TRUE;
      } else
         result = EGL_FALSE;
   }

   CLIENT_UNLOCK();

   return result;
}

#endif

#if EGL_BRCM_perf_stats

EGLAPI void eglPerfStatsResetBRCM(void)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   RPC_CALL0(eglPerfStatsResetBRCM_impl, thread, EGLPERFSTATSRESETBRCM_ID);
}

EGLAPI void eglPerfStatsGetBRCM(char *buffer, EGLint buffer_len, EGLBoolean reset)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   // int len = _min(_max(buffer_len, 0), KHDISPATCH_WORKSPACE_SIZE);

   RPC_CALL3_OUT_BULK(eglPerfStatsGetBRCM_impl,
                      thread,
                      EGLPERFSTATSGETBRCM_ID,
                      RPC_INT(buffer_len),
                      RPC_BOOLEAN(reset),
                      buffer);
}

#endif

#if EGL_BRCM_mem_usage

EGLAPI void eglProcessMemUsageGetBRCM(uint32_t id_0, uint32_t id_1, char *buffer, uint32_t buffer_len)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

   RPC_CALL4_OUT_BULK(eglIntGetProcessMemUsage_impl,
                      thread,
                      EGLINTGETPROCESSMEMUSAGE_ID,
                      RPC_UINT(id_0),
                      RPC_UINT(id_1),
                      RPC_UINT(buffer_len),
                      buffer);
}

#endif
