#!/usr/bin/env python3
"""
ra_stress -- lifecycle stress driver for RetroArch.

Usage, from the repository root:
    python3 tools/ra_stress.py --core <core> --content <content> [options]

See tools/ra_stress.md for setup, per-platform walkthroughs, how to script
an exact op sequence, and what the driver does when the target dies.

Exits non-zero if the target died, so it drops into a shell loop or a CI
job without extra plumbing.

Runs on Python 2.7 and on 3.2 or newer.  Standard library only, no
third-party packages.  Python 3 is what the shebang selects and what this
is developed against; 2.7 works for boxes whose system interpreter is
still that.

Drives an *unmodified-behaviour* RetroArch build over its own UDP command
interface, cycling core loads, content loads, unloads and driver reinits to
shake out bugs in the teardown and rebuild paths. Nothing here reimplements
RetroArch; every state transition is performed by RetroArch's own code
paths, so whatever it finds is a real bug in the frontend rather than in
the harness.

Driver-agnostic: it exercises whichever video, audio and input drivers the
target is configured with.

Requires the LOAD_CONTENT / VIDEO_REINIT command patch, and
`network_cmd_enable = "true"` in retroarch.cfg on the target.

Targets:
  local   spawn a desktop RetroArch binary; exit status gives the signal
  remote  drive an already-running instance (e.g. an iOS device) by IP

Modes:
  soak      repeat a fixed plan until it dies or the cycle budget runs out
  fuzz      randomised op sequences from a seed; every run is reproducible
  replay    re-run a saved reproducer JSON
  minimize  delta-debug a reproducer down to the shortest crashing sequence

Stall detection: the command interface is serviced from runloop_iterate(),
so command round-trip time is a direct measure of main-thread progress.
Anything that blocks the run loop -- a driver waiting on a timeout, a
lock held too long, a slow teardown -- shows up as an elevated RTT even
when nothing crashes.
"""

import argparse
import json
import os
import random
import shlex
import signal
import socket
import string
import struct
import subprocess
import sys
import time
from datetime import datetime

if sys.version_info < (3, 3):
    sys.stderr.write("ra_stress requires Python 3.3 or newer; "
                     "this is %s\n" % sys.version.split()[0])
    sys.exit(1)

DEFAULT_PORT = 55355
DEFAULT_REMOTE_BASE_PORT = 55400

# RETRO_DEVICE_ID_JOYPAD_*
JOYPAD = {
    "b": 0, "y": 1, "select": 2, "start": 3,
    "up": 4, "down": 5, "left": 6, "right": 7,
    "a": 8, "x": 9, "l": 10, "r": 11,
    "l2": 12, "r2": 13, "l3": 14, "r3": 15,
}
RETRO_DEVICE_JOYPAD = 1
DEFAULT_HOLD_MS = 80


def parse_button_pattern(text):
    """Parse 'select,start:150,left:200' into [(name, hold_seconds), ...].

    An empty string yields an empty pattern, which is a legitimate way to
    say "send nothing here".
    """
    out = []
    for item in text.split(","):
        item = item.strip()
        if not item:
            continue
        name, _, ms = item.partition(":")
        name = name.strip().lower()
        if name not in JOYPAD:
            raise ValueError(
                "unknown button %r (known: %s)"
                % (name, ", ".join(sorted(JOYPAD))))
        try:
            hold = (float(ms) if ms else DEFAULT_HOLD_MS) / 1000.0
        except ValueError:
            raise ValueError("bad hold time %r in %r" % (ms, item))
        out.append((name, hold))
    return out


IS_WINDOWS = (os.name == "nt")

# Everything below keeps the floor at Python 3.3, which is where
# time.monotonic() arrived. Nothing here is on a hot path.


def makedirs(path):
    """os.makedirs(exist_ok=) is 3.2+."""
    try:
        os.makedirs(path)
    except OSError:
        if not os.path.isdir(path):
            raise


def say(msg):
    """say(...) is 3.3+."""
    sys.stdout.write(msg + "\n")
    sys.stdout.flush()


def say_err(msg):
    sys.stderr.write(msg + "\n")
    sys.stderr.flush()


# shlex.quote()'s own safe set, spelled out. Using its regex would mean
# re.ASCII, which is 3.x only; an explicit set is ASCII-only everywhere.
_SAFE_CHARS = frozenset(string.ascii_letters + string.digits + "@%_-+=:,./")


def quote(text):
    """shlex.quote() is 3.3+, and pipes.quote is gone in 3.13."""
    if not text:
        return "''"
    for ch in text:
        if ch not in _SAFE_CHARS:
            return "'" + text.replace("'", "'\"'\"'") + "'"
    return text


