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

Two things about how this is measured, both learned the hard way.

A frame size is a property of an ABI, not of the source. The Windows
x64 ABI has the caller reserve 32 bytes of shadow space for every
call, keeps more registers callee-saved, and passes four arguments in
registers where SysV passes six. Every recorded size below therefore
comes out 16 to 192 bytes larger under mingw-w64 than under the
reference toolchain, on identical code. Comparing an absolute
recorded size against a measurement from a different target triple
reports the whole allowlist as regressed and buries anything real in
the noise, so the recorded sizes are only ratcheted when the host is
the reference target; elsewhere they are reported as drift.

The check also only ever sees the code its host compiles, which is
the same blind spot it exists to cover. read_stdin() carried a 5 KiB
INPUT_RECORD array for years inside `#elif defined(_WIN32)`, invisible
to every Linux run of this script. So when a cross-compiler is on
PATH it is used for a second pass, and BUDGET - unlike the recorded
sizes - is enforced on every pass, because a coarse threshold is
robust to a few hundred bytes of ABI overhead in a way that an exact
byte count is not.
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

# The target the sizes in ALLOWLIST were measured on.
REFERENCE_TARGETS = ('x86_64-pc-linux-gnu', 'x86_64-linux-gnu')

# Added to both BUDGET and the recorded sizes when the measuring
# target is not a reference one, so that pure ABI overhead does not
# read as a regression. The largest gap observed between SysV and
# Windows x64 across this tree is 192 bytes; a frame that has really
# grown, by a buffer someone added, clears 256 easily. This is slack
# for a comparison that is already approximate off-reference - it is
# not a raise in the budget, and BUDGET itself does not move.
ABI_SLACK         = 256

# Used for an extra pass when present, to reach code the host's own
# preprocessor throws away. Not required: absent ones are skipped.
CROSS_COMPILERS   = ('x86_64-w64-mingw32-gcc', 'i686-w64-mingw32-gcc')

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
# past its recorded size is caught even while allowlisted. It is the
# size on REFERENCE_TARGETS; see the module docstring for why it is
# not comparable against a measurement from another triple.
ALLOWLIST  = {
}


def toolchains():
    """[(cc, target, is_reference)] for every compiler available here.

    The host compiler is always first: it is the one that decides
    whether a named file failing to compile is an error, and the only
    one that may ratchet ALLOWLIST, and then only if its target is a
    reference one.
    """
    found = []
    for cc in ('gcc',) + CROSS_COMPILERS:
        try:
            r = subprocess.run([cc, '-dumpmachine'], capture_output=True)
        except OSError:
            continue
        if r.returncode != 0:
            continue
        target = r.stdout.decode().strip()
        found.append((cc, target, target in REFERENCE_TARGETS))
    return found


def stack_usage(path, workdir, cc='gcc'):
    """Compile one TU and return ([(func, bytes)], gcc stderr).

    Frames are None when the TU did not build, and the stderr is
    returned with it so a caller that named the file itself can say
    why rather than skipping it silently.

    gcc writes the .su alongside the object named by -o, not alongside
    the source, so the name has to be derived from the object.  Getting
    that wrong is silent: the file simply is not there, every TU
    reports no frames, and the check passes while measuring nothing.
    It did, until a deliberately oversized buffer failed to trip it.
    """
    # gcc runs with cwd=workdir so the .su lands there, which means a
    # relative source path would resolve against the temp directory and
    # not be found.  The tree walk never hit this because SRC_ROOT is
    # built from abspath(__file__); a path typed on the command line is
    # relative more often than not.
    path = os.path.abspath(path)
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
    cmd = [cc, '-O2', '-DPSP', '-DHAVE_COMPRESSION', '-DHAVE_7ZIP',
           '-DHAVE_CHD', '-DHAVE_RPNG', '-DHAVE_RJPEG', '-DHAVE_RBMP',
           '-DHAVE_RTGA', '-DHAVE_RWEBP', '-DHAVE_RDDS', '-DHAVE_RWAV',
           '-I' + INCLUDE, '-fstack-usage', '-c', path, '-o', obj]
    r = subprocess.run(cmd, cwd=workdir, capture_output=True)
    # The .su existing is not proof the compile succeeded: gcc creates
    # it and then bails, so a TU that fails to parse leaves an empty
    # one behind and reports no frames - measuring nothing while
    # looking clean, the same failure the file name once caused.  The
    # exit status is the thing to trust.
    if r.returncode != 0 or not os.path.exists(su):
        # Did not compile on this host (platform-specific TU, missing
        # dependency).  Report it rather than counting it as scanned.
        return None, r.stderr.decode('utf-8', 'replace')
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
    return out, ''


def walk(root):
    """Yield the .c files under root that a console actually builds."""
    for dirpath, _, files in os.walk(root):
        parts = dirpath.split(os.sep)
        # Shipped code only: samples, tools and tests are
        # host programs, not console builds, and their
        # frames are irrelevant to a PSP thread stack.
        if ({'samples', 'tools', 'test', 'tests'} & set(parts)):
            continue
        for fn in sorted(files):
            if fn.endswith('.c'):
                yield os.path.join(dirpath, fn)


