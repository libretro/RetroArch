#include <algorithm>
#include <fstream>
#include <iterator>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <zlib.h>
#include "elf.h"

#pragma pack(push, 1)

struct RplLibsDef
{
   be_val<uint32_t> name;
   be_val<uint32_t> stubStart;
   be_val<uint32_t> stubEnd;
};

#pragma pack(pop)

static const uint32_t LoadAddress = 0x01000000u;
static const uint32_t CodeAddress = 0x02000000u;
static const uint32_t DataAddress = 0x10000000u;
static const uint32_t WiiuLoadAddress = 0xC0000000u;

struct ElfFile
{
   struct Symbol
   {
      std::string name;
      uint32_t address;
      uint32_t size;
      elf::SymbolType type;
      elf::SymbolBinding binding;
      uint32_t outNamePos;
   };

   struct Relocation
   {
      uint32_t target;
      elf::RelocationType type;

      Symbol *symbol;
      uint32_t addend;
   };

   struct DataSection
   {
      std::string name;
      uint32_t address;
      elf::SectionType type;
      elf::SectionFlags flags;

      // Data used if type == SHT_PROGBITS
      std::vector<char> data;

      // Size used if type == SHT_NOBITS
      uint32_t size;
   };

   struct RplImport
   {
      Symbol *trampSymbol;
      Symbol *stubSymbol;
      uint32_t stubAddr;
      uint32_t trampAddr;
   };

   struct RplImportLibrary
   {
      std::string name;
      std::vector<std::unique_ptr<RplImport>> imports;
   };

   uint32_t entryPoint;
   std::vector<std::unique_ptr<DataSection>> dataSections;
   std::vector<std::unique_ptr<Symbol>> symbols;
   std::vector<std::unique_ptr<Relocation>> relocations;
   std::vector<std::unique_ptr<RplImportLibrary>> rplImports;
};

struct InputSection
{
   elf::SectionHeader header;
   std::vector<char> data;
};

static ElfFile::Symbol *
findSymbol(ElfFile &file, uint32_t address)
{
   for (auto &symbol : file.symbols) {
      if (symbol->address == address && symbol->type != elf::STT_NOTYPE) {
         return symbol.get();
      }
   }

   for (auto &symbol : file.symbols) {
      if (symbol->address == address) {
         return symbol.get();
      }
   }

   return nullptr;
}

static ElfFile::RplImport *
findImport(ElfFile &file, uint32_t address)
{
   for (auto &lib : file.rplImports) {
      for (auto &import : lib->imports) {
         if (import->stubAddr == address || import->trampAddr == address) {
            return import.get();
         }
      }
   }

   return nullptr;
}

template<typename Type>
static Type *
getLoaderDataPtr(std::vector<InputSection> &inSections, uint32_t address)
{
   for (auto &section : inSections) {
      auto start = section.header.addr;
      auto end = start + section.data.size();

      if (start <= address && end > address) {
         auto offset = address - start;
         return reinterpret_cast<Type *>(section.data.data() + offset);
      }
   }

   return nullptr;
}

static elf::Symbol *
getSectionSymbol(InputSection &section, size_t index)
{
   auto symbols = reinterpret_cast<elf::Symbol *>(section.data.data());
   return &symbols[index];
};

