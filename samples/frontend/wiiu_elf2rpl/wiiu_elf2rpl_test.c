/* os/wiiu/wut/elf2rpl on a linked Wii U ELF in the shape a -pie link
 * with os/wiiu/link_rpl.ld gives it: .rela.text and .rela.data linked
 * to .dynsym, a resolved target as symbol 0 plus its absolute address,
 * and a weak reference that nothing defined (devkitPPC's libc and
 * libstdc++ make several, to OS hooks the runtime may provide) as its
 * .dynsym entry, still undefined.
 *
 * The RPL elf2rpl writes must carry no relocation against an undefined
 * symbol - the Cafe OS loader cannot resolve one, and a 24-bit branch
 * to address zero is out of reach from the code area - while every
 * relocation against a resolved target survives, and the instruction
 * the link wrote at the weak branch is left as it was.
 *
 * Every section's entry in SHT_RPL_CRCS must be the zlib CRC-32 of its
 * contents, which the loader checks; and a truncated ELF, or an option
 * the tool does not know, must end in its error, not a crash. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif

#ifndef ELF2RPL
#define ELF2RPL "./wiiu_elf2rpl_tool"
#endif

#define IN_PATH    "wiiu_elf2rpl_in.tmp"
#define OUT_PATH   "wiiu_elf2rpl_out.tmp"
#define TRUNC_PATH "wiiu_elf2rpl_trunc.tmp"

#define CODE 0x02000000u
#define DATA 0x10000000u

#define R_PPC_ADDR32    1
#define R_PPC_ADDR16_LO 4
#define R_PPC_ADDR16_HA 6
#define R_PPC_REL24     10

static int failures;

#define CHECK(cond) do { if (!(cond)) { \
   fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
   failures++; } } while (0)

static void put16(unsigned char *p, unsigned v)
{
   p[0] = (unsigned char)(v >> 8);
   p[1] = (unsigned char)v;
}

static void put32(unsigned char *p, unsigned long v)
{
   p[0] = (unsigned char)(v >> 24);
   p[1] = (unsigned char)(v >> 16);
   p[2] = (unsigned char)(v >> 8);
   p[3] = (unsigned char)v;
}

static unsigned get16(const unsigned char *p)
{
   return ((unsigned)p[0] << 8) | p[1];
}

static unsigned long get32(const unsigned char *p)
{
   return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16)
        | ((unsigned long)p[2] << 8) | p[3];
}

/* Section indices of the input. */
enum { S_NULL, S_TEXT, S_DATA, S_RELTEXT, S_RELDATA, S_SYMTAB, S_STRTAB,
       S_DYNSYM, S_DYNSTR, S_SHSTR, S_COUNT };

static unsigned char image[4096];

static void sym(unsigned char *p, unsigned long name, unsigned long value,
      unsigned long size, unsigned info, unsigned shndx)
{
   put32(p, name);
   put32(p + 4, value);
   put32(p + 8, size);
   p[12] = (unsigned char)info;
   p[13] = 0;
   put16(p + 14, shndx);
}

static void rela(unsigned char *p, unsigned long off, unsigned long symidx,
      unsigned type, unsigned long addend)
{
   put32(p, off);
   put32(p + 4, (symidx << 8) | type);
   put32(p + 8, addend);
}

