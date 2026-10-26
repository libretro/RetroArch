/* Tell linker where our application entry is so the garbage collect can work correct */
ENTRY(__entry_menu);

SECTIONS {
	. = 0x00802000;
	.text : {
		*(.text*);
	}
	.rodata : {
		*(.rodata*);
	}
	. = 0x10000000;
	.data : {
		*(.data*);

		__sdata_start = .;
		*(.sdata*);
		__sdata_end = .;

		__sdata2_start = .;
		*(.sdata2*);
		__sdata2_end = .;
	}
	.bss : {
		__bss_start = .;
		*(.bss*);
		*(.sbss*);
		*(COMMON);
		__bss_end = .;
	}
	__CODE_END = .;

}

/******************************************************** FS ********************************************************/
/* coreinit.rpl difference in addresses 0xFE3C00 */
