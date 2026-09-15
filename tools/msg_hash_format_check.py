#!/usr/bin/env python3
"""Translated format string parity check.

Usage, from the repository root:
    python3 tools/msg_hash_format_check.py [intl/msg_hash_xx.h ...]
    python3 tools/msg_hash_format_check.py --selftest

Some rows reach snprintf() as the format string, with the arguments
supplied at the call site - cheevos.c hands
MSG_CHEEVOS_HARDCORE_PAUSED_SYSTEM_NOT_FOR_CORE two char*, connmanctl.c
hands MSG_LOCALAP_STARTING two more. The call site cannot know which
language is loaded, so a translation whose conversions differ from the
English row's changes what snprintf reads off the argument list: a row
that gains a %s reads a pointer that was never passed.

This compares each translated row's ordered conversion sequence against
the English row in intl/msg_hash_us.h and enforces:

  * where English has conversions, the translation has the same ones in
    the same order;
  * no translation uses a positional %n$ conversion - it is POSIX, the
    MSVC runtime does not implement it, and the MSVC jobs build the
    same headers;
  * where a row that is used as a format string has no conversions in
    English, the translation has none either. That set is measured:
    format_site_keys() walks the sources for printf-family calls whose
    format argument resolves to msg_hash_to_str(), directly or through
    a local, and the MSG_* namespace is added to it so a site the scan
    cannot see still gets the rule. Everything else is displayed rather
    than formatted - menu_driver.c strlcpy()s sublabels, help resolves
    through msg_hash_get_help_us_enum(), which returns English whatever
    the language - so a percent sign in prose stays legal there, as it
    must: enforcing it would reject Hungarian's "50%-a" and every other
    language whose percent sign takes a suffix.

A space flag counts as prose rather than a conversion. C defines that
flag for signed conversions only, no English row uses it deliberately,
and without the exemption "at 50% of this value" parses as a %o and
every faithful translation of it looks like a violation.

intl/json2h.py drops a row that fails these rules instead of emitting
it, so the runtime falls back to the English row and a bad Crowdin
sync degrades to untranslated rather than to a bad vararg read. This
check is what covers a header edited by hand.
"""

import glob
import os
import re
import sys

sys.path.insert(0, os.path.join(
      os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'intl'))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import json2h
import msg_hash_packed_check as packed


SPEC = re.compile(r"%(?:(\d+)\$)?([-+ #0']*)(\*|\d+)?(?:\.(\*|\d*))?"
                  r"(hh|h|ll|l|L|j|z|t|q)?([diouxXeEfFgGaAcspn])")


def conversions(s):
    """Return (ordered conversion characters, positional flag)."""
    convs = []
    positional = False
    i = 0
    while True:
        i = s.find('%', i)
        if i < 0:
            break
        if s[i:i + 2] == '%%':
            i += 2
            continue
        m = SPEC.match(s, i)
        if not m:
            i += 1
            continue
        if ' ' in (m.group(2) or ''):
            i = m.end()
            continue
        if m.group(1):
            positional = True
        convs.append(m.group(6))
        i = m.end()
    return tuple(convs), positional


FMT_ARG = {'snprintf': 2, 'vsnprintf': 2, 'sprintf': 1, 'printf': 0,
           'fprintf': 1, 'asprintf': 1, 'vasprintf': 1, 'swprintf': 2,
           'rcheevos_log': 0, 'runloop_msg_queue_pushf': 0,
           'RARCH_LOG': 0, 'RARCH_ERR': 0, 'RARCH_WARN': 0, 'RARCH_DBG': 0,
           'RARCH_LOG_OUTPUT': 0, 'CHEEVOS_LOG': 0, 'CHEEVOS_ERR': 0}
CALL    = re.compile(r'\b(' + '|'.join(FMT_ARG) + r')\s*\(')
ALIAS   = re.compile(r'\b(\w+)\s*=\s*msg_hash_to_str\w*\('
                     r'\s*([A-Z][A-Z0-9_]*)\s*\)')
TO_STR  = re.compile(r'msg_hash_to_str\w*\(\s*([A-Z][A-Z0-9_]*)')
BARE    = re.compile(r'\(?\s*(?:\(\s*const\s+char\s*\*\s*\)\s*)?(\w+)\s*\)?\Z')
SKIP    = ('.git', 'deps', 'intl', 'libretro-common')


