OUTPUT_FORMAT("elf32-powerpc")
OUTPUT_ARCH(powerpc:common)

ENTRY(_start)

MEMORY {
   system   (rwx) : ORIGIN = 0x01000000, LENGTH = 32M
   code     (rwx) : ORIGIN = 0x02000000, LENGTH = 224M
   data     (rw)  : ORIGIN = 0x10000000, LENGTH = 800M
   load     (rwx) : ORIGIN = 0xC0000000, LENGTH = 128M
}

PHDRS {
   hdr_text    PT_LOAD  FILEHDR  PHDRS FLAGS(0x01 | 0x04);
   hdr_data    PT_LOAD                 FLAGS(0x02 | 0x04);
   hdr_srodata PT_LOAD                 FLAGS(0x04);
   hdr_sdata   PT_LOAD                 FLAGS(0x02 | 0x04);
   hdr_sysdata PT_LOAD                 FLAGS(0x02 | 0x04);
}

SECTIONS {
   . = ORIGIN(code);
   PROVIDE(__code_start = .);

   .syscall ALIGN(32) : { *(.syscall) } : hdr_text

   /* Standard code section */
   .text ALIGN(32) : {
      /* .init */
      KEEP( *(.crt0) )
      KEEP( *(.init) )
      . = ALIGN(4);

      /* .text */
      *(.text)
      *(.text.*)
      *(.rodata .rodata.*)
      *(.gnu.linkonce.r.*)
      *(.rodata1)
      *(.glue_7)
      *(.glue_7t)
      *(.stub)
      *(.gnu.warning)
      *(.gnu.linkonce.t*)
      . = ALIGN(4);

      *(.rplTramp.text)
      *(SORT(.rplTramp.text.*))
      . = ALIGN(4);

      /* .fini */
      KEEP( *(.fini) )
      . = ALIGN(4);

   } : hdr_text

   PROVIDE(__code_end = .);

   /* Standard data sections */
   . = ORIGIN(data);

   .data ALIGN(256) : {
      *(.data)
      *(.data.*)
      *(.eh_frame)
      *(.eh_frame_hdr)
      *(.gnu.linkonce.d.*)
      SORT(CONSTRUCTORS)

      . = ALIGN(32);
      __sdata_start = .;
      *(.sdata)
      *(.sdata.*)
      __sdata_end = .;

      . = ALIGN(32);
      __sdata2_start = .;
      *(.sdata2)
      *(.sdata2.*)
      *(.gnu.linkonce.s2.*)
      __sdata2_end = .;
   } : hdr_sdata

   .tdata ALIGN(256) :
   {
      __tdata_lma = .;
      *(.tdata)
      *(.tdata.*)
      *(.gnu.linkonce.td.*)
      . = ALIGN(4);
      __tdata_lma_end = .;
   } : hdr_data

   .preinit_array ALIGN(256) :
   {
      PROVIDE (__preinit_array_start = .);
      KEEP (*(.preinit_array))
      PROVIDE (__preinit_array_end = .);
   } : hdr_data

   .init_array ALIGN(256) :
   {
      PROVIDE (__init_array_start = .);
      KEEP (*(SORT(.init_array.*)))
      KEEP (*(.init_array))
      PROVIDE (__init_array_end = .);
   } : hdr_data

   .fini_array ALIGN(256) :
   {
      PROVIDE (__fini_array_start = .);
      KEEP (*(.fini_array))
      KEEP (*(SORT(.fini_array.*)))
      PROVIDE (__fini_array_end = .);
   } : hdr_data

   .ctors ALIGN(256) :
   {
      __CTOR_LIST__ = .;
      KEEP (*crtbegin.o(.ctors)) /* MUST be first -- GCC requires it */
      KEEP (*(EXCLUDE_FILE (*crtend.o) .ctors))
      KEEP (*(SORT(.ctors.*)))
      KEEP (*(.ctors))
      __CTOR_END__ = .;
   } : hdr_data

   .dtors ALIGN(256) :
   {
      __DTOR_LIST__ = .;
      KEEP (*crtbegin.o(.dtors))
      KEEP (*(EXCLUDE_FILE (*crtend.o) .dtors))
      KEEP (*(SORT(.dtors.*)))
      KEEP (*(.dtors))
      __DTOR_END__ = .;
   } : hdr_data

   __bss_start__ = .;
   .bss ALIGN(256) :
   {
      *(.dynbss)
      *(.bss)
      *(.bss.*)
      *(.gnu.linkonce.b*)
      *(COMMON)
      . = ALIGN(32);

      __sbss_start = .;
      *(.sbss)
      *(.sbss.*)
      __sbss_end = .;
      . = ALIGN(32);

      __sbss2_start = .;
      *(.sbss2)
      *(.sbss2.*)
      *(.gnu.linkonce.sb2.*)
      __sbss2_end = .;
      . = ALIGN(32);

      /* Reserve space for the TLS segment of the main thread */
      __tls_start = .;

      __tbss_start = .;
      *(.tbss)
      *(.tbss.*)
      *(.gnu.linkonce.tb.*)
      *(.tcommon)
      __tbss_end = .;

      . += + SIZEOF(.tdata);
      __tls_end = .;
      . = ALIGN(32);
   } : hdr_data
   __bss_end__ = .;

   _end = .;
   PROVIDE(end = .);

   /* System stuff is for our elf2rpl converter to go through */
   . = ORIGIN(system);

   /* Contains the name of RPLs, referenced by .lib.rplLibs */
   .rodata.rplNames ALIGN(32) : {
      *(.rodata.rplNames)
   } : hdr_sysdata

   /*
    * List of RPL libraries to import, in format:
    *   uint32_t nameAddress -> .rodata.rplNames
    *   uint32_t firstFuncEntry -> .data.rplFuncStubs
    */
   .lib.rplLibs ALIGN(32) : {
      *(.lib.rplLibs)
      KEEP (*(.lib.rplLibs*))
   } : hdr_sysdata

   /*
    * List of functions an RPL exports, in format:
    *   uint32_t trampAddress
    */
   .data.rplFuncStubs ALIGN(32) : {
      *(.data.rplFuncStubs)
   } : hdr_sysdata

   /* Required compiler trash */
   .fixup ALIGN(32) : { *(.fixup*) } : hdr_sysdata
   .got ALIGN(32) : { *(.got*) } : hdr_sysdata
   .gcc_except_table ALIGN(32) : { *(.gcc_except_table*) } : hdr_sysdata
   .hash ALIGN(32) : { *(.hash) } : hdr_sysdata
   .dynsym ALIGN(32) : { *(.dynsym) } : hdr_sysdata

   /* Put all dynamic loader relocations into one section */
   .rela.dyn ALIGN(32) : {
      *(.rela.dyn)
      *(.rela.data.rplFuncStubs)
      *(.rela.lib.rplLibs)
   } : hdr_sysdata

   /* Relocations for .rodata sections */
   .rela.rodata ALIGN(32) :
   {
      *(.rela.rodata .rela.rodata.*)
   } : hdr_sysdata

   /* Relocations for .text sections */
   .rela.text ALIGN(32) :
   {
      *(.rela.text .rela.text.*)
      *(.rela.rplTramp.text)
   } : hdr_sysdata

   /* Relocations for .data sections */
   .rela.data ALIGN(32) :
   {
      *(.rela.data .rela.data.*)
   } : hdr_sysdata

   /* Relocations for .bss sections */
   .rela.bss ALIGN(32) :
   {
      *(.rela.bss .rela.bss.*)
   } : hdr_sysdata

   /* Symbol tables */
   .shstrtab ALIGN(32) : { *(.shstrtab) } : hdr_sysdata
   .symtab ALIGN(32) : { *(.symtab) } : hdr_sysdata
   .strtab ALIGN(32) : { *(.strtab) } : hdr_sysdata

   /DISCARD/ : {
      *(.interp)
      *(.dynstr)
      *(.dynamic)
      *(.comment)
   }

   __SDATA_START__ = __sdata_start;
   __SBSS_END__ = __sbss_end;

   __SDATA2_START__ = __sdata2_start;
   __SBSS2_END__ = __sbss2_end;

   _SDA_BASE_ = __sbss_end;
   _SDA2_BASE_ = __sdata2_start + ((__sbss2_end - __sdata2_start) / 2);
}
