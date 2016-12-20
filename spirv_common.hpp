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

#ifndef SPIRV_CROSS_COMMON_HPP
#define SPIRV_CROSS_COMMON_HPP

#include <cstdio>
#include <cstring>
#include <functional>
#include <sstream>

namespace spirv_cross
{

#ifdef SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS
#ifndef _MSC_VER
[[noreturn]]
#endif
    inline void
    report_and_abort(const std::string &msg)
{
#ifdef NDEBUG
	(void)msg;
#else
	fprintf(stderr, "There was a compiler error: %s\n", msg.c_str());
#endif
	abort();
}

#define SPIRV_CROSS_THROW(x) report_and_abort(x)
#else
class CompilerError : public std::runtime_error
{
public:
	CompilerError(const std::string &str)
	    : std::runtime_error(str)
	{
	}
};

#define SPIRV_CROSS_THROW(x) throw CompilerError(x)
#endif

namespace inner
{
template <typename T>
void join_helper(std::ostringstream &stream, T &&t)
{
	stream << std::forward<T>(t);
}

template <typename T, typename... Ts>
void join_helper(std::ostringstream &stream, T &&t, Ts &&... ts)
{
	stream << std::forward<T>(t);
	join_helper(stream, std::forward<Ts>(ts)...);
}
}

// Helper template to avoid lots of nasty string temporary munging.
template <typename... Ts>
std::string join(Ts &&... ts)
{
	std::ostringstream stream;
	inner::join_helper(stream, std::forward<Ts>(ts)...);
	return stream.str();
}

inline std::string merge(const std::vector<std::string> &list)
{
	std::string s;
	for (auto &elem : list)
	{
		s += elem;
		if (&elem != &list.back())
			s += ", ";
	}
	return s;
}

template <typename T>
inline std::string convert_to_string(T &&t)
{
	return std::to_string(std::forward<T>(t));
}

// Allow implementations to set a convenient standard precision
#ifndef SPIRV_CROSS_FLT_FMT
#define SPIRV_CROSS_FLT_FMT "%.32g"
#endif

inline std::string convert_to_string(float t)
{
	// std::to_string for floating point values is broken.
	// Fallback to something more sane.
	char buf[64];
	sprintf(buf, SPIRV_CROSS_FLT_FMT, t);
	// Ensure that the literal is float.
	if (!strchr(buf, '.') && !strchr(buf, 'e'))
		strcat(buf, ".0");
	return buf;
}

inline std::string convert_to_string(double t)
{
	// std::to_string for floating point values is broken.
	// Fallback to something more sane.
	char buf[64];
	sprintf(buf, SPIRV_CROSS_FLT_FMT, t);
	// Ensure that the literal is float.
	if (!strchr(buf, '.') && !strchr(buf, 'e'))
		strcat(buf, ".0");
	return buf;
}

struct Instruction
{
	Instruction(const std::vector<uint32_t> &spirv, uint32_t &index);

	uint16_t op;
	uint16_t count;
	uint32_t offset;
	uint32_t length;
};

// Helper for Variant interface.
struct IVariant
{
	virtual ~IVariant() = default;
	uint32_t self = 0;
};

enum Types
{
	TypeNone,
	TypeType,
	TypeVariable,
	TypeConstant,
	TypeFunction,
	TypeFunctionPrototype,
	TypePointer,
	TypeBlock,
	TypeExtension,
	TypeExpression,
	TypeConstantOp,
	TypeUndef
};

struct SPIRUndef : IVariant
{
	enum
	{
		type = TypeUndef
	};
	SPIRUndef(uint32_t basetype_)
	    : basetype(basetype_)
	{
	}
	uint32_t basetype;
};

struct SPIRConstantOp : IVariant
{
	enum
	{
		type = TypeConstantOp
	};

	SPIRConstantOp(uint32_t result_type, spv::Op op, const uint32_t *args, uint32_t length)
	    : opcode(op)
	    , arguments(args, args + length)
	    , basetype(result_type)
	{
	}

	spv::Op opcode;
	std::vector<uint32_t> arguments;
	uint32_t basetype;
};

struct SPIRType : IVariant
{
	enum
	{
		type = TypeType
	};

	enum BaseType
	{
		Unknown,
		Void,
		Boolean,
		Char,
		Int,
		UInt,
		Int64,
		UInt64,
		AtomicCounter,
		Float,
		Double,
		Struct,
		Image,
		SampledImage,
		Sampler
	};

	// Scalar/vector/matrix support.
	BaseType basetype = Unknown;
	uint32_t width = 0;
	uint32_t vecsize = 1;
	uint32_t columns = 1;

