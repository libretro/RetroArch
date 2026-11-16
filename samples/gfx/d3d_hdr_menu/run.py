#!/usr/bin/env python3
"""XMB's colours under HDR on D3D11 and D3D12, end to end.

With HDR on, both drivers draw the menu into a UI layer of their own and
composite it over the frame by its alpha. A draw that lowered that alpha
- a snowflake, a plastic icon's body, a soft glyph edge, all drawn at
less than full alpha - showed darker than the background under it: grey
snow, grey icons and grey labels. Nothing like it shows with HDR off.

retroarch.exe runs under Wine (vkd3d, lavapipe) on an Xvfb screen of its
own, with the display taken as HDR-capable (RETROARCH_DXGI_HDR=1), and
the screen is read with ImageMagick's import. For each driver it is shot
six times: XMB still (plastic on, no clock, no background effect), with
the Snow effect and with the Ribbon (its glow on), each with HDR off and
with HDR10 out. Then:

- still, HDR10 against HDR off: the same picture through the HDR10
  encode, which is the same for every pixel and keeps brighter brighter.
  Every pixel's luma with HDR off is put in a bin; each bin's median
  under HDR10 must not fall as the bins go brighter. A white icon or
  label shown darker than the background breaks that order.
- snow against still, HDR off and HDR10: snow only adds white over the
  menu, so next to no pixel may come out darker than in the still shot.
- ribbon against still, the same: the ribbon and its glow only add
  light. A ribbon whose pipeline D3D12 refused left the glow's state
  bound, and the icons after it were not drawn. The ribbon must also
  show: some of the screen brighter than in the still shot.

Shows a grid of the shots in <output dir>/sheet.png on failure.

Usage: run.py <retroarch.exe> <XMB assets dir> <output dir> [driver ...]
       (drivers: d3d11 d3d12, both by default)

The assets directory holds xmb/monochrome (libretro/retroarch-assets).
Needs wine, Xvfb, ImageMagick and a D3DCompiler_47.dll Wine can load
(vkd3d's, next to retroarch.exe, as Windows-wine-views.yml builds it).
"""

import os
import shutil
import subprocess
import sys
import tempfile
import time

W, H = 960, 540
# Below this much luma a still shot's bin is too sparse to judge
MIN_BIN_PIXELS = 200
# How far a brighter bin's HDR10 median may sit under a darker one's:
# the encode is monotonic, this is 8-bit capture noise
ORDER_SLACK = 3
# Snow may land on nothing but must never darken: pixels more than this
# much darker than in the still shot (which the snow is drawn over the
# same pixels of; 1 is rounding), as a share of the screen. A flake that
# lowered the HDR UI layer's alpha showed up to 12 darker, on thousands
# of pixels.
DARKER_BY = 3
DARKER_SHARE = 0.0005

STILL = {
    'menu_shader_pipeline': '0',
    'xmb_menu_shader_pipeline': '0',
}
SNOW = {
    'menu_shader_pipeline': '4',
    'xmb_menu_shader_pipeline': '4',
}
RIBBON = {
    'menu_shader_pipeline': '2',
    'xmb_menu_shader_pipeline': '2',
    'xmb_ribbon_bloom_enable': 'true',
    'xmb_ribbon_particles_enable': 'true',
}
# The ribbon shows: at least this share of the screen this much
# brighter than in the still shot
BRIGHTER_BY = 6
BRIGHTER_SHARE = 0.01


def winpath(v):
    return 'Z:' + v if v.startswith('/') else v


