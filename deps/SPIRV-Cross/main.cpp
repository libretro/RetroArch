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

#include "spirv_cpp.hpp"
#include "spirv_msl.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

using namespace spv;
using namespace spirv_cross;
using namespace std;

#ifdef SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS
#define THROW(x)                   \
	do                             \
	{                              \
		fprintf(stderr, "%s.", x); \
		exit(1);                   \
	} while (0)
#else
#define THROW(x) runtime_error(x)
#endif

struct CLIParser;
struct CLICallbacks
{
	void add(const char *cli, const function<void(CLIParser &)> &func)
	{
		callbacks[cli] = func;
	}
	unordered_map<string, function<void(CLIParser &)>> callbacks;
	function<void()> error_handler;
	function<void(const char *)> default_handler;
};

struct CLIParser
{
	CLIParser(CLICallbacks cbs_, int argc_, char *argv_[])
	    : cbs(move(cbs_))
	    , argc(argc_)
	    , argv(argv_)
	{
	}

	bool parse()
	{
#ifndef SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS
		try
#endif
		{
			while (argc && !ended_state)
			{
				const char *next = *argv++;
				argc--;

				if (*next != '-' && cbs.default_handler)
				{
					cbs.default_handler(next);
				}
				else
				{
					auto itr = cbs.callbacks.find(next);
					if (itr == ::end(cbs.callbacks))
					{
						THROW("Invalid argument");
					}

					itr->second(*this);
				}
			}

			return true;
		}
#ifndef SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS
		catch (...)
		{
			if (cbs.error_handler)
			{
				cbs.error_handler();
			}
			return false;
		}
#endif
	}

	void end()
	{
		ended_state = true;
	}

	uint32_t next_uint()
	{
		if (!argc)
		{
			THROW("Tried to parse uint, but nothing left in arguments");
		}

		uint32_t val = stoul(*argv);
		if (val > numeric_limits<uint32_t>::max())
		{
			THROW("next_uint() out of range");
		}

		argc--;
		argv++;

		return val;
	}

	double next_double()
	{
		if (!argc)
		{
			THROW("Tried to parse double, but nothing left in arguments");
		}

		double val = stod(*argv);

		argc--;
		argv++;

		return val;
	}

	const char *next_string()
	{
		if (!argc)
		{
			THROW("Tried to parse string, but nothing left in arguments");
		}

		const char *ret = *argv;
		argc--;
		argv++;
		return ret;
	}

	CLICallbacks cbs;
	int argc;
	char **argv;
	bool ended_state = false;
};

static vector<uint32_t> read_spirv_file(const char *path)
{
	FILE *file = fopen(path, "rb");
	if (!file)
	{
		fprintf(stderr, "Failed to open SPIRV file: %s\n", path);
		return {};
	}

	fseek(file, 0, SEEK_END);
	long len = ftell(file) / sizeof(uint32_t);
	rewind(file);

	vector<uint32_t> spirv(len);
	if (fread(spirv.data(), sizeof(uint32_t), len, file) != size_t(len))
		spirv.clear();

	fclose(file);
	return spirv;
}

static bool write_string_to_file(const char *path, const char *string)
{
	FILE *file = fopen(path, "w");
	if (!file)
	{
		fprintf(file, "Failed to write file: %s\n", path);
		return false;
	}

	fprintf(file, "%s", string);
	fclose(file);
	return true;
}

