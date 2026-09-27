#!/usr/bin/env python3
"""Find readers left behind by a struct member that a change removed.

Why this exists: when a member goes away, the compiler finds every
reader it compiles. The readers it does not compile are the ones a
Linux box never sees - the Objective-C drivers above all, plus
anything behind a platform #ifdef - and each of those costs a CI round
trip to discover, one lane at a time.

window_position_x and window_position_y went that way. Packing them
into one word updated the Win32 and SDL3 users, because those are what
a grep of the .c and .h files turns up; ui_cocoa.m kept reading both
and broke the macOS job and both PowerPC cross jobs at once.

tools/packed_field_check.py does not cover this: it knows the six axis
names a size or origin pair is spelled with, and window_position_x is
not one of them. The question here is not what a member is called, it
is whether anything still reaches for one the change took away.

So this reads the change rather than the tree's vocabulary. It takes
the struct members removed by a commit range, drops the ones still
declared somewhere (a member that merely moved, or a name another
struct also uses), and reports every remaining reader it can find, in
every language the tree is written in.

Usage:
  tools/removed_member_check.py [--range A..B] [paths ...]
  tools/removed_member_check.py --selftest

--range defaults to origin/master..HEAD, which is the set a patch is
about to deliver. Exits non-zero when a reader is left behind.
"""
import os
import re
import subprocess
import sys

SUFFIXES = ('.c', '.h', '.m', '.mm', '.cpp', '.hpp', '.inc')

# Vendored trees: their members are not ours to remove, and their
# readers are not ours to fix.
PRUNE = ('.git', 'deps', 'media', 'wii/libogc', 'pkg/apple/WebServer')

# A member declaration, as it appears on one line inside a struct body.
# Deliberately narrow: a type (possibly qualified and pointered), a
# name, an optional array bound, a semicolon, and nothing else of
# substance. A function pointer member, a bitfield and a multi-name
# declaration are all left alone rather than guessed at.
#
# The separator between the type and the name has to be a space or a
# star: without that the type's \w+ backtracks into the name and
# "unsigned camera_height;" is read as a member called "t".
DECL = re.compile(
    r'^[ \t]*'
    r'(?:(?:const|volatile|static)[ \t]+)*'
    r'(?:(?:struct|union|enum)[ \t]+\w+'
    r'|(?:unsigned|signed)(?:[ \t]+(?:char|short|int|long))*'
    r'|long(?:[ \t]+long)?(?:[ \t]+int)?'
    r'|short(?:[ \t]+int)?'
    r'|void|char|int|float|double|bool|size_t|ssize_t'
    r'|u?int(?:8|16|32|64|ptr|max)_t'
    r'|\w+)'
    r'(?:[ \t]+\**|[ \t]*\*+)[ \t]*'
    r'(?P<name>[A-Za-z_]\w*)'
    r'[ \t]*(?:\[[^\];]*\])?[ \t]*;[ \t]*(?:/\*.*|//.*)?$')

# Names too common to reason about from a diff alone, and never the
# whole story when one goes: a struct keeping its own `data` or `size`
# says nothing about anyone else's.
# The declared side is deliberately looser than the removed side. A
# member this misses is only ever a lost opportunity; a declaration it
# misses becomes a false alarm, and rmodtracker's
# "int seq_pos, break_pos, row, next_row, tick;" produced twelve of
# them. So anything that reads like a declarator list counts as a
# declaration here, even where DECL will not infer a removal from it.
DECL_LIST = re.compile(
    r'^[ \t]*'
    r'(?:(?:const|volatile|static|extern|struct|union|enum|unsigned'
    r'|signed)[ \t]+)*'
    r'\w+[ \t]+(?P<names>[^;()=]+);')


def declared_names(line):
    """Every name a line declares, read as liberally as is safe."""
    mo = DECL.match(line)
    if mo:
        return (mo.group('name'),)
    mo = DECL_LIST.match(line)
    if not mo:
        return ()
    out = []
    for part in mo.group('names').split(','):
        part = re.sub(r'\[[^\]]*\]', '', part).strip(' \t*')
        if re.fullmatch(r'[A-Za-z_]\w*', part):
            out.append(part)
    return tuple(out)


NOISE = frozenset((
    'data', 'size', 'len', 'ptr', 'buf', 'flags', 'type', 'count',
    'idx', 'index', 'next', 'prev', 'id', 'name', 'val', 'value',
))


def member_names(lines):
    """The member names declared by these source lines."""
    out = set()
    for line in lines:
        mo = DECL.match(line)
        if mo:
            out.add(mo.group('name'))
    return out


def removed_members(diff):
    """Members a diff takes away and does not put back under the same
    name. A rename shows up as one of each, so the added side is
    subtracted; a member that merely moves file is caught by the
    declared-somewhere pass below instead."""
    gone, kept = set(), set()
    for line in diff.split('\n'):
        if line.startswith('---') or line.startswith('+++'):
            continue
        if line.startswith('-'):
            gone |= member_names([line[1:]])
        elif line.startswith('+'):
            kept |= member_names([line[1:]])
    return (gone - kept) - NOISE


def walk(roots):
    for root in roots:
        if os.path.isfile(root):
            yield root
            continue
        for dp, dns, fns in os.walk(root):
            rel = os.path.relpath(dp, '.').replace(os.sep, '/')
            dns[:] = [d for d in dns
                      if not any(('%s/%s' % (rel, d)).lstrip('./') == pr
                                 or d == pr for pr in PRUNE)]
            for fn in sorted(fns):
                if fn.endswith(SUFFIXES):
                    yield os.path.join(dp, fn)


