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

#ifndef SPIRV_CROSS_HPP
#define SPIRV_CROSS_HPP

#include "spirv.hpp"
#include <memory>
#include <stack>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "spirv_common.hpp"

namespace spirv_cross
{
struct Resource
{
	// Resources are identified with their SPIR-V ID.
	// This is the ID of the OpVariable.
	uint32_t id;

	// The type ID of the variable which includes arrays and all type modifications.
	// This type ID is not suitable for parsing OpMemberDecoration of a struct and other decorations in general
	// since these modifications typically happen on the base_type_id.
	uint32_t type_id;

	// The base type of the declared resource.
	// This type is the base type which ignores pointers and arrays of the type_id.
	// This is mostly useful to parse decorations of the underlying type.
	// base_type_id can also be obtained with get_type(get_type(type_id).self).
	uint32_t base_type_id;

	// The declared name (OpName) of the resource.
	// For Buffer blocks, the name actually reflects the externally
	// visible Block name.
	//
	// This name can be retrieved again by using either
	// get_name(id) or get_name(base_type_id) depending if it's a buffer block or not.
	//
	// This name can be an empty string in which case get_fallback_name(id) can be
	// used which obtains a suitable fallback identifier for an ID.
	std::string name;
};

struct ShaderResources
{
	std::vector<Resource> uniform_buffers;
	std::vector<Resource> storage_buffers;
	std::vector<Resource> stage_inputs;
	std::vector<Resource> stage_outputs;
	std::vector<Resource> subpass_inputs;
	std::vector<Resource> storage_images;
	std::vector<Resource> sampled_images;
	std::vector<Resource> atomic_counters;

	// There can only be one push constant block,
	// but keep the vector in case this restriction is lifted in the future.
	std::vector<Resource> push_constant_buffers;

	// For Vulkan GLSL and HLSL source,
	// these correspond to separate texture2D and samplers respectively.
	std::vector<Resource> separate_images;
	std::vector<Resource> separate_samplers;
};

struct CombinedImageSampler
{
	// The ID of the sampler2D variable.
	uint32_t combined_id;
	// The ID of the texture2D variable.
	uint32_t image_id;
	// The ID of the sampler variable.
	uint32_t sampler_id;
};

struct SpecializationConstant
{
	// The ID of the specialization constant.
	uint32_t id;
	// The constant ID of the constant, used in Vulkan during pipeline creation.
	uint32_t constant_id;
};

struct BufferRange
{
	unsigned index;
	size_t offset;
	size_t range;
};

class Compiler
{
public:
	friend class CFG;
	friend class DominatorBuilder;

	// The constructor takes a buffer of SPIR-V words and parses it.
	Compiler(std::vector<uint32_t> ir);

	virtual ~Compiler() = default;

	// After parsing, API users can modify the SPIR-V via reflection and call this
	// to disassemble the SPIR-V into the desired langauage.
	// Sub-classes actually implement this.
	virtual std::string compile();

	// Gets the identifier (OpName) of an ID. If not defined, an empty string will be returned.
	const std::string &get_name(uint32_t id) const;

	// Applies a decoration to an ID. Effectively injects OpDecorate.
	void set_decoration(uint32_t id, spv::Decoration decoration, uint32_t argument = 0);

	// Overrides the identifier OpName of an ID.
	// Identifiers beginning with underscores or identifiers which contain double underscores
	// are reserved by the implementation.
	void set_name(uint32_t id, const std::string &name);

	// Gets a bitmask for the decorations which are applied to ID.
	// I.e. (1ull << spv::DecorationFoo) | (1ull << spv::DecorationBar)
	uint64_t get_decoration_mask(uint32_t id) const;

	// Gets the value for decorations which take arguments.
	// If decoration doesn't exist or decoration is not recognized,
	// 0 will be returned.
	uint32_t get_decoration(uint32_t id, spv::Decoration decoration) const;

	// Removes the decoration for a an ID.
	void unset_decoration(uint32_t id, spv::Decoration decoration);

	// Gets the SPIR-V associated with ID.
	// Mostly used with Resource::type_id and Resource::base_type_id to parse the underlying type of a resource.
	const SPIRType &get_type(uint32_t id) const;

	// Gets the underlying storage class for an OpVariable.
	spv::StorageClass get_storage_class(uint32_t id) const;