static void print_resources(const Compiler &compiler, const char *tag, const vector<Resource> &resources)
{
	fprintf(stderr, "%s\n", tag);
	fprintf(stderr, "=============\n\n");
	for (auto &res : resources)
	{
		auto &type = compiler.get_type(res.type_id);
		auto mask = compiler.get_decoration_mask(res.id);

		// If we don't have a name, use the fallback for the type instead of the variable
		// for SSBOs and UBOs since those are the only meaningful names to use externally.
		// Push constant blocks are still accessed by name and not block name, even though they are technically Blocks.
		bool is_push_constant = compiler.get_storage_class(res.id) == StorageClassPushConstant;
		bool is_block = (compiler.get_decoration_mask(type.self) &
		                 ((1ull << DecorationBlock) | (1ull << DecorationBufferBlock))) != 0;
		bool is_sized_block = is_block && (compiler.get_storage_class(res.id) == StorageClassUniform ||
		                                   compiler.get_storage_class(res.id) == StorageClassUniformConstant);
		uint32_t fallback_id = !is_push_constant && is_block ? res.base_type_id : res.id;

		uint32_t block_size = 0;
		if (is_sized_block)
			block_size = compiler.get_declared_struct_size(compiler.get_type(res.base_type_id));

		string array;
		for (auto arr : type.array)
			array = join("[", arr ? convert_to_string(arr) : "", "]") + array;

		fprintf(stderr, " ID %03u : %s%s", res.id,
		        !res.name.empty() ? res.name.c_str() : compiler.get_fallback_name(fallback_id).c_str(), array.c_str());

		if (mask & (1ull << DecorationLocation))
			fprintf(stderr, " (Location : %u)", compiler.get_decoration(res.id, DecorationLocation));
		if (mask & (1ull << DecorationDescriptorSet))
			fprintf(stderr, " (Set : %u)", compiler.get_decoration(res.id, DecorationDescriptorSet));
		if (mask & (1ull << DecorationBinding))
			fprintf(stderr, " (Binding : %u)", compiler.get_decoration(res.id, DecorationBinding));
		if (mask & (1ull << DecorationInputAttachmentIndex))
			fprintf(stderr, " (Attachment : %u)", compiler.get_decoration(res.id, DecorationInputAttachmentIndex));
		if (is_sized_block)
			fprintf(stderr, " (BlockSize : %u bytes)", block_size);
		fprintf(stderr, "\n");
	}
	fprintf(stderr, "=============\n\n");
}

static const char *execution_model_to_str(spv::ExecutionModel model)
{
	switch (model)
	{
	case spv::ExecutionModelVertex:
		return "vertex";
	case spv::ExecutionModelTessellationControl:
		return "tessellation control";
	case ExecutionModelTessellationEvaluation:
		return "tessellation evaluation";
	case ExecutionModelGeometry:
		return "geometry";
	case ExecutionModelFragment:
		return "fragment";
	case ExecutionModelGLCompute:
		return "compute";
	default:
		return "???";
	}
}