def weighted_choice(rng, population, weights):
    """One draw from random.choices(), which is 3.6+.

    Consumes exactly one rng.random() per call, so a given seed still
    yields a stable sequence -- though not the same sequence 3.6's
    implementation would have produced. Saved reproducers are unaffected:
    they carry the full op list, not the seed alone.
    """
    cum = []
    total = 0.0
    for w in weights:
        total += w
        cum.append(total)
    x = rng.random() * total
    for i, c in enumerate(cum):
        if x < c:
            return population[i]
    return population[-1]


def signal_name(num):
    """signal.Signals is a 3.5+ enum."""
    for name in dir(signal):
        if name.startswith("SIG") and not name.startswith("SIG_"):
            if getattr(signal, name) == num:
                return name
    return "signal %d" % num


def split_args(text):
    """Split a command string into argv.

    shlex defaults to POSIX rules, where a backslash is an escape
    character -- which silently eats the separators in every Windows path.
    Non-POSIX mode keeps them.
    """
    return shlex.split(text, posix=not IS_WINDOWS)


def _make_monotonic():
    """A monotonic clock on every supported interpreter.

    time.monotonic() is 3.3+. Falling back to time.time() would be wrong
    rather than merely old: it steps when NTP corrects the clock, and every
    timeout, probe and stall measurement here is an elapsed-time
    calculation. Ask the platform directly instead.
    """
    builtin = getattr(time, "monotonic", None)
    if builtin is not None:
        return builtin

    import ctypes
    import ctypes.util

    if os.name == "nt":
        # GetTickCount64: milliseconds since boot, Vista and later.
        get_ticks = ctypes.windll.kernel32.GetTickCount64
        get_ticks.restype = ctypes.c_ulonglong
        get_ticks.argtypes = ()
        return lambda: get_ticks() / 1000.0

    if sys.platform == "darwin":
        libc = ctypes.CDLL("/usr/lib/libSystem.dylib", use_errno=True)

        class MachTimebase(ctypes.Structure):
            _fields_ = [("numer", ctypes.c_uint32),
                        ("denom", ctypes.c_uint32)]

        timebase = MachTimebase()
        libc.mach_timebase_info(ctypes.byref(timebase))
        libc.mach_absolute_time.restype = ctypes.c_uint64
        libc.mach_absolute_time.argtypes = ()
        scale = float(timebase.numer) / float(timebase.denom) / 1e9
        return lambda: libc.mach_absolute_time() * scale

    class Timespec(ctypes.Structure):
        _fields_ = [("tv_sec", ctypes.c_long),
                    ("tv_nsec", ctypes.c_long)]

    # clock_gettime lives in libc on modern glibc and in librt on older
    # ones. CLOCK_MONOTONIC is 1 on Linux, 4 on the BSDs.
    clock_id = 4 if "bsd" in sys.platform else 1
    for lib_name in (None, "rt"):
        try:
            path = (ctypes.util.find_library(lib_name)
                    if lib_name else None)
            lib = ctypes.CDLL(path, use_errno=True)
            fn = lib.clock_gettime
        except (OSError, AttributeError):
            continue
        fn.argtypes = [ctypes.c_int, ctypes.POINTER(Timespec)]
        ts = Timespec()
        if fn(clock_id, ctypes.byref(ts)) == 0:
            def monotonic():
                spec = Timespec()
                if fn(clock_id, ctypes.byref(spec)) != 0:
                    err = ctypes.get_errno()
                    raise OSError(err, "clock_gettime failed")
                return spec.tv_sec + spec.tv_nsec / 1e9
            return monotonic

    sys.stderr.write(
        "ra_stress: no monotonic clock available; falling back to "
        "time.time(). A clock adjustment mid-run will corrupt stall "
        "measurements.\n")
    return time.time


_monotonic = _make_monotonic()


def now():
    return _monotonic()


def stamp():
    # datetime.timezone is 3.2+; utcnow() is the portable spelling.
    return datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ")


class Dead(Exception):
    """Target stopped answering."""


