#!/usr/bin/env python3
"""The menu's backdrop blur, end to end, on the drivers that run here.

The shipping frontend opens a dialog over the menu - the first-run
download prompt, which a missing core info directory brings up, or the
on-screen keyboard of a search - on each driver and menu below, once
with Background Blur on and once off, under Xvfb. The window is read
back from the X server and the menu around the dialog measured: the
sum of the differences between neighbouring pixels, which text and
edges make large and a blur makes small.

  vulkan   lavapipe; with a VULKAN_DEBUG=1 build any validation error
           fails the case
  glcore   llvmpipe, core profile
  gl       llvmpipe, compatibility profile

Each pair must:
  - start, draw the dialog and quit cleanly, both times;
  - with the blur on, measure under a third of what it measures off,
    the menu behind blurred rather than dimmed;
  - with the blur off, keep detail: the menu behind still there, and
    a broken read-back not mistaken for a blur.

usage: run.py --retroarch ./retroarch --assets DIR [--only REGEX]
         [--log-dir DIR]
DIR is a retroarch-assets checkout. Needs Xvfb and libX11/libXtst.
Exits 77 when Xvfb or the assets are missing.
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
LVP = '/usr/share/vulkan/icd.d/lvp_icd.json'
# Ozone and XMB take the search on X (the 's' key) after moving into
# Settings; the menu takes the keys one frame or more apart
KEYS_SEARCH = [0xff51, 0xff54, 0xff53, 0x73]

# name, driver, menu, dialog
CASES = [
    ('vulkan-ozone-msgbox', 'vulkan', 'ozone', 'msgbox'),
    ('vulkan-ozone-keyboard', 'vulkan', 'ozone', 'keyboard'),
    ('vulkan-xmb-msgbox', 'vulkan', 'xmb', 'msgbox'),
    ('glcore-ozone-msgbox', 'glcore', 'ozone', 'msgbox'),
    ('glcore-xmb-msgbox', 'glcore', 'xmb', 'msgbox'),
    ('gl-ozone-keyboard', 'gl', 'ozone', 'keyboard'),
    ('gl-xmb-msgbox', 'gl', 'xmb', 'msgbox'),
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


class X:
    """The display, read back and typed into"""

    def __init__(self, display):
        self.x = ctypes.CDLL('libX11.so.6')
        self.t = ctypes.CDLL('libXtst.so.6')
        self.x.XOpenDisplay.restype = ctypes.c_void_p
        self.x.XOpenDisplay.argtypes = [ctypes.c_char_p]
        self.x.XDefaultRootWindow.restype = ctypes.c_ulong
        self.x.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
        self.x.XGetImage.restype = ctypes.POINTER(XImage)
        self.x.XGetImage.argtypes = [ctypes.c_void_p, ctypes.c_ulong,
                                     ctypes.c_int, ctypes.c_int,
                                     ctypes.c_uint, ctypes.c_uint,
                                     ctypes.c_ulong, ctypes.c_int]
        self.x.XFree.argtypes = [ctypes.c_void_p]
        self.x.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
        self.x.XFlush.argtypes = [ctypes.c_void_p]
        self.t.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint,
                                             ctypes.c_int, ctypes.c_ulong]
        self.d = self.x.XOpenDisplay(display.encode())
        if not self.d:
            raise RuntimeError('cannot open %s' % display)

    def grey(self):
        """The window's corner of the screen, as rows of grey levels"""
        img = self.x.XGetImage(self.d, self.x.XDefaultRootWindow(self.d),
                               0, 0, W, H, 0xffffffff, 2)
        if not img:
            return None
        im = img.contents
        rows = []
        if im.bits_per_pixel == 32:
            data = ctypes.string_at(ctypes.cast(im.data, ctypes.c_void_p).value,
                                    im.bytes_per_line * H)
            for y in range(H):
                o = y * im.bytes_per_line
                rows.append([(data[i] * 29 + data[i + 1] * 150
                              + data[i + 2] * 77) >> 8
                             for i in range(o, o + W * 4, 4)])
        self.x.XFree(im.data)
        self.x.XFree(img)
        return rows if rows else None

    def keys(self, syms):
        for sym in syms:
            code = self.x.XKeysymToKeycode(self.d, sym)
            self.t.XTestFakeKeyEvent(self.d, code, 1, 0)
            self.x.XFlush(self.d)
            time.sleep(0.15)
            self.t.XTestFakeKeyEvent(self.d, code, 0, 0)
            self.x.XFlush(self.d)
            time.sleep(0.6)


def detail(rows, y0, y1):
    """The sum of the differences between neighbouring pixels in rows
    y0 .. y1, per pixel"""
    total = 0
    for y in range(y0, y1 - 1):
        r, n = rows[y], rows[y + 1]
        for x in range(W - 1):
            total += abs(r[x + 1] - r[x]) + abs(n[x] - r[x])
    return total / float((y1 - y0 - 1) * (W - 1))


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


