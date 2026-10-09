#!/usr/bin/env python3
"""End-to-end check of RetroArch's view presentation.

Runs the video_views test core in headless gamescope for each case,
takes a GPU screenshot over the network command port (gamescope's own
with the menu open), checks the colour where each view should land,
and checks the views status the core logged. One case also bounds the
GPU memory RetroArch holds, and the pointer cases move the pointer of
gamescope's own X server and look for the menu's cursor under it. The
gl driver doesn't present views, so it runs one case that expects the
packed frame and no PRESENTS. The exact cases check every pixel of
views filled with noise, on glcore once for each way it copies them.
Not run in CI: it needs a GPU, gamescope and a RetroArch build.

Usage: run.py <retroarch binary> <output dir> <driver> [driver ...]
"""

import ctypes
import os
import re
import shlex
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
CORE = os.path.normpath(os.path.join(HERE, '..', 'video_views_libretro.so'))
W, H = 1600, 960
TOLERANCE = 8

RED, BLUE, GREEN = (255, 0, 0), (0, 0, 255), (0, 255, 0)
YELLOW, WHITE = (255, 255, 0), (255, 255, 255)
BLACK, GREY = (0, 0, 0), (32, 32, 32)

# Aspect Ratio: Custom.
CUSTOM_VP = {'aspect_ratio_index': '23'}

# name, map option, settings, [(x, y, rgb)] in a 1600x960 window
CASES = [
    ('3ds-2d', '3ds', {'video_stereo_mode': '0'},
     [(800, 240, GREEN), (800, 720, YELLOW), (200, 240, BLACK),
      (404, 4, WHITE)]),
    ('3ds-force-2d', '3ds_force', {'video_stereo_mode': '0'},
     [(800, 240, RED), (800, 720, YELLOW)]),
    ('3ds-sbs-full', '3ds', {'video_stereo_mode': '2'},
     [(400, 240, RED), (1200, 240, BLUE), (400, 720, YELLOW),
      (1200, 720, YELLOW), (20, 720, BLACK), (4, 4, WHITE),
      (804, 4, WHITE)]),
    ('3ds-sbs-full-threaded', '3ds',
     {'video_stereo_mode': '2', 'video_threaded': 'true'},
     [(400, 240, RED), (1200, 240, BLUE), (400, 720, YELLOW),
      (1200, 720, YELLOW)]),
    ('3ds-sbs-full-swap', '3ds',
     {'video_stereo_mode': '2', 'video_stereo_swap_eyes': 'true'},
     [(400, 240, BLUE), (1200, 240, RED)]),
    ('3ds-sbs-half', '3ds', {'video_stereo_mode': '1'},
     [(400, 240, RED), (1200, 240, BLUE), (400, 720, YELLOW),
      (1200, 720, YELLOW), (100, 240, BLACK)]),
    ('3ds-top-bottom', '3ds', {'video_stereo_mode': '3'},
     [(800, 120, RED), (800, 360, YELLOW), (800, 600, BLUE),
      (800, 840, YELLOW)]),
    ('3ds-anaglyph', '3ds', {'video_stereo_mode': '4'},
     [(800, 240, (116, 0, 255)), (800, 720, (210, 255, 0))]),
    ('3ds-interlaced', '3ds', {'video_stereo_mode': '5'},
     [(800, 240, RED), (800, 241, BLUE), (800, 720, YELLOW)]),
    ('3ds-interlaced-swap', '3ds',
     {'video_stereo_mode': '5', 'video_stereo_swap_eyes': 'true'},
     [(800, 240, BLUE), (800, 241, RED)]),
    ('ds-horizontal', 'ds',
     {'video_stereo_mode': '0', 'video_screen_layout': '1'},
     [(400, 480, GREEN), (1200, 480, YELLOW), (400, 100, BLACK)]),
    ('vb-sbs-full', 'vb', {'video_stereo_mode': '2'},
     [(400, 480, RED), (1200, 480, BLUE)]),
    # A custom viewport larger than the window is cropped, not squeezed:
    # the 8-pixel marker is 66 wide, and an origin left of the window or
    # of an eye's half shows the picture's right part.
    ('vb-custom-crop', 'vb',
     dict(CUSTOM_VP, video_stereo_mode='0', custom_viewport_x='0',
          custom_viewport_y='0', custom_viewport_width='3200',
          custom_viewport_height='1920'),
     [(50, 50, WHITE), (100, 100, GREEN), (1500, 900, GREEN)]),
    ('vb-custom-offset', 'vb',
     dict(CUSTOM_VP, video_stereo_mode='0', custom_viewport_x='-1600',
          custom_viewport_y='-960', custom_viewport_width='3200',
          custom_viewport_height='1920'),
     [(50, 50, GREEN), (800, 480, GREEN)]),
    ('vb-custom-sbs-offset', 'vb',
     dict(CUSTOM_VP, video_stereo_mode='1', custom_viewport_x='-1600',
          custom_viewport_y='0', custom_viewport_width='3200',
          custom_viewport_height='960'),
     [(400, 480, RED), (1200, 480, BLUE), (50, 50, RED)]),
    ('no-map', 'none', {'video_stereo_mode': '2'},
     [(400, 240, RED), (1200, 240, BLUE), (800, 720, YELLOW),
      (100, 720, GREY)]),
    ('invalid-map', 'invalid', {'video_stereo_mode': '2'},
     [(400, 240, RED), (1200, 240, GREY)]),
    # Threaded video crops this frame above its second screen, so the map
    # no longer fits and the cropped frame shows whole, not laid out.
    ('crop-threaded', 'crop',
     {'video_stereo_mode': '0', 'video_screen_layout': '1',
      'video_threaded': 'true'},
     [(800, 100, GREEN), (800, 900, YELLOW), (1500, 480, GREEN)]),
]

