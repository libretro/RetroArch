#!/usr/bin/env python3
"""glcore in a build without slang, end to end.

Built without slang (or with nothing to cross-compile slang with), glcore
draws through the GLSL shader backend, and makes the XMB effects from
their GLSL. Before, the window contexts offered glcore slang only, so
such a build had no shader backend at all and drew nothing - not the
menu, not a frame - and glcore made the effects from their SPIR-V only.

retroarch, built without slang, runs on an Xvfb screen of its own with
Mesa's software rasteriser, and the screen is read with ImageMagick's
import. XMB is shot still (no background effect), with the Ribbon (its
glow on) and with Bokeh. Then:

- the still menu is drawn: its gradient is on the screen, not black,
  and glcore says it uses the GLSL backend;
- each effect shows: some of the screen differs from the still shot.

Usage: run.py <retroarch> <output dir>
Needs Xvfb and ImageMagick.
"""

import os
import shutil
import subprocess
import sys
import tempfile
import time

W, H = 640, 360
# The still menu's gradient: mean luma well clear of a black screen
MIN_STILL_LUMA = 30
# An effect shows: at least this share of the screen this much apart
# from the still shot
DIFFER_BY = 6
DIFFER_SHARE = 0.01

STILL = {'menu_shader_pipeline': '0', 'xmb_menu_shader_pipeline': '0'}
RIBBON = {'menu_shader_pipeline': '2', 'xmb_menu_shader_pipeline': '2',
          'xmb_ribbon_bloom_enable': 'true'}
BOKEH = {'menu_shader_pipeline': '5', 'xmb_menu_shader_pipeline': '5'}


def write_cfg(path, d, scene):
    cfg = {
        'video_driver': 'glcore',
        'video_context_driver': '',
        'input_joypad_driver': 'null',
        'input_autodetect_enable': 'false',
        'menu_driver': 'xmb',
        'xmb_theme': '0',
        'menu_timedate_enable': 'false',
        'menu_battery_level_enable': 'false',
        'menu_core_enable': 'false',
        'menu_mouse_enable': 'false',
        'menu_pointer_enable': 'false',
        'menu_enable_widgets': 'false',
        'video_fullscreen': 'true',
        'video_windowed_fullscreen': 'true',
        'video_font_enable': 'false',
        'video_threaded': 'false',
        'audio_enable': 'false',
        'audio_driver': 'null',
        'microphone_enable': 'false',
        'microphone_driver': 'null',
        'pause_nonactive': 'false',
        'confirm_quit': 'false',
        'config_save_on_exit': 'false',
        'rgui_show_start_screen': 'false',
        'history_list_enable': 'false',
        'log_to_file': 'false',
        'savefile_directory': d,
        'savestate_directory': d,
        'system_directory': d,
        'cache_directory': d,
        'playlist_directory': d,
        'thumbnails_directory': d,
        'assets_directory': d,
    }
    cfg.update(scene)
    with open(path, 'w') as f:
        for k in sorted(cfg):
            f.write('%s = "%s"\n' % (k, cfg[k]))


def grab(display):
    """The screen as raw RGB bytes, or None"""
    p = subprocess.run(['import', '-display', display, '-window', 'root',
                        '-depth', '8', 'rgb:-'], stdout=subprocess.PIPE,
                       stderr=subprocess.DEVNULL)
    if p.returncode or len(p.stdout) != W * H * 3:
        return None
    return p.stdout


def shoot(exe, out_dir, name, scene):
    """One shot once the menu is up, and the run's log"""
    d = tempfile.mkdtemp(prefix='ra-glcore-glsl-')
    cfg = os.path.join(d, 'retroarch.cfg')
    write_cfg(cfg, d, scene)
    display = ':%d' % (60 + os.getpid() % 30)
    xvfb = subprocess.Popen(['Xvfb', display, '-screen', '0',
                             '%dx%dx24' % (W, H)],
                            stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL)
    env = dict(os.environ, DISPLAY=display, LIBGL_ALWAYS_SOFTWARE='1')
    log_path = os.path.join(out_dir, name + '.log')
    ra = None
    shot = None
    try:
        time.sleep(1)
        with open(log_path, 'w') as log:
            ra = subprocess.Popen([exe, '--config', cfg, '--menu', '-v'],
                                  env=env, stdout=log,
                                  stderr=subprocess.STDOUT)
            # The still menu twice the same; the effects never settle,
            # so they are shot once the still one would have been
            deadline = time.monotonic() + 60
            last = None
            latest = None
            seen = 0
            while time.monotonic() < deadline and ra.poll() is None:
                time.sleep(1)
                cur = grab(display)
                if cur is None:
                    continue
                latest = cur
                seen += 1
                if scene is not STILL:
                    if seen >= 6:
                        shot = cur
                        break
                    continue
                if cur == last and seen >= 3:
                    shot = cur
                    break
                last = cur
            # A screen that never settles is judged as it last was
            if shot is None:
                shot = latest
    finally:
        if ra and ra.poll() is None:
            ra.kill()
            ra.wait()
        xvfb.kill()
        xvfb.wait()
        shutil.rmtree(d, ignore_errors=True)
    if shot:
        subprocess.run(['convert', '-size', '%dx%d' % (W, H), '-depth', '8',
                        'rgb:-', os.path.join(out_dir, name + '.png')],
                       input=shot)
    with open(log_path) as f:
        log = f.read()
    return shot, log


def luma(rgb):
    return [(rgb[i] * 54 + rgb[i + 1] * 183 + rgb[i + 2] * 19) >> 8
            for i in range(0, len(rgb), 3)]


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    exe = os.path.abspath(sys.argv[1])
    out_dir = os.path.abspath(sys.argv[2])
    os.makedirs(out_dir, exist_ok=True)
    fails = 0

    still, log = shoot(exe, out_dir, 'still', STILL)
    if not still:
        print('FAIL: no shot of the still menu (see still.log)')
        return 1
    if '[GLCore] Using GLSL shader backend' in log:
        print('PASS: glcore uses the GLSL shader backend')
    else:
        print('FAIL: glcore did not take the GLSL shader backend')
        fails += 1
    still_luma = luma(still)
    mean = sum(still_luma) / float(len(still_luma))
    if mean < MIN_STILL_LUMA:
        print('FAIL: the menu is not drawn (mean luma %.1f)' % mean)
        fails += 1
    else:
        print('PASS: the menu is drawn (mean luma %.1f)' % mean)

    for name, scene in (('ribbon', RIBBON), ('bokeh', BOKEH)):
        shot, _ = shoot(exe, out_dir, name, scene)
        if not shot:
            print('FAIL %s: no shot' % name)
            fails += 1
            continue
        n = sum(1 for a, b in zip(still_luma, luma(shot))
                if abs(a - b) > DIFFER_BY)
        share = n / float(W * H)
        if share < DIFFER_SHARE:
            print('FAIL %s: does not show (%.2f%% of the screen differs)'
                  % (name, share * 100))
            fails += 1
        else:
            print('PASS %s: shows (%.2f%% of the screen differs)'
                  % (name, share * 100))

    print('glcore_glsl_menu: %s' % ('%d check(s) failed' % fails if fails
                                    else 'all checks passed'))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
