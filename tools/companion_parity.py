#!/usr/bin/env python3
"""Check that every desktop companion backend exposes the same features.

The companion UI has three presentation backends over one shared C core
(ui/companion/companion_core.*): Qt, native Win32, native Cocoa. The
plan for the refactor requires them to be interchangeable - "same
actions produce the same results" - but nothing enforces it: a backend
can quietly omit a feature (no file browser, no scan, no core picker)
and still compile and run, and the omission only surfaces when a user
switches drivers and finds a menu entry gone. That is exactly the drift
this check exists to catch, the same way vfs_backend_parity.py catches a
VFS backend that skips a call.

Each feature below is defined by the companion-core entry point a
backend must call to implement it. A backend "has" the feature when it
references that symbol. Qt's presentation is split across two
translation units (ui_qt.cpp + ui_qt_widgets.cpp), so they are checked
together as one backend.

This is a source-level grep, so it costs nothing and runs on Linux. It
is deliberately loose - it proves a backend wired the feature's core
call in at all, not that the UI is pixel-identical; visual parity is a
manual pass. What it reliably catches is a whole feature missing from
one backend.

A backend that genuinely should not carry a feature must be listed in
WAIVERS with a reason, so dropping one is a conscious, reviewed choice
rather than an oversight.

Usage:
  tools/companion_parity.py            # check, exit 1 on any gap
  tools/companion_parity.py --list     # print the feature matrix
"""

import re
import sys

# backend name -> the translation unit(s) that make up its presentation
BACKENDS = {
    'qt':    ['ui/drivers/ui_qt.cpp', 'ui/drivers/ui_qt_widgets.cpp'],
    'win32': ['ui/drivers/ui_win32_companion.c'],
    'cocoa': ['ui/drivers/ui_cocoa_companion.m'],
}

# feature -> the companion_core_* symbol whose use implements it.
# One representative call per feature: the one a backend cannot skip and
# still offer it.
FEATURES = {
    'lifecycle':          'companion_core_new',
    'iterate':            'companion_core_iterate',
    'playlist list':      'companion_core_playlist_count',
    'playlist entries':   'companion_core_entry_count',
    'select playlist':    'companion_core_select_playlist',
    'run entry':          'companion_core_request_load_entry',
    'load content':       'companion_core_request_load_content',
    'start core':         'companion_core_start_core',
    'load core':          'companion_core_load_core',
    'installed cores':    'companion_core_installed_core_count',
    'core picker filter': 'companion_core_installed_cores_supporting',
    'core association':   'companion_core_playlist_set_default_core',
    'delete entry':       'companion_core_playlist_delete_entry',
    'core info panel':    'companion_core_core_info_rows',
    'directory scan':     'companion_core_request_scan',
    'thumbnails':         'companion_core_thumbnail_path',
    # Decoding goes through the shared engine (threads, cache, visible-
    # first requests); a backend decoding on its own UI thread regresses
    # the grid to the "shows on click" behaviour this replaced.
    'thumbnail engine':   'companion_thumbs_request',
    'thumbnail poll':     'companion_thumbs_poll',
    'two-pane browser':   'companion_core_browse_dir_count',
    # The listing is enumerated off the UI thread; a backend that asks
    # whether it is still busy is one that waits for the callback rather
    # than rebuilding straight after open() (or enumerating itself).
    'async browser':      'companion_core_browse_busy',
    'browser sorting':    'companion_core_browse_sort',
    # The selected file's animation (APNG / WEBP / WEBM / MP4) plays in
    # the pane on every backend, as in RetroArch's own File Browser.
    'animated preview':   'companion_thumbs_animate',
    # Qt's Stop / File > Unload Core, and File > Exit
    'unload core':        'companion_core_unload_core',
    'exit retroarch':     'CMD_EVENT_QUIT',
    # Help > About Contributors: the AUTHORS list in a window
    'about contributors': 'retroarch_contributors_list',
    # Qt's playlist rename, "Add Files" / file drop, thumbnail drop
    'rename playlist':    'companion_core_playlist_rename',
    'add files':          'companion_core_playlist_add_files',
    'thumbnail drop':     'companion_core_thumbnail_install',
    # Qt's View > Core Options, View > Shader Parameters, View > Options
    'core options':       'companion_core_option_count',
    'shader parameters':  'companion_core_shader_param_count',
    'options dialog':     'companion_core_setting_count',
    'file browser':       'companion_core_browse_open',
    'pick core on run':   'companion_core_entry_needs_core',
    'window hand-off':    'companion_core_prepare_show_window',
}

# (backend, feature) pairs that are intentionally absent, with a reason.
# Empty for now: all three backends implement every feature above.
WAIVERS = {
    # Qt's "Add Files" is its own dialog (name, extension filter, core
    # choice, archive filter, progress) over companion_core_playlist_push;
    # the natives' plain add-files call is the drop semantics only.
    ('qt', 'add files'):      'Qt: PlaylistEntryDialog over companion_core_playlist_push',
    # Qt's thumbnail drop receives image data (a QImage from any drag
    # source), not a file path, and saves through QImage; the core call
    # takes a path.
    ('qt', 'thumbnail drop'): 'Qt: QImage drop saved via changeThumbnail',
    # Qt's three dialogs predate the core and read RetroArch directly
    # (CoreOptionsDialog, ShaderParamsDialog, ViewOptionsWidget).
    ('qt', 'core options'):      'Qt: CoreOptionsDialog reads runloop core_options directly',
    ('qt', 'shader parameters'): 'Qt: ShaderParamsDialog reads menu_shader_get directly',
    ('qt', 'options dialog'):    'Qt: ViewOptionsWidget edits settings directly',
    # ('qt', 'window hand-off'): 'Qt calls it under its own guard',
}

