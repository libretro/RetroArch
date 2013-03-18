/*
 * Spu simulator library - ELF reader.
 * AUTHORS: Antoine Labour, Robin Green
 * DATE: 2003-Oct-16
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "readelf.h"

const Elf32_Sym *getSymbolByIndex(const ELF_t *elf,int idx)
{
   if(!elf)
      return NULL;

   if(elf->symbolsSection<=0)
      return NULL;

   ELF_section_t *section=elf->sections+elf->symbolsSection;

   if(!section->data)
      return NULL;

   if ((idx<0) || (idx>=(int)elf->symbolCount))
      return NULL;

   return(Elf32_Sym *)(section->data+section->header.sh_entsize*idx);
}

int lookupSymbol(const ELF_t *elf,const char *name)
{
   unsigned int i;

   if(!elf||!name)
      return -1;

   ELF_symbol_t *symbol=elf->symbols;

   if(!symbol)
      return -1;

   for (i = 0; i < elf->symbolCount; ++i, ++symbol)
      if (strcmp(name,symbol->name)==0) 
         return i;
   return -1;
}

static const char *_getStringTable(const Elf32_Ehdr *ehdr)
{
   const char *sectionHeaderStart = (const char*)ehdr + ehdr->e_shoff;
   const Elf32_Shdr *shstrtabHeader = (const Elf32_Shdr*)sectionHeaderStart + ehdr->e_shstrndx;
   return (const char*)ehdr + shstrtabHeader->sh_offset;
}

const char *findSectionInPlace(const char* memory, const char *name, size_t *sectionSize)
{
   const Elf32_Ehdr *ehdr = (const Elf32_Ehdr*)memory;

   //first get the string section header
   const char *shstrtab = _getStringTable(ehdr);

   //find the section
   size_t sectionCount = ehdr->e_shnum;
   const char *sectionHeaderStart = (const char*)ehdr + ehdr->e_shoff;

   for (size_t i=0;i<sectionCount;i++)
   {
      const Elf32_Shdr *sectionHeader = (const Elf32_Shdr *)sectionHeaderStart + i;
      const char *sectionName = shstrtab + sectionHeader->sh_name;
      if (!strcmp(name,sectionName))
      {
         *sectionSize = sectionHeader->sh_size;
         return (const char*)ehdr + sectionHeader->sh_offset;
      }
   }
   return NULL;
}

const char *findSymbolSectionInPlace(const char *memory, size_t *symbolSize, size_t *symbolCount, const char **symbolstrtab)
{
   const Elf32_Ehdr *ehdr = (const Elf32_Ehdr*)memory;

   //find the section
   size_t sectionCount = ehdr->e_shnum;
   const char *sectionHeaderStart = (const char*)ehdr + ehdr->e_shoff;

   for (size_t i = 0; i < sectionCount; i++)
   {
      const Elf32_Shdr *sectionHeader = (const Elf32_Shdr *)sectionHeaderStart + i;
      if (sectionHeader->sh_type == SHT_SYMTAB)
      {
         *symbolSize = sectionHeader->sh_entsize;
         *symbolCount = sectionHeader->sh_size / sectionHeader->sh_entsize;

         const Elf32_Shdr *symbolStrHeader = (const Elf32_Shdr *)sectionHeaderStart + sectionHeader->sh_link;
         *symbolstrtab = (const char*)ehdr + symbolStrHeader->sh_offset;
         return (const char*)ehdr + sectionHeader->sh_offset;
      }
   }

   return NULL;
}


int lookupSymbolValueInPlace(const char* symbolSection, size_t symbolSize, size_t symbolCount, const char *symbolstrtab, const char *name)
{
   for (size_t i=0;i<symbolCount;i++)
   {
      Elf32_Sym* elf_sym = (Elf32_Sym*)symbolSection;

      if (!strcmp(symbolstrtab + elf_sym->st_name, name))
         return elf_sym->st_value;
      symbolSection+= symbolSize;
   }
   return -1;
}

const char *getSymbolByIndexInPlace(const char* symbolSection, size_t symbolSize, size_t symbolCount,  const char *symbolstrtab, int index)
{
   Elf32_Sym* elf_sym = (Elf32_Sym*)symbolSection + index;
   return symbolstrtab + elf_sym->st_name;
}
