#ifndef _CGC_CGBIO_CGBDEFS_HPP
#define _CGC_CGBIO_CGBDEFS_HPP

// Platform depended definitions:
typedef unsigned int	Elf32_Addr;
typedef unsigned short	Elf32_Half;
typedef short		Elf32_Shalf;
typedef unsigned int	Elf32_Off;
typedef signed   int	Elf32_Sword;
typedef unsigned int	Elf32_Word;

typedef unsigned short	Elf64_Half;
typedef short		Elf64_Shalf;

///////////////////////
// ELF Header Constants

// File type
#define ET_NONE        0
#define ET_REL         1
#define ET_EXEC        2
#define ET_DYN         3
#define ET_CORE        4
#define ET_LOOS   0xFE00
#define ET_HIOS   0xFEFF
#define ET_LOPROC 0xFF00
#define ET_HIPROC 0xFFFF

// Machine/Architecture
#define EM_NONE         0 // No machine
#define EM_RSX		0x528e // RSX

// Machine/Architecture flags
#define EM_RSX_NONE	0

// Machine/Architecture ABI version
#define EI_ABIVERSION_RSX	1

// ELF File version
#define EV_NONE    0
#define EV_CURRENT 1

// Identification index
#define EI_MAG0        0
#define EI_MAG1        1
#define EI_MAG2        2
#define EI_MAG3        3
#define EI_CLASS       4
#define EI_DATA        5
#define EI_VERSION     6
#define EI_OSABI       7
#define EI_ABIVERSION  8
#define EI_PAD         9
#define EI_NIDENT     16

// Magic number

#ifndef ELFMAG0
#define ELFMAG0 0x7F
#endif

#define ELFMAG1  'E'
#define ELFMAG2  'L'
#define ELFMAG3  'F'

// File class
#define ELFCLASSNONE 0
#define ELFCLASS32   1
#define ELFCLASS64   2

// Encoding
#define ELFDATANONE 0
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

// OS extensions
#define ELFOSABI_NONE	    0 // No extensions or unspecified
#define ELFOSABI_HPUX       1 // Hewlett-Packard HP-UX
#define ELFOSABI_NETBSD     2 // NetBSD
#define ELFOSABI_LINUX      3 // Linux
#define ELFOSABI_SOLARIS    6 // Sun Solaris
#define ELFOSABI_AIX        7 // AIX
#define ELFOSABI_IRIX       8 // IRIX
#define ELFOSABI_FREEBSD    9 // FreeBSD
#define ELFOSABI_TRU64      10 // Compaq TRU64 UNIX
#define ELFOSABI_MODESTO    11 // Novell Modesto
#define ELFOSABI_OPENBSD    12 // Open BSD
#define ELFOSABI_CGRUNTIME  19 // Cg Run-Time



/////////////////////
// Sections constants

// Section indexes
#ifndef SHN_UNDEF
#define SHN_UNDEF          0
#endif

#ifndef SHN_LORESERVE
#define SHN_LORESERVE 0xFF00
#endif

#ifndef SHN_LOPROC
#define SHN_LOPROC    0xFF00
#endif

#ifndef SHN_HIPROC
#define SHN_HIPROC    0xFF1F
#endif

#ifndef SHN_LOOS
#define SHN_LOOS      0xFF20
#endif

#ifndef SHN_HIOS
#define SHN_HIOS      0xFF3F
#endif

#ifndef SHN_ABS
#define SHN_ABS       0xFFF1
#endif

#ifndef SHN_COMMON
#define SHN_COMMON    0xFFF2
#endif

#ifndef SHN_XINDEX
#define SHN_XINDEX    0xFFFF
#endif

#ifndef SHN_HIRESERVE
#define SHN_HIRESERVE 0xFFFF
#endif

// Section types
#define SHT_NULL                   0
#define SHT_PROGBITS               1
#define SHT_SYMTAB                 2
#define SHT_STRTAB                 3
#define SHT_RELA                   4
#define SHT_HASH                   5
#define SHT_DYNAMIC                6
#define SHT_NOTE                   7
#define SHT_NOBITS                 8
#define SHT_REL                    9
#define SHT_SHLIB                 10
#define SHT_DYNSYM                11
#define SHT_INIT_ARRAY            14
#define SHT_FINI_ARRAY            15
#define SHT_PREINIT_ARRAY         16
#define SHT_GROUP                 17
#define SHT_SYMTAB_SHNDX          18
#define SHT_LOOS          0x60000000
#define SHT_HIOS          0x6fffffff
#define SHT_LOPROC        0x70000000
#define SHT_RSX_PARAM	  0x70000000
#define SHT_RSX_SHADERTAB 0x70000001
#define SHT_RSX_FXTAB     0x70000002
#define SHT_RSX_ANNOTATE  0x70000003
#define SHT_HIPROC        0x7FFFFFFF
#define SHT_LOUSER        0x80000000
#define SHT_HIUSER        0xFFFFFFFF

// Section flags
#ifndef SHF_WRITE
#define SHF_WRITE                   0x1
#endif

#ifndef SHF_ALLOC
#define SHF_ALLOC                   0x2
#endif

#ifndef SHF_EXECINSTR
#define SHF_EXECINSTR               0x4
#endif

#ifndef SHF_MERGE
#define SHF_MERGE                  0x10
#endif

#ifndef SHF_STRINGS
#define SHF_STRINGS                0x20
#endif

#ifndef SHF_INFO_LINK
#define SHF_INFO_LINK              0x40
#endif

#ifndef SHF_LINK_ORDER
#define SHF_LINK_ORDER             0x80
#endif

#ifndef SHF_OS_NONCONFORMING
#define SHF_OS_NONCONFORMING      0x100
#endif

#define SHF_GROUP                 0x200
#define SHF_TLS                   0x400
#define SHF_MASKOS           0x0ff00000

#ifndef SHF_MASKPROC
#define SHF_MASKPROC         0xF0000000
#endif

// Section group flags
#define GRP_COMDAT          0x1
#define GRP_MASKOS   0x0ff00000
#define GRP_MASKPROC 0xf0000000

// Symbol binding
#define STB_LOCAL   0
#define STB_GLOBAL  1
#define STB_WEAK    2
#define STB_LOOS   10
#define STB_HIOS   12
#define STB_LOPROC 13
#define STB_HIPROC 15

// Symbol types
#define STT_NOTYPE   0
#define STT_OBJECT   1
#define STT_FUNC     2
#define STT_SECTION  3
#define STT_FILE     4
#define STT_COMMON   5
#define STT_TLS      6
#define STT_LOOS    10
#define STT_HIOS    12
#define STT_LOPROC  13
#define STT_HIPROC  15

// Symbol visibility
#define STV_DEFAULT   0
#define STV_INTERNAL  1
#define STV_HIDDEN    2
#define STV_PROTECTED 3

// Shader Function type (st_other field)
#define STO_RSX_SHADER 0
#define STO_RSX_EFFECT 1

// Undefined name
#define STN_UNDEF 0

// Relocation types
#define R_RSX_NONE      0
#define R_RSX_FLOAT4	1

/* Note header in a PT_NOTE section */
struct Elf32_Note {
	Elf32_Word	n_namesz;	/* Name size */
	Elf32_Word	n_descsz;	/* Content size */
	Elf32_Word	n_type;		/* Content type */
};


// Relocation entries

// Dynamic structure
struct Elf32_Dyn {
    Elf32_Sword d_tag;
    union {
        Elf32_Word d_val;
        Elf32_Addr d_ptr;
    } d_un;
};


#endif // CGC_CGBIO_CGBDEFS_HPP
