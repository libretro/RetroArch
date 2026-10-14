#!/usr/bin/env python3
"""A core writing into the framebuffer a driver lends it, under Wine.

Runs samples/gfx/threaded_video's harness core in retroarch.exe with
threaded video off, so GET_CURRENT_SOFTWARE_FRAMEBUFFER reaches the
video driver itself, and each case again with it on, where the wrapper
lends its own frame slots - in the driver's frame memory where it has
some, read by the GPU where they lie. The core pushes NULL every third frame, so a loan
is left open across a dupe.

  lent    eight still bars written into the loan, compared with the
          reference driver, which copies the core's frame
  cropped the same, pushed as a window inside the loan, which the
          driver moves to the corner of its texture
  regrow  the core running on, with a frame taller than its geometry
          every 61st run: the driver grows its textures, one of them
          holding the last frame lent, and has to keep that alive while
          the cached frame names it

Each case must have had framebuffers lent (the core's report at
deinit), except on the reference driver, and RetroArch must quit when
asked. A screen the driver drew wrong moves whole blocks of the
comparison, as in run.py.

With SWFB_D3D9EX_PRESENT set to a Max Frame Latency (1 to 4), every
run has Direct3D 9Ex Presentation, V-Sync and Waitable Swapchains on:
a D3D9 driver on a Direct3D9Ex device must then present its window
through the flip model and take that latency, and one without Ex must
present as before.

Usage: swfb.py <retroarch.exe> <harness_core.dll> <output dir> <driver>
       [reference driver, d3d11 by default]
(Windows-wine-menu.yml)
"""

import os
import re
import shutil
import socket
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import run  # noqa: E402

FREEZE = '181'
CASES = (
    ('lent',    {'HARNESS_CORE_SW_FB': '1', 'HARNESS_CORE_FREEZE': FREEZE,
                 'HARNESS_CORE_STAMP': '1'}, True, False),
    ('cropped', {'HARNESS_CORE_SW_FB': '2', 'HARNESS_CORE_FREEZE': FREEZE,
                 'HARNESS_CORE_STAMP': '1'}, True, False),
    ('regrow',  {'HARNESS_CORE_SW_FB': '1'}, False, False),
    # The threaded wrapper lending its own slots, which a driver with
    # frame memory has the GPU read where they lie
    ('lent-threaded',    {'HARNESS_CORE_SW_FB': '1',
                          'HARNESS_CORE_FREEZE': FREEZE,
                          'HARNESS_CORE_STAMP': '1'}, True, True),
    ('cropped-threaded', {'HARNESS_CORE_SW_FB': '2',
                          'HARNESS_CORE_FREEZE': FREEZE,
                          'HARNESS_CORE_STAMP': '1'}, True, True),
    ('regrow-threaded',  {'HARNESS_CORE_SW_FB': '1'}, False, True),
)
REGROW_SECONDS = 20
# What each run logged, by (case, driver)
LOGS = {}
# A D3D9 driver on a Direct3D9Ex device offers the wrapper frame
# memory, and says so once
FRAME_MEM_LINE = 'the GPU reads the frames where they lie'
# Direct3D 9Ex Presentation, and the latency it is to take
PRESENT_LATENCY = os.environ.get('SWFB_D3D9EX_PRESENT', '')
FLIP_LINE = 'Presenting through the flip model'



def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(('127.0.0.1', 0))
    port = s.getsockname()[1]
    s.close()
    return port


