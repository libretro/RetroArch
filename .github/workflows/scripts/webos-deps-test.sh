#!/bin/sh
# Checks webos-deps.sh against a stub gh: transient API failures are
# retried, a lasting outage falls back to the cache, and a cached tag
# downloads nothing.
set -eu

script=$(cd "$(dirname "$0")" && pwd)/webos-deps.sh
t=$(mktemp -d)
trap 'rm -rf "$t"' EXIT
mkdir -p "$t/bin"
cat > "$t/bin/gh" <<'EOF'
#!/bin/sh
n=$(cat "$STUB/count" 2>/dev/null || echo 0)
echo $((n + 1)) > "$STUB/count"
if [ "$n" -lt "$STUB_FAIL" ]; then
   echo "HTTP 500" >&2
   exit 1
fi
case $2 in
view) echo v2 ;;
download)
   echo "$2 $3" >> "$STUB/downloads"
   while [ $# -gt 0 ]; do
      case $1 in
         -p) p=$2; shift ;;
         -D) d=$2; shift ;;
      esac
      shift
   done
   mkdir -p "$d"
   echo x > "$d/$(echo "$p" | sed 's/\*/1.0/')"
   ;;
esac
EOF
chmod +x "$t/bin/gh"
export PATH="$t/bin:$PATH" STUB="$t" WEBOS_DEPS_DELAY=0 WEBOS_DEPS_TRIES=3
fail=0

run() {
   rm -f "$t/count" "$t/downloads"
   STUB_FAIL=$1 WEBOS_DEPS="$t/deps" sh "$script" fetch > "$t/log" 2>&1
}

check() {
   if eval "$2"; then
      echo "ok   $1"
   else
      echo "FAIL $1"
      cat "$t/log"
      fail=1
   fi
}

run 2 && rc=0 || rc=$?
check "transient failures are retried" \
   '[ $rc = 0 ] && [ "$(cat $t/deps/cscd98_buildroot-nc4/TAG)" = v2 ]'

run 0 && rc=0 || rc=$?
check "cached tag downloads nothing" '[ $rc = 0 ] && [ ! -e $t/downloads ]'

echo v1 > "$t/deps/webosbrew_ares-cli-rs/TAG"
run 1000 && rc=0 || rc=$?
check "outage falls back to the cache" \
   '[ $rc = 0 ] && grep -q "using cached v1" $t/log'

rm -rf "$t/deps"
run 1000 && rc=0 || rc=$?
check "outage without a cache fails" '[ $rc != 0 ]'

rm -f "$t/count"
key=$(STUB_FAIL=1000 sh "$script" key 2> "$t/log")
check "key is unresolved during an outage" '[ "$key" = unresolved ]'

rm -f "$t/count"
key=$(STUB_FAIL=0 sh "$script" key)
check "key names the latest releases" \
   '[ ${#key} = 16 ] && [ "$key" != unresolved ]'

exit $fail