def write_cfg(path, d, assets, driver, menu, dialog, blur):
    cfg = {
        'video_driver': driver,
        'video_context_driver': 'vk_x' if driver == 'vulkan' else 'x',
        'video_threaded': 'false',
        'video_fullscreen': 'false',
        'video_scale': '2',
        'video_vsync': 'false',
        'video_font_enable': 'false',
        'menu_driver': menu,
        'menu_backdrop_blur': 'true' if blur else 'false',
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
        'assets_directory': str(assets),
        'joypad_autoconfig_dir': str(assets),
        'content_database_path': str(assets),
        # Missing, it brings up the first-run prompt; there, it does not
        'libretro_info_path': str(d / 'missing') if dialog == 'msgbox'
        else str(assets),
    }
    for k in ('savefile_directory', 'savestate_directory', 'system_directory',
              'cache_directory', 'screenshot_directory', 'playlist_directory',
              'thumbnails_directory'):
        cfg[k] = str(d)
    with open(path, 'w') as f:
        for k in sorted(cfg):
            f.write('%s = "%s"\n' % (k, cfg[k]))


def run_one(exe, d, display, x, assets, driver, menu, dialog, blur):
    """One run: its log, and the screen once the dialog is up"""
    d.mkdir(parents=True, exist_ok=True)
    cfg = d / 'retroarch.cfg'
    write_cfg(cfg, d, assets, driver, menu, dialog, blur)
    env = dict(os.environ, DISPLAY=display, XDG_CONFIG_HOME=str(d),
               HOME=str(d))
    env.pop('WAYLAND_DISPLAY', None)
    if driver == 'vulkan' and not env.get('VK_DRIVER_FILES') \
            and os.path.exists(LVP):
        env['VK_DRIVER_FILES'] = LVP
    if driver != 'vulkan':
        env['LIBGL_ALWAYS_SOFTWARE'] = '1'
    p = subprocess.Popen([exe, '-v', '--menu', '--config', str(cfg)],
                         cwd=d, env=env, stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    rows = None
    try:
        time.sleep(5)
        if dialog == 'keyboard':
            x.keys(KEYS_SEARCH)
            time.sleep(2)
        rows = x.grey()
        p.stdin.write(b'QUIT\n')
        p.stdin.flush()
        out, _ = p.communicate(timeout=60)
    except (subprocess.TimeoutExpired, BrokenPipeError):
        p.kill()
        out, _ = p.communicate()
    return p.returncode, out.decode(errors='replace'), rows


def main():
    a = argparse.ArgumentParser()
    a.add_argument('--retroarch', required=True)
    a.add_argument('--assets', default=os.environ.get('RA_ASSETS', ''))
    a.add_argument('--only', default='')
    a.add_argument('--log-dir', default='')
    a = a.parse_args()
    assets = Path(a.assets).resolve() if a.assets else None
    if not shutil.which('Xvfb'):
        print('backdrop_blur: no Xvfb, skipping')
        return 77
    if not assets or not (assets / 'ozone').is_dir():
        print('backdrop_blur: no retroarch-assets (--assets), skipping')
        return 77
    exe = str(Path(a.retroarch).resolve())
    work = Path(tempfile.mkdtemp(prefix='backdrop-blur-'))
    logs = Path(a.log_dir).resolve() if a.log_dir else work
    xvfb, display = xvfb_start()
    failed = 0
    try:
        x = X(display)
        for name, driver, menu, dialog in CASES:
            if a.only and not re.search(a.only, name):
                continue
            # The menu around the dialog: above it, or above the
            # keyboard's text box
            band = (0, 120) if dialog == 'msgbox' else (0, 36)
            seen = {}
            errors = []
            for blur in (True, False):
                tag = '%s-%s' % (name, 'on' if blur else 'off')
                rc, log, rows = run_one(exe, work / tag, display, x, assets,
                                        driver, menu, dialog, blur)
                logs.mkdir(parents=True, exist_ok=True)
                (logs / ('%s.log' % tag)).write_text(log)
                if rc != 0:
                    errors.append('%s: exit status %d' % (tag, rc))
                if 'Validation Error' in log:
                    errors.append('%s: %d validation error(s)'
                                  % (tag, log.count('Validation Error')))
                if not rows:
                    errors.append('%s: the screen could not be read' % tag)
                    continue
                seen[blur] = detail(rows, band[0], band[1])
            if True in seen and False in seen:
                on, off = seen[True], seen[False]
                if off < 0.2:
                    errors.append('blur off: no detail behind the dialog '
                                  '(%.2f) - nothing drawn, or not read' % off)
                elif on >= off / 3.0:
                    errors.append('blur on: %.2f against %.2f off - the '
                                  'menu behind was not blurred' % (on, off))
                if errors:
                    errors.append('detail %.2f blurred, %.2f not' % (on, off))
            if errors:
                failed += 1
                print('[FAIL] %s' % name)
                for e in errors:
                    print('       %s' % e)
            else:
                print('[pass] %s: detail %.2f blurred, %.2f not'
                      % (name, seen[True], seen[False]))
    finally:
        xvfb.kill()
        xvfb.wait()
    if failed:
        print('backdrop_blur: %d case(s) failed; logs in %s' % (failed, logs))
        return 1
    print('backdrop_blur: all cases passed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
