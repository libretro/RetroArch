/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2016 - Ali Bouhlel
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GX2_SHADER_INL_H
#define GX2_SHADER_INL_H

#ifdef MSB_FIRST
#define to_QWORD(w0, w1) (((u64)(w0) << 32ull) | (w1))
#define to_LE(x) (__builtin_bswap32(x))
#else
#define to_QWORD(w0, w1) (((u64)(w1) << 32ull) | (w0))
#define to_LE(x) (x)
#endif

/* CF */
#define CF_DWORD0(addr) to_LE(addr)

#define CF_DWORD1(popCount, cfConst, cond, count, callCount, inst) \
   to_LE(popCount | (cfConst << 3) | (cond << 8) | (count << 10) | (callCount << 13) | (inst << 23) | (1 << 31))

#define CF_ALU_WORD0(addr, kcacheBank0, kcacheBank1, kcacheMode0) \
   to_LE(addr | (kcacheBank0 << 22) | (kcacheBank1 << 26) | (kcacheMode0 << 30))
#define CF_ALU_WORD1(kcacheMode1, kcacheAddr0, kcacheAddr1, count, altConst, inst) \
   to_LE(kcacheMode1 | (kcacheAddr0 << 2) | (kcacheAddr1 << 10) | (count << 18) | (altConst << 25) | (inst << 26) | (1 << 31))

#define CF_EXP_WORD0(dstReg_and_type, srcReg, srcRel, indexGpr, elemSize)\
   to_LE(dstReg_and_type | (srcReg << 15) | (srcRel << 22) | (indexGpr << 23) | (elemSize << 30))

#define CF_EXP_WORD1(srcSelX, srcSelY, srcSelZ, srcSelW, validPixelMode, inst) \
   to_LE(srcSelX | (srcSelY << 3) | (srcSelZ << 6) | (srcSelW << 9) | (validPixelMode << 22) | (inst << 23) | (1 << 31))

#define CF_ALLOC_EXPORT_WORD0(arrayBase, type, dstReg, dstRel, indexGpr, elemSize) \
   to_LE(arrayBase | (type << 13) | (dstReg << 15) | (dstRel << 22) | (indexGpr << 23) | (elemSize << 30))

#define CF_ALLOC_EXPORT_WORD1_BUF(arraySize, writeMask, inst) \
   to_LE(arraySize | (writeMask << 12) | (inst << 23) | (1 << 31))

#define ALU_SRC_KCACHE0_BASE  0x80
#define ALU_SRC_KCACHE1_BASE  0xA0
#define CF_KCACHE_BANK_LOCK_1 0x1
#define CB1                   0x1
#define CB2                   0x2
#define _0_15                 CF_KCACHE_BANK_LOCK_1

#define KC0(x) (x + ALU_SRC_KCACHE0_BASE)
#define KC1(x) (x + ALU_SRC_KCACHE1_BASE)

#define NO_BARRIER      & ~to_QWORD(0,to_LE(1 << 31))
#define END_OF_PROGRAM  | to_QWORD(0,to_LE(1 << 21))
#define VALID_PIX       | to_QWORD(0,to_LE(1 << 22))
#define WHOLE_QUAD_MODE | to_QWORD(0,to_LE(1 << 30))
#define BURSTCNT(x)     | to_QWORD(0,to_LE(x << 17))
#define WRITE(x)        (x >> 2)
#define ARRAY_SIZE(x)   x
#define ELEM_SIZE(x)    x
#define KCACHE0(bank, mode) | to_QWORD(CF_ALU_WORD0(0, bank, 0, mode), 0)
#define KCACHE1(bank, mode) | to_QWORD(CF_ALU_WORD0(0, 0, bank, 0), CF_ALU_WORD1(mode,0, 0, 0, 0, 0))

#define DEACTIVATE               1
#define UPDATE_EXEC_MASK(mode)   | to_QWORD(0, to_LE(mode << 2))
#define UPDATE_PRED              | to_QWORD(0, to_LE(1ull << 3))
#define CLAMP                    | to_QWORD(0, to_LE(1ull << 31))
#define ALU_LAST                 | to_QWORD(to_LE(1ull << 31), 0)

/* ALU */