# The same cases drawn in hardware: through a GL core context, with each
# origin, on glcore, and into the core's own images on vulkan.
# {driver: [(case, video_views_test_hw)]}
HW_CASES = {
    'glcore': [(name, hw) for hw in ('gl', 'gl_topleft')
               for name in ('3ds-2d', '3ds-sbs-full', '3ds-anaglyph',
                            '3ds-interlaced')],
    'vulkan': [(name, 'vulkan')
               for name in ('3ds-2d', '3ds-sbs-full', '3ds-anaglyph',
                            '3ds-interlaced', '3ds-sbs-full-threaded')],
}

# A frame past the declared maximum in the core's own Vulkan image.
# Nothing is copied out of it, so threaded video does not crop it, and
# it is laid out as any other.
VULKAN_CROP_CASES = [
    ('crop', 'crop', {'video_stereo_mode': '0', 'video_screen_layout': '1'},
     [(400, 480, GREEN), (1200, 480, YELLOW), (400, 100, BLACK)]),
    ('crop-threaded', 'crop',
     {'video_stereo_mode': '0', 'video_screen_layout': '1',
      'video_threaded': 'true'},
     [(400, 480, GREEN), (1200, 480, YELLOW), (400, 100, BLACK)]),
]

# A frame a driver can't lay out is drawn whole, and the menu then lays
# out as for a frame without views. Threaded video crops every crop-map
# frame, so the menu in each stereo mode must match the first mode's.
# gamescope captures it: a GPU screenshot leaves the menu out.
# [(name, map option, settings, stereo modes)]
MENU_CASES = [
    ('crop-threaded-menu', 'crop',
     {'video_screen_layout': '1', 'video_threaded': 'true',
      'menu_driver': 'ozone', 'menu_timedate_enable': 'false'},
     ('0', '2')),
]
# Pixels allowed to differ from the first mode's menu: the cursor's
# border pulses. A menu laid out for one eye differs in over 200000.
MENU_DIFF_MAX = 20000

# The menu draws its own cursor where it has the pointer: with a per-eye
# UI, at the pointer in the copy it is over and at the matching place in
# the other. Each case holds the pointer at two window points on one menu
# row, the second in the second eye's area, so only the cursor differs
# between the two screens. The ds map is not the window's shape, so
# there RGUI, fitted to the core's aspect, does not fill the window.
# [(name, map option, settings, eye areas (x, y, w, h), points)]
POINTER_CASES = [
    ('pointer-2d', '3ds', {'video_stereo_mode': '0'},
     [(0, 0, 1600, 960)], [(900, 420), (1050, 420)]),
    ('pointer-sbs-half', '3ds', {'video_stereo_mode': '1'},
     [(0, 0, 800, 960), (800, 0, 800, 960)], [(450, 420), (1325, 420)]),
    ('pointer-sbs-full', '3ds', {'video_stereo_mode': '2'},
     [(0, 0, 800, 960), (800, 0, 800, 960)], [(450, 420), (1400, 420)]),
    ('pointer-top-bottom', '3ds', {'video_stereo_mode': '3'},
     [(0, 0, 1600, 480), (0, 480, 1600, 480)], [(900, 210), (1050, 690)]),
    ('pointer-anaglyph', '3ds', {'video_stereo_mode': '4'},
     [(0, 0, 1600, 960)], [(900, 420), (1050, 420)]),
    ('pointer-ds-2d', 'ds', {'video_stereo_mode': '0'},
     [(0, 0, 1600, 960)], [(960, 420), (1090, 420)]),
    ('pointer-ds-anaglyph', 'ds', {'video_stereo_mode': '4'},
     [(0, 0, 1600, 960)], [(960, 420), (1090, 420)]),
]
# Themes with a white cursor, which is how the check tells it apart.
POINTER_MENUS = {
    'ozone': {'menu_driver': 'ozone',
              'ozone_menu_color_theme': 'basic_black'},
    'rgui': {'menu_driver': 'rgui', 'rgui_menu_color_theme': '4',
             'rgui_particle_effect': '0'},
}
POINTER_SETTINGS = {'menu_mouse_enable': 'true',
                    'menu_pointer_enable': 'false',
                    'menu_timedate_enable': 'false'}
POINTER_PARK = (40, 900)
# How far the drawn cursor may sit from the pointer, and how much of the
# screen around the pointer counts as that cursor.
POINTER_TOLERANCE = 6
POINTER_REACH = 48

# The gl driver doesn't present views: the map is accepted, STEREO is off,
# and the core's 2D packing shows whole.
GL_CASES = [
    ('3ds-sbs-full-gl', '3ds', {'video_stereo_mode': '2'},
     [(400, 240, GREEN), (1200, 240, GREY), (800, 720, YELLOW),
      (100, 720, GREY)]),
]

