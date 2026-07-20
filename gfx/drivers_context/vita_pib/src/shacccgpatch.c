/*****************************************************************************
 * 
 *  Copyright (c) 2020 by SonicMastr <sonicmastr@gmail.com>
 * 
 *  This file is part of Pigs In A Blanket
 * 
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 * 
 ****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <psp2/types.h>
#include <psp2/shacccg.h>
#include <psp2/kernel/clib.h>
#include "../include/debug.h"

static SceShaccCgSourceFile source;
static const SceShaccCgCompileOutput *output = NULL;

static size_t logShaccCg(const SceShaccCgCompileOutput *output, char *shaderLog)
{
    for (int i = 0; i < output->diagnosticCount; ++i) {
		const SceShaccCgDiagnosticMessage *log = &output->diagnostics[i];
        char diagnosticLevel[8];
        switch (log->level)
        {
            case SCE_SHACCCG_DIAGNOSTIC_LEVEL_INFO:
                strcpy(diagnosticLevel, "INFO");
                break;
            case SCE_SHACCCG_DIAGNOSTIC_LEVEL_WARNING:
                strcpy(diagnosticLevel, "WARNING");
                break;
            case SCE_SHACCCG_DIAGNOSTIC_LEVEL_ERROR:
                strcpy(diagnosticLevel, "ERROR");
                break;
        }
        if (log->location)
            sprintf(shaderLog, "[%s] Line %d: %s\n", diagnosticLevel, log->location->lineNumber, log->message);
		else
            sprintf(shaderLog, "[%s] %s\n", diagnosticLevel, log->message); // Haven't ran into a case where this happens. May need confirmation.
	}
    return strlen(shaderLog);
}

static SceShaccCgSourceFile *openFile_callback(const char *filename, const SceShaccCgSourceLocation *includedFrom, const SceShaccCgCompileOptions *compileOptions, ScePVoid userData, const char **errorString)
{
    return &source;
}

int pglPlatformShaderCompiler_CustomPatch(int a1, void *shader)
{
    source.fileName = "";  // Crashes Otherwise. Name doesn't matter
    source.text = *(char **)(shader + 0x20);  // Shader Data Pointer
    LOG(source.text);
    source.size = *(int *)(shader + 0x24);  // Shader Data Size Pointer

    if (source.size >= 3 && !strncmp(source.text, "GXP", 3))
    {
        SceUInt8 *shaderData = malloc(source.size);
        memcpy(shaderData, source.text, source.size);
        *(SceUInt8**)(shader + 0x34) = shaderData;   // Compiled Shader Data Pointer
        *(SceInt32*)(shader + 0x38) = source.size;  // Compiled Shader Data Size Pointer
        *(int*)(&shader + 0x30) = 1;    // Flags Indicating Successful Compile
        *(int*)(&shader + 0x1d) = 2;
        
        return 1;
    }
    else {
        SceShaccCgCallbackList callback = {0};
        SceShaccCgCompileOptions options;

        sceShaccCgInitializeCallbackList(&callback, 1);
        callback.openFile = openFile_callback;
        sceShaccCgInitializeCompileOptions(&options);

        int shaderType = *(int *)(shader + 0x1c);
        switch (shaderType)
        {
            case 1:
                options.targetProfile = SCE_SHACCCG_PROFILE_VP;
                break;
            case 2:
                options.targetProfile = SCE_SHACCCG_PROFILE_FP;
                break;
        }

        options.mainSourceFile = source.fileName;
        options.entryFunctionName = "main";
        options.macroDefinitions = NULL;
        options.locale = 0; // 0 US, 1 JP
        options.useFx = 0;
        options.warningLevel = 3;
        options.optimizationLevel = 3;
        options.useFastmath = 1;
        options.useFastint = 1;
        options.warningsAsErrors = 0;
        options.useFastprecision = 0;
        options.pedantic = 0;
        options.performanceWarnings = 0;

        output = sceShaccCgCompileProgram(&options, &callback, 0);
    }

    char log[0x1024];
    size_t logLength = logShaccCg(output, log); // Prepare the Shader Log
    SceUInt8 *shaderLogData = malloc(logLength + 1);
    memcpy(shaderLogData, log, logLength + 1);
    *(SceInt32*)(shader + 0x2c) = logLength + 1; // Shader Log Length
    *(SceUInt8**)(shader + 0x28) = shaderLogData; // Shader Log Data

    if (output->programData)
    {
        SceUInt8 *shaderData = malloc(output->programSize);
        memcpy(shaderData, output->programData, output->programSize);
        *(SceUInt8**)(shader + 0x34) = shaderData;   // Compiled Shader Data Pointer
        *(SceInt32*)(shader + 0x38) = output->programSize;  // Compiled Shader Data Size Pointer
        *(int*)(&shader + 0x30) = 1;    // Flags Indicating Successful Compile
        *(int*)(&shader + 0x1d) = 2;

        sceShaccCgDestroyCompileOutput(output);
        return 1;
    }
    *(int*)(&shader + 0x30) = 0;
    return 0;
}
