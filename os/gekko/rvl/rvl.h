/* Wii internals shared between the IOS clients. */

#ifndef GEKKO_RVL_H
#define GEKKO_RVL_H

#include <gekko/gekko.h>

/* Zeroed, 32-byte aligned memory in MEM2 for IOS to use; waits while
 * the pool is full, NULL only for sizes it can never hold. */
void *gk_iobuf_get(uint32_t size);
void  gk_iobuf_put(void *p, uint32_t size);

#endif
