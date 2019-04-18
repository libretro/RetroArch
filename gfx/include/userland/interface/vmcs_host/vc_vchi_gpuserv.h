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

#ifndef VC_VCHI_GPUSERV_H
#define VC_VCHI_GPUSERV_H
#include "interface/vchiq_arm/vchiq.h"

// these go in command word of gpu_job_s
// EXECUTE_VPU and EXECUTE_QPU are valid from host
enum { EXECUTE_NONE, EXECUTE_VPU, EXECUTE_QPU, EXECUTE_SYNC };

struct vpu_job_s {
   // these are function address and parameters for vpu job
   uint32_t q[7];
   uint32_t dummy[21];
};

struct qpu_job_s {
   // parameters for qpu job
   uint32_t jobs;
   uint32_t noflush;
   uint32_t timeout;
   uint32_t dummy;
   uint32_t control[12][2];
};

struct sync_job_s {
   // parameters for syncjob
   // bit 0 set means wait for preceding vpu jobs to complete
   // bit 1 set means wait for preceding qpu jobs to complete
   uint32_t mask;
   uint32_t dummy[27];
};

struct gpu_callback_s {
  // callback to call when complete (can be NULL)
  void (*func)();
  void *cookie;
};

struct gpu_internal_s {
   void *message;
   int refcount;
};

struct gpu_job_s {
  // from enum above
  uint32_t command;
  // qpu or vpu jobs
  union {
    struct vpu_job_s v;
    struct qpu_job_s q;
    struct sync_job_s s;
  } u;
  // callback function to call when complete
  struct gpu_callback_s callback;
  // for internal use - leave as zero
  struct gpu_internal_s internal;
};

/* Initialise gpu service. Returns its interface number. This initialises
   the host side of the interface, it does not send anything to VideoCore. */

VCHPRE_ int32_t VCHPOST_ vc_gpuserv_init(void);

VCHPRE_ void VCHPOST_ vc_gpuserv_deinit(void);

VCHPRE_ int32_t VCHPOST_ vc_gpuserv_execute_code(int num_jobs, struct gpu_job_s jobs[]);

#endif
