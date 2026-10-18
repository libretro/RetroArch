#!/usr/bin/env python3
"""Exercise the real pointer helper across a transient X connection failure."""
import contextlib
import importlib.util
import io
import os
from pathlib import Path
import tempfile
from unittest import mock

spec = importlib.util.spec_from_file_location(
    'views_e2e', Path(__file__).with_name('run.py'))
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


def check(failures):
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        (root / 'pointer.at').write_text('1 450 420\n', encoding='utf-8')
        opened, warped, ticks = [], [], [0.0]

        def open_display(_):
            opened.append(1)
            return None if len(opened) <= failures else 123

        def sync(*args):
            (root / 'pointer.quit').touch()

        def sleep(seconds):
            ticks[0] += seconds

        x11 = mock.Mock()
        x11.XOpenDisplay = open_display
        x11.XDefaultRootWindow = lambda dpy: 456
        x11.XWarpPointer = lambda *args: warped.append(args)
        x11.XSync = sync
        output = io.StringIO()
        with mock.patch.dict(os.environ, DISPLAY=':99'), \
                mock.patch.object(runner, 'XVFB', True), \
                mock.patch.object(runner.ctypes, 'CDLL', return_value=x11), \
                mock.patch.object(runner.time, 'monotonic', lambda: ticks[0]), \
                mock.patch.object(runner.time, 'sleep', sleep), \
                contextlib.redirect_stdout(output):
            result = runner.pointer_helper(directory)
        if failures < 1000:
            assert result == 0, output.getvalue()
            assert len(opened) == failures + 1
            assert warped == [(123, 0, 456, 0, 0, 0, 0, 450, 420)]
            assert (root / 'pointer.done').read_text(encoding='utf-8').strip() == '1'
        else:
            assert result == 1
            assert 5 <= ticks[0] < 5.1
            assert not warped and not (root / 'pointer.done').exists()
            assert 'cannot open DISPLAY=:99' in output.getvalue()


check(0)
check(3)
check(1000)
print('PASS: pointer helper startup, transient connection and bounded failure')
