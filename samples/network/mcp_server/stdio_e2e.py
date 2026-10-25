#!/usr/bin/env python3
"""retroarch --mcp-stdio, end to end, against the video_views test core:
the handshake, the tool list, RUN_FRAMES stepping a running core,
INPUT reaching it (it logs the pad it reads), GET_LOG, log lines as
notifications/message, and quitting when stdin closes.

The harness core, from samples/network/command_frames, maps memory, which
the tools that read it need.

The same core built with HARNESS_NO_MAP publishes no memory map and hands
its RAM over through retro_get_memory_data alone, as snes9x does.

Usage: stdio_e2e.py <retroarch> <video_views_libretro.so> <harness_core.so>
                    <harness_core_nomap.so>
"""
import base64, json, os, shutil, subprocess, sys, tempfile, time, select

ra, core, mem_core, nomap_core = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
cfgdir = tempfile.mkdtemp()
cfg = os.path.join(cfgdir, 'retroarch.cfg')
# a cores directory for LIST_CORES, and a playlist for the playlist tools
coredir = os.path.join(cfgdir, 'cores')
listdir = os.path.join(cfgdir, 'playlists')
os.mkdir(coredir)
# GET_FRAME writes nothing: this stays empty
shotdir = os.path.join(cfgdir, 'screenshots')
os.mkdir(shotdir)
os.mkdir(listdir)
shutil.copy(core, coredir)
rom = os.path.join(cfgdir, 'blank.bin')
with open(rom, 'wb') as f:
    f.write(b'\0' * 16)
with open(os.path.join(listdir, 'e2e.lpl'), 'w') as f:
    json.dump({'version': '1.5', 'items': [
        {'path': rom, 'label': 'Blank', 'core_path': core,
         'core_name': 'video_views', 'crc32': 'DETECT', 'db_name': 'e2e.lpl'}]},
        f)
with open(cfg, 'w') as f:
    f.write('video_driver = "null"\naudio_driver = "null"\ninput_driver = "null"\n'
            'menu_driver = "rgui"\nlog_verbosity = "true"\nfrontend_log_level = "1"\n'
            'pause_nonactive = "false"\nconfig_save_on_exit = "false"\n'
            'video_shader_enable = "false"\n'
            'libretro_directory = "%s"\nplaylist_directory = "%s"\n'
            'screenshot_directory = "%s"\n'
            % (coredir, listdir, shotdir))
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


# Every structuredContent a call gets is held to the tool's outputSchema:
# the required fields, the declared types, and no field it leaves out.
out_schemas = {}
schema_errors = []
schema_checked = set()


def conforms(schema, v, path):
    t = schema.get('type')
    if t == 'integer':
        ok = isinstance(v, int) and not isinstance(v, bool)
    else:
        ok = t is None or isinstance(v, {'object': dict, 'array': list,
                                         'string': str, 'boolean': bool}[t])
    if not ok:
        return ['%s is not %s' % (path, t)]
    errs = []
    if t == 'object':
        props = schema.get('properties')
        errs += ['%s.%s missing' % (path, k)
                 for k in schema.get('required', []) if k not in v]
        for k, x in v.items():
            if props is None:
                continue
            if k not in props:
                errs.append('%s.%s undeclared' % (path, k))
            else:
                errs += conforms(props[k], x, path + '.' + k)
    elif t == 'array':
        for n, x in enumerate(v):
            errs += conforms(schema.get('items', {}), x, '%s[%d]' % (path, n))
    return errs


def call(i, name, args=None):
    send({'jsonrpc': '2.0', 'id': i, 'method': 'tools/call',
          'params': {'name': name, 'arguments': args or {}}})
    msg, notes = recv(i)
    text = msg and msg['result']['content'][-1]['text']
    sc = msg and msg['result'].get('structuredContent')
    if sc is not None and name in out_schemas:
        schema_checked.add(name)
        schema_errors.extend(name + ': ' + e
                             for e in conforms(out_schemas[name], sc, name))
    elif sc is not None and out_schemas:
        schema_errors.append(name + ': structuredContent with no schema')
    if sc is not None and msg['result'].get('isError'):
        schema_errors.append(name + ': structuredContent on an error')
    return msg, text, notes


