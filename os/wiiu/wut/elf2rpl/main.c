/* elf2rpl: turns the Wii U ELF that Makefile.wiiu links (-pie, with
 * os/wiiu/link_rpl.ld) into an RPL the Cafe OS loader takes.
 *
 * The input is read once into memory; its section contents are used
 * where they lie, patched in place for the .rela.dyn relative
 * relocations, and the output is laid out in one buffer and written
 * with one call. Every multi-byte field of both files is big-endian
 * and goes through retro_endianness.h; section CRCs and import
 * signatures through encoding_crc32().
 *
 * Usage: elf2rpl [-v ...] [--] <src> <dst>
 * Each -v raises the detail printed while converting. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#include <boolean.h>
#include <retro_inline.h>
#include <retro_endianness.h>
#include <encodings/crc32.h>

#define LOAD_ADDRESS      0x01000000u
#define CODE_ADDRESS      0x02000000u
#define DATA_ADDRESS      0x10000000u
#define WIIU_LOAD_ADDRESS 0xC0000000u

#define EM_PPC            20
#define ELFCLASS32        1
#define ELFDATA2MSB       2
#define EV_CURRENT        1
#define EABI_CAFE         0xCAFE
#define ELF_MAGIC         0x7F454C46u

#define SHT_NULL          0u
#define SHT_PROGBITS      1u
#define SHT_SYMTAB        2u
#define SHT_STRTAB        3u
#define SHT_RELA          4u
#define SHT_NOBITS        8u
#define SHT_RPL_IMPORTS   0x80000002u
#define SHT_RPL_CRCS      0x80000003u
#define SHT_RPL_FILEINFO  0x80000004u

#define SHF_WRITE         0x1u
#define SHF_ALLOC         0x2u
#define SHF_EXECINSTR     0x4u

#define SHN_UNDEF         0

#define STT_NOTYPE        0
#define STT_OBJECT        1
#define STT_FUNC          2
#define STT_SECTION       3
#define STT_FILE          4

#define STB_LOCAL         0
#define STB_GLOBAL        1

#define R_PPC_NONE        0
#define R_PPC_ADDR32      1
#define R_PPC_RELATIVE    22

#define RPL_IS_RPX        0x2u

#define EHDR_SIZE         0x34u
#define SHDR_SIZE         0x28u
#define SYM_SIZE          0x10u
#define RELA_SIZE         0x0Cu
#define RPLLIBS_SIZE      0x0Cu
#define FILEINFO_SIZE     0x60u

#define NO_INDEX          (-1)

#define VERBOSE_LOADED_SECTIONS     1
#define VERBOSE_LOADED_SYMBOLS      3
#define VERBOSE_LOADED_IMPORTS      2
#define VERBOSE_LOADED_RELOCATIONS  4
#define VERBOSE_LOADED_DYN_RELOC    4
#define VERBOSE_PRUNED_SYMBOLS      2
#define VERBOSE_CONVERT_RELOCATIONS 4
#define VERBOSE_DUP_SYMBOLS         3
#define VERBOSE_OUTPUT_SECTIONS     1
#define VERBOSE_OUTPUT_SYMBOLS      2

#define ALIGN_UP(v, a) (((uint32_t)(v) + ((uint32_t)(a) - 1u)) & ~((uint32_t)(a) - 1u))

static int verbose = 0;

/* ---- big-endian fields ---- */

static INLINE uint32_t rd32(const uint8_t *p)
{
   return retro_get_unaligned_32be((void*)p);
}

static INLINE uint16_t rd16(const uint8_t *p)
{
   return retro_get_unaligned_16be((void*)p);
}

static INLINE void wr32(uint8_t *p, uint32_t v)
{
   retro_set_unaligned_32be(p, v);
}

static INLINE void wr16(uint8_t *p, uint16_t v)
{
   retro_set_unaligned_16be(p, v);
}

/* ---- log output ----
 * The messages print numbers in a base that, once switched to hex,
 * stays hex until a message switches it back, so the log reads the
 * same as it always has. */

static int log_hex = 0;

static void log_s(const char *s)
{
   fputs(s, stdout);
}

static void log_n(const char *s, size_t len)
{
   fwrite(s, 1, len, stdout);
}

static void log_u(uint32_t v)
{
   printf(log_hex ? "%lx" : "%lu", (unsigned long)v);
}

static void log_i(int32_t v)
{
   if (log_hex)
      printf("%lx", (unsigned long)(uint32_t)v);
   else
      printf("%ld", (long)v);
}

/* ---- string storage for names the conversion makes ---- */

typedef struct name_block
{
   struct name_block *next;
   size_t used;
   size_t cap;
   char   text[1];
} name_block_t;

static name_block_t *name_blocks = NULL;

static char *name_join(const char *a, size_t alen, const char *b, size_t blen)
{
   char  *s;
   size_t need = alen + blen + 1;

   if (!name_blocks || name_blocks->cap - name_blocks->used < need)
   {
      size_t cap        = need > 65536 ? need : 65536;
      name_block_t *blk = (name_block_t*)malloc(sizeof(*blk) + cap);
      if (!blk)
         return NULL;
      blk->next   = name_blocks;
      blk->used   = 0;
      blk->cap    = cap;
      name_blocks = blk;
   }
   s = name_blocks->text + name_blocks->used;
   memcpy(s, a, alen);
   memcpy(s + alen, b, blen);
   s[alen + blen]     = '\0';
   name_blocks->used += need;
   return s;
}

static void names_free(void)
{
   while (name_blocks)
   {
      name_block_t *next = name_blocks->next;
      free(name_blocks);
      name_blocks = next;
   }
}

/* ---- growable arrays ---- */

static bool grow(void **p, size_t *cap, size_t need, size_t elem)
{
   size_t n;
   void  *q;
   if (need <= *cap)
      return true;
   n = *cap ? *cap : 16;
   while (n < need)
      n *= 2;
   if (!(q = realloc(*p, n * elem)))
      return false;
   *p   = q;
   *cap = n;
   return true;
}

/* ---- the model ---- */

typedef struct
{
   uint32_t name;
   uint32_t type;
   uint32_t flags;
   uint32_t addr;
   uint32_t offset;
   uint32_t size;
   uint32_t link;
   uint32_t info;
   uint32_t addralign;
   uint32_t entsize;
} shdr_t;

typedef struct
{
   shdr_t   h;
   uint8_t *data;       /* in the input image; NULL when it has none */
   uint32_t data_size;
} in_section_t;

typedef struct
{
   const char *name;
   uint8_t    *data;      /* SHT_PROGBITS: in the input image */
   uint32_t    data_size;
   uint32_t    address;
   uint32_t    type;
   uint32_t    flags;
   uint32_t    size;      /* contents for SHT_PROGBITS, extent for SHT_NOBITS */
   int         out;       /* output section */
   int         in_index;
} data_section_t;

typedef struct
{
   const char *name;
   size_t      name_len;
   uint32_t    address;
   uint32_t    size;
   uint32_t    out_name_pos;
   uint32_t    ref_count;
   int         out_index;
   int         in_section;  /* data section */
   int         out_section; /* set on section symbols */
   uint8_t     type;
   uint8_t     binding;
} symbol_t;

typedef struct
{
   uint32_t target;
   uint32_t type;
   uint32_t addend;
   int      symbol;
   int      symbol_section; /* data section */
   int      target_section; /* output section */
} relocation_t;

typedef struct
{
   uint32_t stub_addr;
   uint32_t tramp_addr;
   int      stub_symbol;
   int      tramp_symbol;
} rpl_import_t;

typedef struct
{
   const char *name;
   size_t      name_len;
   size_t      first;   /* imports are made library by library */
   size_t      count;
} rpl_library_t;

typedef struct
{
   const char *name;
   size_t      name_len;
   shdr_t      h;
   uint8_t    *data;
   size_t      data_size;
   size_t      data_cap;
   uint32_t    index;
   uint32_t    signature;     /* SHT_RPL_IMPORTS */
   int         owned;         /* data is ours to free */
   int         reloc_section;
   int         section_symbol;
} out_section_t;

/* Address to index, the first symbol or import at an address unless
 * the rules in symbol_index_add() say otherwise. */
typedef struct
{
   uint32_t *keys;
   int      *vals;
   size_t    cap;   /* power of two; a slot is free when vals is NO_INDEX */
   size_t    count;
} addr_map_t;

typedef struct
{
   uint8_t        *image;
   size_t          image_size;
   uint32_t        entry_point;

   in_section_t   *in;
   size_t          in_count;

   data_section_t *data;
   size_t          data_count, data_cap;

   symbol_t       *syms;
   size_t          sym_count, sym_cap;

   /* The symbol order: the first four, then the section symbols the
    * output makes, then the rest in the order they were read. */
   int            *order;
   size_t          order_count, order_cap;
   int            *sec_syms;
   size_t          sec_sym_count, sec_sym_cap;

   relocation_t   *relocs;
   size_t          reloc_count, reloc_cap;

   rpl_import_t   *imports;
   size_t          import_count, import_cap;
   rpl_library_t  *libs;
   size_t          lib_count, lib_cap;

   addr_map_t      syms_by_addr;
   addr_map_t      imports_by_addr;

   out_section_t  *out;
   size_t          out_count, out_cap;
} elf2rpl_t;

