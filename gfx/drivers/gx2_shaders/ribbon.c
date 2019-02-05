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
   u64 alu[123 + 51];
} vs_program =
{
   {
      CALL_FS NO_BARRIER,
      ALU(32, 123) KCACHE0(CB1, _0_15),
      ALU(155, 51),
      EXP_DONE(POS0,   _R7, _x, _y, _z, _w),
      EXP_DONE(PARAM0, _R7, _x, _y, _z, _w) NO_BARRIER
      END_OF_PROGRAM
      END_OF_PROGRAM
   },
   {
      ALU_MOV(_R7,_x, _R1,_x),
      ALU_MUL_IEEE(__,_y, KC0(5),_x, ALU_SRC_LITERAL,_x),
      ALU_MUL_IEEE(__,_z, KC0(5),_x, ALU_SRC_LITERAL,_y),
      ALU_MUL_IEEE(__,_w, KC0(5),_x, ALU_SRC_LITERAL,_z),
      ALU_MUL_IEEE(_R127,_w, KC0(5),_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL3(0x3E4CCCCD,0x3C23D70A,0x3DCCCCCD),
      ALU_ADD(__,_x, ALU_SRC_PV _NEG,_z, ALU_SRC_0, _x),
      ALU_ADD(__,_y, _R1,_y, ALU_SRC_PV _NEG ,_w),
      ALU_MOV_x2(__,_z, ALU_SRC_PV,_x),
      ALU_ADD(__,_w, ALU_SRC_PV,_x, ALU_SRC_PV _NEG,_y),
      ALU_ADD(__,__, _R1,_y, ALU_SRC_PV,_w)
      ALU_LAST,
      ALU_MUL(_R127,_x, ALU_SRC_PV,_y,  ALU_SRC_LITERAL,_x),
      ALU_MUL(__,_y, ALU_SRC_PV,_x,  ALU_SRC_LITERAL,_x),
      ALU_MUL_IEEE(__,_z, ALU_SRC_PV,_w,  ALU_SRC_LITERAL,_y),
      ALU_ADD(_R126,_w, _R127 _NEG,_w, ALU_SRC_PV,_z),
      ALU_ADD(_R127,_z, _R7,_x, ALU_SRC_PS,_x)
      ALU_LAST,
      ALU_LITERAL2(0x40E00000,0x3E800000),
      ALU_FLOOR(__,_x, ALU_SRC_PV,_y),
      ALU_FLOOR(__,_y, ALU_SRC_PV,_x),
      ALU_MUL(__,_z, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x),
      ALU_FRACT(_R127,_w, ALU_SRC_PV,_y),
      ALU_MOV_x4(_R124,_x, _R1,_y)
      ALU_LAST,
      ALU_LITERAL(0x40E00000),
      ALU_MUL(__,_x, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x),
      ALU_MUL(_R127,_y, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_y),
      ALU_FRACT(__,_z, ALU_SRC_PV,_z),
      ALU_FLOOR(__,_w, ALU_SRC_PV,_z),
      ALU_MOV_x2(__,__, ALU_SRC_PV,_w)
      ALU_LAST,
      ALU_LITERAL2(0x42640000,0x42E20000),
      ALU_MUL(_R125,_x, ALU_SRC_PV,_z, ALU_SRC_PV,_z),
      ALU_MOV_x2(__,_y, ALU_SRC_PV,_z),
      ALU_FRACT(_R124,_z, _R127,_x),
      ALU_ADD(__,_w, ALU_SRC_PV,_w, ALU_SRC_PV,_x),
      ALU_ADD(_R124,_y, ALU_SRC_PS _NEG,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x40400000),
      ALU_ADD(_R126,_x, ALU_SRC_PV _NEG,_y, ALU_SRC_LITERAL,_x),
      ALU_MUL(_R0,_y, _R127,_w, _R127,_w),
      ALU_ADD(_R127,_z, _R127,_y, ALU_SRC_PV,_w),
      ALU_MULADD(_R0,_w, _R126,_w, ALU_SRC_LITERAL,_y, ALU_SRC_0_5,_x) VEC_120,
      ALU_MULADD(_R2,_y, _R127,_z, ALU_SRC_LITERAL,_y, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL2(0x40400000,0x3E22F983),
      ALU_ADD(__,_x, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x),
      ALU_ADD(_R127,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_y),
      ALU_ADD(__,_z, ALU_SRC_PV,_z, ALU_SRC_1,_x),
      ALU_ADD(_R127,_w, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_z),
      ALU_ADD(_R127,_x, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_w)
      ALU_LAST,
      ALU_LITERAL4(0x42640000,0x42680000,0x42E20000,0x42E40000),
      ALU_ADD(__,_x, _R127,_z, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R126,_y, _R127,_z, ALU_SRC_LITERAL,_y, ALU_SRC_0_5,_x),
      ALU_ADD(__,_z, _R127,_z, ALU_SRC_LITERAL,_z),
      ALU_MULADD(_R126,_w, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_y, ALU_SRC_0_5,_x),
      ALU_MULADD(_R125,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_y, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL3(0x432B0000,0x3E22F983,0x432A0000),
      ALU_MULADD(_R123,_x, _R127,_w, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x),
      ALU_MULADD(_R127,_y, _R127,_x, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x),
      ALU_MULADD(_R127,_z, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x),
      ALU_MULADD(_R123,_w, _R127,_y, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x),
      ALU_MULADD(_R125,_y, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      ALU_FRACT(__,_x, _R126,_y),
      ALU_FRACT(__,_y, _R126,_w),
      ALU_FRACT(_R126,_z, _R125,_w) VEC_120,
      ALU_FRACT(_R126,_w, ALU_SRC_PV,_w),
      ALU_FRACT(_R125,_z, ALU_SRC_PV,_x)
      ALU_LAST,
      ALU_FRACT(__,_x, _R127,_z),
      ALU_FRACT(_R127,_y, _R125,_y),
      ALU_FRACT(__,_z, _R127,_y) VEC_120,
      ALU_MULADD(_R125,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R127,_w, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB,0x40C90FDB),
      ALU_MULADD(_R123,_x, _R126,_w, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R123,_y, _R126,_z, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R126,_z, _R125,_z, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x) VEC_120,
      ALU_MULADD(_R126,_w, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R124,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB,0x40C90FDB),
      ALU_MUL(_R127,_x, _R127,_w, ALU_SRC_LITERAL,_x),
      ALU_MUL(_R127,_y, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x),
      ALU_MUL(__,_z, _R125,_w, ALU_SRC_LITERAL,_x) VEC_120,
      ALU_MULADD(_R123,_w, _R127,_y, ALU_SRC_LITERAL,_z, ALU_SRC_LITERAL,_y),
      ALU_MUL(_R0,_x, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL3(0x3E22F983,0xC0490FDB,0x40C90FDB),
      ALU_MUL(_R2,_x, _R126,_z, ALU_SRC_LITERAL,_x),
      ALU_MUL(_R3,_y, _R126,_w, ALU_SRC_LITERAL,_x),
      ALU_MUL(_R126,_z, _R124,_w, ALU_SRC_LITERAL,_x) VEC_120,
      ALU_MUL(_R126,_w, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x),
      ALU_SIN(__,__, ALU_SRC_PV,_z) SCL_210
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      ALU_MUL(__,_x, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x),
      ALU_MUL(_R6,_y, _R126,_x, _R125,_x),
      ALU_MOV_x2(_R0,_z, _R124,_z),
      ALU_MUL(_R2,_w, _R124,_y, _R0,_y),
      ALU_SIN(__,__, _R127,_x) SCL_210
      ALU_LAST,
      ALU_LITERAL(0x472AEE8C),
      ALU_MUL(__,_x, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R123,_y, _R124,_x, ALU_SRC_LITERAL,_y, ALU_SRC_0_5,_x),
      ALU_FRACT(_R125,_z, _R0,_w),
      ALU_FRACT(_R124,_w, ALU_SRC_PV,_x),
      ALU_SIN(__,__, _R127,_y) SCL_210
      ALU_LAST,
      ALU_LITERAL2(0x472AEE8C,0x3E22F983),
      ALU_FRACT(_R3,_x, _R2,_y),
      ALU_FRACT(_R0,_y, ALU_SRC_PV,_y),
      ALU_MUL(__,_z, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x),
      ALU_FRACT(__,_w, ALU_SRC_PV,_x),
      ALU_SIN(__,__, _R0,_x) SCL_210
      ALU_LAST,
      ALU_LITERAL(0x472AEE8C),
      ALU_ADD(__,_x, _R124 _NEG,_w, ALU_SRC_PV,_w),
      ALU_FRACT(_R5,_y, ALU_SRC_PV,_z),
      ALU_MUL(__,_z, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R1,_w, _R125,_z, ALU_SRC_LITERAL,_z,ALU_SRC_LITERAL,_y),
      ALU_SIN(__,__, _R126,_z) SCL_210
      ALU_LAST,
      ALU_LITERAL3(0x472AEE8C,0xC0490FDB,0x40C90FDB),
      ALU_MUL(_R0,_x, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x),
      ALU_FRACT(_R4,_y, ALU_SRC_PV,_z),
      ALU_MULADD(_R1,_z, ALU_SRC_PV,_x, _R6,_y, _R124,_w) VEC_021,
      ALU_MUL(_R0,_w, _R124,_z, _R124,_z),
      ALU_SIN(_R2,_y, _R126,_w) SCL_210
      ALU_LAST,
      ALU_LITERAL(0x472AEE8C),
      ALU_MUL(__,_x, _R2,_y, ALU_SRC_LITERAL,_x),
      ALU_ADD(__,_y, _R0 _NEG,_z, ALU_SRC_LITERAL,_y),
      ALU_MULADD(_R124,_z, _R0,_y, ALU_SRC_LITERAL,_w, ALU_SRC_LITERAL,_z) VEC_120,
      ALU_FRACT(_R126,_w, _R0,_x),
      ALU_SIN(__,__, _R3,_y) SCL_210
      ALU_LAST,
      ALU_LITERAL4(0x472AEE8C,0x40400000,0xC0490FDB,0x40C90FDB),
      ALU_ADD(__,_x, _R5 _NEG,_y, _R4,_y),
      ALU_MUL(_R125,_y, ALU_SRC_PV,_y, _R0,_w),
      ALU_MUL(__,_z, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x),
      ALU_FRACT(__,_w, ALU_SRC_PV,_x),
      ALU_SIN(__,__, _R2,_x) SCL_210
      ALU_LAST,
      ALU_LITERAL(0x472AEE8C),
      ALU_ADD(__,_x, _R126 _NEG,_w, ALU_SRC_PV,_w),
      ALU_FRACT(_R127,_y, ALU_SRC_PV,_z),
      ALU_MUL(__,_z, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_x, _R6,_y, _R5,_y),
      ALU_MUL(__,__, _R1,_w, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL2(0x472AEE8C,0x3E22F983),
      ALU_MULADD(_R124,_x, ALU_SRC_PV,_x, _R6,_y, _R126,_w),
      ALU_FRACT(_R124,_y, ALU_SRC_PV,_z),
      ALU_ADD(__,_z, _R1 _NEG,_z, ALU_SRC_PV,_w),
      ALU_MULADD(_R123,_w, _R3,_x, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x),
      ALU_COS(__,__, ALU_SRC_PS,_x) SCL_210
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB,0x40C90FDB),
      ALU_ADD(__,_x, ALU_SRC_PV _NEG,_y, _R127,_y),
      ALU_MULADD(_R127,_y, ALU_SRC_PV,_z, _R2,_w, _R1,_z),
      ALU_MUL(_R124,_z, _R124,_z, ALU_SRC_LITERAL,_x),
      ALU_MUL(__,_w, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x),
      ALU_MUL_IEEE(_R124,_w, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL2(0x3E22F983,0x3E4CCCCD),
      ALU_MULADD(_R126,_w, ALU_SRC_PV,_x, _R6,_y, _R124,_y),
      ALU_COS(_R124,_y, ALU_SRC_PV,_w) SCL_210
      ALU_LAST,
      ALU_ADD(__,_x, ALU_SRC_PV _NEG,_w, _R124,_x),
      ALU_MOV(_R7,_w, ALU_SRC_LITERAL,_x),
      ALU_COS(__,__, _R124,_z) SCL_210
      ALU_LAST,
      ALU_LITERAL(0x3F800000),
      ALU_MUL(__,_z, _R124,_y, ALU_SRC_PS,_x),
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_x, _R2,_w, _R126,_w)
      ALU_LAST,
      ALU_MUL_IEEE(_R124,_y, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x),
      ALU_ADD(__,_z, _R127 _NEG,_y, ALU_SRC_PV,_w)
      ALU_LAST,
      ALU_LITERAL(0x3E000000),
      ALU_MULADD(_R123,_y, ALU_SRC_PV,_z, _R125,_y, _R127,_y)
      ALU_LAST,
      ALU_MUL_IEEE(__,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3D888889),
      ALU_ADD(__,_y, ALU_SRC_PV,_x, _R124,_w),
      ALU_ADD(_R7,_z, _R1,_y, ALU_SRC_PV _NEG,_x)
      ALU_LAST,
      ALU_ADD(__,_z, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0xBE99999A),
      ALU_ADD(__,_x, _R124,_y, ALU_SRC_PV _NEG,_z)
      ALU_LAST,
      ALU_MOV(_R7,_y, ALU_SRC_PV _NEG,_x)
      ALU_LAST,
   }
};

__attribute__((aligned(GX2_SHADER_ALIGNMENT)))
static struct
{
   u64 cf[32];
   u64 alu[64-32];
   u64 tex[2 * 2];
}
ps_program =
{
   {
      TEX(64, 2) VALID_PIX,
      ALU(32, 27),
      EXP_DONE(PIX0, _R2, _z, _z, _z, _w)
      END_OF_PROGRAM

   },
   {
      ALU_MUL(__,_x, _R1,_z, _R0,_x),
      ALU_MUL(__,_y, _R1,_y, _R0,_z),
      ALU_MOV(_R2,_z, ALU_SRC_LITERAL,_x),
      ALU_MUL(__,_w, _R1,_x, _R0,_y)
      ALU_LAST,
      ALU_LITERAL(0x3F800000),
      ALU_MULADD(_R123,_x, _R0 _NEG,_y, _R1,_z, ALU_SRC_PV,_y),
      ALU_MULADD(_R123,_y, _R0 _NEG,_z, _R1,_x, ALU_SRC_PV,_x),
      ALU_MULADD(_R127,_z, _R0 _NEG,_x, _R1,_y, ALU_SRC_PV,_w)
      ALU_LAST,
      ALU_DOT4_IEEE(__,_x, ALU_SRC_PV,_x, ALU_SRC_PV,_x),
      ALU_DOT4_IEEE(__,_y, ALU_SRC_PV,_y, ALU_SRC_PV,_y),
      ALU_DOT4_IEEE(__,_z, ALU_SRC_PV,_z, ALU_SRC_PV,_z),
      ALU_DOT4_IEEE(__,_w, ALU_SRC_LITERAL,_x, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_LITERAL(0x80000000),
      ALU_RECIPSQRT_IEEE(__,__, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
      ALU_MULADD(_R123,_w, _R127 _NEG,_z, ALU_SRC_PS,_x, ALU_SRC_1,_x)
      ALU_LAST,
      ALU_MUL(__,_x, ALU_SRC_PV,_w, ALU_SRC_PV,_w)
      ALU_LAST,
      ALU_MULADD(_R123,_z, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      ALU_FRACT(__,_y, ALU_SRC_PV,_z)
      ALU_LAST,
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_y,ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB,0x40C90FDB),
      ALU_MUL(__,_x, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      ALU_COS(__,__, ALU_SRC_PV,_x) SCL_210
      ALU_LAST,
      ALU_ADD(__,_y, ALU_SRC_PS _NEG,_x, ALU_SRC_1,_x)
      ALU_LAST,
      ALU_MUL_IEEE(_R2,_w, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3D4CCCCD),
   },
   {
      TEX_GET_GRADIENTS_H(_R1,_x,_y,_z,_m, _R0,_x,_y,_z,_x, _t0, _s0),
      TEX_GET_GRADIENTS_V(_R0,_x,_y,_z,_m, _R0,_x,_y,_z,_x, _t0, _s0)
   }
};

static GX2AttribVar attributes[] =
{
   { "VertexCoord",  GX2_SHADER_VAR_TYPE_FLOAT2, 0, 0},
};

static GX2AttribStream attribute_stream[] =
{
   {0, 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32,
    GX2_ATTRIB_INDEX_PER_VERTEX, 0, GX2_COMP_SEL(_x, _y, _0, _1), GX2_ENDIAN_SWAP_DEFAULT}
};

static GX2SamplerVar samplers[] =
{
   { "Source", GX2_SAMPLER_VAR_TYPE_SAMPLER_2D, 0 },
};

static GX2UniformBlock uniform_blocks[] = {
    {"UBO", 1, 16}
};

static GX2UniformVar uniform_vars[] = {
    {"constants.time", GX2_SHADER_VAR_TYPE_FLOAT, 1, 0, 0},
};

GX2Shader ribbon_shader =
{
   {
      {
         .sq_pgm_resources_vs.num_gprs = 8,
         .sq_pgm_resources_vs.stack_size = 1,
         .spi_vs_out_config.vs_export_count = 0,
         .num_spi_vs_out_id = 1,
         {
            {.semantic_0 = 0x00, .semantic_1 = 0xFF, .semantic_2 = 0xFF, .semantic_3 = 0xFF},
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
         .sq_pgm_exports_ps.export_mode = 0x2,
         .spi_ps_in_control_0.num_interp = 1,
         .spi_ps_in_control_0.persp_gradient_ena = 1,
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
