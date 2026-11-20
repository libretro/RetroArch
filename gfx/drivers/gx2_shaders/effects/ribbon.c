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

/* The sheet: ribbon_vertex's surface, from VertexCoord (x, z), the
 * ribbon time in UBO[4].w and its alpha in UBO[5].x. SIN and COS take
 * turns in [-0.5, 0.5]. */
__attribute__((aligned(GX2_SHADER_ALIGNMENT)))
static struct
{
   u64 cf[32];
   u64 alu[126];
} vs_program =
{
   {
      CALL_FS NO_BARRIER,
      ALU(32, 120) KCACHE0(CB1, _0_15),
      ALU(152, 6) KCACHE0(CB1, _0_15),
      EXP_DONE(POS0, _R7, _x, _y, _z, _w),
      (EXP_DONE(PARAM0, _R9, _x, _y, _z, _w) NO_BARRIER) END_OF_PROGRAM
   },
   {
ALU_MUL(_R2,_x, KC0(4),_w, ALU_SRC_LITERAL,_x) ALU_LAST,
      ALU_LITERAL(0x3DA3D70A),
      ALU_MULADD(_R2,_x, _R1,_x, ALU_SRC_LITERAL,_x, _R2,_x) ALU_LAST,
      ALU_LITERAL(0x3FB9999A),
      ALU_MULADD(_R3,_x, _R1,_y, ALU_SRC_LITERAL,_x, ALU_SRC_LITERAL,_y) ALU_LAST,
      ALU_LITERAL2(0x3EE66666,0x3F99999A),
      ALU_MUL(_R2,_y, KC0(4),_w, ALU_SRC_LITERAL,_x) ALU_LAST,
      ALU_LITERAL(0xBD75C28F),
      ALU_MULADD(_R2,_y, _R1,_y, ALU_SRC_LITERAL,_x, _R2,_y) ALU_LAST,
      ALU_LITERAL(0x3FE00000),
      ALU_MULADD(_R2,_y, _R1,_x, _R3,_x, _R2,_y) ALU_LAST,
      ALU_MULADD(_R2,_z, _R1,_y, ALU_SRC_LITERAL,_x, ALU_SRC_LITERAL,_y) ALU_LAST,
      ALU_LITERAL2(0x3F99999A,0x40B9999A),
      ALU_MUL(_R2,_z, _R2,_z, _R1,_y) ALU_LAST,
      ALU_MULADD(_R2,_z, _R1,_x, ALU_SRC_LITERAL,_x, _R2,_z) ALU_LAST,
      ALU_LITERAL(0x3F400000),
      ALU_MULADD(_R2,_z, KC0(4),_w, ALU_SRC_LITERAL,_x, _R2,_z) ALU_LAST,
      ALU_LITERAL(0xBDB851EC),
      ALU_MULADD(_R3,_y, _R1,_y, ALU_SRC_LITERAL,_x, ALU_SRC_LITERAL,_y) ALU_LAST,
      ALU_LITERAL2(0xBF19999A,0x400CCCCD),
      ALU_MUL(_R2,_w, KC0(4),_w, ALU_SRC_LITERAL,_x) ALU_LAST,
      ALU_LITERAL(0x3D99999A),
      ALU_MULADD(_R2,_w, _R1,_y, ALU_SRC_LITERAL,_x, _R2,_w) ALU_LAST,
      ALU_LITERAL(0x41200000),
      ALU_MULADD(_R2,_w, _R1,_x, _R3,_y, _R2,_w) ALU_LAST,
      ALU_MULADD(_R4,_x, _R2,_x, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x) ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      ALU_FRACT(_R4,_x, _R4,_x) ALU_LAST,
      ALU_ADD(_R4,_x, _R4,_x, ALU_SRC_0_5 _NEG,_x) ALU_LAST,
      ALU_SIN(_R5,_x, _R4,_x) SCL_210 ALU_LAST,
      ALU_COS(_R6,_x, _R4,_x) SCL_210 ALU_LAST,
      ALU_MULADD(_R4,_y, _R2,_y, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x) ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      ALU_FRACT(_R4,_y, _R4,_y) ALU_LAST,
      ALU_ADD(_R4,_y, _R4,_y, ALU_SRC_0_5 _NEG,_x) ALU_LAST,
      ALU_SIN(_R5,_y, _R4,_y) SCL_210 ALU_LAST,
      ALU_COS(_R6,_y, _R4,_y) SCL_210 ALU_LAST,
      ALU_MULADD(_R4,_z, _R2,_z, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x) ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      ALU_FRACT(_R4,_z, _R4,_z) ALU_LAST,
      ALU_ADD(_R4,_z, _R4,_z, ALU_SRC_0_5 _NEG,_x) ALU_LAST,
      ALU_SIN(_R5,_z, _R4,_z) SCL_210 ALU_LAST,
      ALU_COS(_R6,_z, _R4,_z) SCL_210 ALU_LAST,
      ALU_MULADD(_R4,_w, _R2,_w, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x) ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      ALU_FRACT(_R4,_w, _R4,_w) ALU_LAST,
      ALU_ADD(_R4,_w, _R4,_w, ALU_SRC_0_5 _NEG,_x) ALU_LAST,
      ALU_SIN(_R5,_w, _R4,_w) SCL_210 ALU_LAST,
      ALU_COS(_R6,_w, _R4,_w) SCL_210 ALU_LAST,
      ALU_MULADD(_R3,_z, _R6,_x, ALU_SRC_LITERAL,_x, ALU_SRC_LITERAL,_y) ALU_LAST,
      ALU_LITERAL2(0x3D4CCCCD,0x3E051EB8),
      ALU_MUL(_R3,_w, _R6,_y, _R6,_z) ALU_LAST,
      ALU_MULADD(_R7,_y, _R5,_x, ALU_SRC_LITERAL,_x, ALU_SRC_LITERAL,_y) ALU_LAST,
      ALU_LITERAL2(0x3DF5C28F,0xBE8F5C29),
      ALU_MULADD(_R7,_y, _R3,_z, _R3,_w, _R7,_y) ALU_LAST,
      ALU_MULADD(_R7,_y, _R5,_w, ALU_SRC_LITERAL,_x, _R7,_y) ALU_LAST,
      ALU_LITERAL(0x3D23D70A),
      ALU_MULADD(_R7,_y, _R1,_y, ALU_SRC_LITERAL,_x, _R7,_y) ALU_LAST,
      ALU_LITERAL(0x3D75C28F),
      ALU_MUL(_R8,_x, _R5,_y, _R6,_z) ALU_LAST,
      ALU_MUL(_R8,_y, _R6,_y, _R5,_z) ALU_LAST,
      ALU_MUL(_R8,_z, _R8,_y, ALU_SRC_LITERAL,_x) ALU_LAST,
      ALU_LITERAL(0x3F400000),
      ALU_MULADD(_R8,_z, _R3,_x, _R8,_x, _R8,_z) ALU_LAST,
      ALU_MUL(_R4,_x, _R6,_x, ALU_SRC_LITERAL,_x) ALU_LAST,
      ALU_LITERAL(0x3E322D0E),
      ALU_MUL(_R8,_w, _R5,_x, _R3,_w) ALU_LAST,
      ALU_MULADD(_R4,_x, _R8,_w, ALU_SRC_LITERAL,_x, _R4,_x) ALU_LAST,
      ALU_LITERAL(0xBD947AE1),
      ALU_MULADD(_R4,_x, _R3 _NEG,_z, _R8,_z, _R4,_x) ALU_LAST,
      ALU_MUL(_R8,_w, _R3,_y, _R6,_w) ALU_LAST,
      ALU_MULADD(_R4,_x, _R8,_w, ALU_SRC_LITERAL,_x, _R4,_x) ALU_LAST,
      ALU_LITERAL(0x3D23D70A),
      ALU_MULADD(_R4,_y, _R1,_x, ALU_SRC_LITERAL,_x, ALU_SRC_LITERAL,_y) ALU_LAST,
      ALU_LITERAL2(0x3EE66666,0x3FE00000),
      ALU_MUL(_R4,_y, _R4,_y, _R8,_x) ALU_LAST,
      ALU_MULADD(_R4,_z, _R1,_y, ALU_SRC_LITERAL,_x, ALU_SRC_LITERAL,_y) ALU_LAST,
      ALU_LITERAL2(0x4019999A,0x40B9999A),
      ALU_MULADD(_R4,_y, _R4,_z, _R8,_y, _R4,_y) ALU_LAST,
      ALU_MULADD(_R4,_z, _R1,_x, ALU_SRC_LITERAL,_x, ALU_SRC_LITERAL,_y) ALU_LAST,
      ALU_LITERAL2(0xBF19999A,0x41200000),
      ALU_MUL(_R4,_z, _R4,_z, _R6,_w) ALU_LAST,
      ALU_MULADD(_R4,_w, _R4,_z, ALU_SRC_LITERAL,_x, ALU_SRC_LITERAL,_y) ALU_LAST,
      ALU_LITERAL2(0x3D23D70A,0x3D75C28F),
      ALU_MULADD(_R4,_w, _R3 _NEG,_z, _R4,_y, _R4,_w) ALU_LAST,
      ALU_MULADD(_R7,_w, _R1,_y, ALU_SRC_LITERAL,_x, ALU_SRC_1,_x) ALU_LAST,
      ALU_LITERAL(0x3DCCCCCD),
      ALU_MUL(_R7,_x, _R1,_x, ALU_SRC_LITERAL,_x) ALU_LAST,
      ALU_LITERAL(0x3F8F5C29),
      ALU_MOV(_R7,_z, ALU_SRC_0,_x) ALU_LAST,
      ALU_MUL(_R4,_z, _R4,_w, ALU_SRC_LITERAL,_x) ALU_LAST,
      ALU_LITERAL(0x3F8F5C29),
      ALU_MUL(_R8,_x, _R1,_x, _R4,_x) ALU_LAST,
      ALU_MUL(_R8,_x, _R8,_x, ALU_SRC_LITERAL,_x) ALU_LAST,
      ALU_LITERAL(0xBDE56042),
      ALU_MULADD(_R8,_x, _R7,_y, ALU_SRC_LITERAL,_x, _R8,_x) ALU_LAST,
      ALU_LITERAL(0x3DE56042),
      ALU_MULADD(_R8,_x, _R7 _NEG,_w, _R4,_z, _R8,_x) ALU_LAST,
      ALU_MULADD(_R8,_y, _R4,_x, _R4,_x, ALU_SRC_LITERAL,_x) ALU_LAST,
      ALU_LITERAL(0x3FA0902E),
      ALU_MULADD(_R8,_y, _R4,_z, _R4,_z, _R8,_y) ALU_LAST,
      ALU_MUL(_R8,_z, _R1,_x, _R1,_x) ALU_LAST,
      ALU_MUL(_R8,_z, _R8,_z, ALU_SRC_LITERAL,_x) ALU_LAST,
      ALU_LITERAL(0x3C4D8559),
      ALU_MUL(_R8,_w, _R7,_y, _R7,_y) ALU_LAST,
      ALU_MULADD(_R8,_z, _R8,_w, ALU_SRC_LITERAL,_x, _R8,_z) ALU_LAST,
      ALU_LITERAL(0x3C23D70A),
      ALU_MULADD(_R8,_z, _R7,_w, _R7,_w, _R8,_z) ALU_LAST,
      ALU_MUL(_R8,_y, _R8,_y, _R8,_z) ALU_LAST,
      ALU_RECIPSQRT_IEEE(_R8,_z, _R8,_y) SCL_210 ALU_LAST,
      ALU_MUL(_R9,_x, _R8,_x, _R8,_z) ALU_LAST,
      ALU_MUL(_R8,_x, _R4,_w, _R7,_w) ALU_LAST,
      ALU_MULADD(_R8,_x, _R7,_y, ALU_SRC_LITERAL,_x, _R8,_x) ALU_LAST,
      ALU_LITERAL(0xBDCCCCCD),
      ALU_MUL(_R8,_y, _R7,_w, _R7,_w) ALU_LAST,
      ALU_RECIP_IEEE(_R8,_z, _R8,_y) SCL_210 ALU_LAST,
      ALU_MUL(_R9,_y, _R8,_x, _R8,_z) ALU_LAST,
      ALU_MOV(_R9,_z, _R1,_y) ALU_LAST,
      ALU_MULADD(_R8,_x, _R1,_y, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x) ALU_LAST,
      ALU_LITERAL(0x408E9A53),
      ALU_FRACT(_R8,_x, _R8,_x) ALU_LAST,
      ALU_ADD(_R8,_x, _R8,_x, ALU_SRC_0_5 _NEG,_x) ALU_LAST,
      ALU_COS(_R8,_y, _R8,_x) SCL_210 ALU_LAST,
      ALU_MULADD(_R8,_y, _R8,_y, ALU_SRC_LITERAL,_x, ALU_SRC_LITERAL,_y) ALU_LAST,
      ALU_LITERAL2(0x3EB33333,0x3F266666),
      ALU_MUL(_R9,_w, _R8,_y, KC0(5),_x) ALU_LAST,
   }
};

