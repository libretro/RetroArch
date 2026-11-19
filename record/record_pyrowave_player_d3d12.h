/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_PYROWAVE_PLAYER_D3D12_H
#define RECORD_PYROWAVE_PLAYER_D3D12_H
#include "record_pyrowave_source.h"
#include "record_pyrowave_d3d12.h"
RETRO_BEGIN_DECLS
/* Cold construction from negotiated metadata. The native adapter owns its
 * shader objects; the frontend owns command submission and completion fences.
 * Use the native image/present API to retain a texture through its final draw. */
record_pyrowave_d3d12_t *record_pyrowave_player_d3d12_new(
      const record_pyrowave_source_config_t *config);
RETRO_END_DECLS
#endif