# The core declares Azahar's 8000x4800 maximum, draws its usual frame in
# hardware as Azahar does, and runs a preset keeping eight frames of
# history. Sized for that maximum, the history is 1.75 GiB a chain: the
# limit leaves room for the main chain's, not for a view's.
# [(name, map option, settings, points, GPU memory limit in MiB)]
HISTORY_PRESET = os.path.join(HERE, 'history.slangp')
MEMORY_CASES = [
    ('3ds-sbs-full-history', '3ds',
     {'video_stereo_mode': '2', 'video_shader_enable': 'true'},
     [(400, 240, RED), (1200, 240, BLUE), (400, 720, YELLOW),
      (1200, 720, YELLOW), (4, 4, WHITE), (804, 4, WHITE)],
     3072),
]
MEMORY_HW = {'glcore': 'gl', 'vulkan': 'vulkan'}

# Views arrive pixel for pixel: the core fills each view with noise,
# drawn unfiltered at an integer scale, and every pixel of each view on
# screen must be its source pixel. glcore runs each case again with
# Mesa hiding glCopyImageSubData, so the views are blitted instead, and
# the two screenshots must match.
# [(name, map option, core options, hw)]
EXACT_SETTINGS = {'video_stereo_mode': '0', 'video_scale_integer': 'true',
                  'video_smooth': 'false'}
NOISE = {'video_views_test_pattern': 'noise'}
NOISE_565 = dict(NOISE, video_views_test_format='rgb565')
EXACT_CASES = [
    ('exact-ds', 'ds', NOISE, 'off'),
    ('exact-ds-rgb565', 'ds', NOISE_565, 'off'),
    # The second screen starts 240 columns in.
    ('exact-3ds-rgb565', '3ds', NOISE_565, 'off'),
    # Rows counted from the frame's bottom, and from its top.
    ('exact-3ds', '3ds', NOISE, 'gl'),
    ('exact-3ds', '3ds', NOISE, 'gl_topleft'),
    ('exact-3ds', '3ds', NOISE, 'vulkan'),
]
EXACT_HW = {'glcore': ('off', 'gl', 'gl_topleft'), 'vulkan': ('off', 'vulkan')}
# The core's 2D maps, as (x, y, width, height) in the packed frame.
EXACT_VIEWS = {'ds': [(0, 0, 256, 192), (0, 192, 256, 192)],
               '3ds': [(0, 0, 400, 240), (240, 240, 320, 240)]}
NOISE_COLOURS = [BLACK, RED, GREEN, BLUE, (0, 255, 255), (255, 0, 255),
                 YELLOW]
# Mesa without GL 4.3 and ARB_copy_image: glcore must blit.
NO_COPY_IMAGE = {'MESA_GL_VERSION_OVERRIDE': '4.2',
                 'MESA_EXTENSION_OVERRIDE': '-GL_ARB_copy_image'}
COPY_RE = re.compile(r'\[GLCore\] Views copied by ([\w ]+)\.')

STATUS_RE = re.compile(
    r'\[video_views\] presents=(\d) stereo=(\d) accepted=(\d)')


def read_png(path):
    """8-bit RGB or RGBA, non-interlaced: what rpng writes."""
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


def send(cmd, port):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.sendto(cmd.encode(), ('127.0.0.1', port))
    s.close()


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(('127.0.0.1', 0))
    port = s.getsockname()[1]
    s.close()
    return port


def write_cfg(path, d, driver, settings, port):
    cfg = {
        'video_driver': driver,
        # vk_wayland segfaults on teardown in headless gamescope; stay on X.
        'video_context_driver': 'vk_x' if driver == 'vulkan' else 'x',
        'input_driver': 'x',
        # Keep real controllers and achievements out of the run: the test
        # joypad driver has no pads without a test file ('null' falls
        # back to udev).
        'input_joypad_driver': 'test',
        'input_autodetect_enable': 'false',
        'cheevos_enable': 'false',
        'video_fullscreen': 'true',
        'video_windowed_fullscreen': 'true',
        'video_gpu_screenshot': 'true',
        'video_font_enable': 'false',
        'menu_enable_widgets': 'false',
        'video_stereo_mode': '0',
        'video_stereo_swap_eyes': 'false',
        'video_screen_layout': '0',
        'video_scale_integer': 'false',
        'aspect_ratio_index': '22',
        'audio_enable': 'false',
        'audio_driver': 'null',
        # A core asking for the microphone made ALSA's default capture
        # device block the main thread once.
        'microphone_enable': 'false',
        'microphone_driver': 'null',
        'pause_nonactive': 'false',
        'confirm_quit': 'false',
        'quit_press_twice': 'false',
        'config_save_on_exit': 'false',
        'network_cmd_enable': 'true',
        'network_cmd_port': str(port),
        'history_list_enable': 'false',
        'savestate_thumbnail_enable': 'false',
        'global_core_options': 'true',
        'core_options_path': os.path.join(d, 'opts.cfg'),
        'screenshot_directory': os.path.join(d, 'shots'),
        'savefile_directory': d,
        'savestate_directory': d,
        'system_directory': d,
        'cache_directory': d,
        'log_dir': d,
        'log_to_file': 'true',
        'libretro_log_level': '0',
        # Everything else RetroArch can write by default lives under
        # ~/.config/retroarch; keep it all in the case directory instead.
        # config_set_defaults() computes content_favorites_path etc. from
        # playlist_directory before this file is even parsed, so the
        # playlist paths need setting directly, not just their directory.
        'playlist_directory': d,
        'content_favorites_path': os.path.join(d, 'content_favorites.lpl'),
        'content_history_path': os.path.join(d, 'content_history.lpl'),
        'content_image_history_path':
            os.path.join(d, 'content_image_history.lpl'),
        'content_music_history_path':
            os.path.join(d, 'content_music_history.lpl'),
        'content_video_history_path':
            os.path.join(d, 'content_video_history.lpl'),
        'input_remapping_directory': d,
        'rgui_config_directory': d,
        'video_shader_dir': d,
        'cheat_database_path': d,
        'recording_output_directory': d,
        'recording_config_directory': d,
        'runtime_log_directory': d,
        'thumbnails_directory': d,
        'core_assets_directory': d,
        'dynamic_wallpapers_directory': d,
    }
    cfg.update(settings)
    with open(path, 'w') as f:
        for k in sorted(cfg):
            f.write('%s = "%s"\n' % (k, cfg[k]))


