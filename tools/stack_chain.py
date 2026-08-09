#!/usr/bin/env python3
"""Worst-case cumulative stack per call chain, from a set of roots.

tools/stack_budget.py checks one frame at a time, which is necessary
and not sufficient: a frame does not run alone, it runs on top of
everything that called it. config_file_write() was caught by that
check only because the offending 16 KiB was in its own frame. Ten
1.5 KiB frames in a row pass every per-frame check ever written and
still overflow an 8 KiB thread stack.

This walks the call graph GCC emits with -fcallgraph-info=su and sums
frames along it, reporting the deepest chain from each root. Task
handlers are the roots that matter: retro_task_regular_gather() runs
them on the main thread, but a threaded task queue runs them on a
worker, and on GEKKO that worker has 8 KiB (STACKSIZE in
rthreads/gx_pthread.h).

WHAT THIS IS NOT

A verdict. Three things keep it a screening tool rather than a proof:

  - The deepest path may be infeasible. Nothing here knows that two
    branches are mutually exclusive, so the sum can describe a chain
    that cannot occur.
  - Indirect calls are invisible to the call graph, so a real chain
    can be deeper than anything reported here. The number is not an
    upper bound either.
  - It measures what the *given* build configuration compiles. Run
    with a host config.h and it will sum codecs the target never
    builds - the same roots measure 73 KiB with everything enabled
    and 20 KiB with a console-shaped feature set, and neither figure
    describes a platform nobody built.

So it is deliberately not wired into CI: it would report numbers that
need a real target configuration to mean anything, and a check that
cries wolf gets an allowlist and then gets ignored. Point it at a
tree configured for the target you care about, and read the chains
rather than the totals.

USAGE

  # emit call graphs for the translation units of interest
  mkdir ci && cd ci
  gcc -O2 -DGEKKO -fcallgraph-info=su -c <sources> -I... -include config.h

  # roots: one symbol per line
  tools/stack_chain.py ci roots.txt

A roots file can be built from the task handlers with:

  grep -rhoE '(\\.|->)handler[[:space:]]*=[[:space:]]*[a-zA-Z_][a-zA-Z0-9_]*' \\
      tasks/*.c *.c menu/*.c | sed 's/.*=[[:space:]]*//' | sort -u
"""
import collections
import glob
import os
import re
import sys

NODE = re.compile(r'node:\s*\{\s*title:\s*"([^"]+)"\s*label:\s*"([^"]*?)"', re.S)
EDGE = re.compile(r'edge:\s*\{\s*sourcename:\s*"([^"]+)"\s*targetname:\s*"([^"]+)"')
SIZE = re.compile(r'(\d+)\s+bytes')
# GCC suffixes clones; the roots file names the original.
CLONE = re.compile(r'\.(isra|constprop|part|cold|lto_priv)[0-9.]*$')


def load(ci_dir):
    """size[node], edges[node] -> set(node), byname[symbol] -> [node...]"""
    size = {}
    edges = collections.defaultdict(set)
    byname = collections.defaultdict(list)

    files = glob.glob(os.path.join(ci_dir, '*.ci'))
    if not files:
        print('error: no .ci files in %s - was -fcallgraph-info=su passed?'
              % ci_dir, file=sys.stderr)
        sys.exit(2)

    for path in files:
        with open(path, encoding='utf-8', errors='replace') as f:
            text = f.read()
        for title, label in NODE.findall(text):
            m = SIZE.search(label)
            if not m:
                continue           # declaration only, no body here
            size[title] = int(m.group(1))
            byname[CLONE.sub('', title.split(':')[-1])].append(title)
        for src, dst in EDGE.findall(text):
            edges[src].add(dst)
    return size, edges, byname


def analyse(size, edges, byname, roots, top):
    def targets(node):
        """Resolve a node's callees, including cross-TU ones.

        A call to a function defined in another translation unit is
        emitted as a bare symbol, so it has to be matched back to
        whichever node defines it."""
        out = set()
        for dst in edges.get(node, ()):
            if dst in size:
                out.add(dst)
            else:
                out.update(byname.get(CLONE.sub('', dst.split(':')[-1]), ()))
        return out

    memo = {}

    def worst(node, on_stack):
        # Recursion is reported rather than summed: its depth is a
        # runtime property this cannot see.
        if node in on_stack:
            return 0, ['<recursion>']
        if node in memo:
            return memo[node]
        best, best_path = 0, []
        for dst in targets(node):
            cost, path = worst(dst, on_stack | {node})
            if cost > best:
                best, best_path = cost, path
        result = (size.get(node, 0) + best,
                  [CLONE.sub('', node.split(':')[-1])] + best_path)
        memo[node] = result
        return result

    found = []
    for root in roots:
        for node in byname.get(root, ()):
            total, path = worst(node, frozenset())
            found.append((total, root, path))

    if not found:
        print('error: none of the given roots were found in the call graph',
              file=sys.stderr)
        return 2

    for total, root, path in sorted(found, reverse=True)[:top]:
        print('%7d  %s' % (total, root))
        print('         %s' % ' -> '.join(path[:8]))
    return 0


def main():
    if len(sys.argv) < 3:
        print(__doc__, file=sys.stderr)
        return 2
    ci_dir = sys.argv[1]
    with open(sys.argv[2]) as f:
        roots = [l.strip() for l in f if l.strip()]
    top = int(sys.argv[3]) if len(sys.argv) > 3 else 15
    size, edges, byname = load(ci_dir)
    print('call graph: %d nodes with frames, %d roots requested\n'
          % (len(size), len(roots)))
    return analyse(size, edges, byname, roots, top)


if __name__ == '__main__':
    sys.exit(main())
