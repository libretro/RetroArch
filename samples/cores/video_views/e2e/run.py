#!/usr/bin/env python3
"""End-to-end check of RetroArch's view presentation.

Runs the video_views test core in headless gamescope for each case,
takes a GPU screenshot over the network command port (gamescope's own
with the menu open), checks the colour where each view should land,
and checks the views status the core logged; a case is shot once that
status is final, not after a fixed wait. One case also bounds the GPU
memory RetroArch holds, and the pointer cases move the pointer of
gamescope's own X server and look for the menu's cursor under it. The
exact cases check every pixel of
views filled with noise, drawn straight from the frame by the stock
chain and for a preset copied out of it, on glcore once for each way it
copies them, or a software frame's written straight in; and, on glcore,
a frame drawn whole out of a core's larger framebuffer.
In full it needs a GPU, gamescope and a RetroArch build.

With RA_E2E_XVFB=1 each case runs on an Xvfb server of its own instead
of gamescope, with Mesa's software renderers where there is no GPU:
the screen is read with ImageMagick's import and the pointer moved on
that server, several cases at once (RA_E2E_JOBS, 4 by default). The
GPU memory cases are left out there. Xvfb has no XVideo adaptor: for
the xvideo driver RA_E2E_XV_STANDIN names samples/gfx/xv_standin's
library, preloaded to be one. RA_E2E_ONLY, a
regular expression, runs only the cases whose names it finds; CI runs a
set of them that way (Linux-vulkan-validation.yml).

With RA_E2E_WINE=1 the binary is a retroarch.exe run under Wine, on
Xvfb as above, each worker in a copy of WINEPREFIX of its own, with
the core built by make DLL=1 and paths given as
Wine's Z: drive has them; a D3DCompiler_47.dll next to the binary is
taken over Wine's own (Windows-wine-views.yml). The cases that need a
preset run only on drivers whose views take one.

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
import threading
import time
import zlib
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
WINE = os.environ.get('RA_E2E_WINE') == '1'
CORE = os.path.normpath(os.path.join(
    HERE, '..', 'video_views_libretro.' + ('dll' if WINE else 'so')))
W, H = 1600, 960
TOLERANCE = 8
# How long a case runs on once its views status is final: the frames
# that take up the layout, with threaded video's one behind. Wine's
# software D3D draws a frame in far longer.
SETTLE = 2.0 if WINE else 0.5

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

# The same cases drawn in hardware: through a GL compatibility context on
# gl, a core context with each origin on glcore, and into the core's own
# images on vulkan.
# {driver: [(case, video_views_test_hw)]}
HW_CASES = {
    'gl': [(name, 'gl_compat')
           for name in ('3ds-2d', '3ds-sbs-full', '3ds-anaglyph',
                        '3ds-interlaced')],
    'glcore': [(name, hw) for hw in ('gl', 'gl_topleft')
               for name in ('3ds-2d', '3ds-sbs-full', '3ds-anaglyph',
                            '3ds-interlaced')],
    'vulkan': [(name, 'vulkan')
               for name in ('3ds-2d', '3ds-sbs-full', '3ds-anaglyph',
                            '3ds-interlaced', '3ds-sbs-full-threaded')],
    'd3d11': [(name, 'd3d11')
              for name in ('3ds-2d', '3ds-sbs-full', '3ds-anaglyph',
                           '3ds-interlaced', '3ds-sbs-full-threaded')],
    'd3d12': [(name, 'd3d12')
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
# screen must be its source pixel. The stock chain's views are drawn
# straight from the frame; each case runs again with a passthrough
# preset, whose views are copied out of it, and on glcore once more with
# Mesa hiding glCopyImageSubData, so the views are blitted instead. The
# screenshots must match.
# [(name, map option, core options, hw)]
EXACT_SETTINGS = {'video_stereo_mode': '0', 'video_scale_integer': 'true',
                  'video_smooth': 'false'}
NOISE = {'video_views_test_pattern': 'noise'}
NOISE_565 = dict(NOISE, video_views_test_format='rgb565')
NOISE_LARGE = dict(NOISE, video_views_test_max='large')
EXACT_CASES = [
    ('exact-ds', 'ds', NOISE, 'off'),
    ('exact-ds-rgb565', 'ds', NOISE_565, 'off'),
    # The second screen starts 240 columns in.
    ('exact-3ds-rgb565', '3ds', NOISE_565, 'off'),
    # Rows counted from the frame's bottom, and from its top.
    ('exact-3ds', '3ds', NOISE, 'gl'),
    ('exact-3ds', '3ds', NOISE, 'gl_topleft'),
    ('exact-3ds', '3ds', NOISE, 'gl_compat'),
    ('exact-3ds', '3ds', NOISE, 'vulkan'),
    ('exact-3ds', '3ds', NOISE, 'd3d11'),
    ('exact-3ds', '3ds', NOISE, 'd3d12'),
    # No map: the frame drawn whole. A core's GL framebuffer as large as
    # its maximum holds the frame in a corner, so the stock chain would
    # copy it out; glcore draws it straight from there instead.
    ('whole-3ds', 'none', NOISE_LARGE, 'gl'),
    ('whole-3ds', 'none', NOISE_LARGE, 'gl_topleft'),
]
EXACT_HW = {'gl': ('off', 'gl_compat'),
            'glcore': ('off', 'gl', 'gl_topleft'), 'vulkan': ('off', 'vulkan'),
            'gl1': ('off',), 'd3d10': ('off',), 'd3d11': ('off', 'd3d11'),
            'd3d12': ('off', 'd3d12'), 'd3d9_hlsl': ('off',),
            'sdl2': ('off',), 'sdl3': ('off',), 'd3d8': ('off',),
            'gdi': ('off',), 'sdl': ('off',), 'x11': ('off',)}
# Drivers whose views take a preset: on the others a preset draws the
# frame whole, so the cases that need one are left out there.
PRESET_DRIVERS = ('glcore', 'vulkan', 'd3d10', 'd3d11', 'd3d12')
# gl takes GLSL presets: views through two passes, the first into a
# framebuffer at the view's size or its placement's, and through one,
# each with earlier frames and feedback of its own.
GL_PRESETS = [('-preset', os.path.join(HERE, 'pass.glslp')),
              ('-preset-viewport', os.path.join(HERE, 'pass_viewport.glslp')),
              ('-preset-one', os.path.join(HERE, 'pass_one.glslp')),
              # Each view's own earlier frames and feedback
              ('-preset-prev', os.path.join(HERE, 'prev.glslp')),
              ('-preset-prev-one', os.path.join(HERE, 'prev_one.glslp')),
              ('-preset-feedback', os.path.join(HERE, 'feedback.glslp'))]
# d3d9_hlsl takes Cg presets the same, but for feedback, which it has not.
D3D9_PRESETS = [('-preset', os.path.join(HERE, 'pass.cgp')),
                ('-preset-viewport', os.path.join(HERE, 'pass_viewport.cgp')),
                ('-preset-one', os.path.join(HERE, 'pass_one.cgp')),
                ('-preset-prev', os.path.join(HERE, 'prev.cgp')),
                ('-preset-prev-one', os.path.join(HERE, 'prev_one.cgp'))]
# {driver: its presets of its own shader language}
DRIVER_PRESETS = {'gl': GL_PRESETS, 'd3d9_hlsl': D3D9_PRESETS}
GL_PRESET_CASES = ('3ds-sbs-full', '3ds-top-bottom', '3ds-anaglyph',
                   '3ds-interlaced', 'ds-horizontal')
# Drivers whose views' presets have each view of a software frame written
# straight out of it into a texture of its own, not read in place. Not
# vulkan's, which does from a staging buffer but copies from a linear
# image, as for lavapipe's XRGB8888 frames.
WRITE_SW_VIEWS = ('gl', 'glcore', 'd3d9_hlsl', 'd3d10', 'd3d11', 'd3d12')
# Drivers whose views' presets read a software frame's views where they
# lie when they can.
SW_IN_PLACE_DRIVERS = ('glcore', 'vulkan')
WRITE_SW_RE = re.compile(
    r'\] Views\' presets have each view of a software frame written '
    r'straight in\.')
# Drivers on shader model 4, where the rect presets' gathers of more than
# the red component do not compile.
SM4_DRIVERS = ('d3d10',)
# Drivers that draw anaglyph's frames whole: no blend of theirs makes its
# colours, which mix the channels.
WHOLE_ANAGLYPH_DRIVERS = ('d3d8', 'sdl2', 'sdl3')
# Drivers left out of the pointer cases: on Xvfb SDL 1.2's fullscreen
# window (SDL2's sdl12-compat) reports no pointer motion, even with a
# window manager.
NO_POINTER_DRIVERS = ('sdl',)
# Filtered, a view drawn straight from the frame must stop at its edge
# as its copy does: the ds screens touch in the frame, so a bottom row
# blended with the other screen's top shows. Compared, not checked
# pixel for pixel. [(name, map option, core options, hw)]
SMOOTH_CASES = [
    ('smooth-ds', 'ds', NOISE, 'off'),
    ('smooth-3ds', '3ds', NOISE, 'gl'),
    ('smooth-3ds', '3ds', NOISE, 'vulkan'),
    ('smooth-3ds', '3ds', NOISE, 'd3d11'),
    ('smooth-3ds', '3ds', NOISE, 'd3d12'),
    ('smooth-whole-3ds', 'none', NOISE_LARGE, 'gl'),
]
SMOOTH_SETTINGS = dict(EXACT_SETTINGS, video_smooth='true')
LINEAR_PRESET = os.path.join(HERE, 'history_linear.slangp')
# Drivers that draw the stock chain's views straight from the frame,
# and that draw a frame whole straight from a larger texture.
DIRECT_DRIVERS = ('gl1', 'gl', 'glcore', 'vulkan', 'd3d9_hlsl', 'd3d10',
                  'd3d11', 'd3d12', 'sdl2', 'sdl3', 'd3d8', 'gdi', 'sdl',
                  'x11', 'xvideo')
DIRECT_RE = re.compile(r'\] Views drawn straight from the frame\.')
PRESET_RE = re.compile(r'\] Views drawn through the preset')
WHOLE_DRIVERS = ('glcore',)
WHOLE_RE = re.compile(r'\] Frames drawn straight from the core\'s texture\.')
# The core's 2D maps, as (x, y, width, height) in the packed frame.
EXACT_VIEWS = {'ds': [(0, 0, 256, 192), (0, 192, 256, 192)],
               '3ds': [(0, 0, 400, 240), (240, 240, 320, 240)]}
# Without a map the core still draws its 2D 3ds screens.
EXACT_VIEWS['none'] = EXACT_VIEWS['3ds']
NOISE_COLOURS = [BLACK, RED, GREEN, BLUE, (0, 255, 255), (255, 0, 255),
                 YELLOW]
# A preset reads a frame inside a larger texture where it lies. Each
# preset runs on the core's frame drawn in hardware into a framebuffer
# as large as its maximum, and again on its software frame, which comes
# on its own: the screenshots must match, but for linear filtering with
# border wrap, whose blend of the edge texels with the border is the
# shader's and may round a step from the sampler's.
# [(preset, read in place, tolerance)]
RECT_PRESETS = [('rect_nearest.slangp', True, 0),
                ('rect_nearest_edge.slangp', True, 0),
                ('rect_nearest_repeat.slangp', True, 0),
                ('rect_nearest_mirror.slangp', True, 0),
                ('rect_linear.slangp', True, 0),
                ('rect_linear_border.slangp', True, 1),
                ('rect_full.slangp', True, 0),
                ('rect_two.slangp', True, 0)]
RECT_HW = {'glcore': ('gl', 'gl_topleft')}
IN_PLACE_RE = re.compile(
    r'\[GLCore\] Preset reads frames where the core leaves them\.')
# A view's preset reads the view where it lies in the frame. Each runs
# a rect preset, read in place, and its rect_hist twin, which reads a
# frame of history too and so has the views copied out: the screenshots
# must match, linear with border to within a step as above.
# [(name, map option, core options, hw)]
VIEW_RECT_CASES = [('view-rect-ds', 'ds', NOISE, 'off'),
                   ('view-rect-ds-rgb565', 'ds', NOISE_565, 'off'),
                   ('view-rect-3ds', '3ds', NOISE, 'gl'),
                   ('view-rect-3ds', '3ds', NOISE, 'gl_topleft'),
                   ('view-rect-3ds', '3ds', NOISE, 'vulkan'),
                   ('view-rect-3ds', '3ds', NOISE, 'd3d11'),
                   ('view-rect-3ds', '3ds', NOISE, 'd3d12')]
VIEW_RECT_PRESETS = (('nearest', 0), ('nearest_repeat', 0), ('linear', 0),
                     ('linear_border', 1))
VIEW_IN_PLACE_RE = re.compile(
    r'\] Views\' presets read the frame where it lies\.')

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


# Ports and displays handed to cases running side by side.
TAKEN_LOCK = threading.Lock()
TAKEN_PORTS = set()
TAKEN_DISPLAYS = set()


def free_port():
    with TAKEN_LOCK:
        while True:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.bind(('127.0.0.1', 0))
            port = s.getsockname()[1]
            s.close()
            if port not in TAKEN_PORTS:
                TAKEN_PORTS.add(port)
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
    if WINE:
        cfg = {k: winpath(v) for k, v in cfg.items()}
        cfg.update(video_context_driver='', input_driver='dinput')
    cfg.update(settings)
    with open(path, 'w') as f:
        for k in sorted(cfg):
            f.write('%s = "%s"\n' % (k, cfg[k]))


def winpath(v):
    """An absolute path as Wine's Z: drive has it; anything else as is."""
    return 'Z:' + v if WINE and v.startswith('/') else v