	// Arrays, support array of arrays by having a vector of array sizes.
	std::vector<uint32_t> array;

	// Array elements can be either specialization constants or specialization ops.
	// This array determines how to interpret the array size.
	// If an element is true, the element is a literal,
	// otherwise, it's an expression, which must be resolved on demand.
	// The actual size is not really known until runtime.
	std::vector<bool> array_size_literal;

	// Pointers
	bool pointer = false;
	spv::StorageClass storage = spv::StorageClassGeneric;

	std::vector<uint32_t> member_types;

	struct Image
	{
		uint32_t type;
		spv::Dim dim;
		bool depth;
		bool arrayed;
		bool ms;
		uint32_t sampled;
		spv::ImageFormat format;
	} image;

	// Structs can be declared multiple times if they are used as part of interface blocks.
	// We want to detect this so that we only emit the struct definition once.
	// Since we cannot rely on OpName to be equal, we need to figure out aliases.
	uint32_t type_alias = 0;

	// Used in backends to avoid emitting members with conflicting names.
	std::unordered_set<std::string> member_name_cache;
};

struct SPIRExtension : IVariant
{
	enum
	{
		type = TypeExtension
	};

	enum Extension
	{
		GLSL
	};

	SPIRExtension(Extension ext_)
	    : ext(ext_)
	{
	}

	Extension ext;
};

// SPIREntryPoint is not a variant since its IDs are used to decorate OpFunction,
// so in order to avoid conflicts, we can't stick them in the ids array.
struct SPIREntryPoint
{
	SPIREntryPoint(uint32_t self_, spv::ExecutionModel execution_model, std::string entry_name)
	    : self(self_)
	    , name(std::move(entry_name))
	    , model(execution_model)
	{
	}
	SPIREntryPoint() = default;

	uint32_t self = 0;
	std::string name;
	std::vector<uint32_t> interface_variables;

	uint64_t flags = 0;
	struct
	{
		uint32_t x = 0, y = 0, z = 0;
	} workgroup_size;
	uint32_t invocations = 0;
	uint32_t output_vertices = 0;
	spv::ExecutionModel model;
};

struct SPIRExpression : IVariant
{
	enum
	{
		type = TypeExpression
	};

	// Only created by the backend target to avoid creating tons of temporaries.
	SPIRExpression(std::string expr, uint32_t expression_type_, bool immutable_)
	    : expression(move(expr))
	    , expression_type(expression_type_)
	    , immutable(immutable_)
	{
	}

	// If non-zero, prepend expression with to_expression(base_expression).
	// Used in amortizing multiple calls to to_expression()
	// where in certain cases that would quickly force a temporary when not needed.
	uint32_t base_expression = 0;

	std::string expression;
	uint32_t expression_type = 0;

	// If this expression is a forwarded load,
	// allow us to reference the original variable.
	uint32_t loaded_from = 0;

	// If this expression will never change, we can avoid lots of temporaries
	// in high level source.
	// An expression being immutable can be speculative,
	// it is assumed that this is true almost always.
	bool immutable = false;

	// If this expression has been used while invalidated.
	bool used_while_invalidated = false;

	// A list of expressions which this expression depends on.
	std::vector<uint32_t> expression_dependencies;
};

struct SPIRFunctionPrototype : IVariant
{
	enum
	{
		type = TypeFunctionPrototype
	};

	SPIRFunctionPrototype(uint32_t return_type_)
	    : return_type(return_type_)
	{
	}

	uint32_t return_type;
	std::vector<uint32_t> parameter_types;
};

struct SPIRBlock : IVariant
{
	enum
	{
		type = TypeBlock
	};

	enum Terminator
	{
		Unknown,
		Direct, // Emit next block directly without a particular condition.

		Select, // Block ends with an if/else block.
		MultiSelect, // Block ends with switch statement.

		Return, // Block ends with return.
		Unreachable, // Noop
		Kill // Discard
	};

	enum Merge
	{
		MergeNone,
		MergeLoop,
		MergeSelection
	};

	enum Method
	{
		MergeToSelectForLoop,
		MergeToDirectForLoop
	};

	enum ContinueBlockType
	{
		ContinueNone,

		// Continue block is branchless and has at least one instruction.
		ForLoop,

		// Noop continue block.
		WhileLoop,

		// Continue block is conditional.
		DoWhileLoop,

		// Highly unlikely that anything will use this,
		// since it is really awkward/impossible to express in GLSL.
		ComplexLoop
	};