#define ALU_WORD0(src0Sel, src0Rel, src0Chan, src0Neg, src1Sel, src1Rel, src1Chan, src1Neg, indexMode, predSel) \
   to_LE(src0Sel | ((src0Rel) << 9) | ((src0Chan) << 10) | ((src0Neg) << 12) | ((src1Sel) << 13) | ((src1Rel) << 22) \
                 | ((src1Chan) << 23) | ((src1Neg) << 25) | ((indexMode) << 26) | ((predSel) << 29))

#define ALU_WORD1_OP2(src0Abs, src1Abs, updateExecuteMask, updatePred, writeMask, omod, inst, encoding, bankSwizzle, dstGpr, dstRel, dstChan, clamp) \
      to_LE(src0Abs | (src1Abs << 1) | (updateExecuteMask << 2) | (updatePred << 3) | (writeMask << 4) | (omod << 5) | (inst << 7) | \
                         (encoding << 15) | (bankSwizzle << 18) | ((dstGpr&0x7F) << 21) | (dstRel << 28) | ((dstChan&0x3) << 29) | (clamp << 31))

#define ALU_WORD1_OP3(src2Sel, src2Rel, src2Chan, src2Neg, inst, bankSwizzle, dstGpr, dstRel, dstChan, clamp) \
      to_LE(src2Sel | (src2Rel << 9) | (src2Chan << 10) | (src2Neg << 12) | (inst << 13) | \
     (bankSwizzle << 18) | ((dstGpr&0x7F) << 21) | (dstRel << 28) | ((dstChan&0x3) << 29) | (clamp << 31))

/* TEX */
#define TEX_WORD0(inst, bcFracMode, fetchWholeQuad, resourceID, srcReg, srcRel, altConst) \
   to_LE(inst | (bcFracMode << 5) | (fetchWholeQuad << 7) | (resourceID << 8) | (srcReg << 16) | (srcRel << 23) | (altConst << 24))

#define TEX_WORD1(dstReg, dstRel, dstSelX, dstSelY, dstSelZ, dstSelW, lodBias, coordTypeX, coordTypeY, coordTypeZ, coordTypeW) \
   to_LE(dstReg | (dstRel << 7) | (dstSelX << 9) | (dstSelY << 12) | (dstSelZ << 15) | (dstSelW << 18) | \
   (lodBias << 21) | (coordTypeX << 28) | (coordTypeY << 29) | (coordTypeZ << 30) | (coordTypeW << 31))

#define TEX_WORD2(offsetX, offsetY, offsetZ, samplerID, srcSelX, srcSelY, srcSelZ, srcSelW) \
   to_LE(offsetX | (offsetY << 5) | (offsetZ << 10) | (samplerID << 15) | (srcSelX << 20) | (srcSelY << 23) | (srcSelZ << 26) | (srcSelW << 29))

#define VTX_WORD0(inst, type, buffer_id, srcReg, srcSelX, mega) \
   to_LE(inst | (type << 5) | (buffer_id << 8) | (srcReg << 16) | (srcSelX << 24) | (mega << 26))

#define VTX_WORD1(dstReg, dstSelX, dstSelY, dstSelZ, dstSelW) \
   to_LE(dstReg | (dstSelX << 9) | (dstSelY << 12) | (dstSelZ << 15) | (dstSelW << 18) | (1 << 21))

#define VTX_WORD2(offset, ismega) \
   to_LE(offset| (ismega << 19))

#define _x 0
#define _y 1
#define _z 2
#define _w 3
#define _0 4
#define _1 5
#define _m 7 /*mask*/

#define _xyzw 0b1111
#define _xy__ 0b0011

#define GX2_COMP_SEL(c0, c1, c2, c3) (((c0) << 24) | ((c1) << 16) | ((c2) << 8) | (c3))

#define ALU_LITERAL(v)  to_QWORD(to_LE(v), 0)
#define ALU_LITERAL2(v0,v1)  to_QWORD(to_LE(v0), to_LE(v1))
#define ALU_LITERAL3(v0,v1,v2)  ALU_LITERAL2(v0,v1),ALU_LITERAL(v2)
#define ALU_LITERAL4(v0,v1,v2,v3)  ALU_LITERAL2(v0,v1),ALU_LITERAL2(v2,v3)
#define ALU_LITERAL5(v0,v1,v2,v3,v5)  ALU_LITERAL4(v0,v1,v2,v3),ALU_LITERAL(v4)

