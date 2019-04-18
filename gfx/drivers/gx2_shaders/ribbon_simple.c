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
   u64 alu[88];
} vs_program =
{
   {
      CALL_FS NO_BARRIER,
      ALU(32,88) KCACHE0(CB1, _0_15),
      EXP_DONE(POS0, _R1,_x,_y,_z,_w),
      EXP_DONE(PARAM0, _R0,_m,_m,_m,_m)
      END_OF_PROGRAM
   },
   {
      /* 0 */
      ALU_MUL_IEEE(__,_x, _R1,_y, ALU_SRC_LITERAL,_x),
      ALU_MUL_IEEE(__,_z, KC0(5),_x, ALU_SRC_0_5,_x),
      ALU_MUL(__,_w, _R1,_y, ALU_SRC_LITERAL,_y),
      ALU_MOV(_R1,_z, _R1,_y)
      ALU_LAST,
      ALU_LITERAL2(0x3EAAAAAB, 0x40400000),
      /* 1 */
      ALU_ADD(__,_x, _R1,_x, ALU_SRC_PV,_z),
      ALU_ADD(__,_y, _R1,_x, ALU_SRC_PV,_x),
      ALU_FLOOR(__,_z, ALU_SRC_PV,_w),
      ALU_FRACT(_R127,_w, ALU_SRC_PV,_w),
      ALU_MOV(_R1,_w, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3F800000),
      /* 2 */
      ALU_MUL(_R127,_x, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x),
      ALU_FLOOR(__,_y, ALU_SRC_PV,_x),
      ALU_FRACT(__,_z, ALU_SRC_PV,_x),
      ALU_ADD_x2(__,_w, KC0(5),_x, ALU_SRC_PV,_y),
      ALU_MOV_x2(__,_x, ALU_SRC_PV,_w)
      ALU_LAST,
      ALU_LITERAL(0x42E20000),
      /* 3 */
      ALU_MULADD(_R123,_x, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x),
      ALU_MOV_x2(__,_y, ALU_SRC_PV,_z),
      ALU_ADD(__,_z, ALU_SRC_PV,_y, ALU_SRC_0,_x),
      ALU_MUL(_R126,_w, ALU_SRC_PV,_z, ALU_SRC_PV,_z),
      ALU_ADD(_R125,_w, ALU_SRC_PS _NEG,_x, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL2(0x3E22F983, 0x40400000),
      /* 4 */
      ALU_ADD(__,_x, _R127,_x, ALU_SRC_PV,_z),
      ALU_ADD(__,_y, ALU_SRC_PV _NEG,_y, ALU_SRC_LITERAL,_x),
      ALU_FRACT(_R127,_z, ALU_SRC_PV,_x),
      ALU_MUL(_R127,_w, _R127,_w, _R127,_w)
      ALU_LAST,
      ALU_LITERAL(0x40400000),
      /* 5 */
      ALU_ADD(__,_x, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x),
      ALU_ADD(__,_y, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_y),
      ALU_ADD(__,_z, ALU_SRC_PV,_x, ALU_SRC_1,_x),
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_z, ALU_SRC_0_5,_x),
      ALU_MUL(_R126,_x, ALU_SRC_PV,_y, _R126,_w)
      ALU_LAST,
      ALU_LITERAL3(0x42E40000, 0x42E20000, 0x3E22F983),
      /* 6 */
      ALU_MULADD(_R123,_x, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x),
      ALU_FRACT(__,_y, ALU_SRC_PV,_w),
      ALU_MULADD(_R123,_z, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x),
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x, ALU_SRC_0_5,_x),
      ALU_MULADD(_R127,_x, _R127,_z, ALU_SRC_LITERAL,_z, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL3(0x3E22F983, 0xC0490FDB, 0x40C90FDB),
      /* 7 */
      ALU_FRACT(__,_x, ALU_SRC_PV,_z),
      ALU_FRACT(__,_y, ALU_SRC_PV,_x),
      ALU_FRACT(__,_z, ALU_SRC_PV,_w),
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x),
      ALU_MUL(_R126,_w, _R125,_w, _R127,_w)
      ALU_LAST,
      ALU_LITERAL2(0xC0490FDB, 0x40C90FDB),
      /* 8 */
      ALU_MULADD(_R123,_x, ALU_SRC_PV,_y, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x),
      ALU_MUL(__,_y, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_z),
      ALU_MULADD(_R123,_z, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x),
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_y, ALU_SRC_LITERAL,_x),
      ALU_MUL(_R127,_w, _R127,_x, ALU_SRC_LITERAL,_z)
      ALU_LAST,
      ALU_LITERAL3(0xC0490FDB, 0x40C90FDB, 0x3E22F983),
      /* 9 */
      ALU_MUL(_R127,_x, ALU_SRC_PV,_x, ALU_SRC_LITERAL,_x),
      ALU_MUL(__,_y, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x),
      ALU_MUL(_R127,_z, ALU_SRC_PV,_z, ALU_SRC_LITERAL,_x),
      ALU_SIN(__,_x, ALU_SRC_PV,_y) SCL_210
      ALU_LAST,
      ALU_LITERAL(0x3E22F983),
      /* 10 */
      ALU_MUL(__,_y, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x),
      ALU_SIN(__,_y, ALU_SRC_PV,_y) SCL_210
      ALU_LAST,
      ALU_LITERAL(0x472AEE8C),
      /* 11 */
      ALU_MUL(__,_x, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x),
      ALU_FRACT(_R126,_z, ALU_SRC_PV,_y),
      ALU_SIN(__,_x, _R127,_x) SCL_210
      ALU_LAST,
      ALU_LITERAL(0x472AEE8C),
      /* 12 */
      ALU_MUL(__,_x, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x),
      ALU_FRACT(_R125,_w, ALU_SRC_PV,_x),
      ALU_SIN(__,_x, _R127,_z) SCL_210
      ALU_LAST,
      ALU_LITERAL(0x472AEE8C),
      /* 13 */
      ALU_MUL(__,_y, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x),
      ALU_FRACT(__,_w, ALU_SRC_PV,_x),
      ALU_COS(__,_y, _R127,_w) SCL_210
      ALU_LAST,
      ALU_LITERAL(0x472AEE8C),
      /* 14 */
      ALU_MUL_IEEE(_R127,_x, ALU_SRC_PS,_x, ALU_SRC_LITERAL,_x),
      ALU_ADD(__,_y, _R125 _NEG,_w, ALU_SRC_PV,_w),
      ALU_FRACT(__,_z, ALU_SRC_PV,_y)
      ALU_LAST,
      ALU_LITERAL(0x3DCCCCCD),
      /* 15 */
      ALU_ADD(__,_x, _R126 _NEG,_z, ALU_SRC_PV,_z),
      ALU_MULADD(_R127,_z, ALU_SRC_PV,_y, _R126,_x, _R125,_w)
      ALU_LAST,
      /* 16 */
      ALU_MULADD(_R125,_w, ALU_SRC_PV,_x, _R126,_x, _R126,_z)
      ALU_LAST,
      /* 17 */
      ALU_ADD(__,_x, ALU_SRC_PV _NEG,_w, _R127,_z)
      ALU_LAST,
      /* 18 */
      ALU_MULADD(_R123,_w, ALU_SRC_PV,_x, _R126,_w, _R125,_w)
      ALU_LAST,
      /* 19 */
      ALU_MUL_IEEE(__,_y, ALU_SRC_PV,_w, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3E800000),
      /* 20 */
      ALU_ADD(__,_x, ALU_SRC_PV,_y, _R127,_x)
      ALU_LAST,
      /* 21 */
      ALU_MOV(_R1,_y, ALU_SRC_PV _NEG,_x)
      ALU_LAST,
   },
};

__attribute__((aligned(GX2_SHADER_ALIGNMENT)))
static struct
{
   u64 cf[32];
   u64 alu[3];
} ps_program =
{
   {
      ALU(32,3),
      EXP_DONE(PIX0, _R0,_x,_x,_x,_w)
      END_OF_PROGRAM
   },
   {
      /* 0 */
      ALU_MOV(_R0,_x, ALU_SRC_LITERAL,_x),
      ALU_MOV(_R0,_w, ALU_SRC_LITERAL,_y)
      ALU_LAST,
      ALU_LITERAL2(0x3D4CCCCD, 0x3F800000),
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

GX2Shader ribbon_simple_shader =
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
         .sq_pgm_resources_ps.num_gprs = 1,
         .sq_pgm_resources_ps.stack_size = 0,
         .sq_pgm_exports_ps.export_mode = 0x2,
         .spi_ps_in_control_0.num_interp = 0,
         .spi_ps_in_control_0.position_ena = FALSE,
         .spi_ps_in_control_0.persp_gradient_ena = FALSE,
         .spi_ps_in_control_0.baryc_sample_cntl = spi_baryc_cntl_centers_only,
         .num_spi_ps_input_cntl = 0,
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
