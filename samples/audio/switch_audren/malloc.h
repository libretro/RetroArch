/* <malloc.h> for the driver's memalign(): the host's own, and on
 * Windows, where there is none, a plain allocation the driver's free()
 * can release - the stand-in renderer reads the pool, the alignment is
 * the console's. */
#ifndef SWITCH_AUDREN_MOCK_MALLOC_H
#define SWITCH_AUDREN_MOCK_MALLOC_H
#include_next <malloc.h>
#ifdef _WIN32
#include <stdlib.h>
static __inline void *memalign(size_t alignment, size_t size)
{
   (void)alignment;
   return malloc(size);
}
#endif
#endif