class RetroPad:
    """Remote RetroPad (network gamepad) sender.

    Separate interface from the command socket: raw
    `struct remote_message { int port, device, index, id; uint16_t state; }`
    over UDP to network_remote_base_port + user. Fire-and-forget, no reply.

    Two constraints from input_driver.c worth knowing:
      - button state is latched, so every press needs a matching release
      - exactly one packet is consumed per user per frame, so transitions
        must be spaced at least a frame apart
    """

    FMT = "<iiiiH2x"        # 20 bytes, native LE, trailing pad

    def __init__(self, host, base_port, user=0, enabled=True):
        self.addr = (host, base_port + user)
        self.user = user
        self.enabled = enabled
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def close(self):
        self.sock.close()

    def _send(self, button_id, state):
        if not self.enabled:
            return
        pkt = struct.pack(self.FMT, self.user, RETRO_DEVICE_JOYPAD, 0,
                          button_id, 1 if state else 0)
        self.sock.sendto(pkt, self.addr)

    def tap(self, name, hold=DEFAULT_HOLD_MS / 1000.0):
        """Press and release. `hold` must exceed one frame."""
        self.down(name)
        time.sleep(hold)
        self.up(name)

    def down(self, name):
        """Press and leave held until an explicit release."""
        self._send(JOYPAD[name], 1)
        time.sleep(0.02)

    def up(self, name):
        self._send(JOYPAD[name], 0)
        time.sleep(0.02)

    def release_all(self):
        for bid in range(16):
            self._send(bid, 0)
            time.sleep(0.02)


# --------------------------------------------------------------------------
# command transport
# --------------------------------------------------------------------------

class CommandChannel:
    def __init__(self, host, port, verbose=False):
        self.addr = (host, port)
        self.verbose = verbose
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setblocking(False)
        self.stalls = []
        self.max_rtt = 0.0

    def close(self):
        self.sock.close()

    def _drain(self):
        while True:
            try:
                self.sock.recvfrom(65535)
            except (OSError, IOError):
                return

    def send(self, text):
        if self.verbose:
            say("    -> %s" % text)
        # On 2.7 a str is already bytes; encoding it would force an
        # implicit ascii decode first and blow up on non-ASCII paths.
        payload = text if isinstance(text, bytes) else text.encode("utf-8")
        self.sock.sendto(payload, self.addr)

    def ask(self, text, timeout):
        """Send a replying command; return (reply, rtt) or (None, elapsed)."""
        self._drain()
        t0 = now()
        self.send(text)
        while True:
            remaining = timeout - (now() - t0)
            if remaining <= 0:
                return None, now() - t0
            try:
                data, _ = self.sock.recvfrom(65535)
            except (OSError, IOError):
                time.sleep(0.005)
                continue
            rtt = now() - t0
            self.max_rtt = max(self.max_rtt, rtt)
            reply = data.decode("utf-8", "replace").strip()
            if self.verbose:
                say("    <- %s  (%.0f ms)" % (reply, rtt * 1000))
            return reply, rtt

    def probe(self, timeout, stall_threshold, label=""):
        reply, rtt = self.ask("VERSION", timeout)
        if reply is None:
            raise Dead("no reply to VERSION after %.1fs" % rtt)
        if rtt >= stall_threshold:
            ev = {"t": stamp(), "rtt_ms": round(rtt * 1000), "at": label}
            self.stalls.append(ev)
            say("  !! main-thread stall %.0f ms  (%s)"
                % (rtt * 1000, label))
        return rtt

    def status(self, timeout):
        reply, _ = self.ask("GET_STATUS", timeout)
        return reply


# --------------------------------------------------------------------------
# targets
# --------------------------------------------------------------------------

class Target:
    def start(self):
        pass

    def stop(self):
        pass

    def death_detail(self):
        return "target stopped responding"

    def logs(self):
        return []


class RemoteTarget(Target):
    """An already-running instance, e.g. RetroArch on an iOS device."""

    def __init__(self, host, relaunch_cmd=None, relaunch_wait=25.0):
        self.host = host
        self.relaunch_cmd = relaunch_cmd
        self.relaunch_wait = relaunch_wait

    def start(self):
        if self.relaunch_cmd:
            say("  relaunching: %s" % self.relaunch_cmd)
            subprocess.call(split_args(self.relaunch_cmd))
            time.sleep(self.relaunch_wait)

    def can_restart(self):
        return self.relaunch_cmd is not None

    def death_detail(self):
        return ("target stopped responding; if the app is gone this is a "
                "process death -- collect the crash report from the device")