static bool
read(ElfFile &file, const std::string &filename)
{
   std::ifstream in { filename, std::ifstream::binary };
   std::vector<InputSection> inSections;

   if (!in.is_open()) {
      std::cout << "Could not open " << filename << " for reading" << std::endl;
      return false;
   }

   // Read header
   elf::Header header;
   in.read(reinterpret_cast<char *>(&header), sizeof(elf::Header));

   if (header.magic != elf::HeaderMagic) {
      std::cout << "Invalid ELF magic header" << std::endl;
      return false;
   }

   if (header.fileClass != elf::ELFCLASS32) {
      std::cout << "Unexpected ELF file class" << std::endl;
      return false;
   }

   if (header.encoding != elf::ELFDATA2MSB) {
      std::cout << "Unexpected ELF encoding" << std::endl;
      return false;
   }

   if (header.machine != elf::EM_PPC) {
      std::cout << "Unexpected ELF machine type" << std::endl;
      return false;
   }

   if (header.elfVersion != elf::EV_CURRENT) {
      std::cout << "Unexpected ELF version" << std::endl;
      return false;
   }

   file.entryPoint = header.entry;

   // Read section headers and data
   in.seekg(static_cast<size_t>(header.shoff));
   inSections.resize(header.shnum);

   for (auto &section : inSections) {
      in.read(reinterpret_cast<char *>(&section.header), sizeof(elf::SectionHeader));

      if (!section.header.size || section.header.type == elf::SHT_NOBITS) {
         continue;
      }

      auto pos = in.tellg();
      in.seekg(static_cast<size_t>(section.header.offset));
      section.data.resize(section.header.size);
      in.read(section.data.data(), section.data.size());
      in.seekg(pos);
   }

   auto shStrTab = inSections[header.shstrndx].data.data();

   // Process any loader relocations
   for (auto &section : inSections) {
      if (section.header.type != elf::SHT_RELA) {
         continue;
      }

      auto name = std::string { shStrTab + section.header.name };

      if (name.compare(".rela.dyn") != 0) {
         continue;
      }

      auto symSection = inSections[section.header.link];
      auto relas = reinterpret_cast<elf::Rela *>(section.data.data());
      auto count = section.data.size() / sizeof(elf::Rela);

      for (auto i = 0u; i < count; ++i) {
         auto &rela = relas[i];
         auto index = rela.info >> 8;
         auto symbol = getSectionSymbol(symSection, index);
         auto addr = symbol->value + rela.addend;

         auto type = rela.info & 0xff;
         auto ptr = getLoaderDataPtr<uint32_t>(inSections, rela.offset);

         if (!ptr) {
            std::cout << "Unexpected relocation offset in .rela.dyn section" << std::endl;
            return false;
         }

         switch (type) {
         case elf::R_PPC_RELATIVE:
            *ptr = byte_swap(addr);
            break;
         case elf::R_PPC_NONE:
            // ignore padding
            break;
         default:
            std::cout << "Unexpected relocation type in .rela.dyn section" << std::endl;
            return false;
         }
      }
   }

   // Read text/data sections
   for (auto &section : inSections) {
      if (section.header.addr >= LoadAddress && section.header.addr < CodeAddress) {
         // Skip any load sections
         continue;
      }

      auto name = std::string { shStrTab + section.header.name };

      if (section.header.type == elf::SHT_PROGBITS) {
         auto data = new ElfFile::DataSection();
         data->type = elf::SHT_PROGBITS;
         data->flags = static_cast<elf::SectionFlags>(section.header.flags.value());
         data->name = shStrTab + section.header.name;
         data->address = section.header.addr;
         data->data = section.data;
         file.dataSections.emplace_back(data);
      } else if (section.header.type == elf::SHT_NOBITS) {
         auto bss = new ElfFile::DataSection();
         bss->type = elf::SHT_NOBITS;
         bss->flags = static_cast<elf::SectionFlags>(section.header.flags.value());
         bss->name = shStrTab + section.header.name;
         bss->address = section.header.addr;
         bss->size = section.header.size;
         file.dataSections.emplace_back(bss);
      }
   }

   // Default symbols
   auto symNull = new ElfFile::Symbol();
   symNull->address = 0;
   symNull->size = 0;
   symNull->type = elf::STT_NOTYPE;
   symNull->binding = elf::STB_LOCAL;
   file.symbols.emplace_back(symNull);

   auto symText = new ElfFile::Symbol();
   symText->name = "$TEXT";
   symText->address = CodeAddress;
   symText->size = 0;
   symText->type = elf::STT_SECTION;
   symText->binding = elf::STB_LOCAL;
   file.symbols.emplace_back(symText);

   auto symData = new ElfFile::Symbol();
   symData->name = "$DATA";
   symData->address = DataAddress;
   symData->size = 0;
   symData->type = elf::STT_SECTION;
   symData->binding = elf::STB_LOCAL;
   file.symbols.emplace_back(symData);

   auto symUndef = new ElfFile::Symbol();
   symUndef->name = "$UNDEF";
   symUndef->address = 0;
   symUndef->size = 0;
   symUndef->type = elf::STT_OBJECT;
   symUndef->binding = elf::STB_GLOBAL;
   file.symbols.emplace_back(symUndef);

   // Read symbols
   for (auto &section : inSections) {
      if (section.header.type != elf::SHT_SYMTAB) {
         continue;
      }

      auto name = std::string { shStrTab + section.header.name };

      if (name.compare(".symtab") != 0) {
         std::cout << "Unexpected symbol section " << name << std::endl;
         return false;
      }

      auto strTab = inSections[section.header.link].data.data();
      auto symTab = reinterpret_cast<elf::Symbol *>(section.data.data());
      auto count = section.data.size() / sizeof(elf::Symbol);

      for (auto i = 0u; i < count; ++i) {
         auto &sym = symTab[i];

         if (sym.value >= LoadAddress && sym.value < CodeAddress) {
            // Skip any load symbols
            continue;
         }

         auto type = static_cast<elf::SymbolType>(sym.info & 0xF);
         auto binding = static_cast<elf::SymbolBinding>((sym.info >> 4) & 0xF);

         if (type == elf::STT_NOTYPE && sym.value == 0) {
            // Skip null symbol
            continue;
         }

         if (type == elf::STT_FILE || type == elf::STT_SECTION) {
            // Skip file, section symbols
            continue;
         }

         auto symbol = new ElfFile::Symbol();
         symbol->name = strTab + sym.name;
         symbol->address = sym.value;
         symbol->size = sym.size;
         symbol->type = type;
         symbol->binding = binding;
         file.symbols.emplace_back(symbol);
      }
   }

   // Read RPL imports
   for (auto &section : inSections) {
      auto name = std::string { shStrTab + section.header.name };

      if (name.compare(".lib.rplLibs") != 0) {
         continue;
      }

      auto rplTab = reinterpret_cast<RplLibsDef *>(section.data.data());
      auto count = section.data.size() / sizeof(RplLibsDef);

      for (auto i = 0u; i < count; ++i) {
         auto &rpl = rplTab[i];
         auto lib = new ElfFile::RplImportLibrary();
         lib->name = getLoaderDataPtr<char>(inSections, rpl.name);

         for (auto stubAddr = rpl.stubStart; stubAddr < rpl.stubEnd; stubAddr += 4) {
            auto import = new ElfFile::RplImport();
            import->trampAddr = byte_swap(*getLoaderDataPtr<uint32_t>(inSections, stubAddr));
            import->stubAddr = stubAddr;

            // Get the tramp symbol
            import->trampSymbol = findSymbol(file, import->trampAddr);

            // Create a new symbol to use for the import
            auto stubSymbol = new ElfFile::Symbol();
            import->stubSymbol = stubSymbol;
            stubSymbol->name = import->trampSymbol->name;
            stubSymbol->address = 0;
            stubSymbol->size = 0;
            stubSymbol->binding = elf::STB_GLOBAL;
            stubSymbol->type = elf::STT_FUNC;
            file.symbols.emplace_back(stubSymbol);

            // Rename tramp symbol
            import->trampSymbol->name += "_tramp";

            lib->imports.emplace_back(import);
         }

         file.rplImports.emplace_back(lib);
      }
   }

   // Read relocations
   for (auto &section : inSections) {
      if (section.header.type != elf::SHT_RELA) {
         continue;
      }

      auto name = std::string { shStrTab + section.header.name };

      if (name.compare(".rela.dyn") == 0) {
         // Skip dyn relocations
         continue;
      }

      auto symTab = reinterpret_cast<elf::Symbol *>(inSections[section.header.link].data.data());
      auto relTab = reinterpret_cast<elf::Rela *>(section.data.data());
      auto count = section.data.size() / sizeof(elf::Rela);

      for (auto i = 0u; i < count; ++i) {
         auto relocation = new ElfFile::Relocation();
         auto &rela = relTab[i];

         auto type = rela.info & 0xff;
         auto index = rela.info >> 8;
         auto &sym = symTab[index];
         auto symType = sym.info & 0xf;

         if (symType == elf::STT_SECTION && sym.value == CodeAddress) {
            if (rela.offset < CodeAddress || rela.offset >= DataAddress) {
               std::cout << "Unexpected symbol referenced in relocation section " << name << std::endl;
               return false;
            }
         }

         auto addend = static_cast<uint32_t>(rela.addend);

         if (auto import = findImport(file, addend)) {
            relocation->symbol = import->stubSymbol;
            relocation->addend = 0;
         } else if (auto symbol = findSymbol(file, addend)) {
            relocation->symbol = symbol;
            relocation->addend = 0;
         } else if (addend >= DataAddress && addend < WiiuLoadAddress) {
            relocation->symbol = findSymbol(file, DataAddress);
            relocation->addend = addend - DataAddress;
         } else if (addend >= CodeAddress && addend < DataAddress) {
            relocation->symbol = findSymbol(file, CodeAddress);
            relocation->addend = addend - CodeAddress;
         } else {
            // If we can't find a proper symbol, write the addend in and hope for the best
            auto ptr = getLoaderDataPtr<uint32_t>(inSections, rela.offset);
            *ptr = addend;

            std::cout << "Unexpected addend " << std::hex << addend << " referenced in relocation section " << name << ", continuing." << std::endl;
            continue;
         }

         relocation->target = rela.offset;
         relocation->type = static_cast<elf::RelocationType>(type);
         file.relocations.emplace_back(relocation);
      }
   }

   // Read dyn relocations
   for (auto &section : inSections) {
      if (section.header.type != elf::SHT_RELA) {
         continue;
      }

      auto name = std::string { shStrTab + section.header.name };

      if (name.compare(".rela.dyn") != 0) {
         continue;
      }

      auto symSection = inSections[section.header.link];
      auto relas = reinterpret_cast<elf::Rela *>(section.data.data());
      auto count = section.data.size() / sizeof(elf::Rela);

      for (auto i = 0u; i < count; ++i) {
         auto relocation = new ElfFile::Relocation();
         auto &rela = relas[i];

         auto type = rela.info & 0xff;
         auto index = rela.info >> 8;
         auto symbol = getSectionSymbol(symSection, index);
         auto addr = symbol->value + rela.addend;

         if(type == elf::R_PPC_NONE)
         {
            // ignore padding
            continue;
         }

         if(index == 0)
         {
            auto addend = static_cast<uint32_t>(rela.addend);

             if (auto import = findImport(file, addend)) {
                relocation->symbol = import->stubSymbol;
                relocation->addend = 0;
             } else if (auto symbol = findSymbol(file, addend)) {
                relocation->symbol = symbol;
                relocation->addend = 0;
             } else if (addr >= CodeAddress && addr < DataAddress) {
               index = 1;
               relocation->symbol = findSymbol(file, CodeAddress);
               relocation->addend = rela.addend - CodeAddress;
            } else if (addr >= DataAddress && addr < WiiuLoadAddress) {
               index = 2;
               relocation->symbol = findSymbol(file, DataAddress);
               relocation->addend = rela.addend - DataAddress;
            } else {
               std::cout << "Unexpected symbol address in .rela.dyn section" << std::endl;
               return false;
            }
         }

         switch (type) {
         case elf::R_PPC_RELATIVE:
            type = elf::R_PPC_ADDR32;
            break;
         default:
            std::cout << "Unexpected relocation type in .rela.dyn section" << std::endl;
            return false;
         }

         relocation->target = rela.offset;
         relocation->type = static_cast<elf::RelocationType>(type);

         // Scrap any compiler/linker garbage
         if(relocation->target >= CodeAddress && relocation->target < WiiuLoadAddress)
            file.relocations.emplace_back(relocation);
      }
   }

   return true;
}

