#!/usr/bin/env python3
"""Packed translation table consistency check.

Usage, from the repository root:
    python3 tools/msg_hash_packed_check.py [intl/msg_hash_xx.h ...]
    python3 tools/msg_hash_packed_check.py --selftest

Every intl/msg_hash_<lang>.h that intl/json2h.py emits carries two
objects paired by position and by nothing else: the blob struct holds
the rows as one run of NUL-terminated strings, and ids[] names the enum
each row belongs to. msg_hash_strtab_index_build() walks the blob once
and gives row N to ids[N], so a row present on one side and absent on
the other hands every row after it to the wrong enum, and the menu
fills with real strings belonging to other entries. A C compiler cannot
see that: both objects are well formed on their own, and the sizeof()
contiguity check in the header measures the blob against itself.

The pairing is recoverable because json2h.py names each struct member
after the djb2 hash of the key its row was emitted for, so this checks
it directly: row N's member name must be the name the key in ids[N]
hashes to, under the same preprocessor guards. The row sizes the walk
depends on are checked with it - a member one byte wider than its
string leaves a stray NUL that ends the walk's row early and shifts
everything after it just as surely.

Both checks read the whole file and follow the guards, so this sees
configurations no single build job compiles.
"""

import glob
import os
import re
import sys


def djb2(s):
    h = 5381
    for ch in s:
        h = ((h * 33) + ord(ch)) & 0xffffffff
    return h


def decode_c_literal(src):
    """Decode the text between the quotes of a C string literal to bytes."""
    out  = bytearray()
    data = src.encode('utf-8')
    i    = 0
    n    = len(data)
    while i < n:
        c = data[i]
        if c != 0x5c:
            out.append(c)
            i += 1
            continue
        i += 1
        e = data[i:i + 1].decode('latin1')
        if   e == 'n': out.append(0x0a); i += 1
        elif e == 't': out.append(0x09); i += 1
        elif e == 'r': out.append(0x0d); i += 1
        elif e == 'x':
            j = i + 1
            while j < n and chr(data[j]) in '0123456789abcdefABCDEF':
                j += 1
            out.append(int(data[i + 1:j], 16) & 0xff)
            i = j
        elif e in '01234567':
            j = i
            while j < n and j < i + 3 and chr(data[j]) in '01234567':
                j += 1
            out.append(int(data[i:j], 8) & 0xff)
            i = j
        else:
            out.append(data[i])
            i += 1
    return bytes(out)


LIT    = re.compile(r'"((?:[^"\\]|\\.)*)"')
MEMBER = re.compile(r'\s*char (\w+)\[(\d+)\];')
IDENT  = re.compile(r'\s*\(uint32_t\)(\w+),')
CHUNK  = re.compile(r'^s_[0-9a-f]{8}(?:_c\d+)?_\d+$')


def scan_guarded(lines):
    """Yield (line, guard) for every non-directive line, where guard is the
    enclosing #if stack in the form json2h.py records it."""
    stack = []
    for line in lines:
        s = line.strip()
        if s.startswith('#if'):
            stack.append([s, False])
        elif s.startswith('#else'):
            if stack:
                stack[-1][1] = True
        elif s.startswith('#endif'):
            if stack:
                stack.pop()
        elif s:
            yield line, tuple((g[0], g[1]) for g in stack)


def row_name(member):
    """Strip the chunk suffix json2h.py appends to a split row."""
    if CHUNK.match(member):
        return member.rsplit('_', 1)[0]
    return member


def expected_names(keys):
    """Reproduce json2h.py's member_base_names() from the ids[] order."""
    by_hash = {}
    for key in keys:
        by_hash.setdefault(djb2(key), []).append(key)
    names = {}
    for h, ks in by_hash.items():
        uniq = sorted(set(ks))
        if len(uniq) == 1:
            names[uniq[0]] = 's_%08x' % h
        else:
            for n, key in enumerate(uniq):
                names[key] = 's_%08x_c%u' % (h, n)
    return names


def split_sections(text, lang):
    """Return the four generated regions of a packed header as line lists."""
    def between(start, end, after=0):
        i = text.index(start, after) + len(start)
        j = text.index(end, i)
        return text[i:j].split('\n'), j

    members, at = between('static const struct\n{\n',
                          '\n} msg_hash_%s_blob =' % lang)
    inits,   at = between('_blob =\n{\n', '\n};', at)
    check,   at = between('_blob_check[\n', '\n      )) ? 1 : -1];', at)
    ids,     at = between('_ids[] =\n{\n', '\n};', at)
    return members, inits, check, ids


