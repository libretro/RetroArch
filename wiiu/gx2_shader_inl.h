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
#define to_LE(x) __builtin_bswap32(x)
#else
#define to_LE(x) x
#endif

/* CF */
#define CF_WORD0(addr) to_LE(addr)

#define CF_WORD1(popCount, cfConst, cond, count, callCount, inst) \
   to_LE(popCount | (cfConst << 3) | (cond << 8) | (count << 10) | (callCount << 13) | (inst << 23) | (1 << 31))

#define CF_ALU_WORD0(addr, kcacheBank0, kcacheBank1, kcacheMode0) \
   to_LE(addr | (kcacheBank0 << 16) | (kcacheBank1 << 20) | (kcacheMode0 << 22))
#define CF_ALU_WORD1(kcacheMode1, kcacheAddr0, kcacheAddr1, count, altConst, inst) \
   to_LE(kcacheMode1 | (kcacheAddr0 << 2) | (kcacheAddr1 << 10) | (count << 18) | (altConst << 25) | (inst << 26) | (1 << 31))

#define CF_EXP_WORD0(dstReg_and_type, srcReg, srcRel, indexGpr, elemSize)\
   to_LE(dstReg_and_type | (srcReg << 15) | (srcRel << 22) | (indexGpr << 23) | (elemSize << 30))

#define CF_EXP_WORD1(srcSelX, srcSelY, srcSelZ, srcSelW, validPixelMode, inst) \
   to_LE(srcSelX | (srcSelY << 3) | (srcSelZ << 6) | (srcSelW << 9) | (validPixelMode << 22) | (inst << 23) | (1 << 31))

#define NO_BARRIER      & to_LE(~(1 << 31))
#define END_OF_PROGRAM  | to_LE(1 << 21)
#define VALID_PIX       | to_LE(1 << 22)
#define WHOLE_QUAD_MODE | to_LE(1 << 30)

#define ALU_LAST        to_LE(1 << 31) |

/* ALU */

#define ALU_WORD0(src0Sel, src0Rel, src0Chan, src0Neg, src1Sel, src1Rel, src1Chan, src1Neg, indexMode, predSel) \
   to_LE(src0Sel | (src0Rel << 9) | (src0Chan << 10) | (src0Neg << 12) | (src1Sel << 13) | (src1Rel << 22) \
                 | (src1Chan << 23) | (src1Neg << 25) | (indexMode << 26) | (predSel << 29))

#define ALU_WORD1_OP2(src0Abs, src1Abs, updateExecuteMask, updatePred, writeMask, omod, inst, encoding, bankSwizzle, dstGpr, dstRel, dstChan, clamp) \
      to_LE(src0Abs | (src1Abs << 1) | (updateExecuteMask << 2) | (updatePred << 3) | (writeMask << 4) | (omod << 5) | (inst << 7) | \
                         (encoding << 15) | (bankSwizzle << 18) | (dstGpr << 21) | (dstRel << 28) | (dstChan << 29) | (clamp << 31))

