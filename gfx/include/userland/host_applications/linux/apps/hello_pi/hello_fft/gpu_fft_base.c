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

#include <dlfcn.h>

#include "gpu_fft.h"
#include "mailbox.h"

#define BUS_TO_PHYS(x) ((x)&~0xC0000000)

// V3D spec: http://www.broadcom.com/docs/support/videocore/VideoCoreIV-AG100-R.pdf
#define V3D_L2CACTL (0xC00020>>2)
#define V3D_SLCACTL (0xC00024>>2)
#define V3D_SRQPC   (0xC00430>>2)
#define V3D_SRQUA   (0xC00434>>2)
#define V3D_SRQCS   (0xC0043c>>2)
#define V3D_DBCFG   (0xC00e00>>2)
#define V3D_DBQITE  (0xC00e2c>>2)
#define V3D_DBQITC  (0xC00e30>>2)

// Setting this define to zero on Pi 1 allows GPU_FFT and Open GL
// to co-exist and also improves performance of longer transforms:
#define GPU_FFT_USE_VC4_L2_CACHE 1 // Pi 1 only: cached=1; direct=0

#define GPU_FFT_NO_FLUSH 1
#define GPU_FFT_TIMEOUT 2000 // ms

struct GPU_FFT_HOST {
    unsigned mem_flg, mem_map, peri_addr, peri_size;
};

int gpu_fft_get_host_info(struct GPU_FFT_HOST *info) {
    void *handle;
    unsigned (*bcm_host_get_sdram_address)     (void);
    unsigned (*bcm_host_get_peripheral_address)(void);
    unsigned (*bcm_host_get_peripheral_size)   (void);

    // Pi 1 defaults
    info->peri_addr = 0x20000000;
    info->peri_size = 0x01000000;
    info->mem_flg = GPU_FFT_USE_VC4_L2_CACHE? 0xC : 0x4;
    info->mem_map = GPU_FFT_USE_VC4_L2_CACHE? 0x0 : 0x20000000; // Pi 1 only

    handle = dlopen("libbcm_host.so", RTLD_LAZY);
    if (!handle) return -1;

    *(void **) (&bcm_host_get_sdram_address)      = dlsym(handle, "bcm_host_get_sdram_address");
    *(void **) (&bcm_host_get_peripheral_address) = dlsym(handle, "bcm_host_get_peripheral_address");
    *(void **) (&bcm_host_get_peripheral_size)    = dlsym(handle, "bcm_host_get_peripheral_size");

    if (bcm_host_get_sdram_address && bcm_host_get_sdram_address()!=0x40000000) { // Pi 2?
        info->mem_flg = 0x4; // ARM cannot see VC4 L2 on Pi 2
        info->mem_map = 0x0;
    }

    if (bcm_host_get_peripheral_address) info->peri_addr = bcm_host_get_peripheral_address();
    if (bcm_host_get_peripheral_size)    info->peri_size = bcm_host_get_peripheral_size();

    dlclose(handle);
    return 0;
}

unsigned gpu_fft_base_exec_direct (
    struct GPU_FFT_BASE *base,
    int num_qpus) {

    unsigned q, t;

    base->peri[V3D_DBCFG] = 0; // Disallow IRQ
    base->peri[V3D_DBQITE] = 0; // Disable IRQ
    base->peri[V3D_DBQITC] = -1; // Resets IRQ flags

    base->peri[V3D_L2CACTL] =  1<<2; // Clear L2 cache
    base->peri[V3D_SLCACTL] = -1; // Clear other caches

    base->peri[V3D_SRQCS] = (1<<7) | (1<<8) | (1<<16); // Reset error bit and counts

    for (q=0; q<num_qpus; q++) { // Launch shader(s)
        base->peri[V3D_SRQUA] = base->vc_unifs[q];
        base->peri[V3D_SRQPC] = base->vc_code;
    }

    // Busy wait polling
    for (;;) {
        if (((base->peri[V3D_SRQCS]>>16) & 0xff) == num_qpus) break; // All done?
    }

    return 0;
}

unsigned gpu_fft_base_exec(
    struct GPU_FFT_BASE *base,
    int num_qpus) {

    if (base->vc_msg) {
        // Use mailbox
        // Returns: 0x0 for success; 0x80000000 for timeout
        return execute_qpu(base->mb, num_qpus, base->vc_msg, GPU_FFT_NO_FLUSH, GPU_FFT_TIMEOUT);
    }
    else {
        // Direct register poking
        return gpu_fft_base_exec_direct(base, num_qpus);
    }
}

int gpu_fft_alloc (
    int mb,
    unsigned size,
    struct GPU_FFT_PTR *ptr) {

    struct GPU_FFT_HOST host;
    struct GPU_FFT_BASE *base;
    volatile unsigned *peri;
    unsigned handle;

    if (gpu_fft_get_host_info(&host)) return -5;

    if (qpu_enable(mb, 1)) return -1;

    // Shared memory
    handle = mem_alloc(mb, size, 4096, host.mem_flg);
    if (!handle) {
        qpu_enable(mb, 0);
        return -3;
    }

    peri = (volatile unsigned *) mapmem(host.peri_addr, host.peri_size);
    if (!peri) {
        mem_free(mb, handle);
        qpu_enable(mb, 0);
        return -4;
    }

    ptr->vc = mem_lock(mb, handle);
    ptr->arm.vptr = mapmem(BUS_TO_PHYS(ptr->vc+host.mem_map), size);

    base = (struct GPU_FFT_BASE *) ptr->arm.vptr;
    base->peri      = peri;
    base->peri_size = host.peri_size;
    base->mb        = mb;
    base->handle    = handle;
    base->size      = size;

    return 0;
}

void gpu_fft_base_release(struct GPU_FFT_BASE *base) {
    int mb = base->mb;
    unsigned handle = base->handle, size = base->size;
    unmapmem((void*)base->peri, base->peri_size);
    unmapmem((void*)base, size);
    mem_unlock(mb, handle);
    mem_free(mb, handle);
    qpu_enable(mb, 0);
}

unsigned gpu_fft_ptr_inc (
    struct GPU_FFT_PTR *ptr,
    int bytes) {

    unsigned vc = ptr->vc;
    ptr->vc += bytes;
    ptr->arm.bptr += bytes;
    return vc;
}
