/* Copyright 2011 Dimok
   This code is licensed to you under the terms of the GNU GPL, version 2;
   see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt  */
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include "string.h"

#include <gccore.h>

#include "elf_abi.h"

#define SYSTEM_ARGV           ((struct __argv *) 0x93200000)

#define EXECUTABLE_MEM_ADDR   0x91800000
#define ARENA1_HI_LIMIT       0x81800000

typedef struct _dolheade
{
   uint32_t text_pos[7];
   uint32_t data_pos[11];
   uint32_t text_start[7];
   uint32_t data_start[11];
   uint32_t text_size[7];
   uint32_t data_size[11];
   uint32_t bss_start;
   uint32_t bss_size;
   uint32_t entry_point;
} dolheader;

typedef void (*entrypoint)(void);

static void sync_before_exec(const void *p, uint32_t len)
{
   uint32_t a = (uint32_t)p & ~0x1f;
   uint32_t b = ((uint32_t)p + len + 0x1f) & ~0x1f;

   for ( ; a < b; a += 32)
      asm("dcbst 0,%0 ; sync ; icbi 0,%0" : : "b"(a));

   asm("sync ; isync");
}

/* ======================================================================
 * Determine if a valid ELF image exists at the given memory location.
 * First looks at the ELF header magic field, the makes sure that it is
 * executable and makes sure that it is for a PowerPC.
 * ====================================================================== */
static int32_t valid_elf_image (void *addr)
{
   Elf32_Ehdr *ehdr = (Elf32_Ehdr*)addr;

   if (!IS_ELF (*ehdr))
      return 0;

   if (ehdr->e_type != ET_EXEC)
      return -1;

   if (ehdr->e_machine != EM_PPC)
      return -1;

   return 1;
}

/* ======================================================================
 * A very simple elf loader, assumes the image is valid, returns the
 * entry point address.
 * ====================================================================== */

static uint32_t load_elf_image (void *elfstart)
{
   int i;
   Elf32_Phdr *phdrs = NULL;
   Elf32_Ehdr *ehdr  = (Elf32_Ehdr*) elfstart;

   if(ehdr->e_phoff == 0 || ehdr->e_phnum == 0)
      return 0;

   if(ehdr->e_phentsize != sizeof(Elf32_Phdr))
      return 0;

   phdrs = (Elf32_Phdr*)(elfstart + ehdr->e_phoff);

   for (i = 0; i < ehdr->e_phnum; i++)
   {
      uint8_t *image;

      if(phdrs[i].p_type != PT_LOAD)
         continue;

      phdrs[i].p_paddr &= 0x3FFFFFFF;
      phdrs[i].p_paddr |= 0x80000000;

      if(phdrs[i].p_filesz > phdrs[i].p_memsz)
         return 0;

      if(!phdrs[i].p_filesz)
         continue;

      image = (uint8_t*)(elfstart + phdrs[i].p_offset);
      memcpy ((void *) phdrs[i].p_paddr, (const void *) image, phdrs[i].p_filesz);

      sync_before_exec ((void *) phdrs[i].p_paddr, phdrs[i].p_memsz);
   }

   return ((ehdr->e_entry & 0x3FFFFFFF) | 0x80000000);
}

static uint32_t load_dol_image(const void *dolstart)
{
	uint32_t i;
   dolheader *dolfile = NULL;

	if(!dolstart)
		return 0;

	dolfile = (dolheader *) dolstart;

	for (i = 0; i < 7; i++)
	{
		if ((!dolfile->text_size[i]) || (dolfile->text_start[i] < 0x100))
			continue;

		memcpy((void *) dolfile->text_start[i], dolstart + dolfile->text_pos[i], dolfile->text_size[i]);
		sync_before_exec((void *) dolfile->text_start[i], dolfile->text_size[i]);
	}

	for (i = 0; i < 11; i++)
	{
		if ((!dolfile->data_size[i]) || (dolfile->data_start[i] < 0x100))
			continue;

		memcpy((void *) dolfile->data_start[i], dolstart + dolfile->data_pos[i], dolfile->data_size[i]);
		sync_before_exec((void *) dolfile->data_start[i], dolfile->data_size[i]);
	}

	return dolfile->entry_point;
}

/* if we name this main, GCC inserts the __eabi symbol,
 * even when we specify -mno-eabi. */

void app_booter_main(void)
{
	entrypoint exeEntryPoint;
	uint32_t exeEntryPointAddress = 0;
	void *exeBuffer = (void*)EXECUTABLE_MEM_ADDR;

	if (valid_elf_image(exeBuffer) == 1)
		exeEntryPointAddress = load_elf_image(exeBuffer);
	else
		exeEntryPointAddress = load_dol_image(exeBuffer);

	exeEntryPoint = (entrypoint) exeEntryPointAddress;

	if (!exeEntryPoint)
		return;

	if (SYSTEM_ARGV->argvMagic == ARGV_MAGIC)
	{
		void *new_argv = (void*)(exeEntryPointAddress + 8);
		memcpy(new_argv, SYSTEM_ARGV, sizeof(struct __argv));
		sync_before_exec(new_argv, sizeof(struct __argv));
	}

	exeEntryPoint();
}
