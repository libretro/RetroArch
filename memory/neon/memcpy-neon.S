/*
 * NEON code contributed by Siarhei Siamashka <siarhei.siamashka@nokia.com>.
 * Origin: http://sourceware.org/ml/libc-ports/2009-07/msg00003.html
 *
 * The GNU C Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License.
 *
 * Tweaked for Android by Jim Huang <jserv@0xlab.org>
 */

.arm
.fpu neon

@ void* memcpy(void *destination, const void *source, size_t num)
.global memcpy_neon
.type memcpy_neon, %function
/*
 * ENABLE_UNALIGNED_MEM_ACCESSES macro can be defined to permit the use
 * of unaligned load/store memory accesses supported since ARMv6. This
 * will further improve performance, but can purely theoretically cause
 * problems if somebody decides to set SCTLR.A bit in the OS kernel
 * (to trap each unaligned memory access) or somehow mess with strongly
 * ordered/device memory.
 */
#define ENABLE_UNALIGNED_MEM_ACCESSES 1

#define NEON_MAX_PREFETCH_DISTANCE 320

.align 4
memcpy_neon:
	.fnstart
		mov	ip, r0
		cmp	r2, #16
		blt     4f	@ Have less than 16 bytes to copy

		@ First ensure 16 byte alignment for the destination buffer
		tst	r0, #0xF
		beq	2f
		tst	r0, #1
		ldrneb	r3, [r1], #1
		strneb	r3, [ip], #1
		subne	r2, r2, #1
		tst	ip, #2
#ifdef ENABLE_UNALIGNED_MEM_ACCESSES
		ldrneh	r3, [r1], #2
		strneh	r3, [ip], #2
#else
		ldrneb	r3, [r1], #1
		strneb	r3, [ip], #1
		ldrneb	r3, [r1], #1
		strneb	r3, [ip], #1
#endif
		subne	r2, r2, #2

		tst	ip, #4
		beq	1f
		vld4.8	{d0[0], d1[0], d2[0], d3[0]}, [r1]!
		vst4.8	{d0[0], d1[0], d2[0], d3[0]}, [ip, :32]!
		sub	r2, r2, #4
1:
		tst	ip, #8
		beq	2f
		vld1.8	{d0}, [r1]!
		vst1.8	{d0}, [ip, :64]!
		sub	r2, r2, #8
2:
		subs	r2, r2, #32
		blt	3f
		mov	r3, #32

		@ Main copy loop, 32 bytes are processed per iteration.
		@ ARM instructions are used for doing fine-grained prefetch,
		@ increasing prefetch distance progressively up to
		@ NEON_MAX_PREFETCH_DISTANCE at runtime
1:
		vld1.8	{d0-d3}, [r1]!
		cmp	r3, #(NEON_MAX_PREFETCH_DISTANCE - 32)
		pld	[r1, r3]
		addle	r3, r3, #32
		vst1.8	{d0-d3}, [ip, :128]!
		sub	r2, r2, #32
		cmp	r2, r3
		bge	1b
		cmp	r2, #0
		blt	3f
1:		@ Copy the remaining part of the buffer (already prefetched)
		vld1.8	{d0-d3}, [r1]!
		subs	r2, r2, #32
		vst1.8	{d0-d3}, [ip, :128]!
		bge	1b
3:		@ Copy up to 31 remaining bytes
		tst	r2, #16
		beq	4f
		vld1.8	{d0, d1}, [r1]!
		vst1.8	{d0, d1}, [ip, :128]!
4:
		@ Use ARM instructions exclusively for the final trailing part
		@ not fully fitting into full 16 byte aligned block in order
		@ to avoid "ARM store after NEON store" hazard. Also NEON
		@ pipeline will be (mostly) flushed by the time when the
		@ control returns to the caller, making the use of NEON mostly
		@ transparent (and avoiding hazards in the caller code)

#ifdef ENABLE_UNALIGNED_MEM_ACCESSES
		movs	r3, r2, lsl #29
		ldrcs	r3, [r1], #4
		strcs	r3, [ip], #4
		ldrcs	r3, [r1], #4
		strcs	r3, [ip], #4
		ldrmi	r3, [r1], #4
		strmi	r3, [ip], #4
		movs	r2, r2, lsl #31
		ldrcsh	r3, [r1], #2
		strcsh	r3, [ip], #2
		ldrmib	r3, [r1], #1
		strmib	r3, [ip], #1
#else
		movs	r3, r2, lsl #29
		bcc	1f
	.rept	8
		ldrcsb	r3, [r1], #1
		strcsb	r3, [ip], #1
	.endr
1:
		bpl	1f
	.rept	4
		ldrmib	r3, [r1], #1
		strmib	r3, [ip], #1
	.endr
1:
		movs	r2, r2, lsl #31
		ldrcsb	r3, [r1], #1
		strcsb	r3, [ip], #1
		ldrcsb	r3, [r1], #1
		strcsb	r3, [ip], #1
		ldrmib	r3, [r1], #1
		strmib	r3, [ip], #1
#endif
		bx	lr
	.fnend
