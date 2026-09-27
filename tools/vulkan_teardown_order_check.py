#!/usr/bin/env python3
"""Vulkan teardown-order check.

Holds the order in which the Vulkan video driver retires the objects a
recorded frame is built out of, and the rule that a descriptor pool is
the one call that frees its sets.

Rationale: a command buffer references the descriptor sets and the
pipelines it was recorded with, and a descriptor set references the
layout it was allocated from. A driver is free to keep its own back
references along that chain and to walk them as each pool is
destroyed - MoltenVK does - so teardown has to run innermost first:
command pools, then descriptor pools, then the pipelines that own the
set layout. The other order is legal Vulkan and passes the validation
layer, which is why it needs a check of its own: it faults inside
vkDestroyDescriptorPool, on the video thread, at exit.

Three rules, over gfx/drivers/vulkan.c:

  1. In each teardown function, whichever of vulkan_deinit_*() it
     calls appear in the order above.
  2. No vkFreeDescriptorSets: vkDestroyDescriptorPool releases every
     set allocated from the pool, and the per-set pass over the same
     bookkeeping is what the destroy then repeats.
  3. No VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT in a file
     that frees no sets. The flag is required only by
     vkFreeDescriptorSets, and buys a per-set allocator on the drivers
     that provide one.

Usage:
  vulkan_teardown_order_check.py [--root DIR]
  vulkan_teardown_order_check.py --selftest

Exit status: 0 clean, 1 findings (or self-test failure).
"""

import argparse
import os
import sys
import tempfile

FILES = [
    "gfx/drivers/vulkan.c",
]

# Innermost first. Only the calls a function actually makes are
# checked, so a teardown that does not touch one of these is fine.
ORDER = [
    "vulkan_deinit_command_buffers",
    "vulkan_deinit_descriptor_pool",
    "vulkan_deinit_pipelines",
]

TEARDOWN_FUNCS = [
    "vulkan_free",
    "vulkan_check_swapchain",
]

FREE_SETS = "vkFreeDescriptorSets"
FREE_FLAG = "VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT"


def strip_comments(text):
    """Blank out comments and string bodies, keeping line structure."""
    out = []
    i = 0
    n = len(text)
    while i < n:
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("".join("\n" if c == "\n" else " " for c in text[i:j]))
            i = j
        elif text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif text[i] == '"':
            j = i + 1
            while j < n and text[j] != '"':
                if text[j] == "\\":
                    j += 1
                j += 1
            out.append('"' + " " * max(0, j - i - 1) + '"')
            i = j + 1
        else:
            out.append(text[i])
            i += 1
    return "".join(out)


def function_body(lines, name):
    """Lines of one function's body, by the tree's definition shape:
    the name directly before the parameter list, on a line that starts
    at column 0, closed by a brace at column 0."""
    start = -1
    for idx, line in enumerate(lines):
        if start < 0:
            if (line[:1] not in (" ", "\t", "", "#")
                    and (name + "(") in line.replace(" ", "")
                    and not line.rstrip().endswith(";")):
                start = idx
        elif line.startswith("}"):
            return lines[start:idx]
    return []


def check_order(rel, lines, report):
    """Rule 1, over one file's teardown functions."""
    bad = 0
    for func in TEARDOWN_FUNCS:
        body = function_body(lines, func)
        if not body:
            report("teardown order: %s: %s() not found\n" % (rel, func))
            bad += 1
            continue
        seen = []
        for line in body:
            for call in ORDER:
                if (call + "(") in line.replace(" ", ""):
                    seen.append(call)
        rank = [ORDER.index(c) for c in seen]
        if rank != sorted(rank):
            report("teardown order: %s|%s: %s\n"
                   % (rel, func, " then ".join(seen)))
            report("               expected: %s\n"
                   % " then ".join(c for c in ORDER if c in seen))
            bad += 1
    return bad


def check_file(root, rel, report):
    path = os.path.join(root, rel)
    if not os.path.exists(path):
        return 0
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        text = strip_comments(f.read())
    lines = text.split("\n")
    bad = check_order(rel, lines, report)

    frees = sum(1 for line in lines if (FREE_SETS + "(") in line.replace(" ", ""))
    if frees:
        report("teardown order: %s: %d %s call(s); the pool destroy "
               "frees its sets\n" % (rel, frees, FREE_SETS))
        bad += 1
    elif any(FREE_FLAG in line for line in lines):
        report("teardown order: %s: %s set, but no %s call needs it\n"
               % (rel, FREE_FLAG, FREE_SETS))
        bad += 1
    return bad


def check(root, report=None):
    if report is None:
        report = sys.stderr.write
    bad = 0
    for rel in FILES:
        bad += check_file(root, rel, report)
    return 1 if bad else 0


GOOD = """
static void vulkan_free(void *data)
{
   if (vk->context)
   {
      vulkan_deinit_command_buffers(vk);
      vulkan_deinit_descriptor_pool(vk);
      vulkan_deinit_pipelines(vk);
   }
}

static void vulkan_check_swapchain(vk_t *vk)
{
   vulkan_deinit_command_buffers(vk);
   vulkan_deinit_descriptor_pool(vk);
   vulkan_deinit_pipelines(vk);
}
"""

BAD = """
static void vulkan_free(void *data)
{
   if (vk->context)
   {
      vulkan_deinit_pipelines(vk);
      vulkan_deinit_descriptor_pool(vk);
      vulkan_deinit_command_buffers(vk);
   }
}

static void vulkan_check_swapchain(vk_t *vk)
{
   vulkan_deinit_command_buffers(vk);
   vulkan_deinit_descriptor_pool(vk);
   vulkan_deinit_pipelines(vk);
   pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
}
"""


def selftest():
    quiet = []
    for name, body, want in (("good", GOOD, 0), ("bad", BAD, 1)):
        root = tempfile.mkdtemp()
        path = os.path.join(root, "gfx", "drivers")
        os.makedirs(path)
        with open(os.path.join(path, "vulkan.c"), "w", encoding="utf-8") as f:
            f.write(body)
        got = check(root, report=quiet.append)
        if got != want:
            sys.stderr.write("selftest: %s tree returned %d, wanted %d\n"
                             % (name, got, want))
            return 1
    # The bad tree has to fail for both reasons, not just one.
    if len(quiet) != 3:
        sys.stderr.write("selftest: expected 3 findings, got %d\n" % len(quiet))
        return 1
    sys.stderr.write("selftest: ok\n")
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)), ".."))
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()
    if args.selftest:
        return selftest()
    return check(args.root)


if __name__ == "__main__":
    sys.exit(main())
