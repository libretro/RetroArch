#!/usr/bin/env python3

# Convert *.json to *.h
# Usage: ./json2h.py msg_hash_fr.json

import re
import sys
import json

repack_mode = (len(sys.argv) > 2 and sys.argv[1] == '--repack')

try:
    if repack_mode:
        h_filename = sys.argv[2]
        json_filename = None
    else:
        json_filename = sys.argv[1]
        h_filename = json_filename.replace('.json', '.h')
except IndexError:
    print("Usage: ./json2h.py <msg_hash_xx.json> | --repack <msg_hash_xx.h>")
    sys.exit(1)

if h_filename in ('msg_hash_us.h', 'msg_hash_lbl.h') or (
      json_filename in ('msg_hash_us.json', 'msg_hash_lbl.json')):
    print("Skip")
    sys.exit(0)

p = re.compile(
    r'MSG_HASH\s*(?:\/\*(?:.|[\r\n])*?\*\/\s*)*\(\s*(?:\/\*(?:.|[\r\n])*?\*\/\s*)*[a-zA-Z0-9_]+\s*(?:\/\*(?:.|[\r\n])*?\*\/\s*)*,\s*(?:\/\*(?:.|[\r\n])*?\*\/\s*)*\".*\"\s*(?:\/\*(?:.|[\r\n])*?\*\/\s*)*\)')


def strip_define_bodies(text):
    """Remove #define directives and their continuation lines so macro
    bodies containing MSG_HASH text are never parsed as rows."""
    out = []
    skipping = False
    for line in text.split('\n'):
        if skipping:
            skipping = line.rstrip().endswith('\\')
            continue
        if line.lstrip().startswith('#define'):
            skipping = line.rstrip().endswith('\\')
            continue
        out.append(line)
    return '\n'.join(out)


def expand_def_includes(text, base_dir):
    """Replace generated-region includes of settings_def_*.h with the
    literal MSG_HASH rows their S_ rows denote, preserving guard lines
    from the def file, so the template contains exactly what the
    compiler sees."""
    import os as _os

    def def_to_rows(def_text):
        out = []
        skip_endif = []
        i = 0
        while i < len(def_text):
            line_end = def_text.find('\n', i)
            if line_end < 0:
                line_end = len(def_text)
            ls = def_text[i:line_end].strip()
            if ls.startswith('#'):
                # A guard alternated with the strings pass is always true
                # for string consumers; this expansion mirrors the us.h
                # consumer, which defines the pass, so drop the pair.
                if 'SETTINGS_DEF' in ls:
                    # Pass markers - strings-pass alternations, the
                    # config-pass wrapper on divergent-key rows, the
                    # enum-pass alias shells - are def-file internals,
                    # always true for string consumers; drop the pair.
                    skip_endif.append(True)
                    i = line_end + 1
                    continue
                if ls.startswith('#endif') and skip_endif and skip_endif[-1]:
                    skip_endif.pop()
                    i = line_end + 1
                    continue
                if ls.startswith('#if'):
                    skip_endif.append(False)
                elif ls.startswith('#endif') and skip_endif:
                    skip_endif.pop()
                out.append(def_text[i:line_end])
                i = line_end + 1
                continue
            m = re.match(r'S_(BOOL|UINT|INT|FLOAT|STRING|DIR)(_NS)?(_H)?\s*\(', def_text[i:])
            if not m:
                i = line_end + 1
                continue
            jx = i + m.end()
            depth = 1
            args = ['']
            inq = False
            while depth:
                ch = def_text[jx]
                if inq:
                    args[-1] += ch
                    if ch == '\\':
                        args[-1] += def_text[jx+1]; jx += 1
                    elif ch == '"':
                        inq = False
                elif ch == '"':
                    inq = True; args[-1] += ch
                elif ch == '(':
                    depth += 1; args[-1] += ch
                elif ch == ')':
                    depth -= 1
                    if depth: args[-1] += ch
                elif ch == ',' and depth == 1:
                    args.append('')
                else:
                    args[-1] += ch
                jx += 1
            token = args[1].strip()
            if m.group(2):
                pairs = (('MENU_ENUM_LABEL_VALUE_', args[-1]),)
            else:
                pairs = (('MENU_ENUM_LABEL_VALUE_', args[-2]),
                         ('MENU_ENUM_SUBLABEL_', args[-1]))
            for pfx, val in pairs:
                c = val.find('"'); d = val.rfind('"')
                if c >= 0 and d > c:
                    out.append('MSG_HASH(')
                    out.append('   ' + pfx + token + ',')
                    out.append('   "' + val[c+1:d] + '"')
                    out.append('   )')
            i = jx
        return out

    lines = []
    for line in text.split('\n'):
        im = re.match(r'#include "((?:\.\./)?settings/settings_def_\w+\.h)"', line.strip())
        if im:
            p = _os.path.normpath(_os.path.join(base_dir, im.group(1)))
            if _os.path.exists(p):
                with open(p, encoding='utf-8') as f:
                    lines.extend(def_to_rows(f.read()))
                continue
        lines.append(line)
    return '\n'.join(lines)


