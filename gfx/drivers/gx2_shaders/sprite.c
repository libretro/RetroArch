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
#include <wiiu/gx2/common.h>
#include "gx2_shader_inl.h"
#include "sprite.h"

__attribute__((aligned(GX2_SHADER_ALIGNMENT)))
static struct
{
   u64 cf[32];
   u64 alu[26];
} vs_program =
{
   {
      CALL_FS NO_BARRIER,
      ALU(32, 26) KCACHE0(CB1, _0_15) KCACHE1(CB2, _0_15),
      MEM_RING(WRITE( 0), _R1, _xyzw, ARRAY_SIZE(1), ELEM_SIZE(3)) BURSTCNT(1),
      MEM_RING(WRITE(32), _R0, _xyzw, ARRAY_SIZE(0), ELEM_SIZE(3)) NO_BARRIER
      END_OF_PROGRAM
   },
   {
      ALU_MOV_x2(_R127,_x, _R3,_y), /* @64 */
      ALU_MOV_x2(__,_y, _R3,_x),
      ALU_MOV_x2(_R127,_z, _R3,_w),
      ALU_MOV_x2(__,_w, _R3,_z), /* @70 */
      ALU_RECIP_IEEE(__,__, KC0(0), _x) SCL_210
      ALU_LAST,
      ALU_MUL_IEEE(_R0,_z, ALU_SRC_PV, _w, ALU_SRC_PS, _x),
      ALU_MUL_IEEE(__,_w, ALU_SRC_PV,_y, ALU_SRC_PS,_x),
      ALU_RECIP_IEEE(__,_z, KC0(0),_y) SCL_210
      ALU_LAST,
      ALU_ADD(_R0,_x, ALU_SRC_PV,_w, ALU_SRC_1 _NEG,_x), /* @80 */
      ALU_MUL_IEEE(__,_z, _R127,_x, ALU_SRC_PS,_x),
      ALU_MUL_IEEE(_R0,_w, _R127,_z, ALU_SRC_PS,_x),
      ALU_RECIP_IEEE(__,__, KC1(0),_x) SCL_210
      ALU_LAST,
      ALU_MUL_IEEE(_R3,_x, _R2,_x, ALU_SRC_PS,_x),
      ALU_ADD(_R0,_y, ALU_SRC_PV _NEG,_z, ALU_SRC_1,_x), /* @90 */
      ALU_MUL_IEEE(_R3,_z, _R2,_z, ALU_SRC_PS,_x),
      ALU_RECIP_IEEE(__,__, KC1(0),_y) SCL_210
      ALU_LAST,
      ALU_MUL_IEEE(_R3,_y, _R2,_y, ALU_SRC_PS,_x),
      ALU_MUL_IEEE(_R3,_w, _R2,_w, ALU_SRC_PS,_x)
      ALU_LAST,
      ALU_MOV(_R1,_x, _R1,_x), /* @100 */
      ALU_MOV(_R1,_y, _R1,_y),
      ALU_MOV(_R1,_z, _R1,_z),
      ALU_MOV(_R1,_w, _R1,_w)
      ALU_LAST,
      ALU_MOV(_R2,_x, _R3,_x),
      ALU_MOV(_R2,_y, _R3,_y),
      ALU_MOV(_R2,_z, _R3,_z),
      ALU_MOV(_R2,_w, _R3,_w)
      ALU_LAST,
   }
};

__attribute__((aligned(GX2_SHADER_ALIGNMENT)))
static struct
{
   u64 cf[32];     /* @0 */
   u64 alu[16];    /* @32 */
   u64 tex[1 * 2]; /* @48 */
} ps_program =
{
   {
      TEX(48, 1) VALID_PIX,
      ALU(32, 4),
      EXP_DONE(PIX0, _R0, _x, _y, _z, _w)
      END_OF_PROGRAM
   },
   {
      ALU_MUL(_R0,_x, _R0,_x, _R1,_x),
      ALU_MUL(_R0,_y, _R0,_y, _R1,_y),
      ALU_MUL(_R0,_z, _R0,_z, _R1,_z),
      ALU_MUL(_R0,_w, _R0,_w, _R1,_w)
      ALU_LAST,
   },
   {
      TEX_SAMPLE(_R1,_x,_y,_z,_w, _R1,_x,_y,_0,_x, _t0, _s0)
   }
};

