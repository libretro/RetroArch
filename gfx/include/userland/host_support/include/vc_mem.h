/*
Copyright (c) 2013, Broadcom Europe Ltd
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
#if !defined( VC_MEM_H )
#define VC_MEM_H

#include <linux/ioctl.h>

#define VC_MEM_IOC_MAGIC  'v'

#define VC_MEM_IOC_MEM_PHYS_ADDR    _IOR( VC_MEM_IOC_MAGIC, 0, unsigned long )
#define VC_MEM_IOC_MEM_SIZE         _IOR( VC_MEM_IOC_MAGIC, 1, unsigned int )
#define VC_MEM_IOC_MEM_BASE         _IOR( VC_MEM_IOC_MAGIC, 2, unsigned int )
#define VC_MEM_IOC_MEM_LOAD         _IOR( VC_MEM_IOC_MAGIC, 3, unsigned int )
#define VC_MEM_IOC_MEM_COPY         _IOWR( VC_MEM_IOC_MAGIC, 4, char *)

#if defined( __KERNEL__ )
#define VC_MEM_TO_ARM_ADDR_MASK 0x3FFFFFFF

extern unsigned long mm_vc_mem_phys_addr;
extern unsigned int  mm_vc_mem_size;
extern unsigned int  mm_vc_mem_base;
extern int vc_mem_get_current_size( void );
extern int vc_mem_get_current_base( void );
extern int vc_mem_access_mem( int write_mem, void *buf, uint32_t vc_mem_addr, size_t num_bytes );
#endif

#endif  /* VC_MEM_H */