def expected_status(driver, settings):
    """(presents, stereo) the frontend should report for these settings."""
    return (1, 0 if settings.get('video_stereo_mode', '0') == '0' else 1)


def wait_for_views(p, d, timeout=60):
    """Wait for initialization to reach the core before timing the case."""
    deadline = time.monotonic() + timeout
    while p.poll() is None:
        for name in ('run.log', 'retroarch.log'):
            path = os.path.join(d, name)
            if os.path.exists(path):
                with open(path, errors='replace') as f:
                    if STATUS_RE.search(f.read()):
                        return True
        if time.monotonic() >= deadline:
            return False
        time.sleep(0.1)



def startup_stack(p, d):
    """Keep the native initialization fault site before terminating a case."""
    if WINE or not XVFB or os.name != 'posix' or p.poll() is not None:
        return
    debugger = shutil.which('gdb')
    if not debugger:
        return
    command = [debugger, '-batch', '-ex', 'set pagination off',
               '-ex', 'thread apply all bt', '-p', str(p.pid)]
    if os.environ.get('GITHUB_ACTIONS') == 'true' and os.geteuid() != 0:
        command = ['sudo', '-n'] + command
    with open(os.path.join(d, 'startup-stack.log'), 'w') as log:
        try:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
                           timeout=15)
        except (OSError, subprocess.TimeoutExpired) as error:
            log.write(str(error) + '\n')