def expand_template(text, base_dir='.'):
    return strip_define_bodies(expand_def_includes(text, base_dir))


def parse_message(message):
    # remove all comments before the value (= the string)
    a = message.find('/*')
    b = message.find('*/')
    c = message.find('"')
    new_msg = message
    while (a >= 0 and b >= 0) and (a < c < b or b < c):
        new_msg = new_msg[:a] + new_msg[b + 2:]
        c = new_msg.find('"', a)
        b = new_msg.find('*/', a)
        a = new_msg.find('/*', a)
    # get key word
    word = new_msg[new_msg.find('(') + 1:new_msg.find(',')].strip()

    # remove all comments after the value (= the string)
    a = new_msg.rfind('/*')
    b = new_msg.rfind('*/')
    d = new_msg.rfind('"')
    while (a >= 0 and b >= 0) and (a < d < b or a > d):
        new_msg = new_msg[:a]
        a = new_msg.rfind('/*')
        b = new_msg.rfind('*/')
        d = new_msg.rfind('"')
    # get value
    value = new_msg[c + 1:d]

    return word, value


def parse_messages(text):
    result = p.findall(text)
    seen = set()
    msg_list = []
    for msg in result:
        key, val = parse_message(msg)
        item = {'key': key, 'val': val, 'msg': msg}
        msg_list.append(item)
        if key not in seen:
            seen.add(key)
        else:
            print("Duplicate key: " + key)

    return msg_list


def update(messages, template, source_messages):
    translation = template
    template_messages = parse_messages(template)
    for tp_msg in template_messages:
        old_msg = tp_msg['msg']
        if tp_msg['key'] in messages and messages[tp_msg['key']] != source_messages[tp_msg['key']]:
            tp_msg_val = tp_msg['val']
            tl_msg_val = messages[tp_msg['key']]
            # escape all \
            tl_msg_val = tl_msg_val.replace('\\', r'\\')
            # remove "double-dipping" on escape sequences
            tl_msg_val = re.sub(r'\\\\(?=[nrt])', r'\\', tl_msg_val)
            # escape other symbols
            tl_msg_val = tl_msg_val.replace('"', '\\\"').replace('\n', '')
            # Replace last match, in case the key contains the value string
            new_msg = old_msg[::-1].replace(tp_msg_val[::-1], tl_msg_val[::-1], 1)[::-1]
            translation = translation.replace(old_msg, new_msg)
        # Remove English duplicates and non-translatable strings
        else:
            translation = translation.replace(old_msg + '\n', '')
    return translation




