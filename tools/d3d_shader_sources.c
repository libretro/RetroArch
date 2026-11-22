/* Offline source extraction; compile as C, never part of the frontend. */
#include <stdio.h>
#include <string.h>
#include "../gfx/common/menu_ribbon_shader.h"
static const char shader_opaque[] =
#include "../gfx/drivers/d3d_sm5_shaders/opaque_sm5.hlsl.h"
;
static const char shader_mesh[] =
#include "../gfx/drivers/d3d_sm4_shaders/mesh_sm4.hlsl.h"
;
static const char shader_sprite[] =
#include "../gfx/drivers/d3d_sm4_shaders/sprite_sm4.hlsl.h"
;
static const char shader_views[] =
#include "../gfx/drivers/d3d_sm4_shaders/views_sm4.hlsl.h"
;
static const char shader_hdr[] =
#include "../gfx/drivers/d3d_sm5_shaders/hdr_sm5.hlsl.h"
;
static const char shader_mipmapgen[] =
#include "../gfx/drivers/d3d_sm5_shaders/mipmapgen_sm5.h"
;
static const char shader_planar11[] =
#include "../gfx/drivers/d3d_sm5_shaders/planar_d3d11.hlsl.h"
;
#define D3D11_PLANAR_ARRAY
static const char shader_planar11_array[] =
#include "../gfx/drivers/d3d_sm5_shaders/planar_d3d11.hlsl.h"
;
#undef D3D11_PLANAR_ARRAY
static const char shader_planar12[] =
#include "../gfx/drivers/d3d_sm5_shaders/planar_d3d12.hlsl.h"
;
static const char shader_nv12[] =
#include "../gfx/drivers/d3d_sm5_shaders/nv12_d3d12.hlsl.h"
;
static const char shader_ribbon_simple[] =
#include "../gfx/drivers/d3d_sm4_shaders/effects/ribbon_simple_sm4.hlsl.h"
;
static const char shader_simple_snow[] =
#include "../gfx/drivers/d3d_sm4_shaders/effects/simple_snow_sm4.hlsl.h"
;
static const char shader_snow[] =
#include "../gfx/drivers/d3d_sm4_shaders/effects/snow_sm4.hlsl.h"
;
static const char shader_bokeh[] =
#include "../gfx/drivers/d3d_sm4_shaders/effects/bokeh_sm4.hlsl.h"
;
static const char shader_snowflake[] =
#include "../gfx/drivers/d3d_sm4_shaders/effects/snowflake_sm4.hlsl.h"
;
#include "../gfx/drivers/d3d_sm4_shaders/effects/bloom_sm4.hlsl.h"
static const char *ribbon_source(void) { static char ribbon[4096]; return
#include "../gfx/drivers/d3d_sm4_shaders/effects/ribbon_sm4.hlsl.h"
;}
int main(int argc, char **argv) {
   const char *source = NULL;
   if (argc != 2) return 2;
   if (!strcmp(argv[1], "opaque")) source = shader_opaque;
   if (!strcmp(argv[1], "mesh")) source = shader_mesh;
   if (!strcmp(argv[1], "sprite")) source = shader_sprite;
   if (!strcmp(argv[1], "views")) source = shader_views;
   if (!strcmp(argv[1], "hdr")) source = shader_hdr;
   if (!strcmp(argv[1], "mipmapgen")) source = shader_mipmapgen;
   if (!strcmp(argv[1], "planar11")) source = shader_planar11;
   if (!strcmp(argv[1], "planar11_array")) source = shader_planar11_array;
   if (!strcmp(argv[1], "planar12")) source = shader_planar12;
   if (!strcmp(argv[1], "nv12")) source = shader_nv12;
   if (!strcmp(argv[1], "ribbon_simple")) source = shader_ribbon_simple;
   if (!strcmp(argv[1], "simple_snow")) source = shader_simple_snow;
   if (!strcmp(argv[1], "snow")) source = shader_snow;
   if (!strcmp(argv[1], "bokeh")) source = shader_bokeh;
   if (!strcmp(argv[1], "snowflake")) source = shader_snowflake;
   if (!strcmp(argv[1], "ribbon")) source = ribbon_source();
   if (!strcmp(argv[1], "bloom_vs")) source = bloom_vs_source;
   if (!strcmp(argv[1], "bloom_ps")) source = bloom_ps_source;
   if (!source) return 2;
   return fwrite(source, 1, strlen(source), stdout) != strlen(source);
}