def last_status(d):
    """The last views status the core logged, or None."""
    found = None
    for name in ('run.log', 'retroarch.log'):
        path = os.path.join(d, name)
        if os.path.exists(path):
            with open(path, errors='replace') as f:
                for m in STATUS_RE.finditer(f.read()):
                    found = tuple(int(g) for g in m.groups())
    return found


def wait_settled(p, d, want, timeout=20):
    """Until the core reports the views status want, and the frames
    that take up its layout are drawn; at most timeout seconds, after
    which the case's checks report what went wrong."""
    deadline = time.monotonic() + timeout
    while p.poll() is None and time.monotonic() < deadline:
        found = last_status(d)
        if found and found[:2] == want:
            time.sleep(SETTLE)
            return
        time.sleep(0.05)


def png_done(path):
    """Whether the PNG at path is written to its end."""
    try:
        with open(path, 'rb') as f:
            f.seek(-12, os.SEEK_END)
            return f.read(12)[4:8] == b'IEND'
    except OSError:
        return False


# What RetroArch logs when the driver asked for is not in the build: it
# goes on with another, whose results would pass for the one asked for.
MISSING_DRIVER_RE = re.compile(r"Couldn't find any video driver named")


def status_errors(d, driver, settings):
    """Check the last views status the core logged."""
    found = last_status(d)
    if direct_drawn(d, MISSING_DRIVER_RE):
        return ['video driver "%s" is not in this build' % driver]
    if found is None:
        return ['no "[video_views] presents=" line in the logs']
    want = expected_status(driver, settings)
    errors = []
    if found[:2] != want:
        errors.append('status presents=%d stereo=%d, want presents=%d '
                      'stereo=%d' % (found[0], found[1], want[0], want[1]))
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
    # Unset means autolaunch, not no session bus.
    env['DBUS_SESSION_BUS_ADDRESS'] = 'unix:path=' + os.path.join(
        env['XDG_RUNTIME_DIR'], 'no-session-bus')
    return env


