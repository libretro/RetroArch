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

Then a game: pattern_core.c, built here, hands over a frame whose top
quarter is blue and whose rest is red on the left and green on the
right, as a software frame and drawn on a GL core-profile hardware
context, 320x240 and 640x480, each with no preset and through
invert.glslp (one pass into a
framebuffer of the frame's size, then the stock pass to the screen).
Each must show the right colours in the right places: inverted through
the preset, the right way up, the frame's halves where they belong.
Before glcore's GLSL path drew a software frame at all, it uploaded the
whole power-of-two texture from the core's smaller buffer, left the
menu's blending on (an XRGB frame's alpha is empty), drew the first
pass with whatever program was bound, and read the frame from a
texture of the size gl2 uses rather than its own.

Then the rest of what a GLSL preset does, on the software frame:

- multi.glslp: two passes, the first into a framebuffer twice the
  frame's size, the second to the viewport: inverted, where it belongs;
- history.glslp, with the frame swapping its halves every frame: each
  half the average of this frame and the last (PrevTexture), so an
  even yellow;
- feedback.glslp: half the frame and half the pass's own last output
  (PassFeedback0), which settles on the frame itself;
- invert.glslp with the menu opened and closed over it: still inverted.

Usage: run.py <retroarch> <output dir>
Needs Xvfb, ImageMagick, xdotool and a C compiler.
"""

import os
import shutil
import subprocess
import sys
import tempfile
import time

W, H = 640, 480
HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
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
    scene = dict(scene)
    scene.pop('_core', None)
    scene.pop('_hw', None)
    scene.pop('_preset', None)
    scene.pop('_big', None)
    scene.pop('_flip', None)
    scene.pop('_menu', None)
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
        'video_scale_integer': 'false',
        'video_shader_enable': 'true',
        'menu_pause_libretro': 'false',
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
    env.pop('PATTERN_HW', None)
    env.pop('PATTERN_BIG', None)
    env.pop('PATTERN_FLIP', None)
    if scene.get('_flip'):
        env['PATTERN_FLIP'] = '1'
    if scene.get('_hw'):
        env['PATTERN_HW'] = '1'
    if scene.get('_big'):
        env['PATTERN_BIG'] = '1'
    args = [exe, '--config', cfg, '-v']
    if scene.get('_core'):
        args += ['-L', scene['_core']]
    else:
        args.append('--menu')
    if scene.get('_preset'):
        args.append('--set-shader=' + scene['_preset'])
    log_path = os.path.join(out_dir, name + '.log')
    ra = None
    shot = None
    try:
        time.sleep(1)
        with open(log_path, 'w') as log:
            ra = subprocess.Popen(args, env=env, stdout=log,
                                  stderr=subprocess.STDOUT)
            # The still menu twice the same; the effects never settle,
            # so they are shot once the still one would have been
            deadline = time.monotonic() + 60
            last = None
            latest = None
            seen = 0
            if scene.get('_menu'):
                # Up, then back to the game, before anything is judged
                time.sleep(4)
                for _ in range(2):
                    subprocess.run(['xdotool', 'keydown', 'F1'], env=env)
                    time.sleep(0.3)
                    subprocess.run(['xdotool', 'keyup', 'F1'], env=env)
                    time.sleep(3)
            while time.monotonic() < deadline and ra.poll() is None:
                time.sleep(1)
                cur = grab(display)
                if cur is None:
                    continue
                latest = cur
                # Black is RetroArch not up yet, not a settled screen; a
                # menu that never draws is still judged at the deadline
                if not cur.strip(b'\x00'):
                    continue
                seen += 1
                if scene in (RIBBON, BOKEH) or scene.get('_flip'):
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


def pixel(img, x, y):
    i = (y * W + x) * 3
    return tuple(img[i:i + 3])


def near(a, b):
    return all(abs(p - q) < 40 for p, q in zip(a, b))


def build_core(out_dir):
    """pattern_core.c as a libretro core, or None"""
    so = os.path.join(out_dir, 'pattern_libretro.so')
    cc = os.environ.get('CC', 'cc').split()
    p = subprocess.run(cc + ['-std=gnu99', '-O1', '-fPIC', '-shared',
                             '-I' + os.path.join(REPO, 'libretro-common',
                                                 'include'),
                             '-o', so, os.path.join(HERE, 'pattern_core.c')])
    return so if p.returncode == 0 else None


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

    core = build_core(out_dir)
    if not core:
        print('FAIL: pattern_core.c does not build')
        return 1
    preset = os.path.join(HERE, 'invert.glslp')
    # Top band, left half, right half, the right edge and the bottom
    # left corner (which a frame read from too large a texture does not
    # reach): as handed over, then inverted
    plain = ((0, 0, 255), (255, 0, 0), (0, 255, 0), (0, 255, 0),
             (255, 0, 0))
    inverted = ((255, 255, 0), (0, 255, 255), (255, 0, 255),
                (255, 0, 255), (0, 255, 255))
    for hw, big in ((False, False), (True, False), (False, True),
                    (True, True)):
        for with_preset in (False, True):
            name = '%s%s%s' % ('hw' if hw else 'sw', '-640' if big else '',
                               '-preset' if with_preset else '')
            scene = {'_core': core, '_hw': hw, '_big': big}
            if with_preset:
                scene['_preset'] = preset
            shot, _ = shoot(exe, out_dir, name, scene)
            want = inverted if with_preset else plain
            got = shot and (pixel(shot, W // 2, H // 10),
                            pixel(shot, W // 4, H * 3 // 5),
                            pixel(shot, W * 3 // 4, H * 3 // 5),
                            pixel(shot, W * 39 // 40, H * 3 // 5),
                            pixel(shot, W // 40, H * 39 // 40))
            if got and all(near(g, w) for g, w in zip(got, want)):
                print('PASS %s: the frame where it belongs %s'
                      % (name, got))
            else:
                print('FAIL %s: the frame at the top, left, right, '
                      'right edge and bottom left %s, want %s'
                      % (name, got, want))
                fails += 1

    # The rest of a preset, on the software frame
    yellow = (128, 128, 0)
    for name, scene, want in (
            ('sw-multi', {'_preset': os.path.join(HERE, 'multi.glslp')},
             inverted),
            ('sw-history', {'_preset': os.path.join(HERE, 'history.glslp'),
                            '_flip': True},
             ((0, 0, 255), yellow, yellow, yellow, yellow)),
            ('sw-feedback', {'_preset': os.path.join(HERE,
                                                     'feedback.glslp')},
             plain),
            ('sw-preset-menu', {'_preset': preset, '_menu': True},
             inverted)):
        scene['_core'] = core
        shot, _ = shoot(exe, out_dir, name, scene)
        got = shot and (pixel(shot, W // 2, H // 10),
                        pixel(shot, W // 4, H * 3 // 5),
                        pixel(shot, W * 3 // 4, H * 3 // 5),
                        pixel(shot, W * 39 // 40, H * 3 // 5),
                        pixel(shot, W // 40, H * 39 // 40))
        if got and all(near(g, w) for g, w in zip(got, want)):
            print('PASS %s: as the preset draws it %s' % (name, got))
        else:
            print('FAIL %s: the frame at the top, left, right, right edge '
                  'and bottom left %s, want %s' % (name, got, want))
            fails += 1

    print('glcore_glsl_menu: %s' % ('%d check(s) failed' % fails if fails
                                    else 'all checks passed'))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