class LocalTarget(Target):
    """A desktop RetroArch binary we own, so we can see how it died."""

    def __init__(self, binary, extra_args, log_dir, env=None):
        self.binary = binary
        self.extra_args = extra_args
        self.log_dir = log_dir
        self.env = env or {}
        self.proc = None
        self.logfile = None
        self.logpath = None

    def start(self):
        makedirs(self.log_dir)
        self.logpath = os.path.join(
            self.log_dir, "ra-%s.log" % datetime.now().strftime("%H%M%S_%f"))
        self.logfile = open(self.logpath, "wb")
        env = dict(os.environ)
        env.update(self.env)
        argv = [self.binary, "--verbose"] + self.extra_args
        say("  spawning: %s" % " ".join(quote(a) for a in argv))
        self.proc = subprocess.Popen(
            argv, stdout=self.logfile, stderr=subprocess.STDOUT, env=env)

    def stop(self):
        if self.proc and self.proc.poll() is None:
            self.proc.terminate()
            deadline = now() + 10.0
            while self.proc.poll() is None and now() < deadline:
                time.sleep(0.1)
            if self.proc.poll() is None:
                self.proc.kill()
        if self.logfile:
            self.logfile.close()
            self.logfile = None

    def can_restart(self):
        return True

    # Windows reports a fatal exception as the NTSTATUS value in the exit
    # code rather than as a signal, so "exited with status 3221225477" is
    # really a segfault. Decode the ones worth recognising.
    NTSTATUS = {
        0x80000003: "STATUS_BREAKPOINT (int 3 / assert)",
        0xC0000005: "STATUS_ACCESS_VIOLATION (bad pointer)",
        0xC000001D: "STATUS_ILLEGAL_INSTRUCTION",
        0xC0000094: "STATUS_INTEGER_DIVIDE_BY_ZERO",
        0xC00000FD: "STATUS_STACK_OVERFLOW",
        0xC0000135: "STATUS_DLL_NOT_FOUND",
        0xC0000142: "STATUS_DLL_INIT_FAILED",
        0xC0000374: "STATUS_HEAP_CORRUPTION",
        0xC0000409: "STATUS_STACK_BUFFER_OVERRUN (/GS or __fastfail)",
        0xC0000417: "STATUS_INVALID_CRUNTIME_PARAMETER",
    }

    def death_detail(self):
        if not self.proc:
            return "no process"
        rc = self.proc.poll()
        if rc is None:
            return "process alive but unresponsive -- HANG, not a crash"

        if IS_WINDOWS:
            code = rc & 0xFFFFFFFF
            if code in self.NTSTATUS:
                return "process died: 0x%08X %s" % (code,
                                                    self.NTSTATUS[code])
            if code >= 0xC0000000:
                return ("process died on unrecognised exception 0x%08X"
                        % code)
            return "process exited with status %d" % rc

        if rc < 0:
            return "process died on %s" % signal_name(-rc)
        return "process exited with status %d" % rc

    def logs(self):
        return [self.logpath] if self.logpath else []


# --------------------------------------------------------------------------
# operations
# --------------------------------------------------------------------------

