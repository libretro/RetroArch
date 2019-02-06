# BCM2835 "GPU_FFT" release 2.0
#
# Copyright (c) 2014, Andrew Holme.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the copyright holder nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

.set rb_offsets_re,     rb0 # 8
.set rb_offsets_im,     rb8 # 8
.set rb_0x10,           rb16
.set rb_X_STRIDE,       rb17
.set rb_Y_STRIDE_SRC,   rb18
.set rb_Y_STRIDE_DST,   rb19
.set rb_NX,             rb20
.set rb_NY,             rb21

.set ra_x,              ra0
.set ra_y,              ra1
.set ra_src_base,       ra2
.set ra_dst_base,       ra3
.set ra_src_cell,       ra4
.set ra_dst_cell,       ra5
.set ra_vdw_stride,     ra6

    mov t0s, unif               # src->vc_msg
    ldtmu0                      # r4 = vc_unifs
    add t0s, r4, 3*4            # 3rd unif
    ldtmu0                      # r4 = src->in
    add ra_src_base, r4, unif   # optional offset

    mov t0s, unif               # dst->vc_msg
    ldtmu0                      # r4 = vc_unifs
    add t0s, r4, 3*4            # 3rd unif
    ldtmu0                      # r4 = src->in
    add ra_dst_base, r4, unif   # optional offset

    mov rb_Y_STRIDE_SRC, unif
    mov rb_Y_STRIDE_DST, unif
    mov rb_NX,           unif
    mov rb_NY,           unif

    mov rb_X_STRIDE, 2*4        # sizeof complex
    mov rb_0x10, 0x10

    mov r0, vdw_setup_1(0)
    add r0, r0, rb_Y_STRIDE_DST
    mov r1, 16*4
    sub ra_vdw_stride, r0, r1

    nop; mul24 r0, elem_num, rb_X_STRIDE
.rep i, 8
    mov rb_offsets_re+i, r0
    add rb_offsets_im+i, r0, 4
    add r0, r0, rb_Y_STRIDE_SRC
.endr

    mov ra_y, 0
:outer
    mov ra_x, 0
:inner

    nop; mul24 r1, ra_y, rb_Y_STRIDE_SRC
    nop; mul24 r0, ra_x, rb_X_STRIDE
    add r0, r0, r1
    add ra_src_cell, ra_src_base, r0

    nop; mul24 r1, ra_x, rb_Y_STRIDE_DST
    nop; mul24 r0, ra_y, rb_X_STRIDE
    add r0, r0, r1
    add ra_dst_cell, ra_dst_base, r0

    mov vw_setup, vpm_setup(16, 1, v32(0,0))

        add t0s, ra_src_cell, rb_offsets_re
        add t1s, ra_src_cell, rb_offsets_im
    .rep i, 7
        add t0s, ra_src_cell, rb_offsets_re+1+i
        add t1s, ra_src_cell, rb_offsets_im+1+i
        ldtmu0
        mov vpm, r4
        ldtmu1
        mov vpm, r4
    .endr
        ldtmu0
        mov vpm, r4
        ldtmu1
        mov vpm, r4

    mov vw_setup, vdw_setup_0(16, 16, dma_h32(0,0))
    mov vw_setup, ra_vdw_stride
    mov vw_addr, ra_dst_cell
    mov -, vw_wait

    add ra_x, ra_x, rb_0x10
    nop
    sub.setf -, ra_x, rb_NX
    brr.allnz -, r:inner
    nop
    nop
    nop

    add ra_y, ra_y, 8
    nop
    sub.setf -, ra_y, rb_NY
    brr.allnz -, r:outer
    nop
    nop
    nop

    mov interrupt, 1
    nop; nop; thrend
    nop
    nop
