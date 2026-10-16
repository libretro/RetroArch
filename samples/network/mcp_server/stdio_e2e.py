#!/usr/bin/env python3
"""retroarch --mcp-stdio, end to end, against the video_views test core:
the handshake, the tool list, RUN_FRAMES stepping a running core,
SET_INPUT reaching it (it logs the pad it reads), GET_LOG, log lines as
notifications/message, and quitting when stdin closes.

Usage: stdio_e2e.py <retroarch> <video_views_libretro.so>
"""
import json, os, subprocess, sys, tempfile, time, select

ra, core = sys.argv[1], sys.argv[2]
cfgdir = tempfile.mkdtemp()
cfg = os.path.join(cfgdir, 'retroarch.cfg')
with open(cfg, 'w') as f:
    f.write('video_driver = "null"\naudio_driver = "null"\ninput_driver = "null"\n'
            'menu_driver = "rgui"\nlog_verbosity = "true"\nfrontend_log_level = "1"\n'
            'pause_nonactive = "false"\nconfig_save_on_exit = "false"\n')
p = subprocess.Popen([ra, '--mcp-stdio', '-v', '-c', cfg, '-L', core],
                     stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                     stderr=open(os.path.join(cfgdir, 'stderr.log'), 'wb'))
buf = b''
fails = 0


def check(what, ok):
    global fails
    print('%-60s %s' % (what, 'ok' if ok else 'FAIL'))
    if not ok:
        fails += 1


def send(obj):
    p.stdin.write((json.dumps(obj) + '\n').encode())
    p.stdin.flush()


def recv(want_id, timeout=20):
    """Messages until the one answering want_id; returns (answer, notes)."""
    global buf
    notes = []
    end = time.time() + timeout
    while time.time() < end:
        while b'\n' in buf:
            line, buf = buf.split(b'\n', 1)
            msg = json.loads(line)   # every stdout line is JSON
            if msg.get('id') == want_id:
                return msg, notes
            notes.append(msg)
        r, _, _ = select.select([p.stdout], [], [], 0.2)
        if r:
            chunk = os.read(p.stdout.fileno(), 65536)
            if not chunk:
                break
            buf += chunk
    return None, notes


def call(i, name, args=None):
    send({'jsonrpc': '2.0', 'id': i, 'method': 'tools/call',
          'params': {'name': name, 'arguments': args or {}}})
    msg, notes = recv(i)
    text = msg and msg['result']['content'][-1]['text']
    return msg, text, notes


send({'jsonrpc': '2.0', 'id': 1, 'method': 'initialize',
      'params': {'protocolVersion': '2025-06-18', 'capabilities': {},
                 'clientInfo': {'name': 'e2e', 'version': '1'}}})
msg, _ = recv(1, 60)
check('initialize over stdio', msg and msg['result']['serverInfo']['name'] == 'RetroArch')
check('logging is offered', msg and 'logging' in msg['result']['capabilities'])
send({'jsonrpc': '2.0', 'method': 'notifications/initialized'})
send({'jsonrpc': '2.0', 'id': 2, 'method': 'tools/list', 'params': {}})
msg, _ = recv(2)
names = [t['name'] for t in msg['result']['tools']] if msg else []
for n in ('RUN_FRAMES', 'SET_INPUT', 'RELEASE_INPUT', 'GET_LOG',
          'GET_CORE_OPTION', 'GET_MEMORY_MAP', 'PRESS', 'RUN_UNTIL',
          'GET_INPUT', 'WATCH', 'MENU', 'DISK', 'REINIT', 'STATE',
          'REPLAY', 'STEPS'):
    check('tool %s listed' % n, n in names)
# A family of hotkeys is one tool with a choice, not a tool apiece.
for n in ('MENU_UP', 'DISK_NEXT', 'VIDEO_REINIT', 'SAVE_STATE_SLOT',
          'SEEK_REPLAY', 'HALT_REPLAY', 'SAVE_FILES'):
    check('%s is not a tool of its own' % n, n not in names)
# A hotkey that steps a selection nothing can read back is a command on
# every line interface and not a tool.
for n in ('SAVE_STATE', 'STATE_SLOT_PLUS', 'SHADER_NEXT', 'READ_CORE_RAM',
          'NETPLAY_HOST_TOGGLE'):
    check('%s is not offered as a tool' % n, n not in names)
send({'jsonrpc': '2.0', 'id': 3, 'method': 'tools/call',
      'params': {'name': 'SAVE_STATE', 'arguments': {}}})
msg2, _ = recv(3)
check('and calling one is an unknown tool',
      msg2 and 'error' in msg2 and msg2['error']['code'] == -32602)