XVFB = WINE or os.environ.get('RA_E2E_XVFB') == '1'
# samples/gfx/xv_standin's library, for the xvideo driver on Xvfb
XV_STANDIN = os.environ.get('RA_E2E_XV_STANDIN')
# Cases run side by side, each on an Xvfb of its own, four at a time:
# most of a case is waiting, so more than the cores still gains. One at
# a time in gamescope, whose screenshot names one display. Under Wine
# each worker has a prefix of its own: cases sharing a wineserver move
# each other's pointer. RA_E2E_JOBS sets how many.
JOBS = int(os.environ.get('RA_E2E_JOBS') or 0) or (4 if XVFB else 1)
WINE_PREFIXES = []
WINE_COPIES = []
WINE_WORKER = threading.local()
ONLY = os.environ.get('RA_E2E_ONLY')


def wanted(name):
    """Whether RA_E2E_ONLY lets case @name run."""
    return not ONLY or re.search(ONLY, name) is not None


def wine_prefixes():
    """Gives each worker a copy of the prepared prefix, or the prefix
    itself when there is one worker. The copies go in WINE_COPIES."""
    base = os.environ.get('WINEPREFIX') or os.path.expanduser('~/.wine')
    if JOBS == 1:
        WINE_PREFIXES.append(base)
        return
    prepared = os.path.isdir(base)
    if prepared:
        subprocess.run(['wineserver', '-w'],
                       env=dict(os.environ, WINEPREFIX=base))
    for _ in range(JOBS):
        p = os.path.join(tempfile.mkdtemp(prefix='ra-e2e-wine-'), 'prefix')
        WINE_COPIES.append(p)
        # With no prefix prepared, Wine makes each worker its own.
        if prepared:
            shutil.copytree(base, p, symlinks=True)
        WINE_PREFIXES.append(p)


def wine_prefix():
    """This worker's prefix."""
    if not hasattr(WINE_WORKER, 'prefix'):
        with TAKEN_LOCK:
            WINE_WORKER.prefix = WINE_PREFIXES.pop()
    return WINE_WORKER.prefix


def free_display():
    """An X display number nothing serves, kept for the caller until
    release_display()."""
    with TAKEN_LOCK:
        for n in range(90, 190):
            if n not in TAKEN_DISPLAYS and \
                    not os.path.exists('/tmp/.X11-unix/X%d' % n) and \
                    not os.path.exists('/tmp/.X%d-lock' % n):
                TAKEN_DISPLAYS.add(n)
                return n
    raise RuntimeError('no free X display')


# Drivers whose fullscreen window a window manager sizes: SDL 1.2's
# (SDL2's sdl12-compat) stays 1x1 without one.
WM_DRIVERS = ('sdl',)


