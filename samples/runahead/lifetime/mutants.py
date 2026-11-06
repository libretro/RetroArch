#!/usr/bin/env python3
"""Build private, deliberately broken copies and require the expected failure."""
import argparse
from pathlib import Path
import subprocess
import sys


MUTANTS = {
    'missing-release': (
        '\n   serialization_interface.release(data, size);\n',
        '\n',
        'lifetime_test_counts()->releases == 1'),
    'overwrite-on-refusal': (
        '   if (*owned)\n      destination = serialization_stage;',
        '   if (false && *owned)\n      destination = serialization_stage;',
        'memcmp(copy, buffer, sizeof(copy)) == 0'),
    'missing-halt-message': (
        '      runloop_msg_queue_push(msg, strlen(msg), 2, 10 * 60, true, NULL,\n'
        '            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);\n',
        '',
        '!lifetime_test_counts()->runs && messages == 1 && pauses == 1'),
    'late-registration': (
        '       serialization_interface.release ||\n'
        '       (runloop_state_get_ptr()->current_core.flags & RETRO_CORE_FLAG_INITED))',
        '       serialization_interface.release)',
        '!serialization_register(&callbacks)'),
    'missing-preempt-release': (
        '      for (i = 0; i < preempt->frames; i++)\n'
        '         serialization_release_buffer(preempt->buffer[i], preempt->state_size,\n'
        '               &preempt->state_owned[i]);',
        '      for (i = 0; i < preempt->frames; i++)\n'
        '         (void)preempt->state_owned[i];',
        '!state.preempt_data && lifetime_test_counts()->releases == 3'),
    'stale-history': (
        '   preempt->frame_count = 0;\n   preempt->start_ptr = 0;',
        '   preempt->start_ptr = 0;',
        '!preempt->frame_count && !preempt->start_ptr && !preempt->replay_ptr'),
}


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError('mutation source changed: ' + repr(old))
    return text.replace(old, new, 1)


def generate(name, output):
    sample = Path(__file__).resolve().parent
    root = sample.parents[2]
    source = (root / 'runahead.c').read_text(encoding='utf-8')
    old, new, _ = MUTANTS[name]
    source = replace_once(source, old, new)
    test = (sample / 'lifetime_test.c').read_text(encoding='utf-8')
    test = replace_once(test, '#include "../../../runahead.c"', source)
    test = replace_once(test, '#include "reference_core.h"',
                        '#include "' + (sample / 'reference_core.h').as_posix() + '"')
    # A known exit code avoids native crash dialogs and rejects unrelated faults.
    test = replace_once(test, '#include <assert.h>', r'''#include <assert.h>
#undef assert
#define assert(condition) do { if (!(condition)) { \
   fflush(stdout); fprintf(stderr, "FAIL: %s\n", #condition); \
   exit(99); } } while (0)''')
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('w', encoding='utf-8', newline='\n') as stream:
        stream.write(test)


def check(name, executable):
    result = subprocess.run([str(executable.resolve())], stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, timeout=20)
    expected = 'FAIL: ' + MUTANTS[name][2]
    if result.returncode != 99 or result.stdout.splitlines()[-1:] != [expected]:
        sys.stderr.write(result.stdout)
        raise RuntimeError('%s: expected exit 99 at %s; got %s' %
                           (name, expected, result.returncode))
    print('PASS rejects ' + name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('generate', 'check'))
    parser.add_argument('mutant', choices=MUTANTS)
    parser.add_argument('path', type=Path)
    args = parser.parse_args()
    try:
        if args.action == 'generate':
            generate(args.mutant, args.path)
        else:
            check(args.mutant, args.path)
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        parser.exit(1, str(error) + '\n')


if __name__ == '__main__':
    main()