	enum
	{
		NoDominator = 0xffffffffu
	};

	Terminator terminator = Unknown;
	Merge merge = MergeNone;
	uint32_t next_block = 0;
	uint32_t merge_block = 0;
	uint32_t continue_block = 0;

	uint32_t return_value = 0; // If 0, return nothing (void).
	uint32_t condition = 0;
	uint32_t true_block = 0;
	uint32_t false_block = 0;
	uint32_t default_block = 0;

	std::vector<Instruction> ops;

	struct Phi
	{
		uint32_t local_variable; // flush local variable ...
		uint32_t parent; // If we're in from_block and want to branch into this block ...
		uint32_t function_variable; // to this function-global "phi" variable first.
	};

	// Before entering this block flush out local variables to magical "phi" variables.
	std::vector<Phi> phi_variables;

	// Declare these temporaries before beginning the block.
	// Used for handling complex continue blocks which have side effects.
	std::vector<std::pair<uint32_t, uint32_t>> declare_temporary;

	struct Case
	{
		uint32_t value;
		uint32_t block;
	};
	std::vector<Case> cases;

	// If we have tried to optimize code for this block but failed,
	// keep track of this.
	bool disable_block_optimization = false;

	// If the continue block is complex, fallback to "dumb" for loops.
	bool complex_continue = false;

	// The dominating block which this block might be within.
	// Used in continue; blocks to determine if we really need to write continue.
	uint32_t loop_dominator = 0;

	// All access to these variables are dominated by this block,
	// so before branching anywhere we need to make sure that we declare these variables.
	std::vector<uint32_t> dominated_variables;

	// These are variables which should be declared in a for loop header, if we
	// fail to use a classic for-loop,
	// we remove these variables, and fall back to regular variables outside the loop.
	std::vector<uint32_t> loop_variables;
};

struct SPIRFunction : IVariant
{
	enum
	{
		type = TypeFunction
	};

	SPIRFunction(uint32_t return_type_, uint32_t function_type_)
	    : return_type(return_type_)
	    , function_type(function_type_)
	{
	}

	struct Parameter
	{
		uint32_t type;
		uint32_t id;
		uint32_t read_count;
		uint32_t write_count;
	};

	// When calling a function, and we're remapping separate image samplers,
	// resolve these arguments into combined image samplers and pass them
	// as additional arguments in this order.
	// It gets more complicated as functions can pull in their own globals
	// and combine them with parameters,
	// so we need to distinguish if something is local parameter index
	// or a global ID.
	struct CombinedImageSamplerParameter
	{
		uint32_t id;
		uint32_t image_id;
		uint32_t sampler_id;
		bool global_image;
		bool global_sampler;
	};

	uint32_t return_type;
	uint32_t function_type;
	std::vector<Parameter> arguments;

	// Can be used by backends to add magic arguments.
	// Currently used by combined image/sampler implementation.

	std::vector<Parameter> shadow_arguments;
	std::vector<uint32_t> local_variables;
	uint32_t entry_block = 0;
	std::vector<uint32_t> blocks;
	std::vector<CombinedImageSamplerParameter> combined_parameters;

	void add_local_variable(uint32_t id)
	{
		local_variables.push_back(id);
	}

	void add_parameter(uint32_t parameter_type, uint32_t id)
	{
		// Arguments are read-only until proven otherwise.
		arguments.push_back({ parameter_type, id, 0u, 0u });
	}

	bool active = false;
	bool flush_undeclared = true;
	bool do_combined_parameters = true;
	bool analyzed_variable_scope = false;
};

struct SPIRVariable : IVariant
{
	enum
	{
		type = TypeVariable
	};

	SPIRVariable() = default;
	SPIRVariable(uint32_t basetype_, spv::StorageClass storage_, uint32_t initializer_ = 0)
	    : basetype(basetype_)
	    , storage(storage_)
	    , initializer(initializer_)
	{
	}

	uint32_t basetype = 0;
	spv::StorageClass storage = spv::StorageClassGeneric;
	uint32_t decoration = 0;
	uint32_t initializer = 0;

	std::vector<uint32_t> dereference_chain;
	bool compat_builtin = false;

	// If a variable is shadowed, we only statically assign to it
	// and never actually emit a statement for it.
	// When we read the variable as an expression, just forward
	// shadowed_id as the expression.
	bool statically_assigned = false;
	uint32_t static_expression = 0;

	// Temporaries which can remain forwarded as long as this variable is not modified.
	std::vector<uint32_t> dependees;
	bool forwardable = true;