def write_cfg(path, d, driver, assets, hdr, scene):
    cfg = {
        'video_driver': driver,
        'video_context_driver': '',
        'input_driver': 'dinput',
        'input_joypad_driver': 'null',
        'input_autodetect_enable': 'false',
        'menu_driver': 'xmb',
        'assets_directory': winpath(assets),
        'xmb_theme': '0',
        'xmb_plastic_effect_enable': 'true',
        'menu_timedate_enable': 'false',
        'menu_battery_level_enable': 'false',
        'menu_core_enable': 'false',
        'menu_mouse_enable': 'false',
        'menu_pointer_enable': 'false',
        'menu_enable_widgets': 'false',
        'ui_menubar_enable': 'false',
        'video_fullscreen': 'true',
        'video_windowed_fullscreen': 'true',
        'video_font_enable': 'false',
        'video_hdr_mode': '1' if hdr else '0',
        'video_hdr_menu_nits': '200.000000',
        'video_threaded': 'false',
        'audio_enable': 'false',
        'audio_driver': 'null',
        'microphone_enable': 'false',
        'microphone_driver': 'null',
        'pause_nonactive': 'false',
        'confirm_quit': 'false',
        'config_save_on_exit': 'false',
        'rgui_show_start_screen': 'false',
        'cheevos_enable': 'false',
        'history_list_enable': 'false',
        'log_to_file': 'false',
        'savefile_directory': winpath(d),
        'savestate_directory': winpath(d),
        'system_directory': winpath(d),
        'cache_directory': winpath(d),
        'playlist_directory': winpath(d),
        'thumbnails_directory': winpath(d),
    }
    cfg.update(scene)
    with open(path, 'w') as f:
        for k in sorted(cfg):
            f.write('%s = "%s"\n' % (k, cfg[k]))


def grab(display):
    """The screen as rows of (r, g, b), or None"""
    p = subprocess.run(['import', '-display', display, '-window', 'root',
                        '-depth', '8', 'rgb:-'], stdout=subprocess.PIPE,
                       stderr=subprocess.DEVNULL)
    if p.returncode or len(p.stdout) != W * H * 3:
        return None
    return p.stdout


