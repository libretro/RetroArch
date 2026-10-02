/* A stand-in <3ds.h>: the libctru surface audio/drivers/ctr_csnd_audio.c
 * uses, so the driver builds on the host. csnd_mock.c holds the bodies;
 * the system tick is the test's to move. */

#ifndef CTR_CSND_MOCK_3DS_H
#define CTR_CSND_MOCK_3DS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef s32      Result;

#define BIT(n) (1u << (n))

enum
{
   CSND_ENCODING_PCM8 = 0,
   CSND_ENCODING_PCM16,
   CSND_ENCODING_ADPCM,
   CSND_ENCODING_PSG
};

enum
{
   CSND_LOOPMODE_MANUAL = 0,
   CSND_LOOPMODE_NORMAL,
   CSND_LOOPMODE_ONESHOT,
   CSND_LOOPMODE_NORELOAD
};

#define SOUND_CHANNEL(n)   ((u32)(n) & 0x1F)
#define SOUND_FORMAT(n)    ((u32)(n) << 12)
#define SOUND_LOOPMODE(n)  ((u32)(n) << 10)
#define SOUND_ONE_SHOT     SOUND_LOOPMODE(CSND_LOOPMODE_ONESHOT)
#define SOUND_ENABLE       BIT(14)

extern u32 csndChannels;

u32    CSND_VOL(float vol, float pan);
void   CSND_SetChnRegs(u32 flags, u32 physaddr0, u32 physaddr1,
      u32 totalbytesize, u32 chnVolumes, u32 capVolumes);
void   CSND_SetBlock(int chn, int block, u32 physaddr, u32 size);
void   CSND_SetAdpcmState(int chn, int block, int sample, int index);
void   CSND_SetPlayState(u32 channel, u32 value);
void   CSND_SetVol(u32 channel, u32 chnVolumes, u32 capVolumes);
Result csndExecCmds(bool waitDone);

void  *linearAlloc(size_t size);
void   linearFree(void *mem);
u32    osConvertVirtToPhys(const void *vaddr);
Result GSPGPU_FlushDataCache(const void *adr, u32 size);

u64    svcGetSystemTick(void);
void   svcSleepThread(s64 ns);

#endif
