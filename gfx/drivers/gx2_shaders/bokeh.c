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
   u64 alu[70];
   u64 alu1[19];
   u64 alu2[31];
   u64 alu3[25];
   u64 alu4[31];
   u64 alu5[25];
   u64 alu6[31];
   u64 alu7[25];
   u64 alu8[31];
   u64 alu9[25];
   u64 alu10[31];
   u64 alu11[25];
   u64 alu12[17];
} ps_program =
{
   {
      ALU_PUSH_BEFORE(32,70) KCACHE0(CB1, _0_15),
      JUMP(1, 3) VALID_PIX,
      ALU_POP_AFTER(102,19),
      ALU_PUSH_BEFORE(121,31) KCACHE0(CB1, _0_15),
      JUMP(1, 6) VALID_PIX,
      ALU_POP_AFTER(152,25),
      ALU_PUSH_BEFORE(177,31) KCACHE0(CB1, _0_15),
      JUMP(1, 9) VALID_PIX,
      ALU_POP_AFTER(208,25),
      ALU_PUSH_BEFORE(233,31) KCACHE0(CB1, _0_15),
      JUMP(1, 12) VALID_PIX,
      ALU_POP_AFTER(264,25),
      ALU_PUSH_BEFORE(289,31) KCACHE0(CB1, _0_15),
      JUMP(1, 15) VALID_PIX,
      ALU_POP_AFTER(320,25),
      ALU_PUSH_BEFORE(345,31) KCACHE0(CB1, _0_15),
      JUMP(1, 18) VALID_PIX,
      ALU_POP_AFTER(376,25),
      ALU(401,17),
      EXP_DONE(PIX0, _R0,_x,_y,_z,_w)
      END_OF_PROGRAM
   },
   {
      /* 0 */
      ALU_MOV_x2(__,_x, _R0,_x),
      ALU_MOV_x4(__,_y, KC0(5),_x),
      ALU_MUL(_R127,_z, KC0(4),_x, ALU_SRC_LITERAL,_x),
      ALU_MOV_x2(_R127,_w, _R0,_y),
      ALU_RECIP_IEEE(__,_x, KC0(4),_x) SCL_210
      ALU_LAST,
      ALU_LITERAL(0xBF517A97),
      /* 1 */
      ALU_MUL_IEEE(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x),
      ALU_MUL_IEEE(__,_y, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_y),
      ALU_MUL_IEEE(__,_z, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_z),
      ALU_MUL_IEEE(_R1,_w, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_w),
      ALU_MUL_IEEE(_R127,_x, ALU_SRC_PV,_x, ALU_SRC_PS,_x)
      ALU_LAST,
      ALU_LITERAL4(0x3DE38E39, 0x3E124925, 0x3E2AAAAB, 0x3D888889),
      /* 2 */
      ALU_MULADD(_R123,_x, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x),
      ALU_ADD(__,_y, ALU_SRC_LITERAL,_y, ALU_SRC_PV,_w),
      ALU_MUL(_R2,_z, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_z),
      ALU_MULADD(_R126,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x),
      ALU_RECIP_IEEE(__,_x, KC0(4),_y) SCL_210
      ALU_LAST,
      ALU_LITERAL3(0x3E22F983, 0x3F6BB554, 0x3E99999A),
      /* 3 */
      ALU_MUL_IEEE(_R126,_x, _R127,_w, ALU_SRC_PS,_x),
      ALU_FRACT(_R127,_y, ALU_SRC_PV,_x),
      ALU_ADD(__,_z, ALU_SRC_LITERAL,_x, ALU_SRC_PV,_y),
      ALU_MULADD(_R127,_w, ALU_SRC_LITERAL,_z, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_y),
      ALU_RECIP_IEEE(_R3,_z, KC0(4),_y) SCL_210
      ALU_LAST,
      ALU_LITERAL3(0x343F0981, 0x3F6BB554, 0x3E4CCCD7),
      /* 4 */
      ALU_MULADD(_R123,_x, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x),
      ALU_MUL_IEEE(_R126,_y, ALU_SRC_PS,_x, _R127,_z),
      ALU_FRACT(_R127,_z, _R126,_w),
      ALU_MUL_IEEE(__,_w, KC0(4),_x, ALU_SRC_PS,_x),
      ALU_ADD(_R0,_x, _R127,_x, ALU_SRC_1 _NEG,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 5 */
      ALU_MUL(_R0,_x, ALU_SRC_PS,_x, ALU_SRC_PV,_w),
      ALU_FRACT(__,_y, _R127,_w),
      ALU_MULADD(_R126,_z, _R127,_y, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x),
      ALU_FRACT(__,_w, ALU_SRC_PV,_x),
      ALU_ADD(_R2,_y, _R126,_x, ALU_SRC_1 _NEG,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB, 0x40C90FDB),
      /* 6 */
      ALU_MULADD(_R123,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R123,_y, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_w, ALU_SRC_LITERAL,_z),
      ALU_MOV(_R127,_z, ALU_SRC_0,_x),
      ALU_MULADD(_R123,_w, _R127,_z, ALU_SRC_LITERAL,_w, ALU_SRC_LITERAL,_z),
      ALU_MOV(_R4,_x, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL4(0xBFA64605, 0x40264605, 0xC0490FDB, 0x40C90FDB),
      /* 7 */
      ALU_MUL(_R126,_x, _R126,_z, ALU_SRC_LITERAL,_x),
      ALU_ADD(_R127,_y, _R2,_y, ALU_SRC_PV _NEG,_x),
      ALU_MUL(__,_z, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x),
      ALU_MUL(_R127,_w, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x),
      ALU_MOV(_R4,_y, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 8 */
      ALU_MOV(_R4,_z, ALU_SRC_0,_x),
      ALU_SIN(__,_z, ALU_SRC_PV,_z) SCL_210
      ALU_LAST,
      /* 9 */
      ALU_ADD(__,_z, _R126,_y, ALU_SRC_PS,_x),
      ALU_SIN(_R126,_w, _R126,_x) SCL_210
      ALU_LAST,
      /* 10 */
      ALU_ADD(__,_x, _R0,_x, ALU_SRC_PV _NEG,_z),
      ALU_SIN(_R126,_z, _R127,_w) SCL_210
      ALU_LAST,
      /* 11 */
      ALU_DOT4(__,_x, ALU_SRC_PV,_x, ALU_SRC_PV,_x),
      ALU_DOT4(__,_y, _R127,_y, _R127,_y),
      ALU_DOT4(__,_z, _R127,_z, _R127,_z),
      ALU_DOT4(__,_w, ALU_SRC_LITERAL,_x, ALU_SRC_0,_x),
      ALU_MULADD(_R1,_x, _R126,_w, ALU_SRC_LITERAL,_z, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL3(0x80000000, 0x3E99999A, 0x3E46A7F0),
      /* 12 */
      ALU_MULADD(_R2,_x, _R126,_z, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x),
      ALU_SQRT_IEEE(_R0,_y, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
      ALU_LITERAL2(0x3E99999A, 0x3F8CCCCD),
      /* 13 */
      ALU_PRED_SETGT(__,_x, ALU_SRC_LITERAL,_x, _R0,_y) UPDATE_EXEC_MASK(DEACTIVATE) UPDATE_PRED
      ALU_LAST,
      ALU_LITERAL(0x3E991815),
   },
   {
      /* 14 */
      ALU_ADD_D2(_R127,_x, _R2,_x, _R1,_x),
      ALU_MOV(_R0,_y, ALU_SRC_LITERAL,_x),
      ALU_MOV(_R0,_z, ALU_SRC_LITERAL,_y),
      ALU_ADD(__,_w, _R0,_y, ALU_SRC_LITERAL,_z),
      ALU_MOV(_R1,_y, ALU_SRC_LITERAL,_w)
      ALU_LAST,
      ALU_LITERAL4(0x3E4CCCCD, 0x3E8D6CCC, 0xBD37B680, 0x3E3C9110),
      /* 15 */
      ALU_ADD_D2(_R127,_y, ALU_SRC_PS,_x, ALU_SRC_PV,_y),
      ALU_MUL_IEEE(__,_z, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x) CLAMP,
      ALU_MOV(_R1,_z, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL2(0x407BCF50, 0x3ECCCCCD),
      /* 16 */
      ALU_MULADD(_R123,_x, ALU_SRC_LITERAL,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x),
      ALU_MUL(__,_y, ALU_SRC_PV,_z, ALU_SRC_PV,_z),
      ALU_ADD_D2(_R127,_z, ALU_SRC_PS,_x, _R0,_z)
      ALU_LAST,
      ALU_LITERAL2(0x40400000, 0xC0000000),
      /* 17 */
      ALU_MULADD(_R123,_w, ALU_SRC_PV _NEG,_y, ALU_SRC_PV,_x, ALU_SRC_1,_x)
      ALU_LAST,
      /* 18 */
      ALU_MULADD(_R4,_x, _R127,_z, ALU_SRC_PV,_w, ALU_SRC_0,_x),
      ALU_MULADD(_R4,_y, _R127,_y, ALU_SRC_PV,_w, ALU_SRC_0,_x),
      ALU_MULADD(_R4,_z, _R127,_x, ALU_SRC_PV,_w, ALU_SRC_0,_x)
      ALU_LAST,
   },
   {
      /* 19 */
      ALU_MULADD(_R123,_x, ALU_SRC_LITERAL,_y, _R2,_z, ALU_SRC_LITERAL,_x),
      ALU_MUL(__,_y, KC0(4),_x, ALU_SRC_LITERAL,_z),
      ALU_MOV(_R127,_z, ALU_SRC_0,_x),
      ALU_ADD(__,_w, ALU_SRC_LITERAL,_x, _R1,_w)
      ALU_LAST,
      ALU_LITERAL3(0x3F3C8F80, 0x3E67F55C, 0xBF614144),
      /* 20 */
      ALU_MUL_IEEE(_R127,_x, _R3,_z, ALU_SRC_PV,_y),
      ALU_ADD(__,_z, ALU_SRC_LITERAL,_x, ALU_SRC_PV,_w),
      ALU_FRACT(__,_w, ALU_SRC_PV,_x)
      ALU_LAST,
      ALU_LITERAL(0x3D07CACC),
      /* 21 */
      ALU_MULADD(_R123,_x, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R123,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_z, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL3(0xBFA5310B, 0x4025310B, 0x3E22F983),
      /* 22 */
      ALU_FRACT(__,_x, ALU_SRC_PV,_y),
      ALU_ADD(_R127,_y, _R2,_y, ALU_SRC_PV _NEG,_x)
      ALU_LAST,
      /* 23 */
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB, 0x40C90FDB),
      /* 24 */
      ALU_MUL(__,_z, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 25 */
      ALU_SIN(__,_x, ALU_SRC_PV,_z) SCL_210
      ALU_LAST,
      /* 26 */
      ALU_ADD(__,_z, _R127,_x, ALU_SRC_PS,_x)
      ALU_LAST,
      /* 27 */
      ALU_ADD(__,_x, _R0,_x, ALU_SRC_PV _NEG,_z)
      ALU_LAST,
      /* 28 */
      ALU_DOT4(__,_x, ALU_SRC_PV,_x, ALU_SRC_PV,_x),
      ALU_DOT4(__,_y, _R127,_y, _R127,_y),
      ALU_DOT4(__,_z, _R127,_z, _R127,_z),
      ALU_DOT4(__,_w, ALU_SRC_LITERAL,_x, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL(0x80000000),
      /* 29 */
      ALU_SQRT_IEEE(_R0,_w, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
      /* 30 */
      ALU_PRED_SETGT(__,_x, ALU_SRC_LITERAL,_x, _R0,_w) UPDATE_EXEC_MASK(DEACTIVATE) UPDATE_PRED
      ALU_LAST,
      ALU_LITERAL(0x3E94C42C),
   },
   {
      /* 31 */
      ALU_ADD(__,_x, _R1 _NEG,_x, _R2,_x),
      ALU_MOV(_R0,_y, ALU_SRC_LITERAL,_x),
      ALU_MOV(_R0,_z, ALU_SRC_LITERAL,_y),
      ALU_ADD(__,_w, _R0,_w, ALU_SRC_LITERAL,_z),
      ALU_MOV(_R1,_y, ALU_SRC_LITERAL,_w)
      ALU_LAST,
      ALU_LITERAL4(0x3E4CCCCD, 0x3E6245CD, 0xBD328502, 0x3E16D933),
      /* 32 */
      ALU_MUL_IEEE(__,_x, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x) CLAMP,
      ALU_ADD(__,_y, ALU_SRC_PV _NEG,_y, ALU_SRC_PS,_x),
      ALU_MOV(_R1,_z, ALU_SRC_LITERAL,_y),
      ALU_MULADD(_R127,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_z, _R1,_x)
      ALU_LAST,
      ALU_LITERAL3(0x4081914F, 0x3ECCCCCD, 0x3F6BB554),
      /* 33 */
      ALU_MULADD(_R123,_x, ALU_SRC_LITERAL,_y, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x),
      ALU_MUL(__,_y, ALU_SRC_PV,_x, ALU_SRC_PV,_x),
      ALU_MULADD(_R127,_z, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_z, _R0,_y),
      ALU_ADD(__,_w, _R0 _NEG,_z, ALU_SRC_PV,_z)
      ALU_LAST,
      ALU_LITERAL3(0x40400000, 0xC0000000, 0x3F6BB554),
      /* 34 */
      ALU_MULADD(_R123,_x, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x, _R0,_z),
      ALU_MULADD(_R123,_w, ALU_SRC_PV _NEG,_y, ALU_SRC_PV,_x, ALU_SRC_1,_x)
      ALU_LAST,
      ALU_LITERAL(0x3F6BB554),
      /* 35 */
      ALU_MULADD(_R4,_x, ALU_SRC_PV,_x, ALU_SRC_PV,_w, _R4,_x),
      ALU_MULADD(_R4,_y, _R127,_z, ALU_SRC_PV,_w, _R4,_y),
      ALU_MULADD(_R4,_z, _R127,_w, ALU_SRC_PV,_w, _R4,_z)
      ALU_LAST,
   },
   {
      /* 36 */
      ALU_MUL(__,_x, KC0(4),_x, ALU_SRC_LITERAL,_x),
      ALU_ADD(__,_y, ALU_SRC_LITERAL,_y, _R1,_w),
      ALU_MOV(_R127,_z, ALU_SRC_0,_x),
      ALU_MULADD(_R123,_w, ALU_SRC_LITERAL,_z, _R2,_z, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL3(0x3D025A9D, 0x3EFC57D8, 0x3F7FFDB3),
      /* 37 */
      ALU_FRACT(__,_y, ALU_SRC_PV,_w),
      ALU_ADD(__,_z, ALU_SRC_LITERAL,_x, ALU_SRC_PV,_y),
      ALU_MUL_IEEE(_R127,_w, _R3,_z, ALU_SRC_PV,_x)
      ALU_LAST,
      ALU_LITERAL(0x3F7FFD20),
      /* 38 */
      ALU_MULADD(_R123,_x, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x),
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_z, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL3(0x3E22F983, 0xBFECB330, 0x406CB330),
      /* 39 */
      ALU_ADD(_R127,_y, _R2,_y, ALU_SRC_PV _NEG,_w),
      ALU_FRACT(__,_w, ALU_SRC_PV,_x)
      ALU_LAST,
      /* 40 */
      ALU_MULADD(_R123,_y, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB, 0x40C90FDB),
      /* 41 */
      ALU_MUL(__,_z, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 42 */
      ALU_SIN(__,_x, ALU_SRC_PV,_z) SCL_210
      ALU_LAST,
      /* 43 */
      ALU_ADD(__,_z, _R127,_w, ALU_SRC_PS,_x)
      ALU_LAST,
      /* 44 */
      ALU_ADD(__,_x, _R0,_x, ALU_SRC_PV _NEG,_z)
      ALU_LAST,
      /* 45 */
      ALU_DOT4(__,_x, ALU_SRC_PV,_x, ALU_SRC_PV,_x),
      ALU_DOT4(__,_y, _R127,_y, _R127,_y),
      ALU_DOT4(__,_z, _R127,_z, _R127,_z),
      ALU_DOT4(__,_w, ALU_SRC_LITERAL,_x, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL(0x80000000),
      /* 46 */
      ALU_SQRT_IEEE(_R0,_y, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
      /* 47 */
      ALU_PRED_SETGT(__,_x, ALU_SRC_LITERAL,_x, _R0,_y) UPDATE_EXEC_MASK(DEACTIVATE) UPDATE_PRED
      ALU_LAST,
      ALU_LITERAL(0x3F59665F),
   },
   {
      /* 48 */
      ALU_ADD(__,_x, _R1 _NEG,_x, _R2,_x),
      ALU_MOV(_R0,_y, ALU_SRC_LITERAL,_x),
      ALU_MOV(_R0,_z, ALU_SRC_LITERAL,_y),
      ALU_ADD(__,_w, _R0,_y, ALU_SRC_LITERAL,_z),
      ALU_MOV(_R1,_y, ALU_SRC_LITERAL,_w)
      ALU_LAST,
      ALU_LITERAL4(0x3E4CCCCD, 0x3E1767E8, 0xBE0270A0, 0x3DC9DFE0),
      /* 49 */
      ALU_MUL_IEEE(__,_x, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x) CLAMP,
      ALU_ADD(__,_y, ALU_SRC_PV _NEG,_y, ALU_SRC_PS,_x),
      ALU_MOV(_R1,_z, ALU_SRC_LITERAL,_y),
      ALU_MULADD(_R127,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_z, _R1,_x)
      ALU_LAST,
      ALU_LITERAL3(0x3FB15362, 0x3ECCCCCD, 0x3F7463DB),
      /* 50 */
      ALU_MULADD(_R123,_x, ALU_SRC_LITERAL,_y, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x),
      ALU_MUL(__,_y, ALU_SRC_PV,_x, ALU_SRC_PV,_x),
      ALU_MULADD(_R127,_z, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_z, _R0,_y),
      ALU_ADD(__,_w, _R0 _NEG,_z, ALU_SRC_PV,_z)
      ALU_LAST,
      ALU_LITERAL3(0x40400000, 0xC0000000, 0x3F7463DB),
      /* 51 */
      ALU_MULADD(_R123,_x, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x, _R0,_z),
      ALU_MULADD(_R123,_w, ALU_SRC_PV _NEG,_y, ALU_SRC_PV,_x, ALU_SRC_1,_x)
      ALU_LAST,
      ALU_LITERAL(0x3F7463DB),
      /* 52 */
      ALU_MULADD(_R4,_x, ALU_SRC_PV,_x, ALU_SRC_PV,_w, _R4,_x),
      ALU_MULADD(_R4,_y, _R127,_z, ALU_SRC_PV,_w, _R4,_y),
      ALU_MULADD(_R4,_z, _R127,_w, ALU_SRC_PV,_w, _R4,_z)
      ALU_LAST,
   },
   {
      /* 53 */
      ALU_MULADD(_R123,_x, ALU_SRC_LITERAL,_y, _R2,_z, ALU_SRC_LITERAL,_x),
      ALU_MUL(__,_y, KC0(4),_x, ALU_SRC_LITERAL,_z),
      ALU_MOV(_R127,_z, ALU_SRC_0,_x),
      ALU_ADD(__,_w, ALU_SRC_LITERAL,_x, _R1,_w)
      ALU_LAST,
      ALU_LITERAL3(0x3E807F0A, 0x3E69ABCD, 0x3F688ACD),
      /* 54 */
      ALU_MUL_IEEE(_R127,_x, _R3,_z, ALU_SRC_PV,_y),
      ALU_ADD(__,_z, ALU_SRC_LITERAL,_x, ALU_SRC_PV,_w),
      ALU_FRACT(__,_w, ALU_SRC_PV,_x)
      ALU_LAST,
      ALU_LITERAL(0x3D105B00),
      /* 55 */
      ALU_MULADD(_R123,_x, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R123,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_z, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL3(0xBF9816FE, 0x401816FE, 0x3E22F983),
      /* 56 */
      ALU_FRACT(__,_x, ALU_SRC_PV,_y),
      ALU_ADD(_R127,_y, _R2,_y, ALU_SRC_PV _NEG,_x)
      ALU_LAST,
      /* 57 */
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB, 0x40C90FDB),
      /* 58 */
      ALU_MUL(__,_z, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 59 */
      ALU_SIN(__,_x, ALU_SRC_PV,_z) SCL_210
      ALU_LAST,
      /* 60 */
      ALU_ADD(__,_z, _R127,_x, ALU_SRC_PS,_x)
      ALU_LAST,
      /* 61 */
      ALU_ADD(__,_x, _R0,_x, ALU_SRC_PV _NEG,_z)
      ALU_LAST,
      /* 62 */
      ALU_DOT4(__,_x, ALU_SRC_PV,_x, ALU_SRC_PV,_x),
      ALU_DOT4(__,_y, _R127,_y, _R127,_y),
      ALU_DOT4(__,_z, _R127,_z, _R127,_z),
      ALU_DOT4(__,_w, ALU_SRC_LITERAL,_x, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL(0x80000000),
      /* 63 */
      ALU_SQRT_IEEE(_R0,_w, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
      /* 64 */
      ALU_PRED_SETGT(__,_x, ALU_SRC_LITERAL,_x, _R0,_w) UPDATE_EXEC_MASK(DEACTIVATE) UPDATE_PRED
      ALU_LAST,
      ALU_LITERAL(0x3E40B7F0),
   },
   {
      /* 65 */
      ALU_ADD(__,_x, _R1 _NEG,_x, _R2,_x),
      ALU_MOV(_R0,_y, ALU_SRC_LITERAL,_x),
      ALU_MOV(_R0,_z, ALU_SRC_LITERAL,_y),
      ALU_ADD(__,_w, _R0,_w, ALU_SRC_LITERAL,_z),
      ALU_MOV(_R1,_y, ALU_SRC_LITERAL,_w)
      ALU_LAST,
      ALU_LITERAL4(0x3E4CCCCD, 0x3D9A320C, 0xBCE74321, 0x3D4D9810),
      /* 66 */
      ALU_MUL_IEEE(__,_x, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x) CLAMP,
      ALU_ADD(__,_y, ALU_SRC_PV _NEG,_y, ALU_SRC_PS,_x),
      ALU_MOV(_R1,_z, ALU_SRC_LITERAL,_y),
      ALU_MULADD(_R127,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_z, _R1,_x)
      ALU_LAST,
      ALU_LITERAL3(0x40C80926, 0x3ECCCCCD, 0x3F12103A),
      /* 67 */
      ALU_MULADD(_R123,_x, ALU_SRC_LITERAL,_y, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x),
      ALU_MUL(__,_y, ALU_SRC_PV,_x, ALU_SRC_PV,_x),
      ALU_MULADD(_R127,_z, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_z, _R0,_y),
      ALU_ADD(__,_w, _R0 _NEG,_z, ALU_SRC_PV,_z)
      ALU_LAST,
      ALU_LITERAL3(0x40400000, 0xC0000000, 0x3F12103A),
      /* 68 */
      ALU_MULADD(_R123,_x, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x, _R0,_z),
      ALU_MULADD(_R123,_w, ALU_SRC_PV _NEG,_y, ALU_SRC_PV,_x, ALU_SRC_1,_x)
      ALU_LAST,
      ALU_LITERAL(0x3F12103A),
      /* 69 */
      ALU_MULADD(_R4,_x, ALU_SRC_PV,_x, ALU_SRC_PV,_w, _R4,_x),
      ALU_MULADD(_R4,_y, _R127,_z, ALU_SRC_PV,_w, _R4,_y),
      ALU_MULADD(_R4,_z, _R127,_w, ALU_SRC_PV,_w, _R4,_z)
      ALU_LAST,
   },
   {
      /* 70 */
      ALU_MUL(__,_x, KC0(4),_x, ALU_SRC_LITERAL,_x),
      ALU_ADD(__,_y, ALU_SRC_LITERAL,_y, _R1,_w),
      ALU_MOV(_R127,_z, ALU_SRC_0,_x),
      ALU_MULADD(_R123,_w, ALU_SRC_LITERAL,_z, _R2,_z, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL3(0x3F47AF3F, 0x3D92E648, 0x3E4CCCD4),
      /* 71 */
      ALU_FRACT(__,_y, ALU_SRC_PV,_w),
      ALU_ADD(__,_z, ALU_SRC_LITERAL,_x, ALU_SRC_PV,_y),
      ALU_MUL_IEEE(_R127,_w, _R3,_z, ALU_SRC_PV,_x)
      ALU_LAST,
      ALU_LITERAL(0x34036F48),
      /* 72 */
      ALU_MULADD(_R123,_x, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x),
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_z, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL3(0x3E22F983, 0xBF8F17E6, 0x400F17E6),
      /* 73 */
      ALU_ADD(_R127,_y, _R2,_y, ALU_SRC_PV _NEG,_w),
      ALU_FRACT(__,_w, ALU_SRC_PV,_x)
      ALU_LAST,
      /* 74 */
      ALU_MULADD(_R123,_y, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB, 0x40C90FDB),
      /* 75 */
      ALU_MUL(__,_z, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 76 */
      ALU_SIN(__,_x, ALU_SRC_PV,_z) SCL_210
      ALU_LAST,
      /* 77 */
      ALU_ADD(__,_z, _R127,_w, ALU_SRC_PS,_x)
      ALU_LAST,
      /* 78 */
      ALU_ADD(__,_x, _R0,_x, ALU_SRC_PV _NEG,_z)
      ALU_LAST,
      /* 79 */
      ALU_DOT4(__,_x, ALU_SRC_PV,_x, ALU_SRC_PV,_x),
      ALU_DOT4(__,_y, _R127,_y, _R127,_y),
      ALU_DOT4(__,_z, _R127,_z, _R127,_z),
      ALU_DOT4(__,_w, ALU_SRC_LITERAL,_x, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL(0x80000000),
      /* 80 */
      ALU_SQRT_IEEE(_R0,_y, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
      /* 81 */
      ALU_PRED_SETGT(__,_x, ALU_SRC_LITERAL,_x, _R0,_y) UPDATE_EXEC_MASK(DEACTIVATE) UPDATE_PRED
      ALU_LAST,
      ALU_LITERAL(0x3DF17E5E),
   },
   {
      /* 82 */
      ALU_ADD(__,_x, _R1 _NEG,_x, _R2,_x),
      ALU_MOV(_R0,_y, ALU_SRC_LITERAL,_x),
      ALU_MOV(_R0,_z, ALU_SRC_LITERAL,_y),
      ALU_ADD(__,_w, _R0,_y, ALU_SRC_LITERAL,_z),
      ALU_MOV(_R1,_y, ALU_SRC_LITERAL,_w)
      ALU_LAST,
      ALU_LITERAL4(0x3E4CCCCD, 0x3CB0478A, 0xBC90E56C, 0x3C6B0A0D),
      /* 83 */
      ALU_MUL_IEEE(__,_x, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x) CLAMP,
      ALU_ADD(__,_y, ALU_SRC_PV _NEG,_y, ALU_SRC_PS,_x),
      ALU_MOV(_R1,_z, ALU_SRC_LITERAL,_y),
      ALU_MULADD(_R127,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_z, _R1,_x)
      ALU_LAST,
      ALU_LITERAL3(0x411FA24D, 0x3ECCCCCD, 0x3DF908C0),
      /* 84 */
      ALU_MULADD(_R123,_x, ALU_SRC_LITERAL,_y, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x),
      ALU_MUL(__,_y, ALU_SRC_PV,_x, ALU_SRC_PV,_x),
      ALU_MULADD(_R127,_z, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_z, _R0,_y),
      ALU_ADD(__,_w, _R0 _NEG,_z, ALU_SRC_PV,_z)
      ALU_LAST,
      ALU_LITERAL3(0x40400000, 0xC0000000, 0x3DF908C0),
      /* 85 */
      ALU_MULADD(_R123,_x, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x, _R0,_z),
      ALU_MULADD(_R123,_w, ALU_SRC_PV _NEG,_y, ALU_SRC_PV,_x, ALU_SRC_1,_x)
      ALU_LAST,
      ALU_LITERAL(0x3DF908C0),
      /* 86 */
      ALU_MULADD(_R4,_x, ALU_SRC_PV,_x, ALU_SRC_PV,_w, _R4,_x),
      ALU_MULADD(_R4,_y, _R127,_z, ALU_SRC_PV,_w, _R4,_y),
      ALU_MULADD(_R4,_z, _R127,_w, ALU_SRC_PV,_w, _R4,_z)
      ALU_LAST,
   },
   {
      /* 87 */
      ALU_MULADD(_R123,_x, ALU_SRC_LITERAL,_y, _R2,_z, ALU_SRC_LITERAL,_x),
      ALU_MUL(__,_y, KC0(4),_x, ALU_SRC_LITERAL,_z),
      ALU_MOV(_R127,_z, ALU_SRC_0,_x),
      ALU_ADD(__,_w, ALU_SRC_LITERAL,_x, _R1,_w)
      ALU_LAST,
      ALU_LITERAL3(0x399F9C00, 0x3EC9A118, 0xBE584E5C),
      /* 88 */
      ALU_MUL_IEEE(_R127,_x, _R3,_z, ALU_SRC_PV,_y),
      ALU_ADD(__,_z, ALU_SRC_LITERAL,_x, ALU_SRC_PV,_w),
      ALU_FRACT(__,_w, ALU_SRC_PV,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E7812BA),
      /* 89 */
      ALU_MULADD(_R123,_x, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R123,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_z, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL3(0xBFA3FD9F, 0x4023FD9F, 0x3E22F983),
      /* 90 */
      ALU_FRACT(__,_x, ALU_SRC_PV,_y),
      ALU_ADD(_R127,_y, _R2,_y, ALU_SRC_PV _NEG,_x)
      ALU_LAST,
      /* 91 */
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB, 0x40C90FDB),
      /* 92 */
      ALU_MUL(__,_z, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 93 */
      ALU_SIN(__,_x, ALU_SRC_PV,_z) SCL_210
      ALU_LAST,
      /* 94 */
      ALU_ADD(__,_z, _R127,_x, ALU_SRC_PS,_x)
      ALU_LAST,
      /* 95 */
      ALU_ADD(__,_x, _R0,_x, ALU_SRC_PV _NEG,_z)
      ALU_LAST,
      /* 96 */
      ALU_DOT4(__,_x, ALU_SRC_PV,_x, ALU_SRC_PV,_x),
      ALU_DOT4(__,_y, _R127,_y, _R127,_y),
      ALU_DOT4(__,_z, _R127,_z, _R127,_z),
      ALU_DOT4(__,_w, ALU_SRC_LITERAL,_x, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL(0x80000000),
      /* 97 */
      ALU_SQRT_IEEE(_R0,_w, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
      /* 98 */
      ALU_PRED_SETGT(__,_x, ALU_SRC_LITERAL,_x, _R0,_w) UPDATE_EXEC_MASK(DEACTIVATE) UPDATE_PRED
      ALU_LAST,
      ALU_LITERAL(0x3E8FF67B),
   },
   {
      /* 99 */
      ALU_ADD(__,_x, _R1 _NEG,_x, _R2,_x),
      ALU_MOV(_R0,_y, ALU_SRC_LITERAL,_x),
      ALU_MOV(_R0,_z, ALU_SRC_LITERAL,_y),
      ALU_ADD(__,_w, _R0,_w, ALU_SRC_LITERAL,_z),
      ALU_MOV(_R1,_y, ALU_SRC_LITERAL,_w)
      ALU_LAST,
      ALU_LITERAL4(0x3E4CCCCD, 0x38BF8800, 0xBD2CC161, 0x387F6000),
      /* 100 */
      ALU_MUL_IEEE(__,_x, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x) CLAMP,
      ALU_ADD(__,_y, ALU_SRC_PV _NEG,_y, ALU_SRC_PS,_x),
      ALU_MOV(_R1,_z, ALU_SRC_LITERAL,_y),
      ALU_MULADD(_R127,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_z, _R1,_x)
      ALU_LAST,
      ALU_LITERAL3(0x4085E40A, 0x3ECCCCCD, 0x3CA83EF0),
      /* 101 */
      ALU_MULADD(_R123,_x, ALU_SRC_LITERAL,_y, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x),
      ALU_MUL(__,_y, ALU_SRC_PV,_x, ALU_SRC_PV,_x),
      ALU_MULADD(_R127,_z, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_z, _R0,_y),
      ALU_ADD(__,_w, _R0 _NEG,_z, ALU_SRC_PV,_z)
      ALU_LAST,
      ALU_LITERAL3(0x40400000, 0xC0000000, 0x3CA83EF0),
      /* 102 */
      ALU_MULADD(_R123,_x, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x, _R0,_z),
      ALU_MULADD(_R123,_w, ALU_SRC_PV _NEG,_y, ALU_SRC_PV,_x, ALU_SRC_1,_x)
      ALU_LAST,
      ALU_LITERAL(0x3CA83EF0),
      /* 103 */
      ALU_MULADD(_R4,_x, ALU_SRC_PV,_x, ALU_SRC_PV,_w, _R4,_x),
      ALU_MULADD(_R4,_y, _R127,_z, ALU_SRC_PV,_w, _R4,_y),
      ALU_MULADD(_R4,_z, _R127,_w, ALU_SRC_PV,_w, _R4,_z)
      ALU_LAST,
   },
   {
      /* 104 */
      ALU_MOV(_R0,_z, ALU_SRC_0,_x),
      ALU_MOV(_R0,_w, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3F000000),
      /* 105 */
      ALU_DOT4(__,_x, _R0,_x, _R0,_x),
      ALU_DOT4(__,_y, _R2,_y, _R2,_y),
      ALU_DOT4(__,_z, ALU_SRC_PV,_z, ALU_SRC_PV,_z),
      ALU_DOT4(__,_w, ALU_SRC_LITERAL,_x, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL(0x80000000),
      /* 106 */
      ALU_SQRT_IEEE_D2(__,_x, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
      /* 107 */
      ALU_ADD(__,_w, ALU_SRC_PS _NEG,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3FC00000),
      /* 108 */
      ALU_SQRT_IEEE(__,_x, ALU_SRC_PV,_w) SCL_210
      ALU_LAST,
      /* 109 */
      ALU_MUL(__,_x, _R4,_z, ALU_SRC_PS,_x),
      ALU_MUL(__,_y, _R4,_y, ALU_SRC_PS,_x),
      ALU_MUL(_R0,_x, _R4,_x, ALU_SRC_PS,_x)
      ALU_LAST,
      /* 110 */
      ALU_MOV(_R0,_y, ALU_SRC_PV,_y),
      ALU_MOV(_R0,_z, ALU_SRC_PV,_x)
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

GX2Shader bokeh_shader =
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
         .sq_pgm_resources_ps.num_gprs = 5,
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
      .uniformBlockCount = countof(uniform_blocks), uniform_blocks,
      .uniformVarCount = countof(uniform_vars), uniform_vars,
      .samplerVarCount = countof(samplers), samplers,
   },
   .attribute_stream = attribute_stream,
};