/* ---- address maps ---- */

static INLINE size_t addr_hash(uint32_t key, size_t cap)
{
   return (size_t)((key * 0x9E3779B1u) >> 7) & (cap - 1);
}

static bool addr_map_init(addr_map_t *m, size_t expect)
{
   size_t i, cap = 64;
   while (cap < expect * 2)
      cap *= 2;
   m->keys  = (uint32_t*)malloc(cap * sizeof(*m->keys));
   m->vals  = (int*)malloc(cap * sizeof(*m->vals));
   m->cap   = cap;
   m->count = 0;
   if (!m->keys || !m->vals)
      return false;
   for (i = 0; i < cap; i++)
      m->vals[i] = NO_INDEX;
   return true;
}

static void addr_map_free(addr_map_t *m)
{
   free(m->keys);
   free(m->vals);
}

/* The slot for key: holding it, or the free one it would go in. */
static size_t addr_map_slot(const addr_map_t *m, uint32_t key)
{
   size_t i = addr_hash(key, m->cap);
   while (m->vals[i] != NO_INDEX && m->keys[i] != key)
      i = (i + 1) & (m->cap - 1);
   return i;
}

static bool addr_map_grow(addr_map_t *m)
{
   addr_map_t bigger;
   size_t i;
   if (!addr_map_init(&bigger, m->cap))
   {
      addr_map_free(&bigger);
      return false;
   }
   for (i = 0; i < m->cap; i++)
   {
      if (m->vals[i] != NO_INDEX)
      {
         size_t s         = addr_map_slot(&bigger, m->keys[i]);
         bigger.keys[s]   = m->keys[i];
         bigger.vals[s]   = m->vals[i];
         bigger.count++;
      }
   }
   addr_map_free(m);
   *m = bigger;
   return true;
}

static int addr_map_find(const addr_map_t *m, uint32_t key)
{
   return m->vals[addr_map_slot(m, key)];
}

/* Stores key -> val unless key is there; returns the slot either way,
 * and whether it was already there. */
static bool addr_map_put(addr_map_t *m, uint32_t key, int val,
      size_t *slot, bool *existed)
{
   size_t s;
   if ((m->count + 1) * 2 > m->cap && !addr_map_grow(m))
      return false;
   s = addr_map_slot(m, key);
   *existed = m->vals[s] != NO_INDEX;
   if (!*existed)
   {
      m->keys[s] = key;
      m->vals[s] = val;
      m->count++;
   }
   *slot = s;
   return true;
}

/* ---- symbols ---- */

static int symbol_new(elf2rpl_t *f, uint32_t address, const char *name,
      size_t name_len, uint32_t size, uint8_t type, uint8_t binding)
{
   symbol_t *s;
   if (!grow((void**)&f->syms, &f->sym_cap, f->sym_count + 1, sizeof(*f->syms)))
      return NO_INDEX;
   s               = &f->syms[f->sym_count];
   s->name         = name;
   s->name_len     = name_len;
   s->address      = address;
   s->size         = size;
   s->out_name_pos = 0;
   s->ref_count    = 0;
   s->out_index    = NO_INDEX;
   s->in_section   = NO_INDEX;
   s->out_section  = NO_INDEX;
   s->type         = type;
   s->binding      = binding;
   return (int)f->sym_count++;
}

static void log_dup_sizes(const symbol_t *old, const symbol_t *sym)
{
   log_u(old->size);
   log_s(" / ");
   log_u(sym->size);
   log_s(" ");
   log_u(old->type);
   log_s(" / ");
   log_u(sym->type);
   log_s(" ");
   log_n(old->name, old->name_len);
   log_s(" / ");
   log_n(sym->name, sym->name_len);
   log_s("\n");
}

/* One symbol per address: a typed symbol takes over from an untyped
 * one, a section symbol from anything but a section at a non-zero
 * address, a sized one from an unsized one at a non-zero address;
 * otherwise the first stays. */
static bool symbol_index_add(elf2rpl_t *f, int idx)
{
   size_t slot;
   bool existed;
   const symbol_t *sym = &f->syms[idx];
   const symbol_t *old;

   if (!addr_map_put(&f->syms_by_addr, sym->address, idx, &slot, &existed))
      return false;
   if (!existed)
      return true;

   old = &f->syms[f->syms_by_addr.vals[slot]];

   if (  (old->type == STT_NOTYPE && sym->type != STT_NOTYPE)
      || (   old->type != STT_SECTION
          && sym->type == STT_SECTION
          && sym->address != 0))
   {
      if (verbose >= VERBOSE_DUP_SYMBOLS)
      {
         log_s("Duplicate symbol - changed type ");
         log_hex = 1;
         log_s("0x");
         log_u(sym->address);
         log_s(" ");
         log_n(old->name, old->name_len);
         log_s(" / ");
         log_n(sym->name, sym->name_len);
         log_s("\n");
      }
      f->syms_by_addr.vals[slot] = idx;
   }
   else if ((old->type != STT_SECTION || sym->type == STT_SECTION)
         && sym->type != STT_NOTYPE && old->size == 0
         && sym->size != 0 && sym->size != 0xFFFFFFFFu && sym->address != 0)
   {
      if (verbose >= VERBOSE_DUP_SYMBOLS)
      {
         log_s("Duplicate symbol - changed size ");
         log_hex = 1;
         log_s("0x");
         log_u(sym->address);
         log_s(" ");
         log_dup_sizes(old, sym);
      }
      f->syms_by_addr.vals[slot] = idx;
   }
   else if (  (sym->name_len != old->name_len
            || memcmp(sym->name, old->name, sym->name_len))
         && sym->address != 0)
   {
      if (verbose >= VERBOSE_DUP_SYMBOLS)
      {
         log_s("Duplicate symbol ");
         log_hex = 1;
         log_s("0x");
         log_u(sym->address);
         log_s(" ");
         log_dup_sizes(old, sym);
      }
   }
   return true;
}

static bool symbol_add(elf2rpl_t *f, int idx)
{
   if (!grow((void**)&f->order, &f->order_cap, f->order_count + 1, sizeof(*f->order)))
      return false;
   f->order[f->order_count++] = idx;
   return symbol_index_add(f, idx);
}

static bool symbol_add_section(elf2rpl_t *f, int idx)
{
   if (!symbol_index_add(f, idx))
      return false;
   if (!grow((void**)&f->sec_syms, &f->sec_sym_cap, f->sec_sym_count + 1,
            sizeof(*f->sec_syms)))
      return false;
   f->sec_syms[f->sec_sym_count++] = idx;
   return true;
}

static int symbol_find(const elf2rpl_t *f, uint32_t address)
{
   return addr_map_find(&f->syms_by_addr, address);
}

static int import_find(const elf2rpl_t *f, uint32_t address)
{
   return addr_map_find(&f->imports_by_addr, address);
}

/* ---- data sections ---- */

static INLINE bool data_section_contains(const data_section_t *s, uint32_t address)
{
   if (!(s->flags & SHF_ALLOC) || s->size == 0 || s->size == 0xFFFFFFFFu)
      return false;
   return address >= s->address && address <= s->address + s->size;
}

static int data_section_find(const elf2rpl_t *f, uint32_t address)
{
   size_t i;
   if (address == 0)
      return f->data_count ? 0 : NO_INDEX;
   for (i = 0; i < f->data_count; i++)
      if (data_section_contains(&f->data[i], address))
         return (int)i;
   return NO_INDEX;
}

/* ---- input ---- */

static uint8_t *loader_data(elf2rpl_t *f, uint32_t address)
{
   size_t i;
   for (i = 0; i < f->in_count; i++)
   {
      const in_section_t *s = &f->in[i];
      uint32_t start        = s->h.addr;
      uint32_t end          = start + s->data_size;
      if (start <= address && end > address)
         return s->data + (address - start);
   }
   return NULL;
}

static const char *in_section_name(const elf2rpl_t *f, const in_section_t *strtab,
      uint32_t off)
{
   (void)f;
   if (!strtab->data || off >= strtab->data_size)
      return "";
   return (const char*)strtab->data + off;
}

static const uint8_t *in_symbol(const in_section_t *symtab, uint32_t index)
{
   if (!symtab->data || (uint64_t)index * SYM_SIZE + SYM_SIZE > symtab->data_size)
      return NULL;
   return symtab->data + (size_t)index * SYM_SIZE;
}

static bool read_image(elf2rpl_t *f, const char *path)
{
   FILE *in = fopen(path, "rb");
   long  len;

   if (!in)
   {
      printf("Could not open %s for reading\n", path);
      return false;
   }
   if (fseek(in, 0, SEEK_END) != 0 || (len = ftell(in)) < 0
         || fseek(in, 0, SEEK_SET) != 0)
   {
      fclose(in);
      printf("Could not open %s for reading\n", path);
      return false;
   }
   f->image_size = (size_t)len;
   /* One byte past the image, so a name at its very end still ends. */
   if (!(f->image = (uint8_t*)malloc(f->image_size + 1)))
   {
      fclose(in);
      return false;
   }
   if (fread(f->image, 1, f->image_size, in) != f->image_size)
   {
      fclose(in);
      printf("Could not open %s for reading\n", path);
      return false;
   }
   f->image[f->image_size] = 0;
   fclose(in);
   return true;
}

