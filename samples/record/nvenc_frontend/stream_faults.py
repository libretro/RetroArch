#!/usr/bin/env python3
"""Native recorder failure/cancel recovery on this test's isolated desktop."""
import argparse
from contextlib import ExitStack
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import re
import socket
import subprocess
import time
from stream import port, wait_until


def listening(process, number):
    # Check the owned receiver's listener without consuming its RTMP accept.
    api = ctypes.WinDLL('iphlpapi').GetExtendedTcpTable
    api.argtypes = [ctypes.c_void_p, ctypes.POINTER(wintypes.DWORD), wintypes.BOOL,
                    wintypes.DWORD, wintypes.DWORD, wintypes.DWORD]
    api.restype = wintypes.DWORD
    size = wintypes.DWORD()
    result = api(None, ctypes.byref(size), False, 2, 3, 0)
    assert result in (0, 122), result
    assert 4 <= size.value <= 4 * 1024 * 1024, size.value
    data = ctypes.create_string_buffer(size.value)
    result = api(data, ctypes.byref(size), False, 2, 3, 0)
    if result == 122:
        return False
    assert result == 0, result
    rows = (size.value - 4) // 24
    count = int.from_bytes(data.raw[:4], 'little')
    assert count <= rows
    for index in range(count):
        row = data.raw[4 + 24 * index:28 + 24 * index]
        if int.from_bytes(row[20:24], 'little') == process.pid:
            if socket.ntohs(int.from_bytes(row[8:12], 'little') & 65535) == number:
                return True
    return False


def retire(child):
    if child.poll() is None:
        child.kill()
    child.wait(timeout=10)