def check_text(text, lang, name):
    """Check one packed header's text; return a list of error strings."""
    errors = []
    if ('static const uint32_t msg_hash_%s_ids[] =' % lang) not in text:
        return errors
    try:
        member_l, init_l, check_l, id_l = split_sections(text, lang)
    except ValueError:
        return ['%s: generated regions not found' % name]

    members = []
    for line, guard in scan_guarded(member_l):
        m = MEMBER.match(line)
        if not m:
            errors.append('%s: unparsed struct member: %s' % (name, line))
            continue
        members.append((m.group(1), int(m.group(2)), guard))

    inits = []
    cur   = None
    for line, guard in scan_guarded(init_l):
        if cur is None:
            cur = [b'', guard]
        cur[0] += b''.join(decode_c_literal(p) for p in LIT.findall(line))
        if line.rstrip().endswith(','):
            inits.append((cur[0], cur[1]))
            cur = None
    if cur is not None:
        errors.append('%s: unterminated initializer' % name)

    ids = []
    for line, guard in scan_guarded(id_l):
        m = IDENT.match(line)
        if not m:
            errors.append('%s: unparsed ids entry: %s' % (name, line))
            continue
        ids.append((m.group(1), guard))

    if len(members) != len(inits):
        errors.append('%s: %u struct members but %u initializers'
                      % (name, len(members), len(inits)))
        return errors

    # Each member is exactly its decoded row plus the NUL, or exactly the
    # chunk for a non-final chunk of a split row. Anything wider leaves a
    # NUL the walk stops on; anything narrower runs two rows together.
    rows = []
    for i, ((mname, size, mg), (data, ig)) in enumerate(zip(members, inits)):
        if mg != ig:
            errors.append('%s: member %s is guarded %r and its initializer %r'
                          % (name, mname, mg, ig))
        if b'\x00' in data:
            errors.append('%s: member %s holds an embedded NUL'
                          % (name, mname))
        last = size == len(data) + 1
        if not last and size != len(data):
            errors.append('%s: member %s declared [%u] holds %u bytes'
                          % (name, mname, size, len(data)))
        base = row_name(mname)
        if rows and rows[-1][2] and rows[-1][0] == base:
            rows[-1][1] += size
            rows[-1][2]  = not last
        else:
            if rows and rows[-1][2]:
                errors.append('%s: member %s has no NUL and no continuation'
                              % (name, members[i - 1][0]))
            rows.append([base, size, not last, mg])
    if rows and rows[-1][2]:
        errors.append('%s: the last row is unterminated' % name)

    # The pairing the menu depends on.
    if len(rows) != len(ids):
        errors.append('%s: the blob holds %u rows but ids[] names %u'
                      % (name, len(rows), len(ids)))
    names = expected_names([k for k, _g in ids])
    for i in range(min(len(rows), len(ids))):
        key, ig = ids[i]
        if rows[i][0] != names[key]:
            errors.append('%s: row %u is %s but ids[] pairs it with %s (%s)'
                          % (name, i, rows[i][0], names[key], key))
            break
        if rows[i][3] != ig:
            errors.append('%s: row %u (%s) is guarded %r in the blob and %r '
                          'in ids[]' % (name, i, key, rows[i][3], ig))
            break

    # The sizeof() check is what makes a padding compiler fail the build
    # instead of misindexing, and it does that only while it still adds up
    # to the members above it.
    sums = {}
    m    = re.match(r'\s*\(sizeof\(msg_hash_\w+_blob\) == \((\d+)u',
                    check_l[0] if check_l else '')
    if not m:
        errors.append('%s: size check preamble not found' % name)
    else:
        sums[()] = int(m.group(1))
        for line, guard in scan_guarded(check_l[1:]):
            v = re.match(r'\s*\+ (\d+)u', line)
            if not v:
                errors.append('%s: unparsed size check term: %s'
                              % (name, line))
                continue
            sums[guard] = sums.get(guard, 0) + int(v.group(1))
        want = {}
        for _base, size, _open, guard in rows:
            want[guard] = want.get(guard, 0) + size
        for guard in sorted(set(sums) | set(want), key=repr):
            if sums.get(guard, 0) != want.get(guard, 0):
                errors.append('%s: the size check counts %u bytes under %r, '
                              'the blob has %u' % (name, sums.get(guard, 0),
                                                   guard, want.get(guard, 0)))
    return errors


def check_file(path):
    lang = os.path.basename(path)[len('msg_hash_'):-len('.h')]
    with open(path, 'r', encoding='utf-8') as f:
        return check_text(f.read(), lang, path)