static bool read_elf(elf2rpl_t *f, const char *path)
{
   const uint8_t *eh;
   const in_section_t *shstrtab;
   uint32_t shoff;
   unsigned shnum, shstrndx;
   size_t i, sym_total = 0;
   int *data_index;
   int sym;

   if (!read_image(f, path))
      return false;

   eh = f->image;
   if (f->image_size < EHDR_SIZE || rd32(eh) != ELF_MAGIC)
   {
      log_s("Invalid ELF magic header\n");
      return false;
   }
   if (eh[4] != ELFCLASS32)
   {
      log_s("Unexpected ELF file class\n");
      return false;
   }
   if (eh[5] != ELFDATA2MSB)
   {
      log_s("Unexpected ELF encoding\n");
      return false;
   }
   if (rd16(eh + 18) != EM_PPC)
   {
      log_s("Unexpected ELF machine type\n");
      return false;
   }
   if (eh[6] != EV_CURRENT)
   {
      log_s("Unexpected ELF version\n");
      return false;
   }

   f->entry_point = rd32(eh + 24);
   shoff          = rd32(eh + 32);
   shnum          = rd16(eh + 48);
   shstrndx       = rd16(eh + 50);

   if ((uint64_t)shoff + (uint64_t)shnum * SHDR_SIZE > f->image_size
         || shstrndx >= shnum)
   {
      log_s("Unexpected ELF section header table\n");
      return false;
   }

   /* Section headers and where their contents lie */
   f->in_count = shnum;
   if (!(f->in = (in_section_t*)calloc(shnum ? shnum : 1, sizeof(*f->in))))
      return false;
   for (i = 0; i < shnum; i++)
   {
      const uint8_t *sh = f->image + shoff + i * SHDR_SIZE;
      in_section_t  *s  = &f->in[i];
      s->h.name      = rd32(sh);
      s->h.type      = rd32(sh + 4);
      s->h.flags     = rd32(sh + 8);
      s->h.addr      = rd32(sh + 12);
      s->h.offset    = rd32(sh + 16);
      s->h.size      = rd32(sh + 20);
      s->h.link      = rd32(sh + 24);
      s->h.info      = rd32(sh + 28);
      s->h.addralign = rd32(sh + 32);
      s->h.entsize   = rd32(sh + 36);

      if (!s->h.size || s->h.type == SHT_NOBITS)
         continue;
      if ((uint64_t)s->h.offset + s->h.size > f->image_size)
      {
         log_s("Unexpected ELF section contents\n");
         return false;
      }
      s->data      = f->image + s->h.offset;
      s->data_size = s->h.size;
   }

   shstrtab = &f->in[shstrndx];

   /* Process any loader relocations */
   for (i = 0; i < f->in_count; i++)
   {
      const in_section_t *s = &f->in[i];
      const in_section_t *symtab;
      size_t r, count;

      if (s->h.type != SHT_RELA
            || strcmp(in_section_name(f, shstrtab, s->h.name), ".rela.dyn"))
         continue;
      if (s->h.link >= f->in_count)
      {
         log_s("Unexpected relocation offset in .rela.dyn section\n");
         return false;
      }

      symtab = &f->in[s->h.link];
      count  = s->data_size / RELA_SIZE;

      for (r = 0; r < count; r++)
      {
         const uint8_t *rela  = s->data + r * RELA_SIZE;
         uint32_t       off   = rd32(rela);
         uint32_t       info  = rd32(rela + 4);
         uint32_t       index = info >> 8;
         const uint8_t *esym  = in_symbol(symtab, index);
         uint32_t       addr;
         uint8_t       *ptr;

         if (!esym)
         {
            log_s("Unexpected relocation symbol index in .rela.dyn section\n");
            return false;
         }
         addr = rd32(esym + 4) + rd32(rela + 8);

         if (!(ptr = loader_data(f, off)))
         {
            log_s("Unexpected relocation offset in .rela.dyn section\n");
            return false;
         }

         switch (info & 0xFF)
         {
            case R_PPC_RELATIVE:
               if (verbose >= VERBOSE_LOADED_DYN_RELOC)
               {
                  log_s("Dyn relocation 0x");
                  log_hex = 1;
                  log_u(off);
                  log_s(" 0x");
                  log_u(rd32(ptr));
                  log_s(" -> 0x");
                  log_u(addr);
                  log_s("\n");
               }
               wr32(ptr, addr);
               break;
            case R_PPC_NONE:
               /* ignore padding */
               break;
            default:
               log_s("Unexpected relocation type in .rela.dyn section\n");
               return false;
         }
      }
   }

   /* Read text/data sections */
   if (!(data_index = (int*)malloc((f->in_count ? f->in_count : 1) * sizeof(int))))
      return false;
   for (i = 0; i < f->in_count; i++)
   {
      const in_section_t *s = &f->in[i];
      const char *name;
      data_section_t *d;

      data_index[i] = NO_INDEX;

      if (s->h.addr >= LOAD_ADDRESS && s->h.addr < CODE_ADDRESS)
         continue; /* Skip any load sections */

      name = in_section_name(f, shstrtab, s->h.name);

      if (s->h.type == SHT_PROGBITS || s->h.type == SHT_NOBITS)
      {
         if (!grow((void**)&f->data, &f->data_cap, f->data_count + 1,
                  sizeof(*f->data)))
         {
            free(data_index);
            return false;
         }
         d            = &f->data[f->data_count];
         d->name      = name;
         d->address   = s->h.addr;
         d->type      = s->h.type;
         d->flags     = s->h.flags;
         d->size      = s->h.size;
         d->out       = NO_INDEX;
         d->in_index  = (int)i;
         d->data      = s->h.type == SHT_PROGBITS ? s->data : NULL;
         d->data_size = s->h.type == SHT_PROGBITS ? s->data_size : 0;
         data_index[i] = (int)f->data_count++;
      }

      if (verbose >= VERBOSE_LOADED_SECTIONS)
      {
         if (s->h.type == SHT_PROGBITS)
            log_s("Data section #");
         else if (s->h.type == SHT_NOBITS)
            log_s("BSS section #");
         else
            log_s("Section #");
         log_i((int32_t)i);
         if (s->h.type != SHT_PROGBITS && s->h.type != SHT_NOBITS)
         {
            log_s(" type 0x");
            log_u(s->h.type);
         }
         log_s(" ");
         log_s(name);
         log_hex = 1;
         log_s(": 0x");
         log_u(s->h.addr);
         log_s(" - 0x");
         log_u(s->h.addr + s->h.size);
         log_s("\n");
      }
   }

   for (i = 0; i < f->in_count; i++)
      if (f->in[i].h.type == SHT_SYMTAB)
         sym_total += f->in[i].data_size / SYM_SIZE;
   if (  !addr_map_init(&f->syms_by_addr, sym_total + 1024)
      || !addr_map_init(&f->imports_by_addr, 1024))
   {
      free(data_index);
      return false;
   }

   /* Default symbols: null, $TEXT, $DATA, $UNDEF */
   if (  (sym = symbol_new(f, 0, "", 0, 0, STT_NOTYPE, STB_LOCAL)) == NO_INDEX
      || !symbol_add(f, sym)
      || (sym = symbol_new(f, CODE_ADDRESS, "$TEXT", 5, 0, STT_SECTION, STB_LOCAL)) == NO_INDEX
      || !symbol_add(f, sym)
      || (sym = symbol_new(f, DATA_ADDRESS, "$DATA", 5, 0, STT_SECTION, STB_LOCAL)) == NO_INDEX
      || !symbol_add(f, sym)
      || (sym = symbol_new(f, 0, "$UNDEF", 6, 0, STT_OBJECT, STB_GLOBAL)) == NO_INDEX
      || !symbol_add(f, sym))
   {
      free(data_index);
      return false;
   }

   /* Read symbols */
   for (i = 0; i < f->in_count; i++)
   {
      const in_section_t *s = &f->in[i];
      const in_section_t *strtab;
      size_t n, count;

      if (s->h.type != SHT_SYMTAB)
         continue;

      if (strcmp(in_section_name(f, shstrtab, s->h.name), ".symtab"))
      {
         log_s("Unexpected symbol section ");
         log_s(in_section_name(f, shstrtab, s->h.name));
         log_s("\n");
         free(data_index);
         return false;
      }
      if (s->h.link >= f->in_count)
      {
         free(data_index);
         return false;
      }

      strtab = &f->in[s->h.link];
      count  = s->data_size / SYM_SIZE;

      for (n = 0; n < count; n++)
      {
         const uint8_t *es  = s->data + n * SYM_SIZE;
         uint32_t value     = rd32(es + 4);
         uint8_t  info      = es[12];
         uint8_t  type      = info & 0xF;
         uint16_t shndx     = rd16(es + 14);
         const char *name;
         symbol_t *ns;

         /* Skip any load symbols */
         if (value >= LOAD_ADDRESS && value < CODE_ADDRESS)
            continue;
         /* Skip null symbol */
         if (type == STT_NOTYPE && value == 0)
            continue;
         /* Skip file, section symbols */
         if (type == STT_FILE || type == STT_SECTION)
            continue;

         name = in_section_name(f, strtab, rd32(es));
         if ((sym = symbol_new(f, value, name, strlen(name), rd32(es + 8),
                     type, (info >> 4) & 0xF)) == NO_INDEX)
         {
            free(data_index);
            return false;
         }
         ns = &f->syms[sym];
         if (shndx < f->in_count && data_index[shndx] != NO_INDEX
               && f->data[data_index[shndx]].size != 0)
            ns->in_section = data_index[shndx];

         if (verbose >= VERBOSE_LOADED_SYMBOLS)
         {
            log_s("Symbol ");
            log_n(ns->name, ns->name_len);
            log_s(" 0x");
            log_hex = 1;
            log_u(ns->address);
            log_s(" 0x");
            log_u(ns->size);
            if (ns->in_section != NO_INDEX)
            {
               log_s(" (");
               log_s(f->data[ns->in_section].name);
               log_s(")");
            }
            log_s("\n");
         }

         if (!symbol_add(f, sym))
         {
            free(data_index);
            return false;
         }
      }
   }
   free(data_index);

   /* Read RPL imports */
   for (i = 0; i < f->in_count; i++)
   {
      const in_section_t *s = &f->in[i];
      size_t n, count;

      if (strcmp(in_section_name(f, shstrtab, s->h.name), ".lib.rplLibs"))
         continue;

      count = s->data_size / RPLLIBS_SIZE;
      for (n = 0; n < count; n++)
      {
         const uint8_t *def   = s->data + n * RPLLIBS_SIZE;
         uint32_t stub_start  = rd32(def + 4);
         uint32_t stub_end    = rd32(def + 8);
         const char *lib_name = (const char*)loader_data(f, rd32(def));
         rpl_library_t *lib;
         uint32_t stub_addr;

         if (!lib_name)
         {
            log_s("Unexpected import library name\n");
            return false;
         }
         if (!grow((void**)&f->libs, &f->lib_cap, f->lib_count + 1, sizeof(*f->libs)))
            return false;
         lib           = &f->libs[f->lib_count++];
         lib->name     = lib_name;
         lib->name_len = strlen(lib_name);
         lib->first    = f->import_count;
         lib->count    = 0;

         if (verbose >= VERBOSE_LOADED_SECTIONS)
         {
            log_s("Import library ");
            log_s(lib_name);
            log_s("\n");
         }

         for (stub_addr = stub_start; stub_addr < stub_end; stub_addr += 4)
         {
            const uint8_t *stub = loader_data(f, stub_addr);
            uint32_t tramp_addr;
            int tramp, stub_sym;
            symbol_t *ts;
            rpl_import_t *imp;
            size_t slot;
            bool existed;

            if (!stub)
            {
               log_s("Unexpected import stub address\n");
               return false;
            }
            tramp_addr = rd32(stub);

            /* Get the tramp symbol */
            if ((tramp = symbol_find(f, tramp_addr)) == NO_INDEX)
            {
               log_s("No symbol found for import @ 0x");
               log_hex = 1;
               log_u(tramp_addr);
               log_s(" in ");
               log_s(lib_name);
               log_s("\n");
               return false;
            }
            ts = &f->syms[tramp];

            if (verbose >= VERBOSE_LOADED_IMPORTS)
            {
               log_s("Import symbol ");
               log_n(ts->name, ts->name_len);
               log_s(" 0x");
               log_hex = 1;
               log_u(stub_addr);
               log_s(" -> 0x");
               log_u(tramp_addr);
               log_s("\n");
            }

            /* A new symbol to use for the import, under the tramp's
             * name; the tramp is renamed. */
            if ((stub_sym = symbol_new(f, 0, ts->name, ts->name_len, 0,
                        STT_FUNC, STB_GLOBAL)) == NO_INDEX)
               return false;
            ts = &f->syms[tramp];
            if (!(ts->name = name_join(ts->name, ts->name_len, "_tramp", 6)))
               return false;
            ts->name_len += 6;

            f->syms[stub_sym].ref_count++;
            if (!symbol_add(f, stub_sym))
               return false;

            if (!grow((void**)&f->imports, &f->import_cap, f->import_count + 1,
                     sizeof(*f->imports)))
               return false;
            imp               = &f->imports[f->import_count];
            imp->stub_addr    = stub_addr;
            imp->tramp_addr   = tramp_addr;
            imp->stub_symbol  = stub_sym;
            imp->tramp_symbol = tramp;

            if (!addr_map_put(&f->imports_by_addr, stub_addr,
                     (int)f->import_count, &slot, &existed))
               return false;
            if (existed)
            {
               log_s("Duplicate import stub address ");
               log_n(f->syms[stub_sym].name, f->syms[stub_sym].name_len);
               log_s("\n");
            }
            if (!addr_map_put(&f->imports_by_addr, tramp_addr,
                     (int)f->import_count, &slot, &existed))
               return false;
            if (existed)
            {
               log_s("Duplicate import tramp address ");
               log_n(f->syms[tramp].name, f->syms[tramp].name_len);
               log_s("\n");
            }
            f->import_count++;
            lib->count++;
         }
      }
   }

   /* Read relocations */
   for (i = 0; i < f->in_count; i++)
   {
      const in_section_t *s = &f->in[i];
      const in_section_t *symtab;
      const char *name;
      size_t r, count;

      if (s->h.type != SHT_RELA)
         continue;
      name = in_section_name(f, shstrtab, s->h.name);
      if (!strcmp(name, ".rela.dyn"))
         continue; /* Skip dyn relocations */
      if (s->h.link >= f->in_count)
         return false;

      symtab = &f->in[s->h.link];
      count  = s->data_size / RELA_SIZE;

      if (!grow((void**)&f->relocs, &f->reloc_cap, f->reloc_count + count,
               sizeof(*f->relocs)))
         return false;

      for (r = 0; r < count; r++)
      {
         const uint8_t *rela = s->data + r * RELA_SIZE;
         uint32_t off        = rd32(rela);
         uint32_t info       = rd32(rela + 4);
         uint32_t addend     = rd32(rela + 8);
         uint32_t type       = info & 0xFF;
         uint32_t index      = info >> 8;
         const uint8_t *esym = in_symbol(symtab, index);
         relocation_t *rel;
         int target_sym      = NO_INDEX;
         int target_sec      = NO_INDEX;
         int imp;

         if (!esym)
         {
            log_s("Unexpected relocation symbol index in ");
            log_s(name);
            log_s("\n");
            return false;
         }

         /* A symbol still undefined after the link is a weak reference
          * nothing defined. The link already wrote its final value -
          * zero, or a branch to itself - which does not depend on where
          * the module loads, so there is nothing for the loader to do.
          * Handed to it, the relocation names an undefined symbol and a
          * branch to address zero no 24-bit displacement reaches. */
         if (index != 0 && rd16(esym + 14) == SHN_UNDEF)
         {
            if (verbose >= VERBOSE_LOADED_RELOCATIONS)
            {
               log_s("Relocation at 0x");
               log_hex = 1;
               log_u(off);
               log_s(" against an undefined weak symbol dropped\n");
            }
            continue;
         }

         if ((esym[12] & 0xF) == STT_SECTION && rd32(esym + 4) == CODE_ADDRESS
               && (off < CODE_ADDRESS || off >= DATA_ADDRESS))
         {
            log_s("Unexpected symbol referenced in relocation section ");
            log_s(name);
            log_s("\n");
            return false;
         }

         if (verbose >= VERBOSE_LOADED_RELOCATIONS)
         {
            log_s("Relocation #");
            log_hex = 0;
            log_u((uint32_t)f->reloc_count);
            log_s(" sym index ");
            log_u(index);
            log_s("  offset 0x");
            log_hex = 1;
            log_u(off);
            log_s(" addend 0x");
            log_u(addend);
         }

         if ((imp = import_find(f, addend)) != NO_INDEX)
         {
            target_sym = f->imports[imp].stub_symbol;
            addend     = 0;
            if (verbose >= VERBOSE_LOADED_RELOCATIONS)
               log_s(" -> import ");
         }
         else if ((target_sym = symbol_find(f, addend)) != NO_INDEX)
         {
            addend = 0;
            if (verbose >= VERBOSE_LOADED_RELOCATIONS)
               log_s(" -> symbol ");
         }
         else if ((target_sec = data_section_find(f, addend)) != NO_INDEX)
         {
            addend -= f->data[target_sec].address;
            if (verbose >= VERBOSE_LOADED_RELOCATIONS)
               log_s(" -> section ");
         }
         else
         {
            uint8_t *ptr;
            if (verbose >= VERBOSE_LOADED_RELOCATIONS)
               log_s(" -> not found\n");

            /* If we can't find a proper symbol, write the addend in and
             * hope for the best. Stored in host order, as it always has
             * been. */
            if ((ptr = loader_data(f, off)) != NULL)
               memcpy(ptr, &addend, sizeof(addend));

            log_s("Unexpected addend ");
            log_hex = 1;
            log_u(addend);
            log_s(" referenced in relocation section ");
            log_s(name);
            log_s(", continuing.\n");
            continue;
         }

         if (verbose >= VERBOSE_LOADED_RELOCATIONS)
         {
            if (target_sym != NO_INDEX)
               log_n(f->syms[target_sym].name, f->syms[target_sym].name_len);
            else
               log_s(f->data[target_sec].name);
            if (addend != 0)
            {
               log_s(" + ");
               log_u(addend);
            }
            log_s("\n");
         }

         if (target_sym != NO_INDEX)
            f->syms[target_sym].ref_count++;

         rel                 = &f->relocs[f->reloc_count++];
         rel->target         = off;
         rel->type           = type;
         rel->addend         = addend;
         rel->symbol         = target_sym;
         rel->symbol_section = target_sec;
         rel->target_section = NO_INDEX;
      }
   }

   /* Read dyn relocations */
   for (i = 0; i < f->in_count; i++)
   {
      const in_section_t *s = &f->in[i];
      const in_section_t *symtab;
      size_t r, count;

      if (s->h.type != SHT_RELA
            || strcmp(in_section_name(f, shstrtab, s->h.name), ".rela.dyn"))
         continue;

      symtab = &f->in[s->h.link];
      count  = s->data_size / RELA_SIZE;

      if (!grow((void**)&f->relocs, &f->reloc_cap, f->reloc_count + count,
               sizeof(*f->relocs)))
         return false;

      for (r = 0; r < count; r++)
      {
         const uint8_t *rela = s->data + r * RELA_SIZE;
         uint32_t off        = rd32(rela);
         uint32_t info       = rd32(rela + 4);
         int32_t  raddend    = (int32_t)rd32(rela + 8);
         uint32_t type       = info & 0xFF;
         uint32_t index      = info >> 8;
         const uint8_t *esym = in_symbol(symtab, index);
         uint32_t addr, addend = 0;
         relocation_t *rel;
         int target_sym      = NO_INDEX;
         int target_sec      = NO_INDEX;

         if (type == R_PPC_NONE)
            continue; /* ignore padding */
         if (!esym)
         {
            log_s("Unexpected relocation symbol index in .rela.dyn section\n");
            return false;
         }
         addr = rd32(esym + 4) + (uint32_t)raddend;

         if (verbose >= VERBOSE_LOADED_DYN_RELOC)
         {
            log_s("Dyn relocation #");
            log_hex = 0;
            log_u((uint32_t)f->reloc_count);
            log_s(" offset 0x");
            log_hex = 1;
            log_u(off);
            log_s(" addend 0x");
            log_i(raddend);
            log_s(" addr 0x");
            log_u(addr);
         }

         /* Index 0 is what R_PPC_RELATIVE carries */
         if (index == 0)
         {
            int imp;
            addend = (uint32_t)raddend;

            if ((imp = import_find(f, addend)) != NO_INDEX)
            {
               target_sym = f->imports[imp].stub_symbol;
               addend     = 0;
               if (verbose >= VERBOSE_LOADED_DYN_RELOC)
                  log_s(" -> import ");
            }
            else if ((target_sym = symbol_find(f, addend)) != NO_INDEX)
            {
               addend -= f->syms[target_sym].address;
               if (verbose >= VERBOSE_LOADED_DYN_RELOC)
                  log_s(" -> symbol ");
            }
            else if ((target_sec = data_section_find(f, addend)) != NO_INDEX)
            {
               addend -= f->data[target_sec].address;
               if (verbose >= VERBOSE_LOADED_RELOCATIONS)
                  log_s(" -> section ");
            }
            else
            {
               if (verbose >= VERBOSE_LOADED_DYN_RELOC)
                  log_s(" -> bad\n");
               log_s("Unexpected relocation symbol address 0x");
               log_hex = 1;
               log_u(addr);
               log_s(" in .rela.dyn section\n");
               return false;
            }
         }
         else
         {
            if (verbose >= VERBOSE_LOADED_DYN_RELOC)
            {
               log_s(" -> index ");
               log_hex = 0;
               log_u(index);
               log_s("\n");
            }
            log_s("Unexpected relocation symbol index ");
            log_hex = 0;
            log_u(index);
            log_s(" in .rela.dyn section\n");
            return false;
         }

         if (verbose >= VERBOSE_LOADED_DYN_RELOC)
         {
            if (target_sym != NO_INDEX)
               log_n(f->syms[target_sym].name, f->syms[target_sym].name_len);
            else
               log_s(f->data[target_sec].name);
            if (addend != 0)
            {
               log_s(" + ");
               log_u(addend);
            }
            log_s("\n");
         }

         if (type != R_PPC_RELATIVE)
         {
            log_s("Unexpected relocation type in .rela.dyn section\n");
            return false;
         }

         /* Scrap any compiler/linker garbage */
         if (off >= CODE_ADDRESS && off < WIIU_LOAD_ADDRESS)
         {
            if (target_sym != NO_INDEX)
               f->syms[target_sym].ref_count++;
            rel                 = &f->relocs[f->reloc_count++];
            rel->target         = off;
            rel->type           = R_PPC_ADDR32;
            rel->addend         = addend;
            rel->symbol         = target_sym;
            rel->symbol_section = target_sec;
            rel->target_section = NO_INDEX;
         }
      }
   }

   return true;
}