send({'jsonrpc': '2.0', 'id': 1, 'method': 'initialize',
      'params': {'protocolVersion': '2025-06-18', 'capabilities': {},
                 'clientInfo': {'name': 'e2e', 'version': '1'}}})
msg, _ = recv(1, 60)
check('initialize over stdio', msg and msg['result']['serverInfo']['name'] == 'RetroArch')
check('logging is offered', msg and 'logging' in msg['result']['capabilities'])
check('the handshake says how the tools play a game',
      msg and 'STEPS' in msg['result'].get('instructions', ''))
send({'jsonrpc': '2.0', 'method': 'notifications/initialized'})
send({'jsonrpc': '2.0', 'id': 2, 'method': 'tools/list', 'params': {}})
msg, _ = recv(2)
names = [t['name'] for t in msg['result']['tools']] if msg else []
titles = dict((t['name'], t.get('title')) for t in msg['result']['tools']) if msg else {}
check('a tool has a title for people: %s' % titles.get('RUN_FRAMES'),
      titles.get('RUN_FRAMES') == 'Run frames')
for n in ('RUN_FRAMES', 'INPUT', 'OPTION', 'GET_LOG',
          'CORE_OPTION', 'GET_MEMORY_MAP', 'PRESS', 'RUN_UNTIL',
          'WATCH', 'MENU', 'DISK', 'REINIT', 'STATE', 'GET_FRAME',
          'REPLAY', 'STEPS', 'CORE'):
    check('tool %s listed' % n, n in names)
# A family of hotkeys is one tool with a choice, not a tool apiece.
for n in ('MENU_UP', 'DISK_NEXT', 'VIDEO_REINIT', 'SAVE_STATE_SLOT',
          'SET_INPUT', 'RELEASE_INPUT', 'GET_INPUT', 'GET_OPTION',
          'SET_OPTION', 'GET_CORE_OPTION', 'SET_CORE_OPTION',
          'LOAD_CORE', 'START_CORE', 'UNLOAD_CORE',
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
      'GET_LOG' in schemas and 'INPUT' in schemas
      and 'GET_PLAYLIST' in schemas)
# What a model is handed of each tool is its name, description and
# input schema: that is context a client pays for before it does
# anything, 12055 bytes at 30 tools, held under 15000. Titles,
# annotations and output schemas are for the client and cost the model
# nothing, so the whole list has a looser ceiling, 25000 (21844 now),
# and the 1800 on each tool catches prose an average would not.
seen = sum(len(json.dumps({'name': t['name'],
                           'description': t.get('description', ''),
                           'input_schema': t['inputSchema']},
                          separators=(',', ':')))
           for t in msg['result']['tools'])
check('what a model sees of the tools stays within budget: %d bytes, '
      '%d tools' % (seen, len(names)), seen < 15000)
size = len(json.dumps(msg, separators=(',', ':')))
check('and the whole list: %d bytes' % size, size < 25000)
biggest = max(msg['result']['tools'],
              key=lambda t: len(json.dumps(t, separators=(',', ':'))))
check('and no one tool is over 1800 bytes: %s is %d' % (biggest['name'],
      len(json.dumps(biggest, separators=(',', ':')))),
      len(json.dumps(biggest, separators=(',', ':'))) <= 1800)
tools = dict((t['name'], t) for t in msg['result']['tools'])
check('every tool has a title',
      all(t.get('title') for t in tools.values()))
props = tools['STEPS']['inputSchema']['properties']
check('STEPS declares queue, on or off',
      props.get('queue', {}).get('enum') == ['on', 'off'])
d = props['steps']['description']
check('and its steps name checkpoint and the limits',
      'checkpoint' in d and '32 at most' in d and 'four stop' in d)
out_schemas = dict((n, t['outputSchema']) for n, t in tools.items()
                   if t.get('outputSchema'))

# wait for the core to be running
st = ''
for i in range(50):
    _, st, _ = call(100 + i, 'GET_STATUS')
    if st and ('PLAYING' in st or 'PAUSED' in st):
        break
    time.sleep(0.2)
