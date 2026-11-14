#!/usr/bin/env python3
"""Exercise managed states through an actual RetroArch executable."""
import argparse
import collections
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import time


CASES = {
    "release_once": ("single", "release_once", False),
    "failed_save": ("single", "failed_save", False),
    "halt": ("single", "halt", False),
    "registration_after_init": ("single", "late_registration", False),
    "secondary_disabled": ("single", "release_once", True),
    "secondary_control": ("single", "unregistered", True),
    "preempt_deinit": ("preemptive", "release_once", False),
    "preempt_save_error": ("preemptive", "preempt_save_error", False),
    "preempt_load_error": ("preemptive", "preempt_load_error", False),
    "options_pending": ("preemptive", "options_pending", False),
    "reset_history": ("preemptive", "reset", False),
    "old_version": ("single", "old_version", False),
}


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def read_events(path):
    if not path.exists():
        return []
    # A reader may see a partial final line while the core flushes its trace.
    return [line.split() for line in path.read_text().splitlines()
            if len(line.split()) == 5]


def movie(path):
    header = struct.pack("<IIIIQ", 0x42535632, 1, 0, 0, 0x20261031) + bytes(16)
    frames = [struct.pack("<BHBBBBHhBBBBHh", 0, 2, 0, 1, 0, 0, 256,
                          (i // 6) % 2, 0, 1, 0, 0, 0, (i // 6) % 2) + b"f"
              for i in range(180)]
    path.write_bytes(header + b"".join(frames))


def check_trace(name, rows, replies, log):
    names = [row[0] for row in rows]
    require(rows and names[-1] == "DEINIT", "missing clean core shutdown")
    require("FAIL" not in names, "reference core rejected a frontend operation")
    live = set()
    saved = collections.Counter()
    released = collections.Counter()
    for event, instance, value, frame, phase in rows:
        key = (instance, value)
        if event == "SAVE":
            require(key not in live, "duplicate owner")
            live.add(key)
            saved[key] += 1
        elif event == "LOAD":
            require(key in live, "load of a released or foreign state")
        elif event == "RELEASE":
            require(key in live, "release of a missing or previously released state")
            live.remove(key)
            released[key] += 1
        elif event in ("UNLOAD", "DEINIT"):
            require(not any(owner == instance for owner, _ in live),
                    "state outlived its core")
    require(not live and saved == released, "successful saves were not released exactly once")
    if name in ("old_version", "registration_after_init", "secondary_control"):
        require(not saved, "unsupported registration produced managed states")
    else:
        require(len(saved) >= 3, "managed saves did not execute")
    if name == "failed_save":
        require("REFUSE_SAVE" in names, "save refusal was not reached")
    if name == "halt":
        require("UNSAFE" in names, "unsafe transition was not reached")
        tail = names[names.index("UNSAFE") + 1:]
        require(all(e in ("RELEASE", "UNLOAD", "DEINIT") for e in tail),
                "core called after becoming unsafe")
        require("Core state could not be recovered. Close and reload content." in log,
                "missing recovery message")
        require(any("core memory unavailable" in reply for reply in replies),
                "blocked memory read was not observed")
        require(any(reply.startswith("READ_CORE_MEMORY ") and "-1" not in reply
                    for reply in replies), "healthy memory descriptor was not exercised")
        require(any(reply.startswith("WRITE_CORE_MEMORY ") and "unavailable" in reply
                    for reply in replies), "cached memory write was not blocked")
        require(any("SAVE_STATE_SLOT ERROR" in reply for reply in replies),
                "blocked persistence was not observed")
        require(any("GET_STATUS PAUSED" in reply for reply in replies),
                "unpause did not return to the stopped state")
    if name == "registration_after_init":
        require("LATE_REJECTED" in names, "late registration was not attempted")
    if name in ("secondary_disabled", "secondary_control"):
        expected = 1 if name == "secondary_disabled" else 2
        require(len({row[1] for row in rows if row[0] == "INIT"}) == expected,
                "wrong number of core instances")
    if name.startswith("preempt_"):
        require("LOAD" in names, "Preemptive Frames replay was not reached")
    if name in ("preempt_save_error", "preempt_load_error"):
        marker = "REFUSE_SAVE" if name == "preempt_save_error" else "REFUSE_LOAD"
        require(marker in names, "preempt error path was not reached")
        tail = rows[names.index(marker) + 1:]
        require(sum(row[0] == "RUN" for row in tail) >= 30,
                "ordinary execution did not continue after refusal")
        require(not any(row[0] == "SAVE" for row in tail), "prediction stayed active")
        last_release = max(i for i, row in enumerate(tail) if row[0] == "RELEASE")
        require(any(row[0] == "RUN" for row in tail[last_release + 1:]),
                "owned states were retained until shutdown instead of error cleanup")
        if name == "preempt_save_error":
            canonical = [int(r[3]) for r in rows if r[0] == "RUN" and r[4] != "2"]
            require(canonical == list(range(1, len(canonical) + 1)),
                    "save refusal skipped or repeated a canonical frame")
    if name == "options_pending":
        require("OPTION" in names and "OPTIONS_READ" in names, "option transition not reached")
        start = names.index("OPTION")
        read = names.index("OPTIONS_READ", start)
        require(read - start >= 20, "option was not left pending")
        require("LOAD" not in names[start + 1:read], "history reused while options pending")
        require("LOAD" in names[read + 1:], "replay did not resume after consuming options")
        next_save = names.index("SAVE", start)
        require(names[start + 1:next_save].count("RELEASE") >= 3,
                "pending option did not release populated history")
    if name == "reset_history":
        require("RESET" in names, "reset was not reached")
    return {"saves": sum(saved.values()), "releases": sum(released.values()),
            "runs": names.count("RUN"), "loads": names.count("LOAD")}


def run_case(args, name, root):
    mode, fault, secondary = CASES[name]
    folder = root / name
    folder.mkdir()
    trace = folder / "events.txt"
    content = folder / "test.lifetime"
    content.write_text("savestate lifetime\n")
    replay = folder / "input.bsv"
    movie(replay)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]
    settings = {
        "config_save_on_exit": False, "global_core_options": True,
        "video_driver": "null", "audio_driver": "null", "input_driver": "null",
        "input_joypad_driver": "null", "video_vsync": False, "audio_enable": False,
        "audio_sync": False, "video_threaded": False, "pause_nonactive": False,
        "history_list_enable": False, "auto_overrides_enable": False,
        "menu_enable_widgets": False, "core_info_savestate_bypass": True,
        "savestate_auto_save": False, "savestate_auto_load": False,
        "run_ahead_enabled": mode == "single", "run_ahead_frames": 3,
        "run_ahead_secondary_instance": secondary,
        "preemptive_frames_enable": mode == "preemptive",
        "network_cmd_enable": True, "network_cmd_port": port,
        "network_cmd_bind_address": "127.0.0.1", "savestate_file_compression": False,
        "core_options_path": (folder / "options.cfg").as_posix(), "user_language": 0,
    }
    for key in ("cache_directory", "savefile_directory", "savestate_directory",
                "playlist_directory", "system_directory", "assets_directory",
                "runtime_log_directory", "rgui_config_directory"):
        settings[key] = folder.as_posix()
    config = folder / "retroarch.cfg"
    config.write_text("\n".join(f'{key} = "{str(value).lower() if isinstance(value, bool) else value}"'
                                 for key, value in settings.items()) + "\n")
    environment = dict(os.environ, LIFETIME_TEST_TRACE=str(trace), LIFETIME_TEST_CASE=fault)
    replies = []
    unsafe_at = None
    last_unpause = 0
    status_asked = reset_sent = stopping = memory_primed = False
    log_path = folder / "frontend.log"
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as control, log_path.open("wb") as log:
        control.settimeout(0.001)

        def send(command):
            control.sendto(command.encode(), ("127.0.0.1", port))

        process = subprocess.Popen([str(args.frontend), "-v", "-c", str(config), "-L",
                                    str(args.core), "--play-replay", str(replay),
                                    "--max-frames=110", str(content)], cwd=folder,
                                   env=environment, stdout=log, stderr=subprocess.STDOUT)
        start = time.monotonic()
        try:
            while process.poll() is None:
                rows = read_events(trace)
                now = time.monotonic()
                names = [row[0] for row in rows]
                if name == "halt" and not memory_primed and names.count("RUN") >= 15:
                    send("READ_CORE_MEMORY 0 4")
                    memory_primed = True
                if name == "reset_history" and not reset_sent and names.count("RUN") >= 45:
                    send("RESET off")
                    reset_sent = True
                if "UNSAFE" in names:
                    if unsafe_at is None:
                        unsafe_at = now
                        for command in ("READ_CORE_MEMORY 0 4", "WRITE_CORE_MEMORY 0 00",
                                        "SAVE_STATE_SLOT 2", "RESET off"):
                            send(command)
                    if now - last_unpause > 0.16:
                        send("SET_OPTION pause off")
                        last_unpause = now
                        status_asked = False
                    elif not status_asked and now - last_unpause > 0.08:
                        send("GET_STATUS")
                        status_asked = True
                canonical = sum(row[0] == "RUN" and row[4] != "2" for row in rows)
                if not stopping and ((unsafe_at is not None and now - unsafe_at > 0.6)
                                     or canonical >= 100):
                    send("QUIT")
                    stopping = True
                require(now - start < 30, "frontend timed out")
                try:
                    replies.append(control.recv(8192).decode())
                except (socket.timeout, ConnectionResetError):
                    pass
                time.sleep(0.005)
            require(process.returncode == 0, f"frontend exited with {process.returncode}")
        finally:
            if process.poll() is None:
                send("QUIT")
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
    (folder / "replies.json").write_text(json.dumps(replies, indent=2))
    return check_trace(name, read_events(trace), replies, log_path.read_text(errors="replace"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--frontend", type=Path, required=True)
    parser.add_argument("--core", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--case", action="append", choices=CASES)
    args = parser.parse_args()
    args.frontend = args.frontend.resolve(strict=True)
    args.core = args.core.resolve(strict=True)
    if args.output:
        args.output.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="lifetime-", dir=args.output)).resolve()
    results = {}
    try:
        for name in args.case or CASES:
            results[name] = run_case(args, name, root)
            print("PASS", name, flush=True)
            (root / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    except Exception:
        print("FAIL: evidence retained at", root, flush=True)
        raise
    print(f"lifetime integration: {len(results)} passed; evidence: {root}")


if __name__ == "__main__":
    main()
