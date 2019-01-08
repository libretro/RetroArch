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
   u64 alu[120];
   u64 alu1[19];
   u64 alu2[1];
   u64 alu3[106];
   u64 alu4[19];
   u64 alu5[2];
} ps_program =
{
   {
      ALU_PUSH_BEFORE(32,120) KCACHE0(CB1, _0_15),
      JUMP(0,3) VALID_PIX,
      ALU(152,19),
      ELSE(1, 5) VALID_PIX,
      ALU_POP_AFTER(171,1),
      ALU_PUSH_BEFORE(172,106) KCACHE0(CB1, _0_15),
      JUMP(1, 8) VALID_PIX,
      ALU_POP_AFTER(278,19),
      ALU(297,2),
      EXP_DONE(PIX0, _R0,_x,_x,_x,_x)
      END_OF_PROGRAM
   },
   {
      /* 0 */
      ALU_MOV(_R127,_x, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R124,_y, KC0(5),_x, ALU_SRC_LITERAL,_z, ALU_SRC_LITERAL,_y),
      ALU_MUL(__,_z, KC0(5),_x, ALU_SRC_LITERAL,_y),
      ALU_ADD(_R127,_w, KC0(5),_x, ALU_SRC_1,_x),
      ALU_RECIP_IEEE(__,_x, KC0(4),_x) SCL_210
      ALU_LAST,
      ALU_LITERAL3(0x3F800000, 0x414E4000, 0x3F52C000),
      /* 1 */
      ALU_MUL_IEEE(__,_x, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x),
      ALU_MUL_IEEE_x2(_R126,_y, _R0,_x, ALU_SRC_PS,_x),
      ALU_ADD(__,_z, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_y),
      ALU_MUL_IEEE(__,_w, ALU_SRC_PV,_y, ALU_SRC_0_5,_x),
      ALU_RECIP_IEEE(__,_x, KC0(4),_y) SCL_210
      ALU_LAST,
      ALU_LITERAL2(0x3A83126F, 0xC0000000),
      /* 2 */
      ALU_FLOOR(__,_x, ALU_SRC_PV,_w),
      ALU_MUL_IEEE(_R127,_y, ALU_SRC_PV,_z, ALU_SRC_0_5,_x),
      ALU_MUL_IEEE_x2(_R127,_z, _R0,_y, ALU_SRC_PS,_x),
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x),
      ALU_RECIP_IEEE(_R125,_y, KC0(4),_y) SCL_210
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 3 */
      ALU_DOT4(__,_x, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x),
      ALU_DOT4(__,_y, _R127,_x, ALU_SRC_LITERAL,_y),
      ALU_DOT4(__,_z, _R127,_x, ALU_SRC_0,_x),
      ALU_DOT4(__,_w, ALU_SRC_LITERAL,_z, ALU_SRC_0,_x),
      ALU_FRACT(__,_x, ALU_SRC_PV,_w)
      ALU_LAST,
      ALU_LITERAL3(0x414FD639, 0x429C774C, 0x80000000),
      /* 4 */
      ALU_ADD(_R0,_x, _R126,_y, ALU_SRC_1 _NEG,_x),
      ALU_MULADD(_R123,_y, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R123,_z, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_z, ALU_SRC_0_5,_x),
      ALU_ADD(__,_w, _R127,_y, ALU_SRC_1,_x) VEC_201,
      ALU_MUL_IEEE(_R127,_x, KC0(4),_x, _R125,_y)
      ALU_LAST,
      ALU_LITERAL3(0xC0490FDB, 0x40C90FDB, 0x3E22F983),
      /* 5 */
      ALU_MUL(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x),
      ALU_FRACT(__,_y, ALU_SRC_PV,_z),
      ALU_FLOOR(__,_z, ALU_SRC_PV,_w),
      ALU_MOV(_R1,_w, ALU_SRC_0,_x),
      ALU_ADD(_R0,_y, _R127,_z, ALU_SRC_1 _NEG,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 6 */
      ALU_MUL(_R0,_x, _R0,_x, _R127,_x),
      ALU_MULADD(_R123,_z, ALU_SRC_PV _NEG,_z, ALU_SRC_LITERAL,_x, _R124,_y),
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_z, ALU_SRC_LITERAL,_y),
      ALU_SIN(__,_x, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
      ALU_LITERAL3(0x40000000, 0xC0490FDB, 0x40C90FDB),
      /* 7 */
      ALU_MUL(__,_x, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x),
      ALU_MUL_IEEE(_R127,_z, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_y),
      ALU_ADD(__,_w, ALU_SRC_PV _NEG,_z, ALU_SRC_1,_x),
      ALU_MUL_IEEE(_R0,_z, _R127,_w, ALU_SRC_LITERAL,_z)
      ALU_LAST,
      ALU_LITERAL3(0x3E22F983, 0x3DCCCCCD, 0x3E800000),
      /* 8 */
      ALU_ADD(__,_y, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x),
      ALU_ADD(_R126,_z, _R0 _NEG,_y, ALU_SRC_PV,_w),
      ALU_MOV(_R2,_w, ALU_SRC_1,_x),
      ALU_SIN(__,_y, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
      ALU_LITERAL(0x414E4000),
      /* 9 */
      ALU_SETGT(__,_x, ALU_SRC_0,_x, ALU_SRC_PV,_z),
      ALU_MUL(__,_y, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R123,_z, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_y, ALU_SRC_0_5,_x),
      ALU_SETGT(__,_w, ALU_SRC_PV,_z, ALU_SRC_0,_x),
      ALU_SETE_DX10(_R124,_y, ALU_SRC_PV,_z, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL2(0x472AEE8C, 0x3E22F983),
      /* 10 */
      ALU_FRACT(__,_x, ALU_SRC_PV,_z),
      ALU_ADD(_R125,_z, ALU_SRC_PV,_w, ALU_SRC_PV _NEG,_x),
      ALU_FRACT(__,_w, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 11 */
      ALU_ADD(__,_x, _R127,_z, ALU_SRC_PV,_w),
      ALU_MUL(_R127,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R123,_z, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_z, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL3(0x3FC90FDB, 0xC0490FDB, 0x40C90FDB),
      /* 12 */
      ALU_MUL(__,_x, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R123,_y, ALU_SRC_PV _NEG,_x, ALU_SRC_LITERAL,_z, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL3(0x3E22F983, 0x3FE66666, 0x40666666),
      /* 13 */
      ALU_ADD(__,_x, _R0 _NEG,_x, ALU_SRC_PV,_y),
      ALU_SIN(_R126,_w, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
      /* 14 */
      ALU_MUL(__,_y, ALU_SRC_PV,_x, ALU_SRC_PV,_x),
      ALU_SETE_DX10(_R124,_z, ALU_SRC_PV,_x, ALU_SRC_0,_x),
      ALU_SETGT_DX10(__,_w, ALU_SRC_0,_x, ALU_SRC_PV,_x),
      ALU_RECIP_IEEE(__,_y, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
      /* 15 */
      ALU_MUL(_R126,_x, _R124,_y, ALU_SRC_PV,_w),
      ALU_MUL_IEEE(__,_z, _R126,_z, ALU_SRC_PS,_x),
      ALU_MULADD(_R2,_x, _R126,_z, _R126,_z, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 16 */
      ALU_SETGT(__,_y, ALU_SRC_PV,_z, ALU_SRC_0,_x),
      ALU_MAX_DX10(_R126,_z, ALU_SRC_PV,_z, ALU_SRC_PV _NEG,_z),
      ALU_SETGT(__,_w, ALU_SRC_0,_x, ALU_SRC_PV,_z)
      ALU_LAST,
      /* 17 */
      ALU_ADD(__,_x, ALU_SRC_PV,_y, ALU_SRC_PV _NEG,_w),
      ALU_SETGT_DX10(_R124,_y, ALU_SRC_PV,_z, ALU_SRC_1,_x),
      ALU_RECIP_IEEE(__,_x, ALU_SRC_PV,_z) SCL_210
      ALU_LAST,
      /* 18 */
      ALU_MUL(__,_x, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x),
      ALU_CNDE_INT(_R123,_y, ALU_SRC_PV,_y, _R126,_z, ALU_SRC_PS,_x),
      ALU_CNDE_INT(_R127,_z, ALU_SRC_PV,_y, ALU_SRC_PV,_x, ALU_SRC_PV _NEG,_x)
      ALU_LAST,
      ALU_LITERAL(0x3FC90FDB),
      /* 19 */
      ALU_MIN(__,_x, ALU_SRC_PV,_y, ALU_SRC_1,_x),
      ALU_CNDE_INT(_R127,_w, _R124,_y, ALU_SRC_0,_x, ALU_SRC_PV,_x)
      ALU_LAST,
      /* 20 */
      ALU_MAX(_R126,_z, ALU_SRC_PV,_x, ALU_SRC_1 _NEG,_x)
      ALU_LAST,
      /* 21 */
      ALU_MUL(_R124,_y, ALU_SRC_PV,_z, ALU_SRC_PV,_z)
      ALU_LAST,
      /* 22 */
      ALU_MULADD(_R123,_z, ALU_SRC_LITERAL,_y, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R123,_w, ALU_SRC_LITERAL,_z, ALU_SRC_PV,_y, ALU_SRC_1,_x)
      ALU_LAST,
      ALU_LITERAL3(0x3F43B24E, 0x3D6EE04D, 0x3EDCF805),
      /* 23 */
      ALU_MUL(_R127,_x, _R126 _ABS,_z, ALU_SRC_PV,_w),
      ALU_MULADD(_R123,_y, _R124,_y, ALU_SRC_PV,_z, ALU_SRC_1,_x)
      ALU_LAST,
      /* 24 */
      ALU_RECIP_IEEE(__,_x, ALU_SRC_PV,_y) SCL_210
      ALU_LAST,
      /* 25 */
      ALU_MUL(__,_x, _R127,_x, ALU_SRC_PS,_x)
      ALU_LAST,
      /* 26 */
      ALU_CNDGT(_R123,_z, _R126,_z, ALU_SRC_PV,_x, ALU_SRC_PV _NEG,_x)
      ALU_LAST,
      /* 27 */
      ALU_MULADD(_R124,_y, _R127,_z, ALU_SRC_PV,_z, _R127,_w)
      ALU_LAST,
      /* 28 */
      ALU_SETGT(__,_y, ALU_SRC_PV,_y, ALU_SRC_0,_x),
      ALU_SETGT(__,_w, ALU_SRC_0,_x, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 29 */
      ALU_ADD(__,_x, ALU_SRC_PV,_y, ALU_SRC_PV _NEG,_w)
      ALU_LAST,
      /* 30 */
      ALU_ADD(__,_y, _R125 _NEG,_z, ALU_SRC_PV,_x)
      ALU_LAST,
      /* 31 */
      ALU_MULADD(_R123,_w, ALU_SRC_PV _NEG,_y, ALU_SRC_LITERAL,_x, _R124,_y)
      ALU_LAST,
      ALU_LITERAL(0x3FC90FDB),
      /* 32 */
      ALU_CNDE_INT(_R123,_x, _R126,_x, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x40490FDB),
      /* 33 */
      ALU_CNDE_INT(_R123,_z, _R124,_z, ALU_SRC_PV,_x, _R127,_y)
      ALU_LAST,
      /* 34 */
      ALU_MULADD(_R0,_w, _R126,_w, ALU_SRC_LITERAL,_x, ALU_SRC_PV,_z)
      ALU_LAST,
      ALU_LITERAL(0x41200000),
      /* 35 */
      ALU_PRED_SETGT(__,_x, ALU_SRC_LITERAL,_x, _R2,_x) UPDATE_EXEC_MASK(DEACTIVATE) UPDATE_PRED
      ALU_LAST,
      ALU_LITERAL(0x3AD3D70A),
   },
   {
      /* 36 */
      ALU_SETGT_DX10(_R127,_x, ALU_SRC_LITERAL,_x, _R2,_x),
      ALU_MUL(__,_w, _R0,_w, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL2(0x3A53D70A, 0x41000000),
      /* 37 */
      ALU_MULADD(_R123,_z, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 38 */
      ALU_FRACT(__,_y, ALU_SRC_PV,_z)
      ALU_LAST,
      /* 39 */
      ALU_MULADD(_R123,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB, 0x40C90FDB),
      /* 40 */
      ALU_MUL(__,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 41 */
      ALU_SIN(__,_x, ALU_SRC_PV,_w) SCL_210
      ALU_LAST,
      /* 42 */
      ALU_SETGT_DX10(__,_y, ALU_SRC_0,_x, ALU_SRC_PS,_x)
      ALU_LAST,
      /* 43 */
      ALU_CNDE_INT(_R126,_x, ALU_SRC_PV,_y, ALU_SRC_0,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3F800000),
      /* 44 */
      ALU_ADD(__,_z, ALU_SRC_PV,_x, ALU_SRC_1,_x)
      ALU_LAST,
      /* 45 */
      ALU_CNDE_INT(_R123,_y, _R127,_x, _R126,_x, ALU_SRC_PV,_z)
      ALU_LAST,
      /* 46 */
      ALU_MUL(_R1,_x, ALU_SRC_LITERAL,_x, ALU_SRC_PV,_y)
      ALU_LAST,
      ALU_LITERAL(0x3F258000),
      /* 47 */
      ALU_MOV(_R0,_w, _R2,_w)
      ALU_LAST,
   },
   {
      /* 48 */
      ALU_MOV(_R0,_w, _R1,_w)
      ALU_LAST,
   },
   {
      /* 49 */
      ALU_CNDE_INT(_R1,_z, _R0,_w, ALU_SRC_0,_x, _R1,_x)
      ALU_LAST,
      /* 50 */
      ALU_MOV(_R127,_x, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R126,_y, KC0(5),_x, ALU_SRC_LITERAL,_z, ALU_SRC_LITERAL,_y),
      ALU_MUL(__,_z, KC0(5),_x, ALU_SRC_LITERAL,_y),
      ALU_ADD(__,_w, _R0,_z, ALU_SRC_LITERAL,_y),
      ALU_ADD(_R1,_y, ALU_SRC_0,_x, _R1,_z)
      ALU_LAST,
      ALU_LITERAL3(0x3F800000, 0x411F6000, 0x3F1AA000),
      /* 51 */
      ALU_MUL_IEEE(__,_x, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R123,_y, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_y, ALU_SRC_0_5,_x),
      ALU_MUL_IEEE(__,_z, ALU_SRC_PV,_y, ALU_SRC_0_5,_x),
      ALU_ADD(__,_w, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_z)
      ALU_LAST,
      ALU_LITERAL3(0x3A83126F, 0x3E22F983, 0xC0000000),
      /* 52 */
      ALU_FLOOR(__,_x, ALU_SRC_PV,_z),
      ALU_MUL_IEEE(_R127,_y, ALU_SRC_PV,_w, ALU_SRC_0_5,_x),
      ALU_MULADD(_R123,_z, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x),
      ALU_FRACT(_R127,_w, ALU_SRC_PV,_y)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 53 */
      ALU_DOT4(__,_x, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x),
      ALU_DOT4(__,_y, _R127,_x, ALU_SRC_LITERAL,_y),
      ALU_DOT4(__,_z, _R127,_x, ALU_SRC_0,_x),
      ALU_DOT4(__,_w, ALU_SRC_LITERAL,_z, ALU_SRC_0,_x),
      ALU_FRACT(__,_x, ALU_SRC_PV,_z)
      ALU_LAST,
      ALU_LITERAL3(0x414FD639, 0x429C774C, 0x80000000),
      /* 54 */
      ALU_MULADD(_R123,_x, _R127,_w, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R123,_y, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x),
      ALU_ADD(__,_z, _R127,_y, ALU_SRC_1,_x),
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_z, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL3(0xC0490FDB, 0x40C90FDB, 0x3E22F983),
      /* 55 */
      ALU_MUL(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x),
      ALU_FRACT(__,_y, ALU_SRC_PV,_w),
      ALU_FLOOR(__,_z, ALU_SRC_PV,_z),
      ALU_MUL(_R127,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 56 */
      ALU_MULADD(_R123,_z, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R123,_w, ALU_SRC_PV _NEG,_z, ALU_SRC_LITERAL,_z, _R126,_y),
      ALU_SIN(__,_z, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
      ALU_LITERAL3(0xC0490FDB, 0x40C90FDB, 0x40000000),
      /* 57 */
      ALU_MUL(__,_x, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x),
      ALU_ADD(__,_z, ALU_SRC_PV _NEG,_w, ALU_SRC_1,_x),
      ALU_MUL_IEEE(_R127,_w, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_y),
      ALU_SIN(_R126,_z, _R127,_w) SCL_210
      ALU_LAST,
      ALU_LITERAL2(0x3E22F983, 0x3DCCCCCD),
      /* 58 */
      ALU_ADD(_R125,_w, _R0 _NEG,_y, ALU_SRC_PV,_z),
      ALU_SIN(__,_w, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
      /* 59 */
      ALU_SETGT(__,_x, ALU_SRC_0,_x, ALU_SRC_PV,_w),
      ALU_MUL(__,_y, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x),
      ALU_SETGT(__,_z, ALU_SRC_PV,_w, ALU_SRC_0,_x),
      ALU_SETE_DX10(_R126,_w, ALU_SRC_PV,_w, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL(0x472AEE8C),
      /* 60 */
      ALU_FRACT(__,_z, ALU_SRC_PV,_y),
      ALU_ADD(_R124,_w, ALU_SRC_PV,_z, ALU_SRC_PV _NEG,_x)
      ALU_LAST,
      /* 61 */
      ALU_ADD(__,_x, _R127,_w, ALU_SRC_PV,_z),
      ALU_MUL(_R0,_w, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3FC90FDB),
      /* 62 */
      ALU_MULADD(_R123,_y, ALU_SRC_PV _NEG,_x, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0x3FE66666, 0x40666666),
      /* 63 */
      ALU_ADD(__,_x, _R0 _NEG,_x, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 64 */
      ALU_MUL(__,_y, ALU_SRC_PV,_x, ALU_SRC_PV,_x),
      ALU_SETGT_DX10(__,_z, ALU_SRC_0,_x, ALU_SRC_PV,_x),
      ALU_SETE_DX10(_R127,_w, ALU_SRC_PV,_x, ALU_SRC_0,_x),
      ALU_RECIP_IEEE(__,_y, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
      /* 65 */
      ALU_MUL(_R126,_x, _R126,_w, ALU_SRC_PV,_z),
      ALU_MUL_IEEE(__,_w, _R125,_w, ALU_SRC_PS,_x) VEC_120,
      ALU_MULADD(_R0,_x, _R125,_w, _R125,_w, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 66 */
      ALU_SETGT(__,_y, ALU_SRC_PV,_w, ALU_SRC_0,_x),
      ALU_SETGT(__,_z, ALU_SRC_0,_x, ALU_SRC_PV,_w),
      ALU_MAX_DX10(_R126,_w, ALU_SRC_PV,_w, ALU_SRC_PV _NEG,_w)
      ALU_LAST,
      /* 67 */
      ALU_ADD(__,_x, ALU_SRC_PV,_y, ALU_SRC_PV _NEG,_z),
      ALU_SETGT_DX10(_R126,_y, ALU_SRC_PV,_w, ALU_SRC_1,_x),
      ALU_RECIP_IEEE(__,_x, ALU_SRC_PV,_w) SCL_210
      ALU_LAST,
      /* 68 */
      ALU_MUL(__,_x, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x),
      ALU_CNDE_INT(_R123,_y, ALU_SRC_PV,_y, _R126,_w, ALU_SRC_PS,_x),
      ALU_CNDE_INT(_R125,_w, ALU_SRC_PV,_y, ALU_SRC_PV,_x, ALU_SRC_PV _NEG,_x)
      ALU_LAST,
      ALU_LITERAL(0x3FC90FDB),
      /* 69 */
      ALU_MIN(__,_x, ALU_SRC_PV,_y, ALU_SRC_1,_x),
      ALU_CNDE_INT(_R127,_z, _R126,_y, ALU_SRC_0,_x, ALU_SRC_PV,_x)
      ALU_LAST,
      /* 70 */
      ALU_MAX(_R126,_w, ALU_SRC_PV,_x, ALU_SRC_1 _NEG,_x)
      ALU_LAST,
      /* 71 */
      ALU_MUL(_R126,_y, ALU_SRC_PV,_w, ALU_SRC_PV,_w)
      ALU_LAST,
      /* 72 */
      ALU_MULADD(_R123,_z, ALU_SRC_LITERAL,_x, ALU_SRC_PV,_y, ALU_SRC_1,_x),
      ALU_MULADD(_R123,_w, ALU_SRC_LITERAL,_z, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL3(0x3EDCF805, 0x3F43B24E, 0x3D6EE04D),
      /* 73 */
      ALU_MUL(_R127,_x, _R126 _ABS,_w, ALU_SRC_PV,_z),
      ALU_MULADD(_R123,_y, _R126,_y, ALU_SRC_PV,_w, ALU_SRC_1,_x)
      ALU_LAST,
      /* 74 */
      ALU_RECIP_IEEE(__,_x, ALU_SRC_PV,_y) SCL_210
      ALU_LAST,
      /* 75 */
      ALU_MUL(__,_x, _R127,_x, ALU_SRC_PS,_x)
      ALU_LAST,
      /* 76 */
      ALU_CNDGT(_R123,_w, _R126,_w, ALU_SRC_PV,_x, ALU_SRC_PV _NEG,_x)
      ALU_LAST,
      /* 77 */
      ALU_MULADD(_R126,_y, _R125,_w, ALU_SRC_PV,_w, _R127,_z)
      ALU_LAST,
      /* 78 */
      ALU_SETGT(__,_y, ALU_SRC_PV,_y, ALU_SRC_0,_x),
      ALU_SETGT(__,_z, ALU_SRC_0,_x, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 79 */
      ALU_ADD(__,_x, ALU_SRC_PV,_y, ALU_SRC_PV _NEG,_z)
      ALU_LAST,
      /* 80 */
      ALU_ADD(__,_y, _R124 _NEG,_w, ALU_SRC_PV,_x)
      ALU_LAST,
      /* 81 */
      ALU_MULADD(_R123,_z, ALU_SRC_PV _NEG,_y, ALU_SRC_LITERAL,_x, _R126,_y)
      ALU_LAST,
      ALU_LITERAL(0x3FC90FDB),
      /* 82 */
      ALU_CNDE_INT(_R123,_x, _R126,_x, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x40490FDB),
      /* 83 */
      ALU_CNDE_INT(_R123,_w, _R127,_w, ALU_SRC_PV,_x, _R0,_w)
      ALU_LAST,
      /* 84 */
      ALU_MULADD(_R0,_z, _R126,_z, ALU_SRC_LITERAL,_x, ALU_SRC_PV,_w)
      ALU_LAST,
      ALU_LITERAL(0x41200000),
      /* 85 */
      ALU_PRED_SETGT(__,_x, ALU_SRC_LITERAL,_x, _R0,_x) UPDATE_EXEC_MASK(DEACTIVATE) UPDATE_PRED
      ALU_LAST,
      ALU_LITERAL(0x3A0851EB),
   },
   {
      /* 86 */
      ALU_SETGT_DX10(_R127,_x, ALU_SRC_LITERAL,_x, _R0,_x),
      ALU_MUL(__,_w, _R0,_z, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL2(0x398851EB, 0x41000000),
      /* 87 */
      ALU_MULADD(_R123,_y, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 88 */
      ALU_FRACT(__,_x, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 89 */
      ALU_MULADD(_R123,_z, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB, 0x40C90FDB),
      /* 90 */
      ALU_MUL(__,_w, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 91 */
      ALU_SIN(__,_x, ALU_SRC_PV,_w) SCL_210
      ALU_LAST,
      /* 92 */
      ALU_SETGT_DX10(__,_x, ALU_SRC_0,_x, ALU_SRC_PS,_x)
      ALU_LAST,
      /* 93 */
      ALU_CNDE_INT(_R127,_z, ALU_SRC_PV,_x, ALU_SRC_0,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3F800000),
      /* 94 */
      ALU_ADD(__,_y, ALU_SRC_PV,_z, ALU_SRC_1,_x)
      ALU_LAST,
      /* 95 */
      ALU_CNDE_INT(_R123,_x, _R127,_x, _R127,_z, ALU_SRC_PV,_y)
      ALU_LAST,
      /* 96 */
      ALU_MUL(_R1,_z, ALU_SRC_LITERAL,_x, ALU_SRC_PV,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E550000),
      /* 97 */
      ALU_MOV(_R1,_w, _R2,_w)
      ALU_LAST,
   },
   {
      /* 98 */
      ALU_CNDE_INT(_R0,_w, _R1,_w, ALU_SRC_0,_x, _R1,_z)
      ALU_LAST,
      /* 99 */
      ALU_ADD(_R0,_x, _R1,_y, _R0,_w)
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

GX2Shader snowflake_shader =
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