/* ---- output sections ---- */

static INLINE bool out_section_contains(const out_section_t *s, uint32_t address)
{
   if (!(s->h.flags & SHF_ALLOC) || s->h.size == 0 || s->h.size == 0xFFFFFFFFu)
      return false;
   return address >= s->h.addr && address <= s->h.addr + s->h.size;
}

static int out_section_at(const elf2rpl_t *f, uint32_t address, int hint)
{
   size_t i;
   if (address == 0)
      return 0;
   if (hint != NO_INDEX && out_section_contains(&f->out[hint], address))
      return hint;
   for (i = 0; i < f->out_count; i++)
      if (out_section_contains(&f->out[i], address))
         return (int)i;
   return NO_INDEX;
}

static int out_section_named(const elf2rpl_t *f, const char *name, size_t len)
{
   size_t i;
   for (i = 0; i < f->out_count; i++)
      if (f->out[i].name_len == len && !memcmp(f->out[i].name, name, len))
         return (int)i;
   return NO_INDEX;
}

static int out_section_new(elf2rpl_t *f, const char *name, size_t name_len)
{
   out_section_t *s;
   if (!grow((void**)&f->out, &f->out_cap, f->out_count + 1, sizeof(*f->out)))
      return NO_INDEX;
   s = &f->out[f->out_count];
   memset(s, 0, sizeof(*s));
   s->name           = name;
   s->name_len       = name_len;
   s->index          = 0xFFFFFFFFu;
   s->reloc_section  = NO_INDEX;
   s->section_symbol = NO_INDEX;
   return (int)f->out_count++;
}