def wm_ready_probe(timeout=10):
    """Require the WM to manage a new client, not just publish its identity."""
    x11 = ctypes.CDLL('libX11.so.6')
    pointer, window_type = ctypes.c_void_p, ctypes.c_ulong
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XOpenDisplay.restype = pointer
    x11.XDefaultRootWindow.argtypes = [pointer]
    x11.XDefaultRootWindow.restype = window_type
    x11.XCreateSimpleWindow.argtypes = [
        pointer, window_type, ctypes.c_int, ctypes.c_int,
        ctypes.c_uint, ctypes.c_uint, ctypes.c_uint, window_type, window_type]
    x11.XCreateSimpleWindow.restype = window_type
    for name in ('XMapWindow', 'XDestroyWindow'):
        getattr(x11, name).argtypes = [pointer, window_type]
    for name in ('XFlush', 'XCloseDisplay'):
        getattr(x11, name).argtypes = [pointer]
    display = x11.XOpenDisplay(os.environ['DISPLAY'].encode())
    if not display:
        return 1
    window = x11.XCreateSimpleWindow(
        display, x11.XDefaultRootWindow(display), 0, 0, 32, 32, 0, 0, 0)
    try:
        x11.XMapWindow(display, window)
        x11.XFlush(display)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            state = subprocess.run(
                ['xprop', '-id', hex(window), 'WM_STATE'],
                stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                timeout=3, text=True)
            if state.returncode == 0 and 'window state: Normal' in state.stdout:
                return 0
            time.sleep(0.02)
        return 1
    finally:
        x11.XDestroyWindow(display, window)
        x11.XCloseDisplay(display)


def wm_start(env):
    """openbox on env's display."""
    p = subprocess.Popen(['openbox'], stdout=subprocess.DEVNULL,
                         stderr=subprocess.DEVNULL, env=env,
                         start_new_session=True)
    deadline = time.monotonic() + 30
    try:
        while p.poll() is None and time.monotonic() < deadline:
            # A probe that is slow to answer on a loaded runner, with
            # the other cases' servers and players beside it, is not an
            # answer: ask again until the deadline.
            try:
                probe = subprocess.run(
                    ['xprop', '-root', '_NET_SUPPORTING_WM_CHECK'], env=env,
                    stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                    timeout=5, text=True)
            except subprocess.TimeoutExpired:
                continue
            if probe.returncode == 0 and 'window id #' in probe.stdout:
                ready = subprocess.run(
                    [sys.executable, os.path.abspath(__file__), '--wm-ready'],
                    env=env, timeout=max(0.1, deadline - time.monotonic()))
                if ready.returncode == 0 and p.poll() is None:
                    return p
            time.sleep(0.05)
        raise RuntimeError('openbox did not become ready on ' + env['DISPLAY'])
    except BaseException:
        p.terminate()
        try:
            p.wait(5)
        except subprocess.TimeoutExpired:
            p.kill()
            p.wait()
        raise


def release_display(display):
    with TAKEN_LOCK:
        TAKEN_DISPLAYS.discard(int(display.lstrip(':')))


def xvfb_start(env):
    """An Xvfb of the window's size: its process and DISPLAY."""
    n = free_display()
    # A client disconnect must not reset the server during startup.
    p = subprocess.Popen(['Xvfb', ':%d' % n, '-screen', '0',
                          '%dx%dx24' % (W, H), '-nolisten', 'tcp', '-noreset'],
                         stdout=subprocess.DEVNULL,
                         stderr=subprocess.DEVNULL, env=env,
                         start_new_session=True)
    deadline = time.time() + 10
    while (p.poll() is None and time.time() < deadline
           and not os.path.exists('/tmp/.X11-unix/X%d' % n)):
        time.sleep(0.1)
    time.sleep(0.5)
    if p.poll() is not None or not os.path.exists('/tmp/.X11-unix/X%d' % n):
        if p.poll() is None:
            p.terminate()
            try:
                p.wait(5)
            except subprocess.TimeoutExpired:
                p.kill()
                p.wait()
        release_display(':%d' % n)
        raise RuntimeError('Xvfb did not become ready on :%d' % n)
    return p, ':%d' % n


