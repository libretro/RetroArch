#!/bin/sh
# Build and run the libretrodb tests under AddressSanitizer,
# UndefinedBehaviorSanitizer, LeakSanitizer and ThreadSanitizer.
#
#   ./sweep.sh [tree]
#
# With no argument the tree is inferred from this script's location.
# Exits non-zero if any run reports, so it can gate a series.
#
# The in-tree tests always run.  Setting SWEEP_CORPUS to a directory
# holding .rdb files and the matching harness sources adds the
# larger checks - real databases, a malformed corpus, and the
# threaded runs - which need data too big to keep in the repository.
#
#   SWEEP_CORPUS=~/rdb-corpus ./sweep.sh

set -u
TREE="${1:-$(cd "$(dirname "$0")/../../.." && pwd)}"
CORPUS="${SWEEP_CORPUS:-}"
WORK="$(mktemp -d)"
FAIL=0

DB="$TREE/libretro-db"
LC="$TREE/libretro-common"

LIB="$DB/libretrodb.c $DB/rmsgpack.c $DB/rmsgpack_dom.c $DB/query.c \
$DB/bintree.c $LC/streams/interface_stream.c $LC/streams/file_stream.c \
$LC/streams/memory_stream.c $LC/vfs/vfs_implementation.c \
$LC/compat/compat_strl.c $LC/string/stdstring.c \
$LC/encodings/encoding_utf.c $LC/file/file_path.c \
$LC/compat/fopen_utf8.c $LC/time/rtime.c $LC/lists/string_list.c \
$LC/compat/compat_strcasestr.c $LC/compat/compat_fnmatch.c \
$LC/encodings/encoding_crc32.c $LC/features/features_cpu.c"

INC="-I$LC/include -I$DB"

say()  { printf '  %-46s %s\n' "$1" "$2"; }
bad()  { FAIL=1; printf '  %-46s %s\n' "$1" "$2"; }

# $1 = label, $2 = output file.  Reports if any sanitizer fired.
judge()
{
   n=$(grep -cE 'ERROR: (Address|Thread|Leak)Sanitizer|runtime error|SUMMARY: .*Sanitizer' "$2" 2>/dev/null || true)
   if [ "$n" -eq 0 ]; then say "$1" "clean"; else bad "$1" "$n REPORTS"; fi
}

echo "sweep: $TREE"
if [ -z "$CORPUS" ]; then
   echo "  (no SWEEP_CORPUS set: running the in-tree tests only)"
fi

echo "=== build ==="
for h in t dumpall dump q qoob; do
   [ -n "$CORPUS" ] && [ -f "$CORPUS/$h.c" ] || continue
   gcc -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer $INC \
       -o "$WORK/a_$h" "$CORPUS/$h.c" $LIB 2>"$WORK/build_$h.log" \
     || { bad "build $h (asan)" "FAILED"; sed -n '1,3p' "$WORK/build_$h.log"; }
done
for h in race_win race_err; do
   [ -n "$CORPUS" ] && [ -f "$CORPUS/$h.c" ] || continue
   gcc -g -O1 -fsanitize=thread -fno-omit-frame-pointer -pthread $INC \
       -o "$WORK/t_$h" "$CORPUS/$h.c" $LIB 2>"$WORK/build_$h.log" \
     || bad "build $h (tsan)" "FAILED"
done
say "harnesses" "built"

echo "=== in-tree regression tests (ASan+UBSan+LeakSan) ==="
( cd "$DB/samples/libretrodb" && make clean >/dev/null 2>&1
  make SANITIZER=address,undefined >"$WORK/mk.log" 2>&1 ) \
  || bad "sample build" "FAILED"
mkdir -p "$WORK/td"
if [ -x "$DB/samples/libretrodb/libretrodb_parser_test" ]; then
   ASAN_OPTIONS=detect_leaks=1 \
     "$DB/samples/libretrodb/libretrodb_parser_test" "$WORK/td" \
     >"$WORK/parser.out" 2>&1
   res=$(tr '\r' '\n' < "$WORK/parser.out" | grep -oE '[0-9]+ checks, [0-9]+ failures')
   case "$res" in
      *", 0 failures") judge "parser test ($res)" "$WORK/parser.out" ;;
      *)               bad   "parser test" "${res:-no result}" ;;
   esac
else
   bad "parser test" "not built"
fi
if [ -x "$DB/samples/libretrodb/libretrodb_leak_test" ]; then
   ASAN_OPTIONS=detect_leaks=1 \
     "$DB/samples/libretrodb/libretrodb_leak_test" >"$WORK/leak.out" 2>&1
   judge "leak test" "$WORK/leak.out"
else
   bad "leak test" "not built"
fi
( cd "$DB/samples/libretrodb" && make clean >/dev/null 2>&1 )

