#ifndef __MEM_UTIL__
#define __MEM_UTIL__

#include <stddef.h>
#include <string.h>

#include <boolean.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

char *strcpy_alloc(const char *sourceStr);
char *strcpy_alloc_force(const char *sourceStr);
void strcat_alloc(char ** destStr_p, const char *appendStr);

RETRO_END_DECLS

#endif
