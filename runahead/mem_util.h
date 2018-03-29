#ifndef __MEM_UTIL__
#define __MEM_UTIL__

#include "retro_common_api.h"
#include "boolean.h"

#include <malloc.h>
#include <stddef.h>
#include <string.h>

#define FREE(xxxx) if ((xxxx) != NULL) { free((void*)(xxxx)); } (xxxx) = NULL

RETRO_BEGIN_DECLS

void *malloc_zero(size_t size);
void free_str(char **str_p);
void free_ptr(void **data_p);
char *strcpy_alloc(const char *sourceStr);
char *strcpy_alloc_force(const char *sourceStr);
void strcat_alloc(char ** destStr_p, const char *appendStr);
void *memcpy_alloc(const void *src, size_t size);

RETRO_END_DECLS

#endif

