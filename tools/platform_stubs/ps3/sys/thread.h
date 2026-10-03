/* Compile-only stand-in for PSL1GHT's <sys/thread.h>: enough of the PPU
 * thread surface for a host syntax check of the PS3 drivers. Not a
 * runtime shim. */
#ifndef PS3STUB_SYS_THREAD_H
#define PS3STUB_SYS_THREAD_H

#include <stdint.h>
#include <stddef.h>

typedef uint64_t sys_ppu_thread_t;

/* ps3_defines.h maps SYS_THREAD_CREATE_JOINABLE onto this. */
#define THREAD_JOINABLE 1

int sysThreadCreate(sys_ppu_thread_t *id, void (*entry)(void *arg),
      void *arg, int prio, size_t stacksize, int flags, char *name);
void sysThreadExit(int code);
int sysThreadJoin(sys_ppu_thread_t id, uint64_t *exitcode);

#endif