def check(args, codec, fault, threaded):
    root = args.output_dir.resolve() / f'{codec}-{fault}-{"threaded" if threaded else "direct"}'
    root.mkdir(parents=True, exist_ok=False)
    command_port = port(socket.SOCK_DGRAM)
    log_path, received = root / 'frontend.log', root / 'recovered.flv'
    native = root / 'stream.cfg'
    native.write_text(f'audio_enable = true\nvcodec = "{codec}"\nformat = "flv"\nvideo_qp = 24\n')
    children = []
    with ExitStack() as stack, log_path.open('wb') as log:
        keeper = stack.enter_context(socket.socket())
        keeper.bind(('127.0.0.1', 0))
        stream_port = keeper.getsockname()[1]
        url = f'rtmp://127.0.0.1:{stream_port}/live/test'
        config = root / 'retroarch.cfg'
        config.write_text(f'''video_driver = "vulkan"
video_threaded = "{str(threaded).lower()}"
video_fullscreen = "false"
video_scale = "1.0"
video_vsync = "true"
video_swapchain_bit_depth = "1"
video_hdr_mode = "0"
video_record_game_only = "false"
video_record_allow_frame_drop = "true"
video_font_enable = "false"
record_driver = "vulkan"
video_stream_url = "{url}"
video_stream_config = "{native.as_posix()}"
audio_driver = "null"
audio_format_negotiation = "1"
audio_fastpath_s16 = "false"
menu_driver = "rgui"
menu_show_start_screen = "false"
history_list_enable = "false"
config_save_on_exit = "false"
pause_nonactive = "false"
quit_press_twice = "false"
savestate_auto_save = "false"
savestate_auto_load = "false"
network_cmd_enable = "true"
network_cmd_port = "{command_port}"
''')

        def receiver(path):
            output = stack.enter_context(path.with_suffix('.receiver.log').open('wb'))
            child = subprocess.Popen([args.ffmpeg, '-v', 'error', '-listen', '1', '-timeout', '15',
                                      '-copyts', '-analyzeduration', '500000', '-i', url,
                                      '-map', '0', '-c', 'copy', '-f', 'flv', str(path)],
                                     stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                     stderr=output, creationflags=subprocess.CREATE_NO_WINDOW)
            children.append(child)
            stack.callback(retire, child)
            wait_until(lambda: listening(child, stream_port), child)
            return child

        if fault == 'silent':
            keeper.listen(1)
            keeper.setblocking(False)
        elif fault == 'disconnect':
            keeper.close()
            original = receiver(root / 'interrupted.flv')
        command = [str(args.retroarch.resolve()), '-v', '--config', str(config),
                   '-L', str(args.core.resolve()), '--max-frames', '36000']
        environment = dict(os.environ, NATIVE_RECORD_RATE='48000', NATIVE_RECORD_LAYOUT='3')
        process = subprocess.Popen([str(args.desktop_runner.resolve()), subprocess.list2cmdline(command)],
                                   cwd=root, env=environment, stdin=subprocess.DEVNULL,
                                   stdout=log, stderr=subprocess.STDOUT,
                                   creationflags=subprocess.CREATE_NO_WINDOW)
        children.append(process)
        stack.callback(retire, process)
        try:
            with socket.socket(type=socket.SOCK_DGRAM) as client:
                client.settimeout(.1)
                target = ('127.0.0.1', command_port)

                def send(text):
                    client.sendto((text + '\n').encode(), target)

                def query(name):
                    send('GET_OPTION ' + name)
                    try:
                        answer = client.recv(65536).decode().strip()
                    except (socket.timeout, ConnectionResetError):
                        return None
                    prefix = name + '\t'
                    return answer[len(prefix):] if answer.startswith(prefix) else None

                def progress():
                    baseline = [None]

                    def moved():
                        value = query('frame_count')
                        if value is None:
                            return False
                        current = int(value)
                        if baseline[0] is None:
                            baseline[0] = current
                            return False
                        return current >= baseline[0] + 8

                    wait_until(moved, process)

                progress()
                send('SET_OPTION streaming on')
                wait_until(lambda: 'Starting native ' in log_path.read_text(errors='replace'), process)
                hello = bytearray()
                if fault == 'silent':
                    peer = [None]

                    def greeted():
                        try:
                            if peer[0] is None:
                                peer[0] = stack.enter_context(keeper.accept()[0])
                                peer[0].setblocking(False)
                            hello.extend(peer[0].recv(16384))
                        except BlockingIOError:
                            pass
                        return len(hello) >= 1537

                    wait_until(greeted, process)
                    assert hello[0] == 3
                    progress()
                    send('SET_OPTION streaming off')
                elif fault == 'disconnect':
                    wait_until(lambda: (root / 'interrupted.flv').exists(), process, seconds=30)
                    progress()
                    original.kill()  # Deliberately close only this test's receiver transport.
                    original.wait(timeout=5)
                wait_until(lambda: 'Recording failed (' in log_path.read_text(errors='replace'), process)
                progress()
                wait_until(lambda: query('streaming') == 'off', process)
                keeper.close()
                if fault == 'silent':
                    peer[0].close()
                recovered = receiver(received)
                send('SET_OPTION streaming on')
                wait_until(lambda: log_path.read_text(errors='replace').count('Starting native ') == 2, process)
                wait_until(lambda: received.exists(), process, seconds=30)
                time.sleep(1)
                send('SET_OPTION streaming off')
                wait_until(lambda: 'Recording finalized (' in log_path.read_text(errors='replace'), process)
                progress()

                def quitting():
                    if process.poll() is not None:
                        return True
                    send('QUIT')
                    return False

                wait_until(quitting, process, allow_exit=True)
            assert process.wait(timeout=15) == 0
            assert recovered.wait(timeout=15) == 0
        finally:
            for child in children:
                retire(child)
    text = log_path.read_text(errors='replace')
    assert text.count('Recording failed (') == 1
    assert re.search(r'Recording failed \(session \d+, file -?\d+, native 0\)', text)
    assert not re.search(r'VUID-|SYNC-HAZARD|Validation Error', text)
    assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in text
    final = re.search(r'Recording finalized \((\d+) GPU frames\)', text)
    assert final and int(final[1]) > 20
    info = json.loads(subprocess.check_output([args.ffprobe, '-v', 'error', '-show_streams',
                                              '-show_packets', '-of', 'json', str(received)], timeout=15))
    video = next(s for s in info['streams'] if s['codec_type'] == 'video')
    assert video['codec_name'] == codec
    packets = [p for p in info['packets'] if p['stream_index'] == video['index']]
    assert len(packets) == int(final[1]) and abs(float(packets[0]['pts_time'])) < .0011
    subprocess.run([args.ffmpeg, '-v', 'error', '-xerror', '-i', str(received),
                    '-map', '0:v:0', '-map', '0:a:0', '-fps_mode', 'passthrough', '-f', 'null', '-'],
                   check=True, timeout=20, capture_output=True)
    result = dict(driver="vulkan", codec=codec, fault=fault, threaded=threaded, recovered_frames=len(packets),
                  handshake_bytes=len(hello), validation_errors=0)
    (root / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    print('PASS native stream failure/cancellation and fresh-session recovery: ' + json.dumps(result), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--retroarch', required=True, type=Path)
    parser.add_argument('--core', required=True, type=Path)
    parser.add_argument('--desktop-runner', required=True, type=Path)
    parser.add_argument('--ffmpeg', default='ffmpeg')
    parser.add_argument('--ffprobe', default='ffprobe')
    parser.add_argument('--output-dir', required=True, type=Path)
    parser.add_argument('--codecs', nargs='+', choices=['h264', 'hevc'], default=['h264', 'hevc'])
    parser.add_argument('--faults', nargs='+', choices=['refused', 'silent', 'disconnect'],
                        default=['refused', 'silent', 'disconnect'])
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('native Windows GPU acceptance requires Windows')
    for codec in args.codecs:
        for fault in args.faults:
            for threaded in (False, True):
                check(args, codec, fault, threaded)
