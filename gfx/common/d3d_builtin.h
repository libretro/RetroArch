/* Immutable built-in bytecode. Returned blobs own a wrapper, not the bytes. */
#ifndef GFX_D3D_BUILTIN_H
#define GFX_D3D_BUILTIN_H
#include "d3dcompiler_common.h"
#ifdef D3D_BUILTIN_TEST
bool d3d_builtin_shader(const char *source, size_t size,
      const char *entry, const char *profile, D3DBlob *out);
#endif
bool d3d_builtin_shader_named(const char *program, const char *entry,
      const char *profile, D3DBlob *out);
#endif
