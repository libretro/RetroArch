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
#include <stdio.h>
#include <time.h>

#include "gpu_fft_trans.h"
#include "hello_fft_2d_bitmap.h"

#define log2_N 9
#define N (1<<log2_N)

#define GPU_FFT_ROW(fft, io, y) ((fft)->io+(fft)->step*(y))

unsigned Microseconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec*1000000 + ts.tv_nsec/1000;
}

int main(int argc, char *argv[]) {
    int x, y, ret, mb = mbox_open();
    unsigned t[4];

    struct GPU_FFT_COMPLEX *row;
    struct GPU_FFT_TRANS *trans;
    struct GPU_FFT *fft_pass[2];

    BITMAPFILEHEADER bfh;
    BITMAPINFOHEADER bih;

    // Create Windows bitmap file
    FILE *fp = fopen("hello_fft_2d.bmp", "wb");
    if (!fp) return -666;

    // Write bitmap header
    memset(&bfh, 0, sizeof(bfh));
    bfh.bfType = 0x4D42; //"BM"
    bfh.bfSize = N*N*3;
    bfh.bfOffBits = sizeof(bfh) + sizeof(bih);
    fwrite(&bfh, sizeof(bfh), 1, fp);

    // Write bitmap info
    memset(&bih, 0, sizeof(bih));
    bih.biSize = sizeof(bih);
    bih.biWidth = N;
    bih.biHeight = N;
    bih.biPlanes = 1;
    bih.biBitCount = 24;
    bih.biCompression = BI_RGB;
    fwrite(&bih, sizeof(bih), 1, fp);

    // Prepare 1st FFT pass
    ret = gpu_fft_prepare(mb, log2_N, GPU_FFT_REV, N, fft_pass+0);
    if (ret) {
        return ret;
    }
    // Prepare 2nd FFT pass
    ret = gpu_fft_prepare(mb, log2_N, GPU_FFT_REV, N, fft_pass+1);
    if (ret) {
        gpu_fft_release(fft_pass[0]);
        return ret;
    }
    // Transpose from 1st pass output to 2nd pass input
    ret = gpu_fft_trans_prepare(mb, fft_pass[0], fft_pass[1], &trans);
    if (ret) {
        gpu_fft_release(fft_pass[0]);
        gpu_fft_release(fft_pass[1]);
        return ret;
    }

    // Clear input array
    for (y=0; y<N; y++) {
        row = GPU_FFT_ROW(fft_pass[0], in, y);
        for (x=0; x<N; x++) row[x].re = row[x].im = 0;
    }

    // Setup input data
    GPU_FFT_ROW(fft_pass[0], in,   2)[  2].re = 60;
    GPU_FFT_ROW(fft_pass[0], in, N-2)[N-2].re = 60;

    // ==> FFT() ==> T() ==> FFT() ==>
    usleep(1); /* yield to OS */   t[0] = Microseconds();
    gpu_fft_execute(fft_pass[0]);  t[1] = Microseconds();
    gpu_fft_trans_execute(trans);  t[2] = Microseconds();
    gpu_fft_execute(fft_pass[1]);  t[3] = Microseconds();

    // Write output to bmp file
    for (y=0; y<N; y++) {
        row = GPU_FFT_ROW(fft_pass[1], out, y);
        for (x=0; x<N; x++) {
            fputc(128+row[x].re, fp); // blue
            fputc(128+row[x].re, fp); // green
            fputc(128+row[x].re, fp); // red
        }
    }

    printf( "1st FFT   %6d usecs\n"
            "Transpose %6d usecs\n"
            "2nd FFT   %6d usecs\n",
            t[3]-t[2], t[2]-t[1], t[1]-t[0]);

    // Clean-up properly.  Videocore memory lost if not freed !
    gpu_fft_release(fft_pass[0]);
    gpu_fft_release(fft_pass[1]);
    gpu_fft_trans_release(trans);

    return 0;
}