#define ALU_WORD1_OP3(src2Sel, src2Rel, src2Chan, src2Neg, inst, encoding, bankSwizzle, dstGpr, dstRel, dstChan, clamp) \
      to_LE(src2Sel | (src2Rel << 9) | (src2Chan << 10) | (src2Neg << 12) | (inst << 13) | \
     (encoding << 15) | (bankSwizzle << 18) | (dstGpr << 21) | (dstRel << 28) | (dstChan << 29) | (clamp << 31)

/* TEX */
#define TEX_WORD0(inst, bcFracMode, fetchWholeQuad, resourceID, srcReg, srcRel, altConst) \
   to_LE(inst | (bcFracMode << 5) | (fetchWholeQuad << 7) | (resourceID << 8) | (srcReg << 16) | (srcRel << 23) | (altConst << 24))

#define TEX_WORD1(dstReg, dstRel, dstSelX, dstSelY, dstSelZ, dstSelW, lodBias, coordTypeX, coordTypeY, coordTypeZ, coordTypeW) \
   to_LE(dstReg | (dstRel << 7) | (dstSelX << 9) | (dstSelY << 12) | (dstSelZ << 15) | (dstSelW << 18) | \
   (lodBias << 21) | (coordTypeX << 28) | (coordTypeY << 29) | (coordTypeZ << 30) | (coordTypeW << 31))

#define TEX_WORD2(offsetX, offsetY, offsetZ, samplerID, srcSelX, srcSelY, srcSelZ, srcSelW) \
   to_LE(offsetX | (offsetY << 5) | (offsetZ << 10) | (samplerID << 15) | (srcSelX << 20) | (srcSelY << 23) | (srcSelZ << 26) | (srcSelW << 29))


#define _X 0
#define _Y 1
#define _Z 2
#define _W 3
#define _0 4
#define _1 5

#define GX2_COMP_SEL(c0, c1, c2, c3) (((c0) << 24) | ((c1) << 16) | ((c2) << 8) | (c3))

#define ALU_LITERAL(v)  to_LE(v)

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
#define CF_INST_TEX     0x01
#define CF_INST_CALL_FS 0x13
/* ALU */
#define ALU_INST_ALU     0x8
#define OP2_INST_MOV     0x19

/* EXP */
#define CF_INST_EXP_DONE 0x28

/* TEX */
#define TEX_INST_SAMPLE 0x10

/* EXPORT_TYPE */
#define EXPORT_TYPE_PIXEL  0x0
#define EXPORT_TYPE_POS    0x1
#define EXPORT_TYPE_PARAM  0x2

#define EXPORT_ARRAY_BASE_POS(id)      (0x3C + id)   // [0, 3]
#define EXPORT_ARRAY_BASE_PARAM(id)    id          // [0, 31]
#define EXPORT_ARRAY_BASE_PIX(id)      id

/* exports */
#define POS(id)   EXPORT_ARRAY_BASE_POS(id)   | (EXPORT_TYPE_POS   << 13)
#define PARAM(id) EXPORT_ARRAY_BASE_PARAM(id) | (EXPORT_TYPE_PARAM << 13)
#define PIX(id)   EXPORT_ARRAY_BASE_PIX(id)   | (EXPORT_TYPE_PIXEL << 13)
#define POS0   POS(0)
#define PARAM0 PARAM(0)
#define PIX0   PIX(0)

/* registers */
#define _R(x)  x
#define _R0    _R(0x0)
#define _R1    _R(0x1)
#define _R2    _R(0x2)

/* texture */
#define _t(x)  x
#define _t0    _t(0x0)

/* sampler */
#define _s(x)  x
#define _s0    _s(0x0)

#define CALL_FS CF_WORD0(0), CF_WORD1(0,0,0,0,0,CF_INST_CALL_FS)

#define TEX(addr, cnt) CF_WORD0(addr), CF_WORD1(0x0, 0x0, CF_COND_ACTIVE, 0x0, (cnt - 1), CF_INST_TEX)

#define ALU(addr, cnt) CF_ALU_WORD0(addr, 0x0, 0x0, 0x0), CF_ALU_WORD1(0x0, 0x0, 0x0, (cnt - 1), 0x0, ALU_INST_ALU)

#define EXP_DONE(dstReg_and_type, srcReg, srcSelX, srcSelY, srcSelZ, srcSelW) CF_EXP_WORD0(dstReg_and_type, srcReg, 0x0, 0x0, 0x0), \
   CF_EXP_WORD1(srcSelX, srcSelY, srcSelZ, srcSelW, 0x0, CF_INST_EXP_DONE)

#define ALU_MOV(dstGpr, dstChan, src0Sel, src0Chan) ALU_WORD0(src0Sel, 0x0, src0Chan, 0x0, ALU_SRC_0, 0x0, 0x0, 0x0, 0x0, 0x0), \
   ALU_WORD1_OP2(0x0, 0x0, 0x0, 0x0, 0x1, 0x0, OP2_INST_MOV, 0x0, 0x0, dstGpr, 0x0, dstChan, 0x0)


#define TEX_SAMPLE(dstReg, dstSelX, dstSelY, dstSelZ, dstSelW, srcReg, srcSelX, srcSelY, srcSelZ, srcSelW, resourceID, samplerID)\
   TEX_WORD0(TEX_INST_SAMPLE, 0x0, 0x0, resourceID, srcReg, 0x0, 0x0), \
   TEX_WORD1(dstReg, 0x0, dstSelX, dstSelY, dstSelZ, dstSelW, 0x0, TEX_NORMALIZED, TEX_NORMALIZED, TEX_NORMALIZED, TEX_NORMALIZED), \
   TEX_WORD2(0x0, 0x0, 0x0, samplerID, _X, _Y, _0, _X)

#define _x2(v)        v, v
#define _x4(v)   _x2(v), _x2(v)
#define _x8(v)   _x4(v), _x4(v)
#define _x16(v)  _x8(v), _x8(v)

#define _x9(v)   _x8(v), v
#define _x30(v) _x16(v), _x8(v), _x4(v),_x2(v)
#define _x31(v) _x30(v), v

#endif // GX2_SHADER_INL_H
