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
 * Vertex Shader GLSL source:
 *******************************************************

   attribute vec2 position;
   attribute vec2 tex_coord_in;
   attribute vec4 color_in;
   varying vec2 tex_coord;
   varying vec4 color;
   void main()
   {
      gl_Position = vec4(position, 0.0, 1.0);
      tex_coord = tex_coord_in;
      color = color_in;
   }

 ******************************************************
 * assembly:
 ******************************************************
   00 CALL_FS NO_BARRIER
   01 ALU: ADDR(32) CNT(5)
         0  x: MOV         R3.x,  R3.x
            y: MOV         R3.y,  R3.y
            z: MOV         R2.z,  0.0f
            w: MOV         R2.w,  (0x3F800000, 1.0f).x
   02 EXP_DONE: POS0, R2
   03 EXP: PARAM0, R1  NO_BARRIER
   04 EXP_DONE: PARAM1, R3.xyzz  NO_BARRIER
   END_OF_PROGRAM
 ******************************************************
 */

__attribute__((aligned(GX2_SHADER_ALIGNMENT)))
static struct
{
   u32 cf[32 * 2]; /* first ADDR() * 2 */
   u32 alu[5 * 2]; /* alu CNT() * 2 */
} vs_program =
{
   {
      CALL_FS NO_BARRIER,
      ALU(32, 5),
      EXP_DONE(POS0,   _R2, _X, _Y, _Z, _W),
      EXP(PARAM0, _R1, _X, _Y, _Z, _W) NO_BARRIER,
      EXP_DONE(PARAM1, _R3, _X, _Y, _Z, _Z) NO_BARRIER
      END_OF_PROGRAM
   },
   {
      ALU_MOV(_R3,_X, _R3,_X),
      ALU_MOV(_R3,_Y, _R3,_Y),
      ALU_MOV(_R2,_Z, ALU_SRC_0,_X),
      ALU_LAST
      ALU_MOV(_R2,_W, ALU_SRC_LITERAL,_X), ALU_LITERAL(0x3F800000)
   }
};

/*******************************************************
 * Pixel Shader GLSL source:
 *******************************************************

   varying vec2 tex_coord;
   varying vec4 color;
   uniform sampler2D s;
   void main()
   {
      gl_FragColor = texture2D(s, tex_coord) * color;
   }

 ******************************************************
 * assembly:
 ******************************************************

   00 TEX: ADDR(48) CNT(1) VALID_PIX
         0  SAMPLE R1, R1.xy0x, t0, s0
   01 ALU: ADDR(32) CNT(4)
         1  x: MUL         R0.x,  R0.x,  R1.x
            y: MUL         R0.y,  R0.y,  R1.y
            z: MUL         R0.z,  R0.z,  R1.z
            w: MUL         R0.w,  R0.w,  R1.w
   02 EXP_DONE: PIX0, R0
   END_OF_PROGRAM

 *******************************************************
 */

__attribute__((aligned(GX2_SHADER_ALIGNMENT)))
static struct
{
   u32 cf[32 * 2]; /* first ADDR() * 2 */
   u32 alu[(48-32) * 2]; /* (tex ADDR() - alu ADDR()) * 2 */
   u32 tex[1 * 3]; /* tex CNT() * 3 */
} ps_program =
{
   {
      TEX(48, 1) VALID_PIX,
      ALU(32, 4),
      EXP_DONE(PIX0, _R0, _X, _Y, _Z, _W)
      END_OF_PROGRAM
   },
   {
      ALU_MUL(_R0,_X, _R0,_X, _R1,_X),
      ALU_MUL(_R0,_Y, _R0,_Y, _R1,_Y),
      ALU_MUL(_R0,_Z, _R0,_Z, _R1,_Z),
      ALU_LAST
      ALU_MUL(_R0,_W, _R0,_W, _R1,_W),
   },
   {
      TEX_SAMPLE(_R1,_X,_Y,_Z,_W, _R1,_X,_Y,_0,_X, _t0, _s0)
   }
};

tex_shader_t tex_shader =
{
   {
      {
         .sq_pgm_resources_vs.num_gprs = 4,
         .sq_pgm_resources_vs.stack_size = 1,
         .spi_vs_out_config.vs_export_count = 1,
         .num_spi_vs_out_id = 1,
         {
            {.semantic_0 = 0x00, .semantic_1 = 0x01, .semantic_2 = 0xFF, .semantic_3 = 0xFF},
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
         .sq_vtx_semantic_clear = ~0x7,
         .num_sq_vtx_semantic = 3,
         {
               0,    1,    2, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
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
         .sq_pgm_resources_ps.num_gprs = 2,
         .sq_pgm_exports_ps.export_mode = 0x2,
         .spi_ps_in_control_0.num_interp = 2,
         .spi_ps_in_control_0.persp_gradient_ena = 1,
         .spi_ps_in_control_0.baryc_sample_cntl = spi_baryc_cntl_centers_only,
         .num_spi_ps_input_cntl = 2, {{.semantic = 0, .default_val = 1},{.semantic = 1, .default_val = 1}},
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
      .color = { "color_in", GX2_SHADER_VAR_TYPE_FLOAT4, 0, 0},
      .position =  { "position",     GX2_SHADER_VAR_TYPE_FLOAT2, 0, 1},
      .tex_coord = { "tex_coord_in", GX2_SHADER_VAR_TYPE_FLOAT2, 0, 2},
   },
   .attribute_stream = {
      .color = {
         0, 2, 0, GX2_ATTRIB_FORMAT_UNORM_8_8_8_8,
         GX2_ATTRIB_INDEX_PER_VERTEX, 0, GX2_COMP_SEL(_X, _Y, _Z, _W), GX2_ENDIAN_SWAP_DEFAULT
      },
      .position = {
         1, 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32,
         GX2_ATTRIB_INDEX_PER_VERTEX, 0, GX2_COMP_SEL(_X, _Y, _0, _1), GX2_ENDIAN_SWAP_DEFAULT
      },
      .tex_coord = {
         2, 1, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32,
         GX2_ATTRIB_INDEX_PER_VERTEX, 0, GX2_COMP_SEL(_X, _Y, _0, _1), GX2_ENDIAN_SWAP_DEFAULT
      }
   },
   {},
};