class Runner:
    def __init__(self, chan, args, pad=None):
        self.chan = chan
        self.a = args
        self.pad = pad
        self.open_pattern = parse_button_pattern(args.input_open)
        self.loop_pattern = parse_button_pattern(args.input_loop)

    def play(self, seconds, label):
        """Hold for `seconds` while actually driving the game.

        Idle content sits in attract mode and puts almost no load on the
        driver stack. Bugs that need real work in flight will not show up
        that way, so the soak drives actual play.
        """
        if not self.pad:
            return self.settle(seconds, label)

        end = now() + seconds
        for name, hold in self.open_pattern:
            self.pad.tap(name, hold)
        loop = self.loop_pattern
        i = 0
        while now() < end:
            if loop:
                name, hold = loop[i % len(loop)]
                self.pad.tap(name, hold)
                i += 1
            self.chan.probe(self.a.probe_timeout, self.a.stall_threshold,
                            label)
            time.sleep(self.a.probe_interval)
        self.pad.release_all()

    def settle(self, seconds, label):
        """Hold for `seconds`, probing for main-thread stalls throughout."""
        end = now() + seconds
        while now() < end:
            self.chan.probe(self.a.probe_timeout, self.a.stall_threshold,
                            label)
            time.sleep(self.a.probe_interval)

    def wait_status(self, predicate, timeout, label):
        """Poll GET_STATUS until predicate holds. Long waits are expected
        here -- content loading legitimately blocks the run loop."""
        deadline = now() + timeout
        last = None
        while now() < deadline:
            reply = self.chan.status(self.a.load_timeout)
            if reply is not None:
                last = reply
                if predicate(reply):
                    return reply
            time.sleep(0.25)
        raise Dead("timed out waiting for %s (last status: %r)"
                   % (label, last))

    def op_load(self, core, content):
        base = os.path.basename(content)
        self.chan.send("LOAD_CONTENT %s|%s" % (core, content))
        self.wait_status(
            lambda r: r.startswith("GET_STATUS PLAYING")
            or r.startswith("GET_STATUS PAUSED"),
            self.a.load_timeout_total, "content %s to load" % base)
        self.play(self.a.play_seconds, "playing %s" % base)

    def op_close(self):
        """Quick Menu -> Close Content."""
        self.chan.send("CLOSE_CONTENT")
        self.wait_status(lambda r: "CONTENTLESS" in r,
                         self.a.load_timeout_total, "content to close")
        self.settle(self.a.menu_seconds, "menu/dummy core")

    def _require_pad(self, what):
        if not self.pad:
            raise Dead("sequence uses %s but --input is not enabled" % what)

    def op_press(self, name, hold_ms=None):
        self._require_pad("press")
        hold = (hold_ms if hold_ms is not None
                else DEFAULT_HOLD_MS) / 1000.0
        self.pad.tap(name, hold)

    def op_down(self, name):
        self._require_pad("down")
        self.pad.down(name)

    def op_up(self, name):
        self._require_pad("up")
        self.pad.up(name)

    def op_releaseall(self):
        self._require_pad("releaseall")
        self.pad.release_all()

    def op_wait(self, seconds):
        """Idle for a while, still probing for stalls. Useful for holding a
        button down across a transition."""
        self.settle(float(seconds), "wait")

    def op_start_core(self):
        """Main Menu -> Start Core. Runs the loaded core without content."""
        self.chan.send("START_CORE")
        self.wait_status(
            lambda r: r.startswith("GET_STATUS PLAYING")
            or r.startswith("GET_STATUS PAUSED"),
            self.a.load_timeout_total, "core to start")
        self.play(self.a.play_seconds, "contentless core running")

    def op_load_core(self, core):
        """Main Menu -> Load Core. Loads the core without content."""
        self.chan.send("LOAD_CORE %s" % core)
        self.settle(self.a.menu_seconds, "core loaded")

    def op_unload(self):
        """Main Menu -> Unload Core. Releases the core itself, not just the
        content, so the next load goes through a full dlopen again."""
        self.chan.send("UNLOAD_CORE")
        self.wait_status(lambda r: "CONTENTLESS" in r,
                         self.a.load_timeout_total, "core to unload")
        self.settle(self.a.menu_seconds, "core unloaded")

    def op_reinit(self):
        """Video and input only -- the narrowest reinit scope available.

        DRIVER_FLAGS_NORMALIZE() widens a video-only request to
        video+input, since the input driver is brought up inside
        video_driver_init_internal() and is not separately initialisable.
        """
        self.chan.send("VIDEO_REINIT")
        self.settle(self.a.reinit_seconds, "after VIDEO_REINIT")

    def op_audio_reinit(self):
        self.chan.send("AUDIO_REINIT")
        self.settle(self.a.reinit_seconds, "after AUDIO_REINIT")

    def op_drivers_reinit(self):
        """All drivers, which is what a content transition effectively
        does to the driver stack."""
        self.chan.send("DRIVERS_REINIT")
        self.settle(self.a.reinit_seconds, "after DRIVERS_REINIT")

    def op_menu(self):
        self.chan.send("MENU_TOGGLE")
        self.settle(self.a.menu_seconds, "after MENU_TOGGLE")

    def op_pause(self):
        self.chan.send("PAUSE_TOGGLE")
        self.settle(0.5, "after PAUSE_TOGGLE")

    def op_reset(self):
        self.chan.send("RESET")
        self.settle(1.0, "after RESET")

    def op_savestate(self):
        self.chan.send("SAVE_STATE")
        self.settle(0.75, "after SAVE_STATE")

    def op_loadstate(self):
        self.chan.send("LOAD_STATE")
        self.settle(0.75, "after LOAD_STATE")

    def op_fullscreen(self):
        self.chan.send("FULLSCREEN_TOGGLE")
        self.settle(self.a.reinit_seconds, "after FULLSCREEN_TOGGLE")

    def op_shader(self):
        self.chan.send("SHADER_TOGGLE")
        self.settle(0.75, "after SHADER_TOGGLE")

    def run_op(self, op):
        kind = op[0]
        if kind == "load":
            self.op_load(op[1], op[2])
        elif kind == "close":
            self.op_close()
        elif kind == "unload":
            self.op_unload()
        elif kind == "loadcore":
            self.op_load_core(op[1])
        elif kind == "startcore":
            self.op_start_core()
        elif kind == "press":
            self.op_press(op[1], op[2] if len(op) > 2 else None)
        elif kind == "down":
            self.op_down(op[1])
        elif kind == "up":
            self.op_up(op[1])
        elif kind == "releaseall":
            self.op_releaseall()
        elif kind == "wait":
            self.op_wait(op[1])
        elif kind == "reinit":
            self.op_reinit()
        elif kind == "audioreinit":
            self.op_audio_reinit()
        elif kind == "driversreinit":
            self.op_drivers_reinit()
        elif kind == "menu":
            self.op_menu()
        elif kind == "pause":
            self.op_pause()
        elif kind == "reset":
            self.op_reset()
        elif kind == "savestate":
            self.op_savestate()
        elif kind == "loadstate":
            self.op_loadstate()
        elif kind == "fullscreen":
            self.op_fullscreen()
        elif kind == "shader":
            self.op_shader()
        else:
            raise ValueError("unknown op %r" % (op,))