def expected_status(driver, settings):
    """(presents, stereo) the frontend should report for these settings."""
    if driver == 'gl':
        return (0, 0)
    return (1, 0 if settings.get('video_stereo_mode', '0') == '0' else 1)


def status_errors(d, driver, settings):
    """Check the last views status the core logged."""
    found = None
    for name in ('run.log', 'retroarch.log'):
        path = os.path.join(d, name)
        if os.path.exists(path):
            with open(path, errors='replace') as f:
                for m in STATUS_RE.finditer(f.read()):
                    found = tuple(int(g) for g in m.groups())
    if found is None:
        return ['no "[video_views] presents=" line in the logs']
    want = expected_status(driver, settings)
    errors = []
    if found[:2] != want:
        errors.append('status presents=%d stereo=%d, want presents=%d '
                      'stereo=%d' % (found[0], found[1], want[0], want[1]))
    if driver == 'gl' and found[2] != 1:
        errors.append('map not accepted on gl')
    return errors


# Headless gamescope still opens the display, session bus and runtime
# dir it inherits; give it none. Short and private: a socket path holds
# 108 bytes, so not under the case directory.
DESKTOP_ENV = ('DISPLAY', 'WAYLAND_DISPLAY', 'WAYLAND_SOCKET',
               'DBUS_SESSION_BUS_ADDRESS', 'XAUTHORITY')


def isolated_env():
    env = {k: v for k, v in os.environ.items() if k not in DESKTOP_ENV}
    env['XDG_RUNTIME_DIR'] = tempfile.mkdtemp(
        prefix='ra-e2e-', dir=os.environ.get('RA_E2E_RUNTIME_BASE', '/tmp'))
    return env


def gamescope_shot(d, env, name='menu.png'):
    """What gamescope shows, menu included, in d/name."""
    out = os.path.join(d, name)
    try:
        subprocess.run(['gamescopectl', 'screenshot', out],
                       env=dict(env, GAMESCOPE_WAYLAND_DISPLAY='gamescope-0'),
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       timeout=10)
    except (OSError, subprocess.TimeoutExpired):
        return None
    deadline = time.time() + 5
    while time.time() < deadline and not os.path.exists(out):
        time.sleep(0.5)
    if not os.path.exists(out):
        return None
    time.sleep(1)
    return out


def move_pointer(d, n, point):
    """Has the case's pointer helper put the pointer at a window point,
    and gives the menu time to draw it there."""
    tmp = os.path.join(d, 'pointer.tmp')
    with open(tmp, 'w') as f:
        f.write('%d %d %d\n' % ((n,) + tuple(point)))
    os.replace(tmp, os.path.join(d, 'pointer.at'))
    deadline = time.time() + 5
    while time.time() < deadline:
        try:
            with open(os.path.join(d, 'pointer.done')) as f:
                if f.read().split() == [str(n)]:
                    time.sleep(1.5)
                    return True
        except OSError:
            pass
        time.sleep(0.1)
    return False


def retroarch_pid(gamescope_pid, cfg):
    """The RetroArch gamescope runs with config cfg, or None: the one
    whose arguments start with it, not the shell or gamescope's reaper
    that pass it on. A wrapper script that execs RetroArch counts."""
    parents = {}
    for p in os.listdir('/proc'):
        try:
            with open('/proc/%s/stat' % p) as f:
                parents[int(p)] = int(f.read().rsplit(')', 1)[1].split()[1])
        except (OSError, ValueError, IndexError):
            continue
    ours, found = {gamescope_pid}, True
    while found:
        found = False
        for pid, ppid in parents.items():
            if ppid in ours and pid not in ours:
                ours.add(pid)
                found = True
    for pid in ours:
        try:
            with open('/proc/%d/cmdline' % pid, 'rb') as f:
                argv = f.read().split(b'\0')
        except OSError:
            continue
        if argv[1:3] == [b'--config', cfg.encode()]:
            return pid
    return None


def gpu_mib(pid):
    """GPU memory pid holds, in MiB: drm fdinfo's GTT and VRAM over its
    unique clients. None without a client."""
    d = '/proc/%d/fdinfo' % pid
    seen, kib = set(), 0
    try:
        fds = os.listdir(d)
    except OSError:
        return None
    for fd in fds:
        try:
            with open(os.path.join(d, fd)) as f:
                kv = dict(line.split(':', 1) for line in f if ':' in line)
        except OSError:
            continue
        kv = {k.strip(): v.split() for k, v in kv.items()}
        client = (tuple(kv.get('drm-pdev', ())),
                  tuple(kv.get('drm-client-id', ())))
        if not client[1] or client in seen:
            continue
        seen.add(client)
        for key in ('drm-memory-gtt', 'drm-memory-vram'):
            v = kv.get(key)
            if v:
                unit = v[1] if len(v) > 1 else ''
                kib += int(v[0]) * {'': 1.0 / 1024, 'KiB': 1, 'MiB': 1024,
                                    'GiB': 1 << 20}.get(unit, 1)
    return kib / 1024.0 if seen else None


