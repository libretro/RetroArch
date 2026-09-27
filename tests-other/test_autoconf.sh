#!/bin/bash
# Regression tests for gamepad autoconfiguration (task_autodetect),
# driven end to end through the real binary with the test input
# driver and the .ratst step scripts.
#
# Usage: tests-other/test_autoconf.sh [path-to-retroarch]
#
# Requires a build with HAVE_TEST_DRIVERS (the default).  Runs
# headless under timeout(1).
#
# Assertions match on device+port *fragments* rather than whole log
# lines on purpose: autoconfig runs as asynchronous tasks and the
# log is written from more than one thread, so an occasional line
# arrives with another thread's fragment spliced into it.  That
# interleaving is cosmetic - the message content is intact - so a
# fragment grep is both correct and robust, where a whole-line
# grep -c is flaky.  A run is also repeated a few times where a
# cold-vs-warm (index absent vs present) difference is being
# checked, so a one-off splice cannot pass or fail a phase alone.
#
# Any sanitizer report in any phase fails the suite, so running
# this against an ASan/UBSan build extends the coverage for free.

set -u

RETROARCH="${1:-./retroarch}"
RATST_DIR="$(cd "$(dirname "$0")" && pwd)"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/ratest.XXXXXX")"
HOME_DIR="$WORK/home"
AUTOCONF="$WORK/autoconf"
LOG="$WORK/run.log"
INDEX="$AUTOCONF/.autoconfig_index"
FAILED=0

say()  { printf '%s\n' "$*"; }
pass() { say "[PASS] $*"; }
fail() { say "[FAIL] $*"; FAILED=1; }

# assert_seen <pattern> <description>: pattern present at least once
assert_seen() {
   if grep -qE "$1" "$LOG"; then pass "$2"
   else fail "$2 (missing /$1/)"; fi
}
assert_absent() {
   if grep -qE "$1" "$LOG"; then fail "$2 (found /$1/)"
   else pass "$2"; fi
}
assert_clean() {
   if grep -qE "ERROR: (Address|Leak|Undefined|Thread)Sanitizer|runtime error:|Segmentation fault" "$LOG"; then
      fail "sanitizer/crash output in log"
      grep -E "ERROR: (Address|Leak|Undefined|Thread)Sanitizer|runtime error:" "$LOG" | head -3
   fi
}

