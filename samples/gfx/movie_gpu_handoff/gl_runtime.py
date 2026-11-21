#!/usr/bin/env python3
"""glcore and gl showing the WebM player's frames from their planes.

usage: gl_runtime.py --retroarch ./retroarch [--only REGEX]

A clip red above and blue below plays under Xvfb on each GL driver,
with and without the threaded wrapper. Each run must log that the
driver converted the movie's planes and show both halves, the right way
up, in the window. Needs a build configured with --enable-webmplayer,
Xvfb, libX11 and an ffmpeg with libvpx-vp9; exits 77 without them.
"""
import argparse
import ctypes
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time

W, H = 640, 480
# name, driver, threaded, log line
CASES = [
    ('glcore', 'glcore', False, '[GLCore] Movie frames of 320x240'),
    ('glcore-threaded', 'glcore', True, '[GLCore] Movie frames of 320x240'),
    ('gl', 'gl', False, '[GL] Movie frames of 320x240'),
    ('gl-threaded', 'gl', True, '[GL] Movie frames of 320x240'),
]


class XImage(ctypes.Structure):
    _fields_ = [('width', ctypes.c_int), ('height', ctypes.c_int),
                ('xoffset', ctypes.c_int), ('format', ctypes.c_int),
                ('data', ctypes.POINTER(ctypes.c_ubyte)),
                ('byte_order', ctypes.c_int), ('bitmap_unit', ctypes.c_int),
                ('bitmap_bit_order', ctypes.c_int),
                ('bitmap_pad', ctypes.c_int), ('depth', ctypes.c_int),
                ('bytes_per_line', ctypes.c_int),
                ('bits_per_pixel', ctypes.c_int)]


def pixel(display, px, py):
    """The screen's red, green, blue at px, py"""
    x = ctypes.CDLL('libX11.so.6')
    x.XOpenDisplay.restype = ctypes.c_void_p
    x.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x.XDefaultRootWindow.restype = ctypes.c_ulong
    x.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
    x.XGetImage.restype = ctypes.POINTER(XImage)
    x.XGetImage.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int,
                            ctypes.c_int, ctypes.c_uint, ctypes.c_uint,
                            ctypes.c_ulong, ctypes.c_int]
    x.XFree.argtypes = [ctypes.c_void_p]
    x.XCloseDisplay.argtypes = [ctypes.c_void_p]
    d = x.XOpenDisplay(display.encode())
    if not d:
        raise RuntimeError('cannot open %s' % display)
    img = x.XGetImage(d, x.XDefaultRootWindow(d), px, py, 1, 1,
                      0xffffffff, 2)
    im = img.contents
    data = ctypes.string_at(ctypes.cast(im.data, ctypes.c_void_p).value, 4)
    x.XFree(im.data)
    x.XFree(img)
    x.XCloseDisplay(d)
    return data[2], data[1], data[0]


def xvfb_start():
    for n in range(150, 220):
        if os.path.exists('/tmp/.X%d-lock' % n):
            continue
        p = subprocess.Popen(['Xvfb', ':%d' % n, '-screen', '0',
                              '%dx%dx24' % (W, H), '-nolisten', 'tcp',
                              '-noreset'],
                             stdout=subprocess.DEVNULL,
                             stderr=subprocess.DEVNULL)
        for _ in range(100):
            if p.poll() is not None:
                break
            if os.path.exists('/tmp/.X11-unix/X%d' % n):
                return p, ':%d' % n
            time.sleep(0.1)
        p.kill()
        p.wait()
    raise RuntimeError('no Xvfb display came up')


