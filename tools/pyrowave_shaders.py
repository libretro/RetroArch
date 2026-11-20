#!/usr/bin/env python3
"""Build C89 shader assets from the independently authored PyroWave sources."""
import argparse
import hashlib
from pathlib import Path
import struct
import subprocess
import tempfile
from spirv_optimize import optimize
from glsl_minify import minify


def run(command):
    subprocess.run([str(part) for part in command], check=True)


def array(name, kind, values, columns):
    lines = ["static const %s %s[] = {" % (kind, name)]
    for offset in range(0, len(values), columns):
        lines.append("   " + ", ".join(values[offset:offset + columns]) + ",")
    lines.append("};")
    return "\n".join(lines)


def text_array(name, data):
    if any(value > 127 for value in data) or b"\0" in data:
        raise ValueError("Shader text must be ASCII without embedded NULs")
    return array(name, "char", ["0x%02x" % value for value in data + b"\0"], 16)


def write_changed(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.read_bytes() == data:
        return
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as temporary:
        temporary_path = Path(temporary.name)
        temporary.write(data)
    try:
        temporary_path.replace(path)
    finally:
        temporary_path.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--export-glsl", type=Path,
                        help="Also write desktop .comp and .es.comp files for native tests")
    parser.add_argument("--minify-glsl-exports", action="store_true",
                        help="Also minify exported GLSL files; embedded arrays are always minified")
    parser.add_argument("--export-hlsl", type=Path, help="Also write native Direct3D compute sources")
    parser.add_argument("--export-msl", type=Path, help="Also write Metal 2.1 compute sources")
    parser.add_argument("--export-spirv", type=Path, help="Also write validated Vulkan modules")
    parser.add_argument("--metal-sdk", choices=("macosx", "iphoneos", "appletvos", "iphonesimulator", "appletvsimulator"),
                        help="Validate Metal sources with xcrun and this Apple SDK")
    parser.add_argument("--glslang", default="glslangValidator")
    parser.add_argument("--spirv-cross", default="spirv-cross")
    parser.add_argument("--spirv-opt", default="spirv-opt")
    parser.add_argument("--spirv-val", default="spirv-val")
    parser.add_argument("--check", action="store_true", help="Verify an existing header without rewriting it")
    args = parser.parse_args()
    sources = Path(__file__).resolve().parents[1] / "libretro-common/formats/pyrowave/shaders"
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    groups = {"HLSL": [], "SPIRV": [], "GLSL": [], "GLES": [], "METAL": []}
    exports = []
    metal_exports = []
    spirv_exports = []
    hashes = []
    with tempfile.TemporaryDirectory(prefix="pyrowave-shaders-", dir=output.parent) as temporary:
        directory = Path(temporary)
        for name in ("color", "dwt", "packet"):
            source = sources / (name + ".hlsl")
            source_bytes = source.read_bytes().replace(b"\r\n", b"\n")
            hashes.append(" * %s.hlsl SHA-256 %s" % (name, hashlib.sha256(source_bytes).hexdigest()))
            spirv = directory / (name + ".spv")
            run([args.glslang, "-D", "-V", "--hlsl-iomap", "--shift-uav-binding", "comp",
                 "1" if name == "dwt" else "2", "--shift-UBO-binding", "comp",
                 "2" if name == "dwt" else "4", "-S", "comp", "-e", "main", "-o", spirv, source])
            run([args.spirv_val, "--target-env", "vulkan1.0", spirv])
            # Only the Vulkan module is optimized/stripped. Cross-compilation
            # below still uses the original named IR for GLSL and Metal.
            data = optimize(spirv.read_bytes(), args.spirv_opt, args.spirv_val, True)
            if len(data) % 4 or data[:4] != b"\x03\x02\x23\x07":
                raise ValueError("Compiler did not emit a SPIR-V module")
            words = struct.unpack("<%dI" % (len(data) // 4), data)
            spirv_exports.append((name + ".spv", data))
            groups["SPIRV"].append(array("rpyrowave_shader_" + name + "_spirv", "uint32_t",
                                         ["0x%08xU" % word for word in words], 8))
            groups["HLSL"].append(text_array("rpyrowave_shader_" + name + "_hlsl", source_bytes))
            metal = directory / (name + ".metal")
            run([args.spirv_cross, spirv, "--msl", "--msl-version", "20100",
                 "--msl-decoration-binding", "--rename-entry-point", "main", "rpyrowave_main", "comp",
                 "--output", metal])
            if args.metal_sdk:
                run(["xcrun", "--sdk", args.metal_sdk, "metal",
                     "-std=" + ("macos-metal2.1" if args.metal_sdk == "macosx" else "ios-metal2.1"),
                     "-c", metal,
                     "-o", directory / (name + ".air")])
            data = metal.read_bytes().replace(b"\r\n", b"\n")
            groups["METAL"].append(text_array("rpyrowave_shader_" + name + "_metal", data))
            metal_exports.append((name + ".metal", data))
            for language, version, suffix in (("GLSL", "430", ".comp"), ("GLES", "310", ".es.comp")):
                glsl = directory / (name + suffix)
                command = [args.spirv_cross, spirv, "--version", version,
                           "--combined-samplers-inherit-bindings", "--output", glsl]
                if language == "GLES":
                    command.append("--es")
                run(command)
                run([args.glslang, "-S", "comp", glsl])
                data = glsl.read_bytes().replace(b"\r\n", b"\n")
                groups[language].append(text_array("rpyrowave_shader_" + name + "_" + language.lower(),
                                                   minify(data.decode("ascii")).encode("ascii")))
                exports.append((name + suffix, data))
    header = ["/* Generated by tools/pyrowave_shaders.py. Do not commit build output.", *hashes, " */",
              "#ifndef RPYROWAVE_GENERATED_SHADER_ASSETS_H", "#define RPYROWAVE_GENERATED_SHADER_ASSETS_H",
              "#include <stdint.h>"]
    for language, arrays in groups.items():
        header.extend(["#ifdef RPYROWAVE_SHADER_" + language, *arrays, "#endif"])
    header.append("#endif\n")
    data = "\n".join(header).encode("ascii")
    if args.check:
        if not output.exists() or output.read_bytes() != data:
            raise SystemExit("PyroWave shader assets differ: " + str(output))
    else:
        write_changed(output, data)
        if args.export_hlsl:
            for name in ("color", "dwt", "packet"):
                write_changed(args.export_hlsl / (name + ".hlsl"), (sources / (name + ".hlsl")).read_bytes().replace(b"\r\n", b"\n"))
        if args.export_glsl:
            for name, data in exports:
                write_changed(args.export_glsl / name,
                              minify(data.decode("ascii")).encode("ascii") if args.minify_glsl_exports else data)
        if args.export_msl:
            for name, data in metal_exports:
                write_changed(args.export_msl / name, data)
        if args.export_spirv:
            for name, data in spirv_exports:
                write_changed(args.export_spirv / name, data)
    print("PyroWave shader assets validated: " + str(output))


if __name__ == "__main__":
    main()
