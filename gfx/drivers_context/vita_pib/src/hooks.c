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
#include "../include/shacccgpatch.h"
#include "../include/hooks.h"
#include "../include/patches.h"
#include "../include/debug.h"

tai_hook_ref_t hookRef[NUM_HOOKS];
SceUID hook[NUM_HOOKS];
int customResolutionMode;
tai_module_info_t modInfo;
int systemMode, msaaEnabled = 0;

void loadHooks(PibOptions options)
{
    modInfo.size = sizeof(modInfo);
    taiGetModuleInfo("libScePiglet", &modInfo);
    if (options & PIB_SHACCCG) {
        hook[0] = taiHookFunctionOffset(&hookRef[0], modInfo.modid, 0, 0x32BB4, 1, pglPlatformShaderCompiler_CustomPatch);
        hook[1] = taiHookFunctionExport(&hookRef[1], modInfo.name, 0xB4FE1ABB, 0x919FBCB7, glGetBooleanv_shaderCompilerPatch);
        LOG("ShaccCg Patch: 0x%08x\nEnabled Shader Compiler Response: 0x%08x\n", hook[0], hook[1]);
    }
    hook[2] = taiHookFunctionOffset(&hookRef[2], modInfo.modid, 0, 0x39770, 1, _pglPlatformTextureUploadParams_patch);
    hook[3] = taiHookFunctionExport(&hookRef[3], modInfo.name, 0xB4FE1ABB, 0x4B86317A, eglCreateWindowSurface_resolutionPatch);
    LOG("Texture Upload Params Patch: 0x%08x\n", hook[2]);
    LOG("Resolution Patch: 0x%08x\n", hook[3]);
    if (options & PIB_GET_PROC_ADDR_CORE) {
        hook[4] = taiHookFunctionExport(&hookRef[4], modInfo.name, 0xB4FE1ABB, 0x249A431A, eglGetProcAddress_functionNamePatch);
        LOG("eglGetProcAddress Function Name Patch: 0x%08x\n", hook[4]);
    }
    hook[5] = taiHookFunctionExport(&hookRef[5], modInfo.name, 0xB4FE1ABB, 0x33A55EAB, eglGetConfigAttrib_intervalPatch);
    hook[6] = taiHookFunctionOffset(&hookRef[6], modInfo.modid, 0, 0x158F8, 1, pglDisplaySetSwapInterval_intervalPatch);
    hook[7] = taiHookFunctionImport(&hookRef[7], modInfo.name, 0x5ED8F994, 0x5795E898, sceDisplayWaitVblankStart_intervalPatch);
    LOG("Swap interval Patch: 0x%08x\nWaitVblankStart Patch: 0x%08X\n", hook[5], hook[6]);
    if (options & PIB_ENABLE_MSAA) {
        hook[8] = taiHookFunctionOffset(&hookRef[8], modInfo.modid, 0, 0x17d24, 1, pglMemoryAllocAlign_patch);
        hook[10] = taiHookFunctionOffset(&hookRef[10], modInfo.modid, 0, 0x33074, 1, pglPlatformSurfaceCreateWindow_detect);
    }
    if (options & PIB_ENABLE_MSAA) {
        msaaEnabled = 1;
        hook[16] = taiHookFunctionImport(&hookRef[16], modInfo.name, 0xF76B66BD, 0xED0F6E25, sceGxmColorSurfaceInit_msaaPatch);
        hook[17] = taiHookFunctionImport(&hookRef[17], modInfo.name, 0xF76B66BD, 0x207AF96B, sceGxmCreateRenderTarget_msaaPatch);
        hook[18] = taiHookFunctionImport(&hookRef[18], modInfo.name, 0xF76B66BD, 0xCA9D41D1, sceGxmDepthStencilSurfaceInit_msaaPatch);
        hook[19] = taiHookFunctionImport(&hookRef[19], modInfo.name, 0xF76B66BD, 0x4ED2E49D, sceGxmShaderPatcherCreateFragmentProgram_msaaPatch);
        LOG("MSAA ENABLED!\n");
    }
}

void releaseHooks(void)
{
    for (int i = 0; i < NUM_HOOKS; i++)
    {
        taiHookRelease(hook[i], hookRef[i]);
    }
}
