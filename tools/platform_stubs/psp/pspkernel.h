/* Compile-only PSP stub for the matrix's psp lanes; the shapes
 * psp_audio.c, features_cpu.c and psp1_gfx.c compile against.
 * Never linked. */
#ifndef STUB_PSPKERNEL_H
#define STUB_PSPKERNEL_H

typedef int SceUID;
typedef unsigned int   u32;
typedef unsigned short u16;
typedef unsigned char  u8;

long long sceKernelGetSystemTimeWide(void);

#define PSP_VBLANK_INT 30

void sceKernelDcacheWritebackInvalidateAll(void);
void sceKernelDcacheWritebackRange(const void *addr, unsigned int size);
int  sceKernelRegisterSubIntrHandler(int intno, int no,
      void *handler, void *arg);
int  sceKernelReleaseSubIntrHandler(int intno, int no);
int  sceKernelEnableSubIntr(int intno, int no);
int  sceKernelDisableSubIntr(int intno, int no);

void pspDebugScreenSetColorMode(int mode);
void pspDebugScreenSetBase(void *base);
void pspDebugScreenSetXY(int x, int y);
void pspDebugScreenPuts(const char *str);

#endif