static int write_input(void)
{
   static const char shstr[] = "\0.text\0.data\0.rela.text\0.rela.data\0"
         ".symtab\0.strtab\0.dynsym\0.dynstr\0.shstrtab";
   static const char strtab[] = "\0main\0table\0hook";
   static const unsigned long shname[S_COUNT] =
         { 0, 1, 7, 13, 24, 35, 43, 51, 59, 67 };
   static const unsigned long type[S_COUNT] =
         { 0, 1, 1, 4, 4, 2, 3, 11, 3, 3 };
   static const unsigned long flags[S_COUNT] =
         { 0, 6, 3, 0, 0, 0, 0, 2, 2, 0 };
   unsigned long off[S_COUNT], size[S_COUNT], addr[S_COUNT];
   unsigned long link[S_COUNT], info[S_COUNT], ent[S_COUNT];
   unsigned long pos = 0x40, shoff;
   unsigned char *p;
   FILE *f;
   int i;

   memset(image, 0, sizeof(image));
   memset(addr, 0, sizeof(addr));
   memset(link, 0, sizeof(link));
   memset(info, 0, sizeof(info));
   memset(ent, 0, sizeof(ent));

   /* .text: hook's address and a branch to it, as the link left them
    * for a weak reference nothing defined, then table's address. */
   off[S_TEXT] = pos; size[S_TEXT] = 24; addr[S_TEXT] = CODE;
   p = image + pos;
   put32(p,      0x3d200000ul);   /* lis   r9,hook@ha      */
   put32(p + 4,  0x39290000ul);   /* addi  r9,r9,hook@l    */
   put32(p + 8,  0x48000000ul);   /* b     .  (hook)       */
   put32(p + 12, 0x3d401000ul);   /* lis   r10,table@ha    */
   put32(p + 16, 0x394a0000ul);   /* addi  r10,r10,table@l */
   put32(p + 20, 0x4e800020ul);   /* blr                   */
   pos += 32;

   /* .data: &hook, &main. */
   off[S_DATA] = pos; size[S_DATA] = 8; addr[S_DATA] = DATA;
   put32(image + pos, 0);
   put32(image + pos + 4, CODE);
   pos += 8;

   /* .rela.text: the weak ones against .dynsym entry 1, the resolved
    * ones as symbol 0 plus the absolute target. */
   off[S_RELTEXT] = pos; size[S_RELTEXT] = 5 * 12; ent[S_RELTEXT] = 12;
   link[S_RELTEXT] = S_DYNSYM; info[S_RELTEXT] = S_TEXT;
   rela(image + pos,      CODE + 2,  1, R_PPC_ADDR16_HA, 0);
   rela(image + pos + 12, CODE + 6,  1, R_PPC_ADDR16_LO, 0);
   rela(image + pos + 24, CODE + 8,  1, R_PPC_REL24,     0);
   rela(image + pos + 36, CODE + 14, 0, R_PPC_ADDR16_HA, DATA);
   rela(image + pos + 48, CODE + 18, 0, R_PPC_ADDR16_LO, DATA);
   pos += 5 * 12;

   off[S_RELDATA] = pos; size[S_RELDATA] = 2 * 12; ent[S_RELDATA] = 12;
   link[S_RELDATA] = S_DYNSYM; info[S_RELDATA] = S_DATA;
   rela(image + pos,      DATA,     1, R_PPC_ADDR32, 0);
   rela(image + pos + 12, DATA + 4, 0, R_PPC_ADDR32, CODE);
   pos += 2 * 12;

   /* .symtab: main, table and the undefined weak hook. */
   off[S_SYMTAB] = pos; size[S_SYMTAB] = 4 * 16; ent[S_SYMTAB] = 16;
   link[S_SYMTAB] = S_STRTAB; info[S_SYMTAB] = 1;
   sym(image + pos + 16, 1,  CODE, 24, 0x12, S_TEXT);  /* GLOBAL FUNC   */
   sym(image + pos + 32, 6,  DATA, 8,  0x11, S_DATA);  /* GLOBAL OBJECT */
   sym(image + pos + 48, 12, 0,    0,  0x22, 0);       /* WEAK FUNC, undefined */
   pos += 4 * 16;

   off[S_STRTAB] = pos; size[S_STRTAB] = sizeof(strtab);
   memcpy(image + pos, strtab, sizeof(strtab));
   pos += (sizeof(strtab) + 3) & ~3ul;

   /* .dynsym: the weak hook alone, as the -pie link exports it. */
   off[S_DYNSYM] = pos; size[S_DYNSYM] = 2 * 16; ent[S_DYNSYM] = 16;
   link[S_DYNSYM] = S_DYNSTR; info[S_DYNSYM] = 1;
   sym(image + pos + 16, 1, 0, 0, 0x22, 0);
   pos += 2 * 16;

   off[S_DYNSTR] = pos; size[S_DYNSTR] = 6;
   memcpy(image + pos, "\0hook", 6);
   pos += 8;

   off[S_SHSTR] = pos; size[S_SHSTR] = sizeof(shstr);
   memcpy(image + pos, shstr, sizeof(shstr));
   pos += (sizeof(shstr) + 3) & ~3ul;

   off[S_NULL] = 0; size[S_NULL] = 0;
   shoff = pos;
   for (i = 0; i < S_COUNT; i++)
   {
      p = image + shoff + (unsigned long)i * 40;
      if (i == S_NULL)
         continue;
      put32(p,      shname[i]);
      put32(p + 4,  type[i]);
      put32(p + 8,  flags[i]);
      put32(p + 12, addr[i]);
      put32(p + 16, off[i]);
      put32(p + 20, size[i]);
      put32(p + 24, link[i]);
      put32(p + 28, info[i]);
      put32(p + 32, (type[i] == 1) ? 32 : 4);
      put32(p + 36, ent[i]);
   }
   pos = shoff + S_COUNT * 40;

   /* ELF32, big-endian, PowerPC, a position-independent executable. */
   memcpy(image, "\177ELF", 4);
   image[4] = 1; image[5] = 2; image[6] = 1;
   put16(image + 16, 3);
   put16(image + 18, 20);
   put32(image + 20, 1);
   put32(image + 24, CODE);
   put32(image + 32, shoff);
   put16(image + 40, 52);
   put16(image + 46, 40);
   put16(image + 48, S_COUNT);
   put16(image + 50, S_SHSTR);

   if (!(f = fopen(IN_PATH, "wb")))
      return 0;
   fwrite(image, 1, pos, f);
   fclose(f);
   return 1;
}

