/* Stand-in for ps2sdk's <kernel.h>: the semaphore, alarm and interrupt
 * calls audio/drivers/ps2_audio.c makes, implemented on the host by
 * ps2_mock.c with the shapes the real header gives them. */
#ifndef PS2_AUDIO_MOCK_KERNEL_H
#define PS2_AUDIO_MOCK_KERNEL_H
#include <stdint.h>
typedef int32_t  s32;
typedef uint16_t u16;
typedef uint32_t u32;
typedef struct t_ee_sema
{
   int count, max_count, init_count, wait_threads;
   u32 attr, option;
} ee_sema_t;
s32  CreateSema(ee_sema_t *sema);
s32  DeleteSema(s32 sema_id);
s32  SignalSema(s32 sema_id);
s32  iSignalSema(s32 sema_id);
s32  WaitSema(s32 sema_id);
s32  PollSema(s32 sema_id);
s32  SetAlarm(u16 time, void (*callback)(s32 alarm_id, u16 time, void *common), void *common);
s32  iReleaseAlarm(s32 alarm_id);
int  DIntr(void);
int  EIntr(void);
#define ExitHandler() ((void)0)
#endif
