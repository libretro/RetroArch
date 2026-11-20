/*  RetroArch - A frontend for libretro.
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

/* XMB's background effects, in every shading language a video driver
 * may draw them in. For xmb.c alone. */

#ifndef XMB_EFFECTS_H
#define XMB_EFFECTS_H

#include "../../gfx/gfx_display.h"

#ifdef HAVE_SHADERPIPELINE

#if defined(HAVE_GLSL)
#if defined(VITA)
#include "../../gfx/drivers/gl_cg_shaders/effects/ribbon_simple.cg.vert.h"
#include "../../gfx/drivers/gl_cg_shaders/effects/ribbon_simple.cg.frag.h"
#include "../../gfx/drivers/gl_cg_shaders/effects/ribbon.cg.vert.h"
#include "../../gfx/drivers/gl_cg_shaders/effects/ribbon.cg.frag.h"
#else
#include "../../gfx/drivers/gl3_glsl_shaders/effects/ribbon.glsl.frag.h"
#include "../../gfx/drivers/gl3_glsl_shaders/effects/ribbon_simple.glsl.frag.h"
#include "../../gfx/drivers/gl2_glsl_shaders/effects/ribbon_simple.glsl.vert.h"
#include "../../gfx/drivers/gl3_glsl_shaders/effects/ribbon_simple.glsl.vert.h"
#include "../../gfx/drivers/gl2_glsl_shaders/effects/ribbon_simple.glsl.frag.h"
#include "../../gfx/drivers/gl2_glsl_shaders/effects/snow.glsl.frag.h"
#include "../../gfx/drivers/gl2_glsl_shaders/effects/snow_gles2.glsl.vert.h"
#include "../../gfx/drivers/gl2_glsl_shaders/effects/snow_simple.glsl.frag.h"
#include "../../gfx/drivers/gl2_glsl_shaders/effects/ribbon.glsl.vert.h"
#include "../../gfx/drivers/gl3_glsl_shaders/effects/ribbon.glsl.vert.h"
#include "../../gfx/drivers/gl2_glsl_shaders/effects/ribbon.glsl.frag.h"
#include "../../gfx/drivers/gl2_glsl_shaders/effects/bokeh.glsl.frag.h"
#include "../../gfx/drivers/gl2_glsl_shaders/effects/snowflake.glsl.frag.h"
/* The desktop snow sources; GLES takes one source for both */
#if !defined(HAVE_OPENGLES)
#include "../../gfx/drivers/gl3_glsl_shaders/effects/snow.glsl.frag.h"
#include "../../gfx/drivers/gl3_glsl_shaders/effects/snow_simple.glsl.frag.h"
#include "../../gfx/drivers/gl3_glsl_shaders/effects/bokeh.glsl.frag.h"
#include "../../gfx/drivers/gl3_glsl_shaders/effects/snowflake.glsl.frag.h"
#include "../../gfx/drivers/gl3_glsl_shaders/effects/snow.glsl.vert.h"
#include "../../gfx/drivers/gl2_glsl_shaders/effects/snow.glsl.vert.h"
#endif
#endif
#endif

#if defined(HAVE_VULKAN) || (defined(HAVE_OPENGL_CORE) && defined(HAVE_SLANG))
#define XMB_EFFECTS_SPIRV
#include "../../gfx/common/spirv_shaders.h"
#endif

#if defined(HAVE_D3D9) && (defined(HAVE_HLSL) || defined(HAVE_CG))
#define XMB_EFFECTS_SM3
#include "../../gfx/drivers/d3d_sm3_shaders/effects/shaders_common.h"
#include "../../gfx/drivers/d3d_sm3_shaders/effects/ribbon_sm3.hlsl.h"
#include "../../gfx/drivers/d3d_sm3_shaders/effects/ribbon_simple_sm3.hlsl.h"
#include "../../gfx/drivers/d3d_sm3_shaders/effects/simple_snow_sm3.hlsl.h"
#include "../../gfx/drivers/d3d_sm3_shaders/effects/snow_sm3.hlsl.h"
#include "../../gfx/drivers/d3d_sm3_shaders/effects/bokeh_sm3.hlsl.h"
#include "../../gfx/drivers/d3d_sm3_shaders/effects/snowflake_sm3.hlsl.h"
#endif

#if defined(HAVE_D3D10) || defined(HAVE_D3D11) || defined(HAVE_D3D12)
#define XMB_EFFECTS_SM4
#endif

