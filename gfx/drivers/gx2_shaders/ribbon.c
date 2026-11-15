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
static const u64 vs_program[] =
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

static GX2AttribVar attributes[] =
{
   { "Position", GX2_SHADER_VAR_TYPE_FLOAT4, 0, 0},
   { "Lighting", GX2_SHADER_VAR_TYPE_FLOAT4, 0, 1},
};

static GX2AttribStream attribute_stream[] =
{
   {0, 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32,
    GX2_ATTRIB_INDEX_PER_VERTEX, 0, GX2_COMP_SEL(_x, _y, _z, _w), GX2_ENDIAN_SWAP_DEFAULT},
   {1, 0, 16, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32,
    GX2_ATTRIB_INDEX_PER_VERTEX, 0, GX2_COMP_SEL(_x, _y, _z, _w), GX2_ENDIAN_SWAP_DEFAULT}
};

GX2Shader ribbon_shader =
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
      .size = sizeof(vs_program),
      .program = (uint8_t*)&vs_program,
      .mode = GX2_SHADER_MODE_UNIFORM_BLOCK,
      .uniformBlockCount = 0, NULL,
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
