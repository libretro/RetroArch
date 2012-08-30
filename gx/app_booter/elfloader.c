/*
 * Copyright (c) 2001 William L. Pitts
 * Modifications (c) 2004 Felix Domke
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are freely
 * permitted provided that the above copyright notice and this
 * paragraph and the following disclaimer are duplicated in all
 * such forms.
 *
 * This software is provided "AS IS" and without any express or
 * implied warranties, including, without limitation, the implied
 * warranties of merchantability and fitness for a particular
 * purpose.
 */

#include <stdio.h>

#include "elf_abi.h"
#include "sync.h"
#include "string.h"

/* ======================================================================
 * Determine if a valid ELF image exists at the given memory location.
 * First looks at the ELF header magic field, the makes sure that it is
 * executable and makes sure that it is for a PowerPC.
 * ====================================================================== */
s32 valid_elf_image (void *addr)
{
        Elf32_Ehdr *ehdr; /* Elf header structure pointer */

        ehdr = (Elf32_Ehdr *) addr;

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

u32 load_elf_image (void *elfstart) {
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdrs;
	u8 *image;
	int i;

	ehdr = (Elf32_Ehdr *) elfstart;

	if(ehdr->e_phoff == 0 || ehdr->e_phnum == 0)
		return 0;

	if(ehdr->e_phentsize != sizeof(Elf32_Phdr))
		return 0;

	phdrs = (Elf32_Phdr*)(elfstart + ehdr->e_phoff);

	for(i=0;i<ehdr->e_phnum;i++) {

		if(phdrs[i].p_type != PT_LOAD)
			continue;

		phdrs[i].p_paddr &= 0x3FFFFFFF;
		phdrs[i].p_paddr |= 0x80000000;

		if(phdrs[i].p_filesz > phdrs[i].p_memsz)
			return 0;

		if(!phdrs[i].p_filesz)
			continue;

		image = (u8 *) (elfstart + phdrs[i].p_offset);
		memcpy ((void *) phdrs[i].p_paddr, (const void *) image, phdrs[i].p_filesz);

		sync_before_exec ((void *) phdrs[i].p_paddr, phdrs[i].p_memsz);

		//if(phdrs[i].p_flags & PF_X)
			//ICInvalidateRange ((void *) phdrs[i].p_paddr, phdrs[i].p_memsz);
	}

	return ((ehdr->e_entry & 0x3FFFFFFF) | 0x80000000);
}