check('content running: ' + (st or '').strip(), st and 'PLAYING' in st)


def frame_count(i):
    _, t, _ = call(i, 'OPTION', {'action': 'show', 'option': 'frame_count'})
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
check('and a tool failing is a result, not a JSON-RPC error',
      msg and 'error' not in msg and msg['result']['isError'] is True)
msg, st, _ = call(201, 'GET_STATUS')
sc = msg and msg['result'].get('structuredContent')
check('GET_STATUS is a record: %s' % sc,
      sc and sc.get('state') in ('PLAYING', 'PAUSED') and sc.get('system'))
check('and leaves it paused', st and 'PAUSED' in st)
a = frame_count(202)
call(203, 'RUN_FRAMES', {'count': 10})
b = frame_count(204)
check('10 frames run: %s -> %s' % (a, b), a is not None and b is not None and b - a == 10)
time.sleep(0.5)
c = frame_count(205)
check('and none while paused after', c == b)

msg, t, _ = call(210, 'INPUT', {'action': 'set', 'player': 1, 'control': 'a', 'value': 1})
check('INPUT set: ' + (t or '').strip(), t and t.startswith('1\ta\t'))
sc = msg and msg['result'].get('structuredContent')
check('and the same reply in fields',
      sc and sc['rows'] == [{'player': '1', 'buttons': 'a', 'axes': 'none'}])
msg, t, _ = call(211, 'INPUT', {'action': 'release'})
check('INPUT release', t and 'none\tnone' in t)

# PRESS is INPUT set, RUN_FRAMES, INPUT set and RUN_FRAMES in one. The
# content is paused by here, so the frames it runs are its own.
a = frame_count(212)
msg, t, notes = call(213, 'PRESS', {'player': 1, 'control': 'right+a',
                                    'frames': 4})
b = frame_count(214)
check('PRESS answers after its frames: ' + (t or '').strip(),
      t and t.strip() == 'PRESS 1 right+a 4' and not msg['result']['isError'])
check('and ran exactly those: %s -> %s' % (a, b), b - a == 4)
_, t, _ = call(215, 'INPUT', {'action': 'release'})
check('and let the buttons go', t and 'none\tnone' in t)
# More frames than the reply is waited for is refused outright, rather
# than failing ten seconds later with the frames already running.
_, t, _ = call(216, 'RUN_FRAMES', {'count': 36000})
check('RUN_FRAMES past what a reply waits for is refused now',
      t and t.startswith('RUN_FRAMES ERROR') and 'a frame count from 1 to' in t)
_, t, _ = call(217, 'PRESS', {'player': 1, 'control': 'a', 'frames': 601})
check('and a press is bounded the same way',
      t and t.startswith('PRESS ERROR') and 'INPUT set' in t)
# A stretch of play as one request: this core maps no memory to wait
# on, so the steps that need none. The frame after the press lets the
# core log A let go, which the log checks below read changes of.
msg, t, _ = call(219, 'STEPS', {'steps': 'run 3; press 1 a 2; run 1'})
check('STEPS runs its steps in one request: ' + (t or '').split('\n')[0],
      t and t.startswith('DONE 6') and not msg['result']['isError']
      and msg['result'].get('structuredContent') == {'rows': [], 'done': 6, 'step': 3})
# Queued, it answers at once and the content plays on through it, then
# pauses with nothing left.
msg, t, _ = call(260, 'STEPS', {'steps': 'run 20; run 20', 'queue': 'on'})
check('a queued STEPS answers at once: ' + (t or '').split('\n')[0],
      msg and msg['result'].get('structuredContent') == {'rows': [], 'queued': 2})
for i in range(100):
    msg, t, _ = call(261 + i, 'STEPS', {'steps': '', 'queue': 'on'})
    if t and t.startswith('QUEUED 0'):
        break
    time.sleep(0.02)
_, st, _ = call(362, 'GET_STATUS')
check('and pauses once it has run: ' + (st or '').strip(),
      t and t.startswith('QUEUED 0') and st and 'PAUSED' in st)
time.sleep(0.1)
msg, t, _ = call(363, 'STEPS', {'steps': '', 'queue': 'on'})
sc = msg and msg['result'].get('structuredContent') or {}
check('run dry, it says how long it has sat idle: %s' % sc,
      sc.get('queued') == 0 and sc.get('idle', 0) >= 80)
