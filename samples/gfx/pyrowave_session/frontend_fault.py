# Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later
"""Build an offline finalization-failure fixture; restore the configured build."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-directory', type=Path, required=True)
parser.add_argument('--retroarch', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--receiver', action='store_true',
    help='Fail after the eighth recorded receiver decode, rather than source capture')
parser.add_argument('--retain-unsubmitted', action='store_true',
    help='Receiver reset failure: retain loans through a cold queue retirement marker')
parser.add_argument('--report-marker-loss', action='store_true',
    help='Report device loss after the real retirement-marker submission succeeds')
args = parser.parse_args()
if args.report_marker_loss and not args.retain_unsubmitted:
    parser.error('--report-marker-loss requires --retain-unsubmitted')
if args.output.resolve() == args.retroarch.resolve():
    parser.error('--output must differ from the configured RetroArch executable')
build = args.build_directory.resolve()
renderer = build / 'gfx/drivers/vulkan.c'
original = renderer.read_bytes()
needle = b'VkResult result = vkEndCommandBuffer(vk->cmd);'
if original.count(needle) != 1:
    raise RuntimeError('Expected exactly one PyroWave command-buffer finalization')
fault = b'''VkResult result = vkEndCommandBuffer(vk->cmd);
      static unsigned pyrowave_test_ends;
      if (pyrowave_fence && ++pyrowave_test_ends == 32 && result == VK_SUCCESS)
      {
         result = VK_ERROR_OUT_OF_HOST_MEMORY;
      }'''
core = build / 'record/record_pyrowave_receiver.c'
core_original = None
if args.receiver:
    core_original = core.read_bytes()
    core_working = core_original.replace(b'\r\n', b'\n')
    decode = b'''      if (!c->video_input_end) result = record_pyrowave_vulkan_decode(c->player_vulkan, commands, *completion);
      if (result == RECORD_GPU_QUEUE_OK) c->decoding++;'''
    if core_working.count(decode) != 1:
        raise RuntimeError('Expected exactly one receiver decode admission')
    core_fault = b'''      if (!c->video_input_end) result = record_pyrowave_vulkan_decode(c->player_vulkan, commands, *completion);
      if (result == RECORD_GPU_QUEUE_OK)
      {
         static unsigned pyrowave_test_decodes;
         c->decoding++;
         if (++pyrowave_test_decodes == 8) pyrowave_test_decode_abort = 1;
      }'''
    core_patched = core_working.replace(decode, core_fault)
    core_patched = b'unsigned pyrowave_test_decode_abort;\n' + core_patched
    fault = b'''extern unsigned pyrowave_test_decode_abort;
      VkResult result = vkEndCommandBuffer(vk->cmd);
      if (pyrowave_test_decode_abort && result == VK_SUCCESS)
      {
         pyrowave_test_decode_abort = 0;
         RARCH_LOG("[PyroWave oracle] Recorded decode finalization fault.\\n");
         result = VK_ERROR_OUT_OF_HOST_MEMORY;
      }'''
renderer_patched = original.replace(needle, fault)
if args.retain_unsubmitted:
    renderer_patched = b'static unsigned pyrowave_test_reset_fault;\n' + renderer_patched
    reset = b'&& vkResetCommandBuffer(vk->cmd, 0) == VK_SUCCESS)'
    if renderer_patched.count(reset) != 1:
        raise RuntimeError('Expected exactly one unsubmitted reset guard')
    renderer_patched = renderer_patched.replace(reset,
        b'&& !pyrowave_test_reset_fault && vkResetCommandBuffer(vk->cmd, 0) == VK_SUCCESS)')
    fault_mark = b'pyrowave_test_decode_abort = 0;' if args.receiver else b'result = VK_ERROR_OUT_OF_HOST_MEMORY;'
    renderer_patched = renderer_patched.replace(fault_mark, fault_mark +
        b'\n         pyrowave_test_reset_fault = 1;\n         RARCH_LOG("[PyroWave oracle] Retained unsubmitted decode.\\n");')
    poll = b'   result = vkGetFenceStatus(vk->context->device, marker);'
    if renderer_patched.count(poll) != 1:
        raise RuntimeError('Expected exactly one cold retirement marker poll')
    renderer_patched = renderer_patched.replace(poll, b"""   {
      static unsigned pyrowave_test_marker_polls;
      if (++pyrowave_test_marker_polls <= 7)
      {
         if (pyrowave_test_marker_polls == 1)
            RARCH_LOG("[PyroWave oracle] Retirement marker held.\\n");
         return 0;
      }
   }
""" + poll + b"""
   if (result == VK_SUCCESS)
      RARCH_LOG("[PyroWave oracle] Retirement marker acknowledged.\\n");""")
if args.report_marker_loss:
    submit = b'      result = vulkan_queue_signal_fence(vk->context->queue, marker);'
    if renderer_patched.count(submit) != 1:
        raise RuntimeError('Expected exactly one retirement marker submission')
    renderer_patched = renderer_patched.replace(submit, submit + b"""
      if (result == VK_SUCCESS)
      {
         RARCH_LOG("[PyroWave oracle] Retirement submit loss report.\\n");
         result = VK_ERROR_DEVICE_LOST;
      }""")
try:
    if core_original is not None:
        core.write_bytes(core_patched)
        os.utime(core, None)
    renderer.write_bytes(renderer_patched)
    os.utime(renderer, None)
    subprocess.run(['make', '-j2'], cwd=build, check=True)
    args.output.resolve().parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(args.retroarch.resolve(), args.output.resolve())
finally:
    if core_original is not None:
        core.write_bytes(core_original)
        os.utime(core, None)
    renderer.write_bytes(original)
    os.utime(renderer, None)
    subprocess.run(['make', '-j2'], cwd=build, check=True)
print('PYROWAVE_FINALIZATION_FAULT_BUILT_SOURCE_RESTORED', flush=True)
