#pragma once
#include <wiiu/types.h>
#include <wiiu/gx2r/buffer.h>
#include "enum.h"
#include "sampler.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GX2FetchShader
{
   GX2FetchShaderType type;

   struct
   {
      uint32_t sq_pgm_resources_fs;
   } regs;

   uint32_t size;
   uint8_t *program;
   uint32_t attribCount;
   uint32_t numDivisors;
   uint32_t divisors[2];
} GX2FetchShader;

typedef struct GX2UniformBlock
{
   const char *name;
   uint32_t offset;
   uint32_t size;
} GX2UniformBlock;

typedef struct GX2UniformVar
{
   const char *name;
   GX2ShaderVarType type;
   uint32_t count;
   uint32_t offset;
   int32_t block;
} GX2UniformVar;

typedef struct GX2UniformInitialValue
{
   float value[4];
   uint32_t offset;
} GX2UniformInitialValue;

typedef struct GX2LoopVar
{
   uint32_t offset;
   uint32_t value;
} GX2LoopVar;

typedef struct GX2SamplerVar
{
   const char *name;
   GX2SamplerVarType type;
   uint32_t location;
} GX2SamplerVar;

typedef struct GX2AttribVar
{
   const char *name;
   GX2ShaderVarType type;
   uint32_t count;
   uint32_t location;
} GX2AttribVar;

typedef struct GX2VertexShader
{
   union
   {
      struct
      {
         struct
         {
            unsigned : 2;
            bool prime_cache_on_const : 1;
            bool prime_cache_enable : 1;
            bool uncached_first_inst : 1;
            unsigned  fetch_cache_lines : 3;
            bool prime_cache_on_draw : 1;
            bool prime_cache_pgm_en : 1;
            bool dx10_clamp : 1;
            unsigned : 5;
            unsigned stack_size : 8;
            unsigned num_gprs : 8;
         } sq_pgm_resources_vs;

         struct
         {
            unsigned : 31;
            unsigned enable: 1;
         } vgt_primitiveid_en;

         struct
         {
            unsigned : 18;
            unsigned vs_out_fog_vec_addr : 5;
            bool vs_exports_fog : 1;
            unsigned : 2;
            unsigned vs_export_count : 5;
            bool vs_per_component : 1;
         } spi_vs_out_config;

         uint32_t num_spi_vs_out_id;
         struct
         {
            uint8_t semantic_3;
            uint8_t semantic_2;
            uint8_t semantic_1;
            uint8_t semantic_0;
         } spi_vs_out_id[10];
         struct
         {
            bool clip_dist_ena_7 : 1;
            bool clip_dist_ena_6 : 1;
            bool clip_dist_ena_5 : 1;
            bool clip_dist_ena_4 : 1;
            bool clip_dist_ena_3 : 1;
            bool clip_dist_ena_2 : 1;
            bool clip_dist_ena_1 : 1;
            bool clip_dist_ena_0 : 1;
            bool cull_dist_ena_7 : 1;
            bool cull_dist_ena_6 : 1;
            bool cull_dist_ena_5 : 1;
            bool cull_dist_ena_0 : 1;
            bool cull_dist_ena_4 : 1;
            bool cull_dist_ena_3 : 1;
            bool cull_dist_ena_2 : 1;
            bool cull_dist_ena_1 : 1;
            bool vs_out_misc_side_bus_ena : 1;
            bool vs_out_ccdist1_vec_ena : 1;
            bool vs_out_ccdist0_vec_ena : 1;
            bool vs_out_misc_vec_ena : 1;
            bool use_vtx_kill_flag : 1;
            bool use_vtx_viewport_indx : 1;
            bool use_vtx_render_target_indx : 1;
            bool use_vtx_edge_flag : 1;
            unsigned : 6;
            bool use_vtx_point_size : 1;
            bool use_vtx_gs_cut_flag : 1;
         } pa_cl_vs_out_cntl;
         uint32_t sq_vtx_semantic_clear;
         uint32_t num_sq_vtx_semantic;
         uint32_t sq_vtx_semantic[32]; /* 8 bit */
         struct
         {
            bool buffer_3_en : 1;
            bool buffer_2_en : 1;
            bool buffer_1_en : 1;
            bool buffer_0_en : 1;
         } vgt_strmout_buffer_en;
         struct
         {
            unsigned : 24;
            unsigned vtx_reuse_depth : 8;
         } vgt_vertex_reuse_block_cntl;
         struct
         {
            unsigned : 24;
            unsigned reuse_depth : 8;
         } vgt_hos_reuse_depth;
      };
      u32 vals[52];
   } regs;

   uint32_t size;
   uint8_t *program;
   GX2ShaderMode mode;

   uint32_t uniformBlockCount;
   GX2UniformBlock *uniformBlocks;

   uint32_t uniformVarCount;
   GX2UniformVar *uniformVars;

   uint32_t initialValueCount;
   GX2UniformInitialValue *initialValues;

   uint32_t loopVarCount;
   GX2LoopVar *loopVars;

   uint32_t samplerVarCount;
   GX2SamplerVar *samplerVars;

   uint32_t attribVarCount;
   GX2AttribVar *attribVars;

   uint32_t ringItemSize;

   BOOL hasStreamOut;
   uint32_t streamOutStride[4];

   GX2RBuffer gx2rBuffer;
} GX2VertexShader;

