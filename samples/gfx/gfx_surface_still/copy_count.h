#ifndef GFX_SURFACE_STILL_COPY_COUNT_H
#define GFX_SURFACE_STILL_COPY_COUNT_H

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <stddef.h>
#include <string.h>

void *gfx_surface_still_copy(void *dest, const void *src, size_t size);
#define memcpy gfx_surface_still_copy

#endif
