/* The last code this program runs: copied above everything the next
 * program loads, it loads it and jumps in.  Position independent, no
 * stack, no data of its own; interrupts are off.
 * r3 = struct gk_exec_plan: entry, count, then count segments of
 * dst, src, len, size (len bytes copied, zeros up to size). */

	.text
	.globl	gk_exec_stub
	.globl	gk_exec_stub_end
gk_exec_stub:
	lwz	30,0(3)			/* entry */
	lwz	29,4(3)			/* segments left */
	addi	28,3,8
1:	cmpwi	29,0
	beq	9f
	lwz	4,0(28)			/* dst */
	lwz	5,4(28)			/* src */
	lwz	6,8(28)			/* len */
	lwz	7,12(28)		/* size */
	li	8,0
2:	cmplw	8,6
	bge	3f
	lbzx	9,5,8
	stbx	9,4,8
	addi	8,8,1
	b	2b
3:	li	9,0
4:	cmplw	8,7
	bge	5f
	stbx	9,4,8
	addi	8,8,1
	b	4b
	/* To memory, and out of the instruction cache. */
5:	clrrwi	9,4,5
	add	10,4,7
6:	cmplw	9,10
	bge	7f
	dcbst	0,9
	sync
	icbi	0,9
	addi	9,9,32
	b	6b
7:	sync
	isync
	addi	28,28,16
	addi	29,29,-1
	b	1b
9:	mtctr	30
	bctr
gk_exec_stub_end:

/* Calls r3 on the stack whose top is r4; never returns. */
	.globl	gk_exec_switch
gk_exec_switch:
	clrrwi	1,4,4
	li	0,0
	stwu	0,-16(1)
	mtctr	3
	bctrl
1:	b	1b
