#include <algorithm>
#include <fstream>
#include <iterator>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
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

int verbose = 0;
constexpr int verbose_loaded_sections     = 1;
constexpr int verbose_loaded_symbols      = 3;
constexpr int verbose_loaded_imports      = 2;
constexpr int verbose_loaded_relocations  = 4;
constexpr int verbose_loaded_dyn_reloc    = 4;
constexpr int verbose_pruned_symbols      = 2;
constexpr int verbose_convert_relocations = 4;
constexpr int verbose_dup_symbols         = 3;
constexpr int verbose_output_sections     = 1;
constexpr int verbose_output_symbols      = 2;

struct OutputSection;
using OutputSectionPtr = std::shared_ptr<OutputSection>;

struct ElfFile
{
   struct DataSection
   {
      std::string name;
      uint32_t address           = 0;
      elf::SectionType type      = elf::SHT_NULL;
      elf::SectionFlags flags    = elf::SectionFlags(0);
      OutputSectionPtr outSection;
      int inIndex                  = -1;

      /* Data used if type == SHT_PROGBITS */
      std::vector<char> data;

      /* Size used if type == SHT_NOBITS */
      uint32_t size              = 0;

      DataSection(uint32_t _address, std::string _name,
         elf::SectionType _type, elf::SectionFlags _flags)
         : name(_name), address(_address), type(_type), flags(_flags)
      {}

      static std::shared_ptr<DataSection>
      New(uint32_t _address, std::string _name,
         elf::SectionType _type, elf::SectionFlags _flags)
      {
         return std::make_shared<DataSection>(_address, _name, _type, _flags);
      }

      bool contains(uint32_t _address, uint32_t _size = 0)
      {

         if (!(flags & elf::SHF_ALLOC) || size == 0 || size == uint32_t(-1))
            return false;

         auto start = address;
         auto end = start + size;

         if (_address >= start && _address + _size <= end)
            return true;

         return false;
      }
   };

   using DataSectionPtr = std::shared_ptr<DataSection>;
   using DataSectionList = std::vector<DataSectionPtr>;

   struct Symbol
   {
      std::string name;
      uint32_t address           = 0;
      uint32_t size              = 0;
      elf::SymbolType type       = elf::STT_NOTYPE;
      elf::SymbolBinding binding = elf::STB_LOCAL;
      uint32_t outNamePos        = 0;
      uint32_t refCount          = 0;
      int inIndex                = -1;
      int outIndex               = -1;
      DataSectionPtr inSection;
      OutputSectionPtr outSection;

      Symbol(uint32_t _address, const std::string& _name,
         uint32_t _size, elf::SymbolType _type, elf::SymbolBinding _binding)
         : name(_name), address(_address), size(_size), type(_type), binding(_binding)
      {}

      static std::shared_ptr<Symbol>
      New(uint32_t _address, const std::string& _name,
         uint32_t _size, elf::SymbolType _type, elf::SymbolBinding _binding)
      {
         return std::make_shared<Symbol>(_address, _name, _size, _type, _binding);
      }
   };

   using SymbolPtr = std::shared_ptr<Symbol>;
   using SymbolList = std::vector<SymbolPtr>;
   using SymbolByAddrMap = std::map<uint32_t, SymbolPtr>;

   struct Relocation
   {
      uint32_t target            = 0;
      elf::RelocationType type   = elf::R_PPC_NONE;

      SymbolPtr symbol;
      DataSectionPtr symbolSection;
      uint32_t addend            = 0;
      OutputSectionPtr targetSection;
   };

   using RelocationPtr = std::shared_ptr<Relocation>;
   using RelocationList = std::vector<RelocationPtr>;

   struct RplImport
   {
      SymbolPtr trampSymbol;
      SymbolPtr stubSymbol;
      uint32_t stubAddr          = 0;
      uint32_t trampAddr         = 0;

      RplImport(uint32_t _stubAddr, uint32_t _trampAddr,
         SymbolPtr _stubSymbol, SymbolPtr _trampSymbol)
         : trampSymbol(_trampSymbol), stubSymbol(_stubSymbol),
            stubAddr(_stubAddr), trampAddr(_trampAddr)
      {}

      static std::shared_ptr<RplImport>
      New(uint32_t _stubAddr, uint32_t _trampAddr,
         SymbolPtr _stubSymbol, SymbolPtr _trampSymbol)
      {
         return std::make_shared<RplImport>(_stubAddr, _trampAddr, _stubSymbol, _trampSymbol);
      }
   };

   using RplImportPtr = std::shared_ptr<RplImport>;
   using RplImportList = std::vector<RplImportPtr>;
   using RplImportByAddrMap = std::map<uint32_t, RplImportPtr>;

   struct RplImportLibrary
   {
      std::string name;
      RplImportList imports;

      RplImportLibrary(const std::string& _name)
      : name(_name)
      {}

      static std::shared_ptr<RplImportLibrary>
      New(const std::string& _name)
      {
         return std::make_shared<RplImportLibrary>(_name);
      }

      void
      addImport(const RplImportPtr& import)
      {
         imports.emplace_back(import);
      }
   };

   using RplImportLibraryPtr = std::shared_ptr<RplImportLibrary>;
   using RplImportLibraryList = std::vector<RplImportLibraryPtr>;

   uint32_t entryPoint = 0;

private:
   DataSectionList dataSections;
   SymbolList symbols;
   RelocationList relocations;
   RplImportLibraryList rplImports;

   SymbolByAddrMap symbolsByAddr;
   RplImportByAddrMap importsByAddr;

public:
   void
   addDataSection(const DataSectionPtr& section)
   {
      dataSections.emplace_back(section);
   }