	// If get_name() is an empty string, get the fallback name which will be used
	// instead in the disassembled source.
	virtual const std::string get_fallback_name(uint32_t id) const
	{
		return join("_", id);
	}

	// Given an OpTypeStruct in ID, obtain the identifier for member number "index".
	// This may be an empty string.
	const std::string &get_member_name(uint32_t id, uint32_t index) const;

	// Given an OpTypeStruct in ID, obtain the OpMemberDecoration for member number "index".
	uint32_t get_member_decoration(uint32_t id, uint32_t index, spv::Decoration decoration) const;

	// Sets the member identifier for OpTypeStruct ID, member number "index".
	void set_member_name(uint32_t id, uint32_t index, const std::string &name);

	// Sets the qualified member identifier for OpTypeStruct ID, member number "index".
	void set_member_qualified_name(uint32_t id, uint32_t index, const std::string &name);

	// Gets the decoration mask for a member of a struct, similar to get_decoration_mask.
	uint64_t get_member_decoration_mask(uint32_t id, uint32_t index) const;

	// Similar to set_decoration, but for struct members.
	void set_member_decoration(uint32_t id, uint32_t index, spv::Decoration decoration, uint32_t argument = 0);

	// Unsets a member decoration, similar to unset_decoration.
	void unset_member_decoration(uint32_t id, uint32_t index, spv::Decoration decoration);

	// Gets the fallback name for a member, similar to get_fallback_name.
	virtual const std::string get_fallback_member_name(uint32_t index) const
	{
		return join("_", index);
	}

	// Returns a vector of which members of a struct are potentially in use by a
	// SPIR-V shader. The granularity of this analysis is per-member of a struct.
	// This can be used for Buffer (UBO), BufferBlock (SSBO) and PushConstant blocks.
	// ID is the Resource::id obtained from get_shader_resources().
	std::vector<BufferRange> get_active_buffer_ranges(uint32_t id) const;

	// Returns the effective size of a buffer block.
	size_t get_declared_struct_size(const SPIRType &struct_type) const;

	// Returns the effective size of a buffer block struct member.
	virtual size_t get_declared_struct_member_size(const SPIRType &struct_type, uint32_t index) const;

	// Legacy GLSL compatibility method.
	// Takes a variable with a block interface and flattens it into a T array[N]; array instead.
	// For this to work, all types in the block must not themselves be composites
	// (except vectors and matrices), and all types must be the same.
	// The name of the uniform will be the same as the interface block name.
	void flatten_interface_block(uint32_t id);

	// Returns a set of all global variables which are statically accessed
	// by the control flow graph from the current entry point.
	// Only variables which change the interface for a shader are returned, that is,
	// variables with storage class of Input, Output, Uniform, UniformConstant, PushConstant and AtomicCounter
	// storage classes are returned.
	//
	// To use the returned set as the filter for which variables are used during compilation,
	// this set can be moved to set_enabled_interface_variables().
	std::unordered_set<uint32_t> get_active_interface_variables() const;

	// Sets the interface variables which are used during compilation.
	// By default, all variables are used.
	// Once set, compile() will only consider the set in active_variables.
	void set_enabled_interface_variables(std::unordered_set<uint32_t> active_variables);

	// Query shader resources, use ids with reflection interface to modify or query binding points, etc.
	ShaderResources get_shader_resources() const;

	// Query shader resources, but only return the variables which are part of active_variables.
	// E.g.: get_shader_resources(get_active_variables()) to only return the variables which are statically
	// accessed.
	ShaderResources get_shader_resources(const std::unordered_set<uint32_t> &active_variables) const;

	// Remapped variables are considered built-in variables and a backend will
	// not emit a declaration for this variable.
	// This is mostly useful for making use of builtins which are dependent on extensions.
	void set_remapped_variable_state(uint32_t id, bool remap_enable);
	bool get_remapped_variable_state(uint32_t id) const;

	// For subpassInput variables which are remapped to plain variables,
	// the number of components in the remapped
	// variable must be specified as the backing type of subpass inputs are opaque.
	void set_subpass_input_remapped_components(uint32_t id, uint32_t components);
	uint32_t get_subpass_input_remapped_components(uint32_t id) const;

