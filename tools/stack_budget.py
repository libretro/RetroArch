#!/usr/bin/env python3
"""Fail on libretro-common functions whose stack frame would not fit a
console thread stack.

The smallest thread stack in the tree is GEKKO's 8 KiB - STACKSIZE in
rthreads/gx_pthread.h - with 32 KiB on 3DS (ctr_pthread.h) and 0x10000
on Vita. (rthreads/psp_pthread.h names 8 KiB as well, but nothing
includes it: rthreads.c takes gx_pthread.h under GEKKO and
ctr_pthread.h under _3DS, and PSP falls through to plain pthreads. The
floor is GEKKO's.) Nothing warns when a local buffer outgrows them:
it builds everywhere, and overflows only on a target most contributors
cannot build, at whatever moment that code first runs on a task
thread.

That is not hypothetical. config_file_write() carried a 16 KiB stdio
buffer as a local, twice an 8 KiB stack, and is reached from
input_autoconfigure_connect_handler - a task handler, so it runs off
the main thread whenever the task queue is threaded, on the path a
gamepad being plugged in takes.

Frames are measured with -fstack-usage under -DPSP, because
PATH_MAX_LENGTH is 512 on those targets rather than 2048 and a
host-shaped measurement overstates every function that holds a path
buffer - several archive functions look like 6 KiB on a desktop build
and are under 4 KiB there.

The budget is deliberately well under 8 KiB: a frame is not alone on
the stack, it sits under whatever called it.
"""
import os
import re
import subprocess
import sys
import tempfile

ROOT       = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INCLUDE    = os.path.join(ROOT, 'libretro-common', 'include')
SRC_ROOT   = os.path.join(ROOT, 'libretro-common')

# Bytes. Leaves the majority of an 8 KiB stack for callers.
BUDGET     = 2048

# Frames that exceeded the budget when this check was written, with
# the size measured then.
#
# THE RULE: an entry here is debt, not an exemption. "It is not built
# on a small-stack target" and "nothing reaches it from a task thread"
# are both statements about today, and both have already been wrong
# once - config_file_write() was exempt on exactly that reasoning
# until its caller turned out to be a task handler. Platforms gain
# features, call graphs gain edges, and a frame that is fine now is
# fine only until someone connects it to something. So the question
# for an entry is not "can this overflow today" but "does this need
# to be on the stack at all", and the answer is usually no: the
# fixable ones are a struct or a scratch array that could live on the
# heap or in a state object that is already allocated once.
#
# Fixed under that rule rather than argued about:
#   vh_build                     16784 -> 400   scratch to the heap
#   config_file_write            16432 ->   48  stdio buffer to the heap
#   chd_read_header_core_file    57504 ->   32  whole chd_file to the heap
#   rd_gen_lengths               12768 ->  352  scratch into struct rdeflate
#   rzstd_emit_block              9440 ->    0  FSE tables into encoder scratch
#   sha1_calculate                4304 ->  224  mapped view, else heap buffer
#   filestream_vscanf             4368 ->  272  scan window to the heap
#
# What is left is mostly codec inner loops (rvp9, rh264, raac, ropus,
# rflac, rzstd, rvorbis) whose working sets are large by nature. Large
# by nature is not the same as necessarily-on-the-stack: most of them
# could hold their scratch in the decoder state, which is allocated
# once per stream. They are recorded rather than fixed because that is
# per-codec work with real regression risk, not because they are safe.
#
# The check is a ratchet: it stops the list growing. Removing an entry
# is always welcome; adding one is a decision someone has to write
# down, and "not reachable on the targets I checked" is not one of the
# reasons that counts.
#
# The value is the size at the time of writing, so a frame that grows
# past its recorded size is caught even while allowlisted.
ALLOWLIST  = {
    'cdfs_find_file.constprop': 2160,
    'config_file_dump': 4208,
    'flac_decoder_decode_interleaved': 4816,
    'map_read': 8320,
    'mem_stats_proc_meminfo.constprop': 2160,
    'raac_decode_ics': 4448,
    'raac_filterbank': 16496,
    'raac_imdct': 4240,
    'rchd_decompress': 28736,
    'rchd_map_v5': 2768,
    'rflac__alloc_raw': 4320,
    'rflac_open_with_metadata_private.constprop': 4464,
    'rh264_cabac_decode_islice.constprop.isra': 2240,
    'rh264_cabac_decode_mb_ctx.isra': 2112,
    'rh264_cabac_decode_pslice.constprop.isra': 2608,
    'rh264_video_decode_inter': 7520,
    'rhuff_dec_build': 4240,
    'rhuff_read_tree_packed': 2288,
    'ropus_deinterleave_hadamard': 4160,
    'ropus_deinterleave_hadamard_q': 2112,
    'ropus_interleave_hadamard': 4160,
    'ropus_interleave_hadamard_q': 2112,
    'ropus_silk2_decode': 6480,
    'rvp9_build_inter_pred.isra': 25840,
    'rvp9_build_inter_pred_hbd.isra': 51456,
    'rvp9_convolve8.constprop': 8752,
    'rvp9_convolve8_hbd.constprop': 17392,
    'rvp9_iht_add.isra': 4496,
    'rvp9_iht_add_hbd.isra': 4496,
    'rzstd_huf_read_bmi2': 2368,
    'rzstd_huf_read_sse': 2368,
    'vorbis_decode_packet_rest.isra': 2672,
}


