/* Copyright (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------------
 * The following license statement only applies to this libretro API header (libretro_d3d9.h)
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

#ifndef LIBRETRO_DIRECT3D9_H__
#define LIBRETRO_DIRECT3D9_H__

#include <d3d9.h>

#include "libretro.h"

/* There has never been another version: this one is the whole of the
 * interface, threaded frontends included. A later version appends
 * members and keeps every one of these as it is, so a core accepts any
 * interface_version at or above the one it was built for. */
#define RETRO_HW_RENDER_INTERFACE_D3D9_VERSION 1

/* D3DCompile from d3dcompiler_*.dll, with its parameter types left
 * opaque so that this header needs nothing past <d3d9.h>. A core that
 * has <d3dcompiler.h> casts it to pD3DCompile; the arguments are
 * D3DCompile's: source, its size, source name, D3D_SHADER_MACRO
 * defines, ID3DInclude, entry point, target ("vs_3_0", "ps_2_0", ...),
 * flags1, flags2, and the ID3DBlob code and error blobs. */
typedef HRESULT (WINAPI *retro_d3d9_compile_t)(const void *src,
      SIZE_T src_size, const char *source_name, const void *defines,
      void *include, const char *entry, const char *target,
      UINT flags1, UINT flags2, void **code, void **errors);

/* A core asks for it with RETRO_HW_CONTEXT_D3D9 in SET_HW_RENDER and
 * gets it from GET_HW_RENDER_INTERFACE, inside context_reset and after.
 * The interface and the device are good until context_destroy: ask for
 * the interface again in every context_reset, the pointer may differ.
 *
 * The core renders with the frontend's own device. A Direct3D 9 device
 * is not safe to use from two threads at once, and a frontend may draw
 * and present from a thread of its own, while the core runs. So the two
 * take turns: the core uses the device only between lock_context and
 * unlock_context - creating and releasing resources included - and
 * never across a return from retro_run or across video_refresh.
 *
 * The frame is a texture of the core's own, handed over without a copy:
 * the frontend samples it where it lies. The core keeps one such
 * texture per sync index, and leaves one alone from set_texture until
 * wait_sync_index next returns for the same index.
 *
 * A frame, from the core's side:
 *
 *   i = iface->get_sync_index(iface->handle);
 *   iface->wait_sync_index(iface->handle);        NOT with the lock held
 *   if (iface->lock_context(iface->handle))
 *      ... the frontend has had the device: set every state again ...
 *   ... BeginScene, draw into texture[i], EndScene ...
 *   iface->set_texture(iface->handle, texture[i]);
 *   iface->unlock_context(iface->handle);
 *   video_refresh(RETRO_HW_FRAME_BUFFER_VALID, width, height, 0);
 *
 * The frame is the top-left width x height of the texture's level 0,
 * top row first. The texture is a D3DUSAGE_RENDERTARGET one in
 * D3DPOOL_DEFAULT, any format the device can sample (D3DFMT_X8R8G8B8,
 * D3DFMT_A8R8G8B8, D3DFMT_A2R10G10B10, D3DFMT_A16B16G16R16F, ...),
 * sampled as it is: the frontend draws it through its shaders and its
 * scaling the way it draws a software frame.
 *
 * The device is shared:
 * - The frontend sets the state it draws with whenever it draws, and
 *   leaves it as it is: render targets, depth surface, viewport,
 *   scissor, shaders, vertex declaration, streams, textures, sampler and
 *   render states. Whatever the core left is likewise none of the
 *   frontend's concern.
 * - At video_refresh the core is outside BeginScene/EndScene.
 * - The device may be a Direct3D9Ex one (QueryInterface for
 *   IDirect3DDevice9Ex says so), which has no D3DPOOL_MANAGED.
 *
 * Losing the device: a plain Direct3D 9 device loses every
 * D3DPOOL_DEFAULT resource when it is reset - render targets, depth
 * surfaces, default pool textures and buffers, state blocks, additional
 * swap chains, queries - and cannot be reset while any of them is
 * alive. When the frontend has to reset it (V-Sync switched, the device
 * lost) it calls context_destroy first, where the core releases them,
 * and context_reset after, where it makes them again; managed and system
 * memory resources may be kept. A Direct3D9Ex device loses nothing on a
 * reset, and no call is made. When the frontend makes a new device - the
 * video driver restarted (a fullscreen toggle, say), or the old device
 * could not be reset - the core is told the same way and everything it
 * made is gone. Both calls come on the thread that runs retro_run,
 * possibly from inside video_refresh. cache_context is not honoured: a
 * Direct3D 9 device is never kept across a video driver restart.
 */
struct retro_hw_render_interface_d3d9
{
  /* RETRO_HW_RENDER_INTERFACE_D3D9. */
  enum retro_hw_render_interface_type interface_type;
  /* RETRO_HW_RENDER_INTERFACE_D3D9_VERSION or later. */
  unsigned interface_version;

  /* Opaque handle to the frontend's backend, passed to every function
   * below. */
  void *handle;
  LPDIRECT3DDEVICE9 device;
  /* The frontend's shader compiler; NULL if it found none. */
  retro_d3d9_compile_t D3DCompile;

  /* Takes the device. Blocks while the frontend is using it. Recursive:
   * a thread that holds it may take it again, and releases it as many
   * times.
   *
   * Returns true if the frontend has used the device since the core
   * last released it, and then every state and binding is whatever the
   * frontend left, so the core sets again whatever it relies on before
   * it draws. Returns false if the device is exactly as the core left
   * it. video_refresh counts as the frontend's: the next lock_context
   * after one returns true. */
  bool (*lock_context)(void *handle);

  /* Gives the device back. The core must not touch it again until it
   * has taken it again. */
  void (*unlock_context)(void *handle);

  /* The frame's texture, with the device held, before the core releases
   * it and calls video_refresh(RETRO_HW_FRAME_BUFFER_VALID). NULL hands
   * over no frame.
   *
   * The frontend keeps a reference of its own to the last texture it
   * drew, and draws it again when it presents without the core - the
   * menu, a pause - until it draws another or context_destroy comes. So
   * the core may release a texture whenever wait_sync_index allows it to
   * draw into it again. */
  void (*set_texture)(void *handle, LPDIRECT3DTEXTURE9 texture);

  /* The sync index the next frame belongs to. It changes only inside
   * video_refresh(RETRO_HW_FRAME_BUFFER_VALID), and is always a bit set
   * in get_sync_index_mask(). */
  unsigned (*get_sync_index)(void *handle);

  /* Bit i set: i is an index get_sync_index() can return. The core needs
   * one frame texture per set bit; a mask of 1 means one texture, used
   * again every frame. */
  unsigned (*get_sync_index_mask)(void *handle);

  /* Blocks until the frontend has finished with the texture last handed
   * over for the current sync index; returns at once if there is none.
   * Must not be called with the device held: the frontend needs it to
   * finish. There is no GPU fence to wait: the device has one command
   * stream, and what the frontend has issued on it comes before
   * whatever the core issues next. */
  void (*wait_sync_index)(void *handle);
};

#endif /* LIBRETRO_DIRECT3D9_H__ */
