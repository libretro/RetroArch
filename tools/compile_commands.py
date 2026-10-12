#!/usr/bin/env python3
"""Write compile_commands.json for a configured tree.

clangd and other code navigation read compile_commands.json to know the
defines and include paths each file is built with; griffin and the
per-platform Makefiles leave them guessing otherwise. This asks make
for the commands it would run (make -n -B V=1), so the result is the
configuration ./configure chose, and keeps the compile lines.

Usage:
  tools/compile_commands.py [-f MAKEFILE] [-o OUTPUT] [make args ...]
  tools/compile_commands.py --selftest

Run ./configure first for the default Makefile. Needs nothing beyond
the Python standard library and make.
"""
import json, os, shlex, shutil, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE_EXT = ('.c', '.cpp', '.cc', '.cxx', '.m', '.mm', '.S')


def entries(lines, directory):
   out = []
   seen = set()
   for line in lines:
      line = line.strip()
      if ' -c ' not in ' %s ' % line:
         continue
      try:
         args = shlex.split(line)
      except ValueError:
         continue
      if args and args[0] in ('ccache', 'sccache'):
         args = args[1:]
      srcs = [a for a in args if a.endswith(SOURCE_EXT) and not a.startswith('-')]
      if not args or len(srcs) != 1:
         continue
      src = os.path.normpath(os.path.join(directory, srcs[0]))
      if src in seen:
         continue
      seen.add(src)
      out.append({'directory': directory, 'arguments': args, 'file': src})
   return out


def generate(makefile, extra, directory):
   cmd = ['make', '-n', '-B', '-o', 'config.mk', '-o', 'config.h', 'V=1']
   if makefile:
      cmd += ['-f', makefile]
   cmd += extra
   r = subprocess.run(cmd, cwd=directory, stdout=subprocess.PIPE,
                      stderr=subprocess.PIPE, universal_newlines=True)
   if r.returncode:
      sys.stderr.write(r.stderr)
      sys.exit('compile_commands: make -n failed (has ./configure run?)')
   return entries(r.stdout.split('\n'), directory)


def selftest():
   d = tempfile.mkdtemp(prefix='compile_commands.')
   try:
      os.makedirs(os.path.join(d, 'sub'))
      for name in ('a.c', 'sub/b.c'):
         open(os.path.join(d, name), 'w').close()
      with open(os.path.join(d, 'Makefile'), 'w') as f:
         f.write('ifneq ($(V),1)\nQ := @\nendif\n'
                 'all: a.o sub/b.o\n'
                 '%.o: %.c\n\t$(Q)cc -DX="1 2" -Isub -c -o $@ $<\n')
      got = generate(None, [], d)
      files = sorted(os.path.relpath(e['file'], d) for e in got)
      ok = files == ['a.c', os.path.join('sub', 'b.c')] \
         and all('-DX=1 2' in e['arguments'] for e in got)
      print('selftest: %s' % ('ok' if ok else 'FAIL %r' % got))
      return 0 if ok else 1
   finally:
      shutil.rmtree(d, ignore_errors=True)


def main(argv):
   if argv[:1] == ['--selftest']:
      return selftest()
   makefile = None
   output = os.path.join(ROOT, 'compile_commands.json')
   extra = []
   i = 0
   while i < len(argv):
      if argv[i] in ('-f', '-o') and i + 1 < len(argv):
         if argv[i] == '-f':
            makefile = argv[i + 1]
         else:
            output = argv[i + 1]
         i += 2
         continue
      if argv[i] in ('-h', '--help'):
         print(__doc__.strip())
         return 0
      extra.append(argv[i])
      i += 1
   got = generate(makefile, extra, ROOT)
   if not got:
      sys.exit('compile_commands: make printed no compile commands')
   with open(output, 'w', encoding='utf-8') as f:
      json.dump(got, f, indent=1)
      f.write('\n')
   print('compile_commands: %d files -> %s' % (len(got), output))
   return 0


if __name__ == '__main__':
   sys.exit(main(sys.argv[1:]))