/* The dust: positions and lighting worked out on the CPU */
__attribute__((aligned(GX2_SHADER_ALIGNMENT)))
static const u64 stars_vs_program[] =
{
   CALL_FS NO_BARRIER,
   EXP_DONE(POS0, _R1, _x, _y, _z, _w),
   (EXP_DONE(PARAM0, _R2, _x, _y, _z, _w) NO_BARRIER) END_OF_PROGRAM
};

__attribute__((aligned(GX2_SHADER_ALIGNMENT)))
static struct
{
   u64 cf[32];
   u64 alu[54];
} ps_program =
{
   {
      ALU(32, 54),
      (EXP_DONE(PIX0, _R1, _x, _y, _z, _w) NO_BARRIER) END_OF_PROGRAM
   },
   {
      ALU_MAX(_R2,_x, _R0,_x, _R0 _NEG,_x) ALU_LAST,
      ALU_ADD(_R2,_x, ALU_SRC_1,_x, _R2 _NEG,_x) ALU_LAST,
      ALU_MAX(_R2,_x, _R2,_x, ALU_SRC_0,_x) ALU_LAST,
      ALU_MUL(_R2,_y, _R2,_x, _R2,_x) ALU_LAST,
      ALU_MUL(_R2,_z, _R2,_y, _R2,_y) ALU_LAST,
      ALU_MUL(_R2,_z, _R2,_z, _R2,_y) ALU_LAST,
      ALU_MUL(_R2,_z, _R2,_z, ALU_SRC_LITERAL,_x) ALU_LAST,
      ALU_LITERAL(0x3FE66666),
      ALU_ADD(_R2,_z, _R2,_z, ALU_SRC_LITERAL,_x) ALU_LAST,
      ALU_LITERAL(0x3C9374BC),
      ALU_MAX(_R2,_x, _R0,_z, _R0 _NEG,_z) ALU_LAST,
      ALU_ADD(_R2,_x, _R2,_x, ALU_SRC_LITERAL,_x) ALU_LAST,
      ALU_LITERAL(0xBF266666),
      ALU_MUL(_R2,_x, _R2,_x, ALU_SRC_LITERAL,_x) CLAMP ALU_LAST,
      ALU_LITERAL(0x4036DB6E),
      ALU_MUL(_R2,_y, _R2,_x, ALU_SRC_LITERAL,_x) ALU_LAST,
      ALU_LITERAL(0xC0000000),
      ALU_ADD(_R2,_y, _R2,_y, ALU_SRC_LITERAL,_x) ALU_LAST,
      ALU_LITERAL(0x40400000),
      ALU_MUL(_R2,_x, _R2,_x, _R2,_x) ALU_LAST,
      ALU_MUL(_R2,_x, _R2,_x, _R2,_y) ALU_LAST,
      ALU_ADD(_R2,_x, ALU_SRC_1,_x, _R2 _NEG,_x) ALU_LAST,
      ALU_MUL(_R2,_z, _R2,_z, _R2,_x) ALU_LAST,
      ALU_MAX(_R2,_x, _R0,_y, _R0 _NEG,_y) ALU_LAST,
      ALU_ADD(_R2,_x, _R2,_x, ALU_SRC_LITERAL,_x) ALU_LAST,
      ALU_LITERAL(0xBE0F5C29),
      ALU_MUL(_R2,_x, _R2,_x, ALU_SRC_LITERAL,_x) CLAMP ALU_LAST,
      ALU_LITERAL(0x40A00000),
      ALU_MUL(_R2,_y, _R2,_x, ALU_SRC_LITERAL,_x) ALU_LAST,
      ALU_LITERAL(0xC0000000),
      ALU_ADD(_R2,_y, _R2,_y, ALU_SRC_LITERAL,_x) ALU_LAST,
      ALU_LITERAL(0x40400000),
      ALU_MUL(_R2,_x, _R2,_x, _R2,_x) ALU_LAST,
      ALU_MUL(_R2,_x, _R2,_x, _R2,_y) ALU_LAST,
      ALU_MUL(_R2,_z, _R2,_z, _R2,_x) ALU_LAST,
      ALU_MUL(_R2,_x, _R0,_x, _R0,_x) ALU_LAST,
      ALU_MULADD(_R2,_x, _R0,_y, _R0,_y, _R2,_x) ALU_LAST,
      ALU_ADD(_R2,_x, ALU_SRC_1,_x, _R2 _NEG,_x) ALU_LAST,
      ALU_MAX(_R2,_x, _R2,_x, ALU_SRC_0,_x) ALU_LAST,
      ALU_MUL(_R2,_x, _R2,_x, _R2,_x) ALU_LAST,
      ALU_MUL(_R2,_y, _R2,_x, _R2,_x) ALU_LAST,
      ALU_MUL(_R2,_y, _R2,_y, _R2,_y) ALU_LAST,
      ALU_MUL(_R2,_y, _R2,_y, _R2,_x) ALU_LAST,
      ALU_MUL(_R2,_y, _R2,_y, ALU_SRC_LITERAL,_x) ALU_LAST,
      ALU_LITERAL(0x3FE66666),
      ALU_MULADD(_R2,_y, _R2,_x, ALU_SRC_LITERAL,_x, _R2,_y) ALU_LAST,
      ALU_LITERAL(0x3DF5C28F),
      ALU_ADD(_R2,_x, _R0,_z, ALU_SRC_LITERAL,_x) ALU_LAST,
      ALU_LITERAL(0xBFC00000),
      ALU_CNDGT(_R2,_z, _R2,_x, _R2,_y, _R2,_z) ALU_LAST,
      ALU_MUL(_R1,_x, _R2,_z, _R0,_w) ALU_LAST,
      ALU_MOV(_R1,_y, _R1,_x) ALU_LAST,
      ALU_MOV(_R1,_z, _R1,_x) ALU_LAST,
      ALU_MOV(_R1,_w, ALU_SRC_0,_x) ALU_LAST,
   }
};