static bool out_append(out_section_t *s, const void *p, size_t n)
{
   if (!grow((void**)&s->data, &s->data_cap, s->data_size + n, 1))
      return false;
   memcpy(s->data + s->data_size, p, n);
   s->data_size += n;
   s->owned      = 1;
   return true;
}

static void log_out_section(const out_section_t *s)
{
   log_s("Output section #");
   log_hex = 0;
   log_u(s->index);
   log_hex = 1;
   log_s(" ");
   log_u(s->h.type);
   log_s(" ");
   log_u(s->h.flags);
   log_s(" ");
   log_n(s->name, s->name_len);
   log_s(" (0x");
   log_u(s->h.addr);
   log_s(" - 0x");
   log_u(s->h.addr + s->h.size);
   log_s(")\n");
}

/* Numbers the section the next in order. With a symbol, a section of
 * any size gets a section symbol among the first ones. */
static bool out_section_add(elf2rpl_t *f, int idx, bool with_symbol)
{
   out_section_t *s = &f->out[idx];
   int sym          = NO_INDEX;

   if (with_symbol && s->h.size != 0)
   {
      if ((sym = symbol_new(f, s->h.addr, s->name, s->name_len, 0xFFFFFFFFu,
                  STT_SECTION, STB_LOCAL)) == NO_INDEX)
         return false;
      s                     = &f->out[idx];
      f->syms[sym].out_section = idx;
      s->section_symbol        = sym;
   }
   s->index = (uint32_t)idx;

   if (verbose >= VERBOSE_OUTPUT_SECTIONS)
      log_out_section(s);

   if (with_symbol || s->name_len)
   {
      int named = out_section_named(f, s->name, s->name_len);
      if (named != NO_INDEX && named != idx)
      {
         log_s("Duplicate output section name ");
         log_n(s->name, s->name_len);
         log_s("\n");
      }
   }

   if (sym == NO_INDEX)
      return true;

   if (verbose >= VERBOSE_LOADED_SYMBOLS)
   {
      log_s("Section symbol ");
      log_n(f->syms[sym].name, f->syms[sym].name_len);
      log_s(" 0x");
      log_hex = 1;
      log_u(f->syms[sym].address);
      log_s(" 0x");
      log_u(f->syms[sym].size);
      log_s("\n");
   }

   return symbol_add_section(f, sym);
}

