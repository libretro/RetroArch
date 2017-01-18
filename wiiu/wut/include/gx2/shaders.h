#pragma once
#include <wut.h>
#include "enum.h"
#include "sampler.h"
#include "gx2r/buffer.h"

/**
 * \defgroup gx2_shader Shaders
 * \ingroup gx2
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GX2AttribVar GX2AttribVar;
typedef struct GX2AttribStream GX2AttribStream;
typedef struct GX2FetchShader GX2FetchShader;
typedef struct GX2GeometryShader GX2GeometryShader;
typedef struct GX2LoopVar GX2LoopVar;
typedef struct GX2PixelShader GX2PixelShader;
typedef struct GX2SamplerVar GX2SamplerVar;
typedef struct GX2UniformBlock GX2UniformBlock;
typedef struct GX2UniformVar GX2UniformVar;
typedef struct GX2UniformInitialValue GX2UniformInitialValue;
typedef struct GX2VertexShader GX2VertexShader;

struct GX2FetchShader
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
};
CHECK_OFFSET(GX2FetchShader, 0x0, type);
CHECK_OFFSET(GX2FetchShader, 0x4, regs.sq_pgm_resources_fs);
CHECK_OFFSET(GX2FetchShader, 0x8, size);
CHECK_OFFSET(GX2FetchShader, 0xc, program);
CHECK_OFFSET(GX2FetchShader, 0x10, attribCount);
CHECK_OFFSET(GX2FetchShader, 0x14, numDivisors);
CHECK_OFFSET(GX2FetchShader, 0x18, divisors);
CHECK_SIZE(GX2FetchShader, 0x20);

struct GX2UniformBlock
{
   const char *name;
   uint32_t offset;
   uint32_t size;
};
CHECK_OFFSET(GX2UniformBlock, 0x00, name);
CHECK_OFFSET(GX2UniformBlock, 0x04, offset);
CHECK_OFFSET(GX2UniformBlock, 0x08, size);
CHECK_SIZE(GX2UniformBlock, 0x0C);

struct GX2UniformVar
{
   const char *name;
   GX2ShaderVarType type;
   uint32_t count;
   uint32_t offset;
   int32_t block;
};
CHECK_OFFSET(GX2UniformVar, 0x00, name);
CHECK_OFFSET(GX2UniformVar, 0x04, type);
CHECK_OFFSET(GX2UniformVar, 0x08, count);
CHECK_OFFSET(GX2UniformVar, 0x0C, offset);
CHECK_OFFSET(GX2UniformVar, 0x10, block);
CHECK_SIZE(GX2UniformVar, 0x14);

struct GX2UniformInitialValue
{
   float value[4];
   uint32_t offset;
};
CHECK_OFFSET(GX2UniformInitialValue, 0x00, value);
CHECK_OFFSET(GX2UniformInitialValue, 0x10, offset);
CHECK_SIZE(GX2UniformInitialValue, 0x14);

struct GX2LoopVar
{
   uint32_t offset;
   uint32_t value;
};
CHECK_OFFSET(GX2LoopVar, 0x00, offset);
CHECK_OFFSET(GX2LoopVar, 0x04, value);
CHECK_SIZE(GX2LoopVar, 0x08);

struct GX2SamplerVar
{
   const char *name;
   GX2SamplerVarType type;
   uint32_t location;
};
CHECK_OFFSET(GX2SamplerVar, 0x00, name);
CHECK_OFFSET(GX2SamplerVar, 0x04, type);
CHECK_OFFSET(GX2SamplerVar, 0x08, location);
CHECK_SIZE(GX2SamplerVar, 0x0C);

struct GX2AttribVar
{
   const char *name;
   GX2ShaderVarType type;
   uint32_t count;
   uint32_t location;
};
CHECK_OFFSET(GX2AttribVar, 0x00, name);
CHECK_OFFSET(GX2AttribVar, 0x04, type);
CHECK_OFFSET(GX2AttribVar, 0x08, count);
CHECK_OFFSET(GX2AttribVar, 0x0C, location);
CHECK_SIZE(GX2AttribVar, 0x10);

struct GX2VertexShader
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
};
CHECK_OFFSET(GX2VertexShader, 0x00, regs.sq_pgm_resources_vs);
CHECK_OFFSET(GX2VertexShader, 0x04, regs.vgt_primitiveid_en);
CHECK_OFFSET(GX2VertexShader, 0x08, regs.spi_vs_out_config);
CHECK_OFFSET(GX2VertexShader, 0x0C, regs.num_spi_vs_out_id);
CHECK_OFFSET(GX2VertexShader, 0x10, regs.spi_vs_out_id);
CHECK_OFFSET(GX2VertexShader, 0x38, regs.pa_cl_vs_out_cntl);
CHECK_OFFSET(GX2VertexShader, 0x3C, regs.sq_vtx_semantic_clear);
CHECK_OFFSET(GX2VertexShader, 0x40, regs.num_sq_vtx_semantic);
CHECK_OFFSET(GX2VertexShader, 0x44, regs.sq_vtx_semantic);
CHECK_OFFSET(GX2VertexShader, 0xC4, regs.vgt_strmout_buffer_en);
CHECK_OFFSET(GX2VertexShader, 0xC8, regs.vgt_vertex_reuse_block_cntl);
CHECK_OFFSET(GX2VertexShader, 0xCC, regs.vgt_hos_reuse_depth);
CHECK_OFFSET(GX2VertexShader, 0xD0, size);
CHECK_OFFSET(GX2VertexShader, 0xD4, program);
CHECK_OFFSET(GX2VertexShader, 0xD8, mode);
CHECK_OFFSET(GX2VertexShader, 0xDc, uniformBlockCount);
CHECK_OFFSET(GX2VertexShader, 0xE0, uniformBlocks);
CHECK_OFFSET(GX2VertexShader, 0xE4, uniformVarCount);
CHECK_OFFSET(GX2VertexShader, 0xE8, uniformVars);
CHECK_OFFSET(GX2VertexShader, 0xEc, initialValueCount);
CHECK_OFFSET(GX2VertexShader, 0xF0, initialValues);
CHECK_OFFSET(GX2VertexShader, 0xF4, loopVarCount);
CHECK_OFFSET(GX2VertexShader, 0xF8, loopVars);
CHECK_OFFSET(GX2VertexShader, 0xFc, samplerVarCount);
CHECK_OFFSET(GX2VertexShader, 0x100, samplerVars);
CHECK_OFFSET(GX2VertexShader, 0x104, attribVarCount);
CHECK_OFFSET(GX2VertexShader, 0x108, attribVars);
CHECK_OFFSET(GX2VertexShader, 0x10c, ringItemsize);
CHECK_OFFSET(GX2VertexShader, 0x110, hasStreamOut);
CHECK_OFFSET(GX2VertexShader, 0x114, streamOutStride);
CHECK_OFFSET(GX2VertexShader, 0x124, gx2rBuffer);
CHECK_SIZE(GX2VertexShader, 0x134);

struct GX2PixelShader
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
};
CHECK_OFFSET(GX2PixelShader, 0x00, regs.sq_pgm_resources_ps);
CHECK_OFFSET(GX2PixelShader, 0x04, regs.sq_pgm_exports_ps);
CHECK_OFFSET(GX2PixelShader, 0x08, regs.spi_ps_in_control_0);
CHECK_OFFSET(GX2PixelShader, 0x0C, regs.spi_ps_in_control_1);
CHECK_OFFSET(GX2PixelShader, 0x10, regs.num_spi_ps_input_cntl);
CHECK_OFFSET(GX2PixelShader, 0x14, regs.spi_ps_input_cntls);
CHECK_OFFSET(GX2PixelShader, 0x94, regs.cb_shader_mask);
CHECK_OFFSET(GX2PixelShader, 0x98, regs.cb_shader_control);
CHECK_OFFSET(GX2PixelShader, 0x9C, regs.db_shader_control);
CHECK_OFFSET(GX2PixelShader, 0xA0, regs.spi_input_z);
CHECK_OFFSET(GX2PixelShader, 0xA4, size);
CHECK_OFFSET(GX2PixelShader, 0xA8, program);
CHECK_OFFSET(GX2PixelShader, 0xAC, mode);
CHECK_OFFSET(GX2PixelShader, 0xB0, uniformBlockCount);
CHECK_OFFSET(GX2PixelShader, 0xB4, uniformBlocks);
CHECK_OFFSET(GX2PixelShader, 0xB8, uniformVarCount);
CHECK_OFFSET(GX2PixelShader, 0xBC, uniformVars);
CHECK_OFFSET(GX2PixelShader, 0xC0, initialValueCount);
CHECK_OFFSET(GX2PixelShader, 0xC4, initialValues);
CHECK_OFFSET(GX2PixelShader, 0xC8, loopVarCount);
CHECK_OFFSET(GX2PixelShader, 0xCC, loopVars);
CHECK_OFFSET(GX2PixelShader, 0xD0, samplerVarCount);
CHECK_OFFSET(GX2PixelShader, 0xD4, samplerVars);
CHECK_OFFSET(GX2PixelShader, 0xD8, gx2rBuffer);
CHECK_SIZE(GX2PixelShader, 0xE8);

struct GX2GeometryShader
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
};
CHECK_OFFSET(GX2GeometryShader, 0x00, regs.sq_pgm_resources_gs);
CHECK_OFFSET(GX2GeometryShader, 0x04, regs.vgt_gs_out_prim_type);
CHECK_OFFSET(GX2GeometryShader, 0x08, regs.vgt_gs_mode);
CHECK_OFFSET(GX2GeometryShader, 0x0C, regs.pa_cl_vs_out_cntl);
CHECK_OFFSET(GX2GeometryShader, 0x10, regs.sq_pgm_resources_vs);
CHECK_OFFSET(GX2GeometryShader, 0x14, regs.sq_gs_vert_itemsize);
CHECK_OFFSET(GX2GeometryShader, 0x18, regs.spi_vs_out_config);
CHECK_OFFSET(GX2GeometryShader, 0x1C, regs.num_spi_vs_out_id);
CHECK_OFFSET(GX2GeometryShader, 0x20, regs.spi_vs_out_id);
CHECK_OFFSET(GX2GeometryShader, 0x48, regs.vgt_strmout_buffer_en);
CHECK_OFFSET(GX2GeometryShader, 0x4C, size);
CHECK_OFFSET(GX2GeometryShader, 0x50, program);
CHECK_OFFSET(GX2GeometryShader, 0x54, vertexProgramSize);
CHECK_OFFSET(GX2GeometryShader, 0x58, vertexProgram);
CHECK_OFFSET(GX2GeometryShader, 0x5C, mode);
CHECK_OFFSET(GX2GeometryShader, 0x60, uniformBlockCount);
CHECK_OFFSET(GX2GeometryShader, 0x64, uniformBlocks);
CHECK_OFFSET(GX2GeometryShader, 0x68, uniformVarCount);
CHECK_OFFSET(GX2GeometryShader, 0x6C, uniformVars);
CHECK_OFFSET(GX2GeometryShader, 0x70, initialValueCount);
CHECK_OFFSET(GX2GeometryShader, 0x74, initialValues);
CHECK_OFFSET(GX2GeometryShader, 0x78, loopVarCount);
CHECK_OFFSET(GX2GeometryShader, 0x7C, loopVars);
CHECK_OFFSET(GX2GeometryShader, 0x80, samplerVarCount);
CHECK_OFFSET(GX2GeometryShader, 0x84, samplerVars);
CHECK_OFFSET(GX2GeometryShader, 0x88, ringItemSize);
CHECK_OFFSET(GX2GeometryShader, 0x8C, hasStreamOut);
CHECK_OFFSET(GX2GeometryShader, 0x90, streamOutStride);
CHECK_OFFSET(GX2GeometryShader, 0xA0, gx2rBuffer);
CHECK_SIZE(GX2GeometryShader, 0xB0);

struct GX2AttribStream
{
   uint32_t location;
   uint32_t buffer;
   uint32_t offset;
   GX2AttribFormat format;
   GX2AttribIndexType type;
   uint32_t aluDivisor;
   uint32_t mask;
   GX2EndianSwapMode endianSwap;
};
CHECK_OFFSET(GX2AttribStream, 0x0, location);
CHECK_OFFSET(GX2AttribStream, 0x4, buffer);
CHECK_OFFSET(GX2AttribStream, 0x8, offset);
CHECK_OFFSET(GX2AttribStream, 0xc, format);
CHECK_OFFSET(GX2AttribStream, 0x10, type);
CHECK_OFFSET(GX2AttribStream, 0x14, aluDivisor);
CHECK_OFFSET(GX2AttribStream, 0x18, mask);
CHECK_OFFSET(GX2AttribStream, 0x1c, endianSwap);
CHECK_SIZE(GX2AttribStream, 0x20);

uint32_t
GX2CalcGeometryShaderInputRingBufferSize(uint32_t ringItemSize);

uint32_t
GX2CalcGeometryShaderOutputRingBufferSize(uint32_t ringItemSize);

uint32_t
GX2CalcFetchShaderSizeEx(uint32_t attribs,
                         GX2FetchShaderType fetchShaderType,
                         GX2TessellationMode tesellationMode);

void
GX2InitFetchShaderEx(GX2FetchShader *fetchShader,
                     uint8_t *buffer,
                     uint32_t attribCount,
                     GX2AttribStream *attribs,
                     GX2FetchShaderType type,
                     GX2TessellationMode tessMode);

void
GX2SetFetchShader(GX2FetchShader *shader);

void
GX2SetVertexShader(GX2VertexShader *shader);

void
GX2SetPixelShader(GX2PixelShader *shader);

void
GX2SetGeometryShader(GX2GeometryShader *shader);

void
GX2SetVertexSampler(GX2Sampler *sampler,
                    uint32_t id);

void
GX2SetPixelSampler(GX2Sampler *sampler,
                   uint32_t id);

void
GX2SetGeometrySampler(GX2Sampler *sampler,
                      uint32_t id);

void
GX2SetVertexUniformReg(uint32_t offset,
                       uint32_t count,
                       uint32_t *data);

void
GX2SetPixelUniformReg(uint32_t offset,
                      uint32_t count,
                      uint32_t *data);

void
GX2SetVertexUniformBlock(uint32_t location,
                         uint32_t size,
                         const void *data);

void
GX2SetPixelUniformBlock(uint32_t location,
                        uint32_t size,
                        const void *data);

void
GX2SetGeometryUniformBlock(uint32_t location,
                           uint32_t size,
                           const void *data);

void
GX2SetShaderModeEx(GX2ShaderMode mode,
                   uint32_t numVsGpr, uint32_t numVsStackEntries,
                   uint32_t numGsGpr, uint32_t numGsStackEntries,
                   uint32_t numPsGpr, uint32_t numPsStackEntries);

void
GX2SetStreamOutEnable(BOOL enable);

void
GX2SetGeometryShaderInputRingBuffer(void *buffer,
                                    uint32_t size);

void
GX2SetGeometryShaderOutputRingBuffer(void *buffer,
                                     uint32_t size);

uint32_t
GX2GetPixelShaderGPRs(GX2PixelShader *shader);

uint32_t
GX2GetPixelShaderStackEntries(GX2PixelShader *shader);

uint32_t
GX2GetVertexShaderGPRs(GX2VertexShader *shader);

uint32_t
GX2GetVertexShaderStackEntries(GX2VertexShader *shader);

uint32_t
GX2GetGeometryShaderGPRs(GX2GeometryShader *shader);

uint32_t
GX2GetGeometryShaderStackEntries(GX2GeometryShader *shader);

#ifdef __cplusplus
}
#endif

/** @} */