	bool deferred_declaration = false;
	bool phi_variable = false;
	bool remapped_variable = false;
	uint32_t remapped_components = 0;

	// The block which dominates all access to this variable.
	uint32_t dominator = 0;
	// If true, this variable is a loop variable, when accessing the variable
	// outside a loop,
	// we should statically forward it.
	bool loop_variable = false;
	// Set to true while we're inside the for loop.
	bool loop_variable_enable = false;

	SPIRFunction::Parameter *parameter = nullptr;
};

struct SPIRConstant : IVariant
{
	enum
	{
		type = TypeConstant
	};

	union Constant {
		uint32_t u32;
		int32_t i32;
		float f32;

		uint64_t u64;
		int64_t i64;
		double f64;
	};

	struct ConstantVector
	{
		Constant r[4];
		uint32_t vecsize;
	};

	struct ConstantMatrix
	{
		ConstantVector c[4];
		uint32_t columns;
	};

	inline uint32_t scalar(uint32_t col = 0, uint32_t row = 0) const
	{
		return m.c[col].r[row].u32;
	}

	inline float scalar_f32(uint32_t col = 0, uint32_t row = 0) const
	{
		return m.c[col].r[row].f32;
	}

	inline int32_t scalar_i32(uint32_t col = 0, uint32_t row = 0) const
	{
		return m.c[col].r[row].i32;
	}

	inline double scalar_f64(uint32_t col = 0, uint32_t row = 0) const
	{
		return m.c[col].r[row].f64;
	}

	inline int64_t scalar_i64(uint32_t col = 0, uint32_t row = 0) const
	{
		return m.c[col].r[row].i64;
	}

	inline uint64_t scalar_u64(uint32_t col = 0, uint32_t row = 0) const
	{
		return m.c[col].r[row].u64;
	}

	inline const ConstantVector &vector() const
	{
		return m.c[0];
	}
	inline uint32_t vector_size() const
	{
		return m.c[0].vecsize;
	}
	inline uint32_t columns() const
	{
		return m.columns;
	}

	SPIRConstant(uint32_t constant_type_, const uint32_t *elements, uint32_t num_elements)
	    : constant_type(constant_type_)
	{
		subconstants.insert(end(subconstants), elements, elements + num_elements);
	}

	SPIRConstant(uint32_t constant_type_, uint32_t v0)
	    : constant_type(constant_type_)
	{
		m.c[0].r[0].u32 = v0;
		m.c[0].vecsize = 1;
		m.columns = 1;
	}

	SPIRConstant(uint32_t constant_type_, uint32_t v0, uint32_t v1)
	    : constant_type(constant_type_)
	{
		m.c[0].r[0].u32 = v0;
		m.c[0].r[1].u32 = v1;
		m.c[0].vecsize = 2;
		m.columns = 1;
	}

	SPIRConstant(uint32_t constant_type_, uint32_t v0, uint32_t v1, uint32_t v2)
	    : constant_type(constant_type_)
	{
		m.c[0].r[0].u32 = v0;
		m.c[0].r[1].u32 = v1;
		m.c[0].r[2].u32 = v2;
		m.c[0].vecsize = 3;
		m.columns = 1;
	}

	SPIRConstant(uint32_t constant_type_, uint32_t v0, uint32_t v1, uint32_t v2, uint32_t v3)
	    : constant_type(constant_type_)
	{
		m.c[0].r[0].u32 = v0;
		m.c[0].r[1].u32 = v1;
		m.c[0].r[2].u32 = v2;
		m.c[0].r[3].u32 = v3;
		m.c[0].vecsize = 4;
		m.columns = 1;
	}

	SPIRConstant(uint32_t constant_type_, uint64_t v0)
	    : constant_type(constant_type_)
	{
		m.c[0].r[0].u64 = v0;
		m.c[0].vecsize = 1;
		m.columns = 1;
	}

	SPIRConstant(uint32_t constant_type_, uint64_t v0, uint64_t v1)
	    : constant_type(constant_type_)
	{
		m.c[0].r[0].u64 = v0;
		m.c[0].r[1].u64 = v1;
		m.c[0].vecsize = 2;
		m.columns = 1;
	}

	SPIRConstant(uint32_t constant_type_, uint64_t v0, uint64_t v1, uint64_t v2)
	    : constant_type(constant_type_)
	{
		m.c[0].r[0].u64 = v0;
		m.c[0].r[1].u64 = v1;
		m.c[0].r[2].u64 = v2;
		m.c[0].vecsize = 3;
		m.columns = 1;
	}