struct OutputSection
{
   std::string name;
   elf::SectionHeader header;
   std::vector<char> data;
   OutputSection *relocationSection = nullptr;
   ElfFile::Symbol *sectionSymbol = nullptr;
};

template<typename SymbolIterator>
SymbolIterator addSection(ElfFile &file, std::vector<OutputSection *> &outSections, SymbolIterator symbolIterator, OutputSection *section)
{
   auto sectionSymbol = new ElfFile::Symbol();
   sectionSymbol->name = section->name;
   sectionSymbol->address = section->header.addr;
   sectionSymbol->size = -1;
   sectionSymbol->type = elf::STT_SECTION;
   sectionSymbol->binding = elf::STB_LOCAL;
   section->sectionSymbol = sectionSymbol;
   outSections.push_back(section);
   return file.symbols.insert(symbolIterator, std::unique_ptr<ElfFile::Symbol> { sectionSymbol }) + 1;
};

static uint32_t
getSectionIndex(std::vector<OutputSection *> &outSections, uint32_t address)
{
   for (auto i = 0u; i < outSections.size(); ++i) {
      auto &section = outSections[i];
      auto start = section->header.addr;
      auto end = start + section->header.size;

      if (address >= start && address < end) {
         return i;
      }
   }

   return -1;
}