# A request not queued ends the queue, and its reply says how the
# queue ended: replaced, and on which step.
call(364, 'STEPS', {'steps': 'run 300', 'queue': 'on'})
msg, t, _ = call(365, 'RUN_FRAMES', {'count': 1})
sc = msg and msg['result'].get('structuredContent') or {}
check('a request after a queue says it replaced it: %s' % sc,
      sc.get('frames') == 1 and 'replaced' in sc.get('previous', {})
      and sc['previous'].get('step') == 1)
msg, t, _ = call(366, 'RUN_FRAMES', {'count': 1})
sc = msg and msg['result'].get('structuredContent') or {}
check('and only once', sc == {'rows': [], 'frames': 1})
_, t, _ = call(218, 'PRESS', {'player': 1, 'control': 'left_x', 'frames': 2})
check('an axis is INPUT\'s, not PRESS\'s',
      t and t.startswith('PRESS ERROR') and 'axis' in t)

# This core maps no memory, so RUN_UNTIL cannot wait on a byte here -
# and says so before running a frame, rather than after a wait that
# could not have ended. command_frames covers the waiting itself.
_, t, _ = call(221, 'RUN_UNTIL', {'address': '1000', 'condition': '==',
                                  'value': 0})
check('RUN_UNTIL refuses an address nothing maps: ' + (t or '').strip(),
      t and t.startswith('RUN_UNTIL ERROR'))
send({'jsonrpc': '2.0', 'id': 222, 'method': 'tools/call',
      'params': {'name': 'RUN_UNTIL', 'arguments': {'address': '1000',
                 'condition': '~=', 'value': 0}}})
msg, _ = recv(222)
err = (msg or {}).get('error', {})
check('and a condition it does not take is invalid params',
      err.get('code') == -32602 and '==' in err.get('message', ''))

# INPUT show looks without touching: set must set to answer and release
# lets go before it does.
_, t, _ = call(223, 'INPUT', {'action': 'set', 'player': 1, 'control': 'start', 'value': 1})
_, t2, _ = call(224, 'INPUT', {'action': 'show'})
_, t3, _ = call(225, 'INPUT', {'action': 'show', 'player': 1})
check('INPUT show reads what is held, twice the same: ' + (t2 or '').strip(),
      t2 and t2 == t and t3 == t2)
call(226, 'INPUT', {'action': 'release'})
# A path that is not a core is refused before the running core goes.
_, t, _ = call(227, 'CORE', {'action': 'load', 'core': '/no/such/core.so'})
check('CORE load says why: ' + (t or '').strip(),
      t and t.startswith('CORE ERROR') and 'no core at' in t)
_, st, _ = call(229, 'GET_STATUS')
check('and the content it was running is still running',
      st and ('PLAYING' in st or 'PAUSED' in st))

# A watch rides along with every step, so a step needs no reads after
# it. This core maps no memory, so the refusal is what is checkable
# here; command_frames covers the carrying.
send({'jsonrpc': '2.0', 'id': 232, 'method': 'tools/call',
      'params': {'name': 'MENU', 'arguments': {'action': 'toggle'}}})
msg, _ = recv(232)
err = (msg or {}).get('error', {})
check('MENU names the actions it takes: ' + err.get('message', ''),
      err.get('code') == -32602 and 'up|down' in err.get('message', ''))
send({'jsonrpc': '2.0', 'id': 233, 'method': 'tools/call',
      'params': {'name': 'MENU', 'arguments': {'action': ''}}})
msg, _ = recv(233)
check('and an empty one is not one of them',
      (msg or {}).get('error', {}).get('code') == -32602)
