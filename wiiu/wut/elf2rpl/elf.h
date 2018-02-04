#pragma once
#include <cstdint>
#include "be_val.h"

#pragma pack(push, 1)

namespace elf
{

enum Machine : uint32_t /* e_machine */
{
   EM_PPC = 20 /* PowerPC */
};

enum Encoding : uint32_t /* e_encoding */
{
   ELFDATANONE = 0,
   ELFDATA2LSB = 1,
   ELFDATA2MSB = 2
};

enum Class : uint32_t /* e_class */
{
   ELFCLASSNONE = 0,
   ELFCLASS32 = 1,
   ELFCLASS64 = 2
};

enum Version : uint32_t /* e_elf_version */
{
   EV_NONE = 0,
   EV_CURRENT = 1,
};

enum FileType : uint32_t /* e_type */
{
   ET_NONE = 0,         /* No file type */
   ET_REL = 1,          /* Relocatable file */
   ET_EXEC = 2,         /* Executable file */
   ET_DYN = 3,          /* Shared object file */
   ET_CORE = 4,         /* Core file */
   ET_LOPROC = 0xff00,  /* Beginning of processor-specific codes */
   ET_CAFE_RPL = 0xff01, /* Cafe RPL file */
   ET_HIPROC = 0xffff   /* Processor-specific */
};

enum EABI : uint32_t /* e_abi */
{
   EABI_CAFE = 0xcafe   /* WiiU CafeOS */
};

enum SectionFlags : uint32_t /* sh_flags */
{
   SHF_WRITE = 0x1,
   SHF_ALLOC = 0x2,
   SHF_EXECINSTR = 0x4,
   SHF_DEFLATED = 0x08000000,
   SHF_MASKPROC = 0xF0000000,
};

enum SectionType : uint32_t /* sh_type */
{
   SHT_NULL = 0,                 /* No associated section (inactive entry). */
   SHT_PROGBITS = 1,             /* Program-defined contents. */
   SHT_SYMTAB = 2,               /* Symbol table. */
   SHT_STRTAB = 3,               /* String table. */
   SHT_RELA = 4,                 /* Relocation entries; explicit addends. */
   SHT_HASH = 5,                 /* Symbol hash table. */
   SHT_DYNAMIC = 6,              /* Information for dynamic linking. */
   SHT_NOTE = 7,                 /* Information about the file. */
   SHT_NOBITS = 8,               /* Data occupies no space in the file. */
   SHT_REL = 9,                  /* Relocation entries; no explicit addends. */
   SHT_SHLIB = 10,               /* Reserved. */
   SHT_DYNSYM = 11,              /* Symbol table. */
   SHT_INIT_ARRAY = 14,          /* Pointers to initialization functions. */
   SHT_FINI_ARRAY = 15,          /* Pointers to termination functions. */
   SHT_PREINIT_ARRAY = 16,       /* Pointers to pre-init functions. */
   SHT_GROUP = 17,               /* Section group. */
   SHT_SYMTAB_SHNDX = 18,        /* Indices for SHN_XINDEX entries. */
   SHT_LOPROC = 0x70000000,      /* Lowest processor arch-specific type. */
   SHT_HIPROC = 0x7fffffff,      /* Highest processor arch-specific type. */
   SHT_LOUSER = 0x80000000,      /* Lowest type reserved for applications. */
   SHT_RPL_EXPORTS = 0x80000001, /* RPL Exports */
   SHT_RPL_IMPORTS = 0x80000002, /* RPL Imports */
   SHT_RPL_CRCS = 0x80000003,    /* RPL CRCs */
   SHT_RPL_FILEINFO = 0x80000004,/* RPL FileInfo */
   SHT_HIUSER = 0xffffffff       /* Highest type reserved for applications. */
};

enum SymbolBinding : uint32_t /* st_info >> 4 */
{
   STB_LOCAL = 0,       /* Local symbol, not visible outside obj file containing def */
   STB_GLOBAL = 1,      /* Global symbol, visible to all object files being combined */
   STB_WEAK = 2,        /* Weak symbol, like global but lower-precedence */
   STB_GNU_UNIQUE = 10,
   STB_LOOS = 10,       /* Lowest operating system-specific binding type */
   STB_HIOS = 12,       /* Highest operating system-specific binding type */
   STB_LOPROC = 13,     /* Lowest processor-specific binding type */
   STB_HIPROC = 15      /* Highest processor-specific binding type */
};

enum SymbolType : uint32_t /* st_info & f */
{
   STT_NOTYPE = 0,      /* Symbol's type is not specified */
   STT_OBJECT = 1,      /* Symbol is a data object (variable, array, etc.) */
   STT_FUNC = 2,        /* Symbol is executable code (function, etc.) */
   STT_SECTION = 3,     /* Symbol refers to a section */
   STT_FILE = 4,        /* Local, absolute symbol that refers to a file */
   STT_COMMON = 5,      /* An uninitialized common block */
   STT_TLS = 6,         /* Thread local data object */
   STT_LOOS = 7,        /* Lowest operating system-specific symbol type */
   STT_HIOS = 8,        /* Highest operating system-specific symbol type */
   STT_GNU_IFUNC = 10,  /* GNU indirect function */
   STT_LOPROC = 13,     /* Lowest processor-specific symbol type */
   STT_HIPROC = 15      /* Highest processor-specific symbol type */
};

enum SectionIndex : uint16_t /* st_shndx */
{
   SHN_UNDEF = 0,        /* Undefined */
   SHN_LORESERVE = 0xff00,   /* Reserved range */
   SHN_ABS = 0xfff1,   /* Absolute symbols */
   SHN_COMMON = 0xfff2,   /* Common symbols */
   SHN_XINDEX = 0xffff,   /* Escape -- index stored elsewhere */
   SHN_HIRESERVE = 0xffff
};

enum RelocationType : uint32_t /* r_info & 0xff */
{
   R_PPC_NONE = 0,
   R_PPC_ADDR32 = 1,
   R_PPC_ADDR24 = 2,
   R_PPC_ADDR16 = 3,
   R_PPC_ADDR16_LO = 4,
   R_PPC_ADDR16_HI = 5,
   R_PPC_ADDR16_HA = 6,
   R_PPC_ADDR14 = 7,
   R_PPC_ADDR14_BRTAKEN = 8,
   R_PPC_ADDR14_BRNTAKEN = 9,
   R_PPC_REL24 = 10,
   R_PPC_REL14 = 11,
   R_PPC_REL14_BRTAKEN = 12,
   R_PPC_REL14_BRNTAKEN = 13,
   R_PPC_GOT16 = 14,
   R_PPC_GOT16_LO = 15,
   R_PPC_GOT16_HI = 16,
   R_PPC_GOT16_HA = 17,
   R_PPC_PLTREL24 = 18,
   R_PPC_JMP_SLOT = 21,
   R_PPC_RELATIVE = 22,
   R_PPC_LOCAL24PC = 23,
   R_PPC_REL32 = 26,
   R_PPC_TLS = 67,
   R_PPC_DTPMOD32 = 68,
   R_PPC_TPREL16 = 69,
   R_PPC_TPREL16_LO = 70,
   R_PPC_TPREL16_HI = 71,
   R_PPC_TPREL16_HA = 72,
   R_PPC_TPREL32 = 73,
   R_PPC_DTPREL16 = 74,
   R_PPC_DTPREL16_LO = 75,
   R_PPC_DTPREL16_HI = 76,
   R_PPC_DTPREL16_HA = 77,
   R_PPC_DTPREL32 = 78,
   R_PPC_GOT_TLSGD16 = 79,
   R_PPC_GOT_TLSGD16_LO = 80,
   R_PPC_GOT_TLSGD16_HI = 81,
   R_PPC_GOT_TLSGD16_HA = 82,
   R_PPC_GOT_TLSLD16 = 83,
   R_PPC_GOT_TLSLD16_LO = 84,
   R_PPC_GOT_TLSLD16_HI = 85,
   R_PPC_GOT_TLSLD16_HA = 86,
   R_PPC_GOT_TPREL16 = 87,
   R_PPC_GOT_TPREL16_LO = 88,
   R_PPC_GOT_TPREL16_HI = 89,
   R_PPC_GOT_TPREL16_HA = 90,
   R_PPC_GOT_DTPREL16 = 91,
   R_PPC_GOT_DTPREL16_LO = 92,
   R_PPC_GOT_DTPREL16_HI = 93,
   R_PPC_GOT_DTPREL16_HA = 94,
   R_PPC_TLSGD = 95,
   R_PPC_TLSLD = 96,
   R_PPC_EMB_SDA21 = 109,
   R_PPC_REL16 = 249,
   R_PPC_REL16_LO = 250,
   R_PPC_REL16_HI = 251,
   R_PPC_REL16_HA = 252,
};

enum RplFileInfoFlag : uint32_t
{
   RPL_IS_RPX = 0x2,
};

static const unsigned HeaderMagic = 0x7f454c46;

struct Header
{
   be_val<uint32_t> magic;       /* File identification. */
   be_val<uint8_t> fileClass;    /* File class. */
   be_val<uint8_t> encoding;     /* Data encoding. */
   be_val<uint8_t> elfVersion;   /* File version. */
   be_val<uint16_t> abi;         /* OS/ABI identification. (EABI_*) */
   be_val<uint8_t> pad[7];

