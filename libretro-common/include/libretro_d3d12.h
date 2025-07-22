/* Copyright (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------------
 * The following license statement only applies to this libretro API header (libretro_d3d.h)
 * ---------------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the
 * "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBRETRO_DIRECT3D12_H__
#define LIBRETRO_DIRECT3D12_H__

#include <d3d12.h>
#include <d3dcompiler.h>

#define RETRO_HW_RENDER_INTERFACE_D3D12_VERSION 1

struct retro_hw_render_interface_d3d12
{
  /* Must be set to RETRO_HW_RENDER_INTERFACE_D3D12. */
  enum retro_hw_render_interface_type interface_type;
  /* Must be set to RETRO_HW_RENDER_INTERFACE_D3D12_VERSION. */
  unsigned interface_version;

  /* Opaque handle to the d3d12 backend in the frontend
   * which must be passed along to all function pointers
   * in this interface.
   */
  void* handle;
  ID3D12Device *device;
  ID3D12CommandQueue *queue;
  pD3DCompile D3DCompile;
  D3D12_RESOURCE_STATES required_state;
  void (*set_texture)(void* handle, ID3D12Resource* texture, DXGI_FORMAT format);
};

#endif /* LIBRETRO_DIRECT3D12_H__ */
