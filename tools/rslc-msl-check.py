#!/usr/bin/env python3
"""Type-check rslc's MSL for every stage of a corpus dump.

  tools/rslc-msl-check.py <dumpdir> <rslc_test> [-k] [-o outdir] [-v]

No Metal compiler runs off a Mac, so the text is checked two ways:

- clang++ -fsyntax-only against tools/rslc-msl-shim, a stand-in
  metal_stdlib that declares Metal's types and functions with their
  result types (vector constructors become make_floatN() calls first,
  since clang's extended vectors have no constructor syntax, and
  [[attributes]] are stripped).  This is the C++ half of MSL:
  undeclared names, parameter lists, address-space constness,
  overloads, swizzles.
- the interface against the GLSL source: every sampler at
  [[texture(binding)]] / [[sampler(binding)]], the uniform block at
  [[buffer(0)]] and the push block at [[buffer(1)]], attributes and
  varyings at their locations, outputs at [[color(location)]].

A stage glslang rejects (no .spv in the dump) is skipped.  -k keeps the
MSL in outdir.  Exit status is 1 when any stage fails either check;
`xcrun metal -c` over the kept files is the gate of record on a Mac.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

SHIM = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'rslc-msl-shim')
CTOR = re.compile(r'\b(float|int|uint|bool|half)([234])\(')


def ctors(text):
    """float3(a, b) -> make_float3(a, b)"""
    return CTOR.sub(r'make_\1\2(', text)


def preprocessed(rslc_test, src):
    """The stage as rslc's preprocessor leaves it, one token per line
    (`--pp`), joined so declarations in dead #if branches and comments
    are out of the picture."""
    r = subprocess.run([rslc_test, '--pp', src], capture_output=True, text=True, errors='replace')
    toks = []
    for line in r.stdout.splitlines():
        parts = line.split(' ', 1)
        if len(parts) == 2:
            toks.append(parts[1])
    return ' '.join(toks)


def interface_errors(glsl, msl, fragment):
    errs = []
    for m in re.finditer(r'layout\s*\(([^)]*)\)\s*uniform\s+[iu]?sampler2D\s+(\w+)', glsl):
        b = re.search(r'binding\s*=\s*(\d+)', m.group(1))
        if not b:
            continue
        name = m.group(2)
        if not re.search(r'texture2d<\w+>\s+%s_? \[\[texture\(%s\)\]\]' % (re.escape(name), b.group(1)), msl):
            errs.append('sampler %s not at texture(%s)' % (name, b.group(1)))
    if re.search(r'push_constant', glsl) and '[[buffer(1)]]' not in msl:
        errs.append('push block not at buffer(1)')
    if re.search(r'layout\s*\([^)]*binding\s*=\s*0[^)]*\)\s*uniform\s+\w+\s*\{', glsl) and '[[buffer(0)]]' not in msl:
        errs.append('uniform block not at buffer(0)')
    quals = r'(?:flat\s+|noperspective\s+|smooth\s+|centroid\s+|sample\s+|lowp\s+|mediump\s+|highp\s+)*'
    for m in re.finditer(r'layout\s*\(\s*location\s*=\s*(\d+)\s*\)\s*' + quals + r'(in|out)\s+' + quals + r'\w+\s+(\w+)', glsl):
        loc, kind, name = m.group(1), m.group(2), m.group(3)
        if kind == 'in' and not fragment:
            want = r'%s_? \[\[attribute\(%s\)\]\]' % (re.escape(name), loc)
        elif kind == 'out' and fragment:
            want = r'%s_? \[\[color\(%s\)\]\]' % (re.escape(name), loc)
        else:
            want = r'%s_? \[\[user\(locn%s\)' % (re.escape(name), loc)
        if not re.search(want, msl):
            errs.append('%s %s not at location %s' % (kind, name, loc))
    return errs


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    dumpdir, rslc_test = argv[1], argv[2]
    keep = '-k' in argv
    verbose = '-v' in argv
    outdir = argv[argv.index('-o') + 1] if '-o' in argv else tempfile.mkdtemp(prefix='rslc-msl-')
    os.makedirs(outdir, exist_ok=True)
    stages = sorted(f for f in os.listdir(dumpdir) if f.endswith('.glsl'))
    ok = failed = unprinted = skipped = 0
    for f in stages:
        src = os.path.join(dumpdir, f)
        if not os.path.exists(src[:-5] + '.spv'):
            skipped += 1
            continue
        base = f[:-5]
        out = os.path.join(outdir, base + '.metal')
        r = subprocess.run([rslc_test, '--msl', src], capture_output=True, text=True, errors='replace')
        if r.returncode != 0:
            print('UNPRINTED %s: %s' % (base, r.stderr.strip()))
            unprinted += 1
            continue
        msl = r.stdout
        with open(out, 'w') as o:
            o.write(msl)
        checked = re.sub(r'\[\[[^\]]*\]\]', '', ctors(msl))
        cpp = os.path.join(outdir, base + '.check.cpp')
        with open(cpp, 'w') as o:
            o.write(checked)
        c = subprocess.run(['clang++', '-fsyntax-only', '-std=c++14', '-w', '-fsingle-precision-constant',
                            '-I', SHIM, cpp],
                           capture_output=True, text=True, errors='replace')
        errs = []
        if c.returncode != 0:
            errs.append(c.stderr.strip().splitlines()[0] if c.stderr.strip() else 'clang++ failed')
            if verbose:
                errs.append(c.stderr.strip())
        errs += interface_errors(preprocessed(rslc_test, src), msl, f.endswith('.frag.glsl'))
        if errs:
            print('FAIL %s: %s' % (base, ' | '.join(errs)))
            failed += 1
        else:
            ok += 1
        if not keep:
            os.unlink(cpp)
    print('%d accepted, %d failed, %d not printed, %d skipped, of %d stages'
          % (ok, failed, unprinted, skipped, len(stages)))
    if not keep:
        shutil.rmtree(outdir, ignore_errors=True)
    return 1 if (failed or unprinted) else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
