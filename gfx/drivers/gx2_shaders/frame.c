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
#include "gx2/ra_shaders.h"
#include "gx2_shader_inl.h"
#include "frame.h"
#include <gx2/enum.h>

__attribute__((aligned(GX2_SHADER_PROGRAM_ALIGNMENT)))
static struct
{
   u64 cf[32];
   u64 alu[18];
} vs_program =
{
   {
      CALL_FS NO_BARRIER,
      ALU(32, 18) KCACHE0(CB1, _0_15),
      EXP_DONE(POS0,   _R1, _x, _y, _0, _1),
      (EXP_DONE(PARAM0, _R2, _x, _y, _z, _z) NO_BARRIER)
      END_OF_PROGRAM
   },
   {
      ALU_MUL(__,_x, _R1,_w, KC0(3),_y),
      ALU_MUL(__,_y, _R1,_w, KC0(3),_x),
      ALU_MUL(__,_z, _R1,_w, KC0(3),_w),
      ALU_MUL(__,_w, _R1,_w, KC0(3),_z),
      ALU_MOV(_R0,_x, _R2,_x)
      ALU_LAST,
      ALU_MULADD(_R123,_x, _R1,_z, KC0(2),_y, ALU_SRC_PV,_x),
      ALU_MULADD(_R123,_y, _R1,_z, KC0(2),_x, ALU_SRC_PV,_y),
      ALU_MULADD(_R123,_z, _R1,_z, KC0(2),_w, ALU_SRC_PV,_z),
      ALU_MULADD(_R123,_w, _R1,_z, KC0(2),_z, ALU_SRC_PV,_w),
      ALU_MOV(_R0,_y, _R2,_y)
      ALU_LAST,
      ALU_MULADD(_R123,_x, _R1,_y, KC0(1),_y, ALU_SRC_PV,_x),
      ALU_MULADD(_R123,_y, _R1,_y, KC0(1),_x, ALU_SRC_PV,_y),
      ALU_MULADD(_R123,_z, _R1,_y, KC0(1),_w, ALU_SRC_PV,_z),
      ALU_MULADD(_R123,_w, _R1,_y, KC0(1),_z, ALU_SRC_PV,_w)
      ALU_LAST,
      ALU_MULADD(_R1,_x, _R1,_x, KC0(0),_x, ALU_SRC_PV,_y),
      ALU_MULADD(_R1,_y, _R1,_x, KC0(0),_y, ALU_SRC_PV,_x),
      ALU_MULADD(_R1,_z, _R1,_x, KC0(0),_z, ALU_SRC_PV,_w),
      ALU_MULADD(_R1,_w, _R1,_x, KC0(0),_w, ALU_SRC_PV,_z)
      ALU_LAST,
   }
};

__attribute__((aligned(GX2_SHADER_PROGRAM_ALIGNMENT)))
static struct
{
   u64 cf[16];
   u64 tex[1 * 2];
}
ps_program =
{
   {
      TEX(16, 1) VALID_PIX,
      EXP_DONE(PIX0, _R0, _x, _y, _z, _w)
      END_OF_PROGRAM
   },
   {
      TEX_SAMPLE(_R0,_x,_y,_z,_w, _R0,_x,_y,_0,_0, _t0, _s0)
   }
};

static GX2AttribVar attributes[2] =
{
   { "Position",  GX2_SHADER_VAR_TYPE_FLOAT4, 0, 0},
   { "TexCoord", GX2_SHADER_VAR_TYPE_FLOAT2, 0, 1},
};

static GX2AttribStream attribute_stream[2] =
{
   {0, 0, offsetof(frame_vertex_t, pos), GX2_ATTRIB_FORMAT_FLOAT_32_32,
    GX2_ATTRIB_INDEX_PER_VERTEX, 0, GX2_COMP_SEL(_x, _y, _0, _1), GX2_ENDIAN_SWAP_DEFAULT},
   {1, 0, offsetof(frame_vertex_t, coord), GX2_ATTRIB_FORMAT_FLOAT_32_32,
    GX2_ATTRIB_INDEX_PER_VERTEX, 0, GX2_COMP_SEL(_x, _y, _0, _0), GX2_ENDIAN_SWAP_DEFAULT},
};

static GX2SamplerVar samplers[1] =
{
   { "Source", GX2_SAMPLER_VAR_TYPE_SAMPLER_2D, 0 },
};

static GX2UniformBlock uniform_blocks[1] = {
    {"UBO", 1, 64}
};

static GX2UniformVar uniform_vars[1] = {
    {"global.MVP", GX2_SHADER_VAR_TYPE_FLOAT4X4, 1, 0, 0},
};

GX2Shader frame_shader =
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
               0,    1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
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