	// All operations work on the current entry point.
	// Entry points can be swapped out with set_entry_point().
	// Entry points should be set right after the constructor completes as some reflection functions traverse the graph from the entry point.
	// Resource reflection also depends on the entry point.
	// By default, the current entry point is set to the first OpEntryPoint which appears in the SPIR-V module.
	std::vector<std::string> get_entry_points() const;
	void set_entry_point(const std::string &name);

	// Returns the internal data structure for entry points to allow poking around.
	const SPIREntryPoint &get_entry_point(const std::string &name) const;
	SPIREntryPoint &get_entry_point(const std::string &name);

	// Query and modify OpExecutionMode.
	uint64_t get_execution_mode_mask() const;
	void unset_execution_mode(spv::ExecutionMode mode);
	void set_execution_mode(spv::ExecutionMode mode, uint32_t arg0 = 0, uint32_t arg1 = 0, uint32_t arg2 = 0);

	// Gets argument for an execution mode (LocalSize, Invocations, OutputVertices).
	// For LocalSize, the index argument is used to select the dimension (X = 0, Y = 1, Z = 2).
	// For execution modes which do not have arguments, 0 is returned.
	uint32_t get_execution_mode_argument(spv::ExecutionMode mode, uint32_t index = 0) const;
	spv::ExecutionModel get_execution_model() const;

	// Analyzes all separate image and samplers used from the currently selected entry point,
	// and re-routes them all to a combined image sampler instead.
	// This is required to "support" separate image samplers in targets which do not natively support
	// this feature, like GLSL/ESSL.
	//
	// This must be called before compile() if such remapping is desired.
	// This call will add new sampled images to the SPIR-V,
	// so it will appear in reflection if get_shader_resources() is called after build_combined_image_samplers.
	//
	// If any image/sampler remapping was found, no separate image/samplers will appear in the decompiled output,
	// but will still appear in reflection.
	//
	// The resulting samplers will be void of any decorations like name, descriptor sets and binding points,
	// so this can be added before compile() if desired.
	//
	// Combined image samplers originating from this set are always considered active variables.
	void build_combined_image_samplers();

	// Gets a remapping for the combined image samplers.
	const std::vector<CombinedImageSampler> &get_combined_image_samplers() const
	{
		return combined_image_samplers;
	}

	// Set a new variable type remap callback.
	// The type remapping is designed to allow global interface variable to assume more special types.
	// A typical example here is to remap sampler2D into samplerExternalOES, which currently isn't supported
	// directly by SPIR-V.
	//
	// In compile() while emitting code,
	// for every variable that is declared, including function parameters, the callback will be called
	// and the API user has a chance to change the textual representation of the type used to declare the variable.
	// The API user can detect special patterns in names to guide the remapping.
	void set_variable_type_remap_callback(VariableTypeRemapCallback cb)
	{
		variable_remap_callback = std::move(cb);
	}

	// API for querying which specialization constants exist.
	// To modify a specialization constant before compile(), use get_constant(constant.id),
	// then update constants directly in the SPIRConstant data structure.
	// For composite types, the subconstants can be iterated over and modified.
	// constant_type is the SPIRType for the specialization constant,
	// which can be queried to determine which fields in the unions should be poked at.
	std::vector<SpecializationConstant> get_specialization_constants() const;
	SPIRConstant &get_constant(uint32_t id);
	const SPIRConstant &get_constant(uint32_t id) const;

	uint32_t get_current_id_bound() const
	{
		return uint32_t(ids.size());
	}

protected:
	const uint32_t *stream(const Instruction &instr) const
	{
		// If we're not going to use any arguments, just return nullptr.
		// We want to avoid case where we return an out of range pointer
		// that trips debug assertions on some platforms.
		if (!instr.length)
			return nullptr;

		if (instr.offset + instr.length > spirv.size())
			SPIRV_CROSS_THROW("Compiler::stream() out of range.");
		return &spirv[instr.offset];
	}
	std::vector<uint32_t> spirv;

	std::vector<Instruction> inst;
	std::vector<Variant> ids;
	std::vector<Meta> meta;

	SPIRFunction *current_function = nullptr;
	SPIRBlock *current_block = nullptr;
	std::vector<uint32_t> global_variables;
	std::vector<uint32_t> aliased_variables;
	std::unordered_set<uint32_t> active_interface_variables;
	bool check_active_interface_variables = false;

