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
   struct
   {
      uint32_t sq_pgm_resources_vs;
      uint32_t vgt_primitiveid_en;
      uint32_t spi_vs_out_config;
      uint32_t num_spi_vs_out_id;
      uint32_t spi_vs_out_id[10];
      uint32_t pa_cl_vs_out_cntl;
      uint32_t sq_vtx_semantic_clear;
      uint32_t num_sq_vtx_semantic;
      uint32_t sq_vtx_semantic[32];
      uint32_t vgt_strmout_buffer_en;
      uint32_t vgt_vertex_reuse_block_cntl;
      uint32_t vgt_hos_reuse_depth;
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

   uint32_t ringItemsize;

   BOOL hasStreamOut;
   uint32_t streamOutStride[4];

   GX2RBuffer gx2rBuffer;
} GX2VertexShader;

typedef struct GX2PixelShader
{
   struct
   {
      uint32_t sq_pgm_resources_ps;
      uint32_t sq_pgm_exports_ps;
      uint32_t spi_ps_in_control_0;
      uint32_t spi_ps_in_control_1;
      uint32_t num_spi_ps_input_cntl;
      uint32_t spi_ps_input_cntls[32];
      uint32_t cb_shader_mask;
      uint32_t cb_shader_control;
      uint32_t db_shader_control;
      uint32_t spi_input_z;
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

typedef struct GX2GeometryShader
{
   struct
   {
      uint32_t sq_pgm_resources_gs;
      uint32_t vgt_gs_out_prim_type;
      uint32_t vgt_gs_mode;
      uint32_t pa_cl_vs_out_cntl;
      uint32_t sq_pgm_resources_vs;
      uint32_t sq_gs_vert_itemsize;
      uint32_t spi_vs_out_config;
      uint32_t num_spi_vs_out_id;
      uint32_t spi_vs_out_id[10];
      uint32_t vgt_strmout_buffer_en;
   } regs;

   uint32_t size;
   uint8_t *program;
   uint32_t vertexProgramSize;
   uint8_t *vertexProgram;
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
