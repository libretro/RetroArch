#!/usr/bin/env python3
"""Evaluate shipping Makefile.common for native stream backend combinations."""
import argparse
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--make', default='make')
a = parser.parse_args()
root = Path(__file__).resolve().parents[3]
for d3d, vulkan, audio in ((0, 0, 1), (1, 0, 1), (0, 1, 1), (1, 1, 1), (1, 1, 0)):
    flags = dict(OS='Win32', HAVE_VULKAN=1, HAVE_THREADS=1, HAVE_CONFIGFILE=1,
                 HAVE_RMP4=1, HAVE_RAC3=1, HAVE_RSLC=1, HAVE_RTMP=1, HAVE_RFLV=1,
                 HAVE_D3D12=d3d, HAVE_RECORD_VULKAN=vulkan, HAVE_MF_AUDIO=audio)
    source = 'OBJ :=\n' + ''.join(f'{key} := {value}\n' for key, value in flags.items())
    source += 'include Makefile.common\n.PHONY: probe\nprobe:\n'
    source += '\t@echo $(filter record/record_stream_%.o record/record_aac_audio.o,$(OBJ))\n'
    result = subprocess.run([a.make, '--no-print-directory', '-f', '-', 'probe'],
                            cwd=root, input=source, text=True, capture_output=True, check=True)
    actual = result.stdout.split()
    expected = []
    if audio and (d3d or vulkan):
        expected += ['record/record_stream_gpu.o', 'record/record_aac_audio.o']
        if d3d:
            expected.append('record/record_stream_d3d12.o')
        if vulkan:
            expected.append('record/record_stream_vulkan.o')
    assert sorted(actual) == sorted(expected), (flags, actual, expected)
    print(f'PASS stream objects: D3D12={d3d} Vulkan={vulkan} AAC={audio}')