	SPIRConstant(uint32_t constant_type_, uint64_t v0, uint64_t v1, uint64_t v2, uint64_t v3)
	    : constant_type(constant_type_)
	{
		m.c[0].r[0].u64 = v0;
		m.c[0].r[1].u64 = v1;
		m.c[0].r[2].u64 = v2;
		m.c[0].r[3].u64 = v3;
		m.c[0].vecsize = 4;
		m.columns = 1;
	}

	SPIRConstant(uint32_t constant_type_, const ConstantVector &vec0)
	    : constant_type(constant_type_)
	{
		m.columns = 1;
		m.c[0] = vec0;
	}

	SPIRConstant(uint32_t constant_type_, const ConstantVector &vec0, const ConstantVector &vec1)
	    : constant_type(constant_type_)
	{
		m.columns = 2;
		m.c[0] = vec0;
		m.c[1] = vec1;
	}

	SPIRConstant(uint32_t constant_type_, const ConstantVector &vec0, const ConstantVector &vec1,
	             const ConstantVector &vec2)
	    : constant_type(constant_type_)
	{
		m.columns = 3;
		m.c[0] = vec0;
		m.c[1] = vec1;
		m.c[2] = vec2;
	}

	SPIRConstant(uint32_t constant_type_, const ConstantVector &vec0, const ConstantVector &vec1,
	             const ConstantVector &vec2, const ConstantVector &vec3)
	    : constant_type(constant_type_)
	{
		m.columns = 4;
		m.c[0] = vec0;
		m.c[1] = vec1;
		m.c[2] = vec2;
		m.c[3] = vec3;
	}

	uint32_t constant_type;
	ConstantMatrix m;
	bool specialization = false; // If the constant is a specialization constant.

	// For composites which are constant arrays, etc.
	std::vector<uint32_t> subconstants;
};

class Variant
{
public:
	// MSVC 2013 workaround, we shouldn't need these constructors.
	Variant() = default;
	Variant(Variant &&other)
	{
		*this = std::move(other);
	}
	Variant &operator=(Variant &&other)
	{
		if (this != &other)
		{
			holder = move(other.holder);
			type = other.type;
			other.type = TypeNone;
		}
		return *this;
	}

	void set(std::unique_ptr<IVariant> val, uint32_t new_type)
	{
		holder = std::move(val);
		if (type != TypeNone && type != new_type)
			SPIRV_CROSS_THROW("Overwriting a variant with new type.");
		type = new_type;
	}

	template <typename T>
	T &get()
	{
		if (!holder)
			SPIRV_CROSS_THROW("nullptr");
		if (T::type != type)
			SPIRV_CROSS_THROW("Bad cast");
		return *static_cast<T *>(holder.get());
	}

	template <typename T>
	const T &get() const
	{
		if (!holder)
			SPIRV_CROSS_THROW("nullptr");
		if (T::type != type)
			SPIRV_CROSS_THROW("Bad cast");
		return *static_cast<const T *>(holder.get());
	}

	uint32_t get_type() const
	{
		return type;
	}
	bool empty() const
	{
		return !holder;
	}
	void reset()
	{
		holder.reset();
		type = TypeNone;
	}

private:
	std::unique_ptr<IVariant> holder;
	uint32_t type = TypeNone;
};

template <typename T>
T &variant_get(Variant &var)
{
	return var.get<T>();
}

template <typename T>
const T &variant_get(const Variant &var)
{
	return var.get<T>();
}

template <typename T, typename... P>
T &variant_set(Variant &var, P &&... args)
{
	auto uptr = std::unique_ptr<T>(new T(std::forward<P>(args)...));
	auto ptr = uptr.get();
	var.set(std::move(uptr), T::type);
	return *ptr;
}

struct Meta
{
	struct Decoration
	{
		std::string alias;
		std::string qualified_alias;
		uint64_t decoration_flags = 0;
		spv::BuiltIn builtin_type;
		uint32_t location = 0;
		uint32_t set = 0;
		uint32_t binding = 0;
		uint32_t offset = 0;
		uint32_t array_stride = 0;
		uint32_t input_attachment = 0;
		uint32_t spec_id = 0;
		bool builtin = false;
		bool per_instance = false;
	};

	Decoration decoration;
	std::vector<Decoration> members;
	uint32_t sampler = 0;
};

// A user callback that remaps the type of any variable.
// var_name is the declared name of the variable.
// name_of_type is the textual name of the type which will be used in the code unless written to by the callback.
using VariableTypeRemapCallback =
    std::function<void(const SPIRType &type, const std::string &var_name, std::string &name_of_type)>;
}

#endif
