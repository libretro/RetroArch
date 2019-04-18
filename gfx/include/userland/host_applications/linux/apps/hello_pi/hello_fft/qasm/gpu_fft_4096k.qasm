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

.set STAGES, 22

.include "gpu_fft_2048k.qinc"

##############################################################################
# Twiddles: src

.set TW64_BASE0,    0   # rx_tw_shared
.set TW64_BASE1,    1
.set TW32_BASE,     2
.set TW16_BASE,     3

.set TW48_P2_STEP,  4
.set TW64_P2_STEP,  5

.set TW32_P2_STEP,  6
.set TW16_P2_STEP,  7
.set TW32_P3_STEP,  8
.set TW16_P3_STEP,  9
.set TW32_P4_STEP, 10
.set TW16_P4_STEP, 11

.set TW32_P4_BASE,  0   # rx_tw_unique
.set TW16_P4_BASE,  1

##############################################################################
# Twiddles: dst

.set TW16_STEP, 0  # 1
.set TW32_STEP, 1  # 1
.set TW16,      2  # 5
.set TW32,      7  # 2
.set TW48,      9  # 2
.set TW64,      11 # 2
.set TW48_STEP, 13 # 1
.set TW64_STEP, 14 # 1

##############################################################################
# Registers

.set ra_link_0,         ra0
.set rb_vpm,            rb0
.set ra_save_ptr,       ra1
.set rb_vpm_16,         rb1
.set ra_temp,           ra2
.set rb_vpm_32,         rb2
.set ra_addr_x,         ra3
.set rb_addr_y,         rb3
.set ra_save_32,        ra4
.set rx_save_slave_32,  rb4
.set ra_load_idx,       ra5
.set rb_inst,           rb5
.set ra_sync,           ra6
.set rx_sync_slave,     rb6
.set ra_points,         ra7
.set rb_vpm_48,         rb7
.set ra_link_1,         ra8
.set rb_0x10,           rb8
.set ra_32_re,          ra9
.set rb_32_im,          rb9
.set ra_save_64,        ra10
.set rx_save_slave_64,  rb10

.set ra_64,             ra11 # 4
.set rb_64,             rb11 # 4

.set rx_tw_shared,      ra15
.set rx_tw_unique,      rb15

.set ra_tw_re,          ra16 # 15
.set rb_tw_im,          rb16 # 15

##############################################################################
# Dual-use registers

.set rb_STAGES,         rb_64+0
.set rb_0xF0,           rb_64+1
.set rb_0x40,           rb_64+2

.set ra_vpm_lo,         ra_64+0
.set ra_vpm_hi,         ra_64+1
.set rb_vpm_lo,         rb_vpm_32
.set rb_vpm_hi,         rb_vpm_48
.set ra_vdw_32,         ra_64+3
.set rb_vdw_32,         rb_64+3

##############################################################################
# Constants

mov rb_0x10,    0x10
mov r5rep,      0x1D0

##############################################################################
# Twiddles: ptr

mov rx_tw_shared, unif
mov rx_tw_unique, unif

##############################################################################
# Instance

mov rb_inst, unif
inst_vpm rb_inst, rb_vpm, rb_vpm_16, rb_vpm_32, rb_vpm_48

##############################################################################
# Master/slave procedures

proc ra_save_32, r:1f
body_ra_save_32
:1

proc rx_save_slave_32, r:1f
body_rx_save_slave_32
:1

proc ra_save_64, r:1f
body_ra_save_64
:1

proc rx_save_slave_64, r:1f
body_rx_save_slave_64
:1

proc ra_sync, r:1f
body_ra_sync
:1

proc rx_sync_slave, r:main
body_rx_sync_slave

##############################################################################
# Subroutines

:fft_16
    body_fft_16

:pass_1
    body_pass_64 LOAD_REVERSED, r5
:pass_2
    body_pass_64 LOAD_STRAIGHT, r5
:pass_3
:pass_4
    body_pass_32 LOAD_STRAIGHT

##############################################################################
# Top level

:main
    mov.setf r0, rb_inst
    sub r0, r0, 1
    shl r0, r0, 5
    add.ifnz ra_sync, rx_sync_slave, r0
    mov.ifnz ra_save_32, rx_save_slave_32
    mov.ifnz ra_save_64, rx_save_slave_64

:loop
    mov.setf ra_addr_x, unif # Ping buffer or null
    mov      rb_addr_y, unif # Pong buffer or IRQ enable

    brr.allz -, r:end