# A refused preset leaves the shader setting as it was. A build without
# shaders offers neither SET_SHADER nor the option.
if 'SET_SHADER' in names:
    _, before, _ = call(234, 'OPTION', {'action': 'show', 'option': 'shader'})
    msg, t, _ = call(235, 'SET_SHADER', {'preset': '/no/such/preset.slangp'})
    _, after, _ = call(236, 'OPTION', {'action': 'show', 'option': 'shader'})
    check('a refused SET_SHADER changes nothing: ' + (after or '').strip(),
          msg and msg['result']['isError'] and before and 'off' in before
          and before == after)
    # The null video driver runs no shader, so there is none to toggle:
    # the toggle used to dereference the menu's shader, which no such
    # driver has.
    call(237, 'OPTION', {'action': 'set', 'option': 'shader', 'state': 'on'})
    _, st, _ = call(238, 'GET_STATUS')
    check('and turning shaders on where none run does not crash',
          st and st.startswith('GET_STATUS'))

# RetroArch's own menu, read and driven: every action answers with what
# the menu shows once it has run.
msg, t, _ = call(280, 'MENU', {'action': 'show'})
check('MENU show says when the menu is closed',
      msg and msg['result'].get('structuredContent') == {'rows': [],
                                                          'closed': True})
msg, t, _ = call(281, 'MENU', {'action': 'down'})
check('and refuses to press into a closed menu: ' + (t or '').strip(),
      msg and msg['result']['isError'] and 'MENU open' in t)
msg, t, _ = call(282, 'MENU', {'action': 'open'})
sc = msg and msg['result'].get('structuredContent') or {}
labels = [r['label'] for r in sc.get('rows', [])]
check('MENU open answers with what it shows: %s' % sc.get('title'),
      sc.get('title') and labels and sc.get('selected') == 0
      and sc.get('count') == len(labels))
msg, t, _ = call(283, 'MENU', {'action': 'down'})
sc = msg and msg['result'].get('structuredContent') or {}
check('a press answers after it has moved: %s' % sc.get('selected'),
      sc.get('selected') == 1)
msg, t, _ = call(284, 'MENU', {'action': 'select', 'index': 0})
sc = msg and msg['result'].get('structuredContent') or {}
check('select moves straight to an entry', sc.get('selected') == 0)
msg, t, _ = call(285, 'MENU', {'action': 'select', 'index': 999})
check('and refuses one the list does not have: ' + (t or '').strip(),
      msg and msg['result']['isError'] and 'no entry 999' in t)
msg, t, _ = call(286, 'MENU', {'action': 'b'})
sc = msg and msg['result'].get('structuredContent') or {}
check('b goes back a list: %s' % sc.get('title'),
      sc.get('title') and sc.get('title') != 'Quick Menu' and sc.get('rows'))
msg, t, _ = call(287, 'MENU', {'action': 'show', 'from': 2})
sc = msg and msg['result'].get('structuredContent') or {}
check('show lists from an entry',
      sc.get('rows') and sc['rows'][0]['index'] == '2')


def menu_walk(sc, labels, first_id):
    """Presses a on each label in turn, found as a client finds it."""
    for n, want in enumerate(labels):
        hit = [r['index'] for r in sc.get('rows', []) if r['label'] == want]
        if not hit:
            return {}
        msg, _, _ = call(first_id + n, 'MENU',
                         {'action': 'a', 'index': int(hit[0])})
        sc = msg and msg['result'].get('structuredContent') or {}
    return sc


def menu_home(first_id):
    """Opens the menu and goes back to Main Menu: it opens where it was."""
    msg, _, _ = call(first_id, 'MENU', {'action': 'open'})
    sc = msg and msg['result'].get('structuredContent') or {}
    for n in range(8):
        if sc.get('title') == 'Main Menu':
            break
        msg, _, _ = call(first_id + 1 + n, 'MENU', {'action': 'b'})
        sc = msg and msg['result'].get('structuredContent') or {}
    return sc


# Settings, Drivers, then the menu driver's own list, where the one in
# use is checked.
sc = menu_walk(sc, ('Settings', 'Drivers'), 290)
check('a list says the lists above it: %s' % sc.get('path'),
      sc.get('path') == 'Main Menu > Settings > Drivers')
sc = menu_walk(sc, ('Menu',), 292)
check('a list of choices marks the one in use: %s' % sc.get('rows'),
      {'label': 'rgui', 'value': 'checked'} in
      [dict((k, r[k]) for k in ('label', 'value')) for r in sc.get('rows', [])])
