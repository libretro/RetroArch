# Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later
"""Two actual frontends: discover, select in menu, decline, reconnect, accept, stop.
Uses the existing loopback-only MENU command interface; no test hooks in product code.
"""
import argparse
import os
from pathlib import Path
import re
import socket
import subprocess
import time

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--retroarch', type=Path, required=True)
p.add_argument('--core', type=Path, required=True)
p.add_argument('--shader-directory', type=Path)
p.add_argument('--embedded-shaders', action='store_true')
p.add_argument('--backend', choices=('glcore', 'vulkan', 'd3d11', 'd3d12'), default='glcore')
p.add_argument('--media', type=Path)
p.add_argument('--pixels', action='store_true')
p.add_argument('--transition', action='store_true')
p.add_argument('--resize', action='store_true')
p.add_argument('--require-d3d12-validation', action='store_true')
p.add_argument('--frontend-only', action='store_true')
p.add_argument('--record-driver', default='null')
p.add_argument('--threaded', action='store_true')
p.add_argument('--audio-driver', default='null')
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
if not a.embedded_shaders and not a.shader_directory:
    p.error('Pass --embedded-shaders or --shader-directory')
a.output.mkdir(parents=True, exist_ok=True)
clients = []

def wait(test, message, seconds=30):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if test(): return
        for client in clients:
            if client['process'].poll() is not None:
                raise AssertionError('Frontend exited: ' + client['name'])
        time.sleep(.05)
    raise AssertionError(message)

def command(client, action):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.settimeout(30 if action.startswith('LOAD_CONTENT ') else 3)
        s.sendto((action + '\n').encode(), ('127.0.0.1', client['port']))
        result = s.recv(65536).decode('utf-8')
    with (a.output / (client['name'] + '-menu.log')).open('a', encoding='utf-8') as log:
        log.write(action + '\n' + result + '\n')
    return result

def send(client, action):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.sendto((action + '\n').encode(), ('127.0.0.1', client['port']))

def screenshot(client):
    from PIL import Image
    directory = client['shots']
    old = {path: path.stat().st_mtime_ns for path in directory.glob('*.png')}
    result = []
    send(client, 'SCREENSHOT')
    def completed():
        paths = [path for path in directory.glob('*.png')
                 if path not in old or path.stat().st_mtime_ns != old[path]]
        if not paths: return False
        try:
            with Image.open(next(iter(paths))) as image:
                image.load()
                result.append(image.convert('RGB').resize((128, 96)))
        except (OSError, SyntaxError):
            return False  # The asynchronous writer has only created the file.
        return True
    wait(completed, 'Receiver screenshot did not complete')
    return result[0]

def rows(text):
    return {line.split('\t')[1]: int(line.split('\t')[0]) for line in text.splitlines()
            if re.match(r'^\d+\t', line)}

def choose(client, name):
    listing = command(client, 'MENU show from 0')
    options = rows(listing)
    assert name in options, (name, listing)
    return command(client, 'MENU a ' + str(options[name]))

def recording_menu(client):
    listing = command(client, 'MENU open')
    for _ in range(8):
        if '\nMESSAGE ' in listing:
            listing = command(client, 'MENU b')
            continue
        options = rows(listing)
        if 'PyroWave LAN Discovery' in options: return listing
        if 'PyroWave Devices' in options:
            listing = choose(client, 'PyroWave Devices')
            continue
        if 'Quick Menu' in options:
            listing = choose(client, 'Quick Menu')
        else:
            listing = command(client, 'MENU b')
    raise AssertionError('Cannot reach PyroWave menu: ' + listing)

def text(client):
    return client['path'].read_text(encoding='utf-8', errors='replace')

