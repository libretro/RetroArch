#ifndef __MEMR_H__
#define __MEMR_H__

#include <gctypes.h>

void memr_init();
void* memr_malloc(u32 size);
void memr_free(void *ptr);
void* memr_realloc(void *ptr,u32 newsize);
void* memr_reallocm(void *ptr,u32 newsize);

#endif
