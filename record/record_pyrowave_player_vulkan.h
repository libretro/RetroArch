/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_PYROWAVE_PLAYER_VULKAN_H
#define RECORD_PYROWAVE_PLAYER_VULKAN_H
#include "record_pyrowave_source.h"
#include "record_pyrowave_vulkan.h"
RETRO_BEGIN_DECLS
/* Cold SPIR-V loading and decoder creation from negotiated metadata. Native
 * submission and image/fence loans remain owned by the render queue's caller. */
record_pyrowave_vulkan_t *record_pyrowave_player_vulkan_new(
      const record_pyrowave_source_config_t *config);
RETRO_END_DECLS
#endif
