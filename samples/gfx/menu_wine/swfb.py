#!/usr/bin/env python3
"""A core writing into the framebuffer a driver lends it, under Wine.

Runs samples/gfx/threaded_video's harness core in retroarch.exe with
threaded video off, so GET_CURRENT_SOFTWARE_FRAMEBUFFER reaches the
video driver itself. The core pushes NULL every third frame, so a loan
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
                 'HARNESS_CORE_STAMP': '1'}, True),
    ('cropped', {'HARNESS_CORE_SW_FB': '2', 'HARNESS_CORE_FREEZE': FREEZE,
                 'HARNESS_CORE_STAMP': '1'}, True),
    ('regrow',  {'HARNESS_CORE_SW_FB': '1'}, False),
)
REGROW_SECONDS = 20


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(('127.0.0.1', 0))
    port = s.getsockname()[1]
    s.close()
    return port


def launch(retroarch, core, root, driver, name, core_env, still):
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
                        if last is not None and \
                                run.compare(last, b)[0] <= 0.5:
                            break
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
    with open(log_path, errors='replace') as f:
        m = re.search(r'harness: (\d+) runs, (\d+) framebuffers lent',
                      f.read())
    if m:
        got = int(m.group(2))
    return (shot if os.path.exists(shot) else None), got, clean


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
    for name, core_env, still in CASES:
        shot, got, clean = launch(retroarch, core, root, driver, name,
                                  core_env, still)
        why = []
        if not shot:
            why.append('no screen')
        if not got:
            why.append('nothing lent (%s)' % got)
        if not clean:
            why.append('did not quit when asked')
        if shot and max(max(c) for c in run.blocks(shot)) < 8:
            why.append('nothing drawn')
        if still and shot:
            rshot, _, _ = launch(retroarch, core, root, ref, name,
                                 core_env, still)
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
