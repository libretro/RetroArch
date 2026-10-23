#!/usr/bin/env python3
"""Compile rslc's HLSL for every stage of a corpus dump with a real
HLSL compiler, next to SPIRV-Cross's HLSL of the same stage.

  tools/rslc-hlsl-check.py <dumpdir> <rslc_test> <hlslc> [-m 40|50] [-k]
                           [-o outdir] [-t seconds]

<hlslc> is a command that takes the list file tools/rslc-hlslc.c
reads, one "profile entry file" per line, and prints "ok <file>" or
"FAIL <file>" per entry: tools/rslc-hlslc.c built with MinGW and run
under Wine ("wine hlslc.exe"), or natively on Windows.  The compiler
is whatever D3DCompile that process links: Wine's own (vkd3d-shader)
under Wine, d3dcompiler_47.dll on Windows.

Every stage is printed at shader model -m (both 40 and 50 without
-m), the same model SPIRV-Cross is asked for, and both texts go
through the compiler.  The gate is that nothing rslc prints is
rejected where the Cross text of the same stage is accepted: a stage
the compiler rejects or cannot finish for both is recorded, not
counted, since the driver fails it today too.  A compiler that hangs
on a stage is killed after -t seconds (default 60) and the run goes
on from the next entry.  -k keeps the HLSL in outdir (default a
temporary directory).  Exit status is 1 when any stage regresses.
"""
import os
import select
import shutil
import subprocess
import sys
import tempfile
import time


def profile(name, model):
    return ('vs_%d_0' if name.endswith('.vert.glsl') else 'ps_%d_0') % (model // 10)


def run_compiler(hlslc, entries, timeout, results):
    """entries: list of (profile, path).  Fills results[path] with 'ok',
    'fail:<message>', 'crash' or 'timeout'; restarts the compiler
    after a crash or a hang, from the entry after the one that caused
    it."""
    start = 0
    while start < len(entries):
        fd, listname = tempfile.mkstemp(prefix='rslc-hlsl-', suffix='.txt')
        with os.fdopen(fd, 'w') as f:
            for prof, path in entries[start:]:
                f.write('%s main %s\n' % (prof, path))
        proc = subprocess.Popen(hlslc + ['-l', listname], stdout=subprocess.PIPE,
                                stderr=subprocess.DEVNULL)
        done = 0
        cur = None
        msgs = []
        buf = ''
        last = time.time()
        hung = False
        while True:
            r, _, _ = select.select([proc.stdout], [], [], 1.0)
            if r:
                chunk = os.read(proc.stdout.fileno(), 65536).decode('utf-8', 'replace')
                if not chunk:
                    break
                buf += chunk
                last = time.time()
                while '\n' in buf:
                    line, buf = buf.split('\n', 1)
                    if line.startswith('ok ') or line.startswith('FAIL '):
                        if cur is not None:
                            results[cur[1]] = cur[0] if cur[0] == 'ok' else 'fail:' + ' / '.join(msgs)
                        kind = 'ok' if line.startswith('ok ') else 'fail'
                        cur = (kind, entries[start + done][1])
                        msgs = []
                        done += 1
                    elif line.startswith('   '):
                        msgs.append(line.strip())
            elif proc.poll() is not None:
                break
            elif time.time() - last > timeout:
                hung = True
                proc.kill()
                break
        proc.wait()
        try:
            os.unlink(listname)
        except OSError:
            pass
        if cur is not None:
            results[cur[1]] = cur[0] if cur[0] == 'ok' else 'fail:' + ' / '.join(msgs)
        if start + done >= len(entries):
            break
        # the entry after the last reported one took the compiler down
        results[entries[start + done][1]] = 'timeout' if hung else 'crash'
        start += done + 1


def main(argv):
    if len(argv) < 4:
        print(__doc__)
        return 2
    dumpdir, rslc_test, hlslc = argv[1], argv[2], argv[3].split()
    models = [40, 50]
    keep = False
    outdir = None
    timeout = 60
    i = 4
    while i < len(argv):
        if argv[i] == '-m':
            models = [int(argv[i + 1])]
            i += 2
        elif argv[i] == '-k':
            keep = True
            i += 1
        elif argv[i] == '-o':
            outdir = argv[i + 1]
            i += 2
        elif argv[i] == '-t':
            timeout = float(argv[i + 1])
            i += 2
        else:
            print('unknown option %s' % argv[i])
            return 2
    if outdir is None:
        outdir = tempfile.mkdtemp(prefix='rslc-hlsl-')
    stages = sorted(f for f in os.listdir(dumpdir) if f.endswith('.glsl'))
    regressed = 0
    for model in models:
        mdir = os.path.join(outdir, str(model))
        os.makedirs(mdir, exist_ok=True)
        entries = []
        printed = {}
        for f in stages:
            src = os.path.join(dumpdir, f)
            spv = src[:-5] + '.spv'
            base = f[:-5]
            ours = os.path.join(mdir, base + '.rslc.hlsl')
            theirs = os.path.join(mdir, base + '.cross.hlsl')
            r = subprocess.run([rslc_test, '--hlsl', src, str(model)], capture_output=True,
                               text=True, errors='replace')
            if r.returncode != 0:
                printed[base] = ('rslc: ' + r.stderr.strip(), None)
            else:
                with open(ours, 'w') as o:
                    o.write(r.stdout)
                printed[base] = (None, ours)
            if os.path.exists(spv):
                c = subprocess.run(['spirv-cross', '--hlsl', '--shader-model', str(model), spv,
                                    '--output', theirs], capture_output=True, text=True)
                if c.returncode == 0:
                    entries.append((profile(f, model), theirs))
            if printed[base][1]:
                entries.append((profile(f, model), ours))
        results = {}
        run_compiler(hlslc, entries, timeout, results)
        same = excluded = failed = unprinted = 0
        for base in sorted(printed):
            err, ours = printed[base]
            theirs = os.path.join(mdir, base + '.cross.hlsl')
            cross = results.get(theirs, 'missing')
            if err:
                # rslc did not print: a regression unless Cross fails too
                if cross == 'ok':
                    print('UNPRINTED %d %s: %s' % (model, base, err))
                    unprinted += 1
                    regressed += 1
                else:
                    excluded += 1
                continue
            res = results.get(ours, 'missing')
            if res == 'ok':
                same += 1
            elif cross != 'ok':
                excluded += 1
            else:
                print('FAIL %d %s: %s' % (model, base, res))
                failed += 1
                regressed += 1
        print('SM%d: %d accepted, %d rejected, %d not printed, %d excluded (SPIRV-Cross'
              ' fails there too), of %d stages' % (model, same, failed, unprinted, excluded,
                                                   len(printed)))
    if not keep:
        shutil.rmtree(outdir, ignore_errors=True)
    return 1 if regressed else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
