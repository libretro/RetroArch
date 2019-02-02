/*
* Copyright (c) 2012 Broadcom Europe Ltd
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
 * Common definitions for services
 */

#ifndef _VC_SERVICE_COMMON_DEFS_H_
#define _VC_SERVICE_COMMON_DEFS_H_
#include "vcinclude/common.h"
//Map VCHI return value to internal error code
//VCHI return +1 for retry so we will map it to -2 to allow
//servers to use positive values to indicate alternative return values
typedef enum {
   VC_SERVICE_VCHI_SUCCESS = 0,
   VC_SERVICE_VCHI_VCHIQ_ERROR = -1,
   VC_SERVUCE_VCHI_RETRY = -2,
   VC_SERVICE_VCHI_UNKNOWN_ERROR = -3
} VC_SERVICE_VCHI_STATUS_T;

extern VC_SERVICE_VCHI_STATUS_T vchi2service_status(int32_t x);
extern const char* vchi2service_status_string(VC_SERVICE_VCHI_STATUS_T status);

#endif //#ifndef _VC_SERVICE_COMMON_DEFS_H_
