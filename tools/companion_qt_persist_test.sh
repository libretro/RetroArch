#!/bin/sh
# Regression test: the Qt desktop companion's window geometry and dock
# layout are saved to retroarch.cfg when RetroArch quits with the
# companion window STILL OPEN (nobody should have to close the companion
# first), and a saved layout restores and re-saves identically.
#
# Needs a Qt-enabled RetroArch binary (./retroarch by default, or $RA)
# and runs it headless under QT_QPA_PLATFORM=offscreen with the null
# video/audio/input drivers; --max-frames drives the normal quit path
# (retroarch_main_quit -> config_save_file), exactly as a user quitting
# from the RetroArch menu does.
#
#   tools/companion_qt_persist_test.sh [path/to/retroarch]
set -eu
cd "$(dirname "$0")/.."

RA=${1:-${RA:-./retroarch}}
if [ ! -x "$RA" ]; then
   echo "companion_qt_persist_test: $RA not found - build with --enable-qt first" >&2
   exit 2
fi

OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
CFG="$OUT/retroarch.cfg"
fails=0

fail() { echo "FAIL: $*" >&2; fails=$((fails + 1)); }
row()  { grep "^$1 = " "$CFG" | sed 's/^[^=]*= "\(.*\)"$/\1/' || true; }

# Each run also writes every dock's size limits ($1, see qt_dock_report).
run_ra() {
   RETROARCH_QT_DOCK_REPORT="${1:-$OUT/limits.cur}" \
   QT_QPA_PLATFORM=offscreen HOME="$OUT" XDG_RUNTIME_DIR="$OUT" \
      timeout -s KILL 120 "$RA" -c "$CFG" --max-frames=120 --menu \
      > "$OUT/run.log" 2>&1 || {
         rc=$?
         fail "retroarch exited with status $rc"
         echo "--- last 40 lines of run.log ---" >&2
         tail -n 40 "$OUT/run.log" >&2
         echo "--------------------------------" >&2
      }
}

# A restore sizes docks by pinning their limits for a moment; a limit
# still pinned afterwards leaves a dock that cannot be resized, dragged
# into another column, or put back where it was saved (Tatsuya79: "the
# docks position is wrong", "you cannot resize it like before"). A pin
# shows as a lowered maximum, or a minimum equal to the maximum, where
# the default layout has neither; minimums alone follow the contents.
check_limits() {
   [ -s "$OUT/limits.cur" ] || { fail "$1: no dock report written"; return; }
   bad=$(awk '
      function lim(f, k,   a) { split(substr(f, 3), a, /\.\./); return a[k] }
      # Below the default and below the layout maximum (QLAYOUTSIZE_MAX,
      # what a shown dock gets in place of the widget maximum).
      function low(m, b) { return m + 0 < b + 0 && m + 0 < 524287 }
      NR == FNR { bw[$1] = $4; bh[$1] = $5; next }
      ($1 in bw) {
         if (low(lim($4, 2), lim(bw[$1], 2)) || low(lim($5, 2), lim(bh[$1], 2)) ||
             (lim($4, 1) == lim($4, 2) && lim(bw[$1], 1) != lim(bw[$1], 2)) ||
             (lim($5, 1) == lim($5, 2) && lim(bh[$1], 1) != lim(bh[$1], 2)))
            print "   " $1 ": " $4 " " $5 " (default layout " bw[$1] " " bh[$1] ")"
      }' "$OUT/limits.base" "$OUT/limits.cur")
   if [ -n "$bad" ]; then
      fail "$1: dock size limits left pinned by the restore:"
      printf '%s\n' "$bad" >&2
   fi
}

# ---- pass 0: the default layout with nothing saved: the limits every
# ---- dock must still have after a restore ----
cat > "$CFG" <<'EOF'
menu_driver = "rgui"
video_driver = "null"
audio_driver = "null"
input_driver = "null"
config_save_on_exit = "false"
desktop_menu_enable = "true"
ui_companion_enable = "true"
ui_companion_start_on_boot = "true"
ui_companion_toggle = "true"
desktop_menu_show_welcome_screen = "false"
EOF
run_ra "$OUT/limits.base"
[ -s "$OUT/limits.base" ] || fail "default layout: no dock report written"

# ---- pass 1: fresh config, companion shown on boot, nothing saved yet ----
cat > "$CFG" <<'EOF'
menu_driver = "rgui"
video_driver = "null"
audio_driver = "null"
input_driver = "null"
config_save_on_exit = "true"
desktop_menu_enable = "true"
ui_companion_enable = "true"
ui_companion_start_on_boot = "true"
ui_companion_toggle = "true"
desktop_menu_show_welcome_screen = "false"
desktop_menu_save_geometry = "true"
desktop_menu_save_dock_positions = "true"
desktop_menu_dock_log = "bottom,1,0,150,-,0"
desktop_menu_dock_screenshot = "right,1,0,420,-,0,0"
desktop_menu_dock_title = "right,1,0,210,-,0,1"
desktop_menu_dock_core_info = "float,1,300,200,-,0,100,60"
desktop_menu_shortcuts_order = "log,settings:bogus.0.0,core_options"
desktop_menu_shortcuts_removed = "boxart"
desktop_menu_core_options_window = "-1600,40,500,400"
EOF
run_ra

check_limits "seeded layout"

# Window geometry landed although the window was never closed by hand.
w=$(row desktop_menu_window_width); h=$(row desktop_menu_window_height)
[ "${w:-0}" -gt 0 ] 2>/dev/null || fail "window width not saved at quit (got '$w')"
[ "${h:-0}" -gt 0 ] 2>/dev/null || fail "window height not saved at quit (got '$h')"

# Every dock has a row: seven fields docked, nine (with x,y) floating.
for d in search playlists core boxart title screenshot logo core_info log shortcuts; do
   r=$(row desktop_menu_dock_$d)
   n=$(printf '%s' "$r" | awk -F, '{ print NF }')
   case "$r" in
      float,*) [ "$n" -eq 9 ] || fail "floating dock $d row has $n fields: '$r'" ;;
      left,*|right,*|top,*|bottom,*)
         [ "$n" -eq 7 ] || fail "dock $d row has $n fields: '$r'" ;;
      *) fail "dock $d row missing or malformed: '$r'" ;;
   esac
