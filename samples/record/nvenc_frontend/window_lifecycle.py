"""Native lifecycle actions against this test's isolated desktop and process."""
import ctypes
from ctypes import wintypes
import json
from pathlib import Path
import socket
import subprocess
import time


def resize_window(runner_pid, width, height):
    user = ctypes.WinDLL('user32', use_last_error=True)
    user.OpenDesktopW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD,
                                 wintypes.BOOL, wintypes.DWORD]
    user.OpenDesktopW.restype = wintypes.HANDLE
    user.CloseDesktop.argtypes = [wintypes.HANDLE]
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user.EnumDesktopWindows.argtypes = [wintypes.HANDLE, callback_type, wintypes.LPARAM]
    user.IsWindowVisible.argtypes = [wintypes.HWND]
    user.GetClientRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
    user.SetWindowPos.argtypes = [wintypes.HWND, wintypes.HWND, ctypes.c_int,
                                 ctypes.c_int, ctypes.c_int, ctypes.c_int, wintypes.UINT]
    # The desktop name is owned by desktop_runner.c, not the user's desktop.
    desktop = user.OpenDesktopW(f'RetroArchStreamTest{runner_pid}', 0, False, 0x00c1)
    assert desktop, ('OpenDesktopW', ctypes.get_last_error())
    windows = []

    @callback_type
    def found(window, unused):
        rect = wintypes.RECT()
        if user.IsWindowVisible(window) and user.GetClientRect(window, ctypes.byref(rect)):
            if rect.right > 100 and rect.bottom > 100:
                windows.append(window)
        return True

    try:
        assert user.EnumDesktopWindows(desktop, found, 0)
        assert len(windows) == 1, ('isolated desktop windows', len(windows))
        # SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS: the frontend's
        # window thread handles the resize; the test cannot hold its UI loop.
        assert user.SetWindowPos(windows[0], None, 0, 0, width, height, 0x4014), (
            'SetWindowPos', ctypes.get_last_error())
    finally:
        user.CloseDesktop(desktop)


def run_lifecycle(command, path, config, environment, action):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reserve:
        reserve.bind(('127.0.0.1', 0))
        port = reserve.getsockname()[1]
    with config.open('a', encoding='utf-8') as out:
        out.write(f'\nnetwork_cmd_enable = "true"\nnetwork_cmd_port = "{port}"\n')
    live = Path(path) / 'live.log'
    trace = []
    with live.open('wb') as output, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as client:
        client.settimeout(.15)
        process = subprocess.Popen(command, cwd=path, env=environment,
                                   stdout=output, stderr=subprocess.STDOUT)

        def log():
            return live.read_text(encoding='utf-8', errors='replace')

        def request(text, reply=True):
            client.sendto((text + '\n').encode(), ('127.0.0.1', port))
            if not reply:
                trace.append({'command': text})
                return ''
            answer = client.recv(8192).decode(errors='replace').strip()
            trace.append({'command': text, 'reply': answer})
            return answer

        def until(predicate, description, timeout=15):
            end = time.monotonic() + timeout
            while time.monotonic() < end:
                assert process.poll() is None, (description, process.returncode, log()[-6000:])
                try:
                    if predicate():
                        return
                except (socket.timeout, ConnectionResetError):
                    pass
                time.sleep(.025)
            raise AssertionError((description, log()[-6000:]))

        def frame():
            answer = request('GET_OPTION frame_count')
            assert answer.startswith('frame_count\t'), answer
            return int(answer.split('\t')[1])

        try:
            until(lambda: frame() >= 60 and 'Starting native' in log()
                  and (Path(path) / 'output.mp4').exists(), 'recording startup')
            assert request('GET_OPTION recording') == 'recording\ton'
            first = frame()
            if action == 'resize':
                for width, height in ((820, 500), (1000, 640), (900, 560)):
                    resize_window(process.pid, width, height)
                    trace.append({'resize': [width, height]})
                    baseline = frame()
                    until(lambda: frame() >= baseline + 8, 'frame progress after resize')
            elif action == 'reinit':
                for count in range(2, 5):
                    request('VIDEO_REINIT', reply=False)
                    until(lambda: log().count('[Vulkan] Found GPU #0:') >= count,
                          'video device reinitialization')
                    until(lambda: frame() >= 8, 'frame progress after reinit')
                text = log()
                assert text.index('Recording finalized') < text.index(
                    '[Vulkan] Found GPU #0:', text.index('[Vulkan] Found GPU #0:') + 1), text
            else:
                assert action == 'stop'
                assert request('SET_OPTION recording off') == 'recording\toff'
                until(lambda: frame() >= first + 16, 'frame progress after recording stop')
            until(lambda: 'Recording finalized' in log(), 'asynchronous recording drain')
            assert 'Recording failed' not in log(), log()
            trace.append({'drained': True, 'frame': frame()})
            status = process.wait(timeout=30)
            return subprocess.CompletedProcess(command, status, live.read_bytes())
        finally:
            if process.poll() is None:
                process.kill()  # desktop runner's job closes only its own child
                process.wait(timeout=5)
            (Path(path) / 'actions.json').write_text(json.dumps(trace, indent=2) + '\n')