def case_dir(root, driver, name, hw='off'):
    """The directory a case runs in."""
    if hw != 'off':
        name += '-hw-' + hw
    return os.path.realpath(os.path.join(root, driver + '-' + name))


def run_case(retroarch, root, driver, case, hw='off', menu=False,
             opts=None, args=(), gpu=None, pointer=(), env_extra=None):
    """gpu, a list, gets RetroArch's GPU memory in MiB sampled while it
    settles. pointer, window points, has the menu open with the pointer
    at each in turn, in d/pointer-N.png. env_extra is added to
    RetroArch's environment."""
    name, mapopt, settings, points = case
    d = case_dir(root, driver, name, hw)
    if os.path.commonpath([root, d]) != root or d == root:
        raise RuntimeError('refusing to touch ' + d)
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(os.path.join(d, 'shots'))
    port = free_port()
    cfg = os.path.join(d, 'retroarch.cfg')
    write_cfg(cfg, d, driver, settings, port)
    with open(os.path.join(d, 'opts.cfg'), 'w') as f:
        f.write('video_views_test_map = "%s"\n' % mapopt)
        f.write('video_views_test_hw = "%s"\n' % hw)
        for k in sorted(opts or {}):
            f.write('%s = "%s"\n' % (k, opts[k]))

    # gamescope runs under isolated_env() below, so it can't probe the
    # desktop's display, bus or PipeWire. RetroArch, run as its child,
    # still gets whatever DISPLAY gamescope sets for it, so this guard
    # refuses if that ever turns out to be the desktop's, and points
    # RetroArch's Wayland at a socket that does not exist (unset,
    # libwayland falls back to wayland-0).
    helper = ''
    if pointer:
        helper = ' %s %s --pointer %s &' % (
            shlex.quote(sys.executable),
            shlex.quote(os.path.abspath(__file__)), shlex.quote(d))
    guard = ('case "$DISPLAY" in ""|:0) echo "refusing DISPLAY=$DISPLAY" >&2;'
             ' exit 99;; esac;' + helper +
             ' WAYLAND_DISPLAY=video-views-e2e-no-socket; export WAYLAND_DISPLAY;'
             ' exec "$0" "$@"')
    cmd = ['gamescope', '--backend', 'headless',
           '-w', str(W), '-h', str(H), '-W', str(W), '-H', str(H),
           '-r', '60', '--',
           'sh', '-c', guard,
           retroarch, '--config', cfg, '-L', CORE, '--verbose'] + list(args)
    env = isolated_env()
    env.update(env_extra or {})
    log = open(os.path.join(d, 'run.log'), 'w')
    p = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT,
                         env=env, start_new_session=True)
    shot = None
    try:
        settle = time.time() + 5
        while time.time() < settle:
            time.sleep(0.5)
            pid = retroarch_pid(p.pid, cfg) if gpu is not None else None
            mib = gpu_mib(pid) if pid else None
            if mib is not None:
                gpu.append(mib)
        if pointer:
            send('MENU_TOGGLE', port)
            time.sleep(3)
            # The menu takes the pointer once it has seen it move.
            moved = move_pointer(d, 0, POINTER_PARK)
            for i, point in enumerate(pointer):
                moved = moved and move_pointer(d, i + 1, point)
                shot = gamescope_shot(d, env, 'pointer-%d.png' % i)
                if not (moved and shot):
                    shot = None
                    break
        elif menu:
            send('MENU_TOGGLE', port)
            time.sleep(3)
            shot = gamescope_shot(d, env)
        else:
            send('SCREENSHOT', port)
        deadline = time.time() + 10
        while time.time() < deadline and not shot:
            time.sleep(0.5)
            pngs = [x for x in os.listdir(os.path.join(d, 'shots'))
                    if x.endswith('.png')]
            if pngs:
                time.sleep(1)
                shot = os.path.join(d, 'shots', pngs[0])
        send('QUIT', port)
        try:
            p.wait(10)
        except subprocess.TimeoutExpired:
            pass
    finally:
        if pointer:
            open(os.path.join(d, 'pointer.quit'), 'w').close()
        if p.poll() is None:
            os.killpg(p.pid, signal.SIGTERM)
            try:
                p.wait(5)
            except subprocess.TimeoutExpired:
                os.killpg(p.pid, signal.SIGKILL)
        log.close()
        runtime_dir = env['XDG_RUNTIME_DIR']
        if os.path.basename(runtime_dir).startswith('ra-e2e-'):
            shutil.rmtree(runtime_dir, ignore_errors=True)

    errors = status_errors(d, driver, settings)
    if not shot:
        return errors + ['no screenshot (see %s)'
                         % os.path.join(d, 'run.log')]
    w, h, bpp, rows = read_png(shot)
    if (w, h) != (W, H):
        return errors + ['screenshot is %dx%d, want %dx%d' % (w, h, W, H)]
    for x, y, want in points:
        got = tuple(rows[y][x * bpp:x * bpp + 3])
        if any(abs(g - e) > TOLERANCE for g, e in zip(got, want)):
            errors.append('(%d,%d) got %s want %s' % (x, y, got, want))
    return errors


