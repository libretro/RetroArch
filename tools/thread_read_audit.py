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

A boundary function crosses to the main thread itself - it defers
off-main callers before any singleton is touched, the way
runloop_msg_queue_push() queues a worker's message for the main
thread's drain - and is declared as an `@boundary name` line.  The
walk does not descend into a boundary, and the boundary's own reads
are not charged to its workers: the off-main prefix that makes this
true is what code review holds, so a boundary declaration is a claim
about that function, reviewed like the allowlist pairs.

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
# Driver frame-context functions: under the threaded wrapper these run
# on the video thread while the main thread runs free - the wrapper
# dispatches them through an indirect driver->frame edge the binary
# walk cannot follow, so they are listed as entries directly. Only the
# free-running functions belong here: blocking command handlers (init,
# set_shader, the pokes) execute while the main thread is parked in
# send-and-wait, where a settings read cannot race, and must NOT be
# added or every legal init-time read becomes a false finding. Symbols
# absent from a given build are skipped like any other entry.
#
# Part of the standing gate since the 29-finding backlog it opened
# with was driven to zero: latches and snapshots for the settings,
# rotation, input-sensor, spirv-toggle and HDR-recreate classes,
# and reasoned allowlist entries for the batch-confined and
# vtable-dispatch reads. --frame-context is accepted as a no-op for
# compatibility.
# Audio device callbacks: the function an audio backend calls on its
# own thread - SDL's device thread, libpulse's and PipeWire's loop, the
# CoreAudio render thread, JACK's process thread, the ASIO driver
# thread, the DSP and AX frame interrupts. The backend owns the thread,
# so no sthread_create in this tree names them and the binary walk has
# no edge to follow; they are listed here for the same reason the frame
# context functions are. A driver's init, open and free run on the main
# thread and are not entries - only what the device calls back into.
# Symbols absent from a given build are skipped like any other entry.
DEVICE_CALLBACK_ENTRIES = (
    "sdl1_audio_playback_cb",
    "sdl2_audio_playback_cb",
    "sdl2_microphone_record_cb",
    "sdl3_audio_stream_cb",
    "coreaudio_audio_write_cb",
    "coreaudio_mic_input_cb",
    "ja_process_cb",
    "ja_sample_rate_cb",
    "ja_buffer_size_cb",
    "ja_xrun_cb",
    "ja_shutdown_cb",
    "coreaudio_output_default_listener",
    "coreaudio_output_default_listener_old",
    "coreaudio_mic_default_listener",
    "coreaudio_mic_default_listener_old",
    "opensl_callback",
    "ctr_dsp_audio_frame_cb",
    "gx_audio_dma_callback",
    "wiiu_ax_callback",
    "audioworklet_process_cb",
    "pulse_stream_state_cb",
    "pulse_stream_request_cb",
    "pulse_stream_latency_update_cb",
    "pulse_underrun_update_cb",
    "pulse_buffer_attr_cb",
    "pwire_playback_process_cb",
    "pwire_capture_process_cb",
    "pwire_stream_state_changed_cb",
    "pwire_mic_stream_state_changed_cb",
    "pwire_stream_destroy_cb",
    "asio_cb_buffer_switch",
    "asio_cb_buffer_switch_time_info",
    "asio_cb_sample_rate_changed",
    "asio_cb_message",
    "xa_voice_on_buffer_end",
)

INCLUDE_FRAME_CONTEXT = True
FRAME_CONTEXT_ENTRIES = (
    "gl2_frame",
    "gl2_set_texture_frame",
    "gl3_frame",
    "gl3_set_texture_frame",
    "vulkan_frame",
    "vulkan_set_texture_frame",
    "d3d11_gfx_frame",
    "d3d12_gfx_frame",
)


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
            # A create-call forwarding a function-pointer parameter -
            # sthread_create(thread_func, ...) inside rthreads.c's own
            # sthread_create - names no function. The parameter is
            # declared as (*name)( in the same file; a real entry never
            # is.
            if re.search(r"\(\s*\*\s*%s\s*\)\s*\(" % re.escape(m.group(1)),
                         text):
                continue
            entries.setdefault(m.group(1), rel)
        for m in PTHREAD_RE.finditer(text):
            if re.search(r"\(\s*\*\s*%s\s*\)\s*\(" % re.escape(m.group(1)),
                         text):
                continue
            entries.setdefault(m.group(1), rel)
        for m in HANDLER_RE.finditer(text):
            entries.setdefault(m.group(1), rel)
    for name in CALLBACK_ENTRIES:
        entries.setdefault(name, "(pipeline callback)")
    for name in DEVICE_CALLBACK_ENTRIES:
        entries.setdefault(name, "(audio device callback)")
    if INCLUDE_FRAME_CONTEXT:
        for name in FRAME_CONTEXT_ENTRIES:
            entries.setdefault(name, "(threaded frame context)")
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


def audit(calls, defined, entries, allow, boundaries=frozenset()):
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
            if f in boundaries and f != entry:
                continue
            stack.extend(calls.get(f, ()))
        for f in sorted(seen):
            if f in boundaries and f != entry:
                continue
            hit = calls.get(f, set()) & set(READERS)
            for r in sorted(hit):
                # Two grains: (callee, reader) allowlists one verified
                # function wherever it appears - the surgical form the
                # list's entries use - and (entry, reader) allowlists a
                # reader across a whole root, for roots that own their
                # state wholesale.
                if (f, r) in allow or (entry, r) in allow:
                    continue
                findings.append((entry, f, r))
    return findings, audited


