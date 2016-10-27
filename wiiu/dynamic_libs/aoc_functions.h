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
#ifndef __AOC_FUNCTIONS_H_
#define __AOC_FUNCTIONS_H_

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned int aoc_handle;

#define AOC_TITLE_SIZE              104

typedef struct {
    u64     title_ID;
    u32     group_ID;
    u16     version;
    char    path[88];
} AOC_TitleListType;


void InitAocFunctionPointers(void);
void InitAcquireAoc(void);

extern s32 (* AOC_Initialize)(void);
extern s32 (* AOC_Finalize)(void);
extern u32 (* AOC_CalculateWorkBufferSize)(u32 num_titles);
extern s32 (* AOC_ListTitle)(u32 * num_titles, void * titles, u32 max_titles, void * buffer, u32 buffer_size);
extern s32 (* AOC_OpenTitle)(char* aoc_path, void * title, void * buffer, u32 buffer_size);
extern s32 (* AOC_CloseTitle)(void * title);
extern s32 (* AOC_DeleteContent)(u64 title_id, u16 contentIndexes[], u32 numberOfContent, void * buffer, u32 buffer_size);
extern s32 (* AOC_GetPurchaseInfo)(u32 * bResult, u64 title_id, u16 contentIndexes[], u32 numberOfContent, void * buffer, u32 buffer_size);
#ifdef __cplusplus
}
#endif

#endif // __AOC_FUNCTIONS_H_
