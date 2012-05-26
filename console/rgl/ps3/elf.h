#ifndef ELF_H
#define ELF_H 1

#define RGL_ALIGN_FAST_TRANSFER 128

#define EI_MAG0		0
#define EI_MAG1		1
#define EI_MAG2		2
#define EI_MAG3		3
#define EI_CLASS	4
#define EI_DATA		5
#define EI_VERSION	6
#define EI_OSABI	7
#define EI_ABIVERSION	8
#define EI_PAD		9
#define EI_NIDENT	16

#define ELFMAG0		0x7f
#define ELFMAG1		'E'
#define ELFMAG2		'L'
#define ELFMAG3		'F'

#define ELFCLASSNONE	0
#define ELFCLASS32	1
#define ELFCLASS64	2

#define ELFDATANONE	0
#define ELFDATA2LSB	1
#define ELFDATA2MSB	2

#define EV_NONE		0
#define EV_CURRENT	1

#define ET_NONE		0
#define ET_REL		1
#define ET_EXEC		2
#define ET_DYN		3
#define ET_CORE		4

#define EM_PPC		20
#define EM_PPC64	21

typedef struct {
  unsigned char	e_ident[EI_NIDENT];
  unsigned short	e_type;
  unsigned short	e_machine;
  unsigned int	e_version;
  unsigned int	e_entry;
  unsigned int	e_phoff;
  unsigned int	e_shoff;
  unsigned int	e_flags;
  unsigned short	e_ehsize;
  unsigned short	e_phentsize;
  unsigned short	e_phnum;
  unsigned short	e_shentsize;
  unsigned short	e_shnum;
  unsigned short	e_shstrndx;
} Elf32_Ehdr;

#define PT_NULL		0
#define PT_LOAD		1
#define PT_DYNAMIC	2
#define PT_INTERP	3
#define PT_NOTE		4
#define PT_SHLIB	5
#define PT_PHDR		6
#define PT_LOOS		0x60000000
#define PT_HIOS		0x6fffffff
#define PT_LOPROC	0x70000000
#define PT_HIPROC	0x7fffffff

typedef struct {
  unsigned int	p_type;
  unsigned int	p_offset;
  unsigned int	p_vaddr;
  unsigned int	p_paddr;
  unsigned int	p_filesz;
  unsigned int	p_memsz;
  unsigned int	p_flags;
  unsigned int	p_align;
} Elf32_Phdr;

#define SHT_NULL	0
#define SHT_PROGBITS	1
#define SHT_SYMTAB	2
#define SHT_STRTAB	3
#define SHT_RELA	4
#define SHT_HASH	5
#define SHT_DYNAMIC	6
#define SHT_NOTE	7
#define SHT_NOBITS	8
#define SHT_REL		9
#define SHT_SHLIB	10
#define SHT_DYNSYM	11

#define SHF_WRITE	     (1 << 0)
#define SHF_ALLOC	     (1 << 1)
#define SHF_EXECINSTR	     (1 << 2)
#define SHF_MERGE	     (1 << 4)
#define SHF_STRINGS	     (1 << 5)
#define SHF_INFO_LINK	     (1 << 6)
#define SHF_LINK_ORDER	     (1 << 7)
#define SHF_OS_NONCONFORMING (1 << 8)
#define SHF_MASKOS	     0x0ff00000
#define SHF_MASKPROC	     0xf0000000


typedef struct {
  unsigned int	sh_name;
  unsigned int	sh_type;
  unsigned int	sh_flags;
  unsigned int	sh_addr;
  unsigned int	sh_offset;
  unsigned int	sh_size;
  unsigned int	sh_link;
  unsigned int	sh_info;
  unsigned int	sh_addralign;
  unsigned int	sh_entsize;
} Elf32_Shdr;

#define SHN_UNDEF	0
#define SHN_LORESERVE	0xff00
#define SHN_LOPROC	0xff00
#define SHN_HIPROC	0xff1f
#define SHN_LOOS	0xff20
#define SHN_HIOS	0xff3f
#define SHN_ABS		0xfff1
#define SHN_COMMON	0xfff2
#define SHN_XINDEX	0xffff
#define SHN_HIRESERVE	0xffff

typedef struct {
  unsigned int	st_name;
  unsigned int	st_value;
  unsigned int	st_size;
  unsigned char	st_info;
  unsigned char	st_other;
  unsigned short	st_shndx;
} Elf32_Sym;


typedef struct
{
  unsigned int	r_offset;
  unsigned int	r_info;
} Elf32_Rel;

typedef struct
{
  unsigned int	r_offset;
  unsigned int	r_info;
  int	r_addend;
} Elf32_Rela;

#endif /* ELF_H */
