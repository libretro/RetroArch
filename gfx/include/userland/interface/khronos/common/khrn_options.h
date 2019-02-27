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

#ifndef KHRN_OPTIONS_H
#define KHRN_OPTIONS_H

#include "interface/khronos/common/khrn_int_common.h"
#include "interface/khronos/include/GLES/gl.h"

typedef struct {
   bool     gl_error_assist;           /* Outputs useful info when the error occurs */
   bool     double_buffer;             /* Defaults to triple-buffered */
   bool     no_bin_render_overlap;     /* Prevent bin/render overlap */
   bool     reg_dump_on_lock;          /* Dump h/w registers if the h/w locks-up */
   bool     clif_dump_on_lock;         /* Dump clif file and memory on h/w lock-up */
   bool     force_dither_off;          /* Ensure dithering is always off */
   uint32_t bin_block_size;            /* Set the size of binning memory blocks */
   uint32_t max_bin_blocks;            /* Set the maximum number of binning block in use */

} KHRN_OPTIONS_T;

extern KHRN_OPTIONS_T khrn_options;

extern void khrn_init_options(void);
extern void khrn_error_assist(GLenum error, const char *func);

#ifdef WIN32
#undef __func__
#define __func__ __FUNCTION__
#endif

#endif
