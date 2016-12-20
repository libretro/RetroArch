/*
 * Copyright 2015-2016 The Brenwill Workshop Ltd.
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

#include "spirv_msl.hpp"
#include "GLSL.std.450.h"
#include <algorithm>
#include <numeric>

using namespace spv;
using namespace spirv_cross;
using namespace std;

CompilerMSL::CompilerMSL(vector<uint32_t> spirv_)
    : CompilerGLSL(move(spirv_))
{
	options.vertex.fixup_clipspace = false;
}

string CompilerMSL::compile(MSLConfiguration &msl_cfg, vector<MSLVertexAttr> *p_vtx_attrs,
                            std::vector<MSLResourceBinding> *p_res_bindings)
{
	pad_type_ids_by_pad_len.clear();

	msl_config = msl_cfg;

	vtx_attrs_by_location.clear();
	if (p_vtx_attrs)
		for (auto &va : *p_vtx_attrs)
			vtx_attrs_by_location[va.location] = &va;

	resource_bindings.clear();
	if (p_res_bindings)
	{
		resource_bindings.reserve(p_res_bindings->size());
		for (auto &rb : *p_res_bindings)
			resource_bindings.push_back(&rb);
	}

	extract_builtins();
	localize_global_variables();
	add_interface_structs();
	extract_global_variables_from_functions();

	// Do not deal with ES-isms like precision, older extensions and such.
	options.es = false;
	options.version = 120;
	backend.float_literal_suffix = false;
	backend.uint32_t_literal_suffix = true;
	backend.basic_int_type = "int";
	backend.basic_uint_type = "uint";
	backend.discard_literal = "discard_fragment()";
	backend.swizzle_is_function = false;
	backend.shared_is_implied = false;
	backend.native_row_major_matrix = false;

	uint32_t pass_count = 0;
	do
	{
		if (pass_count >= 3)
			SPIRV_CROSS_THROW("Over 3 compilation loops detected. Must be a bug!");

		reset();

		next_metal_resource_index = MSLResourceBinding(); // Start bindings at zero

		// Move constructor for this type is broken on GCC 4.9 ...
		buffer = unique_ptr<ostringstream>(new ostringstream());

		emit_header();
		emit_resources();
		emit_function_declarations();
		emit_function(get<SPIRFunction>(entry_point), 0);

		pass_count++;
	} while (force_recompile);

	return buffer->str();
}

string CompilerMSL::compile()
{
	MSLConfiguration default_msl_cfg;
	return compile(default_msl_cfg, nullptr, nullptr);
}

// Adds any builtins used by this shader to the builtin_vars collection
void CompilerMSL::extract_builtins()
{
	builtin_vars.clear();

	for (auto &id : ids)
	{
		if (id.get_type() == TypeVariable)
		{
			auto &var = id.get<SPIRVariable>();
			auto &dec = meta[var.self].decoration;

			if (dec.builtin)
				builtin_vars[dec.builtin_type] = var.self;
		}
	}

	auto &execution = get_entry_point();
	if (execution.model == ExecutionModelVertex)
	{
		if (!(builtin_vars[BuiltInVertexIndex] || builtin_vars[BuiltInVertexId]))
			add_builtin(BuiltInVertexIndex);

		if (!(builtin_vars[BuiltInInstanceIndex] || builtin_vars[BuiltInInstanceId]))
			add_builtin(BuiltInInstanceIndex);
	}
}

// Adds an appropriate built-in variable for the specified builtin type.
void CompilerMSL::add_builtin(BuiltIn builtin_type)
{

	// Add a new typed variable for this interface structure.
	uint32_t next_id = increase_bound_by(2);
	uint32_t ib_type_id = next_id++;
	auto &ib_type = set<SPIRType>(ib_type_id);
	ib_type.basetype = SPIRType::UInt;
	ib_type.storage = StorageClassInput;

	uint32_t ib_var_id = next_id++;
	set<SPIRVariable>(ib_var_id, ib_type_id, StorageClassInput, 0);
	set_decoration(ib_var_id, DecorationBuiltIn, builtin_type);
	set_name(ib_var_id, builtin_to_glsl(builtin_type));

	builtin_vars[builtin_type] = ib_var_id;
}

// Move the Private global variables to the entry function.
// Non-constant variables cannot have global scope in Metal.
void CompilerMSL::localize_global_variables()
{
	auto &entry_func = get<SPIRFunction>(entry_point);
	auto iter = global_variables.begin();
	while (iter != global_variables.end())
	{
		uint32_t gv_id = *iter;
		auto &gbl_var = get<SPIRVariable>(gv_id);
		if (gbl_var.storage == StorageClassPrivate)
		{
			entry_func.add_local_variable(gv_id);
			iter = global_variables.erase(iter);
		}
		else
		{
			iter++;
		}
	}
}

// For any global variable accessed directly by a function,
// extract that variable and add it as an argument to that function.
void CompilerMSL::extract_global_variables_from_functions()
{

	// Uniforms
	std::set<uint32_t> global_var_ids;
	for (auto &id : ids)
	{
		if (id.get_type() == TypeVariable)
		{
			auto &var = id.get<SPIRVariable>();
			if (var.storage == StorageClassInput || var.storage == StorageClassUniform ||
			    var.storage == StorageClassUniformConstant || var.storage == StorageClassPushConstant)
				global_var_ids.insert(var.self);
		}
	}

	std::set<uint32_t> added_arg_ids;
	std::set<uint32_t> processed_func_ids;
	extract_global_variables_from_function(entry_point, added_arg_ids, global_var_ids, processed_func_ids);
}

// MSL does not support the use of global variables for shader input content.
// For any global variable accessed directly by the specified function, extract that variable,
// add it as an argument to that function, and the arg to the added_arg_ids collection.
void CompilerMSL::extract_global_variables_from_function(uint32_t func_id, std::set<uint32_t> &added_arg_ids,
                                                         std::set<uint32_t> &global_var_ids,
                                                         std::set<uint32_t> &processed_func_ids)
{
	// Avoid processing a function more than once
	if (processed_func_ids.find(func_id) != processed_func_ids.end())
		return;

	processed_func_ids.insert(func_id);

	auto &func = get<SPIRFunction>(func_id);

	// Recursively establish global args added to functions on which we depend.
	for (auto block : func.blocks)
	{
		auto &b = get<SPIRBlock>(block);
		for (auto &i : b.ops)
		{
			auto ops = stream(i);
			auto op = static_cast<Op>(i.op);

			switch (op)
			{
			case OpLoad:
			case OpAccessChain:
			{
				uint32_t base_id = ops[2];
				if (global_var_ids.find(base_id) != global_var_ids.end())
					added_arg_ids.insert(base_id);
				break;
			}
			case OpFunctionCall:
			{
				uint32_t inner_func_id = ops[2];
				std::set<uint32_t> inner_func_args;
				extract_global_variables_from_function(inner_func_id, inner_func_args, global_var_ids,
				                                       processed_func_ids);
				added_arg_ids.insert(inner_func_args.begin(), inner_func_args.end());
				break;
			}

			default:
				break;
			}
		}
	}

	// Add the global variables as arguments to the function
	if (func_id != entry_point)
	{
		uint32_t next_id = increase_bound_by(uint32_t(added_arg_ids.size()));
		for (uint32_t arg_id : added_arg_ids)
		{
			uint32_t type_id = get<SPIRVariable>(arg_id).basetype;
			func.add_parameter(type_id, next_id);
			set<SPIRVariable>(next_id, type_id, StorageClassFunction);

			// Ensure both the existing and new variables have the same name, and the name is valid
			string vld_name = ensure_valid_name(to_name(arg_id), "v");
			set_name(arg_id, vld_name);
			set_name(next_id, vld_name);

			meta[next_id].decoration.qualified_alias = meta[arg_id].decoration.qualified_alias;
			next_id++;
		}
	}
}

// Adds any interface structure variables needed by this shader
void CompilerMSL::add_interface_structs()
{
	auto &execution = get_entry_point();

	stage_in_var_ids.clear();
	qual_pos_var_name = "";

	uint32_t var_id;
	if (execution.model == ExecutionModelVertex && !vtx_attrs_by_location.empty())
	{
		std::set<uint32_t> vtx_bindings;
		bind_vertex_attributes(vtx_bindings);
		for (uint32_t vb : vtx_bindings)
		{
			var_id = add_interface_struct(StorageClassInput, vb);
			if (var_id)
				stage_in_var_ids.push_back(var_id);
		}
	}
	else
	{
		var_id = add_interface_struct(StorageClassInput);
		if (var_id)
			stage_in_var_ids.push_back(var_id);
	}

	stage_out_var_id = add_interface_struct(StorageClassOutput);
}

// Iterate through the variables and populates each input vertex attribute variable
// from the binding info provided during compiler construction, matching by location.
void CompilerMSL::bind_vertex_attributes(std::set<uint32_t> &bindings)
{
	auto &execution = get_entry_point();

	if (execution.model == ExecutionModelVertex)
	{
		for (auto &id : ids)
		{
			if (id.get_type() == TypeVariable)
			{
				auto &var = id.get<SPIRVariable>();
				auto &type = get<SPIRType>(var.basetype);

				if (var.storage == StorageClassInput && interface_variable_exists_in_entry_point(var.self) &&
				    !is_hidden_variable(var) && type.pointer)
				{
					auto &dec = meta[var.self].decoration;
					MSLVertexAttr *p_va = vtx_attrs_by_location[dec.location];
					if (p_va)
					{
						dec.binding = p_va->msl_buffer;
						dec.offset = p_va->msl_offset;
						dec.array_stride = p_va->msl_stride;
						dec.per_instance = p_va->per_instance;

						// Mark the vertex attributes that were used.
						p_va->used_by_shader = true;
						bindings.insert(p_va->msl_buffer);
					}
				}
			}
		}
	}
}

// Add an the interface structure for the type of storage. For vertex inputs, each
// binding must have its own structure, and a structure is created for vtx_binding.
// For non-vertex input, and all outputs, the vtx_binding argument is ignored.
// Returns the ID of the newly added variable, or zero if no variable was added.
uint32_t CompilerMSL::add_interface_struct(StorageClass storage, uint32_t vtx_binding)
{
	auto &execution = get_entry_point();
	bool incl_builtins = (storage == StorageClassOutput);
	bool match_binding = (execution.model == ExecutionModelVertex) && (storage == StorageClassInput);

	// Accumulate the variables that should appear in the interface struct
	vector<SPIRVariable *> vars;
	for (auto &id : ids)
	{
		if (id.get_type() == TypeVariable)
		{
			auto &var = id.get<SPIRVariable>();
			auto &type = get<SPIRType>(var.basetype);
			auto &dec = meta[var.self].decoration;

			if (var.storage == storage && interface_variable_exists_in_entry_point(var.self) &&
			    !is_hidden_variable(var, incl_builtins) && (!match_binding || (vtx_binding == dec.binding)) &&
			    type.pointer)
			{
				vars.push_back(&var);
			}
		}
	}

	if (vars.empty())
	{
		return 0;
	} // Leave if no variables qualify

	// Add a new typed variable for this interface structure.
	// The initializer expression is allocated here, but populated when the function
	// declaraion is emitted, because it is cleared after each compilation pass.
	uint32_t next_id = increase_bound_by(3);
	uint32_t ib_type_id = next_id++;
	auto &ib_type = set<SPIRType>(ib_type_id);
	ib_type.basetype = SPIRType::Struct;
	ib_type.storage = storage;
	set_decoration(ib_type.self, DecorationBlock);

	uint32_t ib_var_id = next_id++;
	auto &var = set<SPIRVariable>(ib_var_id, ib_type_id, storage, 0);
	var.initializer = next_id++;

	// Set the binding of the variable and mark if packed (used only with vertex inputs)
	auto &var_dec = meta[ib_var_id].decoration;
	var_dec.binding = vtx_binding;

	// Track whether this is vertex input that is indexed, as opposed to stage_in
	bool is_indxd_vtx_input = (execution.model == ExecutionModelVertex && storage == StorageClassInput &&
	                           var_dec.binding != msl_config.vtx_attr_stage_in_binding);

	string ib_var_ref;

	if (storage == StorageClassInput)
	{
		ib_var_ref = stage_in_var_name;

		// Multiple vertex input bindings are available, so qualify each with the Metal buffer index
		if (execution.model == ExecutionModelVertex)
			ib_var_ref += convert_to_string(vtx_binding);
	}

	if (storage == StorageClassOutput)
	{
		ib_var_ref = stage_out_var_name;

		// Add the output interface struct as a local variable to the entry function,
		// and force the entry function to return the output interface struct from
		// any blocks that perform a function return.
		auto &entry_func = get<SPIRFunction>(entry_point);
		entry_func.add_local_variable(ib_var_id);
		for (auto &blk_id : entry_func.blocks)
		{
			auto &blk = get<SPIRBlock>(blk_id);
			if (blk.terminator == SPIRBlock::Return)
				blk.return_value = ib_var_id;
		}
	}

	set_name(ib_type_id, get_entry_point_name() + "_" + ib_var_ref);
	set_name(ib_var_id, ib_var_ref);

	size_t struct_size = 0;
	bool first_elem = true;
	for (auto p_var : vars)
	{
		// For index-accessed vertex attributes, copy the attribute characteristics to the parent
		// structure (all components have same vertex attribute characteristics except offset),
		// and add a reference to the vertex index builtin to the parent struct variable name.
		if (is_indxd_vtx_input && first_elem)
		{
			auto &elem_dec = meta[p_var->self].decoration;
			var_dec.binding = elem_dec.binding;
			var_dec.array_stride = elem_dec.array_stride;
			var_dec.per_instance = elem_dec.per_instance;
			ib_var_ref += "[" + get_vtx_idx_var_name(var_dec.per_instance) + "]";
			first_elem = false;
		}

		auto &type = get<SPIRType>(p_var->basetype);
		if (type.basetype == SPIRType::Struct)
		{
			// Flatten the struct members into the interface struct
			uint32_t i = 0;
			for (auto &member : type.member_types)
			{
				// If needed, add a padding member to the struct to align to the next member's offset.
				uint32_t mbr_offset = get_member_decoration(type.self, i, DecorationOffset);
				struct_size =
				    pad_to_offset(ib_type, is_indxd_vtx_input, (var_dec.offset + mbr_offset), uint32_t(struct_size));

				// Add a reference to the member to the interface struct.
				auto &membertype = get<SPIRType>(member);
				uint32_t ib_mbr_idx = uint32_t(ib_type.member_types.size());
				ib_type.member_types.push_back(membertype.self);

				// Give the member a name, and assign it an offset within the struct.
				string mbr_name = ensure_valid_name(to_qualified_member_name(type, i), "m");
				set_member_name(ib_type.self, ib_mbr_idx, mbr_name);
				set_member_decoration(ib_type.self, ib_mbr_idx, DecorationOffset, uint32_t(struct_size));
				struct_size = get_declared_struct_size(ib_type);

				// Update the original variable reference to include the structure reference
				string qual_var_name = ib_var_ref + "." + mbr_name;
				set_member_qualified_name(type.self, i, qual_var_name);

				// Copy the variable location from the original variable to the member
				uint32_t locn = get_member_decoration(type.self, i, DecorationLocation);
				set_member_decoration(ib_type.self, ib_mbr_idx, DecorationLocation, locn);

				// Mark the member as builtin if needed
				BuiltIn builtin;
				if (is_member_builtin(type, i, &builtin))
				{
					set_member_decoration(ib_type.self, ib_mbr_idx, DecorationBuiltIn, builtin);
					if (builtin == BuiltInPosition)
						qual_pos_var_name = qual_var_name;
				}

				i++;
			}
		}
		else
		{
			// If needed, add a padding member to the struct to align to the next member's offset.
			struct_size = pad_to_offset(ib_type, is_indxd_vtx_input, var_dec.offset, uint32_t(struct_size));

			// Add a reference to the variable type to the interface struct.
			uint32_t ib_mbr_idx = uint32_t(ib_type.member_types.size());
			ib_type.member_types.push_back(type.self);

			// Give the member a name, and assign it an offset within the struct.
			string mbr_name = ensure_valid_name(to_name(p_var->self), "m");
			set_member_name(ib_type.self, ib_mbr_idx, mbr_name);
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationOffset, uint32_t(struct_size));
			struct_size = get_declared_struct_size(ib_type);

			// Update the original variable reference to include the structure reference
			string qual_var_name = ib_var_ref + "." + mbr_name;
			meta[p_var->self].decoration.qualified_alias = qual_var_name;

			// Copy the variable location from the original variable to the member
			auto &dec = meta[p_var->self].decoration;
			set_member_decoration(ib_type.self, ib_mbr_idx, DecorationLocation, dec.location);

			// Mark the member as builtin if needed
			if (is_builtin_variable(*p_var))
			{
				set_member_decoration(ib_type.self, ib_mbr_idx, DecorationBuiltIn, dec.builtin_type);
				if (dec.builtin_type == BuiltInPosition)
					qual_pos_var_name = qual_var_name;
			}
		}
	}

	// Sort the members of the interface structure by their offsets
	MemberSorter memberSorter(ib_type, meta[ib_type.self], MemberSorter::Offset);
	memberSorter.sort();

	return ib_var_id;
}

// Emits the file header info
void CompilerMSL::emit_header()
{
	for (auto &header : header_lines)
		statement(header);

	statement("#include <metal_stdlib>");
	statement("#include <simd/simd.h>");
	statement("");
	statement("using namespace metal;");
	statement("");
}

void CompilerMSL::emit_resources()
{

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

	// Output Uniform buffers and constants
	for (auto &id : ids)
	{
		if (id.get_type() == TypeVariable)
		{
			auto &var = id.get<SPIRVariable>();
			auto &type = get<SPIRType>(var.basetype);

			if (var.storage != StorageClassFunction && type.pointer &&
			    (type.storage == StorageClassUniform || type.storage == StorageClassUniformConstant ||
			     type.storage == StorageClassPushConstant) &&
			    !is_hidden_variable(var) && (meta[type.self].decoration.decoration_flags &
			                                 ((1ull << DecorationBlock) | (1ull << DecorationBufferBlock))))
			{
				emit_struct(type);
			}
		}
	}

	// Output interface blocks.
	for (uint32_t var_id : stage_in_var_ids)
		emit_interface_block(var_id);

	emit_interface_block(stage_out_var_id);

	// TODO: Consolidate and output loose uniforms into an input struct
}

// Override for MSL-specific syntax instructions
void CompilerMSL::emit_instruction(const Instruction &instruction)
{

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

	auto ops = stream(instruction);
	auto opcode = static_cast<Op>(instruction.op);

	switch (opcode)
	{

	// ALU
	case OpFMod:
		BFOP(fmod);
		break;

	// Comparisons
	case OpIEqual:
	case OpLogicalEqual:
	case OpFOrdEqual:
		BOP(==);
		break;

	case OpINotEqual:
	case OpLogicalNotEqual:
	case OpFOrdNotEqual:
		BOP(!=);
		break;

	case OpUGreaterThan:
	case OpSGreaterThan:
	case OpFOrdGreaterThan:
		BOP(>);
		break;

	case OpUGreaterThanEqual:
	case OpSGreaterThanEqual:
	case OpFOrdGreaterThanEqual:
		BOP(>=);
		break;

	case OpULessThan:
	case OpSLessThan:
	case OpFOrdLessThan:
		BOP(<);
		break;

	case OpULessThanEqual:
	case OpSLessThanEqual:
	case OpFOrdLessThanEqual:
		BOP(<=);
		break;

	// Derivatives
	case OpDPdx:
		UFOP(dfdx);
		break;

	case OpDPdy:
		UFOP(dfdy);
		break;

	case OpImageQuerySize:
	{
		auto &type = expression_type(ops[2]);
		uint32_t result_type = ops[0];
		uint32_t id = ops[1];

		if (type.basetype == SPIRType::Image)
		{
			string img_exp = to_expression(ops[2]);
			auto &img_type = type.image;
			switch (img_type.dim)
			{
			case Dim1D:
				if (img_type.arrayed)
					emit_op(result_type, id, join("uint2(", img_exp, ".get_width(), ", img_exp, ".get_array_size())"),
					        false);
				else
					emit_op(result_type, id, join(img_exp, ".get_width()"), true);
				break;

			case Dim2D:
			case DimCube:
				if (img_type.arrayed)
					emit_op(result_type, id, join("uint3(", img_exp, ".get_width(), ", img_exp, ".get_height(), ",
					                              img_exp, ".get_array_size())"),
					        false);
				else
					emit_op(result_type, id, join("uint2(", img_exp, ".get_width(), ", img_exp, ".get_height())"),
					        false);
				break;

			case Dim3D:
				emit_op(result_type, id,
				        join("uint3(", img_exp, ".get_width(), ", img_exp, ".get_height(), ", img_exp, ".get_depth())"),
				        false);
				break;

			default:
				break;
			}
		}
		else
			SPIRV_CROSS_THROW("Invalid type for OpImageQuerySize.");
		break;
	}

	default:
		CompilerGLSL::emit_instruction(instruction);
		break;
	}
}

// Override for MSL-specific extension syntax instructions
void CompilerMSL::emit_glsl_op(uint32_t result_type, uint32_t id, uint32_t eop, const uint32_t *args, uint32_t count)
{
	GLSLstd450 op = static_cast<GLSLstd450>(eop);

	switch (op)
	{
	case GLSLstd450Atan2:
		emit_binary_func_op(result_type, id, args[0], args[1], "atan2");
		break;

	default:
		CompilerGLSL::emit_glsl_op(result_type, id, eop, args, count);
		break;
	}
}

// Emit a structure declaration for the specified interface variable.
void CompilerMSL::emit_interface_block(uint32_t ib_var_id)
{
	if (ib_var_id)
	{
		auto &ib_var = get<SPIRVariable>(ib_var_id);
		auto &ib_type = get<SPIRType>(ib_var.basetype);
		emit_struct(ib_type);
	}
}

// Output a declaration statement for each function.
void CompilerMSL::emit_function_declarations()
{
	for (auto &id : ids)
		if (id.get_type() == TypeFunction)
		{
			auto &func = id.get<SPIRFunction>();
			if (func.self != entry_point)
				emit_function_prototype(func, true);
		}

	statement("");
}

void CompilerMSL::emit_function_prototype(SPIRFunction &func, uint64_t)
{
	emit_function_prototype(func, false);
}

// Emits the declaration signature of the specified function.
// If this is the entry point function, Metal-specific return value and function arguments are added.
void CompilerMSL::emit_function_prototype(SPIRFunction &func, bool is_decl)
{
	local_variable_names = resource_names;
	string decl;

	processing_entry_point = (func.self == entry_point);

	auto &type = get<SPIRType>(func.return_type);
	decl += func_type_decl(type);
	decl += " ";
	decl += clean_func_name(to_name(func.self));

	decl += "(";

	if (processing_entry_point)
	{
		decl += entry_point_args(!func.arguments.empty());

		// If entry point function has a output interface struct, set its initializer.
		// This is done at this late stage because the initialization expression is
		// cleared after each compilation pass.
		if (stage_out_var_id)
		{
			auto &so_var = get<SPIRVariable>(stage_out_var_id);
			auto &so_type = get<SPIRType>(so_var.basetype);
			set<SPIRExpression>(so_var.initializer, "{}", so_type.self, true);
		}
	}

	for (auto &arg : func.arguments)
	{
		add_local_variable_name(arg.id);

		bool is_uniform_struct = false;
		auto *var = maybe_get<SPIRVariable>(arg.id);
		if (var)
		{
			var->parameter = &arg; // Hold a pointer to the parameter so we can invalidate the readonly field if needed.

			// Check if this arg is one of the synthetic uniform args
			// created to handle uniform access inside the function
			auto &var_type = get<SPIRType>(var->basetype);
			is_uniform_struct =
			    ((var_type.basetype == SPIRType::Struct) &&
			     (var_type.storage == StorageClassUniform || var_type.storage == StorageClassUniformConstant ||
			      var_type.storage == StorageClassPushConstant));
		}

		decl += (is_uniform_struct ? "constant " : "thread ");
		decl += argument_decl(arg);

		// Manufacture automatic sampler arg for SampledImage texture
		auto &arg_type = get<SPIRType>(arg.type);
		if (arg_type.basetype == SPIRType::SampledImage)
			decl += ", thread const sampler& " + to_sampler_expression(arg.id);

		if (&arg != &func.arguments.back())
			decl += ", ";
	}

	decl += ")";
	statement(decl, (is_decl ? ";" : ""));
}

// Emit a texture operation
void CompilerMSL::emit_texture_op(const Instruction &i)
{
	auto ops = stream(i);
	auto op = static_cast<Op>(i.op);
	uint32_t length = i.length;

	if (i.offset + length > spirv.size())
		SPIRV_CROSS_THROW("Compiler::compile() opcode out of range.");

	uint32_t result_type = ops[0];
	uint32_t id = ops[1];
	uint32_t img = ops[2];
	uint32_t coord = ops[3];
	uint32_t comp = 0;
	bool gather = false;
	bool fetch = false;
	const uint32_t *opt = nullptr;

	switch (op)
	{
	case OpImageSampleDrefImplicitLod:
	case OpImageSampleDrefExplicitLod:
		opt = &ops[5];
		length -= 5;
		break;

	case OpImageSampleProjDrefImplicitLod:
	case OpImageSampleProjDrefExplicitLod:
		opt = &ops[5];
		length -= 5;
		break;

	case OpImageDrefGather:
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

	case OpImageFetch:
		fetch = true;
		opt = &ops[4];
		length -= 4;
		break;

	case OpImageSampleImplicitLod:
	case OpImageSampleExplicitLod:
	case OpImageSampleProjImplicitLod:
	case OpImageSampleProjExplicitLod:
	default:
		opt = &ops[4];
		length -= 4;
		break;
	}

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
		flags = *opt;
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

	auto &img_type = expression_type(img).image;

	// Texture reference
	string expr = to_expression(img);

	// Texture function and sampler
	if (fetch)
	{
		expr += ".read(";
	}
	else
	{
		expr += std::string(".") + (gather ? "gather" : "sample") + "(" + to_sampler_expression(img) + ", ";
	}

	// Add texture coordinates
	bool forward = should_forward(coord);
	auto coord_expr = to_enclosed_expression(coord);
	string tex_coords = coord_expr;
	string array_coord;

	switch (img_type.dim)
	{
	case spv::DimBuffer:
		break;
	case Dim1D:
		if (img_type.arrayed)
		{
			tex_coords = coord_expr + ".x";
			array_coord = coord_expr + ".y";
			remove_duplicate_swizzle(tex_coords);
			remove_duplicate_swizzle(array_coord);
		}
		else
		{
			tex_coords = coord_expr + ".x";
		}
		break;

	case Dim2D:
		if (msl_config.flip_frag_y)
		{
			string coord_x = coord_expr + ".x";
			remove_duplicate_swizzle(coord_x);
			string coord_y = coord_expr + ".y";
			remove_duplicate_swizzle(coord_y);
			tex_coords = "float2(" + coord_x + ", (1.0 - " + coord_y + "))";
		}
		else
		{
			tex_coords = coord_expr + ".xy";
			remove_duplicate_swizzle(tex_coords);
		}

		if (img_type.arrayed)
		{
			array_coord = coord_expr + ".z";
			remove_duplicate_swizzle(array_coord);
		}

		break;

	case Dim3D:
	case DimCube:
		if (msl_config.flip_frag_y)
		{
			string coord_x = coord_expr + ".x";
			remove_duplicate_swizzle(coord_x);
			string coord_y = coord_expr + ".y";
			remove_duplicate_swizzle(coord_y);
			string coord_z = coord_expr + ".z";
			remove_duplicate_swizzle(coord_z);
			tex_coords = "float3(" + coord_x + ", (1.0 - " + coord_y + "), " + coord_z + ")";
		}
		else
		{
			tex_coords = coord_expr + ".xyz";
			remove_duplicate_swizzle(tex_coords);
		}

		if (img_type.arrayed)
		{
			array_coord = coord_expr + ".w";
			remove_duplicate_swizzle(array_coord);
		}

		break;

	default:
		break;
	}
	expr += tex_coords;

	// Add texture array index
	if (!array_coord.empty())
		expr += ", " + array_coord;

	// LOD Options
	if (bias)
	{
		forward = forward && should_forward(bias);
		expr += ", bias(" + to_expression(bias) + ")";
	}

	if (lod)
	{
		forward = forward && should_forward(lod);
		if (fetch)
		{
			expr += ", " + to_expression(lod);
		}
		else
		{
			expr += ", level(" + to_expression(lod) + ")";
		}
	}

	if (grad_x || grad_y)
	{
		forward = forward && should_forward(grad_x);
		forward = forward && should_forward(grad_y);
		string grad_opt;
		switch (img_type.dim)
		{
		case Dim2D:
			grad_opt = "2d";
			break;
		case Dim3D:
			grad_opt = "3d";
			break;
		case DimCube:
			grad_opt = "cube";
			break;
		default:
			grad_opt = "unsupported_gradient_dimension";
			break;
		}
		expr += ", gradient" + grad_opt + "(" + to_expression(grad_x) + ", " + to_expression(grad_y) + ")";
	}

	// Add offsets
	string offset_expr;
	if (coffset)
	{
		forward = forward && should_forward(coffset);
		offset_expr = to_expression(coffset);
	}
	else if (offset)
	{
		forward = forward && should_forward(offset);
		offset_expr = to_expression(offset);
	}

	if (!offset_expr.empty())
	{
		switch (img_type.dim)
		{
		case Dim2D:
			if (msl_config.flip_frag_y)
			{
				string coord_x = offset_expr + ".x";
				remove_duplicate_swizzle(coord_x);
				string coord_y = offset_expr + ".y";
				remove_duplicate_swizzle(coord_y);
				offset_expr = "float2(" + coord_x + ", (1.0 - " + coord_y + "))";
			}
			else
			{
				offset_expr = offset_expr + ".xy";
				remove_duplicate_swizzle(offset_expr);
			}

			expr += ", " + offset_expr;
			break;

		case Dim3D:
			if (msl_config.flip_frag_y)
			{
				string coord_x = offset_expr + ".x";
				remove_duplicate_swizzle(coord_x);
				string coord_y = offset_expr + ".y";
				remove_duplicate_swizzle(coord_y);
				string coord_z = offset_expr + ".z";
				remove_duplicate_swizzle(coord_z);
				offset_expr = "float3(" + coord_x + ", (1.0 - " + coord_y + "), " + coord_z + ")";
			}
			else
			{
				offset_expr = offset_expr + ".xyz";
				remove_duplicate_swizzle(offset_expr);
			}

			expr += ", " + offset_expr;
			break;

		default:
			break;
		}
	}

	if (comp)
	{
		forward = forward && should_forward(comp);
		expr += ", " + to_expression(comp);
	}

	expr += ")";

	emit_op(result_type, id, expr, forward);
}

// Establish sampled image as expression object and assign the sampler to it.
void CompilerMSL::emit_sampled_image_op(uint32_t result_type, uint32_t result_id, uint32_t image_id, uint32_t samp_id)
{
	set<SPIRExpression>(result_id, to_expression(image_id), result_type, true);
	meta[result_id].sampler = samp_id;
}

// Returns a string representation of the ID, usable as a function arg.
// Manufacture automatic sampler arg for SampledImage texture.
string CompilerMSL::to_func_call_arg(uint32_t id)
{
	string arg_str = CompilerGLSL::to_func_call_arg(id);

	// Manufacture automatic sampler arg if the arg is a SampledImage texture.
	Variant &id_v = ids[id];
	if (id_v.get_type() == TypeVariable)
	{
		auto &var = id_v.get<SPIRVariable>();
		auto &type = get<SPIRType>(var.basetype);
		if (type.basetype == SPIRType::SampledImage)
			arg_str += ", " + to_sampler_expression(id);
	}

	return arg_str;
}

// If the ID represents a sampled image that has been assigned a sampler already,
// generate an expression for the sampler, otherwise generate a fake sampler name
// by appending a suffix to the expression constructed from the ID.
string CompilerMSL::to_sampler_expression(uint32_t id)
{
	uint32_t samp_id = meta[id].sampler;
	return samp_id ? to_expression(samp_id) : to_expression(id) + sampler_name_suffix;
}

// Called automatically at the end of the entry point function
void CompilerMSL::emit_fixup()
{
	auto &execution = get_entry_point();

	if ((execution.model == ExecutionModelVertex) && stage_out_var_id && !qual_pos_var_name.empty())
	{
		if (options.vertex.fixup_clipspace)
		{
			const char *suffix = backend.float_literal_suffix ? "f" : "";
			statement(qual_pos_var_name, ".z = 2.0", suffix, " * ", qual_pos_var_name, ".z - ", qual_pos_var_name,
			          ".w;", "    // Adjust clip-space for Metal");
		}

		if (msl_config.flip_vert_y)
			statement(qual_pos_var_name, ".y = -(", qual_pos_var_name, ".y);", "    // Invert Y-axis for Metal");
	}
}

// Returns a declaration for a structure member.
string CompilerMSL::member_decl(const SPIRType &type, const SPIRType &membertype, uint32_t index)
{
	return join(type_to_glsl(membertype), " ", to_member_name(type, index), type_to_array_glsl(membertype),
	            member_attribute_qualifier(type, index));
}

// Return a MSL qualifier for the specified function attribute member
string CompilerMSL::member_attribute_qualifier(const SPIRType &type, uint32_t index)
{
	auto &execution = get_entry_point();

	BuiltIn builtin;
	bool is_builtin = is_member_builtin(type, index, &builtin);

	// Vertex function inputs
	if (execution.model == ExecutionModelVertex && type.storage == StorageClassInput)
	{
		if (is_builtin)
		{
			switch (builtin)
			{
			case BuiltInVertexId:
			case BuiltInVertexIndex:
			case BuiltInInstanceId:
			case BuiltInInstanceIndex:
				return string(" [[") + builtin_qualifier(builtin) + "]]";

			default:
				return "";
			}
		}
		uint32_t locn = get_ordered_member_location(type.self, index);
		return string(" [[attribute(") + convert_to_string(locn) + ")]]";
	}

	// Vertex function outputs
	if (execution.model == ExecutionModelVertex && type.storage == StorageClassOutput)
	{
		if (is_builtin)
		{
			switch (builtin)
			{
			case BuiltInClipDistance:
				return " /* [[clip_distance]] built-in not yet supported under Metal. */";

			case BuiltInPointSize: // Must output only if really rendering points
				return msl_config.is_rendering_points ? (string(" [[") + builtin_qualifier(builtin) + "]]") : "";

			case BuiltInPosition:
			case BuiltInLayer:
				return string(" [[") + builtin_qualifier(builtin) + "]]";

			default:
				return "";
			}
		}
		uint32_t locn = get_ordered_member_location(type.self, index);
		return string(" [[user(locn") + convert_to_string(locn) + ")]]";
	}

	// Fragment function inputs
	if (execution.model == ExecutionModelFragment && type.storage == StorageClassInput)
	{
		if (is_builtin)
		{
			switch (builtin)
			{
			case BuiltInFrontFacing:
			case BuiltInPointCoord:
			case BuiltInFragCoord:
			case BuiltInSampleId:
			case BuiltInSampleMask:
			case BuiltInLayer:
				return string(" [[") + builtin_qualifier(builtin) + "]]";

			default:
				return "";
			}
		}
		uint32_t locn = get_ordered_member_location(type.self, index);
		return string(" [[user(locn") + convert_to_string(locn) + ")]]";
	}

	// Fragment function outputs
	if (execution.model == ExecutionModelFragment && type.storage == StorageClassOutput)
	{
		if (is_builtin)
		{
			switch (builtin)
			{
			case BuiltInSampleMask:
			case BuiltInFragDepth:
				return string(" [[") + builtin_qualifier(builtin) + "]]";

			default:
				return "";
			}
		}
		uint32_t locn = get_ordered_member_location(type.self, index);
		return string(" [[color(") + convert_to_string(locn) + ")]]";
	}

	return "";
}