/* SRCx_SEL special constants */
#define ALU_SRC_1_DBL_L     0xF4
#define ALU_SRC_1_DBL_M     0xF5
#define ALU_SRC_0_5_DBL_L   0xF6
#define ALU_SRC_0_5_DBL_M   0xF7
#define ALU_SRC_0           0xF8
#define ALU_SRC_1           0xF9
#define ALU_SRC_1_INT       0xFA
#define ALU_SRC_M_1_INT     0xFB
#define ALU_SRC_0_5         0xFC
#define ALU_SRC_LITERAL     0xFD
#define ALU_SRC_PV          0xFE
#define ALU_SRC_PS          0xFF

#define _NEG                | (1 << 12)
#define _ABS                | (1 << 13)

#define ALU_OMOD_OFF          0x0
#define ALU_OMOD_M2           0x1
#define ALU_OMOD_M4           0x2
#define ALU_OMOD_D2           0x3

#define ALU_VEC_012           0x0
#define ALU_VEC_021           0x1
#define ALU_VEC_120           0x2
#define ALU_VEC_102           0x3
#define ALU_VEC_201           0x4
#define ALU_VEC_210           0x5
#define VEC_012               | to_QWORD(0, to_LE(ALU_VEC_012 << 18))
#define VEC_021               | to_QWORD(0, to_LE(ALU_VEC_021 << 18))
#define VEC_120               | to_QWORD(0, to_LE(ALU_VEC_120 << 18))
#define VEC_102               | to_QWORD(0, to_LE(ALU_VEC_102 << 18))
#define VEC_201               | to_QWORD(0, to_LE(ALU_VEC_201 << 18))
#define VEC_210               | to_QWORD(0, to_LE(ALU_VEC_210 << 18))

#define VALID_PIX       | to_QWORD(0,to_LE(1 << 22))

#define ALU_SCL_210           0x0
#define ALU_SCL_122           0x1
#define ALU_SCL_212           0x2
#define ALU_SCL_221           0x3

#define SCL_210               | to_QWORD(0, to_LE(ALU_SCL_210 << 18))
#define SCL_122               | to_QWORD(0, to_LE(ALU_SCL_122 << 18))
#define SCL_212               | to_QWORD(0, to_LE(ALU_SCL_212 << 18))
#define SCL_221               | to_QWORD(0, to_LE(ALU_SCL_221 << 18))

#define FETCH_TYPE(x) x
#define MINI(x) ((x) - 1)
#define MEGA(x) (MINI(x) | 0x80000000)
#define OFFSET(x) x

#define VERTEX_DATA     0
#define INSTANCE_DATA   1
#define NO_INDEX_OFFSET 2

/* CF defines */
#define CF_COND_ACTIVE      0x0
#define CF_COND_FALSE       0x1
#define CF_COND_BOOL        0x2
#define CF_COND_NOT_BOOL    0x3

/* TEX defines */
#define TEX_UNNORMALIZED    0x0
#define TEX_NORMALIZED      0x1

/* instructions */
/* CF */
#define CF_INST_TEX              0x01
#define CF_INST_VTX              0x02
#define CF_INST_JUMP             0x0A
#define CF_INST_ELSE             0x0D
#define CF_INST_CALL_FS          0x13
#define CF_INST_EMIT_VERTEX      0x15
#define CF_INST_MEM_RING         0x26

#define CF_INST_ALU              0x08
#define CF_INST_ALU_PUSH_BEFORE  0x09
#define CF_INST_ALU_POP_AFTER    0x0A
/* ALU */
#define OP2_INST_ADD             0x0
#define OP2_INST_MUL             0x1
#define OP2_INST_MUL_IEEE        0x2
#define OP2_INST_MIN             0x04
#define OP2_INST_MAX             0x03
#define OP2_INST_MAX_DX10        0x05
#define OP2_INST_FRACT           0x10
#define OP2_INST_SETGT           0x09
#define OP2_INST_SETE_DX10       0x0C
#define OP2_INST_SETGT_DX10      0x0D
#define OP2_INST_FLOOR           0x14
#define OP2_INST_MOV             0x19
#define OP2_INST_PRED_SETGT      0x21
#define OP2_INST_PRED_SETE_INT   0x42
#define OP2_INST_DOT4            0x50
#define OP2_INST_DOT4_IEEE       0x51
#define OP2_INST_RECIP_IEEE      0x66
#define OP2_INST_RECIPSQRT_IEEE  0x69
#define OP2_INST_SQRT_IEEE       0x6A
#define OP2_INST_SIN             0x6E
#define OP2_INST_COS             0x6F