def close_client(client):
    if os.name != 'nt':
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.sendto(b'QUIT\n', ('127.0.0.1', client['port']))
        return
    # Use the owned window's close event, as the native receiver oracle does.
    import ctypes as c
    from ctypes import wintypes as w
    callback_type = c.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
    get_pid = c.windll.user32.GetWindowThreadProcessId
    get_pid.argtypes = [w.HWND, c.POINTER(w.DWORD)]
    post = c.windll.user32.PostMessageW
    post.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
    @callback_type
    def visit(window, unused):
        owner = w.DWORD()
        get_pid(window, c.byref(owner))
        if owner.value == client['process'].pid:
            post(window, 0x10, 0, 0)
        return True
    c.windll.user32.EnumWindows(visit, 0)

try:
    for index in range(2):
        name = 'PyroWave-Test-' + str(os.getpid()) + '-' + str(index)
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.bind(('127.0.0.1', 0)); port = s.getsockname()[1]
        shots = a.output / (name + '-frames'); shots.mkdir(exist_ok=True)
        config = a.output / (name + '.cfg')
        config.write_text('\n'.join([
            'video_driver = "' + a.backend + '"', 'video_threaded = "' + ('true' if a.threaded else 'false') + '"',
            'audio_driver = "' + a.audio_driver + '"', 'video_vsync = "true"', 'video_fullscreen = "false"',
            'pause_nonactive = "false"', 'confirm_quit = "false"', 'menu_pause_libretro = "false"',
            'menu_driver = "rgui"', 'menu_show_start_screen = "false"',
            'rgui_browser_directory = "' + a.output.resolve().as_posix() + '"',
            'multimedia_builtin_mediaplayer_enable = "true"', 'menu_thumbnail_preview_audio = "true"',
            'menu_ignore_missing_assets = "true"', 'video_window_custom_size_enable = "true"',
            'video_scale = "2"', 'video_windowed_position_width = "640"', 'video_windowed_position_height = "480"',
            'config_save_on_exit = "false"', 'video_shader_enable = "false"', 'video_gpu_screenshot = "true"',
            'notification_show_screenshot = "false"', 'screenshot_directory = "' + shots.resolve().as_posix() + '"',
            'network_cmd_enable = "true"', 'network_cmd_bind_address = "127.0.0.1"',
            'network_cmd_port = "' + str(port) + '"', 'record_driver = "' + a.record_driver + '"',
            'pyrowave_discovery_enable = "false"', 'pyrowave_device_name = "' + name + '"',
            'pyrowave_shader_directory = "' + ('' if a.embedded_shaders else a.shader_directory.resolve().as_posix()) + '"',
            'assets_directory = "' + (a.output.resolve() / 'absent-assets').as_posix() + '"',
        ]) + '\n', encoding='utf-8')
        path = a.output / (name + '.log'); log = path.open('w', encoding='utf-8')
        startup = None
        if os.name == 'nt':
            startup = subprocess.STARTUPINFO(); startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW; startup.wShowWindow = 0
        launch = [str(a.retroarch.resolve()), '--verbose', '--config', str(config.resolve())]
        if not a.frontend_only or index: launch += ['-L', str(a.core.resolve())]
        process = subprocess.Popen(launch, stdout=log, stderr=log, startupinfo=startup)
        client = dict(name=name, port=port, process=process, log=log, path=path, shots=shots)
        clients.append(client)
        wait(lambda: ('[NetCMD]' in text(client) if a.frontend_only and not index
                       else 'FRONTEND_FIXTURE_RUN' in text(client)), 'Frontend never became ready')
        recording_menu(client)
        assert not any(label.startswith('Send to ') for label in rows(command(client, 'MENU show from 0')))
        choose(client, 'PyroWave LAN Discovery')
        command(client, 'MENU close')
    sender, receiver = clients
    recording_menu(sender)
    wait(lambda: ('Send to ' + receiver['name']) in rows(command(sender, 'MENU show from 0')),
         'Receiver not discovered')
    for accept in (False, True):
        recording_menu(sender)
        choose(sender, 'Send to ' + receiver['name'])
        recording_menu(receiver)
        wait(lambda: 'Accept Incoming PyroWave Stream' in rows(command(receiver, 'MENU show from 0')),
             'Incoming request not shown')
        assert 'Receiver ready.' not in text(receiver), 'Receiver accepted without consent'
        assert 'Stream ready.' not in text(sender), 'Sender started before consent'
        assert 'FRONTEND_FIXTURE_DEINIT' not in text(receiver), 'Receiver unloaded the game'
        if not accept:
            choose(receiver, 'Decline Incoming PyroWave Stream')
            wait(lambda: 'Stream resources retired.' in text(sender), 'Declined sender did not retire')
            time.sleep(2)  # Allow the replacement listener's mDNS probe/announcement.
            wait(lambda: ('Send to ' + receiver['name']) in rows(command(sender, 'MENU open')),
                 'Declined receiver did not become available again')
        else:
            choose(receiver, 'Accept Incoming PyroWave Stream')
            wait(lambda: 'Receiver ready.' in text(receiver) and 'Stream ready.' in text(sender),
                 'Accepted stream did not become ready')
            time.sleep(2)
            if a.pixels:
                from PIL import ImageChops, ImageStat
                recording_menu(sender)
                time.sleep(2)
                first = screenshot(receiver)
                command(sender, 'MENU b')
                time.sleep(2)
                second = screenshot(receiver)
                difference = sum(ImageStat.Stat(ImageChops.difference(first, second)).mean) / 3
                assert difference > .5, ('Menu was not present in received pixels', difference)
            if a.transition:
                fixture = a.output / 'transition.bin'; fixture.write_bytes(b'\x00')
                send(sender, 'LOAD_CONTENT ' + str(a.core.resolve()) + '|' + str(fixture.resolve()))
                wait(lambda: 'Frontend capture rebound.' in text(sender), 'Capture did not rebind after loading content')
                assert 'Closing stream.' not in text(sender).split('Stream ready.', 1)[1], 'Content load closed the stream'
                assert text(receiver).count('Receiver ready.') == 1, 'Content load renegotiated the receiver'
                time.sleep(2)
                count = text(sender).count('Frontend capture rebound.')
                send(sender, 'UNLOAD_CORE')
                wait(lambda: text(sender).count('Frontend capture rebound.') > count,
                     'Capture did not rebind after returning to the main menu')
                assert text(receiver).count('Receiver ready.') == 1
            if a.resize:
                for fullscreen in (True, False):
                    count = text(sender).count('Frontend capture rebound.')
                    if os.name == 'nt':
                        # Resize only this hidden test window; fullscreen can minimize
                        # an occluded window and suspend presentation on Windows.
                        import ctypes as c
                        from ctypes import wintypes as w
                        windows = []
                        get_pid = c.windll.user32.GetWindowThreadProcessId
                        get_pid.argtypes = [w.HWND, c.POINTER(w.DWORD)]
                        get_class = c.windll.user32.GetClassNameW
                        get_class.argtypes = [w.HWND, w.LPWSTR, c.c_int]
                        callback_type = c.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
                        def owned_window(hwnd, _):
                            pid = w.DWORD()
                            get_pid(hwnd, c.byref(pid))
                            name = c.create_unicode_buffer(128)
                            get_class(hwnd, name, len(name))
                            if pid.value == sender['process'].pid and name.value == 'RetroArch':
                                windows.append(hwnd)
                            return True
                        c.windll.user32.EnumWindows(callback_type(owned_window), 0)
                        assert windows, 'Sender window not found'
                        resize = c.windll.user32.SetWindowPos
                        resize.argtypes = [w.HWND, w.HWND, c.c_int, c.c_int, c.c_int, c.c_int, w.UINT]
                        assert resize(windows[0], None, 0, 0, 800 if fullscreen else 640,
                                      600 if fullscreen else 480, 0x16)
                    else:
                        send(sender, 'FULLSCREEN_TOGGLE')
                    wait(lambda: text(sender).count('Frontend capture rebound.') > count,
                         'Capture did not rebind after resizing')
                    assert 'Closing stream.' not in text(sender).split('Stream ready.', 1)[1]
                    if a.pixels:
                        resized = screenshot(receiver)
                        assert text(receiver).count('Receiver ready.') == 1, 'Resize renegotiated wire dimensions'
            if a.media:
                import shutil
                target = a.output / ('stream-media' + a.media.suffix)
                shutil.copyfile(a.media, target)
                listing = command(sender, 'MENU open')
                for _ in range(10):
                    if 'Load Content' in rows(listing): break
                    listing = command(sender, 'MENU b')
                choose(sender, 'Load Content')
                listing = command(sender, 'MENU show from 0')
                if 'Start Directory' in rows(listing): listing = choose(sender, 'Start Directory')
                choose(sender, target.name)
                time.sleep(3)
                assert 'Closing stream.' not in text(sender).split('Stream ready.', 1)[1]
                if a.pixels:
                    played = screenshot(receiver)
                    difference = sum(ImageStat.Stat(ImageChops.difference(first, played)).mean) / 3
                    assert difference > .5, ('Built-in player was not streamed', difference)
                    raw = played.tobytes()
                    pixels = list(zip(raw[0::3], raw[1::3], raw[2::3]))
                    for channel in range(3):
                        count = sum(pixel[channel] > 180 and all(pixel[c] < 80 for c in range(3) if c != channel)
                                    for pixel in pixels)
                        assert count > 50, ('Movie color bars missing from received pixels', channel, count)
                    time.sleep(1)
                    moving = screenshot(receiver)
                    motion = sum(ImageStat.Stat(ImageChops.difference(played, moving)).mean) / 3
                    assert motion > .05, ('Built-in movie frames stopped updating', motion)
                command(sender, 'MENU b')
            recording_menu(sender)
            choose(sender, 'Stop PyroWave Streaming')
            wait(lambda: 'Frontend session restored.' in text(receiver), 'Game session not restored')
            assert text(receiver).count('FRONTEND_FIXTURE_INIT\n') == 1
            assert 'FRONTEND_FIXTURE_DEINIT' not in text(receiver)
    listing = recording_menu(sender)
    values = {line.split('\t')[1]: line.split('\t')[2] for line in listing.splitlines()
              if re.match(r'^\d+\t', line)}
    assert values.get('Record') == a.record_driver, 'LAN sender changed the recording driver setting'
    recording_menu(receiver)
    choose(receiver, 'PyroWave LAN Discovery')
    wait(lambda: ('Send to ' + receiver['name']) not in rows(command(sender, 'MENU show from 0')),
         'Opted-out receiver is still advertised')
finally:
    for client in clients:
        if client['process'].poll() is None:
            try:
                close_client(client)
                client['process'].wait(timeout=15)
            except subprocess.TimeoutExpired:
                client['process'].terminate()
                try: client['process'].wait(timeout=10)
                except subprocess.TimeoutExpired:
                    client['process'].kill(); client['process'].wait(timeout=10)
        client['log'].close()

for client in clients:
    assert client['process'].returncode == 0, ('Frontend did not exit cleanly', client['name'])
    diagnostics = text(client)
    assert 'Validation Error:' not in diagnostics and 'SYNC-HAZARD-' not in diagnostics
    assert '[D3D12 oracle]' not in diagnostics
    if a.require_d3d12_validation:
        assert '[PyroWave oracle] D3D12 GPU validation enabled.' in diagnostics
        counts = re.findall(r'D3D12 validation errors: (\d+)\.', diagnostics)
        assert counts and all(int(count) == 0 for count in counts), counts
print('PASS: discovery, consent, reconnect, retained core and clean frontend shutdown',
      'menu/movie pixels' if a.pixels else '', 'renderer handoff' if a.transition or a.resize else '')
