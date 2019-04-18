#include <asm.h>

	.text
	.section .init

	# crt0.s file for the GameCube V1.1 by Costis (costis@gbaemu.com)!
	#
	# Updates: Added support for clearing the BSS section so that global
	#	     variables are cleared to 0 upon start-up.
	#
	# This is start-up code for initializing the GameCube system and hardware
	# before executing the actual user program code. It clears the GPR's,
	# initializes the FPR's, initializes the Data, Code, and L2 caches, clears
	# and initializes SPR's, and disables exceptions (interrupts).
	#
	# Have fun!!! Please e-mail any suggestions or bugs to costis@gbaemu.com.

	# Entry Point

	.extern __InitBATS
	.extern __InitPS
	.extern __InitFPRS
	.extern __InitCache
	.extern __InitSystem
	.extern __sbss_start, __bss_end, __CheckARGV
	.globl _start, __main, __system_argv
_start:
	b	startup
	.long	0x5f617267
__argv:
	.long	0		# argv magic
	.long 0		# command line
	.long 0		# command line length
	.long	0		# argc
	.long	0		#	argv
	.long 0		# end address of argv

startup:
	bl		__InitBATS			# Initialize BATs to a clear and known state
	bl		__InitGPRS			# Initialize the General Purpose Registers
	bl		__InitHardware		# Initialize some aspects of the Hardware
	bl		__InitSystem		# Initialize more cache aspects, clear a few SPR's, and disable interrupts.

	bl		__CheckARGV		# check for argv & initialise struct

	lis		r3,__isIPL@h
	ori		r3,r3,__isIPL@l
	cmplwi	r3,0
	bne		1f

	# Clear the SBSS section!
	lis		r3,__sbss_start@h
	ori		r3,r3,__sbss_start@l
	li		r4,0
	lis		r5,__sbss_end@h
	ori		r5,r5,__sbss_end@l
	sub		r5,r5,r3
	bl		_memset

	# Clear the BSS section!
	lis		r3,__bss_start@h
	ori		r3,r3,__bss_start@l
	li		r4,0
	lis		r5,__bss_end@h
	ori		r5,r5,__bss_end@l
	sub		r5,r5,r3
	bl		_memset

1:
	b      SYS_Init # Branch to the user code!

__InitGPRS:
	# Clear all of the GPR's to 0
	li      r0,0
	li      r3,0
	li      r4,0
	li      r5,0
	li      r6,0
	li      r7,0
	li      r8,0
	li      r9,0
	li      r10,0
	li      r11,0
	li      r12,0
	li      r14,0
	li      r15,0
	li      r16,0
	li      r17,0
	li      r18,0
	li      r19,0
	li      r20,0
	li      r21,0
	li      r22,0
	li      r23,0
	li      r24,0
	li      r25,0
	li      r26,0
	li      r27,0
	li      r28,0
	li      r29,0
	li      r30,0
	li      r31,0

	lis		sp,__crt0stack@h		    # we take 0x8173FFF0 as the topmost starting point for our stack,this gives us ~128Kb Stack
	ori		sp,sp,__crt0stack@l
	addi	sp,sp,-4
	stw		r0,0(sp)
	stwu	sp,-56(sp)

	lis     r2,_SDA2_BASE_@h
	ori     r2,r2,_SDA2_BASE_@l  # Set the Small Data 2 (Read Only) base register.
	lis     r13,_SDA_BASE_@h
	ori     r13,r13,_SDA_BASE_@l # Set the Small Data (Read\Write) base register.
	blr

__InitHardware:
	# Enable the Floating Point Registers
	mfmsr   r3
	ori     r3,r3,MSR_FP
	mtmsr   r3

	mflr	r31
	bl		__InitPS 			# Initialize Paired Singles
	bl		__InitFPRS		 	# Initialize the FPR's
	bl		__InitCache 		# Initialize the system caches
	mtlr	r31
	blr

	//r3 = ptr, r4 = fill, r5 = size
	.globl _memset
_memset:
	clrlwi.	r6,r5,29
	srwi	r5,r5,2
	subi	r3,r3,4
	mtctr	r5
1:	stwu	r4,4(r3)
	bdnz	1b
	cmplwi	r6,0
	beq		3f
2:	stbu	r4,1(r3)
	addic.	r6,r6,-1
	bne+	2b
3:	blr

	.section .bss
	.balign 8
__crt0stack_end:
	.space 0x4000
__crt0stack:

	.globl __system_argv
	.section	.sdata,"aw",@progbits
	.align 2
	.type	__system_argv, @object
	.size	__system_argv, 4
__system_argv:
	.long	__argv