#define OP3_INST_MULADD          0x10
#define OP3_INST_CNDGT           0x19
#define OP3_INST_CNDE_INT        0x1C
/* EXP */
#define CF_INST_EXP      0x27
#define CF_INST_EXP_DONE 0x28

/* TEX */
#define TEX_INST_GET_GRADIENTS_H 0x07
#define TEX_INST_GET_GRADIENTS_V 0x08
#define TEX_INST_SAMPLE          0x10

/* VTX */
#define VTX_INST_FETCH  0x0

/* EXPORT_TYPE */
#define EXPORT_TYPE_PIXEL  0x0
#define EXPORT_TYPE_POS    0x1
#define EXPORT_TYPE_PARAM  0x2

#define EXPORT_ARRAY_BASE_POS(id)      (0x3C + id)   /* [0, 3] */
#define EXPORT_ARRAY_BASE_PARAM(id)    id          /* [0, 31] */
#define EXPORT_ARRAY_BASE_PIX(id)      id

/* exports */
#define POS(id)   EXPORT_ARRAY_BASE_POS(id)   | (EXPORT_TYPE_POS   << 13)
#define PARAM(id) EXPORT_ARRAY_BASE_PARAM(id) | (EXPORT_TYPE_PARAM << 13)
#define PIX(id)   EXPORT_ARRAY_BASE_PIX(id)   | (EXPORT_TYPE_PIXEL << 13)
#define POS0   POS(0)
#define PARAM0 PARAM(0)
#define PARAM1 PARAM(1)
#define PIX0   PIX(0)

/* registers */
#define __     (0x80) /* invalid regitser (write mask off) */
#define _R(x)  x
#define _R0    _R(0x0)
#define _R1    _R(0x1)
#define _R2    _R(0x2)
#define _R3    _R(0x3)
#define _R4    _R(0x4)
#define _R5    _R(0x5)
#define _R6    _R(0x6)
#define _R7    _R(0x7)
#define _R8    _R(0x8)
#define _R9    _R(0x9)
#define _R10    _R(0xA)
#define _R11    _R(0xB)
#define _R12    _R(0xC)
#define _R13    _R(0xD)
#define _R14    _R(0xE)
#define _R15    _R(0xF)

#define _R120    _R(0x78)
#define _R121    _R(0x79)
#define _R122    _R(0x7A)
#define _R123    _R(0x7B)
#define _R124    _R(0x7C)
#define _R125    _R(0x7D)
#define _R126    _R(0x7E)
#define _R127    _R(0x7F)

/* texture */
#define _t(x)  x
#define _t0    _t(0x0)

/* sampler */
#define _s(x)  x
#define _s0    _s(0x0)

#define _b(x)  x

#define CALL_FS to_QWORD(CF_DWORD0(0), CF_DWORD1(0,0,0,0,0,CF_INST_CALL_FS))

#define TEX(addr, cnt) to_QWORD(CF_DWORD0(addr), CF_DWORD1(0x0, 0x0, CF_COND_ACTIVE, (cnt - 1), 0x0, CF_INST_TEX))
#define VTX(addr, cnt) to_QWORD(CF_DWORD0(addr), CF_DWORD1(0x0, 0x0, CF_COND_ACTIVE, (cnt - 1), 0x0, CF_INST_VTX))
#define JUMP(popCount, addr) to_QWORD(CF_DWORD0(addr), CF_DWORD1(popCount, 0x0, CF_COND_ACTIVE, 0x0, 0x0, CF_INST_JUMP))
#define ELSE(popCount, addr) to_QWORD(CF_DWORD0(addr), CF_DWORD1(popCount, 0x0, CF_COND_ACTIVE, 0x0, 0x0, CF_INST_ELSE))

#define ALU(addr, cnt) to_QWORD(CF_ALU_WORD0(addr, 0x0, 0x0, 0x0), CF_ALU_WORD1(0x0, 0x0, 0x0, (cnt - 1), 0x0, CF_INST_ALU))
#define ALU_PUSH_BEFORE(addr, cnt) to_QWORD(CF_ALU_WORD0(addr, 0x0, 0x0, 0x0), CF_ALU_WORD1(0x0, 0x0, 0x0, (cnt - 1), 0x0, CF_INST_ALU_PUSH_BEFORE))
#define ALU_POP_AFTER(addr, cnt) to_QWORD(CF_ALU_WORD0(addr, 0x0, 0x0, 0x0), CF_ALU_WORD1(0x0, 0x0, 0x0, (cnt - 1), 0x0, CF_INST_ALU_POP_AFTER))

