/* originally from from https://github.com/smealum/ctrulib */

/**
 * @file gpu-old.h
 * @brief Deprecated GPU functions which should not be used in new code.
 * @description These functions have been superseeded by direct GPU register writes, or external GPU libraries.
 * @deprecated
 */
#pragma once

#include <3ds/gpu/gpu.h>

/**
 * @brief Initializes the GPU.
 * @param gsphandle GSP handle to use.
 * @deprecated
 */
void GPU_Init(Handle *gsphandle) DEPRECATED;

/**
 * @brief Resets the GPU.
 * @param gxbuf GX command buffer to use.
 * @param gpuBuf GPU command buffer to use.
 * @param gpuBufSize GPU command buffer size.
 * @deprecated
 */
void GPU_Reset(u32* gxbuf, u32* gpuBuf, u32 gpuBufSize) DEPRECATED;

/**
 * @brief Sets a shader float uniform.
 * @param type Type of shader to set the uniform of.
 * @param startreg Start of the uniform register to set.
 * @param data Data to set.
 * @param numreg Number of registers to set.
 * @deprecated
 */
void GPU_SetFloatUniform(GPU_SHADER_TYPE type, u32 startreg, u32* data, u32 numreg) DEPRECATED;

/**
 * @brief Sets the viewport.
 * @param depthBuffer Buffer to output depth data to.
 * @param colorBuffer Buffer to output color data to.
 * @param x X of the viewport.
 * @param y Y of the viewport.
 * @param w Width of the viewport.
 * @param h Height of the viewport.
 * @deprecated
 */
void GPU_SetViewport(u32* depthBuffer, u32* colorBuffer, u32 x, u32 y, u32 w, u32 h) DEPRECATED;

/**
 * @brief Sets the current scissor test mode.
 * @param mode Scissor test mode to use.
 * @param x X of the scissor region.
 * @param y Y of the scissor region.
 * @param w Width of the scissor region.
 * @param h Height of the scissor region.
 * @deprecated
 */
void GPU_SetScissorTest(GPU_SCISSORMODE mode, u32 left, u32 bottom, u32 right, u32 top) DEPRECATED;

/**
 * @brief Sets the depth map.
 * @param zScale Z scale to use.
 * @param zOffset Z offset to use.
 * @deprecated
 */
void GPU_DepthMap(float zScale, float zOffset) DEPRECATED;

/**
 * @brief Sets the alpha test parameters.
 * @param enable Whether to enable alpha testing.
 * @param function Test function to use.
 * @param ref Reference value to use.
 * @deprecated
 */
void GPU_SetAlphaTest(bool enable, GPU_TESTFUNC function, u8 ref) DEPRECATED;

/**
 * @brief Sets the depth test parameters and pixel write mask.
 * @note GPU_WRITEMASK values can be ORed together.
 * @param enable Whether to enable depth testing.
 * @param function Test function to use.
 * @param writemask Pixel write mask to use.
 * @deprecated
 */
void GPU_SetDepthTestAndWriteMask(bool enable, GPU_TESTFUNC function, GPU_WRITEMASK writemask) DEPRECATED;

/**
 * @brief Sets the stencil test parameters.
 * @param enable Whether to enable stencil testing.
 * @param function Test function to use.
 * @param ref Reference value to use.
 * @param input_mask Input mask to use.
 * @param write_mask Write mask to use.
 * @deprecated
 */
void GPU_SetStencilTest(bool enable, GPU_TESTFUNC function, u8 ref, u8 input_mask, u8 write_mask) DEPRECATED;

/**
 * @brief Sets the stencil test operators.
 * @param sfail Operator to use on source test failure.
 * @param dfail Operator to use on destination test failure.
 * @param pass Operator to use on test passing.
 * @deprecated
 */
void GPU_SetStencilOp(GPU_STENCILOP sfail, GPU_STENCILOP dfail, GPU_STENCILOP pass) DEPRECATED;

/**
 * @brief Sets the face culling mode.
 * @param mode Face culling mode to use.
 * @deprecated
 */
void GPU_SetFaceCulling(GPU_CULLMODE mode) DEPRECATED;

/**
 * @brief Sets the combiner buffer write parameters.
 * @note Use GPU_TEV_BUFFER_WRITE_CONFIG to build the parameters.
 * @note Only the first four TEV stages can write to the combiner buffer.
 * @param rgb_config RGB configuration to use.
 * @param alpha_config Alpha configuration to use.
 * @deprecated
 */
void GPU_SetCombinerBufferWrite(u8 rgb_config, u8 alpha_config) DEPRECATED;