setup() {
   rm -rf "$HOME_DIR" "$AUTOCONF"
   mkdir -p "$HOME_DIR/.config/retroarch" "$AUTOCONF"
   cp "$RATST_DIR"/autoconf/*.cfg "$AUTOCONF/"
   cat > "$HOME_DIR/.config/retroarch/retroarch.cfg" <<CFG
input_driver = "test"
input_joypad_driver = "test"
joypad_autoconfig_dir = "$AUTOCONF"
test_input_file_joypad = "$RATST_DIR/$1"
video_driver = "null"
audio_driver = "null"
menu_driver = "null"
network_cmd_enable = "false"
CFG
}

run() {
   # Pass the config explicitly rather than relying on HOME discovery:
   # the default search path varies with the environment (XDG base,
   # portable-mode marker, an existing user config), and CI showed a
   # run that found no config produces no [Autoconf] lines and fails
   # every assertion identically.  --config is unambiguous.  HOME is
   # still pinned so any writes land in the sandbox.
   HOME="$HOME_DIR" timeout 25 "$RETROARCH" \
      --config "$HOME_DIR/.config/retroarch/retroarch.cfg" \
      --verbose > "$LOG" 2>&1
   assert_clean
   # Guard against a silent no-op: if the run produced no autoconfig
   # activity at all, every content assertion below would fail with
   # the same misleading 'missing pattern' - surface the real cause.
   if ! grep -qE '\[Autoconf\]' "$LOG"; then
      fail "run produced no [Autoconf] output (binary ran? config loaded? test drivers built?)"
      tail -5 "$LOG" | sed 's/^/       /'
   fi
}

index_hash() { md5sum "$INDEX" 2>/dev/null | cut -d' ' -f1; }

# Point the next run at a different .ratst without touching the
# profile directory, so a phase can build an index with one device and
# then connect a different one against it.
use_ratst() {
   sed -i "s#^test_input_file_joypad = .*#test_input_file_joypad = \"$RATST_DIR/$1\"#" \
      "$HOME_DIR/.config/retroarch/retroarch.cfg"
}

say "== autoconf regression suite: $RETROARCH"
[ -x "$RETROARCH" ] || { say "binary not found/executable"; exit 1; }

# ---------------------------------------------------------------
say "-- P1: fresh directory - full scan, index build, full cycle"
setup test_input_autoconf_cycle.ratst
[ ! -e "$INDEX" ] || fail "index present before first run"
run
# Device A (0001:0002) is won by TestpadD's vid/pid tuple (aff 30)
# over TestpadA's name-only 20, and displays as 'device name D' -
# asserting this pins vid/pid-tuple ranking above name ranking.
assert_seen 'device name D configured in port 1'    "P1 connect: vid/pid winner on port 1"
assert_seen 'device B configured in port 2'         "P1 connect: name match on port 2"
# Disconnect assertions check the port, not the device name.  The
# notice carries the port's stored display name when the connect task
# had already applied it, the raw driver-reported name when it had
# not, and a "not available" placeholder when neither is set - see the
# fallbacks in input_autoconfigure_disconnect.  Which one appears
# depends on connect/disconnect task ordering, so pinning the name
# pins a race.  Device identity is asserted on the connect side above,
# where it is stable and where it actually exercises profile matching.
assert_seen 'disconnected from port 1' "P1 disconnect clears port 1"
assert_seen 'device C configured in port 1'          "P1 reconnect: different device reconfigures port 1"
assert_seen 'disconnected from port 2'               "P1 disconnect clears port 2"
[ -s "$INDEX" ] && pass "P1 index written" || fail "P1 index missing/empty"

# ---------------------------------------------------------------
say "-- P2: warm rerun - index present, same outcome, index stable"
H_before="$(index_hash)"
run
assert_seen 'device name D configured in port 1' "P2 vid/pid winner via index"
assert_seen 'device B configured in port 2'      "P2 name match via index"
[ "$H_before" = "$(index_hash)" ] && pass "P2 index untouched by index-hit connects" \
                                  || fail "P2 index rewritten on pure hits"

# ---------------------------------------------------------------
say "-- P3: in-place edit of the winner's vid/pid - verify catch"
# Break the actual winning signal: TestpadD wins device A only on its
# vid/pid tuple.  Zeroing the product id in place (file count
# unchanged, so the freshness header cannot notice) must be caught by
# the winner re-score, and the scan must then fall back to TestpadA's
# name match - which displays as the plain device name 'A'.
setup test_input_autoconf_cycle.ratst
run                                           # build a fresh index
H2="$(index_hash)"
sed -i 's/input_product_id = 2/input_product_id = 0/' "$AUTOCONF/TestpadD_alternative.cfg"
run
assert_seen 'device A configured in port 1'   "P3 fallback: name match after winner's vid/pid broken"
assert_absent 'device name D configured in port 1' "P3 stale vid/pid winner not selected"
[ "$H2" != "$(index_hash)" ] && pass "P3 index healed after verify caught the edit" \
                             || fail "P3 index not rewritten after verify failure"

# ---------------------------------------------------------------
say "-- P4: profile added - file-count freshness invalidates"
setup test_input_autoconf_cycle.ratst
run                                           # fresh index
cp "$AUTOCONF/TestpadA.cfg" "$AUTOCONF/zz_added.cfg"
H3="$(index_hash)"
run
assert_seen 'device name D configured in port 1' "P4 count-stale index falls back; scan finds winner"
[ "$H3" != "$(index_hash)" ] && pass "P4 index rebuilt at new file count" \
                             || fail "P4 index not rebuilt after count change"

# ---------------------------------------------------------------
say "-- P5: winning profile deleted - freshness + fallback recover"
setup test_input_autoconf_cycle.ratst
run                                           # fresh index
rm "$AUTOCONF/TestpadD_alternative.cfg"
run
assert_seen 'device A configured in port 1'      "P5 deletion caught; name match selected"
assert_absent 'device name D configured in port 1' "P5 deleted winner not selected"

# ---------------------------------------------------------------
say "-- P6: unrecognised device - full scan, no index churn"
setup test_input_autoconf_unknown.ratst
run                                           # first run builds index
assert_seen 'not configured'                  "P6 unknown device reported unconfigured"
H6="$(index_hash)"
run
assert_seen 'not configured'                  "P6 unknown device unconfigured on rerun"
[ -n "$H6" ] && [ "$H6" = "$(index_hash)" ] \
   && pass "P6 identical index not rewritten for a no-match device" \
   || fail "P6 index churned on an unrecognised device"

# ---------------------------------------------------------------
say "-- P7: input_phys ranking - matching phys outranks a bogus one"
# TestpadP_phys and TestpadQ_physbogus carry identical vendor/product
# ids and an identical input_device, so they differ only in
# input_phys: 60 for the match against 40 for the mismatch.  Q sorts
# after P here, so the run is repeated below with the order reversed -
# ranking must not depend on which file the walk reaches first.
setup test_input_autoconf_phys.ratst
run
assert_seen 'phys match configured in port 1'      "P7 matching phys wins on a fresh scan"
assert_absent 'phys mismatch configured'           "P7 bogus phys not selected"
run
assert_seen 'phys match configured in port 1'      "P7 matching phys wins via the index"

setup test_input_autoconf_phys.ratst
mv "$AUTOCONF/TestpadP_phys.cfg" "$AUTOCONF/AA_phys.cfg"
run
assert_seen 'phys match configured in port 1'      "P7 matching phys wins when it sorts first"
assert_absent 'phys mismatch configured'           "P7 bogus phys not selected when it sorts last"

# ---------------------------------------------------------------
say "-- P8: in-place edit promoting a non-winner - index must not hide it"
# The regression behind #19540.  The index re-scores only its winner,
# so an entry it *understates* never gets re-read: before the
# directory fingerprint, a rival whose own claim was honest verified
# cleanly and was configured, at its own lower affinity, without the
# directory ever being scanned.
#
# Both edits below are size-preserving, so only the modification times
# separate the edited directory from the indexed one - which is
# exactly the case a *.cfg count could never catch.
setup test_input_autoconf_unknown.ratst
# Demote TestpadP below TestpadQ *before* the index is built: with a
# wrong device name and a wrong phys it scores 20, against Q's honest
# 40.  Both edits are size-preserving, here and below.
sed -i -e 's#Test joypad device P#Test joypad device X#' \
       -e 's#usb-0000:00:14.0-3/input0#usb-0000:00:77.7-7/input7#' \
   "$AUTOCONF/TestpadP_phys.cfg"
# Build the index with a device nothing matches: a connect that scored
# 60 would take the scan's early exit and discard the index build, so
# the phys device itself cannot be the one that writes it.
run
[ -s "$INDEX" ] || fail "P8 no index to go stale"
# Now promote P back to a 60 while the index still records it at 20.
# Q is untouched, so Q's recorded 40 stays honest - and an honest
# claim is what verification checks.  The index therefore ranks Q top,
# re-scores only Q, finds 40 as promised and configures it, never
# reading the profile that now scores 60.  That is #19540.
sleep 1
sed -i -e 's#Test joypad device X#Test joypad device P#' \
       -e 's#usb-0000:00:77.7-7/input7#usb-0000:00:14.0-3/input0#' \
   "$AUTOCONF/TestpadP_phys.cfg"
use_ratst test_input_autoconf_phys.ratst
run
assert_seen 'phys match configured in port 1' \
   "P8 promoted profile selected after a size-preserving edit"
assert_absent 'phys mismatch configured' \
   "P8 understated index does not hide the true winner"

# Same edit, but with no sleep: the index and the edit can land in the
# same whole second, which the stored timestamp alone cannot separate.
setup test_input_autoconf_unknown.ratst
sed -i -e 's#Test joypad device P#Test joypad device X#' \
       -e 's#usb-0000:00:14.0-3/input0#usb-0000:00:77.7-7/input7#' \
   "$AUTOCONF/TestpadP_phys.cfg"
run
[ -s "$INDEX" ] || fail "P8 no index for the same-second case"
sed -i -e 's#Test joypad device X#Test joypad device P#' \
       -e 's#usb-0000:00:77.7-7/input7#usb-0000:00:14.0-3/input0#' \
   "$AUTOCONF/TestpadP_phys.cfg"
use_ratst test_input_autoconf_phys.ratst
run
assert_seen 'phys match configured in port 1' \
   "P8 same-second edit still rescanned"

# ---------------------------------------------------------------
rm -rf "$WORK"
if [ "$FAILED" -eq 0 ]; then
   say "== autoconf regression suite: ALL PASS"; exit 0
fi
say "== autoconf regression suite: FAILURES"; exit 1
