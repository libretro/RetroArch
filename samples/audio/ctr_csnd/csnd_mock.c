/* Bodies for the stand-in <3ds.h>. CSND itself is not modelled: the
 * driver tracks the play position from the system tick alone, so the
 * tick is the device, and the test moves it. */

#include <stdlib.h>
#include <stdint.h>

#include "3ds.h"
#include "csnd_mock.h"

u32 csndChannels = BIT(8) | BIT(9);

u64 mock_tick = 0;

u32 CSND_VOL(float vol, float pan)
{
   (void)vol;
   (void)pan;
   return 0;
}

void CSND_SetChnRegs(u32 flags, u32 physaddr0, u32 physaddr1,
      u32 totalbytesize, u32 chnVolumes, u32 capVolumes)
{
   (void)flags;
   (void)physaddr0;
   (void)physaddr1;
   (void)totalbytesize;
   (void)chnVolumes;
   (void)capVolumes;
}

void CSND_SetBlock(int chn, int block, u32 physaddr, u32 size)
{
   (void)chn;
   (void)block;
   (void)physaddr;
   (void)size;
}

void CSND_SetAdpcmState(int chn, int block, int sample, int index)
{
   (void)chn;
   (void)block;
   (void)sample;
   (void)index;
}

void CSND_SetPlayState(u32 channel, u32 value)
{
   (void)channel;
   (void)value;
}

void CSND_SetVol(u32 channel, u32 chnVolumes, u32 capVolumes)
{
   (void)channel;
   (void)chnVolumes;
   (void)capVolumes;
}

Result csndExecCmds(bool waitDone)
{
   (void)waitDone;
   return 0;
}

void *linearAlloc(size_t size) { return malloc(size); }
void linearFree(void *mem) { free(mem); }

u32 osConvertVirtToPhys(const void *vaddr)
{
   return (u32)(uintptr_t)vaddr;
}

Result GSPGPU_FlushDataCache(const void *adr, u32 size)
{
   (void)adr;
   (void)size;
   return 0;
}

u64 svcGetSystemTick(void) { return mock_tick; }

void svcSleepThread(s64 ns) { (void)ns; }
