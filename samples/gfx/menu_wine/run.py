#!/usr/bin/env python3
"""The menus drawn by each Direct3D driver, compared under Wine.

Starts retroarch.exe with no content for each menu driver on each video
driver named, on an Xvfb of its own, reads the screen once two reads in
a row agree and compares it with the first driver's, block by block:
the mean colour of each block in a grid has to match. A solid quad
drawn with the wrong texture or none - a background, a sidebar, a
selection box, a separator gone dark or transparent - moves whole
blocks, and so does a gradient split along the other diagonal, where
the drivers' own differences in text and edge filtering stay small
(under 5 here, against 52 and 23 for those two faults on d3d9_hlsl).

Clocks, the XMB ribbon and the widgets are off, so the same menu is
drawn every time. A screen holds still only once something is on it:
the first launch in a fresh Wine prefix can take longer than the
settle time to draw its first frame, and two blank reads agree. GDI
is left out: its software quads only come near a gradient.

The screen is 640x480 so RGUI's framebuffer is scaled by a whole
number: at a fraction, GL and D3D round its rows differently. d3d9_cg
needs the Cg runtime, which a mingw-w64 build has not got.

Usage: run.py <retroarch.exe> <output dir> <driver> [driver ...]
RA_MENU_ONLY, a regular expression, runs only the menus it finds.
Each case leaves its screen and log in <output dir>/<menu>/<driver>.
(Windows-wine-menu.yml)
"""

import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import zlib

W, H = 640, 480
GRID_X, GRID_Y = 16, 12
# Largest difference allowed in a block's mean, any channel, out of 255
LIMIT = 10
# Seconds before the first read, and before giving up on a steady one
SETTLE = 4.0
STEADY = 60.0
MENUS = ('ozone', 'xmb', 'materialui', 'rgui')


def read_png(path):
    """8-bit RGB or RGBA, non-interlaced."""
    with open(path, 'rb') as f:
        data = f.read()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('not a PNG')
    pos, idat = 8, b''
    w = h = ct = bd = None
    while pos < len(data):
        ln, = struct.unpack('>I', data[pos:pos + 4])
        typ = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + ln]
        pos += 12 + ln
        if typ == b'IHDR':
            w, h, bd, ct = struct.unpack('>IIBB', body[:10])
        elif typ == b'IDAT':
            idat += body
        elif typ == b'IEND':
            break
    if bd != 8 or ct not in (2, 6):
        raise ValueError('unsupported PNG: depth %s type %s' % (bd, ct))
    bpp = 3 if ct == 2 else 4
    raw = zlib.decompress(idat)
    stride = w * bpp
    rows, prev, i = [], bytearray(stride), 0
    for _ in range(h):
        f = raw[i]
        line = bytearray(raw[i + 1:i + 1 + stride])
        i += 1 + stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1:
                line[x] = (line[x] + a) & 255
            elif f == 2:
                line[x] = (line[x] + b) & 255
            elif f == 3:
                line[x] = (line[x] + ((a + b) >> 1)) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + pr) & 255
        rows.append(line)
        prev = line
    return w, h, bpp, rows


def blocks(path):
    """The mean colour of each block of the grid, row by row."""
    w, h, bpp, rows = read_png(path)
    bw, bh = w // GRID_X, h // GRID_Y
    out = []
    for gy in range(GRID_Y):
        for gx in range(GRID_X):
            s = [0, 0, 0]
            for y in range(gy * bh, (gy + 1) * bh):
                line = rows[y]
                for x in range(gx * bw, (gx + 1) * bw):
                    o = x * bpp
                    s[0] += line[o]
                    s[1] += line[o + 1]
                    s[2] += line[o + 2]
            n = bw * bh
            out.append((s[0] / n, s[1] / n, s[2] / n))
    return out


def drawn(b):
    """Whether any block of @b is brighter than a blank screen."""
    return max(max(c) for c in b) >= 8


def winpath(v):
    return 'Z:' + v if v.startswith('/') else v


def write_cfg(path, d, driver, menu):
    cfg = {
        'video_driver': driver,
        'video_context_driver': '',
        'input_driver': 'dinput',
        'input_joypad_driver': 'null',
        'input_autodetect_enable': 'false',
        'menu_driver': menu,
        'video_fullscreen': 'true',
        'video_windowed_fullscreen': 'true',
        'video_threaded': 'false',
        'video_font_enable': 'false',
        'menu_enable_widgets': 'false',
        'menu_timedate_enable': 'false',
        'menu_battery_level_enable': 'false',
        'menu_core_enable': 'false',
        'menu_scale_factor': '1.000000',
        'menu_wallpaper': '',
        'menu_dynamic_wallpaper_enable': 'false',
        'menu_shader_pipeline': '0',
        'menu_ticker_type': '0',
        'menu_ticker_smooth': 'false',
        'menu_animation_speed': '1.000000',
        'xmb_animation_opening_main_menu': '0',
        'ozone_collapse_sidebar': 'false',
        'audio_enable': 'false',
        'audio_driver': 'null',
        'microphone_enable': 'false',
        'microphone_driver': 'null',
        'cheevos_enable': 'false',
        'pause_nonactive': 'false',
        'confirm_quit': 'false',
        'config_save_on_exit': 'false',
        'notification_show_config_override_load': 'false',
        'history_list_enable': 'false',
        'menu_show_load_content_animation': 'false',
        'assets_directory': d,
        'savefile_directory': d,
        'savestate_directory': d,
        'system_directory': d,
        'cache_directory': d,
        'log_dir': d,
        'log_to_file': 'true',
        'playlist_directory': d,
        'content_favorites_path': os.path.join(d, 'favorites.lpl'),
        'content_history_path': os.path.join(d, 'history.lpl'),
        'content_image_history_path': os.path.join(d, 'images.lpl'),
        'content_music_history_path': os.path.join(d, 'music.lpl'),
        'content_video_history_path': os.path.join(d, 'video.lpl'),
        'input_remapping_directory': d,
        'rgui_config_directory': d,
        'video_shader_dir': d,
        'thumbnails_directory': d,
        'core_assets_directory': d,
        'libretro_directory': d,
        'libretro_info_path': d,
    }
    with open(path, 'w') as f:
        for k in sorted(cfg):
            v = cfg[k]
            f.write('%s = "%s"\n' % (k, winpath(v) if os.path.isabs(v)
                                     else v))