def shoot(exe, assets, out_dir, driver, hdr, scene_name, scene):
    """One shot of the menu, once it has settled: raw RGB bytes."""
    d = tempfile.mkdtemp(prefix='ra-hdr-menu-')
    cfg = os.path.join(d, 'retroarch.cfg')
    write_cfg(cfg, d, driver, assets, hdr, scene)
    display = ':%d' % (90 + os.getpid() % 50)
    xvfb = subprocess.Popen(['Xvfb', display, '-screen', '0',
                             '%dx%dx24' % (W, H)],
                            stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL)
    env = dict(os.environ, DISPLAY=display, WINEDEBUG='-all',
               WINEDLLOVERRIDES='d3dcompiler_47=n')
    env.pop('RETROARCH_DXGI_HDR', None)
    if hdr:
        env['RETROARCH_DXGI_HDR'] = '1'
    log = open(os.path.join(out_dir, '%s-%s-%s.log'
                            % (driver, 'hdr10' if hdr else 'sdr',
                               scene_name)), 'w')
    ra = None
    shot = None
    try:
        time.sleep(1)
        ra = subprocess.Popen(['wine', exe, '--config', winpath(cfg), '-v'],
                              cwd=os.path.dirname(exe), env=env,
                              stdout=log, stderr=subprocess.STDOUT)
        # Settled: the still menu twice the same two seconds apart. The
        # effects never settle; they are shot once the still one would
        # have.
        deadline = time.monotonic() + 90
        last = None
        steady = 0
        while time.monotonic() < deadline and ra.poll() is None:
            time.sleep(2)
            cur = grab(display)
            if cur is None:
                continue
            if scene is not STILL:
                steady += 1
                if steady >= 8:
                    shot = cur
                    break
                continue
            if cur == last and any(cur[i] != cur[0] for i in
                                   range(0, len(cur), 997)):
                shot = cur
                break
            last = cur
    finally:
        if ra and ra.poll() is None:
            ra.kill()
        subprocess.run(['wineserver', '-k'], env=env,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        xvfb.kill()
        xvfb.wait()
        log.close()
        shutil.rmtree(d, ignore_errors=True)
    if shot:
        png = os.path.join(out_dir, '%s-%s-%s.png'
                           % (driver, 'hdr10' if hdr else 'sdr', scene_name))
        subprocess.run(['convert', '-size', '%dx%d' % (W, H), '-depth', '8',
                        'rgb:-', png], input=shot)
    return shot


def luma(rgb):
    return [(rgb[i] * 54 + rgb[i + 1] * 183 + rgb[i + 2] * 19) >> 8
            for i in range(0, len(rgb), 3)]


def check_order(sdr, hdr):
    """Each brighter HDR-off luma bin's HDR10 median at least the last's"""
    bins = {}
    for a, b in zip(luma(sdr), luma(hdr)):
        bins.setdefault(a // 8, []).append(b)
    worst = None
    prev = None
    for k in sorted(bins):
        v = bins[k]
        if len(v) < MIN_BIN_PIXELS:
            continue
        v.sort()
        med = v[len(v) // 2]
        if prev is not None and med < prev[1] - ORDER_SLACK:
            drop = prev[1] - med
            if worst is None or drop > worst[0]:
                worst = (drop, prev[0] * 8, k * 8, prev[1], med)
        if prev is None or med > prev[1]:
            prev = (k, med)
    return worst


def darker_share(still, fx):
    n = 0
    for a, b in zip(luma(still), luma(fx)):
        if b < a - DARKER_BY:
            n += 1
    return n / float(W * H)


def brighter_share(still, fx):
    n = 0
    for a, b in zip(luma(still), luma(fx)):
        if b > a + BRIGHTER_BY:
            n += 1
    return n / float(W * H)


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    exe = os.path.abspath(sys.argv[1])
    assets = os.path.abspath(sys.argv[2])
    out_dir = os.path.abspath(sys.argv[3])
    drivers = sys.argv[4:] or ['d3d11', 'd3d12']
    os.makedirs(out_dir, exist_ok=True)
    fails = 0
    for driver in drivers:
        shots = {}
        for hdr in (False, True):
            for name, scene in (('still', STILL), ('snow', SNOW),
                                ('ribbon', RIBBON)):
                shots[(hdr, name)] = shoot(exe, assets, out_dir, driver,
                                           hdr, name, scene)
        missing = [k for k, v in shots.items() if v is None]
        if missing:
            print('FAIL %s: no settled shot for %s' % (driver, missing))
            fails += 1
            continue
        worst = check_order(shots[(False, 'still')], shots[(True, 'still')])
        if worst:
            print('FAIL %s: under HDR10, what is brighter with HDR off comes'
                  ' out darker: HDR-off luma %d-%d shows at %d, %d-%d at %d'
                  % (driver, worst[1], worst[1] + 7, worst[3], worst[2],
                     worst[2] + 7, worst[4]))
            fails += 1
        else:
            print('PASS %s: still menu keeps its brightness order under HDR10'
                  % driver)
        for hdr in (False, True):
            what = '%s %s' % (driver, 'HDR10' if hdr else 'HDR off')
            for name in ('snow', 'ribbon'):
                share = darker_share(shots[(hdr, 'still')],
                                     shots[(hdr, name)])
                if share > DARKER_SHARE:
                    print('FAIL %s: the %s darkens %.3f%% of the screen'
                          % (what, name, share * 100))
                    fails += 1
                else:
                    print('PASS %s: the %s darkens nothing (%.3f%%)'
                          % (what, name, share * 100))
            share = brighter_share(shots[(hdr, 'still')],
                                   shots[(hdr, 'ribbon')])
            if share < BRIGHTER_SHARE:
                print('FAIL %s: the ribbon does not show (%.3f%% of the '
                      'screen brighter)' % (what, share * 100))
                fails += 1
            else:
                print('PASS %s: the ribbon shows (%.3f%% of the screen '
                      'brighter)' % (what, share * 100))
    if fails:
        names = sorted(f for f in os.listdir(out_dir) if f.endswith('.png'))
        if names:
            subprocess.run(['montage'] + [os.path.join(out_dir, n)
                                          for n in names] +
                           ['-tile', '6x', '-geometry', '480x270+2+2',
                            os.path.join(out_dir, 'sheet.png')])
    print('d3d_hdr_menu: %s' % ('%d check(s) failed' % fails if fails
                                 else 'all checks passed'))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
