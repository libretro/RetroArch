/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../gfx/common/vaapi_hevc_backend.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void rh265_set_hw_backend(const rh265_hw_backend_t *offer)
{
   (void)offer;
   assert(0);
}

int rh265_unset_hw_backend(const rh265_hw_backend_t *offer)
{
   (void)offer;
   assert(0);
   return 0;
}

int main(void)
{
   vaapi_hevc_backend_t backend, before;
   memset(&backend, 0, sizeof(backend));
   before = backend;
   assert(!vaapi_hevc_backend_init(&backend, 4096, 2));
   assert(!memcmp(&backend, &before, sizeof(backend)));
   assert(!vaapi_hevc_backend_publish(&backend, &before, 226, 128));
   assert(!vaapi_hevc_backend_withdraw(&backend, &before));
   puts("PASS: VA factory refuses without threads or lock-free atomics; no publication or worker dependency");
   return 0;
}