   be_val<uint16_t> type;        /* Type of file (ET_*) */
   be_val<uint16_t> machine;     /* Required architecture for this file (EM_*) */
   be_val<uint32_t> version;     /* Must be equal to 1 */
   be_val<uint32_t> entry;       /* Address to jump to in order to start program */
   be_val<uint32_t> phoff;       /* Program header table's file offset, in bytes */
   be_val<uint32_t> shoff;       /* Section header table's file offset, in bytes */
   be_val<uint32_t> flags;       /* Processor-specific flags */
   be_val<uint16_t> ehsize;      /* Size of ELF header, in bytes */
   be_val<uint16_t> phentsize;   /* Size of an entry in the program header table */
   be_val<uint16_t> phnum;       /* Number of entries in the program header table */
   be_val<uint16_t> shentsize;   /* Size of an entry in the section header table */
   be_val<uint16_t> shnum;       /* Number of entries in the section header table */
   be_val<uint16_t> shstrndx;    /* Sect hdr table index of sect name string table */
};
CHECK_SIZE(Header, 0x34);

struct SectionHeader
{
   be_val<uint32_t> name;      /* Section name (index into string table) */
   be_val<uint32_t> type;      /* Section type (SHT_*) */
   be_val<uint32_t> flags;     /* Section flags (SHF_*) */
   be_val<uint32_t> addr;      /* Address where section is to be loaded */
   be_val<uint32_t> offset;    /* File offset of section data, in bytes */
   be_val<uint32_t> size;      /* Size of section, in bytes */
   be_val<uint32_t> link;      /* Section type-specific header table index link */
   be_val<uint32_t> info;      /* Section type-specific extra information */
   be_val<uint32_t> addralign; /* Section address alignment */
   be_val<uint32_t> entsize;   /* Size of records contained within the section */
};
CHECK_SIZE(SectionHeader, 0x28);

struct Symbol
{
   be_val<uint32_t> name;  /* Symbol name (index into string table) */
   be_val<uint32_t> value; /* Value or address associated with the symbol */
   be_val<uint32_t> size;  /* Size of the symbol */
   be_val<uint8_t>  info;  /* Symbol's type and binding attributes */
   be_val<uint8_t>  other; /* Must be zero; reserved */
   be_val<uint16_t> shndx; /* Which section (header table index) it's defined in (SHN_*) */
};
CHECK_SIZE(Symbol, 0x10);

struct Rela
{
   be_val<uint32_t> offset;
   be_val<uint32_t> info;
   be_val<int32_t> addend;
};
CHECK_SIZE(Rela, 0x0C);

struct RplImport
{
   be_val<uint32_t> count;
   be_val<uint32_t> signature;
   char name[1];
};

struct RplExport
{
   struct Export
   {
      be_val<uint32_t> value;
      be_val<uint32_t> name;
   };

