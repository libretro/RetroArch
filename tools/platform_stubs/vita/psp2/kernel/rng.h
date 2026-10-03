/* Host stand-in for vitasdk's psp2/kernel/rng.h: the kernel random
 * source the crypto's random path draws on. Declarations only. */
#ifndef PLATFORM_STUB_PSP2_RNG_H
#define PLATFORM_STUB_PSP2_RNG_H
#include <stddef.h>
int sceKernelGetRandomNumber(void *output, size_t size);
#endif
