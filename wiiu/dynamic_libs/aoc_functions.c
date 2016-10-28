/****************************************************************************
 * Copyright (C) 2015
 * by Dimok
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 ***************************************************************************/
#include "os_functions.h"
#include "aoc_functions.h"

unsigned int aoc_handle __attribute__((section(".data"))) = 0;

EXPORT_DECL(s32, AOC_Initialize, void);
EXPORT_DECL(s32, AOC_Finalize, void);
EXPORT_DECL(u32, AOC_CalculateWorkBufferSize, u32 num_titles);
EXPORT_DECL(s32, AOC_ListTitle, u32 * num_titles, void * titles, u32 max_titles, void * buffer, u32 buffer_size);
EXPORT_DECL(s32, AOC_OpenTitle, char* aoc_path, void * title, void * buffer, u32 buffer_size);
EXPORT_DECL(s32, AOC_CloseTitle, void * title);
EXPORT_DECL(s32, AOC_DeleteContent, u64 title_id, u16 contentIndexes[], u32 numberOfContent, void* buffer, u32 buffer_size);
EXPORT_DECL(s32, AOC_GetPurchaseInfo, u32 * bResult, u64 title_id, u16 contentIndexes[], u32 numberOfContent, void * buffer, u32 buffer_size);

void InitAcquireAoc(void)
{
    OSDynLoad_Acquire("nn_aoc.rpl", &aoc_handle);
}

void InitAocFunctionPointers(void)
{
    InitAcquireAoc();
    if(aoc_handle == 0)
        return;

    //! assigning those is not mandatory and it does not always work to load them
    OSDynLoad_FindExport(aoc_handle, 0, "AOC_Initialize", &AOC_Initialize);
    OSDynLoad_FindExport(aoc_handle, 0, "AOC_Finalize", &AOC_Finalize);
    OSDynLoad_FindExport(aoc_handle, 0, "AOC_CalculateWorkBufferSize", &AOC_CalculateWorkBufferSize);
    OSDynLoad_FindExport(aoc_handle, 0, "AOC_ListTitle", &AOC_ListTitle);
    OSDynLoad_FindExport(aoc_handle, 0, "AOC_OpenTitle", &AOC_OpenTitle);
    OSDynLoad_FindExport(aoc_handle, 0, "AOC_CloseTitle", &AOC_CloseTitle);
    OSDynLoad_FindExport(aoc_handle, 0, "AOC_DeleteContent", &AOC_DeleteContent);
    OSDynLoad_FindExport(aoc_handle, 0, "AOC_GetPurchaseInfo", &AOC_GetPurchaseInfo);
}
