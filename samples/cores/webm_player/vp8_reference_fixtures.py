"""Generated VP8 reference pictures; no personal media fixtures."""
from pathlib import Path
import subprocess
import sys
import tempfile

out = Path(sys.argv[1] if len(sys.argv) > 1 else "fixtures")
out.mkdir(parents=True, exist_ok=True)
for name, geometry, alternate in [("vp8-refs", "320x180", "0"),
                                  ("vp8-altref", "318x242", "1")]:
    encoded = out / (name + ".ivf")
    command = ["ffmpeg", "-v", "error", "-y", "-f", "lavfi", "-i",
               "testsrc2=size=" + geometry + ":rate=30", "-t", "3",
               "-c:v", "libvpx", "-b:v", "300k", "-g", "30",
               "-lag-in-frames", "25", "-auto-alt-ref", alternate]
    with tempfile.TemporaryDirectory(prefix="vp8-reference-") as temporary:
        if alternate == "1":
            command += ["-passlogfile", str(Path(temporary) / "stats")]
            subprocess.run(command + ["-pass", "1", "-f", "null", "-"], check=True)
            command += ["-pass", "2"]
        subprocess.run(command + [str(encoded)], check=True)
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(encoded),
                    "-vsync", "0", "-pix_fmt", "yuv420p", "-f", "rawvideo",
                    str(out / (name + ".yuv"))], check=True)

first = (out / "vp8-refs.ivf").read_bytes()
subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(out / "vp8-altref.ivf"),
                "-c:v", "copy", str(out / "vp8-altref.webm")], check=True)
second = (out / "vp8-altref.ivf").read_bytes()
(out / "vp8-resize.ivf").write_bytes(first + second[32:] + first[32:])
with (out / "vp8-resize.yuv").open("wb") as reference:
    for name in ["vp8-refs", "vp8-altref", "vp8-refs"]:
        reference.write((out / (name + ".yuv")).read_bytes())