def stack_usage(path, workdir):
    """Compile one TU and return [(func, bytes)] from its .su file.

    gcc writes the .su alongside the object named by -o, not alongside
    the source, so the name has to be derived from the object.  Getting
    that wrong is silent: the file simply is not there, every TU
    reports no frames, and the check passes while measuring nothing.
    It did, until a deliberately oversized buffer failed to trip it.
    """
    obj = os.path.join(workdir, 'o.o')
    su  = os.path.join(workdir, 'o.su')
    if os.path.exists(su):
        os.remove(su)
    # The define set has to be at least as wide as a real build, or
    # the check measures code no target compiles and misses code every
    # target does. HAVE_7ZIP is the case that proved it: without it
    # libchdr_chd.c's largest frame is 8320 bytes, with it 57504 - a
    # seven-fold difference in shipped code, under a define that wii,
    # ctr and every desktop build set.
    cmd = ['gcc', '-O2', '-DPSP', '-DHAVE_COMPRESSION', '-DHAVE_7ZIP',
           '-DHAVE_CHD', '-DHAVE_RPNG', '-DHAVE_RJPEG', '-DHAVE_RBMP',
           '-DHAVE_RTGA', '-DHAVE_RWEBP', '-DHAVE_RDDS', '-DHAVE_RWAV',
           '-I' + INCLUDE, '-fstack-usage', '-c', path, '-o', obj]
    r = subprocess.run(cmd, cwd=workdir, capture_output=True)
    if not os.path.exists(su):
        # Did not compile on this host (platform-specific TU, missing
        # dependency).  Report it rather than counting it as scanned.
        return None
    out = []
    with open(su) as f:
        for line in f:
            parts = line.rstrip('\n').split('\t')
            if len(parts) < 2:
                continue
            name = re.sub(r'^.*:', '', parts[0])
            try:
                out.append((name, int(parts[1])))
            except ValueError:
                pass
    return out


def main():
    over    = []
    scanned = 0
    skipped = 0
    with tempfile.TemporaryDirectory() as tmp:
        for dirpath, _, files in os.walk(SRC_ROOT):
            parts = dirpath.split(os.sep)
            # Shipped code only: samples, tools and tests are
            # host programs, not console builds, and their
            # frames are irrelevant to a PSP thread stack.
            if ({'samples', 'tools', 'test', 'tests'} & set(parts)):
                continue
            for fn in sorted(files):
                if not fn.endswith('.c'):
                    continue
                frames = stack_usage(os.path.join(dirpath, fn), tmp)
                if frames is None:
                    skipped += 1
                    continue
                scanned += 1
                for name, size in frames:
                    if size <= BUDGET:
                        continue
                    # Allowlisted, but only at the size recorded: a
                    # frame that has grown since is a new problem.
                    if name in ALLOWLIST and size <= ALLOWLIST[name]:
                        continue
                    over.append((size, name, fn))

    # A run that measured nothing is a broken check, not a clean tree.
    if scanned == 0:
        print('error: no translation unit yielded stack data; the '
              'check is not measuring anything', file=sys.stderr)
        return 2

    if over:
        print('error: stack frames over %d bytes, against an 8 KiB '
              'thread stack on PSP and GX:' % BUDGET, file=sys.stderr)
        for size, name, fn in sorted(over, reverse=True):
            print('   %7d  %s  (%s)' % (size, name, fn), file=sys.stderr)
        print('\nMove the buffer to the heap, shrink it, or - if it '
              'genuinely cannot reach a thread on those targets - add '
              'it to ALLOWLIST in %s with the reason.'
              % os.path.basename(__file__), file=sys.stderr)
        return 1

    print('stack budget: %d translation units measured (%d did not '
          'build on this host), no frame over %d bytes'
          % (scanned, skipped, BUDGET))
    return 0


if __name__ == '__main__':
    sys.exit(main())
