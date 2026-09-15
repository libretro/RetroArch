#!/usr/bin/env python3
"""Worker-thread singleton-read audit.

The frontend's state singletons (config_get_ptr(), runloop_state_get_ptr(),
video_state_get_ptr() and the rest) belong to the main thread; a worker
takes what it needs through published snapshots, latched fields or
arguments captured when the work was posted.  This walks the shipping
binary's call graph from every worker entry point - threads the tree
creates, task handlers the threaded queue dispatches, and the audio
pipeline's callback - and reports every reachable function that calls
one of the singletons.

Owner-domain reads are the sanctioned exception: a subsystem's own
thread may read its own pointer-stable singleton (the video thread and
video_state_get_ptr()).  Those pairs live in
tools/thread_read_allow.list as `entry|reader` lines.

Entries are found in the source (sthread_create/pthread_create argument
symbols, task->handler assignments, task_set_handler calls) so a new
thread or handler is audited the day it lands; the call graph comes
from `objdump -d` of the binary, so what is audited is what ships in
that configuration.  Functions the configuration compiles out are not
seen: run against the fullest builds available.

Usage:
  thread_read_audit.py --binary retroarch [--root DIR] [--allow FILE]
  thread_read_audit.py --selftest         (needs gcc and objdump)

Exit status: 0 clean, 1 findings (or self-test failure).
"""

import argparse
import collections
import os
import re
import subprocess
import sys
import tempfile

READERS = (
    "config_get_ptr",
    "runloop_get_flags",
    "runloop_state_get_ptr",
    "video_state_get_ptr",
    "video_driver_get_ptr",
    "input_state_get_ptr",
    "audio_state_get_ptr",
    "menu_state_get_ptr",
    "disp_get_ptr",
)

STHREAD_RE = re.compile(
    r"\bsthread_create(?:_with_priority)?\s*\(\s*&?([a-z_][a-z_0-9]*)\s*,",
    re.S)
PTHREAD_RE = re.compile(
    r"\bpthread_create\s*\([^,;]+,[^,;]+,\s*&?([a-z_][a-z_0-9]*)\s*,",
    re.S)
HANDLER_RE = re.compile(
    r"(?:task->handler\s*=\s*|task_set_handler\s*\([^,]+,\s*)"
    r"([a-z_][a-z_0-9]*)")
SKIP_DIRS = {".git", "deps", "obj-unix", "pkg", "media", "samples"}
CALLBACK_ENTRIES = ("audio_driver_callback",)


def iter_sources(root):
    for base, dirs, files in os.walk(root):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        for f in files:
            if f.endswith((".c", ".m", ".mm", ".cpp")):
                yield os.path.join(base, f)


def source_entries(root):
    entries = {}
    for path in iter_sources(root):
        rel = os.path.relpath(path, root)
        try:
            with open(path, errors="replace") as f:
                text = f.read()
        except OSError:
            continue
        for m in STHREAD_RE.finditer(text):
            entries.setdefault(m.group(1), rel)
        for m in PTHREAD_RE.finditer(text):
            entries.setdefault(m.group(1), rel)
        for m in HANDLER_RE.finditer(text):
            entries.setdefault(m.group(1), rel)
    for name in CALLBACK_ENTRIES:
        entries.setdefault(name, "(pipeline callback)")
    return entries


def call_graph(disasm_lines):
    calls = collections.defaultdict(set)
    defined = set()
    cur = None
    head = re.compile(r"^[0-9a-f]+ <([^>]+)>:")
    site = re.compile(r"\bcall\s+[0-9a-f]+ <([^>+]+)(?:\+0x[0-9a-f]+)?>")
    for line in disasm_lines:
        m = head.match(line)
        if m:
            cur = m.group(1)
            defined.add(cur)
            continue
        m = site.search(line)
        if m and cur:
            calls[cur].add(m.group(1))
    return calls, defined