# ---------------------------------------------------------------------------
# Packed string-table emitter.
#
# Instead of MSG_HASH(ID, "...") switch-case rows, translation headers are
# emitted as three constant objects consumed by msg_hash.c:
#
#   static const struct { char s0[N0]; ... } msg_hash_<lang>_blob = {...};
#   static const uint32_t msg_hash_<lang>_ids[]  = { (uint32_t)ID, ... };
#   static const uint32_t msg_hash_<lang>_offs[] = { 0, N0, ... };
#
# Every struct member is a char array of exactly the row's decoded byte
# length (+1 for the NUL, except non-final chunks of long rows), each
# initialized by its own string literal of at most 500 bytes, so the file
# is strict C89 (ISO C90 only guarantees 509 bytes per string literal) and
# builds on every compiler including old MSVC.  char arrays have alignment
# 1 so the members are contiguous; a sizeof() compile-time check makes any
# compiler that disagrees fail the build instead of misindexing.  The
# resulting tables are pure read-only data with zero relocations, so the
# OS demand-pages them and languages that are never selected never become
# resident.
#
# Rows inside preprocessor guards keep their guards on the ids[] and
# offs[] entries; the blob members are unconditional (a handful of unused
# strings when a feature is compiled out is cheaper than shifting offsets).
# ---------------------------------------------------------------------------

CHUNK_MAX = 500

def decode_c_literal(src):
    """Decode the text between quotes of a C string literal to bytes."""
    out = bytearray()
    data = src.encode('utf-8')
    i = 0
    n = len(data)
    while i < n:
        c = data[i]
        if c != 0x5c:               # backslash
            out.append(c)
            i += 1
            continue
        i += 1
        e = data[i:i+1].decode('latin1')
        if   e == 'n': out.append(0x0a); i += 1
        elif e == 't': out.append(0x09); i += 1
        elif e == 'r': out.append(0x0d); i += 1
        elif e == '0' and not data[i+1:i+2].isdigit():
            out.append(0x00); i += 1
        elif e == 'x':
            j = i + 1
            while j < n and chr(data[j]) in '0123456789abcdefABCDEF':
                j += 1
            out.append(int(data[i+1:j], 16) & 0xff)
            i = j
        elif e in '01234567':
            j = i
            while j < n and j < i + 3 and chr(data[j]) in '01234567':
                j += 1
            out.append(int(data[i:j], 8) & 0xff)
            i = j
        else:                        # \\ \" \' and anything else: literal
            out.append(data[i])
            i += 1
    return bytes(out)

def encode_c_literal(b):
    """Encode bytes as pure-ASCII C string literal text (no quotes).

    Every byte outside printable ASCII is emitted as a fixed 3-digit
    octal escape, so the source is codepage-proof on compilers without
    a UTF-8 execution charset (MSVC before 2012 interprets raw bytes
    through the system ANSI codepage).  Fixed-width octal cannot be
    extended by a following digit, so no literal splitting is needed."""
    out = []
    for byte in b:
        if   byte == 0x22: out.append('\\"')
        elif byte == 0x5c: out.append('\\\\')
        elif byte == 0x0a: out.append('\\n')
        elif byte == 0x09: out.append('\\t')
        elif byte == 0x0d: out.append('\\r')
        elif byte < 0x20 or byte >= 0x7f:
            out.append('\\%03o' % byte)
        else:
            out.append(chr(byte))
    return ''.join(out)

def encode_chunk_lines(b, width=96, indent='   '):
    """Encode bytes as one or more adjacent pure-ASCII C literals.

    Escaped output can reach 4 source chars per byte; long chunks are
    wrapped into adjacent concatenated literals so source lines stay
    within the ISO C90 minimum line-length guarantee.  Concatenation
    happens before the 509-char literal limit is measured, and chunk
    execution length stays <= 500 bytes, so the C89 lane is unaffected."""
    tokens = []
    for byte in b:
        if   byte == 0x22: tokens.append('\\"')
        elif byte == 0x5c: tokens.append('\\\\')
        elif byte == 0x0a: tokens.append('\\n')
        elif byte == 0x09: tokens.append('\\t')
        elif byte == 0x0d: tokens.append('\\r')
        elif byte < 0x20 or byte >= 0x7f:
            tokens.append('\\%03o' % byte)
        else:
            tokens.append(chr(byte))
    lines = []
    cur   = []
    n     = 0
    for t in tokens:
        if n + len(t) > width and cur:
            lines.append(''.join(cur))
            cur = []
            n   = 0
        cur.append(t)
        n += len(t)
    if cur or not lines:
        lines.append(''.join(cur))
    return ('"\n%s"' % indent).join(lines)

