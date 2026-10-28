#!/bin/sh
# The actions/cache key for apt-install.sh's cache: the packages asked
# for and the runner image. Jobs asking for the same packages share an
# entry, so the repository's cache holds each set once. A new runner
# image makes a new entry, seeded through the prefix by the last one's;
# apt-install.sh refreshes what no longer resolves.
# Off x86-64 the key also names the runner's architecture.
#
#   apt-cache-key.sh "PACKAGE..."   (writes key= and prefix= to
#                                    GITHUB_OUTPUT)
set -eu
pkgs=$(printf '%s\n' $1 | sort -u | tr '\n' ' ')
hash=$(printf '%s' "$pkgs" | sha256sum | cut -c1-16)
arch=
[ "${RUNNER_ARCH:-X64}" = X64 ] || arch="$RUNNER_ARCH-"
prefix="apt-${ImageOS:-unknown}-$arch$hash-"
key="$prefix${ImageVersion:-unknown}"
echo "key=$key" >> "$GITHUB_OUTPUT"
echo "prefix=$prefix" >> "$GITHUB_OUTPUT"
echo "apt cache key: $key"