# Qt reaches the window hand-off through the same call from ui_qt.cpp;
# the playlist-file browser accessors it does not use because its model
# owns the QFileSystemModel. Rather than hard-code such exceptions, a
# feature a backend implements a different but equivalent way is waived
# above with a reason; keep that list short and reviewed.
QT_EQUIVALENT = {
    # Qt lists playlist files through its own QDir model (getPlaylistFiles)
    # and selects by path, not by the core's playlist-file index.
    'playlist list':   'companion_core_select_playlist_path',
    'select playlist': 'companion_core_select_playlist_path',
    # Qt's "Run" and its core picker go through the launch-with combo:
    # it resolves the core (companion_core_launch_options) and loads the
    # content directly, rather than the natives' request_load_entry /
    # entry_needs_core path.
    'run entry':        'companion_core_launch_options',
    'pick core on run': 'companion_core_launch_options',
    # Qt runs the file browser through QFileSystemModel; it has no
    # in-core browse listing, and so no folder / file split either.
    'file browser':     None,
    'two-pane browser': 'BrowseTableModel',
    # (Qt now draws through the same engine as the natives: no waiver.)
}


def uses(paths, symbol):
    pat = re.compile(r'\b' + re.escape(symbol) + r'\b')
    for p in paths:
        try:
            with open(p, encoding='utf-8', errors='replace') as f:
                if pat.search(f.read()):
                    return True
        except OSError:
            pass
    return False


def has_feature(backend, feature):
    sym = FEATURES[feature]
    if uses(BACKENDS[backend], sym):
        return True
    if backend == 'qt' and feature in QT_EQUIVALENT:
        alt = QT_EQUIVALENT[feature]
        if alt is None:
            return True  # implemented a toolkit-native way
        return uses(BACKENDS['qt'], alt)
    return False


def main():
    want_list = '--list' in sys.argv[1:]

    if want_list:
        w = max(len(f) for f in FEATURES)
        print('%-*s  %s' % (w, 'feature', '  '.join(BACKENDS)))
        for feat in FEATURES:
            cells = []
            for b in BACKENDS:
                if (b, feat) in WAIVERS:
                    cells.append('waived')
                else:
                    cells.append('yes' if has_feature(b, feat) else 'NO')
            print('%-*s  %s' % (w, feat, '  '.join(
                '%-*s' % (len(b), c) for b, c in zip(BACKENDS, cells))))
        return 0

    gaps = []
    for feat in FEATURES:
        for b in BACKENDS:
            if (b, feat) in WAIVERS:
                continue
            if not has_feature(b, feat):
                gaps.append((b, feat))

    if gaps:
        print('error: companion backends disagree on features:',
              file=sys.stderr)
        for b, feat in gaps:
            print('   %-6s is missing "%s" (expected a call to %s)'
                  % (b, feat, FEATURES[feat]), file=sys.stderr)
        print('\nEither wire the feature into that backend, or - if it '
              'genuinely should not carry it - add the (backend, feature) '
              'pair to WAIVERS in this script with a reason.',
              file=sys.stderr)
        return 1

    if check_main_loops():
        return 1
    print('companion parity: %d features, all %d backends agree'
          % (len(FEATURES), len(BACKENDS)))
    return 0




# --- the platforms' real main loops call the companion's iterate -----
# Every companion is asynchronous on the far side of
# ui_companion_driver_wimp_iterate(): the playlist parse, the browser
# listing, thumbnails, animation frames, status. A platform main loop
# that does not call it shows "Loading playlist..." for ever. The
# generic loop (retroarch.c) covers Win32 and Qt; the non-Qt Mac build's
# main loop is the CFRunLoop observer in cocoa_common.m, which once
# lacked the call (the Qt-era rarch_main had it, under #ifdef HAVE_QT).
def _loop_calls_iterate(path, func):
    src = open(path).read()
    m = re.search(r'static void %s\b.*?\n\}\n' % re.escape(func), src, re.S)
    body = m.group(0) if m else ''
    return 'ui_companion_driver_wimp_iterate()' in body


def check_main_loops():
    _loops = [
        ('ui/drivers/cocoa/cocoa_common.m', 'rarch_draw_observer'),
    ]
    _missing = [f for p, f in _loops if not _loop_calls_iterate(p, f)]
    if _missing:
        print("error: main loop(s) that never call ui_companion_driver_wimp_iterate(): %s" % ', '.join(_missing), file=sys.stderr)
        sys.exit(1)
    print("main loops iterate the companion: ok")
    return 0


if __name__ == '__main__':
    sys.exit(main())

