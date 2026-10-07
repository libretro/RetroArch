/* Wii internals shared between the IOS clients. */

#ifndef GEKKO_RVL_H
#define GEKKO_RVL_H

#include <gekko/gekko.h>

/* Zeroed, 32-byte aligned memory in MEM2 for IOS to use; waits while
 * the pool is full, NULL only for sizes it can never hold. */
void *gk_iobuf_get(uint32_t size);
void  gk_iobuf_put(void *p, uint32_t size);

/* ipc.c: ES_LaunchTitle of IOS major, the IPC state started over
 * once the new IOS takes requests.  0, or -1 when it did not come
 * up. */
#include <gekko/ios.h>
int gk_ipc_launch_ios(int32_t es_fd, const gk_ios_vec_t *vec,
      uint32_t major);

#endif
