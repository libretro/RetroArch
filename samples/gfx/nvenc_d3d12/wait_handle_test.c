/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
/* nvenc_d3d12_wait_handle follows what nvenc_d3d12_poll waits for: the
 * head slot's output fence. NVENC's async completion event may fire
 * before that fence, and a waiter woken by it finds poll still busy.
 * A fake fence stands behind the real encoder struct; no device and no
 * NVENC runtime are needed. */
#include <stdio.h>
#include "../../../gfx/common/nvenc_d3d12.c"

#if defined(_WIN32) && !defined(__WINRT__)
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %u: %s\n", (unsigned)__LINE__, #x); return 1; } } while (0)

typedef struct fake_fence
{
   ID3D12FenceVtbl *lpVtbl;
   UINT64 completed, armed_value;
   HANDLE armed_event;
} fake_fence_t;

static UINT64 STDMETHODCALLTYPE fake_completed(ID3D12Fence *f)
{
   return ((fake_fence_t*)f)->completed;
}

static HRESULT STDMETHODCALLTYPE fake_arm(ID3D12Fence *f, UINT64 value, HANDLE event)
{
   fake_fence_t *ff = (fake_fence_t*)f;
   if (value <= ff->completed)
      return SetEvent(event) ? S_OK : E_FAIL;
   ff->armed_value = value;
   ff->armed_event = event;
   return S_OK;
}

static void fake_signal(fake_fence_t *ff, UINT64 value)
{
   ff->completed = value;
   if (ff->armed_event && value >= ff->armed_value)
   {
      SetEvent(ff->armed_event);
      ff->armed_event = NULL;
   }
}

int main(void)
{
   static ID3D12FenceVtbl vtbl;
   fake_fence_t   fence;
   nvenc_d3d12_t  e;
   HANDLE         h;

   memset(&vtbl, 0, sizeof(vtbl));
   vtbl.GetCompletedValue    = fake_completed;
   vtbl.SetEventOnCompletion = fake_arm;
   memset(&fence, 0, sizeof(fence));
   fence.lpVtbl = &vtbl;
   memset(&e, 0, sizeof(e));
   CHECK((e.done_event = CreateEvent(NULL, FALSE, FALSE, NULL)) != NULL);
   CHECK((e.eos = CreateEvent(NULL, TRUE, FALSE, NULL)) != NULL);
   CHECK((e.slots[0].event = CreateEvent(NULL, TRUE, FALSE, NULL)) != NULL);
   e.slot_count     = 1;
   e.count          = 1;
   e.slots[0].done  = (ID3D12Fence*)&fence;
   e.slots[0].value = 1;

   /* In flight: the handle is the fence's, not yet signalled */
   CHECK((h = nvenc_d3d12_wait_handle(&e)) != NULL);
   CHECK(WaitForSingleObject(h, 0) == WAIT_TIMEOUT);

   /* NVENC reports completion before the fence: nothing to wake for */
   SetEvent(e.slots[0].event);
   CHECK((h = nvenc_d3d12_wait_handle(&e)) != NULL);
   CHECK(WaitForSingleObject(h, 0) == WAIT_TIMEOUT);

   /* The fence passes: the waiter wakes, once */
   fake_signal(&fence, 1);
   CHECK(WaitForSingleObject(h, 0) == WAIT_OBJECT_0);
   CHECK(WaitForSingleObject(h, 0) == WAIT_TIMEOUT);

   /* Through the fence, only the bitstream lock is left: no handle */
   CHECK(nvenc_d3d12_wait_handle(&e) == NULL);

   /* The next packet arms for its own value */
   e.slots[0].value = 2;
   CHECK((h = nvenc_d3d12_wait_handle(&e)) != NULL);
   CHECK(WaitForSingleObject(h, 0) == WAIT_TIMEOUT);
   fake_signal(&fence, 2);
   CHECK(WaitForSingleObject(h, 0) == WAIT_OBJECT_0);

   /* A held bitstream has nothing to wait on; an empty finishing
    * encoder waits on end of stream */
   e.locked = true;
   CHECK(nvenc_d3d12_wait_handle(&e) == NULL);
   e.locked    = false;
   e.count     = 0;
   e.finishing = true;
   CHECK(nvenc_d3d12_wait_handle(&e) == e.eos);

   CloseHandle(e.done_event);
   CloseHandle(e.eos);
   CloseHandle(e.slots[0].event);
   puts("PASS: nvenc d3d12 wait handle follows the output fence");
   return 0;
}
#else
int main(void)
{
   puts("skip: nvenc d3d12 wait handle (Windows only)");
   return 0;
}
#endif
