#!/usr/bin/env python3
"""Compare the fixed setup with the preserved non-fused historical reference.
The reference is test-only; normal filter builds contain no floating setup.
Also require the portable limb arithmetic to produce identical images.
"""
import hashlib
import math
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent

def main():
    cc = shlex.split(os.environ.get("CC", "cc"))
    runner = shlex.split(os.environ.get("RUNNER", ""))
    flags = shlex.split(os.environ.get("COMPARE_CFLAGS", "-O2"))
    with tempfile.TemporaryDirectory(prefix="snes-fixed-") as tmp:
        images = []
        for name, define in (("reference", "SNES_NTSC_REFERENCE_FLOAT"),
                             ("fixed", None), ("portable", "SNES_NTSC_FIXED_PORTABLE")):
            exe = str(Path(tmp) / (name + ".exe"))
            cmd = cc + (flags if name != "reference" else ["-O2"]) + ["-I" + str(HERE / "../../../libretro-common/include"),
                    "-ffp-contract=off", str(HERE / "snes_fixed_compare.c"), "-o", exe, "-lm"]
            if name != "reference":
                cmd += ["-ffp-contract=fast"]
            if define:
                cmd += ["-D" + define]
            subprocess.run(cmd, check=True)
            images.append(subprocess.check_output(runner + [exe]))
        reference, fixed, portable = images
        assert fixed == portable, "Portable and native fixed arithmetic differ"
        assert len(reference) == len(fixed) and len(fixed) % 2 == 0
        digest = hashlib.sha256(fixed).hexdigest()
        print("Fixed image SHA256:", digest)
        assert digest == "1724e2caa994bf1735311545e83a637c7d683f8aa497b14647d7cd113c043789", "Fixed baseline changed"
        presets = ["composite", "RF", "RGB", "S-video", "monochrome"]
        controls = ["hue", "saturation", "contrast", "brightness", "sharpness",
                    "gamma", "resolution", "artifacts", "fringing", "bleed"]
        presets += [f"{key} {sign}" for key in controls for sign in ("-1", "+1")]
        presets += ["custom decoder -", "custom decoder +", "near-unit rolloff"]
        size = len(fixed) // len(presets)
        for test, label in enumerate(presets):
            changed = total = square = maximum = 0
            for pos in range(test*size, (test+1)*size, 2):
                a = int.from_bytes(reference[pos:pos+2], "little")
                b = int.from_bytes(fixed[pos:pos+2], "little")
                changed += a != b
                for shift, mask in ((11,31), (5,63), (0,31)):
                    delta = abs(((a >> shift)&mask) - ((b >> shift)&mask)) * 255 / mask
                    total += delta
                    square += delta*delta
                    maximum = max(maximum, delta)
            channels = size//2*3
            psnr = 10*math.log10(255*255*channels/square) if square else float("inf")
            print(f"{label:15} changed={100*changed/(size//2):6.2f}% "
                  f"MAE8={total/channels:.6f} max8={maximum:.3f} PSNR={psnr:.3f}dB")
            # Catch gross conversion errors; new hashes cover exact output.
            assert total/channels < 0.1 and maximum <= 255/31 + 0.00001, f"Excessive difference: {label}"

if __name__ == "__main__":
    main()
