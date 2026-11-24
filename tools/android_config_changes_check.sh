#!/bin/sh
# RetroActivityFuture must handle every configuration change itself: an
# unhandled one relaunches the Activity, and RetroArch exits with it.
# A move onto another display can change any of the display-dependent
# fields, so each manifest that declares the Activity lists them all.
#   tools/android_config_changes_check.sh
set -u
required="mcc mnc locale touchscreen keyboard keyboardHidden navigation
orientation screenLayout uiMode screenSize smallestScreenSize fontScale
density colorMode layoutDirection"
manifests="pkg/android/phoenix/AndroidManifest.xml
pkg/android/phoenix-jelly-bean/AndroidManifest.xml"
fail=0
for m in $manifests; do
   if [ ! -f "$m" ]; then
      echo "MISSING $m"; fail=1; continue
   fi
   line=$(grep 'retroactivity.RetroActivityFuture"' "$m" | grep configChanges)
   if [ -z "$line" ]; then
      echo "FAIL $m: RetroActivityFuture declares no configChanges"
      fail=1; continue
   fi
   flags="|$(printf '%s\n' "$line" | sed 's/.*android:configChanges="\([^"]*\)".*/\1/')|"
   for r in $required; do
      case "$flags" in
         *"|$r|"*) ;;
         *) echo "FAIL $m: configChanges lacks $r"; fail=1 ;;
      esac
   done
done
[ $fail -eq 0 ] && echo "PASS android configChanges"
exit $fail
