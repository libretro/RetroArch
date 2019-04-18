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

#include <math.h>

#include "gpu_fft.h"

#define ALPHA(dx) (2*pow(sin((dx)/2),2))
#define  BETA(dx) (sin(dx))

static double k[16] = {0,8,4,4,2,2,2,2,1,1,1,1,1,1,1,1};
static double m[16] = {0,0,0,1,0,1,2,3,0,1,2,3,4,5,6,7};

/****************************************************************************/

static float *twiddles_base_16(double two_pi, float *out, double theta) {
    int i;
    for (i=0; i<16; i++) {
        *out++ = cos(two_pi/16*k[i]*m[i] + theta*k[i]);
        *out++ = sin(two_pi/16*k[i]*m[i] + theta*k[i]);
    }
    return out;
}

static float *twiddles_base_32(double two_pi, float *out, double theta) {
    int i;
    for (i=0; i<16; i++) {
        *out++ = cos(two_pi/32*i + theta);
        *out++ = sin(two_pi/32*i + theta);
    }
    return twiddles_base_16(two_pi, out, 2*theta);
}

static float *twiddles_base_64(double two_pi, float *out) {
    int i;
    for (i=0; i<32; i++) {
        *out++ = cos(two_pi/64*i);
        *out++ = sin(two_pi/64*i);
    }
    return twiddles_base_32(two_pi, out, 0);
}

/****************************************************************************/

static float *twiddles_step_16(double two_pi, float *out, double theta) {
    int i;
    for (i=0; i<16; i++) {
        *out++ = ALPHA(theta*k[i]);
        *out++ =  BETA(theta*k[i]);
    }
    return out;
}

static float *twiddles_step_32(double two_pi, float *out, double theta) {
    int i;
    for (i=0; i<16; i++) {
        *out++ = ALPHA(theta);
        *out++ =  BETA(theta);
    }
    return twiddles_step_16(two_pi, out, 2*theta);
}

static float *twiddles_step_64(double two_pi, float *out, double theta) {
    int i;
    for (i=0; i<32; i++) {
        *out++ = ALPHA(theta);
        *out++ =  BETA(theta);
    }
    return twiddles_step_32(two_pi, out, 2*theta);
}

/****************************************************************************/

static void twiddles_256(double two_pi, float *out) {
    double N=256;
    int q;

    out = twiddles_base_16(two_pi, out, 0);
    out = twiddles_step_16(two_pi, out, two_pi/N * GPU_FFT_QPUS);

    for (q=0; q<GPU_FFT_QPUS; q++)
        out = twiddles_base_16(two_pi, out, two_pi/N*q);
}

static void twiddles_512(double two_pi, float *out) {
    double N=512;
    int q;

    out = twiddles_base_32(two_pi, out, 0);
    out = twiddles_step_16(two_pi, out, two_pi/N * GPU_FFT_QPUS);

    for (q=0; q<GPU_FFT_QPUS; q++)
        out = twiddles_base_16(two_pi, out, two_pi/N*q);
}

static void twiddles_1k(double two_pi, float *out) {
    double N=1024;
    int q;

    out = twiddles_base_32(two_pi, out, 0);
    out = twiddles_step_32(two_pi, out, two_pi/N * GPU_FFT_QPUS);

    for (q=0; q<GPU_FFT_QPUS; q++)
        out = twiddles_base_32(two_pi, out, two_pi/N*q);
}

static void twiddles_2k(double two_pi, float *out) {
    double N=2048;
    int q;

    out = twiddles_base_64(two_pi, out);
    out = twiddles_step_32(two_pi, out, two_pi/N * GPU_FFT_QPUS);

    for (q=0; q<GPU_FFT_QPUS; q++)
        out = twiddles_base_32(two_pi, out, two_pi/N*q);
}

static void twiddles_4k(double two_pi, float *out) {
    double N=4096;
    int q;

    out = twiddles_base_16(two_pi, out, 0);
    out = twiddles_step_16(two_pi, out, two_pi/N * 16);
    out = twiddles_step_16(two_pi, out, two_pi/N * GPU_FFT_QPUS);

    for (q=0; q<GPU_FFT_QPUS; q++)
        out = twiddles_base_16(two_pi, out, two_pi/N*q);
}

static void twiddles_8k(double two_pi, float *out) {
    double N=8192;
    int q;

    out = twiddles_base_32(two_pi, out, 0);
    out = twiddles_step_16(two_pi, out, two_pi/N * 16);
    out = twiddles_step_16(two_pi, out, two_pi/N * GPU_FFT_QPUS);

    for (q=0; q<GPU_FFT_QPUS; q++)
        out = twiddles_base_16(two_pi, out, two_pi/N*q);
}

static void twiddles_16k(double two_pi, float *out) {
    double N=16384;
    int q;

    out = twiddles_base_32(two_pi, out, 0);
    out = twiddles_step_32(two_pi, out, two_pi/N * 16);
    out = twiddles_step_16(two_pi, out, two_pi/N * GPU_FFT_QPUS);

    for (q=0; q<GPU_FFT_QPUS; q++)
        out = twiddles_base_16(two_pi, out, two_pi/N*q);
}

