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


def c89_cut(old_str):
    if old_str.endswith('[...]'):
        return old_str
    new_str = ''
    byte_count = 0
    for c in old_str:
        byte_count += len(c.encode('utf-8'))
        if byte_count > 500:
            return new_str + '[...]'
        new_str += c
    return new_str


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
            if tp_msg['key'].find('_QT_') < 0:
                tl_msg_val = c89_cut(tl_msg_val)
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
    """Encode bytes as C string literal text (without surrounding quotes)."""
    out = []
    prev_octal = False
    for byte in b:
        if prev_octal and chr(byte) in '0123456789':
            out.append('" "')       # split literal so octal cannot extend
        prev_octal = False
        if   byte == 0x22: out.append('\\"')
        elif byte == 0x5c: out.append('\\\\')
        elif byte == 0x0a: out.append('\\n')
        elif byte == 0x09: out.append('\\t')
        elif byte == 0x0d: out.append('\\r')
        elif byte < 0x20:
            out.append('\\%o' % byte)
            prev_octal = True
        else:
            out.append(chr(byte) if byte < 0x80 else None or chr(byte))
    return ''.join(out)

def encode_chunk_bytes(b):
    # work on unicode-safe re-encoding: bytes -> utf-8 text with escapes
    text = b.decode('utf-8')
    out = []
    prev_octal = False
    for ch in text:
        if prev_octal and ch in '0123456789':
            out.append('" "')
        prev_octal = False
        if   ch == '"':  out.append('\\"')
        elif ch == '\\': out.append('\\\\')
        elif ch == '\n': out.append('\\n')
        elif ch == '\t': out.append('\\t')
        elif ch == '\r': out.append('\\r')
        elif ord(ch) < 0x20:
            out.append('\\%o' % ord(ch))
            prev_octal = True
        else:
            out.append(ch)
    return ''.join(out)

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
            rows.append((key, val, tuple((g[0], g[1]) for g in stack)))
        i += 1
    return rows

PRAGMA_BLOCK = (
'#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)\n'
'#if (_MSC_VER >= 1700)\n'
'/* https://support.microsoft.com/en-us/kb/980263 */\n'
'#pragma execution_character_set("utf-8")\n'
'#endif\n'
'#pragma warning(disable:4566)\n'
'#endif\n')

def emit_guard_transition(out, prev, cur):
    if prev == cur:
        return
    for _ in prev:
        out.append('#endif')
    for cond, in_else in cur:
        out.append(cond)
        if in_else:
            out.append('#else')

def pack(text, lang):
    rows = parse_rows_with_guards(text)
    if not rows:
        raise SystemExit('packed emitter: no rows for ' + lang)
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
            name = 's%u' % r if len(chunks) == 1 else 's%u_%u' % (r, ci)
            members.append(('   char %s[%u];' % (name, size), guard))
            inits.append(('   "%s",' % encode_chunk_bytes(cb), guard))
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
    template = template_file.read()
    with open('msg_hash_us.json', 'r+', encoding='utf-8') as source_json_file:
        source_messages = json.load(source_json_file)
        with open(json_filename, 'r+', encoding='utf-8') as json_file:
            messages = json.load(json_file)
            new_translation = update(messages, template, source_messages)
            with open(h_filename, 'w', encoding='utf-8') as h_file:
                h_file.seek(0)
                h_file.write(pack(new_translation, LANG))
                h_file.truncate()
