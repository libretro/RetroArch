#!/usr/bin/env python3
"""An extern "C" definition in a C++ file takes what its C prototype says.

Why this exists: uwp_main.cpp carries the UWP definitions of several
win32_common.h functions (win32_check_window, win32_set_video_mode,
win32_get_client_rect, ...) inside an extern "C" block, and it does not
include win32_common.h. When win32_check_window's size parameters were
folded into one word, every C caller was updated and the compiler saw
each one - but a definition that never sees its prototype is not
checked against it, and the linker matches on the name alone. The UWP
build linked a five-parameter body to four-argument callers, and Xbox
crashed on boot (bisected to 0b2613d).

This reads the tree as text: for each function defined inside an
extern "C" region of a .cpp or .mm file, it finds the prototype(s) of
that name in the .h files and compares the parameter lists, type by
type with the parameter names stripped. A definition whose parameters
differ from its prototype's is reported. A definition with no
prototype anywhere is not (a C++ file's own C-linkage helper is not
ours to judge).

Usage:  python3 tools/extern_c_prototype_check.py [--selftest] [path ...]
Exits non-zero when a mismatch is found.
"""
import os, re, sys, tempfile

MASK = '\x01'
KEYWORDS = {
    'if', 'while', 'for', 'switch', 'return', 'sizeof', 'defined', 'do',
    'else', 'case', 'typedef', 'struct', 'union', 'enum', 'static',
    'extern', 'const', 'inline', 'volatile', 'register', 'signed',
    'unsigned', 'void', 'char', 'short', 'int', 'long', 'float',
    'double', '__attribute__', 'catch', 'new', 'delete', 'throw',
}
TYPE_START = (r'(?:(?:static|extern|inline|const|unsigned|signed|struct|'
              r'union|enum|void|char|short|int|long|float|double|bool|'
              r'size_t|[A-Za-z_]\w*_t|[A-Za-z_]\w*)\s+)+')
SIG = re.compile(r'(?<![\w.>:])(' + TYPE_START + r')\**\s*(\w+)\s*\(')
EXTERN_C = re.compile(r'extern\s+' + MASK + r'{3}\s*({)?')


def mask_source(text):
    """Blank comments and string/char literals, keeping newlines so line
    numbers survive. A masked "C" is three MASK bytes."""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c == '/' and i + 1 < n and text[i + 1] == '*':
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append(''.join(MASK if ch != '\n' else '\n'
                               for ch in text[i:j]))
            i = j
        elif c == '/' and i + 1 < n and text[i + 1] == '/':
            j = text.find('\n', i)
            j = n if j < 0 else j
            out.append(MASK * (j - i))
            i = j
        elif c in '"\'':
            q, j = c, i + 1
            while j < n and text[j] != q:
                j += 2 if text[j] == '\\' else 1
            j = min(j + 1, n)
            out.append(''.join(MASK if ch != '\n' else '\n'
                               for ch in text[i:j]))
            i = j
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def close_bracket(text, open_idx, o='(', c=')'):
    depth = 0
    for j in range(open_idx, len(text)):
        if text[j] == o:
            depth += 1
        elif text[j] == c:
            depth -= 1
            if depth == 0:
                return j
    return -1


def split_params(s):
    """Top-level comma-separated items of a parameter list."""
    items, depth, cur = [], 0, []
    for ch in s:
        if ch in '([{<':
            depth += 1
        elif ch in ')]}>':
            depth -= 1
        if ch == ',' and depth == 0:
            items.append(''.join(cur))
            cur = []
        else:
            cur.append(ch)
    items.append(''.join(cur))
    return items


def normalize_param(p):
    """One parameter as a type only: `unsigned *width` -> `unsigned*`,
    `RECT* rect` -> `RECT*`, `void` -> `void`."""
    p = re.sub(r'\s+', ' ', p.replace(MASK, '')).strip()
    p = re.sub(r'\[[^\]]*\]', '*', p)      # int a[] == int *a
    p = re.sub(r'\s*\*\s*', '*', p)
    toks = re.findall(r'\w+|\*', p)
    # the trailing identifier is the name unless it is the whole type
    if len(toks) >= 2 and re.match(r'^\w+$', toks[-1]) \
            and toks[-1] not in KEYWORDS and not toks[-1].endswith('_t') \
            and toks[-2] not in ('struct', 'union', 'enum'):
        toks = toks[:-1]
    toks = [t for t in toks if t not in ('const', 'volatile', 'register')]
    return ''.join(t if t == '*' else ' ' + t for t in toks).strip()


def normalize_params(s):
    s = s.strip()
    if s in ('', 'void'):
        return ()
    return tuple(normalize_param(x) for x in split_params(s))


