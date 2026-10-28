#!/usr/bin/env python3
"""Two real RetroArch processes in netplay over the loopback.

A host and a client run netplay_e2e_core with no window and no sound.
The client's first player holds a moving left stick, sent through the
network gamepad; the core logs every frame's RAM CRC and the second
port's stick. Passes when:

  - every frame both peers ran has the same CRC (last run wins, as a
    rollback replays frames)
  - with Sync Analog Sticks on, the host's core sees the client's stick;
    off, it sees the stick at rest
  - the core is only ever given a joypad device, never netplay's wire id
  - a lockstep peer never re-runs a frame, but after a resync: lockstep
    asked for, lockstep for a state too large to roll back, and a
    lockstep host with a rollback client
  - a client whose RAM is upset is caught at a check frame and given the
    host's state: frames differ for a while, then never again; in
    lockstep, also when the client lags the host and the state arrives
    labeled with a frame it has not reached; in rollback, with input
    latency, so some checks wait for the client to reach their frame
  - a core whose states never hash alike (a count in them that differs
    between peers) is resynced once at most past the join, then its
    checks are turned off

Usage: netplay_e2e.py /path/to/retroarch /path/to/netplay_e2e_libretro.so
"""

import math
import os
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import time

RUN_S     = 6.0
START_S   = 20.0
MIN_SHARE = 300
# Frames after the join left out: a host sending a state jumps its run
# frame to its input frame, so around a second state (achievements ask
# for one) a core's own frame count runs one off between the peers
SETTLE    = 64


def free_port(kind):
    s = socket.socket(socket.AF_INET, kind)
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


def write_config(d, name, extra):
    path = os.path.join(d, name + ".cfg")
    rows = {
        "video_driver": "null", "audio_driver": "null",
        "input_driver": "null", "input_joypad_driver": "null",
        "menu_driver": "null", "pause_nonactive": "false",
        "config_save_on_exit": "false", "netplay_nickname": name,
        "netplay_check_frames": "30", "netplay_start_as_spectator": "false",
        "netplay_public_announce": "false", "netplay_use_mitm_server": "false",
        "core_info_savestate_bypass": "true",
        "savestate_directory": d, "savefile_directory": d,
        "system_directory": d,
    }
    rows.update(extra)
    with open(path, "w") as f:
        for k, v in rows.items():
            f.write('%s = "%s"\n' % (k, v))
    return path


def read_log(path):
    """frame -> (x, y, crc), the last run of each; the device ids given;
    the frames in the order they ran, from the first state loaded if
    one was"""
    frames, devices, order = {}, [], []
    loaded = False
    if not os.path.exists(path):
        return frames, devices, order
    with open(path) as f:
        for line in f:
            p = line.split()
            if len(p) == 5 and p[0] == "frame":
                n = int(p[1])
                frames[n] = (int(p[2]), int(p[3]), p[4])
                order.append(n)
            elif len(p) == 3 and p[0] == "device":
                devices.append(int(p[2]))
            elif len(p) == 2 and p[0] == "loaded" and not loaded:
                # what ran before the join's state is not the session's
                loaded = True
                frames.clear()
                del order[:]
    return frames, devices, order


def joined_from(order):
    """The first frame compared: the client's first with the host's
    state, and the join settled"""
    return min(order) + SETTLE if order else 0


def replays(order, start):
    """Frames run again after @start: a rollback"""
    n, top = 0, start - 1
    for f in order:
        if f < start:
            continue
        if f <= top:
            n += 1
        top = max(top, f)
    return n


