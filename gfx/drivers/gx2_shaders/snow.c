/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2018 - Ali Bouhlel
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

#include <stddef.h>
#include <malloc.h>
#include <string.h>
#include <wiiu/gx2/common.h>
#include "gx2_shader_inl.h"
#include "menu_shaders.h"

__attribute__((aligned(GX2_SHADER_ALIGNMENT)))
static struct
{
   u64 cf[32];
   u64 alu[16];
} vs_program =
{
   {
      CALL_FS NO_BARRIER,
      ALU(32,16) KCACHE0(CB1, _0_15),
      EXP_DONE(POS0, _R1,_x,_y,_z,_w),
      EXP_DONE(PARAM0, _R0,_m,_m,_m,_m)
      END_OF_PROGRAM
   },
   {
      /* 0 */
      ALU_MUL(__,_x, _R1,_w, KC0(3),_w),
      ALU_MUL(__,_y, _R1,_w, KC0(3),_z),
      ALU_MUL(__,_z, _R1,_w, KC0(3),_y),
      ALU_MUL(__,_w, _R1,_w, KC0(3),_x)
      ALU_LAST,
      /* 1 */
      ALU_MULADD(_R123,_x, _R1,_z, KC0(2),_w, ALU_SRC_PV,_x),
      ALU_MULADD(_R123,_y, _R1,_z, KC0(2),_z, ALU_SRC_PV,_y),
      ALU_MULADD(_R123,_z, _R1,_z, KC0(2),_y, ALU_SRC_PV,_z),
      ALU_MULADD(_R123,_w, _R1,_z, KC0(2),_x, ALU_SRC_PV,_w)
      ALU_LAST,
      /* 2 */
      ALU_MULADD(_R123,_x, _R1,_y, KC0(1),_w, ALU_SRC_PV,_x),
      ALU_MULADD(_R123,_y, _R1,_y, KC0(1),_z, ALU_SRC_PV,_y),
      ALU_MULADD(_R123,_z, _R1,_y, KC0(1),_y, ALU_SRC_PV,_z),
      ALU_MULADD(_R123,_w, _R1,_y, KC0(1),_x, ALU_SRC_PV,_w)
      ALU_LAST,
      /* 3 */
      ALU_MULADD(_R1,_x, _R1,_x, KC0(0),_x, ALU_SRC_PV,_w),
      ALU_MULADD(_R1,_y, _R1,_x, KC0(0),_y, ALU_SRC_PV,_z),
      ALU_MULADD(_R1,_z, _R1,_x, KC0(0),_z, ALU_SRC_PV,_y),
      ALU_MULADD(_R1,_w, _R1,_x, KC0(0),_w, ALU_SRC_PV,_x)
      ALU_LAST,
   },
};

