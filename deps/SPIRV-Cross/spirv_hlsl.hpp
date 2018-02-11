/*
 * Copyright 2016-2018 Robert Konrad
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SPIRV_HLSL_HPP
#define SPIRV_HLSL_HPP

#include "spirv_glsl.hpp"
#include <utility>
#include <vector>

namespace spirv_cross
{
// Interface which remaps vertex inputs to a fixed semantic name to make linking easier.
struct HLSLVertexAttributeRemap
{
	uint32_t location;
	std::string semantic;
};

class CompilerHLSL : public CompilerGLSL
{
public:
	struct Options
	{
		uint32_t shader_model = 30; // TODO: map ps_4_0_level_9_0,... somehow

		// Allows the PointSize builtin, and ignores it, as PointSize is not supported in HLSL.
		bool point_size_compat = false;
	};

	CompilerHLSL(std::vector<uint32_t> spirv_)
	    : CompilerGLSL(move(spirv_))
	{
	}

	CompilerHLSL(const uint32_t *ir, size_t size)
	    : CompilerGLSL(ir, size)
	{
	}

	const Options &get_options() const
	{
		return options;
	}

	void set_options(Options &opts)
	{
		options = opts;
	}

	// Compiles and remaps vertex attributes at specific locations to a fixed semantic.
	// The default is TEXCOORD# where # denotes location.
	// Matrices are unrolled to vectors with notation ${SEMANTIC}_#, where # denotes row.
	// $SEMANTIC is either TEXCOORD# or a semantic name specified here.
	std::string compile(std::vector<HLSLVertexAttributeRemap> vertex_attributes);
	std::string compile() override;

private:
	std::string type_to_glsl(const SPIRType &type, uint32_t id = 0) override;
	std::string image_type_hlsl(const SPIRType &type);
	std::string image_type_hlsl_modern(const SPIRType &type);
	std::string image_type_hlsl_legacy(const SPIRType &type);
	void emit_function_prototype(SPIRFunction &func, uint64_t return_flags) override;
	void emit_hlsl_entry_point();
	void emit_header() override;
	void emit_resources();
	void emit_interface_block_globally(const SPIRVariable &type);
	void emit_interface_block_in_struct(const SPIRVariable &type, std::unordered_set<uint32_t> &active_locations);
	void emit_builtin_inputs_in_struct();
	void emit_builtin_outputs_in_struct();
	void emit_texture_op(const Instruction &i) override;
	void emit_instruction(const Instruction &instruction) override;
	void emit_glsl_op(uint32_t result_type, uint32_t result_id, uint32_t op, const uint32_t *args,
	                  uint32_t count) override;
	void emit_buffer_block(const SPIRVariable &type) override;
	void emit_push_constant_block(const SPIRVariable &var) override;
	void emit_uniform(const SPIRVariable &var) override;
	void emit_modern_uniform(const SPIRVariable &var);
	void emit_legacy_uniform(const SPIRVariable &var);
	void emit_specialization_constants();
	void emit_composite_constants();
	void emit_fixup() override;
	std::string builtin_to_glsl(spv::BuiltIn builtin, spv::StorageClass storage) override;
	std::string layout_for_member(const SPIRType &type, uint32_t index) override;
	std::string to_interpolation_qualifiers(uint64_t flags) override;
	std::string bitcast_glsl_op(const SPIRType &result_type, const SPIRType &argument_type) override;
	std::string to_func_call_arg(uint32_t id) override;
	std::string to_sampler_expression(uint32_t id);
	std::string to_resource_binding(const SPIRVariable &var);
	std::string to_resource_binding_sampler(const SPIRVariable &var);
	void emit_sampled_image_op(uint32_t result_type, uint32_t result_id, uint32_t image_id, uint32_t samp_id) override;
	void emit_access_chain(const Instruction &instruction);
	void emit_load(const Instruction &instruction);
	std::string read_access_chain(const SPIRAccessChain &chain);
	void write_access_chain(const SPIRAccessChain &chain, uint32_t value);
	void emit_store(const Instruction &instruction);
	void emit_atomic(const uint32_t *ops, uint32_t length, spv::Op op);

	void emit_struct_member(const SPIRType &type, uint32_t member_type_id, uint32_t index,
	                        const std::string &qualifier) override;

	const char *to_storage_qualifiers_glsl(const SPIRVariable &var) override;

	Options options;
	bool requires_op_fmod = false;
	bool requires_textureProj = false;
	bool requires_fp16_packing = false;
	bool requires_unorm8_packing = false;
	bool requires_snorm8_packing = false;
	bool requires_unorm16_packing = false;
	bool requires_snorm16_packing = false;
	bool requires_bitfield_insert = false;
	bool requires_bitfield_extract = false;
	uint64_t required_textureSizeVariants = 0;
	void require_texture_query_variant(const SPIRType &type);

	enum TextureQueryVariantDim
	{
		Query1D = 0,
		Query1DArray,
		Query2D,
		Query2DArray,
		Query3D,
		QueryBuffer,
		QueryCube,
		QueryCubeArray,
		Query2DMS,
		Query2DMSArray,
		QueryDimCount
	};

	enum TextureQueryVariantType
	{
		QueryTypeFloat = 0,
		QueryTypeInt = 16,
		QueryTypeUInt = 32,
		QueryTypeCount = 3
	};

	void emit_builtin_variables();
	bool require_output = false;
	bool require_input = false;
	std::vector<HLSLVertexAttributeRemap> remap_vertex_attributes;

	uint32_t type_to_consumed_locations(const SPIRType &type) const;

	void emit_io_block(const SPIRVariable &var);
	std::string to_semantic(uint32_t vertex_location);
};
}

#endif