##############################################################################
# Pass 1

    load_tw rx_tw_shared, TW16+3, TW16_BASE
    load_tw rx_tw_shared, TW32+0, TW32_BASE
    load_tw rx_tw_shared, TW48+0, TW64_BASE0
    load_tw rx_tw_shared, TW64+0, TW64_BASE1
    init_stage 6
    read_rev rb_0x10

        brr ra_link_1, r:pass_1
        nop
        mov r0, 0x200
        add ra_points, ra_points, r0

        mov r1, STAGES
        shr.setf -, ra_points, r1

        brr.allz -, r:pass_1
        nop
        mov r0, 0x200
        add ra_points, ra_points, r0

    bra ra_link_1, ra_sync
    nop
    ldtmu0
    ldtmu0

##############################################################################
# Pass 2

    swap_buffers
    load_tw rx_tw_shared, TW16+3, TW16_BASE
    load_tw rx_tw_shared, TW32+0, TW32_BASE
    load_tw rx_tw_shared, TW48+0, TW64_BASE0
    load_tw rx_tw_shared, TW64+0, TW64_BASE1
    mov ra_tw_re+TW48+1, 0; mov rb_tw_im+TW48+1, 0
    mov ra_tw_re+TW64+1, 0; mov rb_tw_im+TW64+1, 0
    load_tw rx_tw_shared, TW16_STEP, TW16_P2_STEP
    load_tw rx_tw_shared, TW32_STEP, TW32_P2_STEP
    load_tw rx_tw_shared, TW48_STEP, TW48_P2_STEP
    load_tw rx_tw_shared, TW64_STEP, TW64_P2_STEP
    init_stage 6
    read_lin rb_0x10

        brr ra_link_1, r:pass_2
        nop
        mov r0, 0x200
        add ra_points, ra_points, r0

        mov r0, 0xFFFF
        and.setf -, ra_points, r0

        brr.allnz -, r:pass_2
        nop
        mov r0, 0x200
        add.ifnz ra_points, ra_points, r0

        rotate TW64, TW64_STEP
        rotate TW48, TW48_STEP
        next_twiddles_32
        next_twiddles_16

        mov r1, STAGES
        shr.setf -, ra_points, r1

        brr.allz -, r:pass_2
        nop
        mov r0, 0x200
        add ra_points, ra_points, r0

    bra ra_link_1, ra_sync
    nop
    ldtmu0
    ldtmu0

##############################################################################
# Dual-use registers

    mov ra_vpm_lo, rb_vpm
    mov ra_vpm_hi, rb_vpm_16

    mov ra_vdw_32, vdw_setup_0(1, 16, dma_h32( 0,0))
    mov rb_vdw_32, vdw_setup_0(1, 16, dma_h32(32,0))

    mov rb_STAGES, STAGES
    mov rb_0xF0, 0xF0
    mov rb_0x40, 0x40

##############################################################################
# Pass 3

    swap_buffers
    load_tw rx_tw_shared, TW16+3, TW16_BASE
    load_tw rx_tw_shared, TW32+0, TW32_BASE
    load_tw rx_tw_shared, TW16_STEP, TW16_P3_STEP
    load_tw rx_tw_shared, TW32_STEP, TW32_P3_STEP
    init_stage 5
    read_lin rb_0x10

        brr ra_link_1, r:pass_3
        nop
        mov r0, 0x100
        add ra_points, ra_points, r0

        mov r0, 0x3FF
        and.setf -, ra_points, r0

        brr.allnz -, r:pass_3
        nop
        mov r0, 0x100
        add.ifnz ra_points, ra_points, r0

        next_twiddles_32
        next_twiddles_16

        shr.setf -, ra_points, rb_STAGES

        brr.allz -, r:pass_3
        nop
        mov r0, 0x100
        add ra_points, ra_points, r0

    bra ra_link_1, ra_sync
    nop
    ldtmu0
    ldtmu0

##############################################################################
# Pass 4

    swap_buffers
    load_tw rx_tw_unique, TW16+3, TW16_P4_BASE
    load_tw rx_tw_unique, TW32+0, TW32_P4_BASE
    load_tw rx_tw_shared, TW16_STEP, TW16_P4_STEP
    load_tw rx_tw_shared, TW32_STEP, TW32_P4_STEP
    init_stage 5
    read_lin rb_0x10

        brr ra_link_1, r:pass_4
        nop
        mov r0, 0x100
        add ra_points, ra_points, r0

        next_twiddles_32
        next_twiddles_16

        shr.setf -, ra_points, rb_STAGES

        brr.allz -, r:pass_4
        nop
        mov r0, 0x100
        add ra_points, ra_points, r0

    bra ra_link_1, ra_sync
    nop
    ldtmu0
    ldtmu0

##############################################################################

    brr -, r:loop
    nop
    nop
    nop

:end
    exit rb_addr_y
