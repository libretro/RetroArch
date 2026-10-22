#!/usr/bin/env python3
"""Compare rslc's SPIR-V against glslang's over a corpus dump.

For every stage in a directory written by rslc_corpus_dump, the stage is
compiled with `rslc_test --spirv`, validated with spirv-val, and its
interface is compared with the glslang module the dump carries next to
it: the set of module-scope variables by (name, storage class), every
decoration that a driver or reflection reads (Location, Binding,
DescriptorSet, BuiltIn, Block, Flat, NoPerspective, Centroid,
RelaxedPrecision on interface variables), member Offset, MatrixStride
and ArrayStride of block members, capabilities and execution modes.
Instruction streams are not compared beyond the float constants: the gate is that a Vulkan driver
and SPIRV-Cross see the same shader interface, and that every constant
expression folded to the same bits.

  tools/rslc-corpus-check.py <dumpdir> <path to rslc_test> [-v] [-k] [-C]

-k keeps the rslc modules next to the glslang ones as *.rslc.spv.
-C leaves the float constant sets out of the comparison.
Exit status is 1 when any stage differs or fails.
"""
import os
import re
import subprocess
import sys
import tempfile

VAR_DECS = ('Location', 'Binding', 'DescriptorSet', 'BuiltIn', 'Block', 'Flat',
            'NoPerspective', 'Centroid', 'Sample', 'Invariant', 'RelaxedPrecision')
MEMBER_DECS = ('Offset', 'MatrixStride', 'ArrayStride', 'ColMajor', 'RowMajor', 'BuiltIn')
OPAQUE_SC = ('Input', 'Output', 'Uniform', 'UniformConstant', 'PushConstant')


def disasm(path):
    r = subprocess.run(['spirv-dis', '--raw-id', path], capture_output=True, text=True)
    if r.returncode != 0:
        return None
    return r.stdout