	// If our IDs are out of range here as part of opcodes, throw instead of
	// undefined behavior.
	template <typename T, typename... P>
	T &set(uint32_t id, P &&... args)
	{
		auto &var = variant_set<T>(ids.at(id), std::forward<P>(args)...);
		var.self = id;
		return var;
	}

	template <typename T>
	T &get(uint32_t id)
	{
		return variant_get<T>(ids.at(id));
	}

	template <typename T>
	T *maybe_get(uint32_t id)
	{
		if (ids.at(id).get_type() == T::type)
			return &get<T>(id);
		else
			return nullptr;
	}

	template <typename T>
	const T &get(uint32_t id) const
	{
		return variant_get<T>(ids.at(id));
	}

	template <typename T>
	const T *maybe_get(uint32_t id) const
	{
		if (ids.at(id).get_type() == T::type)
			return &get<T>(id);
		else
			return nullptr;
	}

	uint32_t entry_point = 0;
	// Normally, we'd stick SPIREntryPoint in ids array, but it conflicts with SPIRFunction.
	// Entry points can therefore be seen as some sort of meta structure.
	std::unordered_map<uint32_t, SPIREntryPoint> entry_points;
	const SPIREntryPoint &get_entry_point() const;
	SPIREntryPoint &get_entry_point();

	struct Source
	{
		uint32_t version = 0;
		bool es = false;
		bool known = false;

		Source() = default;
	} source;

	std::unordered_set<uint32_t> loop_blocks;
	std::unordered_set<uint32_t> continue_blocks;
	std::unordered_set<uint32_t> loop_merge_targets;
	std::unordered_set<uint32_t> selection_merge_targets;
	std::unordered_set<uint32_t> multiselect_merge_targets;

	virtual std::string to_name(uint32_t id, bool allow_alias = true);
	bool is_builtin_variable(const SPIRVariable &var) const;
	bool is_hidden_variable(const SPIRVariable &var, bool include_builtins = false) const;
	bool is_immutable(uint32_t id) const;
	bool is_member_builtin(const SPIRType &type, uint32_t index, spv::BuiltIn *builtin) const;
	bool is_scalar(const SPIRType &type) const;
	bool is_vector(const SPIRType &type) const;
	bool is_matrix(const SPIRType &type) const;
	const SPIRType &expression_type(uint32_t id) const;
	bool expression_is_lvalue(uint32_t id) const;
	bool variable_storage_is_aliased(const SPIRVariable &var);
	SPIRVariable *maybe_get_backing_variable(uint32_t chain);

	void register_read(uint32_t expr, uint32_t chain, bool forwarded);
	void register_write(uint32_t chain);

	inline bool is_continue(uint32_t next) const
	{
		return continue_blocks.find(next) != end(continue_blocks);
	}

	inline bool is_break(uint32_t next) const
	{
		return loop_merge_targets.find(next) != end(loop_merge_targets) ||
		       multiselect_merge_targets.find(next) != end(multiselect_merge_targets);
	}

	inline bool is_conditional(uint32_t next) const
	{
		return selection_merge_targets.find(next) != end(selection_merge_targets) &&
		       multiselect_merge_targets.find(next) == end(multiselect_merge_targets);
	}

	// Dependency tracking for temporaries read from variables.
	void flush_dependees(SPIRVariable &var);
	void flush_all_active_variables();
	void flush_all_atomic_capable_variables();
	void flush_all_aliased_variables();
	void register_global_read_dependencies(const SPIRBlock &func, uint32_t id);
	void register_global_read_dependencies(const SPIRFunction &func, uint32_t id);
	std::unordered_set<uint32_t> invalid_expressions;

	void update_name_cache(std::unordered_set<std::string> &cache, std::string &name);

	bool function_is_pure(const SPIRFunction &func);
	bool block_is_pure(const SPIRBlock &block);
	bool block_is_outside_flow_control_from_block(const SPIRBlock &from, const SPIRBlock &to);

	bool execution_is_branchless(const SPIRBlock &from, const SPIRBlock &to) const;
	bool execution_is_noop(const SPIRBlock &from, const SPIRBlock &to) const;
	SPIRBlock::ContinueBlockType continue_block_type(const SPIRBlock &continue_block) const;