   void
   addSymbolToIndex(const SymbolPtr& symbol)
   {
      auto iter = symbolsByAddr.find(symbol->address);
      if (iter != symbolsByAddr.end())
      {
         if ((iter->second->type == elf::STT_NOTYPE
               && symbol->type != elf::STT_NOTYPE)
            || (iter->second->type != elf::STT_SECTION
               && symbol->type == elf::STT_SECTION
               && symbol->address != 0))
         {
            if (verbose >= verbose_dup_symbols)
            {
               std::cout << "Duplicate symbol - changed type "
                  << std::hex << "0x" << symbol->address << " "
                  << iter->second->name << " / " << symbol->name
                  << std::endl;
            }
            iter->second = symbol;
         }
         else if ((iter->second->type != elf::STT_SECTION || symbol->type == elf::STT_SECTION)
            && symbol->type != elf::STT_NOTYPE && iter->second->size == 0
            && symbol->size != 0 && symbol->size != uint32_t(-1) && symbol->address != 0)
         {
            if (verbose >= verbose_dup_symbols)
            {
               std::cout << "Duplicate symbol - changed size "
                  << std::hex << "0x" << symbol->address << " "
                  << iter->second->size << " / " << symbol->size << " "
                  << iter->second->type << " / " << symbol->type << " "
                  << iter->second->name << " / " << symbol->name
                  << std::endl;
            }
            iter->second = symbol;
         }
         else
         {
            if (symbol->name != iter->second->name && symbol->address != 0)
            {
               if (verbose >= verbose_dup_symbols)
               {
                  std::cout << "Duplicate symbol "
                     << std::hex << "0x" << symbol->address << " "
                     << iter->second->size << " / " << symbol->size << " "
                     << iter->second->type << " / " << symbol->type << " "
                     << iter->second->name << " / " << symbol->name
                     << std::endl;
               }
            }
         }
      }
      else
      {
         symbolsByAddr.emplace(symbol->address, symbol);
      }
   }

   void
   addSymbol(const SymbolPtr& symbol)
   {
      symbols.emplace_back(symbol);
      addSymbolToIndex(symbol);
   }

   SymbolList::iterator
   insertSymbol(SymbolList::iterator iter, const SymbolPtr& symbol)
   {
      addSymbolToIndex(symbol);
      return symbols.insert(iter, symbol);
   }

   void
   addImport(const RplImportLibraryPtr& lib,
      const RplImportPtr& import)
   {
      lib->addImport(import);
      auto result = importsByAddr.emplace(import->stubAddr, import);
      if (!result.second)
      {
         std::cout << "Duplicate import stub address "
            << import->stubSymbol->name << std::endl;
      }
      result = importsByAddr.emplace(import->trampAddr, import);
      if (!result.second)
      {
         std::cout << "Duplicate import tramp address "
            << import->trampSymbol->name << std::endl;
      }
   }

   void
   addImportLibrary(const RplImportLibraryPtr& lib)
   {
      rplImports.emplace_back(lib);
   }

   void
   addRelocation(const RelocationPtr& relocation)
   {
      relocations.emplace_back(relocation);
   }

   SymbolPtr
   findSymbol(uint32_t address)
   {
      auto iter = symbolsByAddr.find(address);
      if (iter != symbolsByAddr.end())
         return iter->second;
      return nullptr;
   }

   RplImportPtr
   findImport(uint32_t address)
   {
      auto iter = importsByAddr.find(address);
      if (iter != importsByAddr.end())
         return iter->second;
      return nullptr;
   }

   DataSectionPtr
   findSection(uint32_t address,
      const DataSectionPtr& hint = nullptr)
   {
      if (address == 0)
         return dataSections[0];

      if (hint && hint->contains(address))
         return hint;

      for (auto &section : dataSections) {
         if (section->contains(address))
            return section;
      }

      return nullptr;
   }

   DataSectionList&
   getDataSections()
   {
      return dataSections;
   }

   SymbolList&
   getSymbols()
   {
      return symbols;
   }

   RelocationList&
   getRelocations()
   {
      return relocations;
   }

   RplImportLibraryList&
   getImportLibs()
   {
      return rplImports;
   }

   void
   pruneSymbols()
   {
      SymbolList new_symbols;
      new_symbols.resize(symbols.size());
      unsigned d = 0;

      /* Prune out unneeded symbols - but keep those used for relocations! */
      for (unsigned s = 0u; s < symbols.size(); ++s)
      {
         if (!symbols[s]->name.empty() && symbols[s]->type == elf::STT_NOTYPE
            && symbols[s]->size == 0 && symbols[s]->refCount == 0)
         {
            if (verbose >= verbose_pruned_symbols)
            {
               std::cout << "Pruning symbol " << symbols[s]->name << std::endl;
            }
         }
         else
         {
            std::swap(new_symbols[d], symbols[s]);
            ++d;
         }
      }

      new_symbols.resize(d);
      std::swap(new_symbols, symbols);
   }

   void
   indexSymbols()
   {
      for (unsigned i = 0; i < symbols.size(); ++i)
      {
         symbols[i]->outIndex = i;
      }
   }
};

struct InputSection
{
   elf::SectionHeader header;
   std::vector<char> data;
};