#ifdef WIIU
#include "../../gfx/drivers/gx2_shaders/effects/menu_shaders.h"
#endif

#if defined(HAVE_LIBNX) && defined(__SWITCH2__)
#define XMB_EFFECTS_SM86
#include "../../gfx/drivers/deko3d_shaders/menu_effects_sm86.h"
#elif defined(HAVE_LIBNX)
#define XMB_EFFECTS_DKSH
#include "../../gfx/drivers/deko3d_shaders/menu_effects_dksh.h"
#endif

/* Cg for GL, which gl2 draws with where it has no GLSL */
#if defined(HAVE_CG) && defined(HAVE_OPENGL) && !defined(HAVE_OPENGLES)
#define XMB_EFFECTS_CG_GL
#include "../../gfx/drivers/gl_cg_shaders/effects/ribbon.cg.h"
#include "../../gfx/drivers/gl_cg_shaders/effects/ribbon_simple.cg.h"
#include "../../gfx/drivers/gl_cg_shaders/effects/snow_simple.cg.h"
#include "../../gfx/drivers/gl_cg_shaders/effects/snow_heavy.cg.h"
#include "../../gfx/drivers/gl_cg_shaders/effects/bokeh.cg.h"
#include "../../gfx/drivers/gl_cg_shaders/effects/snowflake.cg.h"
#endif

enum xmb_effect
{
   XMB_EFFECT_RIBBON = 0,
   XMB_EFFECT_RIBBON_SIMPLE,
   XMB_EFFECT_SNOW_SIMPLE,
   XMB_EFFECT_SNOW,
   XMB_EFFECT_BOKEH,
   XMB_EFFECT_SNOWFLAKE,
   XMB_EFFECT_COUNT
};

static gfx_display_effect_desc_t xmb_effect_descs[XMB_EFFECT_COUNT];

#ifdef __APPLE__
static const char *const xmb_effect_snow_constant_names[3] = {
   "snowBaseScale", "snowDensity", "snowSpeed" };
static const float xmb_effect_snow_simple_constants[3] = {
   1.25f, 0.50f, 0.15f };
static const float xmb_effect_snow_constants[3] = {
   3.50f, 0.70f, 0.25f };
#endif

