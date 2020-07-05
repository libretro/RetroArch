/*
 * This file is part of vitaGL
 * Copyright 2017, 2018, 2019, 2020 Rinnegatamante
 * Copyright 2020 Asakura Reiko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "vitashark.h"
#include <stdlib.h>
#include "shacccg.h"

// Default path for SceShaccCg module location
#define DEFAULT_SHACCCG_PATH "ur0:/data/libshacccg.suprx"

static void (*shark_log_cb)(const char *msg, shark_log_level msg_level, int line) = NULL;
static shark_warn_level shark_warnings_level = SHARK_WARN_SILENT;

static SceUID shark_module_id = 0;
static uint8_t shark_initialized = 0;
static SceShaccCgCompileOutput *shark_output = NULL;
static SceShaccCgSourceFile shark_input;

// Dummy Open File callback
static SceShaccCgSourceFile *shark_open_file_cb(const char *fileName,
	const SceShaccCgSourceLocation *includedFrom,
	const SceShaccCgCompileOptions *compileOptions,
	const char **errorString)
{
	return &shark_input;
}

int shark_init(const char *path) {
	// Initializing sceShaccCg module
	if (!shark_initialized) {
		shark_module_id = sceKernelLoadStartModule(path ? path : DEFAULT_SHACCCG_PATH, 0, NULL, 0, NULL, NULL);
		if (shark_module_id < 0) return -1;
		sceShaccCgSetDefaultAllocator(malloc, free);
		shark_initialized = 1;
	}
	return 0;
}

void shark_end() {
	if (!shark_initialized) return;
	
	// Terminating sceShaccCg module
	sceKernelStopUnloadModule(shark_module_id, 0, NULL, 0, NULL, NULL);
	shark_initialized = 0;
}

void shark_install_log_cb(void (*cb)(const char *msg, shark_log_level msg_level, int line)) {
	shark_log_cb = cb;
}

void shark_set_warnings_level(shark_warn_level level) {
	// Changing current warnings level
	shark_warnings_level = level;
}

void shark_clear_output() {
	// Clearing sceShaccCg output
	if (shark_output) {
		sceShaccCgDestroyCompileOutput(shark_output);
		shark_output = NULL;
	}
}

SceGxmProgram *shark_compile_shader_extended(const char *src, uint32_t *size, shark_type type, shark_opt opt, int32_t use_fastmath, int32_t use_fastprecision, int32_t use_fastint) {
	if (!shark_initialized) return NULL;
	
	// Forcing usage for memory source for the shader to compile
	shark_input.fileName = "<built-in>";
	shark_input.text = src;
	shark_input.size = *size;
	
	// Properly configuring SceShaccCg with requqested settings
	SceShaccCgCompileOptions options = {0};
	options.mainSourceFile = shark_input.fileName;
	options.targetProfile = type;
	options.entryFunctionName = "main";
	options.macroDefinitions = NULL;
	options.useFx = 1;
	options.warningLevel = shark_warnings_level;
	options.optimizationLevel = opt;
	options.useFastmath = use_fastmath;
	options.useFastint = use_fastint;
	options.useFastprecision = use_fastprecision;
	options.pedantic = shark_warnings_level > SHARK_WARN_MEDIUM ? SHARK_ENABLE : SHARK_DISABLE;
	options.performanceWarnings = shark_warnings_level > SHARK_WARN_SILENT ? SHARK_ENABLE : SHARK_DISABLE;
	
	// Executing shader compilation
	SceShaccCgCallbackList callbacks = {0};
	sceShaccCgInitializeCallbackList(&callbacks, SCE_SHACCCG_TRIVIAL);
	callbacks.openFile = shark_open_file_cb;
	const SceShaccCgCompileOutput *shark_output = sceShaccCgCompileProgram(&options, &callbacks, 0);
	
	// Executing logging
	if (shark_log_cb) {
		for (int i = 0; i < shark_output->diagnosticCount; ++i) {
			const SceShaccCgDiagnosticMessage *log = &shark_output->diagnostics[i];
			shark_log_cb(log->message, log->level, log->location->lineNumber);
		}
	}
	
	// Returning output
	if (shark_output->programData) *size = shark_output->programSize;
	return (SceGxmProgram *)shark_output->programData;
}

SceGxmProgram *shark_compile_shader(const char *src, uint32_t *size, shark_type type) {
	return shark_compile_shader_extended(src, size, type, SHARK_OPT_DEFAULT, SHARK_DISABLE, SHARK_DISABLE, SHARK_DISABLE);
}