#define EXP_DONE(dstReg_and_type, srcReg, srcSelX, srcSelY, srcSelZ, srcSelW) to_QWORD(CF_EXP_WORD0(dstReg_and_type, srcReg, 0x0, 0x0, 0x0), \
   CF_EXP_WORD1(srcSelX, srcSelY, srcSelZ, srcSelW, 0x0, CF_INST_EXP_DONE))

#define EXP(dstReg_and_type, srcReg, srcSelX, srcSelY, srcSelZ, srcSelW) to_QWORD(CF_EXP_WORD0(dstReg_and_type, srcReg, 0x0, 0x0, 0x0), \
   CF_EXP_WORD1(srcSelX, srcSelY, srcSelZ, srcSelW, 0x0, CF_INST_EXP))

#define MEM_RING(arrayBase, dstReg, writeMask, arraySize, elemSize) \
   to_QWORD(CF_ALLOC_EXPORT_WORD0(arrayBase, 0x00, dstReg, 0x00, 0x00, elemSize), \
   CF_ALLOC_EXPORT_WORD1_BUF(arraySize, writeMask, CF_INST_MEM_RING))

#define EMIT_VERTEX to_QWORD(0, CF_DWORD1(0, 0, 0, 0, 0, CF_INST_EMIT_VERTEX))

#define ALU_OP2(inst, dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, omod) \
   to_QWORD(ALU_WORD0(((src0Sel) & ((1 << 13) - 1)), 0x0, src0Chan, 0x0, ((src1Sel) & ((1 << 13) - 1)), 0x0, src1Chan, 0x0, 0x0, 0x0), \
   ALU_WORD1_OP2(((src0Sel) >> 13), ((src1Sel) >> 13), 0x0, 0x0, (((dstGpr&__) >> 7) ^ 0x1), omod, inst, 0x0, 0x0, dstGpr, 0x0, dstChan, 0x0))

#define ALU_OP3(inst, dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, src2Sel, src2Chan) \
   to_QWORD(ALU_WORD0(src0Sel, 0x0, src0Chan, 0x0, src1Sel, 0x0, src1Chan, 0x0, 0x0, 0x0), \
   ALU_WORD1_OP3(src2Sel, 0x0, src2Chan, 0x0, inst, 0x0, dstGpr, 0x0, dstChan, 0x0))

#define ALU_ADD(dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan) \
   ALU_OP2(OP2_INST_ADD, dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, ALU_OMOD_OFF)

#define ALU_ADD_x2(dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan) \
   ALU_OP2(OP2_INST_ADD, dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, ALU_OMOD_M2)

#define ALU_ADD_D2(dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan) \
   ALU_OP2(OP2_INST_ADD, dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, ALU_OMOD_D2)

#define ALU_MUL(dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan) \
   ALU_OP2(OP2_INST_MUL, dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, ALU_OMOD_OFF)

#define ALU_MUL_x2(dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan) \
   ALU_OP2(OP2_INST_MUL, dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, ALU_OMOD_M2)

#define ALU_MUL_x4(dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan) \
   ALU_OP2(OP2_INST_MUL, dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, ALU_OMOD_M4)

#define ALU_MUL_IEEE(dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan) \
   ALU_OP2(OP2_INST_MUL_IEEE, dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, ALU_OMOD_OFF)

#define ALU_MUL_IEEE_x2(dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan) \
   ALU_OP2(OP2_INST_MUL_IEEE, dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, ALU_OMOD_M2)

#define ALU_MUL_IEEE_x4(dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan) \
   ALU_OP2(OP2_INST_MUL_IEEE, dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, ALU_OMOD_M4)

#define ALU_FRACT(dstGpr, dstChan, src0Sel, src0Chan) \
   ALU_OP2(OP2_INST_FRACT, dstGpr, dstChan, src0Sel, src0Chan, ALU_SRC_0, 0x0, ALU_OMOD_OFF)

