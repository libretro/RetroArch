#ifndef STUB_SYSCTL_H
#define STUB_SYSCTL_H
#include <stddef.h>
#define CTL_HW 6
#define HW_MEMSIZE 24
#define HW_PHYSMEM 5
int sysctl(int *name, unsigned namelen, void *old, size_t *oldlen,
      void *newp, size_t newlen);
#endif