def load_allow(path):
    allow = set()
    boundaries = set()
    if not path or not os.path.exists(path):
        return allow, boundaries
    with open(path) as f:
        for raw in f:
            raw = raw.strip()
            if not raw or raw.startswith("#"):
                continue
            if raw.startswith("@boundary"):
                parts = raw.split()
                if len(parts) == 2:
                    boundaries.add(parts[1])
                continue
            parts = raw.split("|")
            if len(parts) == 2:
                allow.add((parts[0], parts[1]))
    return allow, boundaries


def run(binary, root, allow_path, list_unaudited=False,
        objdump="objdump"):
    entries = source_entries(root)
    try:
        out = subprocess.run([objdump, "-d", binary],
                             capture_output=True, text=True, check=True)
    except (OSError, subprocess.CalledProcessError) as e:
        print("objdump failed: %s" % e, file=sys.stderr)
        return 2
    calls, defined = call_graph(out.stdout.splitlines())
    allow, boundaries = load_allow(allow_path)
    findings, audited = audit(calls, defined, entries, allow, boundaries)
    if findings:
        print("%d worker-thread singleton read(s). A worker takes its"
              % len(findings))
        print("state through published snapshots, latched fields or")
        print("arguments captured when the work was posted; for a")
        print("sanctioned owner-domain read add `entry|reader` to")
        print("tools/thread_read_allow.list.")
        for entry, fn, reader in findings:
            print("%s -> %s calls %s" % (entry, fn, reader))
    if list_unaudited:
        absent = sorted(e for e in entries if e not in defined)
        print("%d source entr%s absent from this binary - compiled out"
              % (len(absent), "y" if len(absent) == 1 else "ies"))
        print("here, so nothing this run says covers them. Audit them")
        print("from a build of their platform or configuration:")
        for e in absent:
            print("  %s  (%s)" % (e, entries[e]))
    print("thread read audit: %d entr%s in binary, %d finding(s), "
          "%d allowlisted pair(s), %d boundar%s"
          % (audited, "y" if audited == 1 else "ies",
             len(findings), len(allow),
             len(boundaries), "y" if len(boundaries) == 1 else "ies"),
          file=sys.stderr)
    return 1 if findings else 0


FIXTURE = """
#include <pthread.h>
void *config_get_ptr(void) { static int s; return &s; }
static void leaf_reads(void) { config_get_ptr(); }
static void *bad_worker(void *p) { leaf_reads(); return p; }
static void crossing(void) { leaf_reads(); }
static void *deferring_worker(void *p) { crossing(); return p; }
static void *good_worker(void *p) { return p; }
int main(void)
{
   pthread_t a, b, c;
   pthread_create(&a, 0, bad_worker, 0);
   pthread_create(&b, 0, good_worker, 0);
   pthread_create(&c, 0, deferring_worker, 0);
   pthread_join(a, 0); pthread_join(b, 0); pthread_join(c, 0);
   return 0;
}
"""


def selftest():
    with tempfile.TemporaryDirectory() as td:
        src = os.path.join(td, "fix.c")
        binp = os.path.join(td, "fix")
        with open(src, "w") as f:
            f.write(FIXTURE)
        # A source entry with no definition in the binary: the shape
        # --list-unaudited reports. Not compiled, only discovered.
        with open(os.path.join(td, "ghost.c"), "w") as f:
            f.write("void *ghost_worker(void *p);\n"
                    "void ghost_spawn(void *t)\n"
                    "{ pthread_create(t, 0, ghost_worker, 0); }\n")
        if subprocess.run(["gcc", "-O0", src, "-o", binp,
                           "-lpthread"]).returncode:
            print("selftest: fixture build failed")
            return 1
        entries = source_entries(td)
        out = subprocess.run(["objdump", "-d", binp],
                             capture_output=True, text=True, check=True)
        calls, defined = call_graph(out.stdout.splitlines())
        findings, audited = audit(calls, defined, entries, set())
        ok = (audited == 3
              and any(e == "bad_worker" and r == "config_get_ptr"
                      for e, _, r in findings)
              and any(e == "deferring_worker" for e, _, _ in findings)
              and not any(e == "good_worker" for e, _, _ in findings)
              and "ghost_worker" in entries
              and "ghost_worker" not in defined)
        if not ok:
            print("selftest: FAIL entries=%d findings=%r"
                  % (audited, findings))
            return 1
        allow = {("bad_worker", "config_get_ptr")}
        findings, _ = audit(calls, defined, entries, allow,
                            {"crossing"})
        if findings:
            print("selftest: FAIL allowlist/boundary did not clear")
            return 1
        print("selftest: OK")
        return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--binary")
    ap.add_argument("--root", default=".")
    ap.add_argument("--allow", default="tools/thread_read_allow.list")
    ap.add_argument("--objdump", default="objdump",
        help="objdump to use; e.g. x86_64-w64-mingw32-objdump for a "
             "PE binary from the mingw cross build")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--frame-context", action="store_true",
        help="accepted for compatibility; frame-context entries are "
             "part of the standing gate")
    ap.add_argument("--list-unaudited", action="store_true",
                    help="also list source entry points absent from "
                         "this binary (compiled out on this build)")
    args = ap.parse_args()

    if args.selftest:
        sys.exit(selftest())
    if not args.binary:
        ap.error("--binary is required (or --selftest)")
    sys.exit(run(args.binary, args.root, args.allow,
                 args.list_unaudited, objdump=args.objdump))


if __name__ == "__main__":
    main()