	bool force_recompile = false;

	uint32_t type_struct_member_offset(const SPIRType &type, uint32_t index) const;
	uint32_t type_struct_member_array_stride(const SPIRType &type, uint32_t index) const;

	bool block_is_loop_candidate(const SPIRBlock &block, SPIRBlock::Method method) const;

	uint32_t increase_bound_by(uint32_t incr_amount);

	bool types_are_logically_equivalent(const SPIRType &a, const SPIRType &b) const;
	void inherit_expression_dependencies(uint32_t dst, uint32_t source);

	// For proper multiple entry point support, allow querying if an Input or Output
	// variable is part of that entry points interface.
	bool interface_variable_exists_in_entry_point(uint32_t id) const;

	std::vector<CombinedImageSampler> combined_image_samplers;

	void remap_variable_type_name(const SPIRType &type, const std::string &var_name, std::string &type_name) const
	{
		if (variable_remap_callback)
			variable_remap_callback(type, var_name, type_name);
	}

	void analyze_variable_scope(SPIRFunction &function);

private:
	void parse();
	void parse(const Instruction &i);

	// Used internally to implement various traversals for queries.
	struct OpcodeHandler
	{
		virtual ~OpcodeHandler() = default;

		// Return true if traversal should continue.
		// If false, traversal will end immediately.
		virtual bool handle(spv::Op opcode, const uint32_t *args, uint32_t length) = 0;

		virtual bool follow_function_call(const SPIRFunction &)
		{
			return true;
		}

		virtual void set_current_block(const SPIRBlock &)
		{
		}

		virtual bool begin_function_scope(const uint32_t *, uint32_t)
		{
			return true;
		}

		virtual bool end_function_scope(const uint32_t *, uint32_t)
		{
			return true;
		}
	};

	struct BufferAccessHandler : OpcodeHandler
	{
		BufferAccessHandler(const Compiler &compiler_, std::vector<BufferRange> &ranges_, uint32_t id_)
		    : compiler(compiler_)
		    , ranges(ranges_)
		    , id(id_)
		{
		}

		bool handle(spv::Op opcode, const uint32_t *args, uint32_t length) override;

		const Compiler &compiler;
		std::vector<BufferRange> &ranges;
		uint32_t id;

		std::unordered_set<uint32_t> seen;
	};

	struct InterfaceVariableAccessHandler : OpcodeHandler
	{
		InterfaceVariableAccessHandler(const Compiler &compiler_, std::unordered_set<uint32_t> &variables_)
		    : compiler(compiler_)
		    , variables(variables_)
		{
		}

		bool handle(spv::Op opcode, const uint32_t *args, uint32_t length) override;

		const Compiler &compiler;
		std::unordered_set<uint32_t> &variables;
	};

	struct CombinedImageSamplerHandler : OpcodeHandler
	{
		CombinedImageSamplerHandler(Compiler &compiler_)
		    : compiler(compiler_)
		{
		}
		bool handle(spv::Op opcode, const uint32_t *args, uint32_t length) override;
		bool begin_function_scope(const uint32_t *args, uint32_t length) override;
		bool end_function_scope(const uint32_t *args, uint32_t length) override;

		Compiler &compiler;

		// Each function in the call stack needs its own remapping for parameters so we can deduce which global variable each texture/sampler the parameter is statically bound to.
		std::stack<std::unordered_map<uint32_t, uint32_t>> parameter_remapping;
		std::stack<SPIRFunction *> functions;

		uint32_t remap_parameter(uint32_t id);
		void push_remap_parameters(const SPIRFunction &func, const uint32_t *args, uint32_t length);
		void pop_remap_parameters();
		void register_combined_image_sampler(SPIRFunction &caller, uint32_t texture_id, uint32_t sampler_id);
	};

	bool traverse_all_reachable_opcodes(const SPIRBlock &block, OpcodeHandler &handler) const;
	bool traverse_all_reachable_opcodes(const SPIRFunction &block, OpcodeHandler &handler) const;
	// This must be an ordered data structure so we always pick the same type aliases.
	std::vector<uint32_t> global_struct_cache;

	ShaderResources get_shader_resources(const std::unordered_set<uint32_t> *active_variables) const;

	VariableTypeRemapCallback variable_remap_callback;
};
}

#endif