def call_args(text, open_paren):
    """Split the argument list whose '(' is at open_paren."""
    depth = 0
    cur   = []
    out   = []
    i     = open_paren
    while i < len(text):
        c = text[i]
        if c == '"':
            j = i + 1
            while j < len(text) and not (text[j] == '"' and text[j - 1] != '\\'):
                j += 1
            cur.append(text[i:j + 1])
            i = j + 1
            continue
        if c in '([{':
            depth += 1
        elif c in ')]}':
            depth -= 1
            if depth == 0:
                out.append(''.join(cur))
                return out
        if c == ',' and depth == 1:
            out.append(''.join(cur))
            cur = []
            i  += 1
            continue
        if depth >= 1:
            cur.append(c)
        i += 1
    return out


def format_site_text(text):
    """Keys whose string this translation unit uses as a format string."""
    keys    = set()
    aliases = {}
    for m in ALIAS.finditer(text):
        aliases.setdefault(m.group(1), set()).add(m.group(2))
    for m in CALL.finditer(text):
        args = call_args(text, m.end() - 1)
        i    = FMT_ARG[m.group(1)]
        if len(args) <= i:
            continue
        fmt = args[i].strip()
        keys.update(TO_STR.findall(fmt))
        v = BARE.match(fmt)
        if v and v.group(1) in aliases:
            keys |= aliases[v.group(1)]
    return keys


def format_site_keys(root):
    """Keys used as a format string anywhere in the tree.

    Heuristic in one direction only: a site it misses keeps whatever
    the MSG_* namespace already gives it, and a site it invents costs
    at most a rule applied to a row that did not need it.
    """
    keys = set()
    for base, dirs, files in os.walk(root):
        dirs[:] = [d for d in dirs if d not in SKIP and not d.startswith('.')]
        for fn in files:
            if not fn.endswith(('.c', '.cpp', '.m', '.mm')):
                continue
            with open(os.path.join(base, fn), encoding='utf-8',
                      errors='ignore') as f:
                text = f.read()
            if 'msg_hash_to_str' in text:
                keys |= format_site_text(text)
    return keys


def english_rows(root):
    """key -> English string, from the expanded intl/msg_hash_us.h."""
    path = os.path.join(root, 'intl', 'msg_hash_us.h')
    with open(path, encoding='utf-8') as f:
        text = json2h.expand_template(f.read(), os.path.join(root, 'intl'))
    out = {}
    for key, val, _guard in json2h.parse_rows_with_guards(text):
        out[key] = json2h.decode_c_literal(val).decode('utf-8', 'replace')
    return out


def packed_rows(text, lang):
    """[(key, string)] in blob order, for an already verified header."""
    member_l, init_l, _check_l, id_l = packed.split_sections(text, lang)
    members = []
    for line, _g in packed.scan_guarded(member_l):
        m = packed.MEMBER.match(line)
        members.append((m.group(1), int(m.group(2))))
    strs = []
    cur  = b''
    for line, _g in packed.scan_guarded(init_l):
        cur += b''.join(packed.decode_c_literal(p)
                        for p in packed.LIT.findall(line))
        if line.rstrip().endswith(','):
            strs.append(cur)
            cur = b''
    rows = []
    i    = 0
    while i < len(members):
        data = strs[i]
        while members[i][1] != len(strs[i]) + 1:
            i   += 1
            data += strs[i]
        rows.append(data.decode('utf-8', 'replace'))
        i += 1
    keys = [packed.IDENT.match(l).group(1)
            for l, _g in packed.scan_guarded(id_l)]
    return list(zip(keys, rows))


def row_error(key, english, translation, formatted=None):
    """Return why this row may not ship, or None.

    formatted is the set of keys used as a format string; a key in the
    MSG_* namespace counts as formatted whether or not it is in there.
    """
    ec, _ep = conversions(english)
    tc,  tp = conversions(translation)
    if tp:
        return 'uses a positional conversion, which the MSVC runtime ' \
               'does not implement'
    if ec:
        if tc != ec:
            return 'has conversions %s where English has %s' \
                   % (''.join('%' + c for c in tc) or 'none',
                      ''.join('%' + c for c in ec))
    elif tc and (key.startswith('MSG_') or (formatted and key in formatted)):
        return 'has conversions %s where English has none' \
               % ''.join('%' + c for c in tc)
    return None


def check_file(path, english, formatted=None):
    lang = os.path.basename(path)[len('msg_hash_'):-len('.h')]
    with open(path, encoding='utf-8') as f:
        text = f.read()
    if ('static const uint32_t msg_hash_%s_ids[] =' % lang) not in text:
        return []
    errors = []
    for key, s in packed_rows(text, lang):
        if key not in english:
            continue
        why = row_error(key, english[key], s, formatted)
        if why:
            errors.append('%s: %s %s' % (path, key, why))
    return errors