static uint32_t
getSectionIndex(std::vector<OutputSection *> &outSections, const std::string &name)
{
   for (auto i = 0u; i < outSections.size(); ++i) {
      auto &section = outSections[i];

      if (section->name.compare(name) == 0) {
         return i;
      }
   }

   return -1;
}

static bool
write(ElfFile &file, const std::string &filename)
{
   std::vector<OutputSection *> outSections;
   auto sectionSymbolItr = file.symbols.begin() + 4;

   // Create NULL section
   auto nullSection = new OutputSection();
   memset(&nullSection->header, 0, sizeof(elf::SectionHeader));
   outSections.push_back(nullSection);

   // Create text/data sections
   for (auto &section : file.dataSections) {
      auto out = new OutputSection();
      out->header.name = -1;
      out->header.type = section->type;
      out->header.flags = section->flags;
      out->header.addr = section->address;
      out->header.offset = -1;

      if (section->type == elf::SHT_NOBITS) {
         out->header.size = section->size;
      } else {
         out->header.size = section->data.size();
      }

      out->header.link = 0;
      out->header.info = 0;

      if (section->address == DataAddress) {
         out->header.addralign = 4096;
         out->header.flags |= elf::SHF_WRITE; // .rodata needs to be writable?
      } else {
         out->header.addralign = 256;
      }

      out->header.entsize = 0;

      // Add section
      out->name = section->name;
      out->data = section->data;
      sectionSymbolItr = addSection(file, outSections, sectionSymbolItr, out);
   }

   // Create relocation sections
   for (auto &relocation : file.relocations) {
      OutputSection *targetSection = nullptr;

      for (auto &section : outSections) {
         auto start = section->header.addr;
         auto end = start + section->header.size;

         if (relocation->target >= start && relocation->target < end) {
            targetSection = section;
            break;
         }
      }

      if (!targetSection) {
         std::cout << "Error could not find section for relocation" << std::endl;
         return false;
      }

      if (!targetSection->relocationSection) {
         // Create new relocation section
         auto out = new OutputSection();
         out->header.name = -1;
         out->header.type = elf::SHT_RELA;
         out->header.flags = 0;
         out->header.addr = 0;
         out->header.offset = -1;
         out->header.size = -1;
         out->header.link = -1;
         out->header.info = getSectionIndex(outSections, targetSection->header.addr);
         out->header.addralign = 4;
         out->header.entsize = sizeof(elf::Rela);

         // Add section
         out->name = ".rela" + targetSection->name;
         sectionSymbolItr = addSection(file, outSections, sectionSymbolItr, out);
         targetSection->relocationSection = out;
      }
   }

   // Calculate sizes of symbol/string tables so RPL imports are placed after them
   auto loadAddress = 0xC0000000;
   auto predictStrTabSize = 1;
   auto predictSymTabSize = 1;
   auto predictShstrTabSize = 1;

   for (auto &symbol : file.symbols) {
      predictStrTabSize += symbol->name.size() + 1;
      predictSymTabSize += sizeof(elf::Symbol);
   }

   for (auto &section : outSections) {
      predictShstrTabSize += section->name.size() + 1;
   }

   predictStrTabSize = align_up(predictStrTabSize, 0x10);
   predictSymTabSize = align_up(predictSymTabSize, 0x10);
   predictShstrTabSize = align_up(predictShstrTabSize, 0x10);
   loadAddress += predictStrTabSize + predictSymTabSize + predictShstrTabSize;

   // Create RPL import sections, .fimport_*, .dimport_*
   for (auto &lib : file.rplImports) {
      auto out = new OutputSection();
      out->header.name = -1;
      out->header.type = elf::SHT_RPL_IMPORTS;
      out->header.flags = elf::SHF_ALLOC | elf::SHF_EXECINSTR;
      out->header.addr = loadAddress;
      out->header.offset = -1;
      out->header.link = 0;
      out->header.info = 0;
      out->header.addralign = 4;
      out->header.entsize = 0;
      out->name = ".fimport_" + lib->name;

      // Calculate size
      auto nameSize = align_up(8 + lib->name.size(), 8);
      auto stubSize = 8 + 8 * lib->imports.size();
      out->header.size = std::max(nameSize, stubSize);
      out->data.resize(out->header.size);

      // Setup data
      auto imports = reinterpret_cast<elf::RplImport*>(out->data.data());
      imports->count = lib->imports.size();
      imports->signature = crc32(0, Z_NULL, 0);
      memcpy(imports->name, lib->name.data(), lib->name.size());
      imports->name[lib->name.size()] = 0;

      // Update address of import symbols
      for (auto i = 0u; i < lib->imports.size(); ++i) {
         lib->imports[i]->stubSymbol->address = loadAddress + 8 + i * 8;
      }

      loadAddress = align_up(loadAddress + out->header.size, 4);

      // Add section
      sectionSymbolItr = addSection(file, outSections, sectionSymbolItr, out);
   }

   // Prune out unneeded symbols
   for (auto i = 0u; i < file.symbols.size(); ++i) {
      if (!file.symbols[i]->name.empty() && file.symbols[i]->type == elf::STT_NOTYPE && file.symbols[i]->size == 0) {
         file.symbols.erase(file.symbols.begin() + i);
         i--;
      }
   }

   // NOTICE: FROM NOW ON DO NOT MODIFY mSymbols

   // Convert relocations
   for (auto &relocation : file.relocations) {
      OutputSection *targetSection = nullptr;

      for (auto &section : outSections) {
         auto start = section->header.addr;
         auto end = start + section->header.size;

         if (relocation->target >= start && relocation->target < end) {
            targetSection = section;
            break;
         }
      }

      if (!targetSection || !targetSection->relocationSection) {
         std::cout << "Error could not find section for relocation" << std::endl;
         return false;
      }

      // Get address of relocation->target
      auto relocationSection = targetSection->relocationSection;

      // Find symbol this relocation points to
      auto itr = std::find_if(file.symbols.begin(), file.symbols.end(), [&relocation](auto &val) {
         return val.get() == relocation->symbol;
      });

      auto idx = itr - file.symbols.begin();

      // If the symbol doesn't exist but it is within DATA or TEXT, use those symbols + an addend
      if (itr == file.symbols.end()) {
         if (relocation->symbol->address >= CodeAddress && relocation->symbol->address < DataAddress) {
            idx = 1;
            relocation->addend = relocation->symbol->address - CodeAddress;
            relocation->symbol = findSymbol(file, CodeAddress);
         } else if (relocation->symbol->address >= DataAddress && relocation->symbol->address < WiiuLoadAddress) {
            idx = 2;
            relocation->addend = relocation->symbol->address - DataAddress;
            relocation->symbol = findSymbol(file, DataAddress);
         } else {
            std::cout << "Could not find matching symbol for relocation" << std::endl;
            return false;
         }
      }

      // Create relocation
      elf::Rela rela;
      rela.info = relocation->type | idx << 8;

      if(relocation->type == elf::R_PPC_RELATIVE) {
         rela.info = elf::R_PPC_ADDR32 | idx << 8;
      }

      rela.addend = relocation->addend;
      rela.offset = relocation->target;

      // Append to relocation section data
      char *relaData = reinterpret_cast<char *>(&rela);
      relocationSection->data.insert(relocationSection->data.end(), relaData, relaData + sizeof(elf::Rela));
   }

   // String + Symbol sections
   auto symTabSection = new OutputSection();
   auto strTabSection = new OutputSection();
   auto shStrTabSection = new OutputSection();

   symTabSection->name = ".symtab";
   strTabSection->name = ".strtab";
   shStrTabSection->name = ".shstrtab";

   auto symTabIndex = outSections.size();
   outSections.push_back(symTabSection);

   auto strTabIndex = outSections.size();
   outSections.push_back(strTabSection);

   auto shStrTabIndex = outSections.size();
   outSections.push_back(shStrTabSection);

   // Update relocation sections to link to symtab
   for (auto &section : outSections) {
      if (section->header.type == elf::SHT_RELA) {
         section->header.link = symTabIndex;
      }

      if (section->header.type != elf::SHT_NOBITS) {
         section->header.size = section->data.size();
      }

      if (section->sectionSymbol) {
         section->sectionSymbol->address = section->header.addr;
         section->sectionSymbol->size = section->header.size;
      }
   }

   // Create .strtab
   strTabSection->header.name = 0;
   strTabSection->header.type = elf::SHT_STRTAB;
   strTabSection->header.flags = elf::SHF_ALLOC;
   strTabSection->header.addr = 0;
   strTabSection->header.offset = -1;
   strTabSection->header.size = -1;
   strTabSection->header.link = 0;
   strTabSection->header.info = 0;
   strTabSection->header.addralign = 1;
   strTabSection->header.entsize = 0;

   // Add all symbol names to data, update symbol->outNamePos
   strTabSection->data.push_back(0);

   for (auto &symbol : file.symbols) {
      if (symbol->name.empty()) {
         symbol->outNamePos = 0;
      } else {
         symbol->outNamePos = static_cast<uint32_t>(strTabSection->data.size());
         std::copy(symbol->name.begin(), symbol->name.end(), std::back_inserter(strTabSection->data));
         strTabSection->data.push_back(0);
      }
   }

   // Create .symtab
   symTabSection->header.name = 0;
   symTabSection->header.type = elf::SHT_SYMTAB;
   symTabSection->header.flags = elf::SHF_ALLOC;
   symTabSection->header.addr = 0;
   symTabSection->header.offset = -1;
   symTabSection->header.size = -1;
   symTabSection->header.link = strTabIndex;
   symTabSection->header.info = 0;
   symTabSection->header.addralign = 4;
   symTabSection->header.entsize = sizeof(elf::Symbol);

   for (auto &symbol : file.symbols) {
      elf::Symbol sym;
      auto shndx = getSectionIndex(outSections, symbol->address);

      if (symbol->type == elf::STT_SECTION && symbol->address == 0) {
         shndx = getSectionIndex(outSections, symbol->name);
      }

      if (shndx == (uint32_t)-1) {
         std::cout << "Could not find section for symbol" << std::endl;
         return false;
      }

      sym.name = symbol->outNamePos;
      sym.value = symbol->address;
      sym.size = symbol->size;
      sym.info = symbol->type | (symbol->binding << 4);
      sym.other = 0;
      sym.shndx = shndx;

      //Compound symbol crc into section crc
      auto crcSection = outSections[shndx];
      if(crcSection->header.type == elf::SHT_RPL_IMPORTS && symbol->type != elf::STT_SECTION) {
         auto rplImport = reinterpret_cast<elf::RplImport*>(crcSection->data.data());
         rplImport->signature = crc32(rplImport->signature, reinterpret_cast<Bytef *>(strTabSection->data.data() + sym.name),strlen(strTabSection->data.data() + sym.name)+1);
      }

      // Append to symtab data
      char *symData = reinterpret_cast<char *>(&sym);
      symTabSection->data.insert(symTabSection->data.end(), symData, symData + sizeof(elf::Symbol));
   }

   //Finish SHT_RPL_IMPORTS signatures
   Bytef *zero_buffer = reinterpret_cast<Bytef *>(calloc(0x10, 1));
   for (auto &section : outSections) {
      if(section->header.type == elf::SHT_RPL_IMPORTS) {
         auto rplImport = reinterpret_cast<elf::RplImport*>(section->data.data());
         rplImport->signature = crc32(rplImport->signature, zero_buffer, 0xE);
      }
   }
   free(zero_buffer);

   // Create .shstrtab
   shStrTabSection->header.name = 0;
   shStrTabSection->header.type = elf::SHT_STRTAB;
   shStrTabSection->header.flags = elf::SHF_ALLOC;
   shStrTabSection->header.addr = 0;
   shStrTabSection->header.offset = -1;
   shStrTabSection->header.size = -1;
   shStrTabSection->header.link = 0;
   shStrTabSection->header.info = 0;
   shStrTabSection->header.addralign = 1;
   shStrTabSection->header.entsize = 0;

   // Add all section header names to data, update section->header.name
   shStrTabSection->data.push_back(0);

   for (auto &section : outSections) {
      if (section->name.empty()) {
         section->header.name = 0;
      } else {
         section->header.name = shStrTabSection->data.size();
         std::copy(section->name.begin(), section->name.end(), std::back_inserter(shStrTabSection->data));
         shStrTabSection->data.push_back(0);
      }
   }

   loadAddress = 0xC0000000;

   // Update symtab, strtab, shstrtab section addresses
   symTabSection->header.addr = loadAddress;
   symTabSection->header.size = symTabSection->data.size();

   loadAddress = align_up(symTabSection->header.addr + predictSymTabSize, 16);
   strTabSection->header.addr = loadAddress;
   strTabSection->header.size = strTabSection->data.size();

   loadAddress = align_up(strTabSection->header.addr + predictStrTabSize, 16);
   shStrTabSection->header.addr = loadAddress;
   shStrTabSection->header.size = shStrTabSection->data.size();

   // Create SHT_RPL_FILEINFO section
   auto fileInfoSection = new OutputSection();
   fileInfoSection->header.name = 0;
   fileInfoSection->header.type = elf::SHT_RPL_FILEINFO;
   fileInfoSection->header.flags = 0;
   fileInfoSection->header.addr = 0;
   fileInfoSection->header.offset = -1;
   fileInfoSection->header.size = -1;
   fileInfoSection->header.link = 0;
   fileInfoSection->header.info = 0;
   fileInfoSection->header.addralign = 4;
   fileInfoSection->header.entsize = 0;

   elf::RplFileInfo fileInfo;
   fileInfo.version = 0xCAFE0402;
   fileInfo.textSize = 0;
   fileInfo.textAlign = 32;
   fileInfo.dataSize = 0;
   fileInfo.dataAlign = 4096;
   fileInfo.loadSize = 0;
   fileInfo.loadAlign = 4;
   fileInfo.tempSize = 0;
   fileInfo.trampAdjust = 0;
   fileInfo.trampAddition = 0;
   fileInfo.sdaBase = 0;
   fileInfo.sda2Base = 0;
   fileInfo.stackSize = 0x10000;
   fileInfo.heapSize = 0x8000;
   fileInfo.filename = 0;
   fileInfo.flags = elf::RPL_IS_RPX;
   fileInfo.minVersion = 0x5078;
   fileInfo.compressionLevel = -1;
   fileInfo.fileInfoPad = 0;
   fileInfo.cafeSdkVersion = 0x51BA;
   fileInfo.cafeSdkRevision = 0xCCD1;
   fileInfo.tlsAlignShift = 0;
   fileInfo.tlsModuleIndex = 0;
   fileInfo.runtimeFileInfoSize = 0;
   fileInfo.tagOffset = 0;

   // Count file info textSize, dataSize, loadSize
   for (auto &section : outSections) {
      auto size = section->data.size();

      if (section->header.type == elf::SHT_NOBITS) {
         size = section->header.size;
      }

      if (section->header.addr >= CodeAddress && section->header.addr < DataAddress) {
         auto val = section->header.addr.value() + section->header.size.value() - CodeAddress;
         if(val > fileInfo.textSize) {
            fileInfo.textSize = val;
         }
      } else if (section->header.addr >= DataAddress && section->header.addr < WiiuLoadAddress) {
         auto val = section->header.addr.value() + section->header.size.value() - DataAddress;
         if(val > fileInfo.dataSize) {
            fileInfo.dataSize = val;
         }
      } else if (section->header.addr >= WiiuLoadAddress) {
         auto val = section->header.addr.value() + section->header.size.value() - WiiuLoadAddress;
         if(val > fileInfo.loadSize) {
            fileInfo.loadSize = val;
         }
      } else if (section->header.addr == 0 && section->header.type != elf::SHT_RPL_CRCS && section->header.type != elf::SHT_RPL_FILEINFO) {
         fileInfo.tempSize += (size + 128);
      }
   }

   //TODO: These were calculated based on observation, however some games differ.
   fileInfo.sdaBase = align_up(DataAddress + fileInfo.dataSize + fileInfo.heapSize, 64);
   fileInfo.sda2Base = align_up(DataAddress + fileInfo.heapSize, 64);

   char *fileInfoData = reinterpret_cast<char *>(&fileInfo);
   fileInfoSection->data.insert(fileInfoSection->data.end(), fileInfoData, fileInfoData + sizeof(elf::RplFileInfo));

   // Create SHT_RPL_CRCS section
   auto crcSection = new OutputSection();
   crcSection->header.name = 0;
   crcSection->header.type = elf::SHT_RPL_CRCS;
   crcSection->header.flags = 0;
   crcSection->header.addr = 0;
   crcSection->header.offset = -1;
   crcSection->header.size = -1;
   crcSection->header.link = 0;
   crcSection->header.info = 0;
   crcSection->header.addralign = 4;
   crcSection->header.entsize = 4;

   outSections.push_back(crcSection);
   outSections.push_back(fileInfoSection);

   std::vector<uint32_t> sectionCRCs;

   for (auto &section : outSections) {
      auto crc = 0u;

      if (!section->data.empty()) {
         crc = crc32(0, Z_NULL, 0);
         crc = crc32(crc, reinterpret_cast<Bytef *>(section->data.data()), section->data.size());
      }

      sectionCRCs.push_back(byte_swap(crc));
   }

   char *crcData = reinterpret_cast<char *>(sectionCRCs.data());
   crcSection->data.insert(crcSection->data.end(), crcData, crcData + sizeof(uint32_t) * sectionCRCs.size());

   // Update section sizes and offsets
   auto shoff = align_up(sizeof(elf::Header), 64);
   auto dataOffset = align_up(shoff + outSections.size() * sizeof(elf::SectionHeader), 64);

   // Add CRC and FileInfo sections first
   for (auto &section : outSections) {
      if (section->header.type != elf::SHT_RPL_CRCS && section->header.type != elf::SHT_RPL_FILEINFO) {
         continue;
      }

      if (section->header.type != elf::SHT_NOBITS) {
         section->header.size = section->data.size();
      }

      if (!section->data.empty()) {
         section->header.offset = dataOffset;
         dataOffset = align_up(section->header.offset + section->data.size(), 64);
      } else {
         section->header.offset = 0;
      }
   }

   // Add data sections next
   for (auto &section : outSections) {
      if(section->header.offset != -1) {
         continue;
      }

      if (section->header.addr < DataAddress || section->header.addr >= WiiuLoadAddress) {
         continue;
      }

      if (section->header.type != elf::SHT_NOBITS) {
         section->header.size = section->data.size();
      }

      if (!section->data.empty()) {
         section->header.offset = dataOffset;
         dataOffset = align_up(section->header.offset + section->data.size(), 64);
      } else {
         section->header.offset = 0;
      }
   }

   // Add load sections next
   for (auto &section : outSections) {
      if(section->header.offset != -1) {
         continue;
      }

      if (section->header.addr < WiiuLoadAddress) {
         continue;
      }

      if (section->header.type != elf::SHT_NOBITS) {
         section->header.size = section->data.size();
      }

      if (!section->data.empty()) {
         section->header.offset = dataOffset;
         dataOffset = align_up(section->header.offset + section->data.size(), 64);
      } else {
         section->header.offset = 0;
      }
   }

   // Everything else
   for (auto &section : outSections) {
      if(section->header.offset != -1) {
         continue;
      }

      if (section->header.type != elf::SHT_NOBITS) {
         section->header.size = section->data.size();
      }

      if (!section->data.empty()) {
         section->header.offset = dataOffset;
         dataOffset = align_up(section->header.offset + section->data.size(), 64);
      } else {
         section->header.offset = 0;
      }
   }

   // Write to file
   std::ofstream out { filename, std::ofstream::binary };
   std::vector<char> padding;

   if (!out.is_open()) {
      std::cout << "Could not open " << filename << " for writing" << std::endl;
      return false;
   }

   elf::Header header;
   header.magic = elf::HeaderMagic;
   header.fileClass = 1;
   header.encoding = elf::ELFDATA2MSB;
   header.elfVersion = elf::EV_CURRENT;
   header.abi = elf::EABI_CAFE;
   memset(&header.pad, 0, 7);
   header.type = 0xFE01;
   header.machine = elf::EM_PPC;
   header.version = 1;
   header.entry = file.entryPoint;
   header.phoff = 0;
   header.phentsize = 0;
   header.phnum = 0;
   header.shoff = shoff;
   header.shnum = outSections.size();
   header.shentsize = sizeof(elf::SectionHeader);
   header.flags = 0;
   header.ehsize = sizeof(elf::Header);
   header.shstrndx = shStrTabIndex;
   out.write(reinterpret_cast<char *>(&header), sizeof(elf::Header));

   // Write section headers
   out.seekp(header.shoff.value());

   for (auto &section : outSections) {
      out.write(reinterpret_cast<char *>(&section->header), sizeof(elf::SectionHeader));
   }

   // Write section data
   for (auto &section : outSections) {
      if (!section->data.empty()) {
         out.seekp(section->header.offset.value());
         out.write(section->data.data(), section->data.size());
      }
   }

   return true;
}

int main(int argc, char **argv)
{
   if (argc < 3) {
      std::cout << "Usage: " << argv[0] << " <src> <dst>" << std::endl;
      return -1;
   }

   ElfFile elf;
   auto src = argv[1];
   auto dst = argv[2];

   if (!read(elf, src)) {
      return -1;
   }

   if (!write(elf, dst)) {
      return -1;
   }

   return 0;
}
