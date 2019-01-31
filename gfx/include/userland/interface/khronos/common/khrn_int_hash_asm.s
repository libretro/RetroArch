/*
Copyright (c) 2013, Raspberry Pi Foundation
Copyright (c) 2013, RISC OS Open Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


/* Prevent the stack from becoming executable */
#if defined(__linux__) && defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif

    .text
    .arch armv6
    .object_arch armv4
    .arm
    .altmacro
    .p2align 2

.macro myfunc fname
 .func fname
 .global fname
 /* For ELF format also set function visibility to hidden */
#ifdef __ELF__
 .hidden fname
 .type fname, %function
#endif
fname:
.endm

.macro hashmix A, B, C
        sub     A, A, C
        eor     A, A, C, ror #32-4
        add     C, C, B
        sub     B, B, A
        eor     B, B, A, ror #32-6
        add     A, A, C
        sub     C, C, B
        eor     C, C, B, ror #32-8
        add     B, B, A
        sub     A, A, C
        eor     A, A, C, ror #32-16
        add     C, C, B
        sub     B, B, A
        eor     B, B, A, ror #32-19
        add     A, A, C
        sub     C, C, B
        eor     C, C, B, ror #32-4
        add     B, B, A
.endm

.macro hashfinal A, B, C
        eor     C, C, B
        sub     C, C, B, ror #32-14
        eor     A, A, C
        sub     A, A, C, ror #32-11
        eor     B, B, A
        sub     B, B, A, ror #32-25
        eor     C, C, B
        sub     C, C, B, ror #32-16
        eor     A, A, C
        sub     A, A, C, ror #32-4
        eor     B, B, A
        sub     B, B, A, ror #32-14
        eor     C, C, B
        sub     C, C, B, ror #32-24
.endm

/*
 * uint32_t khrn_hashword(const uint32_t *k, int length, uint32_t initval);
 * On entry:
 * a1 = pointer to buffer
 * a2 = number of 32-bit words
 * a3 = seed
 * On exit:
 * a1 = hash value
 */

.set prefetch_distance, 2

myfunc khrn_hashword
        S       .req    a1
        N       .req    a2
        AA      .req    a3
        BB      .req    a4
        CC      .req    v1
        DAT0    .req    v2
        DAT1    .req    ip
        DAT2    .req    lr

        ldr     BB, =0xDEADBEEF
        push    {CC, DAT0, lr}
        add     AA, AA, N, lsl #2
        add     AA, AA, BB
        mov     BB, AA
        mov     CC, AA

        /* To preload ahead as we go, we need at least (prefetch_distance+2) 32-byte blocks */
        cmp     N, #(prefetch_distance+2)*32/4
        blo     170f

        /* Long case */
        /* Adjust N to simplify inner loop termination. We want it to
         * stop when there are (prefetch_distance+1) complete cache
         * lines to go. */
        sub     N, N, #(prefetch_distance+2)*32/4
        bic     DAT0, S, #31
 .set OFFSET, 0
 .rept prefetch_distance+1
        pld     [DAT0, #OFFSET]
  .set OFFSET, OFFSET+32
 .endr
        and     DAT1, S, #0x1C
        cmp     DAT1, #0x0C
        bhs     156f
154:    /* Now at first complete triple-word within cacheline, with at
         * least one prefetch to go (but no prefetch required until we
         * have processed at least 2 triple-words) */
        ldmia   S!, {DAT0, DAT1, DAT2}
        sub     N, N, #12/4
        add     AA, AA, DAT0
        add     BB, BB, DAT1
        add     CC, CC, DAT2
        hashmix AA, BB, CC
156:    ldmia   S!, {DAT0, DAT1, DAT2}
        sub     N, N, #12/4
        add     AA, AA, DAT0
        add     BB, BB, DAT1
        add     CC, CC, DAT2
        hashmix AA, BB, CC
        tst     S, #0x10
        bne     156b
        bic     DAT0, S, #0x1F
        and     DAT1, S, #0x1F
        pld     [DAT0, #prefetch_distance*32]
        adds    DAT1, N, DAT1, lsr #2
        bpl     154b
        /* Just before the final (prefetch_distance+1) 32-byte blocks,
         * deal with final preload */
        cmp     DAT1, #-32/4
        beq     157f
        pld     [DAT0, #(prefetch_distance+1)*32]
157:    add     N, N, #(prefetch_distance+2)*32/4 - 1 - 3
158:    ldmia   S!, {DAT0, DAT1, DAT2}
        add     AA, AA, DAT0
        add     BB, BB, DAT1
        add     CC, CC, DAT2
        hashmix AA, BB, CC
        subs    N, N, #3
        bhs     158b
        cmp     N, #-2
        ldr     DAT0, [S], #4
        ldrhs   DAT1, [S], #4
        ldrhi   DAT2, [S], #4
        add     AA, AA, DAT0
        addhs   BB, BB, DAT1
        addhi   CC, CC, DAT2
        hashfinal AA, BB, CC
        mov     a1, CC
        pop     {CC, DAT0, pc}

170:    /* Short case */
        cmp     N, #1
        blo     199f
        bic     DAT0, S, #31
        pld     [DAT0]
        add     DAT1, S, N, lsl #2
        sub     DAT1, DAT1, #1
        bic     DAT1, DAT1, #31
        cmp     DAT1, DAT0
        beq     92f
91:     add     DAT0, DAT0, #32
        cmp     DAT0, DAT1
        pld     [DAT0]
        bne     91b
92:     sub     N, N, #1
        b       176f
175:    ldmia   S!, {DAT0, DAT1, DAT2}
        add     AA, AA, DAT0
        add     BB, BB, DAT1
        add     CC, CC, DAT2
        hashmix AA, BB, CC
176:    subs    N, N, #3
        bhs     175b
        cmp     N, #-2
        ldr     DAT0, [S], #4
        ldrhs   DAT1, [S], #4
        ldrhi   DAT2, [S], #4
        add     AA, AA, DAT0
        addhs   BB, BB, DAT1
        addhi   CC, CC, DAT2
        hashfinal AA, BB, CC
199:    mov     a1, CC
        pop     {CC, DAT0, pc}

        .unreq  S
        .unreq  N
        .unreq  AA
        .unreq  BB
        .unreq  CC
        .unreq  DAT0
        .unreq  DAT1
        .unreq  DAT2
.endfunc

.ltorg

