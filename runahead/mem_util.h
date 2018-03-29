#ifndef __MEM_UTIL__
#define __MEM_UTIL__

#include <stddef.h>
#include <string.h>

#include <retro_common_api.h>
#include <boolean.h>

#define FREE(xxxx) if ((xxxx) != NULL) { free((void*)(xxxx)); } (xxxx) = NULL

RETRO_BEGIN_DECLS

char *strcpy_alloc(const char *sourceStr);
char *strcpy_alloc_force(const char *sourceStr);
void strcat_alloc(char ** destStr_p, const char *appendStr);
void *memcpy_alloc(const void *src, size_t size);

RETRO_END_DECLS

#endif