   be_val<uint32_t> count;
   be_val<uint32_t> signature;
   Export exports[1];
};

struct RplCrc
{
   be_val<uint32_t> crc;
};
CHECK_SIZE(RplCrc, 0x04);

struct RplFileInfo
{
   be_val<uint32_t> version;
   be_val<uint32_t> textSize;
   be_val<uint32_t> textAlign;
   be_val<uint32_t> dataSize;
   be_val<uint32_t> dataAlign;
   be_val<uint32_t> loadSize;
   be_val<uint32_t> loadAlign;
   be_val<uint32_t> tempSize;
   be_val<uint32_t> trampAdjust;
   be_val<uint32_t> sdaBase;
   be_val<uint32_t> sda2Base;
   be_val<uint32_t> stackSize;
   be_val<uint32_t> filename;
   be_val<uint32_t> flags;
   be_val<uint32_t> heapSize;
   be_val<uint32_t> tagOffset;
   be_val<uint32_t> minVersion;
   be_val<int32_t> compressionLevel;
   be_val<uint32_t> trampAddition;
   be_val<uint32_t> fileInfoPad;
   be_val<uint32_t> cafeSdkVersion;
   be_val<uint32_t> cafeSdkRevision;
   be_val<uint16_t> tlsModuleIndex;
   be_val<uint16_t> tlsAlignShift;
   be_val<uint32_t> runtimeFileInfoSize;
};
CHECK_SIZE(RplFileInfo, 0x60);

} /* namespace elf */

#pragma pack(pop)