typedef enum
{
   spi_baryc_cntl_centroids_only        = 0,
   spi_baryc_cntl_centers_only          = 1,
   spi_baryc_cntl_centroids_and_centers = 2,
} spi_baryc_cntl;

typedef enum
{
   db_z_order_late_z              = 0,
   db_z_order_early_z_then_late_z = 1,
   db_z_order_re_z                = 2,
   db_z_order_early_z_then_re_z   = 3,
} db_z_order;

typedef struct GX2PixelShader
{
   union
   {
      struct
      {
         struct
         {
            unsigned : 2;
            bool prime_cache_on_const : 1;
            bool prime_cache_enable : 1;
            bool uncached_first_inst : 1;
            unsigned  fetch_cache_lines : 3;
            bool prime_cache_on_draw : 1;
            bool prime_cache_pgm_en : 1;
            bool dx10_clamp : 1;
            unsigned : 5;
            unsigned stack_size : 8;
            unsigned num_gprs : 8;
         } sq_pgm_resources_ps;

         struct
         {
            unsigned : 27;
            unsigned export_mode : 5;
         } sq_pgm_exports_ps;

         struct
         {
            bool baryc_at_sample_ena : 1;
            bool position_sample : 1;
            bool linear_gradient_ena : 1;
            bool persp_gradient_ena : 1;
            spi_baryc_cntl baryc_sample_cntl : 2;
            unsigned param_gen_addr : 7;
            unsigned param_gen : 4;
            unsigned position_addr : 5;
            bool position_centroid : 1;
            bool position_ena : 1;
            unsigned : 2;
            unsigned num_interp : 6;
         } spi_ps_in_control_0;

         struct
         {
            unsigned : 1;
            bool position_ulc : 1;
            unsigned fixed_pt_position_addr : 5;
            bool fixed_pt_position_ena : 1;
            unsigned fog_addr : 7;
            unsigned front_face_addr : 5;
            bool front_face_all_bits : 1;
            unsigned front_face_chan : 2;
            bool front_face_ena : 1;
            unsigned gen_index_pix_addr : 7;
            bool gen_index_pix : 1;
         } spi_ps_in_control_1;

         uint32_t num_spi_ps_input_cntl;

         struct
         {
            unsigned : 13;
            bool sel_sample : 1;
            bool pt_sprite_tex : 1;
            unsigned cyl_wrap : 4;
            bool sel_linear : 1;
            bool sel_centroid : 1;
            bool flat_shade : 1;
            unsigned default_val : 2;
            unsigned semantic : 8;
         } spi_ps_input_cntls[32];

         struct
         {
            unsigned output7_enable : 4;
            unsigned output6_enable : 4;
            unsigned output5_enable : 4;
            unsigned output4_enable : 4;
            unsigned output3_enable : 4;
            unsigned output2_enable : 4;
            unsigned output1_enable : 4;
            unsigned output0_enable : 4;
         } cb_shader_mask;
         struct
         {
            unsigned : 24;
            bool rt7_enable : 1;
            bool rt6_enable : 1;
            bool rt5_enable : 1;
            bool rt4_enable : 1;
            bool rt3_enable : 1;
            bool rt2_enable : 1;
            bool rt1_enable : 1;
            bool rt0_enable : 1;
         } cb_shader_control;
         struct
         {
            unsigned : 19;
            bool alpha_to_mask_disable : 1;
            bool exec_on_noop : 1;
            bool exec_on_hier_fail : 1;
            bool dual_export_enable : 1;
            bool mask_export_enable : 1;
            bool coverage_to_mask_enable : 1;
            bool kill_enable : 1;
            db_z_order z_order : 2;
            unsigned : 2;
            bool z_export_enable : 1;
            bool stencil_ref_export_enable : 1;
         } db_shader_control;

         bool spi_input_z;
      };
      u32 vals[41];
   } regs;

   uint32_t size;
   uint8_t *program;
   GX2ShaderMode mode;

   uint32_t uniformBlockCount;
   GX2UniformBlock *uniformBlocks;

   uint32_t uniformVarCount;
   GX2UniformVar *uniformVars;

   uint32_t initialValueCount;
   GX2UniformInitialValue *initialValues;

   uint32_t loopVarCount;
   GX2LoopVar *loopVars;

   uint32_t samplerVarCount;
   GX2SamplerVar *samplerVars;

   GX2RBuffer gx2rBuffer;
} GX2PixelShader;

