/* Program layout shared by rvl.ld and dol.ld, which set the load
 * address.  The boot stack sits after .bss; the heap starts after it. */

OUTPUT_ARCH(powerpc:common)
ENTRY(_start)

PROVIDE(__gk_stack_size = 0x40000);

/* Only the sections load: the ELF headers in front of them would land
 * on the low-memory globals. */
PHDRS
{
	text PT_LOAD FLAGS(5);
	data PT_LOAD FLAGS(6);
}

SECTIONS
{
	. = __gk_load;

	.crt0 : { KEEP(*(.crt0)) } :text

	.init : { KEEP(*(SORT_NONE(.init))) }
	.text : {
		*(.text.unlikely .text.*_unlikely .text.unlikely.*)
		*(.text.startup .text.startup.*)
		*(.text.hot .text.hot.*)
		*(.text .stub .text.* .gnu.linkonce.t.*)
		*(.glink)
	}
	.fini : { KEEP(*(SORT_NONE(.fini))) }
	PROVIDE(__etext = .);

	.rodata : { *(.rodata .rodata.* .gnu.linkonce.r.*) *(.rodata1) }
	.eh_frame_hdr : { *(.eh_frame_hdr) }
	.eh_frame : { KEEP(*(.eh_frame)) }
	.gcc_except_table : { *(.gcc_except_table .gcc_except_table.*) }

	/* The small-data bases sit 32 KiB into their sections so 16-bit
	 * offsets reach the whole 64 KiB either way. */
	.sdata2 : {
		PROVIDE(__SDATA2_START__ = .);
		_SDA2_BASE_ = . + 0x8000;
		*(.sdata2 .sdata2.* .gnu.linkonce.s2.*)
	}
	.sbss2 : {
		PROVIDE(__SBSS2_START__ = .);
		*(.sbss2 .sbss2.* .gnu.linkonce.sb2.*)
		PROVIDE(__SBSS2_END__ = .);
	}

	.ctors : {
		KEEP(*crtbegin.o(.ctors))
		KEEP(*(EXCLUDE_FILE(*crtend.o) .ctors))
		KEEP(*(SORT(.ctors.*)))
		KEEP(*(.ctors))
	} :data
	.dtors : {
		KEEP(*crtbegin.o(.dtors))
		KEEP(*(EXCLUDE_FILE(*crtend.o) .dtors))
		KEEP(*(SORT(.dtors.*)))
		KEEP(*(.dtors))
	}
	.preinit_array : {
		PROVIDE(__preinit_array_start = .);
		KEEP(*(.preinit_array))
		PROVIDE(__preinit_array_end = .);
	}
	.init_array : {
		PROVIDE(__init_array_start = .);
		KEEP(*(SORT(.init_array.*)))
		KEEP(*(.init_array))
		PROVIDE(__init_array_end = .);
	}
	.fini_array : {
		PROVIDE(__fini_array_start = .);
		KEEP(*(SORT(.fini_array.*)))
		KEEP(*(.fini_array))
		PROVIDE(__fini_array_end = .);
	}

	.data : {
		*(.data .data.* .gnu.linkonce.d.*)
		*(.data1)
	}
	.got1 : { *(.got1) }
	.got2 : { *(.got2) }
	.got : { *(.got.plt) *(.got) }

	.sdata : {
		PROVIDE(__SDATA_START__ = .);
		_SDA_BASE_ = . + 0x8000;
		*(.sdata .sdata.* .gnu.linkonce.s.*)
		*(.lit8) *(.lit4)
	}

	. = ALIGN(32);
	__gk_bss_start = .;
	.sbss : {
		PROVIDE(__sbss_start = .);
		*(.sbss .sbss.* .gnu.linkonce.sb.*)
		*(.scommon)
		PROVIDE(__sbss_end = .);
		PROVIDE(__SBSS_END__ = .);
	}
	.bss : {
		PROVIDE(__bss_start = .);
		*(.bss .bss.* .gnu.linkonce.b.*)
		*(COMMON)
		. = ALIGN(32);
		PROVIDE(__bss_end = .);
	}
	__gk_bss_end = .;

	.gk_stack : {
		__gk_stack_lo = .;
		. += __gk_stack_size;
		__gk_stack_top = .;
	}
	__gk_image_end = .;

	.comment 0 : { *(.comment) }
	.debug 0 : { *(.debug) }
	.debug_info 0 : { *(.debug_info .gnu.linkonce.wi.*) }
	.debug_abbrev 0 : { *(.debug_abbrev) }
	.debug_line 0 : { *(.debug_line .debug_line.* .debug_line_end) }
	.debug_frame 0 : { *(.debug_frame) }
	.debug_str 0 : { *(.debug_str) }
	.debug_loc 0 : { *(.debug_loc) }
	.debug_aranges 0 : { *(.debug_aranges) }
	.debug_ranges 0 : { *(.debug_ranges) }
	.debug_rnglists 0 : { *(.debug_rnglists) }
	.debug_loclists 0 : { *(.debug_loclists) }
	.debug_line_str 0 : { *(.debug_line_str) }
	/DISCARD/ : { *(.note.GNU-stack) *(.gnu_debuglink) }
}