# The revision agreed at initialize is the one answered in: over stdio
# there is no header to carry it on every request.
schemas = [t['name'] for t in msg['result']['tools'] if t.get('outputSchema')]
check('the rows commands declare an outputSchema: %d' % len(schemas),
      'GET_LOG' in schemas and 'SET_INPUT' in schemas
      and 'GET_PLAYLIST' in schemas)
# Every tool's description and schema is context a client pays for
# before it does anything. The total, 25000 bytes, catches a tool added
# by accident; the ceiling of 1800 on each one catches prose, which an
# average would not, as folding small tools into one raises it. 24730
# at 35 tools.
size = len(json.dumps(msg, separators=(',', ':')))
check('the tool list stays within budget: %d bytes, %d tools'
      % (size, len(names)), size < 25000)
biggest = max(msg['result']['tools'],
              key=lambda t: len(json.dumps(t, separators=(',', ':'))))
check('and no one tool is over 1800 bytes: %s is %d' % (biggest['name'],
      len(json.dumps(biggest, separators=(',', ':')))),
      len(json.dumps(biggest, separators=(',', ':'))) <= 1800)

# wait for the core to be running
st = ''
for i in range(50):
    _, st, _ = call(100 + i, 'GET_STATUS')
    if st and ('PLAYING' in st or 'PAUSED' in st):
        break
    time.sleep(0.2)
check('content running: ' + (st or '').strip(), st and 'PLAYING' in st)


def frame_count(i):
    _, t, _ = call(i, 'GET_OPTION', {'option': 'frame_count'})
    try:
        return int(t.split('\t')[1])
    except Exception:
        return None

msg, t, _ = call(200, 'RUN_FRAMES', {'count': 30})
check('RUN_FRAMES answers after the frames: ' + (t or '').strip(),
      t and t.startswith('RUN_FRAMES 30') and not msg['result']['isError'])
check('and its structured result says how many',
      msg and msg['result'].get('structuredContent') == {'rows': [], 'frames': 30})
_, t, _ = call(240, 'REINIT', {'driver': 'audio'})
check('REINIT answers under its own name: ' + (t or '').strip(),
      t and t.strip() == 'REINIT audio')
msg, t, _ = call(241, 'DISK', {'action': 'next'})
check('DISK is refused where the core has no disks: ' + (t or '').strip(),
      t and t.startswith('DISK ERROR') and msg['result']['isError'])
_, st, _ = call(201, 'GET_STATUS')
check('and leaves it paused', st and 'PAUSED' in st)
a = frame_count(202)
call(203, 'RUN_FRAMES', {'count': 10})
b = frame_count(204)
check('10 frames run: %s -> %s' % (a, b), a is not None and b is not None and b - a == 10)
time.sleep(0.5)
c = frame_count(205)
check('and none while paused after', c == b)

msg, t, _ = call(210, 'SET_INPUT', {'player': 1, 'control': 'a', 'value': 1})
check('SET_INPUT: ' + (t or '').strip(), t and t.startswith('1\ta\t'))
sc = msg and msg['result'].get('structuredContent')
check('and the same reply in fields',
      sc and sc['rows'] == [{'player': '1', 'buttons': 'a', 'axes': 'none'}])
msg, t, _ = call(211, 'RELEASE_INPUT', {})
check('RELEASE_INPUT', t and 'none\tnone' in t)

# PRESS is SET_INPUT, RUN_FRAMES, SET_INPUT and RUN_FRAMES in one. The
# content is paused by here, so the frames it runs are its own.
a = frame_count(212)
msg, t, notes = call(213, 'PRESS', {'player': 1, 'control': 'right+a',
                                    'frames': 4})
b = frame_count(214)
check('PRESS answers after its frames: ' + (t or '').strip(),
      t and t.strip() == 'PRESS 1 right+a 4' and not msg['result']['isError'])
check('and ran exactly those: %s -> %s' % (a, b), b - a == 4)
_, t, _ = call(215, 'RELEASE_INPUT', {})
check('and let the buttons go', t and 'none\tnone' in t)
# More frames than the reply is waited for is refused outright, rather
# than failing ten seconds later with the frames already running.
_, t, _ = call(216, 'RUN_FRAMES', {'count': 36000})
check('RUN_FRAMES past what a reply waits for is refused now',
      t and t.startswith('RUN_FRAMES ERROR') and 'frames in one request' in t)
_, t, _ = call(217, 'PRESS', {'player': 1, 'control': 'a', 'frames': 601})
check('and a press is bounded the same way',
      t and t.startswith('PRESS ERROR') and 'SET_INPUT' in t)