echo "=== archive and lzma tests (ASan+UBSan+LeakSan) ==="
# Both cover defects that stopped a scan dead rather than failing:
# an lc=4 stream the decoder refused, and a lookup for a member that
# is not there spinning instead of returning.
for d in "$LC/samples/file/archive_file" "$LC/samples/formats/r7z"; do
   [ -f "$d/Makefile" ] || continue
   name=$(basename "$d")
   ( cd "$d" && make clean >/dev/null 2>&1
     make check SANITIZER=address,undefined ) >"$WORK/$name.log" 2>&1
   rc=$?
   ( cd "$d" && make clean >/dev/null 2>&1 )
   if [ $rc -eq 0 ]; then
      judge "$name" "$WORK/$name.log"
   else
      bad "$name" "exit $rc (a hang here reads as a timeout)"
      grep -E "FAIL|error:" "$WORK/$name.log" | head -3
   fi
done

echo "=== scanner tests (ASan+UBSan+LeakSan) ==="
# These link most of the frontend, so they are slower to build than
# everything else here; they are the only coverage of the scan as a
# whole, including the task teardown and the playlist write.
SCAN_DIR="$TREE/samples/tasks/database"
if [ -f "$SCAN_DIR/Makefile" ]; then
   ( cd "$SCAN_DIR" && make clean >/dev/null 2>&1
     make check SANITIZER=address,undefined >"$WORK/scan.log" 2>&1 )
   res=$(grep -oE '[0-9]+ checks, [0-9]+ failures' "$WORK/scan.log" | tr '\n' ' ')
   if grep -q "0 failures" "$WORK/scan.log" \
      && ! grep -qE '[1-9][0-9]* failures' "$WORK/scan.log"; then
      judge "scanner tests ($res)" "$WORK/scan.log"
   else
      bad "scanner tests" "${res:-did not run}"
      grep -E "FAIL|error:" "$WORK/scan.log" | head -3
   fi
   ( cd "$SCAN_DIR" && make clean >/dev/null 2>&1 )
else
   say "scanner tests" "skipped (sample not present)"
fi

echo "=== real databases (ASan+UBSan+LeakSan) ==="
if [ -n "$CORPUS" ] && [ -d "$CORPUS/real" ] && [ -x "$WORK/a_dumpall" ]; then
   for f in "$CORPUS"/real/*.rdb; do
      [ -e "$f" ] || continue
      ASAN_OPTIONS=detect_leaks=1 "$WORK/a_dumpall" "$f" \
        >"$WORK/rec.out" 2>"$WORK/rec.err"
      judge "$(basename "$f" .rdb) ($(wc -l < "$WORK/rec.out") records)" "$WORK/rec.err"
   done
else
   say "real databases" "skipped (no corpus)"
fi

echo "=== malformed corpus (ASan+UBSan+LeakSan) ==="
if [ -x "$WORK/a_t" ]; then
   n=0; r=0
   for f in "$CORPUS"/rdb/*.rdb "$CORPUS"/idx/*.rdb "$CORPUS"/typeconf*.rdb \
            "$CORPUS"/bigfield.rdb "$CORPUS"/valid/valid.rdb; do
      [ -e "$f" ] || continue
      n=$((n+1))
      ASAN_OPTIONS=detect_leaks=1 timeout 90 "$WORK/a_t" "$f" \
        >"$WORK/c.out" 2>&1
      st=$?
      if [ $st -eq 124 ]; then r=$((r+1)); bad "hang" "$(basename "$f")"; fi
      grep -qE 'ERROR: |runtime error' "$WORK/c.out" && { r=$((r+1)); bad "report" "$(basename "$f")"; }
   done
   [ "$r" -eq 0 ] && say "$n files" "clean"
fi

echo "=== query parser, unterminated buffers (ASan+UBSan) ==="
if [ -n "$CORPUS" ] && [ -x "$WORK/a_qoob" ] && [ -f "$CORPUS/valid/valid.rdb" ]; then
   ASAN_OPTIONS=detect_leaks=1 "$WORK/a_qoob" "$CORPUS/valid/valid.rdb" \
     >"$WORK/q.out" 2>&1
   judge "slice handling" "$WORK/q.out"
fi

echo "=== ThreadSanitizer ==="
if [ -x "$WORK/t_race_win" ]; then
   for f in "$CORPUS"/real/*.rdb "$CORPUS"/valid/valid.rdb; do
      [ -e "$f" ] || continue
      TSAN_OPTIONS=exitcode=0 timeout 400 "$WORK/t_race_win" "$f" \
        >"$WORK/tw.out" 2>&1
      judge "concurrent scans: $(basename "$f" .rdb)" "$WORK/tw.out"
   done
fi
if [ -x "$WORK/t_race_err" ]; then
   # any real database will do; fall back to the small built one
   f=$(ls "$CORPUS"/real/*.rdb 2>/dev/null | head -1)
   [ -n "$f" ] && [ -e "$f" ] || f="$CORPUS/valid/valid.rdb"
   TSAN_OPTIONS=exitcode=0 timeout 400 "$WORK/t_race_err" "$f" \
     >"$WORK/te.out" 2>&1
   judge "concurrent failing-query compiles" "$WORK/te.out"
fi

rm -rf "$WORK"
echo
if [ $FAIL -eq 0 ]; then
   echo "SWEEP CLEAN"
else
   echo "SWEEP FAILED"
fi
exit $FAIL
