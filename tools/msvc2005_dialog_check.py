#!/usr/bin/env python3
"""Check the scope of the VC8 Griffin optimizer workaround.

This checks preprocessor selection, not the VC8 optimizer itself. The
windows_msvc2005_x86 GitLab build remains the native compiler check.
"""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile


def check(source, version, mixer):
    cc = shlex.split(os.environ.get('CC', 'cc'))
    with tempfile.TemporaryDirectory() as directory:
        unit = Path(directory) / 'dialog.c'
        unit.write_text(source, encoding='utf-8')
        flags = [] if version is None else ['-D_MSC_VER=' + str(version)]
        if mixer:
            flags.append('-DHAVE_AUDIOMIXER')
        text = subprocess.check_output(
            cc + ['-E', '-P'] + flags + [str(unit)], text=True)
    pragmas = list(re.finditer(r'#pragma\s+optimize\s*\("",\s*(off|on)\)', text))
    if version != 1400:
        assert not pragmas, 'workaround affects another compiler'
        return
    assert [p.group(1) for p in pragmas] == ['off', 'on'], 'missing VC8 guard'
    function = text.index('static int action_ok_dialog_init(')
    following = text.index('int action_ok_quit(')
    assert pragmas[0].end() < function < pragmas[1].start() < following
    assert text[pragmas[0].end():function].strip() == ''
    assert text[pragmas[1].end():following].strip() == ''


def main():
    root = Path(__file__).resolve().parents[1]
    text = (root / 'menu/cbs/menu_cbs_ok.c').read_text(encoding='utf-8')
    function = text.index('static int action_ok_dialog_init(')
    start = text.rfind('\n}', 0, function) + 2
    end = text.index('\n{', text.index('int action_ok_quit(', function))
    source = text[start:end] + ';\n'
    for version in (None, 1200, 1400, 1500, 1940):
        for mixer in (False, True):
            check(source, version, mixer)
    broken = source.replace('#pragma optimize("", off)', '')
    try:
        check(broken, 1400, False)
    except AssertionError:
        pass
    else:
        raise AssertionError('regression control did not fail')
    print('PASS VC8 dialog optimizer guard (native VC8 build still required)')


if __name__ == '__main__':
    main()
