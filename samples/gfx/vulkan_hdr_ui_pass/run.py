#!/usr/bin/env python3
"""The Vulkan driver's HDR10 frame under the validation layer, end to end.

The shipping frontend, built VULKAN_DEBUG=1, runs a software core on
lavapipe under an Xvfb of depth 30. fake_hdr_layer.c pairs the surface's
10-bit format with the HDR10 colour space, so the driver selects an HDR10
swapchain and takes its whole HDR path. Each case then draws the frame a
different way:

  shader-menu        a two-pass preset and the menu open: the game is
                     rendered to the offscreen buffer and composited,
                     then the UI is drawn into the SDR UI layer and
                     composited over it.
  shader-statistics  the same with the statistics overlay as the UI.
  shader-message     the same with an OSD message (the FPS counter).
  shader-plain       the preset with no UI: no UI layer at all.
  shader-menu-threaded
                     shader-menu with the driver on the video thread.
  stock-menu         no preset, the menu open: the frame goes straight
                     to the swapchain and the UI layer follows.

With a preset and any UI, the UI layer's render pass was left open
before the composite recorded its barriers and began its own render
pass inside it; the validation layer reports both. Every case must
select an HDR10 swapchain, take the HDR path it is meant to, and log
no validation error.

usage: run.py --retroarch ./retroarch --log-dir DIR [--only REGEX]
Needs Xvfb, lavapipe (VK_DRIVER_FILES is set to it when it is found)
and the Khronos validation layer. Exits 77 when Xvfb is missing.
"""
import argparse
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import time

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
LVP = '/usr/share/vulkan/icd.d/lvp_icd.json'

# A two-pass preset: the driver runs a custom chain, which emits SDR,
# so HDR10 output goes through the offscreen buffer and the composite.
PASS = r'''#version 450
layout(std140, set = 0, binding = 0) uniform UBO
{
   mat4 MVP;
} global;

#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;

void main()
{
   gl_Position = global.MVP * Position;
   vTexCoord   = TexCoord;
}

#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D Source;

void main()
{
   FragColor = vec4(texture(Source, vTexCoord).rgb, 1.0);
}
'''

PRESET = '''shaders = 2
shader0 = pass.slang
filter_linear0 = false
scale_type0 = source
scale0 = 1.0
shader1 = pass.slang
filter_linear1 = true
'''

# name, preset, settings, open the menu, UI drawn. OSD text is off
# except in the message case, so the menu is the only UI the menu
# cases draw and the plain case draws none.
CASES = [
    ('shader-menu', True, {}, True, True),
    ('shader-statistics', True, {'statistics_show': 'true'}, False, True),
    ('shader-message', True,
     {'fps_show': 'true', 'video_font_enable': 'true'}, False, True),
    ('shader-plain', True, {}, False, False),
    ('shader-menu-threaded', True, {'video_threaded': 'true'}, True, True),
    ('stock-menu', False, {}, True, True),
]

# What the driver logs, once per change, as it takes each HDR pass.
GAME_PASS = '[Vulkan] HDR game pass:'
COMPOSITE_PASS = '[Vulkan] HDR composite pass:'

FRAMES = 90


def build(d):
    cc = shlex.split(os.environ.get('CC', 'cc'))
    layer = d / 'libfake_hdr_layer.so'
    subprocess.run(cc + ['-std=gnu89', '-O2', '-shared', '-fPIC',
                         '-Wall', '-Wextra',
                         '-Werror=declaration-after-statement',
                         str(HERE / 'fake_hdr_layer.c'), '-o', str(layer)],
                   check=True)
    (d / 'fake_hdr_layer.json').write_text('''{
 "file_format_version": "1.1.2",
 "layer": {
  "name": "VK_LAYER_RA_fake_hdr",
  "type": "GLOBAL",
  "library_path": "./libfake_hdr_layer.so",
  "api_version": "1.3.0",
  "implementation_version": "1",
  "description": "Pairs 10-bit surface formats with HDR10"
 }
}
''')
    core = d / 'hdr_ui_core.so'
    subprocess.run(cc + ['-std=gnu89', '-O1', '-shared', '-fPIC',
                         '-Wall', '-Wextra',
                         '-Werror=declaration-after-statement',
                         '-I' + str(ROOT / 'libretro-common/include'),
                         str(HERE / 'hdr_ui_core.c'), '-o', str(core)],
                   check=True)
    (d / 'pass.slang').write_text(PASS)
    (d / 'hdr_ui.slangp').write_text(PRESET)
    return core


