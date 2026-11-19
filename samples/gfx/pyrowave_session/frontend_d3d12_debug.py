# Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later
"""Build an offline D3D12 GPU-validation fixture; restore its configured build."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-directory', type=Path, required=True)
parser.add_argument('--retroarch', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--omit-receiver-transition', action='store_true',
    help='Build a negative validation fixture with the receiver input barrier removed')
args = parser.parse_args()
if args.output.resolve() == args.retroarch.resolve():
    parser.error('--output must differ from the configured executable')
build = args.build_directory.resolve()
renderer = build / 'gfx/drivers/d3d12.c'
original = renderer.read_bytes()
header = b'#include <math.h>'
init = b'static void d3d12_init_base(d3d12_video_t* d3d12)\n{'
free = b'   Release(d3d12->device);'
# Normalize only the working fixture. Restore the exact original bytes below.
patched = original.replace(b'\r\n', b'\n')
for needle in (header, init, free):
    if patched.count(needle) != 1:
        raise RuntimeError('Expected exactly one D3D12 fixture insertion point: ' + str(needle))
patched = patched.replace(header, header + b'\n#include <d3d12sdklayers.h>')
init_code = b"""static void d3d12_init_base(d3d12_video_t* d3d12)
{
   ID3D12Debug *pyro_debug = NULL;
   ID3D12Debug1 *pyro_debug1 = NULL;
   if (SUCCEEDED(D3D12GetDebugInterface(&IID_ID3D12Debug, (void**)&pyro_debug)))
   {
      pyro_debug->lpVtbl->EnableDebugLayer(pyro_debug);
      if (SUCCEEDED(pyro_debug->lpVtbl->QueryInterface(pyro_debug,
                  &IID_ID3D12Debug1, (void**)&pyro_debug1)))
      {
         pyro_debug1->lpVtbl->SetEnableGPUBasedValidation(pyro_debug1, TRUE);
         RARCH_LOG("[PyroWave oracle] D3D12 GPU validation enabled.\\n");
         pyro_debug1->lpVtbl->Release(pyro_debug1);
      }
      pyro_debug->lpVtbl->Release(pyro_debug);
   }
"""
# Put the scope around the injected statements so existing C89 declarations
# still begin their own block, rather than following the statements above.
init_code += b'   {\n'
patched = patched.replace(init, init_code)
# Close the extra scope at the next function, independent of its name.
a = patched.index(init_code) + len(init_code)
b = patched.index(b'\nstatic ', a)
patched = patched[:b] + b'\n}\n' + patched[b:]
free_code = b"""   {
      ID3D12InfoQueue *pyro_info = NULL;
      unsigned pyro_errors = 0;
      if (FAILED(d3d12->device->lpVtbl->QueryInterface(d3d12->device,
                  &IID_ID3D12InfoQueue, (void**)&pyro_info))) pyro_errors++;
      else
      {
         UINT64 pyro_i;
         UINT64 pyro_count = pyro_info->lpVtbl->GetNumStoredMessagesAllowedByRetrievalFilter(pyro_info);
         for (pyro_i = 0; pyro_i < pyro_count; pyro_i++)
         {
            SIZE_T pyro_length = 0;
            D3D12_MESSAGE *pyro_message;
            if (FAILED(pyro_info->lpVtbl->GetMessage(pyro_info, pyro_i, NULL, &pyro_length)))
            { pyro_errors++; continue; }
            pyro_message = (D3D12_MESSAGE*)malloc(pyro_length);
            if (!pyro_message) { pyro_errors++; continue; }
            if (FAILED(pyro_info->lpVtbl->GetMessage(pyro_info, pyro_i, pyro_message, &pyro_length))) pyro_errors++;
            else if (pyro_message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR)
            {
               RARCH_ERR("[D3D12 oracle] %s\\n", pyro_message->pDescription);
               pyro_errors++;
            }
            free(pyro_message);
         }
         pyro_info->lpVtbl->Release(pyro_info);
      }
      RARCH_LOG("[PyroWave oracle] D3D12 validation errors: %u.\\n", pyro_errors);
   }
"""
patched = patched.replace(free, free_code + free)
if args.omit_receiver_transition:
    transition = b"""D3D12_RESOURCE_TRANSITION(cmd, pyrowave_image,
               D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);"""
    if patched.count(transition) != 1:
        raise RuntimeError('Expected exactly one receiver input transition')
    patched = patched.replace(transition, b'/* Offline negative fixture: input transition omitted. */')
try:
    renderer.write_bytes(patched)
    os.utime(renderer, None)
    subprocess.run(['make', '-j2'], cwd=build, check=True)
    args.output.resolve().parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(args.retroarch.resolve(), args.output.resolve())
finally:
    renderer.write_bytes(original)
    os.utime(renderer, None)
    subprocess.run(['make', '-j2'], cwd=build, check=True)
print('PYROWAVE_D3D12_DEBUG_FIXTURE_BUILT_SOURCE_RESTORED', flush=True)
