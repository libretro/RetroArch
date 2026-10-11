#!/usr/bin/env python3
"""D3D12 wait check.

Flags every whole-queue drain (d3d12_queue_drain) and every CPU wait
on a fence (SetEventOnCompletion) in the D3D12 driver that is not on
the allowlist.

Rationale: a drain signals the queue and waits for everything on it,
so under threaded video it also waits for the work a hardware core
has queued since - its next frame - and it is a stall on every frame
it sits in. The driver's own submissions each carry a signal of their
own (frameSignal for the frame command list, the per-buffer fences of
the upload and lend paths), and what a frame needs is to wait for its
predecessor's (d3d12_fence_wait, d3d12_cmd_begin) or not at all: a
buffer a frame still reads is retired behind its fence (d3d12_retire),
and a lend or an update that finds its buffer busy is declined or
dropped. The frame paths drained once; this check keeps a drain, or a
new fence wait, from coming back without a reason written down.

The allowlist names the sites that legitimately remain, as
`file|function|count` entries, one per line. A new site fails the
check; so does a stale entry, so the list shrinks with the code.

Usage:
  d3d12_idle_wait_check.py [--root DIR] [--allow FILE]
  d3d12_idle_wait_check.py --selftest

Exit status: 0 clean, 1 findings (or self-test failure).
"""

import argparse
import os
import re
import shutil
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vulkan_idle_wait_check import FUNC_RE, strip_comments  # noqa: E402

FILES = [
    "gfx/drivers/d3d12.c",
]

CALL_RE = re.compile(r"\b(d3d12_queue_drain|SetEventOnCompletion)\s*\(")


def scan_file(path, rel):
    """Return {file|function|call: count} of waits in one file."""
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        text = strip_comments(f.read())
    found = {}
    func = "<file scope>"
    for line in text.split("\n"):
        m = FUNC_RE.match(line)
        if m and not line.rstrip().endswith(";"):
            func = m.group(1)
            # The definition of the drain itself is not a call
            continue
        for c in CALL_RE.finditer(line):
            key = "%s|%s|%s" % (rel, func, c.group(1))
            found[key] = found.get(key, 0) + 1
    return found


def check(root, allow_path):
    allow = load_allow_calls(allow_path)
    found = {}
    for rel in FILES:
        path = os.path.join(root, rel)
        if os.path.exists(path):
            found.update(scan_file(path, rel))
    bad = 0
    for key in sorted(found):
        n = found[key]
        want = allow.get(key, 0)
        if n > want:
            sys.stderr.write("wait: %s: %d call(s), %d allowed\n"
                             % (key, n, want))
            bad += 1
    for key in sorted(allow):
        if found.get(key, 0) < allow[key]:
            sys.stderr.write("stale allowlist entry: %s|%d (found %d)\n"
                             % (key, allow[key], found.get(key, 0)))
            bad += 1
    return bad


def load_allow_calls(path):
    """file|function|call|count entries."""
    allow = {}
    if not path or not os.path.exists(path):
        return allow
    with open(path, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            parts = line.split("|")
            if len(parts) != 4:
                sys.stderr.write("bad allowlist line: %s\n" % raw.rstrip())
                sys.exit(1)
            allow["|".join(parts[:3])] = int(parts[3])
    return allow


SELFTEST_SRC = """\
/* d3d12_queue_drain(in a comment) */
static void d3d12_queue_drain(d3d12_video_t *d3d12)
{
   f->lpVtbl->SetEventOnCompletion(f, v, e);
}

static void keep_me(void *p)
{
   d3d12_queue_drain(d3d12);
}

static void new_offender(void *p)
{
   const char *s = "d3d12_queue_drain(in a string)";
   d3d12_queue_drain(d3d12);
}
"""


def selftest():
    tmp = tempfile.mkdtemp()
    try:
        os.makedirs(os.path.join(tmp, "gfx", "drivers"))
        rel = "gfx/drivers/d3d12.c"
        src = os.path.join(tmp, rel)
        with open(src, "w") as f:
            f.write(SELFTEST_SRC)
        allow = os.path.join(tmp, "allow.list")
        with open(allow, "w") as f:
            f.write(rel + "|d3d12_queue_drain|SetEventOnCompletion|1\n")
            f.write(rel + "|keep_me|d3d12_queue_drain|1\n")
            f.write(rel + "|gone|d3d12_queue_drain|1\n")
        found = scan_file(src, rel)
        want = {rel + "|d3d12_queue_drain|SetEventOnCompletion": 1,
                rel + "|keep_me|d3d12_queue_drain": 1,
                rel + "|new_offender|d3d12_queue_drain": 1}
        if found != want:
            sys.stderr.write("selftest: scan mismatch: %r\n" % found)
            return 1
        # one new offender, one stale entry: two findings
        if check(tmp, allow) != 2:
            sys.stderr.write("selftest: expected 2 findings\n")
            return 1
        sys.stderr.write("selftest: ok\n")
        return 0
    finally:
        shutil.rmtree(tmp)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--allow", default=None)
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()
    if args.selftest:
        return selftest()
    allow = args.allow or os.path.join(args.root, "tools",
                                       "d3d12_idle_wait_allow.list")
    bad = check(args.root, allow)
    if bad:
        sys.stderr.write("%d wait finding(s)\n" % bad)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