typedef enum
{
   VGT_GS_OUT_PRIMITIVE_TYPE_POINTLIST = 0,
   VGT_GS_OUT_PRIMITIVE_TYPE_LINESTRIP = 1,
   VGT_GS_OUT_PRIMITIVE_TYPE_TRISTRIP  = 2,
   VGT_GS_OUT_PRIMITIVE_TYPE_MAX_ENUM  = 0xFFFFFFFF
} vgt_gs_out_primitive_type;

typedef enum
{
   VGT_GS_ENABLE_MODE_OFF        = 0,
   VGT_GS_ENABLE_MODE_SCENARIO_A = 1,
   VGT_GS_ENABLE_MODE_SCENARIO_B = 2,
   VGT_GS_ENABLE_MODE_SCENARIO_G = 3,
} vgt_gs_enable_mode;

typedef enum
{
   VGT_GS_CUT_MODE_1024 = 0,
   VGT_GS_CUT_MODE_512  = 1,
   VGT_GS_CUT_MODE_256  = 2,
   VGT_GS_CUT_MODE_128  = 3,
} vgt_gs_cut_mode;

typedef struct GX2GeometryShader
{
   union
   {
      struct
      {
         struct
         {
            unsigned : 2;
            bool prime_cache_on_const : 1;
            bool prime_cache_enable : 1;
            bool uncached_first_inst : 1;
            unsigned  fetch_cache_lines : 3;
            bool prime_cache_on_draw : 1;
            bool prime_cache_pgm_en : 1;
            bool dx10_clamp : 1;
            unsigned : 5;
            unsigned stack_size : 8;
            unsigned num_gprs : 8;
         } sq_pgm_resources_gs;
         vgt_gs_out_primitive_type vgt_gs_out_prim_type;
         struct
         {
            unsigned : 14;
            bool partial_thd_at_eoi : 1;
            bool element_info_en : 1;
            bool fast_compute_mode : 1;
            bool compute_mode : 1;
            unsigned : 2;
            bool gs_c_pack_en : 1;
            unsigned : 2;
            bool mode_hi : 1;
            unsigned : 3;
            vgt_gs_cut_mode cut_mode : 2;
            bool es_passthru : 1;
            vgt_gs_enable_mode mode : 2;
         } vgt_gs_mode;
         struct
         {
            bool clip_dist_ena_7 : 1;
            bool clip_dist_ena_6 : 1;
            bool clip_dist_ena_5 : 1;
            bool clip_dist_ena_4 : 1;
            bool clip_dist_ena_3 : 1;
            bool clip_dist_ena_2 : 1;
            bool clip_dist_ena_1 : 1;
            bool clip_dist_ena_0 : 1;
            bool cull_dist_ena_7 : 1;
            bool cull_dist_ena_6 : 1;
            bool cull_dist_ena_5 : 1;
            bool cull_dist_ena_0 : 1;
            bool cull_dist_ena_4 : 1;
            bool cull_dist_ena_3 : 1;
            bool cull_dist_ena_2 : 1;
            bool cull_dist_ena_1 : 1;
            bool vs_out_misc_side_bus_ena : 1;
            bool vs_out_ccdist1_vec_ena : 1;
            bool vs_out_ccdist0_vec_ena : 1;
            bool vs_out_misc_vec_ena : 1;
            bool use_vtx_kill_flag : 1;
            bool use_vtx_viewport_indx : 1;
            bool use_vtx_render_target_indx : 1;
            bool use_vtx_edge_flag : 1;
            unsigned : 6;
            bool use_vtx_point_size : 1;
            bool use_vtx_gs_cut_flag : 1;
         } pa_cl_vs_out_cntl;
         struct
         {
            unsigned : 2;
            bool prime_cache_on_const : 1;
            bool prime_cache_enable : 1;
            bool uncached_first_inst : 1;
            unsigned  fetch_cache_lines : 3;
            bool prime_cache_on_draw : 1;
            bool prime_cache_pgm_en : 1;
            bool dx10_clamp : 1;
            unsigned : 5;
            unsigned stack_size : 8;
            unsigned num_gprs : 8;
         } sq_pgm_resources_vs;

         uint32_t sq_gs_vert_itemsize; /* 15-bit */

         struct
         {
            unsigned : 18;
            unsigned vs_out_fog_vec_addr : 5;
            bool vs_exports_fog : 1;
            unsigned : 2;
            unsigned vs_export_count : 5;
            bool vs_per_component : 1;
         } spi_vs_out_config;

         uint32_t num_spi_vs_out_id;

         struct
         {
            uint8_t semantic_3;
            uint8_t semantic_2;
            uint8_t semantic_1;
            uint8_t semantic_0;
         } spi_vs_out_id[10];

         struct
         {
            bool buffer_3_en : 1;
            bool buffer_2_en : 1;
            bool buffer_1_en : 1;
            bool buffer_0_en : 1;
         } vgt_strmout_buffer_en;
      };
      u32 vals[19];
   } regs;
   uint32_t size;
   uint8_t *program;
   uint32_t copyProgramSize;
   uint8_t *copyProgram;
   GX2ShaderMode mode;

   uint32_t uniformBlockCount;
   GX2UniformBlock *uniformBlocks;

   uint32_t uniformVarCount;
   GX2UniformVar *uniformVars;

   uint32_t initialValueCount;
   GX2UniformInitialValue *initialValues;

   uint32_t loopVarCount;
   GX2LoopVar *loopVars;

   uint32_t samplerVarCount;
   GX2SamplerVar *samplerVars;

   uint32_t ringItemSize;
   BOOL hasStreamOut;
   uint32_t streamOutStride[4];

   GX2RBuffer gx2rBuffer;
} GX2GeometryShader;