# ---------------------------------------------------------------------------
# Self-test. The fixtures are built here rather than lifted from the tree, so
# the result says the same thing whether or not the shipped headers currently
# pass: it measures the checker.
# ---------------------------------------------------------------------------

def _fixture():
    rows  = [('MENU_ENUM_LABEL_VALUE_A', b'alpha', ()),
             ('MENU_ENUM_LABEL_VALUE_B', b'beta',  (('#ifdef HAVE_X', False),)),
             ('MENU_ENUM_LABEL_VALUE_C', b'gamma', ()),
             ('MENU_ENUM_LABEL_VALUE_D', b'delta', ())]
    names = expected_names([r[0] for r in rows])
    members, inits, ids = [], [], []
    base, guarded = 0, {}
    for key, data, guard in rows:
        pre  = [g[0] for g in guard]
        post = ['#endif' for _g in guard]
        members += pre + ['   char %s[%u];' % (names[key], len(data) + 1)] + post
        inits   += pre + ['   "%s",' % data.decode('ascii')] + post
        ids     += pre + ['   (uint32_t)%s,' % key] + post
        if guard:
            guarded[guard] = guarded.get(guard, 0) + len(data) + 1
        else:
            base += len(data) + 1
    check = ['      (sizeof(msg_hash_zz_blob) == (%uu' % base]
    for guard, size in guarded.items():
        check += [g[0] for g in guard] + ['       + %uu' % size, '#endif']
    return ('static const struct\n{\n' + '\n'.join(members)
            + '\n} msg_hash_zz_blob =\n{\n' + '\n'.join(inits)
            + '\n};\n\ntypedef char msg_hash_zz_blob_check[\n'
            + '\n'.join(check) + '\n      )) ? 1 : -1];\n\n'
            + 'static const uint32_t msg_hash_zz_ids[] =\n{\n'
            + '\n'.join(ids) + '\n};\n')


def selftest():
    failures = [0]

    def expect(label, text, want):
        errors = check_text(text, 'zz', label)
        ok     = (not errors) if want is None else any(want in e
                                                       for e in errors)
        print('%-30s %s' % (label, 'ok' if ok else 'FAILED'))
        if not ok:
            failures[0] += 1
            for e in errors:
                print('    %s' % e)

    good  = _fixture()
    named = expected_names(['MENU_ENUM_LABEL_VALUE_A',
                            'MENU_ENUM_LABEL_VALUE_C'])

    expect('clean fixture', good, None)

    # A retired setting's row taken out of ids[] and left in the blob,
    # which is what renamed every row after it in 5d0f721.
    expect('ids[] row dropped',
           good.replace('   (uint32_t)MENU_ENUM_LABEL_VALUE_C,\n', ''),
           'ids[] pairs it with')

    # The mirror of it: the row taken out of the blob and left in ids[].
    lost = good.replace('   char %s[6];\n'
                        % named['MENU_ENUM_LABEL_VALUE_C'], '')
    expect('blob row dropped', lost.replace('   "gamma",\n', ''),
           'ids[] pairs it with')

    # A member one byte too wide: the walk stops on the padding NUL and
    # every row after it shifts by one.
    expect('member wider than its row',
           good.replace('   char %s[6];\n' % named['MENU_ENUM_LABEL_VALUE_A'],
                        '   char %s[7];\n' % named['MENU_ENUM_LABEL_VALUE_A']),
           'holds 5 bytes')

    # A guard on one side only: the pairing holds for the build that
    # defines it and breaks for the one that does not.
    expect('guard on the blob only',
           good.replace('#ifdef HAVE_X\n   (uint32_t)MENU_ENUM_LABEL_VALUE_B,'
                        '\n#endif\n',
                        '   (uint32_t)MENU_ENUM_LABEL_VALUE_B,\n'),
           'is guarded')

    # A contiguity constant that no longer describes the blob it guards.
    expect('stale size check', good.replace('== (18u', '== (17u'),
           'the size check counts')

    return 1 if failures[0] else 0


def main(argv):
    args = argv[1:]
    if '--selftest' in args:
        return selftest()
    paths = args
    if not paths:
        here  = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        paths = sorted(glob.glob(os.path.join(here, 'intl', 'msg_hash_*.h')))
    failed = 0
    for path in paths:
        errors = check_file(path)
        for e in errors:
            print(e)
        if errors:
            failed += 1
    if failed:
        print('%u packed header(s) failed verification' % failed)
        return 1
    print('%u packed header(s) verified' % len(paths))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