template<typename Type>
static Type *
getLoaderDataPtr(std::vector<InputSection> &inSections, uint32_t address)
{
   for (auto &section : inSections)
   {
      auto start = section.header.addr;
      auto end   = start + section.data.size();

      if (start <= address && end > address)
      {
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
   ElfFile::DataSectionList dataSectionIndex;

   if (!in.is_open()) {
      std::cout << "Could not open " << filename << " for reading" << std::endl;
      return false;
   }

   /* Read header */
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

   /* Read section headers and data */
   in.seekg(static_cast<size_t>(header.shoff));
   inSections.resize(header.shnum);
   dataSectionIndex.resize(header.shnum);

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

   /* Process any loader relocations */
   for (auto &section : inSections)
   {
      if (section.header.type != elf::SHT_RELA)
         continue;

      auto name = std::string { shStrTab + section.header.name };

      if (name.compare(".rela.dyn") != 0)
         continue;

      auto symSection = inSections[section.header.link];
      auto relas = reinterpret_cast<elf::Rela *>(section.data.data());
      auto count = section.data.size() / sizeof(elf::Rela);

      for (unsigned i = 0u; i < count; ++i)
      {
         auto &rela = relas[i];
         auto index = rela.info >> 8;
         auto symbol = getSectionSymbol(symSection, index);
         auto addr = symbol->value + rela.addend;

         auto type = rela.info & 0xff;
         auto ptr = getLoaderDataPtr<uint32_t>(inSections, rela.offset);

         if (!ptr)
         {
            std::cout << "Unexpected relocation offset in .rela.dyn section" << std::endl;
            return false;
         }

         switch (type) {
         case elf::R_PPC_RELATIVE:
            if (verbose >= verbose_loaded_dyn_reloc)
            {
               std::cout << "Dyn relocation 0x" << std::hex << rela.offset << " 0x" << byte_swap(*ptr) << " -> 0x" << addr << std::endl;
            }
            *ptr = byte_swap(addr);
            break;
         case elf::R_PPC_NONE:
            /* ignore padding */
            break;
         default:
            std::cout << "Unexpected relocation type in .rela.dyn section" << std::endl;
            return false;
         }
      }
   }

   int index = -1;
   /* Read text/data sections */
   for (auto &section : inSections) {

      ++index;

      if (section.header.addr >= LoadAddress && section.header.addr < CodeAddress) {
         /* Skip any load sections */
         continue;
      }

      auto name = std::string { shStrTab + section.header.name };
      auto address = section.header.addr.value();
      auto type = elf::SectionType(section.header.type.value());
      auto flags = elf::SectionFlags(section.header.flags.value());
      auto size = section.header.size.value();

      if (type == elf::SHT_PROGBITS) {
         auto data = ElfFile::DataSection::New(address, name, type, flags);
         data->data = section.data;
         data->size = size;
         data->inIndex = index;
         dataSectionIndex[index] = data;
         file.addDataSection(data);

         if (verbose >= verbose_loaded_sections)
         {
            std::cout << "Data section #" << index << ' ' << name
               << std::hex << ": 0x" << address << " - 0x" << address + size << std::endl;
         }
      } else if (type == elf::SHT_NOBITS) {
         auto bss = ElfFile::DataSection::New(address, name, type, flags);
         bss->size = size;
         bss->inIndex = index;
         dataSectionIndex[index] = bss;
         file.addDataSection(bss);

         if (verbose >= verbose_loaded_sections)
         {
            std::cout << "BSS section #" << index << ' ' << name
               << std::hex << ": 0x" << address << " - 0x" << address + size << std::endl;
         }
      } else {
         if (verbose >= verbose_loaded_sections)
         {
            std::cout << "Section #" << index << " type 0x" << type << ' ' << name
               << std::hex << ": 0x" << address << " - 0x" << address + size << std::endl;
         }
      }
   }

   /* Default symbols */
   auto symNull = ElfFile::Symbol::New(0, "", 0, elf::STT_NOTYPE, elf::STB_LOCAL);
   file.addSymbol(symNull);

   auto symText = ElfFile::Symbol::New(CodeAddress, "$TEXT", 0, elf::STT_SECTION, elf::STB_LOCAL);
   file.addSymbol(symText);

   auto symData = ElfFile::Symbol::New(DataAddress, "$DATA", 0, elf::STT_SECTION, elf::STB_LOCAL);
   file.addSymbol(symData);

   auto symUndef = ElfFile::Symbol::New(0, "$UNDEF", 0, elf::STT_OBJECT, elf::STB_GLOBAL);
   file.addSymbol(symUndef);

   /* Read symbols */
   for (auto &section : inSections)
   {
      if (section.header.type != elf::SHT_SYMTAB)
         continue;

      auto name = std::string { shStrTab + section.header.name };

      if (name.compare(".symtab") != 0)
      {
         std::cout << "Unexpected symbol section " << name << std::endl;
         return false;
      }

      auto strTab = inSections[section.header.link].data.data();
      auto symTab = reinterpret_cast<elf::Symbol *>(section.data.data());
      auto count = section.data.size() / sizeof(elf::Symbol);

      for (unsigned i = 0u; i < count; ++i)
      {
         auto &sym = symTab[i];

         /* Skip any load symbols */
         if (sym.value >= LoadAddress && sym.value < CodeAddress)
            continue;

         auto type = elf::SymbolType(sym.info & 0xF);
         auto binding = elf::SymbolBinding((sym.info >> 4) & 0xF);

         if (type == elf::STT_NOTYPE && sym.value == 0) {
            /* Skip null symbol */
            continue;
         }

         if (type == elf::STT_FILE || type == elf::STT_SECTION) {
            /* Skip file, section symbols */
            continue;
         }

         auto name = strTab + sym.name;
         auto symbol = ElfFile::Symbol::New(sym.value, name, sym.size, type, binding);
         auto inSection = dataSectionIndex[sym.shndx];
         if (inSection && inSection->size != 0)
            symbol->inSection = dataSectionIndex[sym.shndx];

         if (verbose >= verbose_loaded_symbols)
         {
            std::cout << "Symbol " << symbol->name << " 0x" << std::hex << symbol->address
               << " 0x" << symbol->size;
            if (symbol->inSection)
            {
               std::cout << " (" << symbol->inSection->name << ')';
            }
            std::cout << '\n';
         }

         file.addSymbol(symbol);
      }
   }

   /* Read RPL imports */
   for (auto &section : inSections) {
      auto name = std::string { shStrTab + section.header.name };

      if (name.compare(".lib.rplLibs") != 0)
         continue;

      auto rplTab = reinterpret_cast<RplLibsDef *>(section.data.data());
      auto count = section.data.size() / sizeof(RplLibsDef);

      for (unsigned i = 0u; i < count; ++i)
      {
         auto &rpl = rplTab[i];
         auto libName = getLoaderDataPtr<char>(inSections, rpl.name);
         auto lib = ElfFile::RplImportLibrary::New(libName);

         if (verbose >= verbose_loaded_sections)
         {
            std::cout << "Import library " << libName << std::endl;
         }

         for (auto stubAddr = rpl.stubStart; stubAddr < rpl.stubEnd; stubAddr += 4) {
            auto trampAddr = byte_swap(*getLoaderDataPtr<uint32_t>(inSections, stubAddr));

            /* Get the tramp symbol */
            auto trampSymbol = file.findSymbol(trampAddr);

            if (!trampSymbol)
            {
               std::cout << "No symbol found for import @ 0x" << std::hex << trampAddr << " in " << libName << std::endl;
            }

            if (verbose >= verbose_loaded_imports)
            {
               std::cout << "Import symbol " << trampSymbol->name << " 0x" << std::hex << stubAddr << " -> 0x" << trampAddr << std::endl;
            }

            /* Create a new symbol to use for the import */
            auto stubSymbol = ElfFile::Symbol::New(0, trampSymbol->name, 0, elf::STT_FUNC, elf::STB_GLOBAL);
            auto import = ElfFile::RplImport::New(stubAddr, trampAddr, stubSymbol, trampSymbol);

            /* Rename tramp symbol */
            /* These are not needed anymore, right */
            /* trampSymbol->refCount++; */
            trampSymbol->name += "_tramp";

            stubSymbol->refCount++;
            file.addSymbol(stubSymbol);
            file.addImport(lib, import);
         }

         file.addImportLibrary(lib);
      }
   }

   ElfFile::DataSectionPtr prevSection;

   /* Read relocations */
   for (auto &section : inSections) {
      if (section.header.type != elf::SHT_RELA)
         continue;

      auto name = std::string { shStrTab + section.header.name };

      if (name.compare(".rela.dyn") == 0) {
         /* Skip dyn relocations */
         continue;
      }

      auto symTab = reinterpret_cast<elf::Symbol *>(inSections[section.header.link].data.data());
      auto relTab = reinterpret_cast<elf::Rela *>(section.data.data());
      auto count = section.data.size() / sizeof(elf::Rela);

      for (unsigned i = 0u; i < count; ++i)
      {
         auto relocation = std::make_shared<ElfFile::Relocation>();
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

         if (verbose >= verbose_loaded_relocations)
         {
            std::cout << "Relocation #" << std::dec << file.getRelocations().size()
               << " sym index " << index << ' '
               << " offset 0x" << std::hex << rela.offset
               << " addend 0x" << addend;
         }

         if (auto import = file.findImport(addend)) {
            relocation->symbol = import->stubSymbol;
            relocation->addend = 0;
            if (verbose >= verbose_loaded_relocations)
            {
               std::cout << " -> import ";
            }
         } else if (auto symbol = file.findSymbol(addend)) {
            relocation->symbol = symbol;
            relocation->addend = 0;
            if (verbose >= verbose_loaded_relocations)
            {
               std::cout << " -> symbol ";
            }
#if 0
         } else if (addend >= DataAddress && addend < WiiuLoadAddress) {
            relocation->symbol = symData;
            relocation->addend = addend - DataAddress;
            if (verbose >= verbose_loaded_relocations)
            {
               std::cout << " -> data symbol ";
            }
         } else if (addend >= CodeAddress && addend < DataAddress) {
            relocation->symbol = symText;
            relocation->addend = addend - CodeAddress;
            if (verbose >= verbose_loaded_relocations)
            {
               std::cout << " -> code symbol ";
            }
#else
         } else if ((prevSection = file.findSection(addend)) != nullptr) {
            relocation->symbolSection = prevSection;
            relocation->addend = addend - prevSection->address;
            if (verbose >= verbose_loaded_relocations)
            {
               std::cout << " -> section ";
            }
#endif
         } else {
            if (verbose >= verbose_loaded_relocations)
            {
               std::cout << " -> not found" << std::endl;
            }

            /* If we can't find a proper symbol, write the addend in and hope for the best */
            auto ptr = getLoaderDataPtr<uint32_t>(inSections, rela.offset);
            *ptr = addend;

            std::cout << "Unexpected addend " << std::hex << addend << " referenced in relocation section " << name << ", continuing." << std::endl;
            continue;
         }

         if (verbose >= verbose_loaded_relocations)
         {
            if (relocation->symbol)
               std::cout << relocation->symbol->name;
            else if (relocation->symbolSection)
               std::cout << relocation->symbolSection->name;
            if (relocation->addend != 0)
               std::cout << " + " << relocation->addend;
            std::cout << std::endl;
         }

         if (relocation->symbol)
            relocation->symbol->refCount++;
         relocation->target = rela.offset;
         relocation->type = static_cast<elf::RelocationType>(type);

         file.addRelocation(relocation);
      }
   }

   /* Read dyn relocations */
   for (auto &section : inSections)
   {
      if (section.header.type != elf::SHT_RELA)
         continue;

      auto name = std::string { shStrTab + section.header.name };

      if (name.compare(".rela.dyn") != 0)
         continue;

      auto symSection = inSections[section.header.link];
      auto relas = reinterpret_cast<elf::Rela *>(section.data.data());
      auto count = section.data.size() / sizeof(elf::Rela);

      for (unsigned i = 0u; i < count; ++i) {
         auto relocation = std::make_shared<ElfFile::Relocation>();
         auto &rela = relas[i];

         auto type = rela.info & 0xff;
         auto index = rela.info >> 8;
         auto symbol = getSectionSymbol(symSection, index);
         auto addr = symbol->value + rela.addend;

         if (type == elf::R_PPC_NONE)
         {
            /* ignore padding */
            continue;
         }

         if (verbose >= verbose_loaded_dyn_reloc)
         {
            std::cout << "Dyn relocation #" << std::dec << file.getRelocations().size()
               << " offset 0x" << std::hex << rela.offset
               << " addend 0x" << rela.addend
               << " addr 0x" << addr;
         }

         // Why index must be == 0? Cause it's required for R_PPC_RELATIVE
         if (index == 0)
         {
            auto addend = static_cast<uint32_t>(rela.addend);

             if (auto import = file.findImport(addend)) {
                relocation->symbol = import->stubSymbol;
                relocation->addend = 0;
                if (verbose >= verbose_loaded_dyn_reloc)
                {
                  std::cout << " -> import ";
                }
             } else if (auto symbol = file.findSymbol(addend)) {
                relocation->symbol = symbol;
                relocation->addend = addend - symbol->address;
                if (verbose >= verbose_loaded_dyn_reloc)
                {
                   std::cout << " -> symbol ";
                }
#if 0
             } else if (addr >= CodeAddress && addr < DataAddress) {
               index = 1;
               relocation->symbol = symText;
               relocation->addend = rela.addend - CodeAddress;
               if (verbose >= verbose_loaded_dyn_reloc)
               {
                  std::cout << " -> code symbol ";
               }
            } else if (addr >= DataAddress && addr < WiiuLoadAddress) {
               index = 2;
               relocation->symbol = symData;
               relocation->addend = rela.addend - DataAddress;
               if (verbose >= verbose_loaded_dyn_reloc)
               {
                  std::cout << " -> data symbol ";
               }
#else
            } else if ((prevSection = file.findSection(addend)) != nullptr) {
               relocation->symbolSection = prevSection;
               relocation->addend = addend - prevSection->address;
               if (verbose >= verbose_loaded_relocations)
               {
                  std::cout << " -> section ";
               }
#endif
            } else {
               if (verbose >= verbose_loaded_dyn_reloc)
               {
                  std::cout << " -> bad" << std::endl;
               }
               std::cout << "Unexpected relocation symbol address 0x" << std::hex << addr
                  << " in .rela.dyn section" << std::endl;
               return false;
            }
         }
         else
         {
            if (verbose >= verbose_loaded_dyn_reloc)
            {
               std::cout << " -> index " << std::dec << index << std::endl;
            }

            std::cout << "Unexpected relocation symbol index " << std::dec << index
               << " in .rela.dyn section" << std::endl;
            return false;
         }

         if (verbose >= verbose_loaded_dyn_reloc)
         {
            if (relocation->symbol)
               std::cout << relocation->symbol->name;
            else if (relocation->symbolSection)
               std::cout << relocation->symbolSection->name;
            if (relocation->addend != 0)
               std::cout << " + " << relocation->addend;
            std::cout << std::endl;
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

         /* Scrap any compiler/linker garbage */
         if(relocation->target >= CodeAddress && relocation->target < WiiuLoadAddress)
         {
            if (relocation->symbol)
               relocation->symbol->refCount++;
            file.addRelocation(relocation);
         }

      }
   }

   return true;
}

struct OutputSection
{
   std::string name;
   elf::SectionHeader header;
   std::vector<char> data;
   std::shared_ptr<OutputSection> relocationSection;
   std::shared_ptr<ElfFile::Symbol> sectionSymbol;
   uint32_t index = uint32_t(-1);

   bool contains(uint32_t address, uint32_t size = 0)
   {

      if (!(header.flags & elf::SHF_ALLOC) || header.size == 0 || header.size == uint32_t(-1))
         return false;

      auto start = header.addr;
      auto end = start + header.size;

      if (address >= start && address + size <= end)
         return true;
      return false;
   }
};

struct OutputSections
{
   std::vector<std::shared_ptr<OutputSection>> sections;
   std::map<std::string, std::shared_ptr<OutputSection>> sections_by_name;

   void addSection(std::shared_ptr<OutputSection> section)
   {
      section->index = sections.size();
      sections.push_back(section);
      if (verbose >= verbose_output_sections)
      {
         auto& header = section->header;
         std::cout << "Output section #" << std::dec << section->index
            << std::hex << ' ' << section->header.type << ' ' << section->header.flags
            << ' ' << section->name
            << " (0x" << header.addr << " - 0x" << header.addr + header.size << ')'
            << std::endl;
      }
      if (!section->name.empty())
      {
         if (sections_by_name.count(section->name))
         {
            std::cout << "Duplicate output section name " << section->name
               << std::endl;
         }
         else
         {
            sections_by_name[section->name] = section;
         }
      }
   };


   template<typename SymbolIterator>
   SymbolIterator addSection(ElfFile &file, SymbolIterator symbolIterator, std::shared_ptr<OutputSection> section)
   {
      ElfFile::SymbolPtr sectionSymbol;
      if (section->header.size != 0)
      {
         sectionSymbol = ElfFile::Symbol::New(section->header.addr, section->name, -1, elf::STT_SECTION, elf::STB_LOCAL);
         sectionSymbol->outSection = section;
         section->sectionSymbol = sectionSymbol;
      }
      section->index = sections.size();
      sections.push_back(section);

      if (verbose >= verbose_output_sections)
      {
         auto& header = section->header;
         std::cout << "Output section #" << std::dec << section->index
            << std::hex << ' ' << section->header.type << ' ' << section->header.flags
            << ' ' << section->name
            << " (0x" << header.addr << " - 0x" << header.addr + header.size << ')'
            << std::endl;
      }

      if (sections_by_name.count(section->name))
      {
         std::cout << "Duplicate output section name " << section->name
            << std::endl;
      }
      else
      {
         sections_by_name[section->name] = section;
      }

      if (!sectionSymbol)
         return symbolIterator;

      if (verbose >= verbose_loaded_symbols)
      {
         std::cout << "Section symbol " << sectionSymbol->name << " 0x" << std::hex << sectionSymbol->address
            << " 0x" << sectionSymbol->size << '\n';
      }

      return file.insertSymbol(symbolIterator, sectionSymbol) + 1;
   };

   uint32_t
   getSectionIndex(uint32_t address)
   {
      if (address == 0)
         return 0;

      for (unsigned i = 0u; i < sections.size(); ++i)
      {
         if (sections[i]->contains(address))
            return i;
      }

      return -1;
   }

   uint32_t
   getSectionIndex(const std::string &name)
   {
      auto iter = sections_by_name.find(name);
      if (iter == sections_by_name.end())
         return -1;
      return iter->second->index;
   }

   std::shared_ptr<OutputSection>
   getSection(uint32_t address,
      const std::shared_ptr<OutputSection>& hint = nullptr)
   {
      if (address == 0)
         return sections[0];

      if (hint && hint->contains(address))
         return hint;

      for (auto &section : sections) {
         if (section->contains(address))
            return section;
      }

      return nullptr;
   }

   std::shared_ptr<OutputSection>
   getSection(const std::string &name)
   {
      auto iter = sections_by_name.find(name);
      if (iter == sections_by_name.end())
         return nullptr;
      return iter->second;
   }

   const std::vector<std::shared_ptr<OutputSection>>&
   getSections()
   {
      return sections;
   }

};

static bool
write(ElfFile &file, const std::string &filename)
{
   OutputSections outSections;
   auto sectionSymbolItr = file.getSymbols().begin() + 4;

   /* Create NULL section */
   auto nullSection = std::make_shared<OutputSection>();
   memset(&nullSection->header, 0, sizeof(elf::SectionHeader));
   outSections.addSection(nullSection);

   /* Create text/data sections */
   for (auto &section : file.getDataSections())
   {
      auto out = std::make_shared<OutputSection>();
      out->header.name = -1;
      out->header.type = section->type;
      out->header.flags = section->flags;
      out->header.addr = section->address;
      out->header.offset = -1;

      if (section->type == elf::SHT_NOBITS)
         out->header.size = section->size;
      else
         out->header.size = section->data.size();

      out->header.link = 0;
      out->header.info = 0;

      if (section->address == DataAddress)
      {
         out->header.addralign = 4096;
         out->header.flags |= elf::SHF_WRITE; /* .rodata needs to be writable? */
      }
      else
         out->header.addralign = 256;

      out->header.entsize = 0;

      /* Add section */
      out->name = section->name;
      out->data = section->data;
      section->outSection = out;
      sectionSymbolItr = outSections.addSection(file, sectionSymbolItr, out);
   }

   std::shared_ptr<OutputSection> targetSection;

   /* Create relocation sections */
   for (auto &relocation : file.getRelocations()) {
      targetSection = outSections.getSection(relocation->target, targetSection);

      if (!targetSection)
      {
         std::cout << "Error could not find section for relocation" << std::endl;
         return false;
      }

      relocation->targetSection = targetSection;

      if (!targetSection->relocationSection)
      {
         /* Create new relocation section */
         auto out = std::make_shared<OutputSection>();
         out->header.name = -1;
         out->header.type = elf::SHT_RELA;
         out->header.flags = 0;
         out->header.addr = 0;
         out->header.offset = -1;
         out->header.size = -1;
         out->header.link = -1;
         out->header.info = targetSection->index;
         out->header.addralign = 4;
         out->header.entsize = sizeof(elf::Rela);

         /* Add section */
         out->name = ".rela" + targetSection->name;
         sectionSymbolItr = outSections.addSection(file, sectionSymbolItr, out);
         targetSection->relocationSection = out;
      }
   }

   /* Calculate sizes of symbol/string tables so RPL imports are placed after them */
   auto loadAddress = 0xC0000000;
   auto predictStrTabSize = 1;
   auto predictSymTabSize = 1;
   auto predictShstrTabSize = 1;

   for (auto &symbol : file.getSymbols())
   {
      predictStrTabSize += symbol->name.size() + 1;
      predictSymTabSize += sizeof(elf::Symbol);
   }

   for (auto &section : outSections.getSections())
      predictShstrTabSize += section->name.size() + 1;

   predictStrTabSize = align_up(predictStrTabSize, 0x10);
   predictSymTabSize = align_up(predictSymTabSize, 0x10);
   predictShstrTabSize = align_up(predictShstrTabSize, 0x10);
   loadAddress += predictStrTabSize + predictSymTabSize + predictShstrTabSize;

   /* Create RPL import sections, .fimport_*, .dimport_* */
   for (auto &lib : file.getImportLibs())
   {
      auto out = std::make_shared<OutputSection>();
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

      /* Calculate size */
      auto nameSize = align_up(8 + lib->name.size(), 8);
      auto stubSize = 8 + 8 * lib->imports.size();
      out->header.size = std::max(nameSize, stubSize);
      out->data.resize(out->header.size);

      /* Setup data */
      auto imports = reinterpret_cast<elf::RplImport*>(out->data.data());
      imports->count = lib->imports.size();
      imports->signature = crc32(0, Z_NULL, 0);
      memcpy(imports->name, lib->name.data(), lib->name.size());
      imports->name[lib->name.size()] = 0;

      /* Update address of import symbols */
      for (unsigned i = 0u; i < lib->imports.size(); ++i)
         lib->imports[i]->stubSymbol->address = loadAddress + 8 + i * 8;

      loadAddress = align_up(loadAddress + out->header.size, 4);

      /* Add section */
      sectionSymbolItr = outSections.addSection(file, sectionSymbolItr, out);
   }

   /* Prune out unneeded symbols - but keep those used for relocations! */
   file.pruneSymbols();
   file.indexSymbols();

   /* NOTICE: FROM NOW ON DO NOT MODIFY mSymbols */

   auto symText = file.findSymbol(CodeAddress);
   auto symData = file.findSymbol(DataAddress);

   int number = -1;
   /* Convert relocations */
   for (auto &relocation : file.getRelocations()) {
      ++number;
      auto targetSection = relocation->targetSection;

      if (!targetSection || !targetSection->relocationSection) {
         std::cout << "Error could not find section for relocation" << std::endl;
         return false;
      }

      /* Get address of relocation->target */
      auto relocationSection = targetSection->relocationSection;

      /* Find symbol this relocation points to */
      if (!relocation->symbol && relocation->symbolSection && relocation->symbolSection->outSection)
         relocation->symbol = relocation->symbolSection->outSection->sectionSymbol;

      auto symbol = relocation->symbol;

      auto idx = symbol->outIndex;
      bool symbol_found = idx != -1;

      if ((verbose >= verbose_convert_relocations) || !symbol_found)
      {
         std::cout << "Converting relocation #" << std::dec << number
            << std::hex << " @ 0x" << relocation->target;
      }
      if ((verbose >= verbose_convert_relocations) && symbol_found)
      {
         std::cout << "-> symbol " << std::dec << idx
            << ' ' << symbol->name << std::endl;
      }

      /* If the symbol doesn't exist but it is within DATA or TEXT, use those symbols + an addend */
      if (!symbol_found) {
         if (relocation->symbol->address >= CodeAddress && relocation->symbol->address < DataAddress) {
            idx = 1;
            relocation->addend = relocation->symbol->address - CodeAddress;
            relocation->symbol = symText;
            std::cout << "-> no symbol, using TEXT" << std::endl;
         } else if (relocation->symbol->address >= DataAddress && relocation->symbol->address < WiiuLoadAddress) {
            idx = 2;
            relocation->addend = relocation->symbol->address - DataAddress;
            relocation->symbol = symData;
            std::cout << "-> no symbol, using DATA" << std::endl;
         } else {
            std::cout << "-> invalid" << std::endl;
            std::cout << "Could not find matching symbol for relocation" << std::endl;
            return false;
         }
      }

      /* Create relocation */
      elf::Rela rela;
      rela.info = relocation->type | idx << 8;

      if(relocation->type == elf::R_PPC_RELATIVE)
         rela.info = elf::R_PPC_ADDR32 | idx << 8;

      rela.addend = relocation->addend;
      rela.offset = relocation->target;

      /* Append to relocation section data */
      char *relaData = reinterpret_cast<char *>(&rela);
      relocationSection->data.insert(relocationSection->data.end(), relaData, relaData + sizeof(elf::Rela));
   }

   /* String + Symbol sections */
   auto symTabSection = std::make_shared<OutputSection>();
   auto strTabSection = std::make_shared<OutputSection>();
   auto shStrTabSection = std::make_shared<OutputSection>();

   symTabSection->name = ".symtab";
   strTabSection->name = ".strtab";
   shStrTabSection->name = ".shstrtab";

   outSections.addSection(symTabSection);
   auto symTabIndex = symTabSection->index;

   outSections.addSection(strTabSection);
   auto strTabIndex = strTabSection->index;

   outSections.addSection(shStrTabSection);
   auto shStrTabIndex = shStrTabSection->index;

   /* Update relocation sections to link to symtab */
   for (auto &section : outSections.getSections())
   {
      if (section->header.type == elf::SHT_RELA)
         section->header.link = symTabIndex;

      if (section->header.type != elf::SHT_NOBITS)
         section->header.size = section->data.size();

      if (section->sectionSymbol)
      {
         section->sectionSymbol->address = section->header.addr;
         section->sectionSymbol->size = section->header.size;
      }
   }

   /* Create .strtab */
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

   /* Add all symbol names to data, update symbol->outNamePos */
   strTabSection->data.push_back(0);

   for (auto &symbol : file.getSymbols())
   {
      if (symbol->name.empty())
         symbol->outNamePos = 0;
      else
      {
         symbol->outNamePos = static_cast<uint32_t>(strTabSection->data.size());
         std::copy(symbol->name.begin(), symbol->name.end(), std::back_inserter(strTabSection->data));
         strTabSection->data.push_back(0);
      }
   }

   /* Create .symtab */
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

   std::shared_ptr<OutputSection> symSection;

   for (auto &symbol : file.getSymbols())
   {
      elf::Symbol sym;
      if (symbol->outSection)
         symSection = symbol->outSection;
      else if (symbol->inSection && symbol->inSection->outSection)
         symSection = symbol->inSection->outSection;
      else if (symbol->type == elf::STT_SECTION && symbol->address == 0)
         symSection = outSections.getSection(symbol->name);
      else
         symSection = outSections.getSection(symbol->address, symSection);

      if (symSection == nullptr)
      {
         std::cout << "Could not find section for symbol " << symbol->name 
            << " 0x" << std::hex << symbol->address << std::endl;
         return false;
      }

      sym.name = symbol->outNamePos;
      sym.value = symbol->address;
      sym.size = symbol->size;
      sym.info = symbol->type | (symbol->binding << 4);
      sym.other = 0;
      sym.shndx = symSection->index;

      if (verbose >= verbose_output_symbols)
      {
         std::cout << std::right
            << std::hex << std::setfill('0') << std::setw(8) << symbol->address
            << ' ' << std::setw(8) << symbol->size << std::dec
            << ' ' << std::setfill(' ') << std::setw(3) << sym.shndx
            << ' ' << std::setw(12) << std::left << symSection->name
            << ' ' << symbol->name << std::endl;
      }

      /* Compound symbol crc into section crc */
      if (symSection->header.type == elf::SHT_RPL_IMPORTS && symbol->type != elf::STT_SECTION)
      {
         auto rplImport = reinterpret_cast<elf::RplImport*>(symSection->data.data());
         rplImport->signature = crc32(rplImport->signature, reinterpret_cast<Bytef *>(strTabSection->data.data() + sym.name),strlen(strTabSection->data.data() + sym.name)+1);
      }

      /* Append to symtab data */
      char *symData = reinterpret_cast<char *>(&sym);
      symTabSection->data.insert(symTabSection->data.end(), symData, symData + sizeof(elf::Symbol));
   }

   /* Finish SHT_RPL_IMPORTS signatures */
   Bytef *zero_buffer = reinterpret_cast<Bytef *>(calloc(0x10, 1));
   for (auto &section : outSections.getSections())
   {
      if(section->header.type == elf::SHT_RPL_IMPORTS)
      {
         auto rplImport = reinterpret_cast<elf::RplImport*>(section->data.data());
         rplImport->signature = crc32(rplImport->signature, zero_buffer, 0xE);
      }
   }
   free(zero_buffer);

   /* Create .shstrtab */
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

   /* Add all section header names to data, update section->header.name */
   shStrTabSection->data.push_back(0);

   for (auto &section : outSections.getSections())
   {
      if (section->name.empty())
         section->header.name = 0;
      else
      {
         section->header.name = shStrTabSection->data.size();
         std::copy(section->name.begin(), section->name.end(), std::back_inserter(shStrTabSection->data));
         shStrTabSection->data.push_back(0);
      }
   }

   loadAddress = 0xC0000000;

   /* Update symtab, strtab, shstrtab section addresses */
   symTabSection->header.addr = loadAddress;
   symTabSection->header.size = symTabSection->data.size();

   loadAddress = align_up(symTabSection->header.addr + predictSymTabSize, 16);
   strTabSection->header.addr = loadAddress;
   strTabSection->header.size = strTabSection->data.size();

   loadAddress = align_up(strTabSection->header.addr + predictStrTabSize, 16);
   shStrTabSection->header.addr = loadAddress;
   shStrTabSection->header.size = shStrTabSection->data.size();

   /* Create SHT_RPL_FILEINFO section */
   auto fileInfoSection = std::make_shared<OutputSection>();
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

   /* Count file info textSize, dataSize, loadSize */
   for (auto &section : outSections.getSections())
   {
      auto size = section->data.size();

      if (section->header.type == elf::SHT_NOBITS)
         size = section->header.size;

      if (section->header.addr >= CodeAddress && section->header.addr < DataAddress)
      {
         auto val = section->header.addr.value() + section->header.size.value() - CodeAddress;
         if(val > fileInfo.textSize)
            fileInfo.textSize = val;
      }
      else if (section->header.addr >= DataAddress && section->header.addr < WiiuLoadAddress)
      {
         auto val = section->header.addr.value() + section->header.size.value() - DataAddress;
         if(val > fileInfo.dataSize)
            fileInfo.dataSize = val;
      }
      else if (section->header.addr >= WiiuLoadAddress)
      {
         auto val = section->header.addr.value() + section->header.size.value() - WiiuLoadAddress;
         if(val > fileInfo.loadSize)
            fileInfo.loadSize = val;
      }
      else if (section->header.addr == 0 && section->header.type != elf::SHT_RPL_CRCS && section->header.type != elf::SHT_RPL_FILEINFO)
         fileInfo.tempSize += (size + 128);
   }

   /* TODO: These were calculated based on observation, however some games differ. */
   fileInfo.sdaBase = align_up(DataAddress + fileInfo.dataSize + fileInfo.heapSize, 64);
   fileInfo.sda2Base = align_up(DataAddress + fileInfo.heapSize, 64);

   char *fileInfoData = reinterpret_cast<char *>(&fileInfo);
   fileInfoSection->data.insert(fileInfoSection->data.end(), fileInfoData, fileInfoData + sizeof(elf::RplFileInfo));

   /* Create SHT_RPL_CRCS section */
   auto crcSection = std::make_shared<OutputSection>();
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

   outSections.addSection(crcSection);
   outSections.addSection(fileInfoSection);

   std::vector<uint32_t> sectionCRCs;

   for (auto &section : outSections.getSections())
   {
      auto crc = 0u;

      if (!section->data.empty())
      {
         crc = crc32(0, Z_NULL, 0);
         crc = crc32(crc, reinterpret_cast<Bytef *>(section->data.data()), section->data.size());
      }

      sectionCRCs.push_back(byte_swap(crc));
   }

   char *crcData = reinterpret_cast<char *>(sectionCRCs.data());
   crcSection->data.insert(crcSection->data.end(), crcData, crcData + sizeof(uint32_t) * sectionCRCs.size());

   /* Update section sizes and offsets */
   auto shoff = align_up(sizeof(elf::Header), 64);
   auto dataOffset = align_up(shoff + outSections.getSections().size() * sizeof(elf::SectionHeader), 64);

   /* Add CRC and FileInfo sections first */
   for (auto &section : outSections.getSections())
   {
      if (section->header.type != elf::SHT_RPL_CRCS && section->header.type != elf::SHT_RPL_FILEINFO)
         continue;

      if (section->header.type != elf::SHT_NOBITS)
         section->header.size = section->data.size();

      if (!section->data.empty())
      {
         section->header.offset = dataOffset;
         dataOffset = align_up(section->header.offset + section->data.size(), 64);
      }
      else
         section->header.offset = 0;
   }

   /* Add data sections next */
   for (auto &section : outSections.getSections())
   {
      if(section->header.offset != -1)
         continue;

      if (section->header.addr < DataAddress || section->header.addr >= WiiuLoadAddress)
         continue;

      if (section->header.type != elf::SHT_NOBITS)
         section->header.size = section->data.size();

      section->header.offset = dataOffset;
      dataOffset = align_up(section->header.offset + section->data.size(), 64);
   }

   /* Add load sections next */
   for (auto &section : outSections.getSections())
   {
      if(section->header.offset != -1)
         continue;

      if (section->header.addr < WiiuLoadAddress)
         continue;

      if (section->header.type != elf::SHT_NOBITS)
         section->header.size = section->data.size();

      if (!section->data.empty()) {
         section->header.offset = dataOffset;
         dataOffset = align_up(section->header.offset + section->data.size(), 64);
      } else {
         section->header.offset = 0;
      }
   }

   /* Everything else */
   for (auto &section : outSections.getSections()) {
      if(section->header.offset != -1)
         continue;

      if (section->header.type != elf::SHT_NOBITS)
         section->header.size = section->data.size();

      if (!section->data.empty())
      {
         section->header.offset = dataOffset;
         dataOffset = align_up(section->header.offset + section->data.size(), 64);
      }
      else
         section->header.offset = 0;
   }

   /* Write to file */
   std::ofstream out { filename, std::ofstream::binary };
   std::vector<char> padding;

   if (!out.is_open())
   {
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
   header.shnum = outSections.getSections().size();
   header.shentsize = sizeof(elf::SectionHeader);
   header.flags = 0;
   header.ehsize = sizeof(elf::Header);
   header.shstrndx = shStrTabIndex;
   out.write(reinterpret_cast<char *>(&header), sizeof(elf::Header));

   /* Write section headers */
   out.seekp(header.shoff.value());

   for (auto &section : outSections.getSections())
      out.write(reinterpret_cast<char *>(&section->header), sizeof(elf::SectionHeader));

   /* Write section data */
   for (auto &section : outSections.getSections())
   {
      if (!section->data.empty())
      {
         out.seekp(section->header.offset.value());
         out.write(section->data.data(), section->data.size());
      }
   }

   return true;
}

int main(int argc, char **argv)
{
   int arg_start = 1;

   while (argc >= arg_start + 1 && argv[arg_start][0] == '-')
   {
      if (argv[arg_start] == std::string("--"))
      {
         ++arg_start;
         break;
      }
      else if (argv[arg_start] == std::string("-v"))
      {
         ++verbose;
         ++arg_start;
      }
   }

   if (argc < arg_start + 1)
   {
      std::cout << "Usage: " << argv[0] << " <src> <dst>" << std::endl;
      return -1;
   }

   ElfFile elf;
   auto src = argv[arg_start];
   auto dst = argv[arg_start + 1];

   if (!read(elf, src))
      return -1;

   if (!write(elf, dst))
      return -1;

   return 0;
}
