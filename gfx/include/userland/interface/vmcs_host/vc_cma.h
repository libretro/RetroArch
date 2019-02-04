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
#ifndef _VC_CMA_H_
#define _VC_CMA_H_

#include "interface/vcos/vcos.h"
#include "interface/vchiq_arm/vchiq.h"

#ifdef __linux__

#include <linux/ioctl.h>

#define VC_CMA_IOC_MAGIC 0xc5

#define VC_CMA_IOC_RESERVE _IO(VC_CMA_IOC_MAGIC, 0)

#endif

#define VC_CMA_FOURCC VCHIQ_MAKE_FOURCC('C','M','A',' ')
#define VC_CMA_VERSION 2

#define VC_CMA_CHUNK_ORDER 6  /* 256K */
#define VC_CMA_CHUNK_SIZE (4096 << VC_CMA_CHUNK_ORDER)
#define VC_CMA_MAX_PARAMS_PER_MSG ((VCHIQ_MAX_MSG_SIZE - sizeof(unsigned short)) / sizeof(unsigned short))

enum
{
   VC_CMA_MSG_QUIT,
   VC_CMA_MSG_OPEN,
   VC_CMA_MSG_TICK,
   VC_CMA_MSG_ALLOC,     /* chunk count */
   VC_CMA_MSG_FREE,      /* chunk, chunk, ... */
   VC_CMA_MSG_ALLOCATED, /* chunk, chunk, ... */
   VC_CMA_MSG_REQUEST_ALLOC, /* chunk count */
   VC_CMA_MSG_REQUEST_FREE,  /* chunk count */
   VC_CMA_MSG_RESERVE,   /* bytes lo, bytes hi */
   VC_CMA_MSG_MAX
};

typedef struct vc_cma_msg_struct
{
    unsigned short type;
    unsigned short params[VC_CMA_MAX_PARAMS_PER_MSG];
} VC_CMA_MSG_T;

#endif
