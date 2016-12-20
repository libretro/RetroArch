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

#ifndef SPIRV_CROSS_GLSL_HPP
#define SPIRV_CROSS_GLSL_HPP

#include "spirv_cross.hpp"
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace spirv_cross
{
enum PlsFormat
{
	PlsNone = 0,

	PlsR11FG11FB10F,
	PlsR32F,
	PlsRG16F,
	PlsRGB10A2,
	PlsRGBA8,
	PlsRG16,

	PlsRGBA8I,
	PlsRG16I,

	PlsRGB10A2UI,
	PlsRGBA8UI,
	PlsRG16UI,
	PlsR32UI
};

struct PlsRemap
{
	uint32_t id;
	PlsFormat format;
};

class CompilerGLSL : public Compiler
{
public:
	struct Options
	{
		uint32_t version = 450;
		bool es = false;
		bool force_temporary = false;

		// If true, variables will be moved to their appropriate scope through CFG analysis.
		bool cfg_analysis = true;

		// If true, Vulkan GLSL features are used instead of GL-compatible features.
		// Mostly useful for debugging SPIR-V files.
		bool vulkan_semantics = false;

		enum Precision
		{
			DontCare,
			Lowp,
			Mediump,
			Highp
		};

		struct
		{
			// In vertex shaders, rewrite [0, w] depth (Vulkan/D3D style) to [-w, w] depth (GL style).
			bool fixup_clipspace = true;
		} vertex;

		struct
		{
			// Add precision mediump float in ES targets when emitting GLES source.
			// Add precision highp int in ES targets when emitting GLES source.
			Precision default_float_precision = Mediump;
			Precision default_int_precision = Highp;
		} fragment;
	};

	void remap_pixel_local_storage(std::vector<PlsRemap> inputs, std::vector<PlsRemap> outputs)
	{
		pls_inputs = std::move(inputs);
		pls_outputs = std::move(outputs);
		remap_pls_variables();
	}

	CompilerGLSL(std::vector<uint32_t> spirv_)
	    : Compiler(move(spirv_))
	{
		if (source.known)
		{
			options.es = source.es;
			options.version = source.version;
		}
	}

	const Options &get_options() const
	{
		return options;
	}
	void set_options(Options &opts)
	{
		options = opts;
	}

	std::string compile() override;

	// Returns the current string held in the conversion buffer. Useful for
	// capturing what has been converted so far when compile() throws an error.
	std::string get_partial_source();

	// Adds a line to be added right after #version in GLSL backend.
	// This is useful for enabling custom extensions which are outside the scope of SPIRV-Cross.
	// This can be combined with variable remapping.
	// A new-line will be added.
	//
	// While add_header_line() is a more generic way of adding arbitrary text to the header
	// of a GLSL file, require_extension() should be used when adding extensions since it will
	// avoid creating collisions with SPIRV-Cross generated extensions.
	//
	// Code added via add_header_line() is typically backend-specific.
	void add_header_line(const std::string &str);

	// Adds an extension which is required to run this shader, e.g.
	// require_extension("GL_KHR_my_extension");
	void require_extension(const std::string &ext);

protected:
	void reset();
	void emit_function(SPIRFunction &func, uint64_t return_flags);

	// Virtualize methods which need to be overridden by subclass targets like C++ and such.
	virtual void emit_function_prototype(SPIRFunction &func, uint64_t return_flags);
	virtual void emit_instruction(const Instruction &instr);
	virtual void emit_glsl_op(uint32_t result_type, uint32_t result_id, uint32_t op, const uint32_t *args,
	                          uint32_t count);
	virtual void emit_header();
	virtual void emit_sampled_image_op(uint32_t result_type, uint32_t result_id, uint32_t image_id, uint32_t samp_id);
	virtual void emit_texture_op(const Instruction &i);
	virtual std::string type_to_glsl(const SPIRType &type);
	virtual std::string builtin_to_glsl(spv::BuiltIn builtin);
	virtual std::string member_decl(const SPIRType &type, const SPIRType &member_type, uint32_t member);
	virtual std::string image_type_glsl(const SPIRType &type);
	virtual std::string constant_expression(const SPIRConstant &c);
	std::string constant_op_expression(const SPIRConstantOp &cop);
	virtual std::string constant_expression_vector(const SPIRConstant &c, uint32_t vector);
	virtual void emit_fixup();
	virtual std::string variable_decl(const SPIRType &type, const std::string &name);
	virtual std::string to_func_call_arg(uint32_t id);

	std::unique_ptr<std::ostringstream> buffer;

	template <typename T>
	inline void statement_inner(T &&t)
	{
		(*buffer) << std::forward<T>(t);
		statement_count++;
	}

	template <typename T, typename... Ts>
	inline void statement_inner(T &&t, Ts &&... ts)
	{
		(*buffer) << std::forward<T>(t);
		statement_count++;
		statement_inner(std::forward<Ts>(ts)...);
	}

	template <typename... Ts>
	inline void statement(Ts &&... ts)
	{
		if (redirect_statement)
			redirect_statement->push_back(join(std::forward<Ts>(ts)...));
		else
		{
			for (uint32_t i = 0; i < indent; i++)
				(*buffer) << "    ";

			statement_inner(std::forward<Ts>(ts)...);
			(*buffer) << '\n';
		}
	}

	template <typename... Ts>
	inline void statement_no_indent(Ts &&... ts)
	{
		auto old_indent = indent;
		indent = 0;
		statement(std::forward<Ts>(ts)...);
		indent = old_indent;
	}

	// Used for implementing continue blocks where
	// we want to obtain a list of statements we can merge
	// on a single line separated by comma.
	std::vector<std::string> *redirect_statement = nullptr;
	const SPIRBlock *current_continue_block = nullptr;

	void begin_scope();
	void end_scope();
	void end_scope_decl();
	void end_scope_decl(const std::string &decl);

	Options options;

	std::string type_to_array_glsl(const SPIRType &type);
	std::string to_array_size(const SPIRType &type, uint32_t index);
	uint32_t to_array_size_literal(const SPIRType &type, uint32_t index) const;
	std::string variable_decl(const SPIRVariable &variable);

	void add_local_variable_name(uint32_t id);
	void add_resource_name(uint32_t id);
	void add_member_name(SPIRType &type, uint32_t name);

	bool is_non_native_row_major_matrix(uint32_t id);
	bool member_is_non_native_row_major_matrix(const SPIRType &type, uint32_t index);
	virtual std::string convert_row_major_matrix(std::string exp_str);

	std::unordered_set<std::string> local_variable_names;
	std::unordered_set<std::string> resource_names;

	bool processing_entry_point = false;

	// Can be overriden by subclass backends for trivial things which
	// shouldn't need polymorphism.
	struct BackendVariations
	{
		std::string discard_literal = "discard";
		bool float_literal_suffix = false;
		bool double_literal_suffix = true;
		bool uint32_t_literal_suffix = true;
		bool long_long_literal_suffix = false;
		const char *basic_int_type = "int";
		const char *basic_uint_type = "uint";
		bool swizzle_is_function = false;
		bool shared_is_implied = false;
		bool flexible_member_array_supported = true;
		bool explicit_struct_type = false;
		bool use_initializer_list = false;
		bool native_row_major_matrix = true;
	} backend;

	void emit_struct(SPIRType &type);
	void emit_resources();
	void emit_buffer_block(const SPIRVariable &type);
	void emit_push_constant_block(const SPIRVariable &var);
	void emit_push_constant_block_vulkan(const SPIRVariable &var);
	void emit_push_constant_block_glsl(const SPIRVariable &var);
	void emit_interface_block(const SPIRVariable &type);
	void emit_block_chain(SPIRBlock &block);
	void emit_specialization_constant(const SPIRConstant &constant);
	std::string emit_continue_block(uint32_t continue_block);
	bool attempt_emit_loop_header(SPIRBlock &block, SPIRBlock::Method method);
	void emit_uniform(const SPIRVariable &var);
	void propagate_loop_dominators(const SPIRBlock &block);

	void branch(uint32_t from, uint32_t to);
	void branch(uint32_t from, uint32_t cond, uint32_t true_block, uint32_t false_block);
	void flush_phi(uint32_t from, uint32_t to);
	bool flush_phi_required(uint32_t from, uint32_t to);
	void flush_variable_declaration(uint32_t id);
	void flush_undeclared_variables(SPIRBlock &block);

	bool should_forward(uint32_t id);
	void emit_mix_op(uint32_t result_type, uint32_t id, uint32_t left, uint32_t right, uint32_t lerp);
	bool to_trivial_mix_op(const SPIRType &type, std::string &op, uint32_t left, uint32_t right, uint32_t lerp);
	void emit_quaternary_func_op(uint32_t result_type, uint32_t result_id, uint32_t op0, uint32_t op1, uint32_t op2,
	                             uint32_t op3, const char *op);
	void emit_trinary_func_op(uint32_t result_type, uint32_t result_id, uint32_t op0, uint32_t op1, uint32_t op2,
	                          const char *op);
	void emit_binary_func_op(uint32_t result_type, uint32_t result_id, uint32_t op0, uint32_t op1, const char *op);
	void emit_binary_func_op_cast(uint32_t result_type, uint32_t result_id, uint32_t op0, uint32_t op1, const char *op,
	                              SPIRType::BaseType input_type, bool skip_cast_if_equal_type);
	void emit_unary_func_op(uint32_t result_type, uint32_t result_id, uint32_t op0, const char *op);
	void emit_binary_op(uint32_t result_type, uint32_t result_id, uint32_t op0, uint32_t op1, const char *op);
	void emit_binary_op_cast(uint32_t result_type, uint32_t result_id, uint32_t op0, uint32_t op1, const char *op,
	                         SPIRType::BaseType input_type, bool skip_cast_if_equal_type);

	SPIRType binary_op_bitcast_helper(std::string &cast_op0, std::string &cast_op1, SPIRType::BaseType &input_type,
	                                  uint32_t op0, uint32_t op1, bool skip_cast_if_equal_type);

	void emit_unary_op(uint32_t result_type, uint32_t result_id, uint32_t op0, const char *op);
	bool expression_is_forwarded(uint32_t id);
	SPIRExpression &emit_op(uint32_t result_type, uint32_t result_id, const std::string &rhs, bool forward_rhs,
	                        bool suppress_usage_tracking = false);
	std::string access_chain(uint32_t base, const uint32_t *indices, uint32_t count, bool index_is_literal,
	                         bool chain_only = false);

	const char *index_to_swizzle(uint32_t index);
	std::string remap_swizzle(uint32_t result_type, uint32_t input_components, uint32_t expr);
	std::string declare_temporary(uint32_t type, uint32_t id);
	void append_global_func_args(const SPIRFunction &func, uint32_t index, std::vector<std::string> &arglist);
	std::string to_expression(uint32_t id);
	std::string to_enclosed_expression(uint32_t id);
	void strip_enclosed_expression(std::string &expr);
	std::string to_member_name(const SPIRType &type, uint32_t index);
	std::string type_to_glsl_constructor(const SPIRType &type);
	std::string argument_decl(const SPIRFunction::Parameter &arg);
	std::string to_qualifiers_glsl(uint32_t id);
	const char *to_precision_qualifiers_glsl(uint32_t id);
	const char *flags_to_precision_qualifiers_glsl(const SPIRType &type, uint64_t flags);
	const char *format_to_glsl(spv::ImageFormat format);
	std::string layout_for_member(const SPIRType &type, uint32_t index);
	std::string to_interpolation_qualifiers(uint64_t flags);
	uint64_t combined_decoration_for_member(const SPIRType &type, uint32_t index);
	std::string layout_for_variable(const SPIRVariable &variable);
	std::string to_combined_image_sampler(uint32_t image_id, uint32_t samp_id);
	bool skip_argument(uint32_t id) const;

	bool ssbo_is_std430_packing(const SPIRType &type);
	uint32_t type_to_std430_base_size(const SPIRType &type);
	uint32_t type_to_std430_alignment(const SPIRType &type, uint64_t flags);
	uint32_t type_to_std430_array_stride(const SPIRType &type, uint64_t flags);
	uint32_t type_to_std430_size(const SPIRType &type, uint64_t flags);

	std::string bitcast_glsl(const SPIRType &result_type, uint32_t arg);
	std::string bitcast_glsl_op(const SPIRType &result_type, const SPIRType &argument_type);
	std::string build_composite_combiner(const uint32_t *elems, uint32_t length);
	bool remove_duplicate_swizzle(std::string &op);
	bool remove_unity_swizzle(uint32_t base, std::string &op);

	// Can modify flags to remote readonly/writeonly if image type
	// and force recompile.
	bool check_atomic_image(uint32_t id);

	void replace_illegal_names();

	void replace_fragment_output(SPIRVariable &var);
	void replace_fragment_outputs();
	std::string legacy_tex_op(const std::string &op, const SPIRType &imgtype);

	uint32_t indent = 0;

	std::unordered_set<uint32_t> emitted_functions;

	// Usage tracking. If a temporary is used more than once, use the temporary instead to
	// avoid AST explosion when SPIRV is generated with pure SSA and doesn't write stuff to variables.
	std::unordered_map<uint32_t, uint32_t> expression_usage_counts;
	std::unordered_set<uint32_t> forced_temporaries;
	std::unordered_set<uint32_t> forwarded_temporaries;
	void track_expression_read(uint32_t id);

	std::unordered_set<std::string> forced_extensions;
	std::vector<std::string> header_lines;

	uint32_t statement_count;

	inline bool is_legacy() const
	{
		return (options.es && options.version < 300) || (!options.es && options.version < 130);
	}

	inline bool is_legacy_es() const
	{
		return options.es && options.version < 300;
	}

	inline bool is_legacy_desktop() const
	{
		return !options.es && options.version < 130;
	}

	bool args_will_forward(uint32_t id, const uint32_t *args, uint32_t num_args, bool pure);
	void register_call_out_argument(uint32_t id);
	void register_impure_function_call();

	// GL_EXT_shader_pixel_local_storage support.
	std::vector<PlsRemap> pls_inputs;
	std::vector<PlsRemap> pls_outputs;
	std::string pls_decl(const PlsRemap &variable);
	const char *to_pls_qualifiers_glsl(const SPIRVariable &variable);
	void emit_pls();
	void remap_pls_variables();

	void add_variable(std::unordered_set<std::string> &variables, uint32_t id);
	void check_function_call_constraints(const uint32_t *args, uint32_t length);
	void handle_invalid_expression(uint32_t id);
	void find_static_extensions();

	std::string emit_for_loop_initializers(const SPIRBlock &block);

	bool optimize_read_modify_write(const std::string &lhs, const std::string &rhs);
};
}

#endif