static GX2AttribVar stars_attributes[] =
{
   { "Position", GX2_SHADER_VAR_TYPE_FLOAT4, 0, 0},
   { "Lighting", GX2_SHADER_VAR_TYPE_FLOAT4, 0, 1},
};

static GX2AttribStream stars_attribute_stream[] =
{
   {0, 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32,
    GX2_ATTRIB_INDEX_PER_VERTEX, 0, GX2_COMP_SEL(_x, _y, _z, _w), GX2_ENDIAN_SWAP_DEFAULT},
   {1, 0, 16, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32,
    GX2_ATTRIB_INDEX_PER_VERTEX, 0, GX2_COMP_SEL(_x, _y, _z, _w), GX2_ENDIAN_SWAP_DEFAULT}
};

GX2Shader ribbon_stars_shader =
{
   {
      {
         .sq_pgm_resources_vs.num_gprs = 3,
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
         .sq_vtx_semantic_clear = ~0x3,
         .num_sq_vtx_semantic = 2,
         {
               0, 1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
         },
         .vgt_vertex_reuse_block_cntl.vtx_reuse_depth = 0xE,
         .vgt_hos_reuse_depth.reuse_depth = 0x10,
      }, /* regs */
      .size = sizeof(stars_vs_program),
      .program = (uint8_t*)&stars_vs_program,
      .mode = GX2_SHADER_MODE_UNIFORM_BLOCK,
      .uniformBlockCount = 0, NULL,
      .uniformVarCount = 0, NULL,
      .attribVarCount = countof(stars_attributes), stars_attributes,
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
      .uniformBlockCount = 0, NULL,
      .uniformVarCount = 0, NULL,
   },
   .attribute_stream = stars_attribute_stream,
};

static GX2AttribVar attributes[] =
{
   { "VertexCoord", GX2_SHADER_VAR_TYPE_FLOAT2, 0, 0},
};

static GX2UniformBlock uniform_blocks[] =
{
   {"UBO", 1, sizeof(menu_shader_uniform_t)}
};

static GX2AttribStream attribute_stream[] =
{
   {0, 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32,
    GX2_ATTRIB_INDEX_PER_VERTEX, 0, GX2_COMP_SEL(_x, _y, _0, _1), GX2_ENDIAN_SWAP_DEFAULT}
};

GX2Shader ribbon_shader =
{
   {
      {
         .sq_pgm_resources_vs.num_gprs = 10,
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
      .uniformVarCount = 0, NULL,
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
      .uniformBlockCount = 0, NULL,
      .uniformVarCount = 0, NULL,
   },
   .attribute_stream = attribute_stream,
};