def launch(retroarch, core, root, driver, name, core_env, still,
           threaded):
    """(screen or None, lent framebuffers or None, quit cleanly)."""
    d = os.path.join(root, name, driver)
    os.makedirs(d, exist_ok=True)
    cfg = os.path.join(d, 'retroarch.cfg')
    port = free_port()
    run.write_cfg(cfg, d, driver, 'rgui')
    with open(cfg, 'a') as f:
        f.write('network_cmd_enable = "true"\n')
        f.write('network_cmd_port = "%d"\n' % port)
        f.write('video_smooth = "true"\n')
        f.write('aspect_ratio_index = "22"\n')
        # The log says which device was made
        f.write('log_verbosity = "true"\n')
        if PRESENT_LATENCY:
            f.write('video_d3d9ex_present = "true"\n')
            f.write('video_vsync = "true"\n')
            f.write('video_waitable_swapchains = "true"\n')
            f.write('video_max_frame_latency = "%s"\n' % PRESENT_LATENCY)
    with open(cfg) as f:
        text = f.read()
    with open(cfg, 'w') as f:
        f.write(text.replace('video_threaded = "false"',
                             'video_threaded = "%s"'
                             % ('true' if threaded else 'false')))
    shot = os.path.join(d, 'screen.png')
    log_path = os.path.join(d, 'stdout.log')
    env = dict(os.environ)
    env.update(core_env)
    env['HARNESS_CORE_REPORT'] = '1'
    env['WINEDEBUG'] = '-all'
    n = run.free_display()
    xvfb = subprocess.Popen(['Xvfb', ':%d' % n, '-screen', '0',
                             '%dx%dx24' % (run.W, run.H), '-nolisten',
                             'tcp'], stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL,
                            start_new_session=True)
    ra, got, clean = None, None, False
    try:
        deadline = time.time() + 10
        while (time.time() < deadline
               and not os.path.exists('/tmp/.X11-unix/X%d' % n)):
            time.sleep(0.1)
        env['DISPLAY'] = ':%d' % n
        with open(log_path, 'w') as log:
            ra = subprocess.Popen(['wine', retroarch, '--config',
                                   run.winpath(cfg), '-L',
                                   run.winpath(core)],
                                  stdout=log, stderr=subprocess.STDOUT,
                                  env=env, start_new_session=True)
            time.sleep(run.SETTLE)
            if still:
                last, deadline = None, time.time() + run.STEADY
                while time.time() < deadline and ra.poll() is None:
                    if os.path.exists(shot):
                        os.remove(shot)
                    subprocess.run(['import', '-display', env['DISPLAY'],
                                    '-window', 'root', 'PNG24:' + shot],
                                   stdout=subprocess.DEVNULL,
                                   stderr=subprocess.DEVNULL, timeout=30)
                    if os.path.exists(shot):
                        b = run.blocks(shot)
                        if not run.drawn(b):
                            last = None
                        elif last is not None and \
                                run.compare(last, b)[0] <= 0.5:
                            break
                        else:
                            last = b
                    time.sleep(1.0)
            else:
                time.sleep(REGROW_SECONDS)
                subprocess.run(['import', '-display', env['DISPLAY'],
                                '-window', 'root', 'PNG24:' + shot],
                               stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL, timeout=30)
            if ra.poll() is None:
                run_send('QUIT', port)
                try:
                    ra.wait(timeout=30)
                    clean = True
                except subprocess.TimeoutExpired:
                    pass
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
    # The core reports on stdout, RetroArch logs to a file beside it
    text = ''
    for fn in sorted(os.listdir(d)):
        if fn.endswith('.log'):
            with open(os.path.join(d, fn), errors='replace') as f:
                text += f.read()
    m = re.search(r'harness: (\d+) runs, (\d+) framebuffers lent', text)
    if m:
        got = int(m.group(2))
    LOGS[(name, driver)] = text
    return (shot if os.path.exists(shot) else None), got, clean


def launch_once_more(retroarch, core, root, driver, name, core_env,
                     still, threaded):
    """launch(), again once when RetroArch went before logging a line:
    Wine now and then loses a process at its start. A real failure to
    start fails twice."""
    r = launch(retroarch, core, root, driver, name, core_env, still,
               threaded)
    if r[0] is None and not LOGS.get((name, driver)):
        print('note %s/%s: gone before it logged, run again'
              % (name, driver))
        r = launch(retroarch, core, root, driver, name, core_env, still,
                   threaded)
    return r


def run_send(cmd, port):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.sendto(cmd.encode(), ('127.0.0.1', port))
    s.close()


def main():
    if len(sys.argv) < 5:
        sys.stderr.write(__doc__)
        return 2
    retroarch = os.path.abspath(sys.argv[1])
    core = os.path.abspath(sys.argv[2])
    root = os.path.abspath(sys.argv[3])
    driver = sys.argv[4]
    ref = sys.argv[5] if len(sys.argv) > 5 else 'd3d11'
    failed = 0
    if os.path.isdir(root):
        shutil.rmtree(root)
    os.makedirs(root)
    for name, core_env, still, threaded in CASES:
        shot, got, clean = launch_once_more(retroarch, core, root, driver,
                                            name, core_env, still,
                                            threaded)
        why = []
        if not shot:
            why.append('no screen')
        if not got:
            why.append('nothing lent (%s)' % got)
        if not clean:
            why.append('did not quit when asked')
        if shot and not run.drawn(run.blocks(shot)):
            why.append('nothing drawn')
        if driver.startswith('d3d9'):
            log = LOGS.get((name, driver), '')
            legacy = os.environ.get('RETROARCH_D3D9EX') == '0'
            ex = 'Direct3D9Ex device' in log
            if not ex and 'Direct3D 9 device' not in log:
                why.append('no word of the device made')
            elif legacy and ex:
                why.append('Direct3D9Ex made with RETROARCH_D3D9EX=0')
            elif threaded and ex and FRAME_MEM_LINE not in log:
                why.append('frame memory not taken on a Direct3D9Ex device')
            if PRESENT_LATENCY and ex:
                want = ('Requesting %s maximum frame latency, using %s.'
                        % (PRESENT_LATENCY, PRESENT_LATENCY))
                if FLIP_LINE not in log:
                    why.append('the window not presented in the flip model')
                if want not in log:
                    why.append('frame latency %s not taken'
                               % PRESENT_LATENCY)
            elif PRESENT_LATENCY and 'Presenting ' in log:
                why.append('Direct3D 9Ex Presentation without Ex')
        if still and shot:
            rshot, _, _ = launch_once_more(retroarch, core, root, ref,
                                           name, core_env, still, threaded)
            if not rshot:
                why.append('no screen from %s' % ref)
            else:
                worst, at = run.compare(run.blocks(rshot), run.blocks(shot))
                if worst > run.LIMIT:
                    why.append('block %s differs from %s by %.1f'
                               % (at, ref, worst))
        if why:
            print('fail %s/%s: %s' % (name, driver, ', '.join(why)))
            failed += 1
        else:
            print('pass %s/%s (%d lent)' % (name, driver, got))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