static unsigned char *read_all(const char *path, long *len)
{
   FILE *f = fopen(path, "rb");
   unsigned char *buf;
   if (!f)
      return NULL;
   fseek(f, 0, SEEK_END);
   *len = ftell(f);
   fseek(f, 0, SEEK_SET);
   buf = (unsigned char*)malloc((size_t)*len + 1);
   if (buf && fread(buf, 1, (size_t)*len, f) != (size_t)*len)
   {
      free(buf);
      buf = NULL;
   }
   fclose(f);
   return buf;
}

/* zlib's CRC-32, a bit at a time, to check the tool's against */
static unsigned long crc32_ref(const unsigned char *p, unsigned long n)
{
   unsigned long crc = 0xFFFFFFFFul;
   while (n--)
   {
      int k;
      crc ^= *p++;
      for (k = 0; k < 8; k++)
         crc = (crc >> 1) ^ (0xEDB88320ul & (0ul - (crc & 1ul)));
   }
   return crc ^ 0xFFFFFFFFul;
}

/* The tool gave up with its own error (exit -1), rather than crashing
 * or being stopped by a sanitizer. */
static int failed_cleanly(int rc)
{
#ifdef _WIN32
   return rc == -1 || rc == 255;
#else
   return rc != -1 && WIFEXITED(rc) && WEXITSTATUS(rc) == 255;
#endif
}

static int bad_input_rejected(const unsigned char *elf, long elf_len)
{
   static const long cuts[] = { 0, 4, 20, 51, 52, 60, 200, 400, 1000, -1 };
   char cmd[256];
   int ok = 1, c;

   for (c = 0; cuts[c] >= 0; c++)
   {
      long  cut = cuts[c] < elf_len ? cuts[c] : elf_len - 1;
      FILE *f   = fopen(TRUNC_PATH, "wb");
      int   rc;
      if (!f)
         return 0;
      if (cut && fwrite(elf, 1, (size_t)cut, f) != (size_t)cut)
         ok = 0;
      fclose(f);
      sprintf(cmd, "%s %s %s", ELF2RPL, TRUNC_PATH, OUT_PATH);
      rc = system(cmd);
      if (!failed_cleanly(rc))
      {
         fprintf(stderr, "ELF cut to %ld bytes: elf2rpl returned %d\n", cut, rc);
         ok = 0;
      }
   }
   remove(TRUNC_PATH);
   remove(OUT_PATH);

   sprintf(cmd, "%s -x %s %s", ELF2RPL, IN_PATH, OUT_PATH);
   if (!failed_cleanly(system(cmd)))
   {
      fprintf(stderr, "elf2rpl took an unknown option\n");
      ok = 0;
   }
   remove(OUT_PATH);
   return ok;
}