def free_display():
    for n in range(90, 190):
        if not os.path.exists('/tmp/.X11-unix/X%d' % n) and \
                not os.path.exists('/tmp/.X%d-lock' % n):
            return n
    raise RuntimeError('no free X display')


def shoot(retroarch, root, driver, menu):
    """The screen with @menu drawn by @driver, or None."""
    d = os.path.join(root, menu, driver)
    os.makedirs(d, exist_ok=True)
    cfg = os.path.join(d, 'retroarch.cfg')
    write_cfg(cfg, d, driver, menu)
    shot = os.path.join(d, 'screen.png')
    env = dict(os.environ)
    n = free_display()
    xvfb = subprocess.Popen(['Xvfb', ':%d' % n, '-screen', '0',
                             '%dx%dx24' % (W, H), '-nolisten', 'tcp'],
                            stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL,
                            start_new_session=True)
    ra = None
    try:
        deadline = time.time() + 10
        while (time.time() < deadline
               and not os.path.exists('/tmp/.X11-unix/X%d' % n)):
            time.sleep(0.1)
        env['DISPLAY'] = ':%d' % n
        env['WINEDEBUG'] = '-all'
        with open(os.path.join(d, 'stdout.log'), 'w') as log:
            ra = subprocess.Popen(['wine', retroarch, '--config',
                                   winpath(cfg), '--menu'],
                                  stdout=log, stderr=subprocess.STDOUT,
                                  env=env, start_new_session=True)
            time.sleep(SETTLE)
            last, deadline = None, time.time() + STEADY
            while time.time() < deadline:
                if ra.poll() is not None:
                    return None
                if os.path.exists(shot):
                    os.remove(shot)
                subprocess.run(['import', '-display', env['DISPLAY'],
                                '-window', 'root', 'PNG24:' + shot],
                               stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL, timeout=30)
                if os.path.exists(shot):
                    b = blocks(shot)
                    if not drawn(b):
                        last = None
                    elif last is not None and compare(last, b)[0] <= 0.5:
                        return shot
                    else:
                        last = b
                time.sleep(1.0)
            print('note %s/%s: the menu never held still' % (menu, driver))
    finally:
        if ra and ra.poll() is None:
            ra.kill()
            ra.wait()
        subprocess.run(['wineserver', '-k'], env=env,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        xvfb.terminate()
        xvfb.wait()
        for f in ('/tmp/.X%d-lock' % n, '/tmp/.X11-unix/X%d' % n):
            if os.path.exists(f):
                os.remove(f)
    return shot if os.path.exists(shot) else None


def compare(ref, other):
    """The largest channel difference of any block, and where it is."""
    worst, at = 0.0, 0
    for i, (a, b) in enumerate(zip(ref, other)):
        diff = max(abs(a[0] - b[0]), abs(a[1] - b[1]), abs(a[2] - b[2]))
        if diff > worst:
            worst, at = diff, i
    return worst, (at % GRID_X, at // GRID_X)


def main():
    if len(sys.argv) < 4:
        sys.stderr.write(__doc__)
        return 2
    retroarch = os.path.abspath(sys.argv[1])
    root = os.path.abspath(sys.argv[2])
    drivers = sys.argv[3:]
    only = os.environ.get('RA_MENU_ONLY')
    failed = 0
    if os.path.isdir(root):
        shutil.rmtree(root)
    os.makedirs(root)
    for menu in MENUS:
        if only and not re.search(only, menu):
            continue
        ref = None
        for driver in drivers:
            shot = shoot(retroarch, root, driver, menu)
            if not shot:
                print('fail %s/%s: no screen' % (menu, driver))
                failed += 1
                continue
            b = blocks(shot)
            if ref is None:
                ref = (driver, b)
                # A blank screen is no reference
                if not drawn(b):
                    print('fail %s/%s: nothing drawn' % (menu, driver))
                    failed += 1
                else:
                    print('ref  %s/%s' % (menu, driver))
                continue
            worst, at = compare(ref[1], b)
            if worst > LIMIT:
                print('fail %s/%s: block %s differs from %s by %.1f'
                      % (menu, driver, at, ref[0], worst))
                failed += 1
            else:
                print('pass %s/%s (%.1f)' % (menu, driver, worst))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