msg, t, _ = call(288, 'MENU', {'action': 'close'})
check('and close closes it',
      msg and msg['result'].get('structuredContent', {}).get('closed'))
# What waits on a second press says so, and b lets it go.
sc = menu_walk(menu_home(310), ('Quick Menu', 'Close Content'), 294)
check('a confirmation is a message over the list: %s' % sc.get('message'),
      'close content' in sc.get('message', '').lower())
msg, _, _ = call(295, 'MENU', {'action': 'b'})
sc = msg and msg['result'].get('structuredContent') or {}
_, st, _ = call(296, 'GET_STATUS')
check('and b dismisses it, closing nothing: %s' % sc.get('title'),
      'message' not in sc and st and 'CONTENTLESS' not in st)
# An entry that asks for text opens a keyboard, which type answers.
sc = menu_walk(menu_home(320), ('Settings', 'User', 'Username'), 298)
check('an entry that asks for text says so: %s' % sc.get('keyboard'),
      sc.get('keyboard') == 'Username')
msg, t, _ = call(305, 'MENU', {'action': 'type', 'text': 'two\nlines'})
check('type refuses a control character: ' + (t or '').strip(),
      msg and msg['result']['isError'] and 'control character' in t)
msg, t, _ = call(301, 'MENU', {'action': 'type', 'text': 'e2e player'})
sc = msg and msg['result'].get('structuredContent') or {}
check('and type enters it',
      'keyboard' not in sc and {'label': 'Username', 'value': 'e2e player'}
      in [dict((k, r[k]) for k in ('label', 'value'))
          for r in sc.get('rows', [])])
msg, t, _ = call(302, 'MENU', {'action': 'type', 'text': 'x'})
check('type with no keyboard open is refused: ' + (t or '').strip(),
      msg and msg['result']['isError'] and 'no keyboard' in t)
call(303, 'MENU', {'action': 'close'})
# MENU open and close are the menu's switch: OPTION has none.
msg, t, _ = call(304, 'OPTION', {'action': 'show', 'option': 'menu'})
check('menu is no longer an option', msg and msg['result']['isError'])

# GET_FRAME: the frame as an image, scaled down, and no file written.
def frame_of(msg):
    """(width, height) of a reply's PNG image, or None."""
    for c in (msg or {}).get('result', {}).get('content', []):
        if c.get('type') == 'image' and c.get('mimeType') == 'image/png':
            png = base64.b64decode(c['data'])
            if png[:8] == b'\x89PNG\r\n\x1a\n':
                return (int.from_bytes(png[16:20], 'big'),
                        int.from_bytes(png[20:24], 'big'))
    return None


msg, t, _ = call(310, 'GET_FRAME', {})
wh = frame_of(msg)
check('GET_FRAME answers with the frame as a PNG: %s, %s' % (wh, t),
      wh and max(wh) <= 320 and t == 'GET_FRAME %dx%d' % wh
      and not msg['result']['isError'])
msg, t, _ = call(311, 'GET_FRAME', {'size': 64})
wh = frame_of(msg)
check('and scaled down to fit the size asked for: %s' % (wh,),
      wh and max(wh) <= 64)
msg, t, _ = call(312, 'GET_FRAME', {'size': 1000})
check('a size past 640 is refused: ' + (t or '').strip(),
      msg and msg['result']['isError'] and '64 to 640' in t)
check('and nothing was written: %s' % os.listdir(shotdir),
      not os.listdir(shotdir))

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

# RESET with paused on is a fresh start that waits for the first step.
call(244, 'OPTION', {'action': 'set', 'option': 'pause', 'state': 'off'})
_, st0, _ = call(248, 'GET_STATUS')
msg, t, _ = call(245, 'RESET', {'paused': 'on'})
_, st, _ = call(246, 'GET_STATUS')
check('RESET paused resets and stays paused: ' + (st or '').strip(),
      st0 and 'PLAYING' in st0
      and t and t.strip().startswith('RESET') and not msg['result']['isError']
      and st and 'PAUSED' in st)

# LOAD_CONTENT with paused on stops the game the frame it has loaded, so
# nothing runs before the first step: a title screen's demo, say.
msg, t, _ = call(249, 'LOAD_CONTENT', {'core': core, 'content': rom,
                                       'paused': ''})
