/*
BCM2835 "GPU_FFT" release 3.0
Copyright (c) 2015, Andrew Holme.
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

#ifndef __GPU_FFT__
#define __GPU_FFT__

#define GPU_FFT_QPUS 8

#define GPU_FFT_PI 3.14159265358979323846

#define GPU_FFT_FWD 0 // forward FFT
#define GPU_FFT_REV 1 // inverse FFT

struct GPU_FFT_COMPLEX {
    float re, im;
};

struct GPU_FFT_PTR {
    unsigned vc;
    union { struct GPU_FFT_COMPLEX *cptr;
            void                   *vptr;
            char                   *bptr;
            float                  *fptr;
            unsigned               *uptr; } arm;
};

struct GPU_FFT_BASE {
    int mb;
    unsigned handle, size, vc_msg, vc_code, vc_unifs[GPU_FFT_QPUS], peri_size;
    volatile unsigned *peri;
};

struct GPU_FFT {
    struct GPU_FFT_BASE base;
    struct GPU_FFT_COMPLEX *in, *out;
    int x, y, step;
};

int gpu_fft_prepare(
    int mb,         // mailbox file_desc
    int log2_N,     // log2(FFT_length) = 8...22
    int direction,  // GPU_FFT_FWD: fft(); GPU_FFT_REV: ifft()
    int jobs,       // number of transforms in batch
    struct GPU_FFT **fft);

unsigned gpu_fft_execute(
    struct GPU_FFT *info);

void gpu_fft_release(
    struct GPU_FFT *info);

// private
int           gpu_fft_twiddle_size(int, int *, int *, int *);
void          gpu_fft_twiddle_data(int, int, float *);
unsigned int  gpu_fft_shader_size(int);
unsigned int *gpu_fft_shader_code(int);

// gpu_fft_base:

unsigned gpu_fft_base_exec (
    struct GPU_FFT_BASE *base,
    int num_qpus);

int gpu_fft_alloc (
    int mb,
    unsigned size,
    struct GPU_FFT_PTR *ptr);

void gpu_fft_base_release(
    struct GPU_FFT_BASE *base);

unsigned gpu_fft_ptr_inc (
    struct GPU_FFT_PTR *ptr,
    int bytes);

#endif // __GPU_FFT__
