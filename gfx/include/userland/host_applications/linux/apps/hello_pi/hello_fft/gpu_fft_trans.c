/*
BCM2835 "GPU_FFT" release 2.0
Copyright (c) 2014, Andrew Holme.
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

#include <string.h>

#include "gpu_fft_trans.h"

static unsigned int shader_trans[1024] = {
    #include "hex/shader_trans.hex"
};

int gpu_fft_trans_prepare(
    int mb,
    struct GPU_FFT *src,
    struct GPU_FFT *dst,
    struct GPU_FFT_TRANS **out) {

    unsigned size, info_bytes, code_bytes, unif_bytes, mail_bytes;
    int ret;

    struct GPU_FFT_TRANS *info;
    struct GPU_FFT_BASE *base;
    struct GPU_FFT_PTR ptr;

    info_bytes = code_bytes = unif_bytes = mail_bytes = 4096; // 4k align

    size  = info_bytes +        // control
            code_bytes +        // shader, aligned
            unif_bytes +        // uniforms
            mail_bytes;         // mailbox message

    ret = gpu_fft_alloc(mb, size, &ptr);
    if (ret) return ret;

    // Control header
    info = (struct GPU_FFT_TRANS *) ptr.arm.vptr;
    base = (struct GPU_FFT_BASE *) info;
    gpu_fft_ptr_inc(&ptr, info_bytes);

    // Shader code
    memcpy(ptr.arm.vptr, shader_trans, code_bytes);
    base->vc_code = gpu_fft_ptr_inc(&ptr, code_bytes);

    // Uniforms
    ptr.arm.uptr[0] = src->base.vc_msg;
    ptr.arm.uptr[1] = ((char*)src->out) - ((char*)src->in); // output buffer offset
    ptr.arm.uptr[2] = dst->base.vc_msg;
    ptr.arm.uptr[3] = 0;
    ptr.arm.uptr[4] = src->step * sizeof(struct GPU_FFT_COMPLEX);
    ptr.arm.uptr[5] = dst->step * sizeof(struct GPU_FFT_COMPLEX);
    ptr.arm.uptr[6] = src->x < dst->y? src->x : dst->y;
    ptr.arm.uptr[7] = src->y < dst->x? src->y : dst->x;
    base->vc_unifs[0] = gpu_fft_ptr_inc(&ptr, unif_bytes);

    // Mailbox message
    ptr.arm.uptr[0] = base->vc_unifs[0];
    ptr.arm.uptr[1] = base->vc_code;
    base->vc_msg = gpu_fft_ptr_inc(&ptr, mail_bytes);

    *out = info;
    return 0;
}

unsigned gpu_fft_trans_execute(struct GPU_FFT_TRANS *info) {
    return gpu_fft_base_exec(&info->base, 1);
}

void gpu_fft_trans_release(struct GPU_FFT_TRANS *info) {
    gpu_fft_base_release(&info->base);
}