typedef struct GX2AttribStream
{
   uint32_t location;
   uint32_t buffer;
   uint32_t offset;
   GX2AttribFormat format;
   GX2AttribIndexType type;
   uint32_t aluDivisor;
   uint32_t mask;
   GX2EndianSwapMode endianSwap;
} GX2AttribStream;

uint32_t GX2CalcGeometryShaderInputRingBufferSize(uint32_t ringItemSize);
uint32_t GX2CalcGeometryShaderOutputRingBufferSize(uint32_t ringItemSize);

uint32_t GX2CalcFetchShaderSizeEx(uint32_t attribs, GX2FetchShaderType fetchShaderType,
                                  GX2TessellationMode tesellationMode);

void GX2InitFetchShaderEx(GX2FetchShader *fetchShader, uint8_t *buffer, uint32_t attribCount,
                          GX2AttribStream *attribs, GX2FetchShaderType type, GX2TessellationMode tessMode);

void GX2SetFetchShader(GX2FetchShader *shader);
void GX2SetVertexShader(GX2VertexShader *shader);
void GX2SetPixelShader(GX2PixelShader *shader);
void GX2SetGeometryShader(GX2GeometryShader *shader);

void GX2SetVertexSampler(GX2Sampler *sampler, uint32_t id);
void GX2SetPixelSampler(GX2Sampler *sampler, uint32_t id);
void GX2SetGeometrySampler(GX2Sampler *sampler, uint32_t id);
void GX2SetVertexUniformReg(uint32_t offset, uint32_t count, uint32_t *data);
void GX2SetPixelUniformReg(uint32_t offset, uint32_t count, uint32_t *data);
void GX2SetVertexUniformBlock(uint32_t location, uint32_t size, const void *data);
void GX2SetPixelUniformBlock(uint32_t location, uint32_t size, const void *data);
void GX2SetGeometryUniformBlock(uint32_t location, uint32_t size, const void *data);

void GX2SetShaderModeEx(GX2ShaderMode mode,
                        uint32_t numVsGpr, uint32_t numVsStackEntries,
                        uint32_t numGsGpr, uint32_t numGsStackEntries,
                        uint32_t numPsGpr, uint32_t numPsStackEntries);

static inline void GX2SetShaderMode(GX2ShaderMode mode)
{
   if (mode == GX2_SHADER_MODE_GEOMETRY_SHADER)
      GX2SetShaderModeEx(mode, 44, 32, 64, 48, 76, 176);
   else
      GX2SetShaderModeEx(mode, 48, 64, 0, 0, 200, 192);
}

void GX2SetStreamOutEnable(BOOL enable);
void GX2SetGeometryShaderInputRingBuffer(void *buffer, uint32_t size);
void GX2SetGeometryShaderOutputRingBuffer(void *buffer, uint32_t size);

uint32_t GX2GetPixelShaderGPRs(GX2PixelShader *shader);
uint32_t GX2GetPixelShaderStackEntries(GX2PixelShader *shader);
uint32_t GX2GetVertexShaderGPRs(GX2VertexShader *shader);
uint32_t GX2GetVertexShaderStackEntries(GX2VertexShader *shader);
uint32_t GX2GetGeometryShaderGPRs(GX2GeometryShader *shader);
uint32_t GX2GetGeometryShaderStackEntries(GX2GeometryShader *shader);

#ifdef __cplusplus
}
#endif

/** @} */
