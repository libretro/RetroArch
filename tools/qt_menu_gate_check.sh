#!/bin/sh
# Compile the Qt companion's two units with the menu and without it, as
# a build configured --disable-menu with Qt enabled has them: the
# settings widgets exist only with the menu, and no CI job builds Qt
# with the menu off. Needs Qt 6 or Qt 5 development files.
#
#   tools/qt_menu_gate_check.sh
set -eu

cd "$(dirname "$0")/.."

CXX=${CXX:-c++}
if pkg-config --exists Qt6Widgets 2>/dev/null; then
   QT="-std=c++17 $(pkg-config --cflags Qt6Widgets)"
elif pkg-config --exists Qt5Widgets 2>/dev/null; then
   QT="-std=c++11 $(pkg-config --cflags Qt5Widgets)"
else
   echo "qt_menu_gate_check: no Qt 6 or Qt 5 development files" >&2
   exit 1
fi

INC="-I. -Ilibretro-common/include -Ideps"
DEFS="-DRARCH_INTERNAL -DHAVE_THREADS -DHAVE_CONFIGFILE -DHAVE_QT -fPIC"
fail=0

for menu in -DHAVE_MENU ""; do
   name="with the menu"
   [ -n "$menu" ] || name="without the menu"
   for tu in ui/drivers/ui_qt.cpp ui/drivers/ui_qt_widgets.cpp; do
      if ! out=$($CXX -Wall -Wno-unused-function $INC $DEFS $menu $QT \
            -fsyntax-only "$tu" 2>&1); then
         echo "FAIL  $tu, $name"
         echo "$out" | grep ": error: " | head -8
         fail=1
      else
         echo "ok    $tu, $name"
      fi
   done
done

if [ "$fail" != 0 ]; then
   echo "qt_menu_gate_check: FAIL"
   exit 1
fi
echo "qt_menu_gate_check: OK"
