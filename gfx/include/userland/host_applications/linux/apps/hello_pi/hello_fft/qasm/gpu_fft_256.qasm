# BCM2835 "GPU_FFT" release 3.0
#
# Copyright (c) 2015, Andrew Holme.
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

.set STAGES, 8

.include "gpu_fft.qinc"

##############################################################################
# Twiddles: src

.set TW16_P1_BASE,  0   # rx_tw_shared
.set TW16_P2_STEP,  1

.set TW16_P2_BASE,  0   # rx_tw_unique

##############################################################################
# Twiddles: dst

.set TW16_STEP, 0  # 1
.set TW16,      1  # 5

##############################################################################
# Registers

.set ra_link_1,         ra0
#                       rb0
.set ra_save_ptr,       ra1
#                       rb1
.set ra_temp,           ra2
#                       rb2
.set ra_addr_x,         ra3
.set rb_addr_y,         rb3
.set ra_save_16,        ra4
.set rx_save_slave_16,  rb4
.set ra_load_idx,       ra5
.set rb_inst,           rb5
.set ra_sync,           ra6
.set rx_sync_slave,     rb6
.set ra_points,         ra7
#                       rb7

.set rx_tw_shared,      ra8
.set rx_tw_unique,      rb8

.set ra_tw_re,          ra9 # 6
.set rb_tw_im,          rb9 # 6

.set ra_vpm,            ra27
.set rb_vpm,            rb27
.set ra_vdw,            ra28
.set rb_vdw,            rb28

.set rx_0x5555,         ra29
.set rx_0x3333,         ra30
.set rx_0x0F0F,         ra31

.set rb_0x40,           rb30
.set rb_0x80,           rb31

##############################################################################
# Register alias

.set ra_link_0, ra_save_16

##############################################################################
# Constants

mov rb_0x40,    0x40
mov rb_0x80,    0x80

mov rx_0x5555,  0x5555
mov rx_0x3333,  0x3333
mov rx_0x0F0F,  0x0F0F

mov ra_vdw, vdw_setup_0(16, 16, dma_h32( 0,0))
mov rb_vdw, vdw_setup_0(16, 16, dma_h32(16,0))

##############################################################################
# Twiddles: ptr

mov rx_tw_shared, unif
mov rx_tw_unique, unif

##############################################################################
# Instance

mov rb_inst, unif
inst_vpm rb_inst, ra_vpm, rb_vpm, -, -

##############################################################################
# Master/slave procedures

proc ra_save_16, r:1f
body_ra_save_16 ra_vpm, ra_vdw
:1

proc rx_save_slave_16, r:1f
body_rx_save_slave_16 ra_vpm
:1

proc ra_sync, r:1f
body_ra_sync
:1

proc rx_sync_slave, r:main
body_rx_sync_slave

##############################################################################
# Redefining this macro

.macro read_rev, stride
    add ra_load_idx, ra_load_idx, stride; mov r0, ra_load_idx

    bit_rev 1, rx_0x5555    # 16 SIMD
    bit_rev 2, rx_0x3333
    bit_rev 4, rx_0x0F0F

    shl r0, r0, 3           # {idx[0:7], 1'b0, 2'b0}
    add r1, r0, 4           # {idx[0:7], 1'b1, 2'b0}

    add t0s, ra_addr_x, r0
    add t0s, ra_addr_x, r1
.endm

##############################################################################
# Subroutines

:fft_16
    body_fft_16

:pass_1
:pass_2
    brr -, r:fft_16
    nop;        ldtmu0
    mov r0, r4; ldtmu0
    mov r1, r4

##############################################################################
# Top level

:main
    mov.setf r0, rb_inst
    sub r0, r0, 1
    shl r0, r0, 5
    add.ifnz ra_sync, rx_sync_slave, r0
    mov.ifnz ra_save_16, rx_save_slave_16

:loop
    mov.setf ra_addr_x, unif # Ping buffer or null
    mov      rb_addr_y, unif # Pong buffer or IRQ enable

    brr.allz -, r:end

##############################################################################
# Pass 1

    load_tw rx_tw_shared, TW16+3, TW16_P1_BASE
    init_stage 4
    read_rev rb_0x80
    read_rev rb_0x80

.rep i, 2
    brr ra_link_1, r:pass_1
    nop
    mov ra_vpm, rb_vpm; mov rb_vpm, ra_vpm
    mov ra_vdw, rb_vdw; mov rb_vdw, ra_vdw
.endr

    bra ra_link_1, ra_sync
    nop
    nop
    nop

##############################################################################
# Pass 2

    swap_buffers
    load_tw rx_tw_unique, TW16+3, TW16_P2_BASE
    load_tw rx_tw_shared, TW16_STEP, TW16_P2_STEP
    init_stage 4
    read_lin rb_0x80
    read_lin rb_0x80

    brr ra_link_1, r:pass_2
    nop
    mov ra_vpm, rb_vpm; mov rb_vpm, ra_vpm
    mov ra_vdw, rb_vdw; mov rb_vdw, ra_vdw

    next_twiddles_16

    brr ra_link_1, r:pass_2
    nop
    mov ra_vpm, rb_vpm; mov rb_vpm, ra_vpm
    mov ra_vdw, rb_vdw; mov rb_vdw, ra_vdw

    bra ra_link_1, ra_sync
    nop
    nop
    nop

##############################################################################

    brr -, r:loop
    nop
    nop
    nop

:end
    exit rb_addr_y
