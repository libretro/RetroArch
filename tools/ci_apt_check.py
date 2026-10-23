#!/usr/bin/env python3
"""Every package a runner job installs goes through apt-install.sh.

  tools/ci_apt_check.py              checks .github/workflows
  tools/ci_apt_check.py --selftest

A bare apt-get install or update has no bound: a stalled mirror holds
the job until its own timeout cancels it, and nothing is cached, so
every run fetches the packages again. apt-install.sh bounds the
fetch, falls back to the main archive and installs from the job's
cache. A job lists its packages in APT_PKGS and installs them once.

Container jobs (their image's apt, under its own bounds) are not
checked, nor the jobs in ALLOWED.
"""
import os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ci_local import yaml_load, WF_DIR  # noqa: E402

ALLOWED = {
   ('webOS.yml', 'build'): 'installs the .deb files it downloaded',
}

APT = re.compile(r'\bapt(?:-get)?\s+(?:-\S+\s+)*(install|update|upgrade)\b')

def problems(name, text):
   out = []
   wf = yaml_load(text)
   for jid, job in (wf.get('jobs') or {}).items():
      if not isinstance(job, dict) or job.get('container'):
         continue
      if (name, jid) in ALLOWED:
         continue
      for i, step in enumerate(job.get('steps') or []):
         run = step.get('run') or ''
         for n, line in enumerate(str(run).splitlines(), 1):
            code = line.split('#', 1)[0]
            if APT.search(code):
               out.append('%s: job %s, step "%s", line %d: %s' % (
                  name, jid, step.get('name') or i + 1, n, line.strip()))
   return out

FIXTURE = '''jobs:
  bare:
    runs-on: ubuntu-latest
    steps:
      - name: Deps
        run: |
          sudo apt-get update -qq
          # sudo apt-get install -y comment
          command -v x || sudo apt-get install -y -qq x
  good:
    runs-on: ubuntu-latest
    env:
      APT_PKGS: x
    steps:
      - run: sh .github/workflows/scripts/apt-install.sh $APT_PKGS
  boxed:
    runs-on: ubuntu-latest
    container: debian:12
    steps:
      - run: apt-get install -y x
'''

def selftest():
   got = problems('f.yml', FIXTURE)
   want = ['f.yml: job bare, step "Deps", line 1: sudo apt-get update -qq',
           'f.yml: job bare, step "Deps", line 3: '
           'command -v x || sudo apt-get install -y -qq x']
   if got != want:
      print('selftest FAIL:\n  got  %r\n  want %r' % (got, want))
      return 1
   print('selftest ok')
   return 0

def main(argv):
   if argv[1:] == ['--selftest']:
      return selftest()
   bad = []
   for f in sorted(os.listdir(WF_DIR)):
      if f.endswith(('.yml', '.yaml')):
         with open(os.path.join(WF_DIR, f)) as fh:
            bad += problems(f, fh.read())
   for b in bad:
      print(b)
   if bad:
      print('%d apt call(s) outside apt-install.sh: list the packages in '
            'the job\'s APT_PKGS (see .github/workflows/scripts/'
            'apt-install.sh)' % len(bad))
      return 1
   print('every runner job installs through apt-install.sh')
   return 0

if __name__ == '__main__':
   sys.exit(main(sys.argv))
