#!/usr/bin/env python3
"""Print every corpus stage as CafeGLSL-shaped GLSL (what os/wiiu/slang/
slang-cafe hands Nintendo's compiler) and have glslangValidator accept
it as GLSL 150.

`rslc_test --cafe` prints the stage as RSLC_DST_GLSL_CAFE: #version
150, the uniform and push blocks as two uniform blocks, a location on
every in and out.  Desktop GLSL 150 takes those locations only under
GL_ARB_explicit_attrib_location / GL_ARB_separate_shader_objects, so
the check inserts the two #extension lines after #version before
validating; the Wii U tool emits none, as the lane never has.  A
rejection is a fault only when SPIRV-Cross printing glslang's module
at version 150 is accepted: what 150 cannot express fails on the Wii U
today as well and is counted apart.  CafeGLSL itself runs only on a
Cafe SDK box; this is the syntax gate that runs anywhere.

  tools/rslc-cafe-check.py <dumpdir> <path to rslc_test> [-v] [-k outdir]

Exit status is 1 when any stage fails to print or to validate.
"""
import os
import subprocess
import sys
import tempfile

GLSLANG = os.environ.get('GLSLANG_VALIDATOR', 'glslangValidator')
EXT = ('#extension GL_ARB_explicit_attrib_location : require\n'
       '#extension GL_ARB_separate_shader_objects : require\n')


def with_extensions(text):
    nl = text.find('\n')
    return text[:nl + 1] + EXT + text[nl + 1:]


def cross_rejects(spv, stage, tmp):
    c = subprocess.run(['spirv-cross', '--version', '150', spv], capture_output=True, text=True, errors='replace')
    if c.returncode != 0:
        return True
    out = os.path.join(tmp, 'cross.' + stage)
    with open(out, 'w') as fh:
        fh.write(c.stdout)
    g = subprocess.run([GLSLANG, '-S', stage, out], capture_output=True, text=True, errors='replace')
    return g.returncode != 0


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    dumpdir, rslc_test = argv[1], argv[2]
    verbose = '-v' in argv
    keep = argv[argv.index('-k') + 1] if '-k' in argv else None
    if keep:
        os.makedirs(keep, exist_ok=True)
    stages = sorted(f for f in os.listdir(dumpdir) if f.endswith('.glsl'))
    ok = fail = both = skipped = 0
    tmp = tempfile.mkdtemp(prefix='rslc-cafe-')
    for f in stages:
        src = os.path.join(dumpdir, f)
        if not os.path.exists(src[:-5] + '.spv'):
            skipped += 1
            continue
        stage = 'vert' if f.endswith('.vert.glsl') else 'frag'
        r = subprocess.run([rslc_test, '--cafe', src], capture_output=True, text=True, errors='replace')
        if r.returncode != 0:
            fail += 1
            print('FAIL %s: %s' % (f, r.stderr.strip()))
            continue
        if keep:
            with open(os.path.join(keep, f[:-5] + '.' + stage), 'w') as fh:
                fh.write(r.stdout)
        out = os.path.join(tmp, 'stage.' + stage)
        with open(out, 'w') as fh:
            fh.write(with_extensions(r.stdout))
        g = subprocess.run([GLSLANG, '-S', stage, out], capture_output=True, text=True, errors='replace')
        if g.returncode != 0:
            msg = [l for l in g.stdout.splitlines() if 'ERROR' in l][:1]
            msg = msg[0] if msg else g.stdout.strip()[:200]
            if cross_rejects(src[:-5] + '.spv', stage, tmp):
                both += 1
                if verbose:
                    print('BOTH %s: %s' % (f, msg))
                continue
            fail += 1
            print('REJECT %s: %s' % (f, msg))
            if verbose:
                print(r.stdout)
            continue
        ok += 1
    print('%d accepted, %d failed, %d rejected by GLSL 150 with SPIRV-Cross too, %d stages skipped'
          % (ok, fail, both, skipped))
    return 1 if fail else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
