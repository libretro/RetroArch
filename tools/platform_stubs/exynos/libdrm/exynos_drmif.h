#ifndef EXYNOS_DRMIF_STUB_H
#define EXYNOS_DRMIF_STUB_H
/* Stand-in for libdrm's exynos_drmif.h: the types and prototypes
 * gfx/drivers/exynos_gfx.c uses, in the shapes libdrm gives them, so
 * the driver is compiled - and its calls are checked - where libdrm is
 * built without its exynos module. */
#include <stddef.h>
#include <stdint.h>

struct exynos_device
{
   int fd;
};

struct exynos_bo
{
   struct exynos_device *dev;
   uint32_t handle;
   uint32_t flags;
   size_t size;
   void *vaddr;
   uint32_t name;
};

struct exynos_device *exynos_device_create(int fd);
void exynos_device_destroy(struct exynos_device *dev);
struct exynos_bo *exynos_bo_create(struct exynos_device *dev,
      size_t size, uint32_t flags);
void exynos_bo_destroy(struct exynos_bo *bo);
void *exynos_bo_map(struct exynos_bo *bo);

#endif
