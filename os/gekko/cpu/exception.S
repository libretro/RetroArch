/* Exception entry and exit, context switching, lazy FP.
 *
 * SPRG0 holds the frame the next exception saves into: the running
 * thread outside exception context, a fallback frame inside it (an
 * exception there is fatal).  SPRG2 holds the physical address of
 * gk_scratch for the FP-unavailable path, which saves only what it
 * touches so it can also run inside a handler. */

#include "../kernel/kernel.h"

#define SPRG0 272
#define SPRG1 273
#define SPRG2 274

/* Vector stub, copied to 0x80000000 + vec; runs untranslated. */
.macro VECSTUB vec
	mtspr	SPRG1,4
	mfspr	4,SPRG0
	clrlwi	4,4,2
	stw	3,CTX_GPR(3)(4)
	mfspr	3,SPRG1
	stw	3,CTX_GPR(4)(4)
	mfsrr0	3
	stw	3,CTX_SRR0(4)
	mfsrr1	3
	stw	3,CTX_SRR1(4)
	mfcr	3
	stw	3,CTX_CR(4)
	li	3,\vec
	lis	4,gk_exc_entry@h
	ori	4,4,gk_exc_entry@l
	mtsrr0	4
	li	4,MSR_KERNEL
	mtsrr1	4
	rfi
.endm

	.section .rodata.gk_vectors,"a"
	.balign	4
	.globl	gk_vector_stubs
gk_vector_stubs:
	.irp	vec, 0x0100,0x0200,0x0300,0x0400,0x0500,0x0600,0x0700,0x0900,0x0a00,0x0b00,0x0c00,0x0d00,0x0e00,0x0f00,0x1300,0x1400,0x1700
	.long	\vec
	.long	1f - 0f
0:	VECSTUB	\vec
1:
	.endr
	.long	0

	/* FP unavailable (0x800). */
	.globl	gk_fpu_stub
gk_fpu_stub:
	mtspr	SPRG1,4
	mfspr	4,SPRG2
	stw	3,SCR_R3(4)
	mfspr	3,SPRG1
	stw	3,SCR_R4(4)
	stw	5,SCR_R5(4)
	mfcr	3
	stw	3,SCR_CR(4)
	mfsrr0	3
	stw	3,SCR_SRR0(4)
	mfsrr1	3
	stw	3,SCR_SRR1(4)
	lis	4,gk_fp_switch@h
	ori	4,4,gk_fp_switch@l
	mtsrr0	4
	li	4,(MSR_KERNEL | 0x2000)
	mtsrr1	4
	rfi
	.globl	gk_fpu_stub_end
gk_fpu_stub_end:

.macro FP_SAVE base
	.irp	n, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
	stfd	\n,FPX_FPR(\n)(\base)
	psq_st	\n,FPX_PS(\n)(\base),0,0
	.endr
	mffs	0
	stfd	0,FPX_FPSCR(\base)
.endm

/* psq_l writes both halves, then lfd puts back the exact double. */
.macro FP_LOAD base
	lfd	0,FPX_FPSCR(\base)
	mtfsf	0xff,0
	.irp	n, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
	psq_l	\n,FPX_PS(\n)(\base),0,0
	lfd	\n,FPX_FPR(\n)(\base)
	.endr
.endm

	.text

/* Translated, kernel MSR; r3 = vector; the stub saved r3, r4, CR,
 * SRR0 and SRR1 into the SPRG0 frame. */
	.globl	gk_exc_entry
gk_exc_entry:
	mfspr	4,SPRG0
	stw	0,CTX_GPR(0)(4)
	stw	1,CTX_GPR(1)(4)
	stw	2,CTX_GPR(2)(4)
	stmw	5,CTX_GPR(5)(4)
	mflr	5
	stw	5,CTX_LR(4)
	mfctr	5
	stw	5,CTX_CTR(4)
	mfxer	5
	stw	5,CTX_XER(4)

	lis	1,gk_irq_stack_top@h
	ori	1,1,gk_irq_stack_top@l
	li	0,0
	stwu	0,-16(1)
	lis	2,_SDA2_BASE_@h
	ori	2,2,_SDA2_BASE_@l
	lis	13,_SDA_BASE_@h
	ori	13,13,_SDA_BASE_@l

	lis	5,gk_nested_frame@h
	ori	5,5,gk_nested_frame@l
	mtspr	SPRG0,5
	li	5,1
	lis	6,gk_in_exception@ha
	stw	5,gk_in_exception@l(6)

	bl	gk_exception_dispatch

	li	5,0
	lis	6,gk_in_exception@ha
	stw	5,gk_in_exception@l(6)
	/* fall through: resume the thread in r3 */

/* Load the register frame in r3 and return into it. */
	mtspr	SPRG0,3
	lwz	5,CTX_LR(3)
	mtlr	5
	lwz	5,CTX_CTR(3)
	mtctr	5
	lwz	5,CTX_XER(3)
	mtxer	5
	lwz	5,CTX_CR(3)
	mtcr	5
	lwz	5,CTX_SRR0(3)
	mtsrr0	5
	lwz	5,CTX_SRR1(3)
	mtsrr1	5
	lwz	0,CTX_GPR(0)(3)
	lwz	1,CTX_GPR(1)(3)
	lwz	2,CTX_GPR(2)(3)
	lmw	5,CTX_GPR(5)(3)
	lwz	4,CTX_GPR(4)(3)
	lwz	3,CTX_GPR(3)(3)
	rfi

/* FP unavailable, translated with FP on.  Hands the FPRs from their
 * owner to the running thread, or to the handler in exception
 * context (which leaves them ownerless). */
gk_fp_switch:
	lis	5,gk_fp_owner@ha
	lwz	3,gk_fp_owner@l(5)
	lis	4,gk_in_exception@ha
	lwz	4,gk_in_exception@l(4)
	cmpwi	4,0
	bne	1f
	lis	4,gk_cur@ha
	lwz	4,gk_cur@l(4)
	cmpw	3,4
	beq	3f			/* still ours: just turn FP back on */
1:	cmpwi	3,0
	beq	2f
	addi	3,3,THR_FP
	FP_SAVE	3
2:	lis	4,gk_in_exception@ha
	lwz	4,gk_in_exception@l(4)
	cmpwi	4,0
	bne	4f
	lis	4,gk_cur@ha
	lwz	3,gk_cur@l(4)
	stw	3,gk_fp_owner@l(5)
	addi	3,3,THR_FP
	FP_LOAD	3
	b	3f
4:	li	3,0
	stw	3,gk_fp_owner@l(5)
	lis	4,gk_fp_zero@ha
	lfd	0,gk_fp_zero@l(4)
	mtfsf	0xff,0
3:	lis	4,gk_scratch@h
	ori	4,4,gk_scratch@l
	lwz	3,SCR_SRR0(4)
	mtsrr0	3
	lwz	3,SCR_SRR1(4)
	ori	3,3,0x2000
	mtsrr1	3
	lwz	3,SCR_CR(4)
	mtcr	3
	lwz	5,SCR_R5(4)
	lwz	3,SCR_R3(4)
	lwz	4,SCR_R4(4)
	rfi

/* Thread context, interrupts off: enter the scheduler. */
	.globl	gk_syscall_resched
gk_syscall_resched:
	sc
	blr

	.section .sdata2.gk_fp_zero,"a"
	.balign	8
gk_fp_zero:
	.long	0, 0

	.section .bss.gk_irq_stack,"aw",@nobits
	.balign	32
	.space	16384
	.globl	gk_irq_stack_top
gk_irq_stack_top:
