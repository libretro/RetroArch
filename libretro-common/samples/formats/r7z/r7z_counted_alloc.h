/* Forced into r7z_archive.c for r7z_bcj2_test: its allocations are
 * counted, so the test can see what an extraction holds at once. */
#ifndef R7Z_COUNTED_ALLOC_H
#define R7Z_COUNTED_ALLOC_H
#include <stdlib.h>
void *counted_malloc(size_t n);
void *counted_calloc(size_t n, size_t size);
void *counted_realloc(void *p, size_t n);
void  counted_free(void *p);
#define malloc  counted_malloc
#define calloc  counted_calloc
#define realloc counted_realloc
#define free    counted_free
#endif