def run(retroarch, core, analog, label, host_extra=None, client_extra=None,
        pad=0, lockstep=(False, False), desync=0, client_pace=4, junk=False):
    d     = tempfile.mkdtemp(prefix="netplay_e2e_")
    nport = free_port(socket.SOCK_STREAM)
    rport = free_port(socket.SOCK_DGRAM)
    fails = []
    procs = []
    try:
        flag = "true" if analog else "false"
        host_cfg = write_config(d, "host",
                                dict(host_extra or {}, netplay_analog_joypads=flag))
        client_cfg = write_config(d, "client", dict(client_extra or {}, **{
            "network_remote_enable": "true",
            "network_remote_enable_user_p1": "true",
            "network_remote_base_port": str(rport)}))
        env = dict(os.environ, NETPLAY_E2E_PAD=str(pad),
                   NETPLAY_E2E_JUNK="1" if junk else "0")
        host_log, client_log = (os.path.join(d, n + ".log")
                                for n in ("host", "client"))

        procs.append(subprocess.Popen(
            [retroarch, "-v", "-c", host_cfg, "-L", core,
             "--host", "--port", str(nport)],
            env=dict(env, NETPLAY_E2E_LOG=host_log),
            stdout=open(os.path.join(d, "host.txt"), "w"),
            stderr=subprocess.STDOUT))
        deadline = time.time() + START_S
        while time.time() < deadline and not (
                os.path.exists(host_log) and os.path.getsize(host_log) > 0):
            time.sleep(0.1)

        procs.append(subprocess.Popen(
            [retroarch, "-v", "-c", client_cfg, "-L", core,
             "--connect", "127.0.0.1", "--port", str(nport)],
            env=dict(env, NETPLAY_E2E_LOG=client_log,
                     NETPLAY_E2E_DESYNC=str(desync),
                     NETPLAY_E2E_PACE_MS=str(client_pace)),
            stdout=open(os.path.join(d, "client.txt"), "w"),
            stderr=subprocess.STDOUT))

        # the stick: x swings, y holds
        pad_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        t0 = time.time()
        joined = None
        while time.time() - t0 < START_S + RUN_S:
            t = time.time() - t0
            x = int(20000 * math.sin(t * 3.0)) or 1
            for idx_id, v in ((0, x), (1, -12345)):
                msg = struct.pack("<iiiiH2x", 0, 5, 0, idx_id, v & 0xffff)
                pad_sock.sendto(msg, ("127.0.0.1", rport))
            if joined is None and os.path.exists(client_log) \
                    and os.path.getsize(client_log) > 4096:
                joined = time.time()
            if joined is not None and time.time() - joined > RUN_S:
                break
            if any(p.poll() is not None for p in procs):
                break
            time.sleep(0.02)
        pad_sock.close()

        for p in procs:
            if p.poll() is not None:
                fails.append("%s: a retroarch quit early (%d)" % (label, p.returncode))
        for p in procs:
            if p.poll() is None:
                p.terminate()
        for p in procs:
            try:
                p.wait(timeout=10)
            except subprocess.TimeoutExpired:
                p.kill()

        host, host_dev, horder = read_log(host_log)
        client, client_dev, corder = read_log(client_log)
        start  = joined_from(corder)
        # the newest frames may not have been confirmed and replayed yet
        last   = min(max(host or [0]), max(client or [0])) - 60
        shared = [n for n in sorted(client) if start <= n <= last and n in host]
        moved  = sum(1 for n in shared if host[n][0] != 0)
        differ = [n for n in shared if host[n][2] != client[n][2]]
        print("[info] %s: %d frames shared from %d, stick moved on the host in"
              " %d, %d differ" % (label, len(shared), start, moved, len(differ)))

        for who, order, want in (("host", horder, lockstep[0]),
                                 ("client", corder, lockstep[1])):
            again = replays(order, start)
            # a resync's state is loaded where the host labeled it, and
            # run on from there
            if want and again and not desync:
                fails.append("%s: the lockstep %s re-ran %d frames" % (label, who, again))
        if len(shared) < MIN_SHARE:
            fails.append("%s: only %d frames ran on both" % (label, len(shared)))
        if desync:
            late = [n for n in differ if n > shared[len(shared) // 2]] if shared else []
            if not differ:
                fails.append("%s: the upset RAM never showed" % label)
            elif late:
                fails.append("%s: still differing at frame %d, not mended" % (label, late[0]))
        elif differ:
            fails.append("%s: %d frames differ, the first %d" % (label, len(differ), differ[0]))
        if analog and moved < len(shared) // 2:
            fails.append("%s: the host saw the client's stick on %d of %d frames"
                         % (label, moved, len(shared)))
        if not analog and moved:
            fails.append("%s: the stick crossed with sync off (%d frames)" % (label, moved))
        if junk:
            with open(os.path.join(d, "client.txt"), errors="replace") as f:
                text  = f.read()
            loads = text.count("Loaded the host's state")
            if loads > 2:
                fails.append("%s: the client loaded the host's state %d times"
                             % (label, loads))
            if "desync checks are off" not in text:
                fails.append("%s: the CRC checks were never turned off" % label)
        for dev in host_dev + client_dev:
            if dev & ~0xff00 not in (0, 1):
                fails.append("%s: the core was given device 0x%x" % (label, dev))
                break
        if fails:
            for n in ("host.txt", "client.txt"):
                with open(os.path.join(d, n), errors="replace") as f:
                    lines = [l.rstrip() for l in f if "[Netplay]" in l or "rror" in l]
                print("  %s:" % n)
                for l in lines[-25:]:
                    print("    " + l)
    finally:
        for p in procs:
            if p.poll() is None:
                p.kill()
        shutil.rmtree(d, ignore_errors=True)
    return fails


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    retroarch, core = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    lock   = {"netplay_lockstep": "true"}
    fails  = run(retroarch, core, True, "sync analog sticks on")
    fails += run(retroarch, core, False, "sync analog sticks off")
    fails += run(retroarch, core, True, "lockstep", lock, lock,
                 lockstep=(True, True))
    fails += run(retroarch, core, True, "lockstep host, rollback client",
                 lock, None, lockstep=(True, False))
    fails += run(retroarch, core, True, "lockstep, a desync mended", lock, lock,
                 lockstep=(True, True), desync=200)
    # a client slower than the host is behind the frame the host's state
    # is labeled with when it arrives
    fails += run(retroarch, core, True, "lockstep, a desync mended, slow client",
                 lock, lock, lockstep=(True, True), desync=200, client_pace=12)
    # the host's input latency makes the client lag the frames the host
    # confirms, so some CRCs arrive before the client has run their frame
    fails += run(retroarch, core, True, "rollback, a desync mended",
                 {"netplay_input_latency_frames_min": "8"}, None, desync=200)
    fails += run(retroarch, core, True, "states that never hash alike",
                 junk=True)
    # past the size rollback keeps one of per frame: lockstep unasked
    fails += run(retroarch, core, True, "a 40 MB state", pad=40 << 20,
                 lockstep=(True, True))
    for f in fails:
        print("[FAIL] " + f)
    print("%s netplay_e2e" % ("FAIL" if fails else "PASS"))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