int main(void)
{
   unsigned char *rpl, *elf;
   long len = 0, elf_len = 0;
   unsigned long shoff, crc_off = 0, crc_count = 0;
   unsigned shnum, i;
   int kept = 0, undef = 0, rel24 = 0, branch_ok = 0, crc_bad = 0;
   char cmd[256];

   if (!write_input())
   {
      fprintf(stderr, "FAIL could not write %s\n", IN_PATH);
      return 1;
   }
   sprintf(cmd, "%s %s %s", ELF2RPL, IN_PATH, OUT_PATH);
   remove(OUT_PATH);
   CHECK(system(cmd) == 0);
   if (!(rpl = read_all(OUT_PATH, &len)))
   {
      fprintf(stderr, "FAIL elf2rpl wrote no RPL\n");
      remove(IN_PATH);
      return 1;
   }

   shoff = get32(rpl + 32);
   shnum = get16(rpl + 48);
   for (i = 0; i < shnum; i++)
   {
      const unsigned char *sh = rpl + shoff + (unsigned long)i * 40;
      unsigned long type = get32(sh + 4);
      unsigned long sh_addr = get32(sh + 12);
      unsigned long sh_off  = get32(sh + 16);
      unsigned long sh_size = get32(sh + 20);
      unsigned long sh_link = get32(sh + 24);

      if (type == 0x80000003ul)
      {
         crc_off   = sh_off;
         crc_count = sh_size / 4;
      }

      /* The weak branch the link wrote, still as it was. */
      if (type == 1 && sh_addr == CODE && sh_size >= 12)
         branch_ok = get32(rpl + sh_off + 8) == 0x48000000ul;

      if (type == 4)
      {
         const unsigned char *symsh = rpl + shoff + sh_link * 40;
         unsigned long symoff = get32(symsh + 16);
         unsigned long n = sh_size / 12, r;
         for (r = 0; r < n; r++)
         {
            const unsigned char *re = rpl + sh_off + r * 12;
            unsigned long rinfo = get32(re + 4);
            unsigned long idx   = rinfo >> 8;
            unsigned shndx      = get16(rpl + symoff + idx * 16 + 14);
            kept++;
            if ((rinfo & 0xff) == R_PPC_REL24)
               rel24++;
            if (idx == 0 || shndx == 0)
            {
               fprintf(stderr, "relocation at 0x%08lx type %lu against an "
                     "undefined symbol\n", get32(re), rinfo & 0xff);
               undef++;
            }
         }
      }
   }

   CHECK(undef == 0);
   CHECK(rel24 == 0);
   CHECK(kept == 3);        /* table@ha, table@l and &main survive */
   CHECK(branch_ok);

   /* One CRC per section, its own and empty ones zero */
   CHECK(crc_count == shnum);
   for (i = 0; i < shnum && crc_count == shnum; i++)
   {
      const unsigned char *sh = rpl + shoff + (unsigned long)i * 40;
      unsigned long type = get32(sh + 4);
      unsigned long size = get32(sh + 20);
      unsigned long want = (type == 0x80000003ul || type == 8 || !size)
         ? 0 : crc32_ref(rpl + get32(sh + 16), size);
      if (get32(rpl + crc_off + i * 4) != want)
      {
         fprintf(stderr, "section %u CRC %08lx, want %08lx\n", i,
               get32(rpl + crc_off + i * 4), want);
         crc_bad++;
      }
   }
   CHECK(crc_bad == 0);
   free(rpl);

   CHECK((elf = read_all(IN_PATH, &elf_len)) != NULL);
   if (elf)
   {
      CHECK(bad_input_rejected(elf, elf_len));
      free(elf);
   }

   remove(IN_PATH);
   remove(OUT_PATH);
   if (failures)
   {
      fprintf(stderr, "%d failure(s)\n", failures);
      return 1;
   }
   printf("wiiu_elf2rpl: ok\n");
   return 0;
}