#define ALU_FLOOR(dstGpr, dstChan, src0Sel, src0Chan) \
   ALU_OP2(OP2_INST_FLOOR, dstGpr, dstChan, src0Sel, src0Chan, ALU_SRC_0, 0x0, ALU_OMOD_OFF)

#define ALU_SQRT_IEEE(dstGpr, dstChan, src0Sel, src0Chan) \
   ALU_OP2(OP2_INST_SQRT_IEEE, dstGpr, dstChan, src0Sel, src0Chan, ALU_SRC_0, 0x0, ALU_OMOD_OFF)

#define ALU_SQRT_IEEE_D2(dstGpr, dstChan, src0Sel, src0Chan) \
   ALU_OP2(OP2_INST_SQRT_IEEE, dstGpr, dstChan, src0Sel, src0Chan, ALU_SRC_0, 0x0, ALU_OMOD_D2)

#define ALU_MOV(dstGpr, dstChan, src0Sel, src0Chan) \
   ALU_OP2(OP2_INST_MOV, dstGpr, dstChan, src0Sel, src0Chan, ALU_SRC_0, 0x0, ALU_OMOD_OFF)

#define ALU_MOV_D2(dstGpr, dstChan, src0Sel, src0Chan) \
   ALU_OP2(OP2_INST_MOV, dstGpr, dstChan, src0Sel, src0Chan, ALU_SRC_0, 0x0, ALU_OMOD_D2)

#define ALU_MOV_x2(dstGpr, dstChan, src0Sel, src0Chan) \
   ALU_OP2(OP2_INST_MOV, dstGpr, dstChan, src0Sel, src0Chan, ALU_SRC_0, 0x0, ALU_OMOD_M2)

#define ALU_MOV_x4(dstGpr, dstChan, src0Sel, src0Chan) \
   ALU_OP2(OP2_INST_MOV, dstGpr, dstChan, src0Sel, src0Chan, ALU_SRC_0, 0x0, ALU_OMOD_M4)

#define ALU_DOT4_IEEE(dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan) \
   ALU_OP2(OP2_INST_DOT4_IEEE, dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, ALU_OMOD_OFF)

#define ALU_DOT4(dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan) \
   ALU_OP2(OP2_INST_DOT4, dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, ALU_OMOD_OFF)

#define ALU_PRED_SETGT(dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan) \
   ALU_OP2(OP2_INST_PRED_SETGT, dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, ALU_OMOD_OFF)

#define ALU_SETE_DX10(dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan) \
   ALU_OP2(OP2_INST_SETE_DX10, dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, ALU_OMOD_OFF)

#define ALU_SETGT_DX10(dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan) \
   ALU_OP2(OP2_INST_SETGT_DX10, dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, ALU_OMOD_OFF)

#define ALU_SETGT(dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan) \
   ALU_OP2(OP2_INST_SETGT, dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, ALU_OMOD_OFF)

#define ALU_PRED_SETE_INT(dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan) \
   ALU_OP2(OP2_INST_PRED_SETE_INT, dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, ALU_OMOD_OFF)

#define ALU_MIN(dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan) \
   ALU_OP2(OP2_INST_MIN, dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, ALU_OMOD_OFF)

#define ALU_MAX(dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan) \
   ALU_OP2(OP2_INST_MAX, dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, ALU_OMOD_OFF)

#define ALU_MAX_DX10(dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan) \
   ALU_OP2(OP2_INST_MAX_DX10, dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, ALU_OMOD_OFF)

#define ALU_RECIP_IEEE(dstGpr, dstChan, src0Sel, src0Chan) \
   ALU_OP2(OP2_INST_RECIP_IEEE, dstGpr, dstChan, src0Sel, src0Chan, ALU_SRC_0, 0x0, ALU_OMOD_OFF)

#define ALU_RECIPSQRT_IEEE(dstGpr, dstChan, src0Sel, src0Chan) \
   ALU_OP2(OP2_INST_RECIPSQRT_IEEE, dstGpr, dstChan, src0Sel, src0Chan, ALU_SRC_0, 0x0, ALU_OMOD_OFF)

#define ALU_SIN(dstGpr, dstChan, src0Sel, src0Chan) \
   ALU_OP2(OP2_INST_SIN, dstGpr, dstChan, src0Sel, src0Chan, ALU_SRC_0, 0x0, ALU_OMOD_OFF)

