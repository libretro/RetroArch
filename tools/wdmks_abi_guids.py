#!/usr/bin/env python3
"""Compare the KS GUIDs in audio/drivers/wdmks.c against the system's.

The sizes, offsets and constants in that header are asserted against
ks.h at compile time by samples/audio/wdmks_abi. The GUIDs cannot be:
ks.h declares them through DEFINE_GUIDSTRUCT, which needs linkage, so
a translation unit that names one does not link without the DDK's
import library.

What it does give is the canonical string beside each definition, and
that is what this compares our literals against. Run with the include
directory holding ks.h and ksmedia.h:

    tools/wdmks_abi_guids.py /usr/x86_64-w64-mingw32/include

Exits non-zero on any mismatch, and on any GUID in our header it could
not find a counterpart for - a GUID that silently stopped being
checked would be worse than one that never was.
"""

import re
import sys
import os

# Ours -> the system's name. The two WAVE-format subtypes are built by
# a macro from a format tag rather than written out, so they are
# checked against that family instead.
PAIRS = {
    "ra_ks_category_audio":           "KSCATEGORY_AUDIO",
    "ra_ks_category_render":          "KSCATEGORY_RENDER",
    "ra_ks_category_capture":         "KSCATEGORY_CAPTURE",
    "ra_ks_category_realtime":        "KSCATEGORY_REALTIME",
    "ra_ks_propsetid_pin":            "KSPROPSETID_Pin",
    "ra_ks_interfacesetid_standard":  "KSINTERFACESETID_Standard",
    "ra_ks_mediumsetid_standard":     "KSMEDIUMSETID_Standard",
    "ra_ks_propsetid_connection":     "KSPROPSETID_Connection",
    "ra_ks_propsetid_audio":          "KSPROPSETID_Audio",
    "ra_ks_propsetid_rtaudio":        "KSPROPSETID_RtAudio",
    "ra_ks_dataformat_type_audio":    "KSDATAFORMAT_TYPE_AUDIO",
    "ra_ks_dataformat_subtype_pcm":   "KSDATAFORMAT_SUBTYPE_PCM",
    "ra_ks_dataformat_specifier_wfx": "KSDATAFORMAT_SPECIFIER_WAVEFORMATEX",
}

# The WAVE format family: xxxxxxxx-0000-0010-8000-00aa00389b71.
WAVE_FAMILY = {
    "ra_ks_dataformat_subtype_float": 0x0003,
}


def system_guids(incdir):
    out = {}
    for name in ("ks.h", "ksmedia.h"):
        path = os.path.join(incdir, name)
        if not os.path.exists(path):
            sys.exit("no %s in %s" % (name, incdir))
        text = open(path, errors="ignore").read()
        for m in re.finditer(r'DEFINE_GUIDSTRUCT\("([0-9A-Fa-f-]{36})",\s*(\w+)\)', text):
            out[m.group(2)] = m.group(1).lower()
    return out


def our_guids(header):
    out = {}
    text = open(header).read().replace("\\\n", "")
    # The macro's own definition looks like a use of it.
    text = re.sub(r'#\s*define\s+RA_KS_GUID\([^\n]*', '', text)
    for m in re.finditer(r'RA_KS_GUID\(\s*(\w+)\s*,\s*([^)]+)\)', text):
        vals = [int(v.strip(), 16) for v in m.group(2).split(',')]
        if len(vals) != 11:
            sys.exit("%s: expected 11 fields, got %d" % (m.group(1), len(vals)))
        out[m.group(1)] = "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x" % tuple(vals)
    return out


def main():
    incdir = sys.argv[1] if len(sys.argv) > 1 else "/usr/x86_64-w64-mingw32/include"
    here   = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ours   = our_guids(os.path.join(here, "audio", "drivers", "wdmks.c"))
    theirs = system_guids(incdir)
    bad    = 0

    for name, value in sorted(ours.items()):
        if name in PAIRS:
            want = theirs.get(PAIRS[name])
            if want is None:
                print("  %-34s NOT FOUND in the system headers" % PAIRS[name])
                bad += 1
            elif want != value:
                print("  %-34s MISMATCH\n      ours   %s\n      system %s"
                      % (PAIRS[name], value, want))
                bad += 1
            else:
                print("  %-34s matches" % PAIRS[name])
        elif name in WAVE_FAMILY:
            want = "%08x-0000-0010-8000-00aa00389b71" % WAVE_FAMILY[name]
            if want != value:
                print("  %-34s MISMATCH\n      ours   %s\n      family %s"
                      % (name, value, want))
                bad += 1
            else:
                print("  %-34s matches the WAVE format family" % name)
        else:
            print("  %-34s IS NOT CHECKED - add it to this script" % name)
            bad += 1

    if bad:
        print("%d GUID problem(s)" % bad)
        return 1
    print("every KS GUID matches the system headers")
    return 0


if __name__ == "__main__":
    sys.exit(main())