__attribute__((aligned(GX2_SHADER_ALIGNMENT)))
static struct
{
   u64 cf[32];
   u64 alu[56];
   u64 alu1[27];
   u64 alu2[51];
   u64 alu3[27];
   u64 alu4[52];
   u64 alu5[27];
   u64 alu6[51];
   u64 alu7[27];
   u64 alu8[52];
   u64 alu9[27];
   u64 alu10[52];
   u64 alu11[27];
   u64 alu12[52];
   u64 alu13[27];
   u64 alu14[52];
   u64 alu15[27];
   u64 alu16[6];
} ps_program =
{
   {
      ALU_PUSH_BEFORE(32,56) KCACHE0(CB1, _0_15),
      JUMP(1, 3) VALID_PIX,
      ALU_POP_AFTER(88,27),
      ALU_PUSH_BEFORE(115,51),
      JUMP(1, 6) VALID_PIX,
      ALU_POP_AFTER(166,27),
      ALU_PUSH_BEFORE(193,52),
      JUMP(1, 9) VALID_PIX,
      ALU_POP_AFTER(245,27),
      ALU_PUSH_BEFORE(272,51),
      JUMP(1, 12) VALID_PIX,
      ALU_POP_AFTER(323,27),
      ALU_PUSH_BEFORE(350,52),
      JUMP(1, 15) VALID_PIX,
      ALU_POP_AFTER(402,27),
      ALU_PUSH_BEFORE(429,52),
      JUMP(1, 18) VALID_PIX,
      ALU_POP_AFTER(481,27),
      ALU_PUSH_BEFORE(508,52),
      JUMP(1, 21) VALID_PIX,
      ALU_POP_AFTER(560,27),
      ALU_PUSH_BEFORE(587,52),
      JUMP(1, 24) VALID_PIX,
      ALU_POP_AFTER(639,27),
      ALU(666,6),
      EXP_DONE(PIX0, _R0,_y,_y,_y,_w)
      END_OF_PROGRAM
   },
   {
      /* 0 */
      ALU_MUL(__,_w, KC0(5),_x, ALU_SRC_LITERAL,_x),
      ALU_RECIP_IEEE(__,_w, KC0(4),_x) SCL_210
      ALU_LAST,
      ALU_LITERAL(0x3ECCCCCD),
      /* 1 */
      ALU_MUL_IEEE(_R1,_x, _R0,_x, ALU_SRC_PS,_x),
      ALU_MUL_IEEE(_R0,_y, _R0,_y, ALU_SRC_PS,_x),
      ALU_MUL(_R2,_z, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E800000),
      /* 2 */
      ALU_MUL_x2(__,_x, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x),
      ALU_ADD(_R1,_y, ALU_SRC_PV _NEG,_y, ALU_SRC_1,_x),
      ALU_MOV_x2(_R127,_z, ALU_SRC_PV _NEG,_z),
      ALU_MOV_x4(__,_w, ALU_SRC_PV,_z)
      ALU_LAST,
      ALU_LITERAL(0x40490FD0),
      /* 3 */
      ALU_ADD(_R126,_z, ALU_SRC_PV,_y, ALU_SRC_PV,_w),
      ALU_MULADD(_R1,_w, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x, ALU_SRC_PV,_x)
      ALU_LAST,
      ALU_LITERAL(0x3F99999A),
      /* 4 */
      ALU_ADD(__,_z, ALU_SRC_PV,_w, ALU_SRC_1,_x)
      ALU_LAST,
      /* 5 */
      ALU_MULADD(_R123,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 6 */
      ALU_FRACT(__,_w, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 7 */
      ALU_MULADD(_R123,_x, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB, 0x40C90FDB),
      /* 8 */
      ALU_MUL(__,_z, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 9 */
      ALU_COS(__,_x, ALU_SRC_PV,_z) SCL_210
      ALU_LAST,
      /* 10 */
      ALU_MUL_IEEE_x4(__,_w, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E000000),
      /* 11 */
      ALU_ADD(_R0,_x, _R1,_x, ALU_SRC_PV,_w)
      ALU_LAST,
      /* 12 */
      ALU_ADD(__,_w, ALU_SRC_PV,_x, _R127,_z)
      ALU_LAST,
      /* 13 */
      ALU_MUL_IEEE(_R0,_x, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x),
      ALU_MUL_IEEE(_R0,_y, _R126,_z, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x41A00000),
      /* 14 */
      ALU_FLOOR(__,_x, ALU_SRC_PV,_x),
      ALU_FLOOR(__,_y, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 15 */
      ALU_DOT4(__,_x, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x),
      ALU_DOT4(__,_y, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_y),
      ALU_DOT4(__,_z, ALU_SRC_PV,_y, ALU_SRC_0,_x),
      ALU_DOT4(__,_w, ALU_SRC_LITERAL,_z, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL3(0x414FD639, 0x429C774C, 0x80000000),
      /* 16 */
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 17 */
      ALU_FRACT(__,_z, ALU_SRC_PV,_w)
      ALU_LAST,
      /* 18 */
      ALU_MULADD(_R123,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB, 0x40C90FDB),
      /* 19 */
      ALU_MUL(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 20 */
      ALU_SIN(__,_x, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
      /* 21 */
      ALU_MUL(__,_y, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x472AEE8C),
      /* 22 */
      ALU_FRACT(_R0,_z, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 23 */
      ALU_SETGT_DX10(__,_x, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E0F5C29),
      /* 24 */
      ALU_CNDE_INT(_R1,_z, ALU_SRC_PV,_x, ALU_SRC_0,_x, ALU_SRC_LITERAL,_x),
      ALU_CNDE_INT(_R0,_w, ALU_SRC_PV,_x, ALU_SRC_0,_x, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL(0x3F800000),
      /* 25 */
      ALU_PRED_SETE_INT(__,_x, _R1,_z, ALU_SRC_0,_x) UPDATE_EXEC_MASK(DEACTIVATE) UPDATE_PRED
      ALU_LAST,
   },
   {
      /* 26 */
      ALU_FRACT(_R127,_x, _R0,_x),
      ALU_MOV_x2(__,_y, _R0,_z),
      ALU_FRACT(_R127,_z, _R0,_y),
      ALU_MOV(__,_w, _R0,_z)
      ALU_LAST,
      /* 27 */
      ALU_FRACT(__,_x, ALU_SRC_PV,_y),
      ALU_ADD(__,_y, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x40000000),
      /* 28 */
      ALU_MULADD(_R127,_y, ALU_SRC_PV _NEG,_y, ALU_SRC_LITERAL,_x, _R127,_z),
      ALU_ADD(__,_z, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL2(0x3E800000, 0x40000000),
      /* 29 */
      ALU_MULADD(_R123,_z, ALU_SRC_PV _NEG,_z, ALU_SRC_LITERAL,_x, _R127,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E800000),
      /* 30 */
      ALU_MUL(__,_x, ALU_SRC_PV,_z, ALU_SRC_PV,_z)
      ALU_LAST,
      /* 31 */
      ALU_MULADD(_R123,_w, _R127,_y, _R127,_y, ALU_SRC_PV,_x)
      ALU_LAST,
      /* 32 */
      ALU_SQRT_IEEE(__,_x, ALU_SRC_PV,_w) SCL_210
      ALU_LAST,
      /* 33 */
      ALU_MUL(__,_y, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x) CLAMP
      ALU_LAST,
      ALU_LITERAL(0x4136DB6E),
      /* 34 */
      ALU_MUL(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3FC90FDB),
      /* 35 */
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 36 */
      ALU_FRACT(__,_z, ALU_SRC_PV,_w)
      ALU_LAST,
      /* 37 */
      ALU_MULADD(_R123,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB, 0x40C90FDB),
      /* 38 */
      ALU_MUL(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 39 */
      ALU_COS_D2(_R0,_w, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
   },
   {
      /* 40 */
      ALU_MUL(__,_x, _R2,_z, ALU_SRC_LITERAL,_x),
      ALU_ADD(__,_y, _R1,_w, ALU_SRC_LITERAL,_y),
      ALU_MULADD(_R2,_x, _R0,_w, ALU_SRC_1,_x, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL2(0x3F333333, 0x3FB6DB6E),
      /* 41 */
      ALU_MOV_x2(_R127,_x, ALU_SRC_PV _NEG,_x),
      ALU_MOV_x4(__,_y, ALU_SRC_PV,_x),
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 42 */
      ALU_FRACT(__,_x, ALU_SRC_PV,_w),
      ALU_ADD(__,_y, _R1,_y, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 43 */
      ALU_MUL_IEEE(_R126,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R123,_z, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_z, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL3(0x3FB6DB6E, 0xC0490FDB, 0x40C90FDB),
      /* 44 */
      ALU_MUL(__,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 45 */
      ALU_COS(__,_x, ALU_SRC_PV,_y) SCL_210
      ALU_LAST,
      /* 46 */
      ALU_MUL_IEEE_x4(__,_z, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3DB33333),
      /* 47 */
      ALU_ADD(_R0,_x, _R1,_x, ALU_SRC_PV,_z)
      ALU_LAST,
      /* 48 */
      ALU_ADD(__,_z, ALU_SRC_PV,_x, _R127,_x)
      ALU_LAST,
      /* 49 */
      ALU_MUL_IEEE(__,_w, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3FB6DB6E),
      /* 50 */
      ALU_MUL_IEEE(_R0,_x, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x),
      ALU_MUL_IEEE(_R0,_y, _R126,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x41A00000),
      /* 51 */
      ALU_FLOOR(__,_x, ALU_SRC_PV,_x),
      ALU_FLOOR(__,_y, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 52 */
      ALU_DOT4(__,_x, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x),
      ALU_DOT4(__,_y, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_y),
      ALU_DOT4(__,_z, ALU_SRC_PV,_y, ALU_SRC_0,_x),
      ALU_DOT4(__,_w, ALU_SRC_LITERAL,_z, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL3(0x414FD639, 0x429C774C, 0x80000000),
      /* 53 */
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 54 */
      ALU_FRACT(__,_z, ALU_SRC_PV,_w)
      ALU_LAST,
      /* 55 */
      ALU_MULADD(_R123,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB, 0x40C90FDB),
      /* 56 */
      ALU_MUL(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 57 */
      ALU_SIN(__,_x, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
      /* 58 */
      ALU_MUL(__,_y, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x472AEE8C),
      /* 59 */
      ALU_FRACT(_R0,_z, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 60 */
      ALU_SETGT_DX10(__,_x, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E0F5C29),
      /* 61 */
      ALU_CNDE_INT(_R1,_z, ALU_SRC_PV,_x, ALU_SRC_0,_x, ALU_SRC_LITERAL,_x),
      ALU_CNDE_INT(_R0,_w, ALU_SRC_PV,_x, _R0,_w, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL(0x3F800000),
      /* 62 */
      ALU_PRED_SETE_INT(__,_x, _R1,_z, ALU_SRC_0,_x) UPDATE_EXEC_MASK(DEACTIVATE) UPDATE_PRED
      ALU_LAST,
   },
   {
      /* 63 */
      ALU_FRACT(_R127,_x, _R0,_x),
      ALU_MOV_x2(__,_y, _R0,_z),
      ALU_FRACT(_R127,_z, _R0,_y),
      ALU_MOV(__,_w, _R0,_z)
      ALU_LAST,
      /* 64 */
      ALU_FRACT(__,_x, ALU_SRC_PV,_y),
      ALU_ADD(__,_y, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x40000000),
      /* 65 */
      ALU_MULADD(_R127,_y, ALU_SRC_PV _NEG,_y, ALU_SRC_LITERAL,_x, _R127,_z),
      ALU_ADD(__,_z, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL2(0x3E800000, 0x40000000),
      /* 66 */
      ALU_MULADD(_R123,_z, ALU_SRC_PV _NEG,_z, ALU_SRC_LITERAL,_x, _R127,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E800000),
      /* 67 */
      ALU_MUL(__,_x, ALU_SRC_PV,_z, ALU_SRC_PV,_z)
      ALU_LAST,
      /* 68 */
      ALU_MULADD(_R123,_w, _R127,_y, _R127,_y, ALU_SRC_PV,_x)
      ALU_LAST,
      /* 69 */
      ALU_SQRT_IEEE(__,_x, ALU_SRC_PV,_w) SCL_210
      ALU_LAST,
      /* 70 */
      ALU_MUL(__,_y, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x) CLAMP
      ALU_LAST,
      ALU_LITERAL(0x4136DB6E),
      /* 71 */
      ALU_MUL(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3FC90FDB),
      /* 72 */
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 73 */
      ALU_FRACT(__,_z, ALU_SRC_PV,_w)
      ALU_LAST,
      /* 74 */
      ALU_MULADD(_R123,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB, 0x40C90FDB),
      /* 75 */
      ALU_MUL(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 76 */
      ALU_COS_D2(_R0,_w, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
   },
   {
      /* 77 */
      ALU_MUL(__,_x, _R2,_z, ALU_SRC_LITERAL,_x),
      ALU_ADD(__,_y, _R1,_w, ALU_SRC_LITERAL,_y),
      ALU_MULADD(_R2,_x, _R0,_w, ALU_SRC_LITERAL,_z, _R2,_x) VEC_021
      ALU_LAST,
      ALU_LITERAL3(0x3F19999A, 0x3FD55555, 0x3F59999A),
      /* 78 */
      ALU_MOV_x2(_R127,_x, ALU_SRC_PV _NEG,_x),
      ALU_MOV_x4(__,_y, ALU_SRC_PV,_x),
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 79 */
      ALU_FRACT(__,_x, ALU_SRC_PV,_w),
      ALU_ADD(__,_y, _R1,_y, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 80 */
      ALU_MUL_IEEE(_R126,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R123,_z, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_z, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL3(0x3FD55555, 0xC0490FDB, 0x40C90FDB),
      /* 81 */
      ALU_MUL(__,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 82 */
      ALU_COS(__,_x, ALU_SRC_PV,_y) SCL_210
      ALU_LAST,
      /* 83 */
      ALU_MUL_IEEE_x4(__,_z, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3D99999A),
      /* 84 */
      ALU_ADD(_R0,_x, _R1,_x, ALU_SRC_PV,_z)
      ALU_LAST,
      /* 85 */
      ALU_ADD(__,_z, ALU_SRC_PV,_x, _R127,_x)
      ALU_LAST,
      /* 86 */
      ALU_MUL_IEEE(__,_w, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3FD55555),
      /* 87 */
      ALU_MUL_IEEE(_R0,_x, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x),
      ALU_MUL_IEEE(_R0,_y, _R126,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x41A00000),
      /* 88 */
      ALU_FLOOR(__,_x, ALU_SRC_PV,_x),
      ALU_FLOOR(__,_y, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 89 */
      ALU_DOT4(__,_x, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x),
      ALU_DOT4(__,_y, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_y),
      ALU_DOT4(__,_z, ALU_SRC_PV,_y, ALU_SRC_0,_x),
      ALU_DOT4(__,_w, ALU_SRC_LITERAL,_z, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL3(0x414FD639, 0x429C774C, 0x80000000),
      /* 90 */
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 91 */
      ALU_FRACT(__,_z, ALU_SRC_PV,_w)
      ALU_LAST,
      /* 92 */
      ALU_MULADD(_R123,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB, 0x40C90FDB),
      /* 93 */
      ALU_MUL(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 94 */
      ALU_SIN(__,_x, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
      /* 95 */
      ALU_MUL(__,_y, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x472AEE8C),
      /* 96 */
      ALU_FRACT(_R0,_z, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 97 */
      ALU_SETGT_DX10(__,_x, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E0F5C29),
      /* 98 */
      ALU_CNDE_INT(_R1,_z, ALU_SRC_PV,_x, ALU_SRC_0,_x, ALU_SRC_LITERAL,_x),
      ALU_CNDE_INT(_R0,_w, ALU_SRC_PV,_x, _R0,_w, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL(0x3F800000),
      /* 99 */
      ALU_PRED_SETE_INT(__,_x, _R1,_z, ALU_SRC_0,_x) UPDATE_EXEC_MASK(DEACTIVATE) UPDATE_PRED
      ALU_LAST,
   },
   {
      /* 100 */
      ALU_FRACT(_R127,_x, _R0,_x),
      ALU_MOV_x2(__,_y, _R0,_z),
      ALU_FRACT(_R127,_z, _R0,_y),
      ALU_MOV(__,_w, _R0,_z)
      ALU_LAST,
      /* 101 */
      ALU_FRACT(__,_x, ALU_SRC_PV,_y),
      ALU_ADD(__,_y, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x40000000),
      /* 102 */
      ALU_MULADD(_R127,_y, ALU_SRC_PV _NEG,_y, ALU_SRC_LITERAL,_x, _R127,_z),
      ALU_ADD(__,_z, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL2(0x3E800000, 0x40000000),
      /* 103 */
      ALU_MULADD(_R123,_z, ALU_SRC_PV _NEG,_z, ALU_SRC_LITERAL,_x, _R127,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E800000),
      /* 104 */
      ALU_MUL(__,_x, ALU_SRC_PV,_z, ALU_SRC_PV,_z)
      ALU_LAST,
      /* 105 */
      ALU_MULADD(_R123,_w, _R127,_y, _R127,_y, ALU_SRC_PV,_x)
      ALU_LAST,
      /* 106 */
      ALU_SQRT_IEEE(__,_x, ALU_SRC_PV,_w) SCL_210
      ALU_LAST,
      /* 107 */
      ALU_MUL(__,_y, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x) CLAMP
      ALU_LAST,
      ALU_LITERAL(0x4136DB6E),
      /* 108 */
      ALU_MUL(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3FC90FDB),
      /* 109 */
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 110 */
      ALU_FRACT(__,_z, ALU_SRC_PV,_w)
      ALU_LAST,
      /* 111 */
      ALU_MULADD(_R123,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB, 0x40C90FDB),
      /* 112 */
      ALU_MUL(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 113 */
      ALU_COS_D2(_R0,_w, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
   },
   {
      /* 114 */
      ALU_MOV_D2(__,_x, _R2,_z),
      ALU_ADD(__,_y, _R1,_w, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R2,_x, _R0,_w, ALU_SRC_LITERAL,_y, _R2,_x) VEC_021
      ALU_LAST,
      ALU_LITERAL2(0x40000000, 0x3F4CCCCD),
      /* 115 */
      ALU_MOV_x2(_R127,_x, ALU_SRC_PV _NEG,_x),
      ALU_MUL_x4(__,_y, ALU_SRC_PV,_x, ALU_SRC_1,_x),
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 116 */
      ALU_FRACT(__,_x, ALU_SRC_PV,_w),
      ALU_ADD(__,_y, _R1,_y, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 117 */
      ALU_MUL_IEEE(_R126,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R123,_z, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_z, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL3(0x40000000, 0xC0490FDB, 0x40C90FDB),
      /* 118 */
      ALU_MUL(__,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 119 */
      ALU_COS(__,_x, ALU_SRC_PV,_y) SCL_210
      ALU_LAST,
      /* 120 */
      ALU_MUL_IEEE_x4(__,_z, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3D800000),
      /* 121 */
      ALU_ADD(_R0,_x, _R1,_x, ALU_SRC_PV,_z)
      ALU_LAST,
      /* 122 */
      ALU_ADD(__,_z, ALU_SRC_PV,_x, _R127,_x)
      ALU_LAST,
      /* 123 */
      ALU_MUL_IEEE(__,_w, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x40000000),
      /* 124 */
      ALU_MUL_IEEE(_R0,_x, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x),
      ALU_MUL_IEEE(_R0,_y, _R126,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x41A00000),
      /* 125 */
      ALU_FLOOR(__,_x, ALU_SRC_PV,_x),
      ALU_FLOOR(__,_y, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 126 */
      ALU_DOT4(__,_x, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x),
      ALU_DOT4(__,_y, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_y),
      ALU_DOT4(__,_z, ALU_SRC_PV,_y, ALU_SRC_0,_x),
      ALU_DOT4(__,_w, ALU_SRC_LITERAL,_z, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL3(0x414FD639, 0x429C774C, 0x80000000),
      /* 127 */
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 128 */
      ALU_FRACT(__,_z, ALU_SRC_PV,_w)
      ALU_LAST,
      /* 129 */
      ALU_MULADD(_R123,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB, 0x40C90FDB),
      /* 130 */
      ALU_MUL(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 131 */
      ALU_SIN(__,_x, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
      /* 132 */
      ALU_MUL(__,_y, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x472AEE8C),
      /* 133 */
      ALU_FRACT(_R0,_z, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 134 */
      ALU_SETGT_DX10(__,_x, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E0F5C29),
      /* 135 */
      ALU_CNDE_INT(_R1,_z, ALU_SRC_PV,_x, ALU_SRC_0,_x, ALU_SRC_LITERAL,_x),
      ALU_CNDE_INT(_R0,_w, ALU_SRC_PV,_x, _R0,_w, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL(0x3F800000),
      /* 136 */
      ALU_PRED_SETE_INT(__,_x, _R1,_z, ALU_SRC_0,_x) UPDATE_EXEC_MASK(DEACTIVATE) UPDATE_PRED
      ALU_LAST,
   },
   {
      /* 137 */
      ALU_FRACT(_R127,_x, _R0,_x),
      ALU_MOV_x2(__,_y, _R0,_z),
      ALU_FRACT(_R127,_z, _R0,_y),
      ALU_MOV(__,_w, _R0,_z)
      ALU_LAST,
      /* 138 */
      ALU_FRACT(__,_x, ALU_SRC_PV,_y),
      ALU_ADD(__,_y, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x40000000),
      /* 139 */
      ALU_MULADD(_R127,_y, ALU_SRC_PV _NEG,_y, ALU_SRC_LITERAL,_x, _R127,_z),
      ALU_ADD(__,_z, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL2(0x3E800000, 0x40000000),
      /* 140 */
      ALU_MULADD(_R123,_z, ALU_SRC_PV _NEG,_z, ALU_SRC_LITERAL,_x, _R127,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E800000),
      /* 141 */
      ALU_MUL(__,_x, ALU_SRC_PV,_z, ALU_SRC_PV,_z)
      ALU_LAST,
      /* 142 */
      ALU_MULADD(_R123,_w, _R127,_y, _R127,_y, ALU_SRC_PV,_x)
      ALU_LAST,
      /* 143 */
      ALU_SQRT_IEEE(__,_x, ALU_SRC_PV,_w) SCL_210
      ALU_LAST,
      /* 144 */
      ALU_MUL(__,_y, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x) CLAMP
      ALU_LAST,
      ALU_LITERAL(0x4136DB6E),
      /* 145 */
      ALU_MUL(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3FC90FDB),
      /* 146 */
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 147 */
      ALU_FRACT(__,_z, ALU_SRC_PV,_w)
      ALU_LAST,
      /* 148 */
      ALU_MULADD(_R123,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB, 0x40C90FDB),
      /* 149 */
      ALU_MUL(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 150 */
      ALU_COS_D2(_R0,_w, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
   },
   {
      /* 151 */
      ALU_MUL(__,_x, _R2,_z, ALU_SRC_LITERAL,_x),
      ALU_ADD(__,_y, _R1,_w, ALU_SRC_LITERAL,_y),
      ALU_MULADD(_R2,_x, _R0,_w, ALU_SRC_LITERAL,_z, _R2,_x) VEC_021
      ALU_LAST,
      ALU_LITERAL3(0x3ECCCCCD, 0x40200000, 0x3F400000),
      /* 152 */
      ALU_MOV_x2(_R127,_x, ALU_SRC_PV _NEG,_x),
      ALU_MOV_x4(__,_y, ALU_SRC_PV,_x),
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 153 */
      ALU_FRACT(__,_x, ALU_SRC_PV,_w),
      ALU_ADD(__,_y, _R1,_y, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 154 */
      ALU_MUL_IEEE(_R126,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R123,_z, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_z, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL3(0x40200000, 0xC0490FDB, 0x40C90FDB),
      /* 155 */
      ALU_MUL(__,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 156 */
      ALU_COS(__,_x, ALU_SRC_PV,_y) SCL_210
      ALU_LAST,
      /* 157 */
      ALU_MUL_IEEE_x4(__,_z, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3D4CCCCD),
      /* 158 */
      ALU_ADD(_R0,_x, _R1,_x, ALU_SRC_PV,_z)
      ALU_LAST,
      /* 159 */
      ALU_ADD(__,_z, ALU_SRC_PV,_x, _R127,_x)
      ALU_LAST,
      /* 160 */
      ALU_MUL_IEEE(__,_w, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x40200000),
      /* 161 */
      ALU_MUL_IEEE(_R0,_x, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x),
      ALU_MUL_IEEE(_R0,_y, _R126,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x41A00000),
      /* 162 */
      ALU_FLOOR(__,_x, ALU_SRC_PV,_x),
      ALU_FLOOR(__,_y, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 163 */
      ALU_DOT4(__,_x, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x),
      ALU_DOT4(__,_y, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_y),
      ALU_DOT4(__,_z, ALU_SRC_PV,_y, ALU_SRC_0,_x),
      ALU_DOT4(__,_w, ALU_SRC_LITERAL,_z, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL3(0x414FD639, 0x429C774C, 0x80000000),
      /* 164 */
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 165 */
      ALU_FRACT(__,_z, ALU_SRC_PV,_w)
      ALU_LAST,
      /* 166 */
      ALU_MULADD(_R123,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB, 0x40C90FDB),
      /* 167 */
      ALU_MUL(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 168 */
      ALU_SIN(__,_x, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
      /* 169 */
      ALU_MUL(__,_y, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x472AEE8C),
      /* 170 */
      ALU_FRACT(_R0,_z, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 171 */
      ALU_SETGT_DX10(__,_x, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E0F5C29),
      /* 172 */
      ALU_CNDE_INT(_R1,_z, ALU_SRC_PV,_x, ALU_SRC_0,_x, ALU_SRC_LITERAL,_x),
      ALU_CNDE_INT(_R0,_w, ALU_SRC_PV,_x, _R0,_w, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL(0x3F800000),
      /* 173 */
      ALU_PRED_SETE_INT(__,_x, _R1,_z, ALU_SRC_0,_x) UPDATE_EXEC_MASK(DEACTIVATE) UPDATE_PRED
      ALU_LAST,
   },
   {
      /* 174 */
      ALU_FRACT(_R127,_x, _R0,_x),
      ALU_MOV_x2(__,_y, _R0,_z),
      ALU_FRACT(_R127,_z, _R0,_y),
      ALU_MOV(__,_w, _R0,_z)
      ALU_LAST,
      /* 175 */
      ALU_FRACT(__,_x, ALU_SRC_PV,_y),
      ALU_ADD(__,_y, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x40000000),
      /* 176 */
      ALU_MULADD(_R127,_y, ALU_SRC_PV _NEG,_y, ALU_SRC_LITERAL,_x, _R127,_z),
      ALU_ADD(__,_z, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL2(0x3E800000, 0x40000000),
      /* 177 */
      ALU_MULADD(_R123,_z, ALU_SRC_PV _NEG,_z, ALU_SRC_LITERAL,_x, _R127,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E800000),
      /* 178 */
      ALU_MUL(__,_x, ALU_SRC_PV,_z, ALU_SRC_PV,_z)
      ALU_LAST,
      /* 179 */
      ALU_MULADD(_R123,_w, _R127,_y, _R127,_y, ALU_SRC_PV,_x)
      ALU_LAST,
      /* 180 */
      ALU_SQRT_IEEE(__,_x, ALU_SRC_PV,_w) SCL_210
      ALU_LAST,
      /* 181 */
      ALU_MUL(__,_y, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x) CLAMP
      ALU_LAST,
      ALU_LITERAL(0x4136DB6E),
      /* 182 */
      ALU_MUL(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3FC90FDB),
      /* 183 */
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 184 */
      ALU_FRACT(__,_z, ALU_SRC_PV,_w)
      ALU_LAST,
      /* 185 */
      ALU_MULADD(_R123,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB, 0x40C90FDB),
      /* 186 */
      ALU_MUL(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 187 */
      ALU_COS_D2(_R0,_w, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
   },
   {
      /* 188 */
      ALU_MUL(__,_x, _R2,_z, ALU_SRC_LITERAL,_x),
      ALU_ADD(__,_y, _R1,_w, ALU_SRC_LITERAL,_y),
      ALU_MULADD(_R2,_x, _R0,_w, ALU_SRC_LITERAL,_z, _R2,_x) VEC_021
      ALU_LAST,
      ALU_LITERAL3(0x3E99999A, 0x40555555, 0x3F333333),
      /* 189 */
      ALU_MOV_x2(_R127,_x, ALU_SRC_PV _NEG,_x),
      ALU_MOV_x4(__,_y, ALU_SRC_PV,_x),
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 190 */
      ALU_FRACT(__,_x, ALU_SRC_PV,_w),
      ALU_ADD(__,_y, _R1,_y, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 191 */
      ALU_MUL_IEEE(_R126,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R123,_z, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_z, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL3(0x40555555, 0xC0490FDB, 0x40C90FDB),
      /* 192 */
      ALU_MUL(__,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 193 */
      ALU_COS(__,_x, ALU_SRC_PV,_y) SCL_210
      ALU_LAST,
      /* 194 */
      ALU_MUL_IEEE_x4(__,_z, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3D19999A),
      /* 195 */
      ALU_ADD(_R0,_x, _R1,_x, ALU_SRC_PV,_z)
      ALU_LAST,
      /* 196 */
      ALU_ADD(__,_z, ALU_SRC_PV,_x, _R127,_x)
      ALU_LAST,
      /* 197 */
      ALU_MUL_IEEE(__,_w, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x40555555),
      /* 198 */
      ALU_MUL_IEEE(_R0,_x, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x),
      ALU_MUL_IEEE(_R0,_y, _R126,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x41A00000),
      /* 199 */
      ALU_FLOOR(__,_x, ALU_SRC_PV,_x),
      ALU_FLOOR(__,_y, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 200 */
      ALU_DOT4(__,_x, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x),
      ALU_DOT4(__,_y, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_y),
      ALU_DOT4(__,_z, ALU_SRC_PV,_y, ALU_SRC_0,_x),
      ALU_DOT4(__,_w, ALU_SRC_LITERAL,_z, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL3(0x414FD639, 0x429C774C, 0x80000000),
      /* 201 */
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 202 */
      ALU_FRACT(__,_z, ALU_SRC_PV,_w)
      ALU_LAST,
      /* 203 */
      ALU_MULADD(_R123,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB, 0x40C90FDB),
      /* 204 */
      ALU_MUL(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 205 */
      ALU_SIN(__,_x, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
      /* 206 */
      ALU_MUL(__,_y, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x472AEE8C),
      /* 207 */
      ALU_FRACT(_R0,_z, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 208 */
      ALU_SETGT_DX10(__,_x, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E0F5C29),
      /* 209 */
      ALU_CNDE_INT(_R1,_z, ALU_SRC_PV,_x, ALU_SRC_0,_x, ALU_SRC_LITERAL,_x),
      ALU_CNDE_INT(_R0,_w, ALU_SRC_PV,_x, _R0,_w, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL(0x3F800000),
      /* 210 */
      ALU_PRED_SETE_INT(__,_x, _R1,_z, ALU_SRC_0,_x) UPDATE_EXEC_MASK(DEACTIVATE) UPDATE_PRED
      ALU_LAST,
   },
   {
      /* 211 */
      ALU_FRACT(_R127,_x, _R0,_x),
      ALU_MOV_x2(__,_y, _R0,_z),
      ALU_FRACT(_R127,_z, _R0,_y),
      ALU_MOV(__,_w, _R0,_z)
      ALU_LAST,
      /* 212 */
      ALU_FRACT(__,_x, ALU_SRC_PV,_y),
      ALU_ADD(__,_y, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x40000000),
      /* 213 */
      ALU_MULADD(_R127,_y, ALU_SRC_PV _NEG,_y, ALU_SRC_LITERAL,_x, _R127,_z),
      ALU_ADD(__,_z, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL2(0x3E800000, 0x40000000),
      /* 214 */
      ALU_MULADD(_R123,_z, ALU_SRC_PV _NEG,_z, ALU_SRC_LITERAL,_x, _R127,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E800000),
      /* 215 */
      ALU_MUL(__,_x, ALU_SRC_PV,_z, ALU_SRC_PV,_z)
      ALU_LAST,
      /* 216 */
      ALU_MULADD(_R123,_w, _R127,_y, _R127,_y, ALU_SRC_PV,_x)
      ALU_LAST,
      /* 217 */
      ALU_SQRT_IEEE(__,_x, ALU_SRC_PV,_w) SCL_210
      ALU_LAST,
      /* 218 */
      ALU_MUL(__,_y, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x) CLAMP
      ALU_LAST,
      ALU_LITERAL(0x4136DB6E),
      /* 219 */
      ALU_MUL(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3FC90FDB),
      /* 220 */
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 221 */
      ALU_FRACT(__,_z, ALU_SRC_PV,_w)
      ALU_LAST,
      /* 222 */
      ALU_MULADD(_R123,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB, 0x40C90FDB),
      /* 223 */
      ALU_MUL(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 224 */
      ALU_COS_D2(_R0,_w, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
   },
   {
      /* 225 */
      ALU_MUL(__,_x, _R2,_z, ALU_SRC_LITERAL,_x),
      ALU_ADD(__,_y, _R1,_w, ALU_SRC_LITERAL,_y),
      ALU_MULADD(_R2,_x, _R0,_w, ALU_SRC_LITERAL,_z, _R2,_x) VEC_021
      ALU_LAST,
      ALU_LITERAL3(0x3E800000, 0x40800000, 0x3F266666),
      /* 226 */
      ALU_MOV_x2(_R127,_x, ALU_SRC_PV _NEG,_x),
      ALU_MOV_x4(__,_y, ALU_SRC_PV,_x),
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 227 */
      ALU_FRACT(__,_x, ALU_SRC_PV,_w),
      ALU_ADD(__,_y, _R1,_y, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 228 */
      ALU_MUL_IEEE(_R126,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R123,_z, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_z, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL3(0x40800000, 0xC0490FDB, 0x40C90FDB),
      /* 229 */
      ALU_MUL(__,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 230 */
      ALU_COS(__,_x, ALU_SRC_PV,_y) SCL_210
      ALU_LAST,
      /* 231 */
      ALU_MUL_IEEE_x4(__,_z, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3D000000),
      /* 232 */
      ALU_ADD(_R0,_x, _R1,_x, ALU_SRC_PV,_z)
      ALU_LAST,
      /* 233 */
      ALU_ADD(__,_z, ALU_SRC_PV,_x, _R127,_x)
      ALU_LAST,
      /* 234 */
      ALU_MUL_IEEE(__,_w, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x40800000),
      /* 235 */
      ALU_MUL_IEEE(_R0,_x, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x),
      ALU_MUL_IEEE(_R0,_y, _R126,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x41A00000),
      /* 236 */
      ALU_FLOOR(__,_x, ALU_SRC_PV,_x),
      ALU_FLOOR(__,_y, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 237 */
      ALU_DOT4(__,_x, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x),
      ALU_DOT4(__,_y, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_y),
      ALU_DOT4(__,_z, ALU_SRC_PV,_y, ALU_SRC_0,_x),
      ALU_DOT4(__,_w, ALU_SRC_LITERAL,_z, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL3(0x414FD639, 0x429C774C, 0x80000000),
      /* 238 */
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 239 */
      ALU_FRACT(__,_z, ALU_SRC_PV,_w)
      ALU_LAST,
      /* 240 */
      ALU_MULADD(_R123,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB, 0x40C90FDB),
      /* 241 */
      ALU_MUL(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 242 */
      ALU_SIN(__,_x, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
      /* 243 */
      ALU_MUL(__,_y, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x472AEE8C),
      /* 244 */
      ALU_FRACT(_R0,_z, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 245 */
      ALU_SETGT_DX10(__,_x, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E0F5C29),
      /* 246 */
      ALU_CNDE_INT(_R1,_z, ALU_SRC_PV,_x, ALU_SRC_0,_x, ALU_SRC_LITERAL,_x),
      ALU_CNDE_INT(_R0,_w, ALU_SRC_PV,_x, _R0,_w, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL(0x3F800000),
      /* 247 */
      ALU_PRED_SETE_INT(__,_x, _R1,_z, ALU_SRC_0,_x) UPDATE_EXEC_MASK(DEACTIVATE) UPDATE_PRED
      ALU_LAST,
   },
   {
      /* 248 */
      ALU_FRACT(_R127,_x, _R0,_x),
      ALU_MOV_x2(__,_y, _R0,_z),
      ALU_FRACT(_R127,_z, _R0,_y),
      ALU_MOV(__,_w, _R0,_z)
      ALU_LAST,
      /* 249 */
      ALU_FRACT(__,_x, ALU_SRC_PV,_y),
      ALU_ADD(__,_y, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x40000000),
      /* 250 */
      ALU_MULADD(_R127,_y, ALU_SRC_PV _NEG,_y, ALU_SRC_LITERAL,_x, _R127,_z),
      ALU_ADD(__,_z, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL2(0x3E800000, 0x40000000),
      /* 251 */
      ALU_MULADD(_R123,_z, ALU_SRC_PV _NEG,_z, ALU_SRC_LITERAL,_x, _R127,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E800000),
      /* 252 */
      ALU_MUL(__,_x, ALU_SRC_PV,_z, ALU_SRC_PV,_z)
      ALU_LAST,
      /* 253 */
      ALU_MULADD(_R123,_w, _R127,_y, _R127,_y, ALU_SRC_PV,_x)
      ALU_LAST,
      /* 254 */
      ALU_SQRT_IEEE(__,_x, ALU_SRC_PV,_w) SCL_210
      ALU_LAST,
      /* 255 */
      ALU_MUL(__,_y, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x) CLAMP
      ALU_LAST,
      ALU_LITERAL(0x4136DB6E),
      /* 256 */
      ALU_MUL(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3FC90FDB),
      /* 257 */
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 258 */
      ALU_FRACT(__,_z, ALU_SRC_PV,_w)
      ALU_LAST,
      /* 259 */
      ALU_MULADD(_R123,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB, 0x40C90FDB),
      /* 260 */
      ALU_MUL(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 261 */
      ALU_COS_D2(_R0,_w, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
   },
   {
      /* 262 */
      ALU_MUL(__,_x, _R2,_z, ALU_SRC_LITERAL,_x),
      ALU_ADD(__,_y, _R1,_w, ALU_SRC_LITERAL,_y),
      ALU_MULADD(_R0,_x, _R0,_w, ALU_SRC_LITERAL,_z, _R2,_x) VEC_021
      ALU_LAST,
      ALU_LITERAL3(0x3E000000, 0x41000000, 0x3F200000),
      /* 263 */
      ALU_MOV_x2(_R127,_x, ALU_SRC_PV _NEG,_x),
      ALU_MOV_x4(__,_y, ALU_SRC_PV,_x),
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 264 */
      ALU_FRACT(__,_x, ALU_SRC_PV,_w),
      ALU_ADD(__,_y, _R1,_y, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 265 */
      ALU_MUL_IEEE(_R126,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R123,_z, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_z, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL3(0x41000000, 0xC0490FDB, 0x40C90FDB),
      /* 266 */
      ALU_MUL(__,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 267 */
      ALU_COS(__,_x, ALU_SRC_PV,_y) SCL_210
      ALU_LAST,
      /* 268 */
      ALU_MUL_IEEE_x4(__,_z, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3C800000),
      /* 269 */
      ALU_ADD(_R1,_x, _R1,_x, ALU_SRC_PV,_z)
      ALU_LAST,
      /* 270 */
      ALU_ADD(__,_z, ALU_SRC_PV,_x, _R127,_x)
      ALU_LAST,
      /* 271 */
      ALU_MUL_IEEE(__,_w, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x41000000),
      /* 272 */
      ALU_MUL_IEEE(_R1,_x, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x),
      ALU_MUL_IEEE(_R0,_y, _R126,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x41A00000),
      /* 273 */
      ALU_FLOOR(__,_x, ALU_SRC_PV,_x),
      ALU_FLOOR(__,_y, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 274 */
      ALU_DOT4(__,_x, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x),
      ALU_DOT4(__,_y, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_y),
      ALU_DOT4(__,_z, ALU_SRC_PV,_y, ALU_SRC_0,_x),
      ALU_DOT4(__,_w, ALU_SRC_LITERAL,_z, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL3(0x414FD639, 0x429C774C, 0x80000000),
      /* 275 */
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 276 */
      ALU_FRACT(__,_z, ALU_SRC_PV,_w)
      ALU_LAST,
      /* 277 */
      ALU_MULADD(_R123,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB, 0x40C90FDB),
      /* 278 */
      ALU_MUL(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 279 */
      ALU_SIN(__,_x, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
      /* 280 */
      ALU_MUL(__,_y, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x472AEE8C),
      /* 281 */
      ALU_FRACT(_R0,_z, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 282 */
      ALU_SETGT_DX10(__,_x, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E0F5C29),
      /* 283 */
      ALU_CNDE_INT(_R1,_z, ALU_SRC_PV,_x, ALU_SRC_0,_x, ALU_SRC_LITERAL,_x),
      ALU_CNDE_INT(_R0,_w, ALU_SRC_PV,_x, _R0,_w, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL(0x3F800000),
      /* 284 */
      ALU_PRED_SETE_INT(__,_x, _R1,_z, ALU_SRC_0,_x) UPDATE_EXEC_MASK(DEACTIVATE) UPDATE_PRED
      ALU_LAST,
   },
   {
      /* 285 */
      ALU_FRACT(_R127,_x, _R1,_x),
      ALU_MOV_x2(__,_y, _R0,_z),
      ALU_FRACT(_R127,_z, _R0,_y),
      ALU_MOV(__,_w, _R0,_z)
      ALU_LAST,
      /* 286 */
      ALU_FRACT(__,_x, ALU_SRC_PV,_y),
      ALU_ADD(__,_y, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x40000000),
      /* 287 */
      ALU_MULADD(_R127,_y, ALU_SRC_PV _NEG,_y, ALU_SRC_LITERAL,_x, _R127,_z),
      ALU_ADD(__,_z, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL2(0x3E800000, 0x40000000),
      /* 288 */
      ALU_MULADD(_R123,_z, ALU_SRC_PV _NEG,_z, ALU_SRC_LITERAL,_x, _R127,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E800000),
      /* 289 */
      ALU_MUL(__,_x, ALU_SRC_PV,_z, ALU_SRC_PV,_z)
      ALU_LAST,
      /* 290 */
      ALU_MULADD(_R123,_w, _R127,_y, _R127,_y, ALU_SRC_PV,_x)
      ALU_LAST,
      /* 291 */
      ALU_SQRT_IEEE(__,_x, ALU_SRC_PV,_w) SCL_210
      ALU_LAST,
      /* 292 */
      ALU_MUL(__,_y, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x) CLAMP
      ALU_LAST,
      ALU_LITERAL(0x4136DB6E),
      /* 293 */
      ALU_MUL(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3FC90FDB),
      /* 294 */
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 295 */
      ALU_FRACT(__,_z, ALU_SRC_PV,_w)
      ALU_LAST,
      /* 296 */
      ALU_MULADD(_R123,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB, 0x40C90FDB),
      /* 297 */
      ALU_MUL(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 298 */
      ALU_COS_D2(_R0,_w, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
   },
   {
      /* 299 */
      ALU_MULADD(_R127,_x, _R0,_w, ALU_SRC_LITERAL,_x, _R0,_x),
      ALU_MOV(_R0,_y, ALU_SRC_LITERAL,_y),
      ALU_MOV_x4(__,_z, _R1,_y)
      ALU_LAST,
      ALU_LITERAL2(0x3F100000, 0x3F800000),
      /* 300 */
      ALU_MIN(__,_y, ALU_SRC_PV,_z, ALU_SRC_1,_x)
      ALU_LAST,
      /* 301 */
      ALU_MUL(_R0,_w, _R127,_x, ALU_SRC_PV,_y)
      ALU_LAST,
   },
};

static GX2AttribVar attributes[] =
{
   { "Position",  GX2_SHADER_VAR_TYPE_FLOAT4, 0, 0},
   { "TexCoord",  GX2_SHADER_VAR_TYPE_FLOAT2, 0, 1},
};

static GX2AttribStream attribute_stream[] =
{
   {0, 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32,
    GX2_ATTRIB_INDEX_PER_VERTEX, 0, GX2_COMP_SEL(_x, _y, _0, _1), GX2_ENDIAN_SWAP_DEFAULT},
   {1, 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32,
    GX2_ATTRIB_INDEX_PER_VERTEX, 0, GX2_COMP_SEL(_x, _y, _0, _0), GX2_ENDIAN_SWAP_DEFAULT},
};

static GX2SamplerVar samplers[] =
{
   { "Source", GX2_SAMPLER_VAR_TYPE_SAMPLER_2D, 0 },
};

static GX2UniformBlock uniform_blocks[] = {
    {"UBO", 1, 96}
};

static GX2UniformVar uniform_vars[] = {
   {"global.MVP", GX2_SHADER_VAR_TYPE_FLOAT, 1, 0, 0},
   {"global.OutputSize", GX2_SHADER_VAR_TYPE_FLOAT, 1, 16, 0},
   {"global.time", GX2_SHADER_VAR_TYPE_FLOAT, 1, 20, 0},
};

GX2Shader snow_shader =
{
   {
      {
         .sq_pgm_resources_vs.num_gprs = 2,
         .sq_pgm_resources_vs.stack_size = 1,
         .spi_vs_out_config.vs_export_count = 0,
         .num_spi_vs_out_id = 1,
         {
            {.semantic_0 = 0xFF, .semantic_1 = 0xFF, .semantic_2 = 0xFF, .semantic_3 = 0xFF},
            {.semantic_0 = 0xFF, .semantic_1 = 0xFF, .semantic_2 = 0xFF, .semantic_3 = 0xFF},
            {.semantic_0 = 0xFF, .semantic_1 = 0xFF, .semantic_2 = 0xFF, .semantic_3 = 0xFF},
            {.semantic_0 = 0xFF, .semantic_1 = 0xFF, .semantic_2 = 0xFF, .semantic_3 = 0xFF},
            {.semantic_0 = 0xFF, .semantic_1 = 0xFF, .semantic_2 = 0xFF, .semantic_3 = 0xFF},
            {.semantic_0 = 0xFF, .semantic_1 = 0xFF, .semantic_2 = 0xFF, .semantic_3 = 0xFF},
            {.semantic_0 = 0xFF, .semantic_1 = 0xFF, .semantic_2 = 0xFF, .semantic_3 = 0xFF},
            {.semantic_0 = 0xFF, .semantic_1 = 0xFF, .semantic_2 = 0xFF, .semantic_3 = 0xFF},
            {.semantic_0 = 0xFF, .semantic_1 = 0xFF, .semantic_2 = 0xFF, .semantic_3 = 0xFF},
            {.semantic_0 = 0xFF, .semantic_1 = 0xFF, .semantic_2 = 0xFF, .semantic_3 = 0xFF},
         },
         .sq_vtx_semantic_clear = ~0x1,
         .num_sq_vtx_semantic = 1,
         {
               0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
         },
         .vgt_vertex_reuse_block_cntl.vtx_reuse_depth = 0xE,
         .vgt_hos_reuse_depth.reuse_depth = 0x10,
      }, /* regs */
      .size = sizeof(vs_program),
      .program = (uint8_t*)&vs_program,
      .mode = GX2_SHADER_MODE_UNIFORM_BLOCK,
      .uniformBlockCount = countof(uniform_blocks), uniform_blocks,
      .uniformVarCount = countof(uniform_vars), uniform_vars,
      .attribVarCount = countof(attributes), attributes,
   },
   {
      {
         .sq_pgm_resources_ps.num_gprs = 3,
         .sq_pgm_resources_ps.stack_size = 1,
         .sq_pgm_exports_ps.export_mode = 0x2,
         .spi_ps_in_control_0.num_interp = 1,
         .spi_ps_in_control_0.position_ena = TRUE,
         .spi_ps_in_control_0.persp_gradient_ena = FALSE,
         .spi_ps_in_control_0.baryc_sample_cntl = spi_baryc_cntl_centers_only,
         .num_spi_ps_input_cntl = 1, {{.semantic = 0, .default_val = 1}},
         .cb_shader_mask.output0_enable = 0xF,
         .cb_shader_control.rt0_enable = TRUE,
         .db_shader_control.z_order = db_z_order_early_z_then_late_z,
      }, /* regs */
      .size = sizeof(ps_program),
      .program = (uint8_t*)&ps_program,
      .mode = GX2_SHADER_MODE_UNIFORM_BLOCK,
      .samplerVarCount = countof(samplers), samplers,
   },
   .attribute_stream = attribute_stream,
};