// Returns the location decoration of the member with the specified index in the specified type.
// If the location of the member has been explicitly set, that location is used. If not, this
// function assumes the members are ordered in their location order, and simply returns the
// index as the location.
uint32_t CompilerMSL::get_ordered_member_location(uint32_t type_id, uint32_t index)
{
	auto &m = meta.at(type_id);
	if (index < m.members.size())
	{
		auto &dec = m.members[index];
		if (dec.decoration_flags & (1ull << DecorationLocation))
			return dec.location;
	}

	return index;
}

string CompilerMSL::constant_expression(const SPIRConstant &c)
{
	if (!c.subconstants.empty())
	{
		// Handles Arrays and structures.
		string res = "{";
		for (auto &elem : c.subconstants)
		{
			res += constant_expression(get<SPIRConstant>(elem));
			if (&elem != &c.subconstants.back())
				res += ", ";
		}
		res += "}";
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

// Returns the type declaration for a function, including the
// entry type if the current function is the entry point function
string CompilerMSL::func_type_decl(SPIRType &type)
{
	auto &execution = get_entry_point();
	// The regular function return type. If not processing the entry point function, that's all we need
	string return_type = type_to_glsl(type);
	if (!processing_entry_point)
		return return_type;

	// If an outgoing interface block has been defined, override the entry point return type
	if (stage_out_var_id)
	{
		auto &so_var = get<SPIRVariable>(stage_out_var_id);
		auto &so_type = get<SPIRType>(so_var.basetype);
		return_type = type_to_glsl(so_type);
	}

	// Prepend a entry type, based on the execution model
	string entry_type;
	switch (execution.model)
	{
	case ExecutionModelVertex:
		entry_type = "vertex";
		break;
	case ExecutionModelFragment:
		entry_type = (execution.flags & (1ull << ExecutionModeEarlyFragmentTests)) ?
		                 "fragment [[ early_fragment_tests ]]" :
		                 "fragment";
		break;
	case ExecutionModelGLCompute:
	case ExecutionModelKernel:
		entry_type = "kernel";
		break;
	default:
		entry_type = "unknown";
		break;
	}

	return entry_type + " " + return_type;
}

// Ensures the function name is not "main", which is illegal in MSL
string CompilerMSL::clean_func_name(string func_name)
{
	static std::string _clean_msl_main_func_name = "mmain";
	return (func_name == "main") ? _clean_msl_main_func_name : func_name;
}

// Returns a string containing a comma-delimited list of args for the entry point function
string CompilerMSL::entry_point_args(bool append_comma)
{
	auto &execution = get_entry_point();
	string ep_args;

	// Stage-in structures
	for (uint32_t var_id : stage_in_var_ids)
	{
		auto &var = get<SPIRVariable>(var_id);
		auto &type = get<SPIRType>(var.basetype);
		auto &dec = meta[var.self].decoration;

		bool use_stage_in =
		    (execution.model != ExecutionModelVertex || dec.binding == msl_config.vtx_attr_stage_in_binding);

		if (!ep_args.empty())
			ep_args += ", ";
		if (use_stage_in)
			ep_args += type_to_glsl(type) + " " + to_name(var.self) + " [[stage_in]]";
		else
			ep_args += "device " + type_to_glsl(type) + "* " + to_name(var.self) + " [[buffer(" +
			           convert_to_string(dec.binding) + ")]]";
	}

	// Uniforms
	for (auto &id : ids)
	{
		if (id.get_type() == TypeVariable)
		{
			auto &var = id.get<SPIRVariable>();
			auto &type = get<SPIRType>(var.basetype);

			if (is_hidden_variable(var, true))
				continue;

			if (var.storage == StorageClassUniform || var.storage == StorageClassUniformConstant ||
			    var.storage == StorageClassPushConstant)
			{
				switch (type.basetype)
				{
				case SPIRType::Struct:
					if (!ep_args.empty())
						ep_args += ", ";
					ep_args += "constant " + type_to_glsl(type) + "& " + to_name(var.self);
					ep_args += " [[buffer(" + convert_to_string(get_metal_resource_index(var, type.basetype)) + ")]]";
					break;
				case SPIRType::Sampler:
					if (!ep_args.empty())
						ep_args += ", ";
					ep_args += type_to_glsl(type) + " " + to_name(var.self);
					ep_args += " [[sampler(" + convert_to_string(get_metal_resource_index(var, type.basetype)) + ")]]";
					break;
				case SPIRType::Image:
					if (!ep_args.empty())
						ep_args += ", ";
					ep_args += type_to_glsl(type) + " " + to_name(var.self);
					ep_args += " [[texture(" + convert_to_string(get_metal_resource_index(var, type.basetype)) + ")]]";
					break;
				case SPIRType::SampledImage:
					if (!ep_args.empty())
						ep_args += ", ";
					ep_args += type_to_glsl(type) + " " + to_name(var.self);
					ep_args +=
					    " [[texture(" + convert_to_string(get_metal_resource_index(var, SPIRType::Image)) + ")]]";
					if (type.image.dim != DimBuffer)
					{
						ep_args += ", sampler " + to_sampler_expression(var.self);
						ep_args +=
						    " [[sampler(" + convert_to_string(get_metal_resource_index(var, SPIRType::Sampler)) + ")]]";
					}
					break;
				default:
					break;
				}
			}
			if (var.storage == StorageClassInput && is_builtin_variable(var))
			{
				if (!ep_args.empty())
					ep_args += ", ";
				BuiltIn bi_type = meta[var.self].decoration.builtin_type;
				ep_args += builtin_type_decl(bi_type) + " " + to_expression(var.self);
				ep_args += " [[" + builtin_qualifier(bi_type) + "]]";
			}
		}
	}

	if (!ep_args.empty() && append_comma)
		ep_args += ", ";

	return ep_args;
}

// Returns the Metal index of the resource of the specified type as used by the specified variable.
uint32_t CompilerMSL::get_metal_resource_index(SPIRVariable &var, SPIRType::BaseType basetype)
{
	auto &execution = get_entry_point();
	auto &var_dec = meta[var.self].decoration;
	uint32_t var_desc_set = (var.storage == StorageClassPushConstant) ? kPushConstDescSet : var_dec.set;
	uint32_t var_binding = (var.storage == StorageClassPushConstant) ? kPushConstBinding : var_dec.binding;

	// If a matching binding has been specified, find and use it
	for (auto p_res_bind : resource_bindings)
	{
		if (p_res_bind->stage == execution.model && p_res_bind->desc_set == var_desc_set &&
		    p_res_bind->binding == var_binding)
		{

			p_res_bind->used_by_shader = true;
			switch (basetype)
			{
			case SPIRType::Struct:
				return p_res_bind->msl_buffer;
			case SPIRType::Image:
				return p_res_bind->msl_texture;
			case SPIRType::Sampler:
				return p_res_bind->msl_sampler;
			default:
				return 0;
			}
		}
	}

	// If a binding has not been specified, revert to incrementing resource indices
	switch (basetype)
	{
	case SPIRType::Struct:
		return next_metal_resource_index.msl_buffer++;
	case SPIRType::Image:
		return next_metal_resource_index.msl_texture++;
	case SPIRType::Sampler:
		return next_metal_resource_index.msl_sampler++;
	default:
		return 0;
	}
}

// Returns the name of the entry point of this shader
string CompilerMSL::get_entry_point_name()
{
	return clean_func_name(to_name(entry_point));
}

// Returns the name of either the vertex index or instance index builtin
string CompilerMSL::get_vtx_idx_var_name(bool per_instance)
{
	BuiltIn builtin;
	uint32_t var_id;

	// Try modern builtin name first
	builtin = per_instance ? BuiltInInstanceIndex : BuiltInVertexIndex;
	var_id = builtin_vars[builtin];
	if (var_id)
		return to_expression(var_id);

	// Try legacy builtin name second
	builtin = per_instance ? BuiltInInstanceId : BuiltInVertexId;
	var_id = builtin_vars[builtin];
	if (var_id)
		return to_expression(var_id);

	return "missing_vtx_idx_var";
}

// If the struct contains indexed vertex input, and the offset is greater than the current
// size of the struct, appends a padding member to the struct, and returns the offset to
// use for the next member, which is the offset provided. Otherwise, no padding is added,
// and the struct size is returned.
uint32_t CompilerMSL::pad_to_offset(SPIRType &struct_type, bool is_indxd_vtx_input, uint32_t offset,
                                    uint32_t struct_size)
{
	if (!(is_indxd_vtx_input && offset > struct_size))
		return struct_size;

	auto &pad_type = get_pad_type(offset - struct_size);
	uint32_t mbr_idx = uint32_t(struct_type.member_types.size());
	struct_type.member_types.push_back(pad_type.self);
	set_member_name(struct_type.self, mbr_idx, ("pad" + convert_to_string(mbr_idx)));
	set_member_decoration(struct_type.self, mbr_idx, DecorationOffset, struct_size);
	return offset;
}

// Returns a char array type suitable for use as a padding member in a packed struct
SPIRType &CompilerMSL::get_pad_type(uint32_t pad_len)
{
	uint32_t pad_type_id = pad_type_ids_by_pad_len[pad_len];
	if (pad_type_id != 0)
		return get<SPIRType>(pad_type_id);

	pad_type_id = increase_bound_by(1);
	auto &ib_type = set<SPIRType>(pad_type_id);
	ib_type.storage = StorageClassGeneric;
	ib_type.basetype = SPIRType::Char;
	ib_type.width = 8;
	ib_type.array.push_back(pad_len);
	ib_type.array_size_literal.push_back(true);
	set_decoration(ib_type.self, DecorationArrayStride, pad_len);

	pad_type_ids_by_pad_len[pad_len] = pad_type_id;
	return ib_type;
}

string CompilerMSL::argument_decl(const SPIRFunction::Parameter &arg)
{
	auto &type = expression_type(arg.id);
	bool constref = !type.pointer || arg.write_count == 0;

	auto &var = get<SPIRVariable>(arg.id);
	return join(constref ? "const " : "", type_to_glsl(type), "& ", to_name(var.self), type_to_array_glsl(type));
}

// If we're currently in the entry point function, and the object
// has a qualified name, use it, otherwise use the standard name.
string CompilerMSL::to_name(uint32_t id, bool allow_alias)
{
	if (current_function && (current_function->self == entry_point))
	{
		string qual_name = meta.at(id).decoration.qualified_alias;
		if (!qual_name.empty())
			return qual_name;
	}
	return Compiler::to_name(id, allow_alias);
}

// Returns a name that combines the name of the struct with the name of the member, except for Builtins
string CompilerMSL::to_qualified_member_name(const SPIRType &type, uint32_t index)
{
	//Start with existing member name
	string mbr_name = to_member_name(type, index);

	// Don't qualify Builtin names because they are unique and are treated as such when building expressions
	if (is_member_builtin(type, index, nullptr))
		return mbr_name;

	// Strip any underscore prefix from member name
	size_t startPos = mbr_name.find_first_not_of("_");
	mbr_name = (startPos != std::string::npos) ? mbr_name.substr(startPos) : "";
	return join(to_name(type.self), "_", mbr_name);
}

// Ensures that the specified name is permanently usable by prepending a prefix
// if the first chars are _ and a digit, which indicate a transient name.
string CompilerMSL::ensure_valid_name(string name, string pfx)
{
	if (name.size() >= 2 && name[0] == '_' && isdigit(name[1]))
		return join(pfx, name);
	else
		return name;
}

// Returns an MSL string describing  the SPIR-V type
string CompilerMSL::type_to_glsl(const SPIRType &type)
{
	// Ignore the pointer type since GLSL doesn't have pointers.

	switch (type.basetype)
	{
	case SPIRType::Struct:
		// Need OpName lookup here to get a "sensible" name for a struct.
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

	if (is_scalar(type)) // Scalar builtin
	{
		switch (type.basetype)
		{
		case SPIRType::Boolean:
			return "bool";
		case SPIRType::Char:
			return "char";
		case SPIRType::Int:
			return (type.width == 16 ? "short" : "int");
		case SPIRType::UInt:
			return (type.width == 16 ? "ushort" : "uint");
		case SPIRType::AtomicCounter:
			return "atomic_uint";
		case SPIRType::Float:
			return (type.width == 16 ? "half" : "float");
		default:
			return "unknown_type";
		}
	}
	else if (is_vector(type)) // Vector builtin
	{
		switch (type.basetype)
		{
		case SPIRType::Boolean:
			return join("bool", type.vecsize);
		case SPIRType::Char:
			return join("char", type.vecsize);
			;
		case SPIRType::Int:
			return join((type.width == 16 ? "short" : "int"), type.vecsize);
		case SPIRType::UInt:
			return join((type.width == 16 ? "ushort" : "uint"), type.vecsize);
		case SPIRType::Float:
			return join((type.width == 16 ? "half" : "float"), type.vecsize);
		default:
			return "unknown_type";
		}
	}
	else
	{
		switch (type.basetype)
		{
		case SPIRType::Boolean:
		case SPIRType::Int:
		case SPIRType::UInt:
		case SPIRType::Float:
			return join((type.width == 16 ? "half" : "float"), type.columns, "x", type.vecsize);
		default:
			return "unknown_type";
		}
	}
}

// Returns an MSL string describing  the SPIR-V image type
string CompilerMSL::image_type_glsl(const SPIRType &type)
{
	string img_type_name;

	auto &img_type = type.image;
	if (img_type.depth)
	{
		switch (img_type.dim)
		{
		case spv::Dim2D:
			img_type_name += (img_type.ms ? "depth2d_ms" : (img_type.arrayed ? "depth2d_array" : "depth2d"));
			break;
		case spv::DimCube:
			img_type_name += (img_type.arrayed ? "depthcube_array" : "depthcube");
			break;
		default:
			img_type_name += "unknown_depth_texture_type";
			break;
		}
	}
	else
	{
		switch (img_type.dim)
		{
		case spv::Dim1D:
			img_type_name += (img_type.arrayed ? "texture1d_array" : "texture1d");
			break;
		case spv::DimBuffer:
		case spv::Dim2D:
			img_type_name += (img_type.ms ? "texture2d_ms" : (img_type.arrayed ? "texture2d_array" : "texture2d"));
			break;
		case spv::Dim3D:
			img_type_name += "texture3d";
			break;
		case spv::DimCube:
			img_type_name += (img_type.arrayed ? "texturecube_array" : "texturecube");
			break;
		default:
			img_type_name += "unknown_texture_type";
			break;
		}
	}

	// Append the pixel type
	auto &img_pix_type = get<SPIRType>(img_type.type);
	img_type_name += "<" + type_to_glsl(img_pix_type) + ">";

	return img_type_name;
}

// Returns an MSL string identifying the name of a SPIR-V builtin
string CompilerMSL::builtin_to_glsl(BuiltIn builtin)
{
	switch (builtin)
	{
	case BuiltInPosition:
		return qual_pos_var_name.empty() ? (stage_out_var_name + ".gl_Position") : qual_pos_var_name;
	case BuiltInPointSize:
		return (stage_out_var_name + ".gl_PointSize");
	case BuiltInVertexId:
		return "gl_VertexID";
	case BuiltInInstanceId:
		return "gl_InstanceID";
	case BuiltInVertexIndex:
		return "gl_VertexIndex";
	case BuiltInInstanceIndex:
		return "gl_InstanceIndex";
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

// Returns an MSL string attribute qualifer for a SPIR-V builtin
string CompilerMSL::builtin_qualifier(BuiltIn builtin)
{
	auto &execution = get_entry_point();

	switch (builtin)
	{
	// Vertex function in
	case BuiltInVertexId:
		return "vertex_id";
	case BuiltInVertexIndex:
		return "vertex_id";
	case BuiltInInstanceId:
		return "instance_id";
	case BuiltInInstanceIndex:
		return "instance_id";

	// Vertex function out
	case BuiltInClipDistance:
		return "clip_distance";
	case BuiltInPointSize:
		return "point_size";
	case BuiltInPosition:
		return "position";
	case BuiltInLayer:
		return "render_target_array_index";

	// Fragment function in
	case BuiltInFrontFacing:
		return "front_facing";
	case BuiltInPointCoord:
		return "point_coord";
	case BuiltInFragCoord:
		return "position";
	case BuiltInSampleId:
		return "sample_id";
	case BuiltInSampleMask:
		return "sample_mask";

	// Fragment function out
	case BuiltInFragDepth:
	{
		if (execution.flags & (1ull << ExecutionModeDepthGreater))
			return "depth(greater)";

		if (execution.flags & (1ull << ExecutionModeDepthLess))
			return "depth(less)";

		if (execution.flags & (1ull << ExecutionModeDepthUnchanged))
			return "depth(any)";
	}

	default:
		return "unsupported-built-in";
	}
}

// Returns an MSL string type declaration for a SPIR-V builtin
string CompilerMSL::builtin_type_decl(BuiltIn builtin)
{
	switch (builtin)
	{
	// Vertex function in
	case BuiltInVertexId:
		return "uint";
	case BuiltInVertexIndex:
		return "uint";
	case BuiltInInstanceId:
		return "uint";
	case BuiltInInstanceIndex:
		return "uint";

	// Vertex function out
	case BuiltInClipDistance:
		return "float";
	case BuiltInPointSize:
		return "float";
	case BuiltInPosition:
		return "float4";

	// Fragment function in
	case BuiltInFrontFacing:
		return "bool";
	case BuiltInPointCoord:
		return "float2";
	case BuiltInFragCoord:
		return "float4";
	case BuiltInSampleId:
		return "uint";
	case BuiltInSampleMask:
		return "uint";

	default:
		return "unsupported-built-in-type";
	}
}

// Returns the effective size of a buffer block struct member.
size_t CompilerMSL::get_declared_struct_member_size(const SPIRType &struct_type, uint32_t index) const
{
	auto &type = get<SPIRType>(struct_type.member_types[index]);
	auto dec_mask = get_member_decoration_mask(struct_type.self, index);
	return get_declared_type_size(type, dec_mask);
}

// Returns the effective size of a variable type.
size_t CompilerMSL::get_declared_type_size(const SPIRType &type) const
{
	return get_declared_type_size(type, get_decoration_mask(type.self));
}

// Returns the effective size of a variable type or member type,
// taking into consideration the specified mask of decorations.
size_t CompilerMSL::get_declared_type_size(const SPIRType &type, uint64_t dec_mask) const
{
	if (type.basetype == SPIRType::Struct)
		return get_declared_struct_size(type);

	switch (type.basetype)
	{
	case SPIRType::Unknown:
	case SPIRType::Void:
	case SPIRType::AtomicCounter:
	case SPIRType::Image:
	case SPIRType::SampledImage:
	case SPIRType::Sampler:
		SPIRV_CROSS_THROW("Querying size of object with opaque size.");
	default:
		break;
	}

	size_t component_size = type.width / 8;
	unsigned vecsize = type.vecsize;
	unsigned columns = type.columns;

	if (type.array.empty())
	{
		// Vectors.
		if (columns == 1)
			return vecsize * component_size;
		else
		{
			// Per SPIR-V spec, matrices must be tightly packed and aligned up for vec3 accesses.
			if ((dec_mask & (1ull << DecorationRowMajor)) && columns == 3)
				columns = 4;
			else if ((dec_mask & (1ull << DecorationColMajor)) && vecsize == 3)
				vecsize = 4;

			return vecsize * columns * component_size;
		}
	}
	else
	{
		// For arrays, we can use ArrayStride to get an easy check.
		// ArrayStride is part of the array type not OpMemberDecorate.
		auto &dec = meta[type.self].decoration;
		if (dec.decoration_flags & (1ull << DecorationArrayStride))
			return dec.array_stride * to_array_size_literal(type, uint32_t(type.array.size()) - 1);
		else
		{
			SPIRV_CROSS_THROW("Type does not have ArrayStride set.");
		}
	}
}

// Sort both type and meta member content based on builtin status (put builtins at end), then by location.
void MemberSorter::sort()
{
	// Create a temporary array of consecutive member indices and sort it base on
	// how the members should be reordered, based on builtin and location meta info.
	size_t mbr_cnt = type.member_types.size();
	vector<uint32_t> mbr_idxs(mbr_cnt);
	iota(mbr_idxs.begin(), mbr_idxs.end(), 0); // Fill with consecutive indices
	std::sort(mbr_idxs.begin(), mbr_idxs.end(), *this); // Sort member indices based on member locations

	// Move type and meta member info to the order defined by the sorted member indices.
	// This is done by creating temporary copies of both member types and meta, and then
	// copying back to the original content at the sorted indices.
	auto mbr_types_cpy = type.member_types;
	auto mbr_meta_cpy = meta.members;
	for (uint32_t mbr_idx = 0; mbr_idx < mbr_cnt; mbr_idx++)
	{
		type.member_types[mbr_idx] = mbr_types_cpy[mbr_idxs[mbr_idx]];
		meta.members[mbr_idx] = mbr_meta_cpy[mbr_idxs[mbr_idx]];
	}
}

// Sort first by builtin status (put builtins at end), then by location.
bool MemberSorter::operator()(uint32_t mbr_idx1, uint32_t mbr_idx2)
{
	auto &mbr_meta1 = meta.members[mbr_idx1];
	auto &mbr_meta2 = meta.members[mbr_idx2];
	if (mbr_meta1.builtin != mbr_meta2.builtin)
		return mbr_meta2.builtin;
	else
		switch (sort_aspect)
		{
		case Location:
			return mbr_meta1.location < mbr_meta2.location;
		case Offset:
			return mbr_meta1.offset < mbr_meta2.offset;
		default:
			return false;
		}
}
