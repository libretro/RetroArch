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
p.add_argument('--shader-directory', type=Path, required=True)
p.add_argument('--backend', choices=('glcore', 'vulkan', 'd3d12'), default='glcore')
p.add_argument('--record-driver', default='null')
p.add_argument('--threaded', action='store_true')
p.add_argument('--audio-driver', default='null')
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
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
        s.settimeout(3)
        s.sendto((action + '\n').encode(), ('127.0.0.1', client['port']))
        result = s.recv(65536).decode('utf-8')
    with (a.output / (client['name'] + '-menu.log')).open('a', encoding='utf-8') as log:
        log.write(action + '\n' + result + '\n')
    return result

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

try:
    for index in range(2):
        name = 'PyroWave-Test-' + str(os.getpid()) + '-' + str(index)
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.bind(('127.0.0.1', 0)); port = s.getsockname()[1]
        config = a.output / (name + '.cfg')
        config.write_text('\n'.join([
            'video_driver = "' + a.backend + '"', 'video_threaded = "' + ('true' if a.threaded else 'false') + '"',
            'audio_driver = "' + a.audio_driver + '"', 'video_vsync = "true"', 'video_fullscreen = "false"',
            'pause_nonactive = "false"', 'menu_pause_libretro = "false"',
            'menu_driver = "rgui"', 'menu_show_start_screen = "false"',
            'config_save_on_exit = "false"', 'video_shader_enable = "false"',
            'network_cmd_enable = "true"', 'network_cmd_bind_address = "127.0.0.1"',
            'network_cmd_port = "' + str(port) + '"', 'record_driver = "' + a.record_driver + '"',
            'pyrowave_discovery_enable = "false"', 'pyrowave_device_name = "' + name + '"',
            'pyrowave_shader_directory = "' + a.shader_directory.resolve().as_posix() + '"',
        ]) + '\n', encoding='utf-8')
        path = a.output / (name + '.log'); log = path.open('w', encoding='utf-8')
        startup = None
        if os.name == 'nt':
            startup = subprocess.STARTUPINFO(); startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW; startup.wShowWindow = 0
        process = subprocess.Popen([str(a.retroarch.resolve()), '--verbose', '--config', str(config.resolve()),
                                    '-L', str(a.core.resolve())], stdout=log, stderr=log, startupinfo=startup)
        client = dict(name=name, port=port, process=process, log=log, path=path)
        clients.append(client)
        wait(lambda: 'FRONTEND_FIXTURE_RUN' in text(client), 'Core never ran')
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
    print('PASS: menu opt-in/discovery/withdrawal, driver override, decline/accept, reconnect, stop and retained core')
finally:
    for client in clients:
        if client['process'].poll() is None:
            try:
                with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
                    s.sendto(b'QUIT\n', ('127.0.0.1', client['port']))
                client['process'].wait(timeout=15)
            except subprocess.TimeoutExpired:
                client['process'].terminate(); client['process'].wait(timeout=10)
        client['log'].close()
