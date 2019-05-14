#!/bin/sh

set -eu

cd -- "$(cd -- "${0%/*}/" && pwd -P)"

. ../qb/qb.init.sh

error=
fix=

usage='intl_bom.sh - Tests files for a UTF-8 BOM at the header.

  Usage: intl_bom.sh [OPTIONS]
    -f, --fix               Automatically add a BOM when missing.
    -h, --help              Shows this message.'

while [ $# -gt 0 ]; do
   option="$1"
   shift
   case "$option" in
      -- ) break ;;
      -f|--fix ) fix=1 ;;
      -h|--help ) die 0 "$usage" ;;
      * ) die 1 "Unrecognized option '$option', use -h for help." ;;
   esac
done

trap 'err=$?; rm -f tmp-*.c tmp-*.h; trap - EXIT; exit $err' EXIT INT

for file in *.c *.h; do
  hex="$(hexdump -n 3 -C "$file" | head -1 | tr -s '[:blank:]')"
  bom="${hex% *}"
  if [ "${bom#* }" != 'ef bb bf' ]; then
    if [ -n "$fix" ]; then
      printf '\xEF\xBB\xBF' | cat - "$file" > "tmp-$file"
      mv "tmp-$file" "$file"
    else
      error="$error $file"
    fi
  fi
done

if [ -n "$error" ]; then
  eval "set -- $error"
  for err do
    die : "ERROR: $err is missing an UTF-8 BOM at the header."
  done
  die 1 "Run 'intl/intl_bom.sh -f' and commit the results to fix these files."
fi

exit 0
