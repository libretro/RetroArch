/* DOL and ELF program images. */

#include "exec_image.h"

#define ARGV_MAGIC 0x5f617267u

static uint32_t be32(const uint8_t *p)
{
   return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
        | ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

static uint32_t be16(const uint8_t *p)
{
   return ((uint32_t)p[0] << 8) | p[1];
}

/* Physical, or a cached virtual address, as a cached address. */
static uint32_t cached(uint32_t a)
{
   return (a & 0x3fffffffu) | 0x80000000u;
}

static int add(struct gk_exec_plan *plan, size_t image_len,
      const struct gk_exec_mem *mem, uint32_t dst, uint32_t src,
      uint32_t len, uint32_t size)
{
   struct gk_exec_seg *s;
   uint32_t end;
   if (!size)
      return 0;
   if (len > size || src > image_len || len > image_len - src
         || plan->count == GK_EXEC_SEGS)
      return -1;
   dst = cached(dst);
   /* What falls below the program area (ELF headers loaded with the
    * first segment) stays out of the system's low memory. */
   if (dst < mem->mem1_lo && size > mem->mem1_lo - dst)
   {
      uint32_t cut = mem->mem1_lo - dst;
      dst  += cut;
      src  += len > cut ? cut : len;
      len   = len > cut ? len - cut : 0;
      size -= cut;
   }
   end = dst + size;
   if (end < dst)
      return -1;
   if (!(dst >= mem->mem1_lo && end <= mem->mem1_hi)
         && !(mem->mem2_hi && dst >= mem->mem2_lo && end <= mem->mem2_hi))
      return -1;
   s       = &plan->seg[plan->count++];
   s->dst  = dst;
   s->src  = src;
   s->len  = len;
   s->size = size;
   return 0;
}

static int parse_elf(const uint8_t *img, size_t len,
      const struct gk_exec_mem *mem, struct gk_exec_plan *plan)
{
   uint32_t phoff, phentsize, phnum, i;
   if (len < 52 || img[4] != 1 || img[5] != 2 || be16(img + 16) != 2
         || be16(img + 18) != 20)   /* 32-bit, big-endian, EXEC, PowerPC */
      return -1;
   plan->entry = cached(be32(img + 24));
   phoff       = be32(img + 28);
   phentsize   = be16(img + 42);
   phnum       = be16(img + 44);
   if (phentsize < 32 || phoff > len || phnum > (len - phoff) / phentsize)
      return -1;
   for (i = 0; i < phnum; i++)
   {
      const uint8_t *ph = img + phoff + i * phentsize;
      uint32_t paddr    = be32(ph + 12);
      if (be32(ph) != 1)            /* PT_LOAD */
         continue;
      if (add(plan, len, mem, paddr ? paddr : be32(ph + 8), be32(ph + 4),
               be32(ph + 16), be32(ph + 20)))
         return -1;
   }
   return 0;
}

static int parse_dol(const uint8_t *img, size_t len,
      const struct gk_exec_mem *mem, struct gk_exec_plan *plan)
{
   unsigned i;
   if (len < 0x100)
      return -1;
   /* .bss first: sections loaded over it keep their contents. */
   if (add(plan, len, mem, be32(img + 0xd8), 0, 0, be32(img + 0xdc)))
      return -1;
   for (i = 0; i < 18; i++)
   {
      uint32_t size = be32(img + 0x90 + i * 4);
      if (size && add(plan, len, mem, be32(img + 0x48 + i * 4),
               be32(img + i * 4), size, size))
         return -1;
   }
   plan->entry = cached(be32(img + 0xe0));
   return 0;
}

int gk_exec_parse(const uint8_t *image, size_t len,
      const struct gk_exec_mem *mem, struct gk_exec_plan *plan)
{
   uint32_t i;
   int ret;
   plan->count = 0;
   if (len >= 4 && be32(image) == 0x7f454c46u)
      ret = parse_elf(image, len, mem, plan);
   else
      ret = parse_dol(image, len, mem, plan);
   if (ret || !plan->count)
      return -1;
   for (i = 0; i < plan->count; i++)
   {
      const struct gk_exec_seg *s = &plan->seg[i];
      if (plan->entry >= s->dst && plan->entry < s->dst + s->len)
         return 0;
   }
   return -1;
}

uint32_t gk_exec_argv_offset(const uint8_t *image,
      const struct gk_exec_plan *plan)
{
   uint32_t i;
   for (i = 0; i < plan->count; i++)
   {
      const struct gk_exec_seg *s = &plan->seg[i];
      /* The tag, then six words. */
      if (plan->entry >= s->dst && plan->entry - s->dst <= s->len
            && s->len - (plan->entry - s->dst) >= 32)
      {
         uint32_t off = s->src + (plan->entry - s->dst);
         return be32(image + off + 4) == ARGV_MAGIC ? off + 8 : 0;
      }
   }
   return 0;
}
