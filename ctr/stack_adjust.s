
	.arm
	.align 2

	.global	initSystem
	.type	initSystem,	%function

initSystem:
	ldr	r2, =saved_stack
	str	sp, [r2]
	str	lr, [r2,#4]

	bics	sp, sp, #7

	bl	__libctru_init

	bl	__appInit
	bl	__libc_init_array

	ldr	r2, =saved_stack
	ldr	lr, [r2,#4]
 	bx	lr


	.global	__ctru_exit
	.type	__ctru_exit,	%function

__ctru_exit:
	bl	__libc_fini_array
	bl	__appExit


	ldr	r2, =saved_stack
	ldr	sp, [r2]
	b	__libctru_exit

	.data
	.align 2
__stacksize__:
	.word	0x100000
	.weak	__stacksize__

	.bss
	.align 2
saved_stack:
	.space 8