__attribute__((aligned(GX2_SHADER_ALIGNMENT)))
static struct
{
   u64 cf[32];     /* @0 */
   u64 alu[80-32]; /* @32 */
   u64 tex[3 * 2]; /* @80 */
} gs_program =
{
   {
      TEX(80, 3),
      MEM_RING(WRITE(  0), _R7, _xyzw, ARRAY_SIZE(0), ELEM_SIZE(3)),
      ALU(32, 33),
      MEM_RING(WRITE( 16), _R2, _xy__, ARRAY_SIZE(0), ELEM_SIZE(3)),
      MEM_RING(WRITE( 32), _R3, _xyzw, ARRAY_SIZE(0), ELEM_SIZE(3)) NO_BARRIER,
      EMIT_VERTEX,
      MEM_RING(WRITE( 48), _R7, _xyzw, ARRAY_SIZE(0), ELEM_SIZE(3)) NO_BARRIER,
      MEM_RING(WRITE( 64), _R4, _xy__, ARRAY_SIZE(0), ELEM_SIZE(3)) NO_BARRIER,
      MEM_RING(WRITE( 80), _R0, _xyzw, ARRAY_SIZE(0), ELEM_SIZE(3)) NO_BARRIER,
      EMIT_VERTEX,
      MEM_RING(WRITE( 96), _R7, _xyzw, ARRAY_SIZE(0), ELEM_SIZE(3)) NO_BARRIER,
      MEM_RING(WRITE(112), _R5, _xy__, ARRAY_SIZE(0), ELEM_SIZE(3)) NO_BARRIER,
      MEM_RING(WRITE(128), _R6, _xyzw, ARRAY_SIZE(0), ELEM_SIZE(3)) NO_BARRIER,
      EMIT_VERTEX,
      MEM_RING(WRITE(144), _R7, _xyzw, ARRAY_SIZE(0), ELEM_SIZE(3)) NO_BARRIER,
      MEM_RING(WRITE(160), _R8, _xy__, ARRAY_SIZE(0), ELEM_SIZE(3)) NO_BARRIER,
      MEM_RING(WRITE(176), _R9, _xyzw, ARRAY_SIZE(0), ELEM_SIZE(3)) NO_BARRIER,
      EMIT_VERTEX
      END_OF_PROGRAM
   },
   {
      ALU_MOV(_R127,_x, _R1,_z),
      ALU_MOV(__,_y, ALU_SRC_0,_x),
      ALU_MOV(_R3,_z, ALU_SRC_0,_x),
      ALU_MOV(_R127,_w, ALU_SRC_0,_x),
      ALU_MOV(_R3,_w, ALU_SRC_LITERAL,_x)
      ALU_LAST,
      ALU_LITERAL(0x3F800000),
      ALU_ADD(_R3,_x, _R1,_x, ALU_SRC_PV,_x),
      ALU_ADD(_R3,_y, _R1,_y, ALU_SRC_PV,_y),
      ALU_MOV(__,_z, _R0,_z),
      ALU_MOV(__,_w, ALU_SRC_0,_x),
      ALU_ADD(_R4,_x, _R0,_x, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_ADD(_R2,_x, _R0,_x, ALU_SRC_PV,_z),
      ALU_ADD(_R2,_y, _R0,_y, ALU_SRC_PV,_w),
      ALU_MOV(_R127,_z, _R1 _NEG,_w),
      ALU_MOV(_R126,_w, _R1 _NEG,_w),
      ALU_ADD(_R4,_y, _R0,_y, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_ADD(_R5,_x, _R0,_x, _R0,_z),
      ALU_ADD(_R5,_y, _R0,_y, _R0,_w),
      ALU_MOV(__,_z, _R0,_w),
      ALU_ADD(_R8,_x, _R127,_w, _R0,_x)
      ALU_LAST,
      ALU_ADD(_R0,_x, _R1,_x, ALU_SRC_0,_x),
      ALU_ADD(_R8,_y, ALU_SRC_PV,_z, _R0,_y),
      ALU_MOV(_R0,_z, _R3,_z),
      ALU_MOV(_R0,_w, _R3,_w),
      ALU_ADD(_R0,_y, _R1,_y, ALU_SRC_0,_x)
      ALU_LAST,
      ALU_ADD(_R6,_x, _R1,_x, _R127,_x) VEC_021,
      ALU_ADD(_R6,_y, _R1,_y, _R127,_z),
      ALU_MOV(_R6,_z, _R3,_z),
      ALU_MOV(_R6,_w, _R3,_w),
      ALU_ADD(_R9,_x, _R127,_w, _R1,_x)
      ALU_LAST,
      ALU_ADD(_R9,_y, _R126,_w, _R1,_y),
      ALU_MOV(_R9,_z, _R3,_z),
      ALU_MOV(_R9,_w, _R3,_w) VEC_120
      ALU_LAST,
   },
   {
      VTX_FETCH(_R7,_x,_y,_z,_w, _R0,_x, _b(159), FETCH_TYPE(NO_INDEX_OFFSET), MEGA(16), OFFSET(0)), /* @160 */
      VTX_FETCH(_R1,_x,_y,_z,_w, _R0,_x, _b(159), FETCH_TYPE(NO_INDEX_OFFSET), MEGA(16), OFFSET(32)),
      VTX_FETCH(_R0,_x,_y,_z,_w, _R0,_x, _b(159), FETCH_TYPE(NO_INDEX_OFFSET), MEGA(16), OFFSET(16)),
   }
};

__attribute__((aligned(GX2_SHADER_ALIGNMENT)))
static struct
{
   u64 cf[16];     /* @0 */
   u64 vtx[3 * 2]; /* @16 */
} gs_copy_program=
{
   {
      VTX(16, 3),
      EXP_DONE(POS0, _R1,_x,_y,_z,_w),
      EXP_DONE(PARAM0, _R2,_x,_y,_z,_w) BURSTCNT(1)
      END_OF_PROGRAM
   },
   {
      VTX_FETCH(_R1,_x,_y,_z,_w, _R0,_x, _b(159), FETCH_TYPE(NO_INDEX_OFFSET), MEGA(16), OFFSET(32)),
      VTX_FETCH(_R2,_x,_y,_z,_w, _R0,_x, _b(159), FETCH_TYPE(NO_INDEX_OFFSET), MEGA(32), OFFSET(0)),
      VTX_FETCH(_R3,_x,_y,_z,_w, _R0,_x, _b(159), FETCH_TYPE(NO_INDEX_OFFSET), MINI(16), OFFSET(16)),
   }
};

static GX2AttribVar attributes[] =
{
   {"position", GX2_SHADER_VAR_TYPE_FLOAT4, 0, 0},
   {"coords",   GX2_SHADER_VAR_TYPE_FLOAT4, 0, 1},
   {"color",    GX2_SHADER_VAR_TYPE_FLOAT4, 0, 2},
};

static GX2AttribStream attribute_stream[] =
{
   {0, 0, offsetof(sprite_vertex_t, pos),   GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32,
    GX2_ATTRIB_INDEX_PER_VERTEX, 0, GX2_COMP_SEL(_x, _y, _z, _w), GX2_ENDIAN_SWAP_DEFAULT},
   {1, 0, offsetof(sprite_vertex_t, coord), GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32,
    GX2_ATTRIB_INDEX_PER_VERTEX, 0, GX2_COMP_SEL(_x, _y, _z, _w), GX2_ENDIAN_SWAP_DEFAULT},
   {2, 0, offsetof(sprite_vertex_t, color), GX2_ATTRIB_FORMAT_UNORM_8_8_8_8,
    GX2_ATTRIB_INDEX_PER_VERTEX, 0, GX2_COMP_SEL(_x, _y, _z, _w), GX2_ENDIAN_SWAP_DEFAULT},
};

static GX2SamplerVar samplers[] =
{
   { "s", GX2_SAMPLER_VAR_TYPE_SAMPLER_2D, 0 },
};

static GX2UniformBlock uniform_blocks[] =
{
    {"UBO_vp", 1, sizeof(GX2_vec2)},
    {"UBO_tex", 2, sizeof(GX2_vec2)},
};

static GX2UniformVar uniform_vars[] =
{
    {"vp_size",  GX2_SHADER_VAR_TYPE_FLOAT2, 1, 0, 0},
    {"tex_size", GX2_SHADER_VAR_TYPE_FLOAT2, 1, 0, 1},
};

GX2Shader sprite_shader =
{
   {
      {
         .sq_pgm_resources_vs.num_gprs = 4,
         .sq_pgm_resources_vs.stack_size = 1,
         .vgt_primitiveid_en.enable = TRUE,
         .spi_vs_out_config.vs_export_count = 0,
         .num_spi_vs_out_id = 0,
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
         .sq_vtx_semantic_clear = ~0x7,
         .num_sq_vtx_semantic = 3,
         {
               2,    1,    0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
         },
         .vgt_vertex_reuse_block_cntl.vtx_reuse_depth = 0x0,
         .vgt_hos_reuse_depth.reuse_depth = 0x0,
      }, /* regs */
      .size = sizeof(vs_program),
      .program = (uint8_t*)&vs_program,
      .mode = GX2_SHADER_MODE_GEOMETRY_SHADER,
      .uniformBlockCount = countof(uniform_blocks), uniform_blocks,
      .uniformVarCount = countof(uniform_vars), uniform_vars,
      .attribVarCount = countof(attributes), attributes,
      .ringItemSize = 12,
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
      .mode = GX2_SHADER_MODE_GEOMETRY_SHADER,
      .samplerVarCount = countof(samplers), samplers,
   },
   {
      {
         .sq_pgm_resources_gs.num_gprs = 10,
         .vgt_gs_out_prim_type = VGT_GS_OUT_PRIMITIVE_TYPE_TRISTRIP,
         .vgt_gs_mode.mode = VGT_GS_ENABLE_MODE_SCENARIO_G,
         .vgt_gs_mode.cut_mode = VGT_GS_CUT_MODE_128,
         .num_spi_vs_out_id = 1,
         {
            {.semantic_0 =    0, .semantic_1 =    1, .semantic_2 = 0xFF, .semantic_3 = 0xFF},
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
         .sq_pgm_resources_vs.num_gprs = 4,
         .sq_gs_vert_itemsize = 12,
         .spi_vs_out_config.vs_export_count = 1,
      }, /* regs */
      .size = sizeof(gs_program),
      .program = (uint8_t*)&gs_program,
      .copyProgramSize = sizeof(gs_copy_program),
      .copyProgram = (uint8_t*)&gs_copy_program,
      .mode = GX2_SHADER_MODE_GEOMETRY_SHADER,
      .ringItemSize = 48,
   },
   .attribute_stream = attribute_stream,
};
