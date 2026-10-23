/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../gfx/common/vaapi_av1_backend.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void rav1_set_hw_backend(const rav1_hw_backend_t *offer)
{
   (void)offer;
   assert(0);
}

int rav1_unset_hw_backend(const rav1_hw_backend_t *offer)
{
   (void)offer;
   assert(0);
   return 0;
}

int main(void)
{
   vaapi_av1_backend_t backend, before;
   memset(&backend, 0, sizeof(backend));
   before = backend;
   assert(!vaapi_av1_backend_init(&backend, 2));
   assert(!memcmp(&backend, &before, sizeof(backend)));
   assert(!vaapi_av1_backend_publish(&backend, &before, 226, 128));
   assert(!vaapi_av1_backend_withdraw(&backend, &before));
   puts("PASS: VA factory refuses without threads or lock-free atomics; no "
        "publication or worker dependency");
   return 0;
}