# A stretch of play as one request: this core maps no memory to wait
# on, so the steps that need none. The frame after the press lets the
# core log A let go, which the log checks below read changes of.
msg, t, _ = call(219, 'STEPS', {'steps': 'run 3; press 1 a 2; run 1'})
check('STEPS runs its steps in one request: ' + (t or '').split('\n')[0],
      t and t.startswith('DONE 6') and not msg['result']['isError']
      and msg['result'].get('structuredContent') == {'rows': [], 'done': 6, 'step': 3})
_, t, _ = call(218, 'PRESS', {'player': 1, 'control': 'left_x', 'frames': 2})
check('an axis is SET_INPUT\'s, not PRESS\'s',
      t and t.startswith('PRESS ERROR') and 'axis' in t)

# This core maps no memory, so RUN_UNTIL cannot wait on a byte here -
# and says so before running a frame, rather than after a wait that
# could not have ended. command_frames covers the waiting itself.
_, t, _ = call(221, 'RUN_UNTIL', {'address': '1000', 'condition': '==',
                                  'value': 0})
check('RUN_UNTIL refuses an address nothing maps: ' + (t or '').strip(),
      t and t.startswith('RUN_UNTIL ERROR'))
_, t, _ = call(222, 'RUN_UNTIL', {'address': '1000', 'condition': '~=',
                                  'value': 0})
check('and a condition it does not take',
      t and t.startswith('RUN_UNTIL ERROR') and '==' in t)

# GET_INPUT looks without touching: SET_INPUT must set to answer and
# RELEASE_INPUT lets go before it does.
_, t, _ = call(223, 'SET_INPUT', {'player': 1, 'control': 'start', 'value': 1})
_, t2, _ = call(224, 'GET_INPUT', {})
_, t3, _ = call(225, 'GET_INPUT', {'player': 1})
check('GET_INPUT reads what is held, twice the same: ' + (t2 or '').strip(),
      t2 and t2 == t and t3 == t2)
call(226, 'RELEASE_INPUT', {})
# A path that is not a core is refused before the running core goes.
_, t, _ = call(227, 'LOAD_CORE', {'core': '/no/such/core.so'})
check('LOAD_CORE says why: ' + (t or '').strip(),
      t and 'no core at' in t)
_, st, _ = call(229, 'GET_STATUS')
check('and the content it was running is still running',
      st and ('PLAYING' in st or 'PAUSED' in st))

# A watch rides along with every step, so a step needs no reads after
# it. This core maps no memory, so the refusal is what is checkable
# here; command_frames covers the carrying.
_, t, _ = call(232, 'MENU', {'button': 'toggle'})
check('MENU names the buttons it takes: ' + (t or '').strip(),
      t and t.startswith('MENU ERROR') and 'up|down' in t)

_, t, _ = call(230, 'WATCH', {})
check('WATCH starts empty: ' + (t or '').strip(), t and 'none' in t)
_, t, _ = call(231, 'WATCH', {'watches': 'x 1000'})
check('and refuses an address nothing maps',
      t and t.startswith('WATCH ERROR'))

msg, t, _ = call(220, 'GET_LOG', {})
check('GET_LOG has lines and NEXT', t and '\nNEXT ' in '\n' + t and '[INFO]' in t)
sc = msg and msg['result'].get('structuredContent')
check('GET_LOG answers where it got to, and not the page over again',
      sc == {'next': int(t.rsplit('NEXT ', 1)[1])} and len(t) > 100)

send({'jsonrpc': '2.0', 'id': 230, 'method': 'logging/setLevel',
      'params': {'level': 'info'}})
_, notes = recv(230)
for k, (name, args) in enumerate([('PRESS', {'player': 1, 'control': 'a',
                                              'frames': 3}),
                                  ('RUN_FRAMES', {'count': 3}),
                                  ('GET_STATUS', {})]):
    _, _, n2 = call(240 + k, name, args)
    notes += n2
logs = [n['params']['data'] for n in notes if n.get('method') == 'notifications/message']
print('   notifications:', [l[:72] for l in logs[:4]])
check('the core reads PRESS\'s A (bit 8) on the frames it runs',
      any('buttons=0x0100' in l for l in logs))
check('and reads it let go of afterwards',
      any('buttons=0x0000' in l for l in logs))
check('log lines arrive as notifications after setLevel',
      any(n.get('method') == 'notifications/message' for n in notes))

p.stdin.close()
try:
    rc = p.wait(30)
    check('closing stdin quits (exit %d)' % rc, True)
except subprocess.TimeoutExpired:
    p.kill()
    check('closing stdin quits', False)
with open(os.path.join(cfgdir, 'stderr.log'), 'rb') as f:
    err = f.read()
check('the log went to stderr', b'[INFO]' in err)
print('FAIL' if fails else 'PASS')
sys.exit(1 if fails else 0)
