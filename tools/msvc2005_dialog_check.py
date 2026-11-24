#!/usr/bin/env python3
"""Check the scope of the VC8 Griffin optimizer workarounds.

This checks preprocessor selection, not the VC8 optimizer itself. The
windows_msvc2005_x86 GitLab build remains the native compiler check.
"""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile


def check(source, version, mixer, function_name, following_name):
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
    function = text.index(function_name)
    following = text.index(following_name)
    assert pragmas[0].end() < function < pragmas[1].start() < following
    assert text[pragmas[0].end():function].strip() == ''
    assert text[pragmas[1].end():following].strip() == ''


def main():
    root = Path(__file__).resolve().parents[1]
    cases = (
        ('menu/cbs/menu_cbs_ok.c', 'static int action_ok_dialog_init(',
         'int action_ok_quit('),
        ('menu/drivers/xmb.c', 'static void xmb_init_horizontal_list(',
         'static void xmb_toggle_horizontal_list('),
    )
    for path, function_name, following_name in cases:
        text = (root / path).read_text(encoding='utf-8')
        function = text.index(function_name)
        start = text.rfind('\n}', 0, function) + 2
        end = text.index('\n{', text.index(following_name, function))
        source = text[start:end] + ';\n'
        for version in (None, 1200, 1400, 1500, 1940):
            for mixer in (False, True):
                check(source, version, mixer, function_name, following_name)
        for directive in ('off', 'on'):
            broken = source.replace('#pragma optimize("", %s)' % directive, '')
            try:
                check(broken, 1400, False, function_name, following_name)
            except AssertionError:
                pass
            else:
                raise AssertionError('regression control did not fail: ' + path)
    print('PASS VC8 dialog and XMB optimizer guards (native VC8 build still required)')


if __name__ == '__main__':
    main()