def describe(op):
    if op[0] == "load":
        return "load %s" % os.path.basename(op[2])
    if op[0] == "loadcore":
        return "loadcore %s" % os.path.basename(op[1])
    if op[0] in ("press", "down", "up"):
        return "%s %s" % (op[0], op[1])
    if op[0] == "wait":
        return "wait %ss" % op[1]
    return op[0]


# --------------------------------------------------------------------------
# plans
# --------------------------------------------------------------------------

def plan_from_log(core, contents):
    """The shape of the 2026-07-27 session: three content loads, each
    separated by a return to the dummy core."""
    ops = []
    for c in contents:
        ops.append(("load", core, c))
        ops.append(("close",))
    return ops


def plan_fuzz(rng, core, contents, length):
    """Randomised sequence. Keeps track of whether content is loaded so we
    only emit ops that are meaningful in the current state."""
    ops = []
    loaded = False
    while len(ops) < length:
        if not loaded:
            choice = weighted_choice(
                rng,
                ["load", "reinit", "driversreinit", "audioreinit", "menu"],
                [62, 14, 10, 6, 8])
        else:
            choice = weighted_choice(
                rng,
                ["close", "unload", "reinit", "driversreinit",
                 "audioreinit", "menu", "pause", "reset",
                 "savestate", "loadstate", "shader", "load"],
                [20, 12, 14, 10, 6, 6, 4, 4, 4, 4, 4, 12])
        if choice == "load":
            ops.append(("load", core, rng.choice(contents)))
            loaded = True
        elif choice in ("close", "unload"):
            ops.append((choice,))
            loaded = False
        else:
            ops.append((choice,))
    return ops


# --------------------------------------------------------------------------
# session
# --------------------------------------------------------------------------

def run_sequence(target, args, ops, label):
    """Run one full session. Returns a result dict."""
    result = {
        "label": label,
        "started": stamp(),
        "ops": [list(o) for o in ops],
        "crashed": False,
        "completed": 0,
        "stalls": [],
        "max_rtt_ms": 0,
    }

    target.start()
    chan = CommandChannel(args.host, args.port, args.verbose)
    pad = RetroPad(args.host, args.remote_base_port, args.input_port,
                   enabled=args.input)
    try:
        # Preflight: the instance must be answering before we trust any
        # later silence as a death.
        deadline = now() + args.startup_timeout
        while True:
            reply, _ = chan.ask("VERSION", 2.0)
            if reply:
                say("  target alive: %s" % reply)
                break
            if now() > deadline:
                say_err("  !! target never answered on %s:%d -- is "
                        "network_cmd_enable set?" % (args.host, args.port))
                result["error"] = "no response at startup"
                return result
            time.sleep(1.0)

        runner = Runner(chan, args, pad if args.input else None)
        for i, op in enumerate(ops):
            say("  [%3d/%3d] %s" % (i + 1, len(ops), describe(op)))
            try:
                runner.run_op(op)
            except Dead as e:
                result["crashed"] = True
                result["completed"] = i
                result["died_at"] = describe(op)
                result["died_index"] = i
                result["reason"] = str(e)
                time.sleep(2.0)
                result["detail"] = target.death_detail()
                say("  ** DIED at op %d (%s): %s\n     %s"
                    % (i, describe(op), e, result["detail"]))
                break
        else:
            result["completed"] = len(ops)
    finally:
        result["stalls"] = chan.stalls
        result["max_rtt_ms"] = round(chan.max_rtt * 1000)
        result["logs"] = target.logs()
        pad.close()
        chan.close()
        if not result["crashed"]:
            target.stop()

    return result