def strip(text):
    """Blank out comments and string literals so a member named in
    prose or in a log line is not mistaken for a reader."""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c == '/' and i + 1 < n and text[i + 1] == '*':
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r'[^\n]', ' ', text[i:j]))
            i = j
        elif c == '/' and i + 1 < n and text[i + 1] == '/':
            j = text.find('\n', i)
            j = n if j < 0 else j
            out.append(' ' * (j - i))
            i = j
        elif c in '"\'':
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == '\\' else 1
            j = min(j + 1, n)
            out.append(re.sub(r'[^\n]', ' ', text[i:j]))
            i = j
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def scan(names, texts):
    """Every read of one of `names` through . or ->, in files where
    the name is not declared as a member any more. Returns a list of
    (path, line, name)."""
    # Every member name the tree still declares, gathered in one pass:
    # asking the question once per name costs names x lines, which a
    # range of a couple of hundred commits does not finish.
    declared = set()
    for text in texts.values():
        for line in text.split('\n'):
            declared.update(declared_names(line))
    orphans = sorted(names - declared)
    if not orphans:
        return []
    pat = re.compile(r'(?:->|\.)\s*(%s)\b'
                     % '|'.join(re.escape(n) for n in orphans))
    hits = []
    for path in sorted(texts):
        text = strip(texts[path])
        for mo in pat.finditer(text):
            hits.append((path, text[:mo.start()].count('\n') + 1,
                         mo.group(1)))
    return hits


SELFTEST = (
    # (what it is, diff, tree, how many readers are expected)
    ('a member removed with its readers is clean',
     '-   unsigned window_position_x;\n'
     '-   unsigned window_position_y;\n'
     '+   unsigned window_position_pos;\n',
     {'ui.m': 'p = s->uints.window_position_pos;\n'},
     0),
    ('a reader left in a language no lane here compiles is caught',
     '-   unsigned window_position_x;\n'
     '-   unsigned window_position_y;\n'
     '+   unsigned window_position_pos;\n',
     {'ui.m': 'r.x = s->uints.window_position_x;\n'
              'r.y = s->uints.window_position_y;\n'},
     2),
    ('a member that only moved is not reported',
     '-   unsigned pos_width;\n',
     {'a.h': 'struct s {\n   unsigned pos_width;\n};\n',
      'b.c': 'v = g->pos_width;\n'},
     0),
    ('a name another struct still declares is not reported',
     '-   int scissor_x;\n',
     {'a.h': 'struct other {\n   int scissor_x;\n};\n',
      'b.c': 'v = o->scissor_x;\n'},
     0),
    ('a renamed member is not reported against its new name',
     '-   unsigned tex_w;\n'
     '+   unsigned tex_dims;\n',
     {'a.c': 'v = t->tex_dims;\n'},
     0),
    ('the name in a comment or a log string is not a reader',
     '-   unsigned atlas_width;\n',
     {'a.c': '/* atlas_width used to say this */\n'
             'RARCH_LOG("x.atlas_width\\n");\n'},
     0),
    ('a member declared only as a bare word is still found',
     '-   unsigned camera_height;\n',
     {'a.mm': 'x = self->camera_height;\n'},
     1),
    ('a common name is left alone',
     '-   void *data;\n',
     {'a.c': 'v = q->data;\n'},
     0),
    ('a name declared in a declarator list still counts as declared',
     '-   int next_row;\n',
     {'a.c': 'int seq_pos, break_pos, row, next_row, tick;\n'
             'v = replay->next_row;\n'},
     0),
)


def selftest():
    bad = 0
    for what, diff, tree, want in SELFTEST:
        got = len(scan(removed_members(diff), tree))
        mark = 'ok  '
        if got != want:
            mark, bad = 'FAIL', bad + 1
        print('%s %s (%d reader(s), wanted %d)' % (mark, what, got, want))
    print('%d fixture(s), %d failed' % (len(SELFTEST), bad))
    return 1 if bad else 0


def main(argv):
    if '--selftest' in argv:
        return selftest()
    rng = 'origin/master..HEAD'
    args = []
    i = 0
    while i < len(argv):
        if argv[i] == '--range':
            i += 1
            rng = argv[i]
        elif argv[i].startswith('--range='):
            rng = argv[i].split('=', 1)[1]
        else:
            args.append(argv[i])
        i += 1

    try:
        diff = subprocess.run(['git', 'diff', '--unified=0', rng],
                              capture_output=True, text=True,
                              check=True).stdout
    except (OSError, subprocess.CalledProcessError) as e:
        print('cannot read %s: %s' % (rng, e))
        return 2

    names = removed_members(diff)
    if not names:
        print('%s removes no struct member' % rng)
        return 0

    texts = {}
    for p in walk(args or ['.']):
        try:
            texts[p] = open(p, errors='replace').read()
        except OSError:
            pass

    hits = scan(names, texts)
    for path, line, name in hits:
        print('%s:%d: %s - %s no longer exists' % (path, line, name, name))
    print('%s removes %d member(s), %d reader(s) left behind'
          % (rng, len(names), len(hits)))
    return 1 if hits else 0


sys.exit(main(sys.argv[1:]))
