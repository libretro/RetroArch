/* Bodies for the stand-in <3ds.h>. CSND is not modelled: its driver
 * tracks the play position from the system tick alone, so the tick is
 * the device. NDSP's is the channel's sample position. The test moves
 * both. */

#include <stdlib.h>
#include <stdint.h>

#include "3ds.h"
#include "ctr_mock.h"
#include <features/features_cpu.h>

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

u32 mock_sample_pos       = 0;
u32 mock_frames_per_sleep = 64;

void svcSleepThread(s64 ns)
{
   (void)ns;
   mock_sample_pos = (mock_sample_pos + mock_frames_per_sleep) & 2047u;
}

Result ndspInit(void) { return 0; }
void   ndspExit(void) { }
void   ndspSetCallback(ndspCallback callback, void *data) { (void)callback; (void)data; }
void   ndspSetOutputMode(int mode) { (void)mode; }
void   ndspSetClippingMode(int mode) { (void)mode; }
void   ndspSetOutputCount(int count) { (void)count; }
void   ndspSetMasterVol(float volume) { (void)volume; }
void   ndspChnReset(int id) { (void)id; }
void   ndspChnSetFormat(int id, u16 format) { (void)id; (void)format; }
void   ndspChnSetInterp(int id, int type) { (void)id; (void)type; }
void   ndspChnSetRate(int id, float rate) { (void)id; (void)rate; }
void   ndspChnWaveBufClear(int id) { (void)id; }
void   ndspChnWaveBufAdd(int id, ndspWaveBuf *buf) { (void)id; (void)buf; }
u32    ndspChnGetSamplePos(int id) { (void)id; return mock_sample_pos; }

Result DSP_FlushDataCache(const void *address, u32 size)
{
   (void)address;
   (void)size;
   return 0;
}

void LightEvent_Init(LightEvent *event, ResetType reset_type)
{
   (void)reset_type;
   event->state = 0;
}
void LightEvent_Signal(LightEvent *event) { event->state = 1; }
int  LightEvent_TryWait(LightEvent *event) { int s = event->state; event->state = 0; return s; }
int  LightEvent_WaitTimeout(LightEvent *event, s64 timeout_ns)
{
   svcSleepThread(timeout_ns);
   return LightEvent_TryWait(event);
}

bool aptMainLoop(void) { return true; }

/* The frontend and libretro-common symbols the drivers reach. */
bool retroarch_main_quit(void) { return true; }
retro_perf_tick_t cpu_features_get_perf_counter(void) { return (retro_perf_tick_t)mock_tick; }
