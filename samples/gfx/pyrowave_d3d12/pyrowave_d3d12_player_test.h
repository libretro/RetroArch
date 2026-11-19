/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../record/record_pyrowave_player_d3d12.h"
#include "../../../gfx/common/d3dcompiler_common.h"

static const char *test_pyrowave_d3d12_directory;

/* The standalone oracle has no frontend logging runtime. Compile through
 * the same native compiler entry point used by the frontend helper. */
bool d3d_compile(const char *src, size_t len, LPCSTR name,
      LPCSTR entry, LPCSTR target, D3DBlob *out)
{
   ID3DBlob *errors = NULL;
   HRESULT result = D3DCompile(src, len, name, NULL, NULL, entry,
         target, 0, 0, out, &errors);
   if (errors)
   {
      if (FAILED(result)) fputs((const char*)ID3D10Blob_GetBufferPointer(errors), stderr);
      ID3D10Blob_Release(errors);
   }
   return SUCCEEDED(result);
}

static record_pyrowave_d3d12_t *test_pyrowave_d3d12_factory_new(
      const record_pyrowave_d3d12_config_t *cfg)
{
   record_pyrowave_source_config_t setup;
   if (!cfg->decode) return record_pyrowave_d3d12_new(cfg);
   memset(&setup, 0, sizeof(setup));
   setup.device = cfg->device; setup.directory = test_pyrowave_d3d12_directory;
   setup.stream.sequence = cfg->sequence;
   setup.stream.maximum_frame_bytes = cfg->maximum_bytes;
   setup.stream.bit_depth = cfg->bit_depth;
   setup.slots = cfg->slots; setup.texture_mode = cfg->texture_mode;
   return record_pyrowave_player_d3d12_new(&setup);
}
#define record_pyrowave_d3d12_new test_pyrowave_d3d12_factory_new
