#ifndef _ELFLOADER_H_
#define _ELFLOADER_H_

s32 valid_elf_image (void *addr);
u32 load_elf_image (void *addr);

#endif