check('an empty paused is left out, not put on the path: ' + (t or '').strip(),
      t and t.startswith('LOAD_CONTENT') and not msg['result']['isError'])
send({'jsonrpc': '2.0', 'id': 254, 'method': 'tools/call',
      'params': {'name': 'LOAD_CONTENT', 'arguments': {
          'core': core, 'content': rom, 'paused': True}}})
msg, _ = recv(254)
err = (msg or {}).get('error', {})
check('a JSON true for paused is invalid params: ' + err.get('message', ''),
      err.get('code') == -32602 and '"paused"' in err.get('message', ''))
msg, t, _ = call(250, 'LOAD_CONTENT', {'core': core, 'content': rom,
                                       'paused': 'on'}, )
check('LOAD_CONTENT paused answers once loaded: ' + (t or '').strip(),
      t and t.startswith('LOAD_CONTENT') and not msg['result']['isError'])
a = frame_count(251)
time.sleep(1)
b = frame_count(252)
_, st, _ = call(253, 'GET_STATUS')
check('and stays paused: %s -> %s, %s' % (a, b, (st or '').strip()),
      a is not None and a == b and st and 'PAUSED' in st)

# The lists and the core's options, then the memory tools, on a core
# that maps some.
msg, t, _ = call(270, 'LIST_CORES', {})
check('LIST_CORES finds the core in its directory',
      t and 'video_views' in t)
msg, t, _ = call(271, 'LIST_PLAYLISTS', {})
check('LIST_PLAYLISTS finds the playlist', t and 'e2e.lpl' in t)
msg, t, _ = call(272, 'GET_PLAYLIST', {'playlist': 'e2e.lpl'})
sc = msg and msg['result'].get('structuredContent') or {}
check('and GET_PLAYLIST its entry: %s' % sc.get('rows'),
      [r.get('label') for r in sc.get('rows', [])] == ['Blank'])
msg, t, _ = call(273, 'CORE_OPTION', {'action': 'show'})
check('CORE_OPTION show lists the core\'s options',
      msg and not msg['result']['isError']
      and msg['result'].get('structuredContent', {}).get('rows'))
msg, t, _ = call(274, 'LOAD_CONTENT', {'core': mem_core, 'content': rom,
                                       'paused': 'on'})
check('a core that maps memory loads: ' + (t or '').strip(),
      t and t.startswith('LOAD_CONTENT') and not msg['result']['isError'])
msg, t, _ = call(275, 'GET_MEMORY_MAP', {})
check('GET_MEMORY_MAP lists its regions',
      len((msg and msg['result'].get('structuredContent') or {})
          .get('rows', [])) == 2)
msg, t, _ = call(276, 'READ_CORE_MEMORY', {'address': '1000', 'bytes': 4})
sc = msg and msg['result'].get('structuredContent') or {}
check('READ_CORE_MEMORY is a record: %s' % sc,
      sc == {'address': '1000', 'bytes': '00 01 02 03'})
msg, t, _ = call(277, 'READ_CORE_MEMORY', {'address': '101e', 'bytes': 4})
sc = msg and msg['result'].get('structuredContent') or {}
check('and says where memory ran out: %s' % sc,
      sc.get('truncated') is True and sc.get('read') == 2)
msg, t, _ = call(278, 'RUN_UNTIL', {'address': '1007', 'condition': '!=',
                                    'value': 0})
sc = msg and msg['result'].get('structuredContent') or {}
check('RUN_UNTIL waits on a byte: %s' % sc,
      'met' in sc and sc.get('value', 0) != 0)

# A core with no memory map: its RAM, as it hands it over - system RAM
# from 0, save RAM straight after - is what every memory tool reads.
msg, t, _ = call(279, 'LOAD_CONTENT', {'core': nomap_core, 'content': rom,
                                       'paused': 'on'})