static void print_resources(const Compiler &compiler, const ShaderResources &res)
{
	uint64_t modes = compiler.get_execution_mode_mask();

	fprintf(stderr, "Entry points:\n");
	auto entry_points = compiler.get_entry_points();
	for (auto &e : entry_points)
		fprintf(stderr, "  %s (%s)\n", e.c_str(), execution_model_to_str(compiler.get_entry_point(e).model));
	fprintf(stderr, "\n");

	fprintf(stderr, "Execution modes:\n");
	for (unsigned i = 0; i < 64; i++)
	{
		if (!(modes & (1ull << i)))
			continue;

		auto mode = static_cast<ExecutionMode>(i);
		uint32_t arg0 = compiler.get_execution_mode_argument(mode, 0);
		uint32_t arg1 = compiler.get_execution_mode_argument(mode, 1);
		uint32_t arg2 = compiler.get_execution_mode_argument(mode, 2);

		switch (static_cast<ExecutionMode>(i))
		{
		case ExecutionModeInvocations:
			fprintf(stderr, "  Invocations: %u\n", arg0);
			break;

		case ExecutionModeLocalSize:
			fprintf(stderr, "  LocalSize: (%u, %u, %u)\n", arg0, arg1, arg2);
			break;

		case ExecutionModeOutputVertices:
			fprintf(stderr, "  OutputVertices: %u\n", arg0);
			break;

#define CHECK_MODE(m)                  \
	case ExecutionMode##m:             \
		fprintf(stderr, "  %s\n", #m); \
		break
			CHECK_MODE(SpacingEqual);
			CHECK_MODE(SpacingFractionalEven);
			CHECK_MODE(SpacingFractionalOdd);
			CHECK_MODE(VertexOrderCw);
			CHECK_MODE(VertexOrderCcw);
			CHECK_MODE(PixelCenterInteger);
			CHECK_MODE(OriginUpperLeft);
			CHECK_MODE(OriginLowerLeft);
			CHECK_MODE(EarlyFragmentTests);
			CHECK_MODE(PointMode);
			CHECK_MODE(Xfb);
			CHECK_MODE(DepthReplacing);
			CHECK_MODE(DepthGreater);
			CHECK_MODE(DepthLess);
			CHECK_MODE(DepthUnchanged);
			CHECK_MODE(LocalSizeHint);
			CHECK_MODE(InputPoints);
			CHECK_MODE(InputLines);
			CHECK_MODE(InputLinesAdjacency);
			CHECK_MODE(Triangles);
			CHECK_MODE(InputTrianglesAdjacency);
			CHECK_MODE(Quads);
			CHECK_MODE(Isolines);
			CHECK_MODE(OutputPoints);
			CHECK_MODE(OutputLineStrip);
			CHECK_MODE(OutputTriangleStrip);
			CHECK_MODE(VecTypeHint);
			CHECK_MODE(ContractionOff);

		default:
			break;
		}
	}
	fprintf(stderr, "\n");

	print_resources(compiler, "subpass inputs", res.subpass_inputs);
	print_resources(compiler, "inputs", res.stage_inputs);
	print_resources(compiler, "outputs", res.stage_outputs);
	print_resources(compiler, "textures", res.sampled_images);
	print_resources(compiler, "separate images", res.separate_images);
	print_resources(compiler, "separate samplers", res.separate_samplers);
	print_resources(compiler, "images", res.storage_images);
	print_resources(compiler, "ssbos", res.storage_buffers);
	print_resources(compiler, "ubos", res.uniform_buffers);
	print_resources(compiler, "push", res.push_constant_buffers);
	print_resources(compiler, "counters", res.atomic_counters);
}

static void print_push_constant_resources(const Compiler &compiler, const vector<Resource> &res)
{
	for (auto &block : res)
	{
		auto ranges = compiler.get_active_buffer_ranges(block.id);
		fprintf(stderr, "Active members in buffer: %s\n",
		        !block.name.empty() ? block.name.c_str() : compiler.get_fallback_name(block.id).c_str());

		fprintf(stderr, "==================\n\n");
		for (auto &range : ranges)
		{
			const auto &name = compiler.get_member_name(block.base_type_id, range.index);

			fprintf(stderr, "Member #%3u (%s): Offset: %4u, Range: %4u\n", range.index,
			        !name.empty() ? name.c_str() : compiler.get_fallback_member_name(range.index).c_str(),
			        unsigned(range.offset), unsigned(range.range));
		}
		fprintf(stderr, "==================\n\n");
	}
}

static void print_spec_constants(const Compiler &compiler)
{
	auto spec_constants = compiler.get_specialization_constants();
	fprintf(stderr, "Specialization constants\n");
	fprintf(stderr, "==================\n\n");
	for (auto &c : spec_constants)
		fprintf(stderr, "ID: %u, Spec ID: %u\n", c.id, c.constant_id);
	fprintf(stderr, "==================\n\n");
}

struct PLSArg
{
	PlsFormat format;
	string name;
};

struct Remap
{
	string src_name;
	string dst_name;
	unsigned components;
};

struct VariableTypeRemap
{
	string variable_name;
	string new_variable_type;
};

struct CLIArguments
{
	const char *input = nullptr;
	const char *output = nullptr;
	const char *cpp_interface_name = nullptr;
	uint32_t version = 0;
	bool es = false;
	bool set_version = false;
	bool set_es = false;
	bool dump_resources = false;
	bool force_temporary = false;
	bool flatten_ubo = false;
	bool fixup = false;
	vector<PLSArg> pls_in;
	vector<PLSArg> pls_out;
	vector<Remap> remaps;
	vector<string> extensions;
	vector<VariableTypeRemap> variable_type_remaps;
	string entry;

	uint32_t iterations = 1;
	bool cpp = false;
	bool metal = false;
	bool vulkan_semantics = false;
	bool remove_unused = false;
	bool cfg_analysis = true;
};

static void print_help()
{
	fprintf(stderr, "Usage: spirv-cross [--output <output path>] [SPIR-V file] [--es] [--no-es] [--no-cfg-analysis] "
	                "[--version <GLSL "
	                "version>] [--dump-resources] [--help] [--force-temporary] [--cpp] [--cpp-interface-name <name>] "
	                "[--metal] [--vulkan-semantics] [--flatten-ubo] [--fixup-clipspace] [--iterations iter] [--pls-in "
	                "format input-name] [--pls-out format output-name] [--remap source_name target_name components] "
	                "[--extension ext] [--entry name] [--remove-unused-variables] "
	                "[--remap-variable-type <variable_name> <new_variable_type>]\n");
}

static bool remap_generic(Compiler &compiler, const vector<Resource> &resources, const Remap &remap)
{
	auto itr =
	    find_if(begin(resources), end(resources), [&remap](const Resource &res) { return res.name == remap.src_name; });

	if (itr != end(resources))
	{
		compiler.set_remapped_variable_state(itr->id, true);
		compiler.set_name(itr->id, remap.dst_name);
		compiler.set_subpass_input_remapped_components(itr->id, remap.components);
		return true;
	}
	else
		return false;
}

static vector<PlsRemap> remap_pls(const vector<PLSArg> &pls_variables, const vector<Resource> &resources,
                                  const vector<Resource> *secondary_resources)
{
	vector<PlsRemap> ret;

	for (auto &pls : pls_variables)
	{
		bool found = false;
		for (auto &res : resources)
		{
			if (res.name == pls.name)
			{
				ret.push_back({ res.id, pls.format });
				found = true;
				break;
			}
		}

		if (!found && secondary_resources)
		{
			for (auto &res : *secondary_resources)
			{
				if (res.name == pls.name)
				{
					ret.push_back({ res.id, pls.format });
					found = true;
					break;
				}
			}
		}

		if (!found)
			fprintf(stderr, "Did not find stage input/output/target with name \"%s\".\n", pls.name.c_str());
	}

	return ret;
}

static PlsFormat pls_format(const char *str)
{
	if (!strcmp(str, "r11f_g11f_b10f"))
		return PlsR11FG11FB10F;
	else if (!strcmp(str, "r32f"))
		return PlsR32F;
	else if (!strcmp(str, "rg16f"))
		return PlsRG16F;
	else if (!strcmp(str, "rg16"))
		return PlsRG16;
	else if (!strcmp(str, "rgb10_a2"))
		return PlsRGB10A2;
	else if (!strcmp(str, "rgba8"))
		return PlsRGBA8;
	else if (!strcmp(str, "rgba8i"))
		return PlsRGBA8I;
	else if (!strcmp(str, "rgba8ui"))
		return PlsRGBA8UI;
	else if (!strcmp(str, "rg16i"))
		return PlsRG16I;
	else if (!strcmp(str, "rgb10_a2ui"))
		return PlsRGB10A2UI;
	else if (!strcmp(str, "rg16ui"))
		return PlsRG16UI;
	else if (!strcmp(str, "r32ui"))
		return PlsR32UI;
	else
		return PlsNone;
}

int main(int argc, char *argv[])
{
	CLIArguments args;
	CLICallbacks cbs;

	cbs.add("--help", [](CLIParser &parser) {
		print_help();
		parser.end();
	});
	cbs.add("--output", [&args](CLIParser &parser) { args.output = parser.next_string(); });
	cbs.add("--es", [&args](CLIParser &) {
		args.es = true;
		args.set_es = true;
	});
	cbs.add("--no-es", [&args](CLIParser &) {
		args.es = false;
		args.set_es = true;
	});
	cbs.add("--version", [&args](CLIParser &parser) {
		args.version = parser.next_uint();
		args.set_version = true;
	});
	cbs.add("--no-cfg-analysis", [&args](CLIParser &) { args.cfg_analysis = false; });
	cbs.add("--dump-resources", [&args](CLIParser &) { args.dump_resources = true; });
	cbs.add("--force-temporary", [&args](CLIParser &) { args.force_temporary = true; });
	cbs.add("--flatten-ubo", [&args](CLIParser &) { args.flatten_ubo = true; });
	cbs.add("--fixup-clipspace", [&args](CLIParser &) { args.fixup = true; });
	cbs.add("--iterations", [&args](CLIParser &parser) { args.iterations = parser.next_uint(); });
	cbs.add("--cpp", [&args](CLIParser &) { args.cpp = true; });
	cbs.add("--cpp-interface-name", [&args](CLIParser &parser) { args.cpp_interface_name = parser.next_string(); });
	cbs.add("--metal", [&args](CLIParser &) { args.metal = true; });
	cbs.add("--vulkan-semantics", [&args](CLIParser &) { args.vulkan_semantics = true; });
	cbs.add("--extension", [&args](CLIParser &parser) { args.extensions.push_back(parser.next_string()); });
	cbs.add("--entry", [&args](CLIParser &parser) { args.entry = parser.next_string(); });
	cbs.add("--remap", [&args](CLIParser &parser) {
		string src = parser.next_string();
		string dst = parser.next_string();
		uint32_t components = parser.next_uint();
		args.remaps.push_back({ move(src), move(dst), components });
	});

	cbs.add("--remap-variable-type", [&args](CLIParser &parser) {
		string var_name = parser.next_string();
		string new_type = parser.next_string();
		args.variable_type_remaps.push_back({ move(var_name), move(new_type) });
	});

	cbs.add("--pls-in", [&args](CLIParser &parser) {
		auto fmt = pls_format(parser.next_string());
		auto name = parser.next_string();
		args.pls_in.push_back({ move(fmt), move(name) });
	});
	cbs.add("--pls-out", [&args](CLIParser &parser) {
		auto fmt = pls_format(parser.next_string());
		auto name = parser.next_string();
		args.pls_out.push_back({ move(fmt), move(name) });
	});

	cbs.add("--remove-unused-variables", [&args](CLIParser &) { args.remove_unused = true; });

	cbs.default_handler = [&args](const char *value) { args.input = value; };
	cbs.error_handler = [] { print_help(); };

	CLIParser parser{ move(cbs), argc - 1, argv + 1 };
	if (!parser.parse())
	{
		return EXIT_FAILURE;
	}
	else if (parser.ended_state)
	{
		return EXIT_SUCCESS;
	}

	if (!args.input)
	{
		fprintf(stderr, "Didn't specify input file.\n");
		print_help();
		return EXIT_FAILURE;
	}

	unique_ptr<CompilerGLSL> compiler;

	bool combined_image_samplers = false;

	if (args.cpp)
	{
		compiler = unique_ptr<CompilerGLSL>(new CompilerCPP(read_spirv_file(args.input)));
		if (args.cpp_interface_name)
			static_cast<CompilerCPP *>(compiler.get())->set_interface_name(args.cpp_interface_name);
	}
	else if (args.metal)
		compiler = unique_ptr<CompilerMSL>(new CompilerMSL(read_spirv_file(args.input)));
	else
	{
		combined_image_samplers = !args.vulkan_semantics;
		compiler = unique_ptr<CompilerGLSL>(new CompilerGLSL(read_spirv_file(args.input)));
	}

	if (!args.variable_type_remaps.empty())
	{
		auto remap_cb = [&](const SPIRType &, const string &name, string &out) -> void {
			for (const VariableTypeRemap &remap : args.variable_type_remaps)
				if (name == remap.variable_name)
					out = remap.new_variable_type;
		};

		compiler->set_variable_type_remap_callback(move(remap_cb));
	}

	if (!args.entry.empty())
		compiler->set_entry_point(args.entry);

	if (!args.set_version && !compiler->get_options().version)
	{
		fprintf(stderr, "Didn't specify GLSL version and SPIR-V did not specify language.\n");
		print_help();
		return EXIT_FAILURE;
	}

	CompilerGLSL::Options opts = compiler->get_options();
	if (args.set_version)
		opts.version = args.version;
	if (args.set_es)
		opts.es = args.es;
	opts.force_temporary = args.force_temporary;
	opts.vulkan_semantics = args.vulkan_semantics;
	opts.vertex.fixup_clipspace = args.fixup;
	opts.cfg_analysis = args.cfg_analysis;
	compiler->set_options(opts);

	ShaderResources res;
	if (args.remove_unused)
	{
		auto active = compiler->get_active_interface_variables();
		res = compiler->get_shader_resources(active);
		compiler->set_enabled_interface_variables(move(active));
	}
	else
		res = compiler->get_shader_resources();

	if (args.flatten_ubo)
		for (auto &ubo : res.uniform_buffers)
			compiler->flatten_interface_block(ubo.id);

	auto pls_inputs = remap_pls(args.pls_in, res.stage_inputs, &res.subpass_inputs);
	auto pls_outputs = remap_pls(args.pls_out, res.stage_outputs, nullptr);
	compiler->remap_pixel_local_storage(move(pls_inputs), move(pls_outputs));

	for (auto &ext : args.extensions)
		compiler->require_extension(ext);

	for (auto &remap : args.remaps)
	{
		if (remap_generic(*compiler, res.stage_inputs, remap))
			continue;
		if (remap_generic(*compiler, res.stage_outputs, remap))
			continue;
		if (remap_generic(*compiler, res.subpass_inputs, remap))
			continue;
	}

	if (args.dump_resources)
	{
		print_resources(*compiler, res);
		print_push_constant_resources(*compiler, res.push_constant_buffers);
		print_spec_constants(*compiler);
	}

	if (combined_image_samplers)
	{
		compiler->build_combined_image_samplers();
		// Give the remapped combined samplers new names.
		for (auto &remap : compiler->get_combined_image_samplers())
		{
			compiler->set_name(remap.combined_id, join("SPIRV_Cross_Combined", compiler->get_name(remap.image_id),
			                                           compiler->get_name(remap.sampler_id)));
		}
	}

	string glsl;
	for (uint32_t i = 0; i < args.iterations; i++)
		glsl = compiler->compile();

	if (args.output)
		write_string_to_file(args.output, glsl.c_str());
	else
		printf("%s", glsl.c_str());
}