def xvfb_start():
    """An Xvfb of depth 30 on a free display: its process and DISPLAY."""
    for n in range(140, 200):
        if os.path.exists('/tmp/.X%d-lock' % n):
            continue
        p = subprocess.Popen(['Xvfb', ':%d' % n, '-screen', '0',
                              '640x480x30', '-nolisten', 'tcp'],
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


def write_cfg(path, d, settings, preset):
    cfg = {
        'video_driver': 'vulkan',
        'video_context_driver': 'vk_x',
        'video_hdr_enable': 'true',
        'video_hdr_mode': '1',
        'video_hdr_paper_white_nits': '203',
        'video_hdr_max_nits': '1000',
        'video_hdr_menu_nits': '203',
        'video_hdr_use_display_peak': 'false',
        'video_threaded': 'false',
        'video_fullscreen': 'false',
        'video_scale': '2',
        'video_vsync': 'false',
        'video_shader_enable': 'true' if preset else 'false',
        'video_shader_dir': str(d),
        'video_font_enable': 'false',
        'menu_enable_widgets': 'false',
        'menu_driver': 'rgui',
        'menu_show_start_screen': 'false',
        'menu_pause_libretro': 'false',
        'fps_show': 'false',
        'video_statistics_show': 'false',
        'input_driver': 'x',
        'input_joypad_driver': 'null',
        'input_autodetect_enable': 'false',
        'input_overlay_enable': 'false',
        'cheevos_enable': 'false',
        'audio_enable': 'false',
        'audio_driver': 'null',
        'microphone_enable': 'false',
        'pause_nonactive': 'false',
        'confirm_quit': 'false',
        'quit_press_twice': 'false',
        'config_save_on_exit': 'false',
        'stdin_cmd_enable': 'true',
        'network_cmd_enable': 'false',
        'history_list_enable': 'false',
        'auto_shaders_enable': 'false',
        'savefile_directory': str(d),
        'savestate_directory': str(d),
        'system_directory': str(d),
        'cache_directory': str(d),
        'screenshot_directory': str(d),
        'playlist_directory': str(d),
        'content_favorites_path': str(d / 'favorites.lpl'),
        'content_history_path': str(d / 'history.lpl'),
        'content_image_history_path': str(d / 'image_history.lpl'),
        'content_music_history_path': str(d / 'music_history.lpl'),
        'content_video_history_path': str(d / 'video_history.lpl'),
        'input_remapping_directory': str(d),
        'rgui_config_directory': str(d),
        'runtime_log_directory': str(d),
        'thumbnails_directory': str(d),
        'core_assets_directory': str(d),
        'core_options_path': str(d / 'opts.cfg'),
    }
    cfg.update(settings)
    with open(path, 'w') as f:
        for k in sorted(cfg):
            f.write('%s = "%s"\n' % (k, cfg[k]))


def run_case(exe, d, core, display, name, preset, settings, menu, ui,
             logs):
    cfg = d / ('%s.cfg' % name)
    write_cfg(cfg, d, settings, preset)
    env = dict(os.environ, DISPLAY=display,
               VK_ADD_LAYER_PATH=str(d),
               VK_INSTANCE_LAYERS='VK_LAYER_RA_fake_hdr',
               XDG_CONFIG_HOME=str(d), HOME=str(d))
    if not env.get('VK_DRIVER_FILES') and os.path.exists(LVP):
        env['VK_DRIVER_FILES'] = LVP
    cmd = [exe, '-v', '--config', str(cfg), '-L', str(core)]
    if preset:
        cmd += ['--set-shader', str(d / 'hdr_ui.slangp')]
    if not menu:
        cmd += ['--max-frames', str(FRAMES)]
    p = subprocess.Popen(cmd, cwd=d, env=env, stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        if menu:
            # stdin commands are polled once a frame: the menu opens
            # with the first frames and is drawn until the quit.
            time.sleep(2)
            p.stdin.write(b'MENU_TOGGLE\n')
            p.stdin.flush()
            time.sleep(6)
            p.stdin.write(b'QUIT\n')
            p.stdin.flush()
        out, _ = p.communicate(timeout=120)
    except (subprocess.TimeoutExpired, BrokenPipeError):
        p.kill()
        out, _ = p.communicate()
    log = logs / ('%s.log' % name)
    log.write_bytes(out)
    text = out.decode(errors='replace')

    errors = []
    if p.returncode != 0:
        errors.append('exit status %d' % p.returncode)
    if 'fake-hdr: HDR10 swapchain' not in text:
        errors.append('no HDR10 swapchain was created (fell back to SDR?)')
    if 'Validation Error' in text:
        errors.append('%d validation error(s)' % text.count('Validation Error'))
        vuids = {}
        for line in text.splitlines():
            if 'Validation Error' in line:
                m = re.search(r'\[ (\S+) \]', line)
                vuid = m.group(1) if m else line.strip()[:200]
                vuids[vuid] = vuids.get(vuid, 0) + 1
        for vuid in sorted(vuids):
            errors.append('  %s x%d' % (vuid, vuids[vuid]))
    # The preset is what puts the game through the offscreen buffer
    # and the game pass; any UI is what adds the UI layer and its
    # composite. A case that did not take the path it names proves
    # nothing, so it fails.
    if preset and GAME_PASS not in text:
        errors.append('no HDR game pass: the preset was not loaded')
    if not preset and GAME_PASS in text:
        errors.append('an HDR game pass with no preset')
    if ui and COMPOSITE_PASS not in text:
        errors.append('no HDR composite pass: no UI was drawn'
                      + (' (the menu did not open)' if menu else ''))
    if not ui and COMPOSITE_PASS in text:
        errors.append('an HDR composite pass with no UI')
    return errors


def main():
    a = argparse.ArgumentParser()
    a.add_argument('--retroarch', required=True)
    a.add_argument('--log-dir', required=True)
    a.add_argument('--only', default='')
    a = a.parse_args()
    if not shutil.which('Xvfb'):
        print('vulkan_hdr_ui_pass: no Xvfb, skipping')
        return 77
    exe = str(Path(a.retroarch).resolve())
    logs = Path(a.log_dir).resolve()
    logs.mkdir(parents=True, exist_ok=True)
    failed = 0
    xvfb, display = xvfb_start()
    try:
        with tempfile.TemporaryDirectory(prefix='ra-hdr-ui-') as d:
            d = Path(d)
            core = build(d)
            for name, preset, settings, menu, ui in CASES:
                if a.only and not re.search(a.only, name):
                    continue
                errors = run_case(exe, d, core, display, name, preset,
                                  settings, menu, ui, logs)
                if errors:
                    failed += 1
                    print('FAIL: %s (%s)' % (name, logs / (name + '.log')))
                    for e in errors:
                        print('  ' + e)
                else:
                    print('PASS: %s' % name)
    finally:
        xvfb.terminate()
        xvfb.wait()
    if failed:
        print('vulkan_hdr_ui_pass: %d case(s) failed' % failed)
        return 1
    print('vulkan_hdr_ui_pass: all cases passed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