msg, t, _ = call(280, 'GET_MEMORY_MAP', {})
rows = (msg and msg['result'].get('structuredContent') or {}).get('rows', [])
check('a core with no map lists the RAM it hands over: %s'
      % [(r['start'], r['length'], r['kind'], r['space']) for r in rows],
      [(r['start'], r['length'], r['kind'], r['space']) for r in rows]
      == [('0', '10', 'system', 'core'), ('10', '10', 'save', 'core')])
msg, t, _ = call(281, 'READ_CORE_MEMORY', {'address': 'e', 'bytes': 4})
sc = msg and msg['result'].get('structuredContent') or {}
check('and reads it, across into save RAM: %s' % sc,
      sc == {'address': 'e', 'bytes': '0E 0F 10 11'})
msg, t, _ = call(282, 'WATCH', {'watches': 'f 7'})
check('WATCH watches it: ' + (t or '').strip().replace('\n', ' | '),
      msg and not msg['result']['isError'])
msg, t, _ = call(283, 'RUN_UNTIL', {'address': '7', 'condition': '!=',
                                    'value': 7})
sc = msg and msg['result'].get('structuredContent') or {}
check('and a condition waits on it: %s' % sc, 'met' in sc)
call(284, 'WATCH', {'watches': 'f off'})

missing = sorted(set(out_schemas) - schema_checked)
check('every tool with an outputSchema was checked: %s' % (missing or 'all'),
      not missing)
check('and every reply matched it', not schema_errors)
for e in schema_errors[:5]:
    print('   ', e)

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

# From a cold start, with no content running yet, paused still pauses:
# after the first frame, which is what marks a core running, so the
# steps that need one still work.
p = subprocess.Popen([ra, '--mcp-stdio', '-c', cfg],
                     stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                     stderr=subprocess.DEVNULL)
buf = b''
send({'jsonrpc': '2.0', 'id': 1, 'method': 'initialize',
      'params': {'protocolVersion': '2025-06-18', 'capabilities': {},
                 'clientInfo': {'name': 'e2e', 'version': '1'}}})
recv(1, 60)
send({'jsonrpc': '2.0', 'method': 'notifications/initialized'})
# With nothing running the menu cannot close, and there is no core to
# ask for options: both say so, and what to do.
msg, t, _ = call(7, 'MENU', {'action': 'close'})
check('MENU close with no content says the menu stays open: '
      + (t or '').strip(),
      msg and msg['result']['isError'] and 'stays open' in t)
msg, t, _ = call(8, 'CORE_OPTION', {'action': 'show'})
check('CORE_OPTION with no core running says so: ' + (t or '').strip(),
      msg and msg['result']['isError'] and 'no core is running' in t)
msg, t, _ = call(2, 'LOAD_CONTENT', {'core': core, 'content': rom,
                                     'paused': 'on'})
check('cold LOAD_CONTENT paused answers once loaded: ' + (t or '').strip(),
      t and t.startswith('LOAD_CONTENT') and not msg['result']['isError'])
# The frames shown go on counting while a pause shows the last one
# again, so whether the game runs is read off the game: this core logs
# the pad when it changes, which only a frame it runs can do.
send({'jsonrpc': '2.0', 'id': 3, 'method': 'logging/setLevel',
      'params': {'level': 'info'}})
_, notes = recv(3)
_, _, n2 = call(4, 'INPUT', {'action': 'set', 'player': 1, 'control': 'a', 'value': 1})
notes += n2
time.sleep(1)
_, st, n2 = call(5, 'GET_STATUS')
notes += n2
logs = [n['params']['data'] for n in notes if n.get('method') == 'notifications/message']
check('and stays paused, the game reading nothing: ' + (st or '').strip(),
      st and 'PAUSED' in st and not any('buttons=0x0100' in l for l in logs))
msg, t, n2 = call(6, 'RUN_FRAMES', {'count': 3})
logs = [n['params']['data'] for n in n2 if n.get('method') == 'notifications/message']
check('and steps from there, the game reading A: ' + (t or '').strip(),
      t and t.startswith('RUN_FRAMES 3') and not msg['result']['isError']
      and any('buttons=0x0100' in l for l in logs))
p.stdin.close()
try:
    p.wait(30)
except subprocess.TimeoutExpired:
    p.kill()
print('FAIL' if fails else 'PASS')
sys.exit(1 if fails else 0)
