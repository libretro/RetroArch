/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PYROWAVE_D3D12_DEVICE5_TEST_H
#define PYROWAVE_D3D12_DEVICE5_TEST_H

/* Test-only COM ABI for old SDKs. SDK Device5 places RemoveDevice at slot
 * 58, after IUnknown and 55 inherited methods. No other slots are invoked. */
typedef struct pyrowave_test_device5 pyrowave_test_device5_t;
typedef struct pyrowave_test_device5_vtable
{
   HRESULT (STDMETHODCALLTYPE *query)(pyrowave_test_device5_t*, const IID*, void**);
   ULONG (STDMETHODCALLTYPE *retain)(pyrowave_test_device5_t*);
   ULONG (STDMETHODCALLTYPE *release)(pyrowave_test_device5_t*);
   void (STDMETHODCALLTYPE *reserved[55])(void);
   void (STDMETHODCALLTYPE *remove)(pyrowave_test_device5_t*);
} pyrowave_test_device5_vtable_t;

struct pyrowave_test_device5
{
   const pyrowave_test_device5_vtable_t *vtable;
};

static const IID pyrowave_test_device5_iid =
{ 0x8b4f173b, 0x2fea, 0x4b80, { 0x8f, 0x58, 0x43, 0x07, 0x19, 0x1a, 0xb9, 0x5d } };
#endif
