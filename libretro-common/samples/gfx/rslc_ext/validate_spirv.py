#!/usr/bin/env python3
"""Validate emitted modules with only their required Vulkan feature settings."""
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile


def flags(path):
    if path.name == 'unsized_uniform.spv':
        # VK_EXT_shader_uniform_buffer_unsized_array permits the runtime
        # array in Uniform/Block; std430 also requires standard UBO layout.
        # SPIRV-Tools before that extension still rejects this legal module.
        return ['--target-env', 'vulkan1.1', '--uniform-buffer-standard-layout']
    return ['--target-env', 'vulkan1.1' if path.name == 'subgroup.spv' else 'vulkan1.0']


def reject_bad_offset(validator, path):
    words = list(struct.unpack('<' + 'I' * (path.stat().st_size // 4), path.read_bytes()))
    at, changed = 5, False
    while at < len(words):
        count, opcode = words[at] >> 16, words[at] & 0xffff
        assert count and at + count <= len(words), 'Malformed fixture instruction'
        # The struct's final float[3] is at std430 offset 76. A deliberately
        # unaligned offset must still fail despite the enabled feature rules.
        if opcode == 72 and count == 5 and words[at + 2:at + 5] == [4, 35, 76]:
            words[at + 4] = 75
            changed = True
            break
        at += count
    assert changed, 'No intended std430 member found in the emitted fixture'
    with tempfile.TemporaryDirectory(prefix='rslc-invalid-offset-') as directory:
        bad = Path(directory) / 'unaligned.spv'
        bad.write_bytes(struct.pack('<' + 'I' * len(words), *words))
        result = subprocess.run([validator] + flags(path) + [str(bad)],
                                capture_output=True, text=True, timeout=30)
    assert result.returncode != 0 and '75' in result.stderr and 'align' in result.stderr, (
        'Validator did not reject the deliberately unaligned member', result.stderr)
    print('[pass] Vulkan validator rejects unaligned extended UBO member', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--validator', default='spirv-val')
    args = parser.parse_args()
    paths = sorted(args.directory.glob('*.spv'))
    assert paths and (args.directory / 'unsized_uniform.spv') in paths, 'Missing emitted fixtures'
    for path in paths:
        subprocess.run([args.validator] + flags(path) + [str(path)], check=True, timeout=30)
        print(f'[pass] Vulkan SPIR-V: {path.name}', flush=True)
    reject_bad_offset(args.validator, args.directory / 'unsized_uniform.spv')


if __name__ == '__main__':
    main()