done

# The seeded layout was honoured: log shown at the bottom, screenshots
# pulled out of the thumbnail tab group into its own dock on the right.
case "$(row desktop_menu_dock_log)" in
   bottom,1,*) ;;
   *) fail "log dock not restored shown at the bottom: '$(row desktop_menu_dock_log)'" ;;
esac
case "$(row desktop_menu_dock_screenshot)" in
   right,1,*,-,*) ;;
   *) fail "screenshot dock not restored as its own right-side dock: '$(row desktop_menu_dock_screenshot)'" ;;
esac
# Screenshots above Title Screen, both standing alone on the right, and
# Screenshots the taller of the two - the order and the height rows won
# over the built-in dock order (Tatsuya79: "it keeps putting title screen
# on top and screenshot on bottom when restarting", "screenshot is
# smaller than the other one").
field() { printf '%s' "$1" | awk -F, -v n="$2" '{ print $n }'; }
ss=$(row desktop_menu_dock_screenshot); ts=$(row desktop_menu_dock_title)
so=$(field "$ss" 7); to=$(field "$ts" 7)
[ "${so:-9}" -lt "${to:-0}" ] 2>/dev/null \
   || fail "screenshot dock (slot $so) not above title dock (slot $to): '$ss' / '$ts'"
case "$ts" in
   right,1,*,-,*) ;;
   *) fail "title dock not restored as its own right-side dock: '$ts'" ;;
esac
sh_=$(field "$ss" 4); th_=$(field "$ts" 4)
[ "${sh_:-0}" -gt "${th_:-0}" ] 2>/dev/null \
   || fail "screenshot dock ($sh_) not taller than title dock ($th_) as saved (420 vs 210)"
# The floating Core Info dock came back floating, at its saved size
# and position.
case "$(row desktop_menu_dock_core_info)" in
   float,1,300,200,-,0,0,100,60) ;;
   *) fail "floating dock not restored in place: '$(row desktop_menu_dock_core_info)'" ;;
esac
# The remaining thumbnail dock is still tabbed onto boxart.
for d in logo; do
   case "$(row desktop_menu_dock_$d)" in
      *,boxart,*) ;;
      *) fail "$d dock lost its boxart tab partner: '$(row desktop_menu_dock_$d)'" ;;
   esac
done

# Shortcuts: nothing rewrites the order row unasked (a foreign token
# such as another build's pinned page survives), the removed button
# stays removed, the default dock layout gave Shortcuts a row, and a
# window row on a monitor left of the primary is kept as it was.
[ "$(row desktop_menu_shortcuts_order)" = "log,settings:bogus.0.0,core_options" ] \
   || fail "shortcut order rewritten at startup: '$(row desktop_menu_shortcuts_order)'"
[ "$(row desktop_menu_shortcuts_removed)" = boxart ] \
   || fail "removed shortcuts not kept: '$(row desktop_menu_shortcuts_removed)'"
case "$(row desktop_menu_dock_shortcuts)" in
   left,1,*) ;;
   *) fail "shortcuts dock not on the left by default: '$(row desktop_menu_dock_shortcuts)'" ;;
esac
[ "$(row desktop_menu_core_options_window)" = "-1600,40,500,400" ] \
   || fail "core options window row changed: '$(row desktop_menu_core_options_window)'"

# ---- pass 2: restore what pass 1 saved, then re-save: must be identical ----
grep '^desktop_menu_' "$CFG" | sort > "$OUT/pass1.rows"
run_ra
grep '^desktop_menu_' "$CFG" | sort > "$OUT/pass2.rows"
check_limits "restored layout"
if ! cmp -s "$OUT/pass1.rows" "$OUT/pass2.rows"; then
   fail "layout changed across a restore/re-save round trip:"
   diff "$OUT/pass1.rows" "$OUT/pass2.rows" >&2 || true
fi

if [ "$fails" -ne 0 ]; then
   echo "companion_qt_persist_test: $fails failure(s)" >&2
   exit 1
fi
echo "companion_qt_persist_test: OK"