def noise(x, y):
    """The core's noise_colour()."""
    h = (x * 0x9E3779B1 + y * 0x85EBCA77) & 0xffffffff
    h ^= h >> 15
    h = (h * 0x2C1B3C6D) & 0xffffffff
    h ^= h >> 12
    return NOISE_COLOURS[h % 7]


def rgb_row(rows, bpp, y, x, w):
    """w pixels of row y from column x, as RGB bytes."""
    seg = bytearray(rows[y][x * bpp:(x + w) * bpp])
    if bpp == 4:
        del seg[3::4]
    return seg


def white_boxes(rows, bpp):
    """(x, y, width, height) of each white rectangle, from its top-left
    pixel: the views' markers, which no noise colour matches."""
    white = bytes(WHITE)

    def is_white(x, y):
        return x >= 0 and y >= 0 and rows[y][x * bpp:x * bpp + 3] == white

    boxes = []
    for y, row in enumerate(rows):
        i = row.find(white)
        while i >= 0:
            x = i // bpp
            if (i % bpp == 0 and not is_white(x - 1, y)
                    and not is_white(x, y - 1)):
                w = h = 1
                while x + w < W and is_white(x + w, y):
                    w += 1
                while y + h < H and is_white(x, y + h):
                    h += 1
                boxes.append((x, y, w, h))
            i = row.find(white, i + 1)
    return boxes


def view_mismatches(rows, bpp, box, view):
    """Pixels of view, at the scale of the marker box from its corner,
    that are not their source pixel; None when box can't be its
    marker."""
    bx, by, bw, bh = box
    vx, vy, vw, vh = view
    if bw != bh or bw % 8:
        return None
    k = bw // 8
    if bx + vw * k > W or by + vh * k > H:
        return None
    bad = 0
    for sy in range(vh):
        want = b''.join(
            bytes(WHITE if sx < 8 and sy < 8 else noise(vx + sx, vy + sy)) * k
            for sx in range(vw))
        for r in range(k):
            got = rgb_row(rows, bpp, by + sy * k + r, bx, vw * k)
            if got != want:
                bad += sum(got[i:i + 3] != want[i:i + 3]
                           for i in range(0, len(want), 3))
    return bad


def exact_errors(rows, bpp, views):
    """Each view must show every one of its pixels at some integer scale,
    found by its marker, and the screens, one view each, stack in order
    from the top."""
    boxes = white_boxes(rows, bpp)
    errors = []
    tops = []
    for view in views:
        found = [(view_mismatches(rows, bpp, b, view), b) for b in boxes]
        found = [(m, b) for m, b in found if m is not None]
        exact = [b for m, b in found if m == 0]
        if exact:
            tops.append(exact[0][1])
        else:
            errors.append('view %s: %s' % (
                view, 'no marker found' if not found else
                'best match has %d wrong pixels' % min(found)[0]))
    if not errors and tops != sorted(tops):
        errors.append('screens out of order: tops at %s' % tops)
    return errors


def copy_path(d):
    """How glcore's log says it copies views out of the frame."""
    found = None
    for name in ('run.log', 'retroarch.log'):
        path = os.path.join(d, name)
        if os.path.exists(path):
            with open(path, errors='replace') as f:
                for m in COPY_RE.finditer(f.read()):
                    found = m.group(1)
    return found


def run_exact_case(retroarch, root, driver, case, suffix='',
                   env_extra=None, path=None):
    """The errors, and the screenshot's rows for comparing runs. path is
    how glcore must say it copies views."""
    name, mapopt, opts, hw = case
    name += suffix
    errors = run_case(retroarch, root, driver,
                      (name, mapopt, EXACT_SETTINGS, []), hw, opts=opts,
                      env_extra=env_extra)
    d = case_dir(root, driver, name, hw)
    if path and copy_path(d) != path:
        errors.append('views copied by %s, want %s' % (copy_path(d), path))
    shots = sorted(x for x in os.listdir(os.path.join(d, 'shots'))
                   if x.endswith('.png'))
    if not shots:
        return errors, None
    w, h, bpp, rows = read_png(os.path.join(d, 'shots', shots[0]))
    if (w, h) != (W, H):
        return errors, None
    return errors + exact_errors(rows, bpp, EXACT_VIEWS[mapopt]), rows


def run_menu_case(retroarch, root, driver, case):
    """The case with the menu open in each of its stereo modes; each
    mode's screen must match the first's."""
    name, mapopt, settings, modes = case
    errors, images = [], []
    for mode in modes:
        c = ('%s-%s' % (name, mode), mapopt,
             dict(settings, video_stereo_mode=mode), [])
        errors += run_case(retroarch, root, driver, c, menu=True)
        path = os.path.join(root, '%s-%s-%s' % (driver, name, mode),
                            'menu.png')
        try:
            images.append(read_png(path) if os.path.exists(path) else None)
        except ValueError:
            images.append(None)
    if errors or None in images:
        return errors or ['no menu screenshot']
    w, h, bpp, ref = images[0]
    for mode, (_, _, mbpp, rows) in zip(modes[1:], images[1:]):
        diff = 0
        for y in range(h):
            if rows[y] == ref[y] and mbpp == bpp:
                continue
            for x in range(w):
                a = ref[y][x * bpp:x * bpp + 3]
                b = rows[y][x * mbpp:x * mbpp + 3]
                if any(abs(p - q) > TOLERANCE for p, q in zip(a, b)):
                    diff += 1
        if diff > MENU_DIFF_MAX:
            errors.append('stereo mode %s: %d pixels differ from mode %s'
                          % (mode, diff, modes[0]))
    return errors


