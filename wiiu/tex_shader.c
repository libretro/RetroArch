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

#include <stddef.h>
#include <malloc.h>
#include <string.h>
#include "tex_shader.h"
#include "gx2_shader_inl.h"

/*******************************************************
 *******************************************************
 *
 * Vertex Shader GLSL source:
 *
 *******************************************************
 *******************************************************
 *
 * attribute vec2 position;
 * attribute vec2 tex_coord_in;
 * varying vec2 tex_coord;
 * void main()
 * {
 *    gl_Position = vec4(position, 0.0, 1.0);
 *    tex_coord = tex_coord_in;
 * }
 *
 ******************************************************
 ******************************************************
 *
 * assembly output from AMD's GPU ShaderAnalyzer :
 *
 ******************************************************
 ******************************************************
 *
 * 00 CALL_FS NO_BARRIER
 * 01 ALU: ADDR(32) CNT(5)
 *        0  x: MOV         R2.x,  R2.x
 *           y: MOV         R2.y,  R2.y
 *           z: MOV         R1.z,  0.0f
 *           w: MOV         R1.w,  (0x3F800000, 1.0f).x
 * 02 EXP_DONE: POS0, R1
 * 03 EXP_DONE: PARAM0, R2.xyzz  NO_BARRIER
 * END_OF_PROGRAM
 *
 ******************************************************
 ******************************************************
 */

__attribute__((aligned(GX2_SHADER_ALIGNMENT)))
static struct
{
   u32 cf[32 * 2]; /* first ADDR() * 2 */
   u32 alu[5 * 2]; /* CNT() sum * 2 */
} vs_program =
{
   {
      CALL_FS NO_BARRIER,
      ALU(32, 5),
      EXP_DONE(POS0,   _R1, _X, _Y, _Z, _W),
      EXP_DONE(PARAM0, _R2, _X, _Y, _Z, _Z) NO_BARRIER
      END_OF_PROGRAM
   },
   {
      ALU_MOV(_R2, _X, _R2,             _X),
      ALU_MOV(_R2, _Y, _R2,             _Y),
      ALU_MOV(_R1, _Z, ALU_SRC_0,       _X),
      ALU_LAST ALU_MOV(_R1, _W, ALU_SRC_LITERAL, _X), ALU_LITERAL(0x3F800000)
   }
};

/*******************************************************
 *******************************************************
 *
 * Pixel Shader GLSL source:
 *
 *******************************************************
 *******************************************************
 *
 * varying vec2 tex_coord;
 * uniform sampler2D s;
 * void main()
 * {
 *    gl_FragColor = texture2D(s, tex_coord);
 * }
 *
 ******************************************************
 ******************************************************
 *
 * assembly output from AMD's GPU ShaderAnalyzer :
 *
 ******************************************************
 ******************************************************
 *
 * 00 TEX: ADDR(16) CNT(1) VALID_PIX
 *        0  SAMPLE R0, R0.xy0x, t0, s0
 * 01 EXP_DONE: PIX0, R0
 * END_OF_PROGRAM
 *
 *******************************************************
 *******************************************************
 */

__attribute__((aligned(GX2_SHADER_ALIGNMENT)))
static struct
{
   u32 cf[16 * 2]; /* first ADDR() * 2 */
   u32 tex[1 * 3]; /* CNT() sum * 3 */
} ps_program =
{
   {
      TEX(16, 1) VALID_PIX,
      EXP_DONE(PIX0, _R0, _X, _Y, _Z, _W)
      END_OF_PROGRAM
   },
   {
      TEX_SAMPLE(_R0, _X, _Y, _Z, _W, _R0, _X, _Y, _0, _X, _t0, _s0)
   }
};

tex_shader_t tex_shader =
{
   {
      {
         .sq_pgm_resources_vs.num_gprs = 3,
         .sq_pgm_resources_vs.stack_size = 1,
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
      .mode = GX2_SHADER_MODE_UNIFORM_REGISTER,
      .attribVarCount = sizeof(tex_shader.attributes) / sizeof(GX2AttribVar), (GX2AttribVar*) &tex_shader.attributes,
   },
   {
      {
         .sq_pgm_resources_ps.num_gprs = 1,
         .sq_pgm_exports_ps.export_mode = 0x2,
         .spi_ps_in_control_0.num_interp = 1,
         .spi_ps_in_control_0.persp_gradient_ena = 1,
         .spi_ps_in_control_0.baryc_sample_cntl = spi_baryc_cntl_centers_only,
         .num_spi_ps_input_cntl = 1, {{.default_val = 1},},
         .cb_shader_mask.output0_enable = 0xF,
         .cb_shader_control.rt0_enable = TRUE,
         .db_shader_control.z_order = db_z_order_early_z_then_late_z,
      }, /* regs */
      .size = sizeof(ps_program),
      .program = (uint8_t*)&ps_program,
      .mode = GX2_SHADER_MODE_UNIFORM_REGISTER,
      .samplerVarCount = 1,
      .samplerVars = (GX2SamplerVar*) &tex_shader.sampler,
   },
   .sampler = { "s", GX2_SAMPLER_VAR_TYPE_SAMPLER_2D, 0 },
   .attributes = {
      .position =  { "position",     GX2_SHADER_VAR_TYPE_FLOAT2, 0, 0},
      .tex_coord = { "tex_coord_in", GX2_SHADER_VAR_TYPE_FLOAT2, 0, 1}
   },
   .attribute_stream = {
      .position = {
         0, 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32,
         GX2_ATTRIB_INDEX_PER_VERTEX, 0, GX2_COMP_SEL(_X, _Y, _0, _1), GX2_ENDIAN_SWAP_DEFAULT
      },
      .tex_coord = {
         1, 1, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32,
         GX2_ATTRIB_INDEX_PER_VERTEX, 0, GX2_COMP_SEL(_X, _Y, _0, _1), GX2_ENDIAN_SWAP_DEFAULT
      }
   },
   {},
};
