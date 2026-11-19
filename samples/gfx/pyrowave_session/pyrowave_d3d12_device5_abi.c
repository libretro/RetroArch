/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define CINTERFACE
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <d3d12.h>
#include "pyrowave_d3d12_device5_test.h"

#ifdef __ID3D12Device5_INTERFACE_DEFINED__
typedef char pyrowave_test_device5_remove_offset[
   offsetof(ID3D12Device5Vtbl, RemoveDevice)
      == offsetof(pyrowave_test_device5_vtable_t, remove) ? 1 : -1];
typedef char pyrowave_test_device5_release_offset[
   offsetof(ID3D12Device5Vtbl, Release)
      == offsetof(pyrowave_test_device5_vtable_t, release) ? 1 : -1];

int main(void)
{
   if (memcmp(&IID_ID3D12Device5, &pyrowave_test_device5_iid, sizeof(IID))) return 1;
   puts("Device5 test binding: SDK GUID and method offsets PASS");
   return 0;
}
#else
int main(void)
{
   (void)pyrowave_test_device5_iid.Data1;
   puts("Device5 legacy binding compiles; SDK comparison unavailable (requires a newer SDK)");
   return 77;
}
#endif