# ---------------------------------------------------------------------------
# Self-test. The cases are written here rather than taken from intl/, so the
# result says the same thing whether or not the shipped headers pass.
# ---------------------------------------------------------------------------

CASES = [
    ('matching conversions',
     'MSG_X', 'Starting with SSID=%s and key=%s',
     'Uruchamianie z SSID=%s i kluczem=%s', None),
    ('a conversion lost in translation',
     'MSG_X', 'Starting with SSID=%s and key=%s',
     'Uruchamianie z SSID =% i kluczem=%s', 'where English has'),
    ('a conversion invented',
     'MSG_X', 'Runahead enabled.', 'Wykonywanie %u.', 'where English has none'),
    ('the same conversions reordered positionally',
     'MSG_X', 'No achievements for %s using %s',
     '\u4f7f\u7528 %2$s \u65e0\u6cd5\u4e3a %1$s', 'positional'),
    ('a percent sign in English prose',
     'MENU_ENUM_SUBLABEL_X', 'Kept at 50% of this value.',
     'Gehalten bei 50% dieses Wertes.', None),
    ('a percent sign taking a suffix',
     'MENU_ENUM_SUBLABEL_X', 'Set to 100% for symmetry.',
     '50%-a szimmetria.', None),
    ('a conversion invented in a format row outside MSG_',
     'MENU_ENUM_LABEL_VALUE_FMT', 'Applying: Default', 'Toepassen: %s',
     'where English has none'),
    ('a percent sign in a row that is only displayed',
     'MENU_ENUM_LABEL_VALUE_X', 'Applying: Default', 'Toepassen: 50%-a',
     None),
    ('a doubled percent sign',
     'MSG_X', 'Filled to %u%%', 'Gevuld tot %u%%', None),
    ('prose percent in a formatted row',
     'MSG_X', 'Loaded.', 'Geladen op 50%-a', 'where English has none'),
    ('a width and length modifier',
     'MSG_X', 'Address %08X size %zu', 'Adres %08X rozmiar %zu', None),
    ('a conversion type changed',
     'MSG_X', 'Address %08X', 'Adres %08s', 'where English has'),
]

# The format-site scan, which decides whether the third rule applies to a
# row outside the MSG_* namespace.
SITE_CASES = [
    ('a format argument straight from the table',
     'snprintf(b, sizeof(b), msg_hash_to_str(MENU_ENUM_LABEL_VALUE_A), n);',
     True),
    ('a format argument through a local',
     'const char *fmt = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_A);\n'
     'snprintf(b, sizeof(b), fmt, n);', True),
    ('a table string copied rather than formatted',
     'strlcpy(b, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_A), sizeof(b));',
     False),
    ('a table string passed as a value',
     'snprintf(b, sizeof(b), "%s: %u", '
     'msg_hash_to_str(MENU_ENUM_LABEL_VALUE_A), n);', False),
    ('a local that is split rather than formatted',
     'const char *fmt = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_A);\n'
     'snprintf(b, sizeof(b), "%.*s%u", (int)(p - fmt), fmt, n);', False),
]


def selftest():
    failures = 0
    for label, source, want in SITE_CASES:
        got = 'MENU_ENUM_LABEL_VALUE_A' in format_site_text(source)
        ok  = got == want
        print('%-40s %s' % (label, 'ok' if ok else 'FAILED'))
        if not ok:
            failures += 1
    for label, key, en, tl, want in CASES:
        got = row_error(key, en, tl, {'MENU_ENUM_LABEL_VALUE_FMT'})
        ok  = (got is None) if want is None else (got is not None
                                                  and want in got)
        print('%-40s %s' % (label, 'ok' if ok else 'FAILED'))
        if not ok:
            failures += 1
            print('    got: %r' % got)
    return 1 if failures else 0


def main(argv):
    args = argv[1:]
    if '--selftest' in args:
        return selftest()
    root      = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    english   = english_rows(root)
    formatted = format_site_keys(root)
    paths     = args or sorted(glob.glob(os.path.join(root, 'intl',
                                                      'msg_hash_*.h')))
    failed = 0
    for path in paths:
        errors = check_file(path, english, formatted)
        for e in errors:
            print(e)
        if errors:
            failed += 1
    if failed:
        print('%u header(s) hold a row whose conversions do not match '
              'English' % failed)
        return 1
    print('%u header(s) checked against %u English rows, %u of them used '
          'as a format string' % (len(paths), len(english),
                                  len(formatted & set(english))))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
