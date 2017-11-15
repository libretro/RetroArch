#ifndef __SI_H__
#define __SI_H__

#include <gctypes.h>

#define SI_CHAN0				0
#define SI_CHAN1				1
#define SI_CHAN2				2
#define SI_CHAN3				3
#define SI_MAX_CHAN             4

#define SI_CHAN0_BIT			0x80000000
#define SI_CHAN1_BIT			0x40000000
#define SI_CHAN2_BIT			0x20000000
#define SI_CHAN3_BIT			0x10000000
#define SI_CHAN_BIT(chn)		(SI_CHAN0_BIT>>(chn))

#define SI_ERROR_UNDER_RUN      0x0001
#define SI_ERROR_OVER_RUN       0x0002
#define SI_ERROR_COLLISION      0x0004
#define SI_ERROR_NO_RESPONSE    0x0008
#define SI_ERROR_WRST           0x0010
#define SI_ERROR_RDST           0x0020
#define SI_ERR_UNKNOWN			0x0040
#define SI_ERR_BUSY				0x0080

//
// CMD_TYPE_AND_STATUS response data
//
#define SI_TYPE_MASK            0x18000000u
#define SI_TYPE_N64             0x00000000u
#define SI_TYPE_DOLPHIN         0x08000000u
#define SI_TYPE_GC              SI_TYPE_DOLPHIN

// GameCube specific
#define SI_GC_WIRELESS          0x80000000u
#define SI_GC_NOMOTOR           0x20000000u // no rumble motor
#define SI_GC_STANDARD          0x01000000u // dolphin standard controller

// WaveBird specific
#define SI_WIRELESS_RECEIVED    0x40000000u // 0: no wireless unit
#define SI_WIRELESS_IR          0x04000000u // 0: IR  1: RF
#define SI_WIRELESS_STATE       0x02000000u // 0: variable  1: fixed
#define SI_WIRELESS_ORIGIN      0x00200000u // 0: invalid  1: valid
#define SI_WIRELESS_FIX_ID      0x00100000u // 0: not fixed  1: fixed
#define SI_WIRELESS_TYPE        0x000f0000u
#define SI_WIRELESS_LITE_MASK   0x000c0000u // 0: normal 1: lite controller
#define SI_WIRELESS_LITE        0x00040000u // 0: normal 1: lite controller
#define SI_WIRELESS_CONT_MASK   0x00080000u // 0: non-controller 1: non-controller
#define SI_WIRELESS_CONT        0x00000000u
#define SI_WIRELESS_ID          0x00c0ff00u
#define SI_WIRELESS_TYPE_ID     (SI_WIRELESS_TYPE | SI_WIRELESS_ID)

#define SI_N64_CONTROLLER       (SI_TYPE_N64 | 0x05000000)
#define SI_N64_MIC              (SI_TYPE_N64 | 0x00010000)
#define SI_N64_KEYBOARD         (SI_TYPE_N64 | 0x00020000)
#define SI_N64_MOUSE            (SI_TYPE_N64 | 0x02000000)
#define SI_GBA                  (SI_TYPE_N64 | 0x00040000)
#define SI_GC_CONTROLLER        (SI_TYPE_GC | SI_GC_STANDARD)
#define SI_GC_RECEIVER          (SI_TYPE_GC | SI_GC_WIRELESS)
#define SI_GC_WAVEBIRD          (SI_TYPE_GC | SI_GC_WIRELESS | SI_GC_STANDARD | SI_WIRELESS_STATE | SI_WIRELESS_FIX_ID)
#define SI_GC_KEYBOARD          (SI_TYPE_GC | 0x00200000)
#define SI_GC_STEERING          (SI_TYPE_GC | 0x00000000)

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef void (*SICallback)(s32,u32);
typedef void (*RDSTHandler)(u32,void*);

u32 SI_Sync();
u32 SI_Busy();
u32 SI_IsChanBusy(s32 chan);
void SI_EnablePolling(u32 poll);
void SI_DisablePolling(u32 poll);
void SI_SetCommand(s32 chan,u32 cmd);
u32 SI_GetStatus(s32 chan);
u32 SI_GetResponse(s32 chan,void *buf);
u32 SI_GetResponseRaw(s32 chan);
void SI_RefreshSamplingRate();
u32 SI_Transfer(s32 chan,void *out,u32 out_len,void *in,u32 in_len,SICallback cb,u32 us_delay);
u32 SI_GetTypeAsync(s32 chan,SICallback cb);
u32 SI_GetType(s32 chan);
u32 SI_GetCommand(s32 chan);
void SI_TransferCommands();
u32 SI_RegisterPollingHandler(RDSTHandler handler);
u32 SI_UnregisterPollingHandler(RDSTHandler handler);
u32 SI_EnablePollingInterrupt(s32 enable);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
