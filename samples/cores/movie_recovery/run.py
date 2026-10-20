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
a = p.parse_args()
if os.name != "nt":
    p.error("native Windows hardware is required")
fixtures = a.fixtures.resolve()
reference = (fixtures / "reference.rgb").read_bytes()
stride = 256 * 192 * 3
assert len(reference) == 12 * stride
manifest = json.loads((fixtures / "fixture.json").read_text())
for name, key in [("hevc-aac.mp4", "movie_sha256"), ("reference.rgb", "reference_sha256")]:
    if hashlib.sha256((fixtures / name).read_bytes()).hexdigest() != manifest[key]:
        raise RuntimeError("fixture hash mismatch: " + name)
output = a.output.resolve()
output.mkdir(parents=True, exist_ok=True)
results = []
for driver in a.drivers.split(","):
    if driver not in ("d3d11", "d3d12"):
        p.error("supported native acceptance drivers: d3d11,d3d12")
    directory = output / driver
    directory.mkdir()
    cmd = [os.environ.get("GDB", "gdb"), "--batch", "-return-child-result", "-ex", "run",
           "-ex", "thread apply all bt", "--args", str(a.harness.resolve()),
           str(fixtures / "hevc-aac.mp4"), driver, str(directory)]
    with (directory / "runtime.log").open("wb") as log:
        child = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT,
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
    frames = []
    for index in range(12):
        raw = (directory / ("frame-%02d.ppm" % index)).read_bytes()
        header, dimensions, limit, actual = raw.split(b"\n", 3)
        width, height = map(int, dimensions.split())
        assert header == b"P6" and limit == b"255" and len(actual) == width * height * 3
        # The rejected picture repeats the preceding front; the next ordinal
        # must then recover without searching or shifting the reference.
        ordinal = 3 if index == 4 else index
        expected = reference[ordinal * stride:(ordinal + 1) * stride]
        errors = []
        for y in range(3, 189, 2):
            for x in range(3, 253, 2):
                if any(max(expected[((y+dy)*256+x+dx)*3+c]
                           for dy in range(-2, 3) for dx in range(-2, 3)) -
                       min(expected[((y+dy)*256+x+dx)*3+c]
                           for dy in range(-2, 3) for dx in range(-2, 3)) > 2
                       for c in range(3)):
                    continue
                ax = min(width-1, int((x+.5)*width/256))
                ay = min(height-1, int((y+.5)*height/192))
                ap, ep = (ay*width+ax)*3, (y*256+x)*3
                errors.append(max(abs(actual[ap+c]-expected[ep+c]) for c in range(3)))
        assert len(errors) >= 20
        frames.append({"frame": index, "reference_ordinal": ordinal, "points": len(errors),
                       "max_error": max(errors), "failed_points": sum(v > 3 for v in errors)})
    result = {"driver": driver, "frames": frames, "gdb_exit": code}
    results.append(result)
    (output / "result.json").write_text(json.dumps(results, indent=2))
    if any(frame["failed_points"] for frame in frames):
        raise RuntimeError("movie recovery pixels differ from the fixed reference: " + driver)
    print("[pass] actual coded movie import, refusal, CPU fallback and driver recovery: " + driver)