def chunk_bytes(b, limit):
    """Split bytes at UTF-8 codepoint boundaries, chunks <= limit bytes."""
    chunks = []
    i = 0
    while i < len(b):
        j = min(i + limit, len(b))
        while j > i and j < len(b) and (b[j] & 0xc0) == 0x80:
            j -= 1
        chunks.append(b[i:j])
        i = j
    return chunks

def parse_rows_with_guards(text):
    rows  = []
    stack = []
    lines = text.split('\n')
    i = 0
    while i < len(lines):
        s = lines[i].strip()
        if s.startswith('#if'):
            stack.append([s, False])
        elif s.startswith('#else'):
            if stack: stack[-1][1] = True
        elif s.startswith('#elif'):
            raise SystemExit('packed emitter: #elif not supported')
        elif s.startswith('#endif'):
            if stack: stack.pop()
        elif s.startswith('MSG_HASH'):
            block = [lines[i]]
            while not lines[i].rstrip().endswith(')'):
                i += 1
                block.append(lines[i])
            key, val = parse_message('\n'.join(block))
            if not key_is_sane(key):
                raise SystemExit('packed emitter: insane key ' + repr(key))
            rows.append((key, val, tuple((g[0], g[1]) for g in stack)))
        i += 1
    return rows

PRAGMA_BLOCK = (
'/* Pure-ASCII source: every non-ASCII byte is a fixed 3-digit octal\n'
' * escape, so no execution-charset pragma is needed and the encoded\n'
' * UTF-8 bytes survive any compiler codepage (including MSVC 2003-2010,\n'
' * which have no way to consume raw UTF-8 source reliably).\n'
' * C4045 is disabled for old MSVC: non-final chunk members are sized\n'
' * without a NUL slot, which is valid C89; the sizeof compile check\n'
' * below still catches any real size mismatch. */\n'
'#if defined(_MSC_VER) && (_MSC_VER < 1900)\n'
'#pragma warning(disable:4045)\n'
'#endif\n')

def dedup_guards(g):
    # A def row's own platform guard can repeat an enclosing template
    # guard; nested identical plain guards are meaningless, so emit
    # each open condition once. Else-branches are never deduplicated.
    out, open_conds = [], set()
    for cond, in_else in g:
        if not in_else and cond in open_conds:
            continue
        out.append((cond, in_else))
        open_conds.add(cond)
    return tuple(out)

def emit_guard_transition(out, prev, cur):
    prev = dedup_guards(prev)
    cur = dedup_guards(cur)
    if prev == cur:
        return
    for _ in prev:
        out.append('#endif')
    for cond, in_else in cur:
        out.append(cond)
        if in_else:
            out.append('#else')