def cursor_pixels(rows, others, bpp):
    """Where one screen has the cursor and the other doesn't: the white
    pixels that differ. The box around the row under the pointer pulses
    between the two as well, in colour."""
    found = []
    for y in range(H):
        if rows[y] == others[y]:
            continue
        for x in range(W):
            a = rows[y][x * bpp:x * bpp + 3]
            if min(a) < 200:
                continue
            b = others[y][x * bpp:x * bpp + 3]
            if any(abs(p - q) > TOLERANCE for p, q in zip(a, b)):
                found.append((x, y))
    return found


def box_of(pixels):
    xs, ys = [p[0] for p in pixels], [p[1] for p in pixels]
    return (min(xs), min(ys), max(xs), max(ys))


def run_pointer_case(retroarch, root, driver, menu, case):
    """The case's menu with the pointer at each of its points. Each
    screen must show the cursor at the pointer and at the same place in
    every other eye's area, and nowhere else."""
    name, mapopt, settings, areas, points = case
    c = ('%s-%s' % (name, menu), mapopt,
         dict(POINTER_SETTINGS, **dict(POINTER_MENUS[menu], **settings)), [])
    errors = run_case(retroarch, root, driver, c, pointer=points)
    d = os.path.join(root, '%s-%s' % (driver, c[0]))
    images = []
    for i in range(len(points)):
        try:
            images.append(read_png(os.path.join(d, 'pointer-%d.png' % i)))
        except (OSError, ValueError):
            return errors or ['no screenshot with the pointer']
    if errors:
        return errors
    if any(im[:3] != (W, H, images[0][2]) for im in images):
        return ['a screenshot is not %dx%d' % (W, H)]
    shapes = []
    for i, pt in enumerate(points):
        bpp, rows = images[i][2:]
        home = [a for a in areas
                if a[0] <= pt[0] < a[0] + a[2]
                and a[1] <= pt[1] < a[1] + a[3]][0]
        pixels = cursor_pixels(rows, images[1 - i][3], bpp)
        for area in areas:
            want = (pt[0] - home[0] + area[0], pt[1] - home[1] + area[1])
            near = set(p for p in pixels
                       if max(abs(p[0] - want[0]),
                              abs(p[1] - want[1])) <= POINTER_REACH)
            pixels = [p for p in pixels if p not in near]
            if not near:
                errors.append('pointer (%d,%d): no cursor at (%d,%d)'
                              % (pt + want))
                continue
            box = box_of(near)
            # The pointer is the arrow's tip, or without the assets the
            # middle of a square.
            if not (box[0] - POINTER_TOLERANCE <= want[0]
                    <= box[2] + POINTER_TOLERANCE
                    and box[1] - POINTER_TOLERANCE <= want[1]
                    <= box[3] + POINTER_TOLERANCE):
                errors.append('pointer (%d,%d): cursor in (%d,%d)-(%d,%d), '
                              'want it at (%d,%d)' % (pt + box + want))
            shapes.append((box[0] - want[0], box[1] - want[1],
                           box[2] - want[0], box[3] - want[1]))
        if pixels:
            errors.append('pointer (%d,%d): a cursor in (%d,%d)-(%d,%d)'
                          % (pt + box_of(pixels)))
    # The same cursor about every pointer, in every copy.
    if any(abs(p - q) > 2 for s in shapes for p, q in zip(s, shapes[0])):
        errors.append('cursors differ about the pointer: %s' % shapes)
    return errors


def run_memory_case(retroarch, root, driver, case):
    """The case with the large maximum and the history preset, and
    RetroArch's peak GPU memory in MiB."""
    name, mapopt, settings, points, limit = case
    gpu = []
    errors = run_case(retroarch, root, driver,
                      (name, mapopt, settings, points), MEMORY_HW[driver],
                      opts={'video_views_test_max': 'large'},
                      args=('--set-shader=' + HISTORY_PRESET,), gpu=gpu)
    peak = max(gpu) if gpu else None
    if peak is None:
        errors.append('no GPU memory reading')
    elif peak > limit:
        errors.append('GPU memory peaked at %d MiB, limit %d'
                      % (peak, limit))
    return errors, peak


def gamescope_root(x11, dpy):
    """dpy's root window if this process runs inside the gamescope that
    owns it, else None: the server names its gamescope on its root."""
    x11.XInternAtom.restype = ctypes.c_ulong
    x11.XInternAtom.argtypes = [ctypes.c_void_p, ctypes.c_char_p,
                                ctypes.c_int]
    x11.XDefaultRootWindow.restype = ctypes.c_ulong
    x11.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
    x11.XGetWindowProperty.argtypes = [
        ctypes.c_void_p, ctypes.c_ulong, ctypes.c_ulong, ctypes.c_long,
        ctypes.c_long, ctypes.c_int, ctypes.c_ulong,
        ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.c_ulong),
        ctypes.POINTER(ctypes.POINTER(ctypes.c_ulong))]
    root = x11.XDefaultRootWindow(dpy)
    atom = x11.XInternAtom(dpy, b'GAMESCOPE_PID', 1)
    kind, fmt = ctypes.c_ulong(0), ctypes.c_int(0)
    count, left = ctypes.c_ulong(0), ctypes.c_ulong(0)
    value = ctypes.POINTER(ctypes.c_ulong)()
    if not atom or x11.XGetWindowProperty(
            dpy, root, atom, 0, 1, 0, 6, ctypes.byref(kind),
            ctypes.byref(fmt), ctypes.byref(count), ctypes.byref(left),
            ctypes.byref(value)) or fmt.value != 32 or count.value != 1:
        return None
    owner, pid = value[0], os.getpid()
    while pid > 1:
        try:
            with open('/proc/%d/stat' % pid) as f:
                pid = int(f.read().rsplit(')', 1)[1].split()[1])
        except (OSError, ValueError, IndexError):
            return None
        if pid == owner:
            return root
    return None


