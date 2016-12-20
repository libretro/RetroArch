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
         0x00000103, 0x00000000, 0x00000000, 0x00000001, /* sq_pgm_resources_vs, vgt_primitiveid_en, spi_vs_out_config, num_spi_vs_out_id */
         { 0xffffff00, _x9(0xffffffff) }, /* spi_vs_out_id @10 */
         0x00000000, 0xfffffffc, 0x00000002, /* pa_cl_vs_out_cntl, sq_vtx_semantic_clear, num_sq_vtx_semantic */
         {
            0x00000000, 0x00000001, _x30(0x000000ff) /* sq_vtx_semantic @32 */
         },
         0x00000000, 0x0000000e, 0x00000010 /* vgt_strmout_buffer_en, vgt_vertex_reuse_block_cntl, vgt_hos_reuse_depth */
      },                                                    /* regs */
      sizeof(vs_program),                                   /* size */
      (uint8_t*)&vs_program,                                /* program */
      GX2_SHADER_MODE_UNIFORM_REGISTER,                     /* mode */
      0,                                                    /* uniformBlockCount */
      NULL,                                                 /* uniformBlocks */
      0,                                                    /* uniformVarCount */
      NULL,                                                 /* uniformVars */
      0,                                                    /* initialValueCount */
      NULL,                                                 /* initialValues */
      0,                                                    /* loopVarCount */
      NULL,                                                 /* loopVars */
      0,                                                    /* samplerVarCount */
      NULL,                                                 /* samplerVars */
      sizeof(tex_shader.attributes) / sizeof(GX2AttribVar), /* attribVarCount */
      (GX2AttribVar*) &tex_shader.attributes,               /* attribVars */
      0,                                                    /* ringItemsize */
      FALSE,                                                /* hasStreamOut */
      {0},                                                  /* streamOutStride @4 */
      {}                                                    /* gx2rBuffer */
   },
   {
      {
         0x00000001, 0x00000002, 0x14000001, 0x00000000, /* sq_pgm_resources_ps, sq_pgm_exports_ps, spi_ps_in_control_0, spi_ps_in_control_1 */
         0x00000001, /* num_spi_ps_input_cntl */
         { 0x00000100, _x30(0x00000000)}, /* spi_ps_input_cntls @ 32*/
         0x0000000f, 0x00000001, 0x00000010, 0x00000000 /* cb_shader_mask, cb_shader_control, db_shader_control, spi_input_z */
      },                                                    /* regs */
      sizeof(ps_program),                                   /* size */
      (uint8_t*)&ps_program,                                /* program */
      GX2_SHADER_MODE_UNIFORM_REGISTER,                     /* mode */
      0,                                                    /* uniformBlockCount */
      NULL,                                                 /* uniformBlocks */
      0,                                                    /* uniformVarCount */
      NULL,                                                 /* uniformVars */
      0,                                                    /* initialValueCount */
      NULL,                                                 /* initialValues */
      0,                                                    /* loopVarCount */
      NULL,                                                 /* loopVars */
      1,                                                    /* samplerVarCount */
      (GX2SamplerVar*) &tex_shader.sampler,                 /* samplerVars */
      {}                                                    /* gx2rBuffer */
   },
   { "s", GX2_SAMPLER_VAR_TYPE_SAMPLER_2D, 0 },
   {
      { "position",     GX2_SHADER_VAR_TYPE_FLOAT2, 0, 0},
      { "tex_coord_in", GX2_SHADER_VAR_TYPE_FLOAT2, 0, 1}
   },
   {
      {
         0, 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32,
         GX2_ATTRIB_INDEX_PER_VERTEX, 0, GX2_COMP_SEL(_X, _Y, _0, _1), GX2_ENDIAN_SWAP_DEFAULT
      },
      {
         1, 1, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32,
         GX2_ATTRIB_INDEX_PER_VERTEX, 0, GX2_COMP_SEL(_X, _Y, _0, _1), GX2_ENDIAN_SWAP_DEFAULT
      }
   },
   {},
};
