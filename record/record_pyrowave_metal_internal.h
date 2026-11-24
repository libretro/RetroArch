/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_PYROWAVE_METAL_INTERNAL_H
#define RECORD_PYROWAVE_METAL_INTERNAL_H
#include "record_pyrowave_metal.h"

/* Typed C ABI for the Metal runtime and portable ownership tests. All object
 * creation is cold. buffers(cpu=0) are private; cpu=1 uses shared or managed
 * cached storage. synchronize records GPU->CPU visibility before completion. */
typedef struct record_pyrowave_metal_api
{
   void *user;
   void *(*retain)(void*, void*);
   void (*release)(void*, void*);
   void (*destroy)(void*);
   size_t (*buffer_limit)(void*, void*);
   void *(*buffer_new)(void*, void*, size_t, bool);
   void *(*buffer_data)(void*, void*);
   void (*buffer_flush)(void*, void*, size_t);
   void *(*texture_new)(void*, void*, unsigned, unsigned);
   void *(*pipeline_new)(void*, void*, const char*, unsigned);
   bool (*source_valid)(void*, void*, void*, unsigned, unsigned);
   bool (*commands_valid)(void*, void*, void*, void*);
   unsigned (*status)(void*, void*);
   void *(*compute_begin)(void*, void*);
   void (*compute_pipeline)(void*, void*, void*);
   void (*compute_buffer)(void*, void*, void*, size_t, unsigned);
   void (*compute_texture)(void*, void*, void*, unsigned);
   void (*compute_dispatch)(void*, void*, unsigned, unsigned, unsigned, unsigned, unsigned);
   void (*compute_barrier)(void*, void*, bool);
   void (*end)(void*, void*);
   void *(*blit_begin)(void*, void*);
   void (*blit_copy)(void*, void*, void*, size_t, void*, size_t, size_t);
   void (*synchronize)(void*, void*, void*);
   void *(*pool_push)(void*);
   void (*pool_pop)(void*, void*);
} record_pyrowave_metal_api_t;

bool record_pyrowave_metal_native_api(record_pyrowave_metal_api_t *api, void *device, void *queue);
record_pyrowave_metal_t *record_pyrowave_metal_new_with_api(
      const record_pyrowave_metal_config_t *config, const record_pyrowave_metal_api_t *api);
#endif
