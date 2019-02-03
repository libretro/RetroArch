/*-------------------------------------------------------------

decrementer_handler.S -- PPC decrementer exception support

Copyright (C) 2004
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#include <asm.h>

#define EXCEPTION_PROLOG			\
	mfspr    r0,912;				\
	stw      r0,GQR0_OFFSET(sp);    \
	mfspr    r0,913;				\
	stw      r0,GQR1_OFFSET(sp);    \
	mfspr    r0,914;				\
	stw      r0,GQR2_OFFSET(sp);    \
	mfspr    r0,915;				\
	stw      r0,GQR3_OFFSET(sp);    \
	mfspr    r0,916;				\
	stw      r0,GQR4_OFFSET(sp);    \
	mfspr    r0,917;				\
	stw      r0,GQR5_OFFSET(sp);    \
	mfspr    r0,918;				\
	stw      r0,GQR6_OFFSET(sp);    \
	mfspr    r0,919;				\
	stw      r0,GQR7_OFFSET(sp);	\
	stw      r6,GPR6_OFFSET(sp);	\
	stw      r7,GPR7_OFFSET(sp);	\
	stw      r8,GPR8_OFFSET(sp);	\
	stw      r9,GPR9_OFFSET(sp);	\
	stw      r10,GPR10_OFFSET(sp);	\
	stw      r11,GPR11_OFFSET(sp);	\
	stw      r12,GPR12_OFFSET(sp);	\
	stw      r13,GPR13_OFFSET(sp);	\
	stw      r14,GPR14_OFFSET(sp);	\
	stw      r15,GPR15_OFFSET(sp)

#define EXCEPTION_EPILOG			\
	lwz		r4,GQR0_OFFSET(sp);		\
	mtspr	912,r4;					\
	lwz		r4,GQR1_OFFSET(sp);		\
	mtspr	913,r4;					\
	lwz		r4,GQR2_OFFSET(sp);		\
	mtspr	914,r4;					\
	lwz		r4,GQR3_OFFSET(sp);		\
	mtspr	915,r4;					\
	lwz		r4,GQR4_OFFSET(sp);		\
	mtspr	916,r4;					\
	lwz		r4,GQR5_OFFSET(sp);		\
	mtspr	917,r4;					\
	lwz		r4,GQR6_OFFSET(sp);		\
	mtspr	918,r4;					\
	lwz		r4,GQR7_OFFSET(sp);		\
	mtspr	919,r4;					\
	lwz		r15,GPR15_OFFSET(sp);	\
	lwz		r14,GPR14_OFFSET(sp);	\
	lwz		r13,GPR13_OFFSET(sp);	\
	lwz		r12,GPR12_OFFSET(sp);	\
	lwz		r11,GPR11_OFFSET(sp);	\
	lwz		r10,GPR10_OFFSET(sp);	\
	lwz		r9,GPR9_OFFSET(sp);		\
	lwz		r8,GPR8_OFFSET(sp);		\
	lwz		r7,GPR7_OFFSET(sp);		\
	lwz		r6,GPR6_OFFSET(sp);		\
	lwz		r5,GPR5_OFFSET(sp)

	.extern c_decrementer_handler
	.globl	dec_exceptionhandler
dec_exceptionhandler:
	stwu	sp,-EXCEPTION_FRAME_END(sp)		//now we're able to adjust the stackpointer with it's cached address

	EXCEPTION_PROLOG

	mfmsr	r3
	ori		r3,r3,MSR_RI
	mtmsr	r3
	isync

	addi	r14,sp,0
	lis		r15,_thread_dispatch_disable_level@ha

	mfspr	r3,SPRG0
	cmpwi	r3,0
	bne		nested
	mfspr	sp,SPRG1

nested:
	addi	r3,r3,1
	lwz		r6,_thread_dispatch_disable_level@l(r15)
	mtspr	SPRG0,r3
	addi	r6,r6,1
	stw		r6,_thread_dispatch_disable_level@l(r15)

	addi	r3,r14,0x08
	bl		c_decrementer_handler

	mfspr	r4,SPRG0
	lwz		r3,_thread_dispatch_disable_level@l(r15)
	addi	r4,r4,-1
	addic.	r3,r3,-1
	mtspr	SPRG0,r4
	stw		r3,_thread_dispatch_disable_level@l(r15)
	addi	sp,r14,0
	bne		easy_exit

	lis		r4,_context_switch_want@ha
	lwz		r5,_context_switch_want@l(r4)
	cmpwi	r5,0
	beq		easy_exit

switch:
	bl		__thread_dispatch

easy_exit:
	lwz		r4,CR_OFFSET(sp)
	mtcr	r4
	lwz		r4,LR_OFFSET(sp)
	mtlr	r4
	lwz		r4,CTR_OFFSET(sp)
	mtctr	r4
	lwz		r4,XER_OFFSET(sp)
	mtxer	r4

	EXCEPTION_EPILOG

	mfmsr	r4
	rlwinm	r4,r4,0,31,29
	mtmsr	r4
	isync

	lwz		r0,GPR0_OFFSET(sp)
	lwz		toc,GPR2_OFFSET(sp)

	lwz		r4,SRR0_OFFSET(sp)
	mtsrr0	r4
	lwz		r4,SRR1_OFFSET(sp)
	rlwinm	r4, r4, 0, 19, 17
	mtsrr1	r4

	lwz		r4,GPR4_OFFSET(sp)
	lwz		r3,GPR3_OFFSET(sp)
	addi	sp,sp,EXCEPTION_FRAME_END
	rfi
