#!/usr/bin/env python3
"""Writes the trimmed fixtures: for each system named in cases.txt, the
Named_Boxarts lines of a full `git ls-tree -r --name-only HEAD` listing
whose title folds like a query's, plus a few sequel neighbours. A name
holding '[' is kept only for a query that holds one or a TOSEC date, so
check a bracket change against the full listings too. Lines are copied
as git wrote them, quoted or not.

usage: make_fixtures.py NAMES_DIR CASES OUT_DIR
"""
import os
import re
import sys
import unicodedata

ROMAN = {'ii': '2', 'iii': '3', 'iv': '4', 'vi': '6', 'vii': '7', 'viii': '8',
         'ix': '9', 'xi': '11', 'xii': '12', 'xiii': '13', 'xiv': '14',
         'xv': '15', 'xvi': '16', 'xvii': '17', 'xviii': '18', 'xix': '19',
         'xx': '20'}
DATED = re.compile(r'\((19|20)[0-9x]{2}')
ESC = {'n': '\n', 't': '\t', '"': '"', '\\': '\\', 'a': '\a', 'b': '\b',
       'f': '\f', 'r': '\r', 'v': '\v'}


def unquote(line):
    if not line.startswith('"'):
        return line
    out = bytearray()
    s = line[1:-1]
    i = 0
    while i < len(s):
        if s[i] == '\\' and s[i + 1] in '01234567':
            out.append(int(s[i + 1:i + 4], 8))
            i += 4
        elif s[i] == '\\':
            out += ESC[s[i + 1]].encode()
            i += 2
        else:
            out += s[i].encode()
            i += 1
    return out.decode('utf-8')


def title(stem):
    for sep in (' (', ' [', '('):
        j = stem.find(sep)
        if j > 0:
            stem = stem[:j]
    return stem.replace('][', ' ii ')


def words(text):
    text = unicodedata.normalize('NFKD', text.replace('’', ''))
    text = text.replace('⁄', ' ')
    text = text.encode('ascii', 'ignore').decode().lower().replace("'", '')
    return [w for w in re.split(r'[^0-9a-z]+', text)
            if w and w not in ('the', 'a', 'an', 'and')]


def loose(stem):
    out = ''
    for seg in re.split(r' - |: |(?<! )_ ', title(stem)):
        w = words(seg)
        if w and w[-1] in ROMAN:
            w[-1] = ROMAN[w[-1]]
        for x in w:
            if out[-1:].isdigit() and x[0].isdigit():
                out += ' '
            out += x
    return out


def family(stem):
    w = words(title(stem))
    if len(w) > 1 and (w[-1].isdigit() or w[-1] in ROMAN):
        w = w[:-1]
    return ''.join(w)


def main():
    names_dir, cases_path, out_dir = sys.argv[1:4]
    queries = {}
    for line in open(cases_path, encoding='utf-8'):
        line = line.rstrip('\n')
        if line and not line.startswith('#'):
            f = line.split('|')
            if f[0] != '-':
                queries.setdefault(f[0], []).append(f[1])
    for system in sorted(queries):
        rows = []
        path = os.path.join(names_dir, system + '.index')
        for raw in open(path, encoding='utf-8'):
            raw = raw.rstrip('\n')
            name = unquote(raw)
            if name.startswith('Named_Boxarts/') and '/' not in name[14:]:
                rows.append((raw, name[14:].rsplit('.', 1)[0]))
        keep = set()
        for q in queries[system]:
            tosec = bool(DATED.search(q))
            sequels = []
            for raw, stem in rows:
                if '[' in stem and '[' not in q and not tosec:
                    continue
                if loose(stem) == loose(q):
                    keep.add(raw)
                elif (family(stem) == family(q) and '[' not in stem and
                      bool(DATED.search(stem)) == tosec):
                    sequels.append(raw)
            keep.update(sorted(sequels)[:6])
        out = os.path.join(out_dir, system + '.index')
        with open(out, 'w', encoding='utf-8', newline='\n') as f:
            for raw, stem in rows:
                if raw in keep:
                    f.write(raw + '\n')


main()
