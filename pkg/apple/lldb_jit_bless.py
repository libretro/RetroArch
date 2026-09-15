"""LLDB handler for RetroArch's JIT page-blessing protocol.

On iOS 26+ TXM devices, pages mapped R-X cannot be executed until a
debugger has written to each one; the write itself is what blesses the
page, so every 16KB page has to be dirtied individually.  RetroArch asks
for this with the universal.js protocol -- x16 = command, brk #0xf00d --
which StikDebug answers from JavaScript.  This is the same answer for
lldb, so Xcode can be used in StikDebug's place.

Loaded from pkg/apple/LLDBInitFile, which Xcode reads via the scheme's
"LLDB Init File" setting.
"""

import os
import time

import lldb

BRK_F00D           = 0xD43E01A0   # brk #0xf00d
CMD_DETACH         = 0
CMD_PREPARE_REGION = 1

PAGE_SIZE = 0x4000                # arm64 iOS/macOS
FILL_BYTE = 0x69                  # the byte StikDebug writes

# Pages blessed per debugger write.  1 is what StikDebug does: one
# 1-byte write per page.  A write spanning k pages blesses all k, so
# raising this trades transferred bytes against packet round-trips --
# worth tuning if blessing a large pool over USB is latency-bound.
PAGES_PER_WRITE = max(1, int(os.environ.get("RA_BLESS_PAGES_PER_WRITE", "1")))

# Honoring CMD_DETACH would end the Xcode session, which defeats the
# point of debugging in Xcode; acknowledge it and stay attached instead.
# Set RA_BLESS_HONOR_DETACH=1 to detach for real, as StikDebug does.
HONOR_DETACH = os.environ.get("RA_BLESS_HONOR_DETACH") == "1"


def _read_u32(process, addr):
    err = lldb.SBError()
    data = process.ReadMemory(addr, 4, err)
    if not err.Success() or data is None or len(data) != 4:
        return None
    return int.from_bytes(data, "little")


def _bless(process, ptr, size, log):
    """Dirty every page in [ptr, ptr+size) with debugger writes."""
    if size == 0:
        return True, 0

    first = ptr & ~(PAGE_SIZE - 1)
    last  = (ptr + size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)
    pages = (last - first) // PAGE_SIZE

    started = time.monotonic()
    done    = 0
    while done < pages:
        group = min(PAGES_PER_WRITE, pages - done)
        # Minimal buffer that still touches every page in the group.
        buf = bytes([FILL_BYTE]) * ((group - 1) * PAGE_SIZE + 1)
        err = lldb.SBError()
        addr = first + done * PAGE_SIZE
        process.WriteMemory(addr, buf, err)
        if not err.Success():
            log("[JITBless] write failed at 0x%x after %d/%d pages: %s"
                % (addr, done, pages, err.GetCString()))
            return False, done
        done += group

    log("[JITBless] blessed %d pages (%.1f MB) at 0x%x in %.2fs"
        % (pages, pages * PAGE_SIZE / (1024.0 * 1024.0), ptr,
           time.monotonic() - started))
    return True, pages


class JITBlessHook:
    """Stop hook that services brk #0xf00d wherever it is executed.

    Matching on the trapping instruction rather than on a symbol keeps
    this working for every call site, and in release builds, the same way
    universal.js does.
    """

    def __init__(self, target, extra_args, internal_dict):
        pass

    def handle_stop(self, exe_ctx, stream):
        def log(msg):
            stream.Print(msg + "\n")

        process = exe_ctx.GetProcess()
        frame   = exe_ctx.GetFrame()
        if not frame.IsValid():
            return True

        pc = frame.GetPC()
        if _read_u32(process, pc) != BRK_F00D:
            return True                  # not ours; stop normally

        cmd = frame.FindRegister("x16").GetValueAsUnsigned()
        if cmd == CMD_PREPARE_REGION:
            ptr  = frame.FindRegister("x0").GetValueAsUnsigned()
            size = frame.FindRegister("x1").GetValueAsUnsigned()
            if ptr == 0 and size != 0:
                log("[JITBless] x0=0 asks the debugger to allocate the "
                    "region; not implemented")
                return True
            ok, _ = _bless(process, ptr, size, log)
            if not ok:
                # Leave the process stopped at the brk: continuing would
                # report success for pages that were never blessed.
                return True
            # universal.js hands the region back in x0.
            frame.FindRegister("x0").SetValueFromCString("0x%x" % ptr)
        elif cmd == CMD_DETACH:
            if HONOR_DETACH:
                log("[JITBless] detaching")
                frame.SetPC(pc + 4)
                process.Detach()
                return False
            log("[JITBless] detach requested; staying attached")
        else:
            log("[JITBless] unknown command x16=%d at 0x%x" % (cmd, pc))
            return True

        if not frame.SetPC(pc + 4):
            log("[JITBless] could not advance pc past brk at 0x%x" % pc)
            return True
        return False                     # handled; auto-continue


def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand(
        "target stop-hook add -P %s.JITBlessHook" % __name__)
