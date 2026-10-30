#!/usr/bin/env python3
"""End-to-end check of RetroArch's headset output.

Runs the video_views test core with Headset Output on against Monado's
simulated headset in headless gamescope, through the test API layer in
samples/openxr/test_layer, which records every xrEndFrame's quads and
saves the images they show. Not run in CI: it needs a GPU, gamescope,
Monado, and a RetroArch built with HAVE_OPENXR.

Usage: run.py <retroarch> <out dir> <monado.env> [--validate|--gl] [case ...]

The custom-repeat case needs the opt-in shipping harness. Build it with
python3 samples/settings/video_screens/build.py --runtime --output-dir <build>
and pass <build>/video_screens_test as <retroarch>. The runner supplies
RA_SCREEN_TEST_SCRIPT and RA_SCREEN_TEST_ACK for bounded, acknowledged
main-thread edits; the ordinary frontend and its objects are unchanged.

Build the test core and layer first (make -C samples/cores/video_views,
make -C samples/openxr/test_layer). monado.env.example here runs
Monado's null compositor and simulated headset; RetroArch reaches Monado
through XR_RUNTIME_JSON, so the system's active runtime is not used.
gamescope and Monado get none of the desktop's display, session bus or
runtime dir.

monado.env holds KEY=VALUE lines (values may be double-quoted): MODE
(inprocess or service), RUNTIME_JSON (Monado's manifest), CLIENT_ENV and
SERVICE_ENV (space-separated KEY=VALUE pairs), SERVICE_SOCKET and
VALIDATION_BASELINE, relative to monado.env, by default
validation-baseline.txt here: the messages raised without RetroArch's
headset code, one per line, a VUID and then text the message must
contain (an object's name), so a VUID alone does not hide RetroArch's
own.
--gl runs the desktop GL lanes instead: chosen cases cloned for glcore on
EGL/Wayland and on GLX, and gl on EGL/X11, each named <lane>-<case>; the
layer reads their GL images itself. Validation is Vulkan's alone.
--validate runs RetroArch under the Vulkan validation layer, which must
be installed, and fails a case on any other message, or when none of the
baseline's appear: the window's own come every run, so without them the
layer did not load.
"""

import copy
import ctypes
import json
import math
import os
import re
import shlex
import shutil
import signal
import struct
import subprocess
import sys
import time
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'samples', 'cores', 'video_views', 'e2e'))
from run import (CORE, free_port, isolated_env, read_png, send,  # noqa: E402
                 write_cfg)

PRESET = os.path.join(HERE, 'output_size.slangp')
LAYER_DIR = os.path.join(ROOT, 'samples', 'openxr', 'test_layer')
LAYER = 'XR_APILAYER_RETROARCH_test_recorder'
# The same layer offering the Steam Frame's controller extension.
FRAME_LAYER = 'XR_APILAYER_RETROARCH_test_frame'
SYSTEM_LAYERS = '/usr/share/openxr/1/api_layers/explicit.d'
W, H = 1600, 960
TOL = 8
POS = 0.01
BLEND = 2  # XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT
VUID = re.compile(r'(VUID-[A-Za-z0-9_-]+|UNASSIGNED-[A-Za-z0-9_.-]+)')
MESSAGE = re.compile(r'Validation (Error|Warning|Performance Warning):')
DENSITY = re.compile(r'\[OpenXR\] (\d+) pixels across (\d+) degrees per eye')
DESTROY = re.compile(r'\[video_views\] context_destroy waited on the device '
                     r'from (\d+) to (\d+) us')
# The Monado client's sockets live under a relative XDG_RUNTIME_DIR in
# the case's directory: an absolute scratch path overflows sun_path.
RUNTIME_DIR = 'run'

RED, BLUE, GREEN = (255, 0, 0), (0, 0, 255), (0, 255, 0)
YELLOW, WHITE, GREY = (255, 255, 0), (255, 255, 255), (32, 32, 32)
MAGENTA = (255, 0, 255)
BLACK = (0, 0, 0)


def read_env(path):
    env = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith('#') and '=' in line:
                k, v = line.split('=', 1)
                env[k.strip()] = v.strip().strip('"')
    return env


def pairs(s):
    return dict(p.split('=', 1) for p in s.split() if '=' in p)


def read_baseline(path):
    entries = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith('#'):
                parts = line.split(None, 1)
                entries.append((parts[0], parts[1] if len(parts) > 1 else ''))
    return entries


def unexpected(log, baseline):
    """The VUIDs of validation messages the baseline does not cover, and
    how many it does. A message runs from its first line to a blank line
    or the next one."""
    blocks, cur = [], None
    for line in log.splitlines():
        if MESSAGE.search(line):
            if cur:
                blocks.append(cur)
            cur = [line]
        elif cur is not None:
            if line.strip():
                cur.append(line)
            else:
                blocks.append(cur)
                cur = None
    if cur:
        blocks.append(cur)
    extra = set()
    known = 0
    for b in blocks:
        text = '\n'.join(b)
        m = VUID.search(text)
        vuid = m.group(1) if m else b[0].strip()
        if any(vuid == v and (t(text) if callable(t) else t in text)
               for v, t in baseline):
            known += 1
        else:
            extra.add(vuid)
    return sorted(extra), known


class Result(object):
    def __init__(self, d):
        self.dir = d
        self.log = ''
        self.frames = []
        self.releases = []
        self.chains = {}
        self.marks = {}
        self.screen_frames = {}
        self.shot = None
        self.events = []
        # What each 'until' step that timed out waited for.
        self.missed = []
        # A pixel of the window inside the 2D map's top screen.
        self.top_px = (800, 240)
        # Seconds after a video reinit's hotkey that its new session's
        # first frames are not held to what the session then shows.
        self.reinit_grace = 0.5


def load(res):
    path = os.path.join(res.dir, 'xr', 'frames.jsonl')
    if os.path.exists(path):
        with open(path) as f:
            for line in f:
                try:
                    e = json.loads(line)
                except ValueError:
                    continue  # a line cut short at exit
                res.events.append(e)
                if e['ev'] == 'frame':
                    res.frames.append(e)
                elif e['ev'] == 'release':
                    res.releases.append(e)
                elif e['ev'] == 'swapchain':
                    res.chains[e['sc']] = e
    # stdout (the validation layer's messages) and RetroArch's log file.
    for name in ('run.log', 'retroarch.log'):
        p = os.path.join(res.dir, name)
        if os.path.exists(p):
            with open(p, errors='replace') as f:
                res.log += f.read()


def stop(p):
    if p is None or p.poll() is not None:
        return
    os.killpg(p.pid, signal.SIGTERM)
    try:
        p.wait(5)
    except subprocess.TimeoutExpired:
        os.killpg(p.pid, signal.SIGKILL)
        p.wait(5)


def wait_shot(d):
    deadline = time.time() + 10
    while time.time() < deadline:
        time.sleep(0.5)
        pngs = [x for x in os.listdir(os.path.join(d, 'shots'))
                if x.endswith('.png')]
        if pngs:
            time.sleep(1)
            return os.path.join(d, 'shots', sorted(pngs)[0])
    return None


def wait_frame(d, pick, timeout):
    """Whether a frame recorded from now on, within timeout seconds, has
    something pick finds."""
    path = os.path.join(d, 'xr', 'frames.jsonl')
    deadline = time.monotonic() + timeout
    pos = os.path.getsize(path) if os.path.exists(path) else 0
    part = b''
    while time.monotonic() < deadline:
        data = b''
        if os.path.exists(path):
            with open(path, 'rb') as f:
                f.seek(pos)
                data = f.read()
        pos += len(data)
        lines = (part + data).split(b'\n')
        part = lines.pop()
        for line in lines:
            try:
                e = json.loads(line)
            except ValueError:
                continue  # the rest of a line begun before the wait
            if isinstance(e, dict) and e.get('ev') == 'frame' and pick(e):
                return True
        time.sleep(0.01)
    return False


def write_overlay(d):
    """An overlay whose top-left sixteenth is opaque magenta, and the
    settings that show it over the menu."""
    rows = b''
    for y in range(16):
        rows += b'\0' + b''.join(bytes(MAGENTA + (255,)) if x < 4 and y < 4
                                  else bytes(4) for x in range(16))

    def chunk(kind, data):
        return (struct.pack('>I', len(data)) + kind + data
                + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff))
    with open(os.path.join(d, 'corner.png'), 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n'
                + chunk(b'IHDR', struct.pack('>IIBBBBB', 16, 16, 8, 6, 0, 0, 0))
                + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))
    cfg = os.path.join(d, 'corner.cfg')
    with open(cfg, 'w') as f:
        f.write('overlays = 1\noverlay0_overlay = "corner.png"\n'
                'overlay0_full_screen = true\noverlay0_normalized = true\n'
                'overlay0_descs = 0\n')
    return {'input_overlay_enable': 'true', 'input_overlay': cfg,
            'input_overlay_hide_in_menu': 'false',
            'input_overlay_opacity': '1.0'}


def capture_errors(res, case):
    """A GL lane's session on the binding its context driver makes, and
    every image the layer read."""
    errors = []
    failed = [e for e in res.events if e.get('ev') == 'capture_failed']
    if failed:
        errors.append('%d GL images could not be read (first: swapchain %d, '
                      'status %d, error %d)' % (len(failed), failed[0]['sc'],
                                                failed[0]['status'],
                                                failed[0]['error']))
    if case.get('lane'):
        want = 'glx' if case.get('context') == 'x' else 'egl'
        got = [e.get('graphics') for e in res.events
               if e.get('ev') == 'session']
        if got and any(g != want for g in got):
            errors.append('sessions on %s, want %s' % (', '.join(got), want))
    return errors


def run_case(retroarch, root, monado, case, validate):
    d = os.path.realpath(os.path.join(root, case['name']))
    if os.path.commonpath([root, d]) != root or d == root:
        raise RuntimeError('refusing to touch ' + d)
    shutil.rmtree(d, ignore_errors=True)
    for sub in ('shots', 'xr', RUNTIME_DIR, 'config'):
        os.makedirs(os.path.join(d, sub))
    os.chmod(os.path.join(d, RUNTIME_DIR), 0o700)

    # A screen carried 5 cm or more snaps: in the 3DS layout the other
    # screen's edge, 3.2 cm off, would pull it 2.8 cm. The snap cases
    # turn it on. Edit Smoothing too: a smoothed screen lags the scripted
    # hand; the smoothing cases turn it on. The menu in front of screen
    # 1, as the edit cases expect; custom-menu-spot checks it off.
    settings = {'video_openxr_enable': 'true',
                'video_openxr_distance': '1.8',
                'video_openxr_width': '1.6',
                'video_openxr_edit_snap': 'false',
                'video_openxr_edit_smoothing': '0',
                'video_openxr_menu_follow': 'true'}
    if case.get('overlay'):
        settings.update(write_overlay(d))
    settings.update(case.get('settings', {}))
    if case.get('context'):
        settings['video_context_driver'] = case['context']
    if case.get('context') == 'wayland':
        # gamescope sends its own Wayland clients no fullscreen size: a
        # window of the output's instead. A build with SDL3 takes the
        # size with the saved position.
        size = case.get('screen', (W, H))
        settings.update({'video_fullscreen': 'false',
                         'video_window_show_decorations': 'false',
                         'video_window_custom_size_enable': 'true',
                         'video_window_save_positions': 'true',
                         'video_windowed_position_width': str(size[0]),
                         'video_windowed_position_height': str(size[1])})
    screen_script = os.path.join(d, 'screens.txt')
    screen_ack = os.path.join(d, 'screens.ack')
    open(screen_script, 'w').close()
    port = free_port()
    cfg = os.path.join(d, 'retroarch.cfg')
    write_cfg(cfg, d, case.get('driver', 'vulkan'), settings, port)
    options = {'video_views_test_map': case.get('map', '3ds'),
               'video_views_test_hw': 'off'}
    options.update(case.get('options', {}))
    with open(os.path.join(d, 'opts.cfg'), 'w') as f:
        for k in sorted(options):
            f.write('%s = "%s"\n' % (k, options[k]))
    script = os.path.join(d, 'script.txt')
    with open(script, 'w') as f:
        f.write(case.get('script', ''))

    runtime = monado['RUNTIME_JSON']
    if case.get('no_runtime'):
        # A manifest whose library is missing: the loader fails instead
        # of falling back to another runtime.
        runtime = os.path.join(d, 'missing_runtime.json')
        with open(runtime, 'w') as f:
            json.dump({'file_format_version': '1.0.0',
                       'runtime': {'name': 'missing',
                                   'library_path':
                                   '/nonexistent/libopenxr_missing.so'}}, f)

    child = {
        'XR_RUNTIME_JSON': runtime,
        'XR_API_LAYER_PATH': LAYER_DIR + ':' + SYSTEM_LAYERS,
        'XR_ENABLE_API_LAYERS': case.get('layer', LAYER),
        'RA_XR_LAYER_OUT': os.path.join(d, 'xr'),
        'RA_XR_LAYER_SNAP_EVERY': '30',
        'RA_XR_LAYER_SCRIPT': script,
        'XDG_RUNTIME_DIR': RUNTIME_DIR,
        'WAYLAND_DISPLAY': 'openxr-e2e-no-socket',
    }
    child.update(RA_SCREEN_TEST_SCRIPT=screen_script,
                 RA_SCREEN_TEST_ACK=screen_ack)
    child.update(case.get('env', {}))
    child.update(pairs(monado.get('CLIENT_ENV', '')))
    if validate:
        child['VK_INSTANCE_LAYERS'] = 'VK_LAYER_KHRONOS_validation'
    exports = ' '.join('%s=%s' % (k, shlex.quote(v))
                       for k, v in sorted(child.items()))
    # Never reach the desktop: refuse the real display; what the runtime
    # and RetroArch open lives in this case's directory. The gamescope
    # WSI layer still needs gamescope's own socket, by an absolute path in
    # gamescope's private runtime dir.
    helper = ''
    if case.get('unfocus'):
        helper = ' %s %s --unfocus-when %s &' % (
            shlex.quote(sys.executable), shlex.quote(os.path.abspath(__file__)),
            shlex.quote(os.path.join(d, 'unfocus')))
    # A Wayland lane's window is gamescope's own client, on its socket.
    wayland = case.get('context') == 'wayland'
    own_socket = (' [ -n "${GAMESCOPE_WAYLAND_DISPLAY:-}" ] || exit 97;'
                  ' export WAYLAND_DISPLAY="$GAMESCOPE_WAYLAND_DISPLAY";'
                  if wayland else '')
    guard = ('case "$DISPLAY" in ""|:0) echo "refusing DISPLAY=$DISPLAY" >&2;'
             ' exit 99;; esac; cd ' + shlex.quote(d) + ' || exit 98;' + helper +
             ' case "${GAMESCOPE_WAYLAND_DISPLAY:-}" in ""|/*) ;;'
             ' *) GAMESCOPE_WAYLAND_DISPLAY="$XDG_RUNTIME_DIR/'
             '$GAMESCOPE_WAYLAND_DISPLAY"; export GAMESCOPE_WAYLAND_DISPLAY;;'
             ' esac; export ' + exports + ';' + own_socket +
             ' exec "$0" "$@"')

    w, h = case.get('screen', (W, H))
    cmd = ['gamescope', '--backend', 'headless'] + (
           ['--expose-wayland'] if wayland else []) + [
           '-w', str(w), '-h', str(h), '-W', str(w), '-H', str(h),
           '-r', '60', '--', 'sh', '-c', guard,
           retroarch, '--config', cfg, '-L', case.get('core', CORE),
           '--verbose'] + case.get('args', [])
    if case.get('content'):
        cmd.append(case['content'])

    # gamescope and the service get none of the desktop's display, bus or
    # runtime dir.
    env = isolated_env()
    svc = svc_log = log = p = None
    res = Result(d)
    res.top_px = case.get('top_px', res.top_px)
    res.reinit_grace = case.get('reinit_grace', res.reinit_grace)
    try:
        if monado.get('MODE') == 'service':
            svc_env = dict(env)
            svc_env['WAYLAND_DISPLAY'] = 'openxr-e2e-no-socket'
            svc_env['XDG_RUNTIME_DIR'] = RUNTIME_DIR
            # Monado's and libsurvive's config stay out of ~/.config.
            svc_env['XDG_CONFIG_HOME'] = os.path.join(d, 'config')
            svc_env.update(pairs(monado.get('SERVICE_ENV', '')))
            svc_log = open(os.path.join(d, 'service.log'), 'w')
            svc = subprocess.Popen(['monado-service'], cwd=d, env=svc_env,
                                   stdout=svc_log, stderr=subprocess.STDOUT,
                                   stdin=subprocess.DEVNULL,
                                   start_new_session=True)
            sock = os.path.join(d, RUNTIME_DIR, monado.get(
                'SERVICE_SOCKET', 'monado_comp_ipc'))
            deadline = time.time() + 10
            while time.time() < deadline and not os.path.exists(sock):
                time.sleep(0.1)

        log = open(os.path.join(d, 'run.log'), 'w')
        p = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT,
                             env=env, start_new_session=True)
        for kind, arg in case['steps']:
            if kind == 'wait':
                time.sleep(arg)
            elif kind == 'send':
                send(arg, port)
            elif kind == 'mark':
                res.marks[arg] = time.monotonic()
            elif kind == 'script':
                tmp = script + '.tmp'
                with open(tmp, 'w') as f:
                    f.write(arg + '\n')
                os.replace(tmp, script)
            elif kind == 'screen':
                serial = str(arg[0])
                tmp = screen_script + '.tmp'
                with open(tmp, 'w') as f:
                    f.write(' '.join(map(str, arg)))
                os.replace(tmp, screen_script)
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline:
                    if os.path.exists(screen_ack):
                        with open(screen_ack) as f:
                            ack = f.read().split()
                            if len(ack) == 2 and ack[0] == serial:
                                res.screen_frames[int(serial)] = int(ack[1])
                                break
                    time.sleep(0.01)
                else:
                    raise RuntimeError('screen edit not acknowledged; use the '
                                       'video_screens --runtime test binary')
            elif kind == 'unfocus':
                open(os.path.join(d, 'unfocus'), 'w').close()
            elif kind == 'touch':
                open(os.path.join(d, arg), 'w').close()
            elif kind == 'wait_exit':
                try:
                    p.wait(arg)
                    res.marks['exited'] = time.monotonic()
                except subprocess.TimeoutExpired:
                    pass
            elif kind == 'shot':
                send('SCREENSHOT', port)
                res.shot = wait_shot(d)
            elif kind == 'until':
                what, pick, timeout = arg
                if not wait_frame(d, pick, timeout):
                    res.missed.append('%s within %g s' % (what, timeout))
        # Without its command interface it cannot be told to quit: stop()
        # ends it.
        if settings.get('network_cmd_enable') != 'false':
            send('QUIT', port)
            try:
                p.wait(10)
            except subprocess.TimeoutExpired:
                res.marks['hung'] = time.monotonic()
    finally:
        stop(p)
        stop(svc)
        if log:
            log.close()
        if svc_log:
            svc_log.close()
        runtime_dir = env['XDG_RUNTIME_DIR']
        if os.path.basename(runtime_dir).startswith('ra-e2e-'):
            shutil.rmtree(runtime_dir, ignore_errors=True)
    load(res)
    with open(os.path.join(d, 'marks.json'), 'w') as f:
        json.dump({'marks': res.marks, 'screen_frames': res.screen_frames}, f)
    return res


def quads(frame):
    return [q for q in frame['layers'] if q['type'] == 'quad']


def last_snap(res):
    for fr in reversed(res.frames):
        if fr['snap'] and quads(fr):
            return fr
    return None


def image(res, fr, q):
    return os.path.join(res.dir, 'xr', 'snap_%d_sc%d_l%d.png'
                        % (fr['n'], q['sc'], q['layer']))


def colour(path, fx, fy):
    w, h, bpp, rows = read_png(path)
    x = min(w - 1, int(fx * w))
    y = min(h - 1, int(fy * h))
    px = rows[y][x * bpp:x * bpp + bpp]
    return tuple(px[:3]), (px[3] if bpp == 4 else 255)


def near(got, want, tol=TOL):
    return all(abs(a - b) <= tol for a, b in zip(got, want))


def at(q, want):
    return all(abs(a - b) <= POS for a, b in zip(q['pose'][:3], want))


def sized(q, want):
    return all(abs(a - b) <= POS for a, b in zip(q['size'], want))


def facing(q, want):
    """The same orientation, whatever the quaternion's sign."""
    return abs(sum(a * b for a, b in zip(q['pose'][3:], want))) >= 0.999


def no_quads(res):
    return ['no frame with quads and images (see %s)'
            % os.path.join(res.dir, 'run.log')]


def projections(frame):
    return [x for x in frame['layers'] if x['type'] == 'projection']


def last_projection_snap(res):
    for fr in reversed(res.frames):
        if fr['snap'] and projections(fr):
            return fr
    return None


HMD_RE = re.compile(r'\[video_views\] hmd=(\d) recommended=(\d+)x(\d+)')
STATUS_RE = re.compile(r'\[video_views\] presents=(\d) stereo=(\d) '
                       r'accepted=(\d) views=(\d+)')
EYES_RE = re.compile(r'\[video_views\] vr eyes frame=(\d+) '
                     r'left=([-\d.]+),([-\d.]+),'
                     r'([-\d.]+) right=([-\d.]+),([-\d.]+),([-\d.]+) '
                     r'fov=([-\d.]+),([-\d.]+),([-\d.]+),([-\d.]+) '
                     r'rfov=([-\d.]+),([-\d.]+),([-\d.]+),([-\d.]+)')
FLAGS_RE = re.compile(r'\[video_views\] vr frame flags=(\d+)')
RECENTERED, TARGET_RESIZED = 1, 2
EYE_W, EYE_H = 400, 240  # the 3DS map's top screen, one eye
EXACT = 1.5e-5  # the core and the layer both log five decimals


def frame_tag(path, v, size):
    """The frame number's low sixteen bits along the bottom of an eye
    the core drew, or None where a block is neither white nor black."""
    tag = 0
    for b in range(16):
        c, _ = colour(path, (24 + 20 * b) * v['rect'][2] / EYE_W / size[0],
                      (EYE_H - 16) * v['rect'][3] / EYE_H / size[1])
        if near(c, WHITE):
            tag |= 1 << b
        elif not near(c, BLACK):
            return None
    return tag


def tracked_size(res):
    """The recommended eye size the core was last given with HMD set."""
    hmd = [m for m in HMD_RE.findall(res.log) if m[0] == '1']
    return (int(hmd[-1][1]), int(hmd[-1][2])) if hmd else None


def check_paused(res, start, end):
    """The core ran no frame between the marks start and end: the eyes
    of every snapped projection between them carry one frame tag, and
    there is one."""
    size = tracked_size(res)
    if not size:
        return ['the core never saw HMD']
    t0, t1 = res.marks[start] * 1e6, res.marks[end] * 1e6
    tags = set(frame_tag(image(res, f, v), v, size) for f in res.frames
               if f['snap'] and t0 < f['t_us'] < t1
               for p in projections(f) for v in p['views'])
    if not tags:
        return ['no snapped projection between %s and %s' % (start, end)]
    if len(tags) != 1 or None in tags:
        return ['the core ran between %s and %s: the eyes show frames %s'
                % (start, end, sorted(tags, key=str))]
    return []


def check_tracked(space='local', beside=True):
    """HMD reaches the core, its tracked map is taken, and its two eyes
    are a projection behind the other screen: one swapchain per eye at
    the recommended size, each eye drawn 1:1 from its top-left, with the
    poses and fields of view the core drew that frame from, in the space
    it asked for (LOCAL where the runtime has no STAGE). beside False:
    the eyes are all the headset shows, as gl's window gives way."""
    def check(res):
        errors = []
        size = tracked_size(res)
        if not size or not size[0] or not size[1]:
            return ['the core never saw HMD with an eye size: %s'
                    % HMD_RE.findall(res.log)]
        status = STATUS_RE.findall(res.log)
        if not status or status[-1][2] != '1':
            errors.append('the tracked map was not taken: %s' % status[-1:])
        fr = last_projection_snap(res)
        if not fr:
            return errors + ['no snapped frame with a projection (see %s)'
                             % os.path.join(res.dir, 'run.log')]
        if fr['layers'][0]['type'] != 'projection':
            errors.append('the projection is not behind the screens: %s'
                          % [x['type'] for x in fr['layers']])
        p = projections(fr)[0]
        want = space
        if space == 'stage' and \
                'No STAGE space: tracked content is seated.' in res.log:
            want = 'local'
        if p['space'] != want:
            errors.append('the projection is in %s, want %s'
                          % (p['space'], want))
        if len(p['views']) != 2:
            return errors + ['%d projection views' % len(p['views'])]
        lv, rv = p['views']
        if lv['sc'] == rv['sc']:
            errors.append('both eyes come from one swapchain')
        for v, what in ((lv, 'left'), (rv, 'right')):
            sc = res.chains.get(v['sc'])
            if not sc or (sc['w'], sc['h']) != size:
                errors.append('the %s eye\'s swapchain is %s, want %dx%d'
                              % (what, sc and (sc['w'], sc['h']), size[0],
                                 size[1]))
            if v['rect'] != [0, 0, min(EYE_W, size[0]), min(EYE_H, size[1])]:
                errors.append('the %s eye shows %s' % (what, v['rect']))
            if v['layer'] != 0:
                errors.append('the %s eye shows array layer %d'
                              % (what, v['layer']))
        for v, colour_want, what in ((lv, RED, 'left eye'),
                                     (rv, BLUE, 'right eye')):
            path = image(res, fr, v)
            c, _ = colour(path, EYE_W * 0.5 / size[0], EYE_H * 0.5 / size[1])
            if not near(c, colour_want):
                errors.append('%s shows %s, want %s' % (what, c, colour_want))
            c, _ = colour(path, 2.0 / size[0], 2.0 / size[1])
            if not near(c, WHITE):
                errors.append('%s top-left is %s, want white' % (what, c))
        q = [x for x in quads(fr) if not x['flags'] & BLEND]
        if not beside:
            if quads(fr):
                errors.append('quads beside the projection: %s'
                              % [x['eye'] for x in quads(fr)])
        elif [x['eye'] for x in q] != ['both']:
            errors.append('screens beside the projection: %s'
                          % [x['eye'] for x in q])
        else:
            c, _ = colour(image(res, fr, q[0]), 0.5, 0.5)
            if not near(c, YELLOW):
                errors.append('the bottom screen shows %s' % (c,))
        eyes = [(int(m[0]), [float(x) for x in m[1:]])
                for m in EYES_RE.findall(res.log)]
        tags = [frame_tag(image(res, fr, v), v, size) for v in (lv, rv)]
        if not eyes:
            errors.append('the core logged no eye poses')
        elif None in tags or tags[0] != tags[1]:
            errors.append('the eyes are tagged frames %s' % tags)
        else:
            # The headset moves a few millimetres a sample: the poses
            # shown must be the ones logged for the frame shown, which
            # its tag numbers (a run is far short of 65536 frames).
            frame = tags[0]
            upto = [x for x in eyes if x[0] <= frame]
            if not upto:
                return errors + ['no eye poses logged by frame %d' % frame]
            e = max(upto, key=lambda x: x[0])[1]
            for v, pos, what in ((lv, e[0:3], 'left'), (rv, e[3:6], 'right')):
                if any(abs(a - b) > EXACT for a, b in zip(v['pose'][:3], pos)):
                    errors.append('the %s eye is shown at %s, drawn at %s '
                                  'in frame %d' % (what, v['pose'][:3], pos,
                                                   frame))
            for v, drawn, what in ((lv, e[6:10], 'left'),
                                   (rv, e[10:14], 'right')):
                tans = [abs(math.tan(a)) for a in v['fov']]
                if any(abs(a - b) > 0.01 for a, b in zip(tans, drawn)):
                    errors.append('the %s eye is shown with tangents %s, '
                                  'drawn with %s' % (what, tans, drawn))
        return errors
    return check


def check_tracked_flat(res, native=False):
    """A core's eyes going flat while it runs: the projection goes, and
    their screen is a quad pair again. native: as check_screens's."""
    flat = res.marks.get('flat', 0) * 1e6
    errors = []
    if not any(projections(f) for f in res.frames if f['t_us'] < flat):
        errors.append('no projection before the core went flat')
    if '[video_views] vr flat' not in res.log:
        return errors + ['the core never went flat']
    after = [f for f in res.frames if f['t_us'] > flat + 1e6]
    if any(projections(f) for f in after):
        errors.append('the projection stayed after the core went flat')
    fr = last_snap(res)
    if not after or not fr or fr['t_us'] <= flat + 1e6:
        return errors + ['no snapped frame after the core went flat']
    return errors + check_screens(native=native)(res)


SLOT_REFUSED = re.compile(r'\[OpenXR\] No (\d+)x(\d+) swapchain with 2 layer')


def check_tracked_fallback(res):
    """Tracked eyes whose projection the runtime refuses stay a quad
    pair, as a flat map's."""
    size = tracked_size(res)
    if not size:
        return ['the core never saw HMD']
    errors = []
    if '%dx%d' % size not in ['%sx%s' % m for m in
                              SLOT_REFUSED.findall(res.log)]:
        errors.append('the projection slot was not refused: %s'
                      % SLOT_REFUSED.findall(res.log))
    status = STATUS_RE.findall(res.log)
    if not status or status[-1][2] != '1':
        errors.append('the tracked map was not taken: %s' % status[-1:])
    if any(projections(f) for f in res.frames):
        errors.append('a projection was shown')
    return errors + check_screens()(res)


def check_tracked_refused(res):
    """A TRACKED map without a pair of eyes is refused while HMD is set."""
    size = tracked_size(res)
    status = STATUS_RE.findall(res.log)
    if not size:
        return ['the core never saw HMD']
    if not status or status[-1][2] != '0':
        return ['the map without eyes was taken: %s' % status[-1:]]
    if any(projections(f) for f in res.frames):
        return ['a projection was shown']
    return []


def check_flat_size(res, native=False):
    """A map without TRACKED under HMD is screens, as before, and the core
    is still given the eye size. native: as check_screens's."""
    errors = check_screens(native=native)(res)
    size = tracked_size(res)
    if not size or not size[0] or not size[1]:
        errors.append('no eye size with HMD: %s' % HMD_RE.findall(res.log))
    if any(projections(f) for f in res.frames):
        errors.append('a projection was shown for a map without TRACKED')
    return errors


def check_tracked_menu(res):
    """The menu opens in front of the tracked eyes, which stay."""
    opened = res.marks.get('menu', 0) * 1e6
    late = [f for f in res.frames if f['t_us'] > opened + 2e6]
    menu = [f for f in late if any(x['type'] == 'quad' and x['flags'] & BLEND
                                   for x in f['layers'])]
    if not menu:
        return ['no frame with the menu after it opened']
    errors = []
    for f in menu[-3:]:
        kinds = [x['type'] for x in f['layers']]
        if not kinds or kinds[0] != 'projection':
            errors.append('frame %d shows %s, want the projection first'
                          % (f['n'], kinds))
    return errors


def recenter_flags(res):
    """How many frames the core saw RECENTERED in: it logs a frame's
    flags whenever there are any."""
    return sum(1 for m in FLAGS_RE.finditer(res.log)
               if int(m.group(1)) & RECENTERED)


def same_frame_errors(res):
    """After the recenter mark, the first headset frame where the screens
    move is the first where the seated world's space moves in STAGE."""
    def moves(a, b):
        return (a is not None and b is not None
                and any(abs(x - y) > 1e-3 for x, y in zip(a[:3], b[:3])))
    asked = res.marks['recenter'] * 1e6
    screens = world = was_q = was_s = None
    for f in res.frames:
        q = quads(f)[0]['pose'] if quads(f) else None
        s = projections(f)[0].get('stage') if projections(f) else None
        if f['t_us'] > asked:
            if screens is None and moves(was_q, q):
                screens = f['n']
            if world is None and moves(was_s, s):
                world = f['n']
        was_q = q if q is not None else was_q
        was_s = s if s is not None else was_s
    if screens is None or world is None:
        return ['after the recenter the screens moved at frame %s, the '
                'world at frame %s' % (screens, world)]
    if screens != world:
        return ['the screens moved at frame %d, the seated world at frame '
                '%d' % (screens, world)]
    return []


def check_tracked_recenter(res, screens=True):
    """The Recenter hotkey moves the screens and a seated world together,
    in one headset frame: the core hears RECENTERED once, and the
    projection goes on. screens False: the eyes are all there is, as
    with gl, and the world alone moves."""
    errors = []
    if 'Headset recenter requested' not in res.log:
        errors.append('the network command did not reach the hotkey')
    n = recenter_flags(res)
    if n != 1:
        errors.append('the core saw RECENTERED %d times, want 1' % n)
    shown = [f for f in res.frames if projections(f)]
    late = [f for f in shown
            if f['t_us'] > res.marks['recenter'] * 1e6 + 1e6]
    if not late:
        errors.append('no projection after the recenter')
    if screens:
        return errors + same_frame_errors(res)
    asked = res.marks['recenter'] * 1e6
    was = [projections(f)[0].get('stage') for f in shown
           if f['t_us'] < asked]
    now = [projections(f)[0].get('stage') for f in late]
    if not was or not now or not was[-1] or not now[-1]:
        errors.append('the eyes\' space was not located in STAGE on both '
                      'sides of the recenter')
    elif same_place([was[-1]], [now[-1]]):
        errors.append('the seated world did not move with the recenter')
    return errors


def check_tracked_recenter_eyes(res):
    return check_tracked_recenter(res, screens=False)


# The anchor at the scripted head 0.5, 0.2, 0.3, turned 90 degrees left.
TURNED = [0.5, 0.2, 0.3, 0.0, 0.70711, 0.0, 0.70711]


def check_tracked_recenter_paused(space, screens=True):
    """A recenter while tracked content is paused keeps its frozen frame
    on screen, with the poses the core drew it from. In space 'local' the
    seated world moves with the screens to the turned head's anchor, in
    the same headset frame; in 'stage' it stays in the room. The core
    hears RECENTERED once. screens False: the eyes are all there is, as
    with gl."""
    def check(res):
        errors = []
        if 'Headset recenter requested' not in res.log:
            errors.append('the network command did not reach the hotkey')
        errors += check_paused(res, 'recenter', 'resume')
        asked = res.marks['recenter'] * 1e6
        resumed = res.marks['resume'] * 1e6
        paused = [f for f in res.frames
                  if asked + 5e5 < f['t_us'] < resumed and f['layers']]
        if not paused:
            return errors + ['no frames between the recenter and the unpause']
        blank = [f['n'] for f in paused if not projections(f)]
        if blank:
            errors.append('%d frames without the projection while paused, '
                          'first %d' % (len(blank), blank[0]))
        before = [f for f in res.frames
                  if f['t_us'] < asked and projections(f)
                  and (quads(f) or not screens)]
        if not before or not projections(before[-1])[0].get('stage'):
            return errors + ['no projection located in STAGE before the '
                             'recenter']
        b = projections(before[-1])[0]
        # Where the space the eyes are shown in should be in STAGE.
        want = b['stage']
        if space == 'local':
            want = in_stage(b, {'pose': TURNED})
        shown = [f for f in paused if projections(f)]
        for f in shown:
            p = projections(f)[0]
            if p['space'] != space:
                errors.append('frame %d shows the eyes in %s, want %s'
                              % (f['n'], p['space'], space))
                break
            if any(abs(x - y) > EXACT for v, w in zip(p['views'], b['views'])
                   for x, y in zip(v['pose'], w['pose'])):
                errors.append('frame %d shows the eyes at other poses than '
                              'the paused frame\'s' % f['n'])
                break
            if not same_place([want], [p.get('stage')]):
                errors.append('frame %d shows the eyes\' space at %s in '
                              'STAGE, want %s' % (f['n'], p.get('stage'),
                                                  want))
                break
        snapped = [f for f in shown if f['snap']]
        if snapped:
            errors += shown_as_drawn(res, snapped[-1])
        # The screens went to the turned anchor either way.
        if screens:
            q0 = quads(before[-1])[0]
            q1 = [x for x in quads(paused[-1])
                  if x['eye'] == q0['eye'] and x['flags'] == q0['flags']]
            moved = in_stage({'stage': TURNED}, q0)
            if not q1 or not same_place([moved], [q1[0]['pose']]):
                errors.append('the screen at %s did not go to %s with the '
                              'recenter: %s' % (q0['pose'], moved,
                                                q1 and q1[0]['pose']))
        if not [f for f in res.frames
                if f['t_us'] > resumed + 1e6 and projections(f)]:
            errors.append('no projection after the unpause')
        if recenter_flags(res) != 1:
            errors.append('the core saw RECENTERED %d times, want 1'
                          % recenter_flags(res))
        if space == 'local' and screens:
            errors += same_frame_errors(res)
        return errors
    return check


def check_tracked_runtime_recenter(res):
    """The runtime's LOCAL change is a recenter too; a STAGE change is
    not one for a seated core."""
    errors = []
    n = recenter_flags(res)
    if n != 1:
        errors.append('the core saw RECENTERED %d times, want 1 (for '
                      'the LOCAL change)' % n)
    if res.log.count('[OpenXR] Recentered by the runtime.') != 1:
        errors.append('the runtime\'s recenter was not logged once')
    return errors


def check_tracked_exit(res):
    """EXITING ends the session: HMD goes, no projection is submitted
    after, and the window keeps running."""
    errors = check_exit_window(res)
    hmd = [m[0] for m in HMD_RE.findall(res.log)]
    if '1' not in hmd or hmd[-1] != '0':
        errors.append('HMD did not go with the session: %s' % hmd)
    lost = res.marks['lost'] * 1e6
    late = [f for f in res.frames if f['t_us'] > lost + 5e5 and projections(f)]
    if late:
        errors.append('%d frames with a projection after the session ended'
                      % len(late))
    return errors


def check_tracked_stopping(res):
    """A session the runtime stops is alive but not running: HMD goes
    with it."""
    errors = []
    if '[OpenXR] Session stopping.' not in res.log:
        errors.append('the session did not stop')
    hmd = [m[0] for m in HMD_RE.findall(res.log)]
    if '1' not in hmd or hmd[-1] != '0':
        errors.append('HMD did not go with the session: %s' % hmd)
    return errors


SESSION_CREATED = '[OpenXR] Session created.'


def check_tracked_reinit(res):
    """A video driver reinit while tracked content runs: the new driver
    takes the map's content info again, so the core sees HMD with an eye
    size on both sides, and the projection comes back in the space the
    core asked for (LOCAL where the runtime has no STAGE)."""
    first = res.log.find(SESSION_CREATED)
    again = res.log.find(SESSION_CREATED, first + 1) if first >= 0 else -1
    if again < 0:
        return ['the video driver did not restart']
    errors = []
    before = [m for m in HMD_RE.findall(res.log[:again]) if m[0] == '1'
              and int(m[1]) and int(m[2])]
    if not before:
        errors.append('the core saw no HMD with an eye size before the '
                      'reinit: %s' % HMD_RE.findall(res.log[:again]))
    last = HMD_RE.findall(res.log)[-1:]
    if not last or last[0][0] != '1' or not int(last[0][1]) \
            or not int(last[0][2]):
        errors.append('the core sees %s after the reinit, want HMD with an '
                      'eye size' % last)
    for line in ('Unable to set VR content info', 'No eye target size'):
        if line in res.log:
            errors.append('"%s" in the log' % line)
    reinit = res.marks['reinit'] * 1e6
    late = [p['space'] for f in res.frames if f['t_us'] > reinit + 2e6
            for p in projections(f)]
    want = 'stage'
    if 'No STAGE space: tracked content is seated.' in res.log[:again]:
        want = 'local'
    if not late:
        errors.append('no projection after the reinit')
    elif set(late) != set([want]):
        errors.append('the projection is in %s after the reinit, want %s'
                      % (sorted(set(late)), want))
    return errors


def shown_as_drawn(res, fr):
    """Errors unless snapped frame fr's projection shows the eyes with
    the poses and fields of view the core logged for the frame they are
    tagged with."""
    size = tracked_size(res)
    views = projections(fr)[0]['views']
    if len(views) != 2:
        return ['%d projection views' % len(views)]
    tags = [frame_tag(image(res, fr, v), v, size) for v in views]
    if None in tags or tags[0] != tags[1]:
        return ['the eyes are tagged frames %s' % tags]
    upto = [(int(m[0]), [float(x) for x in m[1:]])
            for m in EYES_RE.findall(res.log) if int(m[0]) <= tags[0]]
    if not upto:
        return ['no eye poses logged by frame %d' % tags[0]]
    e = max(upto, key=lambda x: x[0])[1]
    errors = []
    for v, pos, fov, what in ((views[0], e[0:3], e[6:10], 'left'),
                              (views[1], e[3:6], e[10:14], 'right')):
        if any(abs(a - b) > EXACT for a, b in zip(v['pose'][:3], pos)):
            errors.append('the %s eye is shown at %s, drawn at %s in frame '
                          '%d' % (what, v['pose'][:3], pos, tags[0]))
        tans = [abs(math.tan(a)) for a in v['fov']]
        if any(abs(a - b) > 0.01 for a, b in zip(tans, fov)):
            errors.append('the %s eye is shown with tangents %s, drawn '
                          'with %s' % (what, tans, fov))
    return errors


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw,
            aw * bw - ax * bx - ay * by - az * bz)


def in_stage(p, v):
    """Projection p's view v as a pose in STAGE, through the layer's
    location of p's space there; None without one."""
    s = p.get('stage')
    if not s:
        return None
    q = s[3:]
    r = qmul(qmul(q, tuple(v['pose'][:3]) + (0.0,)),
             (-q[0], -q[1], -q[2], q[3]))
    return ([a + b for a, b in zip(s[:3], r[:3])]
            + list(qmul(q, v['pose'][3:])))


def same_place(a, b):
    """Two eyes' STAGE poses are the same within the layer's rounding:
    0.2 mm, and 0.2 degrees between the orientations."""
    def turn(x, y):
        d = sum(i * j for i, j in zip(x, y))
        n = math.sqrt(sum(i * i for i in x) * sum(j * j for j in y))
        return abs(d) / n
    return all(x and y and all(abs(i - j) <= 2e-4 for i, j in zip(x[:3], y[:3]))
               and turn(x[3:], y[3:]) >= math.cos(math.radians(0.1))
               for x, y in zip(a, b))


def recentered_after(res, pos):
    """The frames the core saw RECENTERED in, from log offset pos on."""
    return sum(1 for m in FLAGS_RE.finditer(res.log, pos)
               if int(m.group(1)) & RECENTERED)


def check_tracked_reinit_recentered(space):
    """A video driver reinit while seated content runs: the new session's
    LOCAL is wherever the runtime puts it, so the core hears RECENTERED
    once, with its first sample after; the projection goes on in space,
    or with None, there is none (a core that only reads the poses)."""
    def check(res):
        first = res.log.find(SESSION_CREATED)
        again = res.log.find(SESSION_CREATED, first + 1) if first >= 0 else -1
        if again < 0:
            return ['the video driver did not restart']
        errors = []
        n = recentered_after(res, again)
        if n != 1:
            errors.append('the core saw RECENTERED %d times after the '
                          'reinit, want 1' % n)
        if recentered_after(res, 0) != n:
            errors.append('the core saw RECENTERED before the reinit')
        bad = [x.strip() for x in res.log.splitlines() if '[ERROR]' in x]
        if bad:
            errors.append('%d error lines, first: %s' % (len(bad), bad[0]))
        reinit = res.marks['reinit'] * 1e6
        late = set(p['space'] for f in res.frames if f['t_us'] > reinit + 2e6
                   for p in projections(f))
        if late != (set([space]) if space else set()):
            errors.append('the projection is in %s after the reinit, want %s'
                          % (sorted(late), space))
        return errors
    return check


def check_tracked_reinit_resized(res):
    """A video driver reinit to a headset that recommends another eye
    size: the core hears TARGET_RESIZED once, after the reinit, though
    the new session's first frames have no poses, and it is given the
    new size."""
    first = res.log.find(SESSION_CREATED)
    again = res.log.find(SESSION_CREATED, first + 1) if first >= 0 else -1
    if again < 0:
        return ['the video driver did not restart']
    errors = []
    told = [m.start() for m in FLAGS_RE.finditer(res.log)
            if int(m.group(1)) & TARGET_RESIZED]
    if len(told) != 1 or told[0] < again:
        errors.append('the core saw TARGET_RESIZED %d times, %d after the '
                      'reinit, want once after it'
                      % (len(told), sum(1 for t in told if t > again)))
    if tracked_size(res) != RESIZED:
        errors.append('the core was last given %s, want %s'
                      % (tracked_size(res), RESIZED))
    return errors


def check_tracked_recenter_unlocated(res):
    """A recenter while the runtime cannot locate the eyes: the core
    hears RECENTERED once, when they are located again."""
    n = recenter_flags(res)
    return [] if n == 1 else ['the core saw RECENTERED %d times, want 1' % n]


def check_tracked_reinit_paused(shown, space):
    """A video driver reinit while tracked content is paused: the core
    runs no frame until the unpause, and nothing logs an error. Until
    then the headset shows, by shown: 'projection', the kept frame's
    eyes where they were in the room, as STAGE places them in both
    sessions, from the poses the core drew them from; 'pair', no
    projection but the eyes as a quad pair, as without STAGE nothing
    places them; None, no projection, as a hardware frame is not kept.
    After the unpause the projection is back, in space, and a seated
    core, whose origin moved, hears RECENTERED once."""
    def check(res):
        first = res.log.find(SESSION_CREATED)
        again = res.log.find(SESSION_CREATED, first + 1) if first >= 0 else -1
        if again < 0:
            return ['the video driver did not restart']
        errors = check_paused(res, 'paused', 'resume')
        bad = [x.strip() for x in res.log.splitlines() if '[ERROR]' in x]
        if bad:
            errors.append('%d error lines, first: %s' % (len(bad), bad[0]))
        reinit = res.marks['reinit'] * 1e6
        resumed = res.marks['resume'] * 1e6
        # A new session's first frame can show the eyes as a quad pair,
        # on any driver, while STAGE has not located the kept views yet:
        # the grace covers the restart and that frame.
        paused = [f for f in res.frames
                  if reinit + res.reinit_grace * 1e6 < f['t_us'] < resumed
                  and f['layers']]
        if not paused:
            return errors + ['no frames between the reinit and the unpause']
        shown_proj = [f for f in paused if projections(f)]
        if shown == 'projection':
            blank = [f['n'] for f in paused if not projections(f)]
            if blank:
                errors.append('%d frames without the projection while '
                              'paused, first %d' % (len(blank), blank[0]))
            spaces = set(p['space'] for f in shown_proj
                         for p in projections(f))
            if shown_proj and spaces != set([space]):
                errors.append('the projection is in %s while paused, want %s'
                              % (sorted(spaces), space))
            # The last projection before the reinit shows the poses the
            # frame was drawn from; every one after shows them at the same
            # place in the room, never at poses sampled since.
            before = [f for f in res.frames if f['snap'] and projections(f)
                      and res.marks['paused'] * 1e6 < f['t_us'] < reinit]
            # check_paused sees this frame's tags only in a snapped one.
            if not [f for f in shown_proj if f['snap']]:
                errors.append('no snapped projection while paused after the '
                              'reinit')
            if not before:
                errors.append('no snapped projection between the pause and '
                              'the reinit')
            else:
                errors += shown_as_drawn(res, before[-1])
                p = projections(before[-1])[0]
                want = [in_stage(p, v) for v in p['views']]
                moved = [f['n'] for f in shown_proj
                         if not same_place(want, [
                             in_stage(projections(f)[0], v)
                             for v in projections(f)[0]['views']])]
                if None in want:
                    errors.append('no STAGE pose before the reinit')
                elif moved:
                    errors.append('%d frames show the eyes elsewhere in the '
                                  'room, first %d' % (len(moved), moved[0]))
        elif shown_proj:
            errors.append('%d frames with a projection while paused after the '
                          'reinit, first %d' % (len(shown_proj),
                                                shown_proj[0]['n']))
        if shown == 'pair':
            unpaired = [f['n'] for f in paused
                        if not set(['left', 'right'])
                        <= set(x['eye'] for x in quads(f))]
            if unpaired:
                errors.append('%d frames without the eyes\' quad pair while '
                              'paused, first %d' % (len(unpaired),
                                                    unpaired[0]))
        late = [p['space'] for f in res.frames if f['t_us'] > resumed + 1e6
                for p in projections(f)]
        if not late:
            errors.append('no projection after the unpause')
        elif set(late) != set([space]):
            errors.append('the projection is in %s after the unpause, want %s'
                          % (sorted(set(late)), space))
        want = 1 if space == 'local' else 0
        n = recentered_after(res, again)
        if n != want or recentered_after(res, 0) != n:
            errors.append('the core saw RECENTERED %d times after the reinit '
                          'and %d before, want %d and 0'
                          % (n, recentered_after(res, 0) - n, want))
        return errors
    return check


REINIT_RUNNING = [('wait', 6), ('mark', 'reinit'),
                  ('send', 'FULLSCREEN_TOGGLE'), ('wait', 6)]
# The next instance's headset recommends another eye size, and its
# session's first frames have no poses, as SteamVR's take a while.
RESIZED = (1600, 1680)
REINIT_RESIZED = [('wait', 6), ('script', 'eyes %d %d' % RESIZED),
                  ('wait', 1), ('mark', 'reinit'),
                  ('script', 'eyes %d %d\ninvalid views' % RESIZED),
                  ('send', 'FULLSCREEN_TOGGLE'), ('wait', 4),
                  ('script', 'eyes %d %d' % RESIZED), ('wait', 4)]

# The pause hotkey, then the fullscreen one, which restarts the driver.
REINIT_PAUSED = [('wait', 6), ('send', 'PAUSE_TOGGLE'), ('wait', 1),
                 ('mark', 'paused'), ('wait', 3), ('mark', 'reinit'),
                 ('send', 'FULLSCREEN_TOGGLE'), ('wait', 4), ('mark', 'resume'),
                 ('send', 'PAUSE_TOGGLE'), ('wait', 3)]
# The head turns while paused, so the frozen frame was drawn from other
# poses than the anchor the recenter takes.
RECENTER_PAUSED = [('wait', 6), ('send', 'PAUSE_TOGGLE'), ('wait', 1),
                   ('script', 'head 0.5 0.2 0.3 90'), ('wait', 1),
                   ('mark', 'recenter'), ('send', 'HEADSET_RECENTER'),
                   ('wait', 3), ('mark', 'resume'), ('send', 'PAUSE_TOGGLE'),
                   ('wait', 3)]
# The new session's first STAGE locations are not had yet.
REINIT_PAUSED_UNLOCATED = (REINIT_PAUSED[:4]
                           + [('wait', 2), ('script', 'invalid stage 3'),
                              ('wait', 1)]
                           + REINIT_PAUSED[5:])


def image_size(res, q, native=None, distance=1.8):
    """Errors unless q's swapchain has the size video_xr_image_dims()
    gives at the density the runtime recommended: the quad's angular
    width in headset pixels, in the quad's shape; with a shader preset
    (native, the source's size), never below the source's width. The log
    rounds the angle, so allow a pixel or two."""
    m = DENSITY.search(res.log)
    sc = res.chains.get(q['sc'])
    if not m or not sc:
        return ['no headset density or swapchain for quad sc%d' % q['sc']]
    px_per_rad = int(m.group(1)) / math.radians(int(m.group(2)))
    angular = 2.0 * math.atan(q['size'][0] / (2.0 * distance)) * px_per_rad
    floor = native[0] if native else 0
    slack = 0 if floor > angular * 1.02 + 1 else 2 + int(angular * 0.01)
    w = max(floor, math.ceil(angular))
    h = w * q['size'][1] / q['size'][0]
    errors = []
    if abs(sc['w'] - w) > slack or abs(sc['h'] - h) > slack + 1:
        errors.append('sc%d is %dx%d, want about %dx%d (%.1f px/rad)'
                      % (q['sc'], sc['w'], sc['h'], w, round(h), px_per_rad))
    if q['rect'] != [0, 0, sc['w'], sc['h']]:
        errors.append('sc%d shows %s, want all of it' % (q['sc'], q['rect']))
    return errors


def check_screens(swap=False, horizontal=False, native=False):
    """native: the images are never smaller than their views, as gl's
    cuts of its window are."""
    def check(res):
        fr = last_snap(res)
        if not fr:
            return no_quads(res)
        q = [x for x in quads(fr) if not x['flags'] & BLEND]
        left = [x for x in q if x['eye'] == 'left']
        right = [x for x in q if x['eye'] == 'right']
        both = [x for x in q if x['eye'] == 'both']
        if (len(left), len(right), len(both)) != (1, 1, 1):
            return ['want a left, a right and a both-eye quad, got %s'
                    % [x['eye'] for x in q]]
        lq, rq, bq = left[0], right[0], both[0]
        errors = []
        if lq['sc'] == rq['sc'] or (lq['layer'], rq['layer']) != (0, 0):
            errors.append('the eyes share an image')
        for x in (lq, rq):
            if not at(x, (0.0, 0.0, -1.8)) or not sized(x, (1.6, 0.96)):
                errors.append('top screen at %s size %s'
                              % (x['pose'][:3], x['size']))
            if not facing(x, (0.0, 0.0, 0.0, 1.0)):
                errors.append('top screen turned %s' % x['pose'][3:])
        want = (1.472, 0.0, -1.8) if horizontal else (0.0, -0.992, -1.8)
        if not at(bq, want) or not sized(bq, (1.28, 0.96)):
            errors.append('bottom screen at %s size %s, want %s'
                          % (bq['pose'][:3], bq['size'], want))
        errors += image_size(res, lq, (400, 240) if native else None)
        errors += image_size(res, rq, (400, 240) if native else None)
        errors += image_size(res, bq, (320, 240) if native else None)
        if any(res.chains.get(x['sc'], {}).get('layers') != 1
               for x in (lq, rq)):
            errors.append('the eyes\' images are not one layer each')
        want_l, want_r = (BLUE, RED) if swap else (RED, BLUE)
        for x, want, what in ((lq, want_l, 'left eye'),
                              (rq, want_r, 'right eye'),
                              (bq, YELLOW, 'bottom screen')):
            c, _ = colour(image(res, fr, x), 0.5, 0.5)
            if not near(c, want):
                errors.append('%s shows %s, want %s' % (what, c, want))
        # Each view's white marker is at its top-left: not flipped.
        for x, what in ((lq, 'left eye'), (rq, 'right eye'),
                        (bq, 'bottom screen')):
            c, _ = colour(image(res, fr, x), 0.01, 0.015)
            if not near(c, WHITE):
                errors.append('%s top-left is %s, want white' % (what, c))
        return errors
    return check


def threaded(check):
    def run(res):
        errors = check(res)
        if 'Starting threaded video driver' not in res.log:
            errors.append('threaded video did not start')
        return errors
    return run


def size_colour(w, h):
    """What output_size.slangp draws into a w x h target: the final
    pass's size, and in blue its first pass's, at half the viewport."""
    return tuple(int(round(v / 2048.0 * 255.0)) for v in (w, h, w / 2.0))


def sized_like(res, fr, q, what, tol=2):
    """Errors unless q's image shows the colour for its own size."""
    sc = res.chains.get(q['sc'])
    if not sc:
        return ['no swapchain for %s' % what]
    want = size_colour(sc['w'], sc['h'])
    c, _ = colour(image(res, fr, q), 0.5, 0.5)
    if not near(c, want, tol):
        return ['%s (%dx%d) shows %s, want %s' % (what, sc['w'], sc['h'],
                                                  c, want)]
    return []


def window_like(res, points, tol=2):
    """Errors unless the window screenshot shows, at each (fx, fy), the
    colour for a (w, h) target."""
    if not res.shot:
        return ['no window screenshot']
    errors = []
    for fx, fy, w, h, what in points:
        c, _ = colour(res.shot, fx, fy)
        if not near(c, size_colour(w, h), tol):
            errors.append('window %s (%dx%d) shows %s, want %s'
                          % (what, w, h, c, size_colour(w, h)))
    return errors


def check_sized_screens(res):
    """Every headset image drawn as a first draw at its own size, and
    the window at its rectangles: 2D, the top screen over the bottom."""
    fr = last_snap(res)
    if not fr:
        return no_quads(res)
    q = [x for x in quads(fr) if not x['flags'] & BLEND]
    eyes = dict((x['eye'], x) for x in q)
    if sorted(eyes) != ['both', 'left', 'right']:
        return ['want a left, a right and a both-eye quad, got %s'
                % [x['eye'] for x in q]]
    errors = []
    for eye, what, native in (('left', 'left eye', (400, 240)),
                              ('right', 'right eye', (400, 240)),
                              ('both', 'bottom screen', (320, 240))):
        errors += sized_like(res, fr, eyes[eye], what)
        # A preset's images are never smaller than their sources.
        errors += image_size(res, eyes[eye], native)
    k = min(W / 400.0, H / 480.0)
    return errors + window_like(res, (
        (0.5, 0.25, 400 * k, 240 * k, 'top screen'),
        (0.5, 0.75, 320 * k, 240 * k, 'bottom screen')))


def check_sized_frame(res):
    fr = last_snap(res)
    if not fr:
        return no_quads(res)
    q = [x for x in quads(fr) if not x['flags'] & BLEND]
    if len(q) != 1 or q[0]['eye'] != 'both':
        return ['want one quad for both eyes, got %s' % [x['eye'] for x in q]]
    # The core's frame has the window's shape, so it fills the window.
    return (sized_like(res, fr, q[0], 'frame')
            + image_size(res, q[0], (800, 480))
            + window_like(res, ((0.5, 0.5, W, H, 'frame'),)))


def check_crop(res):
    """Threaded video crops this frame above its second screen: the map
    no longer fits, so the headset shows the cropped frame whole, as the
    window does, not two screens."""
    fr = last_snap(res)
    if not fr:
        return no_quads(res)
    q = [x for x in quads(fr) if not x['flags'] & BLEND]
    if len(q) != 1 or q[0]['eye'] != 'both':
        return ['want one quad for both eyes, got %s' % [x['eye'] for x in q]]
    errors = []
    if 'cropping to 655 rows' not in res.log:
        errors.append('threaded video did not crop the frame')
    if not at(q[0], (0.0, 0.0, -1.8)) or not sized(q[0], (1.6, 0.96)):
        errors.append('frame at %s size %s' % (q[0]['pose'][:3], q[0]['size']))
    # 480 of the 655 rows are the first screen's.
    for fx, fy, want in ((0.5, 0.25, GREEN), (0.5, 0.9, YELLOW)):
        c, _ = colour(image(res, fr, q[0]), fx, fy)
        if not near(c, want):
            errors.append('(%.2f,%.2f) shows %s, want %s' % (fx, fy, c, want))
    return errors


def check_frame(res):
    fr = last_snap(res)
    if not fr:
        return no_quads(res)
    q = [x for x in quads(fr) if not x['flags'] & BLEND]
    if len(q) != 1 or q[0]['eye'] != 'both':
        return ['want one quad for both eyes, got %s' % [x['eye'] for x in q]]
    errors = []
    if not at(q[0], (0.0, 0.0, -1.8)) or not sized(q[0], (1.6, 0.96)):
        errors.append('frame at %s size %s' % (q[0]['pose'][:3], q[0]['size']))
    errors += image_size(res, q[0])
    for fx, fy, want in ((0.25, 0.25, RED), (0.75, 0.25, BLUE),
                         (0.5, 0.75, YELLOW), (0.1, 0.75, GREY)):
        c, _ = colour(image(res, fr, q[0]), fx, fy)
        if not near(c, want):
            errors.append('(%.2f,%.2f) shows %s, want %s' % (fx, fy, c, want))
    return errors


MID = (128, 128, 128)


def pattern_errors(res, fr, q, what, smooth, box=(0.2, 0.2, 0.8, 0.8)):
    """A one-pixel checkerboard in q's image, over box (fractions of the
    image): mid grey throughout when shrunk smoothly, else only black and
    white pixels, both of them."""
    w, h, bpp, rows = read_png(image(res, fr, q))
    seen = set()
    for y in range(int(box[1] * h), int(box[3] * h)):
        for x in range(int(box[0] * w), int(box[2] * w)):
            px = tuple(rows[y][x * bpp:x * bpp + 3])
            if smooth:
                if not near(px, MID, 24):
                    return ['%s is %s at (%d,%d), want grey: the checkerboard '
                            'shrunk smoothly' % (what, px, x, y)]
            elif near(px, (0, 0, 0)):
                seen.add('black')
            elif near(px, WHITE):
                seen.add('white')
            else:
                return ['%s is %s at (%d,%d), want black or white: the '
                        'checkerboard enlarged unfiltered' % (what, px, x, y)]
    if not smooth and len(seen) < 2:
        return ['%s shows only %s' % (what, sorted(seen))]
    return []


def check_checker(smooth, distance=1.8):
    """The 3DS screens as one-pixel checkerboards: each image the size
    the headset shows, smaller than its source with the checkerboard
    shrunk smoothly into it (smooth), or no smaller and enlarged with
    Bilinear Filtering off, as before."""
    def check(res):
        fr = last_snap(res)
        if not fr:
            return no_quads(res)
        eyes = dict((x['eye'], x) for x in quads(fr)
                    if not x['flags'] & BLEND)
        if sorted(eyes) != ['both', 'left', 'right']:
            return ['want a left, a right and a both-eye quad, got %s'
                    % sorted(eyes)]
        errors = []
        for eye, what, src in (('left', 'left eye', (400, 240)),
                               ('right', 'right eye', (400, 240)),
                               ('both', 'bottom screen', (320, 240))):
            q = eyes[eye]
            sc = res.chains.get(q['sc'])
            if not sc:
                errors.append('no swapchain for the %s' % what)
                continue
            errors += image_size(res, q, distance=distance)
            if (sc['w'] < src[0] or sc['h'] < src[1]) != smooth:
                errors.append('the %s\'s image is %dx%d for its %dx%d '
                              'source, want it %s'
                              % (what, sc['w'], sc['h'], src[0], src[1],
                                 'smaller' if smooth else 'no smaller'))
            errors += pattern_errors(res, fr, q, what, smooth)
        if smooth and '[OpenXR] Slot 0: 400x240,' in res.log:
            errors.append('slot 0 was made at its source\'s 400x240')
        return errors
    return check


def check_frame_checker(res):
    """No map: the whole 800x480 frame, its top screens one-pixel
    checkerboards, in an image the size the headset shows with the
    checkerboard shrunk smoothly into it."""
    fr = last_snap(res)
    if not fr:
        return no_quads(res)
    q = [x for x in quads(fr) if not x['flags'] & BLEND]
    if len(q) != 1 or q[0]['eye'] != 'both':
        return ['want one quad for both eyes, got %s' % [x['eye'] for x in q]]
    sc = res.chains.get(q[0]['sc'])
    if not sc:
        return ['no swapchain for the frame']
    errors = image_size(res, q[0])
    if sc['w'] >= 800 or sc['h'] >= 480:
        errors.append('the frame\'s image is %dx%d, want it smaller than '
                      'the 800x480 frame' % (sc['w'], sc['h']))
    # The left top screen, clear of its marker and its edges.
    return errors + pattern_errors(res, fr, q[0], 'frame', True,
                                   (0.1, 0.1, 0.45, 0.4))


def menu_size(res, q, ui, distance=1.7):
    """Errors unless the menu quad's swapchain has the UI's size, at
    most twice the headset's pixels across the quad, in its shape."""
    m = DENSITY.search(res.log)
    sc = res.chains.get(q['sc'])
    if not m or not sc:
        return ['no headset density or swapchain for the menu']
    px_per_rad = int(m.group(1)) / math.radians(int(m.group(2)))
    most = 4.0 * math.atan(q['size'][0] / (2.0 * distance)) * px_per_rad
    if ui[0] <= most * 0.98 - 1:
        w, h, slack = ui[0], ui[1], 0
    else:
        w = min(ui[0], math.ceil(most))
        h = w * q['size'][1] / q['size'][0]
        slack = 2 + int(most * 0.01)
    errors = []
    if abs(sc['w'] - w) > slack or abs(sc['h'] - h) > slack + 1:
        errors.append('menu sc%d is %dx%d, want about %dx%d (%.1f px/rad)'
                      % (q['sc'], sc['w'], sc['h'], w, round(h), px_per_rad))
    if q['rect'] != [0, 0, sc['w'], sc['h']]:
        errors.append('menu sc%d shows %s, want all of it'
                      % (q['sc'], q['rect']))
    return errors


def check_menu(ui=(W, H)):
    def check(res):
        fr = last_snap(res)
        if not fr:
            return no_quads(res)
        q = quads(fr)
        menus = [x for x in q if x['flags'] & BLEND]
        if len(menus) != 1:
            return ['want one blended menu quad, got %d' % len(menus)]
        m = menus[0]
        errors = []
        if len(q) != 4:
            errors.append('want the three screen quads behind the menu, '
                          'got %d' % (len(q) - 1))
        if q[-1] is not m:
            errors.append('the menu is not drawn last')
        if not at(m, (0.0, 0.0, -1.7)) or not sized(m, (1.6, 0.96)):
            errors.append('menu at %s size %s' % (m['pose'][:3], m['size']))
        errors += menu_size(res, m, ui)
        # The UI over running content is translucent; a core's frame
        # there would be opaque.
        _, alpha = colour(image(res, fr, m), 0.5, 0.5)
        if not 0 < alpha < 255:
            errors.append('the menu\'s centre has alpha %d' % alpha)
        return errors
    return check


def check_menu_overlay(res):
    """The overlay shows in the window, never in the headset's menu."""
    errors = check_menu()(res)
    fr = last_snap(res)
    menus = [x for x in quads(fr) if x['flags'] & BLEND] if fr else []
    if menus:
        c, _ = colour(image(res, fr, menus[0]), 0.05, 0.05)
        if near(c, MAGENTA, 40):
            errors.append('the overlay is in the headset\'s menu quad')
    if not res.shot:
        errors.append('no window screenshot')
    else:
        c, _ = colour(res.shot, 0.05, 0.05)
        if not near(c, MAGENTA):
            errors.append('the window shows %s at the overlay, want %s'
                          % (c, MAGENTA))
    return errors


def check_menu_closed(res):
    last = [f for f in res.frames if quads(f)]
    if not last:
        return no_quads(res)
    if any(x['flags'] & BLEND for x in quads(last[-1])):
        return ['the menu quad stayed after the menu closed']
    if not any(x['flags'] & BLEND for f in last for x in quads(f)):
        return ['the menu quad never appeared']
    return []


# RGUI, OK and Cancel unswapped, from the Quick Menu: back to the Main
# Menu, Settings (row 5), Video (1), Stereo 3D & Screens (5), and down
# to Screen Distance (row 15).
DISTANCE_NAV = ([('send', 'MENU_B'), ('wait', 0.8)]
                + [('send', 'MENU_DOWN'), ('wait', 0.3)] * 5
                + [('send', 'MENU_A'), ('wait', 0.8)]
                + [('send', 'MENU_DOWN'), ('wait', 0.3)]
                + [('send', 'MENU_A'), ('wait', 0.8)]
                + [('send', 'MENU_DOWN'), ('wait', 0.3)] * 5
                + [('send', 'MENU_A'), ('wait', 0.8)]
                + [('send', 'MENU_DOWN'), ('wait', 0.3)] * 15)
STEP_RIGHT = [('send', 'MENU_RIGHT'), ('wait', 2)]
STEP_LEFT = [('send', 'MENU_LEFT'), ('wait', 2)]
# Screen Distance out and in, Screen Width out and back, and Width once
# more just after the menu reopens. Every new swapchain's image comes a
# frame late, so each step has the old ones to show. busy new waits
# for the session: the laser cursor's swapchain is filled as it is
# made, and a timeout on that wait fails it.
MENU_RESIZE = ([('wait', 6), ('script', 'busy new'), ('send', 'MENU_TOGGLE'),
                ('wait', 3)]
               + DISTANCE_NAV + STEP_RIGHT * 2 + STEP_LEFT * 4 + STEP_RIGHT * 2
               + [('send', 'MENU_DOWN'), ('wait', 0.3)]
               + STEP_RIGHT * 2 + STEP_LEFT * 2
               + [('mark', 'closed'), ('send', 'MENU_TOGGLE'), ('wait', 3),
                  ('mark', 'reopen'), ('send', 'MENU_TOGGLE'), ('wait', 2)]
               + STEP_RIGHT + [('mark', 'done'), ('send', 'MENU_TOGGLE'),
                               ('wait', 3)])
DISTANCES = [1.8, 1.9, 2.0, 1.9, 1.8, 1.7, 1.6, 1.7, 1.8]
WIDTHS = [1.6, 1.7, 1.8, 1.7, 1.6, 1.7]
SLOT_MADE = re.compile(r'\[OpenXR\] Slot (\d+): (\d+x\d+),')
SCREEN_COLOURS = {'left': RED, 'right': BLUE, 'both': YELLOW}


def stepped(values):
    out = []
    for v in values:
        if not out or abs(out[-1] - v) > POS:
            out.append(round(v, 2))
    return out


def check_menu_resize(res):
    """Each step makes the screens' and the menu's swapchains again at
    another size, and the old ones show until the new ones are drawn: no
    frame leaves a screen or the open menu out, or shows an image the
    core did not draw."""
    tops = [x for f in res.frames for x in quads(f) if x['eye'] == 'left']
    if not tops:
        return no_quads(res)
    dist = stepped(-x['pose'][2] for x in tops)
    if dist[:2] != DISTANCES[:2]:
        return ['the screens went %s m, not 1.8 then 1.9: the menu walk '
                'missed Screen Distance' % dist]
    errors = []
    width = stepped(x['size'][0] for x in tops)
    if dist != DISTANCES or width != WIDTHS:
        errors.append('Screen Distance went %s and Width %s, want %s and %s'
                      % (dist, width, DISTANCES, WIDTHS))
    made, resized = {}, []
    for slot, size in SLOT_MADE.findall(res.log):
        if made.get(slot, size) != size:
            resized.append(slot)
        made[slot] = size
    if not resized:
        errors.append('no slot was made again at another size')
    n = [len([x for x in quads(f) if not x['flags'] & BLEND])
         for f in res.frames]
    if 3 not in n:
        return errors + ['no frame shows all three screens']
    first = n.index(3)
    last = max(i for i, f in enumerate(res.frames) if quads(f))
    shown = res.frames[first:last + 1]
    fewer = [f['n'] for f, k in zip(shown, n[first:]) if k < 3]
    if fewer:
        errors.append('%d frames show fewer than three screens (first n=%d)'
                      % (len(fewer), fewer[0]))
    gone = []
    for t0, t1 in ((0, res.marks['closed']),
                   (res.marks['reopen'], res.marks['done'])):
        up = [f for f in res.frames if t0 * 1e6 < f['t_us'] < t1 * 1e6]
        since = [i for i, f in enumerate(up) if menus(f)]
        if not since:
            errors.append('the menu did not show')
            continue
        gone += [f['n'] for f in up[since[0]:] if not menus(f)]
    if gone:
        errors.append('the open menu left %d frames out (first n=%d)'
                      % (len(gone), gone[0]))
    cut, undrawn = [], []
    for f in shown:
        for x in quads(f):
            sc = res.chains.get(x['sc'])
            if not sc or x['rect'] != [0, 0, sc['w'], sc['h']]:
                cut.append((f['n'], x['sc'], x['rect']))
            if f['snap'] and not x['flags'] & BLEND:
                path = image(res, f, x)
                if not os.path.exists(path):
                    undrawn.append((f['n'], x['sc'], 'no image'))
                    continue
                c, _ = colour(path, 0.5, 0.5)
                if not near(c, SCREEN_COLOURS[x['eye']]):
                    undrawn.append((f['n'], x['sc'], c))
    if cut:
        errors.append('%d quads do not show all of their swapchain '
                      '(first n=%d sc%d %s)' % ((len(cut),) + cut[0]))
    if undrawn:
        errors.append('%d screen images the core did not draw (first n=%d '
                      'sc%d %s)' % ((len(undrawn),) + undrawn[0]))
    bad = [x.strip() for x in res.log.splitlines() if '[ERROR]' in x]
    if bad:
        errors.append('%d error lines, first: %s' % (len(bad), bad[0]))
    return errors


def check_recenter(res):
    shown = [f for f in res.frames if quads(f)]
    if not shown:
        return no_quads(res)
    errors = []
    first = [x for x in quads(shown[0]) if x['eye'] == 'left']
    last = [x for x in quads(shown[-1]) if x['eye'] == 'left']
    if not first or not last:
        return ['no left-eye quad']
    if not at(first[0], (0.0, 0.0, -1.8)):
        errors.append('screen 0 started at %s' % first[0]['pose'][:3])
    # The scripted head: 0.5, 0.2, 0.3, turned 90 degrees left.
    if not at(last[0], (-1.3, 0.2, 0.3)):
        errors.append('screen 0 at %s after recentering, want (-1.3, 0.2, 0.3)'
                      % last[0]['pose'][:3])
    if not facing(last[0], (0.0, 0.70711, 0.0, 0.70711)):
        errors.append('screen 0 turned %s, want a quarter turn left'
                      % last[0]['pose'][3:])
    if 'Headset recenter requested' not in res.log:
        errors.append('the network command did not reach the hotkey')
    return errors


def check_recenter_menu(res):
    """The hotkey recenters while the menu is open: the menu and the
    screens go to the turned head's anchor. A press with the menu
    closed is the second, and no press runs twice."""
    mark = res.marks['recenter'] * 1e6
    shown = [f for f in res.frames if quads(f)]
    if not shown:
        return no_quads(res)
    before = [f for f in shown if f['t_us'] < mark]
    after = [f for f in shown
             if mark + 1e6 < f['t_us'] < res.marks['closed'] * 1e6]
    if not before or not after:
        return ['no frames on both sides of the recenter']
    errors = []
    for name, fr, want, turn in (
            ('before', before[-1], (0.0, 0.0, -1.7), None),
            ('after', after[-1], (-1.2, 0.2, 0.3),
             (0.0, 0.70711, 0.0, 0.70711))):
        menus = [x for x in quads(fr) if x['flags'] & BLEND]
        if len(menus) != 1:
            return ['want one blended menu quad %s the recenter, got %d'
                    % (name, len(menus))]
        if not at(menus[0], want):
            errors.append('menu at %s %s the recenter, want %s'
                          % (menus[0]['pose'][:3], name, want))
        if turn and not facing(menus[0], turn):
            errors.append('menu turned %s, want a quarter turn left'
                          % menus[0]['pose'][3:])
    screens = [x for x in quads(after[-1])
               if not x['flags'] & BLEND and x['eye'] == 'left']
    if not screens or not at(screens[0], (-1.3, 0.2, 0.3)):
        errors.append('screen 0 at %s after the recenter, want (-1.3, 0.2, 0.3)'
                      % (screens[0]['pose'][:3] if screens else None))
    if res.log.count('Headset recenter requested') != 2:
        errors.append('the hotkey ran %d times for two presses'
                      % res.log.count('Headset recenter requested'))
    return errors


def check_runtime_recenter(res):
    """The runtime's own recenter moves LOCAL: the screens go back
    straight ahead after a hotkey recenter. A STAGE change leaves them."""
    stage = res.marks.get('stage', 0) * 1e6
    local = res.marks.get('local', 0) * 1e6
    held = [x for f in res.frames if stage + 1e6 < f['t_us'] < local
            for x in quads(f) if x['eye'] == 'left']
    shown = [f for f in res.frames if quads(f)]
    if not held or not shown:
        return no_quads(res)
    errors = []
    moved = [x['pose'][:3] for x in held if not at(x, (-1.3, 0.2, 0.3))]
    if moved:
        errors.append('screen 0 left the hotkey\'s place before the LOCAL '
                      'change: %s' % moved[0])
    last = [x for x in quads(shown[-1]) if x['eye'] == 'left']
    if (     not last or not at(last[0], (0.0, 0.0, -1.8))
            or not facing(last[0], (0.0, 0.0, 0.0, 1.0))):
        errors.append('screen 0 ends at %s, want straight ahead'
                      % (last[0]['pose'] if last else None))
    n = res.log.count('[OpenXR] Recentered by the runtime.')
    if n != 1:
        errors.append('%d runtime recenters logged, want 1' % n)
    return errors


def check_pacing(res, still=True):
    """The headset's frames keep its own rate while the core is paused,
    and no new images are drawn for it. still False: images may come all
    the same, as gl's window is copied whenever it is presented."""
    if 'paused' not in res.marks or 'resumed' not in res.marks:
        return ['the run did not pause']
    t0 = (res.marks['paused'] + 0.5) * 1e6
    t1 = res.marks['resumed'] * 1e6
    periods = sorted(f['period_ns'] for f in res.frames if f['period_ns'] > 0)
    if not periods:
        return ['no frame period recorded']
    period = periods[len(periods) // 2]
    frames = [f for f in res.frames if t0 <= f['t_us'] <= t1]
    released = [r for r in res.releases if t0 <= r['t_us'] <= t1]
    before = [r for r in res.releases if r['t_us'] < t0 - 1e6]
    want = (t1 - t0) * 1e3 / period
    errors = []
    if abs(len(frames) - want) > 0.15 * want:
        errors.append('%d headset frames while paused, want about %.0f'
                      % (len(frames), want))
    if still and len(released) > 2:
        errors.append('%d images released while paused, want none'
                      % len(released))
    if not before:
        errors.append('no images were released before the pause')
    return errors


def check_pacing_live(res):
    return check_pacing(res, still=False)


def check_focus(res):
    """Pause Content When Not Active with the window unfocused: the content
    runs while the headset session is focused, and pauses while the
    session is only visible."""
    if '[e2e] FocusOut sent to window' not in res.log:
        return ['no FocusOut was sent to the window']
    t = dict((k, v * 1e6) for k, v in res.marks.items())
    errors = []
    for a, b, runs, what in (('unfocused', 'visible', True, 'focused'),
                             ('visible', 'refocused', False, 'visible'),
                             ('refocused', 'end', True, 'focused again')):
        n = len([r for r in res.releases if t[a] + 1e6 <= r['t_us'] <= t[b]])
        if runs and n < 30:
            errors.append('%d images released with the session %s, want the '
                          'content running' % (n, what))
        elif not runs and n > 2:
            errors.append('%d images released with the session %s, want the '
                          'content paused' % (n, what))
    return errors


def window_is(res, want, what):
    if not res.shot:
        return ['no screenshot']
    w, h, bpp, rows = read_png(res.shot)
    x, y = res.top_px
    got = tuple(rows[y][x * bpp:x * bpp + 3])
    if not near(got, want):
        return ['the window shows %s at the top screen, want %s (%s)'
                % (got, want, what)]
    return []


def check_no_runtime(res):
    errors = []
    # The loader may fail listing extensions or creating the instance.
    if (     '[OpenXR] No runtime' not in res.log
            and '[OpenXR] The runtime lacks' not in res.log):
        errors.append('no "[OpenXR] No runtime" in the log')
    if any(quads(f) for f in res.frames):
        errors.append('quads were submitted without a runtime')
    return errors + window_is(res, GREEN, 'the 2D map')


def check_session_fails(res):
    errors = []
    for line in ('[OpenXR] Rebuilding video without headset output.',
                 '[Video] Reinitialising the video driver at its request.',
                 '[OpenXR] Starting once without headset output after a failure.'):
        if line not in res.log:
            errors.append('missing "%s"' % line)
    if any(quads(f) for f in res.frames):
        errors.append('quads were submitted after the session failed')
    return errors + window_is(res, GREEN, 'the 2D map')


def check_session_lost(res):
    errors = []
    if '[OpenXR] The headset session ended' not in res.log:
        errors.append('no "[OpenXR] The headset session ended" in the log')
    lost = res.marks.get('lost', 0) * 1e6
    late = [f for f in res.frames if f['t_us'] > lost + 500000 and quads(f)]
    if late:
        errors.append('%d frames with quads after the session was lost'
                      % len(late))
    return errors + window_is(res, GREEN, 'the 2D map again')


def check_exit_steam(res):
    """Launched by Steam, the runtime's EXITING (Steam's Exit Game) quits
    RetroArch cleanly before Steam would kill it."""
    errors = []
    if 'exited' not in res.marks:
        errors.append('RetroArch kept running after EXITING')
    elif res.marks['exited'] - res.marks.get('exit', 0) > 3:
        errors.append('quit %.1f s after EXITING, want under 3'
                      % (res.marks['exited'] - res.marks.get('exit', 0)))
    if '[Video] The headset asked to exit; quitting for Steam.' not in res.log:
        errors.append('no quit line in the log')
    return errors


def check_exit_window(res):
    """Not launched by Steam: EXITING ends the session, the window keeps
    the picture and RetroArch keeps running."""
    errors = check_session_lost(res)
    if 'exited' in res.marks:
        errors.append('RetroArch quit without Steam')
    if ('[Video] The headset asked to exit; the window keeps running.'
            not in res.log):
        errors.append('no "window keeps running" line in the log')
    return errors


def check_options_follow(res):
    """With the menu open the core runs no frames; when the headset's
    session ends its views status changes, and the frontend asks the core
    to update its options' display there and then."""
    errors = []
    if '[OpenXR] The headset session ended' not in res.log:
        errors.append('no "[OpenXR] The headset session ended" in the log')
    seen = re.findall(r'\[video_views\] options display status=(\d+)',
                      res.log)
    # Status 7 (HMD) while the headset tracks; 1 only after it ended.
    if '7' not in seen or '1' not in seen[seen.index('7'):]:
        errors.append('no options display update from status 7 to 1 (2D) '
                      'after the session ended: %s' % seen)
    return errors


# ---- Headset input ----

PAD_RE = re.compile(r'\[video_views\] pad port=(\d) buttons=0x([0-9a-f]+) '
                    r'lx=(-?\d+) ly=(-?\d+) rx=(-?\d+) ry=(-?\d+) '
                    r'l2=(-?\d+) r2=(-?\d+)')
POINTER_RE = re.compile(r'\[video_views\] pointer (?:x=(-?\d+) y=(-?\d+) '
                        r'pressed=1 packed=\((\d+),(\d+)\)|pressed=0)')
GUN_RE = re.compile(r'\[video_views\] lightgun x=(-?\d+) y=(-?\d+) '
                    r'offscreen=(\d) trigger=(\d) '
                    r'packed=\((-?\d+),(-?\d+)\)')
RUMBLE_RE = re.compile(r'\[video_views\] rumble port=(\d) strong=(\d+) '
                       r'weak=(\d+)')
MENU_RE = re.compile(r'\[Menu\] Headset pointer x=(-?\d+) y=(-?\d+) '
                     r'pressed=(\d) selection=(\d+)')
RP = dict((n, 1 << i) for i, n in enumerate(
    ('B', 'Y', 'SELECT', 'START', 'UP', 'DOWN', 'LEFT', 'RIGHT',
     'A', 'X', 'L', 'R', 'L2', 'R2', 'L3', 'R3')))
PROFILES = ('/interaction_profiles/valve/index_controller',
            '/interaction_profiles/oculus/touch_controller',
            '/interaction_profiles/htc/vive_controller',
            '/interaction_profiles/khr/simple_controller')


def script(*lines):
    return '\n'.join(lines)


def core_events(res):
    """The core's and the menu's input lines, in log order, as
    (kind, fields): pad, pointer, gun, rumble, menu."""
    out = []
    for line in res.log.splitlines():
        m = PAD_RE.search(line)
        if m:
            v = [int(g) for g in m.groups()[2:]]
            out.append(('pad', {'port': int(m.group(1)),
                                'buttons': int(m.group(2), 16),
                                'lx': v[0], 'ly': v[1], 'rx': v[2],
                                'ry': v[3], 'l2': v[4], 'r2': v[5]}))
            continue
        m = POINTER_RE.search(line)
        if m:
            if m.group(1) is None:
                out.append(('pointer', {'pressed': 0}))
            else:
                out.append(('pointer', {'pressed': 1,
                                        'cx': int(m.group(3)),
                                        'cy': int(m.group(4))}))
            continue
        m = GUN_RE.search(line)
        if m:
            v = [int(g) for g in m.groups()]
            out.append(('gun', {'x': v[0], 'y': v[1], 'offscreen': v[2],
                                'trigger': v[3], 'cx': v[4], 'cy': v[5]}))
            continue
        m = RUMBLE_RE.search(line)
        if m:
            v = [int(g) for g in m.groups()]
            out.append(('rumble', {'port': v[0], 'strong': v[1],
                                   'weak': v[2]}))
            continue
        m = MENU_RE.search(line)
        if m:
            v = [int(g) for g in m.groups()]
            out.append(('menu', {'x': v[0], 'y': v[1], 'pressed': v[2],
                                 'selection': v[3]}))
    return out


def kinds(evs, kind):
    return [f for k, f in evs if k == kind]


def pads(res, port):
    return [f for f in kinds(core_events(res), 'pad') if f['port'] == port]


def events(res, name):
    return [e for e in res.events if e['ev'] == name]


def close(got, want, tol=2):
    return all(abs(a - b) <= tol for a, b in zip(got, want))


def find(seq, start, pred):
    """The index of the first item from start on that matches, or -1."""
    for i in range(max(start, 0), len(seq)):
        if pred(seq[i]):
            return i
    return -1


def moved(f):
    return any(f[k] for k in ('buttons', 'lx', 'ly', 'rx', 'ry', 'l2', 'r2'))


def controllers(res):
    """RetroArch's controller lines in order, one thread's: 'live' when
    they are read again, 'released' when focus is lost."""
    return ['released' if 'released' in line else 'live'
            for line in res.log.splitlines()
            if '[OpenXR] Controllers: ' in line
            or '[OpenXR] Controllers released' in line]


def bindings_errors(res):
    got = dict((e['profile'], e['result']) for e in events(res, 'bindings'))
    return ['bindings for %s: %s (see Monado\'s warning in run.log)'
            % (p, got.get(p, 'not suggested'))
            for p in PROFILES if got.get(p) != 0]


def sync_errors(res, want):
    seen = set(tuple(sorted(e['sets'])) for e in events(res, 'sync'))
    seen.discard(())
    if not seen:
        return ['xrSyncActions never made a set active']
    return ['synced %s, want only %s' % (list(s), sorted(want))
            for s in sorted(seen) if s != tuple(sorted(want))]


def cursors(frame):
    """The laser's dots: small blended quads from the image's dot."""
    return [q for q in quads(frame)
            if q['flags'] & BLEND and q['size'][0] < 0.1
            and q['rect'][0] == 0]


def menus(frame):
    """The menu: large blended quads, not edit mode's outlines."""
    return [q for q in quads(frame)
            if q['flags'] & BLEND and q['size'][0] >= 0.1
            and q['rect'][0] == 0]


ALL_BUTTONS = script(*['action combined/%s 1' % n for n in (
    'b', 'y', 'select', 'start', 'dpad_up', 'dpad_down', 'dpad_left',
    'dpad_right', 'a', 'x', 'l', 'r', 'l2', 'r2', 'l3', 'r3')])
STICKS = script('action combined/left_stick 0.5 0.5',
                'action combined/right_stick -1 -0.25',
                'action combined/l2 0.25', 'action combined/r2 0.75')
SEPARATE = script('action separate/b@left 1',
                  'action separate/stick@left 0.5 0.5',
                  'action separate/a@right 1',
                  'action separate/r2@right 0.6',
                  'action combined/x 1')
HELD = script('action combined/b 1', 'action combined/left_stick 1 0')
RETURNED = script('action combined/a 1', 'action combined/left_stick -1 0')
DPAD = script('action combined/left_stick 0.8 0.8',
              'action combined/right_stick 0.5 0')


FRAME_PROFILE = '/interaction_profiles/valve/frame_controller_valve'
_FL = '/user/hand/left/input/'
_FR = '/user/hand/right/input/'
# The Frame's right controller has A, B, X and Y, the left a D-pad; the
# right stick's click opens the RetroArch menu in place of R3, and the
# grips are left to edit mode, whose Reset is the left D-pad's down.
FRAME_BINDS = set([
    ('combined/b', _FR + 'a/click'), ('combined/a', _FR + 'b/click'),
    ('combined/y', _FR + 'x/click'), ('combined/x', _FR + 'y/click'),
    ('combined/dpad_up', _FL + 'dpad_up/click'),
    ('combined/dpad_down', _FL + 'dpad_down/click'),
    ('combined/dpad_left', _FL + 'dpad_left/click'),
    ('combined/dpad_right', _FL + 'dpad_right/click'),
    ('combined/select', _FL + 'view/click'),
    ('combined/start', _FR + 'menu/click'),
    ('combined/l', _FL + 'bumper/click'), ('combined/r', _FR + 'bumper/click'),
    ('combined/l2', _FL + 'trigger/value'),
    ('combined/r2', _FR + 'trigger/value'),
    ('combined/l3', _FL + 'thumbstick/click'),
    ('combined/left_stick', _FL + 'thumbstick'),
    ('combined/right_stick', _FR + 'thumbstick'),
    ('combined/menu', _FR + 'thumbstick/click'),
    ('separate/b', _FL + 'dpad_down/click'), ('separate/b', _FR + 'a/click'),
    ('separate/a', _FL + 'dpad_right/click'), ('separate/a', _FR + 'b/click'),
    ('separate/r', _FL + 'bumper/click'), ('separate/r', _FR + 'bumper/click'),
    ('separate/r2', _FL + 'trigger/value'),
    ('separate/r2', _FR + 'trigger/value'),
    ('separate/start', _FL + 'thumbstick/click'),
    ('separate/start', _FR + 'thumbstick/click'),
    ('separate/stick', _FL + 'thumbstick'),
    ('separate/stick', _FR + 'thumbstick'),
    ('separate/menu', _FL + 'view/click'),
    ('edit/grab', _FL + 'squeeze/value'), ('edit/grab', _FR + 'squeeze/value'),
    ('edit/tilt', _FL + 'trigger/value'), ('edit/tilt', _FR + 'trigger/value'),
    ('edit/stick', _FL + 'thumbstick'), ('edit/stick', _FR + 'thumbstick'),
    ('edit/reset', _FL + 'dpad_down/click'), ('edit/reset', _FR + 'a/click'),
    ('edit/menu', _FR + 'thumbstick/click'),
    ('edit/select_next', _FL + 'dpad_right/click'),
    ('edit/select_previous', _FL + 'dpad_left/click'),
    ('pointer/aim', _FL + 'aim/pose'), ('pointer/aim', _FR + 'aim/pose'),
    ('pointer/rumble', '/user/hand/left/output/haptic'),
    ('pointer/rumble', '/user/hand/right/output/haptic')])


def check_frame_profile(res):
    """A runtime offering the Frame's extension gets it enabled and the
    Frame's own bindings, besides the usual profiles."""
    errors = bindings_errors(res)
    got = [e for e in events(res, 'bindings') if e['profile'] == FRAME_PROFILE]
    if not got:
        return errors + ['no bindings suggested for %s' % FRAME_PROFILE]
    if got[0]['result'] != 0:
        errors.append('bindings for %s: %s' % (FRAME_PROFILE, got[0]['result']))
    binds = set(tuple(b) for b in got[0]['binds'])
    for b in sorted(FRAME_BINDS - binds):
        errors.append('Frame binding missing: %s <- %s' % b)
    for b in sorted(binds - FRAME_BINDS):
        errors.append('Frame binding not wanted: %s <- %s' % b)
    if '[OpenXR] Bindings suggested for %s.' % FRAME_PROFILE not in res.log:
        errors.append('no "Bindings suggested for %s" in the log'
                      % FRAME_PROFILE)
    return errors


def check_no_frame_profile(res):
    """Without the extension the Frame's profile is left alone."""
    errors = bindings_errors(res)
    if any(e['profile'] == FRAME_PROFILE for e in events(res, 'bindings')):
        errors.append('bindings suggested for %s without its extension'
                      % FRAME_PROFILE)
    return errors


TOUCH_PROFILE = '/interaction_profiles/oculus/touch_controller'
START_MENU = 'action combined/start_menu 1'


def touch_start_menu_binds(res):
    """Touch's one menu button is Start and the RetroArch menu."""
    got = [e for e in events(res, 'bindings')
           if e['profile'] == TOUCH_PROFILE]
    if not got:
        return ['no bindings suggested for %s' % TOUCH_PROFILE]
    binds = set(tuple(b) for b in got[0]['binds'])
    errors = []
    if ('combined/start_menu',
            '/user/hand/left/input/menu/click') not in binds:
        errors.append('Touch\'s menu button is not combined/start_menu')
    if any(b[0] == 'combined/start' for b in binds):
        errors.append('Touch still binds combined/start')
    return errors


def start_seen(res):
    return any(f['buttons'] & RP['START'] for f in pads(res, 0))


def check_start_menu_tap(res):
    """Touch's menu button tapped: Start for the core, no menu."""
    errors = touch_start_menu_binds(res)
    if not start_seen(res):
        errors.append('a tap gave the core no Start')
    if any(menus(f) for f in res.frames):
        errors.append('a tap opened the menu')
    return errors


def check_start_menu_hold(res):
    """Held past a second and let go, the RetroArch menu opens (the
    input driver toggles on release); the core has Start while it is
    held."""
    errors = touch_start_menu_binds(res)
    release = res.marks['release'] * 1e6
    opened = [f['t_us'] for f in res.frames if menus(f)]
    if not opened:
        errors.append('holding the menu button did not open the menu')
    elif opened[0] < release:
        errors.append('the menu opened %.2f s before the release'
                      % ((release - opened[0]) / 1e6))
    elif opened[0] - release > 1.5e6:
        errors.append('the menu opened %.2f s after the release'
                      % ((opened[0] - release) / 1e6))
    if not start_seen(res):
        errors.append('the core had no Start while the button was held')
    return errors


# The headset's controllers as a controller: a joypad device in a slot,
# read through the port's binds.
DEVICE_IN_RE = re.compile(r'\[Input\] Device \d+, "([^"]+)" '
                          r'\([0-9a-f]{4}:[0-9a-f]{4}, (\w+)\): '
                          r'(?:new|back), in slot (\d+)\.')
DEVICE_LEFT_RE = re.compile(r'\[Input\] Device \d+, "([^"]+)": '
                            r'left slot (\d+)\.')
# The test joypad driver's pad in slot 0, from the start or later.
PAD_FIRST = os.path.join(HERE, 'pad-first.ratst')
PAD_LATER = os.path.join(HERE, 'pad-later.ratst')
# RGUI, OK and Cancel unswapped, from the Quick Menu: back to the Main
# Menu, Settings (row 5), Input (3), RetroPad Binds, Port 1 Controls, and
# B Button (row 14), whose OK starts the capture.
BIND_B_NAV = ([('send', 'MENU_B'), ('wait', 0.5)]
              + [('send', 'MENU_DOWN'), ('wait', 0.3)] * 5
              + [('send', 'MENU_A'), ('wait', 0.5)]
              + [('send', 'MENU_DOWN'), ('wait', 0.3)] * 3
              + [('send', 'MENU_A'), ('wait', 0.5)] * 3
              + [('send', 'MENU_DOWN'), ('wait', 0.3)] * 14
              + [('send', 'MENU_A'), ('wait', 1)])


def device_lines(res):
    """The registry's lines, in log order: ('in' or 'left', name, slot),
    a pad of the test driver's named 'pad'."""
    out = []
    for line in res.log.splitlines():
        m = DEVICE_IN_RE.search(line)
        if m:
            out.append(('in', 'pad' if m.group(2) == 'test' else m.group(1),
                        int(m.group(3))))
        m = DEVICE_LEFT_RE.search(line)
        if m:
            out.append(('left', m.group(1), int(m.group(2))))
    return out


def check_input_device(res):
    """The session's controllers are one controller, in slot 0 and on
    port 1, read through the port, and gone with the session."""
    errors = []
    if '[Autoconf] OpenXR Controllers configured in port 1.' not in res.log:
        errors.append('no "OpenXR Controllers configured in port 1" in the '
                      'log')
    seq = [l for l in device_lines(res) if l[1] != 'pad']
    if seq[:1] != [('in', 'OpenXR Controllers', 0)]:
        errors.append('the device lines start %s, want the device in slot 0'
                      % seq[:2])
    elif ('left', 'OpenXR Controllers', 0) not in seq[1:]:
        errors.append('the device never left slot 0 when the session ended: '
                      '%s' % seq)
    if ('[Autoconf] OpenXR Controllers disconnected from port 1.'
            not in res.log):
        errors.append('no "OpenXR Controllers disconnected from port 1" in '
                      'the log')
    if find(pads(res, 0), 0, lambda f: f['buttons'] == RP['A']) < 0:
        errors.append('A never reached player 1 through the device')
    return errors


def check_input_device_second(res):
    """A pad in slot 0 from the start: the headset is the next controller,
    slot 1 and port 2, which is the core's player 2."""
    errors = []
    if device_lines(res)[:2] != [('in', 'pad', 0),
                                 ('in', 'OpenXR Controllers', 1)]:
        errors.append('the device lines start %s, want the pad in slot 0 '
                      'and the headset in slot 1' % device_lines(res)[:3])
    if '[Autoconf] OpenXR Controllers configured in port 2.' not in res.log:
        errors.append('no "OpenXR Controllers configured in port 2" in the '
                      'log')
    if find(pads(res, 1), 0, lambda f: f['buttons'] == RP['A']) < 0:
        errors.append('A never reached player 2')
    if any(f['buttons'] for f in pads(res, 0)):
        errors.append('the headset reached player 1')
    return errors


def check_input_device_taken(res):
    """A pad the driver puts in the headset's slot takes it; the headset
    moves to slot 1 and keeps player 1."""
    errors = []
    seq = [l for l in device_lines(res) if l[0] == 'in']
    if seq[:3] != [('in', 'OpenXR Controllers', 0), ('in', 'pad', 0),
                   ('in', 'OpenXR Controllers', 1)]:
        errors.append('the device lines are %s, want the headset in slot '
                      '0, the pad there, the headset in slot 1' % seq[:4])
    if find(pads(res, 0), 0, lambda f: f['buttons'] == RP['A']) < 0:
        errors.append('A never reached player 1 after the move')
    if any(f['buttons'] for f in pads(res, 1)):
        errors.append('the headset reached player 2')
    return errors


def check_input_hotkeys(res):
    """Hotkey Enable on L3 and Screenshot on X: X alone reaches the core
    and takes no screenshot; with L3 held it takes one."""
    errors = []
    if find(pads(res, 0), 0, lambda f: f['buttons'] == RP['X']) < 0:
        errors.append('X alone never reached player 1')
    shots = [os.path.join(res.dir, 'shots', x)
             for x in os.listdir(os.path.join(res.dir, 'shots'))
             if x.endswith('.png')]
    combo = res.marks.get('combo', 0) + time.time() - time.monotonic()
    early = [s for s in shots if os.path.getmtime(s) < combo]
    if early:
        errors.append('X without Hotkey Enable took a screenshot')
    if len(shots) - len(early) != 1:
        errors.append('L3 with X took %d screenshots, want 1'
                      % (len(shots) - len(early)))
    return errors


def check_input_hotkeys_separate(res):
    """Separate, Menu Toggle on R: the right hand is port 2 and opens
    nothing; the left hand is port 1, the hotkey port, and opens the
    menu."""
    errors = []
    for name, port in (('Left', 1), ('Right', 2)):
        if ('[Autoconf] OpenXR %s Controller configured in port %d.'
                % (name, port)) not in res.log:
            errors.append('no "OpenXR %s Controller configured in port %d" '
                          'in the log' % (name, port))
    left = res.marks.get('left', 0) * 1e6
    opened = [f['t_us'] for f in res.frames if menus(f)]
    if any(t < left for t in opened):
        errors.append('R on the right hand opened the menu')
    if not any(t >= left for t in opened):
        errors.append('R on the left hand did not open the menu')
    return errors


def check_input_port_binds(res):
    """Port 1's own binds with A and B swapped apply to the headset: its
    A press, then its B press, reach player 1 as B, then A."""
    presses = []
    for f in pads(res, 0):
        if not presses or presses[-1] != f['buttons']:
            presses.append(f['buttons'])
    presses = [b for b in presses if b]
    if presses != [RP['B'], RP['A']]:
        return ['the headset\'s A then B reached player 1 as %s, want B '
                'then A' % ['0x%x' % b for b in presses]]
    return []


def check_input_bind_capture(res):
    """X captured into Port 1's B Button through the menu: X is then B
    too."""
    p0 = pads(res, 0)
    if find(p0, 0, lambda f: f['buttons'] == RP['B'] | RP['X']) < 0:
        return ['X never reached player 1 as B after the capture: %s'
                % p0[-3:]]
    return []


def check_combined(res):
    errors = bindings_errors(res) + sync_errors(res, ('combined', 'pointer'))
    p0 = pads(res, 0)
    i = find(p0, 0, lambda f: f['buttons'] == 0xffff
             and (f['l2'], f['r2']) == (32767, 32767))
    # Sticks up are negative Y; analog L2/R2 with only R2 past halfway.
    j = find(p0, i + 1, lambda f: f['buttons'] == RP['R2'] and close(
        (f['lx'], f['ly'], f['rx'], f['ry'], f['l2'], f['r2']),
        (16383, -16383, -32767, 8191, 8191, 24575)))
    k = find(p0, j + 1, lambda f: not moved(f))
    if i < 0:
        errors.append('player 1 never had every button and both triggers: %s'
                      % p0[-3:])
    elif j < 0:
        errors.append('player 1 never had the sticks and half triggers: %s'
                      % p0[i:i + 3])
    elif k < 0:
        errors.append('player 1 was not released')
    if any(moved(f) for f in pads(res, 1)):
        errors.append('player 2 moved in Combined')
    if '[OpenXR] Controllers: combined.' not in res.log:
        errors.append('no "[OpenXR] Controllers: combined." in the log')
    if 'Headset recenter requested' not in res.log:
        errors.append('Recenter did not reach the hotkey')
    if not any(menus(f) for f in res.frames):
        errors.append('RetroArch Menu did not open the menu')
    return errors


def check_separate(res):
    errors = bindings_errors(res) + sync_errors(res, ('separate', 'pointer'))
    p0, p1 = pads(res, 0), pads(res, 1)
    if find(p0, 0, lambda f: f['buttons'] == RP['B']
            and close((f['lx'], f['ly']), (16383, -16383))) < 0:
        errors.append('player 1 never had B and the left stick: %s' % p0[-3:])
    if find(p1, 0, lambda f: f['buttons'] == RP['A'] | RP['R2']
            and close((f['r2'],), (19660,))) < 0:
        errors.append('player 2 never had A and R2: %s' % p1[-3:])
    if any(f['buttons'] & RP['X'] for f in p0 + p1):
        errors.append('an action of the unsynced Combined set reached a pad')
    if not p0 or moved(p0[-1]) or not p1 or moved(p1[-1]):
        errors.append('the pads were not released at the end')
    if '[OpenXR] Controllers: separate.' not in res.log:
        errors.append('no "[OpenXR] Controllers: separate." in the log')
    return errors


def check_input_focus(res):
    p0 = pads(res, 0)
    i = find(p0, 0, lambda f: f['buttons'] == RP['B'] and f['lx'] == 32767)
    j = find(p0, i + 1, lambda f: not moved(f)) if i >= 0 else -1
    k = (find(p0, j + 1, lambda f: f['buttons'] == RP['B']
              and f['lx'] == 32767) if j >= 0 else -1)
    errors = []
    if i < 0:
        errors.append('B and the stick never reached player 1')
    elif j < 0:
        errors.append('still held after the headset lost focus')
    elif k < 0:
        errors.append('not held again once the headset had focus back')
    if 'Controllers released: the headset is not focused' not in res.log:
        errors.append('no "Controllers released" in the log')
    return errors


def check_input_sync_unfocused(res):
    """xrSyncActions answers XR_SESSION_NOT_FOCUSED (8), which succeeds
    with nothing active, ahead of the state event: the pads release and
    nothing is logged as not read."""
    p0 = pads(res, 0)
    i = find(p0, 0, lambda f: f['buttons'] == RP['B'] and f['lx'] == 32767)
    j = find(p0, i + 1, lambda f: not moved(f)) if i >= 0 else -1
    k = (find(p0, j + 1, lambda f: f['buttons'] == RP['B']
              and f['lx'] == 32767) if j >= 0 else -1)
    errors = []
    if i < 0:
        errors.append('B and the stick never reached player 1')
    elif j < 0:
        errors.append('still held while the sync said not focused')
    elif k < 0:
        errors.append('not held again once the sync had focus back')
    if 'Controllers not read' in res.log:
        errors.append('"Controllers not read" in the log for a sync that '
                      'succeeded')
    return errors


def check_input_unfocused(res):
    """The window unfocused, with Pause Content When Not Active and no
    background joypads: check_focus()'s pause rule, and the headset's
    controllers still reach player 1 while its session is focused. The
    content is paused while the session is only visible, so the core
    never reads the release: RetroArch's log shows it."""
    errors = check_focus(res)
    p0 = pads(res, 0)
    i = find(p0, 0, lambda f: f['buttons'] == RP['B'] and f['lx'] == 32767)
    # RETURNED is only scripted once the session is focused again.
    j = (find(p0, i + 1, lambda f: f['buttons'] == RP['A']
              and f['lx'] == -32767) if i >= 0 else -1)
    if i < 0:
        errors.append('B and the stick never reached player 1 with the '
                      'window unfocused')
    elif j < 0:
        errors.append('A and the stick never reached player 1 once the '
                      'headset had focus back: %s' % p0[i:i + 3])
    seq = controllers(res)
    if seq[:3] != ['live', 'released', 'live']:
        errors.append('controllers went %s, want live, released while '
                      'visible, live again' % seq)
    return errors


def check_input_dpad(res):
    """Forced Left Analog on player 1: the left stick is the D-pad and
    reads centred, the right stays analog."""
    p0 = pads(res, 0)
    i = find(p0, 0, lambda f: f['buttons'] == RP['UP'] | RP['RIGHT']
             and (f['lx'], f['ly']) == (0, 0)
             and close((f['rx'], f['ry']), (16383, 0)))
    errors = []
    if i < 0:
        errors.append('player 1 never had Up and Right from the left stick, '
                      'centred, with the right stick at 0.5: %s' % p0[-3:])
    elif find(p0, i + 1, lambda f: not moved(f)) < 0:
        errors.append('player 1 was not released')
    return errors


# Rays from a hand at (0.2, -0.4, -0.3) to points on the default 3DS
# quads: screen 0 at (0, 0, -1.8) 1.6x0.96, screen 1 at (0, -0.992, -1.8)
# 1.28x0.96, the menu at (0, 0, -1.7) 1.6x0.96.
AIM_BOTTOM = 'aim right 0.2 -0.4 -0.3 -0.32 -1.232 -1.8'   # screen 1, 0.25 0.75
AIM_TOP = 'aim right 0.2 -0.4 -0.3 0.4 0.24 -1.8'          # screen 0, 0.75 0.25
AIM_GUN = 'aim right 0.2 -0.4 -0.3 -0.4 0.24 -1.8'         # screen 0, 0.25 0.25
AIM_MISS = 'aim right 0.2 -0.4 -0.3 3.0 0.0 -1.8'          # beside every quad
AIM_LEFT_MISS = 'aim left -0.2 -0.4 -0.3 -3.0 0.0 -1.8'    # beside every quad
AIM_LEFT_BOTTOM = 'aim left -0.2 -0.4 -0.3 -0.32 -0.752 -1.8'   # screen 1, 0.25 0.25
AIM_RIGHT_BOTTOM = 'aim right 0.2 -0.4 -0.3 0.32 -1.232 -1.8'   # screen 1, 0.75 0.75
L2 = 'action combined/l2 1'
R2 = 'action combined/r2 1'


def trigger_errors(res):
    if any(f['buttons'] & (RP['L2'] | RP['R2']) or f['l2'] or f['r2']
           for f in pads(res, 0)):
        return ['a trigger that pointed also pressed L2 or R2']
    return []


def rect_colour(res, fr, q, fx, fy):
    """colour() at a fraction of the part of its image a quad shows."""
    w, h, bpp, rows = read_png(image(res, fr, q))
    x0, y0, rw, rh = q['rect']
    x = min(w - 1, x0 + int(fx * rw))
    y = min(h - 1, y0 + int(fy * rh))
    px = rows[y][x * bpp:x * bpp + bpp]
    return tuple(px[:3]), (px[3] if bpp == 4 else 255)


def cursor_errors(res, want):
    shown = [f for f in res.frames if cursors(f)]
    if not shown:
        return ['no laser dot']
    errors = []
    c = cursors(shown[-1])[0]
    if not at(c, want):
        errors.append('the dot is at %s, want %s' % (c['pose'][:3], want))
    snaps = [f for f in shown if f['snap']]
    if not snaps:
        errors.append('no snapshot with the dot')
    else:
        fr = snaps[-1]
        dot = cursors(fr)[0]
        rgb, alpha = rect_colour(res, fr, dot, 0.5, 0.5)
        if not near(rgb, WHITE) or alpha < 250:
            errors.append('the dot\'s centre is %s alpha %d' % (rgb, alpha))
        _, alpha = rect_colour(res, fr, dot, 0.02, 0.02)
        if alpha:
            errors.append('the dot\'s corner is not transparent')
    if cursors(res.frames[-1]):
        errors.append('the dot stayed after the aim went away')
    return errors


def check_touch(res):
    evs = core_events(res)
    presses = [f for f in kinds(evs, 'pointer') if f['pressed']]
    errors = []
    # Screen 1 is the view (240, 240, 320, 240) of the 800x480 frame.
    if not presses:
        errors.append('no touch reached the core')
    elif not close((presses[0]['cx'], presses[0]['cy']), (320, 420), 3):
        errors.append('touched at %d,%d, want 320,420'
                      % (presses[0]['cx'], presses[0]['cy']))
    if not any(not f['pressed'] for f in kinds(evs, 'pointer')):
        errors.append('the touch was not released')
    return (errors + trigger_errors(res)
            + cursor_errors(res, (-0.32, -1.232, -1.8)))


def check_top_auto(res):
    errors = []
    if find(pads(res, 0), 0, lambda f: f['buttons'] & RP['R2']
            and f['r2'] == 32767) < 0:
        errors.append('the trigger on the top screen was not R2')
    if any(f['pressed'] for f in kinds(core_events(res), 'pointer')):
        errors.append('the top screen was touched in Auto')
    if any(cursors(f) for f in res.frames):
        errors.append('a dot showed on the top screen in Auto')
    return errors


def check_touch_drag(res):
    """A touch dragged off screen 1 onto the top screen and back, the
    trigger held: no R2, read offscreen while off, and back on screen 1 it
    touches again, as a stylus does."""
    evs = core_events(res)
    touches = kinds(evs, 'pointer')
    errors = []
    first = find(touches, 0, lambda f: f['pressed'])
    lift = (find(touches, first + 1, lambda f: not f['pressed'])
            if first >= 0 else -1)
    again = (find(touches, lift + 1, lambda f: f['pressed'])
             if lift >= 0 else -1)
    if first < 0:
        return ['no touch reached the core']
    if not packed_at(touches[first], (320, 420)):
        errors.append('touched at %s, want 320,420' % touches[first])
    if lift < 0:
        errors.append('the touch did not lift off the screen')
    elif again < 0:
        errors.append('the held trigger did not touch again back on the '
                      'screen')
    elif not packed_at(touches[again], (320, 420)):
        errors.append('touched again at %s, want 320,420' % touches[again])
    runs = sum(1 for i, f in enumerate(touches) if f['pressed']
               and (i == 0 or not touches[i - 1]['pressed']))
    if runs != 2:
        errors.append('touched %d times, want twice' % runs)
    t = find(evs, 0, lambda kf: kf[0] == 'pointer' and kf[1]['pressed'])
    off = find(evs, t + 1, lambda kf: kf[0] == 'gun' and kf[1]['offscreen'])
    if off < 0:
        errors.append('the dragged touch did not read offscreen')
    elif evs[off][1]['trigger']:
        errors.append('the gun fired off the screens in Auto')
    return errors + trigger_errors(res)


def check_touch_held(res):
    """R2 held on the top screen, then aimed onto screen 1: it stays R2
    and touches nothing; let go and pulled again there, it touches."""
    evs = core_events(res)

    def r2(kf, on):
        return (kf[0] == 'pad' and kf[1]['port'] == 0
                and bool(kf[1]['buttons'] & RP['R2']) == on)
    i = find(evs, 0, lambda kf: r2(kf, True))
    j = find(evs, i + 1, lambda kf: r2(kf, False)) if i >= 0 else -1
    touch = find(evs, 0, lambda kf: kf[0] == 'pointer' and kf[1]['pressed'])
    if i < 0:
        return ['the trigger on the top screen was not R2']
    if j < 0:
        return ['R2 was not let go']
    errors = []
    if not any(k == 'gun' and packed_at(f, (320, 420)) for k, f in evs[i:j]):
        errors.append('the laser never pointed at screen 1 while R2 was held')
    if touch < 0:
        errors.append('a new pull on screen 1 did not touch it')
    elif touch < j:
        errors.append('R2 held onto screen 1 touched it')
    elif not packed_at(evs[touch][1], (320, 420)):
        errors.append('touched at %s, want 320,420' % evs[touch][1])
    if any(r2(kf, True) for kf in evs[j:]):
        errors.append('the new pull on screen 1 pressed R2')
    return errors


def check_laser_off(res):
    """Laser Off: both triggers on screen 1 are L2 and R2, nothing touches
    it, the light gun never reads where the hands aim, and no dot shows.
    The x driver's gun reads offscreen too, so only the aim tells."""
    evs = core_events(res)
    both = RP['L2'] | RP['R2']
    errors = []
    if find(pads(res, 0), 0, lambda f: f['buttons'] & both == both
            and (f['l2'], f['r2']) == (32767, 32767)) < 0:
        errors.append('the triggers on screen 1 were not L2 and R2')
    if any(f['pressed'] for f in kinds(evs, 'pointer')):
        errors.append('screen 1 was touched with the laser off')
    laser = [f for f in kinds(evs, 'gun') if f['trigger']
             or packed_at(f, (320, 420)) or packed_at(f, (320, 300))]
    if laser:
        errors.append('the laser answered the light gun: %s' % laser[:2])
    if any(cursors(f) for f in res.frames):
        errors.append('a dot showed with the laser off')
    return errors


def hold_errors(res, first, back):
    """The pressing hand loses tracking mid-press: the touch and the gun
    hold their last point with no lift, and follow the ray from first to
    back once it is tracked again."""
    evs = core_events(res)
    t0 = (res.marks['lost'] + 0.05) * 1e6
    t1 = res.marks['found'] * 1e6
    lost = [f for f in res.frames if t0 < f['t_us'] < t1]
    errors = []
    if not lost or any(cursors(f) for f in lost):
        errors.append('the laser\'s dot did not go while tracking was lost')
    touches = kinds(evs, 'pointer')
    i = find(touches, 0, lambda f: f['pressed'] and packed_at(f, first))
    j = (find(touches, i + 1, lambda f: f['pressed'] and packed_at(f, back))
         if i >= 0 else -1)
    if i < 0:
        errors.append('no touch at %s: %s' % (first, touches[:3]))
    elif j < 0:
        errors.append('the touch did not follow the ray to %s once tracked '
                      'again: %s' % (back, touches[i:i + 3]))
    elif not all(f['pressed'] for f in touches[i:j + 1]):
        errors.append('the touch lifted while tracking was lost')
    guns = kinds(evs, 'gun')
    i = find(guns, 0, lambda g: g['trigger'] and packed_at(g, first))
    j = (find(guns, i + 1, lambda g: g['trigger'] and packed_at(g, back))
         if i >= 0 else -1)
    if i < 0 or j < 0:
        errors.append('the gun did not fire at %s then %s: %s'
                      % (first, back, guns[-4:]))
    elif any(g['offscreen'] or not g['trigger'] for g in guns[i:j + 1]):
        errors.append('the gun left its point while tracking was lost: %s'
                      % guns[i:j + 1])
    return errors + trigger_errors(res)


def check_touch_tracking(res):
    return hold_errors(res, (320, 420), (480, 420))


def check_gun_hold(res):
    return hold_errors(res, (100, 60), (300, 60))


def check_gun_sweep(res):
    """Always: the held trigger swept off screen 0 and back keeps firing,
    offscreen and on, with no new pull."""
    guns = kinds(core_events(res), 'gun')
    hit = find(guns, 0, lambda g: g['trigger'] and packed_at(g, (100, 60)))
    off = (find(guns, hit + 1, lambda g: g['offscreen'])
           if hit >= 0 else -1)
    back = (find(guns, off + 1, lambda g: not g['offscreen'])
            if off >= 0 else -1)
    if hit < 0:
        return ['the gun never fired on screen 0: %s' % guns[-3:]]
    if off < 0 or back < 0:
        return ['the gun did not leave screen 0 and come back: %s'
                % guns[hit:hit + 4]]
    errors = []
    if not all(g['trigger'] for g in guns[hit:back + 1]):
        errors.append('the held trigger stopped firing across the edge: %s'
                      % guns[hit:back + 1])
    if not packed_at(guns[back], (100, 60)):
        errors.append('back on screen 0 at %s, want 100,60' % guns[back])
    return errors + trigger_errors(res)


def check_lightgun(res):
    """Always: the trigger fires the gun on screen 0, then off every
    screen (an off-screen shot) with no R2; the other hand, off the
    screens too, keeps L2."""
    evs = core_events(res)
    errors = []
    hit = find(evs, 0, lambda kf: kf[0] == 'gun'
               and not kf[1]['offscreen'] and kf[1]['trigger'])
    miss = (find(evs, hit + 1, lambda kf: kf[0] == 'gun'
                 and kf[1]['offscreen']) if hit >= 0 else -1)
    p0 = [i for i, (k, f) in enumerate(evs) if k == 'pad' and f['port'] == 0]
    if hit < 0:
        return ['the light gun never fired on screen 0: %s'
                % kinds(evs, 'gun')[-3:]]
    g = evs[hit][1]
    # Screen 0 maps into the left eye's view (0, 0, 400, 240).
    if not close((g['cx'], g['cy']), (100, 60), 3):
        errors.append('the gun aimed at %d,%d, want 100,60' % (g['cx'], g['cy']))
    if miss < 0:
        errors.append('the gun did not read offscreen on a miss')
    else:
        m = evs[miss][1]
        if (m['x'], m['y'], m['trigger']) != (-32768, -32768, 1):
            errors.append('the shot off the screens reads %s' % m)
        # The core logs a frame's pads just before its gun: the pad lines
        # directly before the miss are the miss's frame.
        k = miss
        while k > 0 and evs[k - 1][0] == 'pad':
            k -= 1
        pointing = ([i for i in p0 if i < hit][-1:]
                    + [i for i in p0 if hit < i < k])
        off = [i for i in p0 if i >= k]
        if any(evs[i][1]['buttons'] & RP['R2'] for i in pointing):
            errors.append('R2 while the gun pointed at the screen')
        if any(evs[i][1]['buttons'] & RP['R2'] for i in off):
            errors.append('R2 while the gun shot off the screens')
        if not any(evs[i][1]['buttons'] & RP['L2'] for i in off):
            errors.append('the other hand off the screens did not press L2')
    return errors + cursor_errors(res, (-0.4, 0.24, -1.8))


def packed_at(f, want):
    return 'cx' in f and close((f['cx'], f['cy']), want, 3)


def check_two_hands(res):
    evs = core_events(res)
    touches = kinds(evs, 'pointer')
    errors = []
    left, right = (320, 300), (480, 420)
    # Before any trigger, both on screen 1: the right hand points.
    aimed = [f for f in kinds(evs, 'gun')
             if packed_at(f, left) or packed_at(f, right)]
    if not aimed or not packed_at(aimed[0], right):
        errors.append('the right hand did not point first: %s' % aimed[:2])
    i = find(touches, 0, lambda f: f['pressed'] and packed_at(f, left))
    j = (find(touches, i + 1, lambda f: f['pressed'] and packed_at(f, right))
         if i >= 0 else -1)
    if i < 0:
        errors.append('the left hand did not touch first: %s' % touches[:3])
    elif j < 0:
        errors.append('the right hand did not take over when its trigger '
                      'went down: %s' % touches[i:i + 3])
    elif not any(not f['pressed'] for f in touches[i + 1:j]):
        errors.append('the touch dragged from hand to hand without a lift')
    if not any(len(cursors(f)) == 2 for f in res.frames):
        errors.append('no frame with a dot for each hand')
    return errors + trigger_errors(res)


def check_frame_always(res):
    """A core without views in Always: the whole frame is a light gun
    target, mapped over the frame the core last sent."""
    evs = core_events(res)
    shots = [f for f in kinds(evs, 'gun')
             if not f['offscreen'] and f['trigger']]
    errors = []
    # (0.25, 0.25) of the 800x480 frame.
    if not shots:
        errors.append('the light gun never fired on the frame: %s'
                      % kinds(evs, 'gun')[-3:])
    elif not close((shots[0]['cx'], shots[0]['cy']), (200, 120), 3):
        errors.append('the gun aimed at %d,%d, want 200,120'
                      % (shots[0]['cx'], shots[0]['cy']))
    return (errors + trigger_errors(res)
            + cursor_errors(res, (-0.4, 0.24, -1.8)))


def check_single_auto(res):
    """A core without views in Auto: no screen is live, so the laser
    leaves the core's pointer and light gun to the mouse, and the trigger
    stays R2; with the menu open, the laser points at the menu alone. The
    laser's miss reads -32768; the x driver's gun follows the mouse,
    which headless gamescope leaves at the window's corner (-32767)."""
    evs = core_events(res)
    errors = []
    if find(pads(res, 0), 0, lambda f: f['buttons'] & RP['R2']
            and f['r2'] == 32767) < 0:
        errors.append('the trigger on the only screen was not R2')
    laser = [f for f in kinds(evs, 'gun')
             if (f['x'], f['y']) == (-32768, -32768) or f['trigger']]
    if laser:
        errors.append('the laser answered the light gun: %s' % laser[:2])
    if any(f['pressed'] for f in kinds(evs, 'pointer')):
        errors.append('the only screen was touched in Auto')
    shown = [f for f in res.frames if cursors(f)]
    if not shown or not all(menus(f) for f in shown):
        errors.append('the dot was not on the open menu alone')
    return errors


def check_gun_tracking(res):
    """Always: the right hand takes the gun on screen 0, then loses
    tracking. It keeps the gun: its trigger still shoots off the screens,
    the left trigger stays L2, and tracked again the right still aims."""
    guns = kinds(core_events(res), 'gun')
    p0 = pads(res, 0)
    errors = []
    hit = find(guns, 0, lambda g: g['trigger'] and packed_at(g, (100, 60)))
    shot = (find(guns, hit + 1, lambda g: g['offscreen'] and g['trigger'])
            if hit >= 0 else -1)
    back = (find(guns, shot + 1, lambda g: not g['trigger']
                 and packed_at(g, (100, 60))) if shot >= 0 else -1)
    if hit < 0:
        errors.append('the right hand never shot screen 0: %s' % guns[-3:])
    elif shot < 0:
        errors.append('the untracked right hand\'s trigger did not shoot '
                      'off the screens: %s' % guns[hit:hit + 4])
    elif back < 0:
        errors.append('tracked again, the right hand did not aim the gun: %s'
                      % guns[shot:shot + 3])
    if not any(f['buttons'] & RP['L2'] and f['l2'] for f in p0):
        errors.append('the left trigger was not L2')
    if any(f['buttons'] & RP['R2'] or f['r2'] for f in p0):
        errors.append('the right trigger pressed R2')
    return errors


AIM_MENU_HIGH = 'aim right 0.2 -0.4 -0.3 0.16 0.192 -1.7'   # the menu, 0.6 0.3
AIM_MENU_LOW = 'aim right 0.2 -0.4 -0.3 0.16 -0.048 -1.7'   # the menu, 0.6 0.55


def check_menu_laser(res):
    """Ozone at 1600x960: pointing lower moves the selection down, and the
    trigger presses on the menu and lets go. The press is a second long,
    so it activates nothing; the click cases click."""
    lines = kinds(core_events(res), 'menu')
    high = [i for i, f in enumerate(lines)
            if close((f['x'], f['y']), (960, 288), 4)]
    low = [i for i, f in enumerate(lines)
           if close((f['x'], f['y']), (960, 528), 4)]
    if not high or not low:
        return ['the laser did not reach the menu at both points: %s'
                % lines[:6]]
    errors = []
    before = lines[high[-1]]['selection']
    hover = [lines[i]['selection'] for i in low if not lines[i]['pressed']]
    if not any(s > before for s in hover):
        errors.append('pointing lower did not move the selection down '
                      '(%d, then %s)' % (before, hover))
    press = [i for i in low if lines[i]['pressed']]
    if not press:
        errors.append('the trigger did not press on the menu')
    elif find(lines, press[0] + 1, lambda f: not f['pressed']) < 0:
        errors.append('the press on the menu was not released')
    shown = [f for f in res.frames if cursors(f)]
    if not shown or not any(at(c, (0.16, -0.048, -1.7))
                            for c in cursors(shown[-1])):
        errors.append('no dot on the menu where the laser points')
    return errors


HEAD_TURNED = 'head 0.5 0.2 0.3 90'
# AIM_MENU_HIGH, AIM_MENU_LOW and AIM_BOTTOM from a head at HEAD_TURNED:
# the same rays relative to the head, given in LOCAL.
AIM_MENU_HIGH_TURNED = 'aim right 0.2 -0.2 0.1 -1.2 0.392 0.14'
AIM_MENU_LOW_TURNED = 'aim right 0.2 -0.2 0.1 -1.2 0.152 0.14'
AIM_BOTTOM_TURNED = 'aim right 0.2 -0.2 0.1 -1.3 -1.032 0.62'


def screen0(frame):
    left = [q for q in quads(frame) if q['eye'] == 'left']
    return left[0] if left else None


def check_placement_follow(res):
    """Follow Head: the screens ride in VIEW space, straight ahead of
    the eyes whichever way the head turns, and the laser's dot on
    screen 1 with them."""
    shown = [f for f in res.frames if screen0(f)]
    if not shown:
        return no_quads(res)
    errors = []
    turned = res.marks.get('turned', 0) * 1e6
    late = [f for f in shown if f['t_us'] > turned + 1e6]
    if not late:
        errors.append('no frames after the head turned')
    for f in [shown[0]] + late[-1:]:
        q = screen0(f)
        if q['space'] != 'view':
            errors.append('screen 0 in %s space, want view' % q['space'])
        elif not at(q, (0.0, 0.0, -1.8)) or not facing(q, (0, 0, 0, 1)):
            errors.append('screen 0 at %s, want straight ahead'
                          % q['pose'])
    dots = [f for f in late if cursors(f)]
    if not dots:
        errors.append('no laser dot after the head turned')
    else:
        c = cursors(dots[-1])[0]
        if c['space'] != 'view' or not at(c, DOT_BOTTOM):
            errors.append('the dot is at %s in %s space, want %s in view'
                          % (c['pose'][:3], c['space'], DOT_BOTTOM))
    return errors


def check_placement_lazy(res):
    """Lazy Follow: a small turn leaves the screens; a large one has them
    glide, not jump, back in front of the head."""
    small = res.marks.get('small', 0) * 1e6
    big = res.marks.get('big', 0) * 1e6
    held = [screen0(f) for f in res.frames
            if small + 1e6 < f['t_us'] < big and screen0(f)]
    after = [screen0(f) for f in res.frames
             if f['t_us'] > big and screen0(f)]
    if not held or not after:
        return no_quads(res)
    errors = []
    if any(q['space'] != 'local' for q in held + after):
        errors.append('a screen outside LOCAL')
    moved = [q['pose'][:3] for q in held if not at(q, (0.0, 0.0, -1.8))]
    if moved:
        errors.append('a 20 degree turn moved screen 0 to %s' % moved[0])
    end = after[-1]
    if (     not at(end, (-1.3, 0.2, 0.3))
            or not facing(end, (0.0, 0.70711, 0.0, 0.70711))):
        errors.append('screen 0 ends at %s, want in front of the turned '
                      'head' % end['pose'])
    if not [q for q in after if not at(q, (0.0, 0.0, -1.8))
            and not at(q, (-1.3, 0.2, 0.3))]:
        errors.append('screen 0 jumped instead of gliding')
    return errors


def check_placement_follow_laser(res):
    """Follow Head with the menu open: the laser points at the menu and
    its dot sits on it, all in VIEW space."""
    errors = check_menu_laser(res)
    shown = [f for f in res.frames if cursors(f)]
    if shown and any(c['space'] != 'view' for c in cursors(shown[-1])):
        errors.append('the laser\'s dot is not in VIEW space')
    return errors


def check_menu_rgui(res):
    """RGUI: the point is in its own small framebuffer, and a click
    selects the entry under it, Take Screenshot, which takes one."""
    lines = kinds(core_events(res), 'menu')
    if not lines:
        return ['the laser never reached the menu']
    errors = []
    if max(f['y'] for f in lines) >= 480:
        errors.append('menu pointer y %d is window pixels, not RGUI\'s'
                      % max(f['y'] for f in lines))
    press = find(lines, 0, lambda f: f['pressed'])
    if press < 0:
        return errors + ['the trigger did not press on the menu']
    first = lines[0]['selection']
    if not any(f['selection'] > first for f in lines[press:]):
        errors.append('clicking lower did not move RGUI\'s selection (%d)'
                      % first)
    if not [x for x in os.listdir(os.path.join(res.dir, 'shots'))
            if x.endswith('.png')]:
        errors.append('the click took no screenshot')
    return errors


# A held hand drifts: 2 degrees right of AIM_MENU_HIGH, on its row. A
# deliberate point: 6 degrees left of it, on its row too.
AIM_MENU_SHAKE = 'aim right 0.2 -0.4 -0.3 0.214 0.192 -1.7'   # 0.634 0.3
AIM_MENU_MOVE = 'aim right 0.2 -0.4 -0.3 0.0 0.192 -1.7'      # 0.5 0.3
# The menu's header: Ozone takes a tap or short press there as Back, a
# long press as nothing. Not at its top edge: a menu cursor drawn across
# it wraps the Vulkan viewport (master's gfx_display_vk_draw()).
AIM_MENU_TOP = 'aim right 0.2 -0.4 -0.3 0.16 0.4224 -1.7'         # 0.6 0.06
AIM_LEFT_MENU_TOP = 'aim left -0.2 -0.4 -0.3 -0.16 0.4224 -1.7'   # 0.4 0.06
AIM_LEFT_YIELD = 'aim left -0.2 -0.4 -0.3 -3.0 0.0 -1.8'   # off every quad
AIM_LEFT_TURN = 'aim left -0.2 -0.4 -0.3 -3.9 0.0 -1.8'    # 6 degrees on
STICK_DOWN = 'action combined/left_stick 0 -1'
# Stick taps shorter than the menu's repeat delay move one entry each;
# two from Restart keep Ozone's list from scrolling (scrolling wraps the
# viewport too). Each drift lasts a tenth of a second, long enough for
# Ozone to take it as a move, and the stick taps once mid-drift.
SHAKE = [step for _ in range(2) for step in (
    ('script', AIM_MENU_SHAKE), ('wait', 0.1),
    ('script', AIM_MENU_HIGH), ('wait', 0.1))] + [
    ('script', script(AIM_MENU_SHAKE, STICK_DOWN)), ('wait', 0.05),
    ('script', script(AIM_MENU_HIGH, STICK_DOWN)), ('wait', 0.05),
    ('script', AIM_MENU_SHAKE), ('wait', 0.1),
    ('script', AIM_MENU_HIGH), ('wait', 0.1)]
# Where AIM_MENU_HIGH and AIM_MENU_MOVE land in Ozone's 1600x960, and
# their dots.
MENU_HIGH_PX, MENU_MOVE_PX = (960, 288), (800, 288)
DOT_HIGH, DOT_MOVE = (0.16, 0.192, -1.7), (0.0, 0.192, -1.7)


def at_px(f, want):
    return close((f['x'], f['y']), want, 4)


def hidden_errors(res, hide, show, want, until=None):
    """No dot from 0.3 s after the mark hide until the mark show, and a
    dot at want from 0.3 s after show (and before the mark until)."""
    t0 = (res.marks[hide] + 0.3) * 1e6
    t1 = res.marks[show] * 1e6
    t2 = (res.marks[show] + 0.3) * 1e6
    t3 = res.marks[until] * 1e6 if until else float('inf')
    hidden = [f for f in res.frames if t0 < f['t_us'] < t1]
    errors = []
    if not hidden:
        errors.append('no headset frame between %s and %s' % (hide, show))
    elif any(cursors(f) for f in hidden):
        errors.append('a dot showed between %s and %s' % (hide, show))
    if not any(at(c, want) for f in res.frames if t2 < f['t_us'] < t3
               for c in cursors(f)):
        errors.append('no dot at %s after %s' % (want, show))
    return errors


def stayed_hidden_errors(res, lines):
    """Hover lines: none off the row the laser left until the pull, and
    the pull's own line unpressed on the moved aim."""
    back = find(lines, 0, lambda f: at_px(f, MENU_MOVE_PX))
    if back < 1:
        return ['the laser did not come back on the menu after the pull: %s'
                % lines[:6]], back
    errors = []
    if any(not at_px(f, MENU_HIGH_PX) for f in lines[:back]):
        errors.append('the laser read on the menu while it yielded: %s'
                      % [(f['x'], f['selection']) for f in lines[:back]])
    if any(f['pressed'] for f in lines):
        errors.append('the pull pressed the menu: %s'
                      % [(f['selection'], f['pressed']) for f in lines])
    return errors, back


def check_menu_stick(res):
    """Ozone: the headset's stick takes the menu from the laser. The
    laser hovers Restart (1); the stick taps once under it still and
    once while it drifts 2 degrees, and nothing of the laser reads or
    shows, nor when turned 6 degrees; a pull brings it back unpressed,
    hovering its row. Its first line back still has the stick's
    selection (3): the menu reads that before the hover acts."""
    lines = kinds(core_events(res), 'menu')
    errors, back = stayed_hidden_errors(res, lines)
    if back < 1:
        return errors
    if lines[back]['selection'] != 3:
        errors.append('the laser came back to selection %d, want the '
                      'stick\'s 3' % lines[back]['selection'])
    if lines[-1]['selection'] != 1 or not at_px(lines[-1], MENU_MOVE_PX):
        errors.append('the laser\'s hover did not come back: %s'
                      % [(f['x'], f['selection']) for f in lines[back:]])
    return errors + hidden_errors(res, 'yield', 'pull', DOT_MOVE)


def check_menu_pull(res):
    """Ozone: the laser hovers Restart (1) and yields to a stick tap (2).
    A trigger pulled then brings the laser back and is spent: it presses
    nothing, and no R2 scrolls the menu past 2. The next pull presses."""
    lines = kinds(core_events(res), 'menu')
    presses = [i for i, f in enumerate(lines) if f['pressed']
               and (i == 0 or not lines[i - 1]['pressed'])]
    errors = []
    if not any(f['selection'] == 2 and not f['pressed'] for f in lines):
        errors.append('the laser never read the stick\'s selection (2) '
                      'unpressed: %s'
                      % [(f['selection'], f['pressed']) for f in lines])
    if len(presses) != 1:
        errors.append('the menu was pressed %d times, want once: %s'
                      % (len(presses),
                         [(f['selection'], f['pressed']) for f in lines]))
    if lines and max(f['selection'] for f in lines) > 2:
        errors.append('the menu scrolled past the stick\'s selection: %s'
                      % [f['selection'] for f in lines])
    return errors + hidden_errors(res, 'yield', 'pull', DOT_HIGH)


def check_menu_press_pull(res):
    """Ozone: a stick tap and a trigger pull in one poll yield the laser
    and spend the pull: no press, no R2 past the stick's selection (2),
    no dot. A second pull brings the laser back unpressed; the third
    presses."""
    lines = kinds(core_events(res), 'menu')
    presses = [i for i, f in enumerate(lines) if f['pressed']
               and (i == 0 or not lines[i - 1]['pressed'])]
    errors = []
    if len(presses) != 1:
        errors.append('the menu was pressed %d times, want once: %s'
                      % (len(presses),
                         [(f['selection'], f['pressed']) for f in lines]))
    if lines and max(f['selection'] for f in lines) > 2:
        errors.append('the menu scrolled past the stick\'s selection: %s'
                      % [f['selection'] for f in lines])
    return errors + hidden_errors(res, 'yield', 'pull', DOT_HIGH)


def check_menu_yield_moves(res):
    """Ozone: both hands turning 6 degrees and more leave the yield
    alone: no hover, no dot until a pull brings the laser back."""
    errors, back = stayed_hidden_errors(res, kinds(core_events(res), 'menu'))
    return errors + hidden_errors(res, 'yield', 'pull', DOT_MOVE)


def check_menu_yield_ends(res):
    """Ozone: the laser's toggle twice, and the menu closing and
    reopening, each end a stick tap's yield: the dot is back on the menu
    after each, and gone between a tap and its end."""
    errors = hidden_errors(res, 'yield1', 'toggle', DOT_HIGH, 'yield2')
    return errors + hidden_errors(res, 'yield2', 'reopen', DOT_HIGH)


LASER_NOTICE = '[OpenXR] Laser Pointer: '


def check_laser_toggle(back, want):
    """The toggle turns the laser Off and then back to back, with a
    notice each time: a dot at want before and after, none while Off."""
    def check(res):
        notices = [line.split(LASER_NOTICE, 1)[1].strip()
                   for line in res.log.splitlines() if LASER_NOTICE in line]
        errors = []
        if notices != ['Off', back]:
            errors.append('notices %s, want Off then %s' % (notices, back))
        t = res.marks['off'] * 1e6
        if not any(at(c, want) for f in res.frames if f['t_us'] < t
                   for c in cursors(f)):
            errors.append('no dot at %s before the toggle' % (want,))
        return errors + hidden_errors(res, 'off', 'on', want)
    return check


DOT_BOTTOM = (-0.32, -1.232, -1.8)


def check_laser_action(res):
    """The headset's Laser Pointer action toggles as the hotkey does, and
    no profile suggests a binding for it, as for Recenter: the user binds
    it in the runtime."""
    bound = ['%s <- %s' % (b[0], b[1]) for e in events(res, 'bindings')
             for b in e['binds'] if b[0].endswith('/laser')]
    errors = bindings_errors(res)
    if bound:
        errors.append('Laser Pointer suggested: %s' % bound)
    return errors + check_laser_toggle('Auto', DOT_BOTTOM)(res)


def laser_action_steps(action):
    """The laser on screen 1 and the action pressed twice: Off, back."""
    press = script(AIM_BOTTOM, action)
    return [('wait', 6), ('script', AIM_BOTTOM), ('wait', 2),
            ('mark', 'off'), ('script', press), ('wait', 1),
            ('script', AIM_BOTTOM), ('wait', 1), ('mark', 'on'),
            ('script', press), ('wait', 1), ('script', AIM_BOTTOM),
            ('wait', 2)]


def check_menu_hands(res):
    """Both hands on the menu: the right points first, the left's trigger
    takes the press, and the right's, pulled while the left's is held,
    takes it back with a lift between."""
    lines = kinds(core_events(res), 'menu')
    left, right = (640, 57), (960, 57)
    errors = []
    if not lines or not close((lines[0]['x'], lines[0]['y']), right, 4):
        errors.append('the right hand did not point first: %s' % lines[:2])
    i = find(lines, 0, lambda f: f['pressed']
             and close((f['x'], f['y']), left, 4))
    j = (find(lines, i + 1, lambda f: f['pressed']
              and close((f['x'], f['y']), right, 4)) if i >= 0 else -1)
    if i < 0:
        errors.append('the left hand did not press on the menu: %s'
                      % lines[:4])
    elif j < 0:
        errors.append('the right hand did not take the press over: %s'
                      % lines[i:i + 3])
    elif not any(not f['pressed'] for f in lines[i + 1:j]):
        errors.append('the press dragged from hand to hand without a lift')
    if not any(len(cursors(f)) == 2 for f in res.frames):
        errors.append('no frame with a dot for each hand')
    return errors


# RGUI's 320x240 rows are 11 px from y 26; its Quick Menu has Reset at 1
# and Take Screenshot at 6.
AIM_RGUI_RESET = 'aim right 0.2 -0.4 -0.3 0.16 0.3101 -1.7'        # 0.6 0.177
AIM_RGUI_SHOT = 'aim right 0.2 -0.4 -0.3 0.16 0.09 -1.7'           # 0.6 0.406
# The vb map's frame is 3.4 times as wide as high, so RGUI is fitted to
# 1600x467 at y 247 in the window, and Reset's row is lower on the quad.
AIM_RGUI_RESET_VB = 'aim right 0.2 -0.4 -0.3 0.16 0.1507 -1.7'     # 0.6 0.343
# Shorter than a tap (200 ms).
CLICK = 0.1


def on_quad(c, q):
    """A dot in the plane of a quad that faces the viewer, inside it."""
    d = [a - b for a, b in zip(c['pose'][:3], q['pose'][:3])]
    return (abs(d[2]) <= 0.05 and abs(d[0]) <= q['size'][0] / 2
            and abs(d[1]) <= q['size'][1] / 2)


def menu_cursor_errors(res, uv):
    """The laser's dot is its cursor: no menu snapshot may show the
    menu's own mouse cursor, a bright mark, around the laser's point,
    and at least one snapshot is taken with the laser on the menu."""
    pointed = 0
    for fr in res.frames:
        for q in (menus(fr) if fr['snap'] else []):
            path = image(res, fr, q)
            if not os.path.exists(path):
                continue
            pointed += any(on_quad(c, q) for c in cursors(fr))
            w, h, bpp, rows = read_png(path)
            for y in range(max(0, int((uv[1] - 0.05) * h)),
                           min(h, int((uv[1] + 0.05) * h) + 1)):
                for x in range(max(0, int((uv[0] - 0.03) * w)),
                               min(w, int((uv[0] + 0.03) * w) + 1)):
                    if min(rows[y][x * bpp:x * bpp + 3]) > 200:
                        return ['snapshot %d shows the menu\'s own cursor '
                                'at %s' % (fr['n'], uv)]
    if not pointed:
        return ['no menu snapshot with the laser on the menu']
    return []


def click_errors(res, before, uv):
    """One quick click with `before` selected: the pointed entry, Restart,
    acts once (the core resets once), and at the release, not the press."""
    evs = []
    for line in res.log.splitlines():
        m = MENU_RE.search(line)
        if m:
            evs.append(int(m.group(3)))
        elif '[Core] Reset.' in line:
            evs.append('reset')
    press = find(evs, 0, lambda e: e == 1)
    if press < 0:
        return ['the trigger did not press on the menu']
    lines = kinds(core_events(res), 'menu')
    at_press = [f for f in lines if f['pressed']][0]['selection']
    release = find(evs, press + 1, lambda e: e == 0)
    resets = [i for i, e in enumerate(evs) if e == 'reset']
    errors = []
    if at_press != before:
        errors.append('selection %d at the press, want %d'
                      % (at_press, before))
    if len(resets) != 1:
        errors.append('the click restarted %d times, want once'
                      % len(resets))
    elif release < 0 or resets[0] < release:
        errors.append('the click acted on the press, before the release')
    return errors + menu_cursor_errors(res, uv)


def check_menu_click(res):
    """Ozone: the laser's hover selects Restart, and a quick click on
    it restarts once."""
    return click_errors(res, 1, (0.6, 0.3))


def check_menu_held(res):
    """R2 held when the menu opens under the laser, on RGUI's Reset: the
    laser points, but the held trigger neither presses nor clicks."""
    lines = kinds(core_events(res), 'menu')
    errors = []
    if find(pads(res, 0), 0, lambda f: f['buttons'] & RP['R2']) < 0:
        errors.append('the trigger was not R2 before the menu opened')
    if not lines:
        errors.append('the laser did not point at the menu')
    if any(f['pressed'] for f in lines):
        errors.append('the held trigger pressed on the menu')
    if '[Core] Reset.' in res.log:
        errors.append('the held trigger clicked Reset')
    return errors


def check_menu_glitch(res):
    """RGUI: the pointing hand loses tracking for 0.2 s mid-click on
    Reset. The press stays where it was: one reset, at the release."""
    t0 = (res.marks['lost'] + 0.05) * 1e6
    t1 = res.marks['found'] * 1e6
    lost = [f for f in res.frames if t0 < f['t_us'] < t1]
    errors = []
    if not lost or any(cursors(f) for f in lost):
        errors.append('the laser\'s dot did not go while tracking was lost')
    return errors + click_errors(res, 0, (0.6, 0.177))


def check_menu_close_held(res):
    """RGUI: the menu closes while the laser presses Reset and opens after
    the release. That press ended with the menu, so the reopened menu
    reads no click; a fresh click then resets once."""
    evs = []
    for line in res.log.splitlines():
        m = MENU_RE.search(line)
        if m:
            evs.append(int(m.group(3)))
        elif '[Core] Reset.' in line:
            evs.append('reset')
    starts = [i for i, e in enumerate(evs)
              if e == 1 and (i == 0 or evs[i - 1] != 1)]
    resets = [i for i, e in enumerate(evs) if e == 'reset']
    if resets and (len(starts) < 2 or resets[0] < starts[1]):
        return ['the reopened menu clicked the press it closed on']
    if len(starts) < 2:
        return ['%d presses on the menu, want 2' % len(starts)]
    if len(resets) != 1:
        return ['the fresh click reset %d times, want once' % len(resets)]
    return []


def check_menu_click_rgui_views(res):
    """RGUI letterboxed to a view map's aspect: the laser reads Reset's
    row where it is drawn, and a quick click there restarts once."""
    rows = sorted(set((f['y'] - 26) // 11
                      for f in kinds(core_events(res), 'menu')))
    errors = []
    if rows != [1]:
        errors.append('the laser read RGUI rows %s, want [1]' % rows)
    return errors + click_errors(res, 0, (0.6, 0.343))


def check_menu_click_rgui(res):
    """RGUI, with Core Options selected: a quick click on Reset restarts
    once; the press does nothing to Core Options."""
    return click_errors(res, 4, (0.6, 0.177))


def haptics(res, hand):
    return [e for e in res.events
            if e['ev'] in ('haptic', 'haptic_stop') and e['hand'] == hand]


def rumble_errors(res, want):
    """want: hand -> amplitude every haptic call on it should have. A
    steady rumble is applied again at least every 0.6 s, three times or
    more in a hand's first two-second hold."""
    errors = []
    for hand, amp in sorted(want.items()):
        got = haptics(res, hand)
        on = [e['amplitude'] for e in got if e['ev'] == 'haptic']
        if not on or any(abs(a - amp) > 0.01 for a in on):
            errors.append('the %s hand rumbled at %s, want %.2f'
                          % (hand, on, amp))
        if not got or got[-1]['ev'] != 'haptic_stop':
            errors.append('the %s hand did not stop' % hand)
        holds, hold = [], []
        for e in got:
            if e['ev'] == 'haptic':
                hold.append(e['t_us'])
            elif hold:
                holds.append(hold)
                hold = []
        if hold:
            holds.append(hold)
        if holds and len(holds[0]) < 3:
            errors.append('the %s hand was applied %d times in its first '
                          'hold, want 3 or more' % (hand, len(holds[0])))
        gaps = [b - a for h in holds for a, b in zip(h, h[1:])]
        if gaps and max(gaps) > 600000:
            errors.append('the %s hand went %.2f s between applies'
                          % (hand, max(gaps) / 1e6))
    return errors


def core_rumbled(res):
    if find(kinds(core_events(res), 'rumble'), 0, lambda f: (
            f['port'], f['strong'], f['weak']) == (0, 49152, 16384)) < 0:
        return ['the core did not rumble']
    return []


def check_rumble(res):
    """Combined; a pause stops the headset's rumble, and it comes back
    after."""
    errors = rumble_errors(res, {'left': 0.75, 'right': 0.25})
    paused = res.marks['paused'] * 1e6
    resumed = res.marks['resumed'] * 1e6
    for hand in ('left', 'right'):
        got = haptics(res, hand)
        stop = find(got, 0, lambda e: e['ev'] == 'haptic_stop'
                    and paused < e['t_us'] < resumed)
        if stop < 0:
            errors.append('the %s hand did not stop for the pause' % hand)
        elif any(e['ev'] == 'haptic' and e['t_us'] < resumed
                 for e in got[stop:]):
            errors.append('the %s hand rumbled again while paused' % hand)
        if not any(e['ev'] == 'haptic' and e['t_us'] > resumed for e in got):
            errors.append('the %s hand did not rumble again after the pause'
                          % hand)
    return errors + core_rumbled(res)


def check_rumble_gain(res):
    """Vibration Strength at 50%: the test driver has no gain of its own,
    so the core's strength is scaled once, in software."""
    return (rumble_errors(res, {'left': 0.375, 'right': 0.125})
            + core_rumbled(res))


def check_rumble_separate(res):
    errors = rumble_errors(res, {'left': 0.75, 'right': 0.5})
    left = [e for e in haptics(res, 'left') if e['ev'] == 'haptic']
    right = [e for e in haptics(res, 'right') if e['ev'] == 'haptic']
    if left and right and right[0]['t_us'] < left[0]['t_us']:
        errors.append('the right hand rumbled before player 2 pressed Start')
    return errors


def check_hw_teardown(res):
    """The core's context_destroy waits on the device without the queue
    lock, at a video reinit and at unload: the headset's frames stop
    before it and start again after."""
    waits = [(int(a), int(b)) for a, b in DESTROY.findall(res.log)]
    if len(waits) != 2:
        return ['%d context_destroy waits, want 2 (reinit, unload)'
                % len(waits)]
    errors = []
    for i, (t0, t1) in enumerate(waits):
        inside = [f for f in res.frames if t0 <= f['t_us'] <= t1]
        if inside:
            errors.append('%d headset frames while the core waited on the '
                          'device (%s)' % (len(inside), ('reinit', 'unload')[i]))
        end = waits[i + 1][0] if i + 1 < len(waits) else float('inf')
        if not [f for f in res.frames if t1 < f['t_us'] < end]:
            errors.append('no headset frames after the %s'
                          % ('reinit', 'unload')[i])
    # A staged close keeps the driver presenting the window after the
    # unload: with its XR thread stopped, it draws nothing for the headset.
    shown = True
    for line in res.log.splitlines():
        if '[OpenXR] Session created.' in line:
            shown = True
        elif '[Core] Unloading game...' in line:
            shown = False
        elif '[OpenXR] Slot ' in line and not shown:
            errors.append('a headset image made after the unload, before '
                          'the next session: ' + line.strip())
    return errors


def check_kept_retry(res):
    """A kept context's session that the runtime ended starts again at
    the next video reinit, on the same device."""
    errors = []
    for line in ('[OpenXR] The headset session ended',
                 '[Vulkan] Using cached Vulkan context.'):
        if line not in res.log:
            errors.append('missing "%s"' % line)
    made = res.log.count('[OpenXR] Session created.')
    if made != 2:
        errors.append('%d sessions created, want 2' % made)
    t = res.marks.get('reinit', 0) * 1e6
    after = [f for f in res.frames if f['t_us'] > t + 1e6]
    if len(after) < 40:
        errors.append('%d headset frames after the reinit' % len(after))
    stereo = re.findall(r'\[video_views\] presents=\d stereo=(\d)', res.log)
    if '0' not in stereo or stereo[-1:] != ['1']:
        errors.append('the core saw stereo %s, want it off and on again'
                      % ' '.join(stereo))
    ready = res.log.count('[OpenXR] Headset controllers ready.')
    if ready != 2:
        errors.append('headset controllers ready %d times, want 2 (one '
                      'per session)' % ready)
    if '[OpenXR] Headset controllers unavailable' in res.log:
        errors.append('headset controllers unavailable in a session')
    # RetroArch's log shows the release when the session ends, and the
    # new session's.
    seq = controllers(res)
    if seq != ['live', 'released', 'live']:
        errors.append('controllers went %s, want live, released when the '
                      'session ended, live in the new one' % seq)
    # The laser's image is VIDEO_XR_CURSOR_W by VIDEO_XR_CURSOR_H (its
    # dot, edit mode's outlines, the menu's handle bars and the beam's
    # strips); each start makes one, and each start here made a session.
    dots = [e for e in events(res, 'swapchain')
            if (e['w'], e['h']) == (CURSOR_W, CURSOR_H)]
    if len(dots) != 2:
        errors.append('%d laser dot swapchains, want 2 (one per start)'
                      % len(dots))
    # The new session's suggestions are made again, and accepted.
    suggested = [(e['profile'], e['result']) for e in events(res, 'bindings')]
    if (sorted(suggested) != sorted((p, 0) for p in PROFILES * 2)
            or res.log.count('[OpenXR] Bindings suggested for')
            != 2 * len(PROFILES) or 'Bindings refused' in res.log):
        errors.append('bindings suggested %s, want each profile accepted '
                      'once per session' % suggested)
    return errors


def check_kept_lost(res):
    """A kept context whose runtime lost the instance needs the content
    loaded again: the reinit says so and makes no session."""
    errors = []
    for line in ('[OpenXR] The headset session ended',
                 '[Vulkan] Using cached Vulkan context.',
                 'headset output starts when it is loaded again'):
        if line not in res.log:
            errors.append('missing "%s"' % line)
    made = res.log.count('[OpenXR] Session created.')
    if made != 1:
        errors.append('%d sessions created, want 1' % made)
    t = res.marks.get('reinit', 0) * 1e6
    after = [f for f in res.frames if f['t_us'] > t]
    if after:
        errors.append('%d headset frames after the reinit' % len(after))
    return errors


def kept_leak(text):
    """What a kept context's reinit leaks, headset or not, on master too:
    two unnamed buffers and two unnamed memory objects."""
    m = re.search(r'has 4 leaked objects that have not been destroyed\.\n'
                  r'(.*)', text)
    objects = m.group(1).rstrip('. ').split(', ') if m else []
    kinds = sorted(re.sub(r' 0x[0-9a-f]+$', '', o) for o in objects)
    return kinds == ['VkBuffer', 'VkBuffer', 'VkDeviceMemory', 'VkDeviceMemory']


# ---- Headset pacing ----
# The test core's frame log (video_views_test_fps), on CLOCK_MONOTONIC
# like the layer's t_us and the marks (time.monotonic()).
CORE_FRAME = re.compile(r'\[video_views\] frame (\d+) at (\d+) us')
FPS10 = {'video_views_test_fps': '10'}
FPS16 = {'video_views_test_fps': '16'}
# The window's own interval: without the headset the core keeps the
# window's 60 Hz.
WINDOW1 = {'video_swap_interval': '1'}
PACE_STEPS = [('wait', 8), ('mark', 'from'), ('wait', 5), ('mark', 'to')]


def marks_missing(res, *names):
    return [n for n in names if n not in res.marks]


def window(res, a, b, lead=0.0):
    """From lead seconds after mark a to mark b, in us."""
    return (res.marks[a] + lead) * 1e6, res.marks[b] * 1e6


def core_fps(res, t0, t1):
    """The core's frames a second between t0 and t1, from its log."""
    pts = [(int(n), int(t)) for n, t in CORE_FRAME.findall(res.log)
           if t0 <= int(t) <= t1]
    if len(pts) < 2 or pts[-1][1] <= pts[0][1]:
        return 0.0
    return (pts[-1][0] - pts[0][0]) * 1e6 / (pts[-1][1] - pts[0][1])


def headset_hz(res, t0, t1):
    """The headset's frames a second between t0 and t1, and the period
    it reported, in ms."""
    fr = [f for f in res.frames if t0 <= f['t_us'] <= t1]
    if len(fr) < 2 or fr[-1]['t_us'] <= fr[0]['t_us']:
        return 0.0, 0.0
    periods = sorted(f['period_ns'] for f in fr)
    return ((len(fr) - 1) * 1e6 / (fr[-1]['t_us'] - fr[0]['t_us']),
            periods[len(periods) // 2] / 1e6)


# The window presenting without waiting under vsync: Vulkan's swapchain
# going non-blocking (vk_x), or a GL driver's swap interval of 0.
UNWAITED = ('VK_DATA_FLAG_EMULATE_MAILBOX requires non-zero swap_interval',
            '[GLCore] Headset pacing on: the window presents without '
            'waiting.',
            '[GL] Headset pacing on: the window presents without waiting.')
TICKS_LINE = "[OpenXR] Pacing on the headset's frames."
CLOCK_LINE = ('[OpenXR] Pacing on the clock while the headset does not '
              'show the session.')


def pace_errors(res, t0, t1, n):
    """Errors unless, between t0 and t1, the core's image was released
    every n headset frames: each gap between releases within half a
    headset frame of n periods, and n headset frames between most."""
    frames = [f for f in res.frames if t0 <= f['t_us'] <= t1]
    periods = sorted(f['period_ns'] for f in frames if f['period_ns'] > 0)
    shown = [q for f in frames for q in quads(f) if not q['flags'] & BLEND]
    if len(frames) < 10 or not periods or not shown:
        return ['%d headset frames with the core\'s quad between %.1f and '
                '%.1f s' % (len(frames), t0 / 1e6, t1 / 1e6)]
    sc = shown[-1]['sc']
    period = periods[len(periods) // 2] / 1e3
    rel = [r['t_us'] for r in res.releases
           if r['sc'] == sc and t0 <= r['t_us'] <= t1]
    if len(rel) < 5:
        return ['%d images released on sc%d, want a steady stream'
                % (len(rel), sc)]
    gaps = [b - a for a, b in zip(rel, rel[1:])]
    off = [round(g / 1e3, 1) for g in gaps
           if abs(g - n * period) > period / 2]
    ts = [f['t_us'] for f in frames]
    counts = sorted(sum(1 for t in ts if a < t <= b)
                    for a, b in zip(rel, rel[1:]))
    errors = []
    if off:
        errors.append('%d of %d gaps between releases are not %d headset '
                      'frames of %.1f ms: %s'
                      % (len(off), len(gaps), n, period / 1e3, off[:6]))
    if counts[len(counts) // 2] != n:
        errors.append('%d headset frames a core frame, want %d'
                      % (counts[len(counts) // 2], n))
    return errors


def check_rate_change(res):
    """Monado's 20 Hz, then a 10 Hz headset (the layer's divide 2): the
    XR thread measures each, and the pace follows without a restart."""
    if marks_missing(res, 'from', 'change', 'to'):
        return ['the run did not reach its marks']
    errors = []
    for a, b, lead, hz, n in (('from', 'change', 0.0, 20.0, 2),
                              ('change', 'to', 3.0, 10.0, 1)):
        t0, t1 = window(res, a, b, lead)
        errors += ['at %.0f Hz: %s' % (hz, e)
                   for e in pace_errors(res, t0, t1, n)]
        if paced_line(hz, n) not in res.log:
            errors.append('no "%s" in the log' % paced_line(hz, n))
        rate, period = headset_hz(res, t0, t1)
        if abs(rate - hz) > 0.1 * hz or abs(period - 1e3 / hz) > 0.5:
            errors.append('the headset ran at %.1f Hz, period %.1f ms; '
                          'want %.0f Hz' % (rate, period, hz))
        line = '[OpenXR] The headset runs at %.2f Hz.' % hz
        count = res.log.count(line)
        if count != 1:
            errors.append('"%s" logged %d times, want once' % (line, count))
    if 'Game = 10.00 Hz' not in res.log:
        errors.append('the core did not report 10 fps')
    if not CORE_FRAME.search(res.log):
        errors.append('no frame lines from the core')
    for fn in ('xrWaitFrame', 'xrBeginFrame', 'xrEndFrame'):
        if '[OpenXR] %s failed' % fn in res.log:
            errors.append('%s failed' % fn)
    return errors


# driver_adjust_system_rates() while the headset paces the core.
PACED = re.compile(r'\[Video\] The headset paces the core: ([\d.]+) Hz / '
                   r'(\d+)\.')
LOSS_LINE = '[OpenXR] Session loss pending.'
LATE_LINE = '[OpenXR] No headset frame for two intervals'


def paced_line(hz, n):
    return '[Video] The headset paces the core: %.2f Hz / %d.' % (hz, n)


def check_paced(n, hz, end=None):
    """The headset at hz paces the core: its image every n headset
    frames at hz / n fps, and the window no longer waits for vsync.
    end: a log line past which the clock line is not held against it."""
    def check(res):
        if marks_missing(res, 'from', 'to'):
            return ['the run did not reach its marks']
        t0, t1 = window(res, 'from', 'to')
        errors = pace_errors(res, t0, t1, n)
        if paced_line(hz, n) not in res.log:
            errors.append('no "%s" in the log' % paced_line(hz, n))
        if not any(line in res.log for line in UNWAITED):
            errors.append('the window never presented without waiting')
        log = res.log.split(end)[0] if end else res.log
        if CLOCK_LINE in log:
            errors.append('the core paced on the clock, not the headset')
        fps = core_fps(res, t0, t1)
        if abs(fps - hz / n) > 0.1 * hz / n:
            errors.append('the core ran at %.1f fps, want %.1f'
                          % (fps, hz / n))
        return errors
    return check


def check_hidden(res):
    """The session not visible: the core keeps its rate on the clock and
    nothing hangs; visible again, the headset paces it again."""
    if marks_missing(res, 'hidden', 'shown', 'to'):
        return ['the run did not reach its marks']
    errors = []
    t0, t1 = window(res, 'hidden', 'shown', 1.0)
    fps = core_fps(res, t0, t1)
    if abs(fps - 10.0) > 1.5:
        errors.append('the core ran at %.1f fps while the headset did not '
                      'show it, want 10' % fps)
    t0, t1 = window(res, 'shown', 'to', 2.0)
    errors += pace_errors(res, t0, t1, 2)
    for line in (CLOCK_LINE, TICKS_LINE):
        if line not in res.log:
            errors.append('no "%s" in the log' % line)
    if res.log.rfind(TICKS_LINE) < res.log.rfind(CLOCK_LINE):
        errors.append('the headset did not pace the core again')
    return errors


def check_fastforward(res):
    """Fast-forward waits for nothing: the core outruns the headset.
    After it, the headset paces the core again."""
    if marks_missing(res, 'ff', 'ffend', 'to'):
        return ['the run did not reach its marks']
    errors = []
    t0, t1 = window(res, 'ff', 'ffend', 0.5)
    fps = core_fps(res, t0, t1)
    if fps < 40.0:
        errors.append('the core ran at %.1f fps in fast-forward, want well '
                      'past the headset\'s 20 Hz' % fps)
    t0, t1 = window(res, 'ffend', 'to', 2.0)
    errors += pace_errors(res, t0, t1, 2)
    return errors


def check_window_back(res):
    """Paced, then the session lost: the window's interval returns and
    the core keeps about the window's rate, not unthrottled."""
    errors = check_paced(2, 20.0, LOSS_LINE)(res)
    if LATE_LINE in res.log:
        errors.append('the core waited out the headset that was gone')
    if marks_missing(res, 'lost', 'end'):
        return ['the run did not reach its marks']
    t0, t1 = window(res, 'lost', 'end', 1.0)
    fps = core_fps(res, t0, t1)
    if not 30.0 <= fps <= 90.0:
        errors.append('the core ran at %.1f fps once the headset was gone, '
                      'want the window\'s pace (about 60)' % fps)
    return errors


def check_loss_waiting(res):
    """The session lost while the core waits on the headset: the wait
    ends with the loss, not after two intervals."""
    if marks_missing(res, 'lost', 'end'):
        return ['the run did not reach its marks']
    errors = []
    if LOSS_LINE not in res.log:
        errors.append('no "%s" in the log' % LOSS_LINE)
    if paced_line(20.0, 2) not in res.log.split(LOSS_LINE)[0]:
        errors.append('the headset did not pace the core before the loss')
    if LATE_LINE in res.log:
        errors.append('the core waited out the headset that was gone')
    return errors


def check_hidden_slow(res):
    """A headset that slows while it does not show the session: its
    periods are not measured then, so the rate and the pace stay."""
    if marks_missing(res, 'hidden', 'shown', 'to'):
        return ['the run did not reach its marks']
    errors = []
    line = '[OpenXR] The headset runs at'
    n = res.log.count(line)
    if n != 1:
        errors.append('"%s" logged %d times, want once' % (line, n))
    for hz, k in PACED.findall(res.log):
        if (float(hz), int(k)) != (20.0, 2):
            errors.append('the core was paced at %s Hz / %s' % (hz, k))
    t0, t1 = window(res, 'hidden', 'shown', 1.0)
    fps = core_fps(res, t0, t1)
    if abs(fps - 10.0) > 1.5:
        errors.append('the core ran at %.1f fps while the headset did not '
                      'show it, want 10' % fps)
    t0, t1 = window(res, 'shown', 'to', 2.0)
    errors += pace_errors(res, t0, t1, 2)
    return errors


def check_stopping(res):
    """The session stopping while the core waits on the headset: the wait
    ends with it, not after two intervals, and the core keeps its rate."""
    if marks_missing(res, 'from', 'stop', 'end'):
        return ['the run did not reach its marks']
    errors = []
    if '[OpenXR] Session stopping.' not in res.log:
        errors.append('the session did not stop')
    if paced_line(20.0, 2) not in res.log:
        errors.append('the headset did not pace the core')
    if LATE_LINE in res.log:
        errors.append('the core waited out the headset that stopped')
    t0, t1 = window(res, 'stop', 'end', 1.0)
    fps = core_fps(res, t0, t1)
    if abs(fps - 10.0) > 1.5:
        errors.append('the core ran at %.1f fps once the session stopped, '
                      'want 10' % fps)
    return errors


def check_unpaced(res):
    """No headset pacing: the core keeps the window's 60 Hz as before."""
    if marks_missing(res, 'from', 'to'):
        return ['the run did not reach its marks']
    errors = []
    if PACED.search(res.log):
        errors.append('the headset paced the core')
    t0, t1 = window(res, 'from', 'to')
    fps = core_fps(res, t0, t1)
    if fps < 30.0:
        errors.append('the core ran at %.1f fps, want the window\'s pace '
                      '(about 60)' % fps)
    return errors


def check_request(hz, n, rate):
    """What the headset was asked for (None: nothing), and the pace at
    the rate it then runs at."""
    def check(res):
        if marks_missing(res, 'from', 'to'):
            return ['the run did not reach its marks']
        got = [round(e['hz'], 2) for e in events(res, 'refresh_request')]
        want = [] if hz is None else [hz]
        errors = []
        if got != want:
            errors.append('asked the headset for %s Hz, want %s'
                          % (got, want))
        t0, t1 = window(res, 'from', 'to')
        errors += pace_errors(res, t0, t1, n)
        if paced_line(rate, n) not in res.log:
            errors.append('no "%s" in the log' % paced_line(rate, n))
        return errors
    return check


MISFIT = ("[OpenXR] The headset runs at %d Hz, which doesn't fit this "
          "game's %.2f fps")


def check_misfit(res):
    """16 fps on the 20 Hz headset: no pacing, the notice once however
    often the video driver restarts, and the core at the window's
    pace."""
    errors = check_unpaced(res)
    n = res.log.count(MISFIT % (20, 16.0))
    if n != 1:
        errors.append('the misfit notice was logged %d times, want once' % n)
    if res.log.count('[OpenXR] Session created.') < 2:
        errors.append('the video driver did not restart')
    return errors


SETTLE = [('wait', 8)]
VULKAN = {'video_views_test_hw': 'vulkan'}
TEARDOWN = [('wait', 6), ('send', 'FULLSCREEN_TOGGLE'), ('wait', 4),
            ('send', 'CLOSE_CONTENT'), ('wait', 4)]
KEPT_LEAK = [('VUID-vkDestroyDevice-device-05137', kept_leak)]
SIZED = {'video_shader_enable': 'true'}
CHECKER = {'video_views_test_pattern': 'checker'}
SHOT = [('wait', 8), ('shot', None)]
# SteamVR on the Steam Frame shows array layer 0 of every quad.
INDEX0 = {'RA_XR_LAYER_QUAD_INDEX0': '1'}
# Master's threaded HW ring raises these for a core's own images.
THREADED_HW = [
    ('VUID-vkQueueSubmit-fence-00063', 'submitted in SIGNALED state'),
    ('VUID-vkQueueSubmit-pSignalSemaphores-00067',
     'pSubmits[0].pSignalSemaphores[0]')]

def check_custom(kind, legacy=False):
    def check(res):
        if kind == 'tracked':
            return check_tracked(beside=not legacy)(res)
        fr = last_snap(res)
        if not fr:
            return no_quads(res)
        qs = [q for q in quads(fr) if not q['flags'] & BLEND]
        if kind == 'menu':
            if legacy:
                errors = window_quad(res, fr)
                if not errors and near(colour(image(res, fr, quads(fr)[0]), .25, .25)[0], RED):
                    errors.append('combined fallback is missing its menu')
                return errors
            ms = menus(fr)
            return ([] if not qs and len(ms) == 1 and
                    at(ms[0], (0, 0, -1.7)) else
                    ['hidden screens must leave the centered menu available'])
        if kind == 'hidden':
            return ([] if len(qs) == 1 and qs[0]['eye'] == 'both' else
                    ['hidden stereo screen still submitted'])
        eyes = {q['eye']: q for q in qs}
        if not all(e in eyes for e in ('left', 'right')):
            return ['custom stereo pair missing']
        errors = []
        if kind == 'offset':
            presses = [f for f in kinds(core_events(res), 'pointer') if f['pressed']]
            if not presses or not close((presses[0]['cx'], presses[0]['cy']), (320, 420), 3):
                errors.append('custom screen did not map the touch to 320,420')
        position = (0.25, 0.1, -1.5)
        rotation = (0, 0, 0, 1)
        size = (1.6, 0.96)
        if kind == 'rotation':
            rotation = (0, math.sin(math.pi / 8), 0, math.cos(math.pi / 8))
            size = (2.4, 1.44)
        if kind == 'repeat':
            position = (0.75, 0.1, -1.5)
            size = (6.4, 3.84)
        if kind == 'recenter':
            position = (-1.0, 0.3, 0.05)
            rotation = (0, math.sin(math.pi / 4), 0, math.cos(math.pi / 4))
        for eye in ('left', 'right'):
            q = eyes[eye]
            if not at(q, position) or not facing(q, rotation) or not sized(q, size):
                errors.append('%s custom pose/size: %s %s' %
                              (eye, q['pose'], q['size']))
        if kind == 'repeat':
            held = {sc for sc, c in res.chains.items() if (c['w'], c['h']) == (400, 240)}
            if not held or any(r['sc'] in held and
                               res.marks['busy'] * 1e6 < r['t_us'] < res.marks['free'] * 1e6
                               for r in res.releases):
                errors.append('screen images were not held during the repeat')
            if not legacy:
                for eye in ('left', 'right'):
                    errors += image_size(res, eyes[eye], (400, 240),
                                         distance=math.sqrt(sum(v * v for v in position)))
            resized = [f for f in res.frames if f['snap'] and
                       res.marks['resized'] * 1e6 < f['t_us'] <
                       res.marks['restored'] * 1e6 and len(quads(f)) == 3]
            if not resized:
                errors.append('three resized layers did not become available')
            elif not legacy:
                for q in quads(resized[-1]):
                    native = (320, 240) if q['eye'] == 'both' else (400, 240)
                    errors += image_size(res, q, native,
                                         distance=math.sqrt(sum(v * v for v in q['pose'][:3])))
            hidden = [f for f in res.frames if
                      (res.marks['hidden'] + .3) * 1e6 < f['t_us'] <
                      res.marks['shown'] * 1e6]
            first_hidden = next((i for i, f in enumerate(hidden)
                                 if quads(f) and all(q['eye'] == 'both' for q in quads(f))), None)
            if first_hidden is None or any(q['eye'] != 'both' for f in hidden[first_hidden:] for q in quads(f)):
                errors.append('paused screen did not stay hidden')
            before = [f for f in res.frames if f['snap'] and
                      res.marks['paused'] * 1e6 < f['t_us'] <
                      res.marks['moved'] * 1e6 and quads(f)]
            if not before:
                errors.append('no paused image before moving')
            else:
                old = next(q for q in quads(before[-1]) if q['eye'] == 'left')
                feedback_before = colour(image(res, before[-1], old), .5, .5)
                feedback_after = colour(image(res, fr, eyes['left']), .5, .5)
                if feedback_after != feedback_before:
                    errors.append('paused feedback image advanced')
                if not legacy:
                    expected = round((res.screen_frames[4] % 251) * 255 / 250)
                    if abs(feedback_before[0][1] - expected) > 3:
                        errors.append('re-shown frame tag %d, want %d from latest source'
                                      % (feedback_before[0][1], expected))
                if at(old, eyes['left']['pose'][:3]):
                    errors.append('paused quad did not move')
        return errors
    return check


def check_menu_spot(res):
    """Menu in Front of Screen 1 off: with screen 1 moved, the menu
    opens at its own spot straight ahead, as before screens moved."""
    fr = last_snap(res)
    if not fr:
        return no_quads(res)
    ms = menus(fr)
    if len(ms) != 1 or not at(ms[0], (0, 0, -1.7)):
        return ['menu at %s, want its spot' %
                ([m['pose'][:3] for m in ms],)]
    return []


def check_frame_custom(window=False):
    """Without a map the whole frame, or gl's window, is screen 1: moved
    by its offsets and sized for where it is."""
    def check(res):
        fr = last_snap(res)
        if not fr:
            return no_quads(res)
        q = [x for x in quads(fr) if not x['flags'] & BLEND]
        if len(q) != 1 or q[0]['eye'] != 'both':
            return ['want one quad for both eyes, got %s' % [x['eye'] for x in q]]
        errors = []
        position = (0.25, 0.1, -1.5)
        if not at(q[0], position) or not sized(q[0], (1.6, 0.96)):
            errors.append('frame at %s size %s' % (q[0]['pose'][:3], q[0]['size']))
        return errors + image_size(res, q[0], (W, H) if window else None,
                                   distance=math.sqrt(sum(v * v for v in position)))
    return check


CUSTOM_OFFSET = {'video_screen1_xr_x': '0.25',
                 'video_screen1_xr_y': '0.1', 'video_screen1_xr_z': '0.3',
                 'video_screen2_xr_x': '-0.2', 'video_screen2_xr_y': '0.1',
                 'video_screen2_xr_z': '0.2'}
CUSTOM_AIM = 'aim right 0.2 -0.4 -0.3 -0.52 -1.132 -1.6'
CUSTOM_CASES = [
    {'name': 'custom-offset', 'settings': CUSTOM_OFFSET,
     'steps': [('wait', 6), ('script', CUSTOM_AIM), ('wait', 1),
               ('script', script(CUSTOM_AIM, R2)), ('wait', 1),
               ('script', ''), ('wait', 2)], 'check': check_custom('offset')},
    {'name': 'custom-rotation',
     'settings': dict(CUSTOM_OFFSET, video_screen1_xr_yaw='45',
                      video_screen1_xr_scale='1.5'),
     'steps': SETTLE, 'check': check_custom('rotation')},
    {'name': 'custom-hidden', 'settings': {'video_screen1_xr_visible': 'false'},
     'steps': SETTLE, 'check': check_custom('hidden')},
    {'name': 'custom-recenter', 'settings': CUSTOM_OFFSET,
     'steps': [('wait', 6), ('script', 'head 0.5 0.2 0.3 90'), ('wait', 1),
               ('send', 'HEADSET_RECENTER'), ('wait', 3)],
     'check': check_custom('recenter')},
    {'name': 'custom-menu',
     'settings': {'video_screen1_xr_visible': 'false',
                  'video_screen2_xr_visible': 'false', 'menu_driver': 'ozone'},
     'steps': [('wait', 6), ('send', 'MENU_TOGGLE'), ('wait', 4)],
     'check': check_custom('menu')},
    {'name': 'custom-menu-spot',
     'settings': dict(CUSTOM_OFFSET, video_openxr_menu_follow='false',
                      menu_driver='ozone'),
     'steps': [('wait', 6), ('send', 'MENU_TOGGLE'), ('wait', 4)],
     'check': check_menu_spot},
    {'name': 'custom-tracked',
     'settings': {'video_screen1_xr_x': '10', 'video_screen1_xr_yaw': '180',
                  'video_screen1_xr_scale': '10', 'video_screen1_xr_visible': 'false'},
     'options': {'video_views_test_vr': 'local'},
     'steps': SETTLE, 'check': check_custom('tracked')},
    {'name': 'custom-repeat', 'settings': CUSTOM_OFFSET,
     'script': 'eyes 400 240',
     'args': ['--set-shader=' + os.path.join(HERE, 'feedback.slangp')],
     'steps': [('wait', 6), ('screen', (1, 8, 0.25, 4.0, 1)),
               ('mark', 'resized'), ('wait', 2), ('mark', 'restored'),
               ('screen', (2, 8, 0.25, 1.0, 1)), ('wait', 2),
               ('mark', 'running_hidden'),
               ('script', 'state 3\neyes 400 240\nbusy eyes'),
               ('screen', (3, 0, 0.25, 1.0, 0)), ('wait', 2),
               ('send', 'PAUSE_TOGGLE'), ('wait', 1),
               ('script', 'state 5'), ('screen', (4, 0, 0.25, 1.0, 1)),
               ('mark', 'busy'), ('wait', 0.5), ('mark', 'free'),
               ('script', 'state 5\nbusy off'), ('wait', 2),
               ('mark', 'paused'), ('wait', 2),
               ('mark', 'moved'), ('screen', (5, 0, 0.75, 1.0, 1)), ('wait', 2),
               ('screen', (6, 0, 0.75, 1.0, 0)), ('mark', 'hidden'), ('wait', 2),
               ('mark', 'shown'), ('screen', (7, 0, 0.75, 4.0, 1)), ('wait', 3)],
     'check': check_custom('repeat')},
    {'name': 'no-map-custom', 'map': 'none', 'settings': CUSTOM_OFFSET,
     'steps': SETTLE, 'check': check_frame_custom()},
    # A views core's frame shown whole is as laid out, not screen 1.
    {'name': 'crop-threaded-custom', 'map': 'crop',
     'settings': dict(CUSTOM_OFFSET, video_threaded='true'), 'steps': SETTLE,
     'check': threaded(check_crop)},
]


# ---- Edit Screens ----
# The 3DS quads as in the laser cases. While editing, the menu's quad is
# up 10 cm in front of screen 1 and as large: it is taken only by its
# handle bar, centred under it, so aims through its middle reach the
# screen behind. A right hand at x,y,0 aiming straight ahead moves what
# it holds by its own moves.
EDIT = 'HEADSET_EDIT_SCREENS'
EDIT_ON = '[OpenXR] Edit Screens on.'
EDIT_OFF = '[OpenXR] Edit Screens off.'
EDIT_GRIP = 'action edit/grab@right 1'
EDIT_GRIP_LEFT = 'action edit/grab@left 1'
EDIT_TILT = 'action edit/tilt@right 1'
EDIT_RESET = 'action edit/reset@right 1'
EDIT_NEXT = 'action edit/select_next 1'
EDIT_SETS = ['edit', 'pointer']
GAME_SETS = ['combined', 'pointer']
SAVE = {'config_save_on_exit': 'true'}
ID = (0.0, 0.0, 0.0, 1.0)
ROLL30 = (0.0, 0.0, math.sin(math.pi / 12.0), math.cos(math.pi / 12.0))
S0 = (0.0, 0.0, -1.8)
S1 = (0.0, -0.992, -1.8)
MENU_SPOT = (0.0, 0.0, -1.7)
MENU_W, MENU_H = 1.6, 0.96
# The laser's image, laid out as gfx/video_xr.h lays it out: its dot,
# edit mode's faint and bright outlines, the menu's faint and bright
# handle bars, the beam's strips to the dot and into nothing.
CURSOR_DIM = 32
OUTLINE_DIM = 256
OUTLINE_BAND = 2
OUTLINE_EDGE = 3 * OUTLINE_BAND
HANDLE_COLS, HANDLE_ROWS = 128, 8
# VIDEO_XR_OUTLINE_GAP, and the rim's fraction of an outline with the
# half texel filtering adds.
OUTLINE_GAP = 0.005
OUTLINE_F = (OUTLINE_EDGE + 0.5) / OUTLINE_DIM
BEAM_LEN, BEAM_ROWS = 128, 4
OUTLINE_X = tuple(CURSOR_DIM + 1 + k * (OUTLINE_DIM + 1) for k in (0, 1))
HANDLE_X = tuple(OUTLINE_X[0] + 2 * (OUTLINE_DIM + 1) + k * (HANDLE_COLS + 1)
                 for k in (0, 1))
BEAM_X = tuple(HANDLE_X[0] + 2 * (HANDLE_COLS + 1) + k * (BEAM_LEN + 1)
               for k in (0, 1))
# A back's solid cell after the beam strips; a back shows the square
# BACK_MARGIN inside it, centred on the middle row.
BACK_DIM, BACK_MARGIN, BACK_LEVEL = 8, 4, 40
BACK_CELL = BACK_DIM + 2 * BACK_MARGIN
BACK_X = BEAM_X[1] + BEAM_LEN + 1
BACK_Y = (OUTLINE_DIM - BACK_CELL) // 2
BACK_RECT = (BACK_X + BACK_MARGIN, BACK_Y + BACK_MARGIN, BACK_DIM, BACK_DIM)
# A hinge's green strip after the back's cell, centred on the middle
# row.
HINGE_COLS, HINGE_ROWS = 128, 4
HINGE_X = BACK_X + BACK_CELL + 1
HINGE_Y = (OUTLINE_DIM - HINGE_ROWS) // 2
# A hidden hinge slot's rows, empty either way up.
HINGE_HIDDEN_Y = 0
# The hinge's line, VIDEO_XR_HINGE_WIDTH wide, each end
# VIDEO_XR_HINGE_LIFT nearer the head; its strip's green,
# VIDEO_XR_HINGE_RB/_G at VIDEO_XR_HINGE_LEVEL, premultiplied. An
# outline hides an edge's rim VIDEO_XR_OUTLINE_CROP texels deep.
HINGE_WIDTH, HINGE_LIFT = 0.006, 0.01
HINGE_LEVEL, HINGE_RB, HINGE_G = 166, 60, 220
OUTLINE_CROP = 8
# Edit mode's tags, one cell for each item it can choose (0, the laser;
# a slot plus one), after the hinge's strip, each centred on the middle
# row.
TAG_COLS, TAG_ROWS, TAGS = 128, 30, 10
TAG_X = tuple(HINGE_X + HINGE_COLS + 1 + k * (TAG_COLS + 1)
              for k in range(TAGS))
TAG_Y = (OUTLINE_DIM - TAG_ROWS) // 2
# Edit mode's cut outlines after the tags, faint ones then bright, one
# for each cut: the outline's square without those edges' rims.
CUTS = ('top', 'bottom', 'right', 'left', 'top-bottom', 'right-left')
CUT_X = tuple(TAG_X[-1] + TAG_COLS + 1 + k * (OUTLINE_DIM + 1)
              for k in range(2 * len(CUTS)))
CURSOR_W = CUT_X[-1] + OUTLINE_DIM
# Its width without the cut outlines, then without the tags too, for
# runtimes too narrow for them.
CURSOR_TAGGED_W = TAG_X[-1] + TAG_COLS
CURSOR_PLAIN_W = HINGE_X + HINGE_COLS
TAG_HEIGHT, TAG_LIFT, TAG_FAINT = 0.02, 0.05, 160
CURSOR_H = OUTLINE_DIM
# The beam: VIDEO_XR_BEAM_WIDTH, _REACH and _LEVEL; AIM_GUN's and
# AIM_MISS's rays; the head where the beams' cases put it.
BEAM_WIDTH = 0.004
BEAM_REACH = 1.5
BEAM_LEVEL = 160
GUN_FROM = (0.2, -0.4, -0.3)
GUN_AT = (-0.4, 0.24, -1.8)
MISS_AT = (3.0, 0.0, -1.8)
HEAD_AT_ORIGIN = 'head 0 0 0 0'
EDIT_POS = 0.012


def qrot(q, v):
    return qmul(qmul(q, tuple(v) + (0.0,)), (-q[0], -q[1], -q[2], q[3]))[:3]


def compose(a, b):
    """Pose b, in a's frame, in a's space; a pose is (position, quat)."""
    return (tuple(x + y for x, y in zip(a[0], qrot(a[1], b[0]))),
            qmul(a[1], b[1]))


def inverse(a):
    q = (-a[1][0], -a[1][1], -a[1][2], a[1][3])
    return (tuple(-v for v in qrot(q, a[0])), q)


def euler(q):
    """Yaw, pitch and roll in degrees, as video_xr_euler has them."""
    x, y, z, w = q
    sp = max(-1.0, min(1.0, -2.0 * (y * z - w * x)))
    if abs(sp) > 0.99999:
        return (math.degrees(math.atan2(-2.0 * (x * z - w * y),
                                        1.0 - 2.0 * (y * y + z * z))),
                math.degrees(math.asin(sp)), 0.0)
    return (math.degrees(math.atan2(2.0 * (x * z + w * y),
                                    1.0 - 2.0 * (x * x + y * y))),
            math.degrees(math.asin(sp)),
            math.degrees(math.atan2(2.0 * (x * y + w * z),
                                    1.0 - 2.0 * (x * x + z * z))))


def from_euler(yaw, pitch, roll):
    """The turn yaw, pitch and roll settings give an item."""
    cy, sy = math.cos(math.radians(yaw) / 2), math.sin(math.radians(yaw) / 2)
    cp, sp = (math.cos(math.radians(pitch) / 2),
              math.sin(math.radians(pitch) / 2))
    cr, sr = (math.cos(math.radians(roll) / 2),
              math.sin(math.radians(roll) / 2))
    return (cy * sp * cr + sy * cp * sr, sy * cp * cr - cy * sp * sr,
            cy * cp * sr - sy * sp * cr, cy * cp * cr + sy * sp * sr)


def aim_pose(p, t, roll=0.0):
    """The test layer's aim at p towards t."""
    f = [b - a for a, b in zip(p, t)]
    n = math.sqrt(sum(v * v for v in f))
    return (tuple(p), from_euler(math.degrees(math.atan2(-f[0], -f[2])),
                                 math.degrees(math.asin(f[1] / n)), roll))


def held(item, aim0, aim1, tilt):
    """Where a hand that took item at aim0 puts it at aim1: turned as
    the aim's heading turned, its pitch and roll kept, unless the
    trigger tilts it. Neither aim points near straight up or down."""
    pose = compose(aim1, compose(inverse(aim0), item))
    if tilt:
        return pose
    turn = euler(aim1[1])[0] - euler(aim0[1])[0]
    return (pose[0], qmul(from_euler(turn, 0.0, 0.0), item[1]))


def aim_line(p, t, hand='right', roll=None):
    line = 'aim %s %g %g %g %g %g %g' % ((hand,) + tuple(p) + tuple(t))
    return line if roll is None else '%s %g' % (line, roll)


def edit_aim(x=0.0, y=0.0, roll=None, z=-1.8):
    """The right hand at x,y,0 aiming straight ahead, rolled."""
    return aim_line((x, y, 0.0), (x, y, z), roll=roll)


def edit_aim_turned(x=0.0, y=0.0, pitch=0.0, yaw=0.0, z=-1.8):
    """edit_aim()'s hand, its aim turned pitch degrees up and yaw degrees
    left, as the unit test's t_turn_aim turns them."""
    return aim_line((x, y, 0.0),
                    (x - 1.8 * math.tan(math.radians(yaw)),
                     y + 1.8 * math.tan(math.radians(pitch)), z))


def edit_shake(x, y, deg, secs):
    """Steps gripping with edit_aim_turned()'s hand at x, y for secs
    seconds, its aim turned every 0.02 s as the unit tests' t_shake
    turns it: deg degrees in pitch and yaw at 8 Hz and a fifth of that
    at 2.3 Hz, the yaw a quarter period behind."""
    steps = []
    for k in range(int(round(secs / 0.02))):
        t = k * 0.02
        a = 2.0 * math.pi * 8.0 * t
        b = 2.0 * math.pi * 2.3 * t + 1.0
        p = deg * (math.sin(a) + 0.2 * math.sin(b))
        w = deg * (math.sin(a - math.pi / 2.0)
                   + 0.2 * math.sin(b - math.pi / 2.0))
        steps += [('script', script(edit_aim_turned(x, y, pitch=p, yaw=w),
                                    EDIT_GRIP)),
                  ('wait', 0.02)]
    return steps


def outline_size(w, h):
    """An outline's quad round an item w by h."""
    return tuple((s + 2.0 * OUTLINE_GAP) / (1.0 - 2.0 * OUTLINE_F)
                 for s in (w, h))


def handle_y(menu_y, h):
    """The height of the handle under a menu at menu_y, h high: 5 mm
    below its outline."""
    return menu_y - (outline_size(0.0, h)[1] + 0.03) / 2.0 - 0.005


def edit_screen0(frame):
    """Screen 1's two eyes."""
    return [q for q in quads(frame) if not q['flags'] & BLEND
            and q['eye'] in ('left', 'right')]


def screen2(frame):
    q = [x for x in quads(frame) if x['eye'] == 'both' and not x['flags']]
    return q[0] if q else None


def frame_quad(frame):
    """A whole frame: the one quad that is not blended."""
    qs = [q for q in quads(frame) if not q['flags'] & BLEND]
    return qs[0] if len(qs) == 1 else None


def menu_quad(frame):
    ms = menus(frame)
    return ms[0] if ms else None


def outlines(frame, lit=None):
    """Edit mode's outlines, whole, a cut's cell or cut to hide an edge's
    rim; lit: only the bright (1) or faint (0)."""
    lits = (0, 1) if lit is None else (lit,)
    xs = [x for k in lits for x in (OUTLINE_X[k], OUTLINE_X[k] + OUTLINE_CROP)
          + CUT_X[k * len(CUTS):(k + 1) * len(CUTS)]]
    return [q for q in quads(frame)
            if q['flags'] & BLEND and q['rect'][3] == OUTLINE_DIM
            and q['rect'][0] in xs]


def outline_cut(o):
    """Whether outline o hides a rim: a cut's cell, or cut."""
    return o['rect'][0] not in OUTLINE_X or o['rect'][2] != OUTLINE_DIM


def outline_pose_errors(frame):
    """Every outline in frame its item's whole outline, cut or not: the
    item's middle, lifted toward the head, its axes and the size round
    it. A cut changes the outline's image alone."""
    items = [q for q in quads(frame) if not q['flags'] & BLEND] \
        + menus(frame)
    errors = []
    for o in outlines(frame):
        if not items:
            return ['frame %d: no items under the outlines' % frame['n']]
        q = min(items, key=lambda i: sum(
            (a - b) ** 2 for a, b in zip(i['pose'][:3], o['pose'][:3])))
        axes = [abs(sum(a * b for a, b in zip(
            qrot(tuple(o['pose'][3:]), v), qrot(tuple(q['pose'][3:]), v))))
                for v in ((1.0, 0.0, 0.0), (0.0, 0.0, 1.0))]
        size = outline_size(*q['size'])
        off = math.sqrt(sum((a - b) ** 2
                            for a, b in zip(o['pose'][:3], q['pose'][:3])))
        if (min(axes) < 0.9999 or off > 0.003
                or any(abs(a - b) > 1e-4 for a, b in zip(o['size'], size))):
            errors.append('frame %d: the outline %s at %s, %s, axes %s, not '
                          'its item\'s whole outline at %s, %s' % (
                              frame['n'], o['rect'],
                              [round(v, 4) for v in o['pose'][:3]],
                              [round(v, 4) for v in o['size']],
                              [round(v, 5) for v in axes],
                              [round(v, 4) for v in q['pose'][:3]],
                              [round(v, 4) for v in size]))
    return errors


def hinge_lines(frame):
    """Edit mode's green hinge lines, the slots shown."""
    return [q for q in hinge_slots(frame) if q['rect'][1] == HINGE_Y]


def hinge_slots(frame):
    """Edit mode's hinge slots, shown or hidden."""
    return [q for q in quads(frame)
            if q['flags'] & BLEND and q['rect'][0] == HINGE_X + 1]


def handles(frame, lit=None):
    """The menu's handle bar; lit as for outlines()."""
    xs = HANDLE_X if lit is None else (HANDLE_X[lit],)
    return [q for q in quads(frame)
            if q['flags'] & BLEND and q['rect'][0] in xs]


def beams(frame, miss=None):
    """The laser's beams; miss: only those to a dot (0) or into nothing
    (1)."""
    xs = BEAM_X if miss is None else (BEAM_X[miss],)
    return [q for q in quads(frame)
            if q['flags'] & BLEND and q['rect'][0] in xs]


def tags(frame):
    """Edit mode's tag naming the chosen item."""
    return [q for q in quads(frame) if q['flags'] & BLEND
            and tuple(q['rect'][2:]) == (TAG_COLS, TAG_ROWS)]


def unit(v):
    n = math.sqrt(sum(x * x for x in v))
    return tuple(x / n for x in v)


def rect_texel(res, fr, q, tx, ty):
    """Texel tx, ty of the part of its image a quad shows."""
    w, h, bpp, rows = read_png(image(res, fr, q))
    x, y = q['rect'][0] + tx, q['rect'][1] + ty
    px = rows[y][x * bpp:x * bpp + bpp]
    return tuple(px[:3]), (px[3] if bpp == 4 else 255)


def beam_errors(what, b, p, t, miss, head=(0.0, 0.0, 0.0)):
    """Beam b on the ray from p towards t: its strip, its middle and
    length (to t, or BEAM_REACH into nothing), its x down the ray and
    its face towards head."""
    d = unit(tuple(y - x for x, y in zip(p, t)))
    length = (BEAM_REACH if miss
              else math.sqrt(sum((y - x) ** 2 for x, y in zip(p, t))))
    mid = tuple(x + y * length / 2.0 for x, y in zip(p, d))
    to = tuple(a - m for a, m in zip(head, mid))
    k = sum(a * c for a, c in zip(to, d))
    face = unit(tuple(a - k * c for a, c in zip(to, d)))
    q = tuple(b['pose'][3:])
    errors = []
    if b['rect'][0] != BEAM_X[miss] or tuple(b['rect'][2:]) != (BEAM_LEN,
                                                                  BEAM_ROWS):
        errors.append('%s shows %s of the image' % (what, b['rect']))
    if not near3(b['pose'][:3], mid) \
            or not near3(b['size'], (length, BEAM_WIDTH), 0.002):
        errors.append('%s at %s, %s, want %s, %s' % (
            what, [round(v, 3) for v in b['pose'][:3]], b['size'],
            [round(v, 3) for v in mid], (round(length, 3), BEAM_WIDTH)))
    if sum(a * c for a, c in zip(qrot(q, (1.0, 0.0, 0.0)), d)) < 0.999:
        errors.append('%s does not run down the ray' % what)
    if sum(a * c for a, c in zip(qrot(q, (0.0, 0.0, 1.0)), face)) < 0.999:
        errors.append('%s does not face the head' % what)
    return errors


def strip_errors(res, f, b, miss, what):
    """The beam's strip: white at BEAM_LEVEL in the middle, faded in at
    the hand, and on a miss faded out at the far end."""
    errors = []
    rgb, alpha = rect_texel(res, f, b, BEAM_LEN // 2, BEAM_ROWS // 2)
    if abs(alpha - BEAM_LEVEL) > 2 or abs(rgb[0] - BEAM_LEVEL) > 2:
        errors.append('%s shows %s alpha %d mid-way, want %d' % (
            what, rgb, alpha, BEAM_LEVEL))
    if rect_texel(res, f, b, 0, BEAM_ROWS // 2)[1] >= BEAM_LEVEL // 8:
        errors.append('%s does not fade in at the hand' % what)
    if miss and rect_texel(res, f, b, BEAM_LEN - 1,
                           BEAM_ROWS // 2)[1] >= BEAM_LEVEL // 32:
        errors.append('%s does not fade out at its end' % what)
    return errors


def between(res, start, end=None, pick=quads, delay=0.0):
    """Frames from delay seconds after mark start until mark end that pick
    finds something in."""
    t0 = (res.marks[start] + delay) * 1e6
    t1 = res.marks[end] * 1e6 if end else float('inf')
    return [f for f in res.frames if t0 < f['t_us'] < t1 and pick(f)]


def last_before(res, mark, pick):
    fs = [f for f in res.frames
          if f['t_us'] < res.marks[mark] * 1e6 and pick(f)]
    return pick(fs[-1]) if fs else None


def near3(a, b, tol=EDIT_POS):
    return all(abs(x - y) <= tol for x, y in zip(a, b))


def pose_errors(what, qs, pos, turn=None, size=None):
    """Every quad in qs at pos, turned as turn (yaw, pitch and roll each
    within 0.6 degrees) and size if given."""
    if not qs:
        return ['%s: no frames' % what]
    for q in qs:
        if q is None:
            return ['%s: the quad is missing' % what]
        if not near3(q['pose'][:3], pos):
            return ['%s at %s, want %s' % (
                what, [round(v, 3) for v in q['pose'][:3]],
                [round(v, 3) for v in pos])]
        if turn is not None:
            got, want = euler(q['pose'][3:]), euler(turn)
            if not all(abs((a - b + 180.0) % 360.0 - 180.0) <= 0.6
                       for a, b in zip(got, want)):
                return ['%s turned %s, want %s' % (
                    what, [round(v, 2) for v in got],
                    [round(v, 2) for v in want])]
        if size is not None and not near3(q['size'], size):
            return ['%s sized %s, want %s' % (
                what, [round(v, 4) for v in q['size']],
                [round(v, 4) for v in size])]
    return []


def stereo_errors(frames):
    """Screen 1's eyes together, one item: the same pose and size."""
    for f in frames:
        eyes = edit_screen0(f)
        if len(eyes) != 2 or not (near3(eyes[0]['pose'], eyes[1]['pose'],
                                        1e-4)
                                  and near3(eyes[0]['size'],
                                            eyes[1]['size'], 1e-4)):
            return ['frame %d: screen 1\'s eyes apart: %s'
                    % (f['n'], [(q['pose'][:3], q['size']) for q in eyes])]
    return []


def saved(res):
    """The configuration RetroArch saved at exit."""
    cfg = {}
    path = os.path.join(res.dir, 'retroarch.cfg')
    if os.path.exists(path):
        with open(path) as f:
            for line in f:
                m = re.match(r'(\w+) = "(.*)"', line.strip())
                if m:
                    cfg[m.group(1)] = m.group(2)
    return cfg


def saved_errors(res, what, prefix, base, want, size, last=None,
                 anchor=None):
    """The saved settings put the item where want is, its width and
    height at scale 1 size, from anchor (the identity unless given),
    and where the headset last showed it."""
    cfg = saved(res)
    v = [float(cfg.get(prefix + k, '0'))
         for k in ('x', 'y', 'z', 'yaw', 'pitch', 'roll')]
    scale = float(cfg.get(prefix + 'scale', '1'))
    pose = (tuple(b + o for b, o in zip(base, v[:3])), from_euler(*v[3:]))
    if anchor is not None:
        pose = compose(anchor, pose)
    q = {'pose': list(pose[0]) + list(pose[1]),
         'size': [size[0] * scale, size[1] * scale]}
    errors = pose_errors('%s from its saved settings' % what, [q], want[0],
                         want[1])
    if last is not None:
        errors += pose_errors('%s last shown' % what, [last], pose[0],
                              pose[1], q['size'])
    return errors


def edit_log_errors(res, want, exact=True):
    """Edit mode's on and off lines in order; not exact, a last off may
    follow (quitting ends it)."""
    got = ['on' if EDIT_ON in line else 'off'
           for line in res.log.splitlines()
           if EDIT_ON in line or EDIT_OFF in line]
    if got == want or (not exact and got == want + ['off']):
        return []
    return ['edit mode %s, want %s' % (got, want)]


def edit_sync_errors(res, on, off, game=GAME_SETS):
    """The game's sets before mark on, edit mode's alone until mark off,
    the game's again after."""
    syncs = [(e['t_us'], e['sets']) for e in events(res, 'sync')
             if e['sets']]
    t_on, t_off = res.marks[on] * 1e6, res.marks[off] * 1e6
    before = [s for t, s in syncs if t < t_on]
    during = [s for t, s in syncs if t_on <= t < t_off]
    after = [s for t, s in syncs if t >= t_off]
    errors = []
    if not before or before[-1] != game:
        errors.append('synced %s before %s, want %s' % (before[-1:], on, game))
    if during != [EDIT_SETS]:
        errors.append('synced %s between %s and %s, want only %s'
                      % (during, on, off, EDIT_SETS))
    if not after or after[-1] != game:
        errors.append('synced %s after %s, want %s' % (after, off, game))
    return errors


def core_frames(res, t0, t1):
    """The core's logged frame numbers from t0 to t1 us."""
    return [int(n) for n, t in CORE_FRAME.findall(res.log)
            if t0 <= int(t) <= t1]


def check_edit_grab(menu=True):
    def check(res):
        """A grip on screen 1 through the menu's middle takes both its
        eyes along with the hand, level; they stay where it let go,
        through the release and after edit mode ends, and are saved
        there, the menu following them unplaced. While editing every
        item is outlined, the pointed-at one brightly, and the menu has
        its handle."""
        errors = edit_log_errors(res, ['on', 'off'])
        errors += pose_errors('screen 1 before the grab',
                              [screen1_left(f) for f in
                               between(res, 'on', 'grab', edit_screen0,
                                       0.5)], S0, ID, (1.6, 0.96))
        late = between(res, 'moved', None, edit_screen0)
        if not late:
            return errors + no_quads(res)
        want = (0.3, 0.0, -1.8)
        errors += pose_errors('screen 1 moved',
                              [q for f in late for q in edit_screen0(f)],
                              want, ID, (1.6, 0.96))
        errors += stereo_errors(between(res, 'aimed', None, edit_screen0))
        if not any(f['t_us'] > res.marks['off'] * 1e6 for f in late):
            errors.append('no frame after edit mode ended')
        items = 2
        if menu:
            items = 3
            d = math.sqrt(0.3 * 0.3 + 1.7 * 1.7) / 1.7
            errors += pose_errors(
                'the menu following it',
                [menu_quad(f) for f in between(res, 'moved', 'stop',
                                               delay=0.5)],
                (0.3, 0.0, -1.7), ID, (MENU_W * d, MENU_H * d))
        lit = [f for f in between(res, 'aimed', 'released', outlines)
               if len(outlines(f)) == items and len(outlines(f, 1)) == 1
               and len(handles(f)) == int(menu)]
        if not lit:
            errors.append('no frame outlined all %d items, one brightly%s'
                          % (items, ', with a handle' if menu else ''))
        if any(outlines(f) or handles(f)
               for f in between(res, 'off', delay=0.5)):
            errors.append('outlines after edit mode ended')
        errors += saved_errors(res, 'screen 1', 'video_screen1_xr_', S0,
                               (want, ID), (1.6, 0.96),
                               edit_screen0(late[-1])[0])
        if saved(res).get('video_openxr_menu_placed') != 'false':
            errors.append('grabbing through the menu placed it')
        return errors
    return check


def slot_made(res, slot=0):
    """How often the slot's swapchains were made while editing."""
    made, on = 0, False
    for line in res.log.splitlines():
        if EDIT_ON in line:
            on = True
        elif EDIT_OFF in line:
            on = False
        elif on and '[OpenXR] Slot %d:' % slot in line:
            made += 1
    return made


def check_edit_follow(res):
    """Held, screen 1 moves on nearly every headset frame whose aim moved
    (the laser's dot, placed from the frame's own aim, shows it) while
    the core draws one frame in two: each frame places it from its own
    aim, not from the last video frame. Brought nearer, it keeps its
    image: in every headset frame, not made again for each distance."""
    errors = edit_log_errors(res, ['on', 'off'])
    held = between(res, 'grab', 'released', lambda f: True)
    gaps = [f['n'] for f in held if len(edit_screen0(f)) != 2]
    if not held or gaps:
        errors.append('screen 1 missing from %d of the %d headset frames '
                      'it was held in: %s' % (len(gaps), len(held),
                                              gaps[:10]))
    made = slot_made(res)
    if made > 2:
        errors.append('screen 1\'s swapchains made %d times while editing'
                      % made)
    sweep = [f for f in between(res, 'sweep', 'swept', edit_screen0)
             if cursors(f)]
    xs = [(screen1_left(f)['pose'][0], cursors(f)[0]['pose'][0])
          for f in sweep]
    pairs = [(a, b) for a, b in zip(xs, xs[1:]) if abs(b[1] - a[1]) > 0.001]
    if len(pairs) < 8:
        return errors + ['only %d headset frames saw the aim move'
                         % len(pairs)]
    moved = sum(1 for a, b in pairs if abs(b[0] - a[0]) > 0.001)
    if moved < 0.9 * len(pairs):
        errors.append('screen 1 moved in %d of the %d headset frames whose '
                      'aim moved' % (moved, len(pairs)))
    t0, t1 = res.marks['sweep'] * 1e6, res.marks['swept'] * 1e6
    fps = core_fps(res, t0 - 2e6, t1 + 2e6)
    hz = len(xs) * 1e6 / (t1 - t0)
    if not fps or fps * 1.5 > hz:
        errors.append('the core at %.1f fps, the headset at %.1f Hz: the '
                      'core was not the slower' % (fps, hz))
    errors += stereo_errors(between(res, 'grab', None, edit_screen0))
    want = (0.3, 0.0, -1.5)
    errors += pose_errors('screen 1 let go',
                          [edit_screen0(f)[0] for f in
                           between(res, 'released', None, edit_screen0,
                                   1.0)], want, ID)
    return errors + saved_errors(res, 'screen 1', 'video_screen1_xr_', S0,
                                 (want, ID), (1.6, 0.96))


def no_step_back(frames, pick, want, what):
    """Once at want (moving there along +x), never short of it again."""
    reached = False
    for f in frames:
        q = pick(f)
        if not q:
            continue
        if near3(q['pose'], want):
            reached = True
        elif reached or q['pose'][0] > want[0] + EDIT_POS:
            return ['%s stepped to %s at frame %d, want %s'
                    % (what, [round(v, 3) for v in q['pose'][:3]], f['n'],
                       want)]
    return [] if reached else ['%s never reached %s' % (what, want)]


def screen1_left(frame):
    eyes = edit_screen0(frame)
    return eyes[0] if eyes else None


def screen1_at_yaw(yaw):
    """A pick: screen 1's left eye where it shows exactly yaw."""
    def pick(frame):
        q = screen1_left(frame)
        if q and abs(euler(tuple(q['pose'][3:]))[0] - yaw) <= 0.05:
            return q
        return None
    return pick


def screen2_at_yaw(yaw, past=False):
    """A pick: screen 2 where it shows exactly yaw, or once it has
    turned past it (yaw rising) if past."""
    def pick(frame):
        q = screen2(frame)
        if not q:
            return None
        y = euler(tuple(q['pose'][3:]))[0]
        return q if (y >= yaw if past else abs(y - yaw) <= 0.05) else None
    return pick


def check_edit_release(res):
    """Let go after a sweep under threaded video: screen 1 never steps
    back from where the hand left it, and is saved there."""
    want = (0.15, 0.0, -1.8)
    return (edit_log_errors(res, ['on'], exact=False)
            + no_step_back(between(res, 'swept'), screen1_left, want,
                           'screen 1')
            + stereo_errors(between(res, 'grab', None, edit_screen0))
            + saved_errors(res, 'screen 1', 'video_screen1_xr_', S0,
                           (want, ID), (1.6, 0.96)))


def check_edit_release_moving(res):
    """Let go mid-sweep: screen 1 is let go and saved where a frame at
    most a poll (100 ms at FPS10) before the release showed it, not
    where the release poll's aim puts it, and no frame after the release
    shows it further back."""
    errors = edit_log_errors(res, ['on'], exact=False)
    xs = [(f['t_us'], screen1_left(f)['pose'][0])
          for f in between(res, 'sweep', None, edit_screen0)]
    t_rel = res.marks['released'] * 1e6
    poll, slack = 100000, 20000
    back = [x for t, x in xs if t_rel - poll - slack <= t <= t_rel + slack]
    late = [x for t, x in xs if t > t_rel]
    rest = [screen1_left(f) for f in
            between(res, 'released', None, edit_screen0, 1.0)]
    if not back or not late or not rest:
        return errors + ['no frames around the release']
    want = (rest[-1]['pose'][0], 0.0, -1.8)
    if not any(abs(want[0] - x) <= 0.002 for x in back):
        errors.append('screen 1 let go at %.3f, want where a frame showed '
                      'it a poll before the release: %s'
                      % (want[0], [round(x, 3) for x in back]))
    for x in late:
        if x < want[0] - 0.002:
            errors.append('after the release screen 1 stepped back to '
                          '%.3f, past where it was let go' % x)
            break
    errors += pose_errors('screen 1 let go', rest, want, ID)
    return errors + saved_errors(res, 'screen 1', 'video_screen1_xr_', S0,
                                 (want, ID), (1.6, 0.96))


def check_edit_regrab(res):
    """Let go and grip again at once: the regrab takes screen 1 where it
    was let go, while quads planned at the release are still coming,
    and nudges it on from there."""
    errors = edit_log_errors(res, ['on'], exact=False)
    want = (0.15, 0.0, -1.8)
    errors += pose_errors('screen 1 let go and taken again',
                          [screen1_left(f) for f in
                           between(res, 'released', 'moved_again',
                                   edit_screen0)], want, ID)
    again = (0.25, 0.0, -1.8)
    errors += pose_errors('screen 1 nudged',
                          [screen1_left(f) for f in
                           between(res, 'moved_again', None, edit_screen0,
                                   1.0)], again, ID)
    return errors + saved_errors(res, 'screen 1', 'video_screen1_xr_', S0,
                                 (again, ID), (1.6, 0.96))


def check_edit_handover(res):
    """One hand lets go of screen 1 and takes screen 2 at the next poll:
    screen 1 never steps back, screen 2 moves, both are saved."""
    errors = edit_log_errors(res, ['on'], exact=False)
    want = (0.15, 0.0, -1.8)
    errors += no_step_back(between(res, 'swept'), screen1_left, want,
                           'screen 1')
    b_want = (0.3, -0.992, -1.8)
    errors += pose_errors('screen 2 moved',
                          [screen2(f) for f in
                           between(res, 'b_moved', 'end', delay=1.0)],
                          b_want, ID, (1.28, 0.96))
    errors += saved_errors(res, 'screen 1', 'video_screen1_xr_', S0,
                           (want, ID), (1.6, 0.96))
    return errors + saved_errors(res, 'screen 2', 'video_screen2_xr_', S1,
                                 (b_want, ID), (1.28, 0.96))


def check_edit_tilt(res):
    """Level by default: the wrist's roll leaves screen 1 upright; with
    the trigger held the roll follows from where it was, and stays when
    the trigger is let go."""
    errors = []
    for mark, want, what in (
            ('rolled', ID, 'rolled 30 without the trigger'),
            ('tilted', ID, 'the trigger pressed'),
            ('turned', ROLL30, 'rolled 30 more with the trigger'),
            ('kept', ROLL30, 'the trigger let go, the wrist back')):
        q = last_before(res, mark, edit_screen0)
        if not q:
            errors.append('no frame before %s' % what)
        elif not all(facing(e, want) for e in q):
            errors.append('%s: screen 1 turned %s, want %s'
                          % (what, q[0]['pose'][3:], want))
    return errors + saved_errors(res, 'screen 1', 'video_screen1_xr_', S0,
                                 (S0, ROLL30), (1.6, 0.96))


T_HAND = (0.8, 0.0, -0.3)
T0 = (T_HAND, (0.0, 0.0, -1.8))
T1 = (T_HAND, (0.0, 0.8, -1.8))


def check_edit_turn(tilt):
    want = held((S0, ID), aim_pose(*T0), aim_pose(*T1), tilt)

    def check(res):
        """A hand beside screen 1 turned up carries it up, as video_xr's
        grab does: keeping its yaw, pitch and roll, or with the trigger
        held letting it follow the hand; saved as shown."""
        errors = edit_log_errors(res, ['on'], exact=False)
        if tilt != (abs(euler(want[1])[2]) > 5.0):
            errors.append('the case does not tell level from tilt')
        if tilt != (abs(euler(want[1])[1]) > 5.0):
            errors.append('the case does not tell a held pitch from one '
                          'that follows')
        shown = [screen1_left(f) for f in
                 between(res, 'moved', None, edit_screen0, 1.5)]
        errors += pose_errors('screen 1 turned by the hand', shown, want[0],
                              want[1], (1.6, 0.96))
        return errors + saved_errors(res, 'screen 1', 'video_screen1_xr_',
                                     S0, want, (1.6, 0.96),
                                     shown[-1] if shown else None)
    return check


def check_edit_stick(res):
    """The stick held up pushes screen 1 away along the laser, from a
    hand below and beside it, inside its deadzone does nothing, and
    held right grows it."""
    held_q = last_before(res, 'held', screen1_left)
    pushed = last_before(res, 'pushed', screen1_left)
    dead = last_before(res, 'deaded', screen1_left)
    grown = last_before(res, 'grown', screen1_left)
    if not (held_q and pushed and dead and grown):
        return no_quads(res)
    errors = edit_log_errors(res, ['on', 'off'])
    errors += pose_errors('screen 1 held', [held_q], S0, ID)
    ray = [b - a for a, b in zip(*A0)]
    ray = [v / math.sqrt(sum(r * r for r in ray)) for v in ray]
    disp = [b - a for a, b in zip(S0, pushed['pose'][:3])]
    along = sum(a * b for a, b in zip(disp, ray))
    off = math.sqrt(sum((d - along * r) ** 2 for d, r in zip(disp, ray)))
    if not 0.3 <= along <= 1.3 or off > 0.02:
        errors.append('half a second of push moved screen 1 %.3f along the '
                      'laser and %.3f off it, want 0.3 to 1.3 along'
                      % (along, off))
    errors += pose_errors('screen 1 after the deadzone', [dead],
                          pushed['pose'][:3], ID, pushed['size'])
    k = grown['size'][0] / pushed['size'][0]
    if not 1.2 <= k <= 1.9:
        errors.append('half a second right scaled it by %.3f, want 1.2 to '
                      '1.9' % k)
    errors += pose_errors('screen 1 grown', [grown], pushed['pose'][:3], ID)
    return errors + saved_errors(res, 'screen 1', 'video_screen1_xr_', S0,
                                 (grown['pose'][:3], ID), (1.6, 0.96),
                                 last_before(res, 'off', screen1_left))


def check_edit_reset(menu=True):
    def check(res):
        """Screen 1 from its offsets, grabbed and moved, then reset as
        Reset Screen does, and the menu with it; the menu taken by its
        handle and moved, screen 1 staying, then back in front of screen
        1 on its own reset, unplaced."""
        errors = edit_log_errors(res, ['on', 'off'])
        errors += pose_errors('screen 1 at its offsets',
                              [last_before(res, 'offset', screen1_left)],
                              (0.25, 0.1, -1.5), ID)
        errors += pose_errors('screen 1 moved',
                              [screen1_left(f) for f in
                               between(res, 'moved', 'reset', edit_screen0,
                                       1.0)], (0.55, 0.1, -1.5), ID)
        late = between(res, 'reset', 'off', edit_screen0, 1.0)
        errors += pose_errors('screen 1 reset',
                              [screen1_left(f) for f in late], S0, ID,
                              (1.6, 0.96))
        if menu:
            errors += pose_errors(
                'the menu after the screen\'s reset',
                [menu_quad(f) for f in between(res, 'reset', 'handle',
                                               delay=1.0)],
                MENU_SPOT, ID, (MENU_W, MENU_H))
            errors += pose_errors(
                'the menu taken by its handle',
                [menu_quad(f) for f in between(res, 'placed', 'back',
                                               delay=1.0)],
                (0.5, 0.0, -1.7), ID, (MENU_W, MENU_H))
            errors += pose_errors(
                'its reset',
                [menu_quad(f) for f in between(res, 'back', 'off',
                                               delay=1.0)],
                MENU_SPOT, ID, (MENU_W, MENU_H))
            if saved(res).get('video_openxr_menu_placed') != 'false':
                errors.append('the menu\'s reset left it placed')
        return errors + saved_errors(res, 'screen 1', 'video_screen1_xr_',
                                     S0, (S0, ID), (1.6, 0.96),
                                     screen1_left(late[-1]) if late
                                     else None)
    return check


def check_edit_menu_front(res):
    """Screen 1 moved 60 cm closer takes the menu with it, 10 cm in front,
    as large to the eye as at its spot and submitted after the
    screens."""
    fr = last_snap(res)
    if not fr:
        return no_quads(res)
    ms = menus(fr)
    errors = []
    d = 1.1 / 1.7
    if len(ms) != 1 or not at(ms[0], (0.0, 0.0, -1.1)) \
            or not sized(ms[0], (MENU_W * d, MENU_H * d)):
        errors.append('the menu at %s, want 0,0,-1.1 and %.3f wide'
                      % ([(m['pose'][:3], m['size']) for m in ms],
                         MENU_W * d))
    order = quads(fr)
    if ms and any(order.index(q) > order.index(ms[0])
                  for q in edit_screen0(fr)):
        errors.append('a screen was submitted after the menu')
    return errors


F_D = 0.8 / 1.7
F_HANDLE = handle_y(0.0, MENU_H * F_D)


def check_edit_menu_size(res):
    """Screen 1 0.9 m nearer: the menu follows 0.8 m away as large to the
    eye as at its spot, and taken by its handle it is placed as large
    as it showed."""
    errors = edit_log_errors(res, ['on'], exact=False)
    size = (MENU_W * F_D, MENU_H * F_D)
    errors += pose_errors('the menu following the nearer screen 1',
                          [menu_quad(f) for f in between(res, 'on', 'grab',
                                                         delay=1.0)],
                          (0.0, 0.0, -0.8), ID, size)
    shown = [menu_quad(f) for f in between(res, 'moved', None, menus, 1.5)]
    errors += pose_errors('the menu placed', shown, (0.3, 0.0, -0.8), ID,
                          size)
    if saved(res).get('video_openxr_menu_placed') != 'true':
        errors.append('the menu was not placed')
    return errors + saved_errors(res, 'the menu', 'video_openxr_menu_xr_',
                                 MENU_SPOT, ((0.3, 0.0, -0.8), ID),
                                 (MENU_W, MENU_H),
                                 shown[-1] if shown else None)


# Screen 1 at EDIT_OFFSET's 0.25,0.1,-1.5: the menu following 10 cm in
# front, as large to the eye as at its spot.
M_D = math.sqrt(0.25 * 0.25 + 0.1 * 0.1 + 1.4 * 1.4) / 1.7
M_HANDLE = handle_y(0.1, MENU_H * M_D)


def check_edit_menu_reset(res):
    """The menu following a moved screen 1, taken by its handle and moved,
    screen 1 staying; reset on its handle, it follows screen 1 again,
    unplaced."""
    errors = edit_log_errors(res, ['on', 'off'])
    size = (MENU_W * M_D, MENU_H * M_D)
    errors += pose_errors('the menu following screen 1',
                          [menu_quad(f) for f in between(res, 'on', 'grab',
                                                         delay=1.0)],
                          (0.25, 0.1, -1.4), ID, size)
    errors += pose_errors('the menu taken by its handle',
                          [menu_quad(f) for f in between(res, 'placed',
                                                         'back', delay=1.0)],
                          (0.55, 0.1, -1.4), ID, size)
    errors += pose_errors('screen 1 while the menu moved',
                          [screen1_left(f) for f in between(
                              res, 'grab', 'off', edit_screen0)],
                          (0.25, 0.1, -1.5), ID)
    errors += pose_errors('the menu reset',
                          [menu_quad(f) for f in between(res, 'back', 'off',
                                                         delay=1.0)],
                          (0.25, 0.1, -1.4), ID, size)
    if saved(res).get('video_openxr_menu_placed') != 'false':
        errors.append('the menu\'s reset left it placed')
    return errors


def band_errors(res, f, q, level, what):
    """The three bands across the part of the image q shows, from its
    top edge in, dark, light, dark, at level: through an outline's rim,
    or across the handle bar."""
    if q['rect'][3] == OUTLINE_DIM:
        rows = (0, OUTLINE_BAND, 2 * OUTLINE_BAND)
    else:
        rows = (0, HANDLE_ROWS // 2, HANDLE_ROWS - 1)
    errors = []
    for row, light in zip(rows, (False, True, False)):
        rgb, alpha = rect_texel(res, f, q, q['rect'][2] // 2, row)
        want = level if light else 0
        if abs(alpha - level) > 2 or abs(rgb[0] - want) > 2:
            errors.append('%s row %d shows %s alpha %d, want %d alpha %d'
                          % (what, row, rgb, alpha, want, level))
    return errors


def outline_whole(o, q):
    """Outline o's size uncut, along item q's own x and y."""
    w = o['size'][0] * OUTLINE_DIM / o['rect'][2]
    ox = qrot(tuple(o['pose'][3:]), (1.0, 0.0, 0.0))
    qx = qrot(tuple(q['pose'][3:]), (1.0, 0.0, 0.0))
    if abs(sum(a * b for a, b in zip(ox, qx))) < 0.5:
        return (o['size'][1], w)
    return (w, o['size'][1])


def outline_gap_errors(frame):
    """Every outline's clear middle, where filtering leaves its rim,
    OUTLINE_GAP wider each side than the item it is round: the rim never
    covers the picture. An outline cut to hide an edge's rim counts at
    its whole size, turned back for a top or bottom edge."""
    items = [q for q in quads(frame) if not q['flags'] & BLEND] \
        + menus(frame)
    if not items:
        return ['no items under the outlines']
    errors = []
    for o in outlines(frame):
        q = min(items, key=lambda i: sum(
            (a - b) ** 2 for a, b in zip(i['pose'][:3], o['pose'][:3])))
        gaps = [(s * (1.0 - 2.0 * OUTLINE_F) - w) / 2.0
                for s, w in zip(outline_whole(o, q), q['size'])]
        if any(abs(g - OUTLINE_GAP) > 0.001 for g in gaps):
            errors.append('the outline at %s is %s clear of its item, '
                          'want %.3f' % (
                              [round(v, 3) for v in o['pose'][:3]],
                              [round(g, 4) for g in gaps], OUTLINE_GAP))
    return errors


EDIT_HANDLE = handle_y(0.0, MENU_H)
# Screen 2 flush against screen 1's right edge, the menu out of the way.
FLUSH = {'video_screen2_xr_x': '1.44', 'video_screen2_xr_y': '0.992',
         'video_openxr_menu_placed': 'true', 'video_openxr_menu_xr_x': '-2'}


def texel_at(res, fr, q, x, y):
    """The texel quad q, facing +Z, shows at x, y; None off it."""
    o = tuple(q['pose'][3:])
    lx, ly, _ = qrot((-o[0], -o[1], -o[2], o[3]),
                     (x - q['pose'][0], y - q['pose'][1], 0.0))
    u, v = lx / q['size'][0] + 0.5, 0.5 - ly / q['size'][1]
    if not (0.0 <= u < 1.0 and 0.0 <= v < 1.0):
        return None
    return rect_texel(res, fr, q, int(u * q['rect'][2]),
                      int(v * q['rect'][3]))


def seen_at(res, fr, x, y):
    """What frame fr shows at x, y looking down -Z: its layers that face
    +Z, each in its turn over those before, from black."""
    out = (0, 0, 0)
    for q in quads(fr):
        if qrot(tuple(q['pose'][3:]), (0.0, 0.0, 1.0))[2] < 0.999:
            continue
        t = texel_at(res, fr, q, x, y)
        if t is None:
            continue
        rgb, alpha = t
        out = (tuple(min(255, c + (255 - alpha) * b // 255)
                     for c, b in zip(rgb, out))
               if q['flags'] & BLEND else rgb)
    return out


def check_edit_outlines_flush(res):
    """Screen 2 flush against screen 1's right edge, screen 1 lit: its
    outline goes under the screens, so screen 2's picture covers that
    side of its rim; the other three sides still show."""
    errors = edit_log_errors(res, ['on', 'off'])
    snaps = [f for f in between(res, 'lit', 'off', delay=0.5)
             if f['snap'] and len(outlines(f, 1)) == 1 and screen2(f)]
    if not snaps:
        return errors + ['no snapshot of screen 1 outlined beside screen 2']
    f = snaps[-1]
    o, s2 = outlines(f, 1)[0], screen2(f)
    if not near3(o['pose'], (0.0, 0.0, -1.798)) \
            or not near3(s2['pose'], (1.44, 0.0, -1.8)):
        return errors + ['screen 1\'s outline at %s, screen 2 at %s: not '
                         'flush' % (o['pose'][:3], s2['pose'][:3])]
    tw, th = (s / OUTLINE_DIM for s in o['size'])
    right = o['pose'][0] + o['size'][0] / 2.0
    covered = []
    for k in range(OUTLINE_EDGE):
        for y in (-0.3, 0.0, 0.3):
            x = right - (k + 0.5) * tw
            want = texel_at(res, f, s2, x, y)
            seen = seen_at(res, f, x, y)
            if want is None or any(abs(a - b) > 2
                                   for a, b in zip(seen, want[0])):
                covered.append((round(x, 3), y, seen))
    if covered:
        errors.append('screen 1\'s rim over screen 2\'s picture: %s'
                      % covered[:4])
    light = OUTLINE_BAND + 0.5
    for what, x, y in (
            ('left', o['pose'][0] - o['size'][0] / 2.0 + light * tw, 0.0),
            ('top', 0.0, o['pose'][1] + o['size'][1] / 2.0 - light * th),
            ('bottom', 0.0, o['pose'][1] - o['size'][1] / 2.0 + light * th)):
        seen = seen_at(res, f, x, y)
        if any(c < 253 for c in seen):
            errors.append('screen 1\'s rim shows %s on its %s side, want '
                          'its light band' % (seen, what))
    return errors


def check_edit_outlines(menu=True):
    def check(res):
        """Faint outlines round every item, a bright one round the item
        under a laser, the menu's faint handle bar under it and bright
        with the menu's outline under a laser; none after edit mode."""
        errors = edit_log_errors(res, ['on', 'off'])
        lit = between(res, 'lit', 'handle', delay=0.5)
        items = 3 if menu else 2
        good = [f for f in lit if len(outlines(f)) == items
                and len(outlines(f, 1)) == 1
                and len(handles(f)) == int(menu)]
        if not good:
            return errors + ['no frame with %d outlines, one bright: %s' % (
                items, [[q['rect'][0] for q in outlines(f) + handles(f)]
                        for f in lit[-1:]])]
        b = outlines(good[-1], 1)[0]
        if not near3(b['pose'], (0.0, 0.0, -1.798)) \
                or not near3(b['size'], outline_size(1.6, 0.96), 0.002):
            errors.append('the bright outline at %s, %s, want screen 1\'s'
                          % (b['pose'][:3], b['size']))
        errors += outline_gap_errors(good[-1])
        snaps = [f for f in good if f['snap']]
        if snaps:
            s = snaps[-1]
            errors += band_errors(res, s, outlines(s, 1)[0], 255,
                                 'the bright outline')
            errors += band_errors(res, s, outlines(s, 0)[0], 64,
                                 'a faint outline')
            if menu and handles(s, 0):
                errors += band_errors(res, s, handles(s, 0)[0], 64,
                                     'the faint handle')
        else:
            errors.append('no snapshot of the outlines')
        if menu:
            h = handles(good[-1], 0)
            if len(h) != 1 or not near3(h[0]['pose'],
                                        (0.0, EDIT_HANDLE, -1.698)) \
                    or not near3(h[0]['size'], (MENU_W * 0.4, 0.03)):
                errors.append('the faint handle: %s'
                              % [(q['pose'][:3], q['size']) for q in h])
            on = [f for f in between(res, 'handle', 'off', delay=0.5)
                  if len(handles(f, 1)) == 1 and len(outlines(f, 1)) == 1
                  and near3(outlines(f, 1)[0]['pose'], (0.0, 0.0, -1.698))]
            if not on:
                errors.append('the laser on the handle lit no handle and '
                              'menu outline')
            elif [f for f in on if f['snap']]:
                s = [f for f in on if f['snap']][-1]
                errors += band_errors(res, s, handles(s, 1)[0], 255,
                                     'the bright handle')
        elif any(handles(f) for f in lit):
            errors.append('a handle without a menu quad')
        if any(outlines(f) or handles(f)
               for f in between(res, 'off', delay=1.0)):
            errors.append('outlines after edit mode ended')
        return errors
    return check


def check_edit_beam(res):
    """Edit Mode Only: the dot alone before edit mode; in it a beam from
    the hand to the dot facing the head, then 1.5 m into nothing; the dot
    alone again after."""
    errors = edit_log_errors(res, ['on', 'off'])
    before = between(res, 'before', 'on', delay=0.5)
    if not [f for f in before if cursors(f)]:
        errors.append('no dot before edit mode')
    if any(beams(f) for f in before):
        errors.append('a beam before edit mode')
    hits = [f for f in between(res, 'aimed', 'miss', delay=0.5)
            if len(beams(f)) == 1 and cursors(f)]
    if not hits:
        return errors + ['no frame with one beam and its dot']
    errors += beam_errors('the beam', beams(hits[-1])[0], GUN_FROM, GUN_AT, 0)
    snaps = [f for f in hits if f['snap']]
    if snaps:
        errors += strip_errors(res, snaps[-1], beams(snaps[-1])[0], 0,
                               'the beam')
    else:
        errors.append('no snapshot of the beam')
    misses = [f for f in between(res, 'miss', 'off', delay=0.5)
              if len(beams(f, 1)) == 1]
    if not misses:
        return errors + ['no frame with a beam into nothing']
    errors += beam_errors('the beam into nothing', beams(misses[-1])[0],
                          GUN_FROM, MISS_AT, 1)
    snaps = [f for f in misses if f['snap']]
    if snaps:
        errors += strip_errors(res, snaps[-1], beams(snaps[-1])[0], 1,
                               'the beam into nothing')
    after = between(res, 'off', delay=1.0)
    if any(beams(f) for f in after):
        errors.append('a beam after edit mode')
    return errors


def check_edit_beam_off(res):
    """Laser Beam Off: edit mode's dot and outlines, never a beam."""
    errors = edit_log_errors(res, ['on', 'off'])
    if not [f for f in between(res, 'aimed', 'off', delay=0.5)
            if cursors(f) and outlines(f)]:
        errors.append('no frame with the dot and the outlines')
    if any(beams(f) for f in res.frames):
        errors.append('a beam with Laser Beam Off')
    return errors


def check_laser_beam_always(res):
    """Laser Beam Always, out of edit mode: a beam to the light gun's
    dot, one into nothing beside the screens, none while the laser's
    hotkey hides it, and back after."""
    errors = []
    hits = [f for f in between(res, 'aimed', 'miss', delay=0.5)
            if len(beams(f)) == 1 and cursors(f)]
    if hits:
        errors += beam_errors('the beam', beams(hits[-1])[0], GUN_FROM,
                              GUN_AT, 0)
    else:
        errors.append('no frame with the beam and its dot')
    misses = [f for f in between(res, 'miss', 'off', delay=0.5)
              if len(beams(f, 1)) == 1]
    if misses:
        errors += beam_errors('the beam into nothing', beams(misses[-1])[0],
                              GUN_FROM, MISS_AT, 1)
    else:
        errors.append('no frame with a beam into nothing')
    if any(beams(f) or cursors(f)
           for f in between(res, 'off', 'on', delay=0.5)):
        errors.append('a beam or dot with the laser hidden')
    if not [f for f in between(res, 'on', delay=0.5) if beams(f)]:
        errors.append('no beam once the laser is back')
    return errors


def check_edit_fallback(menu=True):
    def check(res):
        """A views core's frame shown whole (threaded video crops the map
        away) is no item: grip, move and reset leave it, no dot or
        outline shows on it, and screen 1's settings stay."""
        errors = edit_log_errors(res, ['on', 'off'])
        if 'cropping to 655 rows' not in res.log:
            errors.append('threaded video did not crop the frame')
        late = between(res, 'on', 'off', delay=1.0)
        errors += pose_errors('the fallback frame through grip, move and '
                              'reset', [screen2(f) for f in late], S0, ID,
                              (1.6, 0.96))
        dots = [f for f in late if cursors(f)]
        if dots:
            errors.append('a dot on the fallback frame in %d of %d frames'
                          % (len(dots), len(late)))
        for f in late:
            frame = screen2(f)
            if frame and any(near3(q['pose'], frame['pose'][:2]
                                   + [frame['pose'][2] + 0.002])
                             for q in outlines(f)):
                errors.append('frame %d: an outline round the fallback '
                              'frame' % f['n'])
                break
            n = len(outlines(f)) + len(handles(f))
            if n != (2 if menu else 0):
                errors.append('frame %d: %d layers, want %s' % (
                    f['n'], n, 'the menu\'s outline and handle alone'
                    if menu else 'none'))
                break
        cfg = saved(res)
        got = [cfg.get('video_screen1_xr_' + k)
               for k in ('x', 'y', 'z', 'scale')]
        if [float(v or 'nan') for v in got] != [0.5, 0.0, 0.0, 1.0]:
            errors.append('screen 1 saved as x y z scale %s, want 0.5 0 0 1'
                          % got)
        return errors
    return check


def check_edit_no_map(res):
    """A core without a view map is screen 1: its whole frame is grabbed,
    stays where it was put, and is saved there."""
    errors = edit_log_errors(res, ['on', 'off'])
    before = last_before(res, 'grab', frame_quad)
    late = between(res, 'moved', None, frame_quad)
    if not before or not late:
        return errors + no_quads(res)
    want = ((0.3, 0.0, -1.8), ID)
    errors += pose_errors('the frame before the grab', [before], S0, ID)
    errors += pose_errors('the frame moved', [frame_quad(f) for f in late],
                          want[0], ID, before['size'])
    return errors + saved_errors(res, 'the frame', 'video_screen1_xr_', S0,
                                 want, before['size'],
                                 frame_quad(late[-1]))


# Follow Head: the hand's aims in VIEW, through a head turned 30
# degrees, as LOCAL points.
V_HEAD = 'head 0 0 0 30'
V_ANCHOR = ((0.0, 0.0, 0.0), from_euler(30.0, 0.0, 0.0))
A0 = ((0.2, -0.4, -0.3), (0.0, 0.0, -1.8))
A1 = ((0.5, -0.3, -0.3), (0.3, 0.1, -1.8))
A2 = ((0.9, -0.3, -0.3), (0.7, 0.1, -1.8))
MOVED = ((0.3, 0.1, -1.8), ID)


def turned(anchor, a, shift=(0.0, 0.0, 0.0)):
    """Aim a, given in anchor's frame, in LOCAL and moved by shift."""
    return tuple(tuple(x + s for x, s in zip(compose(anchor, (p, ID))[0],
                                             shift)) for p in a)


def check_edit_follow_head(res):
    """Follow Head: screen 1 grabbed and moved in VIEW space keeps that
    spot in view as the head turns, and is saved there."""
    errors = edit_log_errors(res, ['on'], exact=False)
    shown = [screen1_left(f) for f in
             between(res, 'moved', 'turned', edit_screen0, 1.5)]
    late = [screen1_left(f) for f in
            between(res, 'turned', None, edit_screen0, 1.0)]
    if any(q['space'] != 'view' for q in shown + late):
        errors.append('screen 1 out of view space')
    errors += pose_errors('screen 1 moved in VIEW', shown, MOVED[0], ID)
    errors += pose_errors('screen 1 with the head turned', late, MOVED[0],
                          ID)
    return errors + saved_errors(res, 'screen 1', 'video_screen1_xr_', S0,
                                 MOVED, (1.6, 0.96),
                                 shown[-1] if shown else None)


def yaw_of(q):
    return euler(q['pose'][3:])[0]


def check_edit_lazy(res):
    """Lazy Follow holds its glide while screen 1 is held, so screen 2,
    which nothing holds, stays; once it is let go they glide."""
    errors = edit_log_errors(res, ['on'], exact=False)
    held_frames = between(res, 'turned', 'released', screen2, 0.3)
    if not held_frames:
        return errors + ['no frames while held']
    errors += pose_errors('screen 1 held still',
                          [screen1_left(f) for f in held_frames], S0, ID)
    yaws = [yaw_of(screen2(f)) for f in held_frames]
    if any(abs(y) > 0.5 for y in yaws):
        errors.append('the screens glided to %.1f degrees while held'
                      % max(yaws, key=abs))
    late = between(res, 'released', None, screen2, 2.0)
    if not late or abs(yaw_of(screen2(late[-1]))) < 45.0:
        errors.append('no glide after the release: %s' % (
            late and round(yaw_of(screen2(late[-1])), 1)))
    return errors


L0 = ((-0.2, -0.4, -0.3), (0.0, 0.0, -1.8))
L1 = ((0.0, -0.4, -0.3), (0.2, 0.0, -1.8))
R1 = ((0.6, -0.4, -0.3), (0.4, 0.0, -1.8))
B0 = ((0.2, -0.6, -0.3), (0.0, -0.992, -1.8))
B1 = ((0.5, -0.6, -0.3), (0.3, -0.992, -1.8))
LEFT_AT = (0.2, 0.0, -1.8)
BOTTOM_AT = (0.3, -0.992, -1.8)


def check_edit_two_hands(res):
    """One item a hand: the right on the left's screen 1 does nothing,
    then takes screen 2 while the left still holds screen 1."""
    errors = edit_log_errors(res, ['on'], exact=False)
    errors += pose_errors('screen 1 held by the left hand alone',
                          [screen1_left(f) for f in
                           between(res, 'right_moved', 'bottom',
                                   edit_screen0, 1.5)], LEFT_AT, ID)
    errors += pose_errors('screen 2 before the right hand took it',
                          [screen2(f) for f in
                           between(res, 'right_moved', 'bottom',
                                   delay=1.5)], S1, ID)
    errors += pose_errors('screen 2 moved by the right hand',
                          [screen2(f) for f in
                           between(res, 'bottom_moved', delay=1.5)],
                          BOTTOM_AT, ID, (1.28, 0.96))
    errors += pose_errors('screen 1 while the right moved screen 2',
                          [screen1_left(f) for f in
                           between(res, 'bottom_moved', None, edit_screen0,
                                   1.5)], LEFT_AT, ID)
    errors += saved_errors(res, 'screen 1', 'video_screen1_xr_', S0,
                           (LEFT_AT, ID), (1.6, 0.96))
    return errors + saved_errors(res, 'screen 2', 'video_screen2_xr_', S1,
                                 (BOTTOM_AT, ID), (1.28, 0.96))


def check_edit_recenter(res):
    """A recenter during a grab lets go first, then recenters; edit mode
    stays on, and the hand moving on leaves screen 1."""
    errors = edit_log_errors(res, ['on'], exact=False)
    asked = res.log.find('Headset recenter requested')
    if asked < 0:
        errors.append('the recenter did not reach the hotkey')
    elif EDIT_OFF in res.log[:asked]:
        errors.append('edit mode stopped before the recenter')
    syncs = [(e['t_us'], e['sets']) for e in events(res, 'sync')]
    t = res.marks['recenter'] * 1e6
    then = [s for u, s in syncs if u <= t]
    if not then or then[-1] != EDIT_SETS \
            or any(s != EDIT_SETS for u, s in syncs if u > t):
        errors.append('edit mode\'s set not synced through the recenter')
    want = compose(V_ANCHOR, MOVED)
    errors += pose_errors('screen 1 recentered, let go',
                          [screen1_left(f) for f in
                           between(res, 'recenter', 'pulled', edit_screen0,
                                   1.5)], want[0], want[1])
    errors += pose_errors('screen 1 after the hand moved on',
                          [screen1_left(f) for f in
                           between(res, 'pulled', None, edit_screen0,
                                   1.5)], want[0], want[1])
    return errors + saved_errors(res, 'screen 1', 'video_screen1_xr_', S0,
                                 MOVED, (1.6, 0.96))


RC0 = turned(V_ANCHOR, A0)
RC1 = turned(V_ANCHOR, A0, (0.3, 0.0, 0.0))


def check_edit_recentered(res):
    """Grabbed from a turned anchor, screen 1 moves along the room's X,
    and its settings are saved in the anchor's frame."""
    errors = edit_log_errors(res, ['on'], exact=False)
    spot = compose(V_ANCHOR, (S0, ID))
    errors += pose_errors('screen 1 at the turned anchor',
                          [screen1_left(f) for f in
                           between(res, 'on', 'grab', edit_screen0, 1.0)],
                          spot[0], spot[1])
    want = (tuple(a + b for a, b in zip(spot[0], (0.3, 0.0, 0.0))),
            spot[1])
    shown = [screen1_left(f) for f in
             between(res, 'moved', None, edit_screen0, 1.5)]
    errors += pose_errors('screen 1 moved along the room\'s X', shown,
                          want[0], want[1], (1.6, 0.96))
    return errors + saved_errors(res, 'screen 1', 'video_screen1_xr_', S0,
                                 want, (1.6, 0.96),
                                 shown[-1] if shown else None, V_ANCHOR)


def check_edit_pause(res):
    """While Editing Screens: Pause stops the core while edit mode runs,
    started by the command and then by the headset's action, and lets
    it run again after each."""
    errors = edit_log_errors(res, ['on', 'off', 'on', 'off'])
    for on, off, end in (('on', 'off', 'on2'), ('on2', 'off2', None)):
        t_on, t_off = res.marks[on] * 1e6, res.marks[off] * 1e6
        t_end = res.marks[end] * 1e6 if end else float('inf')
        if not core_frames(res, t_on - 3e6, t_on):
            errors.append('the core did not run before %s' % on)
        mid = core_frames(res, t_on + 1e6, t_off)
        if mid:
            errors.append('the core ran while edit mode paused it (%s): %s'
                          % (on, mid))
        if not core_frames(res, t_off + 1e6, t_end):
            errors.append('the core did not run again after %s' % off)
    return errors


AIM_SCREEN0_RIGHT = 'aim right 0.2 -0.4 -0.3 0.4 0.24 -1.8'
AIM_SCREEN0_LEFT = 'aim left -0.2 -0.4 -0.3 -0.4 0.24 -1.8'
DOT_RIGHT, DOT_LEFT = (0.4, 0.24, -1.8), (-0.4, 0.24, -1.8)


def check_edit_command(menu=True):
    def check(res):
        """The command starts and stops edit mode: only its set synced
        between, a dot from each hand on screen 1, which Auto leaves
        alone, only while editing, the menu's quad kept up (gl has none)
        and the core kept running."""
        errors = edit_log_errors(res, ['on', 'off'])
        errors += edit_sync_errors(res, 'on', 'off')
        t_on, t_off = res.marks['on'] * 1e6, res.marks['off'] * 1e6
        pre = [f for f in res.frames
               if res.marks['aim'] * 1e6 + 3e5 < f['t_us'] < t_on]
        mid = [f for f in res.frames if t_on + 5e5 < f['t_us'] < t_off]
        post = [f for f in res.frames if f['t_us'] > t_off + 5e5]
        if not pre or not mid or not post:
            return errors + ['no headset frames around the marks']
        if any(cursors(f) for f in pre + post):
            errors.append('a dot showed on screen 1 outside edit mode')
        for want in (DOT_RIGHT, DOT_LEFT):
            if not any(at(c, want) for f in mid for c in cursors(f)):
                errors.append('no dot at %s while editing' % (want,))
        if any(menus(f) for f in res.frames
               if f['t_us'] < t_on or f['t_us'] > t_off + 1e6):
            errors.append('a menu quad outside edit mode')
        up = [f for f in mid if menus(f)]
        if menu and len(up) != len(mid):
            errors.append('the menu quad was up in %d of %d frames while '
                          'editing' % (len(up), len(mid)))
        elif not menu and up:
            errors.append('gl showed a menu quad while editing')
        if len(core_frames(res, t_on + 1e6, t_off)) < 2:
            errors.append('the core stopped while editing kept it running')
        return errors
    return check


def check_edit_action(game):
    def check(res):
        """The headset's Edit Screens action starts edit mode from the
        game's set and stops it from edit mode's own."""
        return (edit_log_errors(res, ['on', 'off'])
                + edit_sync_errors(res, 'on', 'off', game))
    return check


def check_edit_hotkeys(res):
    """With no command interface the hotkeys are read by their bits: the
    headset's Laser Pointer, Recenter and Edit Screens actions each set
    their own."""
    errors = check_laser_toggle('Auto', DOT_BOTTOM)(res)
    if res.log.count('Headset recenter requested') != 1:
        errors.append('Recenter reached the hotkey %d times, want once'
                      % res.log.count('Headset recenter requested'))
    return (errors + edit_log_errors(res, ['on', 'off'])
            + edit_sync_errors(res, 'edit_on', 'edit_off'))


def check_edit_menu_open(res):
    """Edit mode syncs only its own set with the pointer's, every profile
    takes its bindings, and opening the menu ends it and syncs the game's
    set again."""
    errors = bindings_errors(res) + edit_log_errors(res, ['on', 'off'])
    if 'Edit Screens bindings refused' in res.log:
        errors.append('a profile refused edit mode\'s bindings')
    return errors + edit_sync_errors(res, 'on', 'menu')


def check_edit_menu_ends(res):
    """Edit mode ends with what opened the menu or restarted video, and
    the game's set comes back."""
    return edit_log_errors(res, ['on', 'off']) + edit_sync_errors(
        res, 'on', 'menu')


def check_edit_no_content(res):
    """Without content the menu cannot close, so edit mode does not
    start, and it is not called unavailable."""
    errors = edit_log_errors(res, [])
    syncs = [e['sets'] for e in events(res, 'sync')]
    if EDIT_SETS in syncs:
        errors.append('the edit set was synced without content')
    if GAME_SETS not in syncs:
        errors.append('no session read the controllers')
    if 'Edit Screens unavailable' in res.log:
        errors.append('told unavailable, not that content is needed')
    return errors


# Screen 2 straight behind screen 1, hidden by it from the hand.
BURIED = {'video_screen2_xr_y': '0.992', 'video_screen2_xr_z': '-0.3'}
BURIED_MOVED = (0.6, 0.0, -2.1)
# Screen 2 pulled 30 cm nearer, its top edge 7 mm over the middle of
# the menu's handle bar, in front of it.
FRONT2 = (0.0, EDIT_HANDLE + 0.007 - 0.48, -1.5)


def edit_tap(action, aim):
    """action pressed once, the hand kept at aim."""
    return [('script', script(aim, action)), ('wait', 0.5),
            ('script', aim), ('wait', 0.5)]


def on_handle(dx=0.0, dy=0.0):
    """The right hand aiming straight ahead at the middle of the
    menu's bar, moved dx right and dy up."""
    return aim_line((dx, EDIT_HANDLE + dy, 0.0),
                    (dx, EDIT_HANDLE + dy, -1.8))


def check_edit_choose(menu=True):
    def check(res):
        """Select Next twice chooses screen 2, behind screen 1: its
        outline alone is bright while the laser is on screen 1; the grip
        takes it from there, it moves with the hand and is saved where
        the headset last showed it; screen 1 stays."""
        errors = edit_log_errors(res, ['on', 'off'])
        lit = between(res, 'chosen', 'grab', delay=0.5)
        items = 3 if menu else 2
        good = [f for f in lit if len(outlines(f)) == items
                and len(outlines(f, 1)) == 1
                and near3(outlines(f, 1)[0]['pose'], (0.0, 0.0, -2.098))]
        if not good:
            errors.append('screen 2 not lit alone while chosen: %s' % [
                [(q['rect'][0], [round(v, 3) for v in q['pose'][:3]])
                 for q in outlines(f)] for f in lit[-1:]])
        errors += pose_errors('screen 2 taken from behind screen 1',
                              [screen2(f) for f in
                               between(res, 'moved', 'released')],
                              BURIED_MOVED, ID, (1.28, 0.96))
        errors += pose_errors('screen 1 while screen 2 moved',
                              [screen1_left(f) for f in
                               between(res, 'grab', 'off', edit_screen0)],
                              S0, ID, (1.6, 0.96))
        errors += saved_errors(res, 'screen 2', 'video_screen2_xr_', S1,
                               (BURIED_MOVED, ID), (1.28, 0.96),
                               last_before(res, 'off', screen2))
        return errors
    return check


def check_edit_handle_first(res):
    """A grip aimed at the menu's bar, through screen 2 which is
    nearer, takes the menu: it moves with the hand and is saved placed;
    screen 2 stays."""
    errors = edit_log_errors(res, ['on', 'off'])
    errors += pose_errors('the menu taken by its handle',
                          [menu_quad(f) for f in
                           between(res, 'moved', 'released', delay=0.5)],
                          (0.3, 0.0, -1.7), ID, (MENU_W, MENU_H))
    errors += pose_errors('screen 2 in front of the bar',
                          [screen2(f) for f in
                           between(res, 'on', 'off', delay=0.5)],
                          FRONT2, ID, (1.28, 0.96))
    cfg = saved(res)
    if cfg.get('video_openxr_menu_placed') != 'true' \
            or abs(float(cfg.get('video_openxr_menu_xr_x', '0')) - 0.3) \
            > EDIT_POS:
        errors.append('the menu saved placed %s at x %s, want true at 0.3'
                      % (cfg.get('video_openxr_menu_placed'),
                         cfg.get('video_openxr_menu_xr_x')))
    return errors


# Screen 2 20 cm nearer than the menu, under its handle bar.
UNDER_BAR = dict(SAVE, video_screen2_xr_y='%g' % (FRONT2[1] - S1[1]),
                 video_screen2_xr_z='0.3')


def sticky_steps(first):
    """From first, the laser 3 cm under the bar's middle, in its margin
    over screen 2; the grip there carries what it takes 30 cm right."""
    return edit_start(first) + [
        ('script', on_handle(0.0, -0.03)), ('wait', 1), ('mark', 'grab'),
        ('script', script(on_handle(0.0, -0.03), EDIT_GRIP)), ('wait', 1),
        ('script', script(on_handle(0.3, -0.03), EDIT_GRIP)), ('wait', 1),
        ('mark', 'moved'), ('wait', 1), ('script', on_handle(0.3, -0.03)),
        ('mark', 'released'), ('wait', 1), ('send', EDIT), ('wait', 2),
        ('mark', 'off'), ('wait', 1)]


def check_edit_handle_sticky(stuck):
    def check(res):
        """The laser in the bar's margin over screen 2: come from the
        bar (stuck), the menu is the one lit and the grip takes it;
        come from screen 2, screen 2 is lit and taken and the menu
        stays."""
        errors = edit_log_errors(res, ['on', 'off'])
        want = outline_size(MENU_W, MENU_H) if stuck \
            else outline_size(1.28, 0.96)
        frames = between(res, 'on', 'grab', delay=2.5)
        if not frames:
            errors.append('no frame with the laser in the margin')
        for f in frames:
            got = [q['size'] for q in outlines(f, 1)]
            if len(got) != 1 or any(abs(a - b) > 0.002
                                    for a, b in zip(got[0], want)):
                errors.append('in the margin %s lit, want only %s'
                              % (got, want))
                break
        if stuck:
            errors += pose_errors('the menu taken by its stuck bar',
                                  [menu_quad(f) for f in
                                   between(res, 'moved', 'released',
                                           delay=0.5)],
                                  (0.3, 0.0, -1.7), ID, (MENU_W, MENU_H))
            errors += pose_errors('screen 2 under the margin',
                                  [screen2(f) for f in
                                   between(res, 'on', 'off', delay=0.5)],
                                  FRONT2, ID, (1.28, 0.96))
        else:
            errors += pose_errors('screen 2 taken through the margin',
                                  [screen2(f) for f in
                                   between(res, 'moved', 'released',
                                           delay=0.5)],
                                  (0.3, FRONT2[1], FRONT2[2]), ID,
                                  (1.28, 0.96))
            errors += pose_errors('the menu left in place',
                                  [menu_quad(f) for f in
                                   between(res, 'on', 'released', delay=0.5)],
                                  MENU_SPOT, ID, (MENU_W, MENU_H))
        return errors
    return check


# Screen 2 grabbed at its spot and held 0.7-2 cm off standing flush
# right of screen 1, level and in its plane. The buzz is
# INPUT_OPENXR_SNAP_TICK_AMP for INPUT_OPENXR_SNAP_TICK_NS; a hinge's,
# INPUT_OPENXR_HINGE_BUZZ_AMP for INPUT_OPENXR_HINGE_BUZZ_NS.
SNAP_GRAB = (0.0, -0.992, 0.0)
SNAP_HAND = (1.447, 0.02, 0.02)
SNAPPED = (1.44, 0.0, -1.8)
SNAP_TICK = (0.5, 20000000)
HINGE_BUZZ = (0.8, 150000000)


def buzzes(res, kind, start=None, end=None):
    """The right hand's buzzes of kind (SNAP_TICK or HINGE_BUZZ), after
    mark start and before mark end."""
    lo = res.marks[start] * 1e6 if start else float('-inf')
    hi = res.marks[end] * 1e6 if end else float('inf')
    return [e for e in haptics(res, 'right') if e['ev'] == 'haptic'
            and lo < e['t_us'] < hi
            and abs(e['amplitude'] - kind[0]) < 1e-3
            and e['duration_ns'] == kind[1]]


def straight(p):
    """The right hand at p aiming straight ahead."""
    return aim_line(p, (p[0], p[1], p[2] - 1.8))


def span(a0, a1, b0, b1):
    """The middle and length of where [a0, a1] and [b0, b1] overlap."""
    lo, hi = max(a0, b0), min(a1, b1)
    return (lo + hi) / 2.0, hi - lo


def hinge_line_errors(frames, want, axis):
    """frames each show one hinge line: the green strip a texel in from
    each end, HINGE_WIDTH wide, along world axis axis, each end
    HINGE_LIFT from where want(frame) puts the line (its middle and
    length, on the edges midway across the gap) toward the head the
    ribbon faces. The head, both ends and the ribbon's normal share a
    plane, so neither end moves out of it."""
    if not frames:
        return ['no frames to check for a hinge line']
    for f in frames:
        ls = hinge_lines(f)
        if len(ls) != 1:
            return ['frame %d: %d hinge lines, want 1' % (f['n'], len(ls))]
        line = ls[0]
        mid, length = want(f)
        turn = tuple(line['pose'][3:])
        along = qrot(turn, (1.0, 0.0, 0.0))
        side = qrot(turn, (0.0, 1.0, 0.0))
        facing = qrot(turn, (0.0, 0.0, 1.0))
        way = [0.0, 0.0, 0.0]
        way[axis] = 1.0 if along[axis] > 0.0 else -1.0
        lifts = []
        for s in (-1.0, 1.0):
            shown = [c + s * line['size'][0] / 2.0 * a
                     for c, a in zip(line['pose'][:3], along)]
            seam = [m + s * length / 2.0 * w for m, w in zip(mid, way)]
            lifts.append([a - b for a, b in zip(shown, seam)])
        if (tuple(line['rect']) != (HINGE_X + 1, HINGE_Y, HINGE_COLS - 2,
                                    HINGE_ROWS)
                or abs(line['size'][1] - HINGE_WIDTH) > 1e-4
                or abs(abs(along[axis]) - 1.0) > 1e-3
                or any(abs(math.sqrt(sum(v * v for v in d)) - HINGE_LIFT)
                       > HINGE_GAP_POS
                       or sum(v * w for v, w in zip(d, facing))
                       < 0.8 * HINGE_LIFT
                       or abs(sum(v * w for v, w in zip(d, side)))
                       > HINGE_GAP_POS for d in lifts)):
            return ['frame %d: the hinge line shows %s at %s, %s, along %s, '
                    'its ends moved %s; want %s, %.3f long, %.3f wide, '
                    'each end %.3f toward the head' % (
                        f['n'], line['rect'],
                        [round(v, 4) for v in line['pose'][:3]],
                        line['size'], [round(v, 3) for v in along],
                        [[round(v, 4) for v in d] for d in lifts],
                        [round(v, 4) for v in mid], length, HINGE_WIDTH,
                        HINGE_LIFT)]
    return []


def cut_outline_errors(frame, want):
    """frame's outlines all whole but those want lists, (the middle of
    the item it is round, its cut in CUTS): each the cut's whole cell,
    faint or bright; every outline its item's whole outline in pose and
    size."""
    cut = [o for o in outlines(frame) if outline_cut(o)]
    if len(cut) != len(want):
        return ['frame %d: %d outlines cut, want %d'
                % (frame['n'], len(cut), len(want))]
    errors = outline_pose_errors(frame)
    for mid, name in want:
        o = min(cut, key=lambda c: sum((a - b) ** 2 for a, b in
                                       zip(c['pose'][:3], mid)))
        cells = [CUT_X[k * len(CUTS) + CUTS.index(name)] for k in (0, 1)]
        if o['rect'][0] not in cells or tuple(o['rect'][1:]) != (
                0, OUTLINE_DIM, OUTLINE_DIM):
            errors.append('frame %d: the outline round %s shows %s; want '
                          'the %s cut\'s cell at %s' % (
                              frame['n'], [round(v, 3) for v in mid],
                              o['rect'], name, cells))
    return errors


def crop_outline_errors(frame, want):
    """Below the cut outlines' width: frame's outlines all whole but
    those want lists, (the middle of the item it is round, the texels
    its square starts in, turned a quarter for a top or bottom edge)."""
    cut = [o for o in outlines(frame) if outline_cut(o)]
    if len(cut) != len(want):
        return ['frame %d: %d outlines cut, want %d'
                % (frame['n'], len(cut), len(want))]
    errors = []
    for mid, off, turned in want:
        o = min(cut, key=lambda c: sum((a - b) ** 2 for a, b in
                                       zip(c['pose'][:3], mid)))
        base = OUTLINE_X[1] if o['rect'][0] >= OUTLINE_X[1] else OUTLINE_X[0]
        x = qrot(tuple(o['pose'][3:]), (1.0, 0.0, 0.0))
        if (o['rect'][0] - base != off
                or o['rect'][2] != OUTLINE_DIM - OUTLINE_CROP
                or (abs(x[0]) < 1e-3) != turned):
            errors.append('frame %d: the outline round %s shows %s, x %s; '
                          'want %d in, turned %s' % (
                              frame['n'], [round(v, 3) for v in mid],
                              o['rect'], [round(v, 3) for v in x], off,
                              turned))
    return errors


def hinge_texel_errors(res, f):
    """The hinge line's strip in f's image: green, red equal to blue,
    premultiplied at HINGE_LEVEL."""
    line = hinge_lines(f)[0]
    rgb, alpha = rect_texel(res, f, line, line['rect'][2] // 2,
                            HINGE_ROWS // 2)
    rb = (HINGE_RB * HINGE_LEVEL + 127) // 255
    g = (HINGE_G * HINGE_LEVEL + 127) // 255
    if (abs(alpha - HINGE_LEVEL) > 2 or abs(rgb[0] - rb) > 2
            or abs(rgb[1] - g) > 2 or rgb[0] != rgb[2]):
        return ['the hinge line shows %s alpha %d, want (%d, %d, %d) '
                'alpha %d' % (rgb, alpha, rb, g, rb, HINGE_LEVEL)]
    return []


def check_edit_snap(snap):
    def check(res):
        """Screen 2, taken at its spot 3.2 cm under screen 1, stays
        there until carried; held just off flush right of screen 1: with
        Snap to Screens it shows and is saved touching it, level, in
        its plane, the hinge marked by a green line and its rims gone
        while held and once let go there, and the hand gets the snap's
        buzz, the hinge's long buzz once; without, it stays where the
        hand put it, with no buzz."""
        want = SNAPPED if snap else (SNAP_HAND[0], SNAP_HAND[1],
                                     SNAP_HAND[2] - 1.8)
        errors = edit_log_errors(res, ['on', 'off'])
        errors += pose_errors('screen 2 just taken',
                              [screen2(f) for f in
                               between(res, 'grab', 'carry', delay=0.5)],
                              S1, ID, (1.28, 0.96))
        errors += pose_errors('screen 2 held',
                              [screen2(f) for f in
                               between(res, 'held', 'released', delay=0.5)],
                              want, ID, (1.28, 0.96))
        errors += pose_errors('screen 2 let go',
                              [screen2(f) for f in
                               between(res, 'released', 'off', delay=0.5)],
                              want, ID, (1.28, 0.96))
        errors += saved_errors(res, 'screen 2', 'video_screen2_xr_', S1,
                               (want, ID), (1.28, 0.96),
                               last_before(res, 'off', screen2))
        ticks = buzzes(res, SNAP_TICK, 'grab') + buzzes(res, HINGE_BUZZ,
                                                        'grab')
        if snap and not ticks:
            errors.append('no snap buzz on the right hand: %s'
                          % haptics(res, 'right'))
        if not snap and ticks:
            errors.append('a snap buzz with snapping off: %s' % ticks)
        hinged = buzzes(res, HINGE_BUZZ, 'grab')
        if snap and len(hinged) != 1:
            errors.append('%d hinge buzzes on the right hand, want 1 as it '
                          'hinged flush: %s' % (len(hinged), hinged))
        if snap:
            # At Hinge Gap 0 its spot, 3.2 cm under screen 1 and
            # centred, is a free gap the hinge holds exactly: shown from
            # edit mode's start.
            def spot(f):
                return (0.0, -0.496, -1.8), 1.28
            errors += hinge_line_errors(
                [f for f in between(res, 'on', 'grab', delay=0.5)
                 if screen2(f)], spot, 0)
            # The hand is 0.7 cm off, inside EDIT_POS.
            xs = [(what, q['pose'][0])
                  for a, b, what in (('held', 'released', 'held'),
                                     ('released', 'off', 'let go'))
                  for q in (screen2(f) for f in between(res, a, b,
                                                        delay=0.5))
                  if q]
            xs.append(('saved', S1[0] + float(
                saved(res).get('video_screen2_xr_x', '0'))))
            for what, x in xs:
                if abs(x - SNAPPED[0]) > HINGE_GAP_POS:
                    errors.append('screen 2 %s at x %.4f, want %.3f'
                                  % (what, x, SNAPPED[0]))
                    break
            # Flush right of screen 1 it hinges on that edge: the line
            # up the gap's middle where the edges overlap, both rims gone.
            held = [f for f in between(res, 'held', 'released', delay=0.5)
                    if screen2(f)]

            def flush(f):
                s = screen2(f)['pose']
                y, length = span(s[1] - 0.48, s[1] + 0.48, -0.48, 0.48)
                gap = s[0] - 0.64 - 0.8
                return (0.8 + gap / 2.0, y, -1.8), length
            errors += hinge_line_errors(held, flush, 1)
            for f in held[-1:]:
                errors += cut_outline_errors(f, [
                    (S0, 'right'),
                    (tuple(screen2(f)['pose'][:3]), 'left')])
            # Let go flush, it rests hinged: the line and the cut rims
            # stay.
            rest = [f for f in between(res, 'released', 'resting',
                                       delay=0.5) if screen2(f)]
            errors += hinge_line_errors(rest, flush, 1)
            for f in rest[-1:]:
                errors += cut_outline_errors(f, [
                    (S0, 'right'),
                    (tuple(screen2(f)['pose'][:3]), 'left')])
            if not rest:
                errors.append('no frame of screen 2 let go')
        else:
            for f in between(res, 'released', 'off', delay=0.5):
                if hinge_lines(f) or any(outline_cut(o)
                                         for o in outlines(f)):
                    errors.append('frame %d: a hinge line or cut outline '
                                  'with Snap to Screens off' % f['n'])
                    break
        return errors
    return check


def check_edit_snap_edge(res):
    """Screen 2 brought through the fade of its pull to screen 1's
    edge and rested at the end of its whole pull, swaying across it: one
    buzz as it takes hold and none while it sways; 7 cm off and back,
    one more; the hinge's long buzz once, on the way, the hinge holding
    from 7 cm off."""
    errors = edit_log_errors(res, ['on', 'off'])
    for a, b, what in (('edge', 'away', 'resting at the edge'),
                       ('away', 'back', 'back from 7 cm off')):
        ticks = [e for e in haptics(res, 'right') if e['ev'] == 'haptic'
                 and res.marks[a] * 1e6 < e['t_us'] < res.marks[b] * 1e6
                 and abs(e['amplitude'] - SNAP_TICK[0]) < 1e-3
                 and e['duration_ns'] == SNAP_TICK[1]]
        if len(ticks) != 1:
            errors.append('%d snap buzzes %s, want 1: %s'
                          % (len(ticks), what, ticks))
    hinged = buzzes(res, HINGE_BUZZ, 'grab', 'edge')
    later = buzzes(res, HINGE_BUZZ, 'edge', 'back')
    if len(hinged) != 1 or later:
        errors.append('%d hinge buzzes on the way to the edge and %d from '
                      'there, want 1 and 0' % (len(hinged), len(later)))
    return errors


def edit_start(first, wait=2):
    """Settle with the hand at first, then start edit mode."""
    return [('wait', 6), ('script', first), ('wait', 1), ('mark', 'on'),
            ('send', EDIT), ('wait', wait)]


def edit_press(action):
    return [('script', action), ('wait', 1), ('script', ''), ('wait', 2)]


def sweep(steps, dx, base=edit_aim):
    """The grip held while the hand moves dx more each 50 ms."""
    return [step for i in range(1, steps + 1) for step in (
        ('script', script(base(dx * i), EDIT_GRIP)), ('wait', 0.05))]


def edit_aim_near(d):
    """The right hand d right of and d nearer than edit_aim()'s, aiming
    straight ahead: what it holds comes nearer, its image larger."""
    return aim_line((d, 0.0, d), (d, 0.0, d - 1.8))


# Screen 2 taken at its spot and carried round to screen 1's right.
EDIT_SNAP_STEPS = edit_start(straight(SNAP_GRAB), 1) + [
    ('mark', 'grab'), ('script', script(straight(SNAP_GRAB), EDIT_GRIP)),
    ('wait', 1.5), ('mark', 'carry')] + [
    step for p in ((0.5, -0.6, 0.0), (1.0, -0.2, 0.0), SNAP_HAND)
    for step in (('script', script(straight(p), EDIT_GRIP)), ('wait', 0.3))
] + [('mark', 'held'), ('wait', 1.5), ('script', straight(SNAP_HAND)),
     ('mark', 'released'), ('wait', 1.5), ('mark', 'resting'),
     ('send', EDIT), ('wait', 2),
     ('mark', 'off'), ('wait', 1)]
# Screen 2 carried 7 cm off flush right of screen 1, then 1.5 cm off,
# in the edge's fade, then 0.9 cm off, inside its whole pull, swaying
# 2 mm across its end four times; then 7 cm off again and back.
EDGE_IN, EDGE_OUT, EDGE_AWAY = (1.449, 0.0, 0.0), (1.451, 0.0, 0.0), \
    (1.51, 0.0, 0.0)
EDGE_NEAR = (1.455, 0.0, 0.0)
EDIT_SNAP_EDGE_STEPS = edit_start(straight(SNAP_GRAB), 1) + [
    ('mark', 'grab'), ('script', script(straight(SNAP_GRAB), EDIT_GRIP)),
    ('wait', 1.5)] + [
    step for p in ((0.5, -0.6, 0.0), (1.0, -0.2, 0.0), EDGE_AWAY)
    for step in (('script', script(straight(p), EDIT_GRIP)), ('wait', 0.3))
] + [('wait', 0.5), ('mark', 'edge')] + [
    step for p in (EDGE_NEAR,) + (EDGE_IN, EDGE_OUT) * 4
    for step in (('script', script(straight(p), EDIT_GRIP)), ('wait', 0.25))
] + [('script', script(straight(EDGE_IN), EDIT_GRIP)), ('wait', 0.5),
     ('mark', 'away'), ('script', script(straight(EDGE_AWAY), EDIT_GRIP)),
     ('wait', 0.5), ('script', script(straight(EDGE_IN), EDIT_GRIP)),
     ('wait', 0.5), ('mark', 'back'), ('script', straight(EDGE_IN)),
     ('wait', 1), ('send', EDIT), ('wait', 2), ('mark', 'off'), ('wait', 1)]


def check_edit_turn_stick(res):
    """The left stick held right for half a second turns screen 1,
    which the right hand holds, anticlockwise seen from above about its
    own middle, about 45 degrees at 90 a second; saved as shown."""
    errors = edit_log_errors(res, ['on', 'off'])
    q = last_before(res, 'turned', screen1_left)
    if not q:
        return errors + ['no frame of screen 1 before the stick let go']
    yaw, pitch, roll = euler(tuple(q['pose'][3:]))
    if not 20.0 <= yaw <= 70.0 or abs(pitch) > 0.6 or abs(roll) > 0.6:
        errors.append('screen 1 turned to yaw %.1f pitch %.1f roll %.1f, '
                      'want 20 to 70, 0, 0' % (yaw, pitch, roll))
    if any(abs(a - b) > EDIT_POS for a, b in zip(q['pose'][:3], S0)):
        errors.append('screen 1 moved to %s as it turned, want %s'
                      % (q['pose'][:3], S0))
    return errors + saved_errors(res, 'screen 1', 'video_screen1_xr_', S0,
                                 (tuple(q['pose'][:3]),
                                  tuple(q['pose'][3:])), (1.6, 0.96),
                                 last_before(res, 'off', screen1_left))


def check_edit_tilt_stick(res):
    """The left stick held up for half a second tips screen 1, which the
    right hand holds, back about its own left-right line through its
    middle, about 45 degrees at 90 a second: its top edge goes away from
    the user and its bottom edge comes nearer; saved as shown."""
    errors = edit_log_errors(res, ['on', 'off'])
    q = last_before(res, 'tilted', screen1_left)
    if not q:
        return errors + ['no frame of screen 1 before the stick let go']
    rot = tuple(q['pose'][3:])
    yaw, pitch, roll = euler(rot)
    if not -70.0 <= pitch <= -20.0 or abs(yaw) > 0.6 or abs(roll) > 0.6:
        errors.append('screen 1 tilted to yaw %.1f pitch %.1f roll %.1f, '
                      'want 0, -20 to -70, 0' % (yaw, pitch, roll))
    if any(abs(a - b) > EDIT_POS for a, b in zip(q['pose'][:3], S0)):
        errors.append('screen 1 moved to %s as it tilted, want %s'
                      % (q['pose'][:3], S0))
    edge = math.sqrt(sum(v * v for v in S0) + 0.48 ** 2)
    top, bottom = (math.sqrt(sum((p + e) ** 2 for p, e in
                                 zip(q['pose'][:3], qrot(rot, (0.0, h, 0.0)))))
                   for h in (0.48, -0.48))
    if not top > edge + 0.1 or not bottom < edge - 0.1:
        errors.append('screen 1 top edge %.3f m and bottom edge %.3f m from '
                      'the user, want the top past %.3f and the bottom '
                      'inside it' % (top, bottom, edge))
    return errors + saved_errors(res, 'screen 1', 'video_screen1_xr_', S0,
                                 (tuple(q['pose'][:3]),
                                  tuple(q['pose'][3:])), (1.6, 0.96),
                                 last_before(res, 'off', screen1_left))


def check_edit_step_stick(res, step=15.0):
    """The left stick, held right at an eighth of full speed (11.25
    degrees a second) until a frame shows the step, turns screen 1,
    which the right hand holds and which faces the user, the head held
    still at the anchor, into the 15 degree step's hold, so it shows and
    is saved at exactly 15, and the right hand buzzes once, as it takes
    hold of the step. Taken at the 0 step, it does not buzz there. The
    step the stick ends in is step. Not a set time: stalled polls cut
    the stick's turn short."""
    errors = edit_log_errors(res, ['on', 'off'])
    q = last_before(res, 'turned', screen1_left)
    if not q:
        return errors + ['no frame of screen 1 before the stick let go']
    yaw, pitch, roll = euler(tuple(q['pose'][3:]))
    if abs(yaw - step) > 0.05 or abs(pitch) > 0.05 or abs(roll) > 0.05:
        errors.append('screen 1 turned to yaw %.3f pitch %.3f roll %.3f, '
                      'want %g, 0, 0' % (yaw, pitch, roll, step))
    if any(abs(a - b) > EDIT_POS for a, b in zip(q['pose'][:3], S0)):
        errors.append('screen 1 moved to %s as it turned, want %s'
                      % (q['pose'][:3], S0))
    ticks = [e for e in haptics(res, 'right') if e['ev'] == 'haptic'
             and res.marks['held'] * 1e6 < e['t_us']
             < res.marks['turned'] * 1e6
             and abs(e['amplitude'] - SNAP_TICK[0]) < 1e-3
             and e['duration_ns'] == SNAP_TICK[1]]
    if len(ticks) != 1:
        errors.append('%d step buzzes on the right hand, want 1: %s'
                      % (len(ticks), ticks))
    return errors + saved_errors(res, 'screen 1', 'video_screen1_xr_', S0,
                                 (tuple(q['pose'][:3]),
                                  tuple(q['pose'][3:])), (1.6, 0.96),
                                 last_before(res, 'off', screen1_left))


def check_edit_step_45(res):
    """With Snap to Angles at 45 degrees, the stick as in edit-step-stick
    turns screen 1 on past 15 and 30 with no hold there, then into the
    45 step's hold, so it shows and is saved at exactly 45, with one
    buzz."""
    errors = check_edit_step_stick(res, 45.0)
    yaws = [euler(tuple(q['pose'][3:]))[0] for q in
            map(screen1_left, between(res, 'held', 'turned', screen1_left))]
    for s in (15.0, 30.0):
        near = [y for y in yaws if abs(y - s) < 2.0]
        if not near or all(abs(y - s) < 0.05 for y in near):
            errors.append('screen 1 held at %g on its way to 45: %s'
                          % (s, ['%.3f' % y for y in near[:8]]))
    return errors


# Screen 2 at screen 1's right corner, 18 degrees short of square
# (yaw -72, facing left): chosen, gripped, and turned by the left stick
# at an eighth of full speed (11.25 degrees a second) until a frame
# shows it square, it reaches the 5 degree window and clicks square at
# the corner.
SQUARE = dict(SAVE, video_openxr_edit_snap='true',
              video_openxr_edit_hinge_gap='0',
              video_screen2_xr_x='0.8', video_screen2_xr_y='0.992',
              video_screen2_xr_z='0.64', video_screen2_xr_yaw='-72')
SQUARED = (0.8, 0.0, -1.16)
YAW_M90 = (0.0, -math.sqrt(0.5), 0.0, math.sqrt(0.5))

# Screen 2 swung back 16 degrees from square about screen 1's right
# edge, the corner's shared edge: its middle on the circle round it.
SWUNG_74 = (0.8 + 0.64 * math.cos(math.radians(74.0)), 0.0,
            -1.8 + 0.64 * math.sin(math.radians(74.0)))

# A hinge holds its gap exactly: 1 mm tells the Hinge Gaps apart.
HINGE_GAP_POS = 0.001

# Screen 2 raised 2.5 cm, its top edge 0.7 cm under screen 1's bottom
# edge, inside Hinge Gap 0's click, Snap to Angles at 30 degrees.
HINGE_3DS = dict(SAVE, video_openxr_edit_snap='true',
                 video_openxr_edit_hinge_gap='0',
                 video_openxr_edit_snap_angles='2',
                 video_screen2_xr_y='0.025')

# The default Hinge Gap, 2 cm: screen 2 raised 1.6 cm, its top edge
# 1.6 cm under screen 1's bottom edge, Snap to Angles at 30 degrees.
HINGE_GAP = dict(SAVE, video_openxr_edit_snap='true',
                 video_openxr_edit_snap_angles='2',
                 video_screen2_xr_y='0.016')


def quad_point(q, x, y):
    """Where the point x, y of quad q's own plane is."""
    qx, qy, qz, qw = q['pose'][3:]
    tx = 2.0 * (qy * 0.0 - qz * y)
    ty = 2.0 * (qz * x - qx * 0.0)
    tz = 2.0 * (qx * y - qy * x)
    return (q['pose'][0] + x + qw * tx + (qy * tz - qz * ty),
            q['pose'][1] + y + qw * ty + (qz * tx - qx * tz),
            q['pose'][2] + qw * tz + (qx * ty - qy * tx))


def screen2_at_pitch(deg):
    """A pick: screen 2 where it shows exactly deg from flat, either
    way."""
    def pick(frame):
        q = screen2(frame)
        if q and abs(abs(euler(tuple(q['pose'][3:]))[1]) - deg) <= 0.05:
            return q
        return None
    return pick


def check_edit_hinge_3ds(res, gap=0.0, narrow=False):
    """Screen 2, taken under screen 1 and carried 6 cm along its edge
    so its snaps are whole, hinges flat on screen 1's bottom edge; the
    left stick, held at an eighth of full speed until a frame shows it
    30 degrees from flat, swings it about that edge into the 30 step's
    hold: it shows and is saved there, its top edge Hinge Gap (gap)
    under screen 1's bottom edge, in its plane, and the right hand
    gets the hinge's long buzz once as it hinges and one tick for the
    step. While hinged a green line runs along the hinge midway across
    the gap where the edges overlap, screen 1's bottom rim and screen
    2's top rim gone; let go there, it rests hinged: both stay until
    edit mode ends. Taken again where it rests, it is hinged from its
    first frame: the line and the cut rims never go, it does not move,
    nothing buzzes. On every frame, as it hinges, each outline is its
    item's whole one in pose and size. Not a set
    time: stalled polls cut the swing short."""
    errors = edit_log_errors(res, ['on', 'off'])
    # Below the cut outlines' width, a cut turns and crops its outline.
    for f in [] if narrow else between(res, 'on', 'off'):
        bent = outline_pose_errors(f)
        if bent:
            errors += bent[:1]
            break

    def cuts(f):
        if narrow:
            return crop_outline_errors(f, [
                (S0, OUTLINE_CROP, True),
                (tuple(screen2(f)['pose'][:3]), 0, True)])
        return cut_outline_errors(f, [
            (S0, 'bottom'),
            (tuple(screen2(f)['pose'][:3]), 'top')])
    # Near the hinge but not held there (7 mm into Hinge Gap 0's click,
    # 1.6 cm against 2 cm), nothing shows until it is hinged.
    for f in between(res, 'on', 'grab', delay=0.5):
        if hinge_lines(f) or any(outline_cut(o)
                                 for o in outlines(f)):
            errors.append('frame %d: a hinge line or cut outline before '
                          'screen 2 was hinged' % f['n'])
            break
    q = last_before(res, 'swung', screen2)
    if not q:
        return errors + ['no frame of screen 2 before the stick let go']
    yaw, pitch, roll = euler(tuple(q['pose'][3:]))
    if abs(abs(pitch) - 30.0) > 0.05 or abs(yaw) > 0.05 or abs(roll) > 0.05:
        errors.append('screen 2 swung to yaw %.3f pitch %.3f roll %.3f, '
                      'want 0, +-30, 0' % (yaw, pitch, roll))
    edge = quad_point(q, 0.0, 0.48)
    if (     abs(edge[1] + 0.48 + gap) > HINGE_GAP_POS
            or abs(edge[2] + 1.8) > HINGE_GAP_POS):
        errors.append('screen 2 top edge at %s, want on y %.3f, z -1.8'
                      % ([round(v, 4) for v in edge], -0.48 - gap))
    ticks = [e for e in haptics(res, 'right') if e['ev'] == 'haptic'
             and res.marks['held'] * 1e6 < e['t_us']
             < res.marks['swung'] * 1e6
             and abs(e['amplitude'] - SNAP_TICK[0]) < 1e-3
             and e['duration_ns'] == SNAP_TICK[1]]
    if len(ticks) != 1:
        errors.append('%d buzzes on the right hand in the swing, want 1: %s'
                      % (len(ticks), ticks))
    held = [f for f in between(res, 'held', 'swung', delay=0.2)
            if screen2(f)]

    def hinge(f):
        x, length = span(screen2(f)['pose'][0] - 0.64,
                         screen2(f)['pose'][0] + 0.64, -0.8, 0.8)
        return (x, -0.48 - gap / 2.0, -1.8), length
    errors += hinge_line_errors(held, hinge, 0)
    for f in held[:1] + held[-1:]:
        errors += cuts(f)
    errors += outline_gap_errors(held[0]) if held else ['no hinged frame']
    snaps = [f for f in held if f['snap'] and hinge_lines(f)]
    if snaps:
        errors += hinge_texel_errors(res, snaps[-1])
        if narrow:
            w = read_png(image(res, snaps[-1], hinge_lines(snaps[-1])[0]))[0]
            if w != CURSOR_TAGGED_W:
                errors.append('the laser\'s image is %d wide, want %d'
                              % (w, CURSOR_TAGGED_W))
    elif gap == 0.0:
        errors.append('no image of a hinged frame')
    # Let go there, it rests hinged: the line and the cut rims stay,
    # with no frame between carried and resting.
    rest = [f for f in between(res, 'released', 'resting', delay=0.2)
            if screen2(f)]
    errors += hinge_line_errors(
        [f for f in between(res, 'held', 'resting') if screen2(f)], hinge, 0)
    for f in rest[:1] + rest[-1:]:
        errors += cuts(f)
    errors += outline_gap_errors(rest[-1]) if rest else \
        ['no frame of screen 2 let go']
    # Taken again where it rests, it is hinged from its first frame.
    again = [f for f in between(res, 'resting', 'dropped') if screen2(f)]
    errors += hinge_line_errors(again, hinge, 0)
    for f in again[:1] + again[-1:]:
        errors += cuts(f)
    if rest and again:
        at = tuple(screen2(rest[-1])['pose'][:3])
        moved = [f['n'] for f in again
                 if not near3(screen2(f)['pose'][:3], at, HINGE_GAP_POS)]
        if moved:
            errors.append('screen 2 moved taken again, frames %s' % moved)
    elif not again:
        errors.append('no frame of screen 2 taken again')
    taken = (buzzes(res, HINGE_BUZZ, 'resting', 'dropped')
             + buzzes(res, SNAP_TICK, 'resting', 'dropped'))
    if taken:
        errors.append('%d buzzes taking screen 2 again where it rests '
                      'hinged, want 0: %s' % (len(taken), taken))
    for f in between(res, 'off', delay=0.5):
        if hinge_lines(f):
            errors.append('frame %d: a hinge line after edit mode ended'
                          % f['n'])
            break
    hinged = buzzes(res, HINGE_BUZZ, 'on', 'held')
    swing = buzzes(res, HINGE_BUZZ, 'held', 'swung')
    if len(hinged) != 1 or swing:
        errors.append('%d hinge buzzes taking screen 2 to the hinge and %d '
                      'in the swing, want 1 and 0' % (len(hinged), len(swing)))
    return errors + saved_errors(res, 'screen 2', 'video_screen2_xr_', S1,
                                 (tuple(q['pose'][:3]),
                                  tuple(q['pose'][3:])), (1.28, 0.96),
                                 last_before(res, 'off', screen2))


def check_edit_hinge_push(res):
    """Screen 2 hinged under screen 1 at Hinge Gap 0 and pushed on 4.5
    cm past touching overlaps it with no hinge line; pulled back to 2 cm
    past it hinges again touching, with one long buzz; let go there it
    rests hinged, the line shown, and taken again it stays hinged with
    no buzz; saved touching. On every frame, as it comes off the hinge
    and as it hinges again, each outline is its item's whole one in
    pose and size."""
    errors = edit_log_errors(res, ['on', 'off'])
    for f in between(res, 'on', 'off'):
        bent = outline_pose_errors(f)
        if bent:
            errors += bent[:1]
            break
    touching = -0.96
    for a, b, want in (('pushed', 'pull', 0), ('back', 'released', 1),
                       ('released', 'resting', 1), ('regrab', 'dropped', 1)):
        frames = [f for f in between(res, a, b, delay=0.2) if screen2(f)]
        if not frames:
            errors.append('%s to %s: no frames of screen 2' % (a, b))
            continue
        for f in frames:
            n = len(hinge_lines(f))
            if n != want:
                errors.append('%s to %s, frame %d: %d hinge lines, want %d'
                              % (a, b, f['n'], n, want))
                break
        y = screen2(frames[-1])['pose'][1]
        if want and abs(y - touching) > 1e-4:
            errors.append('%s to %s: screen 2 at y %.4f, want %.4f '
                          'touching' % (a, b, y, touching))
        if not want and y < touching + 0.01:
            errors.append('%s to %s: screen 2 at y %.4f, want it 1 cm or '
                          'more into screen 1' % (a, b, y))
    back = buzzes(res, HINGE_BUZZ, 'pull', 'released')
    if len(back) != 1:
        errors.append('%d hinge buzzes pulled back, want 1: %s'
                      % (len(back), back))
    taken = buzzes(res, HINGE_BUZZ, 'resting', 'dropped')
    if taken:
        errors.append('hinge buzzes taken again, want none: %s' % taken)
    q = last_before(res, 'off', screen2)
    if not q or abs(q['pose'][1] - touching) > 1e-4:
        errors.append('screen 2 saved at %s, want y %.4f touching'
                      % (q and q['pose'][:3], touching))
    return errors


def check_edit_hinge_gap(res):
    """Screen 2 at the default Hinge Gap: as check_edit_hinge_3ds, its
    top edge 2 cm under screen 1's bottom edge."""
    return check_edit_hinge_3ds(res, 0.02)


def check_edit_hinge_3ds_narrow(res):
    """A runtime whose swapchains are 4096 wide: the laser's image has
    its tags but not the cut outlines. As check_edit_hinge_3ds, but each
    cut outline is turned a quarter for a top or bottom and cut to its
    crop."""
    return check_edit_hinge_3ds(res, narrow=True)


def check_edit_hinge_twitch(res, gap=0.02):
    """Screen 2 hinged flat at the default Hinge Gap and held still, let
    go as the aim turns half a degree up, and again half a degree down
    once taken where it rests: each time it rests at the gap where it
    showed before the grip opened, its hinge line on every frame after
    the release, and is saved there."""
    errors = edit_log_errors(res, ['on', 'off'])

    def hinge(f):
        x, length = span(screen2(f)['pose'][0] - 0.64,
                         screen2(f)['pose'][0] + 0.64, -0.8, 0.8)
        return (x, -0.48 - gap / 2.0, -1.8), length

    def at_gap(what, q):
        if not q:
            return ['%s: no frame of screen 2' % what]
        turn = euler(tuple(q['pose'][3:]))
        edge = quad_point(q, 0.0, 0.48)
        if (     any(abs(a) > 0.05 for a in turn)
                or abs(edge[1] + 0.48 + gap) > HINGE_GAP_POS
                or abs(edge[2] + 1.8) > HINGE_GAP_POS):
            return ['%s turned %s, top edge at %s, want flat on y %.3f, '
                    'z -1.8' % (what, [round(a, 3) for a in turn],
                                [round(v, 4) for v in edge], -0.48 - gap)]
        return []
    for rel, rest in (('released', 'resting'), ('released2', 'resting2')):
        frames = [f for f in between(res, rel, rest, delay=0.2)
                  if screen2(f)]
        errors += hinge_line_errors(frames, hinge, 0)
        for f in frames:
            e = at_gap('frame %d after %s' % (f['n'], rel), screen2(f))
            if e:
                errors += e
                break
    cfg = saved(res)
    v = [float(cfg.get('video_screen2_xr_' + k, '0'))
         for k in ('x', 'y', 'z', 'yaw', 'pitch', 'roll')]
    pose = (tuple(b + o for b, o in zip(S1, v[:3])), from_euler(*v[3:]))
    errors += at_gap('screen 2 saved',
                     {'pose': list(pose[0]) + list(pose[1])})
    return errors + saved_errors(res, 'screen 2', 'video_screen2_xr_', S1,
                                 pose, (1.28, 0.96),
                                 last_before(res, 'off', screen2))


def check_edit_hinge_shaken(res):
    """Screen 2 hinged at the default Hinge Gap, its aim shaken half a
    degree as the unit tests' t_shake shakes a hand, 2.8 cm past
    touching, where the stop's hold ends, and 9.8 cm apart, where the
    free gap's does: one hinge line on every frame from the hinge to
    the last shake, and one long buzz in all, as it took hold. Every
    frame from the grab's first held frame has the same layers and
    hinge slots, the held screen's slot hidden until the hinge shows;
    from the hinge on, the same outline rects too. On every frame, the
    hinge's first included, each outline is its item's whole one in
    pose and size."""
    errors = edit_log_errors(res, ['on', 'off'])
    frames = between(res, 'still', 'shaken2')
    if not frames:
        errors.append('no frames from the hinge to the last shake')
    off = [f['n'] for f in frames if len(hinge_lines(f)) != 1]
    if off:
        errors.append('%d of %d frames without one hinge line, from frame '
                      '%d' % (len(off), len(frames), off[0]))

    def counts(f):
        return len(quads(f)), len(hinge_slots(f))

    def rects(f):
        return [tuple(o['rect']) for o in outlines(f)]
    # The first held frame is the first with the held screen's slot,
    # hidden as the hinge has not shown yet.
    held = between(res, 'grab', 'shaken2')
    held = held[next((i for i, f in enumerate(held)
                      if len(hinge_slots(f)) > len(hinge_lines(f))),
                     len(held)):]
    if not held:
        errors.append('no frame from the grab to the last shake with the '
                      'held screen\'s hinge slot hidden')
    for f in between(res, 'grab', 'shaken2'):
        bent = outline_pose_errors(f)
        if bent:
            errors += bent[:1]
            break
    for what, fs, key in (('layers and hinge slots', held, counts),
                          ('outline rects', frames, rects)):
        moved = [f for f in fs if key(f) != key(fs[0])]
        if moved:
            errors.append('%d of %d frames with other %s than frame %d %s, '
                          'from frame %d %s'
                          % (len(moved), len(fs), what, fs[0]['n'],
                             key(fs[0]), moved[0]['n'], key(moved[0])))
    taken = buzzes(res, HINGE_BUZZ)
    if len(taken) != 1:
        errors.append('%d hinge buzzes, want 1' % len(taken))
    return errors


def check_edit_hinge_settle(res):
    """Screen 2 hinged flat at the default Hinge Gap and carried 4.9 cm
    under screen 1, where it shows hinged past the click's reach: let go
    there, it settles at the gap, its hinge line on every frame from
    the hold to the rest, and is saved there; taken again, nothing
    buzzes."""
    errors = edit_log_errors(res, ['on', 'off'])

    def line(gap):
        def hinge(f):
            x, length = span(screen2(f)['pose'][0] - 0.64,
                             screen2(f)['pose'][0] + 0.64, -0.8, 0.8)
            return (x, -0.48 - gap / 2.0, -1.8), length
        return hinge

    def at_gap(what, q, gap):
        if not q:
            return ['%s: no frame of screen 2' % what]
        turn = euler(tuple(q['pose'][3:]))
        edge = quad_point(q, 0.0, 0.48)
        if (     any(abs(a) > 0.05 for a in turn)
                or abs(edge[1] + 0.48 + gap) > HINGE_GAP_POS
                or abs(edge[2] + 1.8) > HINGE_GAP_POS):
            return ['%s turned %s, top edge at %s, want flat on y %.3f, '
                    'z -1.8' % (what, [round(a, 3) for a in turn],
                                [round(v, 4) for v in edge], -0.48 - gap)]
        return []
    for a, b, gap in (('held4', 'released', 0.049),
                      ('released', 'resting', 0.02)):
        frames = [f for f in between(res, a, b, delay=0.2) if screen2(f)]
        errors += hinge_line_errors(frames, line(gap), 0)
        for f in frames:
            e = at_gap('frame %d from %s' % (f['n'], a), screen2(f), gap)
            if e:
                errors += e
                break
    cfg = saved(res)
    v = [float(cfg.get('video_screen2_xr_' + k, '0'))
         for k in ('x', 'y', 'z', 'yaw', 'pitch', 'roll')]
    pose = (tuple(b + o for b, o in zip(S1, v[:3])), from_euler(*v[3:]))
    errors += at_gap('screen 2 saved',
                     {'pose': list(pose[0]) + list(pose[1])}, 0.02)
    taken = buzzes(res, HINGE_BUZZ, 'resting', 'dropped')
    if taken:
        errors.append('%d hinge buzzes taking screen 2 again where it '
                      'settled, want 0: %s' % (len(taken), taken))
    return errors + saved_errors(res, 'screen 2', 'video_screen2_xr_', S1,
                                 pose, (1.28, 0.96),
                                 last_before(res, 'off', screen2))


def check_edit_snap_square(res):
    """Screen 2, turned by the left stick from 18 degrees short of
    square at screen 1's right corner, shows and is saved exactly
    square there, and the right hand buzzes."""
    errors = edit_log_errors(res, ['on', 'off'])
    errors += pose_errors('screen 2 turned square',
                          [screen2(f) for f in
                           between(res, 'held', 'released', delay=0.2)],
                          SQUARED, YAW_M90, (1.28, 0.96))
    errors += saved_errors(res, 'screen 2', 'video_screen2_xr_', S1,
                           (SQUARED, YAW_M90), (1.28, 0.96),
                           last_before(res, 'off', screen2))
    ticks = buzzes(res, SNAP_TICK, 'grab') + buzzes(res, HINGE_BUZZ, 'grab')
    if not ticks:
        errors.append('no snap buzz on the right hand: %s'
                      % haptics(res, 'right'))
    hinged = buzzes(res, HINGE_BUZZ, 'grab')
    if len(hinged) != 1:
        errors.append('%d hinge buzzes on the right hand, want 1 as the '
                      'corner hinged: %s' % (len(hinged), hinged))
    return errors


def check_edit_snap_sticks(res):
    """Screen 2, turned by the left stick from 18 degrees short of
    square at screen 1's right corner until it shows square, stays
    square once the stick is let go; pushed back the other way at the
    same speed until it shows -74, it swings on from square about the
    corner's shared edge, not from where the stick stopped (Snap to
    Angles Off, so no step at 75 holds it), and is saved there. The
    head is held at the anchor. Not a set time: stalled polls cut the
    stick's turn short."""
    errors = edit_log_errors(res, ['on', 'off'])
    errors += pose_errors('screen 2 square, the stick let go',
                          [screen2(f) for f in
                           between(res, 'square', 'push', delay=0.2)],
                          SQUARED, YAW_M90, (1.28, 0.96))
    q = last_before(res, 'released', screen2)
    if not q:
        return errors + ['no frame of screen 2 before the grip let go']
    yaw = euler(tuple(q['pose'][3:]))[0]
    if abs(yaw + 74.0) > 1.5:
        errors.append('screen 2 pushed back to yaw %.2f, want -74, 16 on '
                      'from square' % yaw)
    errors += pose_errors('screen 2 swung back', [q], SWUNG_74)
    return errors + saved_errors(res, 'screen 2', 'video_screen2_xr_', S1,
                                 (tuple(q['pose'][:3]),
                                  tuple(q['pose'][3:])), (1.28, 0.96),
                                 last_before(res, 'off', screen2))


# The right hand holding screen 1 sways half a degree either side every
# 50 ms, a 10 Hz shake 1.6 cm either side at the screen, then stops for
# a tenth of a second and lets go, before a smoothed screen catches up.
SWAY = math.tan(math.radians(0.5)) * 1.8


def sway_aim(k):
    return aim_line((0.0, 0.0, 0.0), (SWAY if k % 2 else -SWAY, 0.0, -1.8))


EDIT_SWAY_STEPS = edit_start(edit_aim()) + [
    ('script', script(edit_aim(), EDIT_GRIP)), ('wait', 1),
    ('mark', 'sway')] + [
    step for k in range(24)
    for step in (('script', script(sway_aim(k), EDIT_GRIP)), ('wait', 0.05))
] + [('wait', 0.1), ('mark', 'still'), ('script', sway_aim(23)),
     ('mark', 'released'),
     ('wait', 1.5), ('send', EDIT), ('wait', 2), ('mark', 'off'),
     ('wait', 1)]


def check_edit_sway(smooth):
    def check(res):
        """Screen 1 held by a hand swaying half a degree either side at
        10 Hz: with Edit Smoothing at Medium it moves less than 60% of
        the sway's 3.1 cm, Off the whole sway. Let go before it catches
        up with the hand, it shows no step from where it was last shown,
        and is saved there. Frames further apart than two of the sway's
        are not compared: after a stall the smoothing catches up in one
        frame."""
        errors = edit_log_errors(res, ['on', 'off'])
        sway = between(res, 'sway', 'still', screen1_left, delay=0.3)
        xs = [screen1_left(f)['pose'][0] for f in sway]
        if len(xs) < 10:
            return errors + ['%d frames of the sway' % len(xs)]
        span = max(xs) - min(xs)
        if smooth and span > 0.6 * 2 * SWAY:
            errors.append('smoothed, screen 1 swayed %.4f m, want under '
                          '%.4f' % (span, 0.6 * 2 * SWAY))
        if not smooth and span < 0.9 * 2 * SWAY:
            errors.append('off, screen 1 swayed %.4f m of %.4f'
                          % (span, 2 * SWAY))
        gaps = sorted(b['t_us'] - a['t_us'] for a, b in zip(sway, sway[1:]))
        period = gaps[len(gaps) // 2]
        after = sway[-1:] + between(res, 'still', 'off', screen1_left)
        steps = [abs(screen1_left(b)['pose'][0] - screen1_left(a)['pose'][0])
                 for a, b in zip(after, after[1:])
                 if b['t_us'] - a['t_us'] <= 2 * period]
        if len(steps) < 20:
            errors.append('%d of %d pairs of frames from the let-go to the '
                          'end without a stall between them, want 20'
                          % (len(steps), len(after) - 1))
        elif max(steps) > 0.004:
            errors.append('screen 1 stepped %.4f m between frames as it '
                          'was let go' % max(steps))
        last = last_before(res, 'off', screen1_left)
        if not last:
            return errors + ['no frame of screen 1 before the end']
        return errors + saved_errors(res, 'screen 1', 'video_screen1_xr_',
                                     S0, (tuple(last['pose'][:3]),
                                          tuple(last['pose'][3:])),
                                     (1.6, 0.96), last)
    return check


TAG_LEFT, TAG_RIGHT = (-0.2, -0.3, -0.3), (0.2, -0.3, -0.3)


def both_aims():
    """The head at the origin; each hand below the eyes and to its side,
    aiming straight ahead at screen 1."""
    return script(HEAD_AT_ORIGIN,
                  aim_line(TAG_LEFT, (TAG_LEFT[0], TAG_LEFT[1], -1.8),
                           hand='left'),
                  aim_line(TAG_RIGHT, (TAG_RIGHT[0], TAG_RIGHT[1], -1.8)))


EDIT_TAG_STEPS = edit_start(both_aims()) + [
    ('wait', 1.5), ('mark', 'next'),
    ('script', script(both_aims(), 'action edit/select_next@right 1')),
    ('wait', 0.5), ('script', both_aims()), ('wait', 1),
    ('mark', 'previous'),
    ('script', script(both_aims(), 'action edit/select_previous@left 1')),
    ('wait', 0.5), ('script', both_aims()), ('wait', 1),
    ('mark', 'end'), ('send', EDIT), ('wait', 2), ('mark', 'off'),
    ('wait', 1)]
EDIT_TAG_NARROW_STEPS = edit_start(both_aims()) + [
    ('wait', 1.5), ('mark', 'end'), ('send', EDIT), ('wait', 2),
    ('mark', 'off'), ('wait', 1)]


def tag_errors(res, what, frames, k, hand):
    """Each frame's one tag shows cell k 5 cm over hand, facing the head
    at the origin; its cell has light texels and dark ones."""
    if not frames:
        return ['no frames %s' % what]
    want = (hand[0], hand[1] + TAG_LIFT, hand[2])
    face = unit(tuple(-a for a in want))
    for f in frames:
        t = tags(f)
        if len(t) != 1:
            return ['%s: %d tags' % (what, len(t))]
        q = t[0]
        z = qrot(tuple(q['pose'][3:]), (0.0, 0.0, 1.0))
        if q['rect'][0] != TAG_X[k] or q['rect'][1] != TAG_Y \
                or not near3(q['pose'], want, 0.002) \
                or abs(q['size'][1] - TAG_HEIGHT) > 1e-4 \
                or sum(a * b for a, b in zip(z, face)) < 0.999:
            return ['%s: tag at %s rect %s facing %s, want cell %d at %s '
                    'facing %s' % (what, q['pose'][:3], q['rect'], z, k,
                                   want, face)]
    snaps = [f for f in frames if f['snap']]
    if not snaps:
        return ['%s: no snapshot of the tag' % what]
    q = tags(snaps[-1])[0]
    w, h, bpp, rows = read_png(image(res, snaps[-1], q))
    light = dark = 0
    for y in range(q['rect'][1], q['rect'][1] + TAG_ROWS):
        for x in range(q['rect'][0], q['rect'][0] + TAG_COLS):
            px = rows[y][x * bpp:x * bpp + bpp]
            a = px[3] if bpp == 4 else 255
            if a >= TAG_FAINT - 8 and px[0] >= TAG_FAINT - 8:
                light += 1
            elif a >= TAG_FAINT - 8 and px[0] <= 8:
                dark += 1
    if not light or not dark:
        return ['%s: the tag\'s cell has %d light and %d dark texels'
                % (what, light, dark)]
    return []


def check_edit_tag_narrow(res):
    """A runtime whose swapchains are narrower than the laser's image
    with its tags: the image without them, as wide as before them; no
    tag, and each hand's beam and dot."""
    errors = edit_log_errors(res, ['on', 'off'])
    frames = between(res, 'on', 'end', delay=2.5)
    if not frames:
        return errors + ['no frames in edit mode']
    for f in frames:
        dots = cursors(f)
        late = [q for q in quads(f) if dots and q['sc'] == dots[0]['sc']
                and q['rect'][0] >= CURSOR_PLAIN_W]
        if late or len(beams(f)) != 2 or len(dots) != 2:
            return errors + ['%d tags, %d beams, %d dots'
                             % (len(late), len(beams(f)), len(dots))]
    snaps = [f for f in frames if f['snap']]
    if not snaps:
        return errors + ['no snapshot of the laser\'s image']
    w = read_png(image(res, snaps[-1], cursors(snaps[-1])[0]))[0]
    if w != CURSOR_PLAIN_W:
        errors.append('the laser\'s image is %d wide, want %d'
                      % (w, CURSOR_PLAIN_W))
    return errors


def check_edit_tag(res):
    """The tag starts over the left hand reading Laser; the right hand's
    Select Next moves it over the right reading Screen 1; the left's
    Select Previous moves it back. Light text with a dark halo."""
    errors = edit_log_errors(res, ['on', 'off'])
    errors += tag_errors(res, 'from the start',
                         between(res, 'on', 'next', delay=2.5),
                         0, TAG_LEFT)
    errors += tag_errors(res, 'after the right\'s Select Next',
                         between(res, 'next', 'previous', delay=0.5),
                         1, TAG_RIGHT)
    errors += tag_errors(res, 'after the left\'s Select Previous',
                         between(res, 'previous', 'end', delay=0.5),
                         0, TAG_LEFT)
    return errors


def check_edit_back(res):
    """From behind, each of screen 1's quads shows its back in its place:
    the laser's image's dark square, opaque, at its size, facing the
    head; every other item too, in no more layers. Its outline faces
    the head too, lit by the hand that points at the back."""
    errors = edit_log_errors(res, ['on', 'off'])
    front = between(res, 'front', 'turn', pick=edit_screen0, delay=0.5)
    behind = between(res, 'behind', 'end', pick=edit_screen0, delay=0.5)
    if not front or not behind:
        return errors + ['%d frames in front, %d behind'
                         % (len(front), len(behind))]
    want = edit_screen0(front[-1])
    dots = cursors(front[-1])
    items = [q for q in quads(front[-1]) if dots and q['sc'] != dots[0]['sc']]
    if len(want) != 2 or len(items) < 3 \
            or any(tuple(q['rect']) == BACK_RECT for q in items):
        return errors + ['in front: %s' % [q['rect'] for q in items]]
    for f in behind:
        qs = edit_screen0(f)
        backs = [q for q in quads(f) if tuple(q['rect']) == BACK_RECT
                 and not q['flags'] & BLEND]
        if len(quads(f)) != len(quads(front[-1])) or len(qs) != 2 \
                or len(backs) != len(items):
            return errors + ['from behind %d layers, %d backs; in front %d '
                             'layers, %d items'
                             % (len(quads(f)), len(backs),
                                len(quads(front[-1])), len(items))]
        for q, w in zip(qs, want):
            z = qrot(tuple(q['pose'][3:]), (0.0, 0.0, 1.0))
            if tuple(q['rect']) != BACK_RECT \
                    or not near3(q['pose'][:3], w['pose'][:3]) \
                    or not near(q['size'], w['size'], 1e-4) or z[2] > -0.999:
                return errors + ['screen 1 from behind: rect %s at %s, %s, '
                                 'facing %s; in front at %s, %s'
                                 % (q['rect'], q['pose'][:3], q['size'], z,
                                    w['pose'][:3], w['size'])]
        rims = [q for q in outlines(f) if near3(q['pose'], S0)]
        z = qrot(tuple(rims[0]['pose'][3:]), (0.0, 0.0, 1.0)) if rims \
            else (0.0, 0.0, 1.0)
        if len(rims) != 1 or rims[0]['rect'][0] != OUTLINE_X[1] \
                or z[2] > -0.999 or rims[0]['pose'][2] > S0[2]:
            return errors + ['screen 1\'s outline from behind: %s facing %s'
                             % ([(q['rect'][0], q['pose'][:3]) for q in rims],
                                z)]
    snaps = [f for f in behind if f['snap']]
    if not snaps:
        return errors + ['no snapshot from behind']
    w, h, bpp, rows = read_png(image(res, snaps[-1],
                                     edit_screen0(snaps[-1])[0]))
    x0, y0, bw, bh = BACK_RECT
    bad = 0
    for y in range(y0, y0 + bh):
        for x in range(x0, x0 + bw):
            px = rows[y][x * bpp:x * bpp + bpp]
            if any(abs(c - BACK_LEVEL) > 2 for c in px[:3]) \
                    or (bpp == 4 and px[3] != 255):
                bad += 1
    if bad:
        errors.append('%d texels of the back are not its dark grey: %s'
                      % (bad, list(rows[y0][x0 * bpp:x0 * bpp + bpp])))
    return errors


EDIT_BACK_HAND = (0.3, 0.0, -3.3)
EDIT_BACK_STEPS = edit_start(script(HEAD_AT_ORIGIN, edit_aim(0.3))) + [
    ('mark', 'front'), ('wait', 1.5), ('mark', 'turn'),
    ('script', script('head 0 0 -3.6 180',
                      aim_line(EDIT_BACK_HAND, (0.3, 0.0, -1.8)))),
    ('wait', 0.5), ('mark', 'behind'), ('wait', 1.5), ('mark', 'end'),
    ('send', EDIT), ('wait', 2), ('mark', 'off'), ('wait', 1)]


EDIT_GRAB_STEPS = edit_start(edit_aim(), 1) + [
    ('mark', 'aimed'), ('wait', 1), ('mark', 'grab'),
    ('script', script(edit_aim(), EDIT_GRIP)), ('wait', 1),
    ('script', script(edit_aim(0.3), EDIT_GRIP)), ('wait', 1),
    ('mark', 'moved'), ('wait', 1),
    ('script', edit_aim(0.3)), ('mark', 'released'), ('wait', 1),
    ('script', edit_aim(0.6)), ('wait', 1), ('mark', 'stop'),
    ('send', EDIT), ('wait', 2), ('mark', 'off'), ('wait', 1)]
EDIT_HOLD = edit_start(edit_aim()) + [
    ('mark', 'grab'), ('script', script(edit_aim(), EDIT_GRIP)),
    ('wait', 1), ('mark', 'sweep')]
EDIT_OFFSET = dict((k, v) for k, v in CUSTOM_OFFSET.items()
                   if k.startswith('video_screen1_'))
EDIT_CASES = [
    {'name': 'edit-grab', 'map': '3ds', 'settings': SAVE,
     'steps': EDIT_GRAB_STEPS, 'check': check_edit_grab()},
    {'name': 'edit-grab-threaded', 'map': '3ds',
     'settings': dict(SAVE, video_threaded='true'), 'steps': EDIT_GRAB_STEPS,
     'check': threaded(check_edit_grab())},
    {'name': 'edit-follow', 'map': '3ds', 'options': FPS10, 'settings': SAVE,
     'steps': EDIT_HOLD + sweep(20, 0.015, edit_aim_near)
     + [('mark', 'swept'), ('wait', 0.5), ('script', edit_aim_near(0.3)),
        ('mark', 'released'), ('wait', 2), ('send', EDIT), ('wait', 1)],
     'check': check_edit_follow},
    {'name': 'edit-release-threaded', 'map': '3ds',
     'settings': dict(SAVE, video_threaded='true'),
     'steps': EDIT_HOLD + sweep(10, 0.015)
     + [('mark', 'swept'), ('wait', 0.15), ('script', edit_aim(0.15)),
        ('mark', 'released'), ('wait', 1), ('script', edit_aim(0.6)),
        ('wait', 1.5)],
     'check': threaded(check_edit_release)},
    # The grip opens on a step that also moves the hand.
    {'name': 'edit-release-moving', 'map': '3ds', 'options': FPS10,
     'settings': SAVE,
     'steps': EDIT_HOLD + sweep(9, 0.03)
     + [('script', edit_aim(0.3)), ('mark', 'released'), ('wait', 2)],
     'check': check_edit_release_moving},
    {'name': 'edit-regrab-threaded', 'map': '3ds', 'options': FPS10,
     'settings': dict(SAVE, video_threaded='true'),
     'steps': EDIT_HOLD + sweep(10, 0.015)
     + [('wait', 0.25), ('script', edit_aim(0.15)), ('mark', 'released'),
        ('wait', 0.1), ('script', script(edit_aim(0.15), EDIT_GRIP)),
        ('wait', 1), ('script', script(edit_aim(0.25), EDIT_GRIP)),
        ('mark', 'moved_again'), ('wait', 1.5), ('script', edit_aim(0.25)),
        ('wait', 1.5)],
     'check': threaded(check_edit_regrab)},
    {'name': 'edit-handover-threaded', 'map': '3ds',
     'settings': dict(SAVE, video_threaded='true'),
     'steps': EDIT_HOLD + sweep(10, 0.015)
     + [('mark', 'swept'), ('wait', 0.15), ('script', edit_aim(0.15)),
        ('mark', 'released'), ('wait', 0.15),
        ('script', script(edit_aim(0.0, -0.992), EDIT_GRIP)), ('wait', 1),
        ('script', script(edit_aim(0.3, -0.992), EDIT_GRIP)),
        ('mark', 'b_moved'), ('wait', 1.5),
        ('script', edit_aim(0.3, -0.992)), ('wait', 1.5),
        ('mark', 'end')],
     'check': threaded(check_edit_handover)},
    {'name': 'edit-tilt', 'map': '3ds', 'settings': SAVE,
     'steps': edit_start(edit_aim(), 1) + [
         ('script', script(edit_aim(), EDIT_GRIP)), ('wait', 1),
         ('script', script(edit_aim(roll=30), EDIT_GRIP)), ('wait', 1),
         ('mark', 'rolled'),
         ('script', script(edit_aim(roll=30), EDIT_GRIP, EDIT_TILT)),
         ('wait', 1), ('mark', 'tilted'),
         ('script', script(edit_aim(roll=60), EDIT_GRIP, EDIT_TILT)),
         ('wait', 1), ('mark', 'turned'),
         ('script', script(edit_aim(roll=0), EDIT_GRIP)), ('wait', 1),
         ('mark', 'kept'), ('script', edit_aim()), ('wait', 1),
         ('send', EDIT), ('wait', 1)],
     'check': check_edit_tilt},
]
for _tilt in (False, True):
    _keys = [EDIT_TILT] if _tilt else []
    EDIT_CASES.append(
        {'name': 'edit-turn-tilt' if _tilt else 'edit-turn', 'map': '3ds',
         'settings': SAVE,
         'steps': edit_start(aim_line(*T0)) + [
             ('script', script(aim_line(*T0), EDIT_GRIP, *_keys)),
             ('wait', 1.5), ('mark', 'moved'),
             ('script', script(aim_line(*T1), EDIT_GRIP, *_keys)),
             ('wait', 2), ('script', aim_line(*T1)), ('wait', 1.5)],
         'check': check_edit_turn(_tilt)})
EDIT_STICK = script(aim_line(*A0), EDIT_GRIP)
EDIT_CASES += [
    {'name': 'edit-stick', 'map': '3ds', 'settings': SAVE,
     'env': {'RA_XR_LAYER_SNAP_EVERY': '0'},
     'steps': edit_start(aim_line(*A0), 1) + [
         ('script', EDIT_STICK), ('wait', 1), ('mark', 'held'),
         ('script', script(EDIT_STICK, 'action edit/stick@right 0 1')),
         ('wait', 0.5), ('script', EDIT_STICK), ('wait', 1),
         ('mark', 'pushed'),
         ('script', script(EDIT_STICK, 'action edit/stick@right 0.15 0.15')),
         ('wait', 1), ('script', EDIT_STICK), ('wait', 1),
         ('mark', 'deaded'),
         ('script', script(EDIT_STICK, 'action edit/stick@right 1 0')),
         ('wait', 0.5), ('script', EDIT_STICK), ('wait', 1),
         ('mark', 'grown'), ('script', aim_line(*A0)), ('wait', 1),
         ('mark', 'off'), ('send', EDIT), ('wait', 1)],
     'check': check_edit_stick},
    # Screen 1 from 0.25,0.1,-1.5, the menu following it until the reset.
    {'name': 'edit-reset', 'map': '3ds', 'settings': dict(SAVE,
                                                          **EDIT_OFFSET),
     'steps': edit_start(edit_aim(0.25, 0.1), 1) + [
         ('mark', 'offset'),
         ('script', script(edit_aim(0.25, 0.1), EDIT_GRIP)), ('wait', 1),
         ('script', script(edit_aim(0.55, 0.1), EDIT_GRIP)), ('wait', 1),
         ('script', edit_aim(0.55, 0.1)), ('wait', 0.5), ('mark', 'moved'),
         ('wait', 1.5), ('mark', 'reset'),
         ('script', script(edit_aim(0.55, 0.1), EDIT_RESET)),
         ('wait', 0.5), ('script', edit_aim(0.55, 0.1)), ('wait', 1.5),
         ('script', edit_aim(0.0, EDIT_HANDLE, z=-1.7)), ('wait', 1),
         ('mark', 'handle'),
         ('script', script(edit_aim(0.0, EDIT_HANDLE, z=-1.7), EDIT_GRIP)),
         ('wait', 1),
         ('script', script(edit_aim(0.5, EDIT_HANDLE, z=-1.7), EDIT_GRIP)),
         ('wait', 1), ('script', edit_aim(0.5, EDIT_HANDLE, z=-1.7)),
         ('mark', 'placed'), ('wait', 1.5), ('mark', 'back'),
         ('script', script(edit_aim(0.5, EDIT_HANDLE, z=-1.7), EDIT_RESET)),
         ('wait', 0.5), ('script', ''), ('wait', 1.5), ('mark', 'off'),
         ('send', EDIT), ('wait', 1)],
     'check': check_edit_reset()},
    {'name': 'edit-menu-front', 'map': '3ds',
     'settings': {'video_screen1_xr_z': '0.6', 'menu_driver': 'ozone'},
     'steps': [('wait', 6), ('send', 'MENU_TOGGLE'), ('wait', 4)],
     'check': check_edit_menu_front},
    {'name': 'edit-menu-size', 'map': '3ds',
     'settings': dict(SAVE, video_screen1_xr_z='0.9'),
     'steps': edit_start(edit_aim(0.0, F_HANDLE, z=-0.8)) + [
         ('mark', 'grab'),
         ('script', script(edit_aim(0.0, F_HANDLE, z=-0.8), EDIT_GRIP)),
         ('wait', 1.5), ('mark', 'moved'),
         ('script', script(edit_aim(0.3, F_HANDLE, z=-0.8), EDIT_GRIP)),
         ('wait', 2), ('script', edit_aim(0.3, F_HANDLE, z=-0.8)),
         ('wait', 1.5)],
     'check': check_edit_menu_size},
    {'name': 'edit-menu-reset', 'map': '3ds',
     'settings': dict(SAVE, **EDIT_OFFSET),
     'steps': edit_start(edit_aim(0.25, M_HANDLE, z=-1.4)) + [
         ('mark', 'grab'),
         ('script', script(edit_aim(0.25, M_HANDLE, z=-1.4), EDIT_GRIP)),
         ('wait', 1),
         ('script', script(edit_aim(0.55, M_HANDLE, z=-1.4), EDIT_GRIP)),
         ('wait', 1), ('script', edit_aim(0.55, M_HANDLE, z=-1.4)),
         ('mark', 'placed'), ('wait', 1.5), ('mark', 'back'),
         ('script', script(edit_aim(0.55, M_HANDLE, z=-1.4), EDIT_RESET)),
         ('wait', 0.5), ('script', ''), ('wait', 1.5), ('mark', 'off'),
         ('send', EDIT), ('wait', 1)],
     'check': check_edit_menu_reset},
    {'name': 'edit-outlines', 'map': '3ds',
     'env': {'RA_XR_LAYER_SNAP_EVERY': '10'},
     'steps': edit_start(edit_aim(), 1) + [
         ('mark', 'lit'), ('wait', 3),
         ('script', edit_aim(0.0, EDIT_HANDLE, z=-1.7)), ('mark', 'handle'),
         ('wait', 2), ('send', EDIT), ('mark', 'off'), ('wait', 2)],
     'check': check_edit_outlines()},
    {'name': 'edit-outlines-flush', 'map': '3ds', 'settings': FLUSH,
     'env': {'RA_XR_LAYER_SNAP_EVERY': '10'},
     'steps': edit_start(edit_aim(), 1) + [
         ('mark', 'lit'), ('wait', 3), ('send', EDIT), ('mark', 'off'),
         ('wait', 2)],
     'check': check_edit_outlines_flush},
    {'name': 'edit-beam', 'map': '3ds',
     'settings': {'video_openxr_laser': '1'},
     'env': {'RA_XR_LAYER_SNAP_EVERY': '10'},
     'steps': [('wait', 6), ('script', script(HEAD_AT_ORIGIN, AIM_GUN)),
               ('wait', 1), ('mark', 'before'), ('wait', 2), ('mark', 'on'),
               ('send', EDIT), ('wait', 1), ('mark', 'aimed'), ('wait', 2),
               ('mark', 'miss'), ('script', script(HEAD_AT_ORIGIN, AIM_MISS)),
               ('wait', 2), ('mark', 'off'), ('send', EDIT), ('wait', 2)],
     'check': check_edit_beam},
    {'name': 'edit-beam-off', 'map': '3ds',
     'settings': {'video_openxr_laser_beam': '0'},
     'steps': [('wait', 6), ('script', script(HEAD_AT_ORIGIN, AIM_GUN)),
               ('wait', 1), ('mark', 'on'), ('send', EDIT), ('wait', 1),
               ('mark', 'aimed'), ('wait', 2), ('mark', 'off'),
               ('send', EDIT), ('wait', 1)],
     'check': check_edit_beam_off},
    {'name': 'laser-beam-always', 'map': '3ds',
     'settings': {'video_openxr_laser': '1', 'video_openxr_laser_beam': '2'},
     'steps': [('wait', 6), ('script', script(HEAD_AT_ORIGIN, AIM_GUN)),
               ('wait', 1), ('mark', 'aimed'), ('wait', 2), ('mark', 'miss'),
               ('script', script(HEAD_AT_ORIGIN, AIM_MISS)), ('wait', 2),
               ('mark', 'off'), ('send', 'LASER_POINTER_TOGGLE'),
               ('wait', 2), ('mark', 'on'),
               ('send', 'LASER_POINTER_TOGGLE'), ('wait', 2)],
     'check': check_laser_beam_always},
    {'name': 'edit-fallback', 'map': 'crop',
     'settings': dict(SAVE, video_threaded='true', video_screen1_xr_x='0.5'),
     'steps': edit_start(edit_aim()) + [
         ('script', script(edit_aim(), EDIT_GRIP)), ('wait', 1.5),
         ('script', script(edit_aim(0.3), EDIT_GRIP)), ('wait', 2),
         ('script', edit_aim()), ('wait', 1),
         ('script', script(edit_aim(), EDIT_RESET)), ('wait', 0.5),
         ('script', edit_aim()), ('wait', 1.5), ('mark', 'off'),
         ('send', EDIT), ('wait', 1)],
     'check': threaded(check_edit_fallback())},
    {'name': 'edit-no-map', 'map': 'none', 'settings': SAVE,
     'steps': edit_start(edit_aim(), 1) + [
         ('mark', 'grab'), ('script', script(edit_aim(), EDIT_GRIP)),
         ('wait', 1), ('script', script(edit_aim(0.3), EDIT_GRIP)),
         ('wait', 1), ('script', edit_aim(0.3)), ('wait', 1),
         ('mark', 'moved'), ('send', EDIT), ('wait', 2)],
     'check': check_edit_no_map},
    {'name': 'edit-follow-head', 'map': '3ds',
     'settings': dict(SAVE, video_openxr_placement='1'), 'script': V_HEAD,
     'steps': edit_start(script(V_HEAD, aim_line(*turned(V_ANCHOR, A0))))
     + [('script', script(V_HEAD, aim_line(*turned(V_ANCHOR, A0)),
                          EDIT_GRIP)), ('wait', 1.5), ('mark', 'moved'),
        ('script', script(V_HEAD, aim_line(*turned(V_ANCHOR, A1)),
                          EDIT_GRIP)), ('wait', 2),
        ('script', script(V_HEAD, aim_line(*turned(V_ANCHOR, A1)))),
        ('wait', 1.5), ('mark', 'turned'), ('script', HEAD_TURNED),
        ('wait', 2)],
     'check': check_edit_follow_head},
    {'name': 'edit-lazy', 'map': '3ds',
     'settings': {'video_openxr_placement': '2'}, 'script': 'head 0 0 0 0',
     'steps': edit_start(edit_aim()) + [
         ('script', script(edit_aim(), EDIT_GRIP)), ('wait', 1),
         ('script', script('head 0 0 0 90', edit_aim(), EDIT_GRIP)),
         ('mark', 'turned'), ('wait', 2.5), ('script', 'head 0 0 0 90'),
         ('mark', 'released'), ('wait', 3)],
     'check': check_edit_lazy},
    {'name': 'edit-two-hands', 'map': '3ds', 'settings': SAVE,
     'steps': edit_start(script(aim_line(*L0, hand='left'),
                                aim_line(*A0))) + [
         ('script', script(aim_line(*L0, hand='left'), aim_line(*A0),
                           EDIT_GRIP_LEFT)), ('wait', 1.5),
         ('script', script(aim_line(*L0, hand='left'), aim_line(*A0),
                           EDIT_GRIP_LEFT, EDIT_GRIP)), ('wait', 1.5),
         ('script', script(aim_line(*L1, hand='left'), aim_line(*A0),
                           EDIT_GRIP_LEFT, EDIT_GRIP)), ('wait', 1.5),
         ('mark', 'right_moved'),
         ('script', script(aim_line(*L1, hand='left'), aim_line(*R1),
                           EDIT_GRIP_LEFT, EDIT_GRIP)), ('wait', 2),
         ('mark', 'bottom'),
         ('script', script(aim_line(*L1, hand='left'), aim_line(*B0),
                           EDIT_GRIP_LEFT)), ('wait', 1),
         ('script', script(aim_line(*L1, hand='left'), aim_line(*B0),
                           EDIT_GRIP_LEFT, EDIT_GRIP)), ('wait', 1.5),
         ('mark', 'bottom_moved'),
         ('script', script(aim_line(*L1, hand='left'), aim_line(*B1),
                           EDIT_GRIP_LEFT, EDIT_GRIP)), ('wait', 2),
         ('script', script(aim_line(*L1, hand='left'), aim_line(*B1))),
         ('wait', 1.5)],
     'check': check_edit_two_hands},
    {'name': 'edit-recenter', 'map': '3ds', 'settings': SAVE,
     'steps': edit_start(aim_line(*A0)) + [
         ('script', script(aim_line(*A0), EDIT_GRIP)), ('wait', 1.5),
         ('script', script(aim_line(*A1), EDIT_GRIP)), ('wait', 2),
         ('script', script(V_HEAD, aim_line(*A1), EDIT_GRIP)), ('wait', 1),
         ('mark', 'recenter'), ('send', 'HEADSET_RECENTER'), ('wait', 2),
         ('mark', 'pulled'),
         ('script', script(V_HEAD, aim_line(*A2), EDIT_GRIP)), ('wait', 2)],
     'check': check_edit_recenter},
    {'name': 'edit-recentered', 'map': '3ds', 'settings': SAVE,
     'steps': [('wait', 6), ('script', V_HEAD), ('wait', 1),
               ('send', 'HEADSET_RECENTER'), ('wait', 2),
               ('script', script(V_HEAD, aim_line(*RC0))), ('wait', 1),
               ('mark', 'on'), ('send', EDIT), ('wait', 2), ('mark', 'grab'),
               ('script', script(V_HEAD, aim_line(*RC0), EDIT_GRIP)),
               ('wait', 1.5), ('mark', 'moved'),
               ('script', script(V_HEAD, aim_line(*RC1), EDIT_GRIP)),
               ('wait', 2), ('script', script(V_HEAD, aim_line(*RC1))),
               ('wait', 1.5)],
     'check': check_edit_recentered},
    # Started by the command, then by the headset's action, which raises
    # it: each pause lands between frames.
    {'name': 'edit-pause', 'map': '3ds', 'options': FPS16,
     'settings': {'video_openxr_edit_pause': 'true'},
     'steps': [('wait', 6), ('mark', 'on'), ('send', EDIT), ('wait', 4),
               ('mark', 'off'), ('send', EDIT), ('wait', 4), ('mark', 'on2'),
               ('script', 'action combined/edit_screens 1'), ('wait', 1),
               ('script', ''), ('wait', 3), ('mark', 'off2'),
               ('script', 'action edit/edit_screens 1'), ('wait', 1),
               ('script', ''), ('wait', 4)],
     'check': check_edit_pause},
    {'name': 'edit-command', 'map': '3ds', 'options': FPS16,
     'steps': [('wait', 6), ('mark', 'aim'),
               ('script', script(AIM_SCREEN0_RIGHT, AIM_SCREEN0_LEFT)),
               ('wait', 2), ('mark', 'on'), ('send', EDIT), ('wait', 4),
               ('mark', 'off'), ('send', EDIT), ('wait', 3)],
     'check': check_edit_command()},
    {'name': 'edit-action', 'map': '3ds',
     'steps': [('wait', 6), ('mark', 'on')]
     + edit_press('action combined/edit_screens 1') + [('mark', 'off')]
     + edit_press('action edit/edit_screens 1'),
     'check': check_edit_action(GAME_SETS)},
    {'name': 'edit-action-separate', 'map': '3ds',
     'settings': {'video_openxr_controllers': '1'},
     'steps': [('wait', 6), ('mark', 'on')]
     + edit_press('action separate/edit_screens 1') + [('mark', 'off')]
     + edit_press('action edit/edit_screens 1'),
     'check': check_edit_action(['separate', 'pointer'])},
    # No command interface, so the hotkeys are read by their bits, and
    # RetroArch is stopped, not told to quit.
    {'name': 'edit-hotkeys-nocmd', 'map': '3ds',
     'settings': {'network_cmd_enable': 'false'},
     'steps': laser_action_steps('action combined/laser 1') + [
         ('script', 'action combined/recenter 1'), ('wait', 1),
         ('script', ''), ('wait', 1), ('mark', 'edit_on')]
     + edit_press('action combined/edit_screens 1') + [('mark', 'edit_off')]
     + edit_press('action edit/edit_screens 1'),
     'check': check_edit_hotkeys},
    {'name': 'edit-menu-open', 'map': '3ds',
     'settings': {'menu_driver': 'ozone'},
     'steps': [('wait', 6), ('mark', 'on'), ('send', EDIT), ('wait', 3),
               ('mark', 'menu'), ('send', 'MENU_TOGGLE'), ('wait', 3)],
     'check': check_edit_menu_open},
    {'name': 'edit-menu-action', 'map': '3ds',
     'settings': {'menu_driver': 'ozone'},
     'steps': [('wait', 6), ('mark', 'on'), ('send', EDIT), ('wait', 3),
               ('mark', 'menu')] + edit_press('action edit/menu 1'),
     'check': check_edit_menu_ends},
    # The fullscreen hotkey restarts the driver: edit mode ends with it.
    {'name': 'edit-reinit', 'map': '3ds',
     'steps': [('wait', 6), ('mark', 'on'), ('send', EDIT), ('wait', 3),
               ('mark', 'menu'), ('send', 'FULLSCREEN_TOGGLE'), ('wait', 6)],
     'check': check_edit_menu_ends},
    # From the open menu: it closes, and edit mode starts.
    {'name': 'edit-from-menu', 'map': '3ds',
     'settings': {'menu_driver': 'ozone'},
     'steps': [('wait', 6), ('send', 'MENU_TOGGLE'), ('wait', 3),
               ('mark', 'on'), ('send', EDIT), ('wait', 3), ('mark', 'off'),
               ('send', EDIT), ('wait', 3)],
     'check': check_edit_action(GAME_SETS)},
    {'name': 'edit-no-content', 'map': '3ds',
     'core': '/nonexistent/no_core_libretro.so',
     'steps': [('wait', 8), ('send', EDIT), ('wait', 3)],
     'check': check_edit_no_content},
    # Two presses of Select Next choose screen 2, buried behind screen 1.
    {'name': 'edit-choose', 'map': '3ds', 'settings': dict(SAVE, **BURIED),
     'steps': edit_start(edit_aim()) + edit_tap(EDIT_NEXT, edit_aim())
     + edit_tap(EDIT_NEXT, edit_aim()) + [
         ('mark', 'chosen'), ('wait', 1.5), ('mark', 'grab'),
         ('script', script(edit_aim(), EDIT_GRIP)), ('wait', 1),
         ('script', script(edit_aim(0.6), EDIT_GRIP)), ('wait', 1),
         ('mark', 'moved'), ('wait', 1), ('script', edit_aim(0.6)),
         ('mark', 'released'), ('wait', 1), ('send', EDIT), ('wait', 2),
         ('mark', 'off'), ('wait', 1)],
     'check': check_edit_choose()},
    {'name': 'edit-handle-first', 'map': '3ds', 'settings': UNDER_BAR,
     'steps': edit_start(on_handle()) + [
         ('mark', 'grab'), ('script', script(on_handle(), EDIT_GRIP)),
         ('wait', 1), ('script', script(on_handle(0.3), EDIT_GRIP)),
         ('wait', 1), ('mark', 'moved'), ('wait', 1),
         ('script', on_handle(0.3)), ('mark', 'released'), ('wait', 1),
         ('send', EDIT), ('wait', 2), ('mark', 'off'), ('wait', 1)],
     'check': check_edit_handle_first},
    {'name': 'edit-handle-sticky', 'map': '3ds', 'settings': UNDER_BAR,
     'steps': sticky_steps(on_handle()),
     'check': check_edit_handle_sticky(True)},
    {'name': 'edit-handle-margin', 'map': '3ds', 'settings': UNDER_BAR,
     'steps': sticky_steps(on_handle(0.0, -0.07)),
     'check': check_edit_handle_sticky(False)},
    {'name': 'edit-snap', 'map': '3ds',
     'settings': dict(SAVE, video_openxr_edit_snap='true',
                      video_openxr_edit_hinge_gap='0'),
     'steps': EDIT_SNAP_STEPS, 'check': check_edit_snap(True)},
    {'name': 'edit-snap-off', 'map': '3ds', 'settings': SAVE,
     'steps': EDIT_SNAP_STEPS, 'check': check_edit_snap(False)},
    {'name': 'edit-snap-edge', 'map': '3ds',
     'settings': dict(SAVE, video_openxr_edit_snap='true',
                      video_openxr_edit_hinge_gap='0'),
     'steps': EDIT_SNAP_EDGE_STEPS, 'check': check_edit_snap_edge},
    {'name': 'edit-turn-stick', 'map': '3ds', 'settings': SAVE,
     'env': {'RA_XR_LAYER_SNAP_EVERY': '0'},
     'steps': edit_start(edit_aim()) + [
         ('script', script(edit_aim(), EDIT_GRIP)), ('wait', 1),
         ('mark', 'held'),
         ('script', script(edit_aim(), EDIT_GRIP,
                           'action edit/stick@left 1 0')),
         ('wait', 0.5), ('script', script(edit_aim(), EDIT_GRIP)),
         ('wait', 1), ('mark', 'turned'), ('script', edit_aim()),
         ('wait', 1), ('send', EDIT), ('wait', 2), ('mark', 'off'),
         ('wait', 1)],
     'check': check_edit_turn_stick},
    {'name': 'edit-tilt-stick', 'map': '3ds', 'settings': SAVE,
     'env': {'RA_XR_LAYER_SNAP_EVERY': '0'},
     'steps': edit_start(edit_aim()) + [
         ('script', script(edit_aim(), EDIT_GRIP)), ('wait', 1),
         ('mark', 'held'),
         ('script', script(edit_aim(), EDIT_GRIP,
                           'action edit/stick@left 0 1')),
         ('wait', 0.5), ('script', script(edit_aim(), EDIT_GRIP)),
         ('wait', 1), ('mark', 'tilted'), ('script', edit_aim()),
         ('wait', 1), ('send', EDIT), ('wait', 2), ('mark', 'off'),
         ('wait', 1)],
     'check': check_edit_tilt_stick},
    {'name': 'edit-step-stick', 'map': '3ds', 'settings': SAVE,
     'script': HEAD_AT_ORIGIN,
     'steps': edit_start(edit_aim()) + [
         ('script', script(edit_aim(), EDIT_GRIP)), ('wait', 1),
         ('mark', 'held'),
         ('script', script(edit_aim(), EDIT_GRIP,
                           'action edit/stick@left 0.3 0')),
         ('until', ('screen 1 at 15', screen1_at_yaw(15.0), 10)),
         ('script', script(edit_aim(), EDIT_GRIP)),
         ('wait', 1), ('mark', 'turned'), ('script', edit_aim()),
         ('wait', 1), ('send', EDIT), ('wait', 2), ('mark', 'off'),
         ('wait', 1)],
     'check': check_edit_step_stick},
    {'name': 'edit-step-45', 'map': '3ds',
     'settings': dict(SAVE, video_openxr_edit_snap_angles='3'),
     'script': HEAD_AT_ORIGIN,
     'steps': edit_start(edit_aim()) + [
         ('script', script(edit_aim(), EDIT_GRIP)), ('wait', 1),
         ('mark', 'held'),
         ('script', script(edit_aim(), EDIT_GRIP,
                           'action edit/stick@left 0.3 0')),
         ('until', ('screen 1 at 45', screen1_at_yaw(45.0), 10)),
         ('script', script(edit_aim(), EDIT_GRIP)),
         ('wait', 1), ('mark', 'turned'), ('script', edit_aim()),
         ('wait', 1), ('send', EDIT), ('wait', 2), ('mark', 'off'),
         ('wait', 1)],
     'check': check_edit_step_45},
    {'name': 'edit-snap-square', 'map': '3ds', 'settings': SQUARE,
     'steps': edit_start(edit_aim()) + edit_tap(EDIT_NEXT, edit_aim())
     + edit_tap(EDIT_NEXT, edit_aim()) + [
         ('mark', 'grab'), ('script', script(edit_aim(), EDIT_GRIP)),
         ('wait', 1),
         ('script', script(edit_aim(), EDIT_GRIP,
                           'action edit/stick@left -0.3 0')),
         ('until', ('screen 2 square', screen2_at_yaw(-90.0), 10)),
         ('script', script(edit_aim(), EDIT_GRIP)),
         ('wait', 1), ('mark', 'held'), ('wait', 1),
         ('script', edit_aim()), ('mark', 'released'), ('wait', 1),
         ('send', EDIT), ('wait', 2), ('mark', 'off'), ('wait', 1)],
     'check': check_edit_snap_square},
    {'name': 'edit-snap-sticks', 'map': '3ds',
     'settings': dict(SQUARE, video_openxr_edit_snap_angles='0'),
     'script': HEAD_AT_ORIGIN,
     'steps': edit_start(edit_aim()) + edit_tap(EDIT_NEXT, edit_aim())
     + edit_tap(EDIT_NEXT, edit_aim()) + [
         ('script', script(edit_aim(), EDIT_GRIP)), ('wait', 1),
         ('script', script(edit_aim(), EDIT_GRIP,
                           'action edit/stick@left -0.3 0')),
         ('until', ('screen 2 square', screen2_at_yaw(-90.0), 10)),
         ('script', script(edit_aim(), EDIT_GRIP)),
         ('wait', 1), ('mark', 'square'), ('wait', 0.5), ('mark', 'push'),
         ('script', script(edit_aim(), EDIT_GRIP,
                           'action edit/stick@left 0.3 0')),
         ('until', ('screen 2 back at -74', screen2_at_yaw(-74.0, True),
                    10)),
         ('script', script(edit_aim(), EDIT_GRIP)),
         ('wait', 1), ('script', edit_aim()), ('mark', 'released'),
         ('wait', 1), ('send', EDIT), ('wait', 2), ('mark', 'off'),
         ('wait', 1)],
     'check': check_edit_snap_sticks},
    {'name': 'edit-hinge-3ds', 'map': '3ds', 'settings': HINGE_3DS,
     'script': HEAD_AT_ORIGIN,
     'env': {'RA_XR_LAYER_SNAP_EVERY': '10'},
     'steps': edit_start(edit_aim()) + edit_tap(EDIT_NEXT, edit_aim())
     + edit_tap(EDIT_NEXT, edit_aim()) + [
         ('mark', 'grab'),
         ('script', script(edit_aim(), EDIT_GRIP)), ('wait', 1),
         ('script', script(edit_aim(x=0.06), EDIT_GRIP)), ('wait', 1),
         ('mark', 'held'),
         ('script', script(edit_aim(x=0.06), EDIT_GRIP,
                           'action edit/stick@left 0 -0.3')),
         ('until', ('screen 2 at 30', screen2_at_pitch(30.0), 10)),
         ('script', script(edit_aim(x=0.06), EDIT_GRIP)),
         ('wait', 1), ('mark', 'swung'), ('script', edit_aim(x=0.06)),
         ('mark', 'released'),
         ('wait', 1), ('mark', 'resting'),
         ('script', script(edit_aim(x=0.06), EDIT_GRIP)), ('wait', 1),
         ('script', edit_aim(x=0.06)), ('mark', 'dropped'), ('wait', 1),
         ('send', EDIT), ('wait', 2), ('mark', 'off'),
         ('wait', 1)],
     'check': check_edit_hinge_3ds},
    {'name': 'edit-hinge-push', 'map': '3ds', 'settings': HINGE_3DS,
     'script': HEAD_AT_ORIGIN,
     'steps': edit_start(edit_aim()) + edit_tap(EDIT_NEXT, edit_aim())
     + edit_tap(EDIT_NEXT, edit_aim()) + [
         ('mark', 'grab'),
         ('script', script(edit_aim(), EDIT_GRIP)), ('wait', 1),
         ('script', script(edit_aim(x=0.06), EDIT_GRIP)), ('wait', 1),
         ('mark', 'held'),
         ('script', script(edit_aim(x=0.06, y=0.052), EDIT_GRIP)),
         ('wait', 1), ('mark', 'pushed'), ('wait', 0.5), ('mark', 'pull'),
         ('script', script(edit_aim(x=0.06, y=0.027), EDIT_GRIP)),
         ('wait', 1), ('mark', 'back'), ('wait', 0.5),
         ('script', edit_aim(x=0.06, y=0.027)),
         ('mark', 'released'), ('wait', 1), ('mark', 'resting'),
         ('script', script(edit_aim(x=0.06, y=0.027), EDIT_GRIP)),
         ('mark', 'regrab'), ('wait', 1),
         ('script', edit_aim(x=0.06, y=0.027)), ('mark', 'dropped'),
         ('wait', 1), ('send', EDIT), ('wait', 2), ('mark', 'off'),
         ('wait', 1)],
     'check': check_edit_hinge_push},
    {'name': 'edit-hinge-gap', 'map': '3ds', 'settings': HINGE_GAP,
     'script': HEAD_AT_ORIGIN,
     'steps': edit_start(edit_aim()) + edit_tap(EDIT_NEXT, edit_aim())
     + edit_tap(EDIT_NEXT, edit_aim()) + [
         ('mark', 'grab'),
         ('script', script(edit_aim(), EDIT_GRIP)), ('wait', 1),
         ('script', script(edit_aim(x=0.06), EDIT_GRIP)), ('wait', 1),
         ('mark', 'held'),
         ('script', script(edit_aim(x=0.06), EDIT_GRIP,
                           'action edit/stick@left 0 -0.3')),
         ('until', ('screen 2 at 30', screen2_at_pitch(30.0), 10)),
         ('script', script(edit_aim(x=0.06), EDIT_GRIP)),
         ('wait', 1), ('mark', 'swung'), ('script', edit_aim(x=0.06)),
         ('mark', 'released'),
         ('wait', 1), ('mark', 'resting'),
         ('script', script(edit_aim(x=0.06), EDIT_GRIP)), ('wait', 1),
         ('script', edit_aim(x=0.06)), ('mark', 'dropped'), ('wait', 1),
         ('send', EDIT), ('wait', 2), ('mark', 'off'),
         ('wait', 1)],
     'check': check_edit_hinge_gap},
    {'name': 'edit-hinge-twitch', 'map': '3ds',
     'settings': dict(HINGE_GAP, video_openxr_edit_smoothing='0'),
     'script': HEAD_AT_ORIGIN,
     'steps': edit_start(edit_aim()) + edit_tap(EDIT_NEXT, edit_aim())
     + edit_tap(EDIT_NEXT, edit_aim()) + [
         ('mark', 'grab'),
         ('script', script(edit_aim(), EDIT_GRIP)), ('wait', 1),
         ('script', script(edit_aim(x=0.06), EDIT_GRIP)), ('wait', 1),
         ('mark', 'held'), ('wait', 1),
         ('script', edit_aim_turned(x=0.06, pitch=0.5)),
         ('mark', 'released'), ('wait', 1), ('mark', 'resting'),
         ('script', script(edit_aim(x=0.06), EDIT_GRIP)), ('wait', 1),
         ('script', edit_aim_turned(x=0.06, pitch=-0.5)),
         ('mark', 'released2'), ('wait', 1), ('mark', 'resting2'),
         ('send', EDIT), ('wait', 2), ('mark', 'off'),
         ('wait', 1)],
     'check': check_edit_hinge_twitch},
    {'name': 'edit-hinge-settle', 'map': '3ds',
     'settings': dict(HINGE_GAP, video_openxr_edit_smoothing='0'),
     'script': HEAD_AT_ORIGIN,
     'steps': edit_start(edit_aim()) + edit_tap(EDIT_NEXT, edit_aim())
     + edit_tap(EDIT_NEXT, edit_aim()) + [
         ('mark', 'grab'),
         ('script', script(edit_aim(), EDIT_GRIP)), ('wait', 1),
         ('script', script(edit_aim(x=0.06), EDIT_GRIP)), ('wait', 1),
         ('script', script(edit_aim(x=0.06, y=-0.033), EDIT_GRIP)),
         ('wait', 1), ('mark', 'held4'), ('wait', 1),
         ('script', edit_aim(x=0.06, y=-0.033)),
         ('mark', 'released'), ('wait', 1), ('mark', 'resting'),
         ('script', script(edit_aim(x=0.06, y=-0.033), EDIT_GRIP)),
         ('wait', 1), ('script', edit_aim(x=0.06, y=-0.033)),
         ('mark', 'dropped'), ('wait', 1),
         ('send', EDIT), ('wait', 2), ('mark', 'off'),
         ('wait', 1)],
     'check': check_edit_hinge_settle},
    {'name': 'edit-hinge-shaken', 'map': '3ds',
     'settings': dict(HINGE_GAP, video_openxr_edit_smoothing='0'),
     'script': HEAD_AT_ORIGIN,
     'steps': edit_start(edit_aim()) + edit_tap(EDIT_NEXT, edit_aim())
     + edit_tap(EDIT_NEXT, edit_aim()) + [
         ('mark', 'grab'),
         ('script', script(edit_aim(), EDIT_GRIP)), ('wait', 1),
         ('script', script(edit_aim(x=0.06), EDIT_GRIP)), ('wait', 1),
         ('mark', 'still'),
         ('script', script(edit_aim(x=0.06, y=0.044), EDIT_GRIP)),
         ('wait', 0.5)] + edit_shake(0.06, 0.044, 0.5, 2) + [
         ('mark', 'shaken1'),
         ('script', script(edit_aim(x=0.06, y=-0.082), EDIT_GRIP)),
         ('wait', 0.5)] + edit_shake(0.06, -0.082, 0.5, 2) + [
         ('mark', 'shaken2'),
         ('script', edit_aim(x=0.06, y=-0.082)), ('mark', 'released'),
         ('wait', 1), ('send', EDIT), ('wait', 2), ('mark', 'off'),
         ('wait', 1)],
     'check': check_edit_hinge_shaken},
    {'name': 'edit-smooth', 'map': '3ds',
     'settings': dict(SAVE, video_openxr_edit_smoothing='2'),
     'steps': EDIT_SWAY_STEPS, 'check': check_edit_sway(True)},
    {'name': 'edit-smooth-off', 'map': '3ds', 'settings': SAVE,
     'steps': EDIT_SWAY_STEPS, 'check': check_edit_sway(False)},
    {'name': 'edit-tag', 'map': '3ds',
     'env': {'RA_XR_LAYER_SNAP_EVERY': '10'}, 'steps': EDIT_TAG_STEPS,
     'check': check_edit_tag},
    {'name': 'edit-tag-narrow', 'map': '3ds',
     'env': {'RA_XR_LAYER_SNAP_EVERY': '10',
             'RA_XR_LAYER_MAX_WIDTH': '2048'},
     'steps': EDIT_TAG_NARROW_STEPS, 'check': check_edit_tag_narrow},
    {'name': 'edit-back', 'map': '3ds',
     'settings': {'video_openxr_placement': '0'},
     'env': {'RA_XR_LAYER_SNAP_EVERY': '10'}, 'steps': EDIT_BACK_STEPS,
     'check': check_edit_back},
]
EDIT_CASES.append(dict(
    next(c for c in EDIT_CASES if c['name'] == 'edit-hinge-3ds'),
    name='edit-hinge-3ds-narrow',
    env={'RA_XR_LAYER_SNAP_EVERY': '10', 'RA_XR_LAYER_MAX_WIDTH': '4096'},
    check=check_edit_hinge_3ds_narrow))


CASES = [
    {'name': '3ds-stereo', 'map': '3ds', 'steps': SETTLE,
     'check': check_screens()},
    {'name': '3ds-swap', 'map': '3ds',
     'settings': {'video_stereo_swap_eyes': 'true'}, 'steps': SETTLE,
     'check': check_screens(swap=True)},
    {'name': '3ds-stereo-index0', 'map': '3ds', 'env': INDEX0,
     'steps': SETTLE, 'check': check_screens()},
    {'name': '3ds-swap-index0', 'map': '3ds', 'env': INDEX0,
     'settings': {'video_stereo_swap_eyes': 'true'}, 'steps': SETTLE,
     'check': check_screens(swap=True)},
    {'name': '3ds-horizontal', 'map': '3ds',
     'settings': {'video_screen_layout': '1'}, 'steps': SETTLE,
     'check': check_screens(horizontal=True)},
    {'name': 'no-map', 'map': 'none', 'steps': SETTLE,
     'check': check_frame},
    {'name': '3ds-stereo-threaded', 'map': '3ds',
     'settings': {'video_threaded': 'true'}, 'steps': SETTLE,
     'check': threaded(check_screens())},
    {'name': 'crop-threaded', 'map': 'crop',
     'settings': {'video_threaded': 'true'}, 'steps': SETTLE,
     'check': threaded(check_crop)},
    # A Vulkan core's own images, copied out into the headset's views.
    {'name': 'hw-3ds-stereo', 'map': '3ds',
     'options': {'video_views_test_hw': 'vulkan'}, 'steps': SETTLE,
     'check': check_screens()},
    {'name': 'hw-3ds-stereo-threaded', 'map': '3ds',
     'options': {'video_views_test_hw': 'vulkan'},
     'settings': {'video_threaded': 'true'}, 'steps': SETTLE,
     'baseline': THREADED_HW,
     'check': threaded(check_screens())},
    {'name': '3ds-output-size', 'map': '3ds', 'settings': SIZED,
     'args': ['--set-shader=' + PRESET], 'steps': SHOT,
     'check': check_sized_screens},
    {'name': 'no-map-output-size', 'map': 'none', 'settings': SIZED,
     'args': ['--set-shader=' + PRESET], 'steps': SHOT,
     'check': check_sized_frame},
    # Monado shows a screen here about 180 px wide, smaller than any
    # source, so the stock chain shrinks it.
    {'name': '3ds-shrink', 'map': '3ds', 'options': CHECKER,
     'steps': SETTLE, 'check': check_checker(True)},
    {'name': 'no-map-shrink', 'map': 'none', 'options': CHECKER,
     'steps': SETTLE, 'check': check_frame_checker},
    # Half a metre away the screens show larger than their sources.
    {'name': '3ds-enlarge', 'map': '3ds', 'options': CHECKER,
     'settings': {'video_openxr_distance': '0.5'}, 'steps': SETTLE,
     'check': check_checker(False, 0.5)},
    {'name': 'menu', 'map': '3ds', 'settings': {'menu_driver': 'ozone'},
     'steps': [('wait', 6), ('send', 'MENU_TOGGLE'), ('wait', 4)],
     'check': check_menu()},
    # The screens drawn in the same frames as the menu.
    {'name': 'menu-running', 'map': '3ds',
     'settings': {'menu_driver': 'ozone', 'menu_pause_libretro': 'false'},
     'steps': [('wait', 6), ('send', 'MENU_TOGGLE'), ('wait', 4)],
     'check': check_menu()},
    # A UI no bigger than the headset's density allows: copied, not drawn.
    {'name': 'menu-small', 'map': '3ds', 'settings': {'menu_driver': 'ozone'},
     'screen': (320, 192),
     'steps': [('wait', 6), ('send', 'MENU_TOGGLE'), ('wait', 4)],
     'check': check_menu((320, 192))},
    {'name': 'menu-overlay', 'map': '3ds', 'overlay': True,
     'settings': {'menu_driver': 'ozone'},
     'steps': [('wait', 6), ('send', 'MENU_TOGGLE'), ('wait', 3),
               ('shot', None), ('wait', 3)],
     'check': check_menu_overlay},
    {'name': 'menu-closes', 'map': '3ds', 'settings': {'menu_driver': 'ozone'},
     'steps': [('wait', 6), ('send', 'MENU_TOGGLE'), ('wait', 3),
               ('send', 'MENU_TOGGLE'), ('wait', 3)],
     'check': check_menu_closed},
    # Every frame's images saved, to see each new swapchain's first.
    {'name': 'menu-resize', 'map': '3ds',
     'settings': {'menu_driver': 'rgui',
                  'menu_swap_ok_cancel_buttons': 'false'},
     'env': {'RA_XR_LAYER_SNAP_EVERY': '1'}, 'steps': MENU_RESIZE,
     'check': check_menu_resize},
    {'name': 'menu-resize-running', 'map': '3ds',
     'settings': {'menu_driver': 'rgui',
                  'menu_swap_ok_cancel_buttons': 'false',
                  'menu_pause_libretro': 'false'},
     'env': {'RA_XR_LAYER_SNAP_EVERY': '1'}, 'steps': MENU_RESIZE,
     'check': check_menu_resize},
    {'name': 'recenter', 'map': '3ds',
     'steps': [('wait', 6), ('script', 'head 0.5 0.2 0.3 90'), ('wait', 1),
               ('send', 'HEADSET_RECENTER'), ('wait', 3)],
     'check': check_recenter},
    {'name': 'recenter-menu', 'map': '3ds', 'settings': {'menu_driver': 'ozone'},
     'steps': [('wait', 6), ('script', 'head 0.5 0.2 0.3 90'), ('wait', 1),
               ('send', 'MENU_TOGGLE'), ('wait', 3), ('mark', 'recenter'),
               ('send', 'HEADSET_RECENTER'), ('wait', 3), ('mark', 'closed'),
               ('send', 'MENU_TOGGLE'), ('wait', 2),
               ('send', 'HEADSET_RECENTER'), ('wait', 2)],
     'check': check_recenter_menu},
    # The runtime's recenter after the hotkey's (space 2 is LOCAL).
    {'name': 'runtime-recenter', 'map': '3ds',
     'steps': [('wait', 6), ('script', 'head 0.5 0.2 0.3 90'), ('wait', 1),
               ('send', 'HEADSET_RECENTER'), ('wait', 2), ('mark', 'stage'),
               ('script', 'space 3'), ('wait', 2), ('mark', 'local'),
               ('script', 'space 2'), ('wait', 2)],
     'check': check_runtime_recenter},
    {'name': 'pacing', 'map': '3ds',
     'steps': [('wait', 6), ('send', 'PAUSE_TOGGLE'), ('mark', 'paused'),
               ('wait', 3), ('mark', 'resumed'), ('send', 'PAUSE_TOGGLE'),
               ('wait', 2)],
     'check': check_pacing},
    # The window loses its focus; the headset session keeps it, then is
    # only visible (4), then focused (5) again.
    {'name': 'focus', 'map': '3ds', 'unfocus': True,
     'settings': {'pause_nonactive': 'true'},
     'steps': [('wait', 6), ('unfocus', None), ('mark', 'unfocused'),
               ('wait', 3), ('mark', 'visible'), ('script', 'state 4'),
               ('wait', 3), ('mark', 'refocused'), ('script', 'state 5'),
               ('wait', 3), ('mark', 'end')],
     'check': check_focus},
    {'name': 'no-runtime', 'map': '3ds', 'no_runtime': True,
     'steps': [('wait', 6), ('shot', None)],
     'check': check_no_runtime},
    {'name': 'session-fails', 'map': '3ds', 'script': 'fail session\n',
     'steps': [('wait', 8), ('shot', None)],
     'check': check_session_fails},
    {'name': 'session-lost', 'map': '3ds',
     'steps': [('wait', 6), ('script', 'state 7'), ('mark', 'lost'),
               ('wait', 3), ('shot', None)],
     'check': check_session_lost},
    {'name': 'exit-steam', 'map': '3ds', 'env': {'SteamAppId': '1118310'},
     'steps': [('wait', 6), ('mark', 'exit'), ('script', 'state 8'),
               ('wait_exit', 5)],
     'check': check_exit_steam},
    {'name': 'exit-window', 'map': '3ds', 'env': {'SteamAppId': ''},
     'steps': [('wait', 6), ('mark', 'lost'), ('script', 'state 8'),
               ('wait_exit', 3), ('shot', None)],
     'check': check_exit_window},
    {'name': 'options-follow-headset', 'map': '3ds',
     'settings': {'menu_driver': 'ozone'},
     'steps': [('wait', 6), ('send', 'MENU_TOGGLE'), ('wait', 2),
               ('script', 'state 8'), ('wait', 3)],
     'check': check_options_follow},
    # A Vulkan core: reinit and unload while the headset runs.
    {'name': 'hw-teardown', 'map': 'none', 'options': VULKAN,
     'steps': TEARDOWN, 'check': check_hw_teardown},
    {'name': 'hw-teardown-threaded', 'map': 'none', 'options': VULKAN,
     'settings': {'video_threaded': 'true'}, 'steps': TEARDOWN,
     'baseline': THREADED_HW,
     'check': threaded(check_hw_teardown)},
    {'name': 'kept-retry', 'map': 'none',
     'options': {'video_views_test_hw': 'vulkan_keep'},
     'steps': [('wait', 6), ('script', 'state 8'), ('wait', 2),
               ('mark', 'reinit'), ('send', 'FULLSCREEN_TOGGLE'),
               ('wait', 5)],
     'baseline': KEPT_LEAK, 'check': check_kept_retry},
    {'name': 'kept-lost', 'map': 'none',
     'options': {'video_views_test_hw': 'vulkan_keep'},
     'steps': [('wait', 6), ('script', 'fail instance'), ('wait', 2),
               ('mark', 'reinit'), ('send', 'FULLSCREEN_TOGGLE'),
               ('wait', 4)],
     'baseline': KEPT_LEAK, 'check': check_kept_lost},
    {'name': 'input-combined', 'map': '3ds',
     'steps': [('wait', 6), ('script', ALL_BUTTONS), ('wait', 2),
               ('script', STICKS), ('wait', 2), ('script', ''), ('wait', 2),
               ('script', 'action combined/recenter 1'), ('wait', 1),
               ('script', ''), ('wait', 1),
               ('script', 'action combined/menu 1'), ('wait', 1),
               ('script', ''), ('wait', 3)],
     'check': check_combined},
    {'name': 'input-separate', 'map': '3ds',
     'settings': {'video_openxr_controllers': '1'},
     'steps': [('wait', 6), ('script', SEPARATE), ('wait', 2),
               ('script', ''), ('wait', 2)],
     'check': check_separate},
    {'name': 'input-focus', 'map': '3ds',
     'steps': [('wait', 6), ('script', HELD), ('wait', 2),
               ('script', script(HELD, 'state 4')), ('wait', 2),
               ('script', script(HELD, 'state 5')), ('wait', 2),
               ('script', ''), ('wait', 1)],
     'check': check_input_focus},
    {'name': 'input-sync-unfocused', 'map': '3ds',
     'steps': [('wait', 6), ('script', HELD), ('wait', 2),
               ('script', script(HELD, 'sync 8')), ('wait', 2),
               ('script', script(HELD, 'sync 0')), ('wait', 2),
               ('script', ''), ('wait', 1)],
     'check': check_input_sync_unfocused},
    # Output spec section 3, Focus: the headset's focus keeps its
    # controllers and the content going while the window is unfocused;
    # only visible (4), they release and the content pauses.
    {'name': 'input-unfocused', 'map': '3ds', 'unfocus': True,
     'settings': {'pause_nonactive': 'true',
                  'input_joypad_background': 'false'},
     'steps': [('wait', 6), ('unfocus', None), ('mark', 'unfocused'),
               ('script', HELD), ('wait', 3), ('mark', 'visible'),
               ('script', script(HELD, 'state 4')), ('wait', 3),
               ('mark', 'refocused'), ('script', script(RETURNED, 'state 5')),
               ('wait', 3), ('mark', 'end'), ('script', ''), ('wait', 1)],
     'check': check_input_unfocused},
    # Forced, so the core's analog reads leave it on.
    {'name': 'input-frame-profile', 'map': '3ds', 'layer': FRAME_LAYER,
     'steps': [('wait', 6)],
     'check': check_frame_profile},
    {'name': 'input-no-frame-profile', 'map': '3ds',
     'steps': [('wait', 6)],
     'check': check_no_frame_profile},
    {'name': 'input-dpad', 'map': '3ds',
     'settings': {'input_player1_analog_dpad_mode': '3'},
     'steps': [('wait', 6), ('script', DPAD), ('wait', 2), ('script', ''),
               ('wait', 2)],
     'check': check_input_dpad},
    {'name': 'input-touch', 'map': '3ds',
     'steps': [('wait', 6), ('script', AIM_BOTTOM), ('wait', 2),
               ('script', script(AIM_BOTTOM, R2)), ('wait', 2),
               ('script', AIM_BOTTOM), ('wait', 1),
               ('script', ''), ('wait', 2)],
     'check': check_touch},
    {'name': 'input-touch-threaded', 'map': '3ds',
     'settings': {'video_threaded': 'true'},
     'steps': [('wait', 6), ('script', AIM_BOTTOM), ('wait', 2),
               ('script', script(AIM_BOTTOM, R2)), ('wait', 2),
               ('script', AIM_BOTTOM), ('wait', 1),
               ('script', ''), ('wait', 2)],
     'check': check_touch},
    {'name': 'input-top-auto', 'map': '3ds',
     'steps': [('wait', 6), ('script', script(AIM_TOP, R2)), ('wait', 2),
               ('script', ''), ('wait', 1)],
     'check': check_top_auto},
    {'name': 'input-touch-drag', 'map': '3ds',
     'steps': [('wait', 6), ('script', AIM_BOTTOM), ('wait', 1),
               ('script', script(AIM_BOTTOM, R2)), ('wait', 1),
               ('script', script(AIM_TOP, R2)), ('wait', 1),
               ('script', script(AIM_BOTTOM, R2)), ('wait', 1),
               ('script', AIM_BOTTOM), ('wait', 1),
               ('script', ''), ('wait', 1)],
     'check': check_touch_drag},
    {'name': 'input-touch-held', 'map': '3ds',
     'steps': [('wait', 6), ('script', script(AIM_TOP, R2)), ('wait', 1),
               ('script', script(AIM_BOTTOM, R2)), ('wait', 1),
               ('script', AIM_BOTTOM), ('wait', 1),
               ('script', script(AIM_BOTTOM, R2)), ('wait', 1),
               ('script', AIM_BOTTOM), ('wait', 1),
               ('script', ''), ('wait', 1)],
     'check': check_touch_held},
    # The pressing hand untracked mid-touch, then tracked at a new point.
    {'name': 'input-touch-tracking', 'map': '3ds',
     'steps': [('wait', 6), ('script', AIM_BOTTOM), ('wait', 1),
               ('script', script(AIM_BOTTOM, R2)), ('wait', 1),
               ('mark', 'lost'), ('script', script('aim right off', R2)),
               ('wait', 1), ('mark', 'found'),
               ('script', script(AIM_RIGHT_BOTTOM, R2)), ('wait', 1),
               ('script', AIM_RIGHT_BOTTOM), ('wait', 1),
               ('script', ''), ('wait', 1)],
     'check': check_touch_tracking},
    {'name': 'input-laser-off', 'map': '3ds',
     'settings': {'video_openxr_laser': '2'},
     'steps': [('wait', 6),
               ('script', script(AIM_BOTTOM, R2, AIM_LEFT_BOTTOM, L2)),
               ('wait', 2), ('script', script(AIM_BOTTOM, AIM_LEFT_BOTTOM)),
               ('wait', 1), ('script', ''), ('wait', 1)],
     'check': check_laser_off},
    {'name': 'input-lightgun', 'map': '3ds',
     'settings': {'video_openxr_laser': '1'},
     'steps': [('wait', 6), ('script', script(AIM_GUN, R2)), ('wait', 2),
               ('script', script(AIM_MISS, R2)), ('wait', 2),
               ('script', script(AIM_MISS, R2, AIM_LEFT_MISS, L2)),
               ('wait', 2), ('script', ''), ('wait', 2)],
     'check': check_lightgun},
    {'name': 'input-gun-sweep', 'map': '3ds',
     'settings': {'video_openxr_laser': '1'},
     'steps': [('wait', 6), ('script', script(AIM_GUN, R2)), ('wait', 1),
               ('script', script(AIM_MISS, R2)), ('wait', 1),
               ('script', script(AIM_GUN, R2)), ('wait', 1),
               ('script', AIM_GUN), ('wait', 1), ('script', ''),
               ('wait', 1)],
     'check': check_gun_sweep},
    {'name': 'input-gun-hold', 'map': '3ds',
     'settings': {'video_openxr_laser': '1'},
     'steps': [('wait', 6), ('script', AIM_GUN), ('wait', 1),
               ('script', script(AIM_GUN, R2)), ('wait', 1),
               ('mark', 'lost'), ('script', script('aim right off', R2)),
               ('wait', 1), ('mark', 'found'),
               ('script', script(AIM_TOP, R2)), ('wait', 1),
               ('script', AIM_TOP), ('wait', 1), ('script', ''),
               ('wait', 1)],
     'check': check_gun_hold},
    {'name': 'input-two-hands', 'map': '3ds',
     'steps': [('wait', 6),
               ('script', script(AIM_LEFT_BOTTOM, AIM_RIGHT_BOTTOM)),
               ('wait', 1),
               ('script', script(AIM_LEFT_BOTTOM, AIM_RIGHT_BOTTOM, L2)),
               ('wait', 2),
               ('script', script(AIM_LEFT_BOTTOM, AIM_RIGHT_BOTTOM, L2, R2)),
               ('wait', 2),
               ('script', script(AIM_LEFT_BOTTOM, AIM_RIGHT_BOTTOM, R2)),
               ('wait', 1), ('script', ''), ('wait', 1)],
     'check': check_two_hands},
    # No views: one quad of the whole frame, where screen 0 would be.
    {'name': 'input-frame-always', 'map': 'none',
     'settings': {'video_openxr_laser': '1'},
     'steps': [('wait', 6), ('script', script(AIM_GUN, R2)), ('wait', 2),
               ('script', ''), ('wait', 2)],
     'check': check_frame_always},
    # The core runs behind the menu, though RetroArch blocks its input
    # there, and AIM_GUN meets the menu quad in front of the frame.
    {'name': 'input-single-auto', 'map': 'none',
     'settings': {'menu_driver': 'ozone', 'menu_pause_libretro': 'false'},
     'steps': [('wait', 6), ('script', script(AIM_GUN, R2)), ('wait', 2),
               ('script', ''), ('wait', 1), ('send', 'MENU_TOGGLE'),
               ('wait', 2), ('script', AIM_GUN), ('wait', 2),
               ('script', ''), ('send', 'MENU_TOGGLE'), ('wait', 2)],
     'check': check_single_auto},
    {'name': 'input-gun-tracking', 'map': '3ds',
     'settings': {'video_openxr_laser': '1'},
     'steps': [('wait', 6), ('script', script(AIM_GUN, R2)), ('wait', 2),
               ('script', script('aim right off', AIM_LEFT_MISS, L2)),
               ('wait', 2),
               ('script', script('aim right off', AIM_LEFT_MISS, R2)),
               ('wait', 2),
               ('script', script(AIM_GUN, AIM_LEFT_MISS)), ('wait', 2),
               ('script', ''), ('wait', 1)],
     'check': check_gun_tracking},
    {'name': 'input-menu', 'map': '3ds',
     'settings': {'menu_driver': 'ozone', 'frontend_log_level': '0'},
     'steps': [('wait', 6), ('send', 'MENU_TOGGLE'), ('wait', 3),
               ('script', AIM_MENU_HIGH), ('wait', 2),
               ('script', AIM_MENU_LOW), ('wait', 2),
               ('script', script(AIM_MENU_LOW, R2)), ('wait', 1),
               ('script', AIM_MENU_LOW), ('wait', 2)],
     'check': check_menu_laser},
    # Take Screenshot writes into the case's directory; a short press, well
    # inside a second, which would make it a long one.
    {'name': 'input-menu-rgui', 'map': '3ds',
     'settings': {'menu_driver': 'rgui', 'frontend_log_level': '0'},
     'steps': [('wait', 6), ('send', 'MENU_TOGGLE'), ('wait', 3),
               ('script', AIM_RGUI_SHOT), ('wait', 2),
               ('script', script(AIM_RGUI_SHOT, R2)), ('wait', 0.5),
               ('script', AIM_RGUI_SHOT), ('wait', 2)],
     'check': check_menu_rgui},
    {'name': 'input-menu-stick', 'map': '3ds',
     'settings': {'menu_driver': 'ozone', 'frontend_log_level': '0'},
     'steps': [('wait', 6), ('send', 'MENU_TOGGLE'), ('wait', 3),
               ('script', AIM_MENU_HIGH), ('wait', 2), ('mark', 'yield'),
               ('script', script(AIM_MENU_HIGH, STICK_DOWN)), ('wait', 0.1),
               ('script', AIM_MENU_HIGH), ('wait', 1)] + SHAKE
     + [('wait', 1), ('script', AIM_MENU_MOVE), ('wait', 2),
        ('mark', 'pull'), ('script', script(AIM_MENU_MOVE, R2)),
        ('wait', 0.5), ('script', AIM_MENU_MOVE), ('wait', 2)],
     'check': check_menu_stick},
    # Each pull half a second: a short press only highlights, so a
    # failing run's extra press restarts nothing.
    {'name': 'input-menu-pull', 'map': '3ds',
     'settings': {'menu_driver': 'ozone', 'frontend_log_level': '0'},
     'steps': [('wait', 6), ('send', 'MENU_TOGGLE'), ('wait', 3),
               ('script', AIM_MENU_HIGH), ('wait', 2), ('mark', 'yield'),
               ('script', script(AIM_MENU_HIGH, STICK_DOWN)), ('wait', 0.1),
               ('script', AIM_MENU_HIGH), ('wait', 1), ('mark', 'pull'),
               ('script', script(AIM_MENU_HIGH, R2)), ('wait', 0.5),
               ('script', AIM_MENU_HIGH), ('wait', 1),
               ('script', script(AIM_MENU_HIGH, R2)), ('wait', 0.5),
               ('script', AIM_MENU_HIGH), ('wait', 2)],
     'check': check_menu_pull},
    # The hotkey over the network, with the menu open: it works there.
    {'name': 'input-laser-toggle', 'map': '3ds',
     'settings': {'menu_driver': 'ozone'},
     'steps': [('wait', 6), ('send', 'MENU_TOGGLE'), ('wait', 3),
               ('script', AIM_MENU_HIGH), ('wait', 2), ('mark', 'off'),
               ('send', 'LASER_POINTER_TOGGLE'), ('wait', 2), ('mark', 'on'),
               ('send', 'LASER_POINTER_TOGGLE'), ('wait', 2)],
     'check': check_laser_toggle('Auto', DOT_HIGH)},
    # Back to Always, not Auto: Auto shows no dot on the top screen.
    {'name': 'input-laser-toggle-always', 'map': '3ds',
     'settings': {'video_openxr_laser': '1'},
     'steps': [('wait', 6), ('script', AIM_GUN), ('wait', 2),
               ('mark', 'off'), ('send', 'LASER_POINTER_TOGGLE'),
               ('wait', 2), ('mark', 'on'),
               ('send', 'LASER_POINTER_TOGGLE'), ('wait', 2)],
     'check': check_laser_toggle('Always', (-0.4, 0.24, -1.8))},
    {'name': 'input-laser-action', 'map': '3ds',
     'steps': laser_action_steps('action combined/laser 1'),
     'check': check_laser_action},
    {'name': 'input-laser-action-separate', 'map': '3ds',
     'settings': {'video_openxr_controllers': '1'},
     'steps': laser_action_steps('action separate/laser 1'),
     'check': check_laser_action},
    {'name': 'input-menu-press-pull', 'map': '3ds',
     'settings': {'menu_driver': 'ozone', 'frontend_log_level': '0'},
     'steps': [('wait', 6), ('send', 'MENU_TOGGLE'), ('wait', 3),
               ('script', AIM_MENU_HIGH), ('wait', 2), ('mark', 'yield'),
               ('script', script(AIM_MENU_HIGH, STICK_DOWN, R2)),
               ('wait', 0.1),
               ('script', script(AIM_MENU_HIGH, R2)), ('wait', 0.4),
               ('script', AIM_MENU_HIGH), ('wait', 1), ('mark', 'pull'),
               ('script', script(AIM_MENU_HIGH, R2)), ('wait', 0.5),
               ('script', AIM_MENU_HIGH), ('wait', 1),
               ('script', script(AIM_MENU_HIGH, R2)), ('wait', 0.5),
               ('script', AIM_MENU_HIGH), ('wait', 2)],
     'check': check_menu_press_pull},
    # Both hands turn 6 degrees and more: the laser stays hidden until
    # the pull.
    {'name': 'input-menu-yield-moves', 'map': '3ds',
     'settings': {'menu_driver': 'ozone', 'frontend_log_level': '0'},
     'steps': [('wait', 6), ('send', 'MENU_TOGGLE'), ('wait', 3),
               ('script', script(AIM_MENU_HIGH, AIM_LEFT_YIELD)),
               ('wait', 2), ('mark', 'yield'),
               ('script', script(AIM_MENU_HIGH, AIM_LEFT_YIELD, STICK_DOWN)),
               ('wait', 0.1),
               ('script', script(AIM_MENU_HIGH, AIM_LEFT_YIELD)),
               ('wait', 1),
               ('script', script(AIM_MENU_HIGH, AIM_LEFT_TURN)),
               ('wait', 2),
               ('script', script(AIM_MENU_MOVE, AIM_LEFT_TURN)),
               ('wait', 2), ('mark', 'pull'),
               ('script', script(AIM_MENU_MOVE, AIM_LEFT_TURN, R2)),
               ('wait', 0.5),
               ('script', script(AIM_MENU_MOVE, AIM_LEFT_TURN)),
               ('wait', 2)],
     'check': check_menu_yield_moves},
    {'name': 'input-menu-yield-ends', 'map': '3ds',
     'settings': {'menu_driver': 'ozone', 'frontend_log_level': '0'},
     'steps': [('wait', 6), ('send', 'MENU_TOGGLE'), ('wait', 3),
               ('script', AIM_MENU_HIGH), ('wait', 2), ('mark', 'yield1'),
               ('script', script(AIM_MENU_HIGH, STICK_DOWN)), ('wait', 0.1),
               ('script', AIM_MENU_HIGH), ('wait', 1.5), ('mark', 'toggle'),
               ('send', 'LASER_POINTER_TOGGLE'), ('wait', 0.5),
               ('send', 'LASER_POINTER_TOGGLE'), ('wait', 2),
               ('mark', 'yield2'),
               ('script', script(AIM_MENU_HIGH, STICK_DOWN)), ('wait', 0.1),
               ('script', AIM_MENU_HIGH), ('wait', 1.5),
               ('send', 'MENU_TOGGLE'), ('wait', 1), ('mark', 'reopen'),
               ('send', 'MENU_TOGGLE'), ('wait', 3)],
     'check': check_menu_yield_ends},
    {'name': 'input-menu-hands', 'map': '3ds',
     'settings': {'menu_driver': 'ozone', 'frontend_log_level': '0'},
     'steps': [('wait', 6), ('send', 'MENU_TOGGLE'), ('wait', 3),
               ('script', script(AIM_LEFT_MENU_TOP, AIM_MENU_TOP)),
               ('wait', 1),
               ('script', script(AIM_LEFT_MENU_TOP, AIM_MENU_TOP, L2)),
               ('wait', 1.5),
               ('script', script(AIM_LEFT_MENU_TOP, AIM_MENU_TOP, L2, R2)),
               ('wait', 1),
               ('script', script(AIM_LEFT_MENU_TOP, AIM_MENU_TOP, R2)),
               ('wait', 0.5),
               ('script', script(AIM_LEFT_MENU_TOP, AIM_MENU_TOP)),
               ('wait', 1)],
     'check': check_menu_hands},
    # The hover spans two of the layer's snapshots (one per 1.5 s): the
    # click closes the menu.
    {'name': 'input-menu-click', 'map': '3ds',
     'settings': {'menu_driver': 'ozone', 'frontend_log_level': '0',
                  'confirm_reset': 'false'},
     'steps': [('wait', 6), ('send', 'MENU_TOGGLE'), ('wait', 3),
               ('script', AIM_MENU_HIGH), ('wait', 3),
               ('script', script(AIM_MENU_HIGH, R2)), ('wait', CLICK),
               ('script', AIM_MENU_HIGH), ('wait', 2)],
     'check': check_menu_click},
    # R2 held on the top screen, where the menu then opens on Reset; let
    # go well inside the second RGUI still takes as a click.
    {'name': 'input-menu-held', 'map': '3ds',
     'settings': {'menu_driver': 'rgui', 'frontend_log_level': '0',
                  'confirm_reset': 'false'},
     'steps': [('wait', 6), ('script', script(AIM_RGUI_RESET, R2)),
               ('wait', 2), ('send', 'MENU_TOGGLE'), ('wait', 0.5),
               ('script', AIM_RGUI_RESET), ('wait', 2)],
     'check': check_menu_held},
    {'name': 'input-menu-glitch', 'map': '3ds',
     'settings': {'menu_driver': 'rgui', 'frontend_log_level': '0',
                  'confirm_reset': 'false'},
     'steps': [('wait', 6), ('send', 'MENU_TOGGLE'), ('wait', 3),
               ('script', AIM_RGUI_RESET), ('wait', 3),
               ('script', script(AIM_RGUI_RESET, R2)), ('wait', 0.1),
               ('mark', 'lost'), ('script', script('aim right off', R2)),
               ('wait', 0.2), ('mark', 'found'),
               ('script', script(AIM_RGUI_RESET, R2)), ('wait', 0.1),
               ('script', AIM_RGUI_RESET), ('wait', 2)],
     'check': check_menu_glitch},
    # The network command closes the menu past the press's input flush,
    # and opens it again inside the second a click may take.
    {'name': 'input-menu-close-held', 'map': '3ds',
     'settings': {'menu_driver': 'rgui', 'frontend_log_level': '0',
                  'confirm_reset': 'false'},
     'steps': [('wait', 6), ('send', 'MENU_TOGGLE'), ('wait', 3),
               ('script', AIM_RGUI_RESET), ('wait', 2),
               ('script', script(AIM_RGUI_RESET, R2)), ('wait', 0.1),
               ('send', 'MENU_TOGGLE'), ('wait', 0.2),
               ('script', AIM_RGUI_RESET), ('wait', 0.2),
               ('send', 'MENU_TOGGLE'), ('wait', 2),
               ('script', script(AIM_RGUI_RESET, R2)), ('wait', CLICK),
               ('script', AIM_RGUI_RESET), ('wait', 2)],
     'check': check_menu_close_held},
    {'name': 'input-menu-click-rgui-views', 'map': 'vb',
     'settings': {'menu_driver': 'rgui', 'frontend_log_level': '0',
                  'confirm_reset': 'false'},
     'steps': [('wait', 6), ('send', 'MENU_TOGGLE'), ('wait', 3),
               ('script', AIM_RGUI_RESET_VB), ('wait', 3),
               ('script', script(AIM_RGUI_RESET_VB, R2)), ('wait', CLICK),
               ('script', AIM_RGUI_RESET_VB), ('wait', 2)],
     'check': check_menu_click_rgui_views},
    {'name': 'input-menu-click-rgui', 'map': '3ds',
     'settings': {'menu_driver': 'rgui', 'frontend_log_level': '0',
                  'confirm_reset': 'false'},
     'steps': [('wait', 6), ('send', 'MENU_TOGGLE'), ('wait', 3)]
     + [('send', 'MENU_DOWN'), ('wait', 0.2)] * 4
     + [('script', AIM_RGUI_RESET), ('wait', 3),
        ('script', script(AIM_RGUI_RESET, R2)), ('wait', CLICK),
        ('script', AIM_RGUI_RESET), ('wait', 2)],
     'check': check_menu_click_rgui},
    {'name': 'input-rumble', 'map': '3ds',
     'steps': [('wait', 6), ('script', 'action combined/start 1'),
               ('wait', 2), ('mark', 'paused'), ('send', 'PAUSE_TOGGLE'),
               ('wait', 2), ('mark', 'resumed'), ('send', 'PAUSE_TOGGLE'),
               ('wait', 2), ('script', ''), ('wait', 2)],
     'check': check_rumble},
    {'name': 'input-rumble-gain', 'map': '3ds',
     'settings': {'input_rumble_gain': '50'},
     'steps': [('wait', 6), ('script', 'action combined/start 1'),
               ('wait', 2), ('script', ''), ('wait', 2)],
     'check': check_rumble_gain},
    {'name': 'input-rumble-separate', 'map': '3ds',
     'settings': {'video_openxr_controllers': '1'},
     'steps': [('wait', 6), ('script', 'action separate/start@left 1'),
               ('wait', 2), ('script', 'action separate/start@right 1'),
               ('wait', 2), ('script', ''), ('wait', 2)],
     'check': check_rumble_separate},
    # Monado's 20 Hz, then the layer's divide 2 for a 10 Hz headset.
    {'name': 'pace-rate-change', 'map': 'none', 'options': FPS10,
     'settings': WINDOW1,
     'steps': [('wait', 8), ('mark', 'from'), ('wait', 4),
               ('mark', 'change'), ('script', 'divide 2'), ('wait', 7),
               ('mark', 'to')],
     'check': check_rate_change},
    # 10 fps on the 20 Hz headset: two headset frames a core frame.
    {'name': 'pace-2', 'map': 'none', 'options': FPS10, 'settings': WINDOW1,
     'steps': PACE_STEPS, 'check': check_paced(2, 20.0)},
    # 16 fps doesn't fit 20 Hz; the video driver restarts halfway.
    {'name': 'pace-misfit', 'map': 'none', 'options': FPS16,
     'settings': WINDOW1,
     'steps': [('wait', 8), ('send', 'FULLSCREEN_TOGGLE'), ('wait', 8),
               ('mark', 'from'), ('wait', 4), ('mark', 'to')],
     'check': check_misfit},
    {'name': 'pace-threaded', 'map': 'none', 'options': FPS10,
     'settings': dict(WINDOW1, video_threaded='true'), 'steps': PACE_STEPS,
     'check': threaded(check_unpaced)},
    {'name': 'pace-vsync-off', 'map': 'none', 'options': FPS10,
     'settings': dict(WINDOW1, video_vsync='false'), 'steps': PACE_STEPS,
     'check': check_unpaced},
    # The session only synchronized (3) for four seconds, then focused.
    {'name': 'pace-hidden', 'map': 'none', 'options': FPS10,
     'settings': WINDOW1,
     'steps': [('wait', 8), ('mark', 'hidden'), ('script', 'state 3'),
               ('wait', 4), ('mark', 'shown'), ('script', 'state 5'),
               ('wait', 6), ('mark', 'to')],
     'check': check_hidden},
    # The layer's divide 4 only while the session is synchronized.
    {'name': 'pace-hidden-slow', 'map': 'none', 'options': FPS10,
     'settings': WINDOW1,
     'steps': [('wait', 8), ('mark', 'hidden'),
               ('script', 'state 3\ndivide 4'), ('wait', 7),
               ('mark', 'shown'), ('script', 'divide 1\nstate 5'),
               ('wait', 6), ('mark', 'to')],
     'check': check_hidden_slow},
    {'name': 'pace-stopping', 'map': 'none', 'options': FPS10,
     'settings': WINDOW1,
     'steps': [('wait', 8), ('mark', 'from'), ('wait', 3),
               ('mark', 'stop'), ('script', 'state 6'), ('wait', 5),
               ('mark', 'end')],
     'check': check_stopping},
    # Black frame insertion and subframes wait for the window's display,
    # which the headset's pace replaces.
    {'name': 'pace-2-bfi', 'map': 'none', 'options': FPS10,
     'settings': dict(WINDOW1, video_black_frame_insertion='1'),
     'steps': PACE_STEPS, 'check': check_paced(2, 20.0)},
    {'name': 'pace-fastforward', 'map': 'none', 'options': FPS10,
     'settings': WINDOW1,
     'steps': [('wait', 8), ('mark', 'ff'), ('send', 'FAST_FORWARD'),
               ('wait', 3), ('mark', 'ffend'), ('send', 'FAST_FORWARD'),
               ('wait', 6), ('mark', 'to')],
     'check': check_fastforward},
    # Sync to Exact Content Framerate is set aside while the headset paces.
    # 16 fps fits 20 Hz at skew 0.25: the tick paces it, not the timer.
    {'name': 'pace-vrr', 'map': 'none', 'options': FPS16,
     'settings': dict(WINDOW1, vrr_runloop_enable='true',
                      audio_max_timing_skew='0.25'),
     'steps': PACE_STEPS, 'check': check_paced(1, 20.0)},
    {'name': 'pace-window-back', 'map': 'none', 'options': FPS10,
     'settings': WINDOW1,
     'steps': [('wait', 8), ('mark', 'from'), ('wait', 5), ('mark', 'to'),
               ('script', 'state 7'), ('mark', 'lost'), ('wait', 6),
               ('mark', 'end')],
     'check': check_window_back},
    # The core is waiting on the headset when the loss lands, on some
    # runs: the tick's phase against the script's is not controlled.
    {'name': 'pace-loss-waiting', 'map': 'none', 'options': FPS10,
     'settings': dict(WINDOW1, video_openxr_refresh_rate='1'),
     'steps': [('wait', 8), ('mark', 'from'), ('wait', 5), ('mark', 'to'),
               ('script', 'state 7'), ('mark', 'lost'), ('wait', 6),
               ('mark', 'end')],
     'check': check_loss_waiting},
    # The layer lists 20 and 10 Hz and starts at 10: Auto asks for 20.
    {'name': 'pace-auto', 'map': 'none', 'options': FPS10,
     'settings': WINDOW1, 'script': 'rates 20 10\ndivide 2\n',
     'steps': PACE_STEPS, 'check': check_request(20.0, 2, 20.0)},
    {'name': 'pace-headset-choice', 'map': 'none', 'options': FPS10,
     'settings': dict(WINDOW1, video_openxr_refresh_rate='1'),
     'script': 'rates 20 10\ndivide 2\n', 'steps': PACE_STEPS,
     'check': check_request(None, 1, 10.0)},
    {'name': 'pace-fixed-rate', 'map': 'none', 'options': FPS10,
     'settings': dict(WINDOW1, video_openxr_refresh_rate='10'),
     'script': 'rates 20 10\n', 'steps': PACE_STEPS,
     'check': check_request(10.0, 1, 10.0)},
    # Head tracking: the 3DS map's top screen as TRACKED eyes.
    {'name': 'tracked', 'map': '3ds', 'options': {'video_views_test_vr': 'local'},
     'steps': SETTLE, 'check': check_tracked()},
    {'name': 'tracked-stage', 'map': '3ds',
     'options': {'video_views_test_vr': 'stage'}, 'steps': SETTLE,
     'check': check_tracked('stage')},
    {'name': 'tracked-threaded', 'map': '3ds',
     'options': {'video_views_test_vr': 'local'},
     'settings': {'video_threaded': 'true'}, 'steps': SETTLE,
     'check': threaded(check_tracked())},
    {'name': 'tracked-hw', 'map': '3ds',
     'options': {'video_views_test_vr': 'local', 'video_views_test_hw': 'vulkan'},
     'steps': SETTLE, 'check': check_tracked()},
    # 10 fps on the 20 Hz headset: each core frame has its own sample.
    {'name': 'tracked-paced', 'map': '3ds',
     'options': dict(FPS10, video_views_test_vr='local'), 'settings': WINDOW1,
     'steps': SETTLE, 'check': check_tracked()},
    {'name': 'tracked-nopair', 'map': '3ds',
     'options': {'video_views_test_vr': 'nopair'}, 'steps': SETTLE,
     'check': check_tracked_refused},
    {'name': 'flat-hmd-size', 'map': '3ds', 'steps': SETTLE,
     'check': check_flat_size},
    {'name': 'tracked-menu', 'map': '3ds',
     'options': {'video_views_test_vr': 'local'},
     'settings': {'menu_driver': 'ozone'},
     'steps': [('wait', 6), ('mark', 'menu'), ('send', 'MENU_TOGGLE'),
               ('wait', 4)],
     'check': check_tracked_menu},
    {'name': 'tracked-flat', 'map': '3ds',
     'options': {'video_views_test_vr': 'local'},
     'steps': [('wait', 6), ('mark', 'flat'), ('touch', 'vr_flat'),
               ('wait', 5)],
     'check': check_tracked_flat},
    {'name': 'tracked-fallback', 'map': '3ds',
     'options': {'video_views_test_vr': 'local'}, 'script': 'fail eyes\n',
     'steps': SETTLE, 'check': check_tracked_fallback},
    {'name': 'tracked-recenter', 'map': '3ds',
     'options': {'video_views_test_vr': 'local'},
     'steps': [('wait', 6), ('script', 'head 0.5 0.2 0.3 90'), ('wait', 1),
               ('mark', 'recenter'), ('send', 'HEADSET_RECENTER'),
               ('wait', 3)],
     'check': check_tracked_recenter},
    # space 3 is STAGE, space 2 LOCAL.
    {'name': 'tracked-runtime-recenter', 'map': '3ds',
     'options': {'video_views_test_vr': 'local'},
     'steps': [('wait', 6), ('script', 'space 3'), ('wait', 2),
               ('script', 'space 2'), ('wait', 2)],
     'check': check_tracked_runtime_recenter},
    {'name': 'tracked-exit', 'map': '3ds',
     'options': {'video_views_test_vr': 'local'}, 'env': {'SteamAppId': ''},
     'steps': [('wait', 6), ('mark', 'lost'), ('script', 'state 8'),
               ('wait_exit', 3), ('shot', None)],
     'check': check_tracked_exit},
    {'name': 'tracked-stopping', 'map': '3ds',
     'options': {'video_views_test_vr': 'local'},
     'steps': [('wait', 6), ('script', 'state 6'), ('wait', 3)],
     'check': check_tracked_stopping},
    {'name': 'tracked-recenter-paused', 'map': '3ds',
     'options': {'video_views_test_vr': 'local'},
     'steps': RECENTER_PAUSED,
     'check': check_tracked_recenter_paused('local')},
    {'name': 'tracked-recenter-paused-threaded', 'map': '3ds',
     'options': {'video_views_test_vr': 'local'},
     'settings': {'video_threaded': 'true'}, 'steps': RECENTER_PAUSED,
     'check': threaded(check_tracked_recenter_paused('local'))},
    {'name': 'tracked-recenter-paused-stage', 'map': '3ds',
     'options': {'video_views_test_vr': 'stage'},
     'steps': RECENTER_PAUSED,
     'check': check_tracked_recenter_paused('stage')},
    {'name': 'tracked-recenter-menu', 'map': '3ds',
     'options': {'video_views_test_vr': 'local'},
     'settings': {'menu_driver': 'ozone'},
     'steps': [('send', 'MENU_TOGGLE') if s == ('send', 'PAUSE_TOGGLE')
               else s for s in RECENTER_PAUSED],
     'check': check_tracked_recenter_paused('local')},
    {'name': 'tracked-reinit', 'map': '3ds',
     'options': {'video_views_test_vr': 'stage'}, 'steps': REINIT_RUNNING,
     'check': check_tracked_reinit},
    {'name': 'tracked-reinit-resized', 'map': '3ds',
     'options': {'video_views_test_vr': 'local'}, 'steps': REINIT_RESIZED,
     'check': check_tracked_reinit_resized},
    # The recenter is taken while the samples fail, and told after.
    {'name': 'tracked-recenter-unlocated', 'map': '3ds',
     'options': {'video_views_test_vr': 'local'},
     'steps': [('wait', 6),
               ('script', 'head 0.5 0.2 0.3 90\ninvalid views'),
               ('wait', 1), ('mark', 'recenter'),
               ('send', 'HEADSET_RECENTER'), ('wait', 2),
               ('script', 'head 0.5 0.2 0.3 90'), ('wait', 3)],
     'check': check_tracked_recenter_unlocated},
    {'name': 'tracked-reinit-paused', 'map': '3ds',
     'options': {'video_views_test_vr': 'stage'}, 'steps': REINIT_PAUSED,
     'check': check_tracked_reinit_paused('projection', 'stage')},
    {'name': 'tracked-reinit-paused-threaded', 'map': '3ds',
     'options': {'video_views_test_vr': 'stage'},
     'settings': {'video_threaded': 'true'}, 'steps': REINIT_PAUSED,
     'check': threaded(check_tracked_reinit_paused('projection', 'stage'))},
    {'name': 'tracked-reinit-paused-local', 'map': '3ds',
     'options': {'video_views_test_vr': 'local'}, 'steps': REINIT_PAUSED,
     'check': check_tracked_reinit_paused('projection', 'local')},
    {'name': 'tracked-reinit-paused-local-threaded', 'map': '3ds',
     'options': {'video_views_test_vr': 'local'},
     'settings': {'video_threaded': 'true'}, 'steps': REINIT_PAUSED,
     'check': threaded(check_tracked_reinit_paused('projection', 'local'))},
    {'name': 'tracked-reinit-paused-unlocated', 'map': '3ds',
     'options': {'video_views_test_vr': 'local'},
     'steps': REINIT_PAUSED_UNLOCATED,
     'check': check_tracked_reinit_paused('projection', 'local')},
    {'name': 'tracked-reinit-paused-nostage', 'map': '3ds',
     'options': {'video_views_test_vr': 'local'}, 'script': 'fail stage\n',
     'steps': REINIT_PAUSED,
     'check': check_tracked_reinit_paused('pair', 'local')},
    {'name': 'tracked-reinit-local', 'map': '3ds',
     'options': {'video_views_test_vr': 'local'}, 'steps': REINIT_RUNNING,
     'check': check_tracked_reinit_recentered('local')},
    {'name': 'tracked-reinit-local-hw', 'map': '3ds',
     'options': {'video_views_test_vr': 'local',
                 'video_views_test_hw': 'vulkan'},
     'steps': REINIT_RUNNING, 'check': check_tracked_reinit_recentered('local')},
    {'name': 'tracked-reinit-local-hw-threaded', 'map': '3ds',
     'options': {'video_views_test_vr': 'local',
                 'video_views_test_hw': 'vulkan'},
     'settings': {'video_threaded': 'true'}, 'steps': REINIT_RUNNING,
     'check': threaded(check_tracked_reinit_recentered('local'))},
    # Sampled through its pose calls alone: no tracked map.
    {'name': 'tracked-reinit-poses', 'map': '3ds',
     'options': {'video_views_test_vr': 'poses'}, 'steps': REINIT_RUNNING,
     'check': check_tracked_reinit_recentered(None)},
    {'name': 'tracked-reinit-poses-threaded', 'map': '3ds',
     'options': {'video_views_test_vr': 'poses'},
     'settings': {'video_threaded': 'true'}, 'steps': REINIT_RUNNING,
     'check': threaded(check_tracked_reinit_recentered(None))},
    {'name': 'tracked-reinit-paused-hw', 'map': '3ds',
     'options': {'video_views_test_vr': 'stage',
                 'video_views_test_hw': 'vulkan'},
     'steps': REINIT_PAUSED,
     'check': check_tracked_reinit_paused(None, 'stage')},
    {'name': 'input-start-menu-tap', 'map': '3ds',
     'settings': {'menu_driver': 'ozone'},
     'steps': [('wait', 6), ('mark', 'press'), ('script', START_MENU),
               ('wait', 0.3), ('script', ''), ('wait', 3)],
     'check': check_start_menu_tap},
    {'name': 'input-start-menu-hold', 'map': '3ds',
     'settings': {'menu_driver': 'ozone'},
     'steps': [('wait', 6), ('script', START_MENU), ('wait', 1.5),
               ('mark', 'release'), ('script', ''), ('wait', 3)],
     'check': check_start_menu_hold},
    {'name': 'placement-follow', 'map': '3ds',
     'settings': {'video_openxr_placement': '1'},
     'steps': [('script', script('head 0 0 0 0', AIM_BOTTOM)), ('wait', 6),
               ('mark', 'turned'),
               ('script', script(HEAD_TURNED, AIM_BOTTOM_TURNED)),
               ('wait', 3)],
     'check': check_placement_follow},
    {'name': 'placement-lazy', 'map': '3ds',
     'settings': {'video_openxr_placement': '2'}, 'script': 'head 0 0 0 0',
     'steps': [('script', 'head 0 0 0 0'), ('wait', 6), ('mark', 'small'),
               ('script', 'head 0 0 0 20'), ('wait', 2), ('mark', 'big'),
               ('script', HEAD_TURNED), ('wait', 4)],
     'check': check_placement_lazy},
    {'name': 'placement-follow-laser', 'map': '3ds',
     'settings': {'video_openxr_placement': '1', 'menu_driver': 'ozone',
                  'frontend_log_level': '0'},
     'steps': [('script', HEAD_TURNED), ('wait', 6), ('send', 'MENU_TOGGLE'),
               ('wait', 3),
               ('script', script(HEAD_TURNED, AIM_MENU_HIGH_TURNED)),
               ('wait', 2),
               ('script', script(HEAD_TURNED, AIM_MENU_LOW_TURNED)),
               ('wait', 2),
               ('script', script(HEAD_TURNED, AIM_MENU_LOW_TURNED, R2)),
               ('wait', 1),
               ('script', script(HEAD_TURNED, AIM_MENU_LOW_TURNED)),
               ('wait', 2)],
     'check': check_placement_follow_laser},
    {'name': 'input-device', 'map': '3ds', 'env': {'SteamAppId': ''},
     'steps': [('wait', 6), ('script', 'action combined/a 1'), ('wait', 1),
               ('script', ''), ('wait', 1), ('script', 'state 8'),
               ('wait', 3)],
     'check': check_input_device},
    {'name': 'input-device-second', 'map': '3ds',
     'settings': {'test_input_file_joypad': PAD_FIRST},
     'steps': [('wait', 6), ('script', 'action combined/a 1'), ('wait', 1),
               ('script', ''), ('wait', 1)],
     'check': check_input_device_second},
    {'name': 'input-device-taken', 'map': '3ds',
     'settings': {'test_input_file_joypad': PAD_LATER},
     'steps': [('wait', 14), ('script', 'action combined/a 1'), ('wait', 1),
               ('script', ''), ('wait', 1)],
     'check': check_input_device_taken},
    {'name': 'input-hotkeys', 'map': '3ds',
     'settings': {'input_enable_hotkey_btn': '14',
                  'input_screenshot_btn': '9'},
     'steps': [('wait', 6), ('script', 'action combined/x 1'), ('wait', 2),
               ('script', ''), ('wait', 1),
               ('script', 'action combined/l3 1'), ('wait', 1),
               ('mark', 'combo'),
               ('script', script('action combined/l3 1',
                                 'action combined/x 1')),
               ('wait', 2), ('script', ''), ('wait', 2)],
     'check': check_input_hotkeys},
    {'name': 'input-hotkeys-separate', 'map': '3ds',
     'settings': {'video_openxr_controllers': '1', 'menu_driver': 'ozone',
                  'input_menu_toggle_btn': '11'},
     'steps': [('wait', 6), ('script', 'action separate/r@right 1'),
               ('wait', 1), ('script', ''), ('wait', 2), ('mark', 'left'),
               ('script', 'action separate/r@left 1'), ('wait', 0.5),
               ('script', ''), ('wait', 3)],
     'check': check_input_hotkeys_separate},
    {'name': 'input-port-binds', 'map': '3ds',
     'settings': {'input_player1_b_btn': '8', 'input_player1_a_btn': '0'},
     'steps': [('wait', 6), ('script', 'action combined/a 1'), ('wait', 2),
               ('script', ''), ('wait', 1),
               ('script', 'action combined/b 1'), ('wait', 2),
               ('script', ''), ('wait', 1)],
     'check': check_input_port_binds},
    {'name': 'input-bind-capture', 'map': '3ds',
     'settings': {'menu_driver': 'rgui',
                  'menu_swap_ok_cancel_buttons': 'false'},
     'steps': [('wait', 6), ('send', 'MENU_TOGGLE'), ('wait', 3)]
     + BIND_B_NAV
     + [('script', 'action combined/x 1'), ('wait', 2), ('script', ''),
        ('wait', 1), ('send', 'MENU_TOGGLE'), ('wait', 2),
        ('script', 'action combined/x 1'), ('wait', 2), ('script', ''),
        ('wait', 1)],
     'check': check_input_bind_capture},
]


class XFocusChangeEvent(ctypes.Structure):
    _fields_ = [('type', ctypes.c_int), ('serial', ctypes.c_ulong),
                ('send_event', ctypes.c_int), ('display', ctypes.c_void_p),
                ('window', ctypes.c_ulong), ('mode', ctypes.c_int),
                ('detail', ctypes.c_int), ('pad', ctypes.c_long * 24)]


def check_session_fails_gl(res):
    """A GL host's refused session: one line, no video rebuild, the
    window as it was."""
    errors = []
    if '[OpenXR] Continuing without headset output.' not in res.log:
        errors.append('no "Continuing without headset output" line')
    for line in ('[OpenXR] Rebuilding video without headset output.',
                 '[Video] Reinitialising the video driver at its request.'):
        if line in res.log:
            errors.append('unexpected "%s"' % line)
    if any(quads(f) for f in res.frames):
        errors.append('quads were submitted after the session failed')
    return errors + window_is(res, GREEN, 'the 2D map')


def check_menu_reopen(res):
    """A menu opened again after a while closed shows an image released
    since it reopened, never the last one's, and nothing while closed."""
    if 'closed' not in res.marks or 'reopen' not in res.marks:
        return ['marks missing']
    closed = res.marks['closed'] * 1e6 + 500000
    reopen = res.marks['reopen'] * 1e6
    errors = []
    hidden = [f for f in res.frames if closed < f['t_us'] < reopen
              and menus(f)]
    if hidden:
        errors.append('%d frames showed the menu while it was closed'
                      % len(hidden))
    shown = [f for f in res.frames if f['t_us'] > reopen and menus(f)]
    if not shown:
        return errors + ['the menu never came back']
    sc = menus(shown[0])[0]['sc']
    fresh = [r for r in res.releases
             if r['sc'] == sc and reopen < r['t_us'] <= shown[0]['t_us']]
    if not fresh:
        errors.append('the menu came back with an image released before '
                      'it reopened')
    return errors


GL_DESTROY = re.compile(r'\[video_views\] context_destroy at (\d+) us')


def check_hw_teardown_gl(res):
    """A GL core's context_destroy, at a video reinit and at unload: the
    XR thread, which holds a context in the core's share group, has ended
    its session first, and the headset's frames start again only in the
    next one."""
    times = [int(t) for t in GL_DESTROY.findall(res.log)]
    if len(times) != 2:
        return ['%d context_destroy calls, want 2 (reinit, unload)'
                % len(times)]
    made = [e['t_us'] for e in events(res, 'session')]
    ended = [e['t_us'] for e in events(res, 'session_end')]
    errors = []
    for i, t in enumerate(times):
        what = ('reinit', 'unload')[i]
        alive = (sum(1 for x in made if x < t)
                 - sum(1 for x in ended if x < t))
        if alive:
            errors.append('%d session alive at the core\'s context_destroy '
                          '(%s)' % (alive, what))
        later = [x for x in made if x > t]
        if not later:
            errors.append('no session after the %s' % what)
            continue
        between = [f for f in res.frames if t <= f['t_us'] < later[0]]
        if between:
            errors.append('%d headset frames between the core\'s '
                          'context_destroy and the next session (%s)'
                          % (len(between), what))
        end = times[i + 1] if i + 1 < len(times) else float('inf')
        if not [f for f in res.frames if later[0] < f['t_us'] < end]:
            errors.append('no headset frames after the %s' % what)
    return errors


def check_tracked_busy(res):
    """Two recenters while the compositor keeps the eyes' images: the
    headset goes on showing the image it has with the poses that drew
    it, never a later frame's, until a new image is released; then that
    one with its own. The core hears RECENTERED for each."""
    if marks_missing(res, 'busy', 'recenter', 'free'):
        return ['the run did not reach its marks']
    errors = []
    n = recenter_flags(res)
    if n != 2:
        errors.append('the core saw RECENTERED %d times, want 2' % n)
    size = tracked_size(res)
    if not size:
        return errors + ['the core never saw HMD']
    t0, t1 = res.marks['busy'] * 1e6 + 5e5, res.marks['free'] * 1e6
    held = [f for f in res.frames if t0 < f['t_us'] < t1 and f['layers']]
    blank = [f['n'] for f in held if not projections(f)]
    if blank:
        errors.append('%d frames without the projection while its images '
                      'were busy, first %d' % (len(blank), blank[0]))
    snapped = [f for f in held if f['snap'] and projections(f)]
    if not snapped:
        return errors + ['no snapped projection while the eyes were busy']
    tags, eyes, wrong = set(), set(), []
    for f in snapped:
        views = projections(f)[0]['views']
        wrong += ['frame %d: %s' % (f['n'], e)
                  for e in shown_as_drawn(res, f)]
        tags |= set(frame_tag(image(res, f, v), v, size) for v in views)
        eyes |= set(v['sc'] for v in views)
    errors += wrong[:4]
    if len(tags) != 1 or None in tags:
        errors.append('the eyes changed while their images were busy: '
                      'frames %s' % sorted(tags, key=str))
    released = [r for r in res.releases
                if r['sc'] in eyes and t0 < r['t_us'] < t1]
    if released:
        errors.append('%d eye images released while busy: the case did not '
                      'hold them' % len(released))
    after = [f for f in res.frames
             if f['t_us'] > t1 + 1e6 and f['snap'] and projections(f)]
    if not after:
        return errors + ['no snapped projection once the eyes were free']
    errors += shown_as_drawn(res, after[-1])
    last = set(frame_tag(image(res, after[-1], v), v, size)
               for v in projections(after[-1])[0]['views'])
    if last & tags:
        errors.append('the eyes still show frame %s once free'
                      % sorted(last, key=str))
    return errors


def window_quad(res, fr):
    """Errors unless fr shows gl's window whole: one quad for both eyes,
    not blended, of the window's shape and no smaller than its pixels."""
    q = quads(fr)
    if len(q) != 1 or q[0]['eye'] != 'both' or q[0]['flags'] & BLEND:
        return ['want the window as one unblended quad for both eyes, got '
                '%s' % [(x['eye'], x['flags']) for x in q]]
    errors = []
    if not at(q[0], (0.0, 0.0, -1.8)) or not sized(q[0], (1.6, 0.96)):
        errors.append('window at %s size %s'
                      % (q[0]['pose'][:3], q[0]['size']))
    return errors + image_size(res, q[0], (W, H))


def check_window(res):
    """gl without a map: its window whole, the frame in it as the window
    draws it."""
    fr = last_snap(res)
    if not fr:
        return no_quads(res)
    errors = window_quad(res, fr)
    if errors:
        return errors
    q = quads(fr)[0]
    for fx, fy, want in ((0.25, 0.25, RED), (0.75, 0.25, BLUE),
                         (0.5, 0.75, YELLOW), (0.1, 0.75, GREY)):
        c, _ = colour(image(res, fr, q), fx, fy)
        if not near(c, want):
            errors.append('(%.2f,%.2f) shows %s, want %s' % (fx, fy, c, want))
    return errors


def check_window_crop(res):
    """Threaded video crops this frame above its second screen: the map
    no longer fits, so gl shows the headset its window whole, not two
    screens cut where the map says."""
    fr = last_snap(res)
    if not fr:
        return no_quads(res)
    errors = window_quad(res, fr)
    if 'cropping to 655 rows' not in res.log:
        errors.append('threaded video did not crop the frame')
    return errors


def check_window_menu(res):
    """gl's menu is drawn in its window: open, the headset shows the
    window whole with the menu in it, and no quad of the menu's own; the
    map's screens were cut from the window before."""
    fr = last_snap(res)
    if not fr:
        return no_quads(res)
    errors = window_quad(res, fr)
    if errors:
        return errors
    # The top screen's left eye fills the window's top-left quarter in
    # red; the menu over it leaves no plain red at its middle.
    c, _ = colour(image(res, fr, quads(fr)[0]), 0.25, 0.25)
    if near(c, RED):
        errors.append('the window\'s quad shows the frame without the menu')
    if not any(len(quads(f)) == 3 for f in res.frames):
        errors.append('the screens were never shown before the menu opened')
    return errors


def check_window_menu_overlay(res):
    """gl's overlay is drawn in its window, and so in the window the
    headset shows whole."""
    errors = check_window_menu(res)
    fr = last_snap(res)
    if fr and len(quads(fr)) == 1:
        c, _ = colour(image(res, fr, quads(fr)[0]), 0.05, 0.05)
        if not near(c, MAGENTA):
            errors.append('the headset\'s window shows %s at the overlay, '
                          'want %s' % (c, MAGENTA))
    if not res.shot:
        errors.append('no window screenshot')
    else:
        c, _ = colour(res.shot, 0.05, 0.05)
        if not near(c, MAGENTA):
            errors.append('the window shows %s at the overlay, want %s'
                          % (c, MAGENTA))
    return errors


def check_window_recenter_menu(res):
    """The hotkey recenters while gl's menu is open: the window, whole,
    goes to the turned head's anchor, and the screens are there once the
    menu closes. A press with the menu closed is the second, and no
    press runs twice."""
    mark = res.marks['recenter'] * 1e6
    shown = [f for f in res.frames if quads(f)]
    if not shown:
        return no_quads(res)
    before = [f for f in shown if f['t_us'] < mark]
    after = [f for f in shown
             if mark + 1e6 < f['t_us'] < res.marks['closed'] * 1e6]
    if not before or not after:
        return ['no frames on both sides of the recenter']
    errors = []
    for name, fr, want, turn in (
            ('before', before[-1], (0.0, 0.0, -1.8), None),
            ('after', after[-1], (-1.3, 0.2, 0.3),
             (0.0, 0.70711, 0.0, 0.70711))):
        q = quads(fr)
        if len(q) != 1 or q[0]['flags'] & BLEND:
            return ['want the window whole %s the recenter, got %d quads'
                    % (name, len(q))]
        if not at(q[0], want):
            errors.append('window at %s %s the recenter, want %s'
                          % (q[0]['pose'][:3], name, want))
        if turn and not facing(q[0], turn):
            errors.append('window turned %s, want a quarter turn left'
                          % q[0]['pose'][3:])
    screens = [x for x in quads(shown[-1]) if x['eye'] == 'left']
    if not screens or not at(screens[0], (-1.3, 0.2, 0.3)):
        errors.append('screen 0 at %s after the menu closed, want '
                      '(-1.3, 0.2, 0.3)'
                      % (screens[0]['pose'][:3] if screens else None))
    if res.log.count('Headset recenter requested') != 2:
        errors.append('the hotkey ran %d times for two presses'
                      % res.log.count('Headset recenter requested'))
    return errors


def check_tracked_window_menu(res):
    """gl's menu opens in its window, shown whole in front of the tracked
    eyes, which stay."""
    opened = res.marks.get('menu', 0) * 1e6
    late = [f for f in res.frames if f['t_us'] > opened + 2e6]
    menu = [f for f in late if quads(f)]
    if not menu:
        return ['no frame with the window after the menu opened']
    errors = []
    for f in menu[-3:]:
        kinds = [x['type'] for x in f['layers']]
        if kinds != ['projection', 'quad']:
            errors.append('frame %d shows %s, want the projection and the '
                          'window in front' % (f['n'], kinds))
    snapped = [f for f in menu if f['snap']]
    if not snapped:
        return errors + ['no snapped frame with the window']
    return errors + window_quad(res, snapped[-1])


def check_tracked_window_fallback(res):
    """gl's tracked eyes whose projection the runtime refuses leave its
    window whole, as while the menu is open."""
    size = tracked_size(res)
    if not size:
        return ['the core never saw HMD']
    errors = []
    if '%dx%d' % size not in ['%sx%s' % m for m in
                              SLOT_REFUSED.findall(res.log)]:
        errors.append('the projection slot was not refused: %s'
                      % SLOT_REFUSED.findall(res.log))
    status = STATUS_RE.findall(res.log)
    if not status or status[-1][2] != '1':
        errors.append('the tracked map was not taken: %s' % status[-1:])
    if any(projections(f) for f in res.frames):
        errors.append('a projection was shown')
    fr = last_snap(res)
    if not fr:
        return errors + no_quads(res)
    return errors + window_quad(res, fr)


def check_flat_size_native(res):
    return check_flat_size(res, native=True)


def check_tracked_flat_native(res):
    return check_tracked_flat(res, native=True)


def check_window_menu_closed(res):
    """gl's menu closed again: the map's screens, cut from the window,
    where the window whole was."""
    shown = [f for f in res.frames if quads(f)]
    if not shown:
        return no_quads(res)
    if not any(len(quads(f)) == 1 for f in shown):
        return ['the window was never shown whole for the menu']
    if len(quads(shown[-1])) != 3:
        return ['%d quads after the menu closed, want the three screens'
                % len(quads(shown[-1]))]
    return []


def check_window_reopen(res):
    """gl's menu opened again after a while closed: the window whole
    shows an image released since it reopened, and the screens show
    while it is closed."""
    if 'closed' not in res.marks or 'reopen' not in res.marks:
        return ['marks missing']
    closed = res.marks['closed'] * 1e6 + 500000
    reopen = res.marks['reopen'] * 1e6
    errors = []
    whole = [f for f in res.frames if closed < f['t_us'] < reopen
             and len(quads(f)) == 1]
    if whole:
        errors.append('%d frames showed the window whole while the menu '
                      'was closed' % len(whole))
    shown = [f for f in res.frames if f['t_us'] > reopen
             and len(quads(f)) == 1]
    if not shown:
        return errors + ['the window never came back whole']
    sc = quads(shown[0])[0]['sc']
    fresh = [r for r in res.releases
             if r['sc'] == sc and reopen < r['t_us'] <= shown[0]['t_us']]
    if not fresh:
        errors.append('the window came back with an image released before '
                      'the menu reopened')
    return errors


# The desktop GL lanes: (lane, video driver, context driver).
GL_LANES = (
    ('glcore-egl', 'glcore', 'wayland'),
    ('glcore-glx', 'glcore', 'x'),
    ('gl-egl', 'gl', 'egl_x'),
    ('gl-glx', 'gl', 'x'),
)
# Cases only the GL lanes run.
GL_EXTRA = [
    {'name': 'menu-reopen', 'map': '3ds',
     'settings': {'menu_driver': 'ozone'},
     'steps': [('wait', 6), ('send', 'MENU_TOGGLE'), ('wait', 3),
               ('send', 'MENU_TOGGLE'), ('mark', 'closed'), ('wait', 4),
               ('mark', 'reopen'), ('send', 'MENU_TOGGLE'), ('wait', 3)],
     'check': check_menu_reopen},
    # The head turns, the eyes' images stop coming back, and the user
    # recenters twice, moving between: frames drawn since wait their turn.
    {'name': 'tracked-recenter-busy', 'map': '3ds',
     'options': {'video_views_test_vr': 'local'},
     'steps': [('wait', 6), ('script', 'head 0.5 0.2 0.3 90'), ('wait', 1),
               ('mark', 'busy'), ('script', 'busy eyes'), ('wait', 1),
               ('mark', 'recenter'), ('send', 'HEADSET_RECENTER'),
               ('wait', 1), ('script', 'head 0.1 0.4 0.2 0'), ('wait', 0.5),
               ('send', 'HEADSET_RECENTER'), ('wait', 1.5),
               ('mark', 'free'), ('script', 'busy off'), ('wait', 3)],
     'check': check_tracked_busy},
]
CASES += CUSTOM_CASES
CASES += EDIT_CASES

# The cases each GL driver's lanes run, by name in CASES or GL_EXTRA.
GL_BASE = {
    'glcore': ['3ds-stereo', '3ds-swap', '3ds-stereo-index0',
               '3ds-swap-index0', '3ds-horizontal', 'no-map',
               '3ds-stereo-threaded', 'crop-threaded', 'hw-3ds-stereo',
               'hw-3ds-stereo-threaded', '3ds-output-size',
               'no-map-output-size', '3ds-shrink', 'no-map-shrink',
               '3ds-enlarge', 'menu', 'menu-running', 'menu-small',
               'menu-overlay', 'menu-closes', 'menu-resize',
               'menu-resize-running', 'menu-reopen', 'recenter',
               'recenter-menu', 'runtime-recenter', 'no-runtime',
               'session-fails', 'session-lost', 'exit-window',
               'options-follow-headset', 'pacing', 'focus', 'hw-teardown',
               'hw-teardown-threaded', 'pace-rate-change', 'pace-2',
               'pace-misfit', 'pace-threaded', 'pace-vsync-off',
               'pace-hidden', 'pace-hidden-slow', 'pace-stopping',
               'pace-2-bfi', 'pace-fastforward', 'pace-vrr',
               'pace-window-back', 'pace-loss-waiting', 'pace-auto',
               'pace-headset-choice', 'pace-fixed-rate', 'tracked',
               'tracked-stage', 'tracked-threaded', 'tracked-hw',
               'tracked-paced', 'tracked-nopair', 'flat-hmd-size',
               'tracked-menu', 'tracked-flat', 'tracked-fallback',
               'tracked-recenter', 'tracked-runtime-recenter',
               'tracked-exit', 'tracked-stopping', 'tracked-recenter-paused',
               'tracked-recenter-paused-threaded',
               'tracked-recenter-paused-stage', 'tracked-recenter-menu',
               'tracked-recenter-busy', 'tracked-reinit',
               'tracked-reinit-resized', 'tracked-recenter-unlocated',
               'tracked-reinit-paused', 'tracked-reinit-paused-threaded',
               'tracked-reinit-paused-local',
               'tracked-reinit-paused-local-threaded',
               'tracked-reinit-paused-unlocated',
               'tracked-reinit-paused-nostage', 'tracked-reinit-local',
               'tracked-reinit-local-hw', 'tracked-reinit-local-hw-threaded',
               'tracked-reinit-poses', 'tracked-reinit-poses-threaded',
               'tracked-reinit-paused-hw', 'placement-follow',
               'placement-lazy', 'input-touch', 'input-menu',
               'input-lightgun'],
    # gl shows the headset its window: whole, or an untracked map's
    # screens cut from it, and a tracked map's eyes as the projection.
    'gl': ['3ds-stereo', '3ds-swap', '3ds-stereo-index0', '3ds-horizontal',
           'no-map', '3ds-stereo-threaded', 'crop-threaded',
           'hw-3ds-stereo', 'menu',
           'menu-running', 'menu-overlay', 'menu-closes', 'menu-reopen',
           'recenter', 'recenter-menu', 'runtime-recenter', 'no-runtime',
           'session-fails', 'session-lost', 'exit-window', 'pacing',
           'hw-teardown', 'hw-teardown-threaded', 'pace-rate-change',
           'pace-2', 'pace-misfit', 'pace-threaded', 'pace-vsync-off',
           'pace-hidden', 'pace-stopping', 'pace-fastforward',
           'pace-window-back', 'pace-loss-waiting', 'tracked',
           'tracked-stage', 'tracked-threaded', 'tracked-hw',
           'tracked-paced', 'tracked-nopair', 'flat-hmd-size',
           'tracked-menu', 'tracked-flat', 'tracked-fallback',
           'tracked-recenter', 'tracked-runtime-recenter',
           'tracked-stopping', 'tracked-recenter-paused',
           'tracked-recenter-busy', 'tracked-reinit',
           'tracked-reinit-resized', 'tracked-reinit-paused',
           'tracked-reinit-local', 'tracked-reinit-local-hw',
           'tracked-reinit-poses', 'tracked-reinit-paused-hw'],
}
# The edit cases whose work is a host's or a driver's own: the plan and
# its edit count, the hold and hand-over, the outlines, the handle and
# the beam, fixed slots, the menu's quad. gl draws the menu in its
# window: no menu quad to place, outline or take.
GL_EDIT = ['edit-grab', 'edit-grab-threaded', 'edit-command', 'edit-follow',
           'edit-release-threaded', 'edit-release-moving',
           'edit-regrab-threaded', 'edit-handover-threaded', 'edit-reset',
           'edit-outlines', 'edit-outlines-flush', 'edit-beam',
           'edit-fallback', 'edit-no-map', 'edit-lazy', 'edit-two-hands',
           'edit-choose', 'edit-snap', 'edit-tag', 'edit-tag-narrow',
           'edit-back', 'edit-hinge-3ds', 'edit-hinge-shaken',
           'edit-hinge-3ds-narrow']
GL_EDIT_MENU = ['edit-menu-front', 'edit-menu-size']
for custom_driver in GL_BASE:
    GL_BASE[custom_driver] += [c['name'] for c in CUSTOM_CASES] + GL_EDIT
GL_BASE['glcore'] += GL_EDIT_MENU

# Where a GL host's answer is its own, by driver.
GL_CHECKS = {
    'glcore': {'session-fails': check_session_fails_gl,
               'hw-teardown': check_hw_teardown_gl,
               'hw-teardown-threaded': threaded(check_hw_teardown_gl)},
    'gl': {'custom-repeat': check_custom('repeat', legacy=True),
           'custom-menu': check_custom('menu', legacy=True),
           'custom-tracked': check_custom('tracked', legacy=True),
           'no-map-custom': check_frame_custom(window=True),
           'crop-threaded-custom': threaded(check_window_crop),
           'session-fails': check_session_fails_gl,
           'hw-teardown': check_hw_teardown_gl,
           'hw-teardown-threaded': threaded(check_hw_teardown_gl),
           '3ds-stereo': check_screens(native=True),
           '3ds-swap': check_screens(swap=True, native=True),
           '3ds-stereo-index0': check_screens(native=True),
           '3ds-horizontal': check_screens(horizontal=True, native=True),
           '3ds-stereo-threaded': threaded(check_screens(native=True)),
           'hw-3ds-stereo': check_screens(native=True),
           'no-map': check_window,
           'crop-threaded': threaded(check_window_crop),
           'menu': check_window_menu,
           'menu-running': check_window_menu,
           'menu-overlay': check_window_menu_overlay,
           'menu-closes': check_window_menu_closed,
           'menu-reopen': check_window_reopen,
           'recenter-menu': check_window_recenter_menu,
           'pacing': check_pacing_live,
           'tracked-menu': check_tracked_window_menu,
           'tracked': check_tracked(beside=False),
           'tracked-stage': check_tracked('stage', beside=False),
           'tracked-threaded': threaded(check_tracked(beside=False)),
           'tracked-hw': check_tracked(beside=False),
           'tracked-paced': check_tracked(beside=False),
           'flat-hmd-size': check_flat_size_native,
           'tracked-flat': check_tracked_flat_native,
           'tracked-fallback': check_tracked_window_fallback,
           'tracked-recenter': check_tracked_recenter_eyes,
           'tracked-recenter-paused':
               check_tracked_recenter_paused('local', screens=False),
           'edit-grab': check_edit_grab(menu=False),
           'edit-grab-threaded': threaded(check_edit_grab(menu=False)),
           'edit-command': check_edit_command(menu=False),
           'edit-reset': check_edit_reset(menu=False),
           'edit-outlines': check_edit_outlines(menu=False),
           'edit-fallback': threaded(check_edit_fallback(menu=False)),
           'edit-choose': check_edit_choose(menu=False)},
}
# What a lane cannot run: the unfocus helper sends an X event, and the
# Wayland lane's window is not an X one. glcore's GLX lane runs only a
# smoke test of edit mode, whose work differs from EGL's in nothing but
# the binding.
GL_SKIP = {'glcore-egl': ('focus',),
           'glcore-glx': tuple(n for n in GL_EDIT + GL_EDIT_MENU
                               if n not in ('edit-grab', 'edit-command'))}
# A lane that runs only these of its driver's cases: gl's other binding.
GL_ONLY = {'gl-glx': ('3ds-stereo', 'no-map', 'menu', 'menu-closes',
                      'tracked', 'tracked-menu', 'tracked-recenter',
                      'tracked-reinit', 'pace-2', 'session-lost',
                      'hw-teardown') + tuple(c['name'] for c in CUSTOM_CASES)
            + ('edit-grab',)}


def gl_cases():
    """The GL lanes' clones of their cases, CASES left as they are: a
    core's own Vulkan images become a GL context's, and Vulkan's layers
    stay out."""
    by_name = {c['name']: c for c in CASES + GL_EXTRA}
    out = []
    for lane, driver, context in GL_LANES:
        for name in GL_BASE[driver]:
            if (name in GL_SKIP.get(lane, ())
                    or name not in GL_ONLY.get(lane, (name,))):
                continue
            base = by_name[name]
            c = copy.deepcopy(base)
            c['check'] = GL_CHECKS[driver].get(name, base['check'])
            c['name'] = '%s-%s' % (lane, name)
            c['lane'] = lane
            c['driver'] = driver
            c['context'] = context
            opts = c.setdefault('options', {})
            if opts.get('video_views_test_hw', 'off').startswith('vulkan'):
                # A core context is glcore's whatever the setting says;
                # the gl driver takes a compatibility one.
                opts['video_views_test_hw'] = ('gl' if driver == 'glcore'
                                               else 'gl_compat')
                c['gl_hardware'] = True
            c['env'] = {k: v for k, v in c.get('env', {}).items()
                        if not k.startswith('VK_')}
            # gl's window shows the packed frame whole: the top screen is
            # its top-left quarter.
            if driver == 'gl':
                if name == 'custom-repeat':
                    c['args'] = []
                c['top_px'] = (400, 240)
            # gamescope gives a Wayland client its window back over half
            # a second after a reinit, where an X one takes some 40 ms:
            # the new session's first frames come that much later.
            if context == 'wayland':
                c['reinit_grace'] = 1.5
            c.pop('baseline', None)
            out.append(c)
    return out


def select(names, gl, validate):
    """The cases to run, or an error: every name must be one."""
    if gl and validate:
        return None, '--gl and --validate: validation is Vulkan\'s alone'
    pool = gl_cases() if gl else CASES
    known = [c['name'] for c in pool]
    unknown = [n for n in names if n not in known]
    if unknown:
        return None, 'no such case%s: %s' % (
            's' if len(unknown) > 1 else '', ', '.join(unknown))
    return [c for c in pool if not names or c['name'] in names], None


def unfocus_when(trigger):
    """Inside a case's gamescope: once trigger exists, tell the window with
    the X input focus that it lost it. gamescope gives the real focus
    straight back to its one window, so the event is sent, not caused."""
    if os.environ.get('DISPLAY', '') in ('', ':0'):
        return 99
    deadline = time.time() + 60
    while not os.path.exists(trigger):
        if time.time() > deadline:
            return 1
        time.sleep(0.1)
    x11 = ctypes.CDLL('libX11.so.6')
    x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XGetInputFocus.argtypes = [ctypes.c_void_p,
                                   ctypes.POINTER(ctypes.c_ulong),
                                   ctypes.POINTER(ctypes.c_int)]
    x11.XSendEvent.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int,
                               ctypes.c_long, ctypes.c_void_p]
    x11.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]
    x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
    dpy = x11.XOpenDisplay(None)
    if not dpy:
        return 1
    win, revert = ctypes.c_ulong(0), ctypes.c_int(0)
    x11.XGetInputFocus(dpy, ctypes.byref(win), ctypes.byref(revert))
    ev = XFocusChangeEvent()
    ev.type, ev.window = 10, win.value  # FocusOut
    ev.mode, ev.detail = 0, 3           # NotifyNormal, NotifyNonlinear
    sent = x11.XSendEvent(dpy, win.value, 0, 1 << 21, ctypes.byref(ev))
    x11.XSync(dpy, 0)
    x11.XCloseDisplay(dpy)
    if win.value <= 1 or not sent:  # None or PointerRoot
        return 1
    print('[e2e] FocusOut sent to window 0x%x.' % win.value, flush=True)
    return 0


def main():
    if sys.argv[1:2] == ['--unfocus-when']:
        return unfocus_when(sys.argv[2])
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    validate = '--validate' in sys.argv[1:]
    gl = '--gl' in sys.argv[1:]
    if len(args) < 3:
        print(__doc__)
        return 2
    retroarch = os.path.abspath(args[0])
    root = os.path.realpath(args[1])
    monado = read_env(args[2])
    names = args[3:]
    baseline = []
    if validate:
        baseline = read_baseline(os.path.join(
            os.path.dirname(os.path.abspath(args[2])),
            monado.get('VALIDATION_BASELINE', os.path.join(
                HERE, 'validation-baseline.txt'))))
    cases, error = select(names, gl, validate)
    if error:
        print(error)
        return 2
    os.makedirs(root, exist_ok=True)
    failed = 0
    for case in cases:
        res = run_case(retroarch, root, monado, case, validate)
        errors = case['check'](res) + capture_errors(res, case)
        if 'hung' in res.marks:
            errors.append('RetroArch did not quit')
        errors += ['no frame showed ' + m for m in res.missed]
        if validate:
            extra, known = unexpected(res.log,
                                      baseline + case.get('baseline', []))
            if extra:
                errors.append('validation: ' + ', '.join(extra))
            if not known:
                errors.append('validation: no message at all; did the '
                              'layer load?')
        print('%s %s' % ('FAIL' if errors else 'pass', case['name']))
        for e in errors:
            print('    ' + e)
        failed += bool(errors)
    print('%d failed' % failed)
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
