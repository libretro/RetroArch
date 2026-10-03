/* Compile-only PS2 stub for the matrix's ps2 video lane, carrying
 * the semaphore calls the driver's vsync handler uses. Each
 * declaration is the shape the real header gives it,
 * ExitHandler() included: its body is the R5900 sequence the SDK
 * writes, which the lane never assembles because it only parses.
 * A lane that compiled to an object would need a host-side
 * stand-in for it. */
#ifndef STUB_PS2_KERNEL
#define STUB_PS2_KERNEL
#include <tamtypes.h>

extern int DIntr(void);
extern s32 DeleteSema(s32 sema_id);
extern int EIntr(void);
#define ExitHandler() __asm__ __volatile__("sync\nei\n")
extern s32 PollSema(s32 sema_id);
extern s32 WaitSema(s32 sema_id);

typedef struct t_ee_sema
{
int count,
max_count,
init_count,
wait_threads;
u32 attr,
option;
} ee_sema_t;

extern s32 iSignalSema(s32 sema_id);
extern s32 CreateSema(ee_sema_t *sema);

#endif
