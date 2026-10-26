#ifndef __WIIU_HBL_LOADER_H__
#define __WIIU_HBL_LOADER_H__

#include "wiiu/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MEM_BASE
#define MEM_BASE                    (0x00800000)
#endif

#define ARGV_PTR             (*(void* volatile *)(MEM_BASE + 0x1300 + 0x80))

#define MAKE_MAGIC(c0, c1, c2, c3)  (((c0) << 24) |((c1) << 16) |((c2) << 8) | c3)
#define ARGV_MAGIC                  MAKE_MAGIC('_', 'a', 'r', 'g')

int HBL_loadToMemory(const char *filepath, u32 args_size);
void* getApplicationEndAddr(void);

#ifdef __cplusplus
}
#endif

#endif /* __WIIU_HBL_LOADER_H__ */