def save_repro(outdir, result, args, seed=None):
    makedirs(outdir)
    path = os.path.join(
        outdir, "repro-%s.json" % datetime.now().strftime("%Y%m%d-%H%M%S"))
    payload = dict(result)
    payload["seed"] = seed
    payload["host"] = args.host
    with open(path, "w") as f:
        json.dump(payload, f, indent=2)
    say("  reproducer written: %s" % path)
    return path


# --------------------------------------------------------------------------
# minimization (ddmin over the op sequence)
# --------------------------------------------------------------------------

def minimize(target, args, ops):
    """Delta-debug the sequence down to a minimal one that still dies.
    Requires a restartable target."""
    def dies(candidate):
        if not candidate:
            return False
        r = run_sequence(target, args, candidate, "minimize")
        target.stop()
        time.sleep(args.restart_pause)
        return r["crashed"]

    say("\n== minimizing %d ops ==" % len(ops))
    n = 2
    current = list(ops)
    while len(current) >= 2:
        chunk = max(1, len(current) // n)
        chunks = [current[i:i + chunk] for i in range(0, len(current), chunk)]
        reduced = False

        # Index-based complements, because ops repeat and identity
        # comparison would drop the wrong ones.
        for idx in range(len(chunks)):
            complement = []
            for j, c in enumerate(chunks):
                if j != idx:
                    complement.extend(c)
            say("  trying %d ops (dropping chunk %d/%d)"
                % (len(complement), idx + 1, len(chunks)))
            if dies(complement):
                current = complement
                n = max(n - 1, 2)
                reduced = True
                break

        if not reduced:
            if n >= len(current):
                break
            n = min(len(current), n * 2)

    say("== minimized to %d ops ==" % len(current))
    for op in current:
        say("   %s" % describe(op))
    return current


# --------------------------------------------------------------------------
# main
# --------------------------------------------------------------------------

def build_target(args):
    if args.binary:
        env = {}
        for kv in args.env:
            k, _, v = kv.partition("=")
            env[k] = v
        return LocalTarget(args.binary, split_args(args.binary_args),
                           args.outdir, env)
    return RemoteTarget(args.host, args.relaunch_cmd, args.relaunch_wait)


def main():
    p = argparse.ArgumentParser(
        description="RetroArch lifecycle stress driver")

    p.add_argument("--host", default="127.0.0.1",
                   help="IP of the RetroArch instance (the device, for iOS)")
    p.add_argument("--port", type=int, default=DEFAULT_PORT)

    p.add_argument("--binary",
                   help="desktop RetroArch binary to spawn (local target); "
                        "omit to drive an already-running instance")
    p.add_argument("--binary-args", default="",
                   help="extra args for the spawned binary")
    p.add_argument("--env", action="append", default=[],
                   help="KEY=VALUE for the spawned binary, repeatable "
                        "for validation layers, sanitisers, debug knobs")
    p.add_argument("--relaunch-cmd",
                   help="shell command that relaunches the app on a remote "
                        "target, e.g. 'xcrun devicectl device process launch "
                        "--device UDID org.warmenhoven.RetroArch'")
    p.add_argument("--relaunch-wait", type=float, default=25.0)

    p.add_argument("--core",
                   help="core path on the target; required for soak and "
                        "fuzz, ignored by replay and minimize since a "
                        "reproducer carries a core path per load op")
    p.add_argument("--content", action="append", default=[],
                   help="content path on the target, repeatable; same "
                        "applies")

    p.add_argument("--mode", choices=["soak", "fuzz", "replay", "minimize"],
                   default="soak")
    p.add_argument("--cycles", type=int, default=25,
                   help="soak: repetitions of the plan")
    p.add_argument("--fuzz-length", type=int, default=40)
    p.add_argument("--fuzz-runs", type=int, default=20)
    p.add_argument("--seed", type=int)
    p.add_argument("--replay", help="reproducer JSON to replay or minimize")

    p.add_argument("--play-seconds", type=float, default=6.0)
    p.add_argument("--menu-seconds", type=float, default=3.0)
    p.add_argument("--reinit-seconds", type=float, default=3.0)
    p.add_argument("--probe-interval", type=float, default=0.25)
    p.add_argument("--probe-timeout", type=float, default=8.0)
    p.add_argument("--stall-threshold", type=float, default=0.75,
                   help="RTT above which to record a main-thread stall; "
                        "set below the shortest timeout you expect a "
                        "blocked run loop to sit on")
    p.add_argument("--load-timeout", type=float, default=20.0)
    p.add_argument("--load-timeout-total", type=float, default=90.0)
    p.add_argument("--startup-timeout", type=float, default=60.0)
    p.add_argument("--restart-pause", type=float, default=5.0)

    p.add_argument("--input", action="store_true",
                   help="drive gameplay over the Remote RetroPad interface "
                        "while content is running, instead of letting it "
                        "idle in attract mode")
    p.add_argument("--input-open", default="select,start",
                   help="buttons sent once when content starts, as "
                        "'name[:hold_ms],...' (default coins up and starts)")
    p.add_argument("--input-loop", default="left,right,b,b,left,right,b",
                   help="buttons cycled for the rest of the playing phase, "
                        "same syntax; pass '' to send nothing")
    p.add_argument("--input-port", type=int, default=0,
                   help="RetroPad user index (0-based)")
    p.add_argument("--remote-base-port", type=int,
                   default=DEFAULT_REMOTE_BASE_PORT,
                   help="network_remote_base_port on the target")

    p.add_argument("--outdir", default="./ra-stress-out")
    p.add_argument("-v", "--verbose", action="store_true")

    args = p.parse_args()
    makedirs(args.outdir)

    if args.mode in ("soak", "fuzz") and not (args.core and args.content):
        p.error("--core and at least one --content are required for %s"
                % args.mode)

    for opt in ("input_open", "input_loop"):
        try:
            parse_button_pattern(getattr(args, opt))
        except ValueError as e:
            p.error("--%s: %s" % (opt.replace("_", "-"), e))

    seed = args.seed if args.seed is not None else random.randrange(1 << 30)
    rng = random.Random(seed)
    target = build_target(args)

    say("ra_stress  target=%s  mode=%s  seed=%d"
        % (args.host, args.mode, seed))
    say("stall threshold %.0f ms; command RTT measures main-thread "
        "progress\n" % (args.stall_threshold * 1000))

    if args.mode == "replay" or args.mode == "minimize":
        if not args.replay:
            p.error("--replay is required for %s" % args.mode)
        with open(args.replay) as f:
            saved = json.load(f)
        ops = [tuple(o) for o in saved["ops"]]
        if args.mode == "replay":
            r = run_sequence(target, args, ops, "replay")
            say("\ncrashed=%s  stalls=%d  max_rtt=%dms"
                % (r["crashed"], len(r["stalls"]), r["max_rtt_ms"]))
            save_repro(args.outdir, r, args, seed)
            return 1 if (r["crashed"] or r.get("error")) else 0
        if not target.can_restart():
            p.error("minimize needs a restartable target: pass --binary or "
                    "--relaunch-cmd")
        minimal = minimize(target, args, ops)
        r = {"label": "minimized", "started": stamp(),
             "ops": [list(o) for o in minimal], "crashed": True,
             "completed": len(minimal), "stalls": [], "max_rtt_ms": 0}
        save_repro(args.outdir, r, args, seed)
        return 0

    if args.mode == "soak":
        base = plan_from_log(args.core, args.content)
        ops = base * args.cycles
        r = run_sequence(target, args, ops, "soak")
        say("\ncompleted %d/%d ops  crashed=%s  stalls=%d  max_rtt=%dms"
            % (r["completed"], len(ops), r["crashed"], len(r["stalls"]),
               r["max_rtt_ms"]))
        if r["crashed"] or r["stalls"]:
            save_repro(args.outdir, r, args, seed)
        return 1 if (r["crashed"] or r.get("error")) else 0

    # fuzz
    for run in range(args.fuzz_runs):
        run_seed = rng.randrange(1 << 30)
        ops = plan_fuzz(random.Random(run_seed), args.core, args.content,
                        args.fuzz_length)
        say("\n== fuzz run %d/%d  seed=%d  %d ops =="
            % (run + 1, args.fuzz_runs, run_seed, len(ops)))
        r = run_sequence(target, args, ops, "fuzz seed=%d" % run_seed)
        if r.get("error"):
            return 1
        if r["crashed"]:
            path = save_repro(args.outdir, r, args, run_seed)
            say("\nCRASH REPRODUCED. Minimize with:\n"
                "  %s --mode minimize --replay %s --core %s %s\n"
                % (sys.argv[0], path, args.core,
                   " ".join("--content %s" % quote(c)
                            for c in args.content)))
            return 1
        if r["stalls"]:
            save_repro(args.outdir, r, args, run_seed)
        target.stop()
        time.sleep(args.restart_pause)

    say("\nno crash in %d fuzz runs" % args.fuzz_runs)
    return 0


if __name__ == "__main__":
    sys.exit(main())
