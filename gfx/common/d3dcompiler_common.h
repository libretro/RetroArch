/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2018 - Ali Bouhlel
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <retro_inline.h>
#include <boolean.h>

#include "dxgi_common.h"
#include <d3dcommon.h>
#include <d3dcompiler.h>

/* auto-generated */

typedef ID3DBlob*                D3DBlob;
typedef ID3DDestructionNotifier* D3DDestructionNotifier;

#if !defined(__cplusplus) || defined(CINTERFACE)
static INLINE ULONG  D3DReleaseBlob(D3DBlob blob)
{
   return blob->lpVtbl->Release(blob);
}

static INLINE LPVOID D3DGetBufferPointer(D3DBlob blob)
{
   return blob->lpVtbl->GetBufferPointer(blob);
}
static INLINE SIZE_T D3DGetBufferSize(D3DBlob blob)
{
   return blob->lpVtbl->GetBufferSize(blob);
}

static INLINE ULONG  D3DReleaseDestructionNotifier(
      D3DDestructionNotifier destruction_notifier)
{
   return destruction_notifier->lpVtbl->Release(destruction_notifier);
}

static INLINE HRESULT D3DRegisterDestructionCallback(
      D3DDestructionNotifier   destruction_notifier,
      PFN_DESTRUCTION_CALLBACK callback_fn,
      void*                    data,
      UINT*                    callback_id)
{
   return destruction_notifier->lpVtbl->RegisterDestructionCallback(
         destruction_notifier, callback_fn, data, callback_id);
}

static INLINE HRESULT
D3DUnregisterDestructionCallback(
      D3DDestructionNotifier destruction_notifier, UINT callback_id)
{
   return destruction_notifier->lpVtbl->UnregisterDestructionCallback(
         destruction_notifier, callback_id);
}
#endif
/* end of auto-generated */

bool d3d_compile(const char* src, size_t size,
      LPCSTR src_name, LPCSTR entrypoint, LPCSTR target, D3DBlob* out);

bool d3d_compile_from_file(LPCWSTR filename, LPCSTR entrypoint, LPCSTR target, D3DBlob* out);
