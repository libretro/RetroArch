/*-------------------------------------------------------------

cache_asm.S -- Cache interface

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

	.globl DCFlashInvalidate
DCFlashInvalidate:
	mfspr	r3,HID0
	ori		r3,r3,0x0400
	mtspr	HID0,r3
	isync
	blr

	.globl DCEnable
DCEnable:
	mfspr	r3,HID0
	ori		r3,r3,0x4000
	mtspr	HID0,r3
	isync
	blr

	.globl DCDisable
DCDisable:
	mfspr	r3,HID0
	rlwinm	r3,r3,0,18,16
	mtspr	HID0,r3
	isync
	blr

	.globl DCFreeze
DCFreeze:
	mfspr	r3,HID0
	ori		r3,r3,0x1000
	mtspr	HID0,r3
	isync
	blr

	.globl DCUnfreeze
DCUnfreeze:
	mfspr	r3,HID0
	rlwinm	r3,r3,0,20,18
	mtspr	HID0,r3
	isync
	blr

	.globl DCTouchLoad
DCTouchLoad:
	dcbt	r0,r3
	blr

	.globl DCBlockZero
DCBlockZero:
	dcbz	r0,r3
	blr

	.globl DCBlockStore
DCBlockStore:
	dcbst	r0,r3
	blr

	.globl DCBlockFlush
DCBlockFlush:
	dcbf	r0,r3
	blr

	.globl DCBlockInvalidate
DCBlockInvalidate:
	dcbi	r0,r3
	blr

	.globl DCInvalidateRange
DCInvalidateRange:
	cmplwi r4, 0   # zero or negative size?
	blelr
	clrlwi. r5, r3, 27  # check for lower bits set in address
	beq 1f
	addi r4, r4, 0x20
1:
	addi r4, r4, 0x1f
	srwi r4, r4, 5
	mtctr r4
2:
	dcbi r0, r3
	addi r3, r3, 0x20
	bdnz 2b
	blr

	.globl DCFlushRange
DCFlushRange:
	cmplwi r4, 0   # zero or negative size?
	blelr
	clrlwi. r5, r3, 27  # check for lower bits set in address
	beq 1f
	addi r4, r4, 0x20
1:
	addi r4, r4, 0x1f
	srwi r4, r4, 5
	mtctr r4
2:
	dcbf r0, r3
	addi r3, r3, 0x20
	bdnz 2b
	sc
	blr

	.globl DCStoreRange
DCStoreRange:
	cmplwi r4, 0   # zero or negative size?
	blelr
	clrlwi. r5, r3, 27  # check for lower bits set in address
	beq 1f
	addi r4, r4, 0x20
1:
	addi r4, r4, 0x1f
	srwi r4, r4, 5
	mtctr r4
2:
	dcbst r0, r3
	addi r3, r3, 0x20
	bdnz 2b
	sc
	blr

	.globl DCFlushRangeNoSync
DCFlushRangeNoSync:
	cmplwi r4, 0   # zero or negative size?
	blelr
	clrlwi. r5, r3, 27  # check for lower bits set in address
	beq 1f
	addi r4, r4, 0x20
1:
	addi r4, r4, 0x1f
	srwi r4, r4, 5
	mtctr r4
2:
	dcbf r0, r3
	addi r3, r3, 0x20
	bdnz 2b
	blr

	.globl DCStoreRangeNoSync
DCStoreRangeNoSync:
	cmplwi r4, 0   # zero or negative size?
	blelr
	clrlwi. r5, r3, 27  # check for lower bits set in address
	beq 1f
	addi r4, r4, 0x20
1:
	addi r4, r4, 0x1f
	srwi r4, r4, 5
	mtctr r4
2:
	dcbst r0, r3
	addi r3, r3, 0x20
	bdnz 2b
	blr

	.globl DCZeroRange
DCZeroRange:
	cmplwi r4, 0   # zero or negative size?
	blelr
	clrlwi. r5, r3, 27  # check for lower bits set in address
	beq 1f
	addi r4, r4, 0x20
1:
	addi r4, r4, 0x1f
	srwi r4, r4, 5
	mtctr r4
2:
	dcbz r0, r3
	addi r3, r3, 0x20
	bdnz 2b
	blr

	.globl DCTouchRange
DCTouchRange:
	cmplwi r4, 0   # zero or negative size?
	blelr
	clrlwi. r5, r3, 27  # check for lower bits set in address
	beq 1f
	addi r4, r4, 0x20
1:
	addi r4, r4, 0x1f
	srwi r4, r4, 5
	mtctr r4
2:
	dcbt r0, r3
	addi r3, r3, 0x20
	bdnz 2b
	blr

	.globl ICInvalidateRange
ICInvalidateRange:
	cmplwi r4, 0   # zero or negative size?
	blelr
	clrlwi. r5, r3, 27  # check for lower bits set in address
	beq 1f
	addi r4, r4, 0x20
1:
	addi r4, r4, 0x1f
	srwi r4, r4, 5
	mtctr r4
2:
	icbi r0, r3
	addi r3, r3, 0x20
	bdnz 2b
	sync
	isync
	blr

	.globl ICFlashInvalidate
ICFlashInvalidate:
	mfspr	r3,HID0
	ori		r3,r3,0x0800
	mtspr	HID0,r3
	isync
	blr

	.globl ICEnable
ICEnable:
	mfspr	r3,HID0
	ori		r3,r3,0x8000
	mtspr	HID0,r3
	isync
	blr

	.globl ICDisable
ICDisable:
	mfspr	r3,HID0
	rlwinm	r3,r3,0,17,15
	mtspr	HID0,r3
	isync
	blr

	.globl ICFreeze
ICFreeze:
	mfspr	r3,HID0
	ori		r3,r3,0x2000
	mtspr	HID0,r3
	isync
	blr

	.globl ICUnfreeze
ICUnfreeze:
	mfspr	r3,HID0
	rlwinm	r3,r3,0,19,17
	mtspr	HID0,r3
	isync
	blr

	.globl ICBlockInvalidate
ICBlockInvalidate:
	icbi	r0,r3
	blr

	.globl ICSync
ICSync:
	isync
	blr

	.globl L2Init
L2Init:
	mflr	r0
	stw		r0,4(sp)
	stwu	sp,-16(sp)
	stw		r31,12(sp)
	mfmsr	r3
	mr		r31,r3
	sync
	li		r3,48
	mtmsr	r3
	sync
	bl		L2Disable
	bl		L2GlobalInvalidate
	mr		r3,r31
	mtmsr	r3
	lwz		r0,20(sp)
	lwz		r31,12(sp)
	mtlr	r0
	blr

	.globl L2Enable
L2Enable:
	sync
	mfspr	r3,L2CR;
	oris	r0,r3,0x8000
	rlwinm	r3,r0,0,11,9
	mtspr	L2CR,r3
	sync
	blr

	.globl L2Disable
L2Disable:
	sync
	mfspr	r3,L2CR
	clrlwi	r3,r3,1
	mtspr	L2CR,r3
	sync
	blr

	.globl L2GlobalInvalidate
L2GlobalInvalidate:
	mflr	r0
	stw		r0,4(sp)
	stwu	sp,-8(sp)
	bl		L2Disable
	mfspr	r3,L2CR
	oris	r3,r3,0x0020
	mtspr	L2CR,r3
1:	mfspr	r3,L2CR
	clrlwi	r0,r3,31
	cmplwi	r0,0x0000
	bne		1b
	mfspr	r3,L2CR
	rlwinm	r3,r3,0,11,9
	mtspr	L2CR,r3
2:	mfspr	r3,L2CR
	clrlwi	r0,r3,31
	cmplwi	r0,0x0000
	bne		2b
	lwz		r0,12(sp)
	addi	sp,sp,8
	mtlr	r0
	blr

	.globl __LCEnable
__LCEnable:
	mfmsr	r5
	ori		r5,r5,0x1000
	mtmsr	r5
	lis		r3,0x8000
	li		r4,1024
	mtctr	r4
1:	dcbt	r0,r3
	dcbst	r0,r3
	addi    r3,r3,32
	bdnz	1b
	mfspr	r4,HID2
	oris	r4,r4,0x100f
	mtspr	HID2,r4
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	lis		r3,0xe000
	ori		r3,r3,0x0002
	mtspr	DBAT3L,r3
	ori		r3,r3,0x01fe
	mtspr	DBAT3U,r3
	isync
	lis		r3,0xe000
	li		r6,512
	mtctr	r6
	li		r6,0
2:	dcbz_l	r6,r3
	addi	r3,r3,32
	bdnz	2b
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	blr

	.globl LCDisable
LCDisable:
	lis		r3,0xe000
	li		r4,512
	mtctr	r4
1:	dcbi	r0,r3
	addi	r3,r3,32
	bdnz	1b
	mfspr	r4,HID2
	rlwinm	r4,r4,0,4,2
	mtspr	HID2,r4
	blr

	.globl LCAllocOneTag
LCAllocOneTag:
	cmpwi	r3,0
	beq		1f
	dcbi	r0,r4
1:	dcbz_l	r0,r4
	blr

	.globl LCAllocTags
LCAllocTags:
	mflr	r6
	cmplwi	r5,0
	ble		2f
	mtctr	r5
	cmpwi	r3,0
	beq		3f
1:	dcbi	r0,r4
	dcbz_l	r0,r4
	addi	r4,r4,32
	bdnz	1b
	b		2f
3:	dcbz_l	r0,r4
	addi	r4,r4,32
	bdnz	3b
2:	mtlr	r6
	blr

	.globl LCLoadBlocks
LCLoadBlocks:
	extrwi		r6,r5,5,25
	clrlwi		r4,r4,4
	or			r6,r6,r4
	mtspr		DMAU,r6
	clrlslwi	r6,r5,30,2
	or			r6,r6,r3
	ori			r6,r6,0x0012
	mtspr		DMAL,r6
	blr

	.globl LCStoreBlocks
LCStoreBlocks:
	extrwi		r6,r5,5,25
	clrlwi		r4,r4,4
	or			r6,r6,r3
	mtspr		DMAU,r6
	clrlslwi	r6,r5,30,2
	or			r6,r6,r4
	ori			r6,r6,0x0002
	mtspr		DMAL,r6
	blr
