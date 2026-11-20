#!/usr/bin/env python3
"""Maintainer-only SPIR-V size optimization with validation on both sides."""
import argparse
from pathlib import Path
import re
import struct
import subprocess
import tempfile


# The size pipeline without eliminate-dead-members: shared modules are also
# reflected/flattened by GL, which requires the original buffer member layout.
SHARED_SIZE_PASSES = (
    'wrap-opkill eliminate-dead-branches merge-return inline-entry-points-exhaustive '
    'eliminate-dead-functions private-to-local scalar-replacement=0 ssa-rewrite '
    'ccp loop-unroll eliminate-dead-branches simplify-instructions '
    'scalar-replacement=0 eliminate-local-single-store if-conversion '
    'simplify-instructions eliminate-dead-code-aggressive eliminate-dead-branches '
    'merge-blocks convert-local-access-chains eliminate-local-single-block '
    'eliminate-dead-code-aggressive copy-propagate-arrays vector-dce '
    'eliminate-dead-inserts eliminate-local-single-store merge-blocks '
    'ssa-rewrite redundancy-elimination simplify-instructions '
    'eliminate-dead-code-aggressive cfg-cleanup'
).split()


def optimize(data, optimizer='spirv-opt', validator='spirv-val', strip_debug=False):
    with tempfile.TemporaryDirectory(prefix='retroarch-spirv-opt-') as tmp:
        source = Path(tmp) / 'source.spv'
        output = Path(tmp) / 'optimized.spv'
        source.write_bytes(data)
        subprocess.run([validator, '--target-env', 'vulkan1.0', str(source)], check=True)
        flags = ['--preserve-bindings', '--preserve-interface', '--preserve-spec-constants']
        flags += ['-Os'] if strip_debug else ['--' + flag for flag in SHARED_SIZE_PASSES]
        if strip_debug:
            flags.append('--strip-debug')
        subprocess.run([optimizer] + flags + [str(source), '-o', str(output)], check=True)
        subprocess.run([validator, '--target-env', 'vulkan1.0', str(output)], check=True)
        return output.read_bytes()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--format', choices=('binary', 'c'), default='binary')
    parser.add_argument('--spirv-opt', default='spirv-opt')
    parser.add_argument('--spirv-val', default='spirv-val')
    parser.add_argument('--strip-debug', action='store_true',
                        help='Only for modules whose consumers do not reflect names')
    args = parser.parse_args()
    data = args.input.read_bytes()
    if args.format == 'c':
        words = [int(value, 16) for value in re.findall(rb'0x([0-9a-fA-F]+)', data)]
        data = struct.pack('<%dI' % len(words), *words)
    output = optimize(data, args.spirv_opt, args.spirv_val, args.strip_debug)
    if args.format == 'c':
        words = struct.unpack('<%dI' % (len(output) // 4), output)
        rows = [','.join('0x%08x' % value for value in words[i:i+4])
                for i in range(0, len(words), 4)]
        output = ('{' + ',\n'.join(rows) + '}\n').encode('ascii')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=args.output.parent, delete=False) as tmp:
        tmp.write(output)
        name = Path(tmp.name)
    try:
        name.replace(args.output)
    finally:
        name.unlink(missing_ok=True)


if __name__ == '__main__':
    main()
