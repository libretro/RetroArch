#!/usr/bin/env python3
"""Run actual native decoder/import/fallback/replacement presentation checks."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess

here = Path(__file__).resolve().parent
p = argparse.ArgumentParser()
p.add_argument("--harness", type=Path, default=here / "movie_recovery_test.exe")
p.add_argument("--fixtures", type=Path, required=True)
p.add_argument("--output", type=Path, required=True)
p.add_argument("--drivers", default="d3d11,d3d12")
p.add_argument("--threaded", action="store_true")
p.add_argument("--window-mode", choices=("steady", "resize", "fullscreen"), default="steady")
a = p.parse_args()
if os.name != "nt":
    p.error("native Windows hardware is required")
fixtures = a.fixtures.resolve()
reference = (fixtures / "reference.rgb").read_bytes()
stride = 256 * 192 * 3
assert len(reference) == 12 * stride
manifest = json.loads((fixtures / "fixture.json").read_text())
movie_file = manifest.get("movie_file", "hevc-aac.mp4")
for name, key in [(movie_file, "movie_sha256"), ("reference.rgb", "reference_sha256")]:
    if hashlib.sha256((fixtures / name).read_bytes()).hexdigest() != manifest[key]:
        raise RuntimeError("fixture hash mismatch: " + name)
output = a.output.resolve()
output.mkdir(parents=True, exist_ok=True)
results = []
flat_points = {}
for driver in a.drivers.split(","):
    if driver not in ("d3d11", "d3d12", "vulkan"):
        p.error("supported native acceptance drivers: d3d11,d3d12,vulkan")
    directory = output / driver
    directory.mkdir()
    cmd = [os.environ.get("GDB", "gdb"), "--batch", "-return-child-result", "-ex", "run",
           "-ex", "thread apply all bt", "--args", str(a.harness.resolve()),
           str(fixtures / movie_file), driver, str(directory), "true" if a.threaded else "false", a.window_mode]
    if Path(movie_file).suffix.lower() == ".webm":
        cmd.append("continuous")
    with (directory / "runtime.log").open("wb") as log:
        child = subprocess.Popen(cmd, stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
                                 creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            code = child.wait(timeout=90)
        except subprocess.TimeoutExpired:
            subprocess.run(["taskkill", "/PID", str(child.pid), "/T", "/F"], check=False,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            child.wait()
            raise RuntimeError("native probe timed out: " + driver)
    if code:
        raise RuntimeError("native probe failed: " + str(directory / "runtime.log"))
    runtime = (directory / "runtime.log").read_text(errors="replace")
    if "exited normally" not in runtime:
        raise RuntimeError("GDB did not report a normal exit: " + driver)
    if a.window_mode == "fullscreen" and runtime.count("WINDOW_FULLSCREEN ") != 5:
        raise RuntimeError("fullscreen monitor-rectangle checks missing: " + driver)
    restart_hold = "RECOVERY_HOLD tick=9" in runtime
    frames = []
    for index in range(12):
        raw = (directory / ("frame-%02d.ppm" % index)).read_bytes()
        header, dimensions, limit, actual = raw.split(b"\n", 3)
        width, height = map(int, dimensions.split())
        assert header == b"P6" and limit == b"255" and len(actual) == width * height * 3
        expected_dims = (256, 192)
        if a.window_mode == "resize" and 2 <= index < 7:
            expected_dims = (512, 384)
        elif a.window_mode == "fullscreen" and 2 <= index < 7:
            expected_dims = None  # HWND/monitor rectangle asserted by the harness.
        elif a.window_mode != "steady" and index in (7, 8):
            expected_dims = (320, 240)
        if expected_dims is not None and (width, height) != expected_dims:
            raise RuntimeError("unexpected viewport at frame %d: %s" % (index, (width, height)))
        # The rejected picture repeats the preceding front; the next ordinal
        # must then recover without searching or shifting the reference.
        # Only the explicitly reported replacement stall repeats ordinal 8;
        # the next tick must catch up to ordinal 10.
        ordinal = 3 if index == 4 else (8 if index == 9 and restart_hold else index)
        expected = reference[ordinal * stride:(ordinal + 1) * stride]
        if ordinal not in flat_points:
            flat_points[ordinal] = [(x, y) for y in range(3, 189, 2)
                for x in range(3, 253, 2)
                if all(max(expected[((y+dy)*256+x+dx)*3+c]
                           for dy in range(-2, 3) for dx in range(-2, 3)) -
                       min(expected[((y+dy)*256+x+dx)*3+c]
                           for dy in range(-2, 3) for dx in range(-2, 3)) <= 2
                       for c in range(3))]
        errors = []
        for x, y in flat_points[ordinal]:
            ax = min(width-1, int((x+.5)*width/256))
            ay = min(height-1, int((y+.5)*height/192))
            ap, ep = (ay*width+ax)*3, (y*256+x)*3
            errors.append(max(abs(actual[ap+c]-expected[ep+c]) for c in range(3)))
        assert len(errors) >= 20
        frames.append({"frame": index, "reference_ordinal": ordinal, "width": width, "height": height, "points": len(errors),
                       "max_error": max(errors), "failed_points": sum(v > 3 for v in errors)})
    result = {"driver": driver, "frames": frames, "gdb_exit": code, "restart_hold": restart_hold, "threaded": a.threaded, "window_mode": a.window_mode}
    results.append(result)
    (output / "result.json").write_text(json.dumps(results, indent=2))
    if any(frame["failed_points"] for frame in frames):
        raise RuntimeError("movie recovery pixels differ from the fixed reference: " + driver)
    print("[pass] actual coded movie import, refusal, CPU fallback and driver recovery: " + driver)
