#ifndef __BTMEMR_H__
#define __BTMEMR_H__

#include <gctypes.h>

void btmemr_init();
void* btmemr_malloc(u32 size);
void btmemr_free(void *ptr);
void* btmemr_realloc(void *ptr,u32 newsize);
void* btmemr_reallocm(void *ptr,u32 newsize);

#endif
