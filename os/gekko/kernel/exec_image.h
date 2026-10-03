/* Where a DOL or ELF program goes in memory; portable, so the host
 * tests can check it. */

#ifndef GEKKO_EXEC_IMAGE_H
#define GEKKO_EXEC_IMAGE_H

#include <stddef.h>
#include <stdint.h>

#define GK_EXEC_SEGS 18

/* len bytes from src, then zeros up to size, at dst.  The parser
 * leaves src as an offset into the image; the loader makes it an
 * address. */
struct gk_exec_seg
{
   uint32_t dst;
   uint32_t src;
   uint32_t len;
   uint32_t size;
};

struct gk_exec_plan
{
   uint32_t           entry;
   uint32_t           count;
   struct gk_exec_seg seg[GK_EXEC_SEGS];
};

/* Memory a program may load into: [lo, hi) of each bank. */
struct gk_exec_mem
{
   uint32_t mem1_lo, mem1_hi;
   uint32_t mem2_lo, mem2_hi;   /* both 0 on the GameCube */
};

/* 0, or -1 for something that is not a DOL or ELF program, does not
 * fit in mem, or has its entry point outside what it loads. */
int gk_exec_parse(const uint8_t *image, size_t len,
      const struct gk_exec_mem *mem, struct gk_exec_plan *plan);

/* The offset in the image of the argument block after the '_arg' tag
 * at the entry point, or 0 if the program has none. */
uint32_t gk_exec_argv_offset(const uint8_t *image,
      const struct gk_exec_plan *plan);

#endif