/* Fills the descriptions once: some sources are assembled at run time */
static void xmb_effects_describe(void)
{
   static bool described;
   gfx_display_effect_desc_t *d = xmb_effect_descs;
   unsigned i;

   if (described)
      return;
   described = true;
   memset(xmb_effect_descs, 0, sizeof(xmb_effect_descs));

   d[XMB_EFFECT_RIBBON].flags        = GFX_EFFECT_MESH
      | GFX_EFFECT_BLEND_ADD | GFX_EFFECT_RIBBON;
   d[XMB_EFFECT_RIBBON_SIMPLE].flags = GFX_EFFECT_MESH
      | GFX_EFFECT_BLEND_SCREEN;
   d[XMB_EFFECT_BOKEH].flags         = GFX_EFFECT_YFLIP;
   d[XMB_EFFECT_SNOWFLAKE].flags     = GFX_EFFECT_YFLIP;

#if defined(HAVE_GLSL)
#if defined(VITA)
   for (i = 0; i < 2; i++)
   {
      d[XMB_EFFECT_RIBBON].glsl_vertex[i]          = stock_vertex_xmb_ribbon_modern();
      d[XMB_EFFECT_RIBBON].glsl_fragment[i]        = stock_fragment_xmb();
      d[XMB_EFFECT_RIBBON_SIMPLE].glsl_vertex[i]   = stock_vertex_xmb_simple_modern;
      d[XMB_EFFECT_RIBBON_SIMPLE].glsl_fragment[i] = stock_fragment_xmb_ribbon_simple;
   }
#else
   d[XMB_EFFECT_RIBBON].glsl_vertex[0]          = stock_vertex_xmb_ribbon_legacy();
   d[XMB_EFFECT_RIBBON].glsl_fragment[0]        = stock_fragment_xmb();
   d[XMB_EFFECT_RIBBON].glsl_vertex[1]          = stock_vertex_xmb_ribbon_modern();
   d[XMB_EFFECT_RIBBON].glsl_fragment[1]        = core_stock_fragment_xmb();
   d[XMB_EFFECT_RIBBON_SIMPLE].glsl_vertex[0]   = stock_vertex_xmb_ribbon_simple_legacy;
   d[XMB_EFFECT_RIBBON_SIMPLE].glsl_fragment[0] = stock_fragment_xmb_ribbon_simple;
   d[XMB_EFFECT_RIBBON_SIMPLE].glsl_vertex[1]   = stock_vertex_xmb_simple_modern;
   d[XMB_EFFECT_RIBBON_SIMPLE].glsl_fragment[1] = stock_fragment_xmb_ribbon_simple_core;
#if defined(HAVE_OPENGLES)
   for (i = 0; i < 2; i++)
   {
      d[XMB_EFFECT_SNOW_SIMPLE].glsl_vertex[i]   = stock_vertex_xmb_snow;
      d[XMB_EFFECT_SNOW_SIMPLE].glsl_fragment[i] = stock_fragment_xmb_simple_snow;
      d[XMB_EFFECT_SNOW].glsl_vertex[i]          = stock_vertex_xmb_snow;
      d[XMB_EFFECT_SNOW].glsl_fragment[i]        = stock_fragment_xmb_snow;
      d[XMB_EFFECT_BOKEH].glsl_vertex[i]         = stock_vertex_xmb_snow;
      d[XMB_EFFECT_BOKEH].glsl_fragment[i]       = stock_fragment_xmb_bokeh;
      d[XMB_EFFECT_SNOWFLAKE].glsl_vertex[i]     = stock_vertex_xmb_snow;
      d[XMB_EFFECT_SNOWFLAKE].glsl_fragment[i]   = stock_fragment_xmb_snowflake;
   }
#else
   for (i = XMB_EFFECT_SNOW_SIMPLE; i < XMB_EFFECT_COUNT; i++)
   {
      d[i].glsl_vertex[0] = stock_vertex_xmb_snow_legacy;
      d[i].glsl_vertex[1] = stock_vertex_xmb_snow_core;
   }
   d[XMB_EFFECT_SNOW_SIMPLE].glsl_fragment[0] = stock_fragment_xmb_simple_snow;
   d[XMB_EFFECT_SNOW_SIMPLE].glsl_fragment[1] = stock_fragment_xmb_simple_snow_core;
   d[XMB_EFFECT_SNOW].glsl_fragment[0]        = stock_fragment_xmb_snow;
   d[XMB_EFFECT_SNOW].glsl_fragment[1]        = stock_fragment_xmb_snow_core;
   d[XMB_EFFECT_BOKEH].glsl_fragment[0]       = stock_fragment_xmb_bokeh;
   d[XMB_EFFECT_BOKEH].glsl_fragment[1]       = stock_fragment_xmb_bokeh_core;
   d[XMB_EFFECT_SNOWFLAKE].glsl_fragment[0]   = stock_fragment_xmb_snowflake;
   d[XMB_EFFECT_SNOWFLAKE].glsl_fragment[1]   = stock_fragment_xmb_snowflake_core;
#endif
#endif
#endif

#ifdef XMB_EFFECTS_SPIRV
   d[XMB_EFFECT_RIBBON].spirv_vertex            = spirv_pipeline_ribbon_vert;
   d[XMB_EFFECT_RIBBON].spirv_vertex_size       = spirv_pipeline_ribbon_vert_size;
   d[XMB_EFFECT_RIBBON].spirv_fragment          = spirv_pipeline_ribbon_frag;
   d[XMB_EFFECT_RIBBON].spirv_fragment_size     = spirv_pipeline_ribbon_frag_size;
   d[XMB_EFFECT_RIBBON_SIMPLE].spirv_vertex        = spirv_pipeline_ribbon_simple_vert;
   d[XMB_EFFECT_RIBBON_SIMPLE].spirv_vertex_size   = spirv_pipeline_ribbon_simple_vert_size;
   d[XMB_EFFECT_RIBBON_SIMPLE].spirv_fragment      = spirv_pipeline_ribbon_simple_frag;
   d[XMB_EFFECT_RIBBON_SIMPLE].spirv_fragment_size = spirv_pipeline_ribbon_simple_frag_size;
   for (i = XMB_EFFECT_SNOW_SIMPLE; i < XMB_EFFECT_COUNT; i++)
   {
      d[i].spirv_vertex      = spirv_alpha_blend_vert;
      d[i].spirv_vertex_size = spirv_alpha_blend_vert_size;
   }
   d[XMB_EFFECT_SNOW_SIMPLE].spirv_fragment      = spirv_pipeline_snow_simple_frag;
   d[XMB_EFFECT_SNOW_SIMPLE].spirv_fragment_size = spirv_pipeline_snow_simple_frag_size;
   d[XMB_EFFECT_SNOW].spirv_fragment             = spirv_pipeline_snow_frag;
   d[XMB_EFFECT_SNOW].spirv_fragment_size        = spirv_pipeline_snow_frag_size;
   d[XMB_EFFECT_BOKEH].spirv_fragment            = spirv_pipeline_bokeh_frag;
   d[XMB_EFFECT_BOKEH].spirv_fragment_size       = spirv_pipeline_bokeh_frag_size;
   d[XMB_EFFECT_SNOWFLAKE].spirv_fragment        = spirv_pipeline_snowflake_frag;
   d[XMB_EFFECT_SNOWFLAKE].spirv_fragment_size   = spirv_pipeline_snowflake_frag_size;
#endif

#ifdef XMB_EFFECTS_SM3
   d[XMB_EFFECT_RIBBON].hlsl_sm3        = hlsl_ribbon_program();
   d[XMB_EFFECT_RIBBON_SIMPLE].hlsl_sm3 = hlsl_ribbon_simple_program;
   d[XMB_EFFECT_SNOW_SIMPLE].hlsl_sm3   = hlsl_simple_snow_program;
   d[XMB_EFFECT_SNOW].hlsl_sm3          = hlsl_snow_program;
   d[XMB_EFFECT_BOKEH].hlsl_sm3         = hlsl_bokeh_program;
   d[XMB_EFFECT_SNOWFLAKE].hlsl_sm3     = hlsl_snowflake_program;
#endif

#ifdef XMB_EFFECTS_SM4
   d[XMB_EFFECT_RIBBON].d3d_program = "ribbon";
   d[XMB_EFFECT_RIBBON_SIMPLE].d3d_program = "ribbon_simple";
   d[XMB_EFFECT_SNOW_SIMPLE].d3d_program = "simple_snow";
   d[XMB_EFFECT_SNOW].d3d_program = "snow";
   d[XMB_EFFECT_BOKEH].d3d_program = "bokeh";
   d[XMB_EFFECT_SNOWFLAKE].d3d_program = "snowflake";
#endif

#ifdef XMB_EFFECTS_CG_GL
   d[XMB_EFFECT_RIBBON].cg_gl        = stock_xmb_ribbon();
   d[XMB_EFFECT_RIBBON_SIMPLE].cg_gl = stock_xmb_ribbon_simple;
   d[XMB_EFFECT_SNOW_SIMPLE].cg_gl   = stock_xmb_simple_snow;
   d[XMB_EFFECT_SNOW].cg_gl          = stock_xmb_snow_heavy;
   d[XMB_EFFECT_BOKEH].cg_gl         = stock_xmb_bokeh;
   d[XMB_EFFECT_SNOWFLAKE].cg_gl     = stock_xmb_snowflake;
#endif

#ifdef __APPLE__
   d[XMB_EFFECT_RIBBON].metal_vertex          = "ribbon_vertex";
   d[XMB_EFFECT_RIBBON].metal_fragment        = "ribbon_fragment";
   d[XMB_EFFECT_RIBBON_SIMPLE].metal_vertex   = "ribbon_simple_vertex";
   d[XMB_EFFECT_RIBBON_SIMPLE].metal_fragment = "ribbon_simple_fragment";
   for (i = XMB_EFFECT_SNOW_SIMPLE; i < XMB_EFFECT_COUNT; i++)
      d[i].metal_vertex = "stock_vertex";
   d[XMB_EFFECT_SNOW_SIMPLE].metal_fragment       = "snow_fragment";
   d[XMB_EFFECT_SNOW_SIMPLE].metal_constant_names = xmb_effect_snow_constant_names;
   d[XMB_EFFECT_SNOW_SIMPLE].metal_constants      = xmb_effect_snow_simple_constants;
   d[XMB_EFFECT_SNOW_SIMPLE].metal_constant_count = 3;
   d[XMB_EFFECT_SNOW].metal_fragment              = "snow_fragment";
   d[XMB_EFFECT_SNOW].metal_constant_names        = xmb_effect_snow_constant_names;
   d[XMB_EFFECT_SNOW].metal_constants             = xmb_effect_snow_constants;
   d[XMB_EFFECT_SNOW].metal_constant_count        = 3;
   d[XMB_EFFECT_BOKEH].metal_fragment             = "bokeh_fragment";
   d[XMB_EFFECT_SNOWFLAKE].metal_fragment         = "snowflake_fragment";
#endif

#ifdef XMB_EFFECTS_DKSH
#define XMB_DKSH(e, v, f) \
   d[e].dksh_vertex        = v; \
   d[e].dksh_vertex_size   = sizeof(v); \
   d[e].dksh_fragment      = f; \
   d[e].dksh_fragment_size = sizeof(f)
   XMB_DKSH(XMB_EFFECT_RIBBON, deko3d_fx_ribbon_vert_dksh,
         deko3d_fx_ribbon_frag_dksh);
   XMB_DKSH(XMB_EFFECT_RIBBON_SIMPLE, deko3d_fx_ribbon_simple_vert_dksh,
         deko3d_fx_ribbon_simple_frag_dksh);
   XMB_DKSH(XMB_EFFECT_SNOW_SIMPLE, deko3d_fx_quad_vert_dksh,
         deko3d_fx_snow_simple_frag_dksh);
   XMB_DKSH(XMB_EFFECT_SNOW, deko3d_fx_quad_vert_dksh,
         deko3d_fx_snow_frag_dksh);
   XMB_DKSH(XMB_EFFECT_BOKEH, deko3d_fx_quad_vert_dksh,
         deko3d_fx_bokeh_frag_dksh);
   XMB_DKSH(XMB_EFFECT_SNOWFLAKE, deko3d_fx_quad_vert_dksh,
         deko3d_fx_snowflake_frag_dksh);
#undef XMB_DKSH
#endif

#ifdef XMB_EFFECTS_SM86
#define XMB_SM86(e, v, vg, f, fg) \
   d[e].sm86_vertex        = v; \
   d[e].sm86_vertex_size   = sizeof(v); \
   d[e].sm86_vertex_gprs   = vg; \
   d[e].sm86_fragment      = f; \
   d[e].sm86_fragment_size = sizeof(f); \
   d[e].sm86_fragment_gprs = fg
   XMB_SM86(XMB_EFFECT_RIBBON,
         nxg_s2_fx_ribbon_vert, NXG_S2_FX_RIBBON_VERT_GPRS,
         nxg_s2_fx_ribbon_frag, NXG_S2_FX_RIBBON_FRAG_GPRS);
   XMB_SM86(XMB_EFFECT_RIBBON_SIMPLE,
         nxg_s2_fx_ribbon_simple_vert, NXG_S2_FX_RIBBON_SIMPLE_VERT_GPRS,
         nxg_s2_fx_ribbon_simple_frag, NXG_S2_FX_RIBBON_SIMPLE_FRAG_GPRS);
   XMB_SM86(XMB_EFFECT_SNOW_SIMPLE,
         nxg_s2_fx_quad_vert, NXG_S2_FX_QUAD_VERT_GPRS,
         nxg_s2_fx_snow_simple_frag, NXG_S2_FX_SNOW_SIMPLE_FRAG_GPRS);
   XMB_SM86(XMB_EFFECT_SNOW,
         nxg_s2_fx_quad_vert, NXG_S2_FX_QUAD_VERT_GPRS,
         nxg_s2_fx_snow_frag, NXG_S2_FX_SNOW_FRAG_GPRS);
   XMB_SM86(XMB_EFFECT_BOKEH,
         nxg_s2_fx_quad_vert, NXG_S2_FX_QUAD_VERT_GPRS,
         nxg_s2_fx_bokeh_frag, NXG_S2_FX_BOKEH_FRAG_GPRS);
   XMB_SM86(XMB_EFFECT_SNOWFLAKE,
         nxg_s2_fx_quad_vert, NXG_S2_FX_QUAD_VERT_GPRS,
         nxg_s2_fx_snowflake_frag, NXG_S2_FX_SNOWFLAKE_FRAG_GPRS);
#undef XMB_SM86
#endif

#ifdef WIIU
   d[XMB_EFFECT_RIBBON].gx2        = &ribbon_shader;
   d[XMB_EFFECT_RIBBON_SIMPLE].gx2 = &ribbon_simple_shader;
   d[XMB_EFFECT_SNOW_SIMPLE].gx2   = &snow_simple_shader;
   d[XMB_EFFECT_SNOW].gx2          = &snow_shader;
   d[XMB_EFFECT_BOKEH].gx2         = &bokeh_shader;
   d[XMB_EFFECT_SNOWFLAKE].gx2     = &snowflake_shader;
#endif
   (void)i;
}

#endif /* HAVE_SHADERPIPELINE */

#endif
