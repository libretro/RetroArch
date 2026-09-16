/* Compile-only PSP stub for the matrix's psp lane; the shapes
 * psp_audio.c compiles against. Never linked. */
#ifndef STUB_PSPKERNEL_H
#define STUB_PSPKERNEL_H
typedef int SceUID;
long long sceKernelGetSystemTimeWide(void);
#endif
