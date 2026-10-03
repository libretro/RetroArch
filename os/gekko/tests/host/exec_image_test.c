/* kernel/exec_image.c: where DOL and ELF programs load, and what is
 * refused. */

#include <stdio.h>
#include <string.h>

#include "../../kernel/exec_image.h"

static unsigned failures;

#define CHECK(cond, what) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s\n", what); \
         failures++; \
      } \
   } while (0)

static const struct gk_exec_mem wii = {
   0x80003100u, 0x81800000u, 0x90000800u, 0x93300000u };
static const struct gk_exec_mem gc = {
   0x80003100u, 0x81800000u, 0, 0 };

static uint8_t img[0x8000];

static void put32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v >> 24);
   p[1] = (uint8_t)(v >> 16);
   p[2] = (uint8_t)(v >> 8);
   p[3] = (uint8_t)v;
}

static void put16(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v >> 8);
   p[1] = (uint8_t)v;
}

/* A DOL: text 0 at 0x80004000 (0x200 bytes), data 0 at 0x80005000
 * (0x100), .bss 0x80004f00-0x80006000 over the data section. */
static size_t dol(void)
{
   memset(img, 0, sizeof(img));
   put32(img + 0x00, 0x100);
   put32(img + 0x48, 0x80004000u);
   put32(img + 0x90, 0x200);
   put32(img + 0x1c, 0x300);
   put32(img + 0x64, 0x80005000u);
   put32(img + 0xac, 0x100);
   put32(img + 0xd8, 0x80004f00u);
   put32(img + 0xdc, 0x1100);
   put32(img + 0xe0, 0x80004000u);
   put32(img + 0x104, 0x5f617267u);   /* the '_arg' tag at entry + 4 */
   return 0x400;
}

/* An ELF: one segment from file offset 0 at 0x80000000 (headers
 * included, as devkitPPC links), entry 0x80004000; a second in MEM2. */
static size_t elf(uint32_t seg2_paddr)
{
   uint8_t *ph = img + 52;
   memset(img, 0, sizeof(img));
   put32(img, 0x7f454c46u);
   img[4] = 1;
   img[5] = 2;
   put16(img + 16, 2);
   put16(img + 18, 20);
   put32(img + 24, 0x80004000u);
   put32(img + 28, 52);
   put16(img + 42, 32);
   put16(img + 44, 2);
   put32(ph, 1);
   put32(ph + 4, 0);
   put32(ph + 8, 0x80000000u);
   put32(ph + 12, 0x00000000u);
   put32(ph + 16, 0x5000);
   put32(ph + 20, 0x8000);
   put32(ph + 32, 1);
   put32(ph + 36, 0x5000);
   put32(ph + 40, 0x90100000u);
   put32(ph + 44, seg2_paddr);
   put32(ph + 48, 0x800);
   put32(ph + 52, 0x800);
   return 0x5800;
}

int main(void)
{
   struct gk_exec_plan p;
   size_t len;

   len = dol();
   CHECK(!gk_exec_parse(img, len, &wii, &p), "dol: parses");
   CHECK(p.entry == 0x80004000u && p.count == 3, "dol: entry, segments");
   CHECK(p.seg[0].dst == 0x80004f00u && p.seg[0].len == 0
         && p.seg[0].size == 0x1100, "dol: .bss cleared first");
   CHECK(p.seg[1].dst == 0x80004000u && p.seg[1].src == 0x100
         && p.seg[1].len == 0x200, "dol: text");
   CHECK(p.seg[2].dst == 0x80005000u && p.seg[2].src == 0x300,
         "dol: data loads over .bss");
   CHECK(gk_exec_argv_offset(img, &p) == 0x108, "dol: argument block");
   put32(img + 0x104, 0);
   CHECK(gk_exec_argv_offset(img, &p) == 0, "dol: no tag, no arguments");
   CHECK(gk_exec_parse(img, 0x3ff, &wii, &p), "dol: cut-off image");
   dol();
   put32(img + 0x48, 0x01000000u);   /* physical: still MEM1 */
   put32(img + 0xe0, 0x01000000u);
   CHECK(!gk_exec_parse(img, len, &wii, &p) && p.seg[1].dst == 0x81000000u
         && p.entry == 0x81000000u, "dol: physical addresses");
   dol();
   put32(img + 0x48, 0x81800000u - 0x100);
   CHECK(gk_exec_parse(img, len, &wii, &p), "dol: past the end of MEM1");
   dol();
   put32(img + 0xe0, 0x80008000u);
   CHECK(gk_exec_parse(img, len, &wii, &p), "dol: entry nowhere");
   memset(img, 0, sizeof(img));
   CHECK(gk_exec_parse(img, 0x100, &wii, &p), "dol: nothing to load");

   len = elf(0x10100000u);
   CHECK(!gk_exec_parse(img, len, &wii, &p), "elf: parses");
   CHECK(p.count == 2 && p.seg[0].dst == 0x80003100u
         && p.seg[0].src == 0x3100 && p.seg[0].len == 0x5000 - 0x3100
         && p.seg[0].size == 0x8000 - 0x3100,
         "elf: headers stay out of low memory");
   CHECK(p.seg[1].dst == 0x90100000u && p.seg[1].src == 0x5000,
         "elf: MEM2 from the physical address");
   CHECK(gk_exec_parse(img, len, &gc, &p), "elf: no MEM2 on a GameCube");
   elf(0);
   CHECK(!gk_exec_parse(img, len, &wii, &p) && p.seg[1].dst == 0x90100000u,
         "elf: virtual address without a physical one");
   elf(0x10100000u);
   put32(img + 52 + 32 + 20, 0x400);  /* memsz < filesz */
   CHECK(gk_exec_parse(img, len, &wii, &p), "elf: more file than memory");
   elf(0x10100000u);
   put32(img + 52 + 32 + 4, 0x5400);  /* past the end of the file */
   CHECK(gk_exec_parse(img, len, &wii, &p), "elf: data past the file");
   elf(0x10100000u);
   put16(img + 44, 0x7fff);
   CHECK(gk_exec_parse(img, len, &wii, &p), "elf: headers past the file");
   elf(0x10100000u);
   img[5] = 1;
   CHECK(gk_exec_parse(img, len, &wii, &p), "elf: little-endian");
   elf(0x10100000u);
   put32(img + 52 + 16, 0x3800);      /* the entry point is in .bss */
   CHECK(gk_exec_parse(img, len, &wii, &p), "elf: entry not loaded");

   printf("exec_image: %s\n", failures ? "FAILED" : "ok");
   return failures ? 1 : 0;
}