def audit(calls, defined, entries, allow):
    findings = []
    audited = 0
    for entry in sorted(entries):
        if entry not in defined:
            continue
        audited += 1
        seen = set()
        stack = [entry]
        while stack:
            f = stack.pop()
            if f in seen:
                continue
            seen.add(f)
            stack.extend(calls.get(f, ()))
        for f in sorted(seen):
            hit = calls.get(f, set()) & set(READERS)
            for r in sorted(hit):
                if (entry, r) in allow:
                    continue
                findings.append((entry, f, r))
    return findings, audited


def load_allow(path):
    allow = set()
    if not path or not os.path.exists(path):
        return allow
    with open(path) as f:
        for raw in f:
            raw = raw.strip()
            if not raw or raw.startswith("#"):
                continue
            parts = raw.split("|")
            if len(parts) == 2:
                allow.add((parts[0], parts[1]))
    return allow


def run(binary, root, allow_path):
    entries = source_entries(root)
    try:
        out = subprocess.run(["objdump", "-d", binary],
                             capture_output=True, text=True, check=True)
    except (OSError, subprocess.CalledProcessError) as e:
        print("objdump failed: %s" % e, file=sys.stderr)
        return 2
    calls, defined = call_graph(out.stdout.splitlines())
    allow = load_allow(allow_path)
    findings, audited = audit(calls, defined, entries, allow)
    if findings:
        print("%d worker-thread singleton read(s). A worker takes its"
              % len(findings))
        print("state through published snapshots, latched fields or")
        print("arguments captured when the work was posted; for a")
        print("sanctioned owner-domain read add `entry|reader` to")
        print("tools/thread_read_allow.list.")
        for entry, fn, reader in findings:
            print("%s -> %s calls %s" % (entry, fn, reader))
    print("thread read audit: %d entr%s in binary, %d finding(s), "
          "%d allowlisted pair(s)"
          % (audited, "y" if audited == 1 else "ies",
             len(findings), len(allow)), file=sys.stderr)
    return 1 if findings else 0


FIXTURE = """
#include <pthread.h>
void *config_get_ptr(void) { static int s; return &s; }
static void leaf_reads(void) { config_get_ptr(); }
static void *bad_worker(void *p) { leaf_reads(); return p; }
static void *good_worker(void *p) { return p; }
int main(void)
{
   pthread_t a, b;
   pthread_create(&a, 0, bad_worker, 0);
   pthread_create(&b, 0, good_worker, 0);
   pthread_join(a, 0); pthread_join(b, 0);
   return 0;
}
"""


def selftest():
    with tempfile.TemporaryDirectory() as td:
        src = os.path.join(td, "fix.c")
        binp = os.path.join(td, "fix")
        with open(src, "w") as f:
            f.write(FIXTURE)
        if subprocess.run(["gcc", "-O0", src, "-o", binp,
                           "-lpthread"]).returncode:
            print("selftest: fixture build failed")
            return 1
        entries = source_entries(td)
        out = subprocess.run(["objdump", "-d", binp],
                             capture_output=True, text=True, check=True)
        calls, defined = call_graph(out.stdout.splitlines())
        findings, audited = audit(calls, defined, entries, set())
        ok = (audited == 2
              and any(e == "bad_worker" and r == "config_get_ptr"
                      for e, _, r in findings)
              and not any(e == "good_worker" for e, _, _ in findings))
        if not ok:
            print("selftest: FAIL entries=%d findings=%r"
                  % (audited, findings))
            return 1
        allow = {("bad_worker", "config_get_ptr")}
        findings, _ = audit(calls, defined, entries, allow)
        if findings:
            print("selftest: FAIL allowlist did not clear the finding")
            return 1
        print("selftest: OK")
        return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--binary")
    ap.add_argument("--root", default=".")
    ap.add_argument("--allow", default="tools/thread_read_allow.list")
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()
    if args.selftest:
        sys.exit(selftest())
    if not args.binary:
        ap.error("--binary is required (or --selftest)")
    sys.exit(run(args.binary, args.root, args.allow))


if __name__ == "__main__":
    main()
