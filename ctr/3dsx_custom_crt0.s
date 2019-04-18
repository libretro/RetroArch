@---------------------------------------------------------------------------------
@ 3DS processor selection
@---------------------------------------------------------------------------------
	.cpu mpcore
@---------------------------------------------------------------------------------

@---------------------------------------------------------------------------------
	.section ".crt0"
	.global _start, __service_ptr, __apt_appid, __heap_size_hbl, __linear_heap_size_hbl, __system_arglist, __system_runflags
@---------------------------------------------------------------------------------
	.align 2
	.arm
@---------------------------------------------------------------------------------
_start:
@---------------------------------------------------------------------------------
	b startup
	.ascii "_prm"
__service_ptr:
	.word 0 @ Pointer to service handle override list -- if non-NULL it is assumed that we have been launched from a homebrew launcher
__apt_appid:
	.word 0x300 @ Program APPID
__heap_size_hbl:
	.word 24*1024*1024 @ Default heap size (24 MiB)
__linear_heap_size_hbl:
	.word 32*1024*1024 @ Default linear heap size (32 MiB)
__system_arglist:
	.word 0 @ Pointer to argument list (argc (u32) followed by that many NULL terminated strings)
__system_runflags:
	.word 0 @ Flags to signal runtime restrictions to ctrulib
startup:
	@ Save return address
	mov r4, lr
	bics sp, sp, #7

	@ Clear the BSS section
	ldr r0, =__bss_start__
	ldr r1, =__bss_end__
	sub r1, r1, r0
	bl  ClearMem

	@ System initialization
	mov r0, r4
	bl initSystem

	@ Set up argc/argv arguments for main()
	ldr r0, =__system_argc
	ldr r1, =__system_argv
	ldr r0, [r0]
	ldr r1, [r1]

	@ Jump to user code
	ldr r3, =main
	ldr lr, =__ctru_exit
	bx  r3

@---------------------------------------------------------------------------------
@ Clear memory to 0x00 if length != 0
@  r0 = Start Address
@  r1 = Length
@---------------------------------------------------------------------------------
ClearMem:
@---------------------------------------------------------------------------------
	mov  r2, #3     @ Round down to nearest word boundary
	add  r1, r1, r2 @ Shouldn't be needed
	bics r1, r1, r2	@ Clear 2 LSB (and set Z)
	bxeq lr         @ Quit if copy size is 0

	mov	r2, #0
ClrLoop:
	stmia r0!, {r2}
	subs  r1, r1, #4
	bne   ClrLoop

	bx lr