static void shdr_set(shdr_t *h, uint32_t type, uint32_t flags, uint32_t addr,
      uint32_t size, uint32_t link, uint32_t info, uint32_t addralign,
      uint32_t entsize)
{
   h->name      = 0xFFFFFFFFu;
   h->type      = type;
   h->flags     = flags;
   h->addr      = addr;
   h->offset    = 0xFFFFFFFFu;
   h->size      = size;
   h->link      = link;
   h->info      = info;
   h->addralign = addralign;
   h->entsize   = entsize;
}

/* Gives each section a file offset, after the CRC and file info,
 * data sections first, then those loaded at 0xC0000000, then the rest;
 * returns where the next would go. */
static uint32_t place(out_section_t *s, uint32_t at)
{
   if (s->h.type != SHT_NOBITS)
      s->h.size = (uint32_t)s->data_size;
   if (s->data_size)
   {
      s->h.offset = at;
      return ALIGN_UP(at + (uint32_t)s->data_size, 64);
   }
   s->h.offset = 0;
   return at;
}

static bool write_rpl(elf2rpl_t *f, const char *path)
{
   size_t i, n, final_count;
   int *final_order;
   int idx, target = NO_INDEX, sym_hint = NO_INDEX;
   int sym_text, sym_data, symtab, strtab, shstrtab, crcs, fileinfo;
   uint32_t load_address, predict_str, predict_sym, predict_shstr;
   uint32_t text_size = 0, data_size = 0, load_size = 0, temp_size = 0;
   uint32_t shoff, data_offset, file_size, heap_size = 0x8000;
   uint8_t *out, *p;
   uint8_t zeros[16];
   FILE *fp;

   /* NULL section */
   if ((idx = out_section_new(f, "", 0)) == NO_INDEX)
      return false;
   f->out[idx].h.offset = 0;
   if (!out_section_add(f, idx, false))
      return false;

   /* text/data sections */
   for (i = 0; i < f->data_count; i++)
   {
      data_section_t *d = &f->data[i];
      out_section_t  *s;

      if ((idx = out_section_new(f, d->name, strlen(d->name))) == NO_INDEX)
         return false;
      s = &f->out[idx];
      shdr_set(&s->h, d->type, d->flags, d->address,
            d->type == SHT_NOBITS ? d->size : d->data_size, 0, 0, 256, 0);
      if (d->address == DATA_ADDRESS)
      {
         s->h.addralign = 4096;
         s->h.flags    |= SHF_WRITE; /* .rodata needs to be writable? */
      }
      s->data      = d->data;
      s->data_size = d->data_size;
      d->out       = idx;
      if (!out_section_add(f, idx, true))
         return false;
   }

   /* Relocation sections, one per section relocated */
   for (i = 0; i < f->reloc_count; i++)
   {
      relocation_t *rel = &f->relocs[i];

      if ((target = out_section_at(f, rel->target, target)) == NO_INDEX)
      {
         log_s("Error could not find section for relocation\n");
         return false;
      }
      rel->target_section = target;

      if (f->out[target].reloc_section == NO_INDEX)
      {
         const out_section_t *t = &f->out[target];
         char *name             = name_join(".rela", 5, t->name, t->name_len);
         if (!name)
            return false;
         if ((idx = out_section_new(f, name, strlen(name))) == NO_INDEX)
            return false;
         shdr_set(&f->out[idx].h, SHT_RELA, 0, 0, 0xFFFFFFFFu, 0xFFFFFFFFu,
               f->out[target].index, 4, RELA_SIZE);
         f->out[target].reloc_section = idx;
         if (!out_section_add(f, idx, true))
            return false;
      }
   }

   /* Sizes of the symbol and string tables, so the RPL imports go
    * after them */
   predict_str   = 1;
   predict_sym   = 1;
   predict_shstr = 1;
   for (i = 0; i < 4 && i < f->order_count; i++)
   {
      predict_str += (uint32_t)f->syms[f->order[i]].name_len + 1;
      predict_sym += SYM_SIZE;
   }
   for (i = 0; i < f->sec_sym_count; i++)
   {
      predict_str += (uint32_t)f->syms[f->sec_syms[i]].name_len + 1;
      predict_sym += SYM_SIZE;
   }
   for (i = 4; i < f->order_count; i++)
   {
      predict_str += (uint32_t)f->syms[f->order[i]].name_len + 1;
      predict_sym += SYM_SIZE;
   }
   for (i = 0; i < f->out_count; i++)
      predict_shstr += (uint32_t)f->out[i].name_len + 1;
   predict_str   = ALIGN_UP(predict_str, 0x10);
   predict_sym   = ALIGN_UP(predict_sym, 0x10);
   predict_shstr = ALIGN_UP(predict_shstr, 0x10);
   load_address  = WIIU_LOAD_ADDRESS + predict_str + predict_sym + predict_shstr;

   /* RPL import sections, .fimport_* */
   for (i = 0; i < f->lib_count; i++)
   {
      const rpl_library_t *lib = &f->libs[i];
      uint32_t name_size       = ALIGN_UP(8 + lib->name_len, 8);
      uint32_t stub_size       = 8 + 8 * (uint32_t)lib->count;
      uint32_t size            = name_size > stub_size ? name_size : stub_size;
      char *name               = name_join(".fimport_", 9, lib->name, lib->name_len);
      out_section_t *s;

      if (!name || (idx = out_section_new(f, name, 9 + lib->name_len)) == NO_INDEX)
         return false;
      s = &f->out[idx];
      shdr_set(&s->h, SHT_RPL_IMPORTS, SHF_ALLOC | SHF_EXECINSTR, load_address,
            size, 0, 0, 4, 0);

      /* A name whose length is a multiple of eight ends one byte past
       * the section; the terminator is kept out of it. */
      if (!(s->data = (uint8_t*)calloc(size + 1, 1)))
         return false;
      s->data_size = size;
      s->data_cap  = size + 1;
      s->owned     = 1;
      wr32(s->data, (uint32_t)lib->count);
      wr32(s->data + 4, 0);
      memcpy(s->data + 8, lib->name, lib->name_len);
      s->signature = 0;

      /* Addresses of the import symbols */
      for (n = 0; n < lib->count; n++)
         f->syms[f->imports[lib->first + n].stub_symbol].address =
               load_address + 8 + (uint32_t)n * 8;

      load_address = ALIGN_UP(load_address + size, 4);

      if (!out_section_add(f, idx, true))
         return false;
   }

   /* Prune out unneeded symbols - but keep those used for relocations!
    * The order: the first four, the section symbols, the rest. */
   if (!(final_order = (int*)malloc((f->order_count + f->sec_sym_count + 1)
               * sizeof(int))))
      return false;
   final_count = 0;
   for (i = 0; i < f->order_count + f->sec_sym_count; i++)
   {
      int s;
      const symbol_t *sym;
      if (i < 4 && i < f->order_count)
         s = f->order[i];
      else if (i < 4 + f->sec_sym_count && f->order_count >= 4)
         s = f->sec_syms[i - 4];
      else if (f->order_count < 4)
         s = f->sec_syms[i - f->order_count];
      else
         s = f->order[i - f->sec_sym_count];

      sym = &f->syms[s];
      if (sym->name_len && sym->type == STT_NOTYPE && sym->size == 0
            && sym->ref_count == 0)
      {
         if (verbose >= VERBOSE_PRUNED_SYMBOLS)
         {
            log_s("Pruning symbol ");
            log_n(sym->name, sym->name_len);
            log_s("\n");
         }
         continue;
      }
      f->syms[s].out_index       = (int)final_count;
      final_order[final_count++] = s;
   }

   /* From here on the symbol list does not change */
   sym_text = symbol_find(f, CODE_ADDRESS);
   sym_data = symbol_find(f, DATA_ADDRESS);

   /* Convert relocations */
   for (i = 0; i < f->reloc_count; i++)
   {
      relocation_t *rel = &f->relocs[i];
      out_section_t *rs;
      int sym, out_idx;
      uint8_t rela[RELA_SIZE];

      if (rel->target_section == NO_INDEX
            || f->out[rel->target_section].reloc_section == NO_INDEX)
      {
         log_s("Error could not find section for relocation\n");
         free(final_order);
         return false;
      }
      rs = &f->out[f->out[rel->target_section].reloc_section];

      /* The symbol this relocation points to */
      if (rel->symbol == NO_INDEX && rel->symbol_section != NO_INDEX
            && f->data[rel->symbol_section].out != NO_INDEX)
         rel->symbol = f->out[f->data[rel->symbol_section].out].section_symbol;

      sym = rel->symbol;
      if (sym == NO_INDEX)
      {
         log_s("Could not find matching symbol for relocation\n");
         free(final_order);
         return false;
      }
      out_idx = f->syms[sym].out_index;

      if (verbose >= VERBOSE_CONVERT_RELOCATIONS || out_idx == NO_INDEX)
      {
         log_s("Converting relocation #");
         log_hex = 0;
         log_u((uint32_t)i);
         log_hex = 1;
         log_s(" @ 0x");
         log_u(rel->target);
      }
      if (verbose >= VERBOSE_CONVERT_RELOCATIONS && out_idx != NO_INDEX)
      {
         log_s("-> symbol ");
         log_hex = 0;
         log_i(out_idx);
         log_s(" ");
         log_n(f->syms[sym].name, f->syms[sym].name_len);
         log_s("\n");
      }

      /* A symbol not in the table but within DATA or TEXT: those
       * symbols plus an addend */
      if (out_idx == NO_INDEX)
      {
         uint32_t a = f->syms[sym].address;
         if (a >= CODE_ADDRESS && a < DATA_ADDRESS)
         {
            out_idx     = 1;
            rel->addend = a - CODE_ADDRESS;
            rel->symbol = sym_text;
            log_s("-> no symbol, using TEXT\n");
         }
         else if (a >= DATA_ADDRESS && a < WIIU_LOAD_ADDRESS)
         {
            out_idx     = 2;
            rel->addend = a - DATA_ADDRESS;
            rel->symbol = sym_data;
            log_s("-> no symbol, using DATA\n");
         }
         else
         {
            log_s("-> invalid\n");
            log_s("Could not find matching symbol for relocation\n");
            free(final_order);
            return false;
         }
      }

      wr32(rela, rel->target);
      wr32(rela + 4, (rel->type == R_PPC_RELATIVE ? R_PPC_ADDR32 : rel->type)
            | ((uint32_t)out_idx << 8));
      wr32(rela + 8, rel->addend);
      if (!out_append(rs, rela, RELA_SIZE))
      {
         free(final_order);
         return false;
      }
   }

   /* Symbol and string sections */
   if (  (symtab   = out_section_new(f, ".symtab", 7)) == NO_INDEX
      || !out_section_add(f, symtab, false)
      || (strtab   = out_section_new(f, ".strtab", 7)) == NO_INDEX
      || !out_section_add(f, strtab, false)
      || (shstrtab = out_section_new(f, ".shstrtab", 9)) == NO_INDEX
      || !out_section_add(f, shstrtab, false))
   {
      free(final_order);
      return false;
   }

   /* Relocation sections link to the symbol table */
   for (i = 0; i < f->out_count; i++)
   {
      out_section_t *s = &f->out[i];
      if (s->h.type == SHT_RELA)
         s->h.link = (uint32_t)symtab;
      if (s->h.type != SHT_NOBITS)
         s->h.size = (uint32_t)s->data_size;
      if (s->section_symbol != NO_INDEX)
      {
         f->syms[s->section_symbol].address = s->h.addr;
         f->syms[s->section_symbol].size    = s->h.size;
      }
   }

   /* .strtab: every symbol's name, in order */
   {
      out_section_t *s = &f->out[strtab];
      size_t total     = 1;
      for (i = 0; i < final_count; i++)
         if (f->syms[final_order[i]].name_len)
            total += f->syms[final_order[i]].name_len + 1;
      if (!(s->data = (uint8_t*)malloc(total)))
      {
         free(final_order);
         return false;
      }
      s->owned     = 1;
      s->data_cap  = total;
      s->data[0]   = 0;
      s->data_size = 1;
      for (i = 0; i < final_count; i++)
      {
         symbol_t *sym = &f->syms[final_order[i]];
         if (!sym->name_len)
         {
            sym->out_name_pos = 0;
            continue;
         }
         sym->out_name_pos = (uint32_t)s->data_size;
         memcpy(s->data + s->data_size, sym->name, sym->name_len);
         s->data[s->data_size + sym->name_len] = 0;
         s->data_size += sym->name_len + 1;
      }
      shdr_set(&s->h, SHT_STRTAB, SHF_ALLOC, 0, 0xFFFFFFFFu, 0, 0, 1, 0);
      s->h.name = 0;
   }

   /* .symtab */
   {
      out_section_t *s = &f->out[symtab];
      if (!(s->data = (uint8_t*)malloc(final_count * SYM_SIZE + 1)))
      {
         free(final_order);
         return false;
      }
      s->owned    = 1;
      s->data_cap = final_count * SYM_SIZE + 1;
      shdr_set(&s->h, SHT_SYMTAB, SHF_ALLOC, 0, 0xFFFFFFFFu, (uint32_t)strtab,
            0, 4, SYM_SIZE);
      s->h.name = 0;

      for (i = 0; i < final_count; i++)
      {
         const symbol_t *sym = &f->syms[final_order[i]];
         uint8_t *es         = s->data + i * SYM_SIZE;
         out_section_t *ss;

         if (sym->out_section != NO_INDEX)
            sym_hint = sym->out_section;
         else if (sym->in_section != NO_INDEX && f->data[sym->in_section].out != NO_INDEX)
            sym_hint = f->data[sym->in_section].out;
         else if (sym->type == STT_SECTION && sym->address == 0)
            sym_hint = out_section_named(f, sym->name, sym->name_len);
         else
            sym_hint = out_section_at(f, sym->address, sym_hint);

         if (sym_hint == NO_INDEX)
         {
            log_s("Could not find section for symbol ");
            log_n(sym->name, sym->name_len);
            log_s(" 0x");
            log_hex = 1;
            log_u(sym->address);
            log_s("\n");
            free(final_order);
            return false;
         }
         ss = &f->out[sym_hint];

         wr32(es, sym->out_name_pos);
         wr32(es + 4, sym->address);
         wr32(es + 8, sym->size);
         es[12] = (uint8_t)(sym->type | (sym->binding << 4));
         es[13] = 0;
         wr16(es + 14, (uint16_t)ss->index);

         if (verbose >= VERBOSE_OUTPUT_SYMBOLS)
         {
            printf("%08lx %08lx %3u %-12.*s ", (unsigned long)sym->address,
                  (unsigned long)sym->size, (unsigned)(uint16_t)ss->index,
                  (int)ss->name_len, ss->name);
            log_n(sym->name, sym->name_len);
            log_s("\n");
            log_hex = 0;
         }

         /* Compound the symbol into the import section's signature */
         if (ss->h.type == SHT_RPL_IMPORTS && sym->type != STT_SECTION)
            ss->signature = encoding_crc32(ss->signature,
                  f->out[strtab].data + sym->out_name_pos, sym->name_len + 1);
      }
      s->data_size = final_count * SYM_SIZE;
   }
   free(final_order);

   /* Finish the SHT_RPL_IMPORTS signatures */
   memset(zeros, 0, sizeof(zeros));
   for (i = 0; i < f->out_count; i++)
   {
      out_section_t *s = &f->out[i];
      if (s->h.type != SHT_RPL_IMPORTS)
         continue;
      s->signature = encoding_crc32(s->signature, zeros, 0xE);
      wr32(s->data + 4, s->signature);
   }

   /* .shstrtab: every section's name */
   {
      out_section_t *s = &f->out[shstrtab];
      size_t total     = 1;
      for (i = 0; i < f->out_count; i++)
         if (f->out[i].name_len)
            total += f->out[i].name_len + 1;
      if (!(s->data = (uint8_t*)malloc(total)))
         return false;
      s->owned     = 1;
      s->data_cap  = total;
      s->data[0]   = 0;
      s->data_size = 1;
      shdr_set(&s->h, SHT_STRTAB, SHF_ALLOC, 0, 0xFFFFFFFFu, 0, 0, 1, 0);
      s->h.name = 0;
      for (i = 0; i < f->out_count; i++)
      {
         out_section_t *o = &f->out[i];
         if (!o->name_len)
         {
            o->h.name = 0;
            continue;
         }
         o->h.name = (uint32_t)s->data_size;
         memcpy(s->data + s->data_size, o->name, o->name_len);
         s->data[s->data_size + o->name_len] = 0;
         s->data_size += o->name_len + 1;
      }
   }

   /* Addresses of symtab, strtab, shstrtab */
   f->out[symtab].h.addr   = WIIU_LOAD_ADDRESS;
   f->out[symtab].h.size   = (uint32_t)f->out[symtab].data_size;
   f->out[strtab].h.addr   = ALIGN_UP(f->out[symtab].h.addr + predict_sym, 16);
   f->out[strtab].h.size   = (uint32_t)f->out[strtab].data_size;
   f->out[shstrtab].h.addr = ALIGN_UP(f->out[strtab].h.addr + predict_str, 16);
   f->out[shstrtab].h.size = (uint32_t)f->out[shstrtab].data_size;

   /* File info: text, data and load extents, and what the rest needs */
   for (i = 0; i < f->out_count; i++)
   {
      const out_section_t *s = &f->out[i];
      uint32_t size = s->h.type == SHT_NOBITS ? s->h.size : (uint32_t)s->data_size;
      uint32_t end  = s->h.addr + s->h.size;

      if (s->h.addr >= CODE_ADDRESS && s->h.addr < DATA_ADDRESS)
      {
         if (end - CODE_ADDRESS > text_size)
            text_size = end - CODE_ADDRESS;
      }
      else if (s->h.addr >= DATA_ADDRESS && s->h.addr < WIIU_LOAD_ADDRESS)
      {
         if (end - DATA_ADDRESS > data_size)
            data_size = end - DATA_ADDRESS;
      }
      else if (s->h.addr >= WIIU_LOAD_ADDRESS)
      {
         if (end - WIIU_LOAD_ADDRESS > load_size)
            load_size = end - WIIU_LOAD_ADDRESS;
      }
      else if (s->h.addr == 0 && s->h.type != SHT_RPL_CRCS
            && s->h.type != SHT_RPL_FILEINFO)
         temp_size += size + 128;
   }

   if (  (crcs = out_section_new(f, "", 0)) == NO_INDEX
      || (fileinfo = out_section_new(f, "", 0)) == NO_INDEX)
      return false;
   shdr_set(&f->out[crcs].h, SHT_RPL_CRCS, 0, 0, 0xFFFFFFFFu, 0, 0, 4, 4);
   f->out[crcs].h.name = 0;
   shdr_set(&f->out[fileinfo].h, SHT_RPL_FILEINFO, 0, 0, 0xFFFFFFFFu, 0, 0, 4, 0);
   f->out[fileinfo].h.name = 0;
   {
      out_section_t *s = &f->out[fileinfo];
      if (!(s->data = (uint8_t*)calloc(FILEINFO_SIZE, 1)))
         return false;
      s->owned     = 1;
      s->data_size = FILEINFO_SIZE;
      s->data_cap  = FILEINFO_SIZE;
      p            = s->data;
      wr32(p + 0x00, 0xCAFE0402u);                  /* version */
      wr32(p + 0x04, text_size);
      wr32(p + 0x08, 32);                           /* textAlign */
      wr32(p + 0x0C, data_size);
      wr32(p + 0x10, 4096);                         /* dataAlign */
      wr32(p + 0x14, load_size);
      wr32(p + 0x18, 4);                            /* loadAlign */
      wr32(p + 0x1C, temp_size);
      wr32(p + 0x20, 0);                            /* trampAdjust */
      /* These were calculated based on observation, however some games
       * differ. */
      wr32(p + 0x24, ALIGN_UP(DATA_ADDRESS + data_size + heap_size, 64));
      wr32(p + 0x28, ALIGN_UP(DATA_ADDRESS + heap_size, 64));
      wr32(p + 0x2C, 0x10000);                      /* stackSize */
      wr32(p + 0x30, 0);                            /* filename */
      wr32(p + 0x34, RPL_IS_RPX);                   /* flags */
      wr32(p + 0x38, heap_size);
      wr32(p + 0x3C, 0);                            /* tagOffset */
      wr32(p + 0x40, 0x5078);                       /* minVersion */
      wr32(p + 0x44, 0xFFFFFFFFu);                  /* compressionLevel -1 */
      wr32(p + 0x48, 0);                            /* trampAddition */
      wr32(p + 0x4C, 0);                            /* fileInfoPad */
      wr32(p + 0x50, 0x51BA);                       /* cafeSdkVersion */
      wr32(p + 0x54, 0xCCD1);                       /* cafeSdkRevision */
      wr16(p + 0x58, 0);                            /* tlsModuleIndex */
      wr16(p + 0x5A, 0);                            /* tlsAlignShift */
      wr32(p + 0x5C, 0);                            /* runtimeFileInfoSize */
   }

   if (!out_section_add(f, crcs, false) || !out_section_add(f, fileinfo, false))
      return false;

   /* SHT_RPL_CRCS: a CRC of every section's contents, its own empty */
   {
      out_section_t *s = &f->out[crcs];
      size_t count     = f->out_count;
      if (!(s->data = (uint8_t*)malloc(count * 4)))
         return false;
      s->owned    = 1;
      s->data_cap = count * 4;
      for (i = 0; i < count; i++)
      {
         const out_section_t *o = &f->out[i];
         uint32_t crc = (i != (size_t)crcs && o->data_size)
            ? encoding_crc32(0, o->data, o->data_size) : 0;
         wr32(s->data + i * 4, crc);
      }
      s->data_size = count * 4;
   }

   /* Section sizes and offsets */
   shoff       = ALIGN_UP(EHDR_SIZE, 64);
   data_offset = ALIGN_UP(shoff + (uint32_t)f->out_count * SHDR_SIZE, 64);

   for (i = 0; i < f->out_count; i++)
      if (f->out[i].h.type == SHT_RPL_CRCS || f->out[i].h.type == SHT_RPL_FILEINFO)
         data_offset = place(&f->out[i], data_offset);

   for (i = 0; i < f->out_count; i++)
   {
      out_section_t *s = &f->out[i];
      if (s->h.offset != 0xFFFFFFFFu
            || s->h.addr < DATA_ADDRESS || s->h.addr >= WIIU_LOAD_ADDRESS)
         continue;
      if (s->h.type != SHT_NOBITS)
         s->h.size = (uint32_t)s->data_size;
      s->h.offset = data_offset;
      data_offset = ALIGN_UP(data_offset + (uint32_t)s->data_size, 64);
   }

   for (i = 0; i < f->out_count; i++)
   {
      out_section_t *s = &f->out[i];
      if (s->h.offset != 0xFFFFFFFFu || s->h.addr < WIIU_LOAD_ADDRESS)
         continue;
      data_offset = place(s, data_offset);
   }

   for (i = 0; i < f->out_count; i++)
      if (f->out[i].h.offset == 0xFFFFFFFFu)
         data_offset = place(&f->out[i], data_offset);

   /* The file, laid out whole */
   file_size = shoff + (uint32_t)f->out_count * SHDR_SIZE;
   for (i = 0; i < f->out_count; i++)
   {
      const out_section_t *s = &f->out[i];
      if (s->data_size && s->h.offset + (uint32_t)s->data_size > file_size)
         file_size = s->h.offset + (uint32_t)s->data_size;
   }
   if (!(out = (uint8_t*)calloc(file_size, 1)))
      return false;

   wr32(out, ELF_MAGIC);
   out[4] = ELFCLASS32;
   out[5] = ELFDATA2MSB;
   out[6] = EV_CURRENT;
   wr16(out + 7, EABI_CAFE);
   wr16(out + 16, 0xFE01);                      /* type */
   wr16(out + 18, EM_PPC);
   wr32(out + 20, 1);                           /* version */
   wr32(out + 24, f->entry_point);
   wr32(out + 28, 0);                           /* phoff */
   wr32(out + 32, shoff);
   wr32(out + 36, 0);                           /* flags */
   wr16(out + 40, EHDR_SIZE);
   wr16(out + 42, 0);                           /* phentsize */
   wr16(out + 44, 0);                           /* phnum */
   wr16(out + 46, SHDR_SIZE);
   wr16(out + 48, (uint16_t)f->out_count);
   wr16(out + 50, (uint16_t)shstrtab);

   for (i = 0; i < f->out_count; i++)
   {
      const out_section_t *s = &f->out[i];
      p = out + shoff + i * SHDR_SIZE;
      wr32(p,      s->h.name);
      wr32(p + 4,  s->h.type);
      wr32(p + 8,  s->h.flags);
      wr32(p + 12, s->h.addr);
      wr32(p + 16, s->h.offset);
      wr32(p + 20, s->h.size);
      wr32(p + 24, s->h.link);
      wr32(p + 28, s->h.info);
      wr32(p + 32, s->h.addralign);
      wr32(p + 36, s->h.entsize);
      if (s->data_size)
         memcpy(out + s->h.offset, s->data, s->data_size);
   }

   if (!(fp = fopen(path, "wb")))
   {
      printf("Could not open %s for writing\n", path);
      free(out);
      return false;
   }
   if (fwrite(out, 1, file_size, fp) != file_size)
   {
      fclose(fp);
      free(out);
      return false;
   }
   fclose(fp);
   free(out);
   return true;
}

