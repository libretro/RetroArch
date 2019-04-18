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

static unsigned int shader_256[] = {
    #include "hex/shader_256.hex"
};
static unsigned int shader_512[] = {
    #include "hex/shader_512.hex"
};
static unsigned int shader_1k[] = {
    #include "hex/shader_1k.hex"
};
static unsigned int shader_2k[] = {
    #include "hex/shader_2k.hex"
};
static unsigned int shader_4k[] = {
    #include "hex/shader_4k.hex"
};
static unsigned int shader_8k[] = {
    #include "hex/shader_8k.hex"
};
static unsigned int shader_16k[] = {
    #include "hex/shader_16k.hex"
};
static unsigned int shader_32k[] = {
    #include "hex/shader_32k.hex"
};
static unsigned int shader_64k[] = {
    #include "hex/shader_64k.hex"
};
static unsigned int shader_128k[] = {
    #include "hex/shader_128k.hex"
};
static unsigned int shader_256k[] = {
    #include "hex/shader_256k.hex"
};
static unsigned int shader_512k[] = {
    #include "hex/shader_512k.hex"
};
static unsigned int shader_1024k[] = {
    #include "hex/shader_1024k.hex"
};
static unsigned int shader_2048k[] = {
    #include "hex/shader_2048k.hex"
};
static unsigned int shader_4096k[] = {
    #include "hex/shader_4096k.hex"
};

static struct {
    unsigned int size, *code;
}
shaders[] = {
    {sizeof(shader_256), shader_256},
    {sizeof(shader_512), shader_512},
    {sizeof(shader_1k), shader_1k},
    {sizeof(shader_2k), shader_2k},
    {sizeof(shader_4k), shader_4k},
    {sizeof(shader_8k), shader_8k},
    {sizeof(shader_16k), shader_16k},
    {sizeof(shader_32k), shader_32k},
    {sizeof(shader_64k), shader_64k},
    {sizeof(shader_128k), shader_128k},
    {sizeof(shader_256k), shader_256k},
    {sizeof(shader_512k), shader_512k},
    {sizeof(shader_1024k), shader_1024k},
    {sizeof(shader_2048k), shader_2048k},
    {sizeof(shader_4096k), shader_4096k}
};

unsigned int  gpu_fft_shader_size(int log2_N) {
    return shaders[log2_N-8].size;
}

unsigned int *gpu_fft_shader_code(int log2_N) {
    return shaders[log2_N-8].code;
}
