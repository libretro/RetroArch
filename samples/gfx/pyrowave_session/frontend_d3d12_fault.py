# Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later
"""Offline D3D12 receiver failure fixture with a GPU retirement gate."""
import argparse
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-directory', type=Path, required=True)
parser.add_argument('--retroarch', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--stage', choices=('close', 'signal', 'removed', 'removed-queued'), required=True)
parser.add_argument('--sender', action='store_true', help='Fault the thirty-second source capture')
parser.add_argument('--skip-retirement-ack', action='store_true',
    help='Negative fixture: abandon while the retirement marker is pending')
args = parser.parse_args()
if args.output.resolve() == args.retroarch.resolve():
    parser.error('--output must differ from the configured executable')
build = args.build_directory.resolve()
core = build / ('record/drivers/record_pyrowave.c' if args.sender else 'record/record_pyrowave_receiver.c')
renderer = build / 'gfx/drivers/d3d12.c'
original_core, original_renderer = core.read_bytes(), renderer.read_bytes()
patched_core = original_core.replace(b'\r\n', b'\n')
patched = original_renderer.replace(b'\r\n', b'\n')


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError('Expected exactly one fixture insertion point: ' + str(old))
    return text.replace(old, new)


if args.sender:
    capture = b'         timestamp, duration, &c->last_ticket);'
    patched_core = replace_once(patched_core, capture, capture + b'''
   if (result == RECORD_GPU_QUEUE_OK)
   {
      static unsigned pyrowave_test_d3d12_captures;
      if (++pyrowave_test_d3d12_captures == 32) pyrowave_test_d3d12_abort = 1;
   }''')
else:
    decode = b'''      if (!c->video_input_end) result = record_pyrowave_d3d12_decode(player, commands, fence, value);
      if (result == RECORD_GPU_QUEUE_OK) c->decoding++;'''
    patched_core = replace_once(patched_core, decode, b'''      if (!c->video_input_end) result = record_pyrowave_d3d12_decode(player, commands, fence, value);
      if (result == RECORD_GPU_QUEUE_OK)
      {
         static unsigned pyrowave_test_d3d12_decodes;
         c->decoding++;
         if (++pyrowave_test_d3d12_decodes == 8) pyrowave_test_d3d12_abort = 1;
      }''')
patched_core = b'unsigned pyrowave_test_d3d12_abort;\n' + patched_core
if args.skip_retirement_ack and args.sender:
    parser.error('--skip-retirement-ack currently requires the receiver fixture')
if args.skip_retirement_ack:
    old = b'video_d3d12_pyrowave_quiesce(data, c->player_d3d12) > 0'
    patched_core = replace_once(patched_core, old, old.replace(b'> 0', b'>= 0'))
include = b'#include "../../record/record_pyrowave_receiver.h"'
patched = replace_once(patched, include, include + b'''
static ID3D12Fence *pyrowave_test_d3d12_gate;
extern unsigned pyrowave_test_d3d12_abort;''')
attach = b'   d3d12->pyrowave_player = codec;'
patched = replace_once(patched, attach, attach + b'''
   if (FAILED(d3d12->device->lpVtbl->CreateFence(d3d12->device, 0,
         D3D12_FENCE_FLAG_NONE, uuidof(ID3D12Fence), (void**)&pyrowave_test_d3d12_gate)))
      return false;''')
detach = b'   d3d12->pyrowave_player = NULL;'
patched = replace_once(patched, detach, b'   Release(pyrowave_test_d3d12_gate); pyrowave_test_d3d12_gate = NULL;\n' + detach)
if args.stage in ('removed', 'removed-queued'):
    patched = replace_once(patched, include, include + b'\n#include "../../samples/gfx/pyrowave_session/pyrowave_d3d12_device5_test.h"')
start = patched.index(b'static bool d3d12_pyrowave_execute(')
end = patched.index(b'\n#endif', start)
execute = patched[start:end]
close = b'   HRESULT result = d3d12->queue.cmd->lpVtbl->Close(d3d12->queue.cmd);'
fault = b'''      pyrowave_test_d3d12_abort = 0;
      RARCH_LOG("[PyroWave oracle] Recorded D3D12 decode fault.\\n");
      result = E_OUTOFMEMORY;'''
gate = b'      d3d12->queue.handle->lpVtbl->Wait(d3d12->queue.handle, pyrowave_test_d3d12_gate, 1);'
if args.stage == 'removed':
    execute = replace_once(execute, close, close + b'''
   if (pyrowave_test_d3d12_abort && SUCCEEDED(result))
   {
      pyrowave_test_device5_t *removed_device = NULL;
      pyrowave_test_d3d12_abort = 0;
      if (SUCCEEDED(d3d12->device->lpVtbl->QueryInterface(d3d12->device,
               &pyrowave_test_device5_iid, (void**)&removed_device)))
      {
         removed_device->vtable->remove(removed_device);
         removed_device->vtable->release(removed_device);
         if (FAILED(d3d12->device->lpVtbl->GetDeviceRemovedReason(d3d12->device)))
            RARCH_LOG("[PyroWave oracle] D3D12 logical device removed.\\n");
      }
      RARCH_LOG("[PyroWave oracle] Recorded D3D12 decode fault.\\n");
      result = DXGI_ERROR_DEVICE_REMOVED;
   }''')
elif args.stage == 'removed-queued':
    signal = b'''      result = d3d12->queue.handle->lpVtbl->Signal(d3d12->queue.handle,
            d3d12->queue.fence, d3d12->queue.frameSignal);'''
    execute = replace_once(execute, signal, signal + b'''
      if (pyrowave_test_d3d12_abort && SUCCEEDED(result))
      {
         pyrowave_test_device5_t *removed_device = NULL;
         pyrowave_test_d3d12_abort = 0;
         if (SUCCEEDED(d3d12->device->lpVtbl->QueryInterface(d3d12->device,
                  &pyrowave_test_device5_iid, (void**)&removed_device)))
         {
            removed_device->vtable->remove(removed_device);
            removed_device->vtable->release(removed_device);
            if (FAILED(d3d12->device->lpVtbl->GetDeviceRemovedReason(d3d12->device)))
               RARCH_LOG("[PyroWave oracle] D3D12 logical device removed.\\n");
         }
         RARCH_LOG("[PyroWave oracle] Recorded D3D12 decode fault.\\n");
      }''')
elif args.stage == 'close':
    execute = replace_once(execute, close, close + b'''
   if (pyrowave_test_d3d12_abort && SUCCEEDED(result))
   {
''' + gate + b'\n' + fault + b'\n   }')
else:
    execute = replace_once(execute, close, close + b'''
   if (pyrowave_test_d3d12_abort && SUCCEEDED(result))
   {
''' + gate + b'\n   }')
    signal = b'''      result = d3d12->queue.handle->lpVtbl->Signal(d3d12->queue.handle,
            d3d12->queue.fence, d3d12->queue.frameSignal);'''
    execute = replace_once(execute, signal, b'''      if (pyrowave_test_d3d12_abort)
      {
''' + fault + b'\n      }\n      else\n   ' + signal)
patched = patched[:start] + execute + patched[end:]
poll = b'         done = d3d12->pyrowave_marker->lpVtbl->GetCompletedValue(d3d12->pyrowave_marker);'
patched = replace_once(patched, poll, b'''         {
            static unsigned pyrowave_test_d3d12_polls;
            if (++pyrowave_test_d3d12_polls <= 7)
            {
               if (pyrowave_test_d3d12_polls == 1)
                  RARCH_LOG("[PyroWave oracle] D3D12 retirement marker held.\\n");
               return 0;
            }
            if (FAILED(pyrowave_test_d3d12_gate->lpVtbl->Signal(pyrowave_test_d3d12_gate, 1)))
               return -1;
         }
''' + poll)
ack = b'   d3d12->pyrowave_retired = true;\n   return 1;'
patched = replace_once(patched, ack, b'''
   if (SUCCEEDED(removed))
      RARCH_LOG("[PyroWave oracle] D3D12 retirement marker acknowledged.\\n");
   else
      RARCH_LOG("[PyroWave oracle] D3D12 removal acknowledged.\\n");
''' + ack)
timeout = b'            " the device is most likely lost.\\n",\n            (unsigned)D3D12_FENCE_WAIT_MS);'
patched = replace_once(patched, timeout, b'''            " the device is most likely lost (wait=%u done=%u signal=%u label=%u).\\n",
            (unsigned)D3D12_FENCE_WAIT_MS, (unsigned)value,
            (unsigned)fence->lpVtbl->GetCompletedValue(fence),
            (unsigned)d3d12->queue.frameSignal, (unsigned)d3d12->queue.fenceValue);''')

try:
    core.write_bytes(patched_core)
    renderer.write_bytes(patched)
    os.utime(core, None)
    os.utime(renderer, None)
    subprocess.run(['python3', str(Path(__file__).with_name('frontend_d3d12_debug.py')),
        '--build-directory', str(build), '--retroarch', str(args.retroarch.resolve()),
        '--output', str(args.output.resolve())], cwd=build, check=True)
finally:
    core.write_bytes(original_core)
    renderer.write_bytes(original_renderer)
    os.utime(core, None)
    os.utime(renderer, None)
    subprocess.run(['make', '-j2'], cwd=build, check=True)
print('D3D12_FAILURE_FIXTURE_BUILT_SOURCE_RESTORED', args.stage, flush=True)
