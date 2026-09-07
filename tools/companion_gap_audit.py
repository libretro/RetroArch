#!/usr/bin/env python3
"""Feature gap audit: what Qt's companion window offers vs what the
Cocoa and Win32 companions implement. Static (regex evidence per
feature); pair it with COMPANION_AUDIT=1 tools/companion_cocoa_test.sh,
which fires every action the Cocoa controller implements on a real
AppKit under GNUstep. Prints the table and the gap lists; exit 0."""
import re, sys, os
root = os.path.join(os.path.dirname(__file__), '..')
qt  = open(os.path.join(root, 'ui/drivers/ui_qt.cpp')).read()
cc  = open(os.path.join(root, 'ui/drivers/ui_cocoa_companion.m')).read()
w32 = open(os.path.join(root, 'ui/drivers/ui_win32_companion.c')).read()
FEATS = [
 ("File menu: Load Core",             r"MENU_FILE_LOAD_CORE", r"MENU_FILE_LOAD_CORE|loadCore:", r"IDM_CW_LOAD_CORE\b"),
 ("File menu: Unload Core",           r"MENU_FILE_UNLOAD_CORE", r"unloadCore|UNLOAD_CORE", r"IDM_CW_UNLOAD_CORE"),
 ("File menu: Exit RetroArch",        r"MENU_FILE_EXIT", r"quitRetroArch|MENU_FILE_EXIT", r"IDM_CW_QUIT"),
 ("View: Shader Params",              r"onShaderParamsClicked", r"showShaderParams", r"IDM_CW_SHADER_PARAMS"),
 ("View: Core Options",               r"onCoreOptionsClicked", r"showCoreOptions", r"IDM_CW_CORE_OPTIONS"),
 ("View: Options dialog",             r"MENU_VIEW_OPTIONS\b", r"showOptions", r"IDM_CW_OPTIONS\b"),
 ("Help: About",                      r"MENU_HELP_ABOUT\b", r"aboutRetroArch|orderFrontStandardAboutPanel", r"IDM_CW_HELP_ABOUT\b"),
 ("Help: About contributors",         r"CONTRIBUTORS", r"aboutContributors", r"IDM_CW_HELP_CONTRIBUTORS"),
 ("Playlist context: rename",         r"renamePlaylist|PLAYLIST_RENAME", r"renamePlaylist", r"IDM_CW_RENAME_PLAYLIST"),
 ("Playlist context: hide/show",      r"hidePlaylist|HIDE_PLAYLIST", r"hidePlaylist", r"hide_playlist|HIDE"),
 ("Playlist context: download thumbnails", r"thumbnailPack|DOWNLOAD_THUMBNAILS", r"thumbnail_pack|downloadThumbnails", r"thumbnail_pack|DOWNLOAD_THUMB"),
 ("Entry context: set core association", r"SET_CORE_ASSOCIATION|associat", r"associateCore", r"associate|ASSOCIATION"),
 ("Entry context: delete entry",      r"deletePlaylistItem|DELETE_ENTRY|deleteEntry", r"deleteEntry", r"delete_entry|DELETE_ENTRY"),
 ("Drag & drop files onto playlist",  r"onPlaylistFilesDropped", r"acceptDrop", r"WM_DROPFILES"),
 ("Drag & drop thumbnail onto boxart", r"onThumbnailDropped", r"installThumbnailFromPath", r"companion_core_thumbnail_install"),
 ("Scan directory",                   r"onScanDirectoryClicked", r"scanDirectory", r"scan_dir|SCAN"),
 ("Load custom core (file picker)",   r"onLoadCustomCoreClicked", r"loadCustomCore|NSOpenPanel", r"cw_load_custom_core"),
 ("Stop content",                     r"onStopClicked", r"stopContent", r"IDC_CW_STOP_BTN"),
 ("Log dock",                         r"LogWidget", r"toggleLog", r"IDC_CW_LOG|cw_log"),
 ("Theme (dark)",                     r"CUSTOM_THEME|setTheme", r"applyTheme", r"theme"),
 ("Search filters entries",           r"onSearchLineEditEdited", r"searchChanged", r"IDC_CW_SEARCH"),
 ("Search Enter selects+runs",        r"onSearchEnterPressed", r"searchEnter|runSelected", r"cw_search_proc"),
 ("Core Options dialog",              r"CoreOptionsDialog", r"optsTable", r"cw_opts_show"),
 ("Shader params dialog",             r"ShaderParamsDialog", r"shpTable", r"cw_shp_show"),
]
print("%-40s %-6s %-6s %s" % ("feature", "qt", "cocoa", "win32"))
gc, gw = [], []
for f, q, c, w in FEATS:
    hq, hc, hw = (bool(re.search(x, y)) for x, y in ((q, qt), (c, cc), (w, w32)))
    print("%-40s %-6s %-6s %s" % (f, "yes" if hq else "-", "yes" if hc else "NO", "yes" if hw else "NO"))
    if hq and not hc: gc.append(f)
    if hq and not hw: gw.append(f)
print("\nCocoa gaps vs Qt (%d):" % len(gc)); [print("  -", g) for g in gc]
print("Win32 gaps vs Qt (%d):" % len(gw)); [print("  -", g) for g in gw]
