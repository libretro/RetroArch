#!/usr/bin/env python3
"""Chinese on-screen keyboard table check.

Usage, from the repository root:
    python3 tools/osk_chinese_pages_check.py

input/chinese_osk_pages.h holds the Chinese pages of the on-screen
keyboard: an index grid of 44 keys, and one string per page of
characters, laid out as a grid by input_osk_chinese_grid(). This holds:

  * one page string per OSK_CHINESE_<initial>_<n> value of enum
    osk_type, in the same order, each marked with its initial and
    number;
  * every page at most CHINESE_OSK_PAGE_CHARS characters, each exactly
    3 bytes of UTF-8 (the grid cuts the string every 3 bytes), and no
    character on two pages;
  * the index grid exactly 44 keys - the grid has 44 and the menu copies
    them all - with its initials a-z in place and blank where an
    initial has no page;
  * every initial that has pages starting at its first one, which is
    what input_driver.c's osk_chinese_first_page[] jumps to.
"""
import re
import sys

def main():
    pages_h = open('input/chinese_osk_pages.h', encoding='utf-8').read()
    osk_h   = open('input/input_osk.h', encoding='utf-8').read()
    drv_c   = open('input/input_driver.c', encoding='utf-8').read()
    errors  = []

    per = int(re.search(r'#define CHINESE_OSK_PAGE_CHARS (\d+)', pages_h).group(1))
    enum_pages = re.findall(r'OSK_CHINESE_([A-Z])_(\d+),', osk_h)
    table = re.search(r'chinese_osk_pages\[\d+\]\[[^\]]*\] = \{(.*?)\};', pages_h, re.S).group(1)
    rows = re.findall(r'/\* ([a-z]) (\d+) \*/ "([^"]*)"', table)

    if len(rows) != len(enum_pages):
        errors.append('%d page strings for %d enum values' % (len(rows), len(enum_pages)))
    seen = {}
    for i, (enum_row, row) in enumerate(zip(enum_pages, rows)):
        if (enum_row[0].lower(), enum_row[1]) != (row[0], row[1]):
            errors.append('page %d is %s %s, the enum says %s %s'
                          % (i, row[0], row[1], enum_row[0].lower(), enum_row[1]))
        chars = list(row[2])
        if len(chars) > per:
            errors.append('page %s %s has %d characters, over %d' % (row[0], row[1], len(chars), per))
        for c in chars:
            if len(c.encode('utf-8')) != 3:
                errors.append('page %s %s: %r is not 3 bytes of UTF-8' % (row[0], row[1], c))
            if c in seen:
                errors.append('%r is on page %s and page %s %s' % (c, seen[c], row[0], row[1]))
            seen[c] = '%s %s' % (row[0], row[1])
        if not chars:
            errors.append('page %s %s is empty' % (row[0], row[1]))

    index = re.search(r'chinese_index_grid\[44\] = \{(.*?)\};', pages_h, re.S)
    keys = re.findall(r'"((?:[^"\\]|\\.)*)"', index.group(1)) if index else []
    if len(keys) != 44:
        errors.append('the index grid has %d keys, not 44' % len(keys))
    with_pages = {r[0] for r in rows}
    for letter in 'abcdefghijklmnopqrstuvwxyz':
        if letter in with_pages and letter not in keys:
            errors.append('initial %s has pages but no key on the index' % letter)
        if letter not in with_pages and letter in keys:
            errors.append('initial %s has a key on the index but no page' % letter)
    for letter in with_pages:
        if not any(r[0] == letter and r[1] == '1' for r in rows):
            errors.append('initial %s has no first page' % letter)
        if 'OSK_CHINESE_%s_1' % letter.upper() not in drv_c:
            errors.append('osk_chinese_first_page[] does not jump to %s' % letter)

    for e in errors:
        print('FAIL: ' + e)
    if errors:
        return 1
    print('ok: %d pages, %d characters, index of 44 keys' % (len(rows), len(seen)))
    return 0

if __name__ == '__main__':
    sys.exit(main())
