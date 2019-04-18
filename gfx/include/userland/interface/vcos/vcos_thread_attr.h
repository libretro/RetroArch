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

/*=============================================================================
VideoCore OS Abstraction Layer - thread attributes
=============================================================================*/

#ifndef VCOS_THREAD_ATTR_H
#define VCOS_THREAD_ATTR_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \file
 *
 * Attributes for thread creation.
 *
 */

/** Initialize thread attribute struct. This call does not allocate memory,
  * and so cannot fail.
  *
  */
VCOSPRE_ void VCOSPOST_ vcos_thread_attr_init(VCOS_THREAD_ATTR_T *attrs);

/** Set the stack address and size. If not set, a stack will be allocated automatically.
  *
  * This can only be set on some platforms. It will always be possible to set the stack
  * address on VideoCore, but on host platforms, support may well not be available.
  */
#if VCOS_CAN_SET_STACK_ADDR
VCOS_INLINE_DECL
void vcos_thread_attr_setstack(VCOS_THREAD_ATTR_T *attrs, void *addr, VCOS_UNSIGNED sz);
#endif

/** Set the stack size. If not set, a default size will be used. Attempting to call this after having
  * set the stack location with vcos_thread_attr_setstack() will result in undefined behaviour.
  */
VCOS_INLINE_DECL
void vcos_thread_attr_setstacksize(VCOS_THREAD_ATTR_T *attrs, VCOS_UNSIGNED sz);

/** Set the task priority. If not set, a default value will be used.
  */
VCOS_INLINE_DECL
void vcos_thread_attr_setpriority(VCOS_THREAD_ATTR_T *attrs, VCOS_UNSIGNED pri);

/** Set the task cpu affinity. If not set, the default will be used.
  */
VCOS_INLINE_DECL
void vcos_thread_attr_setaffinity(VCOS_THREAD_ATTR_T *attrs, VCOS_UNSIGNED aff);

/** Set the timeslice. If not set the default will be used.
  */
VCOS_INLINE_DECL
void vcos_thread_attr_settimeslice(VCOS_THREAD_ATTR_T *attrs, VCOS_UNSIGNED ts);

/** The thread entry function takes (argc,argv), as per Nucleus, with
  * argc being 0. This may be withdrawn in a future release and should not
  * be used in new code.
  */
VCOS_INLINE_DECL
void _vcos_thread_attr_setlegacyapi(VCOS_THREAD_ATTR_T *attrs, VCOS_UNSIGNED legacy);

VCOS_INLINE_DECL
void vcos_thread_attr_setautostart(VCOS_THREAD_ATTR_T *attrs, VCOS_UNSIGNED autostart);

#ifdef __cplusplus
}
#endif
#endif