def gamescope_shot(d, env, name='menu.png'):
    """What gamescope shows, menu included, in d/name."""
    out = os.path.join(d, name)
    if XVFB:
        try:
            # PNG24: read_png takes 8-bit RGB, not a palette.
            subprocess.run(['import', '-display', env['DISPLAY'], '-window',
                            'root', 'PNG24:' + out], stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL, timeout=20)
        except (OSError, subprocess.TimeoutExpired):
            return None
        return out if os.path.exists(out) else None
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
    # Xvfb has no XVideo adaptor: the stand-in's, preloaded
    if driver == 'xvideo' and XV_STANDIN:
        env['LD_PRELOAD'] = XV_STANDIN
    xvfb = wm = None
    if XVFB:
        xvfb, env['DISPLAY'] = xvfb_start(env)
        if driver in WM_DRIVERS:
            # sdl12-compat otherwise creates a GL renderer on Xvfb.
            env['SDL_RENDER_DRIVER'] = 'software'
            env['SDL_FRAMEBUFFER_ACCELERATION'] = '0'
            try:
                wm = wm_start(env)
            except Exception:
                xvfb.terminate()
                xvfb.wait(5)
                release_display(env['DISPLAY'])
                shutil.rmtree(env['XDG_RUNTIME_DIR'], ignore_errors=True)
                raise
        cmd = cmd[cmd.index('--') + 1:]
    if WINE:
        env['WINEPREFIX'] = wine_prefix()
        env['WINEDEBUG'] = '-all'
        # A D3DCompiler_47.dll next to the binary (vkd3d-utils, say)
        # over Wine's own.
        env['WINEDLLOVERRIDES'] = 'd3dcompiler_47=n,b'
        cmd = cmd[:cmd.index(retroarch)] + [
            'wine', retroarch, '--config', winpath(cfg), '-L',
            winpath(CORE), '--verbose'] + [
                a.replace('=/', '=Z:/') for a in args]
    log = open(os.path.join(d, 'run.log'), 'w')
    p = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT,
                         env=env, start_new_session=True)
    shot = None
    try:
        if not wait_for_views(p, d):
            startup_stack(p, d)
        if gpu is None:
            wait_settled(p, d, expected_status(driver, settings))
        settle = time.time() + 5
        while gpu is not None and time.time() < settle:
            time.sleep(0.5)
            pid = retroarch_pid(p.pid, cfg)
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
            time.sleep(0.05)
            pngs = [x for x in os.listdir(os.path.join(d, 'shots'))
                    if x.endswith('.png')]
            if pngs and png_done(os.path.join(d, 'shots', pngs[0])):
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
        if wm:
            wm.terminate()
            try:
                wm.wait(5)
            except subprocess.TimeoutExpired:
                wm.kill()
        if xvfb:
            xvfb.terminate()
            try:
                xvfb.wait(5)
            except subprocess.TimeoutExpired:
                xvfb.kill()
            release_display(env['DISPLAY'])
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


def direct_drawn(d, regex):
    """Whether the log says the frame or its views were drawn straight
    from the frame's texture, by regex."""
    for name in ('run.log', 'retroarch.log'):
        path = os.path.join(d, name)
        if os.path.exists(path):
            with open(path, errors='replace') as f:
                if regex.search(f.read()):
                    return True
    return False


def run_exact_case(retroarch, root, driver, case, suffix='',
                   env_extra=None, path=None, preset=None, exact=True,
                   in_place=False):
    """The errors, and the screenshot's rows for comparing runs. path is
    how glcore must say it copies views. preset, a preset's path, has the
    views copied; without one a driver in DIRECT_DRIVERS must draw them
    straight from the frame. exact checks each view pixel for pixel.
    in_place: the preset can read views where they lie."""
    name, mapopt, opts, hw = case
    name += suffix
    settings = EXACT_SETTINGS if exact else SMOOTH_SETTINGS
    args = ()
    if preset:
        settings = dict(settings, video_shader_enable='true')
        args = ('--set-shader=' + preset,)
    errors = run_case(retroarch, root, driver,
                      (name, mapopt, settings, []), hw, opts=opts,
                      args=args, env_extra=env_extra)
    d = case_dir(root, driver, name, hw)
    if path and copy_path(d) != path:
        errors.append('views copied by %s, want %s' % (copy_path(d), path))
    if mapopt == 'none':
        regex, drivers, what = WHOLE_RE, WHOLE_DRIVERS, 'frame'
    else:
        regex, drivers, what = DIRECT_RE, DIRECT_DRIVERS, 'views'
    direct = direct_drawn(d, regex)
    if direct != (driver in drivers and not preset):
        errors.append('%s %s straight from the frame' %
                      (what, 'drawn' if direct else 'not drawn'))
    written = bool(preset) and mapopt != 'none' and hw == 'off' \
        and driver in WRITE_SW_VIEWS \
        and not (in_place and driver in SW_IN_PLACE_DRIVERS)
    if driver in WRITE_SW_VIEWS and direct_drawn(d, WRITE_SW_RE) != written:
        errors.append('views %s straight in'
                      % ('not written' if written else 'written'))
    shots = sorted(x for x in os.listdir(os.path.join(d, 'shots'))
                   if x.endswith('.png'))
    if not shots:
        return errors, None
    w, h, bpp, rows = read_png(os.path.join(d, 'shots', shots[0]))
    if (w, h) != (W, H):
        return errors, None
    rows = [bytes(rgb_row(rows, bpp, y, 0, W)) for y in range(H)]
    if exact:
        errors += exact_errors(rows, 3, EXACT_VIEWS[mapopt])
    return errors, rows


def rows_differ(a, b, tolerance):
    """How many pixels of two screenshots' RGB rows differ by more than
    tolerance in a channel."""
    bad = 0
    for ra, rb in zip(a, b):
        if ra == rb:
            continue
        for i in range(0, len(ra), 3):
            if any(abs(ra[i + c] - rb[i + c]) > tolerance for c in range(3)):
                bad += 1
    return bad


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
    deadline = time.monotonic() + 5
    dpy = x11.XOpenDisplay(None)
    while not dpy and time.monotonic() < deadline:
        time.sleep(0.05)
        dpy = x11.XOpenDisplay(None)
    if not dpy:
        print('[e2e] pointer helper cannot open DISPLAY=%s'
              % os.environ['DISPLAY'], flush=True)
        return 1
    x11.XDefaultRootWindow.restype = ctypes.c_ulong
    x11.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
    root = (x11.XDefaultRootWindow(dpy) if XVFB
            else gamescope_root(x11, dpy))
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