def write_cfg(path, d, driver, threaded):
    cfg = {
        'video_driver': driver,
        'video_context_driver': 'x',
        'video_threaded': 'true' if threaded else 'false',
        'video_fullscreen': 'false',
        'video_scale': '2',
        'video_vsync': 'false',
        'video_font_enable': 'false',
        'menu_show_start_screen': 'false',
        'input_driver': 'x',
        'input_joypad_driver': 'null',
        'input_autodetect_enable': 'false',
        'audio_enable': 'false',
        'audio_driver': 'null',
        'microphone_enable': 'false',
        'camera_driver': 'null',
        'pause_nonactive': 'false',
        'confirm_quit': 'false',
        'config_save_on_exit': 'false',
        'stdin_cmd_enable': 'true',
        'network_cmd_enable': 'false',
        'cheevos_enable': 'false',
        'history_list_enable': 'false',
    }
    for k in ('savefile_directory', 'savestate_directory', 'system_directory',
              'cache_directory', 'screenshot_directory', 'playlist_directory',
              'thumbnails_directory', 'assets_directory',
              'libretro_info_path'):
        cfg[k] = str(d)
    with open(path, 'w') as f:
        for k in sorted(cfg):
            f.write('%s = "%s"\n' % (k, cfg[k]))


def run_one(exe, d, display, clip, driver, threaded):
    """One run: its log, and the two halves' colours mid-clip"""
    d.mkdir(parents=True, exist_ok=True)
    cfg = d / 'retroarch.cfg'
    write_cfg(cfg, d, driver, threaded)
    env = dict(os.environ, DISPLAY=display, XDG_CONFIG_HOME=str(d),
               HOME=str(d), LIBGL_ALWAYS_SOFTWARE='1')
    env.pop('WAYLAND_DISPLAY', None)
    p = subprocess.Popen([exe, '-v', '-L', 'webm', '--config', str(cfg),
                          str(clip)], cwd=d, env=env, stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    top = bottom = None
    try:
        time.sleep(3)
        top, bottom = pixel(display, W // 2, H // 4), \
            pixel(display, W // 2, H * 3 // 4)
        p.stdin.write(b'QUIT\n')
        p.stdin.flush()
        out, _ = p.communicate(timeout=60)
    except (subprocess.TimeoutExpired, BrokenPipeError):
        p.kill()
        out, _ = p.communicate()
    return out.decode(errors='replace'), top, bottom


def main():
    a = argparse.ArgumentParser()
    a.add_argument('--retroarch', required=True)
    a.add_argument('--only', default='')
    a = a.parse_args()
    if not shutil.which('Xvfb') or not shutil.which('ffmpeg'):
        print('gl_runtime: no Xvfb or ffmpeg, skipping')
        return 77
    exe = str(Path(a.retroarch).resolve())
    failures = 0
    with tempfile.TemporaryDirectory() as directory:
        tmp = Path(directory)
        clip = tmp / 'clip.webm'
        if subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error',
                           '-y', '-f', 'lavfi', '-i',
                           'color=red:s=320x120:d=6:r=30[a];'
                           'color=blue:s=320x120:d=6:r=30[b];[a][b]vstack',
                           '-c:v', 'libvpx-vp9', '-pix_fmt', 'yuv420p',
                           str(clip)]).returncode:
            print('gl_runtime: ffmpeg cannot write VP9, skipping')
            return 77
        xvfb, display = xvfb_start()
        try:
            for name, driver, threaded, line in CASES:
                if a.only and not re.search(a.only, name):
                    continue
                out, top, bottom = run_one(exe, tmp / name, display, clip,
                                           driver, threaded)
                bad = []
                if line not in out:
                    bad.append('no "%s" in the log' % line)
                if not top or not (top[0] > 180 and top[2] < 80):
                    bad.append('top %s is not red' % (top,))
                if not bottom or not (bottom[2] > 180 and bottom[0] < 80):
                    bad.append('bottom %s is not blue' % (bottom,))
                print('%-16s %s' % (name, 'PASS' if not bad
                                    else 'FAIL: ' + '; '.join(bad)))
                failures += bool(bad)
        finally:
            xvfb.kill()
            xvfb.wait()
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
