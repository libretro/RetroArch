/* Stand-in for libctru's <3ds.h>: what os/ctr/ctr_linear.c names. */
#ifndef CTR_LINEAR_STUB_3DS_H
#define CTR_LINEAR_STUB_3DS_H
#include <3ds/types.h>
typedef enum { MEMOP_FREE = 1 } MemOp;
typedef enum { MEMPERM_READ = 1, MEMPERM_WRITE = 2 } MemPerm;
Result svcControlMemory(u32 *addr_out, u32 addr0, u32 addr1, u32 size,
      MemOp op, MemPerm perm);
void  *linearAlloc(size_t size);
void  *linearMemAlign(size_t size, size_t alignment);
void  *linearRealloc(void *mem, size_t size);
size_t linearGetSize(void *mem);
void   linearFree(void *mem);
u32    linearSpaceFree(void);
#endif
