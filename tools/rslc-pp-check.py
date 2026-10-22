#!/usr/bin/env python3
"""Compare rslc's preprocessor against glslang's over a corpus dump.

For every stage source in a directory written by rslc_corpus_dump, the
stage is preprocessed twice: by `rslc_test --pp` (rslc's token stream,
one "<line> <text>" per token) and by `glslangValidator -E` (text,
re-tokenised here with the same lexical rules).  The token texts must
match exactly, and rslc's line numbers must match the line glslang's
output places each token on, which its -E output carries as #line
directives and blank lines.

  tools/rslc-pp-check.py <dumpdir> <path to rslc_test> [-v]

Exit status is the number of stages that differ, capped at 1.
"""
import os
import re
import subprocess
import sys

GLSLANG = os.environ.get('GLSLANG_VALIDATOR', 'glslangValidator')

TOKEN_RE = re.compile(
    r'[A-Za-z_][A-Za-z_0-9]*'
    r'|0[xX][0-9A-Fa-f]*[uU]?'
    r'|(?:\d+\.\d*|\.\d+|\d+)(?:[eE][+-]?\d+)?(?:[fF]|lf|LF|[uU])?'
    r'|<<=|>>=|<<|>>|\+\+|--|<=|>=|==|!=|&&|\|\||\^\^'
    r'|\+=|-=|\*=|/=|%=|&=|\|=|\^='
    r'|[()\[\]{}.,:;=+\-*/%<>!~&|^?#]'
    r'|"[^"\n]*"'
    r'|\S')
LINE_RE = re.compile(r'^\s*#\s*line\s+(\d+)')


def strip_comments(src):
    out = []
    i = 0
    n = len(src)
    while i < n:
        if src.startswith('//', i):
            j = src.find('\n', i)
            if j < 0:
                break
            i = j
        elif src.startswith('/*', i):
            j = src.find('*/', i + 2)
            if j < 0:
                break
            out.append('\n' * src.count('\n', i, j + 2))
            i = j + 2
        else:
            out.append(src[i])
            i += 1
    return ''.join(out)


def glslang_tokens(text):
    """(line, text) pairs from glslangValidator -E output."""
    toks = []
    line = 1
    for raw in strip_comments(text).split('\n'):
        s = raw.strip()
        m = LINE_RE.match(s)
        if m:
            line = int(m.group(1))
            continue
        if s.startswith('#'):
            # #version, #extension, #pragma are passed through by -E
            line += 1
            continue
        for t in TOKEN_RE.findall(raw):
            toks.append((line, t))
        line += 1
    return toks


def rslc_tokens(text):
    toks = []
    for row in text.split('\n'):
        if not row:
            continue
        line, _, t = row.partition(' ')
        toks.append((int(line), t))
    return toks


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    dumpdir, rslc_test = argv[1], argv[2]
    verbose = '-v' in argv
    stages = sorted(f for f in os.listdir(dumpdir) if f.endswith('.glsl'))
    same = diff = fail = 0
    for f in stages:
        path = os.path.join(dumpdir, f)
        stage = 'vert' if f.endswith('.vert.glsl') else 'frag'
        r = subprocess.run([rslc_test, '--pp', path], capture_output=True, text=True, errors='replace')
        if r.returncode != 0:
            fail += 1
            print('FAIL %s: %s' % (f, r.stderr.strip()))
            continue
        g = subprocess.run([GLSLANG, '-E', '-S', stage, path], capture_output=True, text=True, errors='replace')
        if g.returncode != 0:
            fail += 1
            print('FAIL %s: glslang -E failed' % f)
            continue
        a = rslc_tokens(r.stdout)
        b = glslang_tokens(g.stdout)
        if [t for _, t in a] != [t for _, t in b]:
            diff += 1
            if verbose:
                for i, (x, y) in enumerate(zip(a, b)):
                    if x[1] != y[1]:
                        print('DIFF %s token %d: rslc %r (line %d) glslang %r (line %d)'
                              % (f, i, x[1], x[0], y[1], y[0]))
                        break
                else:
                    print('DIFF %s: length %d vs %d' % (f, len(a), len(b)))
            else:
                print('DIFF %s' % f)
            continue
        bad_line = [(x, y) for x, y in zip(a, b) if x[0] != y[0]]
        if bad_line:
            diff += 1
            x, y = bad_line[0]
            print('LINE %s: %r rslc %d glslang %d' % (f, x[1], x[0], y[0]))
            continue
        same += 1
    print('%d identical, %d differ, %d failed, of %d stages' % (same, diff, fail, len(stages)))
    return 1 if (diff or fail) else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