def sources(args):
    """(path, named) for each TU to measure.

    named is True for a file the caller spelled out, and decides what a
    failure to compile means.  In a walk it means "not this host" and
    is skipped, which is right for a platform TU nobody expects to
    build here.  For a file someone named it means the measurement did
    not happen, and skipping it would print a clean result for a file
    that was never looked at - the same failure mode as the missing
    .su.  A directory given as an argument is a walk of that
    directory, and its files skip like any other walk.
    """
    if not args:
        for path in walk(SRC_ROOT):
            yield path, False
        return
    for arg in args:
        if os.path.isdir(arg):
            for path in walk(arg):
                yield path, False
        else:
            yield arg, True


class Fatal(Exception):
    """A file the caller named could not be measured."""


def measure(cc, is_host, argv, tmp):
    """([(size, name, fn)], scanned, skipped) for one toolchain.

    The strictness about a named file that does not compile is the
    host compiler's alone.  A cross pass is extra coverage over code
    the host preprocesses away, so a POSIX-only TU someone named
    failing to build under mingw is expected rather than an error -
    the host pass has already measured it.
    """
    frames  = []
    scanned = 0
    skipped = 0
    for path, named in sources(argv):
        named = named and is_host
        if named and not os.path.exists(path):
            raise Fatal('error: %s: no such file' % path)
        # A header compiles to a .gch and yields no .su, which
        # would otherwise be reported as "did not compile" with
        # nothing to say about why.
        if named and not path.endswith('.c'):
            raise Fatal('error: %s: not a .c translation unit' % path)
        found, err = stack_usage(path, tmp, cc)
        if found is None:
            if named:
                raise Fatal('error: %s did not compile; nothing was '
                            'measured for it\n%s' % (path, err.rstrip()))
            skipped += 1
            continue
        scanned += 1
        fn = os.path.basename(path)
        frames += [(size, name, fn) for name, size in found]
    return frames, scanned, skipped


def main(argv):
    if any(a in ('-h', '--help') for a in argv):
        print('usage: stack_budget.py [FILE.c | DIR ...]\n\n'
              'With no arguments, measures every shipped .c under\n'
              'libretro-common - what CI runs, and the only form CI\n'
              'depends on.  With arguments, measures exactly those\n'
              'files, and a named file that does not compile is an\n'
              'error rather than a skip.\n\n'
              'Every cross-compiler in CROSS_COMPILERS that is on PATH\n'
              'is measured as well, to reach code this host would\n'
              'preprocess away.  The sizes in ALLOWLIST are enforced\n'
              'only on REFERENCE_TARGETS, since a frame size is a\n'
              'property of an ABI; elsewhere they are reported as\n'
              'drift.')
        return 0

    chains = toolchains()
    if not chains:
        print('error: no C compiler found; the check is not measuring '
              'anything', file=sys.stderr)
        return 2

    over    = []   # hard failures
    drift   = []   # allowlisted, over its recorded size, off-reference
    scanned = 0

    with tempfile.TemporaryDirectory() as tmp:
        for i, (cc, target, is_reference) in enumerate(chains):
            try:
                frames, n, skipped = measure(cc, i == 0, argv, tmp)
            except Fatal as e:
                print(e, file=sys.stderr)
                return 2
            scanned += n
            print('  %-24s %-24s %4d TU measured, %3d not built'
                  % (cc, target, n, skipped))
            # Off-reference, every limit is approximate by the width
            # of the ABI difference, so both of them move together.
            slack = 0 if is_reference else ABI_SLACK
            for size, name, fn in frames:
                if name not in ALLOWLIST:
                    # Never recorded at any size, on any target.
                    if size > BUDGET + slack:
                        over.append((size, name, fn, target))
                elif size > ALLOWLIST[name] + slack:
                    # Grown past what was recorded.  An error on the
                    # target that recorded it, drift anywhere else.
                    if is_reference:
                        over.append((size, name, fn, target))
                    else:
                        drift.append((size, name, fn, target))

    # A run that measured nothing is a broken check, not a clean tree.
    if scanned == 0:
        print('error: no translation unit yielded stack data; the '
              'check is not measuring anything', file=sys.stderr)
        return 2

    if not any(ref for _, _, ref in chains):
        print('\nnote: no reference target (%s) among the compilers '
              'here, so the sizes recorded in ALLOWLIST were not '
              'enforced. CI is authoritative for those.'
              % ', '.join(REFERENCE_TARGETS))

    if drift:
        print('\nnote: allowlisted frames more than %d bytes above '
              'their recorded size on a non-reference target, which is '
              'more than the ABI accounts for:' % ABI_SLACK)
        for size, name, fn, target in sorted(drift, reverse=True):
            print('   %7d  (recorded %d, %+d)  %s  (%s, %s)'
                  % (size, ALLOWLIST[name], size - ALLOWLIST[name],
                     name, fn, target))

    if over:
        print('\nerror: stack frames over %d bytes, against an 8 KiB '
              'thread stack on PSP and GX:' % BUDGET, file=sys.stderr)
        for size, name, fn, target in sorted(over, reverse=True):
            print('   %7d  %s  (%s, %s)' % (size, name, fn, target),
                  file=sys.stderr)
        print('\nMove the buffer to the heap, shrink it, or - if it '
              'genuinely cannot reach a thread on those targets - add '
              'it to ALLOWLIST in %s with the reason.'
              % os.path.basename(__file__), file=sys.stderr)
        return 1

    print('\nstack budget: no frame over %d bytes' % BUDGET)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
