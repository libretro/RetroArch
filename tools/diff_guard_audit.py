#!/usr/bin/env python3
"""Guard audit: given a git diff range, list every #ifdef guard the diff
touches or sits adjacent to, so each gets a -D lane or structural audit."""
import re, subprocess, sys
rng = sys.argv[1] if len(sys.argv) > 1 else 'HEAD~1..HEAD'
d = subprocess.run(['git','diff','-U6',rng], capture_output=True, text=True).stdout
guards = set()
for m in re.finditer(r'^[+\- ].*#\s*(?:ifdef|ifndef|elif|if)\s+(.*)$', d, re.M):
    for tok in re.findall(r'\b(HAVE_\w+|RARCH_\w+|_3DS|ANDROID|IOS|OSX|_WIN32|__\w+__)\b', m.group(1)):
        guards.add(tok)
print("guards in/adjacent to diff:", sorted(guards) if guards else "none")
