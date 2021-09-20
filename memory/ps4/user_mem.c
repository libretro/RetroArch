#include <stdlib.h>
#include <kernel.h>
#include <mspace.h>
#include <defines/ps4_defines.h>
#include "user_mem.h"

static OrbisMspace s_mspace = 0;
static OrbisMallocManagedSize s_mmsize;
static void *s_mem_start = 0;
static size_t s_mem_size = MEM_SIZE;

int malloc_init(void)
{
  int res;

  if (s_mspace)
    return 0;

  res = sceKernelReserveVirtualRange(&s_mem_start, MEM_SIZE, 0, MEM_ALIGN);
  if (res < 0)
    return 1;

  res = sceKernelMapNamedSystemFlexibleMemory(&s_mem_start, MEM_SIZE, SCE_KERNEL_PROT_CPU_RW, SCE_KERNEL_MAP_FIXED, "User Mem");
  if (res < 0)
    return 1;

  s_mspace = sceLibcMspaceCreate("User Mspace", s_mem_start, s_mem_size, 0);
  if (!s_mspace)
    return 1;

  s_mmsize.sz = sizeof(s_mmsize);
  s_mmsize.ver = 1;
  res = sceLibcMspaceMallocStatsFast(s_mspace, &s_mmsize);
  return 0;
}

int malloc_finalize(void)
{
  int res;

  if (s_mspace)
  {
    res = sceLibcMspaceDestroy(s_mspace);
    if (res != 0)
      return 1;
  }

  res = sceKernelReleaseFlexibleMemory(s_mem_start, s_mem_size);
  if (res < 0)
    return 1;

  res = sceKernelMunmap(s_mem_start, s_mem_size);
  if (res < 0)
    return 1;

  return 0;
}

void *malloc(size_t size)
{
  if (!s_mspace)
    malloc_init();

  return sceLibcMspaceMalloc(s_mspace, size);
}

void free(void *ptr)
{

  if (!ptr || !s_mspace)
    return;

    sceLibcMspaceFree(s_mspace, ptr);
}

void *calloc(size_t nelem, size_t size)
{
  if (!s_mspace)
    malloc_init();

  return sceLibcMspaceCalloc(s_mspace, nelem, size);
}

void *realloc(void *ptr, size_t size)
{
  if (!s_mspace)
    malloc_init();

  return sceLibcMspaceRealloc(s_mspace, ptr, size);
}

void *memalign(size_t boundary, size_t size)
{
  if (!s_mspace)
    malloc_init();

  return sceLibcMspaceMemalign(s_mspace, boundary, size);
}

int posix_memalign(void **ptr, size_t boundary, size_t size)
{
  if (!s_mspace)
    malloc_init();

  return sceLibcMspacePosixMemalign(s_mspace, ptr, boundary, size);
}

void *reallocalign(void *ptr, size_t size, size_t boundary)
{
  if (!s_mspace)
    malloc_init();

  return sceLibcMspaceReallocalign(s_mspace, ptr, boundary, size);
}

int malloc_stats(OrbisMallocManagedSize *mmsize)
{
  if (!s_mspace)
    malloc_init();

  return sceLibcMspaceMallocStats(s_mspace, mmsize);
}

int malloc_stats_fast(OrbisMallocManagedSize *mmsize)
{
  if (!s_mspace)
    malloc_init();

  return sceLibcMspaceMallocStatsFast(s_mspace, mmsize);
}

size_t malloc_usable_size(void *ptr)
{
	if (!ptr)
		return 0;

  return sceLibcMspaceMallocUsableSize(ptr);
}

int vasprintf(char **bufp, const char *format, va_list ap)
{
  va_list ap1;
  int bytes;
  char *p;

  va_copy(ap1, ap);

  bytes = vsnprintf(NULL, 0, format, ap1) + 1;
  va_end(ap1);

  *bufp = p = malloc(bytes);
  if (!p)
    return -1;

  return vsnprintf(p, bytes, format, ap);
}

int asprintf(char **bufp, const char *format, ...)
{
  va_list ap, ap1;
  int rv;
  int bytes;
  char *p;

  va_start(ap, format);
  va_copy(ap1, ap);

  bytes = vsnprintf(NULL, 0, format, ap1) + 1;
  va_end(ap1);

  *bufp = p = malloc(bytes);
  if (!p)
    return -1;

  rv = vsnprintf(p, bytes, format, ap);
  va_end(ap);

  return rv;
}

char *strdup(const char *s)
{
  size_t len = strlen(s) + 1;
  void *new_s = malloc(sizeof(char) * len);

  if (!new_s)
    return NULL;

  return (char *)memcpy(new_s, s, len);
}

char *strndup(const char *s, size_t n)
{
  if (!s)
    return NULL;

  char *result;
  size_t len = strnlen(s, n);

  result = (char *)malloc(sizeof(char) * (len + 1));
  if (!result)
    return 0;

  result[len] = '\0';
  return (char *)memcpy(result, s, len);
}

void get_user_mem_size(size_t *max_mem, size_t *cur_mem)
{
  int res;
  size_t size;

  s_mmsize.sz = sizeof(s_mmsize);
  s_mmsize.ver = 1;
  res = sceLibcMspaceMallocStatsFast(s_mspace, &s_mmsize);
  *max_mem += s_mmsize.curSysSz;
  *cur_mem += s_mmsize.curSysSz - s_mmsize.curUseSz;
}