def interface(dis, no_consts=False):
    """A canonical, order-free description of a module's interface,
    plus the set of float constants unless no_consts: glslang folds
    constant expressions in double precision, and a module that folds
    differently is a different shader."""
    names = {}
    member_names = {}
    types = {}       # id -> (kind, operands)
    variables = {}   # id -> storage class
    decs = set()
    mdecs = {}       # (struct id, member) -> set((dec, value))
    tdecs = {}       # type id -> set((dec, value))  (ArrayStride)
    caps = set()
    modes = set()
    consts = {}      # id -> float constant value: folding must round as glslang does
    used = set()     # ids any instruction reads; glslang leaves a few constants dead
    for line in dis.splitlines():
        line = line.strip()
        m = re.match(r'%\d+ = OpConstant %(\d+) (\S+)', line)
        if m:
            if types.get(m.group(1), ('',))[0] == 'Float':
                consts[line.split()[0].lstrip('%')] = m.group(2)
            continue
        if line.startswith('OpName') or line.startswith('OpDecorate'):
            continue
        for ref in re.findall(r'%(\d+)', line.split('=', 1)[-1] if '=' in line else line):
            used.add(ref)
        m = re.match(r'OpName %(\d+) "(.*)"', line)
        if m:
            names[m.group(1)] = m.group(2)
            continue
        m = re.match(r'OpMemberName %(\d+) (\d+) "(.*)"', line)
        if m:
            member_names[(m.group(1), int(m.group(2)))] = m.group(3)
            continue
        m = re.match(r'OpCapability (\w+)', line)
        if m:
            caps.add(m.group(1))
            continue
        m = re.match(r'OpExecutionMode %\d+ (.*)', line)
        if m:
            modes.add(m.group(1))
            continue
        m = re.match(r'%(\d+) = OpType(\w+)\s*(.*)', line)
        if m:
            types[m.group(1)] = (m.group(2), m.group(3).split())
            continue
        m = re.match(r'%(\d+) = OpVariable %(\d+) (\w+)', line)
        if m:
            variables[m.group(1)] = (m.group(3), m.group(2))
            continue
        m = re.match(r'OpDecorate %(\d+) (\w+)\s*(.*)', line)
        if m:
            tid, dec, val = m.group(1), m.group(2), m.group(3).strip()
            if dec == 'ArrayStride':
                tdecs.setdefault(tid, set()).add((dec, val))
            else:
                decs.add((tid, dec, val))
            continue
        m = re.match(r'OpMemberDecorate %(\d+) (\d+) (\w+)\s*(.*)', line)
        if m:
            key = (m.group(1), int(m.group(2)))
            mdecs.setdefault(key, set()).add((m.group(3), m.group(4).strip()))
            continue

    def pointee(ptr):
        t = types.get(ptr)
        if t and t[0] == 'Pointer':
            return t[1][1].lstrip('%')
        return None

    def strides(tid, out):
        """ArrayStride of a (possibly nested) array type, innermost last."""
        t = types.get(tid)
        while t and t[0] == 'Array':
            out.append(tuple(sorted(tdecs.get(tid, ()))))
            tid = t[1][0].lstrip('%')
            t = types.get(tid)

    out = set()
    for vid, (sc, ptr) in variables.items():
        if sc not in OPAQUE_SC:
            continue
        name = names.get(vid, '')
        out.add(('var', name, sc))
        for (tid, dec, val) in decs:
            if tid == vid and dec in VAR_DECS:
                out.add(('dec', name, sc, dec, val))
        st = pointee(ptr)
        t = types.get(st)
        if not t:
            continue
        if t[0] == 'Struct':
            sname = names.get(st, '')
            for (tid, dec, val) in decs:
                if tid == st and dec in VAR_DECS:
                    out.add(('sdec', sname, dec, val))
            for i, member in enumerate(t[1]):
                mname = member_names.get((st, i), '')
                for (dec, val) in mdecs.get((st, i), ()):
                    if dec in MEMBER_DECS:
                        out.add(('mdec', sname, i, mname, dec, val))
                s = []
                strides(member.lstrip('%'), s)
                for depth, stride in enumerate(s):
                    out.add(('mstride', sname, i, mname, depth, stride))
    for c in caps:
        out.add(('cap', c))
    if not no_consts:
        for cid, c in consts.items():
            if cid in used:
                out.add(('const', c))
    for mo in modes:
        out.add(('mode', mo))
    return out


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    dumpdir, rslc_test = argv[1], argv[2]
    verbose = '-v' in argv
    keep = '-k' in argv
    no_consts = '-C' in argv
    stages = sorted(f for f in os.listdir(dumpdir) if f.endswith('.glsl'))
    same = diff = fail = skipped = 0
    tmp = tempfile.mkdtemp(prefix='rslc-check-')
    for f in stages:
        src = os.path.join(dumpdir, f)
        ref = src[:-5] + '.spv'
        if not os.path.exists(ref):
            skipped += 1
            continue
        out = os.path.join(dumpdir, f[:-5] + '.rslc.spv') if keep else os.path.join(tmp, 'out.spv')
        r = subprocess.run([rslc_test, '--spirv', src, out], capture_output=True, text=True, errors='replace')
        if r.returncode != 0:
            fail += 1
            print('FAIL %s: %s' % (f, r.stderr.strip()))
            continue
        v = subprocess.run(['spirv-val', '--target-env', 'vulkan1.0', out], capture_output=True, text=True)
        if v.returncode != 0:
            fail += 1
            print('INVALID %s: %s' % (f, v.stderr.strip().splitlines()[0] if v.stderr else v.stdout.strip()))
            continue
        a = disasm(out)
        b = disasm(ref)
        if a is None or b is None:
            fail += 1
            print('FAIL %s: spirv-dis' % f)
            continue
        ia = interface(a, no_consts)
        ib = interface(b, no_consts)
        if ia != ib:
            diff += 1
            print('DIFF %s' % f)
            if verbose:
                for item in sorted(ia - ib, key=str):
                    print('   rslc only:    %s' % (item,))
                for item in sorted(ib - ia, key=str):
                    print('   glslang only: %s' % (item,))
            continue
        same += 1
    print('%d identical, %d differ, %d failed, %d skipped, of %d stages'
          % (same, diff, fail, skipped, len(stages)))
    return 1 if (diff or fail) else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