def report(name, errors):
    """A case's result lines."""
    return ['%s %s' % ('FAIL' if errors else 'pass', name)] + \
        ['    ' + e for e in errors]


def driver_jobs(retroarch, root, driver):
    """The driver's cases as jobs that can run side by side, each
    returning its result lines and how many of them failed."""
    jobs = []

    def plain(case, hw):
        errors = run_case(retroarch, root, driver, case, hw)
        return report('%s/%s%s' % (driver, case[0], '' if hw == 'off'
                                   else ' (hw ' + hw + ')'), errors), \
            bool(errors)

    def own_preset(case, suffix, preset):
        name, mapopt, settings, points = case
        c = (name + suffix, mapopt,
             dict(settings, video_shader_enable='true'), points)
        errors = run_case(retroarch, root, driver, c,
                          args=('--set-shader=' + preset,))
        if not direct_drawn(case_dir(root, driver, c[0]), PRESET_RE):
            errors.append('views not drawn through the preset')
        if not direct_drawn(case_dir(root, driver, c[0]), WRITE_SW_RE):
            errors.append('views not written straight in')
        return report('%s/%s' % (driver, c[0]), errors), bool(errors)

    def exact(case):
        hw = '' if case[3] == 'off' else ' (hw ' + case[3] + ')'
        copy = 'glCopyImageSubData' if driver == 'glcore' else None
        runs = [('', None, copy, None)]
        if driver in PRESET_DRIVERS:
            runs.append(('-preset', None, copy, HISTORY_PRESET))
        if driver == 'glcore':
            runs.append(('-preset-blit', NO_COPY_IMAGE,
                         'framebuffer blits', HISTORY_PRESET))
        runs += [(suffix, None, None, preset)
                 for suffix, preset in DRIVER_PRESETS.get(driver, [])]
        first, lines, failed = None, [], 0
        for suffix, env_extra, path, preset in runs:
            errors, rows = run_exact_case(
                retroarch, root, driver, case, suffix, env_extra, path,
                preset)
            if first is None:
                first = rows
            elif rows and first and rows != first:
                errors.append('screenshot differs from %s/%s\'s'
                              % (driver, case[0]))
            lines += report('%s/%s%s%s' % (driver, case[0], suffix, hw),
                            errors)
            failed += bool(errors)
        return lines, failed

    def rect(preset, in_place, tolerance):
        name = 'rect-' + preset[:-len('.slangp')]
        path = os.path.join(HERE, preset)
        errors, first = run_exact_case(
            retroarch, root, driver, (name, 'none', NOISE_LARGE, 'off'),
            preset=path, exact=False)
        lines = report('%s/%s' % (driver, name), errors)
        failed = bool(errors)
        for hw in RECT_HW[driver]:
            case = (name, 'none', NOISE_LARGE, hw)
            errors, rows = run_exact_case(retroarch, root, driver, case,
                                          preset=path, exact=False)
            d = case_dir(root, driver, name, hw)
            if direct_drawn(d, IN_PLACE_RE) != in_place:
                errors.append('frame %s where it lies' %
                              ('not read' if in_place else 'read'))
            # A copied frame upside down filters to within a step of the
            # software frame's: weights round the other way.
            bad = rows_differ(rows, first, tolerance if in_place else 1) \
                if rows and first else None
            if bad is None:
                errors.append('no screenshot to compare')
            elif bad:
                errors.append('%d pixels differ from the software '
                              'frame\'s' % bad)
            lines += report('%s/%s (hw %s)' % (driver, name, hw), errors)
            failed += bool(errors)
        return lines, failed

    def view_rect(case, mode, tolerance):
        name, mapopt, opts, hw = case
        rows = {}
        errors = []
        for kind, in_place in (('rect', True), ('rect_hist', False)):
            preset = os.path.join(HERE, '%s_%s.slangp' % (kind, mode))
            c = ('%s-%s-%s' % (name, kind, mode), mapopt, opts, hw)
            e, rows[kind] = run_exact_case(retroarch, root, driver, c,
                                           preset=preset, exact=False,
                                           in_place=in_place)
            errors += e
            d = case_dir(root, driver, c[0], hw)
            sw_out = hw == 'off' and driver not in SW_IN_PLACE_DRIVERS
            if direct_drawn(d, VIEW_IN_PLACE_RE) != (in_place and not sw_out):
                errors.append('%s: views %s where they lie' % (
                    kind, 'not read' if in_place else 'read'))
        if not rows['rect'] or not rows['rect_hist']:
            errors.append('no screenshot to compare')
        elif rows_differ(rows['rect'], rows['rect_hist'], tolerance):
            errors.append('%d pixels differ from the copied views\''
                          % rows_differ(rows['rect'],
                                        rows['rect_hist'], tolerance))
        return report('%s/%s-%s%s' % (driver, name, mode, '' if hw == 'off'
                                      else ' (hw ' + hw + ')'), errors), \
            bool(errors)

    def smooth(case):
        hw = '' if case[3] == 'off' else ' (hw ' + case[3] + ')'
        errors, rows = run_exact_case(retroarch, root, driver, case,
                                      exact=False)
        copy_errors, copy_rows = run_exact_case(
            retroarch, root, driver, case, '-preset',
            preset=LINEAR_PRESET, exact=False)
        errors += copy_errors
        if not rows or not copy_rows:
            errors.append('no screenshot to compare')
        else:
            bad = rows_differ(rows, copy_rows, TOLERANCE)
            if bad:
                errors.append('%d pixels differ from the preset\'s copies'
                              % bad)
        return report('%s/%s%s' % (driver, case[0], hw), errors), \
            bool(errors)

    def memory(case):
        errors, peak = run_memory_case(retroarch, root, driver, case)
        return report('%s/%s (hw %s, GPU %s MiB)'
                      % (driver, case[0], MEMORY_HW[driver],
                         '?' if peak is None else '%d' % peak), errors), \
            bool(errors)

    def menu_case(case):
        errors = run_menu_case(retroarch, root, driver, case)
        return report('%s/%s' % (driver, case[0]), errors), bool(errors)

    def pointer(menu, case):
        errors = run_pointer_case(retroarch, root, driver, menu, case)
        return report('%s/%s-%s' % (driver, case[0], menu), errors), \
            bool(errors)

    runs = [(case, 'off') for case in CASES
            if not (driver in WHOLE_ANAGLYPH_DRIVERS and 'anaglyph' in case[0])]
    runs += [(case, hw) for name, hw in HW_CASES.get(driver, [])
             for case in CASES if case[0] == name]
    if driver == 'vulkan':
        runs += [(case, 'vulkan') for case in VULKAN_CROP_CASES]
    jobs += [lambda c=c, hw=hw: plain(c, hw)
             for c, hw in runs if wanted(c[0])]
    jobs += [lambda c=c, x=x, p=p: own_preset(c, x, p)
             for c in (CASES if driver in DRIVER_PRESETS else [])
             if c[0] in GL_PRESET_CASES and wanted(c[0])
             for x, p in DRIVER_PRESETS[driver]]
    jobs += [lambda c=c: exact(c) for c in EXACT_CASES
             if c[3] in EXACT_HW.get(driver, ()) and wanted(c[0])]
    jobs += [lambda p=p, i=i, t=t: rect(p, i, t)
             for p, i, t in (RECT_PRESETS if driver in RECT_HW else [])
             if wanted('rect-' + p[:-len('.slangp')])]
    jobs += [lambda c=c, m=m, t=t: view_rect(c, m, t)
             for c in VIEW_RECT_CASES
             if c[3] in EXACT_HW.get(driver, ())
             and driver in PRESET_DRIVERS and driver not in SM4_DRIVERS
             and wanted(c[0])
             for m, t in VIEW_RECT_PRESETS]
    jobs += [lambda c=c: smooth(c) for c in SMOOTH_CASES
             if c[3] in EXACT_HW.get(driver, ())
             and driver in PRESET_DRIVERS and wanted(c[0])]
    jobs += [lambda c=c: memory(c)
             for c in (MEMORY_CASES if driver in MEMORY_HW and not XVFB
                       else []) if wanted(c[0])]
    jobs += [lambda c=c: menu_case(c) for c in MENU_CASES if wanted(c[0])]
    jobs += [lambda m=m, c=c: pointer(m, c) for m in sorted(POINTER_MENUS)
             for c in POINTER_CASES
             if wanted(c[0]) and driver not in NO_POINTER_DRIVERS]
    return jobs


