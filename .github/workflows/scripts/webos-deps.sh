#!/bin/sh
# Fetches the webOS packaging tools and NDKs from their latest releases.
#
#   webos-deps.sh key      print the cache key of the latest releases
#   webos-deps.sh fetch    fill WEBOS_DEPS with them
#
# WEBOS_DEPS (default /tmp/webos-deps) holds one directory per release
# repository, with the asset and its tag; actions/cache keeps it. The
# GitHub API fails now and then (HTTP 500), so each call is retried, and
# when it keeps failing the assets the cache already has are used.
set -eu

dir=${WEBOS_DEPS:-/tmp/webos-deps}
delay=${WEBOS_DEPS_DELAY:-15}
tries=${WEBOS_DEPS_TRIES:-5}

deps() {
   cat <<'EOF'
webosbrew/ares-cli-rs ares-package_*_amd64.deb
webosbrew/dev-toolbox-cli webosbrew-toolbox-gen-manifest_*_amd64.deb
openlgtv/buildroot-nc4 arm-webos-linux-gnueabi_sdk-buildroot-x86_64.tar.gz
cscd98/buildroot-nc4 aarch64-webos-linux-gnu_sdk-buildroot-x86_64.tar.gz
EOF
}

retry() {
   n=1
   until "$@" < /dev/null; do
      [ "$n" -lt "$tries" ] || return 1
      echo "webos-deps.sh: attempt $n of $tries failed; retrying" >&2
      sleep $((delay * n))
      n=$((n + 1))
   done
}

latest_tag() {
   retry gh release view -R "$1" --json tagName -q .tagName
}

have() {
   for f in "$1"/$2; do
      [ -s "$f" ] && return 0
   done
   return 1
}

case ${1:-} in
key)
   tags=
   while read -r repo pattern; do
      tag=$(latest_tag "$repo") || { echo unresolved; exit 0; }
      tags="$tags $repo@$tag"
   done <<EOF
$(deps)
EOF
   echo "$tags" | sha1sum | cut -c1-16
   ;;
fetch)
   mkdir -p "$dir"
   while read -r repo pattern; do
      d=$dir/$(echo "$repo" | tr / _)
      if tag=$(latest_tag "$repo"); then
         if [ "$(cat "$d/TAG" 2>/dev/null)" = "$tag" ] && have "$d" "$pattern"; then
            echo "webos-deps.sh: $repo $tag cached"
            continue
         fi
         rm -rf "$d.new"
         if retry gh release download "$tag" -R "$repo" -p "$pattern" \
               -D "$d.new" --clobber && have "$d.new" "$pattern"; then
            echo "$tag" > "$d.new/TAG"
            rm -rf "$d"
            mv "$d.new" "$d"
            echo "webos-deps.sh: $repo $tag downloaded"
            continue
         fi
         rm -rf "$d.new"
      fi
      if have "$d" "$pattern"; then
         echo "::warning::$repo unreachable; using cached $(cat "$d/TAG" 2>/dev/null || echo release)"
      else
         echo "::error::$repo unreachable and not cached"
         exit 1
      fi
   done <<EOF
$(deps)
EOF
   ;;
*)
   echo "usage: webos-deps.sh key|fetch" >&2
   exit 2
   ;;
esac
