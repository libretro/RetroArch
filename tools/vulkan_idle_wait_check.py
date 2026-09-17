#!/usr/bin/env python3
"""Vulkan idle-wait check.

Flags every vkQueueWaitIdle and vkDeviceWaitIdle call in the Vulkan
video driver, the slang filter chain and the Vulkan context code that
is not on the allowlist.

Rationale: an idle wait drains the whole queue, so under threaded video
it also drains the work a hardware core is submitting from its own
thread, and it has to run with queue_lock held, since the Vulkan spec
defines vkDeviceWaitIdle as vkQueueWaitIdle on every queue. A hardware
core that is parked in lock_queue with work still to submit - work the
queue may be waiting on - then never lets the queue go idle, and the
frontend stops (the parallel-GS/Granite present deadlock). The driver's
own objects are only referenced by the driver's own submissions, each
of which carries a fence, so the right wait is on those fences, lock
free: vulkan_wait_own_submissions() in gfx/drivers/vulkan.c. Every
idle wait that used to stand in for it has been replaced; this check
keeps them from coming back.

The allowlist names the sites that legitimately remain, as
`file|function` entries, one per line, with a count. A new idle wait
fails the check; so does a stale entry, so the list shrinks with the
code.

Usage:
  vulkan_idle_wait_check.py [--root DIR] [--allow FILE]
  vulkan_idle_wait_check.py --selftest

Exit status: 0 clean, 1 findings (or self-test failure).
"""

import argparse
import os
import re
import sys
import tempfile

FILES = [
    "gfx/drivers/vulkan.c",
    "gfx/drivers_shader/shader_vulkan.c",
    "gfx/common/vulkan_common.c",
    "gfx/video_thread_hw.c",
]

CALL_RE = re.compile(r"\bvk(Queue|Device)WaitIdle\s*\(")
# A function definition in this tree starts at column 0 with the
# return type and has the name directly before the parameter list on
# the same line: "static void vulkan_free(void *data)".
FUNC_RE = re.compile(r"^[A-Za-z_][A-Za-z_0-9 \t*]*?\b([A-Za-z_][A-Za-z_0-9]*)\s*\(")
COMMENT_LINE_RE = re.compile(r"^\s*(\*|/\*|//)")


def strip_comments(text):
    """Blank out comments and string bodies, keeping line structure."""
    out = []
    i = 0
    n = len(text)
    while i < n:
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("".join("\n" if c == "\n" else " " for c in text[i:j]))
            i = j
        elif text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif text[i] == '"':
            j = i + 1
            while j < n and text[j] != '"':
                if text[j] == "\\":
                    j += 1
                j += 1
            out.append('"' + " " * max(0, j - i - 1) + '"')
            i = j + 1
        else:
            out.append(text[i])
            i += 1
    return "".join(out)


def scan_file(path, rel):
    """Return {function: count} of idle waits in one file."""
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        text = strip_comments(f.read())
    found = {}
    func = "<file scope>"
    for line in text.split("\n"):
        m = FUNC_RE.match(line)
        if m and not line.rstrip().endswith(";"):
            func = m.group(1)
        if CALL_RE.search(line):
            key = "%s|%s" % (rel, func)
            found[key] = found.get(key, 0) + 1
    return found


def load_allow(path):
    allow = {}
    if not path or not os.path.exists(path):
        return allow
    with open(path, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            parts = line.split("|")
            if len(parts) != 3:
                sys.stderr.write("bad allowlist line: %s\n" % raw.rstrip())
                sys.exit(1)
            allow["%s|%s" % (parts[0], parts[1])] = int(parts[2])
    return allow


def check(root, allow_path):
    allow = load_allow(allow_path)
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
            sys.stderr.write("idle wait: %s: %d call(s), %d allowed\n"
                             % (key, n, want))
            bad += 1
    for key in sorted(allow):
        if found.get(key, 0) < allow[key]:
            sys.stderr.write("stale allowlist entry: %s|%d (found %d)\n"
                             % (key, allow[key], found.get(key, 0)))
            bad += 1
    return bad


SELFTEST_SRC = """\
/* vkQueueWaitIdle(in a comment) */
static void keep_me(void *p)
{
   vkQueueWaitIdle(q);
}

static void new_offender(void *p)
{
   const char *s = "vkDeviceWaitIdle(in a string)";
   vkDeviceWaitIdle(d);
}
"""


def selftest():
    tmp = tempfile.mkdtemp()
    try:
        os.makedirs(os.path.join(tmp, "gfx", "drivers"))
        src = os.path.join(tmp, "gfx", "drivers", "vulkan.c")
        with open(src, "w") as f:
            f.write(SELFTEST_SRC)
        allow = os.path.join(tmp, "allow.list")
        with open(allow, "w") as f:
            f.write("gfx/drivers/vulkan.c|keep_me|1\n")
            f.write("gfx/drivers/vulkan.c|gone|1\n")
        found = scan_file(src, "gfx/drivers/vulkan.c")
        if found != {"gfx/drivers/vulkan.c|keep_me": 1,
                     "gfx/drivers/vulkan.c|new_offender": 1}:
            sys.stderr.write("selftest: scan mismatch: %r\n" % found)
            return 1
        # one new offender, one stale entry: two findings
        if check(tmp, allow) != 2:
            sys.stderr.write("selftest: expected 2 findings\n")
            return 1
        sys.stderr.write("selftest: ok\n")
        return 0
    finally:
        import shutil
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
                                       "vulkan_idle_wait_allow.list")
    bad = check(args.root, allow)
    if bad:
        sys.stderr.write("%d idle-wait finding(s)\n" % bad)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
