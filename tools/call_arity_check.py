#!/usr/bin/env python3
"""Find calls that pass the wrong number of arguments, in every platform
branch at once.

Why this exists: a signature change is caught by the compiler at every
call site it compiles. The call sites it does not compile - a console
driver, a webOS-only stand-in, a Win32 context - are found by whichever
CI lane builds that platform, one round trip at a time. Changing
check_window's parameters broke four such lanes in a row that way: the
PS3 driver, two Win32 contexts, and the webOS window check, each behind
an #ifdef nothing on a Linux box parses.

This reads the tree as text instead of compiling it, so a branch guarded
by GEKKO, WEBOS, VITA or _XBOX is examined like any other. For every
function the tree defines or declares, it records how many parameters it
takes; for every call to one of those names, it counts the arguments and
reports a mismatch.

It deliberately says nothing about functions it cannot see the
declaration of (the platform SDKs), about varargs, or about calls
through a function pointer, since none of those can be checked this way
without guessing.

Usage:  python3 tools/call_arity_check.py [path ...]
Exits non-zero when a mismatch is found.
"""
import os, re, sys

MASK = '\x01'
KEYWORDS = {
    'if', 'while', 'for', 'switch', 'return', 'sizeof', 'defined',
    'do', 'else', 'case', 'break', 'continue', 'goto', 'typedef',
    'struct', 'union', 'enum', 'static', 'extern', 'const', 'inline',
    'volatile', 'register', 'signed', 'unsigned', 'void', 'char',
    'short', 'int', 'long', 'float', 'double', '__attribute__',
    'va_start', 'va_arg', 'va_end', 'offsetof', 'alignof',
}
TYPE_START = (r'(?:(?:static|extern|inline|const|unsigned|signed|struct|'
              r'union|enum|void|char|short|int|long|float|double|bool|'
              r'size_t|[A-Za-z_]\w*_t|[A-Za-z_]\w*)\s+)+')


def mask_source(text):
    """Blank comments and literals, preserving offsets and newlines."""
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


def split_args(s):
    """Count top-level comma-separated items in an argument list."""
    if not s.strip():
        return 0
    depth, count = 0, 1
    for ch in s:
        if ch in '([{':
            depth += 1
        elif ch in ')]}':
            depth -= 1
        elif ch == ',' and depth == 0:
            count += 1
    return count


def close_paren(text, open_idx):
    depth = 0
    for j in range(open_idx, len(text)):
        if text[j] == '(':
            depth += 1
        elif text[j] == ')':
            depth -= 1
            if depth == 0:
                return j
    return -1


SIG = re.compile(r'(?<![\w.>])(' + TYPE_START + r')\**\s*(\w+)\s*\(')
CALL = re.compile(r'(?<![\w.>])(\w+)\s*\(')


def sources(roots):
    for root in roots:
        if os.path.isfile(root):
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
                # embedded shader sources are not C
                if fn.endswith(('.cg.h', '.hlsl.h', '.vert.h', '.frag.h',
                                '.glsl.h')):
                    continue
                if '_shaders' in dp:
                    continue
                if fn.endswith(('.c', '.h', '.m')):
                    yield os.path.join(dp, fn)


def main():
    roots = sys.argv[1:] or ['.']
    files = sorted(set(sources(roots)))
    masked = {}
    for p in files:
        try:
            masked[p] = mask_source(open(p, errors='replace').read())
        except OSError:
            pass

    # name -> set of parameter counts seen in declarations/definitions.
    # A harness stub deliberately declares a platform function with a
    # shape of its own (IsWindowVisible(void) under samples/), so those
    # files are checked as callers but never define what an arity is.
    def is_stub(path):
        return ('/samples/' in path or '/test/' in path
                or os.path.basename(path).startswith('stubs_')
                or os.path.basename(path).endswith('_test.c')
                or '/platform_stubs/' in path or '/scripts/stubs/' in path)

    arity, varargs, renamed = {}, set(), set()
    for p, t in masked.items():
        if is_stub(p):
            continue
        # an object-like #define that renames a function, and any
        # function-like macro: neither can be judged by counting the
        # parameters of a function that happens to share the name.
        for m in re.finditer(r'#\s*define\s+(\w+)\s+(\w+)\s*$', t, re.M):
            renamed.add(m.group(1))
        for m in re.finditer(r'#\s*define\s+(\w+)\(', t):
            renamed.add(m.group(1))
        for m in SIG.finditer(t):
            name = m.group(2)
            if name in KEYWORDS:
                continue
            o = m.end() - 1
            c = close_paren(t, o)
            if c < 0:
                continue
            tail = t[c + 1:c + 3].lstrip()
            # a definition or a prototype, not a call inside an expression
            if tail[:1] not in ('{', ';'):
                continue
            params = t[o + 1:c]
            if '...' in params:
                varargs.add(name)
                continue
            if params.strip() in ('void', ''):
                n = 0
            else:
                n = split_args(params)
            arity.setdefault(name, set()).add(n)

    bad = 0
    for p, t in sorted(masked.items()):
        for m in CALL.finditer(t):
            name = m.group(1)
            if (name in KEYWORDS or name in varargs or name in renamed
                    or name not in arity):
                continue
            counts = arity[name]
            # a name declared with more than one arity in the tree is a
            # macro shim or a per-platform prototype; not ours to judge
            if len(counts) != 1:
                continue
            o = m.end() - 1
            c = close_paren(t, o)
            if c < 0:
                continue
            # skip the declaration/definition itself
            tail = t[c + 1:c + 3].lstrip()
            if tail[:1] in ('{', ';') and SIG.search(t, max(0, m.start() - 80),
                                                     m.end()):
                continue
            span = t[o + 1:c]
            # An argument list carrying a preprocessor conditional holds
            # the arguments of every branch at once, so counting them
            # here would judge a call no build ever makes.
            if re.search(r'^\s*#', span, re.M):
                continue
            got = split_args(span)
            want = next(iter(counts))
            if got != want:
                line = t[:m.start()].count('\n') + 1
                print('%s:%d: %s takes %d, called with %d'
                      % (p, line, name, want, got))
                bad += 1
    print('%d call(s) with the wrong argument count' % bad)
    return 1 if bad else 0


sys.exit(main())
