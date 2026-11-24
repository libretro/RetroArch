#!/usr/bin/env python3
"""Minify embedded GLSL without changing tokens, names or directives."""
import argparse
from functools import lru_cache
import json
from pathlib import Path
import re

TOKEN = re.compile(
    r'(?P<space>\s+)|(?P<comment>//[^\n]*|/\*.*?\*/)|'
    r'(?P<quoted>"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\')|'
    r'(?P<number>0[xX](?:[0-9a-fA-F]+(?:\.[0-9a-fA-F]*)?|\.[0-9a-fA-F]+)'
    r'(?:[pP][+-]?[0-9]+)?[uUlLfF]*|(?:[0-9]+\.[0-9]*|\.[0-9]+|[0-9]+)'
    r'(?:[eE][+-]?[0-9]+)?[uUlLfF]*)|'
    r'(?P<identifier>[A-Za-z_][A-Za-z_0-9]*)|'
    r'(?P<operator><<=|>>=|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||\^\^|'
    r'\+=|-=|\*=|/=|%=|&=|\^=|\|=|##|[^\s])', re.S)


def tokens(text):
    result = []
    pos, beginning = 0, True
    while pos < len(text):
        if beginning and re.match(r'[ \t]*#', text[pos:]):
            end = pos
            while True:
                newline = text.find('\n', end)
                if newline < 0:
                    end = len(text)
                    break
                end = newline + 1
                if text[newline-1:newline] != '\\':
                    break
            result.append(('directive', text[pos:end].lstrip(' \t').rstrip('\n') + '\n'))
            pos, beginning = end, True
            continue
        if text.startswith('/*', pos) and text.find('*/', pos + 2) < 0:
            raise ValueError('Unterminated GLSL comment')
        match = TOKEN.match(text, pos)
        if not match:
            raise ValueError('Unrecognized GLSL token')
        kind, value = match.lastgroup, match.group()
        if kind not in ('space', 'comment'):
            result.append((kind, value))
            beginning = False
        elif '\n' in value:
            beginning = True
        pos = match.end()
    return result


@lru_cache(maxsize=16384)
def needs_space(left, right):
    if left == '/' and right in ('/', '*'):
        return True
    return [value for kind, value in tokens(left + right)] != [left, right]


def minify(text):
    # Line-sensitive macros must see the original physical source layout.
    if '__LINE__' in text or '\\\n' in text:
        return text
    original = tokens(text)
    if any(value == '#' for kind, value in original):
        return text
    output, previous = [], None
    for kind, value in original:
        if kind == 'directive':
            if output and not output[-1].endswith('\n'):
                output.append('\n')
            output.append(value)
            previous = None
        else:
            if previous is not None and needs_space(previous, value):
                output.append(' ')
            output.append(value)
            previous = value
    result = ''.join(output)
    if tokens(result) != original:
        raise ValueError('GLSL token stream changed during minification')
    return result


def decode_embedded(data):
    if data.lstrip().startswith('"'):
        return ''.join(json.loads(part, strict=False) for part in re.findall(r'"(?:\\.|[^"\\])*"', data)), 'c-string'
    values = [int(part) for part in re.findall(r'\d+', data)]
    if not values or values[-1] != 0:
        raise ValueError('Embedded GLSL byte array is not terminated')
    return bytes(values[:-1]).decode('ascii'), 'c-bytes'


def read_embedded(path):
    return decode_embedded(path.read_text(encoding='utf-8'))


def encode_embedded(text, kind):
    if kind == 'c-string':
        return ''.join(json.dumps(text[i:i+120]) + '\n' for i in range(0, len(text), 120)) or '""\n'
    values = list(text.encode('ascii')) + [0]
    return ',\n'.join(','.join(str(x) for x in values[i:i+24])
                      for i in range(0, len(values), 24)) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    text, kind = read_embedded(args.input)
    args.output.write_bytes(encode_embedded(minify(text), kind).encode('ascii'))


if __name__ == '__main__':
    main()
