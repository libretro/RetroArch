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
#ifndef MMAL_VC_SHM_H
#define MMAL_VC_SHM_H

/** @file
  *
  * Abstraction layer for MMAL VC shared memory.
  * This API is only used by the MMAL VC component.
  */

#include "mmal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialise the shared memory system */
MMAL_STATUS_T mmal_vc_shm_init(void);

/** Allocate a shared memory buffer */
uint8_t *mmal_vc_shm_alloc(uint32_t size);

/** Free a shared memory buffer */
MMAL_STATUS_T mmal_vc_shm_free(uint8_t *mem);

/** Lock a shared memory buffer */
uint8_t *mmal_vc_shm_lock(uint8_t *mem, uint32_t workaround);

/** Unlock a shared memory buffer */
uint8_t *mmal_vc_shm_unlock(uint8_t *mem, uint32_t *length, uint32_t workaround);

#ifdef __cplusplus
}
#endif

#endif /* MMAL_VC_SHM_H */
