#!/usr/bin/env python3
"""Relink a built frontend with explicit test-only trust for a local TLS receiver."""
import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--build-root', required=True, type=Path)
parser.add_argument('--output', required=True, type=Path)
a = parser.parse_args()
root, output = a.build_root.resolve(), a.output.resolve()
output.parent.mkdir(parents=True, exist_ok=True)
source = Path(__file__).resolve().with_name('stream_tls_main.c')
text = subprocess.check_output(['make', '-n', '-W', 'retroarch.c'], cwd=root, text=True)
commands = [shlex.split(line) for line in text.splitlines() if line and not line.startswith('make[')]
compiles = [c for c in commands if '-c' in c and 'retroarch.c' in c and '-o' in c]
links = [c for c in commands if '-c' not in c and '-o' in c and c[c.index('-o') + 1] in ('retroarch', 'retroarch.exe')]
assert len(compiles) == len(links) == 1, 'one main compile and one full frontend link required'
compiler, link = compiles[0], links[0]
original = compiler[compiler.index('-o') + 1]
main_object, entry_object = output.with_suffix('.nomain.o'), output.with_suffix('.entry.o')


def run(command):
    response = output.with_suffix('.args')
    response.write_text('\n'.join('"' + arg.replace('\\', '\\\\').replace('"', '\\"') + '"'
                                  for arg in command[1:]) + '\n', encoding='utf-8')
    tool = shutil.which(command[0])
    if not tool and os.name == 'nt' and command[0].startswith('/'):
        converted = subprocess.check_output(['cygpath', '-w', command[0]], text=True).strip()
        tool = shutil.which(converted)
    assert tool, command[0]
    subprocess.run([tool, '@' + str(response)], cwd=root, check=True,
                   creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)

entry = compiler.copy()
entry[entry.index('retroarch.c')] = str(source)
entry[entry.index('-o') + 1] = str(entry_object)
run(entry)
compiler[compiler.index('-o') + 1] = str(main_object)
compiler += ['-Umain', '-Dmain=rarch_stream_test_unused_main']
run(compiler)
link[link.index(original)] = str(main_object)
link[link.index('-o') + 1] = str(output)
link.append(str(entry_object))
if os.name == 'nt':
    link.append('-mconsole')
run(link)
print(output, flush=True)
