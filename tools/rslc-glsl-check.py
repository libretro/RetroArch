#!/usr/bin/env python3
"""Print every corpus stage as GLSL with rslc and have glslangValidator
accept it.

For every stage source in a directory written by rslc_corpus_dump,
`rslc_test --glsl` prints the stage as desktop GLSL and as GLSL ES at
the versions given (default 330 and 300 es - what glcore asks for on
a desktop and on a GLES 3.0 device), with the interface named as the
glcore driver names it, and glslangValidator must accept each text as
that stage.  A rejection is a fault only when SPIRV-Cross printing
glslang's module at the same version is accepted: what the version
itself cannot express (arrays of arrays before 430, textureGather
before 400) fails on the glcore path today as well and is counted
apart.  Stages glslang itself rejects (no .spv beside the .glsl) are
skipped.

  tools/rslc-glsl-check.py <dumpdir> <path to rslc_test> [-v]
        [--desktop 330] [--es 300] [--no-es] [--no-desktop]

Exit status is 1 when any stage fails to print or to validate.
"""
import os
import subprocess
import sys
import tempfile

GLSLANG = os.environ.get('GLSLANG_VALIDATOR', 'glslangValidator')


def cross_rejects(spv, stage, version, flags, tmp):
    args = ['spirv-cross', '--version', version] + (['--es'] if flags else []) + [spv]
    c = subprocess.run(args, capture_output=True, text=True, errors='replace')
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
    desktop = 330
    es = 300
    if '--desktop' in argv:
        desktop = int(argv[argv.index('--desktop') + 1])
    if '--es' in argv:
        es = int(argv[argv.index('--es') + 1])
    targets = []
    if '--no-desktop' not in argv:
        targets.append((str(desktop), []))
    if '--no-es' not in argv:
        targets.append((str(es), ['es']))
    stages = sorted(f for f in os.listdir(dumpdir) if f.endswith('.glsl'))
    ok = fail = both = skipped = 0
    tmp = tempfile.mkdtemp(prefix='rslc-glsl-')
    for f in stages:
        src = os.path.join(dumpdir, f)
        if not os.path.exists(src[:-5] + '.spv'):
            skipped += 1
            continue
        stage = 'vert' if f.endswith('.vert.glsl') else 'frag'
        for version, flags in targets:
            r = subprocess.run([rslc_test, '--glsl', src, version] + flags,
                               capture_output=True, text=True, errors='replace')
            label = '%s %s%s' % (f, version, ' es' if flags else '')
            if r.returncode != 0:
                fail += 1
                print('FAIL %s: %s' % (label, r.stderr.strip()))
                continue
            out = os.path.join(tmp, 'stage.' + stage)
            with open(out, 'w') as fh:
                fh.write(r.stdout)
            g = subprocess.run([GLSLANG, '-S', stage, out], capture_output=True, text=True, errors='replace')
            if g.returncode != 0:
                msg = [l for l in g.stdout.splitlines() if 'ERROR' in l][:1]
                msg = msg[0] if msg else g.stdout.strip()[:200]
                # SPIRV-Cross on glslang's module at the same version: a
                # shader the version itself cannot express fails there too,
                # and that is the glcore path's behaviour today
                if cross_rejects(src[:-5] + '.spv', stage, version, flags, tmp):
                    both += 1
                    if verbose:
                        print('BOTH %s: %s' % (label, msg))
                    continue
                fail += 1
                print('REJECT %s: %s' % (label, msg))
                if verbose:
                    print(r.stdout)
                continue
            ok += 1
    print('%d accepted, %d failed, %d rejected by the version with SPIRV-Cross too, %d stages skipped'
          % (ok, fail, both, skipped))
    return 1 if fail else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
