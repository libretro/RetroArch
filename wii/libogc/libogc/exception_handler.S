/*-------------------------------------------------------------

exception_handler.S -- PPC exception handling support

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
	stw      r15,GPR15_OFFSET(sp);

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

	.globl exceptionhandler_start,exceptionhandler_end,exceptionhandler_patch
exceptionhandler_start:
	mtspr		SPRG3,r4
	clrlwi		r4,sp,2			//make sp physical and move new value to r4
	stwu		r4,-EXCEPTION_FRAME_END(r4)
	stw			r0,GPR0_OFFSET(r4)
	stw			sp,GPR1_OFFSET(r4)
	stw			toc,GPR2_OFFSET(r4)
	stw			r3,GPR3_OFFSET(r4)
	mfspr		r3,SPRG3
	stw			r3,GPR4_OFFSET(r4)
	stw			r5,GPR5_OFFSET(r4)
	mfcr		r3
	stw			r3,CR_OFFSET(r4)
	mflr		r3
	stw			r3,LR_OFFSET(r4)
	mfctr		r3
	stw			r3,CTR_OFFSET(r4)
	mfxer		r3
	stw			r3,XER_OFFSET(r4)
	mfmsr		r3
	stw			r3,MSR_OFFSET(r4)
	mfdar		r3
	stw			r3,DAR_OFFSET(r4)
	mfsrr0		r3
	stw			r3,SRR0_OFFSET(r4)
	mfsrr1		r3
	stw			r3,SRR1_OFFSET(r4)
	mr			r5,r3
	nop
	mfmsr		r3
	ori			r3,r3,MSR_IR|MSR_DR
	mtsrr1		r3

exceptionhandler_patch:
	li			r3,0
	stw			r3,EXCEPTION_NUMBER(r4)

	rlwinm.		r5,r5,0,30,30
	lis			r5,default_exceptionhandler@h
	ori			r5,r5,default_exceptionhandler@l
	beq			1f
	lis			r5,_exceptionhandlertable@h
	ori			r5,r5,_exceptionhandlertable@l
	clrlwi		r5,r5,2
	clrlslwi	r3,r3,24,2
	lwzx		r5,r3,r5
1:
	mtsrr0		r5
	rfi
exceptionhandler_end:
	nop

	.extern		c_default_exceptionhandler
	.globl		default_exceptionhandler
default_exceptionhandler:
	stwu		sp,-EXCEPTION_FRAME_END(sp)		//now we're able to adjust the stackpointer with it's cached address

	EXCEPTION_PROLOG

	stmw		r16,GPR16_OFFSET(sp)

	addi		r3,sp,0x08
	bl			c_default_exceptionhandler

	lwz			r4,CR_OFFSET(sp)
	mtcr		r4
	lwz			r4,LR_OFFSET(sp)
	mtlr		r4
	lwz			r4,CTR_OFFSET(sp)
	mtctr		r4
	lwz			r4,XER_OFFSET(sp)
	mtxer		r4

	EXCEPTION_EPILOG

	lmw			r16,GPR16_OFFSET(sp)

	lwz			toc,GPR2_OFFSET(sp)
	lwz			r0,GPR0_OFFSET(sp)

	lwz			r4,SRR0_OFFSET(sp)
	mtsrr0		r4
	lwz			r4,SRR1_OFFSET(sp)
	mtsrr1		r4

	lwz			r4,GPR4_OFFSET(sp)
	lwz			r3,GPR3_OFFSET(sp)
	addi		sp,sp,EXCEPTION_FRAME_END
	rfi

	.extern _cpu_context_save_fp,_cpu_context_restore_fp
	.globl fpu_exceptionhandler
fpu_exceptionhandler:
	stwu		sp,-EXCEPTION_FRAME_END(sp)		//now we're able to adjust the stackpointer with it's cached address

	EXCEPTION_PROLOG

	mfmsr		r4
	ori			r4,r4,MSR_FP
	mtmsr		r4
	isync

	bl			__thread_dispatch_fp

	lwz			r4,CR_OFFSET(sp)
	mtcr		r4
	lwz			r4,LR_OFFSET(sp)
	mtlr		r4
	lwz			r4,CTR_OFFSET(sp)
	mtctr		r4
	lwz			r4,XER_OFFSET(sp)
	mtxer		r4

	EXCEPTION_EPILOG

	mfmsr		r4
	rlwinm		r4,r4,0,19,17
	mtmsr		r4
	isync

	lwz			toc,GPR2_OFFSET(sp)
	lwz			r0,GPR0_OFFSET(sp)

	lwz			r4,SRR0_OFFSET(sp)
	mtsrr0		r4
	lwz			r4,SRR1_OFFSET(sp)
	ori			r4,r4,MSR_FP
	mtsrr1		r4

	lwz			r4,GPR4_OFFSET(sp)
	lwz			r3,GPR3_OFFSET(sp)
	addi		sp,sp,EXCEPTION_FRAME_END
	rfi

	.global systemcallhandler_start,systemcallhandler_end
systemcallhandler_start:
	mtspr	SPRG2,r9
	mtspr	SPRG3,r10
	mfspr	r9,HID0
	ori		r10,r9,0x0008
	mtspr	HID0,r10
	isync
	sync
	mtspr	HID0,r9
	mfspr	r9,SPRG2
	mfspr	r10,SPRG3
	rfi
systemcallhandler_end:
	nop
