/* Compile-only PSP controller stub; see pspkernel.h.
 *
 * SceCtrlData carries exactly what pspsdk's does, in the same order:
 * the PSP has one analog stick, so there is no Rx/Ry here either. The
 * driver's right-stick and L2/R2 reads are behind VITA, and a stub that
 * offered those members would let a lane pass code the real pspsdk
 * rejects. Button values are the real ones. */
#ifndef STUB_PSPCTRL_H
#define STUB_PSPCTRL_H

#define PSP_CTRL_SELECT   0x000001
#define PSP_CTRL_START    0x000008
#define PSP_CTRL_UP       0x000010
#define PSP_CTRL_RIGHT    0x000020
#define PSP_CTRL_DOWN     0x000040
#define PSP_CTRL_LEFT     0x000080
#define PSP_CTRL_LTRIGGER 0x000100
#define PSP_CTRL_RTRIGGER 0x000200
#define PSP_CTRL_TRIANGLE 0x001000
#define PSP_CTRL_CIRCLE   0x002000
#define PSP_CTRL_CROSS    0x004000
#define PSP_CTRL_SQUARE   0x008000
#define PSP_CTRL_HOME     0x010000
#define PSP_CTRL_HOLD     0x020000
#define PSP_CTRL_NOTE     0x800000

#define PSP_CTRL_MODE_DIGITAL 0
#define PSP_CTRL_MODE_ANALOG  1

typedef struct SceCtrlData
{
   unsigned int  TimeStamp;
   unsigned int  Buttons;
   unsigned char Lx;
   unsigned char Ly;
   unsigned char Rsrv[6];
} SceCtrlData;

int sceCtrlSetSamplingCycle(int cycle);
int sceCtrlSetSamplingMode(int mode);
int sceCtrlPeekBufferPositive(SceCtrlData *pad_data, int count);

#endif
