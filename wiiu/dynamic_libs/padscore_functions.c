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
#include "padscore_functions.h"

EXPORT_DECL(void, KPADInit, void);
EXPORT_DECL(s32, WPADProbe, s32 chan, u32 * pad_type);
EXPORT_DECL(s32, WPADSetDataFormat, s32 chan, s32 format);
EXPORT_DECL(void, WPADEnableURCC, s32 enable);
EXPORT_DECL(void, WPADRead, s32 chan, void * data);
EXPORT_DECL(s32, KPADRead, s32 chan, void * data, u32 size);

void InitPadScoreFunctionPointers(void)
{
    unsigned int *funcPointer = 0;
    unsigned int padscore_handle;
    OSDynLoad_Acquire("padscore.rpl", &padscore_handle);

    OS_FIND_EXPORT(padscore_handle, KPADInit);
    OS_FIND_EXPORT(padscore_handle, WPADProbe);
    OS_FIND_EXPORT(padscore_handle, WPADSetDataFormat);
    OS_FIND_EXPORT(padscore_handle, WPADEnableURCC);
    OS_FIND_EXPORT(padscore_handle, WPADRead);
    OS_FIND_EXPORT(padscore_handle, KPADRead);

    KPADInit();
    WPADEnableURCC(1);
}