static void twiddles_32k(double two_pi, float *out) {
    double N=32768;
    int q;

    out = twiddles_base_32(two_pi, out, 0);
    out = twiddles_step_32(two_pi, out, two_pi/N * 32);
    out = twiddles_step_32(two_pi, out, two_pi/N * GPU_FFT_QPUS);

    for (q=0; q<GPU_FFT_QPUS; q++)
        out = twiddles_base_32(two_pi, out, two_pi/N*q);
}

static void twiddles_64k(double two_pi, float *out) {
    double N=65536;
    int q;

    out = twiddles_base_64(two_pi, out);
    out = twiddles_step_32(two_pi, out, two_pi/N * 32);
    out = twiddles_step_32(two_pi, out, two_pi/N * GPU_FFT_QPUS);

    for (q=0; q<GPU_FFT_QPUS; q++)
        out = twiddles_base_32(two_pi, out, two_pi/N*q);
}

static void twiddles_128k(double two_pi, float *out) {
    double N=128*1024;
    int q;

    out = twiddles_base_32(two_pi, out, 0);
    out = twiddles_step_16(two_pi, out, two_pi/N * 16*16);
    out = twiddles_step_16(two_pi, out, two_pi/N * 16);
    out = twiddles_step_16(two_pi, out, two_pi/N * GPU_FFT_QPUS);

    for (q=0; q<GPU_FFT_QPUS; q++)
        out = twiddles_base_16(two_pi, out, two_pi/N*q);
}

static void twiddles_256k(double two_pi, float *out) {
    double N=256*1024;
    int q;

    out = twiddles_base_32(two_pi, out, 0);
    out = twiddles_step_16(two_pi, out, two_pi/N * 32*16);
    out = twiddles_step_16(two_pi, out, two_pi/N * 32);
    out = twiddles_step_32(two_pi, out, two_pi/N * GPU_FFT_QPUS);

    for (q=0; q<GPU_FFT_QPUS; q++)
        out = twiddles_base_32(two_pi, out, two_pi/N*q);
}

static void twiddles_512k(double two_pi, float *out) {
    double N=512*1024;
    int q;

    out = twiddles_base_32(two_pi, out, 0);
    out = twiddles_step_16(two_pi, out, two_pi/N * 32*32);
    out = twiddles_step_32(two_pi, out, two_pi/N * 32);
    out = twiddles_step_32(two_pi, out, two_pi/N * GPU_FFT_QPUS);

    for (q=0; q<GPU_FFT_QPUS; q++)
        out = twiddles_base_32(two_pi, out, two_pi/N*q);
}

static void twiddles_1024k(double two_pi, float *out) {
    double N=1024*1024;
    int q;

    out = twiddles_base_32(two_pi, out, 0);
    out = twiddles_step_32(two_pi, out, two_pi/N * 32*32);
    out = twiddles_step_32(two_pi, out, two_pi/N * 32);
    out = twiddles_step_32(two_pi, out, two_pi/N * GPU_FFT_QPUS);

    for (q=0; q<GPU_FFT_QPUS; q++)
        out = twiddles_base_32(two_pi, out, two_pi/N*q);
}

static void twiddles_2048k(double two_pi, float *out) {
    double N=2048*1024;
    int q;

    out = twiddles_base_64(two_pi, out);
    out = twiddles_step_32(two_pi, out, two_pi/N * 32*32);
    out = twiddles_step_32(two_pi, out, two_pi/N * 32);
    out = twiddles_step_32(two_pi, out, two_pi/N * GPU_FFT_QPUS);

    for (q=0; q<GPU_FFT_QPUS; q++)
        out = twiddles_base_32(two_pi, out, two_pi/N*q);
}

static void twiddles_4096k(double two_pi, float *out) {
    double N=4096*1024;
    int q;

    out = twiddles_base_64(two_pi, out);
    out = twiddles_step_64(two_pi, out, two_pi/N * 32*32);
    out = twiddles_step_32(two_pi, out, two_pi/N * 32);
    out = twiddles_step_32(two_pi, out, two_pi/N * GPU_FFT_QPUS);

    for (q=0; q<GPU_FFT_QPUS; q++)
        out = twiddles_base_32(two_pi, out, two_pi/N*q);
}

/****************************************************************************/

static struct {
    int passes, shared, unique;
    void (*twiddles)(double, float *);
}
shaders[] = {
    {2, 2, 1, twiddles_256},
    {2, 3, 1, twiddles_512},
    {2, 4, 2, twiddles_1k},
    {2, 6, 2, twiddles_2k},
    {3, 3, 1, twiddles_4k},
    {3, 4, 1, twiddles_8k},
    {3, 5, 1, twiddles_16k},
    {3, 6, 2, twiddles_32k},
    {3, 8, 2, twiddles_64k},
    {4, 5, 1, twiddles_128k},
    {4, 6, 2, twiddles_256k},
    {4, 7, 2, twiddles_512k},
    {4, 8, 2, twiddles_1024k},
    {4,10, 2, twiddles_2048k},
    {4,12, 2, twiddles_4096k}
};

int gpu_fft_twiddle_size(int log2_N, int *shared, int *unique, int *passes) {
    if (log2_N<8 || log2_N>22) return -1;
    *shared = shaders[log2_N-8].shared;
    *unique = shaders[log2_N-8].unique;
    *passes = shaders[log2_N-8].passes;
    return 0;
}

void gpu_fft_twiddle_data(int log2_N, int direction, float *out) {
    shaders[log2_N-8].twiddles((direction==GPU_FFT_FWD?-2:2)*GPU_FFT_PI, out);
}