#define ALU_COS(dstGpr, dstChan, src0Sel, src0Chan) \
   ALU_OP2(OP2_INST_COS, dstGpr, dstChan, src0Sel, src0Chan, ALU_SRC_0, 0x0, ALU_OMOD_OFF)

#define ALU_COS_D2(dstGpr, dstChan, src0Sel, src0Chan) \
   ALU_OP2(OP2_INST_COS, dstGpr, dstChan, src0Sel, src0Chan, ALU_SRC_0, 0x0, ALU_OMOD_D2)

#define ALU_MULADD(dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, src2Sel, src2Chan) \
   ALU_OP3(OP3_INST_MULADD, dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, src2Sel, src2Chan)

#define ALU_CNDGT(dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, src2Sel, src2Chan) \
   ALU_OP3(OP3_INST_CNDGT, dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, src2Sel, src2Chan)

#define ALU_CNDE_INT(dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, src2Sel, src2Chan) \
   ALU_OP3(OP3_INST_CNDE_INT, dstGpr, dstChan, src0Sel, src0Chan, src1Sel, src1Chan, src2Sel, src2Chan)

#define TEX_SAMPLE(dstReg, dstSelX, dstSelY, dstSelZ, dstSelW, srcReg, srcSelX, srcSelY, srcSelZ, srcSelW, resourceID, samplerID)\
   to_QWORD(TEX_WORD0(TEX_INST_SAMPLE, 0x0, 0x0, resourceID, srcReg, 0x0, 0x0), \
   TEX_WORD1(dstReg, 0x0, dstSelX, dstSelY, dstSelZ, dstSelW, 0x0, TEX_NORMALIZED, TEX_NORMALIZED, TEX_NORMALIZED, TEX_NORMALIZED)), \
   to_QWORD(TEX_WORD2(0x0, 0x0, 0x0, samplerID, _x, _y, _0, _x), 0x00000000)

#define TEX_GET_GRADIENTS_H(dstReg, dstSelX, dstSelY, dstSelZ, dstSelW, srcReg, srcSelX, srcSelY, srcSelZ, srcSelW, resourceID, samplerID)\
   to_QWORD(TEX_WORD0(TEX_INST_GET_GRADIENTS_H, 0x0, 0x0, resourceID, srcReg, 0x0, 0x0), \
   TEX_WORD1(dstReg, 0x0, dstSelX, dstSelY, dstSelZ, dstSelW, 0x0, TEX_NORMALIZED, TEX_NORMALIZED, TEX_NORMALIZED, TEX_NORMALIZED)), \
   to_QWORD(TEX_WORD2(0x0, 0x0, 0x0, samplerID, _x, _y, _z, _x), 0x00000000)

#define TEX_GET_GRADIENTS_V(dstReg, dstSelX, dstSelY, dstSelZ, dstSelW, srcReg, srcSelX, srcSelY, srcSelZ, srcSelW, resourceID, samplerID)\
   to_QWORD(TEX_WORD0(TEX_INST_GET_GRADIENTS_V, 0x0, 0x0, resourceID, srcReg, 0x0, 0x0), \
   TEX_WORD1(dstReg, 0x0, dstSelX, dstSelY, dstSelZ, dstSelW, 0x0, TEX_NORMALIZED, TEX_NORMALIZED, TEX_NORMALIZED, TEX_NORMALIZED)), \
   to_QWORD(TEX_WORD2(0x0, 0x0, 0x0, samplerID, _x, _y, _z, _x), 0x00000000)

#define VTX_FETCH(dstReg, dstSelX, dstSelY, dstSelZ, dstSelW, srcReg, srcSelX, buffer_id, type, mega, offset) \
   to_QWORD(VTX_WORD0(VTX_INST_FETCH, type, buffer_id, srcReg, srcSelX, mega), VTX_WORD1(dstReg, dstSelX, dstSelY, dstSelZ, dstSelW)) , \
   to_QWORD(VTX_WORD2(offset, (mega >> 31)), 0x00000000)

#define _x2(v)        v, v
#define _x4(v)   _x2(v), _x2(v)
#define _x8(v)   _x4(v), _x4(v)
#define _x16(v)  _x8(v), _x8(v)

#define _x9(v)   _x8(v), v
#define _x30(v) _x16(v), _x8(v), _x4(v),_x2(v)
#define _x31(v) _x30(v), v

#endif /* GX2_SHADER_INL_H */
