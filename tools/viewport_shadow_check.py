#!/usr/bin/env python3
"""Report names that mean a video_viewport_t in one place and something
else in another.

video_viewport_t's members travel packed - pos, dims, full_dims - while
several other viewport types in the tree keep plain x/y/width/height of
their own: VkViewport, MTLViewport, D3DVIEWPORT9, rsx_viewport_t, the
overlay's own viewport rect. They are commonly held in a local called
'vp' or 'viewport', the same names the frontend's viewport uses, so a
tree-wide rename driven by those names silently rewrites the wrong one
and the break only shows up on a platform whose driver nothing here
compiles.

This finds the collisions by declaration rather than by compiling: for
every file, any name declared BOTH as a video_viewport_t somewhere in
the tree AND as some other type in this file. Each hit is a place to
read before trusting a rename; a file with no hits cannot have the
confusion.

Usage: python3 tools/viewport_shadow_check.py [path ...]
Exits non-zero if any collision is found, so it can gate a rename.
"""
import os, re, sys

VP_DECL = re.compile(
    r'(?:struct\s+video_viewport|video_viewport_t)\s*\**\s*'
    r'(\w+)\s*(?:\[[^\]]*\])?\s*[;,)=]')
ANY_DECL = re.compile(
    r'(?<![\w>])([A-Za-z_]\w*(?:\s*\*)*)\s+\**\s*(\w+)\s*'
    r'(?:\[[^\]]*\])?\s*[;,)=]')
NOT_A_TYPE = {
    'return', 'case', 'else', 'if', 'while', 'for', 'switch', 'do',
    'sizeof', 'typedef', 'struct', 'union', 'enum', 'const', 'static',
    'extern', 'volatile', 'register', 'inline', 'goto', 'break',
    'continue', 'default', 'video_viewport_t', 'video_viewport',
    'video_viewport_settings_t',
}
MASK = '\x01'


def mask_source(text):
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


def sources(roots):
    for root in roots:
        if os.path.isfile(root):
            yield root
            continue
        for dp, dns, fns in os.walk(root):
            dns[:] = [d for d in dns if d not in ('.git', 'deps')]
            for fn in fns:
                if fn.endswith(('.c', '.h', '.cpp', '.m', '.mm', '.hpp')):
                    yield os.path.join(dp, fn)


def main():
    roots = sys.argv[1:] or ['.']
    files = sorted(set(sources(roots)))
    masked = {}
    vp_names = set()
    for p in files:
        try:
            masked[p] = mask_source(open(p, errors='replace').read())
        except OSError:
            continue
        if 'video_viewport' in masked[p]:
            vp_names |= {m.group(1) for m in VP_DECL.finditer(masked[p])}
    vp_names.discard('video_viewport')

    # a type that carries its own plain axes is the hazard; one already
    # packed onto pos/dims is not, and an external type we cannot see the
    # definition of is reported either way.
    axes = re.compile(r'\b(?:x|y|w|h|width|height)\s*(?:,\s*\w+\s*)*;')
    defs = {}
    for p in files:
        t = masked.get(p) or ''
        for m in re.finditer(r'(?:typedef\s+)?struct(?:\s+\w+)?\s*\{'
                             r'([^{}]*)\}\s*(\w+)\s*;', t):
            defs[m.group(2)] = m.group(1)

    def hazardous(ty):
        body = defs.get(ty)
        if body is None:
            return True              # not defined here: assume plain axes
        return bool(axes.search(body))

    hits = 0
    for p in files:
        text = masked.get(p)
        if not text:
            continue
        seen = {}
        for m in ANY_DECL.finditer(text):
            ty, name = m.group(1).strip(), m.group(2)
            if name not in vp_names or ty in NOT_A_TYPE:
                continue
            if 'video_viewport' in ty:
                continue
            if not hazardous(ty):
                continue
            seen.setdefault((ty, name), text[:m.start()].count('\n') + 1)
        if seen:
            print('=== %s ===' % p)
            for (ty, name), line in sorted(seen.items(),
                                           key=lambda kv: kv[1]):
                print('  %5d  %s %s%s' % (line, ty, name,
                      '' if ty in defs else '   (no definition here)'))
                hits += 1

    print('%d name collision(s)' % hits)
    return 1 if hits else 0


sys.exit(main())
