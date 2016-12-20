/*
 * Copyright 2015-2016 ARM Limited
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

#include "spirv_glsl.hpp"
#include "GLSL.std.450.h"
#include <algorithm>
#include <assert.h>

using namespace spv;
using namespace spirv_cross;
using namespace std;

// Returns true if an arithmetic operation does not change behavior depending on signedness.
static bool opcode_is_sign_invariant(Op opcode)
{
	switch (opcode)
	{
	case OpIEqual:
	case OpINotEqual:
	case OpISub:
	case OpIAdd:
	case OpIMul:
	case OpShiftLeftLogical:
	case OpBitwiseOr:
	case OpBitwiseXor:
	case OpBitwiseAnd:
		return true;

	default:
		return false;
	}
}

static const char *to_pls_layout(PlsFormat format)
{
	switch (format)
	{
	case PlsR11FG11FB10F:
		return "layout(r11f_g11f_b10f) ";
	case PlsR32F:
		return "layout(r32f) ";
	case PlsRG16F:
		return "layout(rg16f) ";
	case PlsRGB10A2:
		return "layout(rgb10_a2) ";
	case PlsRGBA8:
		return "layout(rgba8) ";
	case PlsRG16:
		return "layout(rg16) ";
	case PlsRGBA8I:
		return "layout(rgba8i)";
	case PlsRG16I:
		return "layout(rg16i) ";
	case PlsRGB10A2UI:
		return "layout(rgb10_a2ui) ";
	case PlsRGBA8UI:
		return "layout(rgba8ui) ";
	case PlsRG16UI:
		return "layout(rg16ui) ";
	case PlsR32UI:
		return "layout(r32ui) ";
	default:
		return "";
	}
}

static SPIRType::BaseType pls_format_to_basetype(PlsFormat format)
{
	switch (format)
	{
	default:
	case PlsR11FG11FB10F:
	case PlsR32F:
	case PlsRG16F:
	case PlsRGB10A2:
	case PlsRGBA8:
	case PlsRG16:
		return SPIRType::Float;

	case PlsRGBA8I:
	case PlsRG16I:
		return SPIRType::Int;

	case PlsRGB10A2UI:
	case PlsRGBA8UI:
	case PlsRG16UI:
	case PlsR32UI:
		return SPIRType::UInt;
	}
}

static uint32_t pls_format_to_components(PlsFormat format)
{
	switch (format)
	{
	default:
	case PlsR32F:
	case PlsR32UI:
		return 1;

	case PlsRG16F:
	case PlsRG16:
	case PlsRG16UI:
	case PlsRG16I:
		return 2;

	case PlsR11FG11FB10F:
		return 3;

	case PlsRGB10A2:
	case PlsRGBA8:
	case PlsRGBA8I:
	case PlsRGB10A2UI:
	case PlsRGBA8UI:
		return 4;
	}
}

void CompilerGLSL::reset()
{
	// We do some speculative optimizations which should pretty much always work out,
	// but just in case the SPIR-V is rather weird, recompile until it's happy.
	// This typically only means one extra pass.
	force_recompile = false;

	// Clear invalid expression tracking.
	invalid_expressions.clear();
	current_function = nullptr;

	// Clear temporary usage tracking.
	expression_usage_counts.clear();
	forwarded_temporaries.clear();

	resource_names.clear();

	for (auto &id : ids)
	{
		if (id.get_type() == TypeVariable)
		{
			// Clear unflushed dependees.
			id.get<SPIRVariable>().dependees.clear();
		}
		else if (id.get_type() == TypeExpression)
		{
			// And remove all expressions.
			id.reset();
		}
		else if (id.get_type() == TypeFunction)
		{
			// Reset active state for all functions.
			id.get<SPIRFunction>().active = false;
			id.get<SPIRFunction>().flush_undeclared = true;
		}
	}

	statement_count = 0;
	indent = 0;
}

void CompilerGLSL::remap_pls_variables()
{
	for (auto &input : pls_inputs)
	{
		auto &var = get<SPIRVariable>(input.id);

		bool input_is_target = false;
		if (var.storage == StorageClassUniformConstant)
		{
			auto &type = get<SPIRType>(var.basetype);
			input_is_target = type.image.dim == DimSubpassData;
		}

		if (var.storage != StorageClassInput && !input_is_target)
			SPIRV_CROSS_THROW("Can only use in and target variables for PLS inputs.");
		var.remapped_variable = true;
	}

	for (auto &output : pls_outputs)
	{
		auto &var = get<SPIRVariable>(output.id);
		if (var.storage != StorageClassOutput)
			SPIRV_CROSS_THROW("Can only use out variables for PLS outputs.");
		var.remapped_variable = true;
	}
}

void CompilerGLSL::find_static_extensions()
{
	for (auto &id : ids)
	{
		if (id.get_type() == TypeType)
		{
			auto &type = id.get<SPIRType>();
			if (type.basetype == SPIRType::Double)
			{
				if (options.es)
					SPIRV_CROSS_THROW("FP64 not supported in ES profile.");
				if (!options.es && options.version < 400)
					require_extension("GL_ARB_gpu_shader_fp64");
			}

			if (type.basetype == SPIRType::Int64 || type.basetype == SPIRType::UInt64)
			{
				if (options.es)
					SPIRV_CROSS_THROW("64-bit integers not supported in ES profile.");
				if (!options.es)
					require_extension("GL_ARB_gpu_shader_int64");
			}
		}
	}

	auto &execution = get_entry_point();
	switch (execution.model)
	{
	case ExecutionModelGLCompute:
		if (!options.es && options.version < 430)
			require_extension("GL_ARB_compute_shader");
		if (options.es && options.version < 310)
			SPIRV_CROSS_THROW("At least ESSL 3.10 required for compute shaders.");
		break;

	case ExecutionModelGeometry:
		if (options.es && options.version < 320)
			require_extension("GL_EXT_geometry_shader");
		if (!options.es && options.version < 320)
			require_extension("GL_ARB_geometry_shader4");

		if ((execution.flags & (1ull << ExecutionModeInvocations)) && execution.invocations != 1)
		{
			// Instanced GS is part of 400 core or this extension.
			if (!options.es && options.version < 400)
				require_extension("GL_ARB_gpu_shader5");
		}
		break;

	case ExecutionModelTessellationEvaluation:
	case ExecutionModelTessellationControl:
		if (options.es && options.version < 320)
			require_extension("GL_EXT_tessellation_shader");
		if (!options.es && options.version < 400)
			require_extension("GL_ARB_tessellation_shader");
		break;

	default:
		break;
	}

	if (!pls_inputs.empty() || !pls_outputs.empty())
		require_extension("GL_EXT_shader_pixel_local_storage");
}

string CompilerGLSL::compile()
{
	// Scan the SPIR-V to find trivial uses of extensions.
	find_static_extensions();

	uint32_t pass_count = 0;
	do
	{
		if (pass_count >= 3)
			SPIRV_CROSS_THROW("Over 3 compilation loops detected. Must be a bug!");

		reset();

		// Move constructor for this type is broken on GCC 4.9 ...
		buffer = unique_ptr<ostringstream>(new ostringstream());

		emit_header();
		emit_resources();

		emit_function(get<SPIRFunction>(entry_point), 0);

		pass_count++;
	} while (force_recompile);

	return buffer->str();
}

std::string CompilerGLSL::get_partial_source()
{
	return buffer->str();
}

void CompilerGLSL::emit_header()
{
	auto &execution = get_entry_point();
	statement("#version ", options.version, options.es && options.version > 100 ? " es" : "");

	// Needed for binding = # on UBOs, etc.
	if (!options.es && options.version < 420)
	{
		statement("#ifdef GL_ARB_shading_language_420pack");
		statement("#extension GL_ARB_shading_language_420pack : require");
		statement("#endif");
	}

	for (auto &ext : forced_extensions)
		statement("#extension ", ext, " : require");

	for (auto &header : header_lines)
		statement(header);

	vector<string> inputs;
	vector<string> outputs;

	switch (execution.model)
	{
	case ExecutionModelGeometry:
		outputs.push_back(join("max_vertices = ", execution.output_vertices));
		if ((execution.flags & (1ull << ExecutionModeInvocations)) && execution.invocations != 1)
			inputs.push_back(join("invocations = ", execution.invocations));
		if (execution.flags & (1ull << ExecutionModeInputPoints))
			inputs.push_back("points");
		if (execution.flags & (1ull << ExecutionModeInputLines))
			inputs.push_back("lines");
		if (execution.flags & (1ull << ExecutionModeInputLinesAdjacency))
			inputs.push_back("lines_adjacency");
		if (execution.flags & (1ull << ExecutionModeTriangles))
			inputs.push_back("triangles");
		if (execution.flags & (1ull << ExecutionModeInputTrianglesAdjacency))
			inputs.push_back("triangles_adjacency");
		if (execution.flags & (1ull << ExecutionModeOutputTriangleStrip))
			outputs.push_back("triangle_strip");
		if (execution.flags & (1ull << ExecutionModeOutputPoints))
			outputs.push_back("points");
		if (execution.flags & (1ull << ExecutionModeOutputLineStrip))
			outputs.push_back("line_strip");
		break;

	case ExecutionModelTessellationControl:
		if (execution.flags & (1ull << ExecutionModeOutputVertices))
			outputs.push_back(join("vertices = ", execution.output_vertices));
		break;

	case ExecutionModelTessellationEvaluation:
		if (execution.flags & (1ull << ExecutionModeQuads))
			inputs.push_back("quads");
		if (execution.flags & (1ull << ExecutionModeTriangles))
			inputs.push_back("triangles");
		if (execution.flags & (1ull << ExecutionModeIsolines))
			inputs.push_back("isolines");
		if (execution.flags & (1ull << ExecutionModePointMode))
			inputs.push_back("point_mode");

		if ((execution.flags & (1ull << ExecutionModeIsolines)) == 0)
		{
			if (execution.flags & (1ull << ExecutionModeVertexOrderCw))
				inputs.push_back("cw");
			if (execution.flags & (1ull << ExecutionModeVertexOrderCcw))
				inputs.push_back("ccw");
		}

		if (execution.flags & (1ull << ExecutionModeSpacingFractionalEven))
			inputs.push_back("fractional_even_spacing");
		if (execution.flags & (1ull << ExecutionModeSpacingFractionalOdd))
			inputs.push_back("fractional_odd_spacing");
		if (execution.flags & (1ull << ExecutionModeSpacingEqual))
			inputs.push_back("equal_spacing");
		break;

	case ExecutionModelGLCompute:
		inputs.push_back(join("local_size_x = ", execution.workgroup_size.x));
		inputs.push_back(join("local_size_y = ", execution.workgroup_size.y));
		inputs.push_back(join("local_size_z = ", execution.workgroup_size.z));
		break;

	case ExecutionModelFragment:
		if (options.es)
		{
			switch (options.fragment.default_float_precision)
			{
			case Options::Lowp:
				statement("precision lowp float;");
				break;

			case Options::Mediump:
				statement("precision mediump float;");
				break;

			case Options::Highp:
				statement("precision highp float;");
				break;

			default:
				break;
			}

			switch (options.fragment.default_int_precision)
			{
			case Options::Lowp:
				statement("precision lowp int;");
				break;

			case Options::Mediump:
				statement("precision mediump int;");
				break;

			case Options::Highp:
				statement("precision highp int;");
				break;

			default:
				break;
			}
		}

		if (execution.flags & (1ull << ExecutionModeEarlyFragmentTests))
			inputs.push_back("early_fragment_tests");
		if (execution.flags & (1ull << ExecutionModeDepthGreater))
			inputs.push_back("depth_greater");
		if (execution.flags & (1ull << ExecutionModeDepthLess))
			inputs.push_back("depth_less");

		break;

	default:
		break;
	}

	if (!inputs.empty())
		statement("layout(", merge(inputs), ") in;");
	if (!outputs.empty())
		statement("layout(", merge(outputs), ") out;");

	statement("");
}

void CompilerGLSL::emit_struct(SPIRType &type)
{
	// Struct types can be stamped out multiple times
	// with just different offsets, matrix layouts, etc ...
	// Type-punning with these types is legal, which complicates things
	// when we are storing struct and array types in an SSBO for example.
	if (type.type_alias != 0)
		return;

	add_resource_name(type.self);
	auto name = type_to_glsl(type);

	statement(!backend.explicit_struct_type ? "struct " : "", name);
	begin_scope();

	type.member_name_cache.clear();

	uint32_t i = 0;
	bool emitted = false;
	for (auto &member : type.member_types)
	{
		add_member_name(type, i);

		auto &membertype = get<SPIRType>(member);
		statement(member_decl(type, membertype, i), ";");
		i++;
		emitted = true;
	}
	end_scope_decl();

	if (emitted)
		statement("");
}

uint64_t CompilerGLSL::combined_decoration_for_member(const SPIRType &type, uint32_t index)
{
	uint64_t flags = 0;
	auto &memb = meta[type.self].members;
	if (index >= memb.size())
		return 0;
	auto &dec = memb[index];

	// If our type is a struct, traverse all the members as well recursively.
	flags |= dec.decoration_flags;
	for (uint32_t i = 0; i < type.member_types.size(); i++)
		flags |= combined_decoration_for_member(get<SPIRType>(type.member_types[i]), i);

	return flags;
}

string CompilerGLSL::to_interpolation_qualifiers(uint64_t flags)
{
	string res;
	//if (flags & (1ull << DecorationSmooth))
	//    res += "smooth ";
	if (flags & (1ull << DecorationFlat))
		res += "flat ";
	if (flags & (1ull << DecorationNoPerspective))
		res += "noperspective ";
	if (flags & (1ull << DecorationCentroid))
		res += "centroid ";
	if (flags & (1ull << DecorationPatch))
		res += "patch ";
	if (flags & (1ull << DecorationSample))
		res += "sample ";
	if (flags & (1ull << DecorationInvariant))
		res += "invariant ";

	return res;
}

string CompilerGLSL::layout_for_member(const SPIRType &type, uint32_t index)
{
	bool is_block = (meta[type.self].decoration.decoration_flags &
	                 ((1ull << DecorationBlock) | (1ull << DecorationBufferBlock))) != 0;
	if (!is_block)
		return "";

	auto &memb = meta[type.self].members;
	if (index >= memb.size())
		return "";
	auto &dec = memb[index];

	vector<string> attr;

	// We can only apply layouts on members in block interfaces.
	// This is a bit problematic because in SPIR-V decorations are applied on the struct types directly.
	// This is not supported on GLSL, so we have to make the assumption that if a struct within our buffer block struct
	// has a decoration, it was originally caused by a top-level layout() qualifier in GLSL.
	//
	// We would like to go from (SPIR-V style):
	//
	// struct Foo { layout(row_major) mat4 matrix; };
	// buffer UBO { Foo foo; };
	//
	// to
	//
	// struct Foo { mat4 matrix; }; // GLSL doesn't support any layout shenanigans in raw struct declarations.
	// buffer UBO { layout(row_major) Foo foo; }; // Apply the layout on top-level.
	auto flags = combined_decoration_for_member(type, index);

	if (flags & (1ull << DecorationRowMajor))
		attr.push_back("row_major");
	// We don't emit any global layouts, so column_major is default.
	//if (flags & (1ull << DecorationColMajor))
	//    attr.push_back("column_major");

	if (dec.decoration_flags & (1ull << DecorationLocation))
		attr.push_back(join("location = ", dec.location));

	if (attr.empty())
		return "";

	string res = "layout(";
	res += merge(attr);
	res += ") ";
	return res;
}

const char *CompilerGLSL::format_to_glsl(spv::ImageFormat format)
{
	auto check_desktop = [this] {
		if (options.es)
			SPIRV_CROSS_THROW("Attempting to use image format not supported in ES profile.");
	};

	switch (format)
	{
	case ImageFormatRgba32f:
		return "rgba32f";
	case ImageFormatRgba16f:
		return "rgba16f";
	case ImageFormatR32f:
		return "r32f";
	case ImageFormatRgba8:
		return "rgba8";
	case ImageFormatRgba8Snorm:
		return "rgba8_snorm";
	case ImageFormatRg32f:
		return "rg32f";
	case ImageFormatRg16f:
		return "rg16f";

	case ImageFormatRgba32i:
		return "rgba32i";
	case ImageFormatRgba16i:
		return "rgba16i";
	case ImageFormatR32i:
		return "r32i";
	case ImageFormatRgba8i:
		return "rgba8i";
	case ImageFormatRg32i:
		return "rg32i";
	case ImageFormatRg16i:
		return "rg16i";

	case ImageFormatRgba32ui:
		return "rgba32ui";
	case ImageFormatRgba16ui:
		return "rgba16ui";
	case ImageFormatR32ui:
		return "r32ui";
	case ImageFormatRgba8ui:
		return "rgba8ui";
	case ImageFormatRg32ui:
		return "rg32ui";
	case ImageFormatRg16ui:
		return "rg16ui";

	// Desktop-only formats
	case ImageFormatR11fG11fB10f:
		check_desktop();
		return "r11f_g11f_b10f";
	case ImageFormatR16f:
		check_desktop();
		return "r16f";
	case ImageFormatRgb10A2:
		check_desktop();
		return "rgb10_a2";
	case ImageFormatR8:
		check_desktop();
		return "r8";
	case ImageFormatRg8:
		check_desktop();
		return "rg8";
	case ImageFormatR16:
		check_desktop();
		return "r16";
	case ImageFormatRg16:
		check_desktop();
		return "rg16";
	case ImageFormatRgba16:
		check_desktop();
		return "rgba16";
	case ImageFormatR16Snorm:
		check_desktop();
		return "r16_snorm";
	case ImageFormatRg16Snorm:
		check_desktop();
		return "rg16_snorm";
	case ImageFormatRgba16Snorm:
		check_desktop();
		return "rgba16_snorm";
	case ImageFormatR8Snorm:
		check_desktop();
		return "r8_snorm";
	case ImageFormatRg8Snorm:
		check_desktop();
		return "rg8_snorm";

	case ImageFormatR8ui:
		check_desktop();
		return "r8ui";
	case ImageFormatRg8ui:
		check_desktop();
		return "rg8ui";
	case ImageFormatR16ui:
		check_desktop();
		return "r16ui";
	case ImageFormatRgb10a2ui:
		check_desktop();
		return "rgb10_a2ui";

	case ImageFormatR8i:
		check_desktop();
		return "r8i";
	case ImageFormatRg8i:
		check_desktop();
		return "rg8i";
	case ImageFormatR16i:
		check_desktop();
		return "r16i";

	default:
	case ImageFormatUnknown:
		return nullptr;
	}
}

uint32_t CompilerGLSL::type_to_std430_base_size(const SPIRType &type)
{
	switch (type.basetype)
	{
	case SPIRType::Double:
	case SPIRType::Int64:
	case SPIRType::UInt64:
		return 8;
	default:
		return 4;
	}
}

uint32_t CompilerGLSL::type_to_std430_alignment(const SPIRType &type, uint64_t flags)
{
	const uint32_t base_alignment = type_to_std430_base_size(type);

	if (type.basetype == SPIRType::Struct)
	{
		// Rule 9. Structs alignments are maximum alignment of its members.
		uint32_t alignment = 0;
		for (uint32_t i = 0; i < type.member_types.size(); i++)
		{
			auto member_flags = meta[type.self].members.at(i).decoration_flags;
			alignment = max(alignment, type_to_std430_alignment(get<SPIRType>(type.member_types[i]), member_flags));
		}

		return alignment;
	}
	else
	{
		// From 7.6.2.2 in GL 4.5 core spec.
		// Rule 1
		if (type.vecsize == 1 && type.columns == 1)
			return base_alignment;

		// Rule 2
		if ((type.vecsize == 2 || type.vecsize == 4) && type.columns == 1)
			return type.vecsize * base_alignment;

		// Rule 3
		if (type.vecsize == 3 && type.columns == 1)
			return 4 * base_alignment;

		// Rule 4 implied. Alignment does not change in std430.

		// Rule 5. Column-major matrices are stored as arrays of
		// vectors.
		if ((flags & (1ull << DecorationColMajor)) && type.columns > 1)
		{
			if (type.vecsize == 3)
				return 4 * base_alignment;
			else
				return type.vecsize * base_alignment;
		}

		// Rule 6 implied.

		// Rule 7.
		if ((flags & (1ull << DecorationRowMajor)) && type.vecsize > 1)
		{
			if (type.columns == 3)
				return 4 * base_alignment;
			else
				return type.columns * base_alignment;
		}

		// Rule 8 implied.
	}

	SPIRV_CROSS_THROW("Did not find suitable std430 rule for type. Bogus decorations?");
}

uint32_t CompilerGLSL::type_to_std430_array_stride(const SPIRType &type, uint64_t flags)
{
	// Array stride is equal to aligned size of the underlying type.
	SPIRType tmp = type;
	tmp.array.pop_back();
	tmp.array_size_literal.pop_back();
	uint32_t size = type_to_std430_size(tmp, flags);
	uint32_t alignment = type_to_std430_alignment(tmp, flags);
	return (size + alignment - 1) & ~(alignment - 1);
}

uint32_t CompilerGLSL::type_to_std430_size(const SPIRType &type, uint64_t flags)
{
	if (!type.array.empty())
		return to_array_size_literal(type, uint32_t(type.array.size()) - 1) * type_to_std430_array_stride(type, flags);

	const uint32_t base_alignment = type_to_std430_base_size(type);
	uint32_t size = 0;

	if (type.basetype == SPIRType::Struct)
	{
		uint32_t pad_alignment = 1;

		for (uint32_t i = 0; i < type.member_types.size(); i++)
		{
			auto member_flags = meta[type.self].members.at(i).decoration_flags;
			auto &member_type = get<SPIRType>(type.member_types[i]);

			uint32_t std430_alignment = type_to_std430_alignment(member_type, member_flags);
			uint32_t alignment = max(std430_alignment, pad_alignment);

			// The next member following a struct member is aligned to the base alignment of the struct that came before.
			// GL 4.5 spec, 7.6.2.2.
			if (member_type.basetype == SPIRType::Struct)
				pad_alignment = std430_alignment;
			else
				pad_alignment = 1;

			size = (size + alignment - 1) & ~(alignment - 1);
			size += type_to_std430_size(member_type, member_flags);
		}
	}
	else
	{
		if (type.columns == 1)
			size = type.vecsize * base_alignment;

		if ((flags & (1ull << DecorationColMajor)) && type.columns > 1)
		{
			if (type.vecsize == 3)
				size = type.columns * 4 * base_alignment;
			else
				size = type.columns * type.vecsize * base_alignment;
		}

		if ((flags & (1ull << DecorationRowMajor)) && type.vecsize > 1)
		{
			if (type.columns == 3)
				size = type.vecsize * 4 * base_alignment;
			else
				size = type.vecsize * type.columns * base_alignment;
		}
	}

	return size;
}

bool CompilerGLSL::ssbo_is_std430_packing(const SPIRType &type)
{
	// This is very tricky and error prone, but try to be exhaustive and correct here.
	// SPIR-V doesn't directly say if we're using std430 or std140.
	// SPIR-V communicates this using Offset and ArrayStride decorations (which is what really matters),
	// so we have to try to infer whether or not the original GLSL source was std140 or std430 based on this information.
	// We do not have to consider shared or packed since these layouts are not allowed in Vulkan SPIR-V (they are useless anyways, and custom offsets would do the same thing).
	//
	// It is almost certain that we're using std430, but it gets tricky with arrays in particular.
	// We will assume std430, but infer std140 if we can prove the struct is not compliant with std430.
	//
	// The only two differences between std140 and std430 are related to padding alignment/array stride
	// in arrays and structs. In std140 they take minimum vec4 alignment.
	// std430 only removes the vec4 requirement.

	uint32_t offset = 0;
	uint32_t pad_alignment = 1;

	for (uint32_t i = 0; i < type.member_types.size(); i++)
	{
		auto &memb_type = get<SPIRType>(type.member_types[i]);
		auto member_flags = meta[type.self].members.at(i).decoration_flags;

		// Verify alignment rules.
		uint32_t std430_alignment = type_to_std430_alignment(memb_type, member_flags);
		uint32_t alignment = max(std430_alignment, pad_alignment);
		offset = (offset + alignment - 1) & ~(alignment - 1);

		// The next member following a struct member is aligned to the base alignment of the struct that came before.
		// GL 4.5 spec, 7.6.2.2.
		if (memb_type.basetype == SPIRType::Struct)
			pad_alignment = std430_alignment;
		else
			pad_alignment = 1;

		uint32_t actual_offset = type_struct_member_offset(type, i);
		if (actual_offset != offset) // This cannot be std430.
			return false;

		// Verify array stride rules.
		if (!memb_type.array.empty() &&
		    type_to_std430_array_stride(memb_type, member_flags) != type_struct_member_array_stride(type, i))
			return false;

		// Verify that sub-structs also follow std430 rules.
		if (!memb_type.member_types.empty() && !ssbo_is_std430_packing(memb_type))
			return false;

		// Bump size.
		offset += type_to_std430_size(memb_type, member_flags);
	}

	return true;
}

string CompilerGLSL::layout_for_variable(const SPIRVariable &var)
{
	// FIXME: Come up with a better solution for when to disable layouts.
	// Having layouts depend on extensions as well as which types
	// of layouts are used. For now, the simple solution is to just disable
	// layouts for legacy versions.
	if (is_legacy())
		return "";

	vector<string> attr;

	auto &dec = meta[var.self].decoration;
	auto &type = get<SPIRType>(var.basetype);
	auto flags = dec.decoration_flags;
	auto typeflags = meta[type.self].decoration.decoration_flags;

	if (options.vulkan_semantics && var.storage == StorageClassPushConstant)
		attr.push_back("push_constant");

	if (flags & (1ull << DecorationRowMajor))
		attr.push_back("row_major");
	if (flags & (1ull << DecorationColMajor))
		attr.push_back("column_major");

	if (options.vulkan_semantics)
	{
		if (flags & (1ull << DecorationInputAttachmentIndex))
			attr.push_back(join("input_attachment_index = ", dec.input_attachment));
	}

	if (flags & (1ull << DecorationLocation))
	{
		uint64_t combined_decoration = 0;
		for (uint32_t i = 0; i < meta[type.self].members.size(); i++)
			combined_decoration |= combined_decoration_for_member(type, i);

		// If our members have location decorations, we don't need to
		// emit location decorations at the top as well (looks weird).
		if ((combined_decoration & (1ull << DecorationLocation)) == 0)
			attr.push_back(join("location = ", dec.location));
	}

	// set = 0 is the default. Do not emit set = decoration in regular GLSL output, but
	// we should preserve it in Vulkan GLSL mode.
	if (var.storage != StorageClassPushConstant)
	{
		if ((flags & (1ull << DecorationDescriptorSet)) && (dec.set != 0 || options.vulkan_semantics))
			attr.push_back(join("set = ", dec.set));
	}

	if (flags & (1ull << DecorationBinding))
		attr.push_back(join("binding = ", dec.binding));
	if (flags & (1ull << DecorationCoherent))
		attr.push_back("coherent");
	if (flags & (1ull << DecorationOffset))
		attr.push_back(join("offset = ", dec.offset));

	// Instead of adding explicit offsets for every element here, just assume we're using std140 or std430.
	// If SPIR-V does not comply with either layout, we cannot really work around it.
	if (var.storage == StorageClassUniform && (typeflags & (1ull << DecorationBlock)))
		attr.push_back("std140");
	else if (var.storage == StorageClassUniform && (typeflags & (1ull << DecorationBufferBlock)))
		attr.push_back(ssbo_is_std430_packing(type) ? "std430" : "std140");
	else if (options.vulkan_semantics && var.storage == StorageClassPushConstant)
		attr.push_back(ssbo_is_std430_packing(type) ? "std430" : "std140");

	// For images, the type itself adds a layout qualifer.
	if (type.basetype == SPIRType::Image)
	{
		const char *fmt = format_to_glsl(type.image.format);
		if (fmt)
			attr.push_back(fmt);
	}

	if (attr.empty())
		return "";

	string res = "layout(";
	res += merge(attr);
	res += ") ";
	return res;
}

void CompilerGLSL::emit_push_constant_block(const SPIRVariable &var)
{
	if (options.vulkan_semantics)
		emit_push_constant_block_vulkan(var);
	else
		emit_push_constant_block_glsl(var);
}

void CompilerGLSL::emit_push_constant_block_vulkan(const SPIRVariable &var)
{
	emit_buffer_block(var);
}

void CompilerGLSL::emit_push_constant_block_glsl(const SPIRVariable &var)
{
	// OpenGL has no concept of push constant blocks, implement it as a uniform struct.
	auto &type = get<SPIRType>(var.basetype);

	auto &flags = meta[var.self].decoration.decoration_flags;
	flags &= ~((1ull << DecorationBinding) | (1ull << DecorationDescriptorSet));

#if 0
    if (flags & ((1ull << DecorationBinding) | (1ull << DecorationDescriptorSet)))
        SPIRV_CROSS_THROW("Push constant blocks cannot be compiled to GLSL with Binding or Set syntax. "
                            "Remap to location with reflection API first or disable these decorations.");
#endif

	// We're emitting the push constant block as a regular struct, so disable the block qualifier temporarily.
	// Otherwise, we will end up emitting layout() qualifiers on naked structs which is not allowed.
	auto &block_flags = meta[type.self].decoration.decoration_flags;
	uint64_t block_flag = block_flags & (1ull << DecorationBlock);
	block_flags &= ~block_flag;

	emit_struct(type);

	block_flags |= block_flag;

	emit_uniform(var);
	statement("");
}

void CompilerGLSL::emit_buffer_block(const SPIRVariable &var)
{
	auto &type = get<SPIRType>(var.basetype);
	bool ssbo = (meta[type.self].decoration.decoration_flags & (1ull << DecorationBufferBlock)) != 0;
	bool is_restrict = (meta[var.self].decoration.decoration_flags & (1ull << DecorationRestrict)) != 0;

	add_resource_name(var.self);

	// Block names should never alias.
	auto buffer_name = to_name(type.self, false);

	// Shaders never use the block by interface name, so we don't
	// have to track this other than updating name caches.
	if (resource_names.find(buffer_name) != end(resource_names))
		buffer_name = get_fallback_name(type.self);
	else
		resource_names.insert(buffer_name);

	statement(layout_for_variable(var), is_restrict ? "restrict " : "", ssbo ? "buffer " : "uniform ", buffer_name);
	begin_scope();

	type.member_name_cache.clear();

	uint32_t i = 0;
	for (auto &member : type.member_types)
	{
		add_member_name(type, i);

		auto &membertype = get<SPIRType>(member);
		statement(member_decl(type, membertype, i), ";");
		i++;
	}

	end_scope_decl(to_name(var.self) + type_to_array_glsl(type));
	statement("");
}

void CompilerGLSL::emit_interface_block(const SPIRVariable &var)
{
	auto &execution = get_entry_point();
	auto &type = get<SPIRType>(var.basetype);

	// Either make it plain in/out or in/out blocks depending on what shader is doing ...
	bool block = (meta[type.self].decoration.decoration_flags & (1ull << DecorationBlock)) != 0;

	const char *qual = nullptr;
	if (is_legacy() && execution.model == ExecutionModelVertex)
		qual = var.storage == StorageClassInput ? "attribute " : "varying ";
	else if (is_legacy() && execution.model == ExecutionModelFragment)
		qual = "varying "; // Fragment outputs are renamed so they never hit this case.
	else
		qual = var.storage == StorageClassInput ? "in " : "out ";

	if (block)
	{
		add_resource_name(var.self);

		// Block names should never alias.
		auto block_name = to_name(type.self, false);

		// Shaders never use the block by interface name, so we don't
		// have to track this other than updating name caches.
		if (resource_names.find(block_name) != end(resource_names))
			block_name = get_fallback_name(type.self);
		else
			resource_names.insert(block_name);

		statement(layout_for_variable(var), qual, block_name);
		begin_scope();

		type.member_name_cache.clear();

		uint32_t i = 0;
		for (auto &member : type.member_types)
		{
			add_member_name(type, i);

			auto &membertype = get<SPIRType>(member);
			statement(member_decl(type, membertype, i), ";");
			i++;
		}

		end_scope_decl(join(to_name(var.self), type_to_array_glsl(type)));
		statement("");
	}
	else
	{
		add_resource_name(var.self);
		statement(layout_for_variable(var), qual, variable_decl(var), ";");
	}
}

void CompilerGLSL::emit_uniform(const SPIRVariable &var)
{
	auto &type = get<SPIRType>(var.basetype);
	if (type.basetype == SPIRType::Image && type.image.sampled == 2)
	{
		if (!options.es && options.version < 420)
			require_extension("GL_ARB_shader_image_load_store");
		else if (options.es && options.version < 310)
			SPIRV_CROSS_THROW("At least ESSL 3.10 required for shader image load store.");
	}

	add_resource_name(var.self);
	statement(layout_for_variable(var), "uniform ", variable_decl(var), ";");
}

void CompilerGLSL::emit_specialization_constant(const SPIRConstant &constant)
{
	auto &type = get<SPIRType>(constant.constant_type);
	auto name = to_name(constant.self);

	statement("layout(constant_id = ", get_decoration(constant.self, DecorationSpecId), ") const ",
	          variable_decl(type, name), " = ", constant_expression(constant), ";");
}

void CompilerGLSL::replace_illegal_names()
{
	// clang-format off
	static const unordered_set<string> keywords = {
		"active", "asm", "atomic_uint", "attribute", "bool", "break", 
		"bvec2", "bvec3", "bvec4", "case", "cast", "centroid", "class", "coherent", "common", "const", "continue", "default", "discard", 
		"dmat2", "dmat2x2", "dmat2x3", "dmat2x4", "dmat3", "dmat3x2", "dmat3x3", "dmat3x4", "dmat4", "dmat4x2", "dmat4x3", "dmat4x4", 
		"do", "double", "dvec2", "dvec3", "dvec4", "else", "enum", "extern", "external", "false", "filter", "fixed", "flat", "float", 
		"for", "fvec2", "fvec3", "fvec4", "goto", "half", "highp", "hvec2", "hvec3", "hvec4", "if", "iimage1D", "iimage1DArray", 
		"iimage2D", "iimage2DArray", "iimage2DMS", "iimage2DMSArray", "iimage2DRect", "iimage3D", "iimageBuffer", "iimageCube", 
		"iimageCubeArray", "image1D", "image1DArray", "image2D", "image2DArray", "image2DMS", "image2DMSArray", "image2DRect", 
		"image3D", "imageBuffer", "imageCube", "imageCubeArray", "in", "inline", "inout", "input", "int", "interface", "invariant", 
		"isampler1D", "isampler1DArray", "isampler2D", "isampler2DArray", "isampler2DMS", "isampler2DMSArray", "isampler2DRect", 
		"isampler3D", "isamplerBuffer", "isamplerCube", "isamplerCubeArray", "ivec2", "ivec3", "ivec4", "layout", "long", "lowp", 
		"mat2", "mat2x2", "mat2x3", "mat2x4", "mat3", "mat3x2", "mat3x3", "mat3x4", "mat4", "mat4x2", "mat4x3", "mat4x4", "mediump", 
		"namespace", "noinline", "noperspective", "out", "output", "packed", "partition", "patch", "precision", "public", "readonly", 
		"resource", "restrict", "return", "row_major", "sample", "sampler1D", "sampler1DArray", "sampler1DArrayShadow", 
		"sampler1DShadow", "sampler2D", "sampler2DArray", "sampler2DArrayShadow", "sampler2DMS", "sampler2DMSArray", 
		"sampler2DRect", "sampler2DRectShadow", "sampler2DShadow", "sampler3D", "sampler3DRect", "samplerBuffer", 
		"samplerCube", "samplerCubeArray", "samplerCubeArrayShadow", "samplerCubeShadow", "short", "sizeof", "smooth", "static", 
		"struct", "subroutine", "superp", "switch", "template", "this", "true", "typedef", "uimage1D", "uimage1DArray", "uimage2D", 
		"uimage2DArray", "uimage2DMS", "uimage2DMSArray", "uimage2DRect", "uimage3D", "uimageBuffer", "uimageCube", 
		"uimageCubeArray", "uint", "uniform", "union", "unsigned", "usampler1D", "usampler1DArray", "usampler2D", "usampler2DArray", 
		"usampler2DMS", "usampler2DMSArray", "usampler2DRect", "usampler3D", "usamplerBuffer", "usamplerCube", 
		"usamplerCubeArray", "using", "uvec2", "uvec3", "uvec4", "varying", "vec2", "vec3", "vec4", "void", "volatile", "volatile", 
		"while", "writeonly"
	};
	// clang-format on

	for (auto &id : ids)
	{
		if (id.get_type() == TypeVariable)
		{
			auto &var = id.get<SPIRVariable>();
			if (!is_hidden_variable(var))
			{
				auto &m = meta[var.self].decoration;
				if (m.alias.compare(0, 3, "gl_") == 0 || keywords.find(m.alias) != end(keywords))
					m.alias = join("_", m.alias);
			}
		}
	}
}

void CompilerGLSL::replace_fragment_output(SPIRVariable &var)
{
	auto &m = meta[var.self].decoration;
	uint32_t location = 0;
	if (m.decoration_flags & (1ull << DecorationLocation))
		location = m.location;

	// If our variable is arrayed, we must not emit the array part of this as the SPIR-V will
	// do the access chain part of this for us.
	auto &type = get<SPIRType>(var.basetype);

	if (type.array.empty())
	{
		// Redirect the write to a specific render target in legacy GLSL.
		m.alias = join("gl_FragData[", location, "]");

		if (is_legacy_es() && location != 0)
			require_extension("GL_EXT_draw_buffers");
	}
	else if (type.array.size() == 1)
	{
		// If location is non-zero, we probably have to add an offset.
		// This gets really tricky since we'd have to inject an offset in the access chain.
		// FIXME: This seems like an extremely odd-ball case, so it's probably fine to leave it like this for now.
		m.alias = "gl_FragData";
		if (location != 0)
			SPIRV_CROSS_THROW("Arrayed output variable used, but location is not 0. "
			                  "This is unimplemented in SPIRV-Cross.");

		if (is_legacy_es())
			require_extension("GL_EXT_draw_buffers");
	}
	else
		SPIRV_CROSS_THROW("Array-of-array output variable used. This cannot be implemented in legacy GLSL.");

	var.compat_builtin = true; // We don't want to declare this variable, but use the name as-is.
}

void CompilerGLSL::replace_fragment_outputs()
{
	for (auto &id : ids)
	{
		if (id.get_type() == TypeVariable)
		{
			auto &var = id.get<SPIRVariable>();
			auto &type = get<SPIRType>(var.basetype);

			if (!is_builtin_variable(var) && !var.remapped_variable && type.pointer &&
			    var.storage == StorageClassOutput)
				replace_fragment_output(var);
		}
	}
}

string CompilerGLSL::remap_swizzle(uint32_t result_type, uint32_t input_components, uint32_t expr)
{
	auto &out_type = get<SPIRType>(result_type);

	if (out_type.vecsize == input_components)
		return to_expression(expr);
	else if (input_components == 1)
		return join(type_to_glsl(out_type), "(", to_expression(expr), ")");
	else
	{
		auto e = to_enclosed_expression(expr) + ".";
		// Just clamp the swizzle index if we have more outputs than inputs.
		for (uint32_t c = 0; c < out_type.vecsize; c++)
			e += index_to_swizzle(min(c, input_components - 1));
		if (backend.swizzle_is_function && out_type.vecsize > 1)
			e += "()";
		return e;
	}
}

void CompilerGLSL::emit_pls()
{
	auto &execution = get_entry_point();
	if (execution.model != ExecutionModelFragment)
		SPIRV_CROSS_THROW("Pixel local storage only supported in fragment shaders.");

	if (!options.es)
		SPIRV_CROSS_THROW("Pixel local storage only supported in OpenGL ES.");

	if (options.version < 300)
		SPIRV_CROSS_THROW("Pixel local storage only supported in ESSL 3.0 and above.");

	if (!pls_inputs.empty())
	{
		statement("__pixel_local_inEXT _PLSIn");
		begin_scope();
		for (auto &input : pls_inputs)
			statement(pls_decl(input), ";");
		end_scope_decl();
		statement("");
	}

	if (!pls_outputs.empty())
	{
		statement("__pixel_local_outEXT _PLSOut");
		begin_scope();
		for (auto &output : pls_outputs)
			statement(pls_decl(output), ";");
		end_scope_decl();
		statement("");
	}
}

void CompilerGLSL::emit_resources()
{
	auto &execution = get_entry_point();

	replace_illegal_names();

	// Legacy GL uses gl_FragData[], redeclare all fragment outputs
	// with builtins.
	if (execution.model == ExecutionModelFragment && is_legacy())
		replace_fragment_outputs();

	// Emit PLS blocks if we have such variables.
	if (!pls_inputs.empty() || !pls_outputs.empty())
		emit_pls();

	bool emitted = false;

	// If emitted Vulkan GLSL,
	// emit specialization constants as actual floats,
	// spec op expressions will redirect to the constant name.
	//
	// TODO: If we have the fringe case that we create a spec constant which depends on a struct type,
	// we'll have to deal with that, but there's currently no known way to express that.
	if (options.vulkan_semantics)
	{
		for (auto &id : ids)
		{
			if (id.get_type() == TypeConstant)
			{
				auto &c = id.get<SPIRConstant>();
				if (!c.specialization)
					continue;

				emit_specialization_constant(c);
				emitted = true;
			}
		}
	}

	if (emitted)
		statement("");
	emitted = false;

	// Output all basic struct types which are not Block or BufferBlock as these are declared inplace
	// when such variables are instantiated.
	for (auto &id : ids)
	{
		if (id.get_type() == TypeType)
		{
			auto &type = id.get<SPIRType>();
			if (type.basetype == SPIRType::Struct && type.array.empty() && !type.pointer &&
			    (meta[type.self].decoration.decoration_flags &
			     ((1ull << DecorationBlock) | (1ull << DecorationBufferBlock))) == 0)
			{
				emit_struct(type);
			}
		}
	}

	// Output UBOs and SSBOs
	for (auto &id : ids)
	{
		if (id.get_type() == TypeVariable)
		{
			auto &var = id.get<SPIRVariable>();
			auto &type = get<SPIRType>(var.basetype);

			if (var.storage != StorageClassFunction && type.pointer && type.storage == StorageClassUniform &&
			    !is_hidden_variable(var) && (meta[type.self].decoration.decoration_flags &
			                                 ((1ull << DecorationBlock) | (1ull << DecorationBufferBlock))))
			{
				emit_buffer_block(var);
			}
		}
	}

	// Output push constant blocks
	for (auto &id : ids)
	{
		if (id.get_type() == TypeVariable)
		{
			auto &var = id.get<SPIRVariable>();
			auto &type = get<SPIRType>(var.basetype);
			if (var.storage != StorageClassFunction && type.pointer && type.storage == StorageClassPushConstant &&
			    !is_hidden_variable(var))
			{
				emit_push_constant_block(var);
			}
		}
	}

	bool skip_separate_image_sampler = !combined_image_samplers.empty() || !options.vulkan_semantics;

	// Output Uniform Constants (values, samplers, images, etc).
	for (auto &id : ids)
	{
		if (id.get_type() == TypeVariable)
		{
			auto &var = id.get<SPIRVariable>();
			auto &type = get<SPIRType>(var.basetype);

			// If we're remapping separate samplers and images, only emit the combined samplers.
			if (skip_separate_image_sampler)
			{
				bool separate_image = type.basetype == SPIRType::Image && type.image.sampled == 1;
				bool separate_sampler = type.basetype == SPIRType::Sampler;
				if (separate_image || separate_sampler)
					continue;
			}

			if (var.storage != StorageClassFunction && type.pointer &&
			    (type.storage == StorageClassUniformConstant || type.storage == StorageClassAtomicCounter) &&
			    !is_hidden_variable(var))
			{
				emit_uniform(var);
				emitted = true;
			}
		}
	}

	if (emitted)
		statement("");
	emitted = false;

	// Output in/out interfaces.
	for (auto &id : ids)
	{
		if (id.get_type() == TypeVariable)
		{
			auto &var = id.get<SPIRVariable>();
			auto &type = get<SPIRType>(var.basetype);

			if (var.storage != StorageClassFunction && type.pointer &&
			    (var.storage == StorageClassInput || var.storage == StorageClassOutput) &&
			    interface_variable_exists_in_entry_point(var.self) && !is_hidden_variable(var))
			{
				emit_interface_block(var);
				emitted = true;
			}
			else if (is_builtin_variable(var))
			{
				// For gl_InstanceIndex emulation on GLES, the API user needs to
				// supply this uniform.
				if (meta[var.self].decoration.builtin_type == BuiltInInstanceIndex && !options.vulkan_semantics)
				{
					statement("uniform int SPIRV_Cross_BaseInstance;");
					emitted = true;
				}
			}
		}
	}

	// Global variables.
	for (auto global : global_variables)
	{
		auto &var = get<SPIRVariable>(global);
		if (var.storage != StorageClassOutput)
		{
			add_resource_name(var.self);
			statement(variable_decl(var), ";");
			emitted = true;
		}
	}

	if (emitted)
		statement("");
}

// Returns a string representation of the ID, usable as a function arg.
// Default is to simply return the expression representation fo the arg ID.
// Subclasses may override to modify the return value.
string CompilerGLSL::to_func_call_arg(uint32_t id)
{
	return to_expression(id);
}

void CompilerGLSL::handle_invalid_expression(uint32_t id)
{
	auto &expr = get<SPIRExpression>(id);

	// This expression has been invalidated in the past.
	// Be careful with this expression next pass ...
	// Used for OpCompositeInsert forwarding atm.
	expr.used_while_invalidated = true;

	// We tried to read an invalidated expression.
	// This means we need another pass at compilation, but next time, force temporary variables so that they cannot be invalidated.
	forced_temporaries.insert(id);
	force_recompile = true;
}

// Sometimes we proactively enclosed an expression where it turns out we might have not needed it after all.
void CompilerGLSL::strip_enclosed_expression(string &expr)
{
	if (expr.size() < 2 || expr.front() != '(' || expr.back() != ')')
		return;

	// Have to make sure that our first and last parens actually enclose everything inside it.
	uint32_t paren_count = 0;
	for (auto &c : expr)
	{
		if (c == '(')
			paren_count++;
		else if (c == ')')
		{
			paren_count--;

			// If we hit 0 and this is not the final char, our first and final parens actually don't
			// enclose the expression, and we cannot strip, e.g.: (a + b) * (c + d).
			if (paren_count == 0 && &c != &expr.back())
				return;
		}
	}
	expr.pop_back();
	expr.erase(begin(expr));
}

// Just like to_expression except that we enclose the expression inside parentheses if needed.
string CompilerGLSL::to_enclosed_expression(uint32_t id)
{
	auto expr = to_expression(id);
	bool need_parens = false;
	uint32_t paren_count = 0;
	for (auto c : expr)
	{
		if (c == '(')
			paren_count++;
		else if (c == ')')
		{
			assert(paren_count);
			paren_count--;
		}
		else if (c == ' ' && paren_count == 0)
		{
			need_parens = true;
			break;
		}
	}
	assert(paren_count == 0);

	// If this expression contains any spaces which are not enclosed by parentheses,
	// we need to enclose it so we can treat the whole string as an expression.
	// This happens when two expressions have been part of a binary op earlier.
	if (need_parens)
		return join('(', expr, ')');
	else
		return expr;
}

string CompilerGLSL::to_expression(uint32_t id)
{
	auto itr = invalid_expressions.find(id);
	if (itr != end(invalid_expressions))
		handle_invalid_expression(id);

	if (ids[id].get_type() == TypeExpression)
	{
		// We might have a more complex chain of dependencies.
		// A possible scenario is that we
		//
		// %1 = OpLoad
		// %2 = OpDoSomething %1 %1. here %2 will have a dependency on %1.
		// %3 = OpDoSomethingAgain %2 %2. Here %3 will lose the link to %1 since we don't propagate the dependencies like that.
		// OpStore %1 %foo // Here we can invalidate %1, and hence all expressions which depend on %1. Only %2 will know since it's part of invalid_expressions.
		// %4 = OpDoSomethingAnotherTime %3 %3 // If we forward all expressions we will see %1 expression after store, not before.
		//
		// However, we can propagate up a list of depended expressions when we used %2, so we can check if %2 is invalid when reading %3 after the store,
		// and see that we should not forward reads of the original variable.
		auto &expr = get<SPIRExpression>(id);
		for (uint32_t dep : expr.expression_dependencies)
			if (invalid_expressions.find(dep) != end(invalid_expressions))
				handle_invalid_expression(dep);
	}

	track_expression_read(id);

	switch (ids[id].get_type())
	{
	case TypeExpression:
	{
		auto &e = get<SPIRExpression>(id);
		if (e.base_expression)
			return to_enclosed_expression(e.base_expression) + e.expression;
		else
			return e.expression;
	}

	case TypeConstant:
	{
		auto &c = get<SPIRConstant>(id);
		if (c.specialization && options.vulkan_semantics)
			return to_name(id);
		else
			return constant_expression(c);
	}

	case TypeConstantOp:
		return constant_op_expression(get<SPIRConstantOp>(id));

	case TypeVariable:
	{
		auto &var = get<SPIRVariable>(id);
		// If we try to use a loop variable before the loop header, we have to redirect it to the static expression,
		// the variable has not been declared yet.
		if (var.statically_assigned || (var.loop_variable && !var.loop_variable_enable))
			return to_expression(var.static_expression);
		else if (var.deferred_declaration)
		{
			var.deferred_declaration = false;
			return variable_decl(var);
		}
		else
		{
			auto &dec = meta[var.self].decoration;
			if (dec.builtin)
				return builtin_to_glsl(dec.builtin_type);
			else
				return to_name(id);
		}
	}

	default:
		return to_name(id);
	}
}

string CompilerGLSL::constant_op_expression(const SPIRConstantOp &cop)
{
	auto &type = get<SPIRType>(cop.basetype);
	bool binary = false;
	bool unary = false;
	string op;

	// TODO: Find a clean way to reuse emit_instruction.
	switch (cop.opcode)
	{
	case OpSConvert:
	case OpUConvert:
	case OpFConvert:
		op = type_to_glsl_constructor(type);
		break;

#define BOP(opname, x) \
	case Op##opname:   \
		binary = true; \
		op = x;        \
		break

#define UOP(opname, x) \
	case Op##opname:   \
		unary = true;  \
		op = x;        \
		break

		UOP(SNegate, "-");
		UOP(Not, "~");
		BOP(IAdd, "+");
		BOP(ISub, "-");
		BOP(IMul, "*");
		BOP(SDiv, "/");
		BOP(UDiv, "/");
		BOP(UMod, "%");
		BOP(SMod, "%");
		BOP(ShiftRightLogical, ">>");
		BOP(ShiftRightArithmetic, ">>");
		BOP(ShiftLeftLogical, "<<");
		BOP(BitwiseOr, "|");
		BOP(BitwiseXor, "^");
		BOP(BitwiseAnd, "&");
		BOP(LogicalOr, "||");
		BOP(LogicalAnd, "&&");
		UOP(LogicalNot, "!");
		BOP(LogicalEqual, "==");
		BOP(LogicalNotEqual, "!=");
		BOP(IEqual, "==");
		BOP(INotEqual, "!=");
		BOP(ULessThan, "<");
		BOP(SLessThan, "<");
		BOP(ULessThanEqual, "<=");
		BOP(SLessThanEqual, "<=");
		BOP(UGreaterThan, ">");
		BOP(SGreaterThan, ">");
		BOP(UGreaterThanEqual, ">=");
		BOP(SGreaterThanEqual, ">=");

	case OpSelect:
	{
		if (cop.arguments.size() < 3)
			SPIRV_CROSS_THROW("Not enough arguments to OpSpecConstantOp.");

		// This one is pretty annoying. It's triggered from
		// uint(bool), int(bool) from spec constants.
		// In order to preserve its compile-time constness in Vulkan GLSL,
		// we need to reduce the OpSelect expression back to this simplified model.
		// If we cannot, fail.
		if (!to_trivial_mix_op(type, op, cop.arguments[2], cop.arguments[1], cop.arguments[0]))
		{
			SPIRV_CROSS_THROW(
			    "Cannot implement specialization constant op OpSelect. "
			    "Need trivial select implementation which can be resolved to a simple cast from boolean.");
		}
		break;
	}

	default:
		// Some opcodes are unimplemented here, these are currently not possible to test from glslang.
		SPIRV_CROSS_THROW("Unimplemented spec constant op.");
	}

	SPIRType::BaseType input_type;
	bool skip_cast_if_equal_type = opcode_is_sign_invariant(cop.opcode);

	switch (cop.opcode)
	{
	case OpIEqual:
	case OpINotEqual:
		input_type = SPIRType::Int;
		break;

	default:
		input_type = type.basetype;
		break;
	}

#undef BOP
#undef UOP
	if (binary)
	{
		if (cop.arguments.size() < 2)
			SPIRV_CROSS_THROW("Not enough arguments to OpSpecConstantOp.");

		string cast_op0;
		string cast_op1;
		auto expected_type = binary_op_bitcast_helper(cast_op0, cast_op1, input_type, cop.arguments[0],
		                                              cop.arguments[1], skip_cast_if_equal_type);

		if (type.basetype != input_type && type.basetype != SPIRType::Boolean)
		{
			expected_type.basetype = input_type;
			auto expr = bitcast_glsl_op(type, expected_type);
			expr += '(';
			expr += join(cast_op0, " ", op, " ", cast_op1);
			expr += ')';
			return expr;
		}
		else
			return join("(", cast_op0, " ", op, " ", cast_op1, ")");
	}
	else if (unary)
	{
		if (cop.arguments.size() < 1)
			SPIRV_CROSS_THROW("Not enough arguments to OpSpecConstantOp.");

		// Auto-bitcast to result type as needed.
		// Works around various casting scenarios in glslang as there is no OpBitcast for specialization constants.
		return join("(", op, bitcast_glsl(type, cop.arguments[0]), ")");
	}
	else
	{
		if (cop.arguments.size() < 1)
			SPIRV_CROSS_THROW("Not enough arguments to OpSpecConstantOp.");
		return join(op, "(", to_expression(cop.arguments[0]), ")");
	}
}

string CompilerGLSL::constant_expression(const SPIRConstant &c)
{
	if (!c.subconstants.empty())
	{
		// Handles Arrays and structures.
		string res;
		if (backend.use_initializer_list)
			res = "{ ";
		else
			res = type_to_glsl_constructor(get<SPIRType>(c.constant_type)) + "(";

		for (auto &elem : c.subconstants)
		{
			auto &subc = get<SPIRConstant>(elem);
			if (subc.specialization && options.vulkan_semantics)
				res += to_name(elem);
			else
				res += constant_expression(get<SPIRConstant>(elem));

			if (&elem != &c.subconstants.back())
				res += ", ";
		}

		res += backend.use_initializer_list ? " }" : ")";
		return res;
	}
	else if (c.columns() == 1)
	{
		return constant_expression_vector(c, 0);
	}
	else
	{
		string res = type_to_glsl(get<SPIRType>(c.constant_type)) + "(";
		for (uint32_t col = 0; col < c.columns(); col++)
		{
			res += constant_expression_vector(c, col);
			if (col + 1 < c.columns())
				res += ", ";
		}
		res += ")";
		return res;
	}
}

string CompilerGLSL::constant_expression_vector(const SPIRConstant &c, uint32_t vector)
{
	auto type = get<SPIRType>(c.constant_type);
	type.columns = 1;

	string res;
	if (c.vector_size() > 1)
		res += type_to_glsl(type) + "(";

	bool splat = c.vector_size() > 1;
	if (splat)
	{
		if (type_to_std430_base_size(type) == 8)
		{
			uint64_t ident = c.scalar_u64(vector, 0);
			for (uint32_t i = 1; i < c.vector_size(); i++)
				if (ident != c.scalar_u64(vector, i))
					splat = false;
		}
		else
		{
			uint32_t ident = c.scalar(vector, 0);
			for (uint32_t i = 1; i < c.vector_size(); i++)
				if (ident != c.scalar(vector, i))
					splat = false;
		}
	}

	switch (type.basetype)
	{
	case SPIRType::Float:
		if (splat)
		{
			res += convert_to_string(c.scalar_f32(vector, 0));
			if (backend.float_literal_suffix)
				res += "f";
		}
		else
		{
			for (uint32_t i = 0; i < c.vector_size(); i++)
			{
				res += convert_to_string(c.scalar_f32(vector, i));
				if (backend.float_literal_suffix)
					res += "f";
				if (i + 1 < c.vector_size())
					res += ", ";
			}
		}
		break;

	case SPIRType::Double:
		if (splat)
		{
			res += convert_to_string(c.scalar_f64(vector, 0));
			if (backend.double_literal_suffix)
				res += "lf";
		}
		else
		{
			for (uint32_t i = 0; i < c.vector_size(); i++)
			{
				res += convert_to_string(c.scalar_f64(vector, i));
				if (backend.double_literal_suffix)
					res += "lf";
				if (i + 1 < c.vector_size())
					res += ", ";
			}
		}
		break;

	case SPIRType::Int64:
		if (splat)
		{
			res += convert_to_string(c.scalar_i64(vector, 0));
			if (backend.long_long_literal_suffix)
				res += "ll";
			else
				res += "l";
		}
		else
		{
			for (uint32_t i = 0; i < c.vector_size(); i++)
			{
				res += convert_to_string(c.scalar_i64(vector, i));
				if (backend.long_long_literal_suffix)
					res += "ll";
				else
					res += "l";
				if (i + 1 < c.vector_size())
					res += ", ";
			}
		}
		break;

	case SPIRType::UInt64:
		if (splat)
		{
			res += convert_to_string(c.scalar_u64(vector, 0));
			if (backend.long_long_literal_suffix)
				res += "ull";
			else
				res += "ul";
		}
		else
		{
			for (uint32_t i = 0; i < c.vector_size(); i++)
			{
				res += convert_to_string(c.scalar_u64(vector, i));
				if (backend.long_long_literal_suffix)
					res += "ull";
				else
					res += "ul";
				if (i + 1 < c.vector_size())
					res += ", ";
			}
		}
		break;

	case SPIRType::UInt:
		if (splat)
		{
			res += convert_to_string(c.scalar(vector, 0));
			if (backend.uint32_t_literal_suffix)
				res += "u";
		}
		else
		{
			for (uint32_t i = 0; i < c.vector_size(); i++)
			{
				res += convert_to_string(c.scalar(vector, i));
				if (backend.uint32_t_literal_suffix)
					res += "u";
				if (i + 1 < c.vector_size())
					res += ", ";
			}
		}
		break;

	case SPIRType::Int:
		if (splat)
			res += convert_to_string(c.scalar_i32(vector, 0));
		else
		{
			for (uint32_t i = 0; i < c.vector_size(); i++)
			{
				res += convert_to_string(c.scalar_i32(vector, i));
				if (i + 1 < c.vector_size())
					res += ", ";
			}
		}
		break;

	case SPIRType::Boolean:
		if (splat)
			res += c.scalar(vector, 0) ? "true" : "false";
		else
		{
			for (uint32_t i = 0; i < c.vector_size(); i++)
			{
				res += c.scalar(vector, i) ? "true" : "false";
				if (i + 1 < c.vector_size())
					res += ", ";
			}
		}
		break;

	default:
		SPIRV_CROSS_THROW("Invalid constant expression basetype.");
	}

	if (c.vector_size() > 1)
		res += ")";

	return res;
}

string CompilerGLSL::declare_temporary(uint32_t result_type, uint32_t result_id)
{
	auto &type = get<SPIRType>(result_type);
	auto flags = meta[result_id].decoration.decoration_flags;

	// If we're declaring temporaries inside continue blocks,
	// we must declare the temporary in the loop header so that the continue block can avoid declaring new variables.
	if (current_continue_block)
	{
		auto &header = get<SPIRBlock>(current_continue_block->loop_dominator);
		if (find_if(begin(header.declare_temporary), end(header.declare_temporary),
		            [result_type, result_id](const pair<uint32_t, uint32_t> &tmp) {
			            return tmp.first == result_type && tmp.second == result_id;
			        }) == end(header.declare_temporary))
		{
			header.declare_temporary.emplace_back(result_type, result_id);
			force_recompile = true;
		}

		return join(to_name(result_id), " = ");
	}
	else
	{
		// The result_id has not been made into an expression yet, so use flags interface.
		return join(flags_to_precision_qualifiers_glsl(type, flags), variable_decl(type, to_name(result_id)), " = ");
	}
}

bool CompilerGLSL::expression_is_forwarded(uint32_t id)
{
	return forwarded_temporaries.find(id) != end(forwarded_temporaries);
}

SPIRExpression &CompilerGLSL::emit_op(uint32_t result_type, uint32_t result_id, const string &rhs, bool forwarding,
                                      bool suppress_usage_tracking)
{
	if (forwarding && (forced_temporaries.find(result_id) == end(forced_temporaries)))
	{
		// Just forward it without temporary.
		// If the forward is trivial, we do not force flushing to temporary for this expression.
		if (!suppress_usage_tracking)
			forwarded_temporaries.insert(result_id);

		return set<SPIRExpression>(result_id, rhs, result_type, true);
	}
	else
	{
		// If expression isn't immutable, bind it to a temporary and make the new temporary immutable (they always are).
		statement(declare_temporary(result_type, result_id), rhs, ";");
		return set<SPIRExpression>(result_id, to_name(result_id), result_type, true);
	}
}

void CompilerGLSL::emit_unary_op(uint32_t result_type, uint32_t result_id, uint32_t op0, const char *op)
{
	bool forward = should_forward(op0);
	emit_op(result_type, result_id, join(op, to_enclosed_expression(op0)), forward);

	if (forward && forced_temporaries.find(result_id) == end(forced_temporaries))
		inherit_expression_dependencies(result_id, op0);
}

void CompilerGLSL::emit_binary_op(uint32_t result_type, uint32_t result_id, uint32_t op0, uint32_t op1, const char *op)
{
	bool forward = should_forward(op0) && should_forward(op1);
	emit_op(result_type, result_id, join(to_enclosed_expression(op0), " ", op, " ", to_enclosed_expression(op1)),
	        forward);

	if (forward && forced_temporaries.find(result_id) == end(forced_temporaries))
	{
		inherit_expression_dependencies(result_id, op0);
		inherit_expression_dependencies(result_id, op1);
	}
}

SPIRType CompilerGLSL::binary_op_bitcast_helper(string &cast_op0, string &cast_op1, SPIRType::BaseType &input_type,
                                                uint32_t op0, uint32_t op1, bool skip_cast_if_equal_type)
{
	auto &type0 = expression_type(op0);
	auto &type1 = expression_type(op1);

	// We have to bitcast if our inputs are of different type, or if our types are not equal to expected inputs.
	// For some functions like OpIEqual and INotEqual, we don't care if inputs are of different types than expected
	// since equality test is exactly the same.
	bool cast = (type0.basetype != type1.basetype) || (!skip_cast_if_equal_type && type0.basetype != input_type);

	// Create a fake type so we can bitcast to it.
	// We only deal with regular arithmetic types here like int, uints and so on.
	SPIRType expected_type;
	expected_type.basetype = input_type;
	expected_type.vecsize = type0.vecsize;
	expected_type.columns = type0.columns;
	expected_type.width = type0.width;

	if (cast)
	{
		cast_op0 = bitcast_glsl(expected_type, op0);
		cast_op1 = bitcast_glsl(expected_type, op1);
	}
	else
	{
		// If we don't cast, our actual input type is that of the first (or second) argument.
		cast_op0 = to_enclosed_expression(op0);
		cast_op1 = to_enclosed_expression(op1);
		input_type = type0.basetype;
	}

	return expected_type;
}

void CompilerGLSL::emit_binary_op_cast(uint32_t result_type, uint32_t result_id, uint32_t op0, uint32_t op1,
                                       const char *op, SPIRType::BaseType input_type, bool skip_cast_if_equal_type)
{
	string cast_op0, cast_op1;
	auto expected_type = binary_op_bitcast_helper(cast_op0, cast_op1, input_type, op0, op1, skip_cast_if_equal_type);
	auto &out_type = get<SPIRType>(result_type);

	// We might have casted away from the result type, so bitcast again.
	// For example, arithmetic right shift with uint inputs.
	// Special case boolean outputs since relational opcodes output booleans instead of int/uint.
	string expr;
	if (out_type.basetype != input_type && out_type.basetype != SPIRType::Boolean)
	{
		expected_type.basetype = input_type;
		expr = bitcast_glsl_op(out_type, expected_type);
		expr += '(';
		expr += join(cast_op0, " ", op, " ", cast_op1);
		expr += ')';
	}
	else
		expr += join(cast_op0, " ", op, " ", cast_op1);

	emit_op(result_type, result_id, expr, should_forward(op0) && should_forward(op1));
}

void CompilerGLSL::emit_unary_func_op(uint32_t result_type, uint32_t result_id, uint32_t op0, const char *op)
{
	bool forward = should_forward(op0);
	emit_op(result_type, result_id, join(op, "(", to_expression(op0), ")"), forward);
	if (forward && forced_temporaries.find(result_id) == end(forced_temporaries))
		inherit_expression_dependencies(result_id, op0);
}

void CompilerGLSL::emit_binary_func_op(uint32_t result_type, uint32_t result_id, uint32_t op0, uint32_t op1,
                                       const char *op)
{
	bool forward = should_forward(op0) && should_forward(op1);
	emit_op(result_type, result_id, join(op, "(", to_expression(op0), ", ", to_expression(op1), ")"), forward);

	if (forward && forced_temporaries.find(result_id) == end(forced_temporaries))
	{
		inherit_expression_dependencies(result_id, op0);
		inherit_expression_dependencies(result_id, op1);
	}
}

void CompilerGLSL::emit_binary_func_op_cast(uint32_t result_type, uint32_t result_id, uint32_t op0, uint32_t op1,
                                            const char *op, SPIRType::BaseType input_type, bool skip_cast_if_equal_type)
{
	string cast_op0, cast_op1;
	auto expected_type = binary_op_bitcast_helper(cast_op0, cast_op1, input_type, op0, op1, skip_cast_if_equal_type);
	auto &out_type = get<SPIRType>(result_type);

	// Special case boolean outputs since relational opcodes output booleans instead of int/uint.
	string expr;
	if (out_type.basetype != input_type && out_type.basetype != SPIRType::Boolean)
	{
		expected_type.basetype = input_type;
		expr = bitcast_glsl_op(out_type, expected_type);
		expr += '(';
		expr += join(op, "(", cast_op0, ", ", cast_op1, ")");
		expr += ')';
	}
	else
	{
		expr += join(op, "(", cast_op0, ", ", cast_op1, ")");
	}

	emit_op(result_type, result_id, expr, should_forward(op0) && should_forward(op1));
}

void CompilerGLSL::emit_trinary_func_op(uint32_t result_type, uint32_t result_id, uint32_t op0, uint32_t op1,
                                        uint32_t op2, const char *op)
{
	bool forward = should_forward(op0) && should_forward(op1) && should_forward(op2);
	emit_op(result_type, result_id,
	        join(op, "(", to_expression(op0), ", ", to_expression(op1), ", ", to_expression(op2), ")"), forward);

	if (forward && forced_temporaries.find(result_id) == end(forced_temporaries))
	{
		inherit_expression_dependencies(result_id, op0);
		inherit_expression_dependencies(result_id, op1);
		inherit_expression_dependencies(result_id, op2);
	}
}

void CompilerGLSL::emit_quaternary_func_op(uint32_t result_type, uint32_t result_id, uint32_t op0, uint32_t op1,
                                           uint32_t op2, uint32_t op3, const char *op)
{
	bool forward = should_forward(op0) && should_forward(op1) && should_forward(op2) && should_forward(op3);
	emit_op(result_type, result_id, join(op, "(", to_expression(op0), ", ", to_expression(op1), ", ",
	                                     to_expression(op2), ", ", to_expression(op3), ")"),
	        forward);

	if (forward && forced_temporaries.find(result_id) == end(forced_temporaries))
	{
		inherit_expression_dependencies(result_id, op0);
		inherit_expression_dependencies(result_id, op1);
		inherit_expression_dependencies(result_id, op2);
		inherit_expression_dependencies(result_id, op3);
	}
}

string CompilerGLSL::legacy_tex_op(const std::string &op, const SPIRType &imgtype)
{
	const char *type;
	switch (imgtype.image.dim)
	{
	case spv::Dim1D:
		type = (imgtype.image.arrayed && !options.es) ? "1DArray" : "1D";
		break;
	case spv::Dim2D:
		type = (imgtype.image.arrayed && !options.es) ? "2DArray" : "2D";
		break;
	case spv::Dim3D:
		type = "3D";
		break;
	case spv::DimCube:
		type = "Cube";
		break;
	case spv::DimBuffer:
		type = "Buffer";
		break;
	case spv::DimSubpassData:
		type = "2D";
		break;
	default:
		type = "";
		break;
	}

	if (op == "textureLod" || op == "textureProjLod")
	{
		if (is_legacy_es())
			require_extension("GL_EXT_shader_texture_lod");
		else if (is_legacy())
			require_extension("GL_ARB_shader_texture_lod");
	}

	if (op == "texture")
		return join("texture", type);
	else if (op == "textureLod")
		return join("texture", type, is_legacy_es() ? "LodEXT" : "Lod");
	else if (op == "textureProj")
		return join("texture", type, "Proj");
	else if (op == "textureProjLod")
		return join("texture", type, is_legacy_es() ? "ProjLodEXT" : "ProjLod");
	else
	{
		SPIRV_CROSS_THROW(join("Unsupported legacy texture op: ", op));
	}
}

bool CompilerGLSL::to_trivial_mix_op(const SPIRType &type, string &op, uint32_t left, uint32_t right, uint32_t lerp)
{
	auto *cleft = maybe_get<SPIRConstant>(left);
	auto *cright = maybe_get<SPIRConstant>(right);
	auto &lerptype = expression_type(lerp);

	// If our targets aren't constants, we cannot use construction.
	if (!cleft || !cright)
		return false;

	// If our targets are spec constants, we cannot use construction.
	if (cleft->specialization || cright->specialization)
		return false;

	// We can only use trivial construction if we have a scalar
	// (should be possible to do it for vectors as well, but that is overkill for now).
	if (lerptype.basetype != SPIRType::Boolean || lerptype.vecsize > 1)
		return false;

	// If our bool selects between 0 and 1, we can cast from bool instead, making our trivial constructor.
	bool ret = false;
	switch (type.basetype)
	{
	case SPIRType::Int:
	case SPIRType::UInt:
		ret = cleft->scalar() == 0 && cright->scalar() == 1;
		break;

	case SPIRType::Float:
		ret = cleft->scalar_f32() == 0.0f && cright->scalar_f32() == 1.0f;
		break;

	case SPIRType::Double:
		ret = cleft->scalar_f64() == 0.0 && cright->scalar_f64() == 1.0;
		break;

	case SPIRType::Int64:
	case SPIRType::UInt64:
		ret = cleft->scalar_u64() == 0 && cright->scalar_u64() == 1;
		break;

	default:
		break;
	}

	if (ret)
		op = type_to_glsl_constructor(type);
	return ret;
}

void CompilerGLSL::emit_mix_op(uint32_t result_type, uint32_t id, uint32_t left, uint32_t right, uint32_t lerp)
{
	auto &lerptype = expression_type(lerp);
	auto &restype = get<SPIRType>(result_type);

	string mix_op;
	bool has_boolean_mix = (options.es && options.version >= 310) || (!options.es && options.version >= 450);
	bool trivial_mix = to_trivial_mix_op(restype, mix_op, left, right, lerp);

	// If we can reduce the mix to a simple cast, do so.
	// This helps for cases like int(bool), uint(bool) which is implemented with
	// OpSelect bool 1 0.
	if (trivial_mix)
	{
		emit_unary_func_op(result_type, id, lerp, mix_op.c_str());
	}
	else if (!has_boolean_mix && lerptype.basetype == SPIRType::Boolean)
	{
		// Boolean mix not supported on desktop without extension.
		// Was added in OpenGL 4.5 with ES 3.1 compat.
		//
		// Could use GL_EXT_shader_integer_mix on desktop at least,
		// but Apple doesn't support it. :(
		// Just implement it as ternary expressions.
		string expr;
		if (lerptype.vecsize == 1)
			expr = join(to_enclosed_expression(lerp), " ? ", to_enclosed_expression(right), " : ",
			            to_enclosed_expression(left));
		else
		{
			auto swiz = [this](uint32_t expression, uint32_t i) {
				return join(to_enclosed_expression(expression), ".", index_to_swizzle(i));
			};

			expr = type_to_glsl_constructor(restype);
			expr += "(";
			for (uint32_t i = 0; i < restype.vecsize; i++)
			{
				expr += swiz(lerp, i);
				expr += " ? ";
				expr += swiz(right, i);
				expr += " : ";
				expr += swiz(left, i);
				if (i + 1 < restype.vecsize)
					expr += ", ";
			}
			expr += ")";
		}

		emit_op(result_type, id, expr, should_forward(left) && should_forward(right) && should_forward(lerp));
	}
	else
		emit_trinary_func_op(result_type, id, left, right, lerp, "mix");
}

string CompilerGLSL::to_combined_image_sampler(uint32_t image_id, uint32_t samp_id)
{
	auto &args = current_function->arguments;

	// For GLSL and ESSL targets, we must enumerate all possible combinations for sampler2D(texture2D, sampler) and redirect
	// all possible combinations into new sampler2D uniforms.
	auto *image = maybe_get_backing_variable(image_id);
	auto *samp = maybe_get_backing_variable(samp_id);
	if (image)
		image_id = image->self;
	if (samp)
		samp_id = samp->self;

	auto image_itr = find_if(begin(args), end(args),
	                         [image_id](const SPIRFunction::Parameter &param) { return param.id == image_id; });

	auto sampler_itr = find_if(begin(args), end(args),
	                           [samp_id](const SPIRFunction::Parameter &param) { return param.id == samp_id; });

	if (image_itr != end(args) || sampler_itr != end(args))
	{
		// If any parameter originates from a parameter, we will find it in our argument list.
		bool global_image = image_itr == end(args);
		bool global_sampler = sampler_itr == end(args);
		uint32_t iid = global_image ? image_id : uint32_t(image_itr - begin(args));
		uint32_t sid = global_sampler ? samp_id : uint32_t(sampler_itr - begin(args));

		auto &combined = current_function->combined_parameters;
		auto itr = find_if(begin(combined), end(combined), [=](const SPIRFunction::CombinedImageSamplerParameter &p) {
			return p.global_image == global_image && p.global_sampler == global_sampler && p.image_id == iid &&
			       p.sampler_id == sid;
		});

		if (itr != end(combined))
			return to_expression(itr->id);
		else
		{
			SPIRV_CROSS_THROW(
			    "Cannot find mapping for combined sampler parameter, was build_combined_image_samplers() used "
			    "before compile() was called?");
		}
	}
	else
	{
		// For global sampler2D, look directly at the global remapping table.
		auto &mapping = combined_image_samplers;
		auto itr = find_if(begin(mapping), end(mapping), [image_id, samp_id](const CombinedImageSampler &combined) {
			return combined.image_id == image_id && combined.sampler_id == samp_id;
		});

		if (itr != end(combined_image_samplers))
			return to_expression(itr->combined_id);
		else
		{
			SPIRV_CROSS_THROW("Cannot find mapping for combined sampler, was build_combined_image_samplers() used "
			                  "before compile() was called?");
		}
	}
}

void CompilerGLSL::emit_sampled_image_op(uint32_t result_type, uint32_t result_id, uint32_t image_id, uint32_t samp_id)
{
	if (options.vulkan_semantics && combined_image_samplers.empty())
	{
		emit_binary_func_op(result_type, result_id, image_id, samp_id,
		                    type_to_glsl(get<SPIRType>(result_type)).c_str());
	}
	else
		emit_op(result_type, result_id, to_combined_image_sampler(image_id, samp_id), true);
}

void CompilerGLSL::emit_texture_op(const Instruction &i)
{
	auto ops = stream(i);
	auto op = static_cast<Op>(i.op);
	uint32_t length = i.length;

	if (i.offset + length > spirv.size())
		SPIRV_CROSS_THROW("Compiler::parse() opcode out of range.");

	uint32_t result_type = ops[0];
	uint32_t id = ops[1];
	uint32_t img = ops[2];
	uint32_t coord = ops[3];
	uint32_t dref = 0;
	uint32_t comp = 0;
	bool gather = false;
	bool proj = false;
	const uint32_t *opt = nullptr;

	switch (op)
	{
	case OpImageSampleDrefImplicitLod:
	case OpImageSampleDrefExplicitLod:
		dref = ops[4];
		opt = &ops[5];
		length -= 5;
		break;

	case OpImageSampleProjDrefImplicitLod:
	case OpImageSampleProjDrefExplicitLod:
		dref = ops[4];
		proj = true;
		opt = &ops[5];
		length -= 5;
		break;

	case OpImageDrefGather:
		dref = ops[4];
		opt = &ops[5];
		gather = true;
		length -= 5;
		break;

	case OpImageGather:
		comp = ops[4];
		opt = &ops[5];
		gather = true;
		length -= 5;
		break;

	case OpImageSampleProjImplicitLod:
	case OpImageSampleProjExplicitLod:
		opt = &ops[4];
		length -= 4;
		proj = true;
		break;

	default:
		opt = &ops[4];
		length -= 4;
		break;
	}

	auto &imgtype = expression_type(img);
	uint32_t coord_components = 0;
	switch (imgtype.image.dim)
	{
	case spv::Dim1D:
		coord_components = 1;
		break;
	case spv::Dim2D:
		coord_components = 2;
		break;
	case spv::Dim3D:
		coord_components = 3;
		break;
	case spv::DimCube:
		coord_components = 3;
		break;
	case spv::DimBuffer:
		coord_components = 1;
		break;
	default:
		coord_components = 2;
		break;
	}

	if (proj)
		coord_components++;
	if (imgtype.image.arrayed)
		coord_components++;

	uint32_t bias = 0;
	uint32_t lod = 0;
	uint32_t grad_x = 0;
	uint32_t grad_y = 0;
	uint32_t coffset = 0;
	uint32_t offset = 0;
	uint32_t coffsets = 0;
	uint32_t sample = 0;
	uint32_t flags = 0;

	if (length)
	{
		flags = opt[0];
		opt++;
		length--;
	}

	auto test = [&](uint32_t &v, uint32_t flag) {
		if (length && (flags & flag))
		{
			v = *opt++;
			length--;
		}
	};

	test(bias, ImageOperandsBiasMask);
	test(lod, ImageOperandsLodMask);
	test(grad_x, ImageOperandsGradMask);
	test(grad_y, ImageOperandsGradMask);
	test(coffset, ImageOperandsConstOffsetMask);
	test(offset, ImageOperandsOffsetMask);
	test(coffsets, ImageOperandsConstOffsetsMask);
	test(sample, ImageOperandsSampleMask);

	string expr;
	string texop;

	if (op == OpImageFetch)
		texop += "texelFetch";
	else
	{
		texop += "texture";

		if (gather)
			texop += "Gather";
		if (coffsets)
			texop += "Offsets";
		if (proj)
			texop += "Proj";
		if (grad_x || grad_y)
			texop += "Grad";
		if (lod)
			texop += "Lod";
	}

	if (coffset || offset)
		texop += "Offset";

	if (is_legacy())
		texop = legacy_tex_op(texop, imgtype);

	expr += texop;
	expr += "(";
	expr += to_expression(img);

	bool swizz_func = backend.swizzle_is_function;
	auto swizzle = [swizz_func](uint32_t comps, uint32_t in_comps) -> const char * {
		if (comps == in_comps)
			return "";

		switch (comps)
		{
		case 1:
			return ".x";
		case 2:
			return swizz_func ? ".xy()" : ".xy";
		case 3:
			return swizz_func ? ".xyz()" : ".xyz";
		default:
			return "";
		}
	};

	bool forward = should_forward(coord);

	// The IR can give us more components than we need, so chop them off as needed.
	auto swizzle_expr = swizzle(coord_components, expression_type(coord).vecsize);
	// Only enclose the UV expression if needed.
	auto coord_expr = (*swizzle_expr == '\0') ? to_expression(coord) : (to_enclosed_expression(coord) + swizzle_expr);

	// TODO: implement rest ... A bit intensive.

	if (dref)
	{
		forward = forward && should_forward(dref);

		// SPIR-V splits dref and coordinate.
		if (coord_components == 4) // GLSL also splits the arguments in two.
		{
			expr += ", ";
			expr += to_expression(coord);
			expr += ", ";
			expr += to_expression(dref);
		}
		else
		{
			// Create a composite which merges coord/dref into a single vector.
			auto type = expression_type(coord);
			type.vecsize = coord_components + 1;
			expr += ", ";
			expr += type_to_glsl_constructor(type);
			expr += "(";
			expr += coord_expr;
			expr += ", ";
			expr += to_expression(dref);
			expr += ")";
		}
	}
	else
	{
		expr += ", ";
		expr += coord_expr;
	}

	if (grad_x || grad_y)
	{
		forward = forward && should_forward(grad_x);
		forward = forward && should_forward(grad_y);
		expr += ", ";
		expr += to_expression(grad_x);
		expr += ", ";
		expr += to_expression(grad_y);
	}

	if (lod)
	{
		forward = forward && should_forward(lod);
		expr += ", ";
		expr += to_expression(lod);
	}

	if (coffset)
	{
		forward = forward && should_forward(coffset);
		expr += ", ";
		expr += to_expression(coffset);
	}
	else if (offset)
	{
		forward = forward && should_forward(offset);
		expr += ", ";
		expr += to_expression(offset);
	}

	if (bias)
	{
		forward = forward && should_forward(bias);
		expr += ", ";
		expr += to_expression(bias);
	}

	if (comp)
	{
		forward = forward && should_forward(comp);
		expr += ", ";
		expr += to_expression(comp);
	}

	if (sample)
	{
		expr += ", ";
		expr += to_expression(sample);
	}

	expr += ")";

	emit_op(result_type, id, expr, forward);
}

void CompilerGLSL::emit_glsl_op(uint32_t result_type, uint32_t id, uint32_t eop, const uint32_t *args, uint32_t)
{
	GLSLstd450 op = static_cast<GLSLstd450>(eop);

	switch (op)
	{
	// FP fiddling
	case GLSLstd450Round:
		emit_unary_func_op(result_type, id, args[0], "round");
		break;

	case GLSLstd450RoundEven:
		if ((options.es && options.version >= 300) || (!options.es && options.version >= 130))
			emit_unary_func_op(result_type, id, args[0], "roundEven");
		else
			SPIRV_CROSS_THROW("roundEven supported only in ESSL 300 and GLSL 130 and up.");
		break;

	case GLSLstd450Trunc:
		emit_unary_func_op(result_type, id, args[0], "trunc");
		break;
	case GLSLstd450SAbs:
	case GLSLstd450FAbs:
		emit_unary_func_op(result_type, id, args[0], "abs");
		break;
	case GLSLstd450SSign:
	case GLSLstd450FSign:
		emit_unary_func_op(result_type, id, args[0], "sign");
		break;
	case GLSLstd450Floor:
		emit_unary_func_op(result_type, id, args[0], "floor");
		break;
	case GLSLstd450Ceil:
		emit_unary_func_op(result_type, id, args[0], "ceil");
		break;
	case GLSLstd450Fract:
		emit_unary_func_op(result_type, id, args[0], "fract");
		break;
	case GLSLstd450Radians:
		emit_unary_func_op(result_type, id, args[0], "radians");
		break;
	case GLSLstd450Degrees:
		emit_unary_func_op(result_type, id, args[0], "degrees");
		break;
	case GLSLstd450Fma:
		emit_trinary_func_op(result_type, id, args[0], args[1], args[2], "fma");
		break;
	case GLSLstd450Modf:
		register_call_out_argument(args[1]);
		forced_temporaries.insert(id);
		emit_binary_func_op(result_type, id, args[0], args[1], "modf");
		break;

	// Minmax
	case GLSLstd450FMin:
	case GLSLstd450UMin:
	case GLSLstd450SMin:
		emit_binary_func_op(result_type, id, args[0], args[1], "min");
		break;
	case GLSLstd450FMax:
	case GLSLstd450UMax:
	case GLSLstd450SMax:
		emit_binary_func_op(result_type, id, args[0], args[1], "max");
		break;
	case GLSLstd450FClamp:
	case GLSLstd450UClamp:
	case GLSLstd450SClamp:
		emit_trinary_func_op(result_type, id, args[0], args[1], args[2], "clamp");
		break;

	// Trig
	case GLSLstd450Sin:
		emit_unary_func_op(result_type, id, args[0], "sin");
		break;
	case GLSLstd450Cos:
		emit_unary_func_op(result_type, id, args[0], "cos");
		break;
	case GLSLstd450Tan:
		emit_unary_func_op(result_type, id, args[0], "tan");
		break;
	case GLSLstd450Asin:
		emit_unary_func_op(result_type, id, args[0], "asin");
		break;
	case GLSLstd450Acos:
		emit_unary_func_op(result_type, id, args[0], "acos");
		break;
	case GLSLstd450Atan:
		emit_unary_func_op(result_type, id, args[0], "atan");
		break;
	case GLSLstd450Sinh:
		emit_unary_func_op(result_type, id, args[0], "sinh");
		break;
	case GLSLstd450Cosh:
		emit_unary_func_op(result_type, id, args[0], "cosh");
		break;
	case GLSLstd450Tanh:
		emit_unary_func_op(result_type, id, args[0], "tanh");
		break;
	case GLSLstd450Asinh:
		emit_unary_func_op(result_type, id, args[0], "asinh");
		break;
	case GLSLstd450Acosh:
		emit_unary_func_op(result_type, id, args[0], "acosh");
		break;
	case GLSLstd450Atanh:
		emit_unary_func_op(result_type, id, args[0], "atanh");
		break;
	case GLSLstd450Atan2:
		emit_binary_func_op(result_type, id, args[0], args[1], "atan");
		break;

	// Exponentials
	case GLSLstd450Pow:
		emit_binary_func_op(result_type, id, args[0], args[1], "pow");
		break;
	case GLSLstd450Exp:
		emit_unary_func_op(result_type, id, args[0], "exp");
		break;
	case GLSLstd450Log:
		emit_unary_func_op(result_type, id, args[0], "log");
		break;
	case GLSLstd450Exp2:
		emit_unary_func_op(result_type, id, args[0], "exp2");
		break;
	case GLSLstd450Log2:
		emit_unary_func_op(result_type, id, args[0], "log2");
		break;
	case GLSLstd450Sqrt:
		emit_unary_func_op(result_type, id, args[0], "sqrt");
		break;
	case GLSLstd450InverseSqrt:
		emit_unary_func_op(result_type, id, args[0], "inversesqrt");
		break;

	// Matrix math
	case GLSLstd450Determinant:
		emit_unary_func_op(result_type, id, args[0], "determinant");
		break;
	case GLSLstd450MatrixInverse:
		emit_unary_func_op(result_type, id, args[0], "inverse");
		break;

	// Lerping
	case GLSLstd450FMix:
	case GLSLstd450IMix:
	{
		emit_mix_op(result_type, id, args[0], args[1], args[2]);
		break;
	}
	case GLSLstd450Step:
		emit_binary_func_op(result_type, id, args[0], args[1], "step");
		break;
	case GLSLstd450SmoothStep:
		emit_trinary_func_op(result_type, id, args[0], args[1], args[2], "smoothstep");
		break;

	// Packing
	case GLSLstd450Frexp:
		register_call_out_argument(args[1]);
		forced_temporaries.insert(id);
		emit_binary_func_op(result_type, id, args[0], args[1], "frexp");
		break;
	case GLSLstd450Ldexp:
		emit_binary_func_op(result_type, id, args[0], args[1], "ldexp");
		break;
	case GLSLstd450PackSnorm4x8:
		emit_unary_func_op(result_type, id, args[0], "packSnorm4x8");
		break;
	case GLSLstd450PackUnorm4x8:
		emit_unary_func_op(result_type, id, args[0], "packUnorm4x8");
		break;
	case GLSLstd450PackSnorm2x16:
		emit_unary_func_op(result_type, id, args[0], "packSnorm2x16");
		break;
	case GLSLstd450PackUnorm2x16:
		emit_unary_func_op(result_type, id, args[0], "packUnorm2x16");
		break;
	case GLSLstd450PackHalf2x16:
		emit_unary_func_op(result_type, id, args[0], "packHalf2x16");
		break;
	case GLSLstd450UnpackSnorm4x8:
		emit_unary_func_op(result_type, id, args[0], "unpackSnorm4x8");
		break;
	case GLSLstd450UnpackUnorm4x8:
		emit_unary_func_op(result_type, id, args[0], "unpackUnorm4x8");
		break;
	case GLSLstd450UnpackSnorm2x16:
		emit_unary_func_op(result_type, id, args[0], "unpackSnorm2x16");
		break;
	case GLSLstd450UnpackUnorm2x16:
		emit_unary_func_op(result_type, id, args[0], "unpackUnorm2x16");
		break;
	case GLSLstd450UnpackHalf2x16:
		emit_unary_func_op(result_type, id, args[0], "unpackHalf2x16");
		break;

	case GLSLstd450PackDouble2x32:
		emit_unary_func_op(result_type, id, args[0], "packDouble2x32");
		break;
	case GLSLstd450UnpackDouble2x32:
		emit_unary_func_op(result_type, id, args[0], "unpackDouble2x32");
		break;

	// Vector math
	case GLSLstd450Length:
		emit_unary_func_op(result_type, id, args[0], "length");
		break;
	case GLSLstd450Distance:
		emit_binary_func_op(result_type, id, args[0], args[1], "distance");
		break;
	case GLSLstd450Cross:
		emit_binary_func_op(result_type, id, args[0], args[1], "cross");
		break;
	case GLSLstd450Normalize:
		emit_unary_func_op(result_type, id, args[0], "normalize");
		break;
	case GLSLstd450FaceForward:
		emit_trinary_func_op(result_type, id, args[0], args[1], args[2], "faceforward");
		break;
	case GLSLstd450Reflect:
		emit_binary_func_op(result_type, id, args[0], args[1], "reflect");
		break;
	case GLSLstd450Refract:
		emit_trinary_func_op(result_type, id, args[0], args[1], args[2], "refract");
		break;

	// Bit-fiddling
	case GLSLstd450FindILsb:
		emit_unary_func_op(result_type, id, args[0], "findLSB");
		break;
	case GLSLstd450FindSMsb:
	case GLSLstd450FindUMsb:
		emit_unary_func_op(result_type, id, args[0], "findMSB");
		break;

	// Multisampled varying
	case GLSLstd450InterpolateAtCentroid:
		emit_unary_func_op(result_type, id, args[0], "interpolateAtCentroid");
		break;
	case GLSLstd450InterpolateAtSample:
		emit_binary_func_op(result_type, id, args[0], args[1], "interpolateAtSample");
		break;
	case GLSLstd450InterpolateAtOffset:
		emit_binary_func_op(result_type, id, args[0], args[1], "interpolateAtOffset");
		break;

	default:
		statement("// unimplemented GLSL op ", eop);
		break;
	}
}

string CompilerGLSL::bitcast_glsl_op(const SPIRType &out_type, const SPIRType &in_type)
{
	if (out_type.basetype == SPIRType::UInt && in_type.basetype == SPIRType::Int)
		return type_to_glsl(out_type);
	else if (out_type.basetype == SPIRType::UInt64 && in_type.basetype == SPIRType::Int64)
		return type_to_glsl(out_type);
	else if (out_type.basetype == SPIRType::UInt && in_type.basetype == SPIRType::Float)
		return "floatBitsToUint";
	else if (out_type.basetype == SPIRType::Int && in_type.basetype == SPIRType::UInt)
		return type_to_glsl(out_type);
	else if (out_type.basetype == SPIRType::Int64 && in_type.basetype == SPIRType::UInt64)
		return type_to_glsl(out_type);
	else if (out_type.basetype == SPIRType::Int && in_type.basetype == SPIRType::Float)
		return "floatBitsToInt";
	else if (out_type.basetype == SPIRType::Float && in_type.basetype == SPIRType::UInt)
		return "uintBitsToFloat";
	else if (out_type.basetype == SPIRType::Float && in_type.basetype == SPIRType::Int)
		return "intBitsToFloat";
	else if (out_type.basetype == SPIRType::Int64 && in_type.basetype == SPIRType::Double)
		return "doubleBitsToInt64";
	else if (out_type.basetype == SPIRType::UInt64 && in_type.basetype == SPIRType::Double)
		return "doubleBitsToUint64";
	else if (out_type.basetype == SPIRType::Double && in_type.basetype == SPIRType::Int64)
		return "int64BitsToDouble";
	else if (out_type.basetype == SPIRType::Double && in_type.basetype == SPIRType::UInt64)
		return "uint64BitsToDouble";
	else
		return "";
}

string CompilerGLSL::bitcast_glsl(const SPIRType &result_type, uint32_t argument)
{
	auto op = bitcast_glsl_op(result_type, expression_type(argument));
	if (op.empty())
		return to_enclosed_expression(argument);
	else
		return join(op, "(", to_expression(argument), ")");
}

string CompilerGLSL::builtin_to_glsl(BuiltIn builtin)
{
	switch (builtin)
	{
	case BuiltInPosition:
		return "gl_Position";
	case BuiltInPointSize:
		return "gl_PointSize";
	case BuiltInVertexId:
		if (options.vulkan_semantics)
			SPIRV_CROSS_THROW(
			    "Cannot implement gl_VertexID in Vulkan GLSL. This shader was created with GL semantics.");
		return "gl_VertexID";
	case BuiltInInstanceId:
		if (options.vulkan_semantics)
			SPIRV_CROSS_THROW(
			    "Cannot implement gl_InstanceID in Vulkan GLSL. This shader was created with GL semantics.");
		return "gl_InstanceID";
	case BuiltInVertexIndex:
		if (options.vulkan_semantics)
			return "gl_VertexIndex";
		else
			return "gl_VertexID"; // gl_VertexID already has the base offset applied.
	case BuiltInInstanceIndex:
		if (options.vulkan_semantics)
			return "gl_InstanceIndex";
		else
			return "(gl_InstanceID + SPIRV_Cross_BaseInstance)"; // ... but not gl_InstanceID.
	case BuiltInPrimitiveId:
		return "gl_PrimitiveID";
	case BuiltInInvocationId:
		return "gl_InvocationID";
	case BuiltInLayer:
		return "gl_Layer";
	case BuiltInTessLevelOuter:
		return "gl_TessLevelOuter";
	case BuiltInTessLevelInner:
		return "gl_TessLevelInner";
	case BuiltInTessCoord:
		return "gl_TessCoord";
	case BuiltInFragCoord:
		return "gl_FragCoord";
	case BuiltInPointCoord:
		return "gl_PointCoord";
	case BuiltInFrontFacing:
		return "gl_FrontFacing";
	case BuiltInFragDepth:
		return "gl_FragDepth";
	case BuiltInNumWorkgroups:
		return "gl_NumWorkGroups";
	case BuiltInWorkgroupSize:
		return "gl_WorkGroupSize";
	case BuiltInWorkgroupId:
		return "gl_WorkGroupID";
	case BuiltInLocalInvocationId:
		return "gl_LocalInvocationID";
	case BuiltInGlobalInvocationId:
		return "gl_GlobalInvocationID";
	case BuiltInLocalInvocationIndex:
		return "gl_LocalInvocationIndex";
	default:
		return "gl_???";
	}
}

const char *CompilerGLSL::index_to_swizzle(uint32_t index)
{
	switch (index)
	{
	case 0:
		return "x";
	case 1:
		return "y";
	case 2:
		return "z";
	case 3:
		return "w";
	default:
		SPIRV_CROSS_THROW("Swizzle index out of range");
	}
}

string CompilerGLSL::access_chain(uint32_t base, const uint32_t *indices, uint32_t count, bool index_is_literal,
                                  bool chain_only)
{
	string expr;
	if (!chain_only)
		expr = to_enclosed_expression(base);

	const auto *type = &expression_type(base);

	// For resolving array accesses, etc, keep a local copy for poking.
	SPIRType temp;

	bool access_chain_is_arrayed = false;
	bool row_major_matrix_needs_conversion = is_non_native_row_major_matrix(base);

	for (uint32_t i = 0; i < count; i++)
	{
		uint32_t index = indices[i];

		// Arrays
		if (!type->array.empty())
		{
			expr += "[";
			if (index_is_literal)
				expr += convert_to_string(index);
			else
				expr += to_expression(index);
			expr += "]";

			// We have to modify the type, so keep a local copy.
			if (&temp != type)
				temp = *type;
			type = &temp;
			temp.array.pop_back();

			access_chain_is_arrayed = true;
		}
		// For structs, the index refers to a constant, which indexes into the members.
		// We also check if this member is a builtin, since we then replace the entire expression with the builtin one.
		else if (type->basetype == SPIRType::Struct)
		{
			if (!index_is_literal)
				index = get<SPIRConstant>(index).scalar();

			if (index >= type->member_types.size())
				SPIRV_CROSS_THROW("Member index is out of bounds!");

			BuiltIn builtin;
			if (is_member_builtin(*type, index, &builtin))
			{
				// FIXME: We rely here on OpName on gl_in/gl_out to make this work properly.
				// To make this properly work by omitting all OpName opcodes,
				// we need to infer gl_in or gl_out based on the builtin, and stage.
				if (access_chain_is_arrayed)
				{
					expr += ".";
					expr += builtin_to_glsl(builtin);
				}
				else
					expr = builtin_to_glsl(builtin);
			}
			else
			{
				expr += ".";
				expr += to_member_name(*type, index);
			}
			row_major_matrix_needs_conversion = member_is_non_native_row_major_matrix(*type, index);
			type = &get<SPIRType>(type->member_types[index]);
		}
		// Matrix -> Vector
		else if (type->columns > 1)
		{
			if (row_major_matrix_needs_conversion)
			{
				expr = convert_row_major_matrix(expr);
				row_major_matrix_needs_conversion = false;
			}

			expr += "[";
			if (index_is_literal)
				expr += convert_to_string(index);
			else
				expr += to_expression(index);
			expr += "]";

			// We have to modify the type, so keep a local copy.
			if (&temp != type)
				temp = *type;
			type = &temp;
			temp.columns = 1;
		}
		// Vector -> Scalar
		else if (type->vecsize > 1)
		{
			if (index_is_literal)
			{
				expr += ".";
				expr += index_to_swizzle(index);
			}
			else if (ids[index].get_type() == TypeConstant)
			{
				auto &c = get<SPIRConstant>(index);
				expr += ".";
				expr += index_to_swizzle(c.scalar());
			}
			else
			{
				expr += "[";
				expr += to_expression(index);
				expr += "]";
			}

			// We have to modify the type, so keep a local copy.
			if (&temp != type)
				temp = *type;
			type = &temp;
			temp.vecsize = 1;
		}
		else
			SPIRV_CROSS_THROW("Cannot subdivide a scalar value!");
	}

	return expr;
}

bool CompilerGLSL::should_forward(uint32_t id)
{
	// Immutable expression can always be forwarded.
	// If not immutable, we can speculate about it by forwarding potentially mutable variables.
	auto *var = maybe_get<SPIRVariable>(id);
	bool forward = var ? var->forwardable : false;
	return (is_immutable(id) || forward) && !options.force_temporary;
}

void CompilerGLSL::track_expression_read(uint32_t id)
{
	// If we try to read a forwarded temporary more than once we will stamp out possibly complex code twice.
	// In this case, it's better to just bind the complex expression to the temporary and read that temporary twice.
	if (expression_is_forwarded(id))
	{
		auto &v = expression_usage_counts[id];
		v++;

		if (v >= 2)
		{
			//if (v == 2)
			//    fprintf(stderr, "ID %u was forced to temporary due to more than 1 expression use!\n", id);

			forced_temporaries.insert(id);
			// Force a recompile after this pass to avoid forwarding this variable.
			force_recompile = true;
		}
	}
}

bool CompilerGLSL::args_will_forward(uint32_t id, const uint32_t *args, uint32_t num_args, bool pure)
{
	if (forced_temporaries.find(id) != end(forced_temporaries))
		return false;

	for (uint32_t i = 0; i < num_args; i++)
		if (!should_forward(args[i]))
			return false;

	// We need to forward globals as well.
	if (!pure)
	{
		for (auto global : global_variables)
			if (!should_forward(global))
				return false;
		for (auto aliased : aliased_variables)
			if (!should_forward(aliased))
				return false;
	}

	return true;
}

void CompilerGLSL::register_impure_function_call()
{
	// Impure functions can modify globals and aliased variables, so invalidate them as well.
	for (auto global : global_variables)
		flush_dependees(get<SPIRVariable>(global));
	for (auto aliased : aliased_variables)
		flush_dependees(get<SPIRVariable>(aliased));
}

void CompilerGLSL::register_call_out_argument(uint32_t id)
{
	register_write(id);

	auto *var = maybe_get<SPIRVariable>(id);
	if (var)
		flush_variable_declaration(var->self);
}

void CompilerGLSL::flush_variable_declaration(uint32_t id)
{
	auto *var = maybe_get<SPIRVariable>(id);
	if (var && var->deferred_declaration)
	{
		statement(variable_decl(*var), ";");
		var->deferred_declaration = false;
	}
}

bool CompilerGLSL::remove_duplicate_swizzle(string &op)
{
	auto pos = op.find_last_of('.');
	if (pos == string::npos || pos == 0)
		return false;

	string final_swiz = op.substr(pos + 1, string::npos);

	if (backend.swizzle_is_function)
	{
		if (final_swiz.size() < 2)
			return false;

		if (final_swiz.substr(final_swiz.size() - 2, string::npos) == "()")
			final_swiz.erase(final_swiz.size() - 2, string::npos);
		else
			return false;
	}

	// Check if final swizzle is of form .x, .xy, .xyz, .xyzw or similar.
	// If so, and previous swizzle is of same length,
	// we can drop the final swizzle altogether.
	for (uint32_t i = 0; i < final_swiz.size(); i++)
	{
		static const char expected[] = { 'x', 'y', 'z', 'w' };
		if (i >= 4 || final_swiz[i] != expected[i])
			return false;
	}

	auto prevpos = op.find_last_of('.', pos - 1);
	if (prevpos == string::npos)
		return false;

	prevpos++;

	// Make sure there are only swizzles here ...
	for (auto i = prevpos; i < pos; i++)
	{
		if (op[i] < 'w' || op[i] > 'z')
		{
			// If swizzles are foo.xyz() like in C++ backend for example, check for that.
			if (backend.swizzle_is_function && i + 2 == pos && op[i] == '(' && op[i + 1] == ')')
				break;
			return false;
		}
	}

	// If original swizzle is large enough, just carve out the components we need.
	// E.g. foobar.wyx.xy will turn into foobar.wy.
	if (pos - prevpos >= final_swiz.size())
	{
		op.erase(prevpos + final_swiz.size(), string::npos);

		// Add back the function call ...
		if (backend.swizzle_is_function)
			op += "()";
	}
	return true;
}

// Optimizes away vector swizzles where we have something like
// vec3 foo;
// foo.xyz <-- swizzle expression does nothing.
// This is a very common pattern after OpCompositeCombine.
bool CompilerGLSL::remove_unity_swizzle(uint32_t base, string &op)
{
	auto pos = op.find_last_of('.');
	if (pos == string::npos || pos == 0)
		return false;

	string final_swiz = op.substr(pos + 1, string::npos);

	if (backend.swizzle_is_function)
	{
		if (final_swiz.size() < 2)
			return false;

		if (final_swiz.substr(final_swiz.size() - 2, string::npos) == "()")
			final_swiz.erase(final_swiz.size() - 2, string::npos);
		else
			return false;
	}

	// Check if final swizzle is of form .x, .xy, .xyz, .xyzw or similar.
	// If so, and previous swizzle is of same length,
	// we can drop the final swizzle altogether.
	for (uint32_t i = 0; i < final_swiz.size(); i++)
	{
		static const char expected[] = { 'x', 'y', 'z', 'w' };
		if (i >= 4 || final_swiz[i] != expected[i])
			return false;
	}

	auto &type = expression_type(base);

	// Sanity checking ...
	assert(type.columns == 1 && type.array.empty());

	if (type.vecsize == final_swiz.size())
		op.erase(pos, string::npos);
	return true;
}

string CompilerGLSL::build_composite_combiner(const uint32_t *elems, uint32_t length)
{
	uint32_t base = 0;
	bool swizzle_optimization = false;
	string op;
	string subop;

	for (uint32_t i = 0; i < length; i++)
	{
		auto *e = maybe_get<SPIRExpression>(elems[i]);

		// If we're merging another scalar which belongs to the same base
		// object, just merge the swizzles to avoid triggering more than 1 expression read as much as possible!
		if (e && e->base_expression && e->base_expression == base)
		{
			// Only supposed to be used for vector swizzle -> scalar.
			assert(!e->expression.empty() && e->expression.front() == '.');
			subop += e->expression.substr(1, string::npos);
			swizzle_optimization = true;
		}
		else
		{
			// We'll likely end up with duplicated swizzles, e.g.
			// foobar.xyz.xyz from patterns like
			// OpVectorSwizzle
			// OpCompositeExtract x 3
			// OpCompositeConstruct 3x + other scalar.
			// Just modify op in-place.
			if (swizzle_optimization)
			{
				if (backend.swizzle_is_function)
					subop += "()";

				// Don't attempt to remove unity swizzling if we managed to remove duplicate swizzles.
				// The base "foo" might be vec4, while foo.xyz is vec3 (OpVectorShuffle) and looks like a vec3 due to the .xyz tacked on.
				// We only want to remove the swizzles if we're certain that the resulting base will be the same vecsize.
				// Essentially, we can only remove one set of swizzles, since that's what we have control over ...
				// Case 1:
				//  foo.yxz.xyz: Duplicate swizzle kicks in, giving foo.yxz, we are done.
				//               foo.yxz was the result of OpVectorShuffle and we don't know the type of foo.
				// Case 2:
				//  foo.xyz: Duplicate swizzle won't kick in.
				//           If foo is vec3, we can remove xyz, giving just foo.
				if (!remove_duplicate_swizzle(subop))
					remove_unity_swizzle(base, subop);

				// Strips away redundant parens if we created them during component extraction.
				strip_enclosed_expression(subop);
				swizzle_optimization = false;
				op += subop;
			}
			else
				op += subop;

			if (i)
				op += ", ";
			subop = to_expression(elems[i]);
		}

		base = e ? e->base_expression : 0;
	}

	if (swizzle_optimization)
	{
		if (backend.swizzle_is_function)
			subop += "()";

		if (!remove_duplicate_swizzle(subop))
			remove_unity_swizzle(base, subop);
		// Strips away redundant parens if we created them during component extraction.
		strip_enclosed_expression(subop);
	}

	op += subop;
	return op;
}

bool CompilerGLSL::skip_argument(uint32_t id) const
{
	if (!combined_image_samplers.empty() || !options.vulkan_semantics)
	{
		auto &type = expression_type(id);
		if (type.basetype == SPIRType::Sampler || (type.basetype == SPIRType::Image && type.image.sampled == 1))
			return true;
	}
	return false;
}

bool CompilerGLSL::optimize_read_modify_write(const string &lhs, const string &rhs)
{
	// Do this with strings because we have a very clear pattern we can check for and it avoids
	// adding lots of special cases to the code emission.
	if (rhs.size() < lhs.size() + 3)
		return false;

	auto index = rhs.find(lhs);
	if (index != 0)
		return false;

	// TODO: Shift operators, but it's not important for now.
	auto op = rhs.find_first_of("+-/*%|&^", lhs.size() + 1);
	if (op != lhs.size() + 1)
		return false;

	char bop = rhs[op];
	auto expr = rhs.substr(lhs.size() + 3);
	// Try to find increments and decrements. Makes it look neater as += 1, -= 1 is fairly rare to see in real code.
	// Find some common patterns which are equivalent.
	if ((bop == '+' || bop == '-') && (expr == "1" || expr == "uint(1)" || expr == "1u" || expr == "int(1u)"))
		statement(lhs, bop, bop, ";");
	else
		statement(lhs, " ", bop, "= ", expr, ";");
	return true;
}

void CompilerGLSL::emit_instruction(const Instruction &instruction)
{
	auto ops = stream(instruction);
	auto opcode = static_cast<Op>(instruction.op);
	uint32_t length = instruction.length;

#define BOP(op) emit_binary_op(ops[0], ops[1], ops[2], ops[3], #op)
#define BOP_CAST(op, type) \
	emit_binary_op_cast(ops[0], ops[1], ops[2], ops[3], #op, type, opcode_is_sign_invariant(opcode))
#define UOP(op) emit_unary_op(ops[0], ops[1], ops[2], #op)
#define QFOP(op) emit_quaternary_func_op(ops[0], ops[1], ops[2], ops[3], ops[4], ops[5], #op)
#define TFOP(op) emit_trinary_func_op(ops[0], ops[1], ops[2], ops[3], ops[4], #op)
#define BFOP(op) emit_binary_func_op(ops[0], ops[1], ops[2], ops[3], #op)
#define BFOP_CAST(op, type) \
	emit_binary_func_op_cast(ops[0], ops[1], ops[2], ops[3], #op, type, opcode_is_sign_invariant(opcode))
#define BFOP(op) emit_binary_func_op(ops[0], ops[1], ops[2], ops[3], #op)
#define UFOP(op) emit_unary_func_op(ops[0], ops[1], ops[2], #op)

	switch (opcode)
	{
	// Dealing with memory
	case OpLoad:
	{
		uint32_t result_type = ops[0];
		uint32_t id = ops[1];
		uint32_t ptr = ops[2];

		flush_variable_declaration(ptr);

		// If we're loading from memory that cannot be changed by the shader,
		// just forward the expression directly to avoid needless temporaries.
		// If an expression is mutable and forwardable, we speculate that it is immutable.
		bool forward = should_forward(ptr) && forced_temporaries.find(id) == end(forced_temporaries);

		// If loading a non-native row-major matrix, convert it to column-major
		auto expr = to_expression(ptr);
		if (is_non_native_row_major_matrix(ptr))
			expr = convert_row_major_matrix(expr);

		// Suppress usage tracking since using same expression multiple times does not imply any extra work.
		emit_op(result_type, id, expr, forward, true);
		register_read(id, ptr, forward);
		break;
	}

	case OpInBoundsAccessChain:
	case OpAccessChain:
	{
		auto *var = maybe_get<SPIRVariable>(ops[2]);
		if (var)
			flush_variable_declaration(var->self);

		// If the base is immutable, the access chain pointer must also be.
		// If an expression is mutable and forwardable, we speculate that it is immutable.
		auto e = access_chain(ops[2], &ops[3], length - 3, false);
		auto &expr = set<SPIRExpression>(ops[1], move(e), ops[0], should_forward(ops[2]));
		expr.loaded_from = ops[2];
		break;
	}

	case OpStore:
	{
		auto *var = maybe_get<SPIRVariable>(ops[0]);

		if (var && var->statically_assigned)
			var->static_expression = ops[1];
		else if (var && var->loop_variable && !var->loop_variable_enable)
			var->static_expression = ops[1];
		else
		{
			auto lhs = to_expression(ops[0]);
			auto rhs = to_expression(ops[1]);

			// It is possible with OpLoad/OpCompositeInsert/OpStore that we get <expr> = <same-expr>.
			// For this case, we don't need to invalidate anything and emit any opcode.
			if (lhs != rhs)
			{
				// Tries to optimize assignments like "<lhs> = <lhs> op expr".
				// While this is purely cosmetic, this is important for legacy ESSL where loop
				// variable increments must be in either i++ or i += const-expr.
				// Without this, we end up with i = i + 1, which is correct GLSL, but not correct GLES 2.0.
				if (!optimize_read_modify_write(lhs, rhs))
					statement(lhs, " = ", rhs, ";");
				register_write(ops[0]);
			}
		}
		break;
	}

	case OpArrayLength:
	{
		uint32_t result_type = ops[0];
		uint32_t id = ops[1];
		auto e = access_chain(ops[2], &ops[3], length - 3, true);
		set<SPIRExpression>(id, e + ".length()", result_type, true);
		break;
	}

	// Function calls
	case OpFunctionCall:
	{
		uint32_t result_type = ops[0];
		uint32_t id = ops[1];
		uint32_t func = ops[2];
		const auto *arg = &ops[3];
		length -= 3;

		auto &callee = get<SPIRFunction>(func);
		bool pure = function_is_pure(callee);

		bool callee_has_out_variables = false;

		// Invalidate out variables passed to functions since they can be OpStore'd to.
		for (uint32_t i = 0; i < length; i++)
		{
			if (callee.arguments[i].write_count)
			{
				register_call_out_argument(arg[i]);
				callee_has_out_variables = true;
			}

			flush_variable_declaration(arg[i]);
		}

		if (!pure)
			register_impure_function_call();

		string funexpr;
		vector<string> arglist;
		funexpr += to_name(func) + "(";
		for (uint32_t i = 0; i < length; i++)
		{
			// Do not pass in separate images or samplers if we're remapping
			// to combined image samplers.
			if (skip_argument(arg[i]))
				continue;

			arglist.push_back(to_func_call_arg(arg[i]));
		}

		for (auto &combined : callee.combined_parameters)
		{
			uint32_t image_id = combined.global_image ? combined.image_id : arg[combined.image_id];
			uint32_t sampler_id = combined.global_sampler ? combined.sampler_id : arg[combined.sampler_id];

			auto *image = maybe_get_backing_variable(image_id);
			if (image)
				image_id = image->self;

			auto *samp = maybe_get_backing_variable(sampler_id);
			if (samp)
				sampler_id = samp->self;

			arglist.push_back(to_combined_image_sampler(image_id, sampler_id));
		}

		append_global_func_args(callee, length, arglist);

		funexpr += merge(arglist);
		funexpr += ")";

		// Check for function call constraints.
		check_function_call_constraints(arg, length);

		if (get<SPIRType>(result_type).basetype != SPIRType::Void)
		{
			// If the function actually writes to an out variable,
			// take the conservative route and do not forward.
			// The problem is that we might not read the function
			// result (and emit the function) before an out variable
			// is read (common case when return value is ignored!
			// In order to avoid start tracking invalid variables,
			// just avoid the forwarding problem altogether.
			bool forward = args_will_forward(id, arg, length, pure) && !callee_has_out_variables && pure &&
			               (forced_temporaries.find(id) == end(forced_temporaries));

			emit_op(result_type, id, funexpr, forward);

			// Function calls are implicit loads from all variables in question.
			// Set dependencies for them.
			for (uint32_t i = 0; i < length; i++)
				register_read(id, arg[i], forward);

			// If we're going to forward the temporary result,
			// put dependencies on every variable that must not change.
			if (forward)
				register_global_read_dependencies(callee, id);
		}
		else
			statement(funexpr, ";");

		break;
	}

	// Composite munging
	case OpCompositeConstruct:
	{
		uint32_t result_type = ops[0];
		uint32_t id = ops[1];
		const auto *elems = &ops[2];
		length -= 2;

		if (!length)
			SPIRV_CROSS_THROW("Invalid input to OpCompositeConstruct.");

		bool forward = true;
		for (uint32_t i = 0; i < length; i++)
			forward = forward && should_forward(elems[i]);

		auto &in_type = expression_type(elems[0]);
		auto &out_type = get<SPIRType>(result_type);

		// Only splat if we have vector constructors.
		// Arrays and structs must be initialized properly in full.
		bool composite = !out_type.array.empty() || out_type.basetype == SPIRType::Struct;
		bool splat = in_type.vecsize == 1 && in_type.columns == 1 && !composite;

		if (splat)
		{
			uint32_t input = elems[0];
			for (uint32_t i = 0; i < length; i++)
				if (input != elems[i])
					splat = false;
		}

		string constructor_op;
		if (backend.use_initializer_list && composite)
		{
			// Only use this path if we are building composites.
			// This path cannot be used for arithmetic.
			constructor_op += "{ ";
			if (splat)
				constructor_op += to_expression(elems[0]);
			else
				constructor_op += build_composite_combiner(elems, length);
			constructor_op += " }";
		}
		else
		{
			constructor_op = type_to_glsl_constructor(get<SPIRType>(result_type)) + "(";
			if (splat)
				constructor_op += to_expression(elems[0]);
			else
				constructor_op += build_composite_combiner(elems, length);
			constructor_op += ")";
		}

		emit_op(result_type, id, constructor_op, forward);
		break;
	}

	case OpVectorInsertDynamic:
	{
		uint32_t result_type = ops[0];
		uint32_t id = ops[1];
		uint32_t vec = ops[2];
		uint32_t comp = ops[3];
		uint32_t index = ops[4];

		flush_variable_declaration(vec);

		// Make a copy, then use access chain to store the variable.
		statement(declare_temporary(result_type, id), to_expression(vec), ";");
		set<SPIRExpression>(id, to_name(id), result_type, true);
		auto chain = access_chain(id, &index, 1, false);
		statement(chain, " = ", to_expression(comp), ";");
		break;
	}

	case OpVectorExtractDynamic:
	{
		uint32_t result_type = ops[0];
		uint32_t id = ops[1];

		auto expr = access_chain(ops[2], &ops[3], 1, false);
		emit_op(result_type, id, expr, should_forward(ops[2]));
		break;
	}

	case OpCompositeExtract:
	{
		uint32_t result_type = ops[0];
		uint32_t id = ops[1];
		length -= 3;

		auto &type = get<SPIRType>(result_type);

		// We can only split the expression here if our expression is forwarded as a temporary.
		bool allow_base_expression = forced_temporaries.find(id) == end(forced_temporaries);

		// Only apply this optimization if result is scalar.
		if (allow_base_expression && should_forward(ops[2]) && type.vecsize == 1 && type.columns == 1 && length == 1)
		{
			// We want to split the access chain from the base.
			// This is so we can later combine different CompositeExtract results
			// with CompositeConstruct without emitting code like
			//
			// vec3 temp = texture(...).xyz
			// vec4(temp.x, temp.y, temp.z, 1.0).
			//
			// when we actually wanted to emit this
			// vec4(texture(...).xyz, 1.0).
			//
			// Including the base will prevent this and would trigger multiple reads
			// from expression causing it to be forced to an actual temporary in GLSL.
			auto expr = access_chain(ops[2], &ops[3], length, true, true);
			auto &e = emit_op(result_type, id, expr, true, !expression_is_forwarded(ops[2]));
			e.base_expression = ops[2];
		}
		else
		{
			auto expr = access_chain(ops[2], &ops[3], length, true);
			emit_op(result_type, id, expr, should_forward(ops[2]), !expression_is_forwarded(ops[2]));
		}
		break;
	}

	case OpCompositeInsert:
	{
		uint32_t result_type = ops[0];
		uint32_t id = ops[1];
		uint32_t obj = ops[2];
		uint32_t composite = ops[3];
		const auto *elems = &ops[4];
		length -= 4;

		flush_variable_declaration(composite);

		auto *expr = maybe_get<SPIRExpression>(id);
		if ((expr && expr->used_while_invalidated) || !should_forward(composite))
		{
			// Make a copy, then use access chain to store the variable.
			statement(declare_temporary(result_type, id), to_expression(composite), ";");
			set<SPIRExpression>(id, to_name(id), result_type, true);
			auto chain = access_chain(id, elems, length, true);
			statement(chain, " = ", to_expression(obj), ";");
		}
		else
		{
			auto chain = access_chain(composite, elems, length, true);
			statement(chain, " = ", to_expression(obj), ";");
			set<SPIRExpression>(id, to_expression(composite), result_type, true);

			register_write(composite);
			register_read(id, composite, true);
			// Invalidate the old expression we inserted into.
			invalid_expressions.insert(composite);
		}
		break;
	}

	case OpCopyMemory:
	{
		uint32_t lhs = ops[0];
		uint32_t rhs = ops[1];
		if (lhs != rhs)
		{
			flush_variable_declaration(lhs);
			flush_variable_declaration(rhs);
			statement(to_expression(lhs), " = ", to_expression(rhs), ";");
			register_write(lhs);
		}
		break;
	}

	case OpCopyObject:
	{
		uint32_t result_type = ops[0];
		uint32_t id = ops[1];
		uint32_t rhs = ops[2];
		bool pointer = get<SPIRType>(result_type).pointer;

		if (expression_is_lvalue(rhs) && !pointer)
		{
			// Need a copy.
			// For pointer types, we copy the pointer itself.
			statement(declare_temporary(result_type, id), to_expression(rhs), ";");
			set<SPIRExpression>(id, to_name(id), result_type, true);
		}
		else
		{
			// RHS expression is immutable, so just forward it.
			// Copying these things really make no sense, but
			// seems to be allowed anyways.
			auto &e = set<SPIRExpression>(id, to_expression(rhs), result_type, true);
			if (pointer)
			{
				auto *var = maybe_get_backing_variable(rhs);
				e.loaded_from = var ? var->self : 0;
			}
		}
		break;
	}

	case OpVectorShuffle:
	{
		uint32_t result_type = ops[0];
		uint32_t id = ops[1];
		uint32_t vec0 = ops[2];
		uint32_t vec1 = ops[3];
		const auto *elems = &ops[4];
		length -= 4;

		auto &type0 = expression_type(vec0);

		bool shuffle = false;
		for (uint32_t i = 0; i < length; i++)
			if (elems[i] >= type0.vecsize)
				shuffle = true;

		string expr;
		bool trivial_forward;

		if (shuffle)
		{
			trivial_forward = !expression_is_forwarded(vec0) && !expression_is_forwarded(vec1);

			// Constructor style and shuffling from two different vectors.
			vector<string> args;
			for (uint32_t i = 0; i < length; i++)
			{
				if (elems[i] >= type0.vecsize)
					args.push_back(join(to_enclosed_expression(vec1), ".", index_to_swizzle(elems[i] - type0.vecsize)));
				else
					args.push_back(join(to_enclosed_expression(vec0), ".", index_to_swizzle(elems[i])));
			}
			expr += join(type_to_glsl_constructor(get<SPIRType>(result_type)), "(", merge(args), ")");
		}
		else
		{
			trivial_forward = !expression_is_forwarded(vec0);

			// We only source from first vector, so can use swizzle.
			expr += to_enclosed_expression(vec0);
			expr += ".";
			for (uint32_t i = 0; i < length; i++)
				expr += index_to_swizzle(elems[i]);
			if (backend.swizzle_is_function && length > 1)
				expr += "()";
		}

		// A shuffle is trivial in that it doesn't actually *do* anything.
		// We inherit the forwardedness from our arguments to avoid flushing out to temporaries when it's not really needed.

		emit_op(result_type, id, expr, should_forward(vec0) && should_forward(vec1), trivial_forward);
		break;
	}

	// ALU
	case OpIsNan:
		UFOP(isnan);
		break;

	case OpIsInf:
		UFOP(isinf);
		break;

	case OpSNegate:
	case OpFNegate:
		UOP(-);
		break;

	case OpIAdd:
	{
		// For simple arith ops, prefer the output type if there's a mismatch to avoid extra bitcasts.
		auto type = get<SPIRType>(ops[0]).basetype;
		BOP_CAST(+, type);
		break;
	}

	case OpFAdd:
		BOP(+);
		break;

	case OpISub:
	{
		auto type = get<SPIRType>(ops[0]).basetype;
		BOP_CAST(-, type);
		break;
	}

	case OpFSub:
		BOP(-);
		break;

	case OpIMul:
	{
		auto type = get<SPIRType>(ops[0]).basetype;
		BOP_CAST(*, type);
		break;
	}

	case OpFMul:
	case OpMatrixTimesVector:
	case OpMatrixTimesScalar:
	case OpVectorTimesScalar:
	case OpVectorTimesMatrix:
	case OpMatrixTimesMatrix:
		BOP(*);
		break;

	case OpOuterProduct:
		BFOP(outerProduct);
		break;

	case OpDot:
		BFOP(dot);
		break;

	case OpTranspose:
		UFOP(transpose);
		break;

	case OpSDiv:
		BOP_CAST(/, SPIRType::Int);
		break;

	case OpUDiv:
		BOP_CAST(/, SPIRType::UInt);
		break;

	case OpFDiv:
		BOP(/);
		break;

	case OpShiftRightLogical:
		BOP_CAST(>>, SPIRType::UInt);
		break;

	case OpShiftRightArithmetic:
		BOP_CAST(>>, SPIRType::Int);
		break;

	case OpShiftLeftLogical:
	{
		auto type = get<SPIRType>(ops[0]).basetype;
		BOP_CAST(<<, type);
		break;
	}

	case OpBitwiseOr:
	{
		auto type = get<SPIRType>(ops[0]).basetype;
		BOP_CAST(|, type);
		break;
	}

	case OpBitwiseXor:
	{
		auto type = get<SPIRType>(ops[0]).basetype;
		BOP_CAST (^, type);
		break;
	}

	case OpBitwiseAnd:
	{
		auto type = get<SPIRType>(ops[0]).basetype;
		BOP_CAST(&, type);
		break;
	}

	case OpNot:
		UOP(~);
		break;

	case OpUMod:
		BOP_CAST(%, SPIRType::UInt);
		break;

	case OpSMod:
		BOP_CAST(%, SPIRType::Int);
		break;

	case OpFMod:
		BFOP(mod);
		break;

	// Relational
	case OpAny:
		UFOP(any);
		break;

	case OpAll:
		UFOP(all);
		break;

	case OpSelect:
		emit_mix_op(ops[0], ops[1], ops[4], ops[3], ops[2]);
		break;

	case OpLogicalOr:
		BOP(||);
		break;

	case OpLogicalAnd:
		BOP(&&);
		break;

	case OpLogicalNot:
		UOP(!);
		break;

	case OpIEqual:
	{
		if (expression_type(ops[2]).vecsize > 1)
			BFOP_CAST(equal, SPIRType::Int);
		else
			BOP_CAST(==, SPIRType::Int);
		break;
	}

	case OpLogicalEqual:
	case OpFOrdEqual:
	{
		if (expression_type(ops[2]).vecsize > 1)
			BFOP(equal);
		else
			BOP(==);
		break;
	}

	case OpINotEqual:
	{
		if (expression_type(ops[2]).vecsize > 1)
			BFOP_CAST(notEqual, SPIRType::Int);
		else
			BOP_CAST(!=, SPIRType::Int);
		break;
	}

	case OpLogicalNotEqual:
	case OpFOrdNotEqual:
	{
		if (expression_type(ops[2]).vecsize > 1)
			BFOP(notEqual);
		else
			BOP(!=);
		break;
	}

	case OpUGreaterThan:
	case OpSGreaterThan:
	{
		auto type = opcode == OpUGreaterThan ? SPIRType::UInt : SPIRType::Int;
		if (expression_type(ops[2]).vecsize > 1)
			BFOP_CAST(greaterThan, type);
		else
			BOP_CAST(>, type);
		break;
	}

	case OpFOrdGreaterThan:
	{
		if (expression_type(ops[2]).vecsize > 1)
			BFOP(greaterThan);
		else
			BOP(>);
		break;
	}

	case OpUGreaterThanEqual:
	case OpSGreaterThanEqual:
	{
		auto type = opcode == OpUGreaterThanEqual ? SPIRType::UInt : SPIRType::Int;
		if (expression_type(ops[2]).vecsize > 1)
			BFOP_CAST(greaterThanEqual, type);
		else
			BOP_CAST(>=, type);
		break;
	}

	case OpFOrdGreaterThanEqual:
	{
		if (expression_type(ops[2]).vecsize > 1)
			BFOP(greaterThanEqual);
		else
			BOP(>=);
		break;
	}

	case OpULessThan:
	case OpSLessThan:
	{
		auto type = opcode == OpULessThan ? SPIRType::UInt : SPIRType::Int;
		if (expression_type(ops[2]).vecsize > 1)
			BFOP_CAST(lessThan, type);
		else
			BOP_CAST(<, type);
		break;
	}

	case OpFOrdLessThan:
	{
		if (expression_type(ops[2]).vecsize > 1)
			BFOP(lessThan);
		else
			BOP(<);
		break;
	}

	case OpULessThanEqual:
	case OpSLessThanEqual:
	{
		auto type = opcode == OpULessThanEqual ? SPIRType::UInt : SPIRType::Int;
		if (expression_type(ops[2]).vecsize > 1)
			BFOP_CAST(lessThanEqual, type);
		else
			BOP_CAST(<=, type);
		break;
	}

	case OpFOrdLessThanEqual:
	{
		if (expression_type(ops[2]).vecsize > 1)
			BFOP(lessThanEqual);
		else
			BOP(<=);
		break;
	}

	// Conversion
	case OpConvertFToU:
	case OpConvertFToS:
	case OpConvertSToF:
	case OpConvertUToF:
	case OpUConvert:
	case OpSConvert:
	case OpFConvert:
	{
		uint32_t result_type = ops[0];
		uint32_t id = ops[1];

		auto func = type_to_glsl_constructor(get<SPIRType>(result_type));
		emit_unary_func_op(result_type, id, ops[2], func.c_str());
		break;
	}

	case OpBitcast:
	{
		uint32_t result_type = ops[0];
		uint32_t id = ops[1];
		uint32_t arg = ops[2];

		auto op = bitcast_glsl_op(get<SPIRType>(result_type), expression_type(arg));
		emit_unary_func_op(result_type, id, arg, op.c_str());
		break;
	}

	case OpQuantizeToF16:
	{
		uint32_t result_type = ops[0];
		uint32_t id = ops[1];
		uint32_t arg = ops[2];

		string op;
		auto &type = get<SPIRType>(result_type);

		switch (type.vecsize)
		{
		case 1:
			op = join("unpackHalf2x16(packHalf2x16(vec2(", to_expression(arg), "))).x");
			break;
		case 2:
			op = join("unpackHalf2x16(packHalf2x16(", to_expression(arg), "))");
			break;
		case 3:
		{
			auto op0 = join("unpackHalf2x16(packHalf2x16(", to_expression(arg), ".xy))");
			auto op1 = join("unpackHalf2x16(packHalf2x16(", to_expression(arg), ".zz)).x");
			op = join("vec3(", op0, ", ", op1, ")");
			break;
		}
		case 4:
		{
			auto op0 = join("unpackHalf2x16(packHalf2x16(", to_expression(arg), ".xy))");
			auto op1 = join("unpackHalf2x16(packHalf2x16(", to_expression(arg), ".zw))");
			op = join("vec4(", op0, ", ", op1, ")");
			break;
		}
		default:
			SPIRV_CROSS_THROW("Illegal argument to OpQuantizeToF16.");
		}

		emit_op(result_type, id, op, should_forward(arg));
		break;
	}

	// Derivatives
	case OpDPdx:
		UFOP(dFdx);
		if (is_legacy_es())
			require_extension("GL_OES_standard_derivatives");
		break;

	case OpDPdy:
		UFOP(dFdy);
		if (is_legacy_es())
			require_extension("GL_OES_standard_derivatives");
		break;

	case OpFwidth:
		UFOP(fwidth);
		if (is_legacy_es())
			require_extension("GL_OES_standard_derivatives");
		break;

	// Bitfield
	case OpBitFieldInsert:
		QFOP(bitfieldInsert);
		break;

	case OpBitFieldSExtract:
	case OpBitFieldUExtract:
		QFOP(bitfieldExtract);
		break;

	case OpBitReverse:
		UFOP(bitfieldReverse);
		break;

	case OpBitCount:
		UFOP(bitCount);
		break;

	// Atomics
	case OpAtomicExchange:
	{
		uint32_t result_type = ops[0];
		uint32_t id = ops[1];
		uint32_t ptr = ops[2];
		// Ignore semantics for now, probably only relevant to CL.
		uint32_t val = ops[5];
		const char *op = check_atomic_image(ptr) ? "imageAtomicExchange" : "atomicExchange";
		forced_temporaries.insert(id);
		emit_binary_func_op(result_type, id, ptr, val, op);
		flush_all_atomic_capable_variables();
		break;
	}

	case OpAtomicCompareExchange:
	{
		uint32_t result_type = ops[0];
		uint32_t id = ops[1];
		uint32_t ptr = ops[2];
		uint32_t val = ops[6];
		uint32_t comp = ops[7];
		const char *op = check_atomic_image(ptr) ? "imageAtomicCompSwap" : "atomicCompSwap";

		forced_temporaries.insert(id);
		emit_trinary_func_op(result_type, id, ptr, comp, val, op);
		flush_all_atomic_capable_variables();
		break;
	}

	case OpAtomicLoad:
		flush_all_atomic_capable_variables();
		// FIXME: Image?
		UFOP(atomicCounter);
		register_read(ops[1], ops[2], should_forward(ops[2]));
		break;

	// OpAtomicStore unimplemented. Not sure what would use that.
	// OpAtomicLoad seems to only be relevant for atomic counters.

	case OpAtomicIIncrement:
		forced_temporaries.insert(ops[1]);
		// FIXME: Image?
		UFOP(atomicCounterIncrement);
		flush_all_atomic_capable_variables();
		register_read(ops[1], ops[2], should_forward(ops[2]));
		break;

	case OpAtomicIDecrement:
		forced_temporaries.insert(ops[1]);
		// FIXME: Image?
		UFOP(atomicCounterDecrement);
		flush_all_atomic_capable_variables();
		register_read(ops[1], ops[2], should_forward(ops[2]));
		break;

	case OpAtomicIAdd:
	{
		const char *op = check_atomic_image(ops[2]) ? "imageAtomicAdd" : "atomicAdd";
		forced_temporaries.insert(ops[1]);
		emit_binary_func_op(ops[0], ops[1], ops[2], ops[5], op);
		flush_all_atomic_capable_variables();
		register_read(ops[1], ops[2], should_forward(ops[2]));
		break;
	}

	case OpAtomicISub:
	{
		const char *op = check_atomic_image(ops[2]) ? "imageAtomicAdd" : "atomicAdd";
		forced_temporaries.insert(ops[1]);
		auto expr = join(op, "(", to_expression(ops[2]), ", -", to_enclosed_expression(ops[5]), ")");
		emit_op(ops[0], ops[1], expr, should_forward(ops[2]) && should_forward(ops[5]));
		flush_all_atomic_capable_variables();
		register_read(ops[1], ops[2], should_forward(ops[2]));
		break;
	}

	case OpAtomicSMin:
	case OpAtomicUMin:
	{
		const char *op = check_atomic_image(ops[2]) ? "imageAtomicMin" : "atomicMin";
		forced_temporaries.insert(ops[1]);
		emit_binary_func_op(ops[0], ops[1], ops[2], ops[5], op);
		flush_all_atomic_capable_variables();
		register_read(ops[1], ops[2], should_forward(ops[2]));
		break;
	}

	case OpAtomicSMax:
	case OpAtomicUMax:
	{
		const char *op = check_atomic_image(ops[2]) ? "imageAtomicMax" : "atomicMax";
		forced_temporaries.insert(ops[1]);
		emit_binary_func_op(ops[0], ops[1], ops[2], ops[5], op);
		flush_all_atomic_capable_variables();
		register_read(ops[1], ops[2], should_forward(ops[2]));
		break;
	}

	case OpAtomicAnd:
	{
		const char *op = check_atomic_image(ops[2]) ? "imageAtomicAnd" : "atomicAnd";
		forced_temporaries.insert(ops[1]);
		emit_binary_func_op(ops[0], ops[1], ops[2], ops[5], op);
		flush_all_atomic_capable_variables();
		register_read(ops[1], ops[2], should_forward(ops[2]));
		break;
	}

	case OpAtomicOr:
	{
		const char *op = check_atomic_image(ops[2]) ? "imageAtomicOr" : "atomicOr";
		forced_temporaries.insert(ops[1]);
		emit_binary_func_op(ops[0], ops[1], ops[2], ops[5], op);
		flush_all_atomic_capable_variables();
		register_read(ops[1], ops[2], should_forward(ops[2]));
		break;
	}

	case OpAtomicXor:
	{
		const char *op = check_atomic_image(ops[2]) ? "imageAtomicXor" : "atomicXor";
		forced_temporaries.insert(ops[1]);
		emit_binary_func_op(ops[0], ops[1], ops[2], ops[5], op);
		flush_all_atomic_capable_variables();
		register_read(ops[1], ops[2], should_forward(ops[2]));
		break;
	}

	// Geometry shaders
	case OpEmitVertex:
		statement("EmitVertex();");
		break;

	case OpEndPrimitive:
		statement("EndPrimitive();");
		break;

	case OpEmitStreamVertex:
		statement("EmitStreamVertex();");
		break;

	case OpEndStreamPrimitive:
		statement("EndStreamPrimitive();");
		break;

	// Textures
	case OpImageSampleExplicitLod:
	case OpImageSampleProjExplicitLod:
	case OpImageSampleDrefExplicitLod:
	case OpImageSampleProjDrefExplicitLod:
	case OpImageSampleImplicitLod:
	case OpImageSampleProjImplicitLod:
	case OpImageSampleDrefImplicitLod:
	case OpImageSampleProjDrefImplicitLod:
	case OpImageFetch:
	case OpImageGather:
	case OpImageDrefGather:
		// Gets a bit hairy, so move this to a separate instruction.
		emit_texture_op(instruction);
		break;

	case OpImage:
	{
		uint32_t result_type = ops[0];
		uint32_t id = ops[1];
		auto &e = emit_op(result_type, id, to_expression(ops[2]), true);

		// When using the image, we need to know which variable it is actually loaded from.
		auto *var = maybe_get_backing_variable(ops[2]);
		e.loaded_from = var ? var->self : 0;
		break;
	}

	case OpImageQueryLod:
	{
		if (!options.es && options.version < 400)
		{
			require_extension("GL_ARB_texture_query_lod");
			// For some reason, the ARB spec is all-caps.
			BFOP(textureQueryLOD);
		}
		else if (options.es)
			SPIRV_CROSS_THROW("textureQueryLod not supported in ES profile.");
		else
			BFOP(textureQueryLod);
		break;
	}

	case OpImageQueryLevels:
	{
		if (!options.es && options.version < 430)
			require_extension("GL_ARB_texture_query_levels");
		if (options.es)
			SPIRV_CROSS_THROW("textureQueryLevels not supported in ES profile.");
		UFOP(textureQueryLevels);
		break;
	}

	case OpImageQuerySamples:
	{
		auto *var = maybe_get_backing_variable(ops[2]);
		if (!var)
			SPIRV_CROSS_THROW(
			    "Bug. OpImageQuerySamples must have a backing variable so we know if the image is sampled or not.");

		auto &type = get<SPIRType>(var->basetype);
		bool image = type.image.sampled == 2;
		if (image)
			UFOP(imageSamples);
		else
			UFOP(textureSamples);
		break;
	}

	case OpSampledImage:
	{
		uint32_t result_type = ops[0];
		uint32_t id = ops[1];
		emit_sampled_image_op(result_type, id, ops[2], ops[3]);
		break;
	}

	case OpImageQuerySizeLod:
		BFOP(textureSize);
		break;

	// Image load/store
	case OpImageRead:
	{
		// We added Nonreadable speculatively to the OpImage variable due to glslangValidator
		// not adding the proper qualifiers.
		// If it turns out we need to read the image after all, remove the qualifier and recompile.
		auto *var = maybe_get_backing_variable(ops[2]);
		if (var)
		{
			auto &flags = meta.at(var->self).decoration.decoration_flags;
			if (flags & (1ull << DecorationNonReadable))
			{
				flags &= ~(1ull << DecorationNonReadable);
				force_recompile = true;
			}
		}

		uint32_t result_type = ops[0];
		uint32_t id = ops[1];

		bool pure;
		string imgexpr;
		auto &type = expression_type(ops[2]);

		if (var && var->remapped_variable) // Remapped input, just read as-is without any op-code
		{
			if (type.image.ms)
				SPIRV_CROSS_THROW("Trying to remap multisampled image to variable, this is not possible.");

			auto itr =
			    find_if(begin(pls_inputs), end(pls_inputs), [var](const PlsRemap &pls) { return pls.id == var->self; });

			if (itr == end(pls_inputs))
			{
				// For non-PLS inputs, we rely on subpass type remapping information to get it right
				// since ImageRead always returns 4-component vectors and the backing type is opaque.
				if (!var->remapped_components)
					SPIRV_CROSS_THROW("subpassInput was remapped, but remap_components is not set correctly.");
				imgexpr = remap_swizzle(result_type, var->remapped_components, ops[2]);
			}
			else
			{
				// PLS input could have different number of components than what the SPIR expects, swizzle to
				// the appropriate vector size.
				uint32_t components = pls_format_to_components(itr->format);
				imgexpr = remap_swizzle(result_type, components, ops[2]);
			}
			pure = true;
		}
		else if (type.image.dim == DimSubpassData)
		{
			if (options.vulkan_semantics)
			{
				// With Vulkan semantics, use the proper Vulkan GLSL construct.
				if (type.image.ms)
				{
					uint32_t operands = ops[4];
					if (operands != ImageOperandsSampleMask || length != 6)
						SPIRV_CROSS_THROW(
						    "Multisampled image used in OpImageRead, but unexpected operand mask was used.");

					uint32_t samples = ops[5];
					imgexpr = join("subpassLoad(", to_expression(ops[2]), ", ", to_expression(samples), ")");
				}
				else
					imgexpr = join("subpassLoad(", to_expression(ops[2]), ")");
			}
			else
			{
				if (type.image.ms)
				{
					uint32_t operands = ops[4];
					if (operands != ImageOperandsSampleMask || length != 6)
						SPIRV_CROSS_THROW(
						    "Multisampled image used in OpImageRead, but unexpected operand mask was used.");

					uint32_t samples = ops[5];
					imgexpr = join("texelFetch(", to_expression(ops[2]), ", ivec2(gl_FragCoord.xy), ",
					               to_expression(samples), ")");
				}
				else
				{
					// Implement subpass loads via texture barrier style sampling.
					imgexpr = join("texelFetch(", to_expression(ops[2]), ", ivec2(gl_FragCoord.xy), 0)");
				}
			}
			pure = true;
		}
		else
		{
			// Plain image load/store.
			if (type.image.ms)
			{
				uint32_t operands = ops[4];
				if (operands != ImageOperandsSampleMask || length != 6)
					SPIRV_CROSS_THROW("Multisampled image used in OpImageRead, but unexpected operand mask was used.");

				uint32_t samples = ops[5];
				imgexpr = join("imageLoad(", to_expression(ops[2]), ", ", to_expression(ops[3]), ", ",
				               to_expression(samples), ")");
			}
			else
				imgexpr = join("imageLoad(", to_expression(ops[2]), ", ", to_expression(ops[3]), ")");
			pure = false;
		}

		if (var && var->forwardable)
		{
			auto &e = emit_op(result_type, id, imgexpr, true);

			// We only need to track dependencies if we're reading from image load/store.
			if (!pure)
			{
				e.loaded_from = var->self;
				var->dependees.push_back(id);
			}
		}
		else
			emit_op(result_type, id, imgexpr, false);
		break;
	}

	case OpImageTexelPointer:
	{
		uint32_t result_type = ops[0];
		uint32_t id = ops[1];
		auto &e = set<SPIRExpression>(id, join(to_expression(ops[2]), ", ", to_expression(ops[3])), result_type, true);

		// When using the pointer, we need to know which variable it is actually loaded from.
		auto *var = maybe_get_backing_variable(ops[2]);
		e.loaded_from = var ? var->self : 0;
		break;
	}

	case OpImageWrite:
	{
		// We added Nonwritable speculatively to the OpImage variable due to glslangValidator
		// not adding the proper qualifiers.
		// If it turns out we need to write to the image after all, remove the qualifier and recompile.
		auto *var = maybe_get_backing_variable(ops[0]);
		if (var)
		{
			auto &flags = meta.at(var->self).decoration.decoration_flags;
			if (flags & (1ull << DecorationNonWritable))
			{
				flags &= ~(1ull << DecorationNonWritable);
				force_recompile = true;
			}
		}

		auto &type = expression_type(ops[0]);
		if (type.image.ms)
		{
			uint32_t operands = ops[3];
			if (operands != ImageOperandsSampleMask || length != 5)
				SPIRV_CROSS_THROW("Multisampled image used in OpImageWrite, but unexpected operand mask was used.");
			uint32_t samples = ops[4];
			statement("imageStore(", to_expression(ops[0]), ", ", to_expression(ops[1]), ", ", to_expression(samples),
			          ", ", to_expression(ops[2]), ");");
		}
		else
			statement("imageStore(", to_expression(ops[0]), ", ", to_expression(ops[1]), ", ", to_expression(ops[2]),
			          ");");

		if (var && variable_storage_is_aliased(*var))
			flush_all_aliased_variables();
		break;
	}

	case OpImageQuerySize:
	{
		auto &type = expression_type(ops[2]);
		uint32_t result_type = ops[0];
		uint32_t id = ops[1];

		if (type.basetype == SPIRType::Image)
		{
			// The size of an image is always constant.
			emit_op(result_type, id, join("imageSize(", to_expression(ops[2]), ")"), true);
		}
		else
			SPIRV_CROSS_THROW("Invalid type for OpImageQuerySize.");
		break;
	}

	// Compute
	case OpControlBarrier:
	{
		// Ignore execution and memory scope.
		if (get_entry_point().model == ExecutionModelGLCompute)
		{
			uint32_t mem = get<SPIRConstant>(ops[2]).scalar();
			if (mem == MemorySemanticsWorkgroupMemoryMask)
				statement("memoryBarrierShared();");
			else if (mem)
				statement("memoryBarrier();");
		}
		statement("barrier();");
		break;
	}

	case OpMemoryBarrier:
	{
		uint32_t mem = get<SPIRConstant>(ops[1]).scalar();

		// We cannot forward any loads beyond the memory barrier.
		if (mem)
			flush_all_active_variables();

		if (mem == MemorySemanticsWorkgroupMemoryMask)
			statement("memoryBarrierShared();");
		else if (mem)
			statement("memoryBarrier();");
		break;
	}

	case OpExtInst:
	{
		uint32_t extension_set = ops[2];
		if (get<SPIRExtension>(extension_set).ext != SPIRExtension::GLSL)
		{
			statement("// unimplemented ext op ", instruction.op);
			break;
		}

		emit_glsl_op(ops[0], ops[1], ops[3], &ops[4], length - 4);
		break;
	}

	default:
		statement("// unimplemented op ", instruction.op);
		break;
	}
}

// Appends function arguments, mapped from global variables, beyond the specified arg index.
// This is used when a function call uses fewer arguments than the function defines.
// This situation may occur if the function signature has been dynamically modified to
// extract global variables referenced from within the function, and convert them to
// function arguments. This is necessary for shader languages that do not support global
// access to shader input content from within a function (eg. Metal). Each additional
// function args uses the name of the global variable. Function nesting will modify the
// functions and calls all the way up the nesting chain.
void CompilerGLSL::append_global_func_args(const SPIRFunction &func, uint32_t index, vector<string> &arglist)
{
	auto &args = func.arguments;
	uint32_t arg_cnt = uint32_t(args.size());
	for (uint32_t arg_idx = index; arg_idx < arg_cnt; arg_idx++)
		arglist.push_back(to_func_call_arg(args[arg_idx].id));
}

string CompilerGLSL::to_member_name(const SPIRType &type, uint32_t index)
{
	auto &memb = meta[type.self].members;
	if (index < memb.size() && !memb[index].alias.empty())
		return memb[index].alias;
	else
		return join("_", index);
}

void CompilerGLSL::add_member_name(SPIRType &type, uint32_t index)
{
	auto &memb = meta[type.self].members;
	if (index < memb.size() && !memb[index].alias.empty())
	{
		auto &name = memb[index].alias;
		if (name.empty())
			return;

		// Reserved for temporaries.
		if (name[0] == '_' && name.size() >= 2 && isdigit(name[1]))
		{
			name.clear();
			return;
		}

		update_name_cache(type.member_name_cache, name);
	}
}

// Checks whether the member is a row_major matrix that requires conversion before use
bool CompilerGLSL::is_non_native_row_major_matrix(uint32_t id)
{
	// Natively supported row-major matrices do not need to be converted.
	if (backend.native_row_major_matrix)
		return false;

	// Non-matrix or column-major matrix types do not need to be converted.
	if (!(meta[id].decoration.decoration_flags & (1ull << DecorationRowMajor)))
		return false;

	// Only square row-major matrices can be converted at this time.
	// Converting non-square matrices will require defining custom GLSL function that
	// swaps matrix elements while retaining the original dimensional form of the matrix.
	const auto type = expression_type(id);
	if (type.columns != type.vecsize)
		SPIRV_CROSS_THROW("Row-major matrices must be square on this platform.");

	return true;
}

// Checks whether the member is a row_major matrix that requires conversion before use
bool CompilerGLSL::member_is_non_native_row_major_matrix(const SPIRType &type, uint32_t index)
{
	// Natively supported row-major matrices do not need to be converted.
	if (backend.native_row_major_matrix)
		return false;

	// Non-matrix or column-major matrix types do not need to be converted.
	if (!(combined_decoration_for_member(type, index) & (1ull << DecorationRowMajor)))
		return false;

	// Only square row-major matrices can be converted at this time.
	// Converting non-square matrices will require defining custom GLSL function that
	// swaps matrix elements while retaining the original dimensional form of the matrix.
	const auto mbr_type = get<SPIRType>(type.member_types[index]);
	if (mbr_type.columns != mbr_type.vecsize)
		SPIRV_CROSS_THROW("Row-major matrices must be square on this platform.");

	return true;
}

// Wraps the expression string in a function call that converts the
// row_major matrix result of the expression to a column_major matrix.
// Base implementation uses the standard library transpose() function.
// Subclasses may override to use a different function.
string CompilerGLSL::convert_row_major_matrix(string exp_str)
{
	strip_enclosed_expression(exp_str);
	return join("transpose(", exp_str, ")");
}

string CompilerGLSL::variable_decl(const SPIRType &type, const string &name)
{
	string type_name = type_to_glsl(type);
	remap_variable_type_name(type, name, type_name);
	return join(type_name, " ", name, type_to_array_glsl(type));
}

string CompilerGLSL::member_decl(const SPIRType &type, const SPIRType &membertype, uint32_t index)
{
	uint64_t memberflags = 0;
	auto &memb = meta[type.self].members;
	if (index < memb.size())
		memberflags = memb[index].decoration_flags;

	string qualifiers;
	bool is_block = (meta[type.self].decoration.decoration_flags &
	                 ((1ull << DecorationBlock) | (1ull << DecorationBufferBlock))) != 0;
	if (is_block)
		qualifiers = to_interpolation_qualifiers(memberflags);

	return join(layout_for_member(type, index), flags_to_precision_qualifiers_glsl(membertype, memberflags), qualifiers,
	            variable_decl(membertype, to_member_name(type, index)));
}

const char *CompilerGLSL::flags_to_precision_qualifiers_glsl(const SPIRType &type, uint64_t flags)
{
	if (options.es)
	{
		auto &execution = get_entry_point();

		// Structs do not have precision qualifiers, neither do doubles (desktop only anyways, so no mediump/highp).
		if (type.basetype != SPIRType::Float && type.basetype != SPIRType::Int && type.basetype != SPIRType::UInt &&
		    type.basetype != SPIRType::Image && type.basetype != SPIRType::SampledImage &&
		    type.basetype != SPIRType::Sampler)
			return "";

		if (flags & (1ull << DecorationRelaxedPrecision))
		{
			bool implied_fmediump = type.basetype == SPIRType::Float &&
			                        options.fragment.default_float_precision == Options::Mediump &&
			                        execution.model == ExecutionModelFragment;

			bool implied_imediump = (type.basetype == SPIRType::Int || type.basetype == SPIRType::UInt) &&
			                        options.fragment.default_int_precision == Options::Mediump &&
			                        execution.model == ExecutionModelFragment;

			return implied_fmediump || implied_imediump ? "" : "mediump ";
		}
		else
		{
			bool implied_fhighp =
			    type.basetype == SPIRType::Float && ((options.fragment.default_float_precision == Options::Highp &&
			                                          execution.model == ExecutionModelFragment) ||
			                                         (execution.model != ExecutionModelFragment));

			bool implied_ihighp = (type.basetype == SPIRType::Int || type.basetype == SPIRType::UInt) &&
			                      ((options.fragment.default_int_precision == Options::Highp &&
			                        execution.model == ExecutionModelFragment) ||
			                       (execution.model != ExecutionModelFragment));

			return implied_fhighp || implied_ihighp ? "" : "highp ";
		}
	}
	else
		return "";
}

const char *CompilerGLSL::to_precision_qualifiers_glsl(uint32_t id)
{
	return flags_to_precision_qualifiers_glsl(expression_type(id), meta[id].decoration.decoration_flags);
}

string CompilerGLSL::to_qualifiers_glsl(uint32_t id)
{
	auto flags = meta[id].decoration.decoration_flags;
	string res;

	auto *var = maybe_get<SPIRVariable>(id);

	if (var && var->storage == StorageClassWorkgroup && !backend.shared_is_implied)
		res += "shared ";

	res += to_precision_qualifiers_glsl(id);
	res += to_interpolation_qualifiers(flags);
	auto &type = expression_type(id);
	if (type.image.dim != DimSubpassData && type.image.sampled == 2)
	{
		if (flags & (1ull << DecorationNonWritable))
			res += "readonly ";
		if (flags & (1ull << DecorationNonReadable))
			res += "writeonly ";
	}

	return res;
}

string CompilerGLSL::argument_decl(const SPIRFunction::Parameter &arg)
{
	// glslangValidator seems to make all arguments pointer no matter what which is rather bizarre ...
	// Not sure if argument being pointer type should make the argument inout.
	auto &type = expression_type(arg.id);
	const char *direction = "";

	if (type.pointer)
	{
		if (arg.write_count && arg.read_count)
			direction = "inout ";
		else if (arg.write_count)
			direction = "out ";
	}

	return join(direction, to_qualifiers_glsl(arg.id), variable_decl(type, to_name(arg.id)));
}

string CompilerGLSL::variable_decl(const SPIRVariable &variable)
{
	// Ignore the pointer type since GLSL doesn't have pointers.
	auto &type = get<SPIRType>(variable.basetype);
	auto res = join(to_qualifiers_glsl(variable.self), variable_decl(type, to_name(variable.self)));
	if (variable.loop_variable)
		res += join(" = ", to_expression(variable.static_expression));
	else if (variable.initializer)
		res += join(" = ", to_expression(variable.initializer));
	return res;
}

const char *CompilerGLSL::to_pls_qualifiers_glsl(const SPIRVariable &variable)
{
	auto flags = meta[variable.self].decoration.decoration_flags;
	if (flags & (1ull << DecorationRelaxedPrecision))
		return "mediump ";
	else
		return "highp ";
}

string CompilerGLSL::pls_decl(const PlsRemap &var)
{
	auto &variable = get<SPIRVariable>(var.id);

	SPIRType type;
	type.vecsize = pls_format_to_components(var.format);
	type.basetype = pls_format_to_basetype(var.format);

	return join(to_pls_layout(var.format), to_pls_qualifiers_glsl(variable), type_to_glsl(type), " ",
	            to_name(variable.self));
}

uint32_t CompilerGLSL::to_array_size_literal(const SPIRType &type, uint32_t index) const
{
	assert(type.array.size() == type.array_size_literal.size());

	if (!type.array_size_literal[index])
		SPIRV_CROSS_THROW("The array size is not a literal, but a specialization constant or spec constant op.");

	return type.array[index];
}

string CompilerGLSL::to_array_size(const SPIRType &type, uint32_t index)
{
	assert(type.array.size() == type.array_size_literal.size());

	auto &size = type.array[index];
	if (!type.array_size_literal[index])
		return to_expression(size);
	else if (size)
		return convert_to_string(size);
	else if (!backend.flexible_member_array_supported)
	{
		// For runtime-sized arrays, we can work around
		// lack of standard support for this by simply having
		// a single element array.
		//
		// Runtime length arrays must always be the last element
		// in an interface block.
		return "1";
	}
	else
		return "";
}

string CompilerGLSL::type_to_array_glsl(const SPIRType &type)
{
	if (type.array.empty())
		return "";

	string res;
	for (auto i = uint32_t(type.array.size()); i; i--)
	{
		res += "[";
		res += to_array_size(type, i - 1);
		res += "]";
	}
	return res;
}

string CompilerGLSL::image_type_glsl(const SPIRType &type)
{
	auto &imagetype = get<SPIRType>(type.image.type);
	string res;

	switch (imagetype.basetype)
	{
	case SPIRType::Int:
		res = "i";
		break;
	case SPIRType::UInt:
		res = "u";
		break;
	default:
		break;
	}

	if (type.basetype == SPIRType::Image && type.image.dim == DimSubpassData && options.vulkan_semantics)
		return res + "subpassInput" + (type.image.ms ? "MS" : "");

	// If we're emulating subpassInput with samplers, force sampler2D
	// so we don't have to specify format.
	if (type.basetype == SPIRType::Image && type.image.dim != DimSubpassData)
		res += type.image.sampled == 2 ? "image" : "texture";
	else
		res += "sampler";

	switch (type.image.dim)
	{
	case Dim1D:
		res += "1D";
		break;
	case Dim2D:
		res += "2D";
		break;
	case Dim3D:
		res += "3D";
		break;
	case DimCube:
		res += "Cube";
		break;

	case DimBuffer:
		if (options.es && options.version < 320)
			require_extension("GL_OES_texture_buffer");
		else if (!options.es && options.version < 300)
			require_extension("GL_EXT_texture_buffer_object");
		res += "Buffer";
		break;

	case DimSubpassData:
		res += "2D";
		break;
	default:
		SPIRV_CROSS_THROW("Only 1D, 2D, 3D, Buffer, InputTarget and Cube textures supported.");
	}

	if (type.image.ms)
		res += "MS";
	if (type.image.arrayed)
	{
		if (is_legacy_desktop())
			require_extension("GL_EXT_texture_array");
		res += "Array";
	}
	if (type.image.depth)
		res += "Shadow";

	return res;
}

string CompilerGLSL::type_to_glsl_constructor(const SPIRType &type)
{
	auto e = type_to_glsl(type);
	for (uint32_t i = 0; i < type.array.size(); i++)
		e += "[]";
	return e;
}

string CompilerGLSL::type_to_glsl(const SPIRType &type)
{
	// Ignore the pointer type since GLSL doesn't have pointers.

	switch (type.basetype)
	{
	case SPIRType::Struct:
		// Need OpName lookup here to get a "sensible" name for a struct.
		if (backend.explicit_struct_type)
			return join("struct ", to_name(type.self));
		else
			return to_name(type.self);

	case SPIRType::Image:
	case SPIRType::SampledImage:
		return image_type_glsl(type);

	case SPIRType::Sampler:
		// Not really used.
		return "sampler";

	case SPIRType::Void:
		return "void";

	default:
		break;
	}

	if (type.vecsize == 1 && type.columns == 1) // Scalar builtin
	{
		switch (type.basetype)
		{
		case SPIRType::Boolean:
			return "bool";
		case SPIRType::Int:
			return backend.basic_int_type;
		case SPIRType::UInt:
			return backend.basic_uint_type;
		case SPIRType::AtomicCounter:
			return "atomic_uint";
		case SPIRType::Float:
			return "float";
		case SPIRType::Double:
			return "double";
		case SPIRType::Int64:
			return "int64_t";
		case SPIRType::UInt64:
			return "uint64_t";
		default:
			return "???";
		}
	}
	else if (type.vecsize > 1 && type.columns == 1) // Vector builtin
	{
		switch (type.basetype)
		{
		case SPIRType::Boolean:
			return join("bvec", type.vecsize);
		case SPIRType::Int:
			return join("ivec", type.vecsize);
		case SPIRType::UInt:
			return join("uvec", type.vecsize);
		case SPIRType::Float:
			return join("vec", type.vecsize);
		case SPIRType::Double:
			return join("dvec", type.vecsize);
		case SPIRType::Int64:
			return join("i64vec", type.vecsize);
		case SPIRType::UInt64:
			return join("u64vec", type.vecsize);
		default:
			return "???";
		}
	}
	else if (type.vecsize == type.columns) // Simple Matrix builtin
	{
		switch (type.basetype)
		{
		case SPIRType::Boolean:
			return join("bmat", type.vecsize);
		case SPIRType::Int:
			return join("imat", type.vecsize);
		case SPIRType::UInt:
			return join("umat", type.vecsize);
		case SPIRType::Float:
			return join("mat", type.vecsize);
		case SPIRType::Double:
			return join("dmat", type.vecsize);
		// Matrix types not supported for int64/uint64.
		default:
			return "???";
		}
	}
	else
	{
		switch (type.basetype)
		{
		case SPIRType::Boolean:
			return join("bmat", type.columns, "x", type.vecsize);
		case SPIRType::Int:
			return join("imat", type.columns, "x", type.vecsize);
		case SPIRType::UInt:
			return join("umat", type.columns, "x", type.vecsize);
		case SPIRType::Float:
			return join("mat", type.columns, "x", type.vecsize);
		case SPIRType::Double:
			return join("dmat", type.columns, "x", type.vecsize);
		// Matrix types not supported for int64/uint64.
		default:
			return "???";
		}
	}
}

void CompilerGLSL::add_variable(unordered_set<string> &variables, uint32_t id)
{
	auto &name = meta[id].decoration.alias;
	if (name.empty())
		return;

	// Reserved for temporaries.
	if (name[0] == '_' && name.size() >= 2 && isdigit(name[1]))
	{
		name.clear();
		return;
	}

	update_name_cache(variables, name);
}

void CompilerGLSL::add_local_variable_name(uint32_t id)
{
	add_variable(local_variable_names, id);
}

void CompilerGLSL::add_resource_name(uint32_t id)
{
	add_variable(resource_names, id);
}

void CompilerGLSL::add_header_line(const std::string &line)
{
	header_lines.push_back(line);
}

void CompilerGLSL::require_extension(const string &ext)
{
	if (forced_extensions.find(ext) == end(forced_extensions))
	{
		forced_extensions.insert(ext);
		force_recompile = true;
	}
}

bool CompilerGLSL::check_atomic_image(uint32_t id)
{
	auto &type = expression_type(id);
	if (type.storage == StorageClassImage)
	{
		if (options.es && options.version < 320)
			require_extension("GL_OES_shader_image_atomic");

		auto *var = maybe_get_backing_variable(id);
		if (var)
		{
			auto &flags = meta.at(var->self).decoration.decoration_flags;
			if (flags & ((1ull << DecorationNonWritable) | (1ull << DecorationNonReadable)))
			{
				flags &= ~(1ull << DecorationNonWritable);
				flags &= ~(1ull << DecorationNonReadable);
				force_recompile = true;
			}
		}
		return true;
	}
	else
		return false;
}

void CompilerGLSL::emit_function_prototype(SPIRFunction &func, uint64_t return_flags)
{
	// Avoid shadow declarations.
	local_variable_names = resource_names;

	string decl;

	auto &type = get<SPIRType>(func.return_type);
	decl += flags_to_precision_qualifiers_glsl(type, return_flags);
	decl += type_to_glsl(type);
	decl += " ";

	if (func.self == entry_point)
	{
		decl += "main";
		processing_entry_point = true;
	}
	else
		decl += to_name(func.self);

	decl += "(";
	vector<string> arglist;
	for (auto &arg : func.arguments)
	{
		// Do not pass in separate images or samplers if we're remapping
		// to combined image samplers.
		if (skip_argument(arg.id))
			continue;

		// Might change the variable name if it already exists in this function.
		// SPIRV OpName doesn't have any semantic effect, so it's valid for an implementation
		// to use same name for variables.
		// Since we want to make the GLSL debuggable and somewhat sane, use fallback names for variables which are duplicates.
		add_local_variable_name(arg.id);

		arglist.push_back(argument_decl(arg));

		// Hold a pointer to the parameter so we can invalidate the readonly field if needed.
		auto *var = maybe_get<SPIRVariable>(arg.id);
		if (var)
			var->parameter = &arg;
	}

	for (auto &arg : func.shadow_arguments)
	{
		// Might change the variable name if it already exists in this function.
		// SPIRV OpName doesn't have any semantic effect, so it's valid for an implementation
		// to use same name for variables.
		// Since we want to make the GLSL debuggable and somewhat sane, use fallback names for variables which are duplicates.
		add_local_variable_name(arg.id);

		arglist.push_back(argument_decl(arg));

		// Hold a pointer to the parameter so we can invalidate the readonly field if needed.
		auto *var = maybe_get<SPIRVariable>(arg.id);
		if (var)
			var->parameter = &arg;
	}

	decl += merge(arglist);
	decl += ")";
	statement(decl);
}

void CompilerGLSL::emit_function(SPIRFunction &func, uint64_t return_flags)
{
	// Avoid potential cycles.
	if (func.active)
		return;
	func.active = true;

	// If we depend on a function, emit that function before we emit our own function.
	for (auto block : func.blocks)
	{
		auto &b = get<SPIRBlock>(block);
		for (auto &i : b.ops)
		{
			auto ops = stream(i);
			auto op = static_cast<Op>(i.op);

			if (op == OpFunctionCall)
			{
				// Recursively emit functions which are called.
				uint32_t id = ops[2];
				emit_function(get<SPIRFunction>(id), meta[ops[1]].decoration.decoration_flags);
			}
		}
	}

	emit_function_prototype(func, return_flags);
	begin_scope();

	current_function = &func;
	auto &entry_block = get<SPIRBlock>(func.entry_block);

	if (!func.analyzed_variable_scope)
	{
		if (options.cfg_analysis)
		{
			analyze_variable_scope(func);

			// Check if we can actually use the loop variables we found in analyze_variable_scope.
			// To use multiple initializers, we need the same type and qualifiers.
			for (auto block : func.blocks)
			{
				auto &b = get<SPIRBlock>(block);
				if (b.loop_variables.size() < 2)
					continue;

				uint64_t flags = get_decoration_mask(b.loop_variables.front());
				uint32_t type = get<SPIRVariable>(b.loop_variables.front()).basetype;
				bool invalid_initializers = false;
				for (auto loop_variable : b.loop_variables)
				{
					if (flags != get_decoration_mask(loop_variable) ||
					    type != get<SPIRVariable>(b.loop_variables.front()).basetype)
					{
						invalid_initializers = true;
						break;
					}
				}

				if (invalid_initializers)
				{
					for (auto loop_variable : b.loop_variables)
						get<SPIRVariable>(loop_variable).loop_variable = false;
					b.loop_variables.clear();
				}
			}
		}
		else
			entry_block.dominated_variables = func.local_variables;
		func.analyzed_variable_scope = true;
	}

	for (auto &v : func.local_variables)
	{
		auto &var = get<SPIRVariable>(v);
		if (expression_is_lvalue(v))
		{
			add_local_variable_name(var.self);

			if (var.initializer)
				statement(variable_decl(var), ";");
			else
			{
				// Don't declare variable until first use to declutter the GLSL output quite a lot.
				// If we don't touch the variable before first branch,
				// declare it then since we need variable declaration to be in top scope.
				var.deferred_declaration = true;
			}
		}
		else
		{
			// HACK: SPIRV likes to use samplers and images as local variables, but GLSL does not allow this.
			// For these types (non-lvalue), we enforce forwarding through a shadowed variable.
			// This means that when we OpStore to these variables, we just write in the expression ID directly.
			// This breaks any kind of branching, since the variable must be statically assigned.
			// Branching on samplers and images would be pretty much impossible to fake in GLSL.
			var.statically_assigned = true;
		}

		var.loop_variable_enable = false;

		// Loop variables are never declared outside their for-loop, so block any implicit declaration.
		if (var.loop_variable)
			var.deferred_declaration = false;
	}

	entry_block.loop_dominator = SPIRBlock::NoDominator;
	emit_block_chain(entry_block);

	end_scope();
	processing_entry_point = false;
	statement("");
}

void CompilerGLSL::emit_fixup()
{
	auto &execution = get_entry_point();
	if (execution.model == ExecutionModelVertex && options.vertex.fixup_clipspace)
	{
		const char *suffix = backend.float_literal_suffix ? "f" : "";
		statement("gl_Position.z = 2.0", suffix, " * gl_Position.z - gl_Position.w;");
	}
}

bool CompilerGLSL::flush_phi_required(uint32_t from, uint32_t to)
{
	auto &child = get<SPIRBlock>(to);
	for (auto &phi : child.phi_variables)
		if (phi.parent == from)
			return true;
	return false;
}

void CompilerGLSL::flush_phi(uint32_t from, uint32_t to)
{
	auto &child = get<SPIRBlock>(to);

	for (auto &phi : child.phi_variables)
		if (phi.parent == from)
			statement(to_expression(phi.function_variable), " = ", to_expression(phi.local_variable), ";");
}

void CompilerGLSL::branch(uint32_t from, uint32_t to)
{
	flush_phi(from, to);
	flush_all_active_variables();

	// This is only a continue if we branch to our loop dominator.
	if (loop_blocks.find(to) != end(loop_blocks) && get<SPIRBlock>(from).loop_dominator == to)
	{
		// This can happen if we had a complex continue block which was emitted.
		// Once the continue block tries to branch to the loop header, just emit continue;
		// and end the chain here.
		statement("continue;");
	}
	else if (is_continue(to))
	{
		auto &to_block = get<SPIRBlock>(to);
		if (to_block.complex_continue)
		{
			// Just emit the whole block chain as is.
			auto usage_counts = expression_usage_counts;
			auto invalid = invalid_expressions;

			emit_block_chain(to_block);

			// Expression usage counts and invalid expressions
			// are moot after returning from the continue block.
			// Since we emit the same block multiple times,
			// we don't want to invalidate ourselves.
			expression_usage_counts = usage_counts;
			invalid_expressions = invalid;
		}
		else
		{
			auto &from_block = get<SPIRBlock>(from);
			auto &dominator = get<SPIRBlock>(from_block.loop_dominator);

			// For non-complex continue blocks, we implicitly branch to the continue block
			// by having the continue block be part of the loop header in for (; ; continue-block).
			bool outside_control_flow = block_is_outside_flow_control_from_block(dominator, from_block);

			// Some simplification for for-loops. We always end up with a useless continue;
			// statement since we branch to a loop block.
			// Walk the CFG, if we uncoditionally execute the block calling continue assuming we're in the loop block,
			// we can avoid writing out an explicit continue statement.
			// Similar optimization to return statements if we know we're outside flow control.
			if (!outside_control_flow)
				statement("continue;");
		}
	}
	else if (is_break(to))
		statement("break;");
	else if (!is_conditional(to))
		emit_block_chain(get<SPIRBlock>(to));
}

void CompilerGLSL::branch(uint32_t from, uint32_t cond, uint32_t true_block, uint32_t false_block)
{
	// If we branch directly to a selection merge target, we don't really need a code path.
	bool true_sub = !is_conditional(true_block);
	bool false_sub = !is_conditional(false_block);

	if (true_sub)
	{
		statement("if (", to_expression(cond), ")");
		begin_scope();
		branch(from, true_block);
		end_scope();

		if (false_sub)
		{
			statement("else");
			begin_scope();
			branch(from, false_block);
			end_scope();
		}
		else if (flush_phi_required(from, false_block))
		{
			statement("else");
			begin_scope();
			flush_phi(from, false_block);
			end_scope();
		}
	}
	else if (false_sub && !true_sub)
	{
		// Only need false path, use negative conditional.
		statement("if (!", to_expression(cond), ")");
		begin_scope();
		branch(from, false_block);
		end_scope();

		if (flush_phi_required(from, true_block))
		{
			statement("else");
			begin_scope();
			flush_phi(from, true_block);
			end_scope();
		}
	}
}

void CompilerGLSL::propagate_loop_dominators(const SPIRBlock &block)
{
	// Propagate down the loop dominator block, so that dominated blocks can back trace.
	if (block.merge == SPIRBlock::MergeLoop || block.loop_dominator)
	{
		uint32_t dominator = block.merge == SPIRBlock::MergeLoop ? block.self : block.loop_dominator;

		auto set_dominator = [this](uint32_t self, uint32_t new_dominator) {
			auto &dominated_block = this->get<SPIRBlock>(self);

			// If we already have a loop dominator, we're trying to break out to merge targets
			// which should not update the loop dominator.
			if (!dominated_block.loop_dominator)
				dominated_block.loop_dominator = new_dominator;
		};

		// After merging a loop, we inherit the loop dominator always.
		if (block.merge_block)
			set_dominator(block.merge_block, block.loop_dominator);

		if (block.true_block)
			set_dominator(block.true_block, dominator);
		if (block.false_block)
			set_dominator(block.false_block, dominator);
		if (block.next_block)
			set_dominator(block.next_block, dominator);

		for (auto &c : block.cases)
			set_dominator(c.block, dominator);

		// In older glslang output continue_block can be == loop header.
		if (block.continue_block && block.continue_block != block.self)
			set_dominator(block.continue_block, dominator);
	}
}

// FIXME: This currently cannot handle complex continue blocks
// as in do-while.
// This should be seen as a "trivial" continue block.
string CompilerGLSL::emit_continue_block(uint32_t continue_block)
{
	auto *block = &get<SPIRBlock>(continue_block);

	// While emitting the continue block, declare_temporary will check this
	// if we have to emit temporaries.
	current_continue_block = block;

	vector<string> statements;

	// Capture all statements into our list.
	auto *old = redirect_statement;
	redirect_statement = &statements;

	// Stamp out all blocks one after each other.
	while (loop_blocks.find(block->self) == end(loop_blocks))
	{
		propagate_loop_dominators(*block);
		// Write out all instructions we have in this block.
		for (auto &op : block->ops)
			emit_instruction(op);

		// For plain branchless for/while continue blocks.
		if (block->next_block)
		{
			flush_phi(continue_block, block->next_block);
			block = &get<SPIRBlock>(block->next_block);
		}
		// For do while blocks. The last block will be a select block.
		else if (block->true_block)
		{
			flush_phi(continue_block, block->true_block);
			block = &get<SPIRBlock>(block->true_block);
		}
	}

	// Restore old pointer.
	redirect_statement = old;

	// Somewhat ugly, strip off the last ';' since we use ',' instead.
	// Ideally, we should select this behavior in statement().
	for (auto &s : statements)
	{
		if (!s.empty() && s.back() == ';')
			s.erase(s.size() - 1, 1);
	}

	current_continue_block = nullptr;
	return merge(statements);
}

string CompilerGLSL::emit_for_loop_initializers(const SPIRBlock &block)
{
	if (block.loop_variables.empty())
		return "";

	if (block.loop_variables.size() == 1)
	{
		return variable_decl(get<SPIRVariable>(block.loop_variables.front()));
	}
	else
	{
		auto &var = get<SPIRVariable>(block.loop_variables.front());
		auto &type = get<SPIRType>(var.basetype);

		// Don't remap the type here as we have multiple names,
		// doesn't make sense to remap types for loop variables anyways.
		// It is assumed here that all relevant qualifiers are equal for all loop variables.
		string expr = join(to_qualifiers_glsl(var.self), type_to_glsl(type), " ");

		for (auto &loop_var : block.loop_variables)
		{
			auto &v = get<SPIRVariable>(loop_var);
			expr += join(to_name(loop_var), " = ", to_expression(v.static_expression));
			if (&loop_var != &block.loop_variables.back())
				expr += ", ";
		}
		return expr;
	}
}

bool CompilerGLSL::attempt_emit_loop_header(SPIRBlock &block, SPIRBlock::Method method)
{
	SPIRBlock::ContinueBlockType continue_type = continue_block_type(get<SPIRBlock>(block.continue_block));

	if (method == SPIRBlock::MergeToSelectForLoop)
	{
		uint32_t current_count = statement_count;
		// If we're trying to create a true for loop,
		// we need to make sure that all opcodes before branch statement do not actually emit any code.
		// We can then take the condition expression and create a for (; cond ; ) { body; } structure instead.
		for (auto &op : block.ops)
			emit_instruction(op);

		bool condition_is_temporary = forced_temporaries.find(block.condition) == end(forced_temporaries);

		// This can work! We only did trivial things which could be forwarded in block body!
		if (current_count == statement_count && condition_is_temporary)
		{
			switch (continue_type)
			{
			case SPIRBlock::ForLoop:
				statement("for (", emit_for_loop_initializers(block), "; ", to_expression(block.condition), "; ",
				          emit_continue_block(block.continue_block), ")");
				break;

			case SPIRBlock::WhileLoop:
				statement("while (", to_expression(block.condition), ")");
				break;

			default:
				SPIRV_CROSS_THROW("For/while loop detected, but need while/for loop semantics.");
			}

			begin_scope();
			return true;
		}
		else
		{
			block.disable_block_optimization = true;
			force_recompile = true;
			begin_scope(); // We'll see an end_scope() later.
			return false;
		}
	}
	else if (method == SPIRBlock::MergeToDirectForLoop)
	{
		auto &child = get<SPIRBlock>(block.next_block);

		// This block may be a dominating block, so make sure we flush undeclared variables before building the for loop header.
		flush_undeclared_variables(child);

		uint32_t current_count = statement_count;

		// If we're trying to create a true for loop,
		// we need to make sure that all opcodes before branch statement do not actually emit any code.
		// We can then take the condition expression and create a for (; cond ; ) { body; } structure instead.
		for (auto &op : child.ops)
			emit_instruction(op);

		bool condition_is_temporary = forced_temporaries.find(child.condition) == end(forced_temporaries);

		if (current_count == statement_count && condition_is_temporary)
		{
			propagate_loop_dominators(child);

			switch (continue_type)
			{
			case SPIRBlock::ForLoop:
				statement("for (", emit_for_loop_initializers(block), "; ", to_expression(child.condition), "; ",
				          emit_continue_block(block.continue_block), ")");
				break;

			case SPIRBlock::WhileLoop:
				statement("while (", to_expression(child.condition), ")");
				break;

			default:
				SPIRV_CROSS_THROW("For/while loop detected, but need while/for loop semantics.");
			}

			begin_scope();
			branch(child.self, child.true_block);
			return true;
		}
		else
		{
			block.disable_block_optimization = true;
			force_recompile = true;
			begin_scope(); // We'll see an end_scope() later.
			return false;
		}
	}
	else
		return false;
}

void CompilerGLSL::flush_undeclared_variables(SPIRBlock &block)
{
	for (auto &v : block.dominated_variables)
	{
		auto &var = get<SPIRVariable>(v);
		if (var.deferred_declaration)
			statement(variable_decl(var), ";");
		var.deferred_declaration = false;
	}
}

void CompilerGLSL::emit_block_chain(SPIRBlock &block)
{
	propagate_loop_dominators(block);

	bool select_branch_to_true_block = false;
	bool skip_direct_branch = false;
	bool emitted_for_loop_header = false;

	// If we need to force temporaries for certain IDs due to continue blocks, do it before starting loop header.
	for (auto &tmp : block.declare_temporary)
	{
		auto flags = meta[tmp.second].decoration.decoration_flags;
		auto &type = get<SPIRType>(tmp.first);
		statement(flags_to_precision_qualifiers_glsl(type, flags), variable_decl(type, to_name(tmp.second)), ";");
	}

	SPIRBlock::ContinueBlockType continue_type = SPIRBlock::ContinueNone;
	if (block.continue_block)
		continue_type = continue_block_type(get<SPIRBlock>(block.continue_block));

	// If we have loop variables, stop masking out access to the variable now.
	for (auto var : block.loop_variables)
		get<SPIRVariable>(var).loop_variable_enable = true;

	// This is the older loop behavior in glslang which branches to loop body directly from the loop header.
	if (block_is_loop_candidate(block, SPIRBlock::MergeToSelectForLoop))
	{
		flush_undeclared_variables(block);
		if (attempt_emit_loop_header(block, SPIRBlock::MergeToSelectForLoop))
		{
			// The body of while, is actually just the true block, so always branch there unconditionally.
			select_branch_to_true_block = true;
			emitted_for_loop_header = true;
		}
	}
	// This is the newer loop behavior in glslang which branches from Loop header directly to
	// a new block, which in turn has a OpBranchSelection without a selection merge.
	else if (block_is_loop_candidate(block, SPIRBlock::MergeToDirectForLoop))
	{
		flush_undeclared_variables(block);
		if (attempt_emit_loop_header(block, SPIRBlock::MergeToDirectForLoop))
		{
			skip_direct_branch = true;
			emitted_for_loop_header = true;
		}
	}
	else if (continue_type == SPIRBlock::DoWhileLoop)
	{
		statement("do");
		begin_scope();
		for (auto &op : block.ops)
			emit_instruction(op);
	}
	else if (block.merge == SPIRBlock::MergeLoop)
	{
		flush_undeclared_variables(block);

		// We have a generic loop without any distinguishable pattern like for, while or do while.
		get<SPIRBlock>(block.continue_block).complex_continue = true;
		continue_type = SPIRBlock::ComplexLoop;

		statement("for (;;)");
		begin_scope();
		for (auto &op : block.ops)
			emit_instruction(op);
	}
	else
	{
		for (auto &op : block.ops)
			emit_instruction(op);
	}

	// If we didn't successfully emit a loop header and we had loop variable candidates, we have a problem
	// as writes to said loop variables might have been masked out, we need a recompile.
	if (!emitted_for_loop_header && !block.loop_variables.empty())
	{
		force_recompile = true;
		for (auto var : block.loop_variables)
			get<SPIRVariable>(var).loop_variable = false;
		block.loop_variables.clear();
	}

	flush_undeclared_variables(block);
	bool emit_next_block = true;

	// Handle end of block.
	switch (block.terminator)
	{
	case SPIRBlock::Direct:
		// True when emitting complex continue block.
		if (block.loop_dominator == block.next_block)
		{
			branch(block.self, block.next_block);
			emit_next_block = false;
		}
		// True if MergeToDirectForLoop succeeded.
		else if (skip_direct_branch)
			emit_next_block = false;
		else if (is_continue(block.next_block) || is_break(block.next_block) || is_conditional(block.next_block))
		{
			branch(block.self, block.next_block);
			emit_next_block = false;
		}
		break;

	case SPIRBlock::Select:
		// True if MergeToSelectForLoop succeeded.
		if (select_branch_to_true_block)
			branch(block.self, block.true_block);
		else
			branch(block.self, block.condition, block.true_block, block.false_block);
		break;

	case SPIRBlock::MultiSelect:
	{
		auto &type = expression_type(block.condition);
		bool uint32_t_case = type.basetype == SPIRType::UInt;

		statement("switch (", to_expression(block.condition), ")");
		begin_scope();

		for (auto &c : block.cases)
		{
			auto case_value =
			    uint32_t_case ? convert_to_string(uint32_t(c.value)) : convert_to_string(int32_t(c.value));
			statement("case ", case_value, ":");
			begin_scope();
			branch(block.self, c.block);
			end_scope();
		}

		if (block.default_block != block.next_block)
		{
			statement("default:");
			begin_scope();
			if (is_break(block.default_block))
				SPIRV_CROSS_THROW("Cannot break; out of a switch statement and out of a loop at the same time ...");
			branch(block.self, block.default_block);
			end_scope();
		}
		else if (flush_phi_required(block.self, block.next_block))
		{
			statement("default:");
			begin_scope();
			flush_phi(block.self, block.next_block);
			statement("break;");
			end_scope();
		}

		end_scope();
		break;
	}

	case SPIRBlock::Return:
		if (processing_entry_point)
			emit_fixup();

		if (block.return_value)
		{
			// OpReturnValue can return Undef, so don't emit anything for this case.
			if (ids.at(block.return_value).get_type() != TypeUndef)
				statement("return ", to_expression(block.return_value), ";");
		}
		// If this block is the very final block and not called from control flow,
		// we do not need an explicit return which looks out of place. Just end the function here.
		// In the very weird case of for(;;) { return; } executing return is unconditional,
		// but we actually need a return here ...
		else if (!block_is_outside_flow_control_from_block(get<SPIRBlock>(current_function->entry_block), block) ||
		         block.loop_dominator != SPIRBlock::NoDominator)
			statement("return;");
		break;

	case SPIRBlock::Kill:
		statement(backend.discard_literal, ";");
		break;

	default:
		SPIRV_CROSS_THROW("Unimplemented block terminator.");
	}

	if (block.next_block && emit_next_block)
	{
		// If we hit this case, we're dealing with an unconditional branch, which means we will output
		// that block after this. If we had selection merge, we already flushed phi variables.
		if (block.merge != SPIRBlock::MergeSelection)
			flush_phi(block.self, block.next_block);
		emit_block_chain(get<SPIRBlock>(block.next_block));
	}

	if (block.merge == SPIRBlock::MergeLoop)
	{
		if (continue_type == SPIRBlock::DoWhileLoop)
		{
			// Make sure that we run the continue block to get the expressions set, but this
			// should become an empty string.
			// We have no fallbacks if we cannot forward everything to temporaries ...
			auto statements = emit_continue_block(block.continue_block);
			if (!statements.empty())
			{
				// The DoWhile block has side effects, force ComplexLoop pattern next pass.
				get<SPIRBlock>(block.continue_block).complex_continue = true;
				force_recompile = true;
			}

			end_scope_decl(join("while (", to_expression(get<SPIRBlock>(block.continue_block).condition), ")"));
		}
		else
			end_scope();

		flush_phi(block.self, block.merge_block);
		emit_block_chain(get<SPIRBlock>(block.merge_block));
	}
}

void CompilerGLSL::begin_scope()
{
	statement("{");
	indent++;
}

void CompilerGLSL::end_scope()
{
	if (!indent)
		SPIRV_CROSS_THROW("Popping empty indent stack.");
	indent--;
	statement("}");
}

void CompilerGLSL::end_scope_decl()
{
	if (!indent)
		SPIRV_CROSS_THROW("Popping empty indent stack.");
	indent--;
	statement("};");
}

void CompilerGLSL::end_scope_decl(const string &decl)
{
	if (!indent)
		SPIRV_CROSS_THROW("Popping empty indent stack.");
	indent--;
	statement("} ", decl, ";");
}

void CompilerGLSL::check_function_call_constraints(const uint32_t *args, uint32_t length)
{
	// If our variable is remapped, and we rely on type-remapping information as
	// well, then we cannot pass the variable as a function parameter.
	// Fixing this is non-trivial without stamping out variants of the same function,
	// so for now warn about this and suggest workarounds instead.
	for (uint32_t i = 0; i < length; i++)
	{
		auto *var = maybe_get<SPIRVariable>(args[i]);
		if (!var || !var->remapped_variable)
			continue;

		auto &type = get<SPIRType>(var->basetype);
		if (type.basetype == SPIRType::Image && type.image.dim == DimSubpassData)
		{
			SPIRV_CROSS_THROW("Tried passing a remapped subpassInput variable to a function. "
			                  "This will not work correctly because type-remapping information is lost. "
			                  "To workaround, please consider not passing the subpass input as a function parameter, "
			                  "or use in/out variables instead which do not need type remapping information.");
		}
	}
}