/**
 * @brief Sets the alpha blending parameters.
 * @note Cannot be used with GPU_SetColorLogicOp.
 * @param colorEquation Blend equation to use for color components.
 * @param alphaEquation Blend equation to use for the alpha component.
 * @param colorSrc Source factor of color components.
 * @param colorDst Destination factor of color components.
 * @param alphaSrc Source factor of the alpha component.
 * @param alphaDst Destination factor of the alpha component.
 * @deprecated
 */
void GPU_SetAlphaBlending(GPU_BLENDEQUATION colorEquation, GPU_BLENDEQUATION alphaEquation,
	GPU_BLENDFACTOR colorSrc, GPU_BLENDFACTOR colorDst,
	GPU_BLENDFACTOR alphaSrc, GPU_BLENDFACTOR alphaDst) DEPRECATED;

/**
 * @brief Sets the color logic operator.
 * @note Cannot be used with GPU_SetAlphaBlending.
 * @param op Operator to set.
 * @deprecated
 */
void GPU_SetColorLogicOp(GPU_LOGICOP op) DEPRECATED;

/**
 * @brief Sets the blending color.
 * @param r Red component.
 * @param g Green component.
 * @param b Blue component.
 * @param a Alpha component.
 * @deprecated
 */
void GPU_SetBlendingColor(u8 r, u8 g, u8 b, u8 a) DEPRECATED;

/**
 * @brief Sets the VBO attribute buffers.
 * @param totalAttributes Total number of attributes.
 * @param baseAddress Base address of the VBO.
 * @param attributeFormats Attribute format data.
 * @param attributeMask Attribute mask.
 * @param attributePermutation Attribute permutations.
 * @param numBuffers Number of buffers.
 * @param bufferOffsets Offsets of the buffers.
 * @param bufferPermutations Buffer permutations.
 * @param bufferNumAttributes Numbers of attributes of the buffers.
 * @deprecated
 */
void GPU_SetAttributeBuffers(u8 totalAttributes, u32* baseAddress, u64 attributeFormats, u16 attributeMask, u64 attributePermutation, u8 numBuffers, u32 bufferOffsets[], u64 bufferPermutations[], u8 bufferNumAttributes[]) DEPRECATED;

/**
 * @brief Sets the enabled texture units.
 * @param units Units to enable. OR texture unit values together to create this value.
 * @deprecated
 */
void GPU_SetTextureEnable(GPU_TEXUNIT units) DEPRECATED;

/**
 * @brief Sets the texture data of a texture unit.
 * @param unit Texture unit to use.
 * @param data Data to load. Must be in linear memory or VRAM.
 * @param width Width of the texture.
 * @param height Height of the texture.
 * @param Parameters of the texture, such as filters and wrap modes.
 * @param colorType Color type of the texture.
 * @deprecated
 */
void GPU_SetTexture(GPU_TEXUNIT unit, u32* data, u16 width, u16 height, u32 param, GPU_TEXCOLOR colorType) DEPRECATED;

/**
 * @brief Sets the border color of a texture unit.
 * @param unit Texture unit to use.
 * @param borderColor The color used for the border when using the @ref GPU_CLAMP_TO_BORDER wrap mode.
 * @deprecated
 */
void GPU_SetTextureBorderColor(GPU_TEXUNIT unit,u32 borderColor) DEPRECATED;

/**
 * @brief Sets the parameters of a texture combiner.
 * @param id ID of the combiner.
 * @param rgbSources RGB source configuration.
 * @param alphaSources Alpha source configuration.
 * @param rgbOperands RGB operand configuration.
 * @param alphaOperands Alpha operand configuration.
 * @param rgbCombine RGB combiner function.
 * @param alphaCombine Alpha combiner function.
 * @param constantColor Constant color to provide.
 * @deprecated
 */
void GPU_SetTexEnv(u8 id, u16 rgbSources, u16 alphaSources, u16 rgbOperands, u16 alphaOperands, GPU_COMBINEFUNC rgbCombine, GPU_COMBINEFUNC alphaCombine, u32 constantColor) DEPRECATED;

/**
 * @brief Draws an array of vertex data.
 * @param primitive Primitive to draw.
 * @param first First vertex to draw.
 * @param count Number of vertices to draw.
 * @deprecated
 */
void GPU_DrawArray(GPU_Primitive_t primitive, u32 first, u32 count) DEPRECATED;

/**
 * @brief Draws vertex elements.
 * @param primitive Primitive to draw.
 * @param indexArray Array of vertex indices to use.
 * @param n Number of vertices to draw.
 * @deprecated
 */
void GPU_DrawElements(GPU_Primitive_t primitive, u32* indexArray, u32 n) DEPRECATED;

/**
 * @brief Finishes drawing.
 * @deprecated
 */
void GPU_FinishDrawing() DEPRECATED;

void GPU_Finalize(void) DEPRECATED;
