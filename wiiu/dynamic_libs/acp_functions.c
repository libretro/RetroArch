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
#include "acp_functions.h"

unsigned int acp_handle __attribute__((section(".data"))) = 0;

EXPORT_DECL(void, GetMetaXml, void * _ACPMetaXml);

void InitAcquireACP(void)
{
    OSDynLoad_Acquire("nn_acp.rpl", &acp_handle);
}

void InitACPFunctionPointers(void)
{
    InitAcquireACP();
    OSDynLoad_FindExport(acp_handle,0,"GetMetaXml__Q2_2nn3acpFP11_ACPMetaXml",&GetMetaXml);
}