static void elf2rpl_free(elf2rpl_t *f)
{
   size_t i;
   for (i = 0; i < f->out_count; i++)
      if (f->out[i].owned)
         free(f->out[i].data);
   free(f->out);
   free(f->relocs);
   free(f->imports);
   free(f->libs);
   free(f->order);
   free(f->sec_syms);
   free(f->syms);
   free(f->data);
   free(f->in);
   free(f->image);
   if (f->syms_by_addr.vals)
      addr_map_free(&f->syms_by_addr);
   if (f->imports_by_addr.vals)
      addr_map_free(&f->imports_by_addr);
   names_free();
}

int main(int argc, char **argv)
{
   int arg_start = 1;
   int ret       = -1;
   elf2rpl_t f;

   while (arg_start < argc && argv[arg_start][0] == '-')
   {
      if (!strcmp(argv[arg_start], "--"))
      {
         ++arg_start;
         break;
      }
      if (strcmp(argv[arg_start], "-v"))
      {
         printf("Unknown option %s\n", argv[arg_start]);
         return -1;
      }
      ++verbose;
      ++arg_start;
   }

   if (argc < arg_start + 2)
   {
      printf("Usage: %s <src> <dst>\n", argv[0]);
      return -1;
   }

   memset(&f, 0, sizeof(f));
   if (read_elf(&f, argv[arg_start]) && write_rpl(&f, argv[arg_start + 1]))
      ret = 0;
   fflush(stdout);
   elf2rpl_free(&f);
   return ret;
}
