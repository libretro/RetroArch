/* Cache maintenance by address range: r3 = start, r4 = length. */

.macro RANGE op, tail
	cmplwi	4,0
	beqlr
	clrlwi	5,3,27
	add	4,4,5
	addi	4,4,31
	srwi	4,4,5
	mtctr	4
	clrrwi	3,3,5
1:	\op	0,3
	addi	3,3,32
	bdnz	1b
	\tail
	blr
.endm

	.text
	.globl	gk_dcache_flush
gk_dcache_flush:
	RANGE	dcbf, sync

	.globl	gk_dcache_store
gk_dcache_store:
	RANGE	dcbst, sync

	.globl	gk_dcache_invalidate
gk_dcache_invalidate:
	RANGE	dcbi, sync

	.globl	gk_icache_invalidate
gk_icache_invalidate:
	RANGE	icbi, "sync; isync"
