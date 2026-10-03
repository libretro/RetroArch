/* Entry point.  The loader leaves us in translated mode at _start;
 * the processor and memory map are set up from scratch in real mode,
 * then C takes over in gk_boot(). */

#include "../kernel/kernel.h"

#define HID0  1008
#define HID2  920
#define HID4  1011
#define GQR0  912

	.section .crt0,"ax"
	.globl	_start
_start:
	b	gk_start
	/* Loaders that pass arguments look for this tag and fill in the
	 * argument block after it: magic, command line, length, argc,
	 * argv, end of argv. */
	.long	0x5f617267
	.globl	gk_loader_argv
gk_loader_argv:
	.long	0,0,0,0,0,0

gk_start:
	bl	1f
1:	mflr	3
	addi	3,3,(2f - 1b)
	clrlwi	3,3,2
	mtsrr0	3
	li	4,0x1000
	mtsrr1	4
	rfi

	/* Real mode from here to the rfi below. */
2:	mfspr	3,HID0
	ori	3,3,0x00a4		/* store gathering, BTIC, branch history */
	mtspr	HID0,3
	isync
	mfspr	3,HID2
	oris	3,3,0xe000		/* quantised load/store, write gather, paired singles */
	mtspr	HID2,3
	isync
#ifdef HW_RVL
	mfspr	3,HID4
	oris	3,3,0x0200		/* BATs 4-7 */
	mtspr	HID4,3
	isync
#endif

	/* Every BAT off, then the map: 0x80000000 cached and
	 * 0xC0000000 uncached over the first 256 MiB, and on the Wii
	 * the same pair at 0x90000000 / 0xD0000000 over MEM2. */
	li	0,0
	mtspr	528,0
	mtspr	530,0
	mtspr	532,0
	mtspr	534,0
	mtspr	536,0
	mtspr	538,0
	mtspr	540,0
	mtspr	542,0
#ifdef HW_RVL
	mtspr	560,0
	mtspr	562,0
	mtspr	564,0
	mtspr	566,0
	mtspr	568,0
	mtspr	570,0
	mtspr	572,0
	mtspr	574,0
#endif
	isync

	lis	3,0x8000
	ori	3,3,0x1fff
	li	4,0x0002
	mtspr	529,4			/* IBAT0: MEM1 cached */
	mtspr	528,3
	mtspr	537,4			/* DBAT0 */
	mtspr	536,3
	lis	3,0xc000
	ori	3,3,0x1fff
	li	4,0x002a
	mtspr	539,4			/* DBAT1: uncached, guarded */
	mtspr	538,3
#ifdef HW_RVL
	lis	3,0x9000
	ori	3,3,0x1fff
	lis	4,0x1000
	ori	4,4,0x0002
	mtspr	561,4			/* IBAT4: MEM2 cached */
	mtspr	560,3
	mtspr	569,4			/* DBAT4 */
	mtspr	568,3
	lis	3,0xd000
	ori	3,3,0x1fff
	lis	4,0x1000
	ori	4,4,0x002a
	mtspr	571,4			/* DBAT5: MEM2 uncached */
	mtspr	570,3
#endif
	isync

	/* Plain floats for quantised loads unless code says otherwise. */
	mtspr	GQR0,0
	mtspr	GQR0+1,0
	mtspr	GQR0+2,0
	mtspr	GQR0+3,0
	mtspr	GQR0+4,0
	mtspr	GQR0+5,0
	mtspr	GQR0+6,0
	mtspr	GQR0+7,0

	lis	3,3f@h
	ori	3,3,3f@l
	mtsrr0	3
	li	4,MSR_KERNEL
	mtsrr1	4
	rfi

	/* Translated again. */
3:	lis	1,__gk_stack_top@h
	ori	1,1,__gk_stack_top@l
	li	0,0
	stwu	0,-16(1)
	lis	2,_SDA2_BASE_@h
	ori	2,2,_SDA2_BASE_@l
	lis	13,_SDA_BASE_@h
	ori	13,13,_SDA_BASE_@l

	/* Zero .sbss through .bss. */
	lis	3,__gk_bss_start@h
	ori	3,3,__gk_bss_start@l
	lis	4,__gk_bss_end@h
	ori	4,4,__gk_bss_end@l
	subf	4,3,4
	srwi.	4,4,2
	beq	5f
	mtctr	4
	addi	3,3,-4
4:	stwu	0,4(3)
	bdnz	4b
5:
	bl	gk_boot
6:	b	6b
