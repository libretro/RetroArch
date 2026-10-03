/* Wii: the L2 cache's fetch and castout enhancements (HID4: 64-byte
 * fetches, two castout buffers, miss under miss, castouts finished
 * before a flash invalidate), set the way the 750CL manual says the
 * L2 is configured: everything written back, the L2 disabled and
 * invalidated, HID4 set, the L2 enabled again with the instruction
 * cache off.
 * r3 = end of MEM1, r4 = end of MEM2, both cached addresses written
 * back from their starts.  Interrupts are off on the way. */

#define HID0  1008
#define HID4  1011
#define L2CR  1017

#define HID4_H4A      0x8000   /* upper halves */
#define HID4_ENHANCE  0x2430   /* L2FM 64 bytes, BCO, L2MUM, L2_CCFI */
#define L2CR_L2E      0x8000
#define L2CR_CE       0x4000
#define L2CR_L2I      0x0020
#define L2CR_L2IP     0x0001   /* lower half */

/* dcbf from r7 up to the register given, a multiple of 32 above. */
.macro FLUSH last
	subf	0,7,\last
	srwi.	0,0,5
	beq	2f
	mtctr	0
1:	dcbf	0,7
	addi	7,7,32
	bdnz	1b
2:
.endm

	.text
	.globl	gk_l2_enhance
gk_l2_enhance:
	mfspr	5,HID4
	andis.	0,5,HID4_H4A
	beqlr
	lis	6,HID4_ENHANCE
	and	0,5,6
	cmplw	0,6
	beqlr

	mfmsr	8
	rlwinm	0,8,0,17,15		/* EE off */
	mtmsr	0
	isync

	/* Everything modified to memory, from L1 and L2 alike. */
	lis	7,0x8000
	FLUSH	3
	lis	7,0x9000
	FLUSH	4
	sync

	/* Off, then invalidated: CE clear, L2I set, until L2IP clears. */
	mfspr	9,L2CR
	rlwinm	10,9,0,2,31
	sync
	mtspr	L2CR,10
	sync
	oris	11,10,L2CR_L2I
	mtspr	L2CR,11
	sync
3:	mfspr	11,L2CR
	andi.	11,11,L2CR_L2IP
	bne	3b
	mtspr	L2CR,10
	sync

	or	5,5,6
	mtspr	HID4,5
	isync

	/* Back on as it was, with the instruction cache off meanwhile. */
	mfspr	11,HID0
	rlwinm	12,11,0,17,15
	mtspr	HID0,12
	isync
	mtspr	L2CR,9
	sync
	mtspr	HID0,11
	isync

	mtmsr	8
	isync
	blr
