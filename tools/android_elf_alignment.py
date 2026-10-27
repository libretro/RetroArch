#!/usr/bin/env python3
"""Check Android 64-bit LOAD and RELRO alignment, including APK libraries."""
import argparse
from pathlib import Path
import struct
import sys
import zipfile

PAGE = 16384


def check(data):
    if len(data) < 64 or data[:6] != b"\x7fELF\x02\x01":
        raise ValueError("expected a little-endian ELF64 library")
    offset = struct.unpack_from("<Q", data, 32)[0]
    size, count = struct.unpack_from("<HH", data, 54)
    if size != 56 or not count or offset > len(data) or count > (len(data) - offset) // size:
        raise ValueError("invalid program header table")
    loads = 0
    for index in range(count):
        kind, _, start, address, _, stored, memory, alignment = struct.unpack_from(
            "<IIQQQQQQ", data, offset + index * size)
        if kind == 1:
            loads += 1
            if alignment < PAGE or alignment & (alignment - 1) or (address - start) % PAGE:
                raise ValueError("LOAD is not aligned to 16 KiB")
            if stored > memory or start > len(data) or stored > len(data) - start:
                raise ValueError("invalid LOAD extent")
        elif kind == 0x6474e552 and (address + memory) % PAGE:
            raise ValueError("RELRO does not end on a 16 KiB boundary")
    if not loads:
        raise ValueError("no LOAD segments")


def selftest():
    header = bytearray(64)
    header[:6] = b"\x7fELF\x02\x01"
    struct.pack_into("<Q", header, 32, 64)
    struct.pack_into("<HH", header, 54, 56, 2)
    load = [1, 4, 0, 0, 0, 176, 176, PAGE]
    relro = [0x6474e552, 4, 0, 176, 0, 0, PAGE - 176, 1]

    def image(a, b):
        return header + struct.pack("<IIQQQQQQ", *a) + struct.pack("<IIQQQQQQ", *b)

    check(image(load, relro))
    cases = [b"", image(load, relro)[:100]]
    for field, value in [(0, 0), (3, 4096), (5, 177), (6, 175), (7, 4096), (7, 24576)]:
        bad = load.copy()
        bad[field] = value
        cases.append(image(bad, relro))
    bad = relro.copy()
    bad[6] -= 1
    cases.append(image(load, bad))
    for data in cases:
        try:
            check(data)
        except ValueError:
            continue
        raise AssertionError("invalid ELF passed alignment check")
    print("Android ELF alignment selftest: ok")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("files", nargs="*", type=Path)
    args = parser.parse_args()
    if args.selftest:
        selftest()
    if not args.selftest and not args.files:
        parser.error("provide a library or APK")
    for path in args.files:
        try:
            if path.suffix == ".apk":
                with zipfile.ZipFile(path) as apk:
                    names = [name for name in apk.namelist() if name.endswith(".so")
                             and name.startswith(("lib/arm64-v8a/", "lib/x86_64/"))]
                    if not names:
                        raise ValueError("APK has no 64-bit libraries")
                    for name in names:
                        check(apk.read(name))
            else:
                check(path.read_bytes())
        except (OSError, ValueError, zipfile.BadZipFile) as error:
            print(f"FAIL {path}: {error}", file=sys.stderr)
            return 1
        print(f"PASS {path}: Android ELF 16 KiB alignment")
    return 0


if __name__ == "__main__":
    sys.exit(main())