def key_is_sane(k):
    # Downloaded translation jsons must only ever contain enum-name keys.
    # Anything else (macro fragments, ##, punctuation) is upstream garbage
    # and must never reach a generated header.
    import re as _re
    return bool(_re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', k))


def djb2(s):
    h = 5381
    for ch in s:
        h = ((h * 33) + ord(ch)) & 0xffffffff
    return h

def member_base_names(rows):
    """Stable member names: djb2 hash of the msg-hash key.

    Positional names (s0, s1, ...) meant a single inserted or removed
    row renamed every later member, rewriting the whole header on
    routine translation syncs.  Key-hashed names only change when the
    row itself changes.  Hash collisions between distinct keys are
    disambiguated deterministically by sorted key order."""
    by_hash = {}
    for key, _val, _guard in rows:
        by_hash.setdefault(djb2(key), []).append(key)
    names = {}
    for h, keys in by_hash.items():
        if len(keys) == 1:
            names[keys[0]] = 's_%08x' % h
        else:
            for n, key in enumerate(sorted(keys)):
                names[key] = 's_%08x_c%u' % (h, n)
    return names

def pack(text, lang):
    rows = parse_rows_with_guards(text)
    if not rows:
        raise SystemExit('packed emitter: no rows for ' + lang)
    seen_keys = set()
    for key, _v, _g in rows:
        if key in seen_keys:
            raise SystemExit('packed emitter: duplicate key ' + key)
        seen_keys.add(key)
    import os as _os
    if _os.path.exists('msg_hash_us.json'):
        with open('msg_hash_us.json', encoding='utf-8') as _f:
            _src = set(json.load(_f))
        _alien = set()
        for _k in seen_keys - _src:
            # h2json legitimately omits language-name rows and rows with
            # preprocessor branches inside the invocation; everything
            # else missing from the source json is garbage.
            if _k.startswith('MENU_ENUM_LABEL_VALUE_LANG_'):
                continue
            if re.search(r'\b%s\s*,[^)]*#' % re.escape(_k), text):
                continue
            _alien.add(_k)
        if _alien:
            raise SystemExit('packed emitter: keys not in source: '
                             + ', '.join(sorted(_alien)[:5]))
    mnames = member_base_names(rows)
    members = []   # (text, guard)
    inits   = []   # (text, guard)
    sizes   = []   # (row_total_bytes, guard)
    base    = 0
    for r, (key, val, guard) in enumerate(rows):
        raw = decode_c_literal(val)
        chunks = chunk_bytes(raw, CHUNK_MAX) if raw else [b'']
        row_total = 0
        for ci, cb in enumerate(chunks):
            last = (ci == len(chunks) - 1)
            size = len(cb) + (1 if last else 0)
            name = mnames[key] if len(chunks) == 1 \
                  else '%s_%u' % (mnames[key], ci)
            members.append(('   char %s[%u];' % (name, size), guard))
            inits.append(('   "%s",' % encode_chunk_lines(cb), guard))
            row_total += size
        if guard:
            sizes.append((row_total, guard))
        else:
            base += row_total

    def emit_guarded(out, entries):
        prev = ()
        for text, guard in entries:
            emit_guard_transition(out, prev, guard); prev = guard
            out.append(text)
        emit_guard_transition(out, prev, ())

    out = []
    out.append('/* THIS FILE IS GENERATED by intl/json2h.py - do not edit.')
    out.append(' * Packed message table; source of truth is the Crowdin')
    out.append(' * project (see intl/crowdin_sync.py). */')
    out.append(PRAGMA_BLOCK)
    out.append('static const struct')
    out.append('{')
    emit_guarded(out, members)
    out.append('} msg_hash_%s_blob =' % lang)
    out.append('{')
    emit_guarded(out, inits)
    out.append('};')
    out.append('')
    out.append('/* Contiguity check: char members have alignment 1, so any')
    out.append(' * compiler that pads this struct fails here instead of')
    out.append(' * misindexing at runtime. */')
    out.append('typedef char msg_hash_%s_blob_check[' % lang)
    out.append('      (sizeof(msg_hash_%s_blob) == (%uu' % (lang, base))
    emit_guarded(out, [('       + %uu' % s, g) for s, g in sizes])
    out.append('      )) ? 1 : -1];')
    out.append('')
    prev = ()
    out.append('static const uint32_t msg_hash_%s_ids[] =' % lang)
    out.append('{')
    for (key, val, guard) in rows:
        emit_guard_transition(out, prev, guard); prev = guard
        out.append('   (uint32_t)%s,' % key)
    emit_guard_transition(out, prev, ())
    out.append('};')
    out.append('')
    return '\n'.join(out)

LANG = h_filename.replace('msg_hash_', '').replace('.h', '')

if repack_mode:
    with open(h_filename, 'r', encoding='utf-8') as f:
        packed = pack(f.read(), LANG)
    with open(h_filename, 'w', encoding='utf-8') as f:
        f.write(packed)
    sys.exit(0)

with open('msg_hash_us.h', 'r', encoding='utf-8') as template_file:
    template = expand_template(template_file.read())
    with open('msg_hash_us.json', 'r+', encoding='utf-8') as source_json_file:
        source_messages = json.load(source_json_file)
        with open(json_filename, 'r+', encoding='utf-8') as json_file:
            messages = json.load(json_file)
            new_translation = update(messages, template, source_messages)
            with open(h_filename, 'w', encoding='utf-8') as h_file:
                h_file.seek(0)
                h_file.write(pack(new_translation, LANG))
                h_file.truncate()