def extern_c_regions(t):
    """Spans of text under C linkage: each `extern "C" { ... }` block,
    and the single declaration after a bare `extern "C"`."""
    for m in EXTERN_C.finditer(t):
        if m.group(1):
            end = close_bracket(t, m.end() - 1, '{', '}')
            yield (m.end(), len(t) if end < 0 else end)
        else:
            end = t.find(';', m.end())
            end = t.find('{', m.end()) if end < 0 else \
                min(end, (t.find('{', m.end()) + 1 or len(t) + 1) - 1)
            yield (m.end(), len(t) if end < 0 else end + 1)


def signatures(t, lo=0, hi=None):
    """(name, params, kind, line) for every prototype (kind ';') or
    definition (kind '{') in t[lo:hi]."""
    hi = len(t) if hi is None else hi
    for m in SIG.finditer(t, lo, hi):
        name = m.group(2)
        if name in KEYWORDS:
            continue
        o = m.end() - 1
        c = close_bracket(t, o)
        if c < 0 or c > hi:
            continue
        # a definition or a prototype, not a call: the body's brace may
        # sit on its own indented line, so skip all whitespace first
        tail = re.match(r'[\s' + MASK + r']*([{;])', t[c + 1:])
        if not tail:
            continue
        params = t[o + 1:c]
        if re.search(r'^\s*#', params, re.M) or '...' in params:
            continue
        yield name, normalize_params(params), tail.group(1), \
            t[:m.start()].count('\n') + 1


def sources(roots, exts):
    for root in roots:
        if os.path.isfile(root):
            if root.endswith(exts):
                yield root
            continue
        for dp, dns, fns in os.walk(root):
            dns[:] = [d for d in dns
                      if d not in ('.git', 'deps', 'pkg', 'media',
                                   'dxsdk', 'MESA', 'vulkan', 'GL',
                                   'GLES2', 'GLES3', 'EGL', 'KHR',
                                   'vita_pib', 'libgo2', 'libogc',
                                   'lwbt', 'lwip', 'libwiikeyboard')]
            for fn in fns:
                if fn.endswith(('.cg.h', '.hlsl.h', '.vert.h', '.frag.h',
                                '.glsl.h')) or '_shaders' in dp:
                    continue
                if fn.endswith(exts):
                    yield os.path.join(dp, fn)


def check(roots):
    protos = {}  # name -> [(path, line, params)]
    for p in sorted(set(sources(roots, ('.h',)))):
        try:
            t = mask_source(open(p, errors='replace').read())
        except OSError:
            continue
        for name, params, kind, line in signatures(t):
            if kind == ';':
                protos.setdefault(name, []).append((p, line, params))

    bad = 0
    for p in sorted(set(sources(roots, ('.cpp', '.mm')))):
        try:
            t = mask_source(open(p, errors='replace').read())
        except OSError:
            continue
        for lo, hi in extern_c_regions(t):
            for name, params, kind, line in signatures(t, lo, hi):
                if kind != '{' or name not in protos:
                    continue
                for hp, hline, hparams in protos[name]:
                    if hparams != params:
                        print('%s:%d: %s(%s) is defined here but %s:%d '
                              'declares %s(%s)'
                              % (p, line, name, ', '.join(params),
                                 hp, hline, name, ', '.join(hparams)))
                        bad += 1
    print('%d extern "C" definition(s) disagree with their prototype'
          % bad)
    return bad


def selftest():
    hdr = ('void win32_check_window(void *data,\n'
           '      bool *quit,\n'
           '      bool *resize, unsigned *dims);\n'
           'bool win32_get_client_rect(RECT* rect);\n'
           'void helper_no_proto_in_cpp(int x);\n')
    good = ('extern "C" {\n'
            '   void win32_check_window(void *data,\n'
            '         bool *quit, bool *resize, unsigned *dims)\n'
            '   { *dims = 0; }\n'
            '   bool win32_get_client_rect(RECT* rect) { return true; }\n'
            '   /* a C++ file\'s own helper: no prototype, not judged */\n'
            '   int cpp_only_thing(float a, float b) { return 0; }\n'
            '}\n')
    bad_count = good.replace(
        'bool *quit, bool *resize, unsigned *dims)',
        'bool *quit, bool *resize, unsigned *width, unsigned *height)')
    bad_type = good.replace('RECT* rect', 'unsigned *rect')
    cases = [('matching definition', good, 0),
             ('parameter count drift (the 0b2613d Xbox crash)', bad_count, 1),
             ('parameter type drift', bad_type, 1)]
    ok = True
    for label, body, want in cases:
        with tempfile.TemporaryDirectory() as d:
            open(os.path.join(d, 'x_common.h'), 'w').write(hdr)
            open(os.path.join(d, 'x_main.cpp'), 'w').write(body)
            got = check([d])
        print('%s: %s' % ('ok  ' if got == want else 'FAIL', label))
        ok &= (got == want)
    return 0 if ok else 1


if __name__ == '__main__':
    args = sys.argv[1:]
    if args[:1] == ['--selftest']:
        sys.exit(selftest())
    sys.exit(1 if check(args or ['.']) else 0)