def pointer_helper(d):
    """Inside a case's gamescope: puts the pointer of gamescope's own X
    server at each point the runner writes to d/pointer.at. Never on a
    display that isn't this gamescope's."""
    if os.environ.get('DISPLAY', '') in ('', ':0'):
        return 99
    x11 = ctypes.CDLL('libX11.so.6')
    x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XWarpPointer.argtypes = [ctypes.c_void_p, ctypes.c_ulong,
                                 ctypes.c_ulong] + [ctypes.c_int] * 6
    x11.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]
    dpy = x11.XOpenDisplay(None)
    if not dpy:
        return 1
    root = gamescope_root(x11, dpy)
    if root is None:
        print('[e2e] refusing %s: not this gamescope\'s X server.'
              % os.environ['DISPLAY'], flush=True)
        return 99
    done, deadline = None, time.time() + 120
    while (time.time() < deadline
           and not os.path.exists(os.path.join(d, 'pointer.quit'))):
        try:
            with open(os.path.join(d, 'pointer.at')) as f:
                n, x, y = (int(v) for v in f.read().split())
        except (OSError, ValueError):
            n = done
        if n != done:
            x11.XWarpPointer(dpy, 0, root, 0, 0, 0, 0, x, y)
            x11.XSync(dpy, 0)
            done = n
            tmp = os.path.join(d, 'pointer.done.tmp')
            with open(tmp, 'w') as f:
                f.write('%d\n' % n)
            os.replace(tmp, os.path.join(d, 'pointer.done'))
            print('[e2e] pointer at (%d,%d).' % (x, y), flush=True)
        time.sleep(0.05)
    return 0


def main():
    if sys.argv[1:2] == ['--pointer']:
        return pointer_helper(sys.argv[2])
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    retroarch = os.path.abspath(sys.argv[1])
    root = os.path.realpath(sys.argv[2])
    os.makedirs(root, exist_ok=True)
    failed = 0
    for driver in sys.argv[3:]:
        if driver == 'gl':
            runs = [(case, 'off') for case in GL_CASES]
        else:
            runs = [(case, 'off') for case in CASES]
        runs += [(case, hw) for name, hw in HW_CASES.get(driver, [])
                 for case in CASES if case[0] == name]
        if driver == 'vulkan':
            runs += [(case, 'vulkan') for case in VULKAN_CROP_CASES]
        for case, hw in runs:
            errors = run_case(retroarch, root, driver, case, hw)
            print('%s %s/%s%s' % ('FAIL' if errors else 'pass', driver,
                                  case[0], '' if hw == 'off' else ' (hw ' + hw + ')'))
            for e in errors:
                print('    ' + e)
            failed += bool(errors)
        for case in [c for c in EXACT_CASES
                     if c[3] in EXACT_HW.get(driver, ())]:
            hw = '' if case[3] == 'off' else ' (hw ' + case[3] + ')'
            errors, rows = run_exact_case(
                retroarch, root, driver, case,
                path='glCopyImageSubData' if driver == 'glcore' else None)
            print('%s %s/%s%s' % ('FAIL' if errors else 'pass', driver,
                                  case[0], hw))
            for e in errors:
                print('    ' + e)
            failed += bool(errors)
            if driver != 'glcore':
                continue
            errors, blit_rows = run_exact_case(
                retroarch, root, driver, case, '-blit', NO_COPY_IMAGE,
                'framebuffer blits')
            if rows and blit_rows and rows != blit_rows:
                errors.append('screenshot differs from glCopyImageSubData\'s')
            print('%s %s/%s-blit%s' % ('FAIL' if errors else 'pass', driver,
                                       case[0], hw))
            for e in errors:
                print('    ' + e)
            failed += bool(errors)
        for case in (MEMORY_CASES if driver in MEMORY_HW else []):
            errors, peak = run_memory_case(retroarch, root, driver, case)
            print('%s %s/%s (hw %s, GPU %s MiB)'
                  % ('FAIL' if errors else 'pass', driver, case[0],
                     MEMORY_HW[driver], '?' if peak is None else '%d' % peak))
            for e in errors:
                print('    ' + e)
            failed += bool(errors)
        for case in (MENU_CASES if driver != 'gl' else []):
            errors = run_menu_case(retroarch, root, driver, case)
            print('%s %s/%s' % ('FAIL' if errors else 'pass', driver,
                                case[0]))
            for e in errors:
                print('    ' + e)
            failed += bool(errors)
        for menu, case in ([(m, c) for m in sorted(POINTER_MENUS)
                            for c in POINTER_CASES]
                           if driver != 'gl' else []):
            errors = run_pointer_case(retroarch, root, driver, menu, case)
            print('%s %s/%s-%s' % ('FAIL' if errors else 'pass', driver,
                                   case[0], menu))
            for e in errors:
                print('    ' + e)
            failed += bool(errors)
    print('%d failed' % failed)
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