def main():
    if len(sys.argv) == 2 and sys.argv[1] == '--wm-ready':
        return wm_ready_probe()
    if sys.argv[1:2] == ['--pointer']:
        return pointer_helper(sys.argv[2])
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    if XVFB and not shutil.which('import'):
        # The menu and pointer cases read the screen through
        # ImageMagick's import.
        print('import (ImageMagick) is needed under Xvfb')
        return 1
    retroarch = os.path.abspath(sys.argv[1])
    root = os.path.realpath(sys.argv[2])
    os.makedirs(root, exist_ok=True)
    jobs = [j for driver in sys.argv[3:]
            for j in driver_jobs(retroarch, root, driver)]
    failed = 0
    try:
        if WINE:
            wine_prefixes()
        with ThreadPoolExecutor(max_workers=JOBS) as pool:
            for future in [pool.submit(j) for j in jobs]:
                lines, n = future.result()
                for line in lines:
                    print(line, flush=True)
                failed += n
    finally:
        for p in WINE_COPIES:
            subprocess.run(['wineserver', '-k'],
                           env=dict(os.environ, WINEPREFIX=p),
                           stderr=subprocess.DEVNULL)
            shutil.rmtree(os.path.dirname(p), ignore_errors=True)
    print('%d failed' % failed)
    return 1 if failed else 0

if __name__ == '__main__':
    sys.exit(main())
